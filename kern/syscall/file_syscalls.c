#include <types.h>
#include <syscall.h>
#include <copyinout.h>

#include <uio.h>

#include <proc.h>
#include <current.h>

#include <limits.h>

#include <vnode.h>
#include <vfs.h>
#include <stat.h>

#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/seek.h>

/*
 * See manpages at http://ece.ubc.ca/~os161/man/syscall/ for a description of these calls
 * All syscalls return 0 on success, error code otherwise.
 * Other return values are stored in the *out parameter.
 */

/* On success, open returns a nonnegative file handle. On error, -1 is returned,
   and errno is set according to the error encountered. */
int
sys_open(int* retval, const char *filename, int flags)
{
	/*
         * Errors
	 * ENODEV 	The device prefix of filename did not exist.
	 * ENOTDIR	A non-final component of filename was not a directory.
	 * ENOENT	A non-final component of filename did not exist.
	 * ENOENT	The named file does not exist, and O_CREAT was not specified.
	 * EEXIST	The named file exists, and O_EXCL was specified.
	 * EISDIR	The named object is a directory, and it was to be opened for writing.
	 * EMFILE	The process's file table was full, or a process-specific limit on open files was reached.
	 * ENFILE	The system file table is full, if such a thing exists, or a system-wide limit on open files was reached.
	 * ENXIO	The named object is a block device with no filesystem mounted on it.
	 * ENOSPC	The file was to be created, and the filesystem involved is full.
	 * EINVAL	flags contained invalid values.
	 * EIO	A hard I/O error occurred.
	 * EFAULT	filename was an invalid pointer.
	 */

        /* return invalid file descriptor by default */
        *retval = -1;

        struct file_table_entry** file_table = curproc->p_file_table;

	/* safely copy in the user specified path */
        char kbuffer[PATH_MAX];
	size_t * got = NULL;
	int error;
	error = copyinstr((const_userptr_t) filename, kbuffer, PATH_MAX, got);
	if (error) { /* may return ENAMETOOLONG or EFAULT */
		return error;
	}

	/* acquire next open file descriptor */
        int fd = 3; /* skip std fd's */
        for (; ; ++fd) {
                if (fd >= __OPEN_MAX) {
                        return  EMFILE;
                }
                if (file_table[fd] == NULL) {
                        break;
                }
        }

	/* Create the fd's vnode through vfs_open. */
        struct vnode* file_vnode;
        error = vfs_open(kbuffer, flags, 0, &file_vnode);
	if (error) {
		return error;
	}

	/* Create a file table entry at fd with file_node and specified flags */
        file_table[fd] = file_table_entry_create(flags, file_vnode);

        *retval = fd;
	return 0;
}


/*
 * sys_read:
 *     given a fd, reads up to buflen bytes from the file specified by fd, at
 *     the location in the file specified by the current seek position of the
 *     file, and stores them in the space pointed to by buf.
 *
 *     Preconditions:  fd entry must exist in table and be openned for reading,
 *                     otherwise returns EBADF. If a vnode exists, it must be a valid vnode, else
 *                     kernel panics. if part or all of address space pointed to by buf is
 *                     invalid, returns EFAULT. The vnode can not be a directory or a symlink.
 *     Postconditions: the offset for the fd is advanced by the number of bytes
 *                     read and buf contains the data
 */
int
sys_read(ssize_t * retval, int fd, void* buf, size_t buflen)
{
	/*
	 * Possible Errors
	 *    +    EBADF	fd is not a valid file descriptor, or was not opened for reading.
	 *    +    EFAULT	Part or all of the address space pointed to by buf is invalid.
	 *    +    EIO		A hardware I/O error occurred reading the data.
	 */


        /* bad fd checks */
        if (fd < 0 || fd >= __OPEN_MAX) {
    		return EBADF;
        }

        /* Obtain user process' open file table */
        struct file_table_entry** file_table = curproc->p_file_table;

        if (file_table[fd] == NULL) {
		return EBADF;
        }

        if (file_table[fd]->open_flags & O_WRONLY) {
                /* The file was opened in writeonly mode and cannot be read */
                return EBADF;
        }

        if (buf == NULL) {
    		return EFAULT;
        }

        lock_acquire(file_table[fd]->fte_lock);

        /* acquire file info */
        off_t offset = file_table[fd]->offset;
        struct vnode * file = file_table[fd]->vnode;

        /* Initialize a uio suitable for I/O from a kernel buffer. */
        char kbuf[buflen];
        struct iovec iov;
        struct uio u;
        uio_kinit(&iov, &u, kbuf, buflen, offset, UIO_READ);

        /* read from the file */
        int error = VOP_READ(file, &u);
        if (error) {
                /* handles EIO */
                goto exit;
        }
        /* safely copy the data into the user buffer */
        error = copyout(kbuf, buf, buflen);
        if (error) {
                /* handles EFAULT */
                goto exit;
        }

        /* advance seek position */
        file_table[fd]->offset += (buflen - u.uio_resid);

        /* return number of bytes read */
        *retval = (buflen - u.uio_resid);

 exit:
        lock_release(file_table[fd]->fte_lock);
        return error;
}

/*
 * sys_write:
 *     given a fd, writes up to nbytes bytes to the file specified by fd, at
 *     the location in the file specified by the current seek position of the
 *     file. Returns number of bytes written.
 *
 *     Preconditions: fd entry must exist in table and be openned for writing,
 *                     otherwise returns EBADF. If a vnode exists, it must be a valid vnode, else
 *                     kernel panics. if part or all of address space pointed to by buf is
 *                     invalid, returns EFAULT. The vnode can not be a directory or a symlink.
 *     Postconditions: the offset for the fd is advanced by the number of bytes
 *                     read and buf contains the data
 */
int
sys_write(ssize_t *retval, int fd, const void *buf, size_t nbytes)
{
        /*
         *  Possible Errors
         *  +  EBADF	fd is not a valid file descriptor, or was not opened for writing.
         *  +  EFAULT	Part or all of the address space pointed to by buf is invalid.
         *  +  ENOSPC	There is no free space remaining on the filesystem containing the file.
         *  +  EIO	A hardware I/O error occurred writing the data.
         */

        /* bad fd checks */
        if (fd < 0 || fd >= __OPEN_MAX) {
                return EBADF;
        }

        /* Obtain user process' open file table */
        struct file_table_entry** file_table = curproc->p_file_table;

        if (file_table[fd] == NULL) {
                return EBADF;
        }

        if ((file_table[fd]->open_flags & O_ACCMODE) == O_RDONLY) {
                /* The file was opened in readonly mode and should not be written to */
                return EBADF;
        }

        lock_acquire(file_table[fd]->fte_lock);

        /* acquire file info */
        off_t offset = file_table[fd]->offset;
        struct vnode * file = file_table[fd]->vnode;

        /* copy the user data in */
        char kbuf[nbytes];
        int error = copyin(buf, kbuf, nbytes);
        if (error) {
                /* handles EFAULT */
                goto exit;
        }

        /* Initialize a uio suitable for I/O from a kernel buffer. */
        struct iovec iov;
        struct uio u;
        uio_kinit(&iov, &u, kbuf, nbytes, offset, UIO_WRITE);

        /* write to the file */
        error = VOP_WRITE(file, &u);
        if (error) {
                goto exit;
        }

        /* advance seek position */
        file_table[fd]->offset += (nbytes - u.uio_resid);

        /* return number of bytes written */
        *retval = (nbytes - u.uio_resid);

 exit:
        lock_release(file_table[fd]->fte_lock);
        return error;

}

int
sys_lseek(off_t *retval, int fd, off_t pos, int whence)
{
        /*
         *  Possible Errors
         *  +  EBADF	fd is not a valid file handle.
         *  +  ESPIPE	fd refers to an object which does not support seeking. **
         *  +  EINVAL	whence is invalid.
         *  +  EINVAL	The resulting seek position would be negative.
         */

        struct file_table_entry** file_table = curproc->p_file_table;

        /* bad fd checks */
        if (fd < 0 || __OPEN_MAX <= fd) {
                return EBADF;
        }

        if (file_table[fd] == NULL) {
                return EBADF;
        }

        /* is seekable check */
        if (!VOP_ISSEEKABLE(file_table[fd]->vnode)) {
                return ESPIPE;
        }

        /* a stat buf is needed in case we need a files size */
        struct stat statbuf;
        int error;
        off_t new_cursor;

        /* Users have 3 different ways of setting the cursor */
        switch(whence) {
            case SEEK_SET:
                new_cursor = pos;
                if (new_cursor < 0) {
                        return EINVAL; /* negative cursor check */
                }
                file_table[fd]->offset = pos;
                break; /* new position is pos */

            case SEEK_CUR:
                new_cursor = file_table[fd]->offset + pos;
                if (new_cursor < 0) {
                        return EINVAL; /* negative cursor check */
                }
                file_table[fd]->offset = new_cursor;
                break; /* new position is cur pos + pos */

            case SEEK_END:
                error = VOP_STAT(file_table[fd]->vnode, &statbuf);
                if (error) {
                        return error; /* error getting vnode info*/
                }
                new_cursor = statbuf.st_size + pos;
                if (new_cursor < 0) {
                        return EINVAL; /* negative cursor check */
                }
                file_table[fd]->offset = new_cursor;
                break; /* new position is pos EOF + pos */

            default:
                return EINVAL;
        }

        *retval = file_table[fd]->offset;
        return 0;
}

int
sys_close(int fd)
{
	/*
	 * Possible Errors
 	 *   +	EBADF	fd is not a valid file handle.
	 *   ?	EIO	A hard I/O error occurred. (vfs_close dosen't return
	 *   		EIO on purpose)
	 */

        if (fd < 0 || __OPEN_MAX <= fd) {
                return EBADF;
        }
        struct file_table_entry** file_table = curproc->p_file_table;
        if (file_table[fd] == NULL) {
                return EBADF;
        }

        file_table_entry_decref(file_table[fd]);
        file_table[fd] = NULL;

        return 0;
}


/* dup2 clones the file handle oldfd onto the file handle newfd.
   If newfd names an already-open file, that file is closed */
int
sys_dup2(int *retval, int oldfd, int newfd)
{
       /*
        * Possible Errors:
        *
        * EBADF:  oldfd is not a valid file handle, or newfd is a value that cannot be a valid file
        *         handle.
        *
        * EMFILE: The process's file table was full, or a process-specific limit on open files was
        *         reached.
        *
        * ENFILE: The system's file table was full, if such a thing is possible,
        *         or a global limit on open files was reached.
        */

        if (oldfd < 0 || oldfd >= __OPEN_MAX) {
                return EBADF;
        }
        if (newfd < 0 || newfd >= __OPEN_MAX) {
                return EBADF;
        }

        struct file_table_entry** file_table = curproc->p_file_table;
        if (file_table[oldfd] == NULL) {
                return EBADF;
        }

        /* From the manual: using dup2 to clone a file handle onto itself has no effect. */
        if (newfd == oldfd) {
                *retval = oldfd;
                return 0;
        }

        if (file_table[newfd] != NULL) {
                int error = sys_close(newfd);
                if (error) {
                        return error;
                }
        }

        file_table[newfd] = file_table[oldfd];

        file_table[oldfd]->refcount += 1;

        *retval = newfd;
        return 0;
}

int
sys_chdir(const char* pathname)
{
	/*
	 * Possible Errors
	 *    +    ENODEV	The device prefix of pathname did not exist.
	 *    +    ENOTDIR	A non-final component of pathname was not a directory.
	 *    +    ENOTDIR	pathname did not refer to a directory.
	 *    +    ENOENT	pathname did not exist.
	 *    +    EIO	A hard I/O error occurred.
	 *    +    EFAULT	pathname was an invalid pointer.
	 */

	/* safely copy in the pathname into a kernel buffer */
	char kbuffer[PATH_MAX];
	size_t * got = NULL;
	int error = copyinstr((const_userptr_t) pathname, kbuffer, PATH_MAX, got);
	if (error) {
                return error; /* handles EFAULT */
        }

	error = vfs_chdir(kbuffer);
	if (error) {
		return error;
        }

	return 0;
}


/* The name of the current directory is computed and stored in buf, an area of size buflen.
 * The length of data actually stored, which must be non-negative, is returned.
 *
 *       Note: this call behaves like read - the name stored in buf is not 0-terminated.
 *
 *       This function is not meant to be called except by the C library;
 *       application programmers should use getcwd instead.
 *
 *       __getcwd (like all system calls) should be atomic. In practice, because of complications
 *       associated with locking both up and down trees, it often isn't quite. Note that the kernel
 *       is not obliged to (and generally cannot) make the __getcwd call atomic with respect to
 *       other threads in the same process accessing the transfer buffer during the operation.
 */
int
sys___getcwd(int *retval, char* buf, size_t buflen)
{
	/*
	 * Possible Errors
	 *    +    ENOENT       A component of the pathname no longer exists.
	 *    +    EIO          A hard I/O error occurred.
	 *    +    EFAULT       buf points to an invalid address.
	 */

        struct iovec iov;
        struct uio ku;

        if (buf == NULL) {
                return EFAULT;
        }

        char* kbuf = kmalloc(buflen);
        if (kbuf == NULL) {
                kprintf("sys_getcwd failed to allocate %zu bytes\n", buflen);
                return ENOMEM;
        }
        uio_kinit(&iov, &ku, kbuf, buflen, 0, UIO_READ);

	int error = vfs_getcwd(&ku);
	if (error) {
                /*  handles EIO & ENOENT*/
		return error;
        }

        const size_t bytes_read = buflen - ku.uio_resid;

        error = copyout(kbuf, (userptr_t)buf, bytes_read);
        if (error) {
                return error;
        }

        *retval = bytes_read;
	return 0;
}
