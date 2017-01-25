/*
 * Copyright 2016-2017, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * main.c -- Trace syscalls. For Linux, uses BCC, ebpf.
 */

#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <sys/types.h>

#include <linux/sched.h>
#include <linux/limits.h>

/* from bcc import BPF */
#include <bcc/libbpf.h>
#include <bcc/bpf_common.h>
#include <bcc/perf_reader.h>

#include <ebpf/ebpf_file_set.h>

#include "strace_bpf.h"

#include "txt.h"
#include "main.h"
#include "utils.h"
#include "cl_parser.h"
#include "attach_probes.h"
#include "ebpf_syscalls.h"
#include "generate_ebpf.h"
#include "print_event_cb.h"


struct cl_options args;
bool Cont = true;
FILE *out_lf;
enum out_fmt out_fmt;

/* XXX HACK Should be fixed in libbcc */
extern int perf_reader_page_cnt;

/*
 * main -- Tool's entry point
 */
int
main(const int argc, char *const argv[])
{
	int st_optind;

	args.pid = -1;
	args.out_sep_ch = '\t';

	/*
	 * XXX Should be set by cl options
	 *    if we need something over syscalls
	 */
	args.pr_arr_max = 1000;

	/* XXX Should be configurable through command line */
	args.out_buf_size = OUT_BUF_SIZE;

	/*
	 * XXX Let's enlarge ring buffers. It's really improve situation
	 *    with lost events. In the future we should do it via cl options.
	 */
	perf_reader_page_cnt *= perf_reader_page_cnt;
	perf_reader_page_cnt *= perf_reader_page_cnt;

	/* Let's parse command-line options */
	st_optind = cl_parser(&args, argc, argv);

	/* Check for JIT acceleration of BPF */
	check_bpf_jit_status(stderr);

	setup_out_lf();

	/* Settuping of out_lf failed */
	if (NULL == out_lf) {
		fprintf(stderr, "ERROR: Exiting\n");

		exit(errno);
	}

	/*
	 * XXX Temporarilly. We should do it in the future together with
	 *    multi-process attaching.
	 */
	if (args.pid != -1 && args.command) {
		fprintf(stderr, "ERROR: "
				"It is currently unsupported to watch for PID"
				" and command simultaneously.\n");
		fprint_help(stderr);
		exit(EXIT_FAILURE);
	}

	if (args.command) {
		struct sigaction sa;

		args.pid = start_command(argc - st_optind, argv + st_optind);

		if (args.pid == -1) {
			fprintf(stderr, "ERROR: "
				"Failed to run: '%s': %m. Exiting.\n",
				argv[st_optind]);
			exit(errno);
		}

		sa.sa_sigaction = sig_chld_handler;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART | SA_SIGINFO |
			SA_NOCLDSTOP | SA_NOCLDWAIT;

		(void) sigaction(SIGCHLD, &sa, NULL);

		sa.sa_sigaction = sig_transmit_handler;
		sa.sa_flags = SA_RESTART;

		(void) sigaction(SIGINT, &sa, NULL);
		(void) sigaction(SIGHUP, &sa, NULL);
		(void) sigaction(SIGQUIT, &sa, NULL);
		(void) sigaction(SIGTERM, &sa, NULL);

		sa.sa_flags = (int)(SA_RESTART | SA_RESETHAND);
		(void) sigaction(SIGSEGV, &sa, NULL);
	}

	if (0 < args.pid) {
		if (!args.command) {
			if (kill(args.pid, 0) == -1) {
				fprintf(stderr,
					"ERROR: Process with pid '%d'"
					" does not exist: '%m'.\n", args.pid);

				/*
				 * XXX As soon as multi-process attaching will
				 *     be done we should print warning here
				 *     and continue.
				 */
				exit(errno);
			}
		}
	}

	/* define BPF program */
	char *bpf_str = generate_ebpf();

	apply_process_attach_code(&bpf_str);

	/* bcc preprocessor is too limited, so let's do it by hands */
	apply_trace_h_header(&bpf_str);

	/* Print resulting code if debug mode */
	if (args.debug)
		fprint_ebpf_code_with_debug_marks(stderr, bpf_str);

	/* XXX We should do it only by user reques */
	save_trace_h();

	/* initialize BPF */
	struct bpf_ctx *b = calloc(1, sizeof(*b));

	if (NULL == b) {
		fprintf(stderr,
			"ERROR:%s: Out of memory. Exiting.\n", __func__);

		return EXIT_FAILURE;
	}

	/* Compiling of generated eBPF code */
	b->module = bpf_module_create_c_from_string(bpf_str, 0, NULL, 0);
	b->debug  = args.debug;

	free(bpf_str);

	if (!attach_probes(b)) {
		/* No probes were attached */
		fprintf(stderr,
			"ERROR: No probes were attached. Exiting.\n");

		if (args.command) {
			/* let's KILL child */
			kill(args.pid, SIGKILL);
		}

		return EXIT_FAILURE;
	}

	/* header */
	print_header[out_fmt](argc, argv);

	/*
	 * Attach callback to perf output. "events" is a name of class declared
	 * with BPF_PERF_OUTPUT() in ebpf/trace_head.c.
	 *
	 * XXX Most likely we should utilise here str_replace for consistence
	 *    increasing.
	 */
#define PERF_OUTPUT_NAME "events"
	int res = attach_callback_to_perf_output(b,
			PERF_OUTPUT_NAME, print_event_cb[out_fmt]);

	if (!res) {
		if (args.command) {
			/* let's child go */
			kill(args.pid, SIGCONT);
		}
	} else {
		fprintf(stderr,
			"ERROR: Can't attach to perf output '%s'. Exiting.\n",
			PERF_OUTPUT_NAME);

		if (args.command) {
			/* let's KILL child */
			kill(args.pid, SIGKILL);
		}

		detach_all(b);
		return EXIT_FAILURE;
	}

	struct perf_reader *readers[b->pr_arr_qty];

	for (unsigned i = 0; i < b->pr_arr_qty; i++)
		readers[i] = b->pr_arr[i]->pr;

	while (Cont) {
		(void) perf_reader_poll((int)b->pr_arr_qty, readers, -1);

		if (!args.command && 0 < args.pid) {
			if (kill(args.pid, 0) == -1) {
				Cont = false;

				fprintf(stderr,
					"ERROR: Process with pid '%d'"
					" has disappeared : '%m'.\n",
					args.pid);

				fprintf(stderr, "Exit.\n");
			}
		}
	}


	detach_all(b);
	return EXIT_SUCCESS;
}
