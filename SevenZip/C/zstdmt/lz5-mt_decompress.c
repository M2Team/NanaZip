
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
	LZ5MT_DCtx *ctx;
	pthread_t pthread;
	LZ5MT_Buffer in;
	LZ5F_decompressionContext_t dctx;
} cwork_t;

struct writelist;
struct writelist {
	size_t frame;
	LZ5MT_Buffer out;
	struct list_head node;
};

struct LZ5MT_DCtx_s {

	/* threads: 1..LZ5MT_THREAD_MAX */
	int threads;

	/* should be used for read from input */
	size_t inputsize;

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
 * Decompression
 ****************************************/

LZ5MT_DCtx *LZ5MT_createDCtx(int threads, int inputsize)
{
	LZ5MT_DCtx *ctx;
	int t;

	/* allocate ctx */
	ctx = (LZ5MT_DCtx *) malloc(sizeof(LZ5MT_DCtx));
	if (!ctx)
		return 0;

	/* check threads value */
	if (threads < 1 || threads > LZ5MT_THREAD_MAX)
		return 0;

	/* setup ctx */
	ctx->threads = threads;
	ctx->insize = 0;
	ctx->outsize = 0;
	ctx->frames = 0;
	ctx->curframe = 0;

	/* will be used for single stream only */
	if (inputsize)
		ctx->inputsize = inputsize;
	else
		ctx->inputsize = 1024 * 64;	/* 64K buffer */

	pthread_mutex_init(&ctx->read_mutex, NULL);
	pthread_mutex_init(&ctx->write_mutex, NULL);

	INIT_LIST_HEAD(&ctx->writelist_free);
	INIT_LIST_HEAD(&ctx->writelist_busy);
	INIT_LIST_HEAD(&ctx->writelist_done);

	ctx->cwork = (cwork_t *) malloc(sizeof(cwork_t) * threads);
	if (!ctx->cwork)
		goto err_cwork;

	for (t = 0; t < threads; t++) {
		cwork_t *w = &ctx->cwork[t];
		w->ctx = ctx;

		/* setup thread work */
		LZ5F_createDecompressionContext(&w->dctx, LZ5F_VERSION);
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

	/* XXX, some catch all other errors */
	return ERROR(read_fail);
}

/**
 * pt_write - queue for decompressed output
 */
static size_t pt_write(LZ5MT_DCtx * ctx, struct writelist *wl)
{
	struct list_head *entry;

	/* move the entry to the done list */
	list_move(&wl->node, &ctx->writelist_done);
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

/**
 * pt_read - read compressed output
 */
static size_t pt_read(LZ5MT_DCtx * ctx, LZ5MT_Buffer * in, size_t * frame)
{
	unsigned char hdrbuf[12];
	LZ5MT_Buffer hdr;
	int rv;

	/* read skippable frame (8 or 12 bytes) */
	pthread_mutex_lock(&ctx->read_mutex);

	/* special case, first 4 bytes already read */
	if (ctx->frames == 0) {
		hdr.buf = hdrbuf + 4;
		hdr.size = 8;
		rv = ctx->fn_read(ctx->arg_read, &hdr);
		if (rv != 0) {
			pthread_mutex_unlock(&ctx->read_mutex);
			return mt_error(rv);
		}
		if (hdr.size != 8)
			goto error_read;
		hdr.buf = hdrbuf;
	} else {
		hdr.buf = hdrbuf;
		hdr.size = 12;
		rv = ctx->fn_read(ctx->arg_read, &hdr);
		if (rv != 0) {
			pthread_mutex_unlock(&ctx->read_mutex);
			return mt_error(rv);
		}
		/* eof reached ? */
		if (hdr.size == 0) {
			pthread_mutex_unlock(&ctx->read_mutex);
			in->size = 0;
			return 0;
		}
		if (hdr.size != 12)
			goto error_read;
		if (MEM_readLE32((unsigned char *)hdr.buf + 0) !=
		    LZ5FMT_MAGIC_SKIPPABLE)
			goto error_data;
	}

	/* check header data */
	if (MEM_readLE32((unsigned char *)hdr.buf + 4) != 4)
		goto error_data;

	ctx->insize += 12;
	/* read new inputsize */
	{
		size_t toRead = MEM_readLE32((unsigned char *)hdr.buf + 8);
		if (in->allocated < toRead) {
			/* need bigger input buffer */
			if (in->allocated)
				in->buf = realloc(in->buf, toRead);
			else
				in->buf = malloc(toRead);
			if (!in->buf)
				goto error_nomem;
			in->allocated = toRead;
		}

		in->size = toRead;
		rv = ctx->fn_read(ctx->arg_read, in);
		/* generic read failure! */
		if (rv != 0) {
			pthread_mutex_unlock(&ctx->read_mutex);
			return mt_error(rv);
		}
		/* needed more bytes! */
		if (in->size != toRead)
			goto error_data;

		ctx->insize += in->size;
	}
	*frame = ctx->frames++;
	pthread_mutex_unlock(&ctx->read_mutex);

	/* done, no error */
	return 0;

 error_data:
	pthread_mutex_unlock(&ctx->read_mutex);
	return ERROR(data_error);
 error_read:
	pthread_mutex_unlock(&ctx->read_mutex);
	return ERROR(read_fail);
 error_nomem:
	pthread_mutex_unlock(&ctx->read_mutex);
	return ERROR(memory_allocation);
}

static void *pt_decompress(void *arg)
{
	cwork_t *w = (cwork_t *) arg;
	LZ5MT_Buffer *in = &w->in;
	LZ5MT_DCtx *ctx = w->ctx;
	size_t result = 0;
	struct writelist *wl;

	for (;;) {
		struct list_head *entry;
		LZ5MT_Buffer *out;

		/* allocate space for new output */
		pthread_mutex_lock(&ctx->write_mutex);
		if (!list_empty(&ctx->writelist_free)) {
			/* take unused entry */
			entry = list_first(&ctx->writelist_free);
			wl = list_entry(entry, struct writelist, node);
			list_move(entry, &ctx->writelist_busy);
		} else {
			/* allocate new one */
			wl = (struct writelist *)
			    malloc(sizeof(struct writelist));
			if (!wl) {
				result = ERROR(memory_allocation);
				goto error_unlock;
			}
			wl->out.buf = 0;
			wl->out.size = 0;
			wl->out.allocated = 0;
			list_add(&wl->node, &ctx->writelist_busy);
		}
		pthread_mutex_unlock(&ctx->write_mutex);
		out = &wl->out;

		/* zero should not happen here! */
		result = pt_read(ctx, in, &wl->frame);
		if (LZ5MT_isError(result)) {
			list_move(&wl->node, &ctx->writelist_free);
			goto error_lock;
		}

		if (in->size == 0)
			break;

		/* mininmal frame */
		if (in->size < 40 && ctx->frames == 1) {
			out->size = 1024 * 64;
		} else {
			/* get frame size for output buffer */
			unsigned char *src = (unsigned char *)in->buf + 6;
			out->size = (size_t) MEM_readLE64(src);
		}

		if (out->allocated < out->size) {
			if (out->allocated)
				out->buf = realloc(out->buf, out->size);
			else
				out->buf = malloc(out->size);
			if (!out->buf) {
				result = ERROR(memory_allocation);
				goto error_lock;
			}
			out->allocated = out->size;
		}

		result =
		    LZ5F_decompress(w->dctx, out->buf, &out->size,
				    in->buf, &in->size, 0);

		if (LZ5F_isError(result)) {
			lz5mt_errcode = result;
			result = ERROR(compression_library);
			goto error_lock;
		}

		if (result != 0) {
			result = ERROR(frame_decompress);
			goto error_lock;
		}

		/* write result */
		pthread_mutex_lock(&ctx->write_mutex);
		result = pt_write(ctx, wl);
		if (LZ5MT_isError(result))
			goto error_unlock;
		pthread_mutex_unlock(&ctx->write_mutex);
	}

	/* everything is okay */
	pthread_mutex_lock(&ctx->write_mutex);
	list_move(&wl->node, &ctx->writelist_free);
	pthread_mutex_unlock(&ctx->write_mutex);
	if (in->allocated)
		free(in->buf);
	return 0;

 error_lock:
	pthread_mutex_lock(&ctx->write_mutex);
 error_unlock:
	list_move(&wl->node, &ctx->writelist_free);
	pthread_mutex_unlock(&ctx->write_mutex);
	if (in->allocated)
		free(in->buf);
	return (void *)result;
}

/* single threaded */
static size_t st_decompress(void *arg)
{
	LZ5MT_DCtx *ctx = (LZ5MT_DCtx *) arg;
	LZ5F_errorCode_t nextToLoad = 0;
	cwork_t *w = &ctx->cwork[0];
	LZ5MT_Buffer Out;
	LZ5MT_Buffer *out = &Out;
	LZ5MT_Buffer *in = &w->in;
	void *magic = in->buf;
	size_t pos = 0;
	int rv;

	/* allocate space for input buffer */
	in->size = ctx->inputsize;
	in->buf = malloc(in->size);
	if (!in->buf)
		return ERROR(memory_allocation);

	/* allocate space for output buffer */
	out->size = ctx->inputsize;
	out->buf = malloc(out->size);
	if (!out->buf) {
		free(in->buf);
		return ERROR(memory_allocation);
	}

	/* we have read already 4 bytes */
	in->size = 4;
	memcpy(in->buf, magic, in->size);

	nextToLoad =
	    LZ5F_decompress(w->dctx, out->buf, &pos, in->buf, &in->size, 0);
	if (LZ5F_isError(nextToLoad)) {
		free(in->buf);
		free(out->buf);
		return ERROR(compression_library);
	}

	for (; nextToLoad; pos = 0) {
		if (nextToLoad > ctx->inputsize)
			nextToLoad = ctx->inputsize;

		/* read new input */
		in->size = nextToLoad;
		rv = ctx->fn_read(ctx->arg_read, in);
		if (rv != 0) {
			free(in->buf);
			free(out->buf);
			return mt_error(rv);
		}

		/* done, eof reached */
		if (in->size == 0)
			break;

		/* still to read, or still to flush */
		while ((pos < in->size) || (out->size == ctx->inputsize)) {
			size_t remaining = in->size - pos;
			out->size = ctx->inputsize;

			/* decompress */
			nextToLoad =
			    LZ5F_decompress(w->dctx, out->buf, &out->size,
					    (unsigned char *)in->buf + pos,
					    &remaining, NULL);
			if (LZ5F_isError(nextToLoad)) {
				free(in->buf);
				free(out->buf);
				return ERROR(compression_library);
			}

			/* have some output */
			if (out->size) {
				rv = ctx->fn_write(ctx->arg_write, out);
				if (rv != 0) {
					free(in->buf);
					free(out->buf);
					return mt_error(rv);
				}
			}

			if (nextToLoad == 0)
				break;

			pos += remaining;
		}
	}

	/* no error */
	free(out->buf);
	free(in->buf);
	return 0;
}

size_t LZ5MT_decompressDCtx(LZ5MT_DCtx * ctx, LZ5MT_RdWr_t * rdwr)
{
	unsigned char buf[4];
	int t, rv;
	cwork_t *w = &ctx->cwork[0];
	LZ5MT_Buffer *in = &w->in;
	void *retval_of_thread = 0;

	if (!ctx)
		return ERROR(compressionParameter_unsupported);

	/* init reading and writing functions */
	ctx->fn_read = rdwr->fn_read;
	ctx->fn_write = rdwr->fn_write;
	ctx->arg_read = rdwr->arg_read;
	ctx->arg_write = rdwr->arg_write;

	/* check for LZ5FMT_MAGIC_SKIPPABLE */
	in->buf = buf;
	in->size = 4;
	rv = ctx->fn_read(ctx->arg_read, in);
	if (rv != 0)
		return mt_error(rv);
	if (in->size != 4)
		return ERROR(data_error);

	/* single threaded with unknown sizes */
	if (MEM_readLE32(buf) != LZ5FMT_MAGIC_SKIPPABLE) {

		/* look for correct magic */
		if (MEM_readLE32(buf) != LZ5FMT_MAGICNUMBER)
			return ERROR(data_error);

		/* decompress single threaded */
		return st_decompress(ctx);
	}

	/* mark unused */
	in->buf = 0;
	in->size = 0;
	in->allocated = 0;

	/* single threaded, but with known sizes */
	if (ctx->threads == 1) {
		/* no pthread_create() needed! */
		void *p = pt_decompress(w);
		if (p)
			return (size_t) p;
		goto okay;
	}

	/* multi threaded */
	for (t = 0; t < ctx->threads; t++) {
		cwork_t *wt = &ctx->cwork[t];
		wt->in.buf = 0;
		wt->in.size = 0;
		wt->in.allocated = 0;
		pthread_create(&wt->pthread, NULL, pt_decompress, wt);
	}

	/* wait for all workers */
	for (t = 0; t < ctx->threads; t++) {
		cwork_t *wt = &ctx->cwork[t];
		void *p = 0;
		pthread_join(wt->pthread, &p);
		if (p)
			retval_of_thread = p;
	}

 okay:
	/* clean up the buffers */
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
size_t LZ5MT_GetInsizeDCtx(LZ5MT_DCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->insize;
}

/* returns the current compressed data size */
size_t LZ5MT_GetOutsizeDCtx(LZ5MT_DCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->outsize;
}

/* returns the current compressed frames */
size_t LZ5MT_GetFramesDCtx(LZ5MT_DCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->curframe;
}

void LZ5MT_freeDCtx(LZ5MT_DCtx * ctx)
{
	int t;

	if (!ctx)
		return;

	for (t = 0; t < ctx->threads; t++) {
		cwork_t *w = &ctx->cwork[t];
		LZ5F_freeDecompressionContext(w->dctx);
	}

	pthread_mutex_destroy(&ctx->read_mutex);
	pthread_mutex_destroy(&ctx->write_mutex);
	free(ctx->cwork);
	free(ctx);
	ctx = 0;

	return;
}
