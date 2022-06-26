/* util.c - memory functions.
 *
 * Copyright (c) 2020, Aleksey Kravchenko <rhash.admin@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE  INCLUDING ALL IMPLIED WARRANTIES OF  MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT,  OR CONSEQUENTIAL DAMAGES  OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE,  DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT,  NEGLIGENCE
 * OR OTHER TORTIOUS ACTION,  ARISING OUT OF  OR IN CONNECTION  WITH THE USE  OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#include "util.h"

#if defined(HAS_POSIX_ALIGNED_ALLOC)

#include <errno.h>

void* rhash_px_aalloc(size_t alignment, size_t size)
{
	void* ptr;
	if ((errno = posix_memalign(&ptr, alignment, size)) != 0)
		return NULL;
	return ptr;
}

#elif defined(HAS_GENERIC_ALIGNED_ALLOC)

#include <assert.h>
#include <stdlib.h>

void* rhash_aligned_alloc(size_t alignment, size_t size)
{
	unsigned char* block = (unsigned char*)malloc(size + alignment);
	assert((alignment & (alignment - 1)) == 0);
	assert(alignment >= sizeof(void*));
	if (block) {
		const size_t alignment_mask = (alignment - 1);
		unsigned char* basement = block + sizeof(void*);
		size_t offset = ((unsigned char*)0 - basement) & alignment_mask;
		void** result = (void**)(basement + offset);
		assert((((unsigned char*)result - (unsigned char*)0) % alignment) == 0);
		result[-1] = block; /* store original pointer */
		return result;
	}
	return NULL;
}

void rhash_aligned_free(void* ptr)
{
	void** pfree = (void**)ptr;
	if (ptr)
		free(pfree[-1]);
}

#else
typedef int dummy_declaration_required_by_strict_iso_c;
#endif /* HAS_POSIX_ALIGNED_ALLOC / HAS_GENERIC_ALIGNED_ALLOC */
