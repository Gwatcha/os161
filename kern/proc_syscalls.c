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
 *     indicating failure. The actual return value is placed in the first
 *     parameter.
 */

#include <types.h>
#include <syscall.h>
#include <copyinout.h>

#include <proc.h>
#include <current.h>

#include <limits.h>

#include <kern/errno.h>


int  
sys_execv(const char *program, char **args) {
	(void) program;
   	(void) *args;
	return 0;
}	

int 
sys_fork(pid_t* retval) {
	(void)  retval;
	return 0;
}

int 
sys_getpid(pid_t* retval) {
	(void) retval;
	return 0;
}

int 
sys_waitpid(pid_t* retval, pid_t pid, int *status, int options) {
	(void) retval;
   	(void) pid;
   	(void) *status;
   	(void) options;
	return 0;
}

int 
sys__exit(int exitcode) {
	(void) exitcode;
	return 0;
}
