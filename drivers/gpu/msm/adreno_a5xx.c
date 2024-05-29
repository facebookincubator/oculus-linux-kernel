// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk/qcom.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/qcom_scm.h>
#include <linux/slab.h>

#include "adreno.h"
#include "adreno_a5xx.h"
#include "adreno_a5xx_packets.h"
#include "adreno_pm4types.h"
#include "adreno_trace.h"
#include "kgsl_trace.h"

static int critical_packet_constructed;
static unsigned int crit_pkts_dwords;

static void a5xx_irq_storm_worker(struct work_struct *work);
static int _read_fw2_block_header(struct kgsl_device *device,
		uint32_t *header, uint32_t remain,
		uint32_t id, uint32_t major, uint32_t minor);
static void a5xx_gpmu_reset(struct work_struct *work);
static int a5xx_gpmu_init(struct adreno_device *adreno_dev);

/**
 * Number of times to check if the regulator enabled before
 * giving up and returning failure.
 */
#define PWR_RETRY 100

/**
 * Number of times to check if the GPMU firmware is initialized before
 * giving up and returning failure.
 */
#define GPMU_FW_INIT_RETRY 5000

#define A530_QFPROM_RAW_PTE_ROW0_MSB 0x134
#define A530_QFPROM_RAW_PTE_ROW2_MSB 0x144

#define A5XX_INT_MASK \
	((1 << A5XX_INT_RBBM_AHB_ERROR) |		\
	 (1 << A5XX_INT_RBBM_TRANSFER_TIMEOUT) |		\
	 (1 << A5XX_INT_RBBM_ME_MS_TIMEOUT) |		\
	 (1 << A5XX_INT_RBBM_PFP_MS_TIMEOUT) |		\
	 (1 << A5XX_INT_RBBM_ETS_MS_TIMEOUT) |		\
	 (1 << A5XX_INT_RBBM_ATB_ASYNC_OVERFLOW) |		\
	 (1 << A5XX_INT_RBBM_GPC_ERROR) |		\
	 (1 << A5XX_INT_CP_HW_ERROR) |	\
	 (1 << A5XX_INT_CP_CACHE_FLUSH_TS) |		\
	 (1 << A5XX_INT_RBBM_ATB_BUS_OVERFLOW) |	\
	 (1 << A5XX_INT_MISC_HANG_DETECT) |		\
	 (1 << A5XX_INT_UCHE_OOB_ACCESS) |		\
	 (1 << A5XX_INT_UCHE_TRAP_INTR) |		\
	 (1 << A5XX_INT_CP_SW) |			\
	 (1 << A5XX_INT_GPMU_FIRMWARE) |                \
	 (1 << A5XX_INT_GPMU_VOLTAGE_DROOP))

static int a5xx_probe(struct platform_device *pdev,
	u32 chipid, const struct adreno_gpu_core *gpucore)
{
	struct adreno_device *adreno_dev;
	struct kgsl_device *device;
	int ret;

	adreno_dev = (struct adreno_device *)
		of_device_get_match_data(&pdev->dev);

	memset(adreno_dev, 0, sizeof(*adreno_dev));

	adreno_dev->gpucore = gpucore;
	adreno_dev->chipid = chipid;

	adreno_reg_offset_init(gpucore->gpudev->reg_offsets);

	adreno_dev->sptp_pc_enabled =
		ADRENO_FEATURE(adreno_dev, ADRENO_SPTP_PC);

	if (adreno_is_a540(adreno_dev))
		adreno_dev->throttling_enabled = true;

	adreno_dev->hwcg_enabled = true;
	adreno_dev->lm_enabled =
		ADRENO_FEATURE(adreno_dev, ADRENO_LM);

	/* Setup defaults that might get changed by the fuse bits */
	adreno_dev->lm_leakage = 0x4e001a;

	device = KGSL_DEVICE(adreno_dev);

	timer_setup(&device->idle_timer, kgsl_timer, 0);

	INIT_WORK(&device->idle_check_ws, kgsl_idle_check);

	ret = adreno_device_probe(pdev, adreno_dev);
	if (ret)
		return ret;

	return adreno_dispatcher_init(adreno_dev);
}

static void _do_fixup(const struct adreno_critical_fixup *fixups, int count,
		uint64_t *gpuaddrs, unsigned int *buffer)
{
	int i;

	for (i = 0; i < count; i++) {
		buffer[fixups[i].lo_offset] =
			lower_32_bits(gpuaddrs[fixups[i].buffer]) |
			fixups[i].mem_offset;

		buffer[fixups[i].hi_offset] =
			upper_32_bits(gpuaddrs[fixups[i].buffer]);
	}
}

static int a5xx_critical_packet_construct(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int *cmds;
	uint64_t gpuaddrs[4];

	adreno_dev->critpkts = kgsl_allocate_global(device,
		PAGE_SIZE * 4, 0, 0, 0, "crit_pkts");
	if (IS_ERR(adreno_dev->critpkts))
		return PTR_ERR(adreno_dev->critpkts);

	adreno_dev->critpkts_secure = kgsl_allocate_global(device,
		PAGE_SIZE, 0, KGSL_MEMFLAGS_SECURE, 0, "crit_pkts_secure");
	if (IS_ERR(adreno_dev->critpkts_secure))
		return PTR_ERR(adreno_dev->critpkts_secure);

	cmds = adreno_dev->critpkts->hostptr;

	gpuaddrs[0] = adreno_dev->critpkts_secure->gpuaddr;
	gpuaddrs[1] = adreno_dev->critpkts->gpuaddr + PAGE_SIZE;
	gpuaddrs[2] = adreno_dev->critpkts->gpuaddr + (PAGE_SIZE * 2);
	gpuaddrs[3] = adreno_dev->critpkts->gpuaddr + (PAGE_SIZE * 3);

	crit_pkts_dwords = ARRAY_SIZE(_a5xx_critical_pkts);

	memcpy(cmds, _a5xx_critical_pkts, crit_pkts_dwords << 2);

	_do_fixup(critical_pkt_fixups, ARRAY_SIZE(critical_pkt_fixups),
		gpuaddrs, cmds);

	cmds = adreno_dev->critpkts->hostptr + PAGE_SIZE;
	memcpy(cmds, _a5xx_critical_pkts_mem01,
			ARRAY_SIZE(_a5xx_critical_pkts_mem01) << 2);

	cmds = adreno_dev->critpkts->hostptr + (PAGE_SIZE * 2);
	memcpy(cmds, _a5xx_critical_pkts_mem02,
			ARRAY_SIZE(_a5xx_critical_pkts_mem02) << 2);

	cmds = adreno_dev->critpkts->hostptr + (PAGE_SIZE * 3);
	memcpy(cmds, _a5xx_critical_pkts_mem03,
			ARRAY_SIZE(_a5xx_critical_pkts_mem03) << 2);

	_do_fixup(critical_pkt_mem03_fixups,
		ARRAY_SIZE(critical_pkt_mem03_fixups), gpuaddrs, cmds);

	critical_packet_constructed = 1;

	return 0;
}

static int a5xx_microcode_read(struct adreno_device *adreno_dev);

static int a5xx_init(struct adreno_device *adreno_dev)
{
	const struct adreno_a5xx_core *a5xx_core = to_a5xx_core(adreno_dev);
	int ret;

	ret = a5xx_ringbuffer_init(adreno_dev);
	if (ret)
		return ret;

	ret = a5xx_microcode_read(adreno_dev);
	if (ret)
		return ret;

	if (a5xx_has_gpmu(adreno_dev))
		INIT_WORK(&adreno_dev->gpmu_work, a5xx_gpmu_reset);

	adreno_dev->highest_bank_bit = a5xx_core->highest_bank_bit;

	INIT_WORK(&adreno_dev->irq_storm_work, a5xx_irq_storm_worker);

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_CRITICAL_PACKETS))
		a5xx_critical_packet_construct(adreno_dev);

	adreno_create_profile_buffer(adreno_dev);
	a5xx_crashdump_init(adreno_dev);

	return 0;
}

static const struct {
	u32 reg;
	u32 base;
	u32 count;
} a5xx_protected_blocks[] = {
	/* RBBM */
	{  A5XX_CP_PROTECT_REG_0,     0x004, 2 },
	{  A5XX_CP_PROTECT_REG_0 + 1, 0x008, 3 },
	{  A5XX_CP_PROTECT_REG_0 + 2, 0x010, 4 },
	{  A5XX_CP_PROTECT_REG_0 + 3, 0x020, 5 },
	{  A5XX_CP_PROTECT_REG_0 + 4, 0x040, 6 },
	{  A5XX_CP_PROTECT_REG_0 + 5, 0x080, 6 },
	/* Content protection */
	{  A5XX_CP_PROTECT_REG_0 + 6, A5XX_RBBM_SECVID_TSB_TRUSTED_BASE_LO, 4 },
	{  A5XX_CP_PROTECT_REG_0 + 7, A5XX_RBBM_SECVID_TRUST_CNTL, 1 },
	/* CP */
	{  A5XX_CP_PROTECT_REG_0 + 8, 0x800, 6 },
	{  A5XX_CP_PROTECT_REG_0 + 9, 0x840, 3 },
	{  A5XX_CP_PROTECT_REG_0 + 10, 0x880, 5 },
	{  A5XX_CP_PROTECT_REG_0 + 11, 0xaa0, 0 },
	/* RB */
	{  A5XX_CP_PROTECT_REG_0 + 12, 0xcc0, 0 },
	{  A5XX_CP_PROTECT_REG_0 + 13, 0xcf0, 1 },
	/* VPC */
	{  A5XX_CP_PROTECT_REG_0 + 14, 0xe68, 3 },
	{  A5XX_CP_PROTECT_REG_0 + 15, 0xe70, 4 },
	/* UCHE */
	{  A5XX_CP_PROTECT_REG_0 + 16, 0xe80, 4 },
	/* A5XX_CP_PROTECT_REG_17 will be used for SMMU */
	/* A5XX_CP_PROTECT_REG_18 - A5XX_CP_PROTECT_REG_31 are available */
};

static void _setprotectreg(struct kgsl_device *device, u32 offset,
		u32 base, u32 count)
{
	kgsl_regwrite(device, offset, 0x60000000 | (count << 24) | (base << 2));
}

static void a5xx_protect_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 reg;
	int i;

	/* enable access protection to privileged registers */
	kgsl_regwrite(device, A5XX_CP_PROTECT_CNTL, 0x00000007);

	for (i = 0; i < ARRAY_SIZE(a5xx_protected_blocks); i++) {
		reg = a5xx_protected_blocks[i].reg;

		_setprotectreg(device, reg, a5xx_protected_blocks[i].base,
			a5xx_protected_blocks[i].count);
	}

	/*
	 * For a530 and a540 the SMMU region is 0x20000 bytes long and 0x10000
	 * bytes on all other targets. The base offset for both is 0x40000.
	 * Write it to the next available slot
	 */
	if (adreno_is_a530(adreno_dev) || adreno_is_a540(adreno_dev))
		_setprotectreg(device, reg + 1, 0x40000, ilog2(0x20000));
	else
		_setprotectreg(device, reg + 1, 0x40000, ilog2(0x10000));
}

/*
 * _poll_gdsc_status() - Poll the GDSC status register
 * @adreno_dev: The adreno device pointer
 * @status_reg: Offset of the status register
 * @status_value: The expected bit value
 *
 * Poll the status register till the power-on bit is equal to the
 * expected value or the max retries are exceeded.
 */
static int _poll_gdsc_status(struct adreno_device *adreno_dev,
				unsigned int status_reg,
				unsigned int status_value)
{
	unsigned int reg, retry = PWR_RETRY;

	/* Bit 20 is the power on bit of SPTP and RAC GDSC status register */
	do {
		udelay(1);
		kgsl_regread(KGSL_DEVICE(adreno_dev), status_reg, &reg);
	} while (((reg & BIT(20)) != (status_value << 20)) && retry--);
	if ((reg & BIT(20)) != (status_value << 20))
		return -ETIMEDOUT;
	return 0;
}

static void a5xx_restore_isense_regs(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int reg, i, ramp = GPMU_ISENSE_SAVE;
	static unsigned int isense_regs[6] = {0xFFFF}, isense_reg_addr[] = {
		A5XX_GPU_CS_DECIMAL_ALIGN,
		A5XX_GPU_CS_SENSOR_PARAM_CORE_1,
		A5XX_GPU_CS_SENSOR_PARAM_CORE_2,
		A5XX_GPU_CS_SW_OV_FUSE_EN,
		A5XX_GPU_CS_ENDPOINT_CALIBRATION_DONE,
		A5XX_GPMU_TEMP_SENSOR_CONFIG};

	if (!adreno_is_a540(adreno_dev))
		return;

	/* read signature */
	kgsl_regread(device, ramp++, &reg);

	if (reg == 0xBABEFACE) {
		/* store memory locations in buffer */
		for (i = 0; i < ARRAY_SIZE(isense_regs); i++)
			kgsl_regread(device, ramp + i, isense_regs + i);

		/* clear signature */
		kgsl_regwrite(device, GPMU_ISENSE_SAVE, 0x0);
	}

	/* if we never stored memory locations - do nothing */
	if (isense_regs[0] == 0xFFFF)
		return;

	/* restore registers from memory */
	for (i = 0; i < ARRAY_SIZE(isense_reg_addr); i++)
		kgsl_regwrite(device, isense_reg_addr[i], isense_regs[i]);

}

/*
 * a5xx_regulator_enable() - Enable any necessary HW regulators
 * @adreno_dev: The adreno device pointer
 *
 * Some HW blocks may need their regulators explicitly enabled
 * on a restart.  Clocks must be on during this call.
 */
static int a5xx_regulator_enable(struct adreno_device *adreno_dev)
{
	unsigned int ret;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (test_bit(ADRENO_DEVICE_GPU_REGULATOR_ENABLED,
			&adreno_dev->priv))
		return 0;

	if (!(adreno_is_a530(adreno_dev) || adreno_is_a540(adreno_dev))) {
		/* Halt the sp_input_clk at HM level */
		kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL, 0x00000055);
		a5xx_hwcg_set(adreno_dev, true);
		/* Turn on sp_input_clk at HM level */
		kgsl_regrmw(device, A5XX_RBBM_CLOCK_CNTL, 0xFF, 0);

		set_bit(ADRENO_DEVICE_GPU_REGULATOR_ENABLED,
			&adreno_dev->priv);
		return 0;
	}

	/*
	 * Turn on smaller power domain first to reduce voltage droop.
	 * Set the default register values; set SW_COLLAPSE to 0.
	 */
	kgsl_regwrite(device, A5XX_GPMU_RBCCU_POWER_CNTL, 0x778000);
	/* Insert a delay between RAC and SPTP GDSC to reduce voltage droop */
	udelay(3);
	ret = _poll_gdsc_status(adreno_dev, A5XX_GPMU_RBCCU_PWR_CLK_STATUS, 1);
	if (ret) {
		dev_err(device->dev, "RBCCU GDSC enable failed\n");
		return ret;
	}

	kgsl_regwrite(device, A5XX_GPMU_SP_POWER_CNTL, 0x778000);
	ret = _poll_gdsc_status(adreno_dev, A5XX_GPMU_SP_PWR_CLK_STATUS, 1);
	if (ret) {
		dev_err(device->dev, "SPTP GDSC enable failed\n");
		return ret;
	}

	/* Disable SP clock */
	kgsl_regrmw(device, A5XX_GPMU_GPMU_SP_CLOCK_CONTROL,
		CNTL_IP_CLK_ENABLE, 0);
	/* Enable hardware clockgating */
	a5xx_hwcg_set(adreno_dev, true);
	/* Enable SP clock */
	kgsl_regrmw(device, A5XX_GPMU_GPMU_SP_CLOCK_CONTROL,
		CNTL_IP_CLK_ENABLE, 1);

	a5xx_restore_isense_regs(adreno_dev);

	set_bit(ADRENO_DEVICE_GPU_REGULATOR_ENABLED, &adreno_dev->priv);
	return 0;
}

/*
 * a5xx_regulator_disable() - Disable any necessary HW regulators
 * @adreno_dev: The adreno device pointer
 *
 * Some HW blocks may need their regulators explicitly disabled
 * on a power down to prevent current spikes.  Clocks must be on
 * during this call.
 */
static void a5xx_regulator_disable(struct adreno_device *adreno_dev)
{
	unsigned int reg;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (adreno_is_a512(adreno_dev) || adreno_is_a508(adreno_dev))
		return;

	if (!test_and_clear_bit(ADRENO_DEVICE_GPU_REGULATOR_ENABLED,
		&adreno_dev->priv))
		return;

	/* If feature is not supported or not enabled */
	if (!adreno_dev->sptp_pc_enabled) {
		/* Set the default register values; set SW_COLLAPSE to 1 */
		kgsl_regwrite(device, A5XX_GPMU_SP_POWER_CNTL, 0x778001);
		/*
		 * Insert a delay between SPTP and RAC GDSC to reduce voltage
		 * droop.
		 */
		udelay(3);
		if (_poll_gdsc_status(adreno_dev,
					A5XX_GPMU_SP_PWR_CLK_STATUS, 0))
			dev_warn(device->dev, "SPTP GDSC disable failed\n");

		kgsl_regwrite(device, A5XX_GPMU_RBCCU_POWER_CNTL, 0x778001);
		if (_poll_gdsc_status(adreno_dev,
					A5XX_GPMU_RBCCU_PWR_CLK_STATUS, 0))
			dev_warn(device->dev, "RBCCU GDSC disable failed\n");
	} else if (test_bit(ADRENO_DEVICE_GPMU_INITIALIZED,
			&adreno_dev->priv)) {
		/* GPMU firmware is supposed to turn off SPTP & RAC GDSCs. */
		kgsl_regread(device, A5XX_GPMU_SP_PWR_CLK_STATUS, &reg);
		if (reg & BIT(20))
			dev_warn(device->dev, "SPTP GDSC is not disabled\n");
		kgsl_regread(device, A5XX_GPMU_RBCCU_PWR_CLK_STATUS, &reg);
		if (reg & BIT(20))
			dev_warn(device->dev, "RBCCU GDSC is not disabled\n");
		/*
		 * GPMU firmware is supposed to set GMEM to non-retention.
		 * Bit 14 is the memory core force on bit.
		 */
		kgsl_regread(device, A5XX_GPMU_RBCCU_CLOCK_CNTL, &reg);
		if (reg & BIT(14))
			dev_warn(device->dev, "GMEM is forced on\n");
	}

	if (adreno_is_a530(adreno_dev)) {
		/* Reset VBIF before PC to avoid popping bogus FIFO entries */
		kgsl_regwrite(device, A5XX_RBBM_BLOCK_SW_RESET_CMD,
			0x003C0000);
		kgsl_regwrite(device, A5XX_RBBM_BLOCK_SW_RESET_CMD, 0);
	}
}

/*
 * a5xx_enable_pc() - Enable the GPMU based power collapse of the SPTP and RAC
 * blocks
 * @adreno_dev: The adreno device pointer
 */
static void a5xx_enable_pc(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!adreno_dev->sptp_pc_enabled)
		return;

	kgsl_regwrite(device, A5XX_GPMU_PWR_COL_INTER_FRAME_CTRL, 0x0000007F);
	kgsl_regwrite(device, A5XX_GPMU_PWR_COL_BINNING_CTRL, 0);
	kgsl_regwrite(device, A5XX_GPMU_PWR_COL_INTER_FRAME_HYST, 0x000A0080);
	kgsl_regwrite(device, A5XX_GPMU_PWR_COL_STAGGER_DELAY, 0x00600040);

	trace_adreno_sp_tp((unsigned long) __builtin_return_address(0));
};

/*
 * The maximum payload of a type4 packet is the max size minus one for the
 * opcode
 */
#define TYPE4_MAX_PAYLOAD (PM4_TYPE4_PKT_SIZE_MAX - 1)

static int _gpmu_create_load_cmds(struct adreno_device *adreno_dev,
	uint32_t *ucode, uint32_t size)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	uint32_t *start, *cmds;
	uint32_t offset = 0;
	uint32_t cmds_size = size;

	/* Add a dword for each PM4 packet */
	cmds_size += (size / TYPE4_MAX_PAYLOAD) + 1;

	/* Add 4 dwords for the protected mode */
	cmds_size += 4;

	if (adreno_dev->gpmu_cmds != NULL)
		return 0;

	adreno_dev->gpmu_cmds = devm_kmalloc(&device->pdev->dev,
		cmds_size << 2, GFP_KERNEL);
	if (adreno_dev->gpmu_cmds == NULL)
		return -ENOMEM;

	cmds = adreno_dev->gpmu_cmds;
	start = cmds;

	/* Turn CP protection OFF */
	cmds += cp_protected_mode(adreno_dev, cmds, 0);

	/*
	 * Prebuild the cmd stream to send to the GPU to load
	 * the GPMU firmware
	 */
	while (size > 0) {
		int tmp_size = size;

		if (size >= TYPE4_MAX_PAYLOAD)
			tmp_size = TYPE4_MAX_PAYLOAD;

		*cmds++ = cp_type4_packet(
				A5XX_GPMU_INST_RAM_BASE + offset,
				tmp_size);

		memcpy(cmds, &ucode[offset], tmp_size << 2);

		cmds += tmp_size;
		offset += tmp_size;
		size -= tmp_size;
	}

	/* Turn CP protection ON */
	cmds += cp_protected_mode(adreno_dev, cmds, 1);

	adreno_dev->gpmu_cmds_size = (size_t) (cmds - start);

	return 0;
}


/*
 * _load_gpmu_firmware() - Load the ucode into the GPMU RAM
 * @adreno_dev: Pointer to adreno device
 */
static int _load_gpmu_firmware(struct adreno_device *adreno_dev)
{
	uint32_t *data;
	const struct firmware *fw = NULL;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_a5xx_core *a5xx_core = to_a5xx_core(adreno_dev);
	uint32_t *cmds, cmd_size;
	int ret =  -EINVAL;
	u32 gmu_major = 1;

	if (!a5xx_has_gpmu(adreno_dev))
		return 0;

	/* a530 used GMU major 1 and A540 used GMU major 3 */
	if (adreno_is_a540(adreno_dev))
		gmu_major = 3;

	/* gpmu fw already saved and verified so do nothing new */
	if (adreno_dev->gpmu_cmds_size != 0)
		return 0;

	if (a5xx_core->gpmufw_name == NULL)
		return 0;

	ret = request_firmware(&fw, a5xx_core->gpmufw_name, &device->pdev->dev);
	if (ret || fw == NULL) {
		dev_err(&device->pdev->dev,
			"request_firmware (%s) failed: %d\n",
			a5xx_core->gpmufw_name, ret);
		return ret;
	}

	data = (uint32_t *)fw->data;

	if (data[0] >= (fw->size / sizeof(uint32_t)) || data[0] < 2)
		goto err;

	if (data[1] != GPMU_FIRMWARE_ID)
		goto err;
	ret = _read_fw2_block_header(device, &data[2],
		data[0] - 2, GPMU_FIRMWARE_ID, gmu_major, 0);
	if (ret)
		goto err;

	/* Integer overflow check for cmd_size */
	if (data[2] > (data[0] - 2))
		goto err;

	cmds = data + data[2] + 3;
	cmd_size = data[0] - data[2] - 2;

	if (cmd_size > GPMU_INST_RAM_SIZE) {
		dev_err(device->dev,
			"GPMU firmware block size is larger than RAM size\n");
		goto err;
	}

	/* Everything is cool, so create some commands */
	ret = _gpmu_create_load_cmds(adreno_dev, cmds, cmd_size);
err:
	if (fw)
		release_firmware(fw);

	return ret;
}

static void a5xx_spin_idle_debug(struct adreno_device *adreno_dev,
				const char *str)
{
	struct kgsl_device *device = &adreno_dev->dev;
	unsigned int rptr, wptr;
	unsigned int status, status3, intstatus;
	unsigned int hwfault;

	dev_err(device->dev, str);

	kgsl_regread(device, A5XX_CP_RB_RPTR, &rptr);
	kgsl_regread(device, A5XX_CP_RB_WPTR, &wptr);

	kgsl_regread(device, A5XX_RBBM_STATUS, &status);
	kgsl_regread(device, A5XX_RBBM_STATUS3, &status3);
	kgsl_regread(device, A5XX_RBBM_INT_0_STATUS, &intstatus);
	kgsl_regread(device, A5XX_CP_HW_FAULT, &hwfault);


	dev_err(device->dev,
		"rb=%d pos=%X/%X rbbm_status=%8.8X/%8.8X int_0_status=%8.8X\n",
		adreno_dev->cur_rb->id, rptr, wptr, status, status3, intstatus);

	dev_err(device->dev, " hwfault=%8.8X\n", hwfault);

	kgsl_device_snapshot(device, NULL, NULL, false);
}

static int _gpmu_send_init_cmds(struct adreno_device *adreno_dev)
{
	struct adreno_ringbuffer *rb = adreno_dev->cur_rb;
	uint32_t *cmds;
	uint32_t size = adreno_dev->gpmu_cmds_size;
	int ret;

	if (size == 0 || adreno_dev->gpmu_cmds == NULL)
		return -EINVAL;

	cmds = adreno_ringbuffer_allocspace(rb, size);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);
	if (cmds == NULL)
		return -ENOSPC;

	/* Copy to the RB the predefined fw sequence cmds */
	memcpy(cmds, adreno_dev->gpmu_cmds, size << 2);

	ret = a5xx_ringbuffer_submit(rb, NULL, true);
	if (!ret) {
		ret = adreno_spin_idle(adreno_dev, 2000);
		if (ret)
			a5xx_spin_idle_debug(adreno_dev,
				"gpmu initialization failed to idle\n");
	}
	return ret;
}

/*
 * a5xx_gpmu_start() - Initialize and start the GPMU
 * @adreno_dev: Pointer to adreno device
 *
 * Load the GPMU microcode, set up any features such as hardware clock gating
 * or IFPC, and take the GPMU out of reset.
 */
static int a5xx_gpmu_start(struct adreno_device *adreno_dev)
{
	int ret;
	unsigned int reg, retry = GPMU_FW_INIT_RETRY;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!a5xx_has_gpmu(adreno_dev))
		return 0;

	ret = _gpmu_send_init_cmds(adreno_dev);
	if (ret)
		return ret;

	if (adreno_is_a530(adreno_dev)) {
		/* GPMU clock gating setup */
		kgsl_regwrite(device, A5XX_GPMU_WFI_CONFIG, 0x00004014);
	}
	/* Kick off GPMU firmware */
	kgsl_regwrite(device, A5XX_GPMU_CM3_SYSRESET, 0);
	/*
	 * The hardware team's estimation of GPMU firmware initialization
	 * latency is about 3000 cycles, that's about 5 to 24 usec.
	 */
	do {
		udelay(1);
		kgsl_regread(device, A5XX_GPMU_GENERAL_0, &reg);
	} while ((reg != 0xBABEFACE) && retry--);

	if (reg != 0xBABEFACE) {
		dev_err(device->dev,
			"GPMU firmware initialization timed out\n");
		return -ETIMEDOUT;
	}

	if (!adreno_is_a530(adreno_dev)) {
		kgsl_regread(device, A5XX_GPMU_GENERAL_1, &reg);

		if (reg) {
			dev_err(device->dev,
				"GPMU firmware initialization failed: %d\n",
				reg);
			return -EIO;
		}
	}
	set_bit(ADRENO_DEVICE_GPMU_INITIALIZED, &adreno_dev->priv);
	/*
	 *  We are in AWARE state and IRQ line from GPU to host is
	 *  disabled.
	 *  Read pending GPMU interrupts and clear GPMU_RBBM_INTR_INFO.
	 */
	kgsl_regread(device, A5XX_GPMU_RBBM_INTR_INFO, &reg);
	/*
	 * Clear RBBM interrupt mask if any of GPMU interrupts
	 * are pending.
	 */
	if (reg)
		kgsl_regwrite(device,
			A5XX_RBBM_INT_CLEAR_CMD,
			1 << A5XX_INT_GPMU_FIRMWARE);
	return ret;
}

void a5xx_hwcg_set(struct adreno_device *adreno_dev, bool on)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_a5xx_core *a5xx_core = to_a5xx_core(adreno_dev);
	int i;

	if (!adreno_dev->hwcg_enabled)
		return;

	for (i = 0; i < a5xx_core->hwcg_count; i++)
		kgsl_regwrite(device, a5xx_core->hwcg[i].offset,
			on ? a5xx_core->hwcg[i].val : 0);

	/* enable top level HWCG */
	kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL, on ? 0xAAA8AA00 : 0);
	kgsl_regwrite(device, A5XX_RBBM_ISDB_CNT, on ? 0x00000182 : 0x00000180);
}

static int _read_fw2_block_header(struct kgsl_device *device,
		uint32_t *header, uint32_t remain,
		uint32_t id, uint32_t major, uint32_t minor)
{
	uint32_t header_size;
	int i = 1;

	if (header == NULL)
		return -ENOMEM;

	header_size = header[0];
	/* Headers have limited size and always occur as pairs of words */
	if (header_size >  MAX_HEADER_SIZE || header_size >= remain ||
				header_size % 2 || header_size == 0)
		return -EINVAL;
	/* Sequences must have an identifying id first thing in their header */
	if (id == GPMU_SEQUENCE_ID) {
		if (header[i] != HEADER_SEQUENCE ||
			(header[i + 1] >= MAX_SEQUENCE_ID))
			return -EINVAL;
		i += 2;
	}
	for (; i < header_size; i += 2) {
		switch (header[i]) {
		/* Major Version */
		case HEADER_MAJOR:
			if ((major > header[i + 1]) &&
				header[i + 1]) {
				dev_err(device->dev,
					"GPMU major version mis-match %d, %d\n",
					major, header[i + 1]);
				return -EINVAL;
			}
			break;
		case HEADER_MINOR:
			if (minor > header[i + 1])
				dev_err(device->dev,
					"GPMU minor version mis-match %d %d\n",
					minor, header[i + 1]);
			break;
		case HEADER_DATE:
		case HEADER_TIME:
			break;
		default:
			dev_err(device->dev, "GPMU unknown header ID %d\n",
					header[i]);
		}
	}
	return 0;
}

/*
 * Read in the register sequence file and save pointers to the
 * necessary sequences.
 *
 * GPU sequence file format (one dword per field unless noted):
 * Block 1 length (length dword field not inclusive)
 * Block 1 type = Sequence = 3
 * Block Header length (length dword field not inclusive)
 * BH field ID = Sequence field ID
 * BH field data = Sequence ID
 * BH field ID
 * BH field data
 * ...
 * Opcode 0 ID
 * Opcode 0 data M words
 * Opcode 1 ID
 * Opcode 1 data N words
 * ...
 * Opcode X ID
 * Opcode X data O words
 * Block 2 length...
 */
static void _load_regfile(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_a5xx_core *a5xx_core = to_a5xx_core(adreno_dev);
	const struct firmware *fw;
	uint64_t block_size = 0, block_total = 0;
	uint32_t fw_size, *block;
	int ret = -EINVAL;
	u32 lm_major = 1;

	if (!a5xx_core->regfw_name)
		return;

	ret = request_firmware(&fw, a5xx_core->regfw_name, &device->pdev->dev);
	if (ret) {
		dev_err(&device->pdev->dev, "request firmware failed %d, %s\n",
				ret, a5xx_core->regfw_name);
		return;
	}

	/* a530v2 lm_major was 3. a530v3 lm_major was 1 */
	if (adreno_is_a530v2(adreno_dev))
		lm_major = 3;

	fw_size = fw->size / sizeof(uint32_t);
	/* Min valid file of size 6, see file description */
	if (fw_size < 6)
		goto err;
	block = (uint32_t *)fw->data;
	/* All offset numbers calculated from file description */
	while (block_total < fw_size) {
		block_size = block[0];
		if (((block_total + block_size) >= fw_size)
				|| block_size < 5)
			goto err;
		if (block[1] != GPMU_SEQUENCE_ID)
			goto err;

		/* For now ignore blocks other than the LM sequence */
		if (block[4] == LM_SEQUENCE_ID) {
			ret = _read_fw2_block_header(device, &block[2],
				block_size - 2, GPMU_SEQUENCE_ID,
				lm_major, 0);
			if (ret)
				goto err;

			if (block[2] > (block_size - 2))
				goto err;
			adreno_dev->lm_sequence = block + block[2] + 3;
			adreno_dev->lm_size = block_size - block[2] - 2;
		}
		block_total += (block_size + 1);
		block += (block_size + 1);
	}
	if (adreno_dev->lm_sequence)
		return;

err:
	release_firmware(fw);
	dev_err(device->dev,
		     "Register file failed to load sz=%d bsz=%llu header=%d\n",
		     fw_size, block_size, ret);
}

static int _execute_reg_sequence(struct adreno_device *adreno_dev,
			uint32_t *opcode, uint32_t length)
{
	uint32_t *cur = opcode;
	uint64_t reg, val;

	/* todo double check the reg writes */
	while ((cur - opcode) < length) {
		if (cur[0] == 1 && (length - (cur - opcode) >= 4)) {
			/* Write a 32 bit value to a 64 bit reg */
			reg = cur[2];
			reg = (reg << 32) | cur[1];
			kgsl_regwrite(KGSL_DEVICE(adreno_dev), reg, cur[3]);
			cur += 4;
		} else if (cur[0] == 2 && (length - (cur - opcode) >= 5)) {
			/* Write a 64 bit value to a 64 bit reg */
			reg = cur[2];
			reg = (reg << 32) | cur[1];
			val = cur[4];
			val = (val << 32) | cur[3];
			kgsl_regwrite(KGSL_DEVICE(adreno_dev), reg, val);
			cur += 5;
		} else if (cur[0] == 3 && (length - (cur - opcode) >= 2)) {
			/* Delay for X usec */
			udelay(cur[1]);
			cur += 2;
		} else
			return -EINVAL;
	}
	return 0;
}

static uint32_t _write_voltage_table(struct adreno_device *adreno_dev,
			unsigned int addr)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	const struct adreno_a5xx_core *a5xx_core = to_a5xx_core(adreno_dev);
	int i;
	struct dev_pm_opp *opp;
	unsigned int mvolt = 0;

	kgsl_regwrite(device, addr++, a5xx_core->max_power);
	kgsl_regwrite(device, addr++, pwr->num_pwrlevels);

	/* Write voltage in mV and frequency in MHz */
	for (i = 0; i < pwr->num_pwrlevels; i++) {
		opp = dev_pm_opp_find_freq_exact(&device->pdev->dev,
				pwr->pwrlevels[i].gpu_freq, true);
		/* _opp_get returns uV, convert to mV */
		if (!IS_ERR(opp)) {
			mvolt = dev_pm_opp_get_voltage(opp) / 1000;
			dev_pm_opp_put(opp);
		}
		kgsl_regwrite(device, addr++, mvolt);
		kgsl_regwrite(device, addr++,
				pwr->pwrlevels[i].gpu_freq / 1000000);
	}
	return (pwr->num_pwrlevels * 2 + 2);
}

static uint32_t lm_limit(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (adreno_dev->lm_limit)
		return adreno_dev->lm_limit;

	if (of_property_read_u32(device->pdev->dev.of_node, "qcom,lm-limit",
		&adreno_dev->lm_limit))
		adreno_dev->lm_limit = LM_DEFAULT_LIMIT;

	return adreno_dev->lm_limit;
}
/*
 * a5xx_lm_init() - Initialize LM/DPM on the GPMU
 * @adreno_dev: The adreno device pointer
 */
static void a530_lm_init(struct adreno_device *adreno_dev)
{
	uint32_t length;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_a5xx_core *a5xx_core = to_a5xx_core(adreno_dev);

	if (!adreno_dev->lm_enabled)
		return;

	/* If something was wrong with the sequence file, return */
	if (adreno_dev->lm_sequence == NULL)
		return;

	/* Write LM registers including DPM ucode, coefficients, and config */
	if (_execute_reg_sequence(adreno_dev, adreno_dev->lm_sequence,
				adreno_dev->lm_size)) {
		/* If the sequence is invalid, it's not getting better */
		adreno_dev->lm_sequence = NULL;
		dev_warn(device->dev,
				"Invalid LM sequence\n");
		return;
	}

	kgsl_regwrite(device, A5XX_GPMU_TEMP_SENSOR_ID, a5xx_core->gpmu_tsens);
	kgsl_regwrite(device, A5XX_GPMU_DELTA_TEMP_THRESHOLD, 0x1);
	kgsl_regwrite(device, A5XX_GPMU_TEMP_SENSOR_CONFIG, 0x1);

	kgsl_regwrite(device, A5XX_GPMU_GPMU_VOLTAGE,
			(0x80000000 | device->pwrctrl.active_pwrlevel));
	/* use the leakage to set this value at runtime */
	kgsl_regwrite(device, A5XX_GPMU_BASE_LEAKAGE,
		adreno_dev->lm_leakage);

	/* Enable the power threshold and set it to 6000m */
	kgsl_regwrite(device, A5XX_GPMU_GPMU_PWR_THRESHOLD,
		0x80000000 | lm_limit(adreno_dev));

	kgsl_regwrite(device, A5XX_GPMU_BEC_ENABLE, 0x10001FFF);
	kgsl_regwrite(device, A5XX_GDPM_CONFIG1, 0x00201FF1);

	/* Send an initial message to the GPMU with the LM voltage table */
	kgsl_regwrite(device, AGC_MSG_STATE, 1);
	kgsl_regwrite(device, AGC_MSG_COMMAND, AGC_POWER_CONFIG_PRODUCTION_ID);
	length = _write_voltage_table(adreno_dev, AGC_MSG_PAYLOAD);
	kgsl_regwrite(device, AGC_MSG_PAYLOAD_SIZE, length * sizeof(uint32_t));
	kgsl_regwrite(device, AGC_INIT_MSG_MAGIC, AGC_INIT_MSG_VALUE);
}

/*
 * a5xx_lm_enable() - Enable the LM/DPM feature on the GPMU
 * @adreno_dev: The adreno device pointer
 */
static void a530_lm_enable(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!adreno_dev->lm_enabled)
		return;

	/* If no sequence properly initialized, return */
	if (adreno_dev->lm_sequence == NULL)
		return;

	kgsl_regwrite(device, A5XX_GDPM_INT_MASK, 0x00000000);
	kgsl_regwrite(device, A5XX_GDPM_INT_EN, 0x0000000A);
	kgsl_regwrite(device, A5XX_GPMU_GPMU_VOLTAGE_INTR_EN_MASK, 0x00000001);
	kgsl_regwrite(device, A5XX_GPMU_TEMP_THRESHOLD_INTR_EN_MASK,
			0x00050000);
	kgsl_regwrite(device, A5XX_GPMU_THROTTLE_UNMASK_FORCE_CTRL,
			0x00030000);

	if (adreno_is_a530(adreno_dev))
		/* Program throttle control, do not enable idle DCS on v3+ */
		kgsl_regwrite(device, A5XX_GPMU_CLOCK_THROTTLE_CTRL,
			adreno_is_a530v2(adreno_dev) ? 0x00060011 : 0x00000011);
}

static void a540_lm_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	uint32_t agc_lm_config = AGC_BCL_DISABLED |
		((ADRENO_CHIPID_PATCH(adreno_dev->chipid) & 0x3)
		<< AGC_GPU_VERSION_SHIFT);
	unsigned int r;

	if (!adreno_dev->throttling_enabled)
		agc_lm_config |= AGC_THROTTLE_DISABLE;

	if (adreno_dev->lm_enabled) {
		agc_lm_config |=
			AGC_LM_CONFIG_ENABLE_GPMU_ADAPTIVE |
			AGC_LM_CONFIG_ISENSE_ENABLE;

		kgsl_regread(device, A5XX_GPMU_TEMP_SENSOR_CONFIG, &r);

		if ((r & GPMU_ISENSE_STATUS) == GPMU_ISENSE_END_POINT_CAL_ERR) {
			dev_err(device->dev,
				"GPMU: ISENSE end point calibration failure\n");
			agc_lm_config |= AGC_LM_CONFIG_ENABLE_ERROR;
		}
	}

	kgsl_regwrite(device, AGC_MSG_STATE, 0x80000001);
	kgsl_regwrite(device, AGC_MSG_COMMAND, AGC_POWER_CONFIG_PRODUCTION_ID);
	(void) _write_voltage_table(adreno_dev, AGC_MSG_PAYLOAD);
	kgsl_regwrite(device, AGC_MSG_PAYLOAD + AGC_LM_CONFIG, agc_lm_config);
	kgsl_regwrite(device, AGC_MSG_PAYLOAD + AGC_LEVEL_CONFIG,
		(unsigned int) ~(GENMASK(LM_DCVS_LIMIT, 0) |
				GENMASK(16+LM_DCVS_LIMIT, 16)));

	kgsl_regwrite(device, AGC_MSG_PAYLOAD_SIZE,
		(AGC_LEVEL_CONFIG + 1) * sizeof(uint32_t));
	kgsl_regwrite(device, AGC_INIT_MSG_MAGIC, AGC_INIT_MSG_VALUE);

	kgsl_regwrite(device, A5XX_GPMU_GPMU_VOLTAGE,
		(0x80000000 | device->pwrctrl.active_pwrlevel));

	kgsl_regwrite(device, A5XX_GPMU_GPMU_PWR_THRESHOLD,
		PWR_THRESHOLD_VALID | lm_limit(adreno_dev));

	kgsl_regwrite(device, A5XX_GPMU_GPMU_VOLTAGE_INTR_EN_MASK,
		VOLTAGE_INTR_EN);
}


static void a5xx_lm_enable(struct adreno_device *adreno_dev)
{
	if (adreno_is_a530(adreno_dev))
		a530_lm_enable(adreno_dev);
}

static void a5xx_lm_init(struct adreno_device *adreno_dev)
{
	if (adreno_is_a530(adreno_dev))
		a530_lm_init(adreno_dev);
	else if (adreno_is_a540(adreno_dev))
		a540_lm_init(adreno_dev);
}

static int gpmu_set_level(struct adreno_device *adreno_dev, unsigned int val)
{
	unsigned int reg;
	int retry = 100;

	kgsl_regwrite(KGSL_DEVICE(adreno_dev), A5XX_GPMU_GPMU_VOLTAGE, val);

	do {
		kgsl_regread(KGSL_DEVICE(adreno_dev), A5XX_GPMU_GPMU_VOLTAGE,
			&reg);
	} while ((reg & 0x80000000) && retry--);

	return (reg & 0x80000000) ? -ETIMEDOUT : 0;
}

/*
 * a5xx_pwrlevel_change_settings() - Program the hardware during power level
 * transitions
 * @adreno_dev: The adreno device pointer
 * @prelevel: The previous power level
 * @postlevel: The new power level
 * @post: True if called after the clock change has taken effect
 */
static void a5xx_pwrlevel_change_settings(struct adreno_device *adreno_dev,
				unsigned int prelevel, unsigned int postlevel,
				bool post)
{
	/*
	 * On pre A540 HW only call through if LMx is supported and enabled, and
	 * always call through for a540
	 */
	if (!adreno_is_a540(adreno_dev) && !adreno_dev->lm_enabled)
		return;

	if (!post) {
		if (gpmu_set_level(adreno_dev, (0x80000010 | postlevel)))
			dev_err(KGSL_DEVICE(adreno_dev)->dev,
				"GPMU pre powerlevel did not stabilize\n");
	} else {
		if (gpmu_set_level(adreno_dev, (0x80000000 | postlevel)))
			dev_err(KGSL_DEVICE(adreno_dev)->dev,
				"GPMU post powerlevel did not stabilize\n");
	}
}

#if IS_ENABLED(CONFIG_COMMON_CLK_QCOM)
static void a5xx_clk_set_options(struct adreno_device *adreno_dev,
	const char *name, struct clk *clk, bool on)
{
	if (!clk)
		return;

	if (!adreno_is_a540(adreno_dev) && !adreno_is_a512(adreno_dev) &&
		!adreno_is_a508(adreno_dev))
		return;

	/* Handle clock settings for GFX PSCBCs */
	if (on) {
		if (!strcmp(name, "mem_iface_clk")) {
			qcom_clk_set_flags(clk, CLKFLAG_NORETAIN_PERIPH);
			qcom_clk_set_flags(clk, CLKFLAG_NORETAIN_MEM);
		} else if (!strcmp(name, "core_clk")) {
			qcom_clk_set_flags(clk, CLKFLAG_RETAIN_PERIPH);
			qcom_clk_set_flags(clk, CLKFLAG_RETAIN_MEM);
		}
	} else {
		if (!strcmp(name, "core_clk")) {
			qcom_clk_set_flags(clk, CLKFLAG_NORETAIN_PERIPH);
			qcom_clk_set_flags(clk, CLKFLAG_NORETAIN_MEM);
		}
	}
}
#endif

/* FW driven idle 10% throttle */
#define IDLE_10PCT 0
/* number of cycles when clock is throttled by 50% (CRC) */
#define CRC_50PCT  1
/* number of cycles when clock is throttled by more than 50% (CRC) */
#define CRC_MORE50PCT 2
/* number of cycles when clock is throttle by less than 50% (CRC) */
#define CRC_LESS50PCT 3

static int64_t a5xx_read_throttling_counters(struct adreno_device *adreno_dev)
{
	int i;
	int64_t adj;
	uint32_t th[ADRENO_GPMU_THROTTLE_COUNTERS];
	struct adreno_busy_data *busy = &adreno_dev->busy_data;

	if (!adreno_dev->throttling_enabled)
		return 0;

	for (i = 0; i < ADRENO_GPMU_THROTTLE_COUNTERS; i++) {
		if (!adreno_dev->gpmu_throttle_counters[i])
			return 0;

		th[i] = counter_delta(KGSL_DEVICE(adreno_dev),
				adreno_dev->gpmu_throttle_counters[i],
				&busy->throttle_cycles[i]);
	}
	adj = th[CRC_MORE50PCT] - th[IDLE_10PCT];
	adj = th[CRC_50PCT] + th[CRC_LESS50PCT] / 3 + (adj < 0 ? 0 : adj) * 3;

	trace_kgsl_clock_throttling(
		th[IDLE_10PCT], th[CRC_50PCT],
		th[CRC_MORE50PCT], th[CRC_LESS50PCT],
		adj);
	return adj;
}

/*
 * a5xx_gpmu_reset() - Re-enable GPMU based power features and restart GPMU
 * @work: Pointer to the work struct for gpmu reset
 *
 * Load the GPMU microcode, set up any features such as hardware clock gating
 * or IFPC, and take the GPMU out of reset.
 */
static void a5xx_gpmu_reset(struct work_struct *work)
{
	struct adreno_device *adreno_dev = container_of(work,
			struct adreno_device, gpmu_work);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (test_bit(ADRENO_DEVICE_GPMU_INITIALIZED, &adreno_dev->priv))
		return;

	/*
	 * If GPMU has already experienced a restart or is in the process of it
	 * after the watchdog timeout, then there is no need to reset GPMU
	 * again.
	 */
	if (device->state != KGSL_STATE_NAP &&
		device->state != KGSL_STATE_AWARE &&
		device->state != KGSL_STATE_ACTIVE)
		return;

	mutex_lock(&device->mutex);

	if (device->state == KGSL_STATE_NAP)
		kgsl_pwrctrl_change_state(device, KGSL_STATE_AWARE);

	if (a5xx_regulator_enable(adreno_dev))
		goto out;

	/* Soft reset of the GPMU block */
	kgsl_regwrite(device, A5XX_RBBM_BLOCK_SW_RESET_CMD, BIT(16));

	/* GPU comes up in secured mode, make it unsecured by default */
	if (!ADRENO_FEATURE(adreno_dev, ADRENO_CONTENT_PROTECTION))
		kgsl_regwrite(device, A5XX_RBBM_SECVID_TRUST_CNTL, 0x0);


	a5xx_gpmu_init(adreno_dev);

out:
	mutex_unlock(&device->mutex);
}

static void _setup_throttling_counters(struct adreno_device *adreno_dev)
{
	int i, ret = 0;

	if (!adreno_is_a540(adreno_dev))
		return;

	for (i = 0; i < ADRENO_GPMU_THROTTLE_COUNTERS; i++) {
		/* reset throttled cycles ivalue */
		adreno_dev->busy_data.throttle_cycles[i] = 0;

		/* Throttle countables start at off set 43 */
		ret |= adreno_perfcounter_kernel_get(adreno_dev,
			KGSL_PERFCOUNTER_GROUP_GPMU_PWR, 43 + i,
			&adreno_dev->gpmu_throttle_counters[i], NULL);
	}

	WARN_ONCE(ret, "Unable to get one or more clock throttling registers\n");
}

/*
 * a5xx_start() - Device start
 * @adreno_dev: Pointer to adreno device
 *
 * a5xx device start
 */
static int a5xx_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_a5xx_core *a5xx_core = to_a5xx_core(adreno_dev);
	unsigned int bit;
	int ret;

	ret = kgsl_mmu_start(device);
	if (ret)
		return ret;

	adreno_get_bus_counters(adreno_dev);
	adreno_perfcounter_restore(adreno_dev);

	adreno_dev->irq_mask = A5XX_INT_MASK;

	if (adreno_is_a530(adreno_dev) &&
			ADRENO_FEATURE(adreno_dev, ADRENO_LM))
		adreno_perfcounter_kernel_get(adreno_dev,
			KGSL_PERFCOUNTER_GROUP_GPMU_PWR, 27,
			&adreno_dev->lm_threshold_count, NULL);

	/* Enable 64 bit addressing */
	kgsl_regwrite(device, A5XX_CP_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A5XX_VSC_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A5XX_GRAS_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A5XX_RB_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A5XX_PC_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A5XX_HLSQ_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A5XX_VFD_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A5XX_VPC_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A5XX_UCHE_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A5XX_SP_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A5XX_TPL1_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A5XX_RBBM_SECVID_TSB_ADDR_MODE_CNTL, 0x1);

	_setup_throttling_counters(adreno_dev);

	/* Set up VBIF registers from the GPU core definition */
	kgsl_regmap_multi_write(&device->regmap, a5xx_core->vbif,
		a5xx_core->vbif_count);

	/* Make all blocks contribute to the GPU BUSY perf counter */
	kgsl_regwrite(device, A5XX_RBBM_PERFCTR_GPU_BUSY_MASKED, 0xFFFFFFFF);

	/* Program RBBM counter 0 to report GPU busy for frequency scaling */
	kgsl_regwrite(device, A5XX_RBBM_PERFCTR_RBBM_SEL_0, 6);

	/*
	 * Enable the RBBM error reporting bits.  This lets us get
	 * useful information on failure
	 */
	kgsl_regwrite(device, A5XX_RBBM_AHB_CNTL0, 0x00000001);

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_FAULT_DETECT_MASK)) {
		/*
		 * We have 4 RB units, and only RB0 activity signals are
		 * working correctly. Mask out RB1-3 activity signals
		 * from the HW hang detection logic as per
		 * recommendation of hardware team.
		 */
		kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_MASK_CNTL11,
				0xF0000000);
		kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_MASK_CNTL12,
				0xFFFFFFFF);
		kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_MASK_CNTL13,
				0xFFFFFFFF);
		kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_MASK_CNTL14,
				0xFFFFFFFF);
		kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_MASK_CNTL15,
				0xFFFFFFFF);
		kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_MASK_CNTL16,
				0xFFFFFFFF);
		kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_MASK_CNTL17,
				0xFFFFFFFF);
		kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_MASK_CNTL18,
				0xFFFFFFFF);
	}

	/*
	 * Set hang detection threshold to 4 million cycles
	 * (0x3FFFF*16)
	 */
	kgsl_regwrite(device, A5XX_RBBM_INTERFACE_HANG_INT_CNTL,
				  (1 << 30) | 0x3FFFF);

	/* Turn on performance counters */
	kgsl_regwrite(device, A5XX_RBBM_PERFCTR_CNTL, 0x01);

	/*
	 * This is to increase performance by restricting VFD's cache access,
	 * so that LRZ and other data get evicted less.
	 */
	kgsl_regwrite(device, A5XX_UCHE_CACHE_WAYS, 0x02);

	/*
	 * Set UCHE_WRITE_THRU_BASE to the UCHE_TRAP_BASE effectively
	 * disabling L2 bypass
	 */
	kgsl_regwrite(device, A5XX_UCHE_TRAP_BASE_LO, 0xffff0000);
	kgsl_regwrite(device, A5XX_UCHE_TRAP_BASE_HI, 0x0001ffff);
	kgsl_regwrite(device, A5XX_UCHE_WRITE_THRU_BASE_LO, 0xffff0000);
	kgsl_regwrite(device, A5XX_UCHE_WRITE_THRU_BASE_HI, 0x0001ffff);

	/* Program the GMEM VA range for the UCHE path */
	kgsl_regwrite(device, A5XX_UCHE_GMEM_RANGE_MIN_LO,
			adreno_dev->uche_gmem_base);
	kgsl_regwrite(device, A5XX_UCHE_GMEM_RANGE_MIN_HI, 0x0);
	kgsl_regwrite(device, A5XX_UCHE_GMEM_RANGE_MAX_LO,
			adreno_dev->uche_gmem_base +
			adreno_dev->gpucore->gmem_size - 1);
	kgsl_regwrite(device, A5XX_UCHE_GMEM_RANGE_MAX_HI, 0x0);

	/*
	 * Below CP registers are 0x0 by default, program init
	 * values based on a5xx flavor.
	 */
	if (adreno_is_a505_or_a506(adreno_dev) || adreno_is_a508(adreno_dev)) {
		kgsl_regwrite(device, A5XX_CP_MEQ_THRESHOLDS, 0x20);
		kgsl_regwrite(device, A5XX_CP_MERCIU_SIZE, 0x400);
		kgsl_regwrite(device, A5XX_CP_ROQ_THRESHOLDS_2, 0x40000030);
		kgsl_regwrite(device, A5XX_CP_ROQ_THRESHOLDS_1, 0x20100D0A);
	} else if (adreno_is_a510(adreno_dev)) {
		kgsl_regwrite(device, A5XX_CP_MEQ_THRESHOLDS, 0x20);
		kgsl_regwrite(device, A5XX_CP_MERCIU_SIZE, 0x20);
		kgsl_regwrite(device, A5XX_CP_ROQ_THRESHOLDS_2, 0x40000030);
		kgsl_regwrite(device, A5XX_CP_ROQ_THRESHOLDS_1, 0x20100D0A);
	} else if (adreno_is_a540(adreno_dev) || adreno_is_a512(adreno_dev)) {
		kgsl_regwrite(device, A5XX_CP_MEQ_THRESHOLDS, 0x40);
		kgsl_regwrite(device, A5XX_CP_MERCIU_SIZE, 0x400);
		kgsl_regwrite(device, A5XX_CP_ROQ_THRESHOLDS_2, 0x80000060);
		kgsl_regwrite(device, A5XX_CP_ROQ_THRESHOLDS_1, 0x40201B16);
	} else {
		kgsl_regwrite(device, A5XX_CP_MEQ_THRESHOLDS, 0x40);
		kgsl_regwrite(device, A5XX_CP_MERCIU_SIZE, 0x40);
		kgsl_regwrite(device, A5XX_CP_ROQ_THRESHOLDS_2, 0x80000060);
		kgsl_regwrite(device, A5XX_CP_ROQ_THRESHOLDS_1, 0x40201B16);
	}

	/*
	 * vtxFifo and primFifo thresholds default values
	 * are different.
	 */
	if (adreno_is_a505_or_a506(adreno_dev) || adreno_is_a508(adreno_dev))
		kgsl_regwrite(device, A5XX_PC_DBG_ECO_CNTL,
						(0x100 << 11 | 0x100 << 22));
	else if (adreno_is_a510(adreno_dev) || adreno_is_a512(adreno_dev))
		kgsl_regwrite(device, A5XX_PC_DBG_ECO_CNTL,
						(0x200 << 11 | 0x200 << 22));
	else
		kgsl_regwrite(device, A5XX_PC_DBG_ECO_CNTL,
						(0x400 << 11 | 0x300 << 22));

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_TWO_PASS_USE_WFI)) {
		/*
		 * Set TWOPASSUSEWFI in A5XX_PC_DBG_ECO_CNTL for
		 * microcodes after v77
		 */
		if ((adreno_compare_pfp_version(adreno_dev, 0x5FF077) >= 0))
			kgsl_regrmw(device, A5XX_PC_DBG_ECO_CNTL, 0, (1 << 8));
	}

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_DISABLE_RB_DP2CLOCKGATING)) {
		/*
		 * Disable RB sampler datapath DP2 clock gating
		 * optimization for 1-SP GPU's, by default it is enabled.
		 */
		kgsl_regrmw(device, A5XX_RB_DBG_ECO_CNT, 0, (1 << 9));
	}
	/*
	 * Disable UCHE global filter as SP can invalidate/flush
	 * independently
	 */
	kgsl_regwrite(device, A5XX_UCHE_MODE_CNTL, BIT(29));
	/* Set the USE_RETENTION_FLOPS chicken bit */
	kgsl_regwrite(device, A5XX_CP_CHICKEN_DBG, 0x02000000);

	/* Enable ISDB mode if requested */
	if (test_bit(ADRENO_DEVICE_ISDB_ENABLED, &adreno_dev->priv)) {
		if (!adreno_active_count_get(adreno_dev)) {
			/*
			 * Disable ME/PFP split timeouts when the debugger is
			 * enabled because the CP doesn't know when a shader is
			 * in active debug
			 */
			kgsl_regwrite(device, A5XX_RBBM_AHB_CNTL1, 0x06FFFFFF);

			/* Force the SP0/SP1 clocks on to enable ISDB */
			kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL_SP0, 0x0);
			kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL_SP1, 0x0);
			kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL_SP2, 0x0);
			kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL_SP3, 0x0);
			kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL2_SP0, 0x0);
			kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL2_SP1, 0x0);
			kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL2_SP2, 0x0);
			kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL2_SP3, 0x0);

			/* disable HWCG */
			kgsl_regwrite(device, A5XX_RBBM_CLOCK_CNTL, 0x0);
			kgsl_regwrite(device, A5XX_RBBM_ISDB_CNT, 0x0);
		} else
			dev_err(device->dev,
				"Active count failed while turning on ISDB\n");
	} else {
		/* if not in ISDB mode enable ME/PFP split notification */
		kgsl_regwrite(device, A5XX_RBBM_AHB_CNTL1, 0xA6FFFFFF);
	}

	kgsl_regwrite(device, A5XX_RBBM_AHB_CNTL2, 0x0000003F);
	bit = adreno_dev->highest_bank_bit ?
		(adreno_dev->highest_bank_bit - 13) & 0x03 : 0;
	/*
	 * Program the highest DDR bank bit that was passed in
	 * from the DT in a handful of registers. Some of these
	 * registers will also be written by the UMD, but we
	 * want to program them in case we happen to use the
	 * UCHE before the UMD does
	 */

	kgsl_regwrite(device, A5XX_TPL1_MODE_CNTL, bit << 7);
	kgsl_regwrite(device, A5XX_RB_MODE_CNTL, bit << 1);
	if (adreno_is_a540(adreno_dev) || adreno_is_a512(adreno_dev))
		kgsl_regwrite(device, A5XX_UCHE_DBG_ECO_CNTL_2, bit);

	/* Disable All flat shading optimization */
	kgsl_regrmw(device, A5XX_VPC_DBG_ECO_CNTL, 0, 0x1 << 10);

	/*
	 * VPC corner case with local memory load kill leads to corrupt
	 * internal state. Normal Disable does not work for all a5x chips.
	 * So do the following setting to disable it.
	 */
	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_DISABLE_LMLOADKILL)) {
		kgsl_regrmw(device, A5XX_VPC_DBG_ECO_CNTL, 0, 0x1 << 23);
		kgsl_regrmw(device, A5XX_HLSQ_DBG_ECO_CNTL, 0x1 << 18, 0);
	}

	if (device->mmu.secured) {
		kgsl_regwrite(device, A5XX_RBBM_SECVID_TSB_CNTL, 0x0);
		kgsl_regwrite(device, A5XX_RBBM_SECVID_TSB_TRUSTED_BASE_LO,
			lower_32_bits(KGSL_IOMMU_SECURE_BASE32));
		kgsl_regwrite(device, A5XX_RBBM_SECVID_TSB_TRUSTED_BASE_HI,
			upper_32_bits(KGSL_IOMMU_SECURE_BASE32));
		kgsl_regwrite(device, A5XX_RBBM_SECVID_TSB_TRUSTED_SIZE,
			FIELD_PREP(GENMASK(31, 12),
			(KGSL_IOMMU_SECURE_SIZE(&device->mmu) / SZ_4K)));
	}

	a5xx_preemption_start(adreno_dev);
	a5xx_protect_init(adreno_dev);

	return 0;
}

/*
 * Follow the ME_INIT sequence with a preemption yield to allow the GPU to move
 * to a different ringbuffer, if desired
 */
static int _preemption_init(
			struct adreno_device *adreno_dev,
			struct adreno_ringbuffer *rb, unsigned int *cmds,
			struct kgsl_context *context)
{
	unsigned int *cmds_orig = cmds;
	uint64_t gpuaddr = rb->preemption_desc->gpuaddr;

	/* Turn CP protection OFF */
	cmds += cp_protected_mode(adreno_dev, cmds, 0);
	/*
	 * CP during context switch will save context switch info to
	 * a5xx_cp_preemption_record pointed by CONTEXT_SWITCH_SAVE_ADDR
	 */
	*cmds++ = cp_type4_packet(A5XX_CP_CONTEXT_SWITCH_SAVE_ADDR_LO, 1);
	*cmds++ = lower_32_bits(gpuaddr);
	*cmds++ = cp_type4_packet(A5XX_CP_CONTEXT_SWITCH_SAVE_ADDR_HI, 1);
	*cmds++ = upper_32_bits(gpuaddr);

	/* Turn CP protection ON */
	cmds += cp_protected_mode(adreno_dev, cmds, 1);

	*cmds++ = cp_type7_packet(CP_PREEMPT_ENABLE_GLOBAL, 1);
	*cmds++ = 0;

	*cmds++ = cp_type7_packet(CP_PREEMPT_ENABLE_LOCAL, 1);
	*cmds++ = 1;

	/* Enable yield in RB only */
	*cmds++ = cp_type7_packet(CP_YIELD_ENABLE, 1);
	*cmds++ = 1;

	*cmds++ = cp_type7_packet(CP_CONTEXT_SWITCH_YIELD, 4);
	cmds += cp_gpuaddr(adreno_dev, cmds, 0x0);
	*cmds++ = 0;
	/* generate interrupt on preemption completion */
	*cmds++ = 1;

	return cmds - cmds_orig;
}

static int a5xx_post_start(struct adreno_device *adreno_dev)
{
	int ret;
	unsigned int *cmds, *start;
	struct adreno_ringbuffer *rb = adreno_dev->cur_rb;

	if (!adreno_is_a530(adreno_dev) &&
		!adreno_is_preemption_enabled(adreno_dev))
		return 0;

	cmds = adreno_ringbuffer_allocspace(rb, 42);
	if (IS_ERR(cmds)) {
		struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

		dev_err(device->dev,
			     "error allocating preemtion init cmds\n");
		return PTR_ERR(cmds);
	}
	start = cmds;

	/*
	 * Send a pipeline stat event whenever the GPU gets powered up
	 * to cause misbehaving perf counters to start ticking
	 */
	if (adreno_is_a530(adreno_dev)) {
		*cmds++ = cp_packet(adreno_dev, CP_EVENT_WRITE, 1);
		*cmds++ = 0xF;
	}

	if (adreno_is_preemption_enabled(adreno_dev)) {
		cmds += _preemption_init(adreno_dev, rb, cmds, NULL);
		rb->_wptr = rb->_wptr - (42 - (cmds - start));
		ret = a5xx_ringbuffer_submit(rb, NULL, false);
	} else {
		rb->_wptr = rb->_wptr - (42 - (cmds - start));
		ret = a5xx_ringbuffer_submit(rb, NULL, true);
	}

	if (!ret) {
		ret = adreno_spin_idle(adreno_dev, 2000);
		if (ret)
			a5xx_spin_idle_debug(adreno_dev,
				"hw initialization failed to idle\n");
	}

	return ret;
}

static int a5xx_gpmu_init(struct adreno_device *adreno_dev)
{
	int ret;

	/* Set up LM before initializing the GPMU */
	a5xx_lm_init(adreno_dev);

	/* Enable SPTP based power collapse before enabling GPMU */
	a5xx_enable_pc(adreno_dev);

	ret = a5xx_gpmu_start(adreno_dev);
	if (ret)
		return ret;

	/* Enable limits management */
	a5xx_lm_enable(adreno_dev);
	return 0;
}

static int a5xx_zap_shader_resume(struct kgsl_device *device)
{
	int ret = qcom_scm_set_remote_state(0, 13);

	if (ret)
		dev_err(device->dev,
			"SCM zap resume call failed: %d\n", ret);

	return ret;
}

/*
 * a5xx_microcode_load() - Load microcode
 * @adreno_dev: Pointer to adreno device
 */
static int a5xx_microcode_load(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_firmware *pm4_fw = ADRENO_FW(adreno_dev, ADRENO_FW_PM4);
	struct adreno_firmware *pfp_fw = ADRENO_FW(adreno_dev, ADRENO_FW_PFP);
	const struct adreno_a5xx_core *a5xx_core = to_a5xx_core(adreno_dev);
	uint64_t gpuaddr;

	gpuaddr = pm4_fw->memdesc->gpuaddr;
	kgsl_regwrite(device, A5XX_CP_PM4_INSTR_BASE_LO,
				lower_32_bits(gpuaddr));
	kgsl_regwrite(device, A5XX_CP_PM4_INSTR_BASE_HI,
				upper_32_bits(gpuaddr));

	gpuaddr = pfp_fw->memdesc->gpuaddr;
	kgsl_regwrite(device, A5XX_CP_PFP_INSTR_BASE_LO,
				lower_32_bits(gpuaddr));
	kgsl_regwrite(device, A5XX_CP_PFP_INSTR_BASE_HI,
				upper_32_bits(gpuaddr));

	/*
	 * Do not invoke to load zap shader if MMU does
	 * not support secure mode.
	 */
	if (!device->mmu.secured)
		return 0;

	if (adreno_dev->zap_loaded && !(ADRENO_FEATURE(adreno_dev,
		ADRENO_CPZ_RETENTION)))
		return a5xx_zap_shader_resume(device);

	return adreno_zap_shader_load(adreno_dev, a5xx_core->zap_name);
}

static int _me_init_ucode_workarounds(struct adreno_device *adreno_dev)
{
	switch (ADRENO_GPUREV(adreno_dev)) {
	case ADRENO_REV_A510:
		return 0x00000001; /* Ucode workaround for token end syncs */
	case ADRENO_REV_A505:
	case ADRENO_REV_A506:
	case ADRENO_REV_A530:
		/*
		 * Ucode workarounds for token end syncs,
		 * WFI after every direct-render 3D mode draw and
		 * WFI after every 2D Mode 3 draw.
		 */
		return 0x0000000B;
	default:
		return 0x00000000; /* No ucode workarounds enabled */
	}
}

/*
 * CP_INIT_MAX_CONTEXT bit tells if the multiple hardware contexts can
 * be used at once of if they should be serialized
 */
#define CP_INIT_MAX_CONTEXT BIT(0)

/* Enables register protection mode */
#define CP_INIT_ERROR_DETECTION_CONTROL BIT(1)

/* Header dump information */
#define CP_INIT_HEADER_DUMP BIT(2) /* Reserved */

/* Default Reset states enabled for PFP and ME */
#define CP_INIT_DEFAULT_RESET_STATE BIT(3)

/* Drawcall filter range */
#define CP_INIT_DRAWCALL_FILTER_RANGE BIT(4)

/* Ucode workaround masks */
#define CP_INIT_UCODE_WORKAROUND_MASK BIT(5)

#define CP_INIT_MASK (CP_INIT_MAX_CONTEXT | \
		CP_INIT_ERROR_DETECTION_CONTROL | \
		CP_INIT_HEADER_DUMP | \
		CP_INIT_DEFAULT_RESET_STATE | \
		CP_INIT_UCODE_WORKAROUND_MASK)

static int a5xx_critical_packet_submit(struct adreno_device *adreno_dev,
					struct adreno_ringbuffer *rb)
{
	unsigned int *cmds;
	int ret;

	if (!critical_packet_constructed)
		return 0;

	cmds = adreno_ringbuffer_allocspace(rb, 4);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);

	*cmds++ = cp_mem_packet(adreno_dev, CP_INDIRECT_BUFFER_PFE, 2, 1);
	cmds += cp_gpuaddr(adreno_dev, cmds, adreno_dev->critpkts->gpuaddr);
	*cmds++ = crit_pkts_dwords;

	ret = a5xx_ringbuffer_submit(rb, NULL, true);
	if (!ret) {
		ret = adreno_spin_idle(adreno_dev, 20);
		if (ret)
			a5xx_spin_idle_debug(adreno_dev,
				"Critical packet submission failed to idle\n");
	}

	return ret;
}

/*
 * a5xx_send_me_init() - Initialize ringbuffer
 * @adreno_dev: Pointer to adreno device
 * @rb: Pointer to the ringbuffer of device
 *
 * Submit commands for ME initialization,
 */
static int a5xx_send_me_init(struct adreno_device *adreno_dev,
			 struct adreno_ringbuffer *rb)
{
	unsigned int *cmds;
	int i = 0, ret;

	cmds = adreno_ringbuffer_allocspace(rb, 9);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);

	cmds[i++] = cp_type7_packet(CP_ME_INIT, 8);

	/* Enabled ordinal mask */
	cmds[i++] = CP_INIT_MASK;

	if (CP_INIT_MASK & CP_INIT_MAX_CONTEXT)
		cmds[i++] = 0x00000003;

	if (CP_INIT_MASK & CP_INIT_ERROR_DETECTION_CONTROL)
		cmds[i++] = 0x20000000;

	if (CP_INIT_MASK & CP_INIT_HEADER_DUMP) {
		/* Header dump address */
		cmds[i++] = 0x00000000;
		/* Header dump enable and dump size */
		cmds[i++] = 0x00000000;
	}

	if (CP_INIT_MASK & CP_INIT_DRAWCALL_FILTER_RANGE) {
		/* Start range */
		cmds[i++] = 0x00000000;
		/* End range (inclusive) */
		cmds[i++] = 0x00000000;
	}

	if (CP_INIT_MASK & CP_INIT_UCODE_WORKAROUND_MASK)
		cmds[i++] = _me_init_ucode_workarounds(adreno_dev);

	ret = a5xx_ringbuffer_submit(rb, NULL, true);
	if (!ret) {
		ret = adreno_spin_idle(adreno_dev, 2000);
		if (ret)
			a5xx_spin_idle_debug(adreno_dev,
				"CP initialization failed to idle\n");
	}

	return ret;
}

/*
 * a5xx_rb_start() - Start the ringbuffer
 * @adreno_dev: Pointer to adreno device
 */
static int a5xx_rb_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_ringbuffer *rb;
	uint64_t addr;
	unsigned int *cmds;
	int ret, i;

	/* Clear all the ringbuffers */
	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		memset(rb->buffer_desc->hostptr, 0xaa, KGSL_RB_SIZE);
		kgsl_sharedmem_writel(device->scratch,
			SCRATCH_RB_OFFSET(rb->id, rptr), 0);

		rb->wptr = 0;
		rb->_wptr = 0;
		rb->wptr_preempt_end = ~0;
	}

	/* Set up the current ringbuffer */
	rb = ADRENO_CURRENT_RINGBUFFER(adreno_dev);
	addr = SCRATCH_RB_GPU_ADDR(device, rb->id, rptr);

	kgsl_regwrite(device, A5XX_CP_RB_RPTR_ADDR_LO, lower_32_bits(addr));
	kgsl_regwrite(device, A5XX_CP_RB_RPTR_ADDR_HI, upper_32_bits(addr));

	/*
	 * The size of the ringbuffer in the hardware is the log2
	 * representation of the size in quadwords (sizedwords / 2).
	 * Also disable the host RPTR shadow register as it might be unreliable
	 * in certain circumstances.
	 */

	kgsl_regwrite(device, A5XX_CP_RB_CNTL,
		A5XX_CP_RB_CNTL_DEFAULT);

	kgsl_regwrite(device, A5XX_CP_RB_BASE,
		lower_32_bits(rb->buffer_desc->gpuaddr));
	kgsl_regwrite(device, A5XX_CP_RB_BASE_HI,
		upper_32_bits(rb->buffer_desc->gpuaddr));

	ret = a5xx_microcode_load(adreno_dev);
	if (ret)
		return ret;

	/* clear ME_HALT to start micro engine */

	kgsl_regwrite(device, A5XX_CP_ME_CNTL, 0);

	ret = a5xx_send_me_init(adreno_dev, rb);
	if (ret)
		return ret;

	/* Run the critical packets if we need to */
	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_CRITICAL_PACKETS)) {
		ret = a5xx_critical_packet_submit(adreno_dev, rb);
		if (ret)
			return ret;
	}

	/*
	 * Try to execute the zap shader if it exists, otherwise just try
	 * directly writing to the control register
	 */
	if (!adreno_dev->zap_loaded)
		kgsl_regwrite(device, A5XX_RBBM_SECVID_TRUST_CNTL, 0);
	else {
		cmds = adreno_ringbuffer_allocspace(rb, 2);
		if (IS_ERR(cmds))
			return  PTR_ERR(cmds);

		*cmds++ = cp_packet(adreno_dev, CP_SET_SECURE_MODE, 1);
		*cmds++ = 0;

		ret = a5xx_ringbuffer_submit(rb, NULL, true);
		if (!ret) {
			ret = adreno_spin_idle(adreno_dev, 2000);
			if (ret) {
				a5xx_spin_idle_debug(adreno_dev,
					"Switch to unsecure failed to idle\n");
				return ret;
			}
		}
	}

	ret = a5xx_gpmu_init(adreno_dev);
	if (ret)
		return ret;

	a5xx_post_start(adreno_dev);

	return 0;
}

/*
 * a5xx_microcode_read() - Read microcode
 * @adreno_dev: Pointer to adreno device
 */
static int a5xx_microcode_read(struct adreno_device *adreno_dev)
{
	int ret;
	struct adreno_firmware *pm4_fw = ADRENO_FW(adreno_dev, ADRENO_FW_PM4);
	struct adreno_firmware *pfp_fw = ADRENO_FW(adreno_dev, ADRENO_FW_PFP);
	const struct adreno_a5xx_core *a5xx_core = to_a5xx_core(adreno_dev);

	ret = adreno_get_firmware(adreno_dev, a5xx_core->pm4fw_name, pm4_fw);
	if (ret)
		return ret;

	ret = adreno_get_firmware(adreno_dev, a5xx_core->pfpfw_name, pfp_fw);
	if (ret)
		return ret;

	ret = _load_gpmu_firmware(adreno_dev);
	if (ret)
		return ret;

	_load_regfile(adreno_dev);

	return ret;
}

/* Register offset defines for A5XX, in order of enum adreno_regs */
static unsigned int a5xx_register_offsets[ADRENO_REG_REGISTER_MAX] = {
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE, A5XX_CP_RB_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE_HI, A5XX_CP_RB_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR_ADDR_LO,
			A5XX_CP_RB_RPTR_ADDR_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR_ADDR_HI,
			A5XX_CP_RB_RPTR_ADDR_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR, A5XX_CP_RB_RPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_WPTR, A5XX_CP_RB_WPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ME_CNTL, A5XX_CP_ME_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_CNTL, A5XX_CP_RB_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BASE, A5XX_CP_IB1_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BASE_HI, A5XX_CP_IB1_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BUFSZ, A5XX_CP_IB1_BUFSZ),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BASE, A5XX_CP_IB2_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BASE_HI, A5XX_CP_IB2_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BUFSZ, A5XX_CP_IB2_BUFSZ),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PROTECT_REG_0, A5XX_CP_PROTECT_REG_0),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PREEMPT, A5XX_CP_CONTEXT_SWITCH_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PREEMPT_DEBUG, ADRENO_REG_SKIP),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PREEMPT_DISABLE, ADRENO_REG_SKIP),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_LO,
				A5XX_CP_CONTEXT_SWITCH_SMMU_INFO_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_HI,
				A5XX_CP_CONTEXT_SWITCH_SMMU_INFO_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_STATUS, A5XX_RBBM_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_STATUS3, A5XX_RBBM_STATUS3),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_0_MASK, A5XX_RBBM_INT_0_MASK),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_CLOCK_CTL, A5XX_RBBM_CLOCK_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SW_RESET_CMD, A5XX_RBBM_SW_RESET_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_GPMU_POWER_COUNTER_ENABLE,
				A5XX_GPMU_POWER_COUNTER_ENABLE),
};

static void a5xx_cp_hw_err_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int status1, status2;

	kgsl_regread(device, A5XX_CP_INTERRUPT_STATUS, &status1);

	if (status1 & BIT(A5XX_CP_OPCODE_ERROR)) {
		unsigned int val;

		kgsl_regwrite(device, A5XX_CP_PFP_STAT_ADDR, 0);

		/*
		 * A5XX_CP_PFP_STAT_DATA is indexed, so read it twice to get the
		 * value we want
		 */
		kgsl_regread(device, A5XX_CP_PFP_STAT_DATA, &val);
		kgsl_regread(device, A5XX_CP_PFP_STAT_DATA, &val);

		dev_crit_ratelimited(device->dev,
					"ringbuffer opcode error | possible opcode=0x%8.8X\n",
					val);
	}
	if (status1 & BIT(A5XX_CP_RESERVED_BIT_ERROR))
		dev_crit_ratelimited(device->dev,
					"ringbuffer reserved bit error interrupt\n");
	if (status1 & BIT(A5XX_CP_HW_FAULT_ERROR)) {
		kgsl_regread(device, A5XX_CP_HW_FAULT, &status2);
		dev_crit_ratelimited(device->dev,
					"CP | Ringbuffer HW fault | status=%x\n",
					status2);
	}
	if (status1 & BIT(A5XX_CP_DMA_ERROR))
		dev_crit_ratelimited(device->dev, "CP | DMA error\n");
	if (status1 & BIT(A5XX_CP_REGISTER_PROTECTION_ERROR)) {
		kgsl_regread(device, A5XX_CP_PROTECT_STATUS, &status2);
		dev_crit_ratelimited(device->dev,
					"CP | Protected mode error| %s | addr=%x | status=%x\n",
					status2 & (1 << 24) ? "WRITE" : "READ",
					(status2 & 0xFFFFF) >> 2, status2);
	}
	if (status1 & BIT(A5XX_CP_AHB_ERROR)) {
		kgsl_regread(device, A5XX_CP_AHB_FAULT, &status2);
		dev_crit_ratelimited(device->dev,
					"ringbuffer AHB error interrupt | status=%x\n",
					status2);
	}
}

static void a5xx_err_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int reg;

	switch (bit) {
	case A5XX_INT_RBBM_AHB_ERROR: {
		kgsl_regread(device, A5XX_RBBM_AHB_ERROR_STATUS, &reg);

		/*
		 * Return the word address of the erroring register so that it
		 * matches the register specification
		 */
		dev_crit_ratelimited(device->dev,
					"RBBM | AHB bus error | %s | addr=%x | ports=%x:%x\n",
					reg & (1 << 28) ? "WRITE" : "READ",
					(reg & 0xFFFFF) >> 2,
					(reg >> 20) & 0x3,
					(reg >> 24) & 0xF);

		/* Clear the error */
		kgsl_regwrite(device, A5XX_RBBM_AHB_CMD, (1 << 4));
		break;
	}
	case A5XX_INT_RBBM_TRANSFER_TIMEOUT:
		dev_crit_ratelimited(device->dev,
					"RBBM: AHB transfer timeout\n");
		break;
	case A5XX_INT_RBBM_ME_MS_TIMEOUT:
		kgsl_regread(device, A5XX_RBBM_AHB_ME_SPLIT_STATUS, &reg);
		dev_crit_ratelimited(device->dev,
					"RBBM | ME master split timeout | status=%x\n",
					reg);
		break;
	case A5XX_INT_RBBM_PFP_MS_TIMEOUT:
		kgsl_regread(device, A5XX_RBBM_AHB_PFP_SPLIT_STATUS, &reg);
		dev_crit_ratelimited(device->dev,
					"RBBM | PFP master split timeout | status=%x\n",
					reg);
		break;
	case A5XX_INT_RBBM_ETS_MS_TIMEOUT:
		dev_crit_ratelimited(device->dev,
					"RBBM: ME master split timeout\n");
		break;
	case A5XX_INT_RBBM_ATB_ASYNC_OVERFLOW:
		dev_crit_ratelimited(device->dev,
					"RBBM: ATB ASYNC overflow\n");
		break;
	case A5XX_INT_RBBM_ATB_BUS_OVERFLOW:
		dev_crit_ratelimited(device->dev,
					"RBBM: ATB bus overflow\n");
		break;
	case A5XX_INT_UCHE_OOB_ACCESS:
		dev_crit_ratelimited(device->dev,
					"UCHE: Out of bounds access\n");
		break;
	case A5XX_INT_UCHE_TRAP_INTR:
		dev_crit_ratelimited(device->dev, "UCHE: Trap interrupt\n");
		break;
	case A5XX_INT_GPMU_VOLTAGE_DROOP:
		dev_crit_ratelimited(device->dev, "GPMU: Voltage droop\n");
		break;
	default:
		dev_crit_ratelimited(device->dev, "Unknown interrupt %d\n",
					bit);
	}
}

static void a5xx_irq_storm_worker(struct work_struct *work)
{
	struct adreno_device *adreno_dev = container_of(work,
			struct adreno_device, irq_storm_work);
	struct kgsl_device *device = &adreno_dev->dev;
	unsigned int status;

	mutex_lock(&device->mutex);

	/* Wait for the storm to clear up */
	do {
		kgsl_regwrite(device, A5XX_RBBM_INT_CLEAR_CMD,
				BIT(A5XX_INT_CP_CACHE_FLUSH_TS));
		kgsl_regread(device, A5XX_RBBM_INT_0_STATUS, &status);
	} while (status & BIT(A5XX_INT_CP_CACHE_FLUSH_TS));

	/* Re-enable the interrupt bit in the mask */
	adreno_dev->irq_mask |= BIT(A5XX_INT_CP_CACHE_FLUSH_TS);
	kgsl_regwrite(device, A5XX_RBBM_INT_0_MASK, adreno_dev->irq_mask);
	clear_bit(ADRENO_DEVICE_CACHE_FLUSH_TS_SUSPENDED, &adreno_dev->priv);

	dev_warn(device->dev, "Re-enabled A5XX_INT_CP_CACHE_FLUSH_TS\n");
	mutex_unlock(&device->mutex);

	/* Reschedule just to make sure everything retires */
	adreno_dispatcher_schedule(device);
}

static void a5xx_cp_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = &adreno_dev->dev;
	unsigned int cur;
	static unsigned int count;
	static unsigned int prev;

	if (test_bit(ADRENO_DEVICE_CACHE_FLUSH_TS_SUSPENDED, &adreno_dev->priv))
		return;

	kgsl_sharedmem_readl(device->memstore, &cur,
			KGSL_MEMSTORE_OFFSET(KGSL_MEMSTORE_GLOBAL,
				ref_wait_ts));

	/*
	 * prev holds a previously read value
	 * from memory.  It should be changed by the GPU with every
	 * interrupt. If the value we know about and the value we just
	 * read are the same, then we are likely in a storm.
	 * If this happens twice, disable the interrupt in the mask
	 * so the dispatcher can take care of the issue. It is then
	 * up to the dispatcher to re-enable the mask once all work
	 * is done and the storm has ended.
	 */
	if (prev == cur) {
		count++;
		if (count == 2) {
			/* disable interrupt from the mask */
			set_bit(ADRENO_DEVICE_CACHE_FLUSH_TS_SUSPENDED,
					&adreno_dev->priv);

			adreno_dev->irq_mask &=
				~BIT(A5XX_INT_CP_CACHE_FLUSH_TS);

			kgsl_regwrite(device, A5XX_RBBM_INT_0_MASK,
				adreno_dev->irq_mask);

			kgsl_schedule_work(&adreno_dev->irq_storm_work);

			return;
		}
	} else {
		count = 0;
		prev = cur;
	}

	a5xx_preemption_trigger(adreno_dev);
	adreno_dispatcher_schedule(device);
}

static const char *gpmu_int_msg[32] = {
	[FW_INTR_INFO] = "FW_INTR_INFO",
	[LLM_ACK_ERR_INTR] = "LLM_ACK_ERR_INTR",
	[ISENS_TRIM_ERR_INTR] = "ISENS_TRIM_ERR_INTR",
	[ISENS_ERR_INTR] = "ISENS_ERR_INTR",
	[ISENS_IDLE_ERR_INTR] = "ISENS_IDLE_ERR_INTR",
	[ISENS_PWR_ON_ERR_INTR] = "ISENS_PWR_ON_ERR_INTR",
	[6 ... 30] = "",
	[WDOG_EXPITED] = "WDOG_EXPITED"};

static void a5xx_gpmu_int_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int reg, i;

	kgsl_regread(device, A5XX_GPMU_RBBM_INTR_INFO, &reg);

	if (reg & (~VALID_GPMU_IRQ)) {
		dev_crit_ratelimited(device->dev,
					"GPMU: Unknown IRQ mask 0x%08lx in 0x%08x\n",
					reg & (~VALID_GPMU_IRQ), reg);
	}

	for (i = 0; i < 32; i++)
		switch (reg & BIT(i)) {
		case BIT(WDOG_EXPITED):
			if (test_and_clear_bit(ADRENO_DEVICE_GPMU_INITIALIZED,
				&adreno_dev->priv)) {
				/* Stop GPMU */
				kgsl_regwrite(device,
					A5XX_GPMU_CM3_SYSRESET, 1);
				kgsl_schedule_work(&adreno_dev->gpmu_work);
			}
			/* fallthrough */
		case BIT(FW_INTR_INFO):
		case BIT(LLM_ACK_ERR_INTR):
		case BIT(ISENS_TRIM_ERR_INTR):
		case BIT(ISENS_ERR_INTR):
		case BIT(ISENS_IDLE_ERR_INTR):
		case BIT(ISENS_PWR_ON_ERR_INTR):
			dev_crit_ratelimited(device->dev,
						"GPMU: interrupt %s(%08lx)\n",
						gpmu_int_msg[i],
						BIT(i));
			break;
	}
}

/*
 * a5x_gpc_err_int_callback() - Isr for GPC error interrupts
 * @adreno_dev: Pointer to device
 * @bit: Interrupt bit
 */
static void a5x_gpc_err_int_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/*
	 * GPC error is typically the result of mistake SW programming.
	 * Force GPU fault for this interrupt so that we can debug it
	 * with help of register dump.
	 */

	dev_crit(device->dev, "RBBM: GPC error\n");
	adreno_irqctrl(adreno_dev, 0);

	/* Trigger a fault in the dispatcher - this will effect a restart */
	adreno_dispatcher_fault(adreno_dev, ADRENO_SOFT_FAULT);
	adreno_dispatcher_schedule(device);
}

u64 a5xx_read_alwayson(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 lo = 0, hi = 0;

	kgsl_regread(device, A5XX_RBBM_ALWAYSON_COUNTER_LO, &lo);

	/* The upper 32 bits are only reliable on A540 targets */
	if (adreno_is_a540(adreno_dev))
		kgsl_regread(device, A5XX_RBBM_ALWAYSON_COUNTER_HI, &hi);

	return (((u64) hi) << 32) | lo;
}


static const struct adreno_irq_funcs a5xx_irq_funcs[32] = {
	ADRENO_IRQ_CALLBACK(NULL),              /* 0 - RBBM_GPU_IDLE */
	ADRENO_IRQ_CALLBACK(a5xx_err_callback), /* 1 - RBBM_AHB_ERROR */
	ADRENO_IRQ_CALLBACK(a5xx_err_callback), /* 2 - RBBM_TRANSFER_TIMEOUT */
	/* 3 - RBBM_ME_MASTER_SPLIT_TIMEOUT  */
	ADRENO_IRQ_CALLBACK(a5xx_err_callback),
	/* 4 - RBBM_PFP_MASTER_SPLIT_TIMEOUT */
	ADRENO_IRQ_CALLBACK(a5xx_err_callback),
	 /* 5 - RBBM_ETS_MASTER_SPLIT_TIMEOUT */
	ADRENO_IRQ_CALLBACK(a5xx_err_callback),
	/* 6 - RBBM_ATB_ASYNC_OVERFLOW */
	ADRENO_IRQ_CALLBACK(a5xx_err_callback),
	ADRENO_IRQ_CALLBACK(a5x_gpc_err_int_callback), /* 7 - GPC_ERR */
	ADRENO_IRQ_CALLBACK(a5xx_preempt_callback),/* 8 - CP_SW */
	ADRENO_IRQ_CALLBACK(a5xx_cp_hw_err_callback), /* 9 - CP_HW_ERROR */
	/* 10 - CP_CCU_FLUSH_DEPTH_TS */
	ADRENO_IRQ_CALLBACK(NULL),
	 /* 11 - CP_CCU_FLUSH_COLOR_TS */
	ADRENO_IRQ_CALLBACK(NULL),
	 /* 12 - CP_CCU_RESOLVE_TS */
	ADRENO_IRQ_CALLBACK(NULL),
	ADRENO_IRQ_CALLBACK(NULL), /* 13 - CP_IB2_INT */
	ADRENO_IRQ_CALLBACK(NULL), /* 14 - CP_IB1_INT */
	ADRENO_IRQ_CALLBACK(NULL), /* 15 - CP_RB_INT */
	/* 16 - CCP_UNUSED_1 */
	ADRENO_IRQ_CALLBACK(NULL),
	ADRENO_IRQ_CALLBACK(NULL), /* 17 - CP_RB_DONE_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 18 - CP_WT_DONE_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 19 - UNKNOWN_1 */
	ADRENO_IRQ_CALLBACK(a5xx_cp_callback), /* 20 - CP_CACHE_FLUSH_TS */
	/* 21 - UNUSED_2 */
	ADRENO_IRQ_CALLBACK(NULL),
	ADRENO_IRQ_CALLBACK(a5xx_err_callback), /* 22 - RBBM_ATB_BUS_OVERFLOW */
	/* 23 - MISC_HANG_DETECT */
	ADRENO_IRQ_CALLBACK(adreno_hang_int_callback),
	ADRENO_IRQ_CALLBACK(a5xx_err_callback), /* 24 - UCHE_OOB_ACCESS */
	ADRENO_IRQ_CALLBACK(a5xx_err_callback), /* 25 - UCHE_TRAP_INTR */
	ADRENO_IRQ_CALLBACK(NULL), /* 26 - DEBBUS_INTR_0 */
	ADRENO_IRQ_CALLBACK(NULL), /* 27 - DEBBUS_INTR_1 */
	ADRENO_IRQ_CALLBACK(a5xx_err_callback), /* 28 - GPMU_VOLTAGE_DROOP */
	ADRENO_IRQ_CALLBACK(a5xx_gpmu_int_callback), /* 29 - GPMU_FIRMWARE */
	ADRENO_IRQ_CALLBACK(NULL), /* 30 - ISDB_CPU_IRQ */
	ADRENO_IRQ_CALLBACK(NULL), /* 31 - ISDB_UNDER_DEBUG */
};

static irqreturn_t a5xx_irq_handler(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	irqreturn_t ret;
	u32 status;

	kgsl_regread(device, A5XX_RBBM_INT_0_STATUS, &status);

	/*
	 * Clear all the interrupt bits except A5XX_INT_RBBM_AHB_ERROR.
	 * The interrupt will stay asserted until it is cleared by the handler
	 * so don't touch it yet to avoid a storm
	 */
	kgsl_regwrite(device, A5XX_RBBM_INT_CLEAR_CMD,
		status & ~A5XX_INT_RBBM_AHB_ERROR);

	/* Call the helper function for callbacks */
	ret = adreno_irq_callbacks(adreno_dev, a5xx_irq_funcs, status);

	trace_kgsl_a5xx_irq_status(adreno_dev, status);

	/* Now chear AHB_ERROR if it was set */
	if (status & A5XX_INT_RBBM_AHB_ERROR)
		kgsl_regwrite(device, A5XX_RBBM_INT_CLEAR_CMD,
			A5XX_INT_RBBM_AHB_ERROR);

	return ret;
}

static bool a5xx_hw_isidle(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 status;

	/*
	 * Due to CRC idle throttling the GPU idle hysteresis on a540 can take
	 * up to 5uS to expire
	 */
	if (adreno_is_a540(adreno_dev))
		udelay(5);

	kgsl_regread(device, A5XX_RBBM_STATUS, &status);

	if (status & 0xfffffffe)
		return false;

	kgsl_regread(device, A5XX_RBBM_INT_0_STATUS, &status);

	/* Return busy if a interrupt is pending */
	return !((status & adreno_dev->irq_mask) ||
		atomic_read(&adreno_dev->pending_irq_refcnt));
}

static int a5xx_clear_pending_transactions(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 mask = A5XX_VBIF_XIN_HALT_CTRL0_MASK;
	int ret;

	kgsl_regwrite(device, A5XX_VBIF_XIN_HALT_CTRL0, mask);
	ret = adreno_wait_for_halt_ack(device, A5XX_VBIF_XIN_HALT_CTRL1, mask);
	kgsl_regwrite(device, A5XX_VBIF_XIN_HALT_CTRL0, 0);

	return ret;
}

static bool a5xx_is_hw_collapsible(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int reg;

	if (!adreno_isidle(adreno_dev))
		return false;

	/* If feature is not supported or enabled, no worry */
	if (!adreno_dev->sptp_pc_enabled)
		return true;
	kgsl_regread(device, A5XX_GPMU_SP_PWR_CLK_STATUS, &reg);
	if (reg & BIT(20))
		return false;
	kgsl_regread(device, A5XX_GPMU_RBCCU_PWR_CLK_STATUS, &reg);
	return !(reg & BIT(20));
}

static void a5xx_remove(struct adreno_device *adreno_dev)
{
	if (ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION))
		del_timer(&adreno_dev->preempt.timer);
}

static void a5xx_power_stats(struct adreno_device *adreno_dev,
		struct kgsl_power_stats *stats)
{
	static u32 rbbm0_hi;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	s64 freq = kgsl_pwrctrl_active_freq(&device->pwrctrl) / 1000000;
	struct adreno_busy_data *busy = &adreno_dev->busy_data;
	s64 gpu_busy = 0;
	u32 lo, hi;
	s64 adj;

	/* Sometimes this counter can go backwards, so try to detect that */
	kgsl_regread(device, A5XX_RBBM_PERFCTR_RBBM_0_LO, &lo);
	kgsl_regread(device, A5XX_RBBM_PERFCTR_RBBM_0_HI, &hi);

	if (busy->gpu_busy) {
		if (lo < busy->gpu_busy) {
			if (hi == rbbm0_hi) {
				dev_warn_once(device->dev,
					"abmormal value from RBBM_0 perfcounter: %x %x\n",
					lo, busy->gpu_busy);
				gpu_busy = 0;
			} else {
				gpu_busy = (UINT_MAX - busy->gpu_busy) + lo;
				rbbm0_hi = hi;
			}
		} else
			gpu_busy = lo - busy->gpu_busy;
	} else {
		gpu_busy = 0;
		rbbm0_hi = 0;
	}

	busy->gpu_busy = lo;

	adj = a5xx_read_throttling_counters(adreno_dev);
	if (-adj <= gpu_busy)
		gpu_busy += adj;
	else
		gpu_busy = 0;

	stats->busy_time = gpu_busy / freq;

	if (adreno_is_a530(adreno_dev) && adreno_dev->lm_threshold_count)
		kgsl_regread(device, adreno_dev->lm_threshold_count,
			&adreno_dev->lm_threshold_cross);
	else if (adreno_is_a540(adreno_dev))
		adreno_dev->lm_threshold_cross = adj;

	if (!device->pwrctrl.bus_control)
		return;

	stats->ram_time = counter_delta(device, adreno_dev->ram_cycles_lo,
		&busy->bif_ram_cycles);

	stats->ram_wait = counter_delta(device, adreno_dev->starved_ram_lo,
		&busy->bif_starved_ram);
}

static int a5xx_setproperty(struct kgsl_device_private *dev_priv,
		u32 type, void __user *value, u32 sizebytes)
{
	struct kgsl_device *device = dev_priv->device;
	u32 enable;

	if (type != KGSL_PROP_PWRCTRL)
		return -ENODEV;

	if (sizebytes != sizeof(enable))
		return -EINVAL;

	if (copy_from_user(&enable, value, sizeof(enable)))
		return -EFAULT;

	mutex_lock(&device->mutex);

	if (enable) {
		device->pwrctrl.ctrl_flags = 0;
		kgsl_pwrscale_enable(device);
	} else {
		kgsl_pwrctrl_change_state(device, KGSL_STATE_ACTIVE);
		device->pwrctrl.ctrl_flags = KGSL_PWR_ON;
		kgsl_pwrscale_disable(device, true);
	}

	mutex_unlock(&device->mutex);

	return 0;
}

#ifdef CONFIG_QCOM_KGSL_CORESIGHT
static struct adreno_coresight_register a5xx_coresight_registers[] = {
	{ A5XX_RBBM_CFG_DBGBUS_SEL_A },
	{ A5XX_RBBM_CFG_DBGBUS_SEL_B },
	{ A5XX_RBBM_CFG_DBGBUS_SEL_C },
	{ A5XX_RBBM_CFG_DBGBUS_SEL_D },
	{ A5XX_RBBM_CFG_DBGBUS_CNTLT },
	{ A5XX_RBBM_CFG_DBGBUS_CNTLM },
	{ A5XX_RBBM_CFG_DBGBUS_OPL },
	{ A5XX_RBBM_CFG_DBGBUS_OPE },
	{ A5XX_RBBM_CFG_DBGBUS_IVTL_0 },
	{ A5XX_RBBM_CFG_DBGBUS_IVTL_1 },
	{ A5XX_RBBM_CFG_DBGBUS_IVTL_2 },
	{ A5XX_RBBM_CFG_DBGBUS_IVTL_3 },
	{ A5XX_RBBM_CFG_DBGBUS_MASKL_0 },
	{ A5XX_RBBM_CFG_DBGBUS_MASKL_1 },
	{ A5XX_RBBM_CFG_DBGBUS_MASKL_2 },
	{ A5XX_RBBM_CFG_DBGBUS_MASKL_3 },
	{ A5XX_RBBM_CFG_DBGBUS_BYTEL_0 },
	{ A5XX_RBBM_CFG_DBGBUS_BYTEL_1 },
	{ A5XX_RBBM_CFG_DBGBUS_IVTE_0 },
	{ A5XX_RBBM_CFG_DBGBUS_IVTE_1 },
	{ A5XX_RBBM_CFG_DBGBUS_IVTE_2 },
	{ A5XX_RBBM_CFG_DBGBUS_IVTE_3 },
	{ A5XX_RBBM_CFG_DBGBUS_MASKE_0 },
	{ A5XX_RBBM_CFG_DBGBUS_MASKE_1 },
	{ A5XX_RBBM_CFG_DBGBUS_MASKE_2 },
	{ A5XX_RBBM_CFG_DBGBUS_MASKE_3 },
	{ A5XX_RBBM_CFG_DBGBUS_NIBBLEE },
	{ A5XX_RBBM_CFG_DBGBUS_PTRC0 },
	{ A5XX_RBBM_CFG_DBGBUS_PTRC1 },
	{ A5XX_RBBM_CFG_DBGBUS_LOADREG },
	{ A5XX_RBBM_CFG_DBGBUS_IDX },
	{ A5XX_RBBM_CFG_DBGBUS_CLRC },
	{ A5XX_RBBM_CFG_DBGBUS_LOADIVT },
	{ A5XX_RBBM_CFG_DBGBUS_EVENT_LOGIC },
	{ A5XX_RBBM_CFG_DBGBUS_OVER },
	{ A5XX_RBBM_CFG_DBGBUS_COUNT0 },
	{ A5XX_RBBM_CFG_DBGBUS_COUNT1 },
	{ A5XX_RBBM_CFG_DBGBUS_COUNT2 },
	{ A5XX_RBBM_CFG_DBGBUS_COUNT3 },
	{ A5XX_RBBM_CFG_DBGBUS_COUNT4 },
	{ A5XX_RBBM_CFG_DBGBUS_COUNT5 },
	{ A5XX_RBBM_CFG_DBGBUS_TRACE_ADDR },
	{ A5XX_RBBM_CFG_DBGBUS_TRACE_BUF0 },
	{ A5XX_RBBM_CFG_DBGBUS_TRACE_BUF1 },
	{ A5XX_RBBM_CFG_DBGBUS_TRACE_BUF2 },
	{ A5XX_RBBM_CFG_DBGBUS_TRACE_BUF3 },
	{ A5XX_RBBM_CFG_DBGBUS_TRACE_BUF4 },
	{ A5XX_RBBM_CFG_DBGBUS_MISR0 },
	{ A5XX_RBBM_CFG_DBGBUS_MISR1 },
	{ A5XX_RBBM_AHB_DBG_CNTL },
	{ A5XX_RBBM_READ_AHB_THROUGH_DBG },
	{ A5XX_RBBM_DBG_LO_HI_GPIO },
	{ A5XX_RBBM_EXT_TRACE_BUS_CNTL },
	{ A5XX_RBBM_EXT_VBIF_DBG_CNTL },
};

static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_a, &a5xx_coresight_registers[0]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_b, &a5xx_coresight_registers[1]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_c, &a5xx_coresight_registers[2]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_d, &a5xx_coresight_registers[3]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_cntlt, &a5xx_coresight_registers[4]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_cntlm, &a5xx_coresight_registers[5]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_opl, &a5xx_coresight_registers[6]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ope, &a5xx_coresight_registers[7]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_0, &a5xx_coresight_registers[8]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_1, &a5xx_coresight_registers[9]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_2, &a5xx_coresight_registers[10]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_3, &a5xx_coresight_registers[11]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_0, &a5xx_coresight_registers[12]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_1, &a5xx_coresight_registers[13]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_2, &a5xx_coresight_registers[14]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_3, &a5xx_coresight_registers[15]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_bytel_0, &a5xx_coresight_registers[16]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_bytel_1, &a5xx_coresight_registers[17]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_0, &a5xx_coresight_registers[18]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_1, &a5xx_coresight_registers[19]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_2, &a5xx_coresight_registers[20]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_3, &a5xx_coresight_registers[21]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_0, &a5xx_coresight_registers[22]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_1, &a5xx_coresight_registers[23]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_2, &a5xx_coresight_registers[24]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_3, &a5xx_coresight_registers[25]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_nibblee, &a5xx_coresight_registers[26]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ptrc0, &a5xx_coresight_registers[27]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ptrc1, &a5xx_coresight_registers[28]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_loadreg, &a5xx_coresight_registers[29]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_idx, &a5xx_coresight_registers[30]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_clrc, &a5xx_coresight_registers[31]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_loadivt, &a5xx_coresight_registers[32]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_event_logic,
				&a5xx_coresight_registers[33]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_over, &a5xx_coresight_registers[34]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_count0, &a5xx_coresight_registers[35]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_count1, &a5xx_coresight_registers[36]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_count2, &a5xx_coresight_registers[37]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_count3, &a5xx_coresight_registers[38]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_count4, &a5xx_coresight_registers[39]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_count5, &a5xx_coresight_registers[40]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_trace_addr,
				&a5xx_coresight_registers[41]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_trace_buf0,
				&a5xx_coresight_registers[42]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_trace_buf1,
				&a5xx_coresight_registers[43]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_trace_buf2,
				&a5xx_coresight_registers[44]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_trace_buf3,
				&a5xx_coresight_registers[45]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_trace_buf4,
				&a5xx_coresight_registers[46]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_misr0, &a5xx_coresight_registers[47]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_misr1, &a5xx_coresight_registers[48]);
static ADRENO_CORESIGHT_ATTR(ahb_dbg_cntl, &a5xx_coresight_registers[49]);
static ADRENO_CORESIGHT_ATTR(read_ahb_through_dbg,
				&a5xx_coresight_registers[50]);
static ADRENO_CORESIGHT_ATTR(dbg_lo_hi_gpio, &a5xx_coresight_registers[51]);
static ADRENO_CORESIGHT_ATTR(ext_trace_bus_cntl, &a5xx_coresight_registers[52]);
static ADRENO_CORESIGHT_ATTR(ext_vbif_dbg_cntl, &a5xx_coresight_registers[53]);

static struct attribute *a5xx_coresight_attrs[] = {
	&coresight_attr_cfg_dbgbus_sel_a.attr.attr,
	&coresight_attr_cfg_dbgbus_sel_b.attr.attr,
	&coresight_attr_cfg_dbgbus_sel_c.attr.attr,
	&coresight_attr_cfg_dbgbus_sel_d.attr.attr,
	&coresight_attr_cfg_dbgbus_cntlt.attr.attr,
	&coresight_attr_cfg_dbgbus_cntlm.attr.attr,
	&coresight_attr_cfg_dbgbus_opl.attr.attr,
	&coresight_attr_cfg_dbgbus_ope.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_0.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_1.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_2.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_3.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_0.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_1.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_2.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_3.attr.attr,
	&coresight_attr_cfg_dbgbus_bytel_0.attr.attr,
	&coresight_attr_cfg_dbgbus_bytel_1.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_0.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_1.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_2.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_3.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_0.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_1.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_2.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_3.attr.attr,
	&coresight_attr_cfg_dbgbus_nibblee.attr.attr,
	&coresight_attr_cfg_dbgbus_ptrc0.attr.attr,
	&coresight_attr_cfg_dbgbus_ptrc1.attr.attr,
	&coresight_attr_cfg_dbgbus_loadreg.attr.attr,
	&coresight_attr_cfg_dbgbus_idx.attr.attr,
	&coresight_attr_cfg_dbgbus_clrc.attr.attr,
	&coresight_attr_cfg_dbgbus_loadivt.attr.attr,
	&coresight_attr_cfg_dbgbus_event_logic.attr.attr,
	&coresight_attr_cfg_dbgbus_over.attr.attr,
	&coresight_attr_cfg_dbgbus_count0.attr.attr,
	&coresight_attr_cfg_dbgbus_count1.attr.attr,
	&coresight_attr_cfg_dbgbus_count2.attr.attr,
	&coresight_attr_cfg_dbgbus_count3.attr.attr,
	&coresight_attr_cfg_dbgbus_count4.attr.attr,
	&coresight_attr_cfg_dbgbus_count5.attr.attr,
	&coresight_attr_cfg_dbgbus_trace_addr.attr.attr,
	&coresight_attr_cfg_dbgbus_trace_buf0.attr.attr,
	&coresight_attr_cfg_dbgbus_trace_buf1.attr.attr,
	&coresight_attr_cfg_dbgbus_trace_buf2.attr.attr,
	&coresight_attr_cfg_dbgbus_trace_buf3.attr.attr,
	&coresight_attr_cfg_dbgbus_trace_buf4.attr.attr,
	&coresight_attr_cfg_dbgbus_misr0.attr.attr,
	&coresight_attr_cfg_dbgbus_misr1.attr.attr,
	&coresight_attr_ahb_dbg_cntl.attr.attr,
	&coresight_attr_read_ahb_through_dbg.attr.attr,
	&coresight_attr_dbg_lo_hi_gpio.attr.attr,
	&coresight_attr_ext_trace_bus_cntl.attr.attr,
	&coresight_attr_ext_vbif_dbg_cntl.attr.attr,
	NULL,
};

static const struct attribute_group a5xx_coresight_group = {
	.attrs = a5xx_coresight_attrs,
};

static const struct attribute_group *a5xx_coresight_groups[] = {
	&a5xx_coresight_group,
	NULL,
};

static struct adreno_coresight a5xx_coresight = {
	.registers = a5xx_coresight_registers,
	.count = ARRAY_SIZE(a5xx_coresight_registers),
	.groups = a5xx_coresight_groups,
};
#endif

const struct adreno_gpudev adreno_a5xx_gpudev = {
	.reg_offsets = a5xx_register_offsets,
#ifdef CONFIG_QCOM_KGSL_CORESIGHT
	.coresight = {&a5xx_coresight},
#endif
	.probe = a5xx_probe,
	.start = a5xx_start,
	.snapshot = a5xx_snapshot,
	.init = a5xx_init,
	.irq_handler = a5xx_irq_handler,
	.rb_start = a5xx_rb_start,
	.regulator_enable = a5xx_regulator_enable,
	.regulator_disable = a5xx_regulator_disable,
	.pwrlevel_change_settings = a5xx_pwrlevel_change_settings,
	.preemption_schedule = a5xx_preemption_schedule,
#if IS_ENABLED(CONFIG_COMMON_CLK_QCOM)
	.clk_set_options = a5xx_clk_set_options,
#endif
	.read_alwayson = a5xx_read_alwayson,
	.hw_isidle = a5xx_hw_isidle,
	.power_ops = &adreno_power_operations,
	.clear_pending_transactions = a5xx_clear_pending_transactions,
	.remove = a5xx_remove,
	.ringbuffer_submitcmd = a5xx_ringbuffer_submitcmd,
	.is_hw_collapsible = a5xx_is_hw_collapsible,
	.power_stats = a5xx_power_stats,
	.setproperty = a5xx_setproperty,
};
