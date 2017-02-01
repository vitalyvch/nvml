/*
 * Copyright 2016-2017, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *
 *	 * Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in
 *	   the documentation and/or other materials provided with the
 *	   distribution.
 *
 *	 * Neither the name of the copyright holder nor the names of its
 *	   contributors may be used to endorse or promote products derived
 *	   from this software without specific prior written permission.
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
 * print_event_cb.c -- print_event_cb() function
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include <linux/ptrace.h>
#include <linux/limits.h>

#include "main.h"
#include "ebpf_syscalls.h"
#include "print_event_cb.h"

/*
 * XXX A bit of black magic to have some US <-> KS portability.
 * PLEASE do not add any other includes afters this comment.
 */
typedef __s32 s32;
typedef __u32 u32;
typedef __s64 s64;
typedef __u64 u64;

enum { TASK_COMM_LEN = 16 };

#include <ebpf/trace.h>

static unsigned long long start_ts_nsec = 0;

static inline const char *sc_num2str(const int64_t sc_num);
static inline void fprint_i64(FILE *f, uint64_t x);
static inline char b2hex(char b);

/*
 * Process event.
 *
 * Also it can be a good idea to use cb_cookie for Args, for out or for static
 *	 variable above.
 */

/*
 * print_header_strace -- Print logs header.
 *
 * XXX A blank for human-readable strace-like logs
 */
static void
print_header_strace(int argc, char *const argv[])
{
	if (Args.timestamp)
		fprintf(Out_lf, "%-14s", "TIME(s)");

	fprintf(Out_lf, "%-7s %-6s %4s %3s %s\n",
		"SYSCALL", "PID_TID", "ARG1", "ERR", "PATH");

	(void) argc;
	(void) argv;
}

/*
 * print_event_strace -- Print syscall's log entry.
 *    Single-line mode only.
 *
 * XXX A blank for human-readable strace-like logs
 */
static void
print_event_strace(void *cb_cookie, void *data, int size)
{
	s64 res, err;
	struct ev_dt_t *const event = data;

	/* XXX Check size arg */
	(void) size;

	/* split return value into result and errno */
	res = (event->ret >= 0) ? event->ret : -1;
	err = (event->ret >= 0) ? 0 : -event->ret;

	if (start_ts_nsec == 0)
		start_ts_nsec = event->start_ts_nsec;

	if (Args.failed && (event->ret >= 0))
		return;

	if (Args.timestamp) {
		unsigned long long delta_nsec =
			event->finish_ts_nsec - start_ts_nsec;
		fprintf(Out_lf, "%-14.9f",
				(double)((double)delta_nsec / 1000000000.0));
	}

	if (0 <= event->sc_id)
		fprintf(Out_lf, "%-7s ", sc_num2str(event->sc_id));
	else
		fprintf(Out_lf, "%-7s ", event->sc_name + 4);

	if (0 == event->packet_type)
		/*
		 * XXX Check presence of aux_str by cheking sc_id
		 *    and size arg
		 */
		fprintf(Out_lf, "%-6llu %4lld %3lld %s\n",
				event->pid_tid, res, err, event->aux_str);
	else
		fprintf(Out_lf, "%-6llu %4lld %3lld %s\n",
				event->pid_tid, res, err, event->str);

	(void) cb_cookie;
}

/* ** Hex logs ** */

/*
 * print_header_hex -- This function prints header for hexadecimal logs.
 */
static void
print_header_hex(int argc, char *const argv[])
{
	for (int i = 0; i < argc; i++) {
		if (i + 1 != argc)
			fprintf(Out_lf, "%s%c", argv[i], Args.out_sep_ch);
		else
			fprintf(Out_lf, "%s\n", argv[i]);
	}

	fprintf(Out_lf, "%s%c", "PID_TID", Args.out_sep_ch);

	if (Args.timestamp)
		fprintf(Out_lf, "%s%c", "TIME(nsec)", Args.out_sep_ch);

	fprintf(Out_lf, "%s%c",  "ERR",	 Args.out_sep_ch);
	fprintf(Out_lf, "%s%c",  "RES",	 Args.out_sep_ch);
	fprintf(Out_lf, "%s%c", "SYSCALL", Args.out_sep_ch);

	fprintf(Out_lf, "%s%c", "ARG1", Args.out_sep_ch);
	fprintf(Out_lf, "%s%c", "ARG2", Args.out_sep_ch);
	fprintf(Out_lf, "%s%c", "ARG3", Args.out_sep_ch);
	fprintf(Out_lf, "%s%c", "ARG4", Args.out_sep_ch);
	fprintf(Out_lf, "%s%c", "ARG5", Args.out_sep_ch);
	fprintf(Out_lf, "%s%c", "ARG6", Args.out_sep_ch);

	/* For COMM and like */
	fprintf(Out_lf, "%s", "AUX_DATA");

	fprintf(Out_lf, "\n");
}

/*
 * b2hex -- This function returns character corresponding to hexadecimal digit.
 */
static inline char
b2hex(char b)
{
	switch (b & 0xF) {
	case   0: return '0';
	case   1: return '1';
	case   2: return '2';
	case   3: return '3';
	case   4: return '4';
	case   5: return '5';
	case   6: return '6';
	case   7: return '7';
	case   8: return '8';
	case   9: return '9';
	case 0xA: return 'A';
	case 0xB: return 'B';
	case 0xC: return 'C';
	case 0xD: return 'D';
	case 0xE: return 'E';
	case 0xF: return 'F';
	}

	return '?';
}

/*
 * fprint_i64 -- This function prints 64-bit integer in hexadecimal form
 *     in stream.
 */
static inline void
fprint_i64(FILE *f, uint64_t x)
{
	char str[2 * sizeof(x)];

	const char *const px = (const char *)&x;

	for (unsigned i = 0; i < sizeof(x); i++) {
		str[sizeof(str) - 1 - 2 * i - 0] = b2hex(px[i]);
		str[sizeof(str) - 1 - 2 * i - 1] = b2hex(px[i]>>4);
	}

	fwrite(str, sizeof(str), 1, f);
}

/*
 * sc_num2str -- This function returns syscall's name by number
 */
static inline const char *
sc_num2str(const int64_t sc_num)
{
	int res;
	static char buf[32];

	if ((0 <= sc_num) && (SC_TBL_SIZE > sc_num)) {
		if (NULL == Syscall_array[sc_num].handler_name)
			goto out;

		return Syscall_array[sc_num].handler_name +
			4 /* strlen("sys_") */;
	}

out:
	res = snprintf(buf, sizeof(buf), "sys_%ld", sc_num);

	assert(res > 0);

	return buf;
}

/*
 * print_event_hex -- This function prints syscall's logs entry in stream.
 *
 * WARNING
 *
 *    PLEASE don't use *printf() calls because it will slow down this
 *		 function too much.
 */
static void
print_event_hex(void *cb_cookie, void *data, int size)
{
	s64 res, err;
	struct ev_dt_t *const event = data;

	/* XXX Check size arg */
	(void) size;

	/* split return value into result and errno */
	res = (event->ret >= 0) ? event->ret : -1;
	err = (event->ret >= 0) ? 0 : -event->ret;

	if (start_ts_nsec == 0)
		start_ts_nsec = event->start_ts_nsec;

	if (Args.failed && (event->ret >= 0))
		return;

	fprint_i64(Out_lf, event->pid_tid);
	fwrite(&Args.out_sep_ch, sizeof(Args.out_sep_ch), 1, Out_lf);

	if (Args.timestamp) {
		unsigned long long delta_nsec =
			event->finish_ts_nsec - start_ts_nsec;

		fprint_i64(Out_lf, delta_nsec);
		fwrite(&Args.out_sep_ch, sizeof(Args.out_sep_ch), 1, Out_lf);
	}

	fprint_i64(Out_lf, (uint64_t)err);
	fwrite(&Args.out_sep_ch, sizeof(Args.out_sep_ch), 1, Out_lf);

	fprint_i64(Out_lf, (uint64_t)res);
	fwrite(&Args.out_sep_ch, sizeof(Args.out_sep_ch), 1, Out_lf);

	if (event->sc_id >= 0)
		fwrite(sc_num2str(event->sc_id),
				strlen(sc_num2str(event->sc_id)),
				1, Out_lf);
	else
		fwrite(event->sc_name + 4,
				strlen(event->sc_name + 4),
				1, Out_lf);
	fwrite(&Args.out_sep_ch, sizeof(Args.out_sep_ch), 1, Out_lf);

	/* "ARG1" */
	switch (event->sc_id) {
	case -2:
		fprint_i64(Out_lf, (uint64_t)event->arg_1);
		break;

	case -1:
		/*
		 * XXX Something unexpected happened. Maybe we should issue a
		 * warning or do something better
		 */
		break;

	default:
		if (EM_file == (EM_file & Syscall_array[event->sc_id].masks)) {
			/*
			 * XXX Check presence of string body by cheking sc_id
			 *    and size arg
			 */
			if (0 == event->packet_type)
				fwrite(event->aux_str,
						strlen(event->aux_str),
						1, Out_lf);
			else
				fwrite(event->str, strlen(event->str),
						1, Out_lf);
		} else if (EM_desc == (EM_desc &
					Syscall_array[event->sc_id].masks))
			fprint_i64(Out_lf, (uint64_t)event->arg_1);
		else if (EM_fileat == (EM_fileat &
					Syscall_array[event->sc_id].masks))
			fprint_i64(Out_lf, (uint64_t)event->arg_1);
		else {
			/*
			 * XXX We don't have any idea about this syscall Args.
			 *    May be we should expand our table with additional
			 *    syscall descriptions.
			 */
		}
		break;
	}
	fwrite(&Args.out_sep_ch, sizeof(Args.out_sep_ch), 1, Out_lf);

	/* "ARG2" */
	switch (event->sc_id) {
	case -2:
		fprint_i64(Out_lf, (uint64_t)event->arg_2);
		break;

	case -1:
		/*
		 * XXX Something unexpected happened. Ma be we should issue a
		 * warning or do something better
		 */
		break;

	default:
		if (EM_fileat == (EM_fileat &
					Syscall_array[event->sc_id].masks)) {
			if (0 == event->packet_type)
				/*
				 * XXX Check presence of aux_str by cheking
				 *    sc_id and size arg
				 */
				fwrite(event->aux_str,
						strlen(event->aux_str),
						1, Out_lf);
		}
		break;
	}
	fwrite(&Args.out_sep_ch, sizeof(Args.out_sep_ch), 1, Out_lf);

	/* "ARG3" */
	switch (event->sc_id) {
	case -2:
		fprint_i64(Out_lf, (uint64_t)event->arg_3);
		break;

	case -1:
		/*
		 * XXX Something unexpected happened. Ma be we should issue a
		 * warning or do something better
		 */
		break;

	default:
		break;
	}
	fwrite(&Args.out_sep_ch, sizeof(Args.out_sep_ch), 1, Out_lf);

	/* "ARG4" */
	switch (event->sc_id) {
	case -2:
		fprint_i64(Out_lf, (uint64_t)event->arg_4);
		break;

	case -1:
		/*
		 * XXX Something unexpected happened. Ma be we should issue a
		 * warning or do something better
		 */
		break;

	default:
		break;
	}
	fwrite(&Args.out_sep_ch, sizeof(Args.out_sep_ch), 1, Out_lf);

	/* "ARG5" */
	switch (event->sc_id) {
	case -2:
		fprint_i64(Out_lf, (uint64_t)event->arg_5);
		break;

	case -1:
		/*
		 * XXX Something unexpected happened. Ma be we should issue a
		 * warning or do something better
		 */
		break;

	default:
		break;
	}
	fwrite(&Args.out_sep_ch, sizeof(Args.out_sep_ch), 1, Out_lf);

	/* "ARG6" */
	switch (event->sc_id) {
	case -2:
		fprint_i64(Out_lf, (uint64_t)event->arg_6);
		break;

	case -1:
		/*
		 * XXX Something unexpected happened. May be we should issue a
		 *    warning or do something better
		 */
		break;

	default:
		break;
	}
	fwrite(&Args.out_sep_ch, sizeof(Args.out_sep_ch), 1, Out_lf);

	/* "AUX_DATA". For COMM and like. XXX */
	/* fwrite(event->comm, strlen(event->comm), 1, Out_lf); */
	fwrite("\n", 1, 1, Out_lf);

	(void) cb_cookie;
}

/*
 * print_event_hex_raw -- This function prints syscall's logs entry in stream.
 *
 * WARNING
 *
 *    PLEASE don't use *printf() calls because it will slow down this
 *		 function too much.
 */
static void
print_event_hex_raw(void *cb_cookie, void *data, int size)
{
	print_event_hex(cb_cookie, data, size);
}

/*
 * print_event_hex_sl -- This function prints syscall's logs entry in stream.
 *    This logger should assemble multi-packet data in one line.
 *
 * WARNING
 *
 *    PLEASE don't use *printf() calls because it will slow down this
 *		 function too much.
 */
static void
print_event_hex_sl(void *cb_cookie, void *data, int size)
{
	print_event_hex(cb_cookie, data, size);
}

/* ** Binary logs ** */

/*
 * XXX We should think about writting 64-bit packet number before length
 *    of packet, but there is probabilty that counting packets will be very
 *    expensive, because we run in multi-threading environment.
 */

/*
 * print_header_bin -- This function writes header in stream.
 */
static void
print_header_bin(int argc, char *const argv[])
{
	size_t  argv_size = 0;

	struct ev_dt_t d = { .sc_id = -1 };

	const size_t d_size = sizeof(d);
	d.header.argc = argc;

	/*
	 * here we assume that our command line will not be longer
	 * than 255 bytes
	 */
	for (int i = 0; i < argc; i++) {
		strcpy(d.header.argv + argv_size, argv[i]);
		argv_size += strlen(argv[i]) + 1;
	}

	if (1 != fwrite(&d_size, sizeof(d_size), 1, Out_lf)) {
		/* ERROR */
		Cont = false;
	}

	if (1 != fwrite(&d, sizeof(d), 1, Out_lf)) {
		/* ERROR */
		Cont = false;
	}
}

/*
 * print_event_bin -- This function writes syscall's log entry in stream
 */
static void
print_event_bin(void *cb_cookie, void *data, int size)
{
	struct ev_dt_t *const event = data;

	/* XXX Check size arg */

	if (Args.failed && (event->ret >= 0))
		return;

	if (1 != fwrite(&size, sizeof(size), 1, Out_lf)) {
		/* ERROR */
		Cont = false;
	}

	if (1 != fwrite(data, (size_t)size, 1, Out_lf)) {
		/* ERROR */
		Cont = false;
	}

	(void) cb_cookie;
}

/*
 * out_fmt_str2enum -- This function parsess log's type
 */
enum out_lf_fmt
out_fmt_str2enum(const char *str)
{
	if (!strcasecmp("bin", str) || !strcasecmp("binary", str))
		return EOF_BIN;

	if (!strcasecmp("strace", str))
		return EOF_STRACE;

	if (!strcasecmp("hex", str) || !strcasecmp("hex_raw", str))
		return EOF_HEX_RAW;

	if (!strcasecmp("hex_sl", str))
		return EOF_HEX_SL;

	return EOF_HEX_RAW;
}

perf_reader_raw_cb Print_event_cb[EOF_QTY + 1] = {
	[EOF_HEX_RAW]	= print_event_hex_raw,
	[EOF_HEX_SL]	= print_event_hex_sl,
	[EOF_BIN]	= print_event_bin,
	[EOF_STRACE] = print_event_strace,
};

print_header_t Print_header[EOF_QTY + 1] = {
	[EOF_HEX_RAW]	= print_header_hex,
	[EOF_HEX_SL]	= print_header_hex,
	[EOF_BIN]	= print_header_bin,
	[EOF_STRACE] = print_header_strace,
};
