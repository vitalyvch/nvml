/*
 * Copyright 2017, Intel Corporation
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
 * ebpf_file_set.c -- A set of ebpf files
 */

#include "ebpf_file_set.h"

const char *ebpf_trace_h_file = "trace.h";

const char *ebpf_head_file = "trace_head.c";
const char *ebpf_libc_tmpl_file = "trace_libc_tmpl.c";
const char *ebpf_file_tmpl_file = "trace_file_tmpl.c";
const char *ebpf_fileat_tmpl_file = "trace_fileat_tmpl.c";
const char *ebpf_fs_path_1_2_arg_tmpl_file = "trace_fs_path_1_2_arg_tmpl.c";
const char *ebpf_fs_path_1_3_arg_tmpl_file = "trace_fs_path_1_3_arg_tmpl.c";
const char *ebpf_fs_path_2_4_arg_tmpl_file = "trace_fs_path_2_4_arg_tmpl.c";
const char *ebpf_fork_tmpl_file = "trace_fork_tmpl.c";
const char *ebpf_vfork_tmpl_file = "trace_vfork_tmpl.c";
const char *ebpf_clone_tmpl_file = "trace_clone_tmpl.c";
const char *ebpf_kern_tmpl_file = "trace_kern_tmpl.c";

const char *ebpf_tp_all_file = "trace_tp_all.c";

const char *ebpf_pid_check_ff_disabled_hook_file =
		"pid_check_ff_disabled_hook.c";
const char *ebpf_pid_check_ff_fast_hook_file =
		"pid_check_ff_fast_hook.c";
const char *ebpf_pid_check_ff_full_hook_file =
		"pid_check_ff_full_hook.c";
