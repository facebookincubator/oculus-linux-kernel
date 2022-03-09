/* Copyright (c) 2012, 2015-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/msm_ion.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/major.h>
#include <linux/dma-buf.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/regulator/consumer.h>
#include <media/msm_media_info.h>
#include <linux/videodev2.h>

#include "sde_rotator_util.h"
#include "sde_rotator_smmu.h"
#include "sde_rotator_debug.h"

#define Y_TILEWIDTH     48
#define Y_TILEHEIGHT    4
#define UV_TILEWIDTH    48
#define UV_TILEHEIGHT   8
#define TILEWIDTH_SIZE  64
#define TILEHEIGHT_SIZE 4

void sde_mdp_get_v_h_subsample_rate(u8 chroma_sample,
		u8 *v_sample, u8 *h_sample)
{
	switch (chroma_sample) {
	case SDE_MDP_CHROMA_H2V1:
		*v_sample = 1;
		*h_sample = 2;
		break;
	case SDE_MDP_CHROMA_H1V2:
		*v_sample = 2;
		*h_sample = 1;
		break;
	case SDE_MDP_CHROMA_420:
		*v_sample = 2;
		*h_sample = 2;
		break;
	default:
		*v_sample = 1;
		*h_sample = 1;
		break;
	}
}

void sde_rot_intersect_rect(struct sde_rect *res_rect,
	const struct sde_rect *dst_rect,
	const struct sde_rect *sci_rect)
{
	int l = max(dst_rect->x, sci_rect->x);
	int t = max(dst_rect->y, sci_rect->y);
	int r = min((dst_rect->x + dst_rect->w), (sci_rect->x + sci_rect->w));
	int b = min((dst_rect->y + dst_rect->h), (sci_rect->y + sci_rect->h));

	if (r < l || b < t)
		*res_rect = (struct sde_rect){0, 0, 0, 0};
	else
		*res_rect = (struct sde_rect){l, t, (r-l), (b-t)};
}

void sde_rot_crop_rect(struct sde_rect *src_rect,
	struct sde_rect *dst_rect,
	const struct sde_rect *sci_rect)
{
	struct sde_rect res;

	sde_rot_intersect_rect(&res, dst_rect, sci_rect);

	if (res.w && res.h) {
		if ((res.w != dst_rect->w) || (res.h != dst_rect->h)) {
			src_rect->x = src_rect->x + (res.x - dst_rect->x);
			src_rect->y = src_rect->y + (res.y - dst_rect->y);
			src_rect->w = res.w;
			src_rect->h = res.h;
		}
		*dst_rect = (struct sde_rect)
			{(res.x - sci_rect->x), (res.y - sci_rect->y),
			res.w, res.h};
	}
}

/*
 * sde_rect_cmp() - compares two rects
 * @rect1 - rect value to compare
 * @rect2 - rect value to compare
 *
 * Returns 1 if the rects are same, 0 otherwise.
 */
int sde_rect_cmp(struct sde_rect *rect1, struct sde_rect *rect2)
{
	return rect1->x == rect2->x && rect1->y == rect2->y &&
	       rect1->w == rect2->w && rect1->h == rect2->h;
}

/*
 * sde_rect_overlap_check() - compare two rects and check if they overlap
 * @rect1 - rect value to compare
 * @rect2 - rect value to compare
 *
 * Returns true if rects overlap, false otherwise.
 */
bool sde_rect_overlap_check(struct sde_rect *rect1, struct sde_rect *rect2)
{
	u32 rect1_left = rect1->x, rect1_right = rect1->x + rect1->w;
	u32 rect1_top = rect1->y, rect1_bottom = rect1->y + rect1->h;
	u32 rect2_left = rect2->x, rect2_right = rect2->x + rect2->w;
	u32 rect2_top = rect2->y, rect2_bottom = rect2->y + rect2->h;

	if ((rect1_right <= rect2_left) ||
	    (rect1_left >= rect2_right) ||
	    (rect1_bottom <= rect2_top) ||
	    (rect1_top >= rect2_bottom))
		return false;

	return true;
}

int sde_mdp_get_rau_strides(u32 w, u32 h,
			       struct sde_mdp_format_params *fmt,
			       struct sde_mdp_plane_sizes *ps)
{
	if (fmt->is_yuv) {
		ps->rau_cnt = DIV_ROUND_UP(w, 64);
		ps->ystride[0] = 64 * 4;
		ps->rau_h[0] = 4;
		ps->rau_h[1] = 2;
		if (fmt->chroma_sample == SDE_MDP_CHROMA_H1V2)
			ps->ystride[1] = 64 * 2;
		else if (fmt->chroma_sample == SDE_MDP_CHROMA_H2V1) {
			ps->ystride[1] = 32 * 4;
			ps->rau_h[1] = 4;
		} else
			ps->ystride[1] = 32 * 2;

		/* account for both chroma components */
		ps->ystride[1] <<= 1;
	} else if (fmt->fetch_planes == SDE_MDP_PLANE_INTERLEAVED) {
		ps->rau_cnt = DIV_ROUND_UP(w, 32);
		ps->ystride[0] = 32 * 4 * fmt->bpp;
		ps->ystride[1] = 0;
		ps->rau_h[0] = 4;
		ps->rau_h[1] = 0;
	} else  {
		SDEROT_ERR("Invalid format=%d\n", fmt->format);
		return -EINVAL;
	}

	ps->ystride[0] *= ps->rau_cnt;
	ps->ystride[1] *= ps->rau_cnt;
	ps->num_planes = 2;

	SDEROT_DBG("BWC rau_cnt=%d strides={%d,%d} heights={%d,%d}\n",
		ps->rau_cnt, ps->ystride[0], ps->ystride[1],
		ps->rau_h[0], ps->rau_h[1]);

	return 0;
}

static int sde_mdp_get_a5x_plane_size(struct sde_mdp_format_params *fmt,
	u32 width, u32 height, struct sde_mdp_plane_sizes *ps)
{
	int rc = 0;

	if (sde_mdp_is_nv12_8b_format(fmt)) {
		ps->num_planes = 2;
		/* Y bitstream stride and plane size */
		ps->ystride[0] = ALIGN(width, 128);
		ps->plane_size[0] = ALIGN(ps->ystride[0] * ALIGN(height, 32),
					4096);

		/* CbCr bitstream stride and plane size */
		ps->ystride[1] = ALIGN(width, 128);
		ps->plane_size[1] = ALIGN(ps->ystride[1] *
			ALIGN(height / 2, 32), 4096);

		if (!sde_mdp_is_ubwc_format(fmt))
			goto done;

		ps->num_planes += 2;

		/* Y meta data stride and plane size */
		ps->ystride[2] = ALIGN(DIV_ROUND_UP(width, 32), 64);
		ps->plane_size[2] = ALIGN(ps->ystride[2] *
			ALIGN(DIV_ROUND_UP(height, 8), 16), 4096);

		/* CbCr meta data stride and plane size */
		ps->ystride[3] = ALIGN(DIV_ROUND_UP(width / 2, 16), 64);
		ps->plane_size[3] = ALIGN(ps->ystride[3] *
			ALIGN(DIV_ROUND_UP(height / 2, 8), 16), 4096);
	} else if (sde_mdp_is_p010_format(fmt)) {
		ps->num_planes = 2;
		/* Y bitstream stride and plane size */
		ps->ystride[0] = ALIGN(width * 2, 256);
		ps->plane_size[0] = ALIGN(ps->ystride[0] * ALIGN(height, 16),
					4096);

		/* CbCr bitstream stride and plane size */
		ps->ystride[1] = ALIGN(width * 2, 256);
		ps->plane_size[1] = ALIGN(ps->ystride[1] *
			ALIGN(height / 2, 16), 4096);

		if (!sde_mdp_is_ubwc_format(fmt))
			goto done;

		ps->num_planes += 2;

		/* Y meta data stride and plane size */
		ps->ystride[2] = ALIGN(DIV_ROUND_UP(width, 32), 64);
		ps->plane_size[2] = ALIGN(ps->ystride[2] *
			ALIGN(DIV_ROUND_UP(height, 4), 16), 4096);

		/* CbCr meta data stride and plane size */
		ps->ystride[3] = ALIGN(DIV_ROUND_UP(width / 2, 16), 64);
		ps->plane_size[3] = ALIGN(ps->ystride[3] *
			ALIGN(DIV_ROUND_UP(height / 2, 4), 16), 4096);
	} else if (sde_mdp_is_tp10_format(fmt)) {
		u32 yWidth   = sde_mdp_general_align(width, 192);
		u32 yHeight  = ALIGN(height, 16);
		u32 uvWidth  = sde_mdp_general_align(width, 192);
		u32 uvHeight = ALIGN(height, 32);

		ps->num_planes = 2;

		/* Y bitstream stride and plane size */
		ps->ystride[0]    = yWidth * TILEWIDTH_SIZE / Y_TILEWIDTH;
		ps->plane_size[0] = ALIGN(ps->ystride[0] *
				(yHeight * TILEHEIGHT_SIZE / Y_TILEHEIGHT),
				4096);

		/* CbCr bitstream stride and plane size */
		ps->ystride[1]    = uvWidth * TILEWIDTH_SIZE / UV_TILEWIDTH;
		ps->plane_size[1] = ALIGN(ps->ystride[1] *
				(uvHeight * TILEHEIGHT_SIZE / UV_TILEHEIGHT),
				4096);

		if (!sde_mdp_is_ubwc_format(fmt))
			goto done;

		ps->num_planes += 2;

		/* Y meta data stride and plane size */
		ps->ystride[2]    = ALIGN(yWidth / Y_TILEWIDTH, 64);
		ps->plane_size[2] = ALIGN(ps->ystride[2] *
				ALIGN((yHeight / Y_TILEHEIGHT), 16), 4096);

		/* CbCr meta data stride and plane size */
		ps->ystride[3]    = ALIGN(uvWidth / UV_TILEWIDTH, 64);
		ps->plane_size[3] = ALIGN(ps->ystride[3] *
				ALIGN((uvHeight / UV_TILEHEIGHT), 16), 4096);
	} else if (sde_mdp_is_rgb_format(fmt)) {
		uint32_t stride_alignment, bpp, aligned_bitstream_width;

		if (fmt->format == SDE_PIX_FMT_RGB_565_UBWC) {
			stride_alignment = 128;
			bpp = 2;
		} else {
			stride_alignment = 64;
			bpp = 4;
		}

		ps->num_planes = 1;

		/* RGB bitstream stride and plane size */
		aligned_bitstream_width = ALIGN(width, stride_alignment);
		ps->ystride[0] = aligned_bitstream_width * bpp;
		ps->plane_size[0] = ALIGN(bpp * aligned_bitstream_width *
			ALIGN(height, 16), 4096);

		if (!sde_mdp_is_ubwc_format(fmt))
			goto done;

		ps->num_planes += 1;

		/* RGB meta data stride and plane size */
		ps->ystride[2] = ALIGN(DIV_ROUND_UP(aligned_bitstream_width,
			16), 64);
		ps->plane_size[2] = ALIGN(ps->ystride[2] *
			ALIGN(DIV_ROUND_UP(height, 4), 16), 4096);
	} else {
		SDEROT_ERR("%s: UBWC format not supported for fmt:%d\n",
			__func__, fmt->format);
		rc = -EINVAL;
	}
done:
	return rc;
}

int sde_mdp_get_plane_sizes(struct sde_mdp_format_params *fmt, u32 w, u32 h,
	struct sde_mdp_plane_sizes *ps, u32 bwc_mode, bool rotation)
{
	int i, rc = 0;
	u32 bpp;

	if (ps == NULL)
		return -EINVAL;

	if ((w > SDE_ROT_MAX_IMG_WIDTH) || (h > SDE_ROT_MAX_IMG_HEIGHT))
		return -ERANGE;

	bpp = fmt->bpp;
	memset(ps, 0, sizeof(struct sde_mdp_plane_sizes));

	if (sde_mdp_is_tilea5x_format(fmt)) {
		rc = sde_mdp_get_a5x_plane_size(fmt, w, h, ps);
	} else if (bwc_mode) {
		u32 height, meta_size;

		rc = sde_mdp_get_rau_strides(w, h, fmt, ps);
		if (rc)
			return rc;

		height = DIV_ROUND_UP(h, ps->rau_h[0]);
		meta_size = DIV_ROUND_UP(ps->rau_cnt, 8);
		ps->ystride[1] += meta_size;
		ps->ystride[0] += ps->ystride[1] + meta_size;
		ps->plane_size[0] = ps->ystride[0] * height;

		ps->ystride[1] = 2;
		ps->plane_size[1] = 2 * ps->rau_cnt * height;

		SDEROT_DBG("BWC data stride=%d size=%d meta size=%d\n",
			ps->ystride[0], ps->plane_size[0], ps->plane_size[1]);
	} else {
		if (fmt->fetch_planes == SDE_MDP_PLANE_INTERLEAVED) {
			ps->num_planes = 1;
			ps->plane_size[0] = w * h * bpp;
			ps->ystride[0] = w * bpp;
		} else if (fmt->format == SDE_PIX_FMT_Y_CBCR_H2V2_VENUS ||
			fmt->format == SDE_PIX_FMT_Y_CRCB_H2V2_VENUS ||
			fmt->format == SDE_PIX_FMT_Y_CBCR_H2V2_P010_VENUS) {

			int cf;

			switch (fmt->format) {
			case SDE_PIX_FMT_Y_CBCR_H2V2_VENUS:
				cf = COLOR_FMT_NV12;
				break;
			case SDE_PIX_FMT_Y_CRCB_H2V2_VENUS:
				cf = COLOR_FMT_NV21;
				break;
			case SDE_PIX_FMT_Y_CBCR_H2V2_P010_VENUS:
				cf = COLOR_FMT_P010;
				break;
			default:
				SDEROT_ERR("unknown color format %d\n",
						fmt->format);
				return -EINVAL;
			}

			ps->num_planes = 2;
			ps->ystride[0] = VENUS_Y_STRIDE(cf, w);
			ps->ystride[1] = VENUS_UV_STRIDE(cf, w);
			ps->plane_size[0] = VENUS_Y_SCANLINES(cf, h) *
				ps->ystride[0];
			ps->plane_size[1] = VENUS_UV_SCANLINES(cf, h) *
				ps->ystride[1];
		} else if (fmt->format == SDE_PIX_FMT_Y_CBCR_H2V2_P010) {
			/*
			 * |<---Y1--->000000<---Y0--->000000|  Plane0
			 * |rrrrrrrrrr000000bbbbbbbbbb000000|  Plane1
			 * |--------------------------------|
			 *  33222222222211111111110000000000  Bit
			 *  10987654321098765432109876543210  Location
			 */
			ps->num_planes = 2;
			ps->ystride[0] = w * 2;
			ps->ystride[1] = w * 2;
			ps->plane_size[0] = ps->ystride[0] * h;
			ps->plane_size[1] = ps->ystride[1] * h / 2;
		} else if (fmt->format == SDE_PIX_FMT_Y_CBCR_H2V2_TP10) {
			u32 yWidth   = sde_mdp_general_align(w, 192);
			u32 yHeight  = ALIGN(h, 16);
			u32 uvWidth  = sde_mdp_general_align(w, 192);
			u32 uvHeight = (ALIGN(h, 32)) / 2;

			ps->num_planes = 2;

			ps->ystride[0] = (yWidth / 3) * 4;
			ps->ystride[1] = (uvWidth / 3) * 4;
			ps->plane_size[0] = ALIGN(ps->ystride[0] * yHeight,
					4096);
			ps->plane_size[1] = ALIGN(ps->ystride[1] * uvHeight,
					4096);
		} else {
			u8 v_subsample, h_subsample, stride_align, height_align;
			u32 chroma_samp;

			chroma_samp = fmt->chroma_sample;

			sde_mdp_get_v_h_subsample_rate(chroma_samp,
				&v_subsample, &h_subsample);

			switch (fmt->format) {
			case SDE_PIX_FMT_Y_CR_CB_GH2V2:
				stride_align = 16;
				height_align = 1;
				break;
			default:
				stride_align = 1;
				height_align = 1;
				break;
			}

			ps->ystride[0] = ALIGN(w, stride_align);
			ps->ystride[1] = ALIGN(w / h_subsample, stride_align);
			ps->plane_size[0] = ps->ystride[0] *
				ALIGN(h, height_align);
			ps->plane_size[1] = ps->ystride[1] * (h / v_subsample);

			if (fmt->fetch_planes == SDE_MDP_PLANE_PSEUDO_PLANAR) {
				ps->num_planes = 2;
				ps->plane_size[1] *= 2;
				ps->ystride[1] *= 2;
			} else { /* planar */
				ps->num_planes = 3;
				ps->plane_size[2] = ps->plane_size[1];
				ps->ystride[2] = ps->ystride[1];
			}
		}
	}

	/* Safe to use MAX_PLANES as ps is memset at start of function */
	for (i = 0; i < SDE_ROT_MAX_PLANES; i++)
		ps->total_size += ps->plane_size[i];

	return rc;
}

static int sde_mdp_a5x_data_check(struct sde_mdp_data *data,
			struct sde_mdp_plane_sizes *ps,
			struct sde_mdp_format_params *fmt)
{
	int i, inc;
	unsigned long data_size = 0;
	dma_addr_t base_addr;

	if (data->p[0].len == ps->plane_size[0])
		goto end;

	/* From this point, assumption is plane 0 is to be divided */
	data_size = data->p[0].len;
	if (data_size < ps->total_size) {
		SDEROT_ERR(
			"insufficient current mem len=%lu required mem len=%u\n",
			data_size, ps->total_size);
		return -ENOMEM;
	}

	base_addr = data->p[0].addr;

	if (sde_mdp_is_yuv_format(fmt)) {
		/************************************************/
		/*      UBWC            **                      */
		/*      buffer          **      MDP PLANE       */
		/*      format          **                      */
		/************************************************/
		/* -------------------  ** -------------------- */
		/* |      Y meta     |  ** |    Y bitstream   | */
		/* |       data      |  ** |       plane      | */
		/* -------------------  ** -------------------- */
		/* |    Y bitstream  |  ** |  CbCr bitstream  | */
		/* |       data      |  ** |       plane      | */
		/* -------------------  ** -------------------- */
		/* |   Cbcr metadata |  ** |       Y meta     | */
		/* |       data      |  ** |       plane      | */
		/* -------------------  ** -------------------- */
		/* |  CbCr bitstream |  ** |     CbCr meta    | */
		/* |       data      |  ** |       plane      | */
		/* -------------------  ** -------------------- */
		/************************************************/

		/* configure Y bitstream plane */
		data->p[0].addr = base_addr + ps->plane_size[2];
		data->p[0].len = ps->plane_size[0];

		/* configure CbCr bitstream plane */
		data->p[1].addr = base_addr + ps->plane_size[0]
			+ ps->plane_size[2] + ps->plane_size[3];
		data->p[1].len = ps->plane_size[1];

		if (!sde_mdp_is_ubwc_format(fmt))
			goto done;

		/* configure Y metadata plane */
		data->p[2].addr = base_addr;
		data->p[2].len = ps->plane_size[2];

		/* configure CbCr metadata plane */
		data->p[3].addr = base_addr + ps->plane_size[0]
			+ ps->plane_size[2];
		data->p[3].len = ps->plane_size[3];
	} else {
		/************************************************/
		/*      UBWC            **                      */
		/*      buffer          **      MDP PLANE       */
		/*      format          **                      */
		/************************************************/
		/* -------------------  ** -------------------- */
		/* |      RGB meta   |  ** |   RGB bitstream  | */
		/* |       data      |  ** |       plane      | */
		/* -------------------  ** -------------------- */
		/* |  RGB bitstream  |  ** |       NONE       | */
		/* |       data      |  ** |                  | */
		/* -------------------  ** -------------------- */
		/*                      ** |     RGB meta     | */
		/*                      ** |       plane      | */
		/*                      ** -------------------- */
		/************************************************/

		/* configure RGB bitstream plane */
		data->p[0].addr = base_addr + ps->plane_size[2];
		data->p[0].len = ps->plane_size[0];

		if (!sde_mdp_is_ubwc_format(fmt))
			goto done;

		/* configure RGB metadata plane */
		data->p[2].addr = base_addr;
		data->p[2].len = ps->plane_size[2];
	}
done:
	data->num_planes = ps->num_planes;

end:
	if (data->num_planes != ps->num_planes) {
		SDEROT_ERR("num_planes don't match: fmt:%d, data:%d, ps:%d\n",
				fmt->format, data->num_planes, ps->num_planes);
		return -EINVAL;
	}

	inc = (sde_mdp_is_yuv_format(fmt) ? 1 : 2);
	for (i = 0; i < SDE_ROT_MAX_PLANES; i += inc) {
		if (data->p[i].len != ps->plane_size[i]) {
			SDEROT_ERR(
				"plane:%d fmt:%d, len does not match: data:%lu, ps:%d\n",
					i, fmt->format, data->p[i].len,
					ps->plane_size[i]);
			return -EINVAL;
		}
	}

	return 0;
}

int sde_mdp_data_check(struct sde_mdp_data *data,
			struct sde_mdp_plane_sizes *ps,
			struct sde_mdp_format_params *fmt)
{
	struct sde_mdp_img_data *prev, *curr;
	int i;

	if (!ps)
		return 0;

	if (!data || data->num_planes == 0)
		return -ENOMEM;

	if (sde_mdp_is_tilea5x_format(fmt))
		return sde_mdp_a5x_data_check(data, ps, fmt);

	SDEROT_DBG("srcp0=%pa len=%lu frame_size=%u\n", &data->p[0].addr,
		data->p[0].len, ps->total_size);

	for (i = 0; i < ps->num_planes; i++) {
		curr = &data->p[i];
		if (i >= data->num_planes) {
			u32 psize = ps->plane_size[i-1];

			prev = &data->p[i-1];
			if (prev->len > psize) {
				curr->len = prev->len - psize;
				prev->len = psize;
			}
			curr->addr = prev->addr + psize;
		}
		if (curr->len < ps->plane_size[i]) {
			SDEROT_ERR("insufficient mem=%lu p=%d len=%u\n",
			       curr->len, i, ps->plane_size[i]);
			return -ENOMEM;
		}
		SDEROT_DBG("plane[%d] addr=%pa len=%lu\n", i,
				&curr->addr, curr->len);
	}
	data->num_planes = ps->num_planes;

	return 0;
}

int sde_validate_offset_for_ubwc_format(
	struct sde_mdp_format_params *fmt, u16 x, u16 y)
{
	int ret;
	u16 micro_w = 0, micro_h = 0;

	ret = sde_rot_get_ubwc_micro_dim(fmt->format, &micro_w, &micro_h);
	if (ret || !micro_w || !micro_h) {
		SDEROT_ERR("Could not get valid micro tile dimensions\n");
		return -EINVAL;
	}

	if (x % (micro_w * UBWC_META_MACRO_W_H)) {
		SDEROT_ERR("x=%d does not align with meta width=%d\n", x,
			micro_w * UBWC_META_MACRO_W_H);
		return -EINVAL;
	}

	if (y % (micro_h * UBWC_META_MACRO_W_H)) {
		SDEROT_ERR("y=%d does not align with meta height=%d\n", y,
			UBWC_META_MACRO_W_H);
		return -EINVAL;
	}
	return ret;
}

/* x and y are assumed to be valid, expected to line up with start of tiles */
void sde_rot_ubwc_data_calc_offset(struct sde_mdp_data *data, u16 x, u16 y,
	struct sde_mdp_plane_sizes *ps, struct sde_mdp_format_params *fmt)
{
	u16 macro_w, micro_w, micro_h;
	u32 offset = 0;
	int ret;

	ret = sde_rot_get_ubwc_micro_dim(fmt->format, &micro_w, &micro_h);
	if (ret || !micro_w || !micro_h) {
		SDEROT_ERR("Could not get valid micro tile dimensions\n");
		return;
	}
	macro_w = 4 * micro_w;

	if (sde_mdp_is_nv12_8b_format(fmt)) {
		u16 chroma_macro_w = macro_w / 2;
		u16 chroma_micro_w = micro_w / 2;

		/* plane 1 and 3 are chroma, with sub sample of 2 */
		offset = y * ps->ystride[0] +
			(x / macro_w) * 4096;
		if (offset < data->p[0].len) {
			data->p[0].addr += offset;
		} else {
			ret = 1;
			goto done;
		}

		offset = y / 2 * ps->ystride[1] +
			((x / 2) / chroma_macro_w) * 4096;
		if (offset < data->p[1].len) {
			data->p[1].addr += offset;
		} else {
			ret = 2;
			goto done;
		}

		offset = (y / micro_h) * ps->ystride[2] +
			((x / micro_w) / UBWC_META_MACRO_W_H) *
			UBWC_META_BLOCK_SIZE;
		if (offset < data->p[2].len) {
			data->p[2].addr += offset;
		} else {
			ret = 3;
			goto done;
		}

		offset = ((y / 2) / micro_h) * ps->ystride[3] +
			(((x / 2) / chroma_micro_w) / UBWC_META_MACRO_W_H) *
			UBWC_META_BLOCK_SIZE;
		if (offset < data->p[3].len) {
			data->p[3].addr += offset;
		} else {
			ret = 4;
			goto done;
		}
	} else if (sde_mdp_is_nv12_10b_format(fmt)) {
		/* TODO: */
		SDEROT_ERR("%c%c%c%c format not implemented yet",
				fmt->format >> 0, fmt->format >> 8,
				fmt->format >> 16, fmt->format >> 24);
		ret = 1;
		goto done;
	} else {
		offset = y * ps->ystride[0] +
			(x / macro_w) * 4096;
		if (offset < data->p[0].len) {
			data->p[0].addr += offset;
		} else {
			ret = 1;
			goto done;
		}

		offset = DIV_ROUND_UP(y, micro_h) * ps->ystride[2] +
			((x / micro_w) / UBWC_META_MACRO_W_H) *
			UBWC_META_BLOCK_SIZE;
		if (offset < data->p[2].len) {
			data->p[2].addr += offset;
		} else {
			ret = 3;
			goto done;
		}
	}

done:
	if (ret) {
		WARN(1, "idx %d, offsets:%u too large for buflen%lu\n",
			(ret - 1), offset, data->p[(ret - 1)].len);
	}
}

void sde_rot_data_calc_offset(struct sde_mdp_data *data, u16 x, u16 y,
	struct sde_mdp_plane_sizes *ps, struct sde_mdp_format_params *fmt)
{
	if ((x == 0) && (y == 0))
		return;

	if (sde_mdp_is_tilea5x_format(fmt)) {
		sde_rot_ubwc_data_calc_offset(data, x, y, ps, fmt);
		return;
	}

	data->p[0].addr += y * ps->ystride[0];

	if (data->num_planes == 1) {
		data->p[0].addr += x * fmt->bpp;
	} else {
		u16 xoff, yoff;
		u8 v_subsample, h_subsample;

		sde_mdp_get_v_h_subsample_rate(fmt->chroma_sample,
			&v_subsample, &h_subsample);

		xoff = x / h_subsample;
		yoff = y / v_subsample;

		data->p[0].addr += x;
		data->p[1].addr += xoff + (yoff * ps->ystride[1]);
		if (data->num_planes == 2) /* pseudo planar */
			data->p[1].addr += xoff;
		else /* planar */
			data->p[2].addr += xoff + (yoff * ps->ystride[2]);
	}
}

static int sde_smmu_get_domain_type(u32 flags, bool rotator)
{
	int type;

	if (flags & SDE_SECURE_OVERLAY_SESSION)
		type = SDE_IOMMU_DOMAIN_ROT_SECURE;
	else
		type = SDE_IOMMU_DOMAIN_ROT_UNSECURE;

	return type;
}

static int sde_mdp_put_img(struct sde_mdp_img_data *data, bool rotator,
		int dir)
{
	u32 domain;

	if (data->flags & SDE_ROT_EXT_IOVA) {
		SDEROT_DBG("buffer %pad/%lx is client mapped\n",
				&data->addr, data->len);
		return 0;
	}

	if (!IS_ERR_OR_NULL(data->srcp_dma_buf)) {
		SDEROT_DBG("ion hdl=%p buf=0x%pa\n", data->srcp_dma_buf,
							&data->addr);
		if (data->mapped) {
			domain = sde_smmu_get_domain_type(data->flags,
				rotator);
			sde_smmu_unmap_dma_buf(data->srcp_table,
						domain, dir,
						data->srcp_dma_buf);
			data->mapped = false;
			SDEROT_DBG("unmap %pad/%lx d:%u f:%x\n", &data->addr,
					data->len, domain, data->flags);
		}
		if (!data->skip_detach) {
			dma_buf_unmap_attachment(data->srcp_attachment,
				data->srcp_table, dir);
			dma_buf_detach(data->srcp_dma_buf,
					data->srcp_attachment);
			if (!(data->flags & SDE_ROT_EXT_DMA_BUF)) {
				dma_buf_put(data->srcp_dma_buf);
				data->srcp_dma_buf = NULL;
			}
		}
	} else {
		return -ENOMEM;
	}

	return 0;
}

static int sde_mdp_is_map_needed(struct sde_mdp_img_data *data)
{
	if (data->flags & SDE_SECURE_CAMERA_SESSION)
		return false;
	return true;
}

static int sde_mdp_get_img(struct sde_fb_data *img,
		struct sde_mdp_img_data *data, struct device *dev,
		bool rotator, int dir)
{
	int ret = -EINVAL;
	unsigned long *len;
	u32 domain;
	dma_addr_t *start;
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	struct ion_client *iclient = mdata->iclient;

	start = &data->addr;
	len = &data->len;
	data->flags |= img->flags;
	data->offset = img->offset;
	if (data->flags & SDE_ROT_EXT_DMA_BUF) {
		data->srcp_dma_buf = img->buffer;
	} else if (data->flags & SDE_ROT_EXT_IOVA) {
		data->addr = img->addr;
		data->len = img->len;
		SDEROT_DBG("use client %pad/%lx\n", &data->addr, data->len);
		return 0;
	} else if (IS_ERR(data->srcp_dma_buf)) {
		SDEROT_ERR("error on ion_import_fd\n");
		ret = PTR_ERR(data->srcp_dma_buf);
		data->srcp_dma_buf = NULL;
		return ret;
	}

	if (sde_mdp_is_map_needed(data)) {
		domain = sde_smmu_get_domain_type(data->flags, rotator);

		SDEROT_DBG("%d domain=%d ihndl=%p\n",
				__LINE__, domain, data->srcp_dma_buf);
		data->srcp_attachment =
			sde_smmu_dma_buf_attach(data->srcp_dma_buf, dev,
					domain);
		if (IS_ERR(data->srcp_attachment)) {
			SDEROT_ERR("%d Failed to attach dma buf\n", __LINE__);
			ret = PTR_ERR(data->srcp_attachment);
			goto err_put;
		}

		SDEROT_DBG("%d attach=%p\n", __LINE__, data->srcp_attachment);
		data->srcp_table =
			dma_buf_map_attachment(data->srcp_attachment, dir);
		if (IS_ERR(data->srcp_table)) {
			SDEROT_ERR("%d Failed to map attachment\n", __LINE__);
			ret = PTR_ERR(data->srcp_table);
			goto err_detach;
		}

		SDEROT_DBG("%d table=%p\n", __LINE__, data->srcp_table);
		data->addr = 0;
		data->len = 0;
		data->mapped = false;
		data->skip_detach = false;
		/* return early, mapping will be done later */
	} else {
		struct ion_handle *ihandle = NULL;
		struct sg_table *sg_ptr = NULL;

		do {
			ihandle = img->handle;
			if (IS_ERR_OR_NULL(ihandle)) {
				ret = -EINVAL;
				SDEROT_ERR("invalid ion handle\n");
				break;
			}

			sg_ptr = ion_sg_table(iclient, ihandle);
			if (sg_ptr == NULL) {
				SDEROT_ERR("ion sg table get failed\n");
				ret = -EINVAL;
				break;
			}

			if (sg_ptr->nents != 1) {
				SDEROT_ERR("ion buffer mapping failed\n");
				ret = -EINVAL;
				break;
			}

			if (((uint64_t)sg_dma_address(sg_ptr->sgl) >=
					PHY_ADDR_4G - sg_ptr->sgl->length)) {
				SDEROT_ERR("ion buffer mapped size invalid\n");
				ret = -EINVAL;
				break;
			}

			data->addr = sg_dma_address(sg_ptr->sgl);
			data->len = sg_ptr->sgl->length;
			data->mapped = true;
			ret = 0;
		} while (0);

		if (!IS_ERR_OR_NULL(ihandle))
			ion_free(iclient, ihandle);
		return ret;
	}

	return 0;
err_detach:
	dma_buf_detach(data->srcp_dma_buf, data->srcp_attachment);
err_put:
	if (!(data->flags & SDE_ROT_EXT_DMA_BUF)) {
		dma_buf_put(data->srcp_dma_buf);
		data->srcp_dma_buf = NULL;
	}
	return ret;
}

static int sde_mdp_map_buffer(struct sde_mdp_img_data *data, bool rotator,
		int dir)
{
	int ret = -EINVAL;
	int domain;
	struct scatterlist *sg;
	unsigned int i;
	struct sg_table *table;

	if (data->addr && data->len)
		return 0;

	if (data->flags & SDE_ROT_EXT_IOVA) {
		SDEROT_DBG("buffer %pad/%lx is client mapped\n",
				&data->addr, data->len);
		return 0;
	}

	if (!IS_ERR_OR_NULL(data->srcp_dma_buf)) {
		if (sde_mdp_is_map_needed(data)) {
			domain = sde_smmu_get_domain_type(data->flags,
					rotator);
			ret = sde_smmu_map_dma_buf(data->srcp_dma_buf,
					data->srcp_table, domain,
					&data->addr, &data->len, dir);
			if (ret < 0) {
				SDEROT_ERR("smmu map buf failed:(%d)\n", ret);
				goto err_unmap;
			}
			SDEROT_DBG("map %pad/%lx d:%u f:%x\n",
					&data->addr,
					data->len,
					domain,
					data->flags);
			data->mapped = true;
		} else {
			data->addr = sg_phys(data->srcp_table->sgl);
			data->len = 0;
			table = data->srcp_table;
			for_each_sg(table->sgl, sg, table->nents, i) {
				data->len += sg->length;
			}
			ret = 0;
		}
	}

	if (!data->addr) {
		SDEROT_ERR("start address is zero!\n");
		sde_mdp_put_img(data, rotator, dir);
		return -ENOMEM;
	}

	if (!ret && (data->offset < data->len)) {
		data->addr += data->offset;
		data->len -= data->offset;

		SDEROT_DBG("ihdl=%p buf=0x%pa len=0x%lx\n",
			 data->srcp_dma_buf, &data->addr, data->len);
	} else {
		sde_mdp_put_img(data, rotator, dir);
		return ret ? : -EOVERFLOW;
	}

	return ret;

err_unmap:
	dma_buf_unmap_attachment(data->srcp_attachment, data->srcp_table, dir);
	dma_buf_detach(data->srcp_dma_buf, data->srcp_attachment);
	if (!(data->flags & SDE_ROT_EXT_DMA_BUF)) {
		dma_buf_put(data->srcp_dma_buf);
		data->srcp_dma_buf = NULL;
	}
	return ret;
}

static int sde_mdp_data_get(struct sde_mdp_data *data,
		struct sde_fb_data *planes, int num_planes, u32 flags,
		struct device *dev, bool rotator, int dir)
{
	int i, rc = 0;

	if ((num_planes <= 0) || (num_planes > SDE_ROT_MAX_PLANES))
		return -EINVAL;

	for (i = 0; i < num_planes; i++) {
		data->p[i].flags = flags;
		rc = sde_mdp_get_img(&planes[i], &data->p[i], dev, rotator,
				dir);
		if (rc) {
			SDEROT_ERR("failed to get buf p=%d flags=%x\n",
					i, flags);
			while (i > 0) {
				i--;
				sde_mdp_put_img(&data->p[i], rotator, dir);
			}
			break;
		}
	}

	data->num_planes = i;

	return rc;
}

int sde_mdp_data_map(struct sde_mdp_data *data, bool rotator, int dir)
{
	int i, rc = 0;

	if (!data || !data->num_planes || data->num_planes > SDE_ROT_MAX_PLANES)
		return -EINVAL;

	for (i = 0; i < data->num_planes; i++) {
		rc = sde_mdp_map_buffer(&data->p[i], rotator, dir);
		if (rc) {
			SDEROT_ERR("failed to map buf p=%d\n", i);
			while (i > 0) {
				i--;
				sde_mdp_put_img(&data->p[i], rotator, dir);
			}
			break;
		}
	}
	SDEROT_EVTLOG(data->num_planes, dir, data->p[0].addr, data->p[0].len,
			data->p[0].mapped);

	return rc;
}

void sde_mdp_data_free(struct sde_mdp_data *data, bool rotator, int dir)
{
	int i;

	sde_smmu_ctrl(1);
	for (i = 0; i < data->num_planes && data->p[i].len; i++)
		sde_mdp_put_img(&data->p[i], rotator, dir);
	sde_smmu_ctrl(0);

	data->num_planes = 0;
}

int sde_mdp_data_get_and_validate_size(struct sde_mdp_data *data,
	struct sde_fb_data *planes, int num_planes, u32 flags,
	struct device *dev, bool rotator, int dir,
	struct sde_layer_buffer *buffer)
{
	struct sde_mdp_format_params *fmt;
	struct sde_mdp_plane_sizes ps;
	int ret, i;
	unsigned long total_buf_len = 0;

	fmt = sde_get_format_params(buffer->format);
	if (!fmt) {
		SDEROT_ERR("Format %d not supported\n", buffer->format);
		return -EINVAL;
	}

	ret = sde_mdp_data_get(data, planes, num_planes,
		flags, dev, rotator, dir);
	if (ret)
		return ret;

	sde_mdp_get_plane_sizes(fmt, buffer->width, buffer->height, &ps, 0, 0);

	for (i = 0; i < num_planes ; i++) {
		unsigned long plane_len = (data->p[i].srcp_dma_buf) ?
				data->p[i].srcp_dma_buf->size : data->p[i].len;

		if (plane_len < planes[i].offset) {
			SDEROT_ERR("Offset=%d larger than buffer size=%lu\n",
				planes[i].offset, plane_len);
			ret = -EINVAL;
			goto buf_too_small;
		}
		total_buf_len += plane_len - planes[i].offset;
	}

	if (total_buf_len < ps.total_size) {
		SDEROT_ERR("Buffer size=%lu, expected size=%d\n",
				total_buf_len,
			ps.total_size);
		ret = -EINVAL;
		goto buf_too_small;
	}
	return 0;

buf_too_small:
	sde_mdp_data_free(data, rotator, dir);
	return ret;
}
