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
#include <getopt.h>
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
#include "attach_probes.h"
#include "ebpf_syscalls.h"
#include "generate_ebpf.h"
#include "print_event_cb.h"


/*
 * fprint_usage -- This function prints usage message in stream.
 */
static void
fprint_usage(FILE *f)
{
	size_t fw_res;

	fw_res = fwrite(_binary_usage_text_txt_start,
			(size_t)_binary_usage_text_txt_size,
			1, f);

	assert(fw_res > 0);
}

/*
 * fprint_help -- This function prints help message in stream.
 */
static void
fprint_help(FILE *f)
{
	size_t fw_res;

	fprint_usage(f);

	fw_res = fwrite("\n", 1, 1, f);

	assert(fw_res > 0);

	fw_res = fwrite(_binary_help_text_txt_start,
			(size_t)_binary_help_text_txt_size,
			1, f);

	assert(fw_res > 0);
}

/*
 * fprint_trace_list -- This function prints description of expressions
 *     in stream.
 */
static void
fprint_trace_list(FILE *f)
{
	size_t fw_res;

	fw_res = fwrite(_binary_trace_list_text_txt_start,
			(size_t)_binary_trace_list_text_txt_size,
			1, f);

	assert(fw_res > 0);
}

struct cl_options args;
bool Cont = true;
FILE *out;
enum out_fmt out_fmt;

/* HACK Should be fixed in libbcc */
extern int perf_reader_page_cnt;

/* 8 Megabytes should be something close to reasonable */
enum { OUT_BUF_SIZE = 8 * 1024 * 1024 };
/* XXX Should be configurable through command line */
static unsigned out_buf_size = OUT_BUF_SIZE;

/*
 * main -- Tool's entry point
 */
int
main(int argc, char *argv[])
{
	args.pid = -1;
	args.out_sep_ch = '\t';

	/*
	 * XXX Should be set by cl options
	 *    if we need something over syscalls
	 */
	args.pr_arr_max = 1000;

	/*
	 * XXX Let's enlarge ring buffers. It's really improve situation
	 *    with lost events. In the future we should do it via cl options.
	 */
	perf_reader_page_cnt *= perf_reader_page_cnt;
	perf_reader_page_cnt *= perf_reader_page_cnt;

	while (1) {
		int c;
		int option_index = 0;

		static struct option long_options[] = {
			{"timestamp",	no_argument,	   0, 't'},
			{"failed",		no_argument,	   0, 'X'},
			{"help",		no_argument,	   0, 'h'},
			{"debug",		no_argument,	   0, 'd'},
			{"list",		no_argument,	   0, 'L'},
			{"ll-list",		no_argument,	   0, 'R'},
			{"builtin-list", no_argument,	   0, 'B'},

			{"fast-follow-fork", optional_argument,	   0, 'F'},
			{"full-follow-fork", optional_argument,	   0, 'f'},

			{"pid",		   required_argument, 0, 'p'},
			{"format",		required_argument, 0, 'l'},
			{"expr",		required_argument, 0, 'e'},
			{"output",		required_argument, 0, 'o'},
			{"hex-separator", required_argument, 0, 'K'},
			{0,	   0,	 0,  0 }
		};

		c = getopt_long(argc, argv, "+tXhdp:o:l:K:e:LRBf::F::",
				long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 't':
			args.timestamp = true;
			break;

		case 'X':
			args.failed = true;
			break;

		case 'h':
			fprint_help(stdout);
			exit(EXIT_SUCCESS);

		case 'd':
			args.debug = true;
			break;

		case 'p':
			args.pid = atoi(optarg);
			if (args.pid < 1) {
				fprintf(stderr,
					"ERROR: wrong value for pid option is"
					" provided: '%s' => '%d'. Exit.\n",
					optarg, args.pid);

				exit(EXIT_FAILURE);
			}
			if (kill(args.pid, 0) == -1) {
				fprintf(stderr,
					"ERROR: Process with pid '%d'"
					" does not exist : '%m'.\n",
					args.pid);

				fprintf(stderr, "Exit.\n");
				exit(EXIT_FAILURE);
			}
			break;

		case 'o':
			args.out_fn = optarg;
			break;

		case 'K':
			args.out_sep_ch = *optarg;
			break;

		case 'e':
			if (!strcasecmp(optarg, "list") ||
					!strcasecmp(optarg, "help")) {
				fprintf(stderr,
					"List of supported expressions:"
					" 'help', 'list', 'trace=set'"
					"\n");
				fprintf(stderr,
					"For list of supported sets you should"
					"use 'trace=help' or 'trace=list'"
					"\n");
				exit(EXIT_SUCCESS);
			} else if (!strcasecmp(optarg, "trace=help") ||
					!strcasecmp(optarg,
						"trace=list")) {
				fprint_trace_list(stderr);
				fprintf(stderr,
					"You can combine sets"
					" by using comma.\n");
				exit(EXIT_SUCCESS);
			}
			args.expr = optarg;
			break;

		case 'l':
			if (!strcasecmp(optarg, "list") ||
					!strcasecmp(optarg, "help")) {
				fprintf(stderr,
					"List of supported formats:"
					"'bin', 'binary', 'hex', 'hex_raw', "
					"'hex_sl', 'strace', "
					"'list' & 'help'\n");
				exit(EXIT_SUCCESS);
			}
			args.out_fmt_str = optarg;
			out_fmt = out_fmt_str2enum(args.out_fmt_str);
			break;

		case 'L':
			get_sc_list(stdout, is_a_sc);
			exit(EXIT_SUCCESS);

		case 'R':
			get_sc_list(stdout, NULL);
			exit(EXIT_SUCCESS);

		case 'B':
			for (unsigned i = 0; i < SC_TBL_SIZE; i++)
				if (NULL != syscall_array[i].handler_name)
					fprintf(stdout,
						"%03d: %-20s\t %s\n",
						syscall_array[i].num,
						syscall_array[i].num_name,
						syscall_array[i].handler_name);
			exit(EXIT_SUCCESS);

		case 'f':
			args.ff_mode = E_FF_FULL;
			if (optarg) {
				args.ff_separate_logs = true;
			}
			break;

		case 'F':
			args.ff_mode = E_FF_FAST;
			if (optarg) {
				args.ff_separate_logs = true;
			}
			break;

		case ':':
			fprintf(stderr, "ERROR: "
				"Missing mandatory option's "
				"argument\n");
			fprint_help(stderr);
			exit(EXIT_FAILURE);

		default:
			fprintf(stderr, "ERROR: "
				"Unknown option: '-%c'\n", c);
		case '?':
			fprint_help(stderr);
			exit(EXIT_FAILURE);
		}
	}

	if (optind < argc)
		args.command = true;

	/* Check for JIT acceleration of BPF */
	check_bpf_jit_status(stderr);

	if (NULL != args.out_fn) {
		out = fopen(args.out_fn, "w");

		if (NULL == out) {
			fprintf(stderr, "ERROR: "
				"Failed to open '%s' for appending: '%m'\n",
				args.out_fn);

			exit(errno);
		}
	} else {
		out = stdout;
	}

	/* XXX We should improve it. May be we should use fd directly */
	/* setbuffer(out, NULL, out_buf_size); */
	(void) out_buf_size;

	if (args.pid != -1 && args.command) {
		fprintf(stderr, "ERROR: "
				"It is currently unsupported to watch for PID"
				" and command simultaneously.\n");
		fprint_help(stderr);
		exit(EXIT_FAILURE);
	}

	if (args.command) {
		struct sigaction sa;

		args.pid = start_command(argc - optind, argv + optind);

		if (args.pid == -1) {
			fprintf(stderr, "ERROR: "
				"Failed to run: '%s': %m. Exiting.\n",
				argv[optind]);
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

	/* define BPF program */
	char *bpf_str = generate_ebpf();

	if (0 < args.pid) {
		char str[64];
		int snp_res;
		char *pid_check_hook;

		snp_res = snprintf(str, sizeof(str), "%d", 	args.pid);

		assert(snp_res > 0);

		pid_check_hook = load_pid_check_hook(args.ff_mode);

		assert(NULL != pid_check_hook);

		str_replace_all(&pid_check_hook, "TRACED_PID", str);

		str_replace_all(&bpf_str, "PID_CHECK_HOOK", pid_check_hook);

		free(pid_check_hook);

		if (!args.command) {
			if (kill(args.pid, 0) == -1) {
				fprintf(stderr,
					"ERROR: Process with pid '%d'"
					" does not exist: '%m'.\n", args.pid);

				exit(errno);
			}
		}
	} else {
		str_replace_all(&bpf_str, "PID_CHECK_HOOK", "");
	}

	char *trace_h = load_file_no_cr(ebpf_trace_h_file);

	str_replace_all(&bpf_str, "#include \"trace.h\"\n", trace_h);

	free(trace_h);

	if (args.debug) {
		fprintf(stderr, "\t>>>>> Generated eBPF code <<<<<\n");

		if (bpf_str) {
			size_t fw_res;

			fw_res = fwrite(bpf_str, strlen(bpf_str), 1, stderr);

			assert(fw_res > 0);
		}

		fprintf(stderr, "\t>>>>> EndOf generated eBPF code <<<<<<\n");
	}

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
	 * with BPF_PERF_OUTPUT() in trace.c.
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
