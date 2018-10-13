#include <types.h>
#include <syscall.h>

#include <uio.h>

#include <proc.h>
#include <current.h>

#include <limits.h>
#include <vfs.h>
#include <kern/errno.h>

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

        // TODO: ensure that this is correctly initialized
        struct vnode** file_vnode = &(file_table[fd]->vnode);

        char buffer[PATH_MAX];
        snprintf(buffer, PATH_MAX, filename);

        return vfs_open(buffer, flags, 0, file_vnode);
}

ssize_t sys_read(int fd, void* buf, size_t buflen)
{
        (void)fd;
        (void)buf;
        (void)buflen;
        return -1;
}

ssize_t sys_write(int fd, const void *buf, size_t nbytes)
{
        (void)fd;
        (void)buf;
        (void)nbytes;
        return -1;
}

off_t sys_lseek(int fd, off_t pos, int whence)
{
        (void)fd;
        (void)pos;
        (void)whence;
        return -1;
}

int sys_close(int fd)
{
        (void)fd;
        return -1;
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
        struct uio myuio;

        uio_kinit(&iov, &myuio, buf, buflen, 0, UIO_READ);
        return vfs_getcwd(&myuio);
}

