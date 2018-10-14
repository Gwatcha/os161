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

// See manpages at http://ece.ubc.ca/~os161/man/syscall/ for a description of these calls

int sys_open(const char *filename, int flags)
{
        struct file_table_entry** file_table = curproc->p_file_table;

        const int first_non_reserved_fd = 3; // skip stdin, stdout, stderr

        // fd, as in file_descriptor
        int fd = first_non_reserved_fd;

        for (; ; ++fd) {
                if (fd >= __OPEN_MAX) {
                        return EMFILE;
                }
                if (file_table[fd] == NULL) {
                        break;
                }
        }

        file_table[fd] = file_table_entry_create();

        struct vnode** file_vnode = &(file_table[fd]->vnode);

        char buffer[PATH_MAX];
        snprintf(buffer, PATH_MAX, filename);

        /* Assert flags are valid flags (are defined in fcntl.h) */
        if (flags != O_RDONLY && flags != O_WRONLY && flags != O_RDWR &&
            flags != O_CREAT && flags != O_EXCL && flags != O_TRUNC && flags != O_APPEND)
            return EINVAL;

        vfs_open(buffer, flags, 0, file_vnode); // TODO: Assert flags are valid

        return fd;
}


/*
 * sys_read:
 *     given a fd, reads up to buflen bytes from the file specified by fd, at
 *     the location in the file specified by the current seek position of the
 *     file, and stores them in the space pointed to by buf. 
 * 
 *     Preconditions: fd entry must exist in table and be openned for reading,
 *                     otherwise returns EBADF. If a vnode exists, it must be a valid vnode, else
 *                     kernel panics. if part or all of address space pointed to by buf is
 *                     invalid, returns EFAULT. The vnode can not be a directory or a symlink.
 *     Postconditions: the offset for the fd is advanced by the number of bytes
 *                     read and buf contains the data
 */
ssize_t sys_read(int fd, void* buf, size_t buflen)
{
        /* Obtain user process' open file table */
        struct file_table_entry** file_table = curproc->p_file_table;

        /* bad fd checks & bulen check */
        if (fd < 0 || fd >= __OPEN_MAX)
            return EBADF;

        if (file_table[fd] == NULL ||
           (file_table[fd]->mode_flags != O_RDONLY && file_table[fd]->mode_flags != O_RDWR )) 
            return EBADF;

        if (buflen <= 0)
            return EFAULT;

        /* acquire file info */
        off_t offset = file_table[fd]->offset;
        struct vnode * file = file_table[fd]->vnode;
        
        /* TODO:, I am not sure if this implementation is smart, since I copy
        twice. I do this to explicitly call copyout, but looking at
        uiomove(), it seems it does this anyway if a user space pointer is
        supplied... may be better to create own uio and call VOP_READ */

        /* Initialize a uio suitable for I/O from a kernel buffer. */
        char kbuf[buflen];
        struct iovec iov;
        struct uio u;
        uio_kinit(&iov, &u, kbuf, buflen, offset, UIO_READ);

        /* read from the file */
        int result = VOP_READ(file, &u);
        if (result != 0) 
            return EIO;

        /* safely copy the data into the user buffer */
        result = copyout(kbuf, buf, buflen); 
        if (result != 0 )
            return EFAULT;

        /* advance seek position */
        file_table[fd]->offset += (buflen - u.uio_resid);
        /* return number of bytes read */
        return (buflen - u.uio_resid);
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
ssize_t sys_write(int fd, const void *buf, size_t nbytes)
{

        /*
         * TODO: Errors to check for:
         *  +  EBADF	fd is not a valid file descriptor, or was not opened for writing.
         *  +  EFAULT	Part or all of the address space pointed to by buf is invalid.
         *  x  ENOSPC	There is no free space remaining on the filesystem containing the file.
         *  x  EIO	A hardware I/O error occurred writing the data.
         */


        /* Obtain user process' open file table */
        struct file_table_entry** file_table = curproc->p_file_table;

        /* bad fd checks */
        if (fd < 0 || fd >= __OPEN_MAX)
            return EBADF;

        if (file_table[fd] == NULL ||
           (file_table[fd]->mode_flags != O_WRONLY && 
            file_table[fd]->mode_flags != O_RDWR && file_table[fd]->mode_flags != O_APPEND )) 
            return EBADF;

        /* acquire file info */
        off_t offset = file_table[fd]->offset;
        struct vnode * file = file_table[fd]->vnode;
        
        /* TODO:, I am not sure if this implementation is smart, since I copy
        twice. I do this to explicitly call copyout, but looking at
        uiomove(), it seems it does this anyway if a user space pointer is
        supplied... thus setting up our own uio might be better */

        /* copy the user data in */
        char kbuf[nbytes];
        int result = copyin(buf, kbuf, nbytes);
        if (result != 0)
            return EFAULT;

        /* Initialize a uio suitable for I/O from a kernel buffer. */
        struct iovec iov;
        struct uio u;
        uio_kinit(&iov, &u, kbuf, nbytes, offset, UIO_WRITE);

        /* write to the file */
        result = VOP_WRITE(file, &u);
        if (result == ENOSPC) 
            return ENOSPC;
        if (result == EIO)
            return EIO;

        /* advance seek position */
        file_table[fd]->offset += (nbytes - u.uio_resid);

        /* return number of bytes written */
        return (nbytes - u.uio_resid);
}

off_t sys_lseek(int fd, off_t pos, int whence)
{
        /*
         * TODO: Errors to check for
         *  +  EBADF	fd is not a valid file handle. 
         *  x  ESPIPE	fd refers to an object which does not support seeking. **
         *  +  EINVAL	whence is invalid. 
         *  +  EINVAL	The resulting seek position would be negative. 
         */

        struct file_table_entry** file_table = curproc->p_file_table;

        /* bad fd checks */
        if (fd < 0 || __OPEN_MAX <= fd) 
            return EBADF;

        if (file_table[fd] == NULL) 
            return EBADF;
        
        /* a stat buf is needed in case we need a files size */
        struct stat statbuf;
        int result;
        off_t new_cursor;

        /* Users have 3 different ways of setting the cursor */
        switch(whence) {
            case SEEK_SET:  new_cursor = pos;
                            if (new_cursor < 0)
                                return EINVAL; /* negative cursor check */
                            file_table[fd]->offset = pos;
                            break; /* new position is pos */  

            case SEEK_CUR:  new_cursor = file_table[fd]->offset + pos; 
                            if (new_cursor < 0)
                                return EINVAL; /* negative cursor check */
                            file_table[fd]->offset = new_cursor;
                            break; /* new position is cur pos + pos */ 

            case SEEK_END:  result = VOP_STAT(file_table[fd]->vnode, &statbuf);
                            if ( result )
                                return result; /* error getting vnode info*/

                            new_cursor = statbuf.st_size - 1 + pos;
                            if (new_cursor < 0)
                                return EINVAL; /* negative cursor check */
                            file_table[fd]->offset = new_cursor;
                            break; /* new position is pos EOF + pos */ 
            default : return EINVAL;
        }

        return file_table[fd]->offset;
}

int sys_close(int fd)
{
        if (fd < 0 || __OPEN_MAX <= fd) {
                return EBADF;
        }
        struct file_table_entry* file_table_entry = curproc->p_file_table[fd];
        if (file_table_entry == NULL) {
                return EBADF;
        }

        // TODO: File reference counting

        vfs_close(file_table_entry->vnode);
        return 0;
}

int sys_dup2(int oldfd, int newfd)
{
        (void)oldfd;
        (void)newfd;
        return -1;
}

int sys_chdir(const char* pathname)
{
        // May need to use copyin/copyout
        char buffer[PATH_MAX];
        snprintf(buffer, PATH_MAX, pathname);
        return vfs_chdir(buffer);
}

int sys___getcwd(char* buf, size_t buflen)
{
        // May need to use copyin/copyout
        struct iovec iov;
        struct uio u;

        uio_kinit(&iov, &u, buf, buflen, 0, UIO_READ);
        return vfs_getcwd(&u);
}

