/*
 * Assignment 5 process related syscalls, contains implementation of syscalls:
 *     read, sys_write, sys_lseek, sys_close, sys_dup2, and sys_chdir.
 *
 *     written by the infamous dream team - recognized world wide as the
 *     'thetraphandlers',
 *
 *     syscall specs may be found at http://ece.ubc.ca/~os161/man/syscall/, the
 *     only difference in the interface between the man page and these is that
 *     the return value is either 0, indicating success, or an error code,
 *     indicating failure. The actual return value (if existent) is placed in
 *     the first parameter.
 */

#include <types.h>
#include <syscall.h>
#include <copyinout.h>
#include <vfs.h>

#include <mips/trapframe.h>

#include <proc.h>
#include <proc_table.h>
#include <current.h>

#include <limits.h>
#include <kern/limits.h>

#include <kern/errno.h>
#include <kern/fcntl.h>

#include <synch.h>
#include <addrspace.h>

/* ------------------------------------------------------------------------- */
/* Private Utility Functions */

/*
 *   copyinstr_array:
 *       given a char** userptr and a pointer to a char** kbuff this function safely
 *       copies it into kbuff, if it is not possible, it returns an error code.
 *       Inputs:
 *           char **userptr, an array of pointers to string addresses all in user
 *           memory, the array is also in user memory. To be proper, it has to be
 *           terminated by 0. All strings are null terminated, if not EFAULT is
 *           returned. If the total size of userptr exceeds maxcopy, E2BIG is
 *           returned.
 *  
 *           char*** kbuff, pass by reference parameter which is of tpye
 *           char**, (an array of strings). will be written the address of the
 *           memory associated with kbuff, may have size larger
 *           than maxcopy
 * 
 *           int * argc, will be written with the number of arguments, 
 * 
 *           int maxcopy, the max number of bytes to copy before returning E2BIG.
 *       Returns: 0 on success, error code on failure. possible error codes: E2BIG, EFAULT
 *       Postconditions: if return was successful, caller must call kfree(*kbuff)
 *                       and kfree(kbuff) when finished with kbuff. If
 *                       unsucessful, caller is to discard kbuff.
 */
static
int
copyinstr_array(char ** user_ptr, char *** kbuff, int* argc, size_t* kargv_size, int maxcopy) {

	int err = 0;
	/* running total number of bytes copied associated with kbuff string
	 * pointers */
	int addr_bytes_copied = 0;
	/* running total number of bytes copied associated with strings */
	int string_bytes_copied = 0;

	/*
	 * kernel buffer memory is dynamically allocated throughout the copyin of
	 * user_ptr, it starts at 128 bytes (2^7) and is doubled if required,
	 *
	 * this is done for two kernel buffers, one which holds the addresses
	 * of strings (*kbuff), and one which holds all the strings (kstrings)
	 *
	 * *kbuff and kstrings are connected so that *kbuff[i] points to
	 * an address inside kstrings which indicates the beginning of a string.
	 * so *kbuff = &kstrings
	 */

	int initial_buf_size = 128;
	*kbuff = kmalloc(initial_buf_size);
	char * kstrings = kmalloc(initial_buf_size);
	int kbuff_size = initial_buf_size;
	int kstrings_size = initial_buf_size;

	int i = 0; /* used as index for user_ptr[] and *kbuff[] */
	int s = 0; /* keeps track of the next available spot in kstrings */

	/* when this loop exits, we have either encountered an error or have
	 * finished copying user memory. on each iteration, we copyin the next
	 * string's address, and copyin the string for that address. In doing
	 * so, we make sure to break on error or termination, as well as resize
	 * *kbuff and kstring if needed */
	while( true ) {

		/*
		 * copy in string address at user_ptr + i to *kbuff[i]
		 */

		char* temp;
		/* FIXME: fault on first copyin! */
		err = copyin( (const_userptr_t) (user_ptr + i), &temp, sizeof(char*) );
		addr_bytes_copied += sizeof(char*);
		if (err) {
			break;
		}
		if (maxcopy < addr_bytes_copied + string_bytes_copied) {
			err = E2BIG;
			break;
		}

		/* termination case */
		if (temp == 0) {
			break;
		}

		(*kbuff)[i] = temp;

		/* resize kbuff case */
		if (addr_bytes_copied == kbuff_size) {
			char ** koldbuff = *kbuff;
			*kbuff = kmalloc(kbuff_size*2);
			memcpy(*kbuff, koldbuff, kbuff_size);
			kbuff_size *= 2;
		}

		/*
		 * copy in the string at address (*kbuff)[i] into kstrings[s]
		 */

		/* len for copyinstr is just the space left in kstrings*/
		size_t  len = kstrings_size - string_bytes_copied;
		size_t got;
		err = copyinstr( (const_userptr_t) (*kbuff)[i], (kstrings + s), len, &got);
		/* resize kstrings case (loop because we may do succesive resizes for this string) */
		while (err == ENAMETOOLONG) {
			err = 0;
			/* check if too big */
			if ( maxcopy < kstrings_size + addr_bytes_copied ) {
				err = E2BIG;
				break;
			}
			/* resize */
			char * koldstrings = kstrings;
			kstrings = kmalloc(kstrings_size*2);
			memcpy(koldstrings, kstrings, kstrings_size);
			kstrings_size *= 2;

			/* free old */
			kfree(koldstrings);

			/* retry copyinstr */
			len = kstrings_size - string_bytes_copied;
			err = copyinstr( (const_userptr_t) user_ptr[i], (kstrings + s), len, &got);
			if (err == EFAULT) {
				break;
			}
		}

		if (err == EFAULT) {
			break;
		}

		/* we have successively copied the next string in */
		string_bytes_copied += got;
		s += got;

		/* store this strings length in (*kbuff)[i], this is to make it easier to
		   set up all the pointers from *kbuff to kstrings afterwards. */
		(*kbuff)[i] = (char*) got;

		if (maxcopy < addr_bytes_copied + string_bytes_copied) {
			err = E2BIG;
			break;
		}

		i += 1;
	}

	/* clean up code in case of error */
	if (err) {
		kfree(*kbuff);
		kfree(kstrings);
		return err;
	}

	/* lastly, set up *kbuff to point to the strings in kstrings
	   note that i is equal to the end of kbuff + 1 */
	s = 0; /* used as index into kstrings */
	for (int j = 0; j < i; j++) {
		s += (int) (*kbuff)[j]; /* both 32 bits */
		(*kbuff)[j] = &kstrings[s];
	}

	/* if we are here, it is an invariant that (*kbuff)[i] == user_ptr[i] for i up to
	   and including the index with 0 */
	/* DEBUG ONLY: check the invariant */
	// for (unsigned int i = 0; kbuff[i] != 0; i++) {
	//         char * temp;
	//         copyin( (const_userptr_t) user_ptr[i], &temp, sizeof(char*) );
	//         KASSERT(kbuff[i] == temp);
	// }

	*argc = i; 
	*kargv_size = addr_bytes_copied + string_bytes_copied; 
	return 0;
}

static
void
enter_forked_process_wrapper(void* data1, unsigned long data2) {
	(void)data2;
	enter_forked_process((struct trapframe*)data1);
}

/* Private Utility Functions */
/* ------------------------------------------------------------------------- */
/* System calls */
int sys_execv(const char *program, char **argv) {

	/*
	 * May return these errors:
	 * ENODEV       The device prefix of program did not exist.
	 * ENOTDIR      A non-final component of program was not a directory.
	 * ENOENT       program did not exist.
	 * EISDIR       program is a directory.
	 * ENOEXEC      program is not in a recognizable executable file format, was for the wrong platform, or contained invalid fields.
	 * ENOMEM       Insufficient virtual memory is available.
	 * E2BIG        The total size of the argument strings exceeeds ARG_MAX.
	 * EIO          A hard I/O error occurred.
	 * EFAULT       One of the arguments is an invalid pointer.
	 */

	int err = 0;

	/* deactivate address space since it will be in a volatile state, in
	 * DUMBVIM, this does nothing */
	struct addrspace * old_as = proc_getas();
	struct addrspace * new_as = NULL; /* null for now */
	as_deactivate();

	/* ---------------------------------------------------------------- */

	/*
	 * 1. Copy arguments (argv) into a kernel buffer, kargv, and program
	 * name into kprogram
	 */

	int argc;
	size_t kargv_size; 
	char ** kargv;
	err = copyinstr_array(argv, &kargv, &argc, &kargv_size, ARG_MAX);
	if (err) {
		/* no need deallocate kargv on copyinstr_array err  */
		as_activate();
		return err;
	}

	size_t got;
	char * kprogram = kmalloc(NAME_MAX);
	err = copyinstr((const_userptr_t)program, kprogram, NAME_MAX, &got);
	if (err) {
		goto err;
	}


	/*
	 * 2. Create new address space
	 */

	new_as = as_create();
	if (new_as == NULL) {
		err = ENOMEM;
		goto err;
	}

	/*
	 * Switch to new address space
	 */

	proc_setas(new_as);

	/*
	 * Load new executable
	 */

	/* Open the file. */
	struct vnode * v;
	err = vfs_open(kprogram, O_RDONLY, 0, &v);
	if (err) {
		goto err;
	}

	vaddr_t entrypoint;
	err = load_elf(v, &entrypoint);
	if (err) {
		vfs_close(v);
		goto err;
	}

	/*
	 * Define new stack region
	 */

	/* Define the user stack in the new address space */
	vaddr_t stackptr;
	err = as_define_stack(new_as, &stackptr);
	if (err) {
		vfs_close(v);
		goto err;
	}

	/*
	 * Copy arguments to new address space, properly arranging them.
	 * Arguments are pointers in registers into user space.
	 *         - a0: argc
	 *         - a1: char** pointer that points to a string array of length argc
	 * we chose to store argc and the program name on the stack, so first
	 * we need to make space on it. Since the stackpointer is subtract then
	 * store, it is currently at the highest address + 1 in the stack
	 * pointer area, so we just subtract the number of bytes read from userspace.
	 */

	stackptr -= kargv_size;
	err = copyout( kargv, (userptr_t) stackptr, kargv_size);
	if (err) {
		vfs_close(v);
		goto err;
	}

	/*
	 * Clean up old address space
	 */

	as_destroy(old_as); /* the point of no return...  */

	/*
	 * 8. Warp to user mode
	 */

	/* clean up before doing so */
	kfree(*kargv);
	kfree(kargv);
	kfree(kprogram);
	vfs_close(v);

	as_activate();
	enter_new_process(argc, (userptr_t) stackptr /*userspace addr of argv*/,
			NULL /*userspace addr of environment*/,
			stackptr, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;

err:
	kfree(*kargv); 
	kfree(kargv);
	kfree(kprogram);

	/* clean up address space */
	proc_setas(old_as);
	as_destroy(new_as); /* DUMBVM handles the case where new_as is NULL */
	as_activate();
	return err;
}

int
sys_fork(pid_t* retval, struct trapframe* trapframe) {

	/* Errors:
	 * EMPROC  The current user already has too many processes.
	 * ENPROC  There are already too many processes on the system.
	 * ENOMEM  Sufficient virtual memory for the new process was not available.
	 */

        const pid_t curpid = curproc->p_pid;

        pid_lock_acquire(curpid);

        KASSERTM(proc_table_entry_exists(curpid), "pid %d", curpid);

	/* Find an unused pid for the child */
        const pid_t child_pid = reserve_pid(curpid);
        if (child_pid == INVALID_PID) {
                pid_lock_release(curpid);
                return ENPROC;
        }

        DEBUG(DB_PROC_TABLE, "fork %d -> %d\n", curpid, child_pid);

        /* Create child process with proc_create */
        struct proc* child_proc = proc_create(curproc->p_name, child_pid);

        /* Add the child's pid to the parent's list of children */
        int error = proc_add_child(curpid, child_pid);
        if (error) {
                pid_lock_release(curpid);
                return error;
        }

        /* Copy the address space */
        as_copy(curproc->p_addrspace, &child_proc->p_addrspace);

        /* Copy the file table */
        file_table_copy(&curproc->p_file_table, &child_proc->p_file_table);

	/* TODO: Copy threads */

	/* TODO: Create kernel thread */

	/* Create a copy of the trapframe for the child */
	struct trapframe* tf_copy = kmalloc(sizeof(struct trapframe));
	memcpy(tf_copy, trapframe, sizeof(struct trapframe));

        /* Fork the child process */
        thread_fork("child", child_proc, &enter_forked_process_wrapper, tf_copy, 0);

        *retval = child_proc->p_pid;

        pid_lock_release(curpid);

	return 0;
}

int
sys_getpid(pid_t* retval) {
	*retval = curproc->p_pid;
	return 0;
}

int
sys_waitpid(pid_t* retval, pid_t pid, int *status, int options) {

	/* Errors:
	 * EINVAL  The options argument requested invalid or unsupported options.
	 * ECHILD  The pid argument named a process that was not a child of the current process.
	 * ESRCH   The pid argument named a nonexistent process.
	 * EFAULT  The status argument was an invalid pointer.
	 */

        if (retval != NULL) {
                *retval = pid;
        }
        if (status != NULL) {
                *status = 0;
        }

        /* TODO: use copy_in/copy_out */
        /* if (status is invalid) { */
        /*         return EFAULT; */
        /* } */

        if (options != 0) {
                return EINVAL;
        }

        if (!proc_table_entry_exists(pid)) {
                return ESRCH;
        }

        pid_t curpid = curproc->p_pid;

        DEBUG(DB_PROC_TABLE, "wait %d on %d\n", curpid, pid);

        pid_lock_acquire(pid);

        if (!proc_has_child(curpid, pid)) {
                pid_lock_release(pid);
                return ECHILD;
        }

        const int exit_status = proc_wait_on_pid(pid);
        if (status != NULL) {
                *status = exit_status;
        }

        pid_lock_release(pid);

        DEBUG(DB_PROC_TABLE, "done wait %d on %d\n", curpid, pid);

	return 0;
}

void
sys__exit(int exitcode) {

	const pid_t curpid = curproc->p_pid;

        DEBUG(DB_PROC_TABLE, "exit %d\n", curpid);

	KASSERTM(proc_table_entry_exists(curpid), "pid %d", curpid);

        const pid_t parent_pid = proc_get_parent(curpid);
        if (parent_pid != INVALID_PID) {
                pid_lock_acquire(parent_pid);
        }
        pid_lock_acquire(curpid);

        struct array* child_pids = proc_get_children(curpid);

	/* Clean up proc_table_entry of each child that has already exited */
	unsigned num_children = child_pids->num;
	for (unsigned i = 0; i < num_children; ++i) {

		pid_t child_pid = (pid_t)array_get(child_pids, i);

                DEBUG(DB_PROC_TABLE, "%d examining child %d\n", curpid, child_pid);

                pid_lock_acquire(child_pid);

		/* child's proc table entry should exist until its parent exits */
                KASSERTM(proc_table_entry_exists(child_pid), "pid %d", child_pid);

		/* this process should be the parent of its children */
                KASSERTM(proc_get_parent(child_pid) == curpid, "pid %d", child_pid);

                if (proc_has_exited(child_pid)) {

                        DEBUG(DB_PROC_TABLE, "remove_proc_table_entry(%d) - child\n", child_pid);
                        remove_proc_table_entry(child_pid);
                }

                pid_lock_release(child_pid);
        }

        proc_exit(curpid, exitcode);

        enum parent_status {
                ps_invalid,
                ps_no_entry,
                ps_has_exited,
                ps_pid_recycled,
                ps_alive,
                ps_count
        };

        enum parent_status pstatus =
                parent_pid == INVALID_PID               ? ps_invalid :
                !proc_table_entry_exists(parent_pid)    ? ps_no_entry :
                proc_has_exited(parent_pid)             ? ps_has_exited :
                !proc_has_child(parent_pid, curpid)     ? ps_pid_recycled :
                                                          ps_alive;

        if (pstatus != ps_alive) {

                /* Destroy this entry; there is no living parent that may call waitpid */
		remove_proc_table_entry(curpid);

                const char* db_format[ps_count] = {
                        "remove_proc_table_entry(%d), parent %d is invalid\n",
                        "remove_proc_table_entry(%d), parent %d has no entry\n",
                        "remove_proc_table_entry(%d), parent %d has exited\n",
                        "remove_proc_table_entry(%d), parent %d has been recycled\n",
                        NULL
                };

                DEBUG(DB_PROC_TABLE, db_format[pstatus], curpid, parent_pid);
        }

        pid_lock_release(curpid);
        if (parent_pid != INVALID_PID) {
                pid_lock_release(parent_pid);
        }

        /* Print in reverse (easy way to tell where the call began and ended) */
        DEBUG(DB_PROC_TABLE, "%d exit\n", curpid);

        thread_exit_destroy_proc();
}
