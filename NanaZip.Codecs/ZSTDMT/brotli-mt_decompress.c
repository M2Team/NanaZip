
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

#include <brotli/decode.h>

#include "brotli-mt.h"
#include "memmt.h"
#include "threading.h"
#include "list.h"

/**
 * multi threaded brotli - multiple workers version
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
	BROTLIMT_DCtx *ctx;
	pthread_t pthread;
	BROTLIMT_Buffer in;
} cwork_t;

struct writelist;
struct writelist {
	size_t frame;
	BROTLIMT_Buffer out;
	struct list_head node;
};

struct BROTLIMT_DCtx_s {

	/* threads: 0, 1..BROTLIMT_THREAD_MAX */
	int threads;
	int threadsset;

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

	/* buffer read initially from begin of stream to distinguish content-based
	 * raw brotli from mt-brotli (if decompress of this buffer would succeed),
	 * hold frame skippable header, frame 0 read by BROTLIMT_decompressDCtx() before
	 * the container path was taken, so pt_read() does not read those bytes
	 * a second time. Also used in pt_read() to read header of next frames. */
	unsigned char prebuf[256+16];
	unsigned char *prepos;
	size_t presize;

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

BROTLIMT_DCtx *BROTLIMT_createDCtx(int threads, int threadsset, int inputsize)
{
	BROTLIMT_DCtx *ctx;
	int t;

	/* check threads value */
	if (!threadsset && threads > BROTLIMT_THREAD_MAX)
		threads = BROTLIMT_THREAD_MAX;
	if ((threads < 0 || threads > BROTLIMT_THREAD_MAX))
		return 0;

	/* allocate ctx */
	ctx = (BROTLIMT_DCtx *) malloc(sizeof(BROTLIMT_DCtx));
	if (!ctx)
		return 0;

	/* setup ctx */
	ctx->threads = threads;
	ctx->threadsset = threadsset;
	ctx->insize = 0;
	ctx->outsize = 0;
	ctx->frames = 0;
	ctx->curframe = 0;

	/* will be used for single stream only */
	if (inputsize)
		ctx->inputsize = inputsize;
	else
		ctx->inputsize = 1024 * 256; /* 256K buffer */

	/* Always initialize the mutexes/lists, even for threads==0: content
	 * based detection in BROTLIMT_decompressDCtx() may discover a
	 * brotli-mt container and promote ctx->threads to 1 after this call,
	 * at which point pt_read()/pt_write() need a valid read_mutex/
	 * write_mutex. The extra init/destroy cost for genuinely single
	 * threaded raw streams is negligible. */
	pthread_mutex_init(&ctx->read_mutex, NULL);
	pthread_mutex_init(&ctx->write_mutex, NULL);

	INIT_LIST_HEAD(&ctx->writelist_free);
	INIT_LIST_HEAD(&ctx->writelist_busy);
	INIT_LIST_HEAD(&ctx->writelist_done);

	if (threads <= 1)
		threads = 1;

	ctx->cwork = (cwork_t *) malloc(sizeof(cwork_t) * threads);
	if (!ctx->cwork)
		goto err_cwork;

	for (t = 0; t < threads; t++) {
		cwork_t *w = &ctx->cwork[t];
		w->ctx = ctx;
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
		return MT_ERROR(read_fail);
	case -2:
		return MT_ERROR(canceled);
	case -3:
		return MT_ERROR(memory_allocation);
	}

	/* XXX, some catch all other errors */
	return MT_ERROR(read_fail);
}

/**
 * pt_write - queue for decompressed output
 */
static size_t pt_write(BROTLIMT_DCtx * ctx, struct writelist *wl)
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
static size_t pt_read(BROTLIMT_DCtx * ctx, BROTLIMT_Buffer * in, size_t * frame, size_t * uncompressed)
{
	BROTLIMT_Buffer hdr;
	int rv;

	/* handle skippable frame (16 bytes) */
	pthread_mutex_lock(&ctx->read_mutex);
	hdr.buf = ctx->prebuf;
	hdr.size = 16;

	/* special case: small buffer was already read by BROTLIMT_decompressDCtx(),
	 * so we must read from there instead of input stream. */
	if (ctx->presize) {
		hdr.buf = ctx->prepos;
		if (ctx->presize >= 16) {
			ctx->prepos += 16;
			ctx->presize -= 16;
		} else {
			/* read missing part of header */
			BROTLIMT_Buffer hdr2;
			hdr2.buf = (char *)hdr.buf + ctx->presize;
			hdr2.size = 16 - ctx->presize;
			rv = ctx->fn_read(ctx->arg_read, &hdr2);
			if (rv != 0) {
				pthread_mutex_unlock(&ctx->read_mutex);
				return mt_error(rv);
			}
			hdr.size = ctx->presize + hdr2.size;
			ctx->presize = 0; // end of pre-buffer.
		}
	} else {
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
	}

	if (hdr.size < 16)
		goto error_read;
	if (MEM_readLE32(hdr.buf) != BROTLIMT_MAGIC_SKIPPABLE)
		goto error_data;

	/* check header data */
	if (MEM_readLE32((char *)hdr.buf + 4) != 8)
		goto error_data;
	if (MEM_readLE16((char *)hdr.buf + 12) != BROTLIMT_MAGICNUMBER)
		goto error_data;

	/* get uncompressed size for output buffer */
	{
		U16 hintsize = MEM_readLE16((char *)hdr.buf + 14);
		*uncompressed = hintsize << 16;
	}

	ctx->insize += 16;
	/* read new inputsize */
	{
		size_t toRead = MEM_readLE32((char *)hdr.buf + 8);
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

		/* if we still have content in pre-buffer */
		if (ctx->presize) {
			if (ctx->presize >= toRead) {
				memcpy(in->buf, ctx->prepos, toRead);
				ctx->prepos += toRead;
				ctx->presize -= toRead;
			} else {
				memcpy(in->buf, ctx->prepos, ctx->presize);
				/* read missing part from stream */
				BROTLIMT_Buffer buf2;
				buf2.buf = (char *)in->buf + ctx->presize;
				buf2.size = toRead - ctx->presize;
				rv = ctx->fn_read(ctx->arg_read, &buf2);
				if (rv != 0) {
					pthread_mutex_unlock(&ctx->read_mutex);
					return mt_error(rv);
				}
				in->size = ctx->presize + buf2.size;
				ctx->presize = 0; // end of pre-buffer.
			}
		} else {
			rv = ctx->fn_read(ctx->arg_read, in);
			/* generic read failure! */
			if (rv != 0) {
				pthread_mutex_unlock(&ctx->read_mutex);
				return mt_error(rv);
			}
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
	return MT_ERROR(data_error);
 error_read:
	pthread_mutex_unlock(&ctx->read_mutex);
	return MT_ERROR(read_fail);
 error_nomem:
	pthread_mutex_unlock(&ctx->read_mutex);
	return MT_ERROR(memory_allocation);
}

static void *pt_decompress(void *arg)
{
	cwork_t *w = (cwork_t *) arg;
	BROTLIMT_Buffer *in = &w->in;
	BROTLIMT_DCtx *ctx = w->ctx;
	size_t result = 0;
	struct writelist *wl;

	for (;;) {
		struct list_head *entry;
		BROTLIMT_Buffer *out;
		int rv;

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
				result = MT_ERROR(memory_allocation);
				/* @wl was never acquired, nothing to give back */
				goto error_unlock_nowl;
			}
			wl->out.buf = 0;
			wl->out.size = 0;
			wl->out.allocated = 0;
			list_add(&wl->node, &ctx->writelist_busy);
		}
		pthread_mutex_unlock(&ctx->write_mutex);
		out = &wl->out;

		/* zero should not happen here! */
		result = pt_read(ctx, in, &wl->frame, &wl->out.size);
		if (BROTLIMT_isError(result)) {
			goto error_lock;
		}

		if (in->size == 0)
			break;

		if (out->allocated < out->size) {
			if (out->allocated)
				out->buf = realloc(out->buf, out->size);
			else
				out->buf = malloc(out->size);
			if (!out->buf) {
				result = MT_ERROR(memory_allocation);
				goto error_lock;
			}
			out->allocated = out->size;
		}

		rv =
		    BrotliDecoderDecompress(in->size, in->buf, &out->size,
					    out->buf);

		if (rv != BROTLI_DECODER_RESULT_SUCCESS) {
			result = MT_ERROR(frame_decompress);
			goto error_lock;
		}

		/* write result */
		pthread_mutex_lock(&ctx->write_mutex);
		result = pt_write(ctx, wl);
		if (BROTLIMT_isError(result))
			goto error_unlock;
		pthread_mutex_unlock(&ctx->write_mutex);
	}

	/* everything is okay */
	result = 0;

 error_lock:
	pthread_mutex_lock(&ctx->write_mutex);
 error_unlock:
	list_move(&wl->node, &ctx->writelist_free);
 error_unlock_nowl:
	pthread_mutex_unlock(&ctx->write_mutex);
	if (in->allocated)
		free(in->buf);
	return (void *)result;
}

/*
 * st_finish_decompress - after st_decompress()'s main loop exits, check
 * the stream finished cleanly and flush whatever output is still sitting
 * in the output buffer. Split out (pre-existing logic, not new in this
 * change) so it doesn't add to the main loop function's complexity either.
 */
static size_t st_finish_decompress(BROTLIMT_DCtx *ctx, BROTLIMT_Buffer *out, uint8_t *next_out, BrotliDecoderResult bres)
{
	int rv;

	if (bres != BROTLI_DECODER_RESULT_SUCCESS)
		return MT_ERROR(data_error); // corrupt input

	out->size = next_out - (uint8_t *)out->buf;
	if (out->size != 0) {
		rv = ctx->fn_write(ctx->arg_write, out);
		if (rv != 0)
			return mt_error(rv);
	}

	return 0;
}

/*
 * single threaded (standard brotli stream, without header/mt-frames)
 *
 * @prefix/@prefixSize: bytes already consumed from the stream (by
 * BROTLIMT_decompressDCtx() while probing for the brotli-mt container
 * magic) that must be fed back as the beginning of the decoded data;
 */
static size_t st_decompress(BROTLIMT_DCtx *ctx, const unsigned char *prefix, size_t prefixSize)
{
	BrotliDecoderResult bres;
	cwork_t *w = &ctx->cwork[0];
	BROTLIMT_Buffer Out;
	BROTLIMT_Buffer *out = &Out;
	BROTLIMT_Buffer *in = &w->in;
	BrotliDecoderState *state;
	const uint8_t* next_in;
	uint8_t* next_out;
	int rv;
	size_t retval = 0;
	size_t allocSize = ctx->inputsize;

	/* allocate space for input buffer */
	in->allocated = allocSize;
	in->size = allocSize;
	in->buf = malloc(in->size);
	if (!in->buf)
		return MT_ERROR(memory_allocation);

	/* allocate space for output buffer */
	out->allocated = out->size = ctx->inputsize * 4;
	out->buf = malloc(out->size);
	if (!out->buf) {
		free(in->buf);
		return MT_ERROR(memory_allocation);
	}
	next_out = out->buf;

	state = BrotliDecoderCreateInstance(NULL, NULL, NULL);
	if (!state) {
		free(in->buf);
		free(out->buf);
	  return MT_ERROR(memory_allocation);
	}
	BrotliDecoderSetParameter(state, BROTLI_DECODER_PARAM_LARGE_WINDOW, 1);

	if (prefixSize) {
		/* decompress read prefix firstly, then start the loop. */
		next_in = (uint8_t*)prefix;
		in->size = prefixSize;
		bres = BrotliDecoderDecompressStream(state, &in->size, &next_in, &out->size, &next_out, 0);
		if (BROTLIMT_isError(bres)) {
			retval = MT_ERROR(data_error);
			goto done;
		}
	} else {
		bres = BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT; // read buffer
	}

	while (1) {

		if (bres == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
			ctx->frames++; // signal for mt-brotli content-based detection, it passed 1st block
			in->size = in->allocated;
			rv = ctx->fn_read(ctx->arg_read, in);
			if (in->size == 0) break;
			if (rv != 0) {
				retval = mt_error(rv);
				goto done;
			}
			next_in = in->buf;
		} else if (bres == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
			out->size = next_out - (uint8_t*)out->buf;
			rv = ctx->fn_write(ctx->arg_write, out);
			if (rv != 0) {
				retval = mt_error(rv);
				goto done;
			}
			next_out = out->buf;
		} else {
			break;
		}

		bres = BrotliDecoderDecompressStream(state, &in->size, &next_in, &out->size, &next_out, 0);
	}

	retval = st_finish_decompress(ctx, out, next_out, bres);

 done:
		free(in->buf);
		free(out->buf);
		BrotliDecoderDestroyInstance(state);
		return retval;
}

size_t BROTLIMT_decompressDCtx(BROTLIMT_DCtx * ctx, BROTLIMT_RdWr_t * rdwr)
{
	size_t ret;
	int t, rv;
	cwork_t *w = &ctx->cwork[0];
	BROTLIMT_Buffer *in = &w->in;

	if (!ctx)
		return MT_ERROR(compressionParameter_unsupported);

	/* init reading and writing functions */
	ctx->fn_read = rdwr->fn_read;
	ctx->fn_write = rdwr->fn_write;
	ctx->arg_read = rdwr->arg_read;
	ctx->arg_write = rdwr->arg_write;

	/* Firstly, to eliminate a risk of misdetection of mt-brotli for raw brotli
	 * stream, we'd try to decompress using st-brotli (without mt-brotli
	 * container detection), and if it would fail by this first buffer without
	 * further reads, we'd try with mt-brotli decompression. */

	ctx->presize = 0;
	if (!ctx->threadsset || !ctx->threads) {

		in->buf = ctx->prebuf;
		in->size = sizeof(ctx->prebuf) - 16; /* reserve up to 16 bytes to be able
																					* read parts of next header later. */
		rv = ctx->fn_read(ctx->arg_read, in);
		if (rv != 0)
			return mt_error(rv);
		ctx->presize = in->size;

		ret = st_decompress(ctx, in->buf, in->size);

		if (
			ret == 0 ||													/* no error */
			ctx->frames ||											/* read more data (passed 1st block) */
			(ctx->threadsset > 0 && !ctx->threads)	/* forced single-threaded (in .br) */
		) {
			return ret;
		}
	}

	ctx->prepos = ctx->prebuf;
	ret = 0;

	/*
	 * brotli-mt detected or selected: make sure we actually use the
	 * framed/multithreaded decode path, even testing/extracting without -mmt.
	 * read_mutex/write_mutex/writelist_* are always initialized in
	 * BROTLIMT_createDCtx() now, so this promotion is safe even when the
	 * context was originally created with threads==0.
	 */
	if (ctx->threads <= 0)
		ctx->threads = 1;

	/* mark unused */
	in->buf = 0;
	in->size = 0;
	in->allocated = 0;

	/* single threaded, but with known sizes */
	if (ctx->threads == 1) {
		/* no pthread_create() needed! */
		void *p = pt_decompress(w);
		if (p)
			ret = (size_t)p;
		goto done;
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
			ret = (size_t)p;
	}

 done:
	/* move remaining done/busy entries to free list */
	while (!list_empty(&ctx->writelist_done)) {
		struct list_head *entry = list_first(&ctx->writelist_done);
		list_move(entry, &ctx->writelist_free);
	}
	while (!list_empty(&ctx->writelist_busy)) {
		struct list_head* entry = list_first(&ctx->writelist_busy);
		list_move(entry, &ctx->writelist_free);
	}
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

	return ret;
}

/* returns current uncompressed data size */
size_t BROTLIMT_GetInsizeDCtx(BROTLIMT_DCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->insize;
}

/* returns the current compressed data size */
size_t BROTLIMT_GetOutsizeDCtx(BROTLIMT_DCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->outsize;
}

/* returns the current compressed frames */
size_t BROTLIMT_GetFramesDCtx(BROTLIMT_DCtx * ctx)
{
	if (!ctx)
		return 0;

	return ctx->curframe;
}

void BROTLIMT_freeDCtx(BROTLIMT_DCtx * ctx)
{
	if (!ctx)
		return;

	/* read_mutex/write_mutex are now always initialized in
	 * BROTLIMT_createDCtx(), regardless of the initial threads value
	 * (see comment there), so always destroy them here too. */
	pthread_mutex_destroy(&ctx->read_mutex);
	pthread_mutex_destroy(&ctx->write_mutex);
	free(ctx->cwork);
	free(ctx);
	ctx = 0;

	return;
}
