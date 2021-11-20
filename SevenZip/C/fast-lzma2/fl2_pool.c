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


/* ======   Dependencies   ======= */
#include <stddef.h>  /* size_t */
#include <stdlib.h>  /* malloc, calloc */
#include "fl2_pool.h"
#include "fl2_internal.h"


#ifndef FL2_SINGLETHREAD

#include "fl2_threading.h"   /* pthread adaptation */

struct FL2POOL_ctx_s {
    /* Keep track of the threads */
    size_t numThreads;

    /* All threads work on the same function and object during a job */
    FL2POOL_function function;
    void *opaque;

    /* The number of threads working on jobs */
    size_t numThreadsBusy;
    /* Indicates the number of threads requested and the values to pass */
    ptrdiff_t queueIndex;
    ptrdiff_t queueEnd;

    /* The mutex protects the queue */
    FL2_pthread_mutex_t queueMutex;
    /* Condition variable for pushers to wait on when the queue is full */
    FL2_pthread_cond_t busyCond;
    /* Condition variable for poppers to wait on when the queue is empty */
    FL2_pthread_cond_t newJobsCond;
    /* Indicates if the queue is shutting down */
    int shutdown;

    /* The threads. Extras to be calloc'd */
    FL2_pthread_t threads[1];
};

/* FL2POOL_thread() :
   Work thread for the thread pool.
   Waits for jobs and executes them.
   @returns : NULL on failure else non-null.
*/
static void* FL2POOL_thread(void* opaque)
{
    FL2POOL_ctx* const ctx = (FL2POOL_ctx*)opaque;
    if (!ctx) { return NULL; }
    FL2_pthread_mutex_lock(&ctx->queueMutex);
    for (;;) {

        /* While the mutex is locked, wait for a non-empty queue or until shutdown */
        while (ctx->queueIndex >= ctx->queueEnd && !ctx->shutdown) {
            FL2_pthread_cond_wait(&ctx->newJobsCond, &ctx->queueMutex);
        }
        /* empty => shutting down: so stop */
        if (ctx->shutdown) {
            FL2_pthread_mutex_unlock(&ctx->queueMutex);
            return opaque;
        }
        /* Pop a job off the queue */
        size_t n = ctx->queueIndex;
        ++ctx->queueIndex;
        ++ctx->numThreadsBusy;
        /* Unlock the mutex and run the job */
        FL2_pthread_mutex_unlock(&ctx->queueMutex);

        ctx->function(ctx->opaque, n);

        FL2_pthread_mutex_lock(&ctx->queueMutex);
        --ctx->numThreadsBusy;
        /* Signal the master thread waiting for jobs to complete */
        FL2_pthread_cond_signal(&ctx->busyCond);
    }  /* for (;;) */
    /* Unreachable */
}

FL2POOL_ctx* FL2POOL_create(size_t numThreads)
{
    FL2POOL_ctx* ctx;
    /* Check the parameters */
    if (!numThreads) { return NULL; }
    /* Allocate the context and zero initialize */
    ctx = calloc(1, sizeof(FL2POOL_ctx) + (numThreads - 1) * sizeof(FL2_pthread_t));
    if (!ctx) { return NULL; }
    /* Initialize the busy count and jobs range */
    ctx->numThreadsBusy = 0;
    ctx->queueIndex = 0;
    ctx->queueEnd = 0;
    (void)FL2_pthread_mutex_init(&ctx->queueMutex, NULL);
    (void)FL2_pthread_cond_init(&ctx->busyCond, NULL);
    (void)FL2_pthread_cond_init(&ctx->newJobsCond, NULL);
    ctx->shutdown = 0;
    ctx->numThreads = 0;
    /* Initialize the threads */
    {   size_t i;
        for (i = 0; i < numThreads; ++i) {
            if (FL2_pthread_create(&ctx->threads[i], NULL, &FL2POOL_thread, ctx)) {
                ctx->numThreads = i;
                FL2POOL_free(ctx);
                return NULL;
        }   }
        ctx->numThreads = numThreads;
    }
    return ctx;
}

/*! FL2POOL_join() :
    Shutdown the queue, wake any sleeping threads, and join all of the threads.
*/
static void FL2POOL_join(FL2POOL_ctx* ctx)
{
    /* Shut down the queue */
    FL2_pthread_mutex_lock(&ctx->queueMutex);
    ctx->shutdown = 1;
    /* Wake up sleeping threads */
    FL2_pthread_cond_broadcast(&ctx->newJobsCond);
    FL2_pthread_mutex_unlock(&ctx->queueMutex);
    /* Join all of the threads */
    for (size_t i = 0; i < ctx->numThreads; ++i)
        FL2_pthread_join(ctx->threads[i], NULL);
}

void FL2POOL_free(FL2POOL_ctx *ctx)
{
    if (!ctx) { return; }
    FL2POOL_join(ctx);
    FL2_pthread_mutex_destroy(&ctx->queueMutex);
    FL2_pthread_cond_destroy(&ctx->busyCond);
    FL2_pthread_cond_destroy(&ctx->newJobsCond);
    free(ctx);
}

size_t FL2POOL_sizeof(FL2POOL_ctx *ctx)
{
    if (ctx==NULL) return 0;  /* supports sizeof NULL */
    return sizeof(*ctx) + ctx->numThreads * sizeof(FL2_pthread_t);
}

void FL2POOL_addRange(void* ctxVoid, FL2POOL_function function, void *opaque, ptrdiff_t first, ptrdiff_t end)
{
    FL2POOL_ctx* const ctx = (FL2POOL_ctx*)ctxVoid;
    if (!ctx)
		return; 

    /* Callers always wait for jobs to complete before adding a new set */
    assert(!ctx->numThreadsBusy);

    FL2_pthread_mutex_lock(&ctx->queueMutex);
    ctx->function = function;
    ctx->opaque = opaque;
    ctx->queueIndex = first;
    ctx->queueEnd = end;
    FL2_pthread_cond_broadcast(&ctx->newJobsCond);
    FL2_pthread_mutex_unlock(&ctx->queueMutex);
}

void FL2POOL_add(void* ctxVoid, FL2POOL_function function, void *opaque, ptrdiff_t n)
{
    FL2POOL_addRange(ctxVoid, function, opaque, n, n + 1);
}

int FL2POOL_waitAll(void *ctxVoid, unsigned timeout)
{
    FL2POOL_ctx* const ctx = (FL2POOL_ctx*)ctxVoid;
    if (!ctx || (!ctx->numThreadsBusy && ctx->queueIndex >= ctx->queueEnd) || ctx->shutdown) { return 0; }

    FL2_pthread_mutex_lock(&ctx->queueMutex);
    /* Need to test for ctx->queueIndex < ctx->queueEnd in case not all jobs have started */
    if (timeout != 0) {
        if ((ctx->numThreadsBusy || ctx->queueIndex < ctx->queueEnd) && !ctx->shutdown)
            FL2_pthread_cond_timedwait(&ctx->busyCond, &ctx->queueMutex, timeout);
    }
    else {
        while ((ctx->numThreadsBusy || ctx->queueIndex < ctx->queueEnd) && !ctx->shutdown)
            FL2_pthread_cond_wait(&ctx->busyCond, &ctx->queueMutex);
    }
    FL2_pthread_mutex_unlock(&ctx->queueMutex);
    return ctx->numThreadsBusy && !ctx->shutdown;
}

size_t FL2POOL_threadsBusy(void * ctx)
{
    return ((FL2POOL_ctx*)ctx)->numThreadsBusy;
}

#endif  /* FL2_SINGLETHREAD */
