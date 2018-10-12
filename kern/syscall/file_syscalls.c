
#include <types.h>
#include <syscall.h>

int sys_chdir(const char* pathname)
{
        (void)pathname;
        return -1;
}

