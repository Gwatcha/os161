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


void proc_table_init(void);

void pid_lock_acquire(pid_t pid);
void pid_lock_release(pid_t pid);
bool pid_lock_do_i_hold(pid_t pid);

/* Returns true if the proc_table contains an entry for pid */
bool proc_table_entry_exists(pid_t pid);

void remove_proc_table_entry(pid_t pid);

/* Returns true if parent is the parent of child */
bool proc_has_child(pid_t parent, pid_t child);

/* Returns error code */
int proc_add_child(pid_t parent, pid_t child);


struct array* proc_get_children(pid_t proc);

/* Returns INVALID_PID if the process does not have a parent */
pid_t proc_get_parent(pid_t proc);

/* Returns the exit status of the child */
int proc_wait_on_child(pid_t parent_pid, pid_t child_pid);

void proc_exit(pid_t proc, int status);

bool proc_has_exited(pid_t proc);


/* Returns INVALID_PID if a pid cannot be reserved */
pid_t reserve_pid(pid_t parent_pid /* may be INVALID_PID */);


#endif /* _PROC_TABLE_H_ */
