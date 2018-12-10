#include <types.h>
#include <kern/errno.h>
#include <page_file.h>
#include <lib.h>
#include <stat.h>
#include <vfs.h>
#include <vnode.h>
#include <kern/iovec.h>
#include <uio.h>
#include <vm.h>

/* a boolean for each page in the swapmap to indicate if it is stored or not */
static bool * swapmap;

static ssize_t swapmap_size;
static struct vnode * swapfile;

void page_file_bootstrap(void)
{
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

                /* kmalloc bool array depending on how many entries we can fit into the img */
                swapmap_size = size_to_page_count(status.st_size);
                swapmap = kmalloc(swapmap_size);
                /* indicate every entry is UNUSED, meaning no page is stored there */
                for ( pfid i = 0; i < swapmap_size; i++) {
                        swapmap[i] = 0;
                }
        }


}

/*
 * Writes PAGE_SIZE bytes to disk (if possible).
 * Returns the index at which the page can be retrieved in future.
 * Returns PF_INVALID if it is not possible to write a page to disk.
 */
pfid page_file_write(const void* src) {

        /* find next free page index */
        pfid i;
        for (i = 0; i < swapmap_size; i++) {
                if ( swapmap[i] == 0 ) {
                        swapmap[i] = 1;
                        /* new uio for swap write transfer */
                        struct iovec iov;
                        struct uio swp_uio;
                        uio_kinit(&iov, &swp_uio, (void*) src, PAGE_SIZE, i*PAGE_SIZE, UIO_WRITE);

                        int result = VOP_WRITE(swapfile, &swp_uio);
                        if (result) {
                                return result;
                        }

                        return i;
                }
            }

        return PF_INVALID;
}

/*
 * Reads PAGE_SIZE bytes from page pfid on disk.
 * Returns an error code if the data cannot be read.
 * Free's the page pfid for future use.
 */
int page_file_read_and_free(pfid index, void* data) {

        if ( index < 0 || index >= swapmap_size || swapmap[index] != 1 ) {
                return PF_INVALID;
        }

        /* new uio for swap read transfer */
        struct iovec iov;
        struct uio swp_uio;
        uio_kinit(&iov, &swp_uio, data, PAGE_SIZE, index*PAGE_SIZE, UIO_READ);

        /* read swp page into data */
        int result = VOP_READ(swapfile, &swp_uio);
        if (result) {
                return result;
        }

        /* mark as free */
        swapmap[index] = 0;
        return 0;
}

/*
 * Free's the page pfid for future use.
 */
void page_file_free(pfid index) {
       KASSERT( !(index < 0 ));
       KASSERT( !(index >= swapmap_size));
       KASSERT( !(swapmap[index] != 1 ));
       swapmap[index] = 0;
}
