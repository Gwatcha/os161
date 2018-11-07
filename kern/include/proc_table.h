/*
 * Copyright (c) 2018
 *	The Trap Handlers!
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _PROC_TABLE_H_
#define _PROC_TABLE_H_

/*
 * Operations on the process table (used in proc.c and proc_syscall.c)
 */
#include "types.h"

#define INVALID_PID -1

/* struct process_table_entry { */
/* 	struct cv* pte_waitpid_cv; */
/* 	struct array pte_child_pids; */
/* 	pid_t pte_parent_pid; */
/* 	bool pte_has_exited; */
/* 	int pte_exit_status; */
/* 	/\* int pte_refcount; *\/ */
/* }; */

void proc_table_init(void);

void pid_lock_acquire(pid_t pid);
void pid_lock_release(pid_t pid);

/* Returns true if the proc_table contains an entry for pid */
bool proc_table_entry_exists(pid_t pid);

/* Returns true if parent is the parent of child */
bool proc_has_child(pid_t parent, pid_t child);

/* Returns INVALID_PID if a pid cannot be reserved */
pid_t reserve_pid(const pid_t* parent_pid /* may be NULL */);


#endif /* _PROC_TABLE_H_ */
