/*
 * Copyright (c) 2018
 *	The Trap Handlers!
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

#ifndef _PAGE_FILE_H_
#define _PAGE_FILE_H_

#define PF_INVALID -1

/* Index of a page on disk */
typedef int pfid;

/*
 * Returns the index at which the page can be retrieved in future
 * Returns PF_INVALID if it is not possible to suspend a page
 */
void page_file_bootstrap(void);

/*
 * Writes PAGE_SIZE bytes to disk (if possible).
 * Returns the index at which the page can be retrieved in future.
 * Returns PF_INVALID if it is not possible to write a page to disk.
 */
pfid page_file_write(const void* src);

/*
 * Reads PAGE_SIZE bytes from page pfid on disk.
 * Returns an error code if the data cannot be read.
 * Free's the page pfid for future use.
 */
int page_file_read_and_free(pfid index, void* data);

/*
 * Free's the page pfid for future use.
 */
void page_file_free(pfid index);

#endif /* _PAGE_FILE_H_ */

