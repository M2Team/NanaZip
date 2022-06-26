/* util.h */
#ifndef UTIL_H
#define UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

/* compile-time assert */
#define RHASH_ASSERT(cond) (void)sizeof(char[1 - 2 * !(cond)])

#if (defined(__GNUC__) && __GNUC__ >= 4 && (__GNUC__ > 4 || __GNUC_MINOR__ >= 1) \
	&& defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4)) \
	|| (defined(__INTEL_COMPILER) && !defined(_WIN32))
/* atomic operations are defined by ICC and GCC >= 4.1, but by the later one supposedly not for ARM */
/* note: ICC on ia64 platform possibly require ia64intrin.h, need testing */
# define atomic_compare_and_swap(ptr, oldval, newval) __sync_val_compare_and_swap(ptr, oldval, newval)
#elif defined(_MSC_VER)
# include <windows.h>
# define atomic_compare_and_swap(ptr, oldval, newval) InterlockedCompareExchange(ptr, newval, oldval)
#elif defined(__sun)
# include <atomic.h>
# define atomic_compare_and_swap(ptr, oldval, newval) atomic_cas_32(ptr, oldval, newval)
#else
/* pray that it will work */
# define atomic_compare_and_swap(ptr, oldval, newval) { if (*(ptr) == (oldval)) *(ptr) = (newval); }
# define NO_ATOMIC_BUILTINS
#endif

/* alignment macros */
#define DEFAULT_ALIGNMENT 64
#define ALIGN_SIZE_BY(size, align) (((size) + ((align) - 1)) & ~((align) - 1))
#define IS_SIZE_ALIGNED_BY(size, align) (((size) & ((align) - 1)) == 0)
#define IS_PTR_ALIGNED_BY(ptr, align) IS_SIZE_ALIGNED_BY((uintptr_t)(ptr), (align))

/* define rhash_aligned_alloc() and rhash_aligned_free() */
#if !defined(NO_WIN32_ALIGNED_ALLOC) && defined(_WIN32)

# define HAS_WIN32_ALIGNED_ALLOC
# include <malloc.h>
# define rhash_aligned_alloc(alignment, size) _aligned_malloc((size), (alignment))
# define rhash_aligned_free(ptr) _aligned_free(ptr)

#elif !defined(NO_STDC_ALIGNED_ALLOC) && (__STDC_VERSION__ >= 201112L || defined(_ISOC11_SOURCE)) \
	&& !defined(__APPLE__) && (!defined(__ANDROID_API__) || __ANDROID_API__ >= 28)

# define HAS_STDC_ALIGNED_ALLOC
# include <stdlib.h>
# define rhash_aligned_alloc(alignment, size) aligned_alloc((alignment), ALIGN_SIZE_BY(size, alignment))
# define rhash_aligned_free(ptr) free(ptr)

#else /* defined(_WIN32) ... */

# include "ustd.h" /* for _POSIX_VERSION macro */

# if !defined(NO_POSIX_ALIGNED_ALLOC) && (_POSIX_VERSION >= 200112L || _XOPEN_SOURCE >= 600)

#  define HAS_POSIX_ALIGNED_ALLOC
#  include <stdlib.h>
#  define rhash_aligned_alloc(alignment, size) rhash_px_aalloc((alignment), ALIGN_SIZE_BY(size, sizeof(void*)))
#  define rhash_aligned_free(ptr) free(ptr)
void* rhash_px_aalloc(size_t size, size_t alignment);

# else

#  define HAS_GENERIC_ALIGNED_ALLOC
void* rhash_aligned_alloc(size_t alignment, size_t size);
void rhash_aligned_free(void* ptr);

# endif /* !defined(NO_POSIX_ALIGNED_ALLOC) ... */
#endif /* defined(_WIN32) ... */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* UTIL_H */
