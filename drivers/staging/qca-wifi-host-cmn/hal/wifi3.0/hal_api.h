/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _HAL_API_H_
#define _HAL_API_H_

#include "qdf_types.h"
#include "qdf_util.h"
#include "qdf_atomic.h"
#include "hal_internal.h"
#include "hif.h"
#include "hif_io32.h"
#include "qdf_platform.h"

#ifdef DUMP_REO_QUEUE_INFO_IN_DDR
#include "hal_hw_headers.h"
#endif

/* Ring index for WBM2SW2 release ring */
#define HAL_IPA_TX_COMP_RING_IDX 2

#if defined(CONFIG_SHADOW_V2) || defined(CONFIG_SHADOW_V3)
#define ignore_shadow false
#define CHECK_SHADOW_REGISTERS true
#else
#define ignore_shadow true
#define CHECK_SHADOW_REGISTERS false
#endif

/* calculate the register address offset from bar0 of shadow register x */
#if defined(QCA_WIFI_QCA6390) || defined(QCA_WIFI_QCA6490) || \
    defined(QCA_WIFI_KIWI)
#define SHADOW_REGISTER_START_ADDRESS_OFFSET 0x000008FC
#define SHADOW_REGISTER_END_ADDRESS_OFFSET \
	((SHADOW_REGISTER_START_ADDRESS_OFFSET) + (4 * (MAX_SHADOW_REGISTERS)))
#define SHADOW_REGISTER(x) ((SHADOW_REGISTER_START_ADDRESS_OFFSET) + (4 * (x)))
#elif defined(QCA_WIFI_QCA6290) || defined(QCA_WIFI_QCN9000)
#define SHADOW_REGISTER_START_ADDRESS_OFFSET 0x00003024
#define SHADOW_REGISTER_END_ADDRESS_OFFSET \
	((SHADOW_REGISTER_START_ADDRESS_OFFSET) + (4 * (MAX_SHADOW_REGISTERS)))
#define SHADOW_REGISTER(x) ((SHADOW_REGISTER_START_ADDRESS_OFFSET) + (4 * (x)))
#elif defined(QCA_WIFI_QCA6750)
#define SHADOW_REGISTER_START_ADDRESS_OFFSET 0x00000504
#define SHADOW_REGISTER_END_ADDRESS_OFFSET \
	((SHADOW_REGISTER_START_ADDRESS_OFFSET) + (4 * (MAX_SHADOW_REGISTERS)))
#define SHADOW_REGISTER(x) ((SHADOW_REGISTER_START_ADDRESS_OFFSET) + (4 * (x)))
#else
#define SHADOW_REGISTER(x) 0
#endif /* QCA_WIFI_QCA6390 || QCA_WIFI_QCA6490 || QCA_WIFI_QCA6750 */

/*
 * BAR + 4K is always accessible, any access outside this
 * space requires force wake procedure.
 * OFFSET = 4K - 32 bytes = 0xFE0
 */
#define MAPPED_REF_OFF 0xFE0

#define HAL_OFFSET(block, field) block ## _ ## field ## _OFFSET

#ifdef ENABLE_VERBOSE_DEBUG
static inline void
hal_set_verbose_debug(bool flag)
{
	is_hal_verbose_debug_enabled = flag;
}
#endif

#ifdef ENABLE_HAL_SOC_STATS
#define HAL_STATS_INC(_handle, _field, _delta) \
{ \
	if (likely(_handle)) \
		_handle->stats._field += _delta; \
}
#else
#define HAL_STATS_INC(_handle, _field, _delta)
#endif

#ifdef ENABLE_HAL_REG_WR_HISTORY
#define HAL_REG_WRITE_FAIL_HIST_ADD(hal_soc, offset, wr_val, rd_val) \
	hal_reg_wr_fail_history_add(hal_soc, offset, wr_val, rd_val)

void hal_reg_wr_fail_history_add(struct hal_soc *hal_soc,
				 uint32_t offset,
				 uint32_t wr_val,
				 uint32_t rd_val);

static inline int hal_history_get_next_index(qdf_atomic_t *table_index,
					     int array_size)
{
	int record_index = qdf_atomic_inc_return(table_index);

	return record_index & (array_size - 1);
}
#else
#define HAL_REG_WRITE_FAIL_HIST_ADD(hal_soc, offset, wr_val, rd_val) \
	hal_err("write failed at reg offset 0x%x, write 0x%x read 0x%x\n", \
		offset,	\
		wr_val,	\
		rd_val)
#endif

/**
 * hal_reg_write_result_check() - check register writing result
 * @hal_soc: HAL soc handle
 * @offset: register offset to read
 * @exp_val: the expected value of register
 * @ret_confirm: result confirm flag
 *
 * Return: none
 */
static inline void hal_reg_write_result_check(struct hal_soc *hal_soc,
					      uint32_t offset,
					      uint32_t exp_val)
{
	uint32_t value;

	value = qdf_ioread32(hal_soc->dev_base_addr + offset);
	if (exp_val != value) {
		HAL_REG_WRITE_FAIL_HIST_ADD(hal_soc, offset, exp_val, value);
		HAL_STATS_INC(hal_soc, reg_write_fail, 1);
	}
}

#ifdef WINDOW_REG_PLD_LOCK_ENABLE
static inline void hal_lock_reg_access(struct hal_soc *soc,
				       unsigned long *flags)
{
	pld_lock_reg_window(soc->qdf_dev->dev, flags);
}

static inline void hal_unlock_reg_access(struct hal_soc *soc,
					 unsigned long *flags)
{
	pld_unlock_reg_window(soc->qdf_dev->dev, flags);
}
#else
static inline void hal_lock_reg_access(struct hal_soc *soc,
				       unsigned long *flags)
{
	qdf_spin_lock_irqsave(&soc->register_access_lock);
}

static inline void hal_unlock_reg_access(struct hal_soc *soc,
					 unsigned long *flags)
{
	qdf_spin_unlock_irqrestore(&soc->register_access_lock);
}
#endif

#ifdef PCIE_REG_WINDOW_LOCAL_NO_CACHE
/**
 * hal_select_window_confirm() - write remap window register and
				 check writing result
 *
 */
static inline void hal_select_window_confirm(struct hal_soc *hal_soc,
					     uint32_t offset)
{
	uint32_t window = (offset >> WINDOW_SHIFT) & WINDOW_VALUE_MASK;

	qdf_iowrite32(hal_soc->dev_base_addr + WINDOW_REG_ADDRESS,
		      WINDOW_ENABLE_BIT | window);
	hal_soc->register_window = window;

	hal_reg_write_result_check(hal_soc, WINDOW_REG_ADDRESS,
				   WINDOW_ENABLE_BIT | window);
}
#else
static inline void hal_select_window_confirm(struct hal_soc *hal_soc,
					     uint32_t offset)
{
	uint32_t window = (offset >> WINDOW_SHIFT) & WINDOW_VALUE_MASK;

	if (window != hal_soc->register_window) {
		qdf_iowrite32(hal_soc->dev_base_addr + WINDOW_REG_ADDRESS,
			      WINDOW_ENABLE_BIT | window);
		hal_soc->register_window = window;

		hal_reg_write_result_check(
					hal_soc,
					WINDOW_REG_ADDRESS,
					WINDOW_ENABLE_BIT | window);
	}
}
#endif

static inline qdf_iomem_t hal_get_window_address(struct hal_soc *hal_soc,
						 qdf_iomem_t addr)
{
	return hal_soc->ops->hal_get_window_address(hal_soc, addr);
}

static inline void hal_tx_init_cmd_credit_ring(hal_soc_handle_t hal_soc_hdl,
					       hal_ring_handle_t hal_ring_hdl)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	return hal_soc->ops->hal_tx_init_cmd_credit_ring(hal_soc_hdl,
							 hal_ring_hdl);
}

/**
 * hal_write32_mb() - Access registers to update configuration
 * @hal_soc: hal soc handle
 * @offset: offset address from the BAR
 * @value: value to write
 *
 * Return: None
 *
 * Description: Register address space is split below:
 *     SHADOW REGION       UNWINDOWED REGION    WINDOWED REGION
 *  |--------------------|-------------------|------------------|
 * BAR  NO FORCE WAKE  BAR+4K  FORCE WAKE  BAR+512K  FORCE WAKE
 *
 * 1. Any access to the shadow region, doesn't need force wake
 *    and windowing logic to access.
 * 2. Any access beyond BAR + 4K:
 *    If init_phase enabled, no force wake is needed and access
 *    should be based on windowed or unwindowed access.
 *    If init_phase disabled, force wake is needed and access
 *    should be based on windowed or unwindowed access.
 *
 * note1: WINDOW_RANGE_MASK = (1 << WINDOW_SHIFT) -1
 * note2: 1 << WINDOW_SHIFT = MAX_UNWINDOWED_ADDRESS
 * note3: WINDOW_VALUE_MASK = big enough that trying to write past
 *                            that window would be a bug
 */
#if !defined(QCA_WIFI_QCA6390) && !defined(QCA_WIFI_QCA6490) && \
    !defined(QCA_WIFI_QCA6750) && !defined(QCA_WIFI_KIWI)
static inline void hal_write32_mb(struct hal_soc *hal_soc, uint32_t offset,
				  uint32_t value)
{
	unsigned long flags;
	qdf_iomem_t new_addr;

	if (!hal_soc->use_register_windowing ||
	    offset < MAX_UNWINDOWED_ADDRESS) {
		qdf_iowrite32(hal_soc->dev_base_addr + offset, value);
	} else if (hal_soc->static_window_map) {
		new_addr = hal_get_window_address(hal_soc,
				hal_soc->dev_base_addr + offset);
		qdf_iowrite32(new_addr, value);
	} else {
		hal_lock_reg_access(hal_soc, &flags);
		hal_select_window_confirm(hal_soc, offset);
		qdf_iowrite32(hal_soc->dev_base_addr + WINDOW_START +
			  (offset & WINDOW_RANGE_MASK), value);
		hal_unlock_reg_access(hal_soc, &flags);
	}
}

#define hal_write32_mb_confirm(_hal_soc, _offset, _value) \
		hal_write32_mb(_hal_soc, _offset, _value)

#define hal_write32_mb_cmem(_hal_soc, _offset, _value)
#else
static inline void hal_write32_mb(struct hal_soc *hal_soc, uint32_t offset,
				  uint32_t value)
{
	int ret;
	unsigned long flags;
	qdf_iomem_t new_addr;

	if (!TARGET_ACCESS_ALLOWED(HIF_GET_SOFTC(
					hal_soc->hif_handle))) {
		hal_err_rl("target access is not allowed");
		return;
	}

	/* Region < BAR + 4K can be directly accessed */
	if (offset < MAPPED_REF_OFF) {
		qdf_iowrite32(hal_soc->dev_base_addr + offset, value);
		return;
	}

	/* Region greater than BAR + 4K */
	if (!hal_soc->init_phase) {
		ret = hif_force_wake_request(hal_soc->hif_handle);
		if (ret) {
			hal_err_rl("Wake up request failed");
			qdf_check_state_before_panic(__func__, __LINE__);
			return;
		}
	}

	if (!hal_soc->use_register_windowing ||
	    offset < MAX_UNWINDOWED_ADDRESS) {
		qdf_iowrite32(hal_soc->dev_base_addr + offset, value);
	} else if (hal_soc->static_window_map) {
		new_addr = hal_get_window_address(
					hal_soc,
					hal_soc->dev_base_addr + offset);
		qdf_iowrite32(new_addr, value);
	} else {
		hal_lock_reg_access(hal_soc, &flags);
		hal_select_window_confirm(hal_soc, offset);
		qdf_iowrite32(hal_soc->dev_base_addr + WINDOW_START +
			  (offset & WINDOW_RANGE_MASK), value);
		hal_unlock_reg_access(hal_soc, &flags);
	}

	if (!hal_soc->init_phase) {
		ret = hif_force_wake_release(hal_soc->hif_handle);
		if (ret) {
			hal_err("Wake up release failed");
			qdf_check_state_before_panic(__func__, __LINE__);
			return;
		}
	}
}

/**
 * hal_write32_mb_confirm() - write register and check writing result
 *
 */
static inline void hal_write32_mb_confirm(struct hal_soc *hal_soc,
					  uint32_t offset,
					  uint32_t value)
{
	int ret;
	unsigned long flags;
	qdf_iomem_t new_addr;

	if (!TARGET_ACCESS_ALLOWED(HIF_GET_SOFTC(
					hal_soc->hif_handle))) {
		hal_err_rl("target access is not allowed");
		return;
	}

	/* Region < BAR + 4K can be directly accessed */
	if (offset < MAPPED_REF_OFF) {
		qdf_iowrite32(hal_soc->dev_base_addr + offset, value);
		return;
	}

	/* Region greater than BAR + 4K */
	if (!hal_soc->init_phase) {
		ret = hif_force_wake_request(hal_soc->hif_handle);
		if (ret) {
			hal_err("Wake up request failed");
			qdf_check_state_before_panic(__func__, __LINE__);
			return;
		}
	}

	if (!hal_soc->use_register_windowing ||
	    offset < MAX_UNWINDOWED_ADDRESS) {
		qdf_iowrite32(hal_soc->dev_base_addr + offset, value);
		hal_reg_write_result_check(hal_soc, offset,
					   value);
	} else if (hal_soc->static_window_map) {
		new_addr = hal_get_window_address(
					hal_soc,
					hal_soc->dev_base_addr + offset);
		qdf_iowrite32(new_addr, value);
		hal_reg_write_result_check(hal_soc,
					   new_addr - hal_soc->dev_base_addr,
					   value);
	} else {
		hal_lock_reg_access(hal_soc, &flags);
		hal_select_window_confirm(hal_soc, offset);
		qdf_iowrite32(hal_soc->dev_base_addr + WINDOW_START +
			  (offset & WINDOW_RANGE_MASK), value);

		hal_reg_write_result_check(
				hal_soc,
				WINDOW_START + (offset & WINDOW_RANGE_MASK),
				value);
		hal_unlock_reg_access(hal_soc, &flags);
	}

	if (!hal_soc->init_phase) {
		ret = hif_force_wake_release(hal_soc->hif_handle);
		if (ret) {
			hal_err("Wake up release failed");
			qdf_check_state_before_panic(__func__, __LINE__);
			return;
		}
	}
}

static inline void hal_write32_mb_cmem(struct hal_soc *hal_soc, uint32_t offset,
				       uint32_t value)
{
	unsigned long flags;
	qdf_iomem_t new_addr;

	if (!TARGET_ACCESS_ALLOWED(HIF_GET_SOFTC(
					hal_soc->hif_handle))) {
		hal_err_rl("%s: target access is not allowed", __func__);
		return;
	}

	if (!hal_soc->use_register_windowing ||
	    offset < MAX_UNWINDOWED_ADDRESS) {
		qdf_iowrite32(hal_soc->dev_base_addr + offset, value);
	} else if (hal_soc->static_window_map) {
		new_addr = hal_get_window_address(
					hal_soc,
					hal_soc->dev_base_addr + offset);
		qdf_iowrite32(new_addr, value);
	} else {
		hal_lock_reg_access(hal_soc, &flags);
		hal_select_window_confirm(hal_soc, offset);
		qdf_iowrite32(hal_soc->dev_base_addr + WINDOW_START +
			  (offset & WINDOW_RANGE_MASK), value);
		hal_unlock_reg_access(hal_soc, &flags);
	}
}
#endif

/**
 * hal_write_address_32_mb - write a value to a register
 *
 */
static inline
void hal_write_address_32_mb(struct hal_soc *hal_soc,
			     qdf_iomem_t addr, uint32_t value, bool wr_confirm)
{
	uint32_t offset;

	if (!hal_soc->use_register_windowing)
		return qdf_iowrite32(addr, value);

	offset = addr - hal_soc->dev_base_addr;

	if (qdf_unlikely(wr_confirm))
		hal_write32_mb_confirm(hal_soc, offset, value);
	else
		hal_write32_mb(hal_soc, offset, value);
}


#ifdef DP_HAL_MULTIWINDOW_DIRECT_ACCESS
static inline void hal_srng_write_address_32_mb(struct hal_soc *hal_soc,
						struct hal_srng *srng,
						void __iomem *addr,
						uint32_t value)
{
	qdf_iowrite32(addr, value);
}
#elif defined(FEATURE_HAL_DELAYED_REG_WRITE)
static inline void hal_srng_write_address_32_mb(struct hal_soc *hal_soc,
						struct hal_srng *srng,
						void __iomem *addr,
						uint32_t value)
{
	hal_delayed_reg_write(hal_soc, srng, addr, value);
}
#else
static inline void hal_srng_write_address_32_mb(struct hal_soc *hal_soc,
						struct hal_srng *srng,
						void __iomem *addr,
						uint32_t value)
{
	hal_write_address_32_mb(hal_soc, addr, value, false);
}
#endif

#if !defined(QCA_WIFI_QCA6390) && !defined(QCA_WIFI_QCA6490) && \
    !defined(QCA_WIFI_QCA6750) && !defined(QCA_WIFI_KIWI)
/**
 * hal_read32_mb() - Access registers to read configuration
 * @hal_soc: hal soc handle
 * @offset: offset address from the BAR
 * @value: value to write
 *
 * Description: Register address space is split below:
 *     SHADOW REGION       UNWINDOWED REGION    WINDOWED REGION
 *  |--------------------|-------------------|------------------|
 * BAR  NO FORCE WAKE  BAR+4K  FORCE WAKE  BAR+512K  FORCE WAKE
 *
 * 1. Any access to the shadow region, doesn't need force wake
 *    and windowing logic to access.
 * 2. Any access beyond BAR + 4K:
 *    If init_phase enabled, no force wake is needed and access
 *    should be based on windowed or unwindowed access.
 *    If init_phase disabled, force wake is needed and access
 *    should be based on windowed or unwindowed access.
 *
 * Return: < 0 for failure/>= 0 for success
 */
static inline uint32_t hal_read32_mb(struct hal_soc *hal_soc, uint32_t offset)
{
	uint32_t ret;
	unsigned long flags;
	qdf_iomem_t new_addr;

	if (!hal_soc->use_register_windowing ||
	    offset < MAX_UNWINDOWED_ADDRESS) {
		return qdf_ioread32(hal_soc->dev_base_addr + offset);
	} else if (hal_soc->static_window_map) {
		new_addr = hal_get_window_address(hal_soc, hal_soc->dev_base_addr + offset);
		return qdf_ioread32(new_addr);
	}

	hal_lock_reg_access(hal_soc, &flags);
	hal_select_window_confirm(hal_soc, offset);
	ret = qdf_ioread32(hal_soc->dev_base_addr + WINDOW_START +
		       (offset & WINDOW_RANGE_MASK));
	hal_unlock_reg_access(hal_soc, &flags);

	return ret;
}

#define hal_read32_mb_cmem(_hal_soc, _offset)
#else
static
uint32_t hal_read32_mb(struct hal_soc *hal_soc, uint32_t offset)
{
	uint32_t ret;
	unsigned long flags;
	qdf_iomem_t new_addr;

	if (!TARGET_ACCESS_ALLOWED(HIF_GET_SOFTC(
					hal_soc->hif_handle))) {
		hal_err_rl("target access is not allowed");
		return 0;
	}

	/* Region < BAR + 4K can be directly accessed */
	if (offset < MAPPED_REF_OFF)
		return qdf_ioread32(hal_soc->dev_base_addr + offset);

	if ((!hal_soc->init_phase) &&
	    hif_force_wake_request(hal_soc->hif_handle)) {
		hal_err("Wake up request failed");
		qdf_check_state_before_panic(__func__, __LINE__);
		return 0;
	}

	if (!hal_soc->use_register_windowing ||
	    offset < MAX_UNWINDOWED_ADDRESS) {
		ret = qdf_ioread32(hal_soc->dev_base_addr + offset);
	} else if (hal_soc->static_window_map) {
		new_addr = hal_get_window_address(
					hal_soc,
					hal_soc->dev_base_addr + offset);
		ret = qdf_ioread32(new_addr);
	} else {
		hal_lock_reg_access(hal_soc, &flags);
		hal_select_window_confirm(hal_soc, offset);
		ret = qdf_ioread32(hal_soc->dev_base_addr + WINDOW_START +
			       (offset & WINDOW_RANGE_MASK));
		hal_unlock_reg_access(hal_soc, &flags);
	}

	if ((!hal_soc->init_phase) &&
	    hif_force_wake_release(hal_soc->hif_handle)) {
		hal_err("Wake up release failed");
		qdf_check_state_before_panic(__func__, __LINE__);
		return 0;
	}

	return ret;
}

static inline
uint32_t hal_read32_mb_cmem(struct hal_soc *hal_soc, uint32_t offset)
{
	uint32_t ret;
	unsigned long flags;
	qdf_iomem_t new_addr;

	if (!TARGET_ACCESS_ALLOWED(HIF_GET_SOFTC(
					hal_soc->hif_handle))) {
		hal_err_rl("%s: target access is not allowed", __func__);
		return 0;
	}

	if (!hal_soc->use_register_windowing ||
	    offset < MAX_UNWINDOWED_ADDRESS) {
		ret = qdf_ioread32(hal_soc->dev_base_addr + offset);
	} else if (hal_soc->static_window_map) {
		new_addr = hal_get_window_address(
					hal_soc,
					hal_soc->dev_base_addr + offset);
		ret = qdf_ioread32(new_addr);
	} else {
		hal_lock_reg_access(hal_soc, &flags);
		hal_select_window_confirm(hal_soc, offset);
		ret = qdf_ioread32(hal_soc->dev_base_addr + WINDOW_START +
			       (offset & WINDOW_RANGE_MASK));
		hal_unlock_reg_access(hal_soc, &flags);
	}
	return ret;
}
#endif

/* Max times allowed for register writing retry */
#define HAL_REG_WRITE_RETRY_MAX		5
/* Delay milliseconds for each time retry */
#define HAL_REG_WRITE_RETRY_DELAY	1

#ifdef GENERIC_SHADOW_REGISTER_ACCESS_ENABLE
/* To check shadow config index range between 0..31 */
#define HAL_SHADOW_REG_INDEX_LOW 32
/* To check shadow config index range between 32..39 */
#define HAL_SHADOW_REG_INDEX_HIGH 40
/* Dirty bit reg offsets corresponding to shadow config index */
#define HAL_SHADOW_REG_DIRTY_BIT_DATA_LOW_OFFSET 0x30C8
#define HAL_SHADOW_REG_DIRTY_BIT_DATA_HIGH_OFFSET 0x30C4
/* PCIE_PCIE_TOP base addr offset */
#define HAL_PCIE_PCIE_TOP_WRAPPER 0x01E00000
/* Max retry attempts to read the dirty bit reg */
#ifdef HAL_CONFIG_SLUB_DEBUG_ON
#define HAL_SHADOW_DIRTY_BIT_POLL_MAX 10000
#else
#define HAL_SHADOW_DIRTY_BIT_POLL_MAX 2000
#endif
/* Delay in usecs for polling dirty bit reg */
#define HAL_SHADOW_DIRTY_BIT_POLL_DELAY 5

/**
 * hal_poll_dirty_bit_reg() - Poll dirty register bit to confirm
 * write was successful
 * @hal_soc: hal soc handle
 * @shadow_config_index: index of shadow reg used to confirm
 * write
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static inline QDF_STATUS hal_poll_dirty_bit_reg(struct hal_soc *hal,
						int shadow_config_index)
{
	uint32_t read_value = 0;
	int retry_cnt = 0;
	uint32_t reg_offset = 0;

	if (shadow_config_index > 0 &&
	    shadow_config_index < HAL_SHADOW_REG_INDEX_LOW) {
		reg_offset =
			HAL_SHADOW_REG_DIRTY_BIT_DATA_LOW_OFFSET;
	} else if (shadow_config_index >= HAL_SHADOW_REG_INDEX_LOW &&
		   shadow_config_index < HAL_SHADOW_REG_INDEX_HIGH) {
		reg_offset =
			HAL_SHADOW_REG_DIRTY_BIT_DATA_HIGH_OFFSET;
	} else {
		hal_err("Invalid shadow_config_index = %d",
			shadow_config_index);
		return QDF_STATUS_E_INVAL;
	}
	while (retry_cnt < HAL_SHADOW_DIRTY_BIT_POLL_MAX) {
		read_value = hal_read32_mb(
				hal, HAL_PCIE_PCIE_TOP_WRAPPER + reg_offset);
		/* Check if dirty bit corresponding to shadow_index is set */
		if (read_value & BIT(shadow_config_index)) {
			/* Dirty reg bit not reset */
			qdf_udelay(HAL_SHADOW_DIRTY_BIT_POLL_DELAY);
			retry_cnt++;
		} else {
			hal_debug("Shadow write: offset 0x%x read val 0x%x",
				  reg_offset, read_value);
			return QDF_STATUS_SUCCESS;
		}
	}
	return QDF_STATUS_E_TIMEOUT;
}

/**
 * hal_write32_mb_shadow_confirm() - write to shadow reg and
 * poll dirty register bit to confirm write
 * @hal_soc: hal soc handle
 * @reg_offset: target reg offset address from BAR
 * @value: value to write
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static inline QDF_STATUS hal_write32_mb_shadow_confirm(
	struct hal_soc *hal,
	uint32_t reg_offset,
	uint32_t value)
{
	int i;
	QDF_STATUS ret;
	uint32_t shadow_reg_offset;
	int shadow_config_index;
	bool is_reg_offset_present = false;

	for (i = 0; i < MAX_GENERIC_SHADOW_REG; i++) {
		/* Found the shadow config for the reg_offset */
		struct shadow_reg_config *hal_shadow_reg_list =
			&hal->list_shadow_reg_config[i];
		if (hal_shadow_reg_list->target_register ==
			reg_offset) {
			shadow_config_index =
				hal_shadow_reg_list->shadow_config_index;
			shadow_reg_offset =
				SHADOW_REGISTER(shadow_config_index);
			hal_write32_mb_confirm(
				hal, shadow_reg_offset, value);
			is_reg_offset_present = true;
			break;
		}
		ret = QDF_STATUS_E_FAILURE;
	}
	if (is_reg_offset_present) {
		ret = hal_poll_dirty_bit_reg(hal, shadow_config_index);
		hal_info("Shadow write:reg 0x%x val 0x%x ret %d",
			 reg_offset, value, ret);
		if (QDF_IS_STATUS_ERROR(ret)) {
			HAL_STATS_INC(hal, shadow_reg_write_fail, 1);
			return ret;
		}
		HAL_STATS_INC(hal, shadow_reg_write_succ, 1);
	}
	return ret;
}

/**
 * hal_write32_mb_confirm_retry() - write register with confirming and
				    do retry/recovery if writing failed
 * @hal_soc: hal soc handle
 * @offset: offset address from the BAR
 * @value: value to write
 * @recovery: is recovery needed or not.
 *
 * Write the register value with confirming and read it back, if
 * read back value is not as expected, do retry for writing, if
 * retry hit max times allowed but still fail, check if recovery
 * needed.
 *
 * Return: None
 */
static inline void hal_write32_mb_confirm_retry(struct hal_soc *hal_soc,
						uint32_t offset,
						uint32_t value,
						bool recovery)
{
	QDF_STATUS ret;

	ret = hal_write32_mb_shadow_confirm(hal_soc, offset, value);
	if (QDF_IS_STATUS_ERROR(ret) && recovery)
		qdf_trigger_self_recovery(NULL, QDF_HAL_REG_WRITE_FAILURE);
}
#else /* GENERIC_SHADOW_REGISTER_ACCESS_ENABLE */

static inline void hal_write32_mb_confirm_retry(struct hal_soc *hal_soc,
						uint32_t offset,
						uint32_t value,
						bool recovery)
{
	uint8_t retry_cnt = 0;
	uint32_t read_value;

	while (retry_cnt <= HAL_REG_WRITE_RETRY_MAX) {
		hal_write32_mb_confirm(hal_soc, offset, value);
		read_value = hal_read32_mb(hal_soc, offset);
		if (qdf_likely(read_value == value))
			break;

		/* write failed, do retry */
		hal_warn("Retry reg offset 0x%x, value 0x%x, read value 0x%x",
			 offset, value, read_value);
		qdf_mdelay(HAL_REG_WRITE_RETRY_DELAY);
		retry_cnt++;
	}

	if (retry_cnt > HAL_REG_WRITE_RETRY_MAX && recovery)
		qdf_trigger_self_recovery(NULL, QDF_HAL_REG_WRITE_FAILURE);
}
#endif /* GENERIC_SHADOW_REGISTER_ACCESS_ENABLE */

#if defined(FEATURE_HAL_DELAYED_REG_WRITE)
/**
 * hal_dump_reg_write_srng_stats() - dump SRNG reg write stats
 * @hal_soc: HAL soc handle
 *
 * Return: none
 */
void hal_dump_reg_write_srng_stats(hal_soc_handle_t hal_soc_hdl);

/**
 * hal_dump_reg_write_stats() - dump reg write stats
 * @hal_soc: HAL soc handle
 *
 * Return: none
 */
void hal_dump_reg_write_stats(hal_soc_handle_t hal_soc_hdl);

/**
 * hal_get_reg_write_pending_work() - get the number of entries
 *		pending in the workqueue to be processed.
 * @hal_soc: HAL soc handle
 *
 * Returns: the number of entries pending to be processed
 */
int hal_get_reg_write_pending_work(void *hal_soc);

#else
static inline void hal_dump_reg_write_srng_stats(hal_soc_handle_t hal_soc_hdl)
{
}

static inline void hal_dump_reg_write_stats(hal_soc_handle_t hal_soc_hdl)
{
}

static inline int hal_get_reg_write_pending_work(void *hal_soc)
{
	return 0;
}
#endif

/**
 * hal_read_address_32_mb() - Read 32-bit value from the register
 * @soc: soc handle
 * @addr: register address to read
 *
 * Return: 32-bit value
 */
static inline
uint32_t hal_read_address_32_mb(struct hal_soc *soc,
				qdf_iomem_t addr)
{
	uint32_t offset;
	uint32_t ret;

	if (!soc->use_register_windowing)
		return qdf_ioread32(addr);

	offset = addr - soc->dev_base_addr;
	ret = hal_read32_mb(soc, offset);
	return ret;
}

/**
 * hal_attach - Initialize HAL layer
 * @hif_handle: Opaque HIF handle
 * @qdf_dev: QDF device
 *
 * Return: Opaque HAL SOC handle
 *		 NULL on failure (if given ring is not available)
 *
 * This function should be called as part of HIF initialization (for accessing
 * copy engines). DP layer will get hal_soc handle using hif_get_hal_handle()
 */
void *hal_attach(struct hif_opaque_softc *hif_handle, qdf_device_t qdf_dev);

/**
 * hal_detach - Detach HAL layer
 * @hal_soc: HAL SOC handle
 *
 * This function should be called as part of HIF detach
 *
 */
extern void hal_detach(void *hal_soc);

#define HAL_SRNG_LMAC_RING 0x80000000
/* SRNG flags passed in hal_srng_params.flags */
#define HAL_SRNG_MSI_SWAP				0x00000008
#define HAL_SRNG_RING_PTR_SWAP			0x00000010
#define HAL_SRNG_DATA_TLV_SWAP			0x00000020
#define HAL_SRNG_LOW_THRES_INTR_ENABLE	0x00010000
#define HAL_SRNG_MSI_INTR				0x00020000
#define HAL_SRNG_CACHED_DESC		0x00040000

#if defined(QCA_WIFI_QCA6490)  || defined(QCA_WIFI_KIWI)
#define HAL_SRNG_PREFETCH_TIMER 1
#else
#define HAL_SRNG_PREFETCH_TIMER 0
#endif

#define PN_SIZE_24 0
#define PN_SIZE_48 1
#define PN_SIZE_128 2

#ifdef FORCE_WAKE
/**
 * hal_set_init_phase() - Indicate initialization of
 *                        datapath rings
 * @soc: hal_soc handle
 * @init_phase: flag to indicate datapath rings
 *              initialization status
 *
 * Return: None
 */
void hal_set_init_phase(hal_soc_handle_t soc, bool init_phase);
#else
static inline
void hal_set_init_phase(hal_soc_handle_t soc, bool init_phase)
{
}
#endif /* FORCE_WAKE */

/**
 * hal_srng_get_entrysize - Returns size of ring entry in bytes. Should be
 * used by callers for calculating the size of memory to be allocated before
 * calling hal_srng_setup to setup the ring
 *
 * @hal_soc: Opaque HAL SOC handle
 * @ring_type: one of the types from hal_ring_type
 *
 */
extern uint32_t hal_srng_get_entrysize(void *hal_soc, int ring_type);

/**
 * hal_srng_max_entries - Returns maximum possible number of ring entries
 * @hal_soc: Opaque HAL SOC handle
 * @ring_type: one of the types from hal_ring_type
 *
 * Return: Maximum number of entries for the given ring_type
 */
uint32_t hal_srng_max_entries(void *hal_soc, int ring_type);

void hal_set_low_threshold(hal_ring_handle_t hal_ring_hdl,
				 uint32_t low_threshold);

/**
 * hal_srng_dump - Dump ring status
 * @srng: hal srng pointer
 */
void hal_srng_dump(struct hal_srng *srng);

/**
 * hal_srng_get_dir - Returns the direction of the ring
 * @hal_soc: Opaque HAL SOC handle
 * @ring_type: one of the types from hal_ring_type
 *
 * Return: Ring direction
 */
enum hal_srng_dir hal_srng_get_dir(void *hal_soc, int ring_type);

/* HAL memory information */
struct hal_mem_info {
	/* dev base virtual addr */
	void *dev_base_addr;
	/* dev base physical addr */
	void *dev_base_paddr;
	/* dev base ce virtual addr - applicable only for qca5018  */
	/* In qca5018 CE register are outside wcss block */
	/* using a separate address space to access CE registers */
	void *dev_base_addr_ce;
	/* dev base ce physical addr */
	void *dev_base_paddr_ce;
	/* Remote virtual pointer memory for HW/FW updates */
	void *shadow_rdptr_mem_vaddr;
	/* Remote physical pointer memory for HW/FW updates */
	void *shadow_rdptr_mem_paddr;
	/* Shared memory for ring pointer updates from host to FW */
	void *shadow_wrptr_mem_vaddr;
	/* Shared physical memory for ring pointer updates from host to FW */
	void *shadow_wrptr_mem_paddr;
	/* lmac srng start id */
	uint8_t lmac_srng_start_id;
};

/* SRNG parameters to be passed to hal_srng_setup */
struct hal_srng_params {
	/* Physical base address of the ring */
	qdf_dma_addr_t ring_base_paddr;
	/* Virtual base address of the ring */
	void *ring_base_vaddr;
	/* Number of entries in ring */
	uint32_t num_entries;
	/* max transfer length */
	uint16_t max_buffer_length;
	/* MSI Address */
	qdf_dma_addr_t msi_addr;
	/* MSI data */
	uint32_t msi_data;
	/* Interrupt timer threshold – in micro seconds */
	uint32_t intr_timer_thres_us;
	/* Interrupt batch counter threshold – in number of ring entries */
	uint32_t intr_batch_cntr_thres_entries;
	/* Low threshold – in number of ring entries
	 * (valid for src rings only)
	 */
	uint32_t low_threshold;
	/* Misc flags */
	uint32_t flags;
	/* Unique ring id */
	uint8_t ring_id;
	/* Source or Destination ring */
	enum hal_srng_dir ring_dir;
	/* Size of ring entry */
	uint32_t entry_size;
	/* hw register base address */
	void *hwreg_base[MAX_SRNG_REG_GROUPS];
	/* prefetch timer config - in micro seconds */
	uint32_t prefetch_timer;
#ifdef WLAN_FEATURE_NEAR_FULL_IRQ
	/* Near full IRQ support flag */
	uint32_t nf_irq_support;
	/* MSI2 Address */
	qdf_dma_addr_t msi2_addr;
	/* MSI2 data */
	uint32_t msi2_data;
	/* Critical threshold */
	uint16_t crit_thresh;
	/* High threshold */
	uint16_t high_thresh;
	/* Safe threshold */
	uint16_t safe_thresh;
#endif
	/* Timer threshold to issue ring pointer update - in micro seconds */
	uint16_t pointer_timer_threshold;
	/* Number threshold of ring entries to issue pointer update */
	uint8_t pointer_num_threshold;
};

/* hal_construct_srng_shadow_regs() - initialize the shadow
 * registers for srngs
 * @hal_soc: hal handle
 *
 * Return: QDF_STATUS_OK on success
 */
QDF_STATUS hal_construct_srng_shadow_regs(void *hal_soc);

/* hal_set_one_shadow_config() - add a config for the specified ring
 * @hal_soc: hal handle
 * @ring_type: ring type
 * @ring_num: ring num
 *
 * The ring type and ring num uniquely specify the ring.  After this call,
 * the hp/tp will be added as the next entry int the shadow register
 * configuration table.  The hal code will use the shadow register address
 * in place of the hp/tp address.
 *
 * This function is exposed, so that the CE module can skip configuring shadow
 * registers for unused ring and rings assigned to the firmware.
 *
 * Return: QDF_STATUS_OK on success
 */
QDF_STATUS hal_set_one_shadow_config(void *hal_soc, int ring_type,
				     int ring_num);
/**
 * hal_get_shadow_config() - retrieve the config table for shadow cfg v2
 * @hal_soc: hal handle
 * @shadow_config: will point to the table after
 * @num_shadow_registers_configured: will contain the number of valid entries
 */
extern void
hal_get_shadow_config(void *hal_soc,
		      struct pld_shadow_reg_v2_cfg **shadow_config,
		      int *num_shadow_registers_configured);

#ifdef CONFIG_SHADOW_V3
/**
 * hal_get_shadow_v3_config() - retrieve the config table for shadow cfg v3
 * @hal_soc: hal handle
 * @shadow_config: will point to the table after
 * @num_shadow_registers_configured: will contain the number of valid entries
 */
extern void
hal_get_shadow_v3_config(void *hal_soc,
			 struct pld_shadow_reg_v3_cfg **shadow_config,
			 int *num_shadow_registers_configured);
#endif

#ifdef WLAN_FEATURE_NEAR_FULL_IRQ
/**
 * hal_srng_is_near_full_irq_supported() - Check if srng supports near full irq
 * @hal_soc: HAL SoC handle [To be validated by caller]
 * @ring_type: srng type
 * @ring_num: The index of the srng (of the same type)
 *
 * Return: true, if srng support near full irq trigger
 *	false, if the srng does not support near full irq support.
 */
bool hal_srng_is_near_full_irq_supported(hal_soc_handle_t hal_soc,
					 int ring_type, int ring_num);
#else
static inline
bool hal_srng_is_near_full_irq_supported(hal_soc_handle_t hal_soc,
					 int ring_type, int ring_num)
{
	return false;
}
#endif

/**
 * hal_srng_setup - Initialize HW SRNG ring.
 *
 * @hal_soc: Opaque HAL SOC handle
 * @ring_type: one of the types from hal_ring_type
 * @ring_num: Ring number if there are multiple rings of
 *		same type (staring from 0)
 * @mac_id: valid MAC Id should be passed if ring type is one of lmac rings
 * @ring_params: SRNG ring params in hal_srng_params structure.
 * @idle_check: Check if ring is idle

 * Callers are expected to allocate contiguous ring memory of size
 * 'num_entries * entry_size' bytes and pass the physical and virtual base
 * addresses through 'ring_base_paddr' and 'ring_base_vaddr' in hal_srng_params
 * structure. Ring base address should be 8 byte aligned and size of each ring
 * entry should be queried using the API hal_srng_get_entrysize
 *
 * Return: Opaque pointer to ring on success
 *		 NULL on failure (if given ring is not available)
 */
extern void *hal_srng_setup(void *hal_soc, int ring_type, int ring_num,
			    int mac_id, struct hal_srng_params *ring_params,
			    bool idle_check);

/**
 * hal_srng_setup_idx - Initialize HW SRNG ring.
 *
 * @hal_soc: Opaque HAL SOC handle
 * @ring_type: one of the types from hal_ring_type
 * @ring_num: Ring number if there are multiple rings of
 *		same type (staring from 0)
 * @mac_id: valid MAC Id should be passed if ring type is one of lmac rings
 * @ring_params: SRNG ring params in hal_srng_params structure.
 * @idle_check: Check if ring is idle
 * @idx: Ring index

 * Callers are expected to allocate contiguous ring memory of size
 * 'num_entries * entry_size' bytes and pass the physical and virtual base
 * addresses through 'ring_base_paddr' and 'ring_base_vaddr' in hal_srng_params
 * structure. Ring base address should be 8 byte aligned and size of each ring
 * entry should be queried using the API hal_srng_get_entrysize
 *
 * Return: Opaque pointer to ring on success
 *		 NULL on failure (if given ring is not available)
 */
extern void *hal_srng_setup_idx(void *hal_soc, int ring_type, int ring_num,
				int mac_id, struct hal_srng_params *ring_params,
				bool idle_check, uint32_t idx);


/* Remapping ids of REO rings */
#define REO_REMAP_TCL 0
#define REO_REMAP_SW1 1
#define REO_REMAP_SW2 2
#define REO_REMAP_SW3 3
#define REO_REMAP_SW4 4
#define REO_REMAP_RELEASE 5
#define REO_REMAP_FW 6
/*
 * In Beryllium: 4 bits REO destination ring value is defined as: 0: TCL
 * 1:SW1  2:SW2  3:SW3  4:SW4  5:Release  6:FW(WIFI)  7:SW5
 * 8:SW6 9:SW7  10:SW8  11: NOT_USED.
 *
 */
#define REO_REMAP_SW5 7
#define REO_REMAP_SW6 8
#define REO_REMAP_SW7 9
#define REO_REMAP_SW8 10

/*
 * Macro to access HWIO_REO_R0_ERROR_DESTINATION_RING_CTRL_IX_0
 * to map destination to rings
 */
#define HAL_REO_ERR_REMAP_IX0(_VALUE, _OFFSET) \
	((_VALUE) << \
	 (HWIO_REO_R0_ERROR_DESTINATION_MAPPING_IX_0_ERROR_ ## \
	  DESTINATION_RING_ ## _OFFSET ## _SHFT))

/*
 * Macro to access HWIO_REO_R0_ERROR_DESTINATION_RING_CTRL_IX_1
 * to map destination to rings
 */
#define HAL_REO_ERR_REMAP_IX1(_VALUE, _OFFSET) \
	((_VALUE) << \
	 (HWIO_REO_R0_ERROR_DESTINATION_MAPPING_IX_1_ERROR_ ## \
	  DESTINATION_RING_ ## _OFFSET ## _SHFT))

/*
 * Macro to access HWIO_REO_R0_DESTINATION_RING_CTRL_IX_0
 * to map destination to rings
 */
#define HAL_REO_REMAP_IX0(_VALUE, _OFFSET) \
	((_VALUE) << \
	 (HWIO_REO_R0_DESTINATION_RING_CTRL_IX_0_DEST_RING_MAPPING_ ## \
	  _OFFSET ## _SHFT))

/*
 * Macro to access HWIO_REO_R0_DESTINATION_RING_CTRL_IX_1
 * to map destination to rings
 */
#define HAL_REO_REMAP_IX2(_VALUE, _OFFSET) \
	((_VALUE) << \
	 (HWIO_REO_R0_DESTINATION_RING_CTRL_IX_2_DEST_RING_MAPPING_ ## \
	  _OFFSET ## _SHFT))

/*
 * Macro to access HWIO_REO_R0_DESTINATION_RING_CTRL_IX_3
 * to map destination to rings
 */
#define HAL_REO_REMAP_IX3(_VALUE, _OFFSET) \
	((_VALUE) << \
	 (HWIO_REO_R0_DESTINATION_RING_CTRL_IX_3_DEST_RING_MAPPING_ ## \
	  _OFFSET ## _SHFT))

/**
 * hal_reo_read_write_ctrl_ix - Read or write REO_DESTINATION_RING_CTRL_IX
 * @hal_soc_hdl: HAL SOC handle
 * @read: boolean value to indicate if read or write
 * @ix0: pointer to store IX0 reg value
 * @ix1: pointer to store IX1 reg value
 * @ix2: pointer to store IX2 reg value
 * @ix3: pointer to store IX3 reg value
 */
void hal_reo_read_write_ctrl_ix(hal_soc_handle_t hal_soc_hdl, bool read,
				uint32_t *ix0, uint32_t *ix1,
				uint32_t *ix2, uint32_t *ix3);

/**
 * hal_srng_set_hp_paddr_confirm() - Set physical address to dest SRNG head
 *  pointer and confirm that write went through by reading back the value
 * @sring: sring pointer
 * @paddr: physical address
 *
 * Return: None
 */
extern void hal_srng_dst_set_hp_paddr_confirm(struct hal_srng *sring,
					      uint64_t paddr);

/**
 * hal_srng_dst_init_hp() - Initilaize head pointer with cached head pointer
 * @hal_soc: hal_soc handle
 * @srng: sring pointer
 * @vaddr: virtual address
 */
void hal_srng_dst_init_hp(struct hal_soc_handle *hal_soc,
			  struct hal_srng *srng,
			  uint32_t *vaddr);

/**
 * hal_srng_cleanup - Deinitialize HW SRNG ring.
 * @hal_soc: Opaque HAL SOC handle
 * @hal_srng: Opaque HAL SRNG pointer
 */
void hal_srng_cleanup(void *hal_soc, hal_ring_handle_t hal_ring_hdl);

static inline bool hal_srng_initialized(hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	return !!srng->initialized;
}

/**
 * hal_srng_dst_peek - Check if there are any entries in the ring (peek)
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Destination ring pointer
 *
 * Caller takes responsibility for any locking needs.
 *
 * Return: Opaque pointer for next ring entry; NULL on failire
 */
static inline
void *hal_srng_dst_peek(hal_soc_handle_t hal_soc_hdl,
			hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	if (srng->u.dst_ring.tp != srng->u.dst_ring.cached_hp)
		return (void *)(&srng->ring_base_vaddr[srng->u.dst_ring.tp]);

	return NULL;
}


/**
 * hal_mem_dma_cache_sync - Cache sync the specified virtual address Range
 * @hal_soc: HAL soc handle
 * @desc: desc start address
 * @entry_size: size of memory to sync
 *
 * Return: void
 */
#if defined(__LINUX_MIPS32_ARCH__) || defined(__LINUX_MIPS64_ARCH__)
static inline void hal_mem_dma_cache_sync(struct hal_soc *soc, uint32_t *desc,
					  uint32_t entry_size)
{
	qdf_nbuf_dma_inv_range((void *)desc, (void *)(desc + entry_size));
}
#else
static inline void hal_mem_dma_cache_sync(struct hal_soc *soc, uint32_t *desc,
					  uint32_t entry_size)
{
	qdf_mem_dma_cache_sync(soc->qdf_dev, qdf_mem_virt_to_phys(desc),
			       QDF_DMA_FROM_DEVICE,
			       (entry_size * sizeof(uint32_t)));
}
#endif

/**
 * hal_srng_access_start_unlocked - Start ring access (unlocked). Should use
 * hal_srng_access_start if locked access is required
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Ring pointer (Source or Destination ring)
 *
 * This API doesn't implement any byte-order conversion on reading hp/tp.
 * So, Use API only for those srngs for which the target writes hp/tp values to
 * the DDR in the Host order.
 *
 * Return: 0 on success; error on failire
 */
static inline int
hal_srng_access_start_unlocked(hal_soc_handle_t hal_soc_hdl,
			       hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	struct hal_soc *soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t *desc;

	if (srng->ring_dir == HAL_SRNG_SRC_RING)
		srng->u.src_ring.cached_tp =
			*(volatile uint32_t *)(srng->u.src_ring.tp_addr);
	else {
		srng->u.dst_ring.cached_hp =
			*(volatile uint32_t *)(srng->u.dst_ring.hp_addr);

		if (srng->flags & HAL_SRNG_CACHED_DESC) {
			desc = hal_srng_dst_peek(hal_soc_hdl, hal_ring_hdl);
			if (qdf_likely(desc)) {
				hal_mem_dma_cache_sync(soc, desc,
						       srng->entry_size);
				qdf_prefetch(desc);
			}
		}
	}

	return 0;
}

/**
 * hal_le_srng_access_start_unlocked_in_cpu_order - Start ring access
 * (unlocked) with endianness correction.
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Ring pointer (Source or Destination ring)
 *
 * This API provides same functionally as hal_srng_access_start_unlocked()
 * except that it converts the little-endian formatted hp/tp values to
 * Host order on reading them. So, this API should only be used for those srngs
 * for which the target always writes hp/tp values in little-endian order
 * regardless of Host order.
 *
 * Also, this API doesn't take the lock. For locked access, use
 * hal_srng_access_start/hal_le_srng_access_start_in_cpu_order.
 *
 * Return: 0 on success; error on failire
 */
static inline int
hal_le_srng_access_start_unlocked_in_cpu_order(
	hal_soc_handle_t hal_soc_hdl,
	hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	struct hal_soc *soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t *desc;

	if (srng->ring_dir == HAL_SRNG_SRC_RING)
		srng->u.src_ring.cached_tp =
			qdf_le32_to_cpu(*(volatile uint32_t *)
					(srng->u.src_ring.tp_addr));
	else {
		srng->u.dst_ring.cached_hp =
			qdf_le32_to_cpu(*(volatile uint32_t *)
					(srng->u.dst_ring.hp_addr));

		if (srng->flags & HAL_SRNG_CACHED_DESC) {
			desc = hal_srng_dst_peek(hal_soc_hdl, hal_ring_hdl);
			if (qdf_likely(desc)) {
				hal_mem_dma_cache_sync(soc, desc,
						       srng->entry_size);
				qdf_prefetch(desc);
			}
		}
	}

	return 0;
}

/**
 * hal_srng_try_access_start - Try to start (locked) ring access
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Ring pointer (Source or Destination ring)
 *
 * Return: 0 on success; error on failure
 */
static inline int hal_srng_try_access_start(hal_soc_handle_t hal_soc_hdl,
					    hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	if (qdf_unlikely(!hal_ring_hdl)) {
		qdf_print("Error: Invalid hal_ring\n");
		return -EINVAL;
	}

	if (!SRNG_TRY_LOCK(&(srng->lock)))
		return -EINVAL;

	return hal_srng_access_start_unlocked(hal_soc_hdl, hal_ring_hdl);
}

/**
 * hal_srng_access_start - Start (locked) ring access
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Ring pointer (Source or Destination ring)
 *
 * This API doesn't implement any byte-order conversion on reading hp/tp.
 * So, Use API only for those srngs for which the target writes hp/tp values to
 * the DDR in the Host order.
 *
 * Return: 0 on success; error on failire
 */
static inline int hal_srng_access_start(hal_soc_handle_t hal_soc_hdl,
					hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	if (qdf_unlikely(!hal_ring_hdl)) {
		qdf_print("Error: Invalid hal_ring\n");
		return -EINVAL;
	}

	SRNG_LOCK(&(srng->lock));

	return hal_srng_access_start_unlocked(hal_soc_hdl, hal_ring_hdl);
}

/**
 * hal_le_srng_access_start_in_cpu_order - Start (locked) ring access with
 * endianness correction
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Ring pointer (Source or Destination ring)
 *
 * This API provides same functionally as hal_srng_access_start()
 * except that it converts the little-endian formatted hp/tp values to
 * Host order on reading them. So, this API should only be used for those srngs
 * for which the target always writes hp/tp values in little-endian order
 * regardless of Host order.
 *
 * Return: 0 on success; error on failire
 */
static inline int
hal_le_srng_access_start_in_cpu_order(
	hal_soc_handle_t hal_soc_hdl,
	hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	if (qdf_unlikely(!hal_ring_hdl)) {
		qdf_print("Error: Invalid hal_ring\n");
		return -EINVAL;
	}

	SRNG_LOCK(&(srng->lock));

	return hal_le_srng_access_start_unlocked_in_cpu_order(
			hal_soc_hdl, hal_ring_hdl);
}

/**
 * hal_srng_dst_get_next - Get next entry from a destination ring
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Destination ring pointer
 *
 * Return: Opaque pointer for next ring entry; NULL on failure
 */
static inline
void *hal_srng_dst_get_next(void *hal_soc,
			    hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t *desc;

	if (srng->u.dst_ring.tp == srng->u.dst_ring.cached_hp)
		return NULL;

	desc = &srng->ring_base_vaddr[srng->u.dst_ring.tp];
	/* TODO: Using % is expensive, but we have to do this since
	 * size of some SRNG rings is not power of 2 (due to descriptor
	 * sizes). Need to create separate API for rings used
	 * per-packet, with sizes power of 2 (TCL2SW, REO2SW,
	 * SW2RXDMA and CE rings)
	 */
	srng->u.dst_ring.tp = (srng->u.dst_ring.tp + srng->entry_size);
	if (srng->u.dst_ring.tp == srng->ring_size)
		srng->u.dst_ring.tp = 0;

	if (srng->flags & HAL_SRNG_CACHED_DESC) {
		struct hal_soc *soc = (struct hal_soc *)hal_soc;
		uint32_t *desc_next;
		uint32_t tp;

		tp = srng->u.dst_ring.tp;
		desc_next = &srng->ring_base_vaddr[srng->u.dst_ring.tp];
		hal_mem_dma_cache_sync(soc, desc_next, srng->entry_size);
		qdf_prefetch(desc_next);
	}

	return (void *)desc;
}

/**
 * hal_srng_dst_get_next_cached - Get cached next entry
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Destination ring pointer
 *
 * Get next entry from a destination ring and move cached tail pointer
 *
 * Return: Opaque pointer for next ring entry; NULL on failure
 */
static inline
void *hal_srng_dst_get_next_cached(void *hal_soc,
				   hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t *desc;
	uint32_t *desc_next;

	if (srng->u.dst_ring.tp == srng->u.dst_ring.cached_hp)
		return NULL;

	desc = &srng->ring_base_vaddr[srng->u.dst_ring.tp];
	/* TODO: Using % is expensive, but we have to do this since
	 * size of some SRNG rings is not power of 2 (due to descriptor
	 * sizes). Need to create separate API for rings used
	 * per-packet, with sizes power of 2 (TCL2SW, REO2SW,
	 * SW2RXDMA and CE rings)
	 */
	srng->u.dst_ring.tp = (srng->u.dst_ring.tp + srng->entry_size);
	if (srng->u.dst_ring.tp == srng->ring_size)
		srng->u.dst_ring.tp = 0;

	desc_next = &srng->ring_base_vaddr[srng->u.dst_ring.tp];
	qdf_prefetch(desc_next);
	return (void *)desc;
}

/**
 * hal_srng_dst_dec_tp - decrement the TP of the Dst ring by one entry
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Destination ring pointer
 *
 * reset the tail pointer in the destination ring by one entry
 *
 */
static inline
void hal_srng_dst_dec_tp(void *hal_soc, hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	if (qdf_unlikely(!srng->u.dst_ring.tp))
		srng->u.dst_ring.tp = (srng->ring_size - srng->entry_size);
	else
		srng->u.dst_ring.tp -= srng->entry_size;
}

static inline int hal_srng_lock(hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	if (qdf_unlikely(!hal_ring_hdl)) {
		qdf_print("error: invalid hal_ring\n");
		return -EINVAL;
	}

	SRNG_LOCK(&(srng->lock));
	return 0;
}

static inline int hal_srng_unlock(hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	if (qdf_unlikely(!hal_ring_hdl)) {
		qdf_print("error: invalid hal_ring\n");
		return -EINVAL;
	}

	SRNG_UNLOCK(&(srng->lock));
	return 0;
}

/**
 * hal_srng_dst_get_next_hp - Get next entry from a destination ring and move
 * cached head pointer
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Destination ring pointer
 *
 * Return: Opaque pointer for next ring entry; NULL on failire
 */
static inline void *
hal_srng_dst_get_next_hp(hal_soc_handle_t hal_soc_hdl,
			 hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t *desc;
	/* TODO: Using % is expensive, but we have to do this since
	 * size of some SRNG rings is not power of 2 (due to descriptor
	 * sizes). Need to create separate API for rings used
	 * per-packet, with sizes power of 2 (TCL2SW, REO2SW,
	 * SW2RXDMA and CE rings)
	 */
	uint32_t next_hp = (srng->u.dst_ring.cached_hp + srng->entry_size) %
		srng->ring_size;

	if (next_hp != srng->u.dst_ring.tp) {
		desc = &(srng->ring_base_vaddr[srng->u.dst_ring.cached_hp]);
		srng->u.dst_ring.cached_hp = next_hp;
		return (void *)desc;
	}

	return NULL;
}

/**
 * hal_srng_dst_peek_sync - Check if there are any entries in the ring (peek)
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Destination ring pointer
 *
 * Sync cached head pointer with HW.
 * Caller takes responsibility for any locking needs.
 *
 * Return: Opaque pointer for next ring entry; NULL on failire
 */
static inline
void *hal_srng_dst_peek_sync(hal_soc_handle_t hal_soc_hdl,
			     hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	srng->u.dst_ring.cached_hp =
		*(volatile uint32_t *)(srng->u.dst_ring.hp_addr);

	if (srng->u.dst_ring.tp != srng->u.dst_ring.cached_hp)
		return (void *)(&(srng->ring_base_vaddr[srng->u.dst_ring.tp]));

	return NULL;
}

/**
 * hal_srng_dst_peek_sync_locked - Peek for any entries in the ring
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Destination ring pointer
 *
 * Sync cached head pointer with HW.
 * This function takes up SRNG_LOCK. Should not be called with SRNG lock held.
 *
 * Return: Opaque pointer for next ring entry; NULL on failire
 */
static inline
void *hal_srng_dst_peek_sync_locked(hal_soc_handle_t hal_soc_hdl,
				    hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	void *ring_desc_ptr = NULL;

	if (qdf_unlikely(!hal_ring_hdl)) {
		qdf_print("Error: Invalid hal_ring\n");
		return  NULL;
	}

	SRNG_LOCK(&srng->lock);

	ring_desc_ptr = hal_srng_dst_peek_sync(hal_soc_hdl, hal_ring_hdl);

	SRNG_UNLOCK(&srng->lock);

	return ring_desc_ptr;
}

#define hal_srng_dst_num_valid_nolock(hal_soc, hal_ring_hdl, sync_hw_ptr) \
		hal_srng_dst_num_valid(hal_soc, hal_ring_hdl, sync_hw_ptr)

/**
 * hal_srng_dst_num_valid - Returns number of valid entries (to be processed
 * by SW) in destination ring
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Destination ring pointer
 * @sync_hw_ptr: Sync cached head pointer with HW
 *
 */
static inline
uint32_t hal_srng_dst_num_valid(void *hal_soc,
				hal_ring_handle_t hal_ring_hdl,
				int sync_hw_ptr)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t hp;
	uint32_t tp = srng->u.dst_ring.tp;

	if (sync_hw_ptr) {
		hp = *(volatile uint32_t *)(srng->u.dst_ring.hp_addr);
		srng->u.dst_ring.cached_hp = hp;
	} else {
		hp = srng->u.dst_ring.cached_hp;
	}

	if (hp >= tp)
		return (hp - tp) / srng->entry_size;

	return (srng->ring_size - tp + hp) / srng->entry_size;
}

/**
 * hal_srng_dst_inv_cached_descs - API to invalidate descriptors in batch mode
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Destination ring pointer
 * @entry_count: call invalidate API if valid entries available
 *
 * Invalidates a set of cached descriptors starting from TP to cached_HP
 *
 * Return - None
 */
static inline void hal_srng_dst_inv_cached_descs(void *hal_soc,
						 hal_ring_handle_t hal_ring_hdl,
						 uint32_t entry_count)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t *first_desc;
	uint32_t *last_desc;
	uint32_t last_desc_index;

	/*
	 * If SRNG does not have cached descriptors this
	 * API call should be a no op
	 */
	if (!(srng->flags & HAL_SRNG_CACHED_DESC))
		return;

	if (!entry_count)
		return;

	first_desc = &srng->ring_base_vaddr[srng->u.dst_ring.tp];

	last_desc_index = (srng->u.dst_ring.tp +
			   (entry_count * srng->entry_size)) %
			  srng->ring_size;

	last_desc =  &srng->ring_base_vaddr[last_desc_index];

	if (last_desc > (uint32_t *)first_desc)
		/* invalidate from tp to cached_hp */
		qdf_nbuf_dma_inv_range_no_dsb((void *)first_desc,
					      (void *)(last_desc));
	else {
		/* invalidate from tp to end of the ring */
		qdf_nbuf_dma_inv_range_no_dsb((void *)first_desc,
					      (void *)srng->ring_vaddr_end);

		/* invalidate from start of ring to cached_hp */
		qdf_nbuf_dma_inv_range_no_dsb((void *)srng->ring_base_vaddr,
					      (void *)last_desc);
	}
	qdf_dsb();
}

/**
 * hal_srng_dst_num_valid_locked - Returns num valid entries to be processed
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Destination ring pointer
 * @sync_hw_ptr: Sync cached head pointer with HW
 *
 * Returns number of valid entries to be processed by the host driver. The
 * function takes up SRNG lock.
 *
 * Return: Number of valid destination entries
 */
static inline uint32_t
hal_srng_dst_num_valid_locked(hal_soc_handle_t hal_soc,
			      hal_ring_handle_t hal_ring_hdl,
			      int sync_hw_ptr)
{
	uint32_t num_valid;
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	SRNG_LOCK(&srng->lock);
	num_valid = hal_srng_dst_num_valid(hal_soc, hal_ring_hdl, sync_hw_ptr);
	SRNG_UNLOCK(&srng->lock);

	return num_valid;
}

/**
 * hal_srng_sync_cachedhp - sync cachehp pointer from hw hp
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Destination ring pointer
 *
 */
static inline
void hal_srng_sync_cachedhp(void *hal_soc,
				hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t hp;

	hp = *(volatile uint32_t *)(srng->u.dst_ring.hp_addr);
	srng->u.dst_ring.cached_hp = hp;
}

/**
 * hal_srng_src_reap_next - Reap next entry from a source ring and move reap
 * pointer. This can be used to release any buffers associated with completed
 * ring entries. Note that this should not be used for posting new descriptor
 * entries. Posting of new entries should be done only using
 * hal_srng_src_get_next_reaped when this function is used for reaping.
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Source ring pointer
 *
 * Return: Opaque pointer for next ring entry; NULL on failire
 */
static inline void *
hal_srng_src_reap_next(void *hal_soc, hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t *desc;

	/* TODO: Using % is expensive, but we have to do this since
	 * size of some SRNG rings is not power of 2 (due to descriptor
	 * sizes). Need to create separate API for rings used
	 * per-packet, with sizes power of 2 (TCL2SW, REO2SW,
	 * SW2RXDMA and CE rings)
	 */
	uint32_t next_reap_hp = (srng->u.src_ring.reap_hp + srng->entry_size) %
		srng->ring_size;

	if (next_reap_hp != srng->u.src_ring.cached_tp) {
		desc = &(srng->ring_base_vaddr[next_reap_hp]);
		srng->u.src_ring.reap_hp = next_reap_hp;
		return (void *)desc;
	}

	return NULL;
}

/**
 * hal_srng_src_get_next_reaped - Get next entry from a source ring that is
 * already reaped using hal_srng_src_reap_next, for posting new entries to
 * the ring
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Source ring pointer
 *
 * Return: Opaque pointer for next (reaped) source ring entry; NULL on failire
 */
static inline void *
hal_srng_src_get_next_reaped(void *hal_soc, hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t *desc;

	if (srng->u.src_ring.hp != srng->u.src_ring.reap_hp) {
		desc = &(srng->ring_base_vaddr[srng->u.src_ring.hp]);
		srng->u.src_ring.hp = (srng->u.src_ring.hp + srng->entry_size) %
			srng->ring_size;

		return (void *)desc;
	}

	return NULL;
}

/**
 * hal_srng_src_pending_reap_next - Reap next entry from a source ring and
 * move reap pointer. This API is used in detach path to release any buffers
 * associated with ring entries which are pending reap.
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Source ring pointer
 *
 * Return: Opaque pointer for next ring entry; NULL on failire
 */
static inline void *
hal_srng_src_pending_reap_next(void *hal_soc, hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t *desc;

	uint32_t next_reap_hp = (srng->u.src_ring.reap_hp + srng->entry_size) %
		srng->ring_size;

	if (next_reap_hp != srng->u.src_ring.hp) {
		desc = &(srng->ring_base_vaddr[next_reap_hp]);
		srng->u.src_ring.reap_hp = next_reap_hp;
		return (void *)desc;
	}

	return NULL;
}

/**
 * hal_srng_src_done_val -
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Source ring pointer
 *
 * Return: Opaque pointer for next ring entry; NULL on failire
 */
static inline uint32_t
hal_srng_src_done_val(void *hal_soc, hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	/* TODO: Using % is expensive, but we have to do this since
	 * size of some SRNG rings is not power of 2 (due to descriptor
	 * sizes). Need to create separate API for rings used
	 * per-packet, with sizes power of 2 (TCL2SW, REO2SW,
	 * SW2RXDMA and CE rings)
	 */
	uint32_t next_reap_hp = (srng->u.src_ring.reap_hp + srng->entry_size) %
		srng->ring_size;

	if (next_reap_hp == srng->u.src_ring.cached_tp)
		return 0;

	if (srng->u.src_ring.cached_tp > next_reap_hp)
		return (srng->u.src_ring.cached_tp - next_reap_hp) /
			srng->entry_size;
	else
		return ((srng->ring_size - next_reap_hp) +
			srng->u.src_ring.cached_tp) / srng->entry_size;
}

/**
 * hal_get_entrysize_from_srng() - Retrieve ring entry size
 * @hal_ring_hdl: Source ring pointer
 *
 * srng->entry_size value is in 4 byte dwords so left shifting
 * this by 2 to return the value of entry_size in bytes.
 *
 * Return: uint8_t
 */
static inline
uint8_t hal_get_entrysize_from_srng(hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	return srng->entry_size << 2;
}

/**
 * hal_get_sw_hptp - Get SW head and tail pointer location for any ring
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Source ring pointer
 * @tailp: Tail Pointer
 * @headp: Head Pointer
 *
 * Return: Update tail pointer and head pointer in arguments.
 */
static inline
void hal_get_sw_hptp(void *hal_soc, hal_ring_handle_t hal_ring_hdl,
		     uint32_t *tailp, uint32_t *headp)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	if (srng->ring_dir == HAL_SRNG_SRC_RING) {
		*headp = srng->u.src_ring.hp;
		*tailp = *srng->u.src_ring.tp_addr;
	} else {
		*tailp = srng->u.dst_ring.tp;
		*headp = *srng->u.dst_ring.hp_addr;
	}
}

#if defined(CLEAR_SW2TCL_CONSUMED_DESC)
/**
 * hal_srng_src_get_next_consumed - Get the next desc if consumed by HW
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Source ring pointer
 *
 * Return: pointer to descriptor if consumed by HW, else NULL
 */
static inline
void *hal_srng_src_get_next_consumed(void *hal_soc,
				     hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t *desc = NULL;
	/* TODO: Using % is expensive, but we have to do this since
	 * size of some SRNG rings is not power of 2 (due to descriptor
	 * sizes). Need to create separate API for rings used
	 * per-packet, with sizes power of 2 (TCL2SW, REO2SW,
	 * SW2RXDMA and CE rings)
	 */
	uint32_t next_entry = (srng->last_desc_cleared + srng->entry_size) %
			      srng->ring_size;

	if (next_entry != srng->u.src_ring.cached_tp) {
		desc = &srng->ring_base_vaddr[next_entry];
		srng->last_desc_cleared = next_entry;
	}

	return desc;
}

#else
static inline
void *hal_srng_src_get_next_consumed(void *hal_soc,
				     hal_ring_handle_t hal_ring_hdl)
{
	return NULL;
}
#endif /* CLEAR_SW2TCL_CONSUMED_DESC */

/**
 * hal_srng_src_peek - get the HP of the SRC ring
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Source ring pointer
 *
 * get the head pointer in the src ring but do not increment it
 */
static inline
void *hal_srng_src_peek(void *hal_soc, hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t *desc;
	uint32_t next_hp = (srng->u.src_ring.hp + srng->entry_size) %
		srng->ring_size;

	if (next_hp != srng->u.src_ring.cached_tp) {
		desc = &(srng->ring_base_vaddr[srng->u.src_ring.hp]);
		return (void *)desc;
	}

	return NULL;
}

/**
 * hal_srng_src_get_next - Get next entry from a source ring and move cached tail pointer
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Source ring pointer
 *
 * Return: Opaque pointer for next ring entry; NULL on failire
 */
static inline
void *hal_srng_src_get_next(void *hal_soc,
			    hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t *desc;
	/* TODO: Using % is expensive, but we have to do this since
	 * size of some SRNG rings is not power of 2 (due to descriptor
	 * sizes). Need to create separate API for rings used
	 * per-packet, with sizes power of 2 (TCL2SW, REO2SW,
	 * SW2RXDMA and CE rings)
	 */
	uint32_t next_hp = (srng->u.src_ring.hp + srng->entry_size) %
		srng->ring_size;

	if (next_hp != srng->u.src_ring.cached_tp) {
		desc = &(srng->ring_base_vaddr[srng->u.src_ring.hp]);
		srng->u.src_ring.hp = next_hp;
		/* TODO: Since reap function is not used by all rings, we can
		 * remove the following update of reap_hp in this function
		 * if we can ensure that only hal_srng_src_get_next_reaped
		 * is used for the rings requiring reap functionality
		 */
		srng->u.src_ring.reap_hp = next_hp;
		return (void *)desc;
	}

	return NULL;
}

/**
 * hal_srng_src_peek_n_get_next - Get next entry from a ring without
 * moving head pointer.
 * hal_srng_src_get_next should be called subsequently to move the head pointer
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Source ring pointer
 *
 * Return: Opaque pointer for next ring entry; NULL on failire
 */
static inline
void *hal_srng_src_peek_n_get_next(hal_soc_handle_t hal_soc_hdl,
				   hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t *desc;

	/* TODO: Using % is expensive, but we have to do this since
	 * size of some SRNG rings is not power of 2 (due to descriptor
	 * sizes). Need to create separate API for rings used
	 * per-packet, with sizes power of 2 (TCL2SW, REO2SW,
	 * SW2RXDMA and CE rings)
	 */
	if (((srng->u.src_ring.hp + srng->entry_size) %
		srng->ring_size) != srng->u.src_ring.cached_tp) {
		desc = &(srng->ring_base_vaddr[(srng->u.src_ring.hp +
						srng->entry_size) %
						srng->ring_size]);
		return (void *)desc;
	}

	return NULL;
}

/**
 * hal_srng_src_peek_n_get_next_next - Get next to next, i.e HP + 2 entry
 * from a ring without moving head pointer.
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Source ring pointer
 *
 * Return: Opaque pointer for next to next ring entry; NULL on failire
 */
static inline
void *hal_srng_src_peek_n_get_next_next(hal_soc_handle_t hal_soc_hdl,
					hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t *desc;

	/* TODO: Using % is expensive, but we have to do this since
	 * size of some SRNG rings is not power of 2 (due to descriptor
	 * sizes). Need to create separate API for rings used
	 * per-packet, with sizes power of 2 (TCL2SW, REO2SW,
	 * SW2RXDMA and CE rings)
	 */
	if ((((srng->u.src_ring.hp + (srng->entry_size)) %
		srng->ring_size) != srng->u.src_ring.cached_tp) &&
	    (((srng->u.src_ring.hp + (srng->entry_size * 2)) %
		srng->ring_size) != srng->u.src_ring.cached_tp)) {
		desc = &(srng->ring_base_vaddr[(srng->u.src_ring.hp +
						(srng->entry_size * 2)) %
						srng->ring_size]);
		return (void *)desc;
	}

	return NULL;
}

/**
 * hal_srng_src_get_cur_hp_n_move_next () - API returns current hp
 * and move hp to next in src ring
 *
 * Usage: This API should only be used at init time replenish.
 *
 * @hal_soc_hdl: HAL soc handle
 * @hal_ring_hdl: Source ring pointer
 *
 */
static inline void *
hal_srng_src_get_cur_hp_n_move_next(hal_soc_handle_t hal_soc_hdl,
				    hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t *cur_desc = NULL;
	uint32_t next_hp;

	cur_desc = &srng->ring_base_vaddr[(srng->u.src_ring.hp)];

	next_hp = (srng->u.src_ring.hp + srng->entry_size) %
		srng->ring_size;

	if (next_hp != srng->u.src_ring.cached_tp)
		srng->u.src_ring.hp = next_hp;

	return (void *)cur_desc;
}

/**
 * hal_srng_src_num_avail - Returns number of available entries in src ring
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Source ring pointer
 * @sync_hw_ptr: Sync cached tail pointer with HW
 *
 */
static inline uint32_t
hal_srng_src_num_avail(void *hal_soc,
		       hal_ring_handle_t hal_ring_hdl, int sync_hw_ptr)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t tp;
	uint32_t hp = srng->u.src_ring.hp;

	if (sync_hw_ptr) {
		tp = *(srng->u.src_ring.tp_addr);
		srng->u.src_ring.cached_tp = tp;
	} else {
		tp = srng->u.src_ring.cached_tp;
	}

	if (tp > hp)
		return ((tp - hp) / srng->entry_size) - 1;
	else
		return ((srng->ring_size - hp + tp) / srng->entry_size) - 1;
}

#ifdef WLAN_DP_SRNG_USAGE_WM_TRACKING
/**
 * hal_srng_clear_ring_usage_wm_locked() - Clear SRNG usage watermark stats
 * @hal_soc_hdl: HAL soc handle
 * @hal_ring_hdl: SRNG handle
 *
 * This function tries to acquire SRNG lock, and hence should not be called
 * from a context which has already acquired the SRNG lock.
 *
 * Return: None
 */
static inline
void hal_srng_clear_ring_usage_wm_locked(hal_soc_handle_t hal_soc_hdl,
					 hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	SRNG_LOCK(&srng->lock);
	srng->high_wm.val = 0;
	srng->high_wm.timestamp = 0;
	qdf_mem_zero(&srng->high_wm.bins[0], sizeof(srng->high_wm.bins[0]) *
					     HAL_SRNG_HIGH_WM_BIN_MAX);
	SRNG_UNLOCK(&srng->lock);
}

/**
 * hal_srng_update_ring_usage_wm_no_lock() - Update the SRNG usage wm stats
 * @hal_soc_hdl: HAL soc handle
 * @hal_ring_hdl: SRNG handle
 *
 * This function should be called with the SRNG lock held.
 *
 * Return: None
 */
static inline
void hal_srng_update_ring_usage_wm_no_lock(hal_soc_handle_t hal_soc_hdl,
					   hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t curr_wm_val = 0;

	if (srng->ring_dir == HAL_SRNG_SRC_RING)
		curr_wm_val = hal_srng_src_num_avail(hal_soc_hdl, hal_ring_hdl,
						     0);
	else
		curr_wm_val = hal_srng_dst_num_valid(hal_soc_hdl, hal_ring_hdl,
						     0);

	if (curr_wm_val > srng->high_wm.val) {
		srng->high_wm.val = curr_wm_val;
		srng->high_wm.timestamp = qdf_get_system_timestamp();
	}

	if (curr_wm_val >=
		srng->high_wm.bin_thresh[HAL_SRNG_HIGH_WM_BIN_90_to_100])
		srng->high_wm.bins[HAL_SRNG_HIGH_WM_BIN_90_to_100]++;
	else if (curr_wm_val >=
		 srng->high_wm.bin_thresh[HAL_SRNG_HIGH_WM_BIN_80_to_90])
		srng->high_wm.bins[HAL_SRNG_HIGH_WM_BIN_80_to_90]++;
	else if (curr_wm_val >=
		 srng->high_wm.bin_thresh[HAL_SRNG_HIGH_WM_BIN_70_to_80])
		srng->high_wm.bins[HAL_SRNG_HIGH_WM_BIN_70_to_80]++;
	else if (curr_wm_val >=
		 srng->high_wm.bin_thresh[HAL_SRNG_HIGH_WM_BIN_60_to_70])
		srng->high_wm.bins[HAL_SRNG_HIGH_WM_BIN_60_to_70]++;
	else if (curr_wm_val >=
		 srng->high_wm.bin_thresh[HAL_SRNG_HIGH_WM_BIN_50_to_60])
		srng->high_wm.bins[HAL_SRNG_HIGH_WM_BIN_50_to_60]++;
	else
		srng->high_wm.bins[HAL_SRNG_HIGH_WM_BIN_BELOW_50_PERCENT]++;
}

static inline
int hal_dump_srng_high_wm_stats(hal_soc_handle_t hal_soc_hdl,
				hal_ring_handle_t hal_ring_hdl,
				char *buf, int buf_len, int pos)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	return qdf_scnprintf(buf + pos, buf_len - pos,
			     "%8u %7u %12llu %10u %10u %10u %10u %10u %10u",
			     srng->ring_id, srng->high_wm.val,
			     srng->high_wm.timestamp,
			     srng->high_wm.bins[HAL_SRNG_HIGH_WM_BIN_BELOW_50_PERCENT],
			     srng->high_wm.bins[HAL_SRNG_HIGH_WM_BIN_50_to_60],
			     srng->high_wm.bins[HAL_SRNG_HIGH_WM_BIN_60_to_70],
			     srng->high_wm.bins[HAL_SRNG_HIGH_WM_BIN_70_to_80],
			     srng->high_wm.bins[HAL_SRNG_HIGH_WM_BIN_80_to_90],
			     srng->high_wm.bins[HAL_SRNG_HIGH_WM_BIN_90_to_100]);
}
#else
/**
 * hal_srng_clear_ring_usage_wm_locked() - Clear SRNG usage watermark stats
 * @hal_soc_hdl: HAL soc handle
 * @hal_ring_hdl: SRNG handle
 *
 * This function tries to acquire SRNG lock, and hence should not be called
 * from a context which has already acquired the SRNG lock.
 *
 * Return: None
 */
static inline
void hal_srng_clear_ring_usage_wm_locked(hal_soc_handle_t hal_soc_hdl,
					 hal_ring_handle_t hal_ring_hdl)
{
}

/**
 * hal_srng_update_ring_usage_wm_no_lock() - Update the SRNG usage wm stats
 * @hal_soc_hdl: HAL soc handle
 * @hal_ring_hdl: SRNG handle
 *
 * This function should be called with the SRNG lock held.
 *
 * Return: None
 */
static inline
void hal_srng_update_ring_usage_wm_no_lock(hal_soc_handle_t hal_soc_hdl,
					   hal_ring_handle_t hal_ring_hdl)
{
}

static inline
int hal_dump_srng_high_wm_stats(hal_soc_handle_t hal_soc_hdl,
				hal_ring_handle_t hal_ring_hdl,
				char *buf, int buf_len, int pos)
{
	return 0;
}
#endif

/**
 * hal_srng_access_end_unlocked - End ring access (unlocked) - update cached
 * ring head/tail pointers to HW.
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Ring pointer (Source or Destination ring)
 *
 * The target expects cached head/tail pointer to be updated to the
 * shared location in the little-endian order, This API ensures that.
 * This API should be used only if hal_srng_access_start_unlocked was used to
 * start ring access
 *
 * Return: None
 */
static inline void
hal_srng_access_end_unlocked(void *hal_soc, hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	/* TODO: See if we need a write memory barrier here */
	if (srng->flags & HAL_SRNG_LMAC_RING) {
		/* For LMAC rings, ring pointer updates are done through FW and
		 * hence written to a shared memory location that is read by FW
		 */
		if (srng->ring_dir == HAL_SRNG_SRC_RING) {
			*srng->u.src_ring.hp_addr =
				qdf_cpu_to_le32(srng->u.src_ring.hp);
		} else {
			*srng->u.dst_ring.tp_addr =
				qdf_cpu_to_le32(srng->u.dst_ring.tp);
		}
	} else {
		if (srng->ring_dir == HAL_SRNG_SRC_RING)
			hal_srng_write_address_32_mb(hal_soc,
						     srng,
						     srng->u.src_ring.hp_addr,
						     srng->u.src_ring.hp);
		else
			hal_srng_write_address_32_mb(hal_soc,
						     srng,
						     srng->u.dst_ring.tp_addr,
						     srng->u.dst_ring.tp);
	}
}

/* hal_srng_access_end_unlocked already handles endianness conversion,
 * use the same.
 */
#define hal_le_srng_access_end_unlocked_in_cpu_order \
	hal_srng_access_end_unlocked

/**
 * hal_srng_access_end - Unlock ring access and update cached ring head/tail
 * pointers to HW
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Ring pointer (Source or Destination ring)
 *
 * The target expects cached head/tail pointer to be updated to the
 * shared location in the little-endian order, This API ensures that.
 * This API should be used only if hal_srng_access_start was used to
 * start ring access
 *
 */
static inline void
hal_srng_access_end(void *hal_soc, hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	if (qdf_unlikely(!hal_ring_hdl)) {
		qdf_print("Error: Invalid hal_ring\n");
		return;
	}

	hal_srng_access_end_unlocked(hal_soc, hal_ring_hdl);
	SRNG_UNLOCK(&(srng->lock));
}

#ifdef FEATURE_RUNTIME_PM
#define hal_srng_access_end_v1 hal_srng_rtpm_access_end

/**
 * hal_srng_rtpm_access_end - RTPM aware, Unlock ring access
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Ring pointer (Source or Destination ring)
 * @rtpm_dbgid: RTPM debug id
 * @is_critical_ctx: Whether the calling context is critical
 *
 * Function updates the HP/TP value to the hardware register.
 * The target expects cached head/tail pointer to be updated to the
 * shared location in the little-endian order, This API ensures that.
 * This API should be used only if hal_srng_access_start was used to
 * start ring access
 *
 * Return: None
 */
void
hal_srng_rtpm_access_end(hal_soc_handle_t hal_soc_hdl,
			 hal_ring_handle_t hal_ring_hdl,
			 uint32_t rtpm_id);
#else
#define hal_srng_access_end_v1(hal_soc_hdl, hal_ring_hdl, rtpm_id) \
	hal_srng_access_end(hal_soc_hdl, hal_ring_hdl)
#endif

/* hal_srng_access_end already handles endianness conversion, so use the same */
#define hal_le_srng_access_end_in_cpu_order \
	hal_srng_access_end

/**
 * hal_srng_access_end_reap - Unlock ring access
 * This should be used only if hal_srng_access_start to start ring access
 * and should be used only while reaping SRC ring completions
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Ring pointer (Source or Destination ring)
 *
 * Return: 0 on success; error on failire
 */
static inline void
hal_srng_access_end_reap(void *hal_soc, hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	SRNG_UNLOCK(&(srng->lock));
}

/* TODO: Check if the following definitions is available in HW headers */
#define WBM_IDLE_SCATTER_BUF_SIZE 32704
#define NUM_MPDUS_PER_LINK_DESC 6
#define NUM_MSDUS_PER_LINK_DESC 7
#define REO_QUEUE_DESC_ALIGN 128

#define LINK_DESC_ALIGN 128

#define ADDRESS_MATCH_TAG_VAL 0x5
/* Number of mpdu link pointers is 9 in case of TX_MPDU_QUEUE_HEAD and 14 in
 * of TX_MPDU_QUEUE_EXT. We are defining a common average count here
 */
#define NUM_MPDU_LINKS_PER_QUEUE_DESC 12

/* TODO: Check with HW team on the scatter buffer size supported. As per WBM
 * MLD, scatter_buffer_size in IDLE_LIST_CONTROL register is 9 bits and size
 * should be specified in 16 word units. But the number of bits defined for
 * this field in HW header files is 5.
 */
#define WBM_IDLE_SCATTER_BUF_NEXT_PTR_SIZE 8


/**
 * hal_idle_list_scatter_buf_size - Get the size of each scatter buffer
 * in an idle list
 *
 * @hal_soc: Opaque HAL SOC handle
 *
 */
static inline
uint32_t hal_idle_list_scatter_buf_size(hal_soc_handle_t hal_soc_hdl)
{
	return WBM_IDLE_SCATTER_BUF_SIZE;
}

/**
 * hal_get_link_desc_size - Get the size of each link descriptor
 *
 * @hal_soc: Opaque HAL SOC handle
 *
 */
static inline uint32_t hal_get_link_desc_size(hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	if (!hal_soc || !hal_soc->ops) {
		qdf_print("Error: Invalid ops\n");
		QDF_BUG(0);
		return -EINVAL;
	}
	if (!hal_soc->ops->hal_get_link_desc_size) {
		qdf_print("Error: Invalid function pointer\n");
		QDF_BUG(0);
		return -EINVAL;
	}
	return hal_soc->ops->hal_get_link_desc_size();
}

/**
 * hal_get_link_desc_align - Get the required start address alignment for
 * link descriptors
 *
 * @hal_soc: Opaque HAL SOC handle
 *
 */
static inline
uint32_t hal_get_link_desc_align(hal_soc_handle_t hal_soc_hdl)
{
	return LINK_DESC_ALIGN;
}

/**
 * hal_num_mpdus_per_link_desc - Get number of mpdus each link desc can hold
 *
 * @hal_soc: Opaque HAL SOC handle
 *
 */
static inline
uint32_t hal_num_mpdus_per_link_desc(hal_soc_handle_t hal_soc_hdl)
{
	return NUM_MPDUS_PER_LINK_DESC;
}

/**
 * hal_num_msdus_per_link_desc - Get number of msdus each link desc can hold
 *
 * @hal_soc: Opaque HAL SOC handle
 *
 */
static inline
uint32_t hal_num_msdus_per_link_desc(hal_soc_handle_t hal_soc_hdl)
{
	return NUM_MSDUS_PER_LINK_DESC;
}

/**
 * hal_num_mpdu_links_per_queue_desc - Get number of mpdu links each queue
 * descriptor can hold
 *
 * @hal_soc: Opaque HAL SOC handle
 *
 */
static inline
uint32_t hal_num_mpdu_links_per_queue_desc(hal_soc_handle_t hal_soc_hdl)
{
	return NUM_MPDU_LINKS_PER_QUEUE_DESC;
}

/**
 * hal_idle_list_scatter_buf_num_entries - Get the number of link desc entries
 * that the given buffer size
 *
 * @hal_soc: Opaque HAL SOC handle
 * @scatter_buf_size: Size of scatter buffer
 *
 */
static inline
uint32_t hal_idle_scatter_buf_num_entries(hal_soc_handle_t hal_soc_hdl,
					  uint32_t scatter_buf_size)
{
	return (scatter_buf_size - WBM_IDLE_SCATTER_BUF_NEXT_PTR_SIZE) /
		hal_srng_get_entrysize(hal_soc_hdl, WBM_IDLE_LINK);
}

/**
 * hal_idle_list_num_scatter_bufs - Get the number of sctater buffer
 * each given buffer size
 *
 * @hal_soc: Opaque HAL SOC handle
 * @total_mem: size of memory to be scattered
 * @scatter_buf_size: Size of scatter buffer
 *
 */
static inline
uint32_t hal_idle_list_num_scatter_bufs(hal_soc_handle_t hal_soc_hdl,
					uint32_t total_mem,
					uint32_t scatter_buf_size)
{
	uint8_t rem = (total_mem % (scatter_buf_size -
			WBM_IDLE_SCATTER_BUF_NEXT_PTR_SIZE)) ? 1 : 0;

	uint32_t num_scatter_bufs = (total_mem / (scatter_buf_size -
				WBM_IDLE_SCATTER_BUF_NEXT_PTR_SIZE)) + rem;

	return num_scatter_bufs;
}

enum hal_pn_type {
	HAL_PN_NONE,
	HAL_PN_WPA,
	HAL_PN_WAPI_EVEN,
	HAL_PN_WAPI_UNEVEN,
};

#define HAL_RX_BA_WINDOW_256 256
#define HAL_RX_BA_WINDOW_1024 1024

/**
 * hal_get_reo_qdesc_align - Get start address alignment for reo
 * queue descriptors
 *
 * @hal_soc: Opaque HAL SOC handle
 *
 */
static inline
uint32_t hal_get_reo_qdesc_align(hal_soc_handle_t hal_soc_hdl)
{
	return REO_QUEUE_DESC_ALIGN;
}

/**
 * hal_srng_get_hp_addr - Get head pointer physical address
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Ring pointer (Source or Destination ring)
 *
 */
static inline qdf_dma_addr_t
hal_srng_get_hp_addr(void *hal_soc,
		     hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	struct hal_soc *hal = (struct hal_soc *)hal_soc;

	if (srng->ring_dir == HAL_SRNG_SRC_RING) {
		if (srng->flags & HAL_SRNG_LMAC_RING)
			return hal->shadow_wrptr_mem_paddr +
				 ((unsigned long)(srng->u.src_ring.hp_addr) -
				  (unsigned long)(hal->shadow_wrptr_mem_vaddr));
		else if (ignore_shadow)
			return (qdf_dma_addr_t)srng->u.src_ring.hp_addr;
		else
			return ((struct hif_softc *)hal->hif_handle)->mem_pa +
				((unsigned long)srng->u.src_ring.hp_addr -
				 (unsigned long)hal->dev_base_addr);

	} else {
		return hal->shadow_rdptr_mem_paddr +
		  ((unsigned long)(srng->u.dst_ring.hp_addr) -
		   (unsigned long)(hal->shadow_rdptr_mem_vaddr));
	}
}

/**
 * hal_srng_get_tp_addr - Get tail pointer physical address
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Ring pointer (Source or Destination ring)
 *
 */
static inline qdf_dma_addr_t
hal_srng_get_tp_addr(void *hal_soc, hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	struct hal_soc *hal = (struct hal_soc *)hal_soc;

	if (srng->ring_dir == HAL_SRNG_SRC_RING) {
		return hal->shadow_rdptr_mem_paddr +
			((unsigned long)(srng->u.src_ring.tp_addr) -
			(unsigned long)(hal->shadow_rdptr_mem_vaddr));
	} else {
		if (srng->flags & HAL_SRNG_LMAC_RING)
			return hal->shadow_wrptr_mem_paddr +
				((unsigned long)(srng->u.dst_ring.tp_addr) -
				 (unsigned long)(hal->shadow_wrptr_mem_vaddr));
		else if (ignore_shadow)
			return (qdf_dma_addr_t)srng->u.dst_ring.tp_addr;
		else
			return ((struct hif_softc *)hal->hif_handle)->mem_pa +
				((unsigned long)srng->u.dst_ring.tp_addr -
				 (unsigned long)hal->dev_base_addr);
	}
}

/**
 * hal_srng_get_num_entries - Get total entries in the HAL Srng
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Ring pointer (Source or Destination ring)
 *
 * Return: total number of entries in hal ring
 */
static inline
uint32_t hal_srng_get_num_entries(hal_soc_handle_t hal_soc_hdl,
				  hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	return srng->num_entries;
}

/**
 * hal_get_srng_params - Retrieve SRNG parameters for a given ring from HAL
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Ring pointer (Source or Destination ring)
 * @ring_params: SRNG parameters will be returned through this structure
 */
void hal_get_srng_params(hal_soc_handle_t hal_soc_hdl,
			 hal_ring_handle_t hal_ring_hdl,
			 struct hal_srng_params *ring_params);

/**
 * hal_mem_info - Retrieve hal memory base address
 *
 * @hal_soc: Opaque HAL SOC handle
 * @mem: pointer to structure to be updated with hal mem info
 */
void hal_get_meminfo(hal_soc_handle_t hal_soc_hdl, struct hal_mem_info *mem);

/**
 * hal_get_target_type - Return target type
 *
 * @hal_soc: Opaque HAL SOC handle
 */
uint32_t hal_get_target_type(hal_soc_handle_t hal_soc_hdl);

/**
 * hal_srng_dst_hw_init - Private function to initialize SRNG
 * destination ring HW
 * @hal_soc: HAL SOC handle
 * @srng: SRNG ring pointer
 * @idle_check: Check if ring is idle
 * @idx: Ring index
 */
static inline void hal_srng_dst_hw_init(struct hal_soc *hal,
					struct hal_srng *srng, bool idle_check,
					uint16_t idx)
{
	hal->ops->hal_srng_dst_hw_init(hal, srng, idle_check, idx);
}

/**
 * hal_srng_src_hw_init - Private function to initialize SRNG
 * source ring HW
 * @hal_soc: HAL SOC handle
 * @srng: SRNG ring pointer
 * @idle_check: Check if ring is idle
 * @idx: Ring index
 */
static inline void hal_srng_src_hw_init(struct hal_soc *hal,
					struct hal_srng *srng, bool idle_check,
					uint16_t idx)
{
	hal->ops->hal_srng_src_hw_init(hal, srng, idle_check, idx);
}

/**
 * hal_srng_hw_disable - Private function to disable SRNG
 * source ring HW
 * @hal_soc: HAL SOC handle
 * @srng: SRNG ring pointer
 */
static inline
void hal_srng_hw_disable(struct hal_soc *hal_soc, struct hal_srng *srng)
{
	if (hal_soc->ops->hal_srng_hw_disable)
		hal_soc->ops->hal_srng_hw_disable(hal_soc, srng);
}

/**
 * hal_get_hw_hptp()  - Get HW head and tail pointer value for any ring
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Source ring pointer
 * @headp: Head Pointer
 * @tailp: Tail Pointer
 * @ring_type: Ring
 *
 * Return: Update tail pointer and head pointer in arguments.
 */
static inline
void hal_get_hw_hptp(hal_soc_handle_t hal_soc_hdl,
		     hal_ring_handle_t hal_ring_hdl,
		     uint32_t *headp, uint32_t *tailp,
		     uint8_t ring_type)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_get_hw_hptp(hal_soc, hal_ring_hdl,
			headp, tailp, ring_type);
}

/**
 * hal_reo_setup - Initialize HW REO block
 *
 * @hal_soc: Opaque HAL SOC handle
 * @reo_params: parameters needed by HAL for REO config
 * @qref_reset: reset qref
 */
static inline void hal_reo_setup(hal_soc_handle_t hal_soc_hdl,
				 void *reoparams, int qref_reset)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_reo_setup(hal_soc, reoparams, qref_reset);
}

static inline
void hal_compute_reo_remap_ix2_ix3(hal_soc_handle_t hal_soc_hdl,
				   uint32_t *ring, uint32_t num_rings,
				   uint32_t *remap1, uint32_t *remap2)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	return hal_soc->ops->hal_compute_reo_remap_ix2_ix3(ring,
					num_rings, remap1, remap2);
}

static inline
void hal_compute_reo_remap_ix0(hal_soc_handle_t hal_soc_hdl, uint32_t *remap0)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	if (hal_soc->ops->hal_compute_reo_remap_ix0)
		hal_soc->ops->hal_compute_reo_remap_ix0(remap0);
}

/**
 * hal_setup_link_idle_list - Setup scattered idle list using the
 * buffer list provided
 *
 * @hal_soc: Opaque HAL SOC handle
 * @scatter_bufs_base_paddr: Array of physical base addresses
 * @scatter_bufs_base_vaddr: Array of virtual base addresses
 * @num_scatter_bufs: Number of scatter buffers in the above lists
 * @scatter_buf_size: Size of each scatter buffer
 * @last_buf_end_offset: Offset to the last entry
 * @num_entries: Total entries of all scatter bufs
 *
 */
static inline
void hal_setup_link_idle_list(hal_soc_handle_t hal_soc_hdl,
			      qdf_dma_addr_t scatter_bufs_base_paddr[],
			      void *scatter_bufs_base_vaddr[],
			      uint32_t num_scatter_bufs,
			      uint32_t scatter_buf_size,
			      uint32_t last_buf_end_offset,
			      uint32_t num_entries)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	hal_soc->ops->hal_setup_link_idle_list(hal_soc, scatter_bufs_base_paddr,
			scatter_bufs_base_vaddr, num_scatter_bufs,
			scatter_buf_size, last_buf_end_offset,
			num_entries);

}

#ifdef DUMP_REO_QUEUE_INFO_IN_DDR
/**
 * hal_dump_rx_reo_queue_desc() - Dump reo queue descriptor fields
 * @hw_qdesc_vaddr_aligned: Pointer to hw reo queue desc virtual addr
 *
 * Use the virtual addr pointer to reo h/w queue desc to read
 * the values from ddr and log them.
 *
 * Return: none
 */
static inline void hal_dump_rx_reo_queue_desc(
	void *hw_qdesc_vaddr_aligned)
{
	struct rx_reo_queue *hw_qdesc =
		(struct rx_reo_queue *)hw_qdesc_vaddr_aligned;

	if (!hw_qdesc)
		return;

	hal_info("receive_queue_number %u vld %u window_jump_2k %u"
		 " hole_count %u ba_window_size %u ignore_ampdu_flag %u"
		 " svld %u ssn %u current_index %u"
		 " disable_duplicate_detection %u soft_reorder_enable %u"
		 " chk_2k_mode %u oor_mode %u mpdu_frames_processed_count %u"
		 " msdu_frames_processed_count %u total_processed_byte_count %u"
		 " late_receive_mpdu_count %u seq_2k_error_detected_flag %u"
		 " pn_error_detected_flag %u current_mpdu_count %u"
		 " current_msdu_count %u timeout_count %u"
		 " forward_due_to_bar_count %u duplicate_count %u"
		 " frames_in_order_count %u bar_received_count %u"
		 " pn_check_needed %u pn_shall_be_even %u"
		 " pn_shall_be_uneven %u pn_size %u",
		 hw_qdesc->receive_queue_number,
		 hw_qdesc->vld,
		 hw_qdesc->window_jump_2k,
		 hw_qdesc->hole_count,
		 hw_qdesc->ba_window_size,
		 hw_qdesc->ignore_ampdu_flag,
		 hw_qdesc->svld,
		 hw_qdesc->ssn,
		 hw_qdesc->current_index,
		 hw_qdesc->disable_duplicate_detection,
		 hw_qdesc->soft_reorder_enable,
		 hw_qdesc->chk_2k_mode,
		 hw_qdesc->oor_mode,
		 hw_qdesc->mpdu_frames_processed_count,
		 hw_qdesc->msdu_frames_processed_count,
		 hw_qdesc->total_processed_byte_count,
		 hw_qdesc->late_receive_mpdu_count,
		 hw_qdesc->seq_2k_error_detected_flag,
		 hw_qdesc->pn_error_detected_flag,
		 hw_qdesc->current_mpdu_count,
		 hw_qdesc->current_msdu_count,
		 hw_qdesc->timeout_count,
		 hw_qdesc->forward_due_to_bar_count,
		 hw_qdesc->duplicate_count,
		 hw_qdesc->frames_in_order_count,
		 hw_qdesc->bar_received_count,
		 hw_qdesc->pn_check_needed,
		 hw_qdesc->pn_shall_be_even,
		 hw_qdesc->pn_shall_be_uneven,
		 hw_qdesc->pn_size);
}

#else /* DUMP_REO_QUEUE_INFO_IN_DDR */

static inline void hal_dump_rx_reo_queue_desc(
	void *hw_qdesc_vaddr_aligned)
{
}
#endif /* DUMP_REO_QUEUE_INFO_IN_DDR */

/**
 * hal_srng_dump_ring_desc() - Dump ring descriptor info
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Source ring pointer
 * @ring_desc: Opaque ring descriptor handle
 */
static inline void hal_srng_dump_ring_desc(hal_soc_handle_t hal_soc_hdl,
					   hal_ring_handle_t hal_ring_hdl,
					   hal_ring_desc_t ring_desc)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_INFO_HIGH,
			   ring_desc, (srng->entry_size << 2));
}

/**
 * hal_srng_dump_ring() - Dump last 128 descs of the ring
 *
 * @hal_soc: Opaque HAL SOC handle
 * @hal_ring_hdl: Source ring pointer
 */
static inline void hal_srng_dump_ring(hal_soc_handle_t hal_soc_hdl,
				      hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t *desc;
	uint32_t tp, i;

	tp = srng->u.dst_ring.tp;

	for (i = 0; i < 128; i++) {
		if (!tp)
			tp = srng->ring_size;

		desc = &srng->ring_base_vaddr[tp - srng->entry_size];
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_DP,
				   QDF_TRACE_LEVEL_DEBUG,
				   desc, (srng->entry_size << 2));

		tp -= srng->entry_size;
	}
}

/*
 * hal_rxdma_desc_to_hal_ring_desc - API to convert rxdma ring desc
 * to opaque dp_ring desc type
 * @ring_desc - rxdma ring desc
 *
 * Return: hal_rxdma_desc_t type
 */
static inline
hal_ring_desc_t hal_rxdma_desc_to_hal_ring_desc(hal_rxdma_desc_t ring_desc)
{
	return (hal_ring_desc_t)ring_desc;
}

/**
 * hal_srng_set_event() - Set hal_srng event
 * @hal_ring_hdl: Source ring pointer
 * @event: SRNG ring event
 *
 * Return: None
 */
static inline void hal_srng_set_event(hal_ring_handle_t hal_ring_hdl, int event)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	qdf_atomic_set_bit(event, &srng->srng_event);
}

/**
 * hal_srng_clear_event() - Clear hal_srng event
 * @hal_ring_hdl: Source ring pointer
 * @event: SRNG ring event
 *
 * Return: None
 */
static inline
void hal_srng_clear_event(hal_ring_handle_t hal_ring_hdl, int event)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	qdf_atomic_clear_bit(event, &srng->srng_event);
}

/**
 * hal_srng_get_clear_event() - Clear srng event and return old value
 * @hal_ring_hdl: Source ring pointer
 * @event: SRNG ring event
 *
 * Return: Return old event value
 */
static inline
int hal_srng_get_clear_event(hal_ring_handle_t hal_ring_hdl, int event)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	return qdf_atomic_test_and_clear_bit(event, &srng->srng_event);
}

/**
 * hal_srng_set_flush_last_ts() - Record last flush time stamp
 * @hal_ring_hdl: Source ring pointer
 *
 * Return: None
 */
static inline void hal_srng_set_flush_last_ts(hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	srng->last_flush_ts = qdf_get_log_timestamp();
}

/**
 * hal_srng_inc_flush_cnt() - Increment flush counter
 * @hal_ring_hdl: Source ring pointer
 *
 * Return: None
 */
static inline void hal_srng_inc_flush_cnt(hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	srng->flush_count++;
}

/**
 * hal_rx_sw_mon_desc_info_get () - Get SW monitor desc info
 *
 * @hal: Core HAL soc handle
 * @ring_desc: Mon dest ring descriptor
 * @desc_info: Desc info to be populated
 *
 * Return void
 */
static inline void
hal_rx_sw_mon_desc_info_get(struct hal_soc *hal,
			    hal_ring_desc_t ring_desc,
			    hal_rx_mon_desc_info_t desc_info)
{
	return hal->ops->hal_rx_sw_mon_desc_info_get(ring_desc, desc_info);
}

/**
 * hal_reo_set_err_dst_remap() - Set REO error destination ring remap
 *				 register value.
 *
 * @hal_soc_hdl: Opaque HAL soc handle
 *
 * Return: None
 */
static inline void hal_reo_set_err_dst_remap(hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	if (hal_soc->ops->hal_reo_set_err_dst_remap)
		hal_soc->ops->hal_reo_set_err_dst_remap(hal_soc);
}

/**
 * hal_reo_enable_pn_in_dest() - Subscribe for previous PN for 2k-jump or
 *			OOR error frames
 * @hal_soc_hdl: Opaque HAL soc handle
 *
 * Return: true if feature is enabled,
 *	false, otherwise.
 */
static inline uint8_t
hal_reo_enable_pn_in_dest(hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	if (hal_soc->ops->hal_reo_enable_pn_in_dest)
		return hal_soc->ops->hal_reo_enable_pn_in_dest(hal_soc);

	return 0;
}

#ifdef GENERIC_SHADOW_REGISTER_ACCESS_ENABLE

/**
 * hal_set_one_target_reg_config() - Populate the target reg
 * offset in hal_soc for one non srng related register at the
 * given list index
 * @hal_soc: hal handle
 * @target_reg_offset: target register offset
 * @list_index: index in hal list for shadow regs
 *
 * Return: none
 */
void hal_set_one_target_reg_config(struct hal_soc *hal,
				   uint32_t target_reg_offset,
				   int list_index);

/**
 * hal_set_shadow_regs() - Populate register offset for
 * registers that need to be populated in list_shadow_reg_config
 * in order to be sent to FW. These reg offsets will be mapped
 * to shadow registers.
 * @hal_soc: hal handle
 *
 * Return: QDF_STATUS_OK on success
 */
QDF_STATUS hal_set_shadow_regs(void *hal_soc);

/**
 * hal_construct_shadow_regs() - initialize the shadow registers
 * for non-srng related register configs
 * @hal_soc: hal handle
 *
 * Return: QDF_STATUS_OK on success
 */
QDF_STATUS hal_construct_shadow_regs(void *hal_soc);

#else /* GENERIC_SHADOW_REGISTER_ACCESS_ENABLE */
static inline void hal_set_one_target_reg_config(
	struct hal_soc *hal,
	uint32_t target_reg_offset,
	int list_index)
{
}

static inline QDF_STATUS hal_set_shadow_regs(void *hal_soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS hal_construct_shadow_regs(void *hal_soc)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* GENERIC_SHADOW_REGISTER_ACCESS_ENABLE */

#ifdef FEATURE_HAL_DELAYED_REG_WRITE
/**
 * hal_flush_reg_write_work() - flush all writes from register write queue
 * @arg: hal_soc pointer
 *
 * Return: None
 */
void hal_flush_reg_write_work(hal_soc_handle_t hal_handle);

#else
static inline void hal_flush_reg_write_work(hal_soc_handle_t hal_handle) { }
#endif

/**
 * hal_get_ring_usage - Calculate the ring usage percentage
 * @hal_ring_hdl: Ring pointer
 * @ring_type: Ring type
 * @headp: pointer to head value
 * @tailp: pointer to tail value
 *
 * Calculate the ring usage percentage for src and dest rings
 *
 * Return: Ring usage percentage
 */
static inline
uint32_t hal_get_ring_usage(
	hal_ring_handle_t hal_ring_hdl,
	enum hal_ring_type ring_type, uint32_t *headp, uint32_t *tailp)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t num_avail, num_valid = 0;
	uint32_t ring_usage;

	if (srng->ring_dir == HAL_SRNG_SRC_RING) {
		if (*tailp > *headp)
			num_avail =  ((*tailp - *headp) / srng->entry_size) - 1;
		else
			num_avail = ((srng->ring_size - *headp + *tailp) /
				     srng->entry_size) - 1;
		if (ring_type == WBM_IDLE_LINK)
			num_valid = num_avail;
		else
			num_valid = srng->num_entries - num_avail;
	} else {
		if (*headp >= *tailp)
			num_valid = ((*headp - *tailp) / srng->entry_size);
		else
			num_valid = ((srng->ring_size - *tailp + *headp) /
				     srng->entry_size);
	}
	ring_usage = (100 * num_valid) / srng->num_entries;
	return ring_usage;
}

/**
 * hal_cmem_write() - function for CMEM buffer writing
 * @hal_soc_hdl: HAL SOC handle
 * @offset: CMEM address
 * @value: value to write
 *
 * Return: None.
 */
static inline void
hal_cmem_write(hal_soc_handle_t hal_soc_hdl, uint32_t offset,
	       uint32_t value)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	if (hal_soc->ops->hal_cmem_write)
		hal_soc->ops->hal_cmem_write(hal_soc_hdl, offset, value);

	return;
}

static inline bool
hal_dmac_cmn_src_rxbuf_ring_get(hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	return hal_soc->dmac_cmn_src_rxbuf_ring;
}

/**
 * hal_srng_dst_prefetch() - function to prefetch 4 destination ring descs
 * @hal_soc_hdl: HAL SOC handle
 * @hal_ring_hdl: Destination ring pointer
 * @num_valid: valid entries in the ring
 *
 * return: last prefetched destination ring descriptor
 */
static inline
void *hal_srng_dst_prefetch(hal_soc_handle_t hal_soc_hdl,
			    hal_ring_handle_t hal_ring_hdl,
			    uint16_t num_valid)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint8_t *desc;
	uint32_t cnt;
	/*
	 * prefetching 4 HW descriptors will ensure atleast by the time
	 * 5th HW descriptor is being processed it is guaranteed that the
	 * 5th HW descriptor, its SW Desc, its nbuf and its nbuf's data
	 * are in cache line. basically ensuring all the 4 (HW, SW, nbuf
	 * & nbuf->data) are prefetched.
	 */
	uint32_t max_prefetch = 4;

	if (srng->u.dst_ring.tp == srng->u.dst_ring.cached_hp)
		return NULL;

	desc = (uint8_t *)&srng->ring_base_vaddr[srng->u.dst_ring.tp];

	if (num_valid < max_prefetch)
		max_prefetch = num_valid;

	for (cnt = 0; cnt < max_prefetch; cnt++) {
		desc += srng->entry_size * sizeof(uint32_t);
		if (desc  == ((uint8_t *)srng->ring_vaddr_end))
			desc = (uint8_t *)&srng->ring_base_vaddr[0];

		qdf_prefetch(desc);
	}
	return (void *)desc;
}

/**
 * hal_srng_dst_prefetch_next_cached_desc() - function to prefetch next desc
 * @hal_soc_hdl: HAL SOC handle
 * @hal_ring_hdl: Destination ring pointer
 * @last_prefetched_hw_desc: last prefetched HW descriptor
 *
 * return: next prefetched destination descriptor
 */
static inline
void *hal_srng_dst_prefetch_next_cached_desc(hal_soc_handle_t hal_soc_hdl,
					     hal_ring_handle_t hal_ring_hdl,
					     uint8_t *last_prefetched_hw_desc)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	if (srng->u.dst_ring.tp == srng->u.dst_ring.cached_hp)
		return NULL;

	last_prefetched_hw_desc += srng->entry_size * sizeof(uint32_t);
	if (last_prefetched_hw_desc == ((uint8_t *)srng->ring_vaddr_end))
		last_prefetched_hw_desc = (uint8_t *)&srng->ring_base_vaddr[0];

	qdf_prefetch(last_prefetched_hw_desc);
	return (void *)last_prefetched_hw_desc;
}

/**
 * hal_srng_dst_prefetch_32_byte_desc() - function to prefetch a desc at
 *					  64 byte offset
 * @hal_soc_hdl: HAL SOC handle
 * @hal_ring_hdl: Destination ring pointer
 * @num_valid: valid entries in the ring
 *
 * return: last prefetched destination ring descriptor
 */
static inline
void *hal_srng_dst_prefetch_32_byte_desc(hal_soc_handle_t hal_soc_hdl,
					 hal_ring_handle_t hal_ring_hdl,
					 uint16_t num_valid)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint8_t *desc;

	if (srng->u.dst_ring.tp == srng->u.dst_ring.cached_hp)
		return NULL;

	desc = (uint8_t *)&srng->ring_base_vaddr[srng->u.dst_ring.tp];

	if ((uintptr_t)desc & 0x3f)
		desc += srng->entry_size * sizeof(uint32_t);
	else
		desc += (srng->entry_size * sizeof(uint32_t)) * 2;

	if (desc  == ((uint8_t *)srng->ring_vaddr_end))
		desc = (uint8_t *)&srng->ring_base_vaddr[0];

	qdf_prefetch(desc);

	return (void *)(desc + srng->entry_size * sizeof(uint32_t));
}

/**
 * hal_srng_dst_prefetch_next_cached_desc() - function to prefetch next desc
 * @hal_soc_hdl: HAL SOC handle
 * @hal_ring_hdl: Destination ring pointer
 * @last_prefetched_hw_desc: last prefetched HW descriptor
 *
 * return: next prefetched destination descriptor
 */
static inline
void *hal_srng_dst_get_next_32_byte_desc(hal_soc_handle_t hal_soc_hdl,
					 hal_ring_handle_t hal_ring_hdl,
					 uint8_t *last_prefetched_hw_desc)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	if (srng->u.dst_ring.tp == srng->u.dst_ring.cached_hp)
		return NULL;

	last_prefetched_hw_desc += srng->entry_size * sizeof(uint32_t);
	if (last_prefetched_hw_desc == ((uint8_t *)srng->ring_vaddr_end))
		last_prefetched_hw_desc = (uint8_t *)&srng->ring_base_vaddr[0];

	return (void *)last_prefetched_hw_desc;
}

/**
 * hal_srng_src_set_hp() - set head idx.
 * @hal_soc_hdl: HAL SOC handle
 * @idx: head idx
 *
 * return: none
 */
static inline
void hal_srng_src_set_hp(hal_ring_handle_t hal_ring_hdl, uint16_t idx)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	srng->u.src_ring.hp = idx * srng->entry_size;
}

/**
 * hal_srng_dst_set_tp() - set tail idx.
 * @hal_soc_hdl: HAL SOC handle
 * @idx: tail idx
 *
 * return: none
 */
static inline
void hal_srng_dst_set_tp(hal_ring_handle_t hal_ring_hdl, uint16_t idx)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;

	srng->u.dst_ring.tp = idx * srng->entry_size;
}

/**
 * hal_srng_src_get_tpidx() - get tail idx
 * @hal_soc_hdl: HAL SOC handle
 *
 * return: tail idx
 */
static inline
uint16_t hal_srng_src_get_tpidx(hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t tp = *(volatile uint32_t *)(srng->u.src_ring.tp_addr);

	return tp / srng->entry_size;
}

/**
 * hal_srng_dst_get_hpidx() - get head idx
 * @hal_soc_hdl: HAL SOC handle
 *
 * return: head idx
 */
static inline
uint16_t hal_srng_dst_get_hpidx(hal_ring_handle_t hal_ring_hdl)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	uint32_t hp = *(volatile uint32_t *)(srng->u.dst_ring.hp_addr);

	return hp / srng->entry_size;
}

#ifdef FEATURE_DIRECT_LINK
/**
 * hal_srng_set_msi_irq_config() - Set the MSI irq configuration for srng
 * @hal_soc_hdl: hal soc handle
 * @hal_ring_hdl: srng handle
 * @addr: MSI address
 * @data: MSI data
 *
 * Return: QDF status
 */
static inline QDF_STATUS
hal_srng_set_msi_irq_config(hal_soc_handle_t hal_soc_hdl,
			    hal_ring_handle_t hal_ring_hdl,
			    struct hal_srng_params *ring_params)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	return hal_soc->ops->hal_srng_set_msi_config(hal_ring_hdl, ring_params);
}
#else
static inline QDF_STATUS
hal_srng_set_msi_irq_config(hal_soc_handle_t hal_soc_hdl,
			    hal_ring_handle_t hal_ring_hdl,
			    struct hal_srng_params *ring_params)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif
#endif /* _HAL_APIH_ */
