// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/slab.h>

#include "swd.h"
#include "swd_registers_qm35.h"
#include "qm35xxx_swd_ops.h"

static void setup_word_bit_line(struct device *dev, u32 address)
{
	u32 wordline;
	u32 bitline;

	/*
	 * RRAM address is calculated using wordline(row) and the bitline(column).
	 * There are 1024 rows and 64 columns in RRAM memory (1024*64*8 = 512KB).
	 */
	wordline = (address >> QM35_WORDLINE_SIZE_SHIFT);
	bitline = (address % QM35_WORDLINE_SIZE_BYTES) >> QM35_BITLINE_SIZE_SHIFT;
	swd_memory_write(dev, QM35_RRAM_WORDLINE_BITLINE_REG,
			(wordline << QM35_WORDLINE_SHIFT) | (bitline << QM35_BITLINE_SHIFT));
}

static int wait_for_status_bit(struct device *dev, u32 bitmask, bool bitstate)
{
	u32 sreg;
	bool is_set;
	u32 timeout_cnt = QM35_RRAM_WRITE_TIMEOUT_REPS;

	do {
		sreg = swd_memory_read(dev, QM35_RRAM_STATUS_REG);
		is_set = (sreg & bitmask) != 0;
	} while (is_set != bitstate && --timeout_cnt > 0);

	if (timeout_cnt == 0)
		return -ETIMEDOUT;
	else
		return 0;
}

static void wait_for_flash_cmd_success(struct device *dev, u32 cmd)
{
	int res = wait_for_status_bit(dev, QM35_RRAM_STATUS_CMD_SUCCESS_MASK, true);

	if (res != 0)
		dev_err(dev, "qm35xxx: failed to wait on cmd(%u) to succeed: %d", cmd, res);
	else
		swd_memory_write(dev, QM35_GFC_IRQ_CLR_REG, QM35_GFC_IRQ_CLR_REG_STATUS);
}

static void flash_write(struct device *dev, const u32 address, const u64 value)
{
	int res;
	u32 ctrl_reg_val;
	const u32 *data = (const u32 *)&value;

	setup_word_bit_line(dev, address);
	swd_memory_write(dev, QM35_RRAM_DATA_0_REG, data[0]);
	swd_memory_write(dev, QM35_RRAM_DATA_1_REG, data[1]);

	// Original ctrl register value without the command bits.
	ctrl_reg_val = swd_memory_read(dev, QM35_RRAM_CTRL_REG) & (~QM35_RRAM_CTRL_CMD_MASK);

	// issue the load cmd and wait for it to succeed.
	swd_memory_write(dev, QM35_RRAM_CTRL_REG, ctrl_reg_val | (u8)QM35_RRAM_CMD_LOAD);
	wait_for_flash_cmd_success(dev, QM35_RRAM_CMD_LOAD);

	// issue the write cmd and wait for it to succeed.
	swd_memory_write(dev, QM35_RRAM_CTRL_REG, ctrl_reg_val | (u8)QM35_RRAM_CMD_WRITE);
	wait_for_flash_cmd_success(dev, QM35_RRAM_CMD_WRITE);

	res = wait_for_status_bit(dev, QM35_RRAM_STATUS_BUSY_MASK, false);
	if (res != 0)
		dev_err(dev, "qm35xxx: failed to wait for busy status to clear: %d", res);
}

int qm35xxx_swd_prepare(struct device *dev)
{
	// Unlock the RRAM.
	swd_memory_write(dev, QM35_RRAM_LOCK_CTRL_REG, QM35_RRAM_DISABLE_LOCK_VAL);
	return 0;
}

int qm35xxx_swd_finalize(struct device *dev)
{
	// Lock the RRAM back.
	swd_memory_write(dev, QM35_RRAM_LOCK_CTRL_REG, QM35_RRAM_ENABLE_LOCK_VAL);
	return 0;

}

int qm35xxx_swd_erase_app(struct device *dev)
{
	// TODO(T121605282): Implement swd erase operation
	return 0;
}

size_t qm35xxx_get_write_chunk_size(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	return devdata->mcu_data.flash_info.block_size;
}

int qm35xxx_swd_write_chunk(struct device *dev, int addr, const u8 *data,
			     size_t len)
{
	int i;
	u64 value;
	u32 bytes_left;

	BUG_ON(addr != 0);

	for (i = 0; i < len; i += sizeof(value)) {
		bytes_left = len - i;
		if (bytes_left >= sizeof(value)) {
			value = *((u64 *)&data[i]);
		} else {
			value = 0;
			memcpy(&value, &data[i], bytes_left);
		}

		flash_write(dev, addr + i, value);
	}

	return 0;
}
