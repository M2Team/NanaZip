﻿
/**
 * Copyright (c) 2016 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 *
 * You can contact the author at:
 * - zstdmt source repository: https://github.com/mcmilk/zstdmt
 */

/**
 * This file will hold wrapper for systems, which do not support Pthreads
 */

#ifdef _WIN32

/**
 * Windows Pthread Wrapper, based on this site:
 * http://www.cse.wustl.edu/~schmidt/win32-cv-1.html
 */

#include "threading.h"

#include <process.h>
#include <errno.h>

static unsigned __stdcall worker(void *arg)
{
	pthread_t *thread = (pthread_t *) arg;
	thread->arg = thread->start_routine(thread->arg);
	return 0;
}

int
pthread_create(pthread_t * thread, const void *unused,
	       void *(*start_routine) (void *), void *arg)
{
	(void)unused;
	thread->arg = arg;
	thread->start_routine = start_routine;
	thread->handle =
	    (HANDLE) _beginthreadex(NULL, 0, worker, thread, 0, NULL);

	if (!thread->handle)
		return errno;
	else
		return 0;
}

int _pthread_join(pthread_t * thread, void **value_ptr)
{
	DWORD result;

	if (!thread->handle)
		return 0;

	result = WaitForSingleObject(thread->handle, INFINITE);
	switch (result) {
	case WAIT_OBJECT_0:
		if (value_ptr)
			*value_ptr = thread->arg;
		return 0;
	case WAIT_ABANDONED:
		return EINVAL;
	default:
		return GetLastError();
	}
}

#endif
