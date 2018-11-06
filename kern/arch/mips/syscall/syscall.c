/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
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

#include <types.h>
#include <kern/errno.h>
#include <kern/syscall.h>
#include <lib.h>
#include <mips/trapframe.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>
#include <copyinout.h>


/*
 * System call dispatcher.
 *
 * A pointer to the trapframe created during exception entry (in
 * exception-*.S) is passed in.
 *
 * The calling conventions for syscalls are as follows: Like ordinary
 * function calls, the first 4 32-bit arguments are passed in the 4
 * argument registers a0-a3. 64-bit arguments are passed in *aligned*
 * pairs of registers, that is, either a0/a1 or a2/a3. This means that
 * if the first argument is 32-bit and the second is 64-bit, a1 is
 * unused.
 *
 * This much is the same as the calling conventions for ordinary
 * function calls. In addition, the system call number is passed in
 * the v0 register.
 *
 * On successful return, the return value is passed back in the v0
 * register, or v0 and v1 if 64-bit. This is also like an ordinary
 * function call, and additionally the a3 register is also set to 0 to
 * indicate success.
 *
 * On an error return, the error code is passed back in the v0
 * register, and the a3 register is set to 1 to indicate failure.
 * (Userlevel code takes care of storing the error code in errno and
 * returning the value -1 from the actual userlevel syscall function.
 * See src/user/lib/libc/arch/mips/syscalls-mips.S and related files.)
 *
 * Upon syscall return the program counter stored in the trapframe
 * must be incremented by one instruction; otherwise the exception
 * return code will restart the "syscall" instruction and the system
 * call will repeat forever.
 *
 * If you run out of registers (which happens quickly with 64-bit
 * values) further arguments must be fetched from the user-level
 * stack, starting at sp+16 to skip over the slots for the
 * registerized values, with copyin().
 */
void
syscall(struct trapframe *tf)
{
	int callno;
	int32_t retval; 
	int err;

	KASSERT(curthread != NULL);
	KASSERT(curthread->t_curspl == 0);
	KASSERT(curthread->t_iplhigh_count == 0);

	callno = tf->tf_v0;

	/*
	 * Initialize retval to 0. Many of the system calls don't
	 * really return a value, just 0 for success and -1 on
	 * error. Since retval is the value returned on success,
	 * initialize it to 0 by default; thus it's not necessary to
	 * deal with it except for calls that return other values,
	 * like write.
	 */

	retval = 0;


	/*
	 * This switch stament is responsible for calling the syscall given the
	 * callno and mediating between the system call and the input/output API
	 * for syscalls, which may be complicated due to 32 bit register
	 * complications (eg. lseek has some parameters on user stack)
	 */
	
	switch (callno) {
	    case SYS_reboot:
		err = sys_reboot(tf->tf_a0);
		break;

	    case SYS___time:
		err = sys___time((userptr_t)tf->tf_a0,
				 (userptr_t)tf->tf_a1);
		break;

		/*
		 * fd related syscalls
		 */

		case SYS_open:
			err = sys_open(&retval, (char*)tf->tf_a0, (int)tf->tf_a1);
			break;
		case SYS_read:
			err = sys_read(&retval, (int)tf->tf_a0, (void*)tf->tf_a1, (size_t)tf->tf_a2);
			break;

		case SYS_write:
			err = sys_write(&retval, (int)tf->tf_a0, (void*)tf->tf_a1, (size_t)tf->tf_a2);
			break;

		case SYS_lseek: 
		{
			/* kwhence is on the user stack, so we must copy it in */
			int kwhence;
			copyin((userptr_t) (tf->tf_sp + 16), &kwhence, 4); 

			/* now we join 2 32bit registers, a2 & a3, which represent the 64bit value offset */
			off_t kpos = ((off_t) tf->tf_a2 << 32) | ((off_t) tf->tf_a3);

			/* our return value is 64 bits as well, so we need to store it in v0 and v1 */
			/* the wrap up code handles v0 for us, we need to store v1. */
			off_t retval_ext;
			err = sys_lseek(&retval_ext, (int)tf->tf_a0, kpos, kwhence);

			retval = (int32_t) ((retval_ext) >> 32);
			tf->tf_v1 = (int32_t) (retval_ext & 0x00000000ffffffff);

			break;
		}
		case SYS_close:
			err = sys_close((int)tf->tf_a0);
			break;

		case SYS_dup2:
			err = sys_dup2(&retval, (int)tf->tf_a0, (int)tf->tf_a1);
			break;

		case SYS_chdir:
			err = sys_chdir((char*)tf->tf_a0);
			break;

		case SYS___getcwd:
			err = sys___getcwd(&retval, (char*)tf->tf_a0, (size_t)tf->tf_a1);
			break;

		/*
		 * Process related syscalls
		 */

		case SYS_fork: /* parent returns here */
                        err = sys_fork(&retval, tf);
			break;

		case SYS_execv: 
			err = sys_execv((const char*)tf->tf_a0, (char **)tf->tf_a1); 
			break;

		case SYS__exit:
                        sys__exit((int)tf->tf_a0);
			break;

		case SYS_waitpid:
			err = sys_waitpid(&retval, (pid_t)tf->tf_a0, (int *)tf->tf_a1, (int)tf->tf_a2);
			break;

		case SYS_getpid:
			err = sys_getpid(&retval);
			break;

	    default:
		kprintf("Unknown syscall %d\n", callno);
		err = ENOSYS;
		break;
	}


	if (err) {
		/*
		 * Return the error code. This gets converted at
		 * userlevel to a return value of -1 and the error
		 * code in errno.
		 */
		tf->tf_v0 = err;
		tf->tf_a3 = 1;      /* signal an error */
	}
	else {
		/* Success. */
		tf->tf_v0 = retval;
		tf->tf_a3 = 0;      /* signal no error */
	}

	/*
	 * Now, advance the program counter, to avoid restarting
	 * the syscall over and over again.
	 */

	tf->tf_epc += 4;

	/* Make sure the syscall code didn't forget to lower spl */
	KASSERT(curthread->t_curspl == 0);
	/* ...or leak any spinlocks */
	KASSERT(curthread->t_iplhigh_count == 0);
}

/*
 * Enter user mode for a newly forked process.
 *
 * This function is provided as a reminder. You need to write
 * both it and the code that calls it.
 *
 * Thus, you can trash it and do things another way if you prefer.
 */
void
enter_forked_process(struct trapframe *copy_of_parent_tf)
{
        struct trapframe child_tf;
        memcpy(&child_tf, copy_of_parent_tf, sizeof(struct trapframe));

        /* Free the copy of the parent's trapframe */
        kfree(copy_of_parent_tf);

        /* Fix the trapframe */
        child_tf.tf_v0 = 0;   /* Return value of 0 */
        child_tf.tf_a3 = 0;   /* No error */
        child_tf.tf_epc += 4; /* Advance the program counter */

        /* kprintf("Fixed up trap frame!"); */
        /* while(true); */

        mips_usermode(&child_tf);
}
