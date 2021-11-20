/**
 * Copyright (c) 2016 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 *
 * You can contact the author at:
 * - zstdmt source repository: https://github.com/mcmilk/zstdmt
 */

/**
 * This file will hold wrapper for systems, which do not support pthreads
 */

/* create fake symbol to avoid empty translation unit warning */
int g_ZSTD_threading_useles_symbol;

#include "fast-lzma2.h"
#include "fl2_threading.h"
#include "util.h"

#if !defined(FL2_SINGLETHREAD) && defined(_WIN32)

/**
 * Windows minimalist Pthread Wrapper, based on :
 * http://www.cse.wustl.edu/~schmidt/win32-cv-1.html
 */


/* ===  Dependencies  === */
#include <process.h>
#include <errno.h>


/* ===  Implementation  === */

static unsigned __stdcall worker(void *arg)
{
    FL2_pthread_t* const thread = (FL2_pthread_t*) arg;
    thread->arg = thread->start_routine(thread->arg);
    return 0;
}

int FL2_pthread_create(FL2_pthread_t* thread, const void* unused,
            void* (*start_routine) (void*), void* arg)
{
    (void)unused;
    thread->arg = arg;
    thread->start_routine = start_routine;
    thread->handle = (HANDLE) _beginthreadex(NULL, 0, worker, thread, 0, NULL);

    if (!thread->handle)
        return errno;
    else
        return 0;
}

int FL2_pthread_join(FL2_pthread_t thread, void **value_ptr)
{
    DWORD result;

    if (!thread.handle) return 0;

    result = WaitForSingleObject(thread.handle, INFINITE);
    switch (result) {
    case WAIT_OBJECT_0:
        if (value_ptr) *value_ptr = thread.arg;
        return 0;
    case WAIT_ABANDONED:
        return EINVAL;
    default:
        return GetLastError();
    }
}

#endif   /* FL2_SINGLETHREAD */

unsigned FL2_checkNbThreads(unsigned nbThreads)
{
#ifndef FL2_SINGLETHREAD
    if (nbThreads == 0) {
        nbThreads = UTIL_countPhysicalCores();
        nbThreads += !nbThreads;
    }
    if (nbThreads > FL2_MAXTHREADS) {
        nbThreads = FL2_MAXTHREADS;
    }
#else
    nbThreads = 1;
#endif
    return nbThreads;
}

