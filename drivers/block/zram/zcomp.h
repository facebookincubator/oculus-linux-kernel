/*
 * Copyright (C) 2014 Sergey Senozhatsky.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#if IS_ENABLED(CONFIG_ZRAM_ZSTD_ADVANCED)
#include <linux/mm.h>
#include <linux/zstd.h>
#else
#include <linux/crypto.h>
#endif

#ifndef _ZCOMP_H_
#define _ZCOMP_H_

struct zcomp_strm {
	/* compression/decompression buffer */
	void *buffer;
#if IS_ENABLED(CONFIG_ZRAM_ZSTD_ADVANCED)
	ZSTD_parameters params;
	ZSTD_CCtx *cctx;
	ZSTD_DCtx *dctx;
	void *cctx_wksp;
	void *dctx_wksp;
#else
	struct crypto_comp *tfm;
#endif
};

/* dynamic per-device compression frontend */
struct zcomp {
	struct zcomp_strm * __percpu *stream;
	const char *name;
#if IS_ENABLED(CONFIG_ZRAM_ZSTD_ADVANCED)
	unsigned int compression_level;
#endif
	struct hlist_node node;
};

int zcomp_cpu_up_prepare(unsigned int cpu, struct hlist_node *node);
int zcomp_cpu_dead(unsigned int cpu, struct hlist_node *node);
ssize_t zcomp_available_show(const char *comp, char *buf);
bool zcomp_available_algorithm(const char *comp);

struct zcomp *zcomp_create(const char *comp, unsigned int compression_level);
void zcomp_destroy(struct zcomp *comp);

struct zcomp_strm *zcomp_stream_get(struct zcomp *comp);
void zcomp_stream_put(struct zcomp *comp);

int zcomp_compress(struct zcomp_strm *zstrm,
		const void *src, unsigned int *dst_len);

int zcomp_decompress(struct zcomp_strm *zstrm,
		const void *src, unsigned int src_len, void *dst);

bool zcomp_set_max_streams(struct zcomp *comp, int num_strm);
#endif /* _ZCOMP_H_ */
