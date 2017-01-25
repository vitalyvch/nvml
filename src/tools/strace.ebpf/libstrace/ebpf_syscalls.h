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
 * ebpf_syscalls.h -- a list of glibc-supported syscalls.
 */

#ifndef EBPF_SYSCALLS_H
#define EBPF_SYSCALLS_H

#include <sys/syscall.h>

/* XXX Subject to re-develop */
enum masks_t {
	/* syscall has fs path as a first arg */
	EM_fs_path_1_arg = 1 << 0,
	/* syscall has fs path as second arg */
	EM_fs_path_2_arg = 1 << 1,
	/* syscall has fs path as third arg */
	EM_fs_path_3_arg = 1 << 2,
	/* syscall has fs path as fourth arg */
	EM_fs_path_4_arg = 1 << 3,
	/* syscall has fs path as fifth arg. For future syscalls. */
	EM_fs_path_5_arg = 1 << 4,
	/* syscall has fs path as sixth arg. For future syscalls. */
	EM_fs_path_6_arg = 1 << 5,

	/* syscall fd path as a first arg */
	EM_fd_1_arg = 1 << 6,
	/* syscall fd path as second arg */
	EM_fd_2_arg = 1 << 7,
	/* syscall fd path as third arg */
	EM_fd_3_arg = 1 << 8,
	/* syscall fd path as fourth arg */
	EM_fd_4_arg = 1 << 9,
	/* syscall fd path as fifth arg. For future syscalls. */
	EM_fd_5_arg = 1 << 10,
	/* syscall fd path as sixth arg. For future syscalls. */
	EM_fd_6_arg = 1 << 11,

	/* syscall returns a fd */
	EM_rdesc = 1 << 12,
	/* syscall returns a pid */
	EM_rpid = 1 << 13,
	/* syscall returns a ptr */
	EM_rptr = 1 << 14,

	/* syscall should be traced in 'kern-all' mode only */
	EM_kern_all = 1 << 15,
	/* syscall should be traced in 'kern-all' and 'libc-all' modes */
	EM_libc_all = 1 << 16,
	/*
	 * syscall should be traced in 'kern-all', 'libc-all' and 'fileio'
	 *    modes
	 */
	EM_fileio = 1 << 17,


	/* syscall accepts fd as a first arg */
	EM_desc = EM_fd_1_arg,
	/* syscall accepts fs path as a first arg */
	EM_file = EM_fs_path_1_arg,
	/* syscall accepts dir fd as a first arg and path as a second */
	EM_fileat = EM_fd_1_arg | EM_fs_path_2_arg,
	/* syscall has fs paths as first and second args. rename() */
	EM_fs_path_1_2_arg = EM_fs_path_1_arg | EM_fs_path_2_arg,
	/* syscall has fs paths as first and third args. linkat() */
	EM_fs_path_1_3_arg = EM_fs_path_1_arg | EM_fs_path_3_arg,
	/* syscall has fs paths as second and forth args. renameat() */
	EM_fs_path_2_4_arg = EM_fs_path_2_arg | EM_fs_path_4_arg,

	EM_ALL = -1,
};

/* Properties of syscall with number 'num' */
struct syscall_descriptor {
	/* Number of syscall */
	unsigned num;
	/* Number of syscall as string */
	const char *num_name;
	/* The name of in-kernel syscall's handler */
	const char *handler_name;
	/* Flags */
	unsigned masks;
};

/* Currently glibc does not have appropriate macro for it */
enum { SC_TBL_SIZE = 1024 };
extern struct syscall_descriptor syscall_array[SC_TBL_SIZE];

#endif /* EBPF_SYSCALLS_H */
