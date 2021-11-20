
/**
 * Copyright (c) 2016 - 2017 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 *
 * You can contact the author at:
 * - zstdmt source repository: https://github.com/mcmilk/zstdmt
 */

#include <stdlib.h>
#include <string.h>

#define LZ5F_DISABLE_OBSOLETE_ENUMS
#include "lz5frame.h"

#include "memmt.h"
#include "threading.h"
#include "list.h"
#include "lz5-mt.h"

/**
 * multi threaded lz5 - multiple workers version
 *
 * - each thread works on his own
 * - no main thread which does reading and then starting the work
 * - needs a callback for reading / writing
 * - each worker does his:
 *   1) get read mutex and read some input
 *   2) release read mutex and do compression
 *   3) get write mutex and write result
 *   4) begin with step 1 again, until no input
 */

/* worker for compression */
typedef struct {
	LZ5MT_CCtx *ctx;
	LZ5F_preferences_t zpref;
	pthread_t pthread;
} cwork_t;

struct writelist;
struct writelist {
	size_t frame;
	LZ5MT_Buffer out;
	struct list_head node;
};

struct LZ5MT_CCtx_s {

	/* level: 1..LZ5MT_LEVEL_MAX */
	int level;

	/* threads: 1..LZ5MT_THREAD_MAX */
	int threads;

	/* should be used for read from input */
	int inputsize;

	/* statistic */
	size_t insize;
	size_t outsize;
	size_t curframe;
	size_t frames;

	/* threading */
	cwork_t *cwork;

	/* reading input */
	pthread_mutex_t read_mutex;
	fn_read *fn_read;
	void *arg_read;

	/* writing output */
	pthread_mutex_t write_mutex;
	fn_write *fn_write;
	void *arg_write;

	/* lists for writing queue */
	struct list_head writelist_free;
	struct list_head writelist_busy;
	struct list_head writelist_done;
};

/* **************************************
 * Compression
 ****************************************/

LZ5MT_CCtx *LZ5MT_createCCtx(int threads, int level, int inputsize)
{
	LZ5MT_CCtx *ctx;
	int t;

	/* allocate ctx */
	ctx = (LZ5MT_CCtx *) malloc(sizeof(LZ5MT_CCtx));
	if (!ctx)
		return 0;

	/* check threads value */
	if (threads < 1 || threads > LZ5MT_THREAD_MAX)
		return 0;

	/* check level */
	if (level < LZ5MT_LEVEL_MIN || level > LZ5MT_LEVEL_MAX)
		return 0;

	/* calculate chunksize for one thread */
	if (inputsize)
		ctx->inputsize = inputsize;
	else
		ctx->inputsize = 1024 * 1024 * 4;

	/* setup ctx */
	ctx->level = level;
	ctx->threads = threads;
	ctx->insize = 0;
	ctx->outsize = 0;
	ctx->frames = 0;
	ctx->curframe = 0;

	pthread_mutex_init(&ctx->read_mutex, NULL);
	pthread_mutex_init(&ctx->write_mutex, NULL);

	/* free -> busy -> out -> free -> ... */
	INIT_LIST_HEAD(&ctx->writelist_free);	/* free, can be used */
	INIT_LIST_HEAD(&ctx->writelist_busy);	/* busy */
	INIT_LIST_HEAD(&ctx->writelist_done);	/* can be written */

	ctx->cwork = (cwork_t *) malloc(sizeof(cwork_t) * threads);
	if (!ctx->cwork)
		goto err_cwork;

	for (t = 0; t < threads; t++) {
		cwork_t *w = &ctx->cwork[t];
		w->ctx = ctx;

		/* setup preferences for that thread */
		memset(&w->zpref, 0, sizeof(LZ5F_preferences_t));
		w->zpref.compressionLevel = level;
		w->zpref.frameInfo.blockMode = LZ5F_blockLinked;
		w->zpref.frameInfo.contentSize = 1;
		w->zpref.frameInfo.contentChecksumFlag =
		    LZ5F_contentChecksumEnabled;

	}

	return ctx;

 err_cwork:
	free(ctx);

	return 0;
}

/**
 * mt_error - return mt lib specific error code
 */
static size_t mt_error(int rv)
{
	switch (rv) {
	case -1:
		return ERROR(read_fail);
	case -2:
		return ERROR(canceled);
	case -3:
		return ERROR(memory_allocation);
	}

	return ERROR(read_fail);
}

/**
 * pt_write - queue for compressed output
 */
static size_t pt_write(LZ5MT_CCtx * ctx, struct writelist *wl)
{
	struct list_head *entry;

	/* move the entry to the done list */
	list_move(&wl->node, &ctx->writelist_done);

	/* the entry isn't the currently needed, return...  */
	if (wl->frame != ctx->curframe)
		return 0;

 again:
	/* check, what can be written ... */
	list_for_each(entry, &ctx->writelist_done) {
		wl = list_entry(entry, struct writelist, node);
		if (wl->frame == ctx->curframe) {
			int rv = ctx->fn_write(ctx->arg_write, &wl->out);
			if (rv != 0)
				return mt_error(rv);
			ctx->outsize += wl->out.size;
			ctx->curframe++;
			list_move(entry, &ctx->writelist_free);
			goto again;
		}
	}

	return 0;
}

static void *pt_compress(void *arg)
{
	cwork_t *w = (cwork_t *) arg;
	LZ5MT_CCtx *ctx = w->ctx;
	size_t result;
	LZ5MT_Buffer in;

	/* inbuf is constant */
	in.size = ctx->inputsize;
	in.buf = malloc(in.size);
	if (!in.buf)
		return (void *)ERROR(memory_allocation);

	for (;;) {
		struct list_head *entry;
		struct writelist *wl;
		int rv;

		/* allocate space for new output */
		pthread_mutex_lock(&ctx->write_mutex);
		if (!list_empty(&ctx->writelist_free)) {
			/* take unused entry */
			entry = list_first(&ctx->writelist_free);
			wl = list_entry(entry, struct writelist, node);
			wl->out.size =
			    LZ5F_compressFrameBound(ctx->inputsize,
						    &w->zpref) + 12;
			list_move(entry, &ctx->writelist_busy);
		} else {
			/* allocate new one */
			wl = (struct writelist *)
			    malloc(sizeof(struct writelist));
			if (!wl) {
				pthread_mutex_unlock(&ctx->write_mutex);
				return (void *)ERROR(memory_allocation);
			}
			wl->out.size =
			    LZ5F_compressFrameBound(ctx->inputsize,
						    &w->zpref) + 12;;
			wl->out.buf = malloc(wl->out.size);
			if (!wl->out.buf) {
				pthread_mutex_unlock(&ctx->write_mutex);
				return (void *)ERROR(memory_allocation);
			}
			list_add(&wl->node, &ctx->writelist_busy);
		}
		pthread_mutex_unlock(&ctx->write_mutex);

		/* read new input */
		pthread_mutex_lock(&ctx->read_mutex);
		in.size = ctx->inputsize;
		rv = ctx->fn_read(ctx->arg_read, &in);
		if (rv != 0) {
			pthread_mutex_unlock(&ctx->read_mutex);
			return (void *)mt_error(rv);
		}

		/* eof */
		if (in.size == 0 && ctx->frames > 0) {
			free(in.buf);
			pthread_mutex_unlock(&ctx->read_mutex);

			pthread_mutex_lock(&ctx->write_mutex);
			list_move(&wl->node, &ctx->writelist_free);
			pthread_mutex_unlock(&ctx->write_mutex);

			goto okay;
		}
		ctx->insize += in.size;
		wl->frame = ctx->frames++;
		pthread_mutex_unlock(&ctx->read_mutex);

		/* compress whole frame */
		result =
		    LZ5F_compressFrame((unsigned char *)wl->out.buf + 12,
				       wl->out.size - 12, in.buf, in.size,
				       &w->zpref);
		if (LZ5F_isError(result)) {
			pthread_mutex_lock(&ctx->write_mutex);
			list_move(&wl->node, &ctx->writelist_free);
			pthread_mutex_unlock(&ctx->write_mutex);
			/* user can lookup that code */
			lz5mt_errcode = result;
			return (void *)ERROR(compression_library);
		}

		/* write skippable frame */
		MEM_writeLE32((unsigned char *)wl->out.buf + 0,
			      LZ5FMT_MAGIC_SKIPPABLE);
		MEM_writeLE32((unsigned char *)wl->out.buf + 4, 4);
		MEM_writeLE32((unsigned char *)wl->out.buf + 8, (U32) result);
		wl->out.size = result + 12;

		/* write result */
		pthread_mutex_lock(&ctx->write_mutex);
		result = pt_write(ctx, wl);
		pthread_mutex_unlock(&ctx->write_mutex);
		if (LZ5MT_isError(result))
			return (void *)result;
	}

 okay:
	return 0;
}

size_t LZ5MT_compressCCtx(LZ5MT_CCtx * ctx, LZ5MT_RdWr_t * rdwr)
{
	int t;
	void *retval_of_thread = 0;

	if (!ctx)
		return ERROR(compressionParameter_unsupported);

	/* init reading and writing functions */
	ctx->fn_read = rdwr->fn_read;
	ctx->fn_write = rdwr->fn_write;
	ctx->arg_read = rdwr->arg_read;
	ctx->arg_write = rdwr->arg_write;

	/* start all workers */
	for (t = 0; t < ctx->threads; t++) {
		cwork_t *w = &ctx->cwork[t];
		pthread_create(&w->pthread, NULL, pt_compress, w);
	}

	/* wait for all workers */
	for (t = 0; t < ctx->threads; t++) {
		cwork_t *w = &ctx->cwork[t];
		void *p = 0;
		pthread_join(w->pthread, &p);
		if (p)
			retval_of_thread = p;
	}

	/* clean up lists */
	while (!list_empty(&ctx->writelist_free)) {
		struct writelist *wl;
		struct list_head *entry;
		entry = list_first(&ctx->writelist_free);
		wl = list_entry(entry, struct writelist, node);
		free(wl->out.buf);
		list_del(&wl->node);
		free(wl);
	}

	return (size_t) retval_of_thread;
}

/* returns current uncompressed data size */
size_t LZ5MT_GetInsizeCCtx(LZ5MT_CCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->insize;
}

/* returns the current compressed data size */
size_t LZ5MT_GetOutsizeCCtx(LZ5MT_CCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->outsize;
}

/* returns the current compressed frames */
size_t LZ5MT_GetFramesCCtx(LZ5MT_CCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->curframe;
}

void LZ5MT_freeCCtx(LZ5MT_CCtx * ctx)
{
	if (!ctx)
		return;

	pthread_mutex_destroy(&ctx->read_mutex);
	pthread_mutex_destroy(&ctx->write_mutex);
	free(ctx->cwork);
	free(ctx);
	ctx = 0;

	return;
}
