/*
 * *  ffu.c
 *
 *  Copyright 2007-2008 Pierre Ossman
 *
 *  Modified by SanDisk Corp., Copyright 2013 SanDisk Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program includes bug.h, card.h, host.h, mmc.h, scatterlist.h,
 * slab.h, ffu.h & swap.h header files
 * The original, unmodified version of this program the mmc_test.c
 * file is obtained under the GPL v2.0 license that is available via
 * http://www.gnu.org/licenses/,
 * or http://www.opensource.org/licenses/gpl-2.0.php
*/

#include <linux/bug.h>
#include <linux/errno.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include "core.h"
#include "card.h"
#include "host.h"
#include "bus.h"
#include "mmc_ops.h"
#include "quirks.h"
#include "sd_ops.h"
#include "pwrseq.h"
#include "ffu_head.h"
#include "ffu.h"

/**
 * struct mmc_ffu_pages - pages allocated by 'alloc_pages()'.
 * @page: first page in the allocation
 * @order: order of the number of pages allocated
 */
struct mmc_ffu_pages {
	struct page *page;
	unsigned int order;
};

/**
 * struct mmc_ffu_mem - allocated memory.
 * @arr: array of allocations
 * @cnt: number of allocations
 */
struct mmc_ffu_mem {
	struct mmc_ffu_pages *arr;
	unsigned int cnt;
};

struct mmc_ffu_area {
	unsigned long max_sz;
	unsigned int max_tfr;
	unsigned int max_segs;
	unsigned int max_seg_sz;
	unsigned int blocks;
	unsigned int sg_len;
	struct mmc_ffu_mem *mem;
	struct scatterlist *sg;
};

unsigned char mr_test_buf[512];

static void mmc_ffu_prepare_mrq(struct mmc_card *card,
	struct mmc_request *mrq, struct scatterlist *sg, unsigned int sg_len,
	u32 arg, unsigned int blocks, unsigned int blksz)
{
	BUG_ON(!mrq || !mrq->cmd || !mrq->data || !mrq->stop);

	if (blocks > 1)
		mrq->cmd->opcode = MMC_WRITE_MULTIPLE_BLOCK;
	else
		mrq->cmd->opcode = MMC_WRITE_BLOCK;

	mrq->cmd->arg = arg;
	if (!mmc_card_blockaddr(card))
		mrq->cmd->arg <<= 9;

	mrq->cmd->flags = MMC_RSP_R1 | MMC_CMD_ADTC;
	if (blocks == 1) {
		mrq->stop = NULL;
	} else {
		mrq->stop->opcode = MMC_STOP_TRANSMISSION;
		mrq->stop->arg = 0;
		mrq->stop->flags = MMC_RSP_R1B | MMC_CMD_AC;
	}

	mrq->data->blksz = blksz;
	mrq->data->blocks = blocks;
	mrq->data->flags = MMC_DATA_WRITE;
	mrq->data->sg = sg;
	mrq->data->sg_len = sg_len;
	pr_info("ffu prepare: blksz:%x, blocks:%x, flag:%x, sg_len:%x\n",blksz,blocks,mrq->data->flags, sg_len);

	mmc_set_data_timeout(mrq->data, card);
}

/*
 * Checks that a normal transfer didn't have any errors
 */
static int mmc_ffu_check_result(struct mmc_request *mrq)
{
	BUG_ON(!mrq || !mrq->cmd || !mrq->data);

	if (mrq->cmd->error != 0)
		return -EINVAL;

	if (mrq->data->error != 0)
		return -EINVAL;

	if (mrq->stop != NULL && mrq->stop->error != 0)
		return -1;

	if (mrq->data->bytes_xfered != (mrq->data->blocks * mrq->data->blksz))
		return -EINVAL;

	return 0;
}

static int mmc_ffu_busy(struct mmc_command *cmd)
{
	return !(cmd->resp[0] & R1_READY_FOR_DATA) ||
		(R1_CURRENT_STATE(cmd->resp[0]) == R1_STATE_PRG);
}

static int mmc_ffu_wait_busy(struct mmc_card *card)
{
	int ret, busy = 0;
	struct mmc_command cmd = {0};

	memset(&cmd, 0, sizeof(struct mmc_command));
	cmd.opcode = MMC_SEND_STATUS;
	cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;

	do {
		ret = mmc_wait_for_cmd(card->host, &cmd, 0);
		if (ret)
			break;

		if (!busy && mmc_ffu_busy(&cmd)) {
			busy = 1;
			if (card->host->caps & MMC_CAP_WAIT_WHILE_BUSY) {
				pr_warn("%s: Warning: Host did not wait for busy state to end.\n",
					mmc_hostname(card->host));
			}
		}

	} while (mmc_ffu_busy(&cmd));

	return ret;
}

/*
 * transfer with certain parameters
 */
static int mmc_ffu_simple_transfer(struct mmc_card *card,
	struct scatterlist *sg, unsigned int sg_len, u32 arg,
	unsigned int blocks, unsigned int blksz)
{
	struct mmc_request mrq = {0};
	struct mmc_command cmd = {0};
	struct mmc_command stop = {0};
	struct mmc_data data = {0};

	mrq.cmd = &cmd;
	mrq.data = &data;
	mrq.stop = &stop;
	mmc_ffu_prepare_mrq(card, &mrq, sg, sg_len, arg, blocks, blksz);
	mmc_wait_for_req(card->host, &mrq);

	mmc_ffu_wait_busy(card);

	return mmc_ffu_check_result(&mrq);
}

/*
 * Map memory into a scatterlist.
 */
static unsigned int mmc_ffu_map_sg(struct mmc_ffu_mem *mem, int size,
	struct scatterlist *sglist)
{
	struct scatterlist *sg = sglist;
	unsigned int i;
	unsigned long sz = size;
	unsigned int sctr_len = 0;
	unsigned long len;

	sg_init_table(sglist, mem->cnt);

	for (i = 0; i < mem->cnt && sz; i++, sz -= len) {
		len = PAGE_SIZE * (1 << mem->arr[i].order);

		if (len > sz) {
			len = sz;
			sz = 0;
		}

		sg_set_page(sg, mem->arr[i].page, len, 0);
		sg = sg_next(sg);
		sctr_len += 1;
	}

	return sctr_len;
}

static void mmc_ffu_free_mem(struct mmc_ffu_mem *mem)
{
	if (!mem)
		return;

	while (mem->cnt--)
		__free_pages(mem->arr[mem->cnt].page, mem->arr[mem->cnt].order);

	kfree(mem->arr);
	kfree(mem);
}

/*
 * Cleanup struct mmc_ffu_area.
 */
static void mmc_ffu_area_cleanup(struct mmc_ffu_area *area)
{
	kfree(area->sg);
	mmc_ffu_free_mem(area->mem);
}

/*
 * Allocate a lot of memory, preferably max_sz but at least min_sz. In case
 * there isn't much memory do not exceed 1/16th total low mem pages. Also do
 * not exceed a maximum number of segments and try not to make segments much
 * bigger than maximum segment size.
 */
static struct mmc_ffu_mem *mmc_ffu_alloc_mem(unsigned long min_sz,
	unsigned long max_sz, unsigned int max_segs, unsigned int max_seg_sz)
{
	unsigned long max_page_cnt = DIV_ROUND_UP(max_sz, PAGE_SIZE);
	unsigned long min_page_cnt = DIV_ROUND_UP(min_sz, PAGE_SIZE);
	unsigned long max_seg_page_cnt = DIV_ROUND_UP(max_seg_sz, PAGE_SIZE);
	unsigned long page_cnt = 0;
	/* we divide by 16 to ensure we will not allocate a big amount
	 * of unnecessary pages */
	unsigned long limit = nr_free_buffer_pages() >> 4;
	struct mmc_ffu_mem *mem;
	gfp_t flags = GFP_KERNEL | GFP_DMA | __GFP_NOWARN | __GFP_NORETRY;

	if (max_page_cnt > limit)
		max_page_cnt = limit;

	if (min_page_cnt > max_page_cnt)
		min_page_cnt = max_page_cnt;

	if (max_segs * max_seg_page_cnt > max_page_cnt)
		max_segs = DIV_ROUND_UP(max_page_cnt, max_seg_page_cnt);

	mem = kzalloc(sizeof(struct mmc_ffu_mem), GFP_KERNEL);
	if (!mem)
		return NULL;

	mem->arr = kzalloc(sizeof(struct mmc_ffu_pages) * max_segs,
		GFP_KERNEL);
	if (!mem->arr)
		goto out_free;

	while (max_page_cnt) {
		struct page *page;
		unsigned int order;

		order = get_order(max_seg_page_cnt << PAGE_SHIFT);

		do {
			page = alloc_pages(flags, order);
		} while (!page && order--);

		if (!page)
			goto out_free;

		mem->arr[mem->cnt].page = page;
		mem->arr[mem->cnt].order = order;
		mem->cnt += 1;
		page_cnt += 1UL << order;
		if (max_page_cnt <= (1UL << order))
			break;
		max_page_cnt -= 1UL << order;
	}

	if (page_cnt < min_page_cnt)
		goto out_free;

	return mem;

out_free:
	mmc_ffu_free_mem(mem);
	return NULL;
}

/*
 * Initialize an area for data transfers.
 * Copy the data to the allocated pages.
 */
static int mmc_ffu_area_init(struct mmc_ffu_area *area, struct mmc_card *card,
	const u8 *data, int size)
{
	int ret;
	int i;
	int length = 0;
	int min_size = 0;

	area->max_tfr = size;

	/* Try to allocate enough memory for a max. sized transfer. Less is OK
	 * because the same memory can be mapped into the scatterlist more than
	 * once. Also, take into account the limits imposed on scatterlist
	 * segments by the host driver.
	 */
	area->mem = mmc_ffu_alloc_mem(1, area->max_tfr, area->max_segs,
		area->max_seg_sz);
	if (!area->mem)
		return -ENOMEM;

	/* copy data to page */
	for (i = 0; i < area->mem->cnt; i++) {
		if (length > size) {
			ret = -EINVAL;
			goto out_free;
		}
		min_size = min(size - length, (int)area->max_seg_sz);
		memcpy(page_address(area->mem->arr[i].page), data + length,
			min_size);
		length += min_size;
	}

	area->sg = kzalloc(sizeof(struct scatterlist) * area->mem->cnt,
		GFP_KERNEL);
	if (!area->sg) {
		ret = -ENOMEM;
		goto out_free;
	}

	area->sg_len = mmc_ffu_map_sg(area->mem, size, area->sg);

	return 0;

out_free:
	mmc_ffu_area_cleanup(area);
	return ret;
}

static int mmc_ffu_write(struct mmc_card *card, u8 *src, u32 arg,
	int size)
{
	int rc;
	struct mmc_ffu_area area = {0};
	int max_tfr;

	pr_info("world mmc_ffu_write: max_segs:%x max_seg_size:%x max_blk_count:%x max_req_size:%x arg:0x%x\n",
				card->host->max_segs,
				card->host->max_seg_size,
				card->host->max_blk_count,
				card->host->max_req_size, arg);

	area.max_segs = card->host->max_segs;
	area.max_seg_sz = card->host->max_seg_size & ~(CARD_BLOCK_SIZE - 1);

	do {
		max_tfr = size;
		if (max_tfr >> 9 > card->host->max_blk_count)
			max_tfr = card->host->max_blk_count << 9;
		if (max_tfr > card->host->max_req_size)
			max_tfr = card->host->max_req_size;
		if (DIV_ROUND_UP(max_tfr, area.max_seg_sz) > area.max_segs)
			max_tfr = area.max_segs * area.max_seg_sz;
		if ((max_tfr > card->host->max_seg_size) &&
			((max_tfr % card->host->max_seg_size) != 0))
			max_tfr = area.max_seg_sz;

		pr_info("world***** card max_tfr:%x\n",max_tfr);

		rc = mmc_ffu_area_init(&area, card, src, max_tfr);
		if (rc != 0)
			goto exit;

		rc = mmc_ffu_simple_transfer(card, area.sg, area.sg_len, arg,
			max_tfr / CARD_BLOCK_SIZE, CARD_BLOCK_SIZE);
		if (rc != 0) {
			pr_err("%s mmc_ffu_simple_transfer %d\n", __func__, rc);
			goto exit;
		}
		src += max_tfr;
		size -= max_tfr;
		mmc_ffu_area_cleanup(&area);
	} while (size > 0);

	return 0;

exit:
	mmc_ffu_area_cleanup(&area);
	return rc;
}

#ifdef FFU_back
extern u8 BIWIN_H07301[512*1024];
#else
extern u8 BIWIN_H07304[512*1024];
#endif
#define EXT_CSD_MODE_CONFIG		30	/* R/W */
#define G1_FW_VERSION 1
#define H1_FW_VERSION 2
#define H4_FW_VERSION 3
int mmc_ffu_download(struct mmc_card *card, int step, int version)
{
	int err;
	u32 arg, fw_sectors;
	int part = 0;
	struct mmc_command cmd = {0};
	u8 *g_ffu_bin_buffer;
	u32 status;
	int i;
	u8 *ext_csd = NULL;

	pr_info("world mmc_ffu_download ********\n");

	err = mmc_get_ext_csd(card, &ext_csd);
	if (err) {
		pr_err("%s: error %d sending ext_csd 0\n", __func__, err);
	}

	mmc_claim_host(card->host);

	/* check emmc status */
	i=0;
	do {
		err = mmc_send_status(card, &status);
		if (err)
			goto exit;
		i++;
	} while ((R1_CURRENT_STATE(status) != R1_STATE_TRAN) && i<10000);
	
	pr_info("world switch to FFU ******** ext_csd[26] = 0x%x\n", ext_csd[26]);
	if (ext_csd[26] != 0) {
		cmd.opcode = 6;
		cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1B | MMC_CMD_AC;
	 	if (!mmc_host_is_spi(card->host))
			cmd.arg = 0x031E0100;
		err = mmc_wait_for_cmd(card->host, &cmd, 0);

		cmd.opcode = 6;
		cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1B | MMC_CMD_AC;
		if (!mmc_host_is_spi(card->host))
			cmd.arg = 0x031E0000;
		err = mmc_wait_for_cmd(card->host, &cmd, 0);
		
		err = 26;
		goto exit;
	}

	/* set device to FFU mode */
	cmd.opcode = 6;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1B | MMC_CMD_AC;
	if (!mmc_host_is_spi(card->host))
		cmd.arg = 0x031E0100;
	err = mmc_wait_for_cmd(card->host, &cmd, 0);

	mdelay(200);

	/* set CMD ARG */
	arg = ext_csd[487] | ext_csd[487+1] << 8 | ext_csd[487+2] << 16 | ext_csd[487+3] << 24;

#ifdef FFU_back
	switch (version) {
		case H4_FW_VERSION:
			pr_info("world mmc_ffu_download downgrading from BIWIN_H07304 to BIWIN_H07301.\n");
			g_ffu_bin_buffer = BIWIN_H07301;
			break;
	}
#else
	switch (version) {
		case G1_FW_VERSION:
			WARN(1, "FFU from BIWIN_G09251 attempted. Not expected");
			goto exit;
		case H1_FW_VERSION:
			pr_info("world mmc_ffu_download upgrading from BIWIN_H07301 to BIWIN_H07304.\n");
			g_ffu_bin_buffer = BIWIN_H07304;
			break;
	}
#endif
	part=0;
	fw_sectors = 0;
	//while((ext_csd[26] == 0) && (fw_sectors != 0x400)) {
	for(part = 0; part < 8; part++) {
		pr_warn("******** FFU P%x err:%d, part:%d, fw_sectors:0x%x********\n", part, err, part, fw_sectors);
		/*  ***** Need FFU data and size ****** */
		err = mmc_ffu_write(card, g_ffu_bin_buffer+(part*512*128), arg, 512*128);
		//err = mmc_ffu_write(card, g_ffu_bin_buffer+(part*512*1024), arg, 512*1024);
		if (err) {
			pr_warn("world FFU: %s: error %d write switching to normal\n",
				mmc_hostname(card->host), err);
			//break;
		}
	}

	mdelay(100);

	/* check emmc status */
	i=0;
	do {
		err = mmc_send_status(card, &status);
		if (err)
			goto exit;
		i++;
	} while ((R1_CURRENT_STATE(status) != R1_STATE_TRAN) && i<10000);

	/* set device to normal mode*/
	cmd.opcode = 6;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1B | MMC_CMD_AC;
	if (!mmc_host_is_spi(card->host))
		cmd.arg = 0x031E0000;

	err = mmc_wait_for_cmd(card->host, &cmd, 0);
	if (err) {
		pr_err("%s: error %d, close ffu fail\n", __func__, err);
	}

	udelay(1000);

exit:
	mmc_release_host(card->host);
	kfree(ext_csd);
	return err;
}
EXPORT_SYMBOL(mmc_ffu_download);
