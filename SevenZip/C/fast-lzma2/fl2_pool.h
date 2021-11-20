/*
* Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
* All rights reserved.
* Modified for FL2 by Conor McCarthy
*
* This source code is licensed under both the BSD-style license (found in the
* LICENSE file in the root directory of this source tree) and the GPLv2 (found
* in the COPYING file in the root directory of this source tree).
* You may select, at your option, one of the above-listed licenses.
*/

#ifndef FL2POOL_H
#define FL2POOL_H

#if defined (__cplusplus)
extern "C" {
#endif


#include <stddef.h>   /* size_t */

typedef struct FL2POOL_ctx_s FL2POOL_ctx;

/*! FL2POOL_create() :
*  Create a thread pool with at most `numThreads` threads.
* `numThreads` must be at least 1.
* @return : FL2POOL_ctx pointer on success, else NULL.
*/
FL2POOL_ctx *FL2POOL_create(size_t numThreads);


/*! FL2POOL_free() :
Free a thread pool returned by FL2POOL_create().
*/
void FL2POOL_free(FL2POOL_ctx *ctx);

/*! FL2POOL_sizeof() :
return memory usage of pool returned by FL2POOL_create().
*/
size_t FL2POOL_sizeof(FL2POOL_ctx *ctx);

/*! FL2POOL_function :
The function type that can be added to a thread pool.
*/
typedef void(*FL2POOL_function)(void *, ptrdiff_t);

/*! FL2POOL_add() :
Add the job `function(opaque)` to the thread pool.
FL2POOL_addRange adds multiple jobs with size_t parameter from first to less than end.
Possibly blocks until there is room in the queue.
Note : The function may be executed asynchronously, so `opaque` must live until the function has been completed.
*/
void FL2POOL_add(void* ctxVoid, FL2POOL_function function, void *opaque, ptrdiff_t n);
void FL2POOL_addRange(void *ctx, FL2POOL_function function, void *opaque, ptrdiff_t first, ptrdiff_t end);

int FL2POOL_waitAll(void *ctx, unsigned timeout);

size_t FL2POOL_threadsBusy(void *ctx);

#if defined (__cplusplus)
}
#endif

#endif
