#include <types.h>

#include <page_file.h>

#include <lib.h>
#include <stat.h>
#include <vfs.h>
#include <vnode.h>

void page_file_bootstrap(void)
{
        struct vnode* swapfile;

        /* Limited scope of buffer */
        {
                char buffer[64];
                snprintf(buffer, sizeof(buffer), "LHD0.img");
                const int error = vfs_open(buffer, 2, 0, &swapfile);

                if (error) {
                        kprintf("Error loading %s: %s\n", buffer, strerror(error));
                        return;
                }
        }

        /* Limited scope of status */
        {
                struct stat status;
                const int error = VOP_STAT(swapfile, &status);

                if (error) {
                        kprintf("Error getting page file status: %s\n", strerror(error));
                        return;
                }

                DEBUG(DB_VM, "Page file memory available: %d\n", (int)status.st_size);
        }
}







