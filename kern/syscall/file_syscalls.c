#include <types.h>
#include <syscall.h>

#include <limits.h>
#include <vfs.h>

// See manpages at http://ece.ubc.ca/~os161/man/syscall/ for a description of these calls

int sys_open(const char *filename, int flags)
{
        (void)filename;
        (void)flags;
        return -1;
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
        char buffer[PATH_MAX];
        snprintf(buffer, PATH_MAX, pathname);
        return vfs_chdir(buffer);
}

int sys___getcwd(char* buf, size_t buflen)
{
        (void)buf;
        (void)buflen;
        return -1;
}

