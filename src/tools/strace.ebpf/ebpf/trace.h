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
 * trace.h -- Data exchange packet between packet filter and reader callback
 */


#ifndef TRACE_H
#define TRACE_H

/*
 * The longest syscall's name is equal to 26 characters:
 *    'SyS_sched_get_priority_max'.
 * Let's to add a space for '\0' and few extra bytes.
 */
enum { E_SC_NAME_SIZE = 32 };

struct ev_dt_t {
	/*
	 * This fild is set for glibc-defined syscalls and describe
	 *    a series of packets for every syscall.
	 *
	 * It is needed because we are limited with stack size of
	 *    512 bytes and used part of stack is initilaized with zeros
	 *    on every call of syscall handlers.
	 *
	 * the value equals to -1 means "header"
	 *
	 * the value equals to 0 means that there will be no additional
	 *    packets sent for this syscall.
	 * the value equals to 1 means that this first packet and there
	 *    will be sent 1 more additional packet.
	 * the value equals to 2 means that this first packet and there
	 *    will be sent 2 more additional packet.
	 * the value equals to 3 means that this first packet and there
	 *    will be sent 3 more additional packet.
	 *
	 * the value equals to 11 means that this first additional packet
	 * the value equals to 12 means that this second additional packet
	 * the value equals to 13 means that this third additional packet
	 *
	 * Content of additional packets is defined by syscall number in
	 *    first packet. There are no additional packets for "sc_id == -2"
	 */
	s64 packet_type;

	/*
	 * Syscall's signature. All packets with same signature belongs to same
	 *    call of same syscall. We need two timestamps here, because we
	 *    can get nesting of syscalls from same pid_tid by calling syscall
	 *    from signal handler, before syscall called from main context has
	 *    returned.
	 */
	struct {
		u64 pid_tid;

		/* Timestamps */
		u64 start_ts_nsec;
		u64 finish_ts_nsec;
	};

	union {
		/* Body of first packet */
		struct {
			/*
			 * the value equals to -2 means that syscall's num is
			 *    unknown for glibc and the field sc_name should be
			 *    used to figuring out syscall.
			 */
			s64 sc_id;

			s64 ret;

			s64 arg_1;
			s64 arg_2;
			s64 arg_3;
			s64 arg_4;
			s64 arg_5;
			s64 arg_6;

			/* should be last in this structure */
			char sc_name[E_SC_NAME_SIZE];
		};

		/* Body of header */
		struct {
			s64 argc;
			char argv[];
		} header;

		/*
		 * Body of string argument. The content and meaning of argument
		 *    is defined by syscall's number in the first packet.
		 */
		char str[1];	/* NAME_MAX */
	};
};

#endif /* TRACE_H */
