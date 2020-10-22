#include <linux/gpio.h>
#include <linux/printk.h>

#include "swd.h"

/*
 * Note: Most of this code was taken, with minimal changes, from the
 *       CV1 HMD_Main firmware
 */

/* Note: These values are defined in
 * ARM Debug Interface Architecture Specification ADIv5.2
 */

#define SWD_VAL_START   1
#define SWD_VAL_DP      0
#define SWD_VAL_AP      1
#define SWD_VAL_WRITE   0
#define SWD_VAL_READ    1
#define SWD_VAL_STOP    0
#define SWD_VAL_PARK    1
#define SWD_VAL_IDLE    0

#define SWD_CLK_FALLING 0
#define SWD_CLK_RISING  1

#define SWD_VAL_OK    	0x01
#define SWD_VAL_WAIT  	0x02
#define SWD_VAL_FAULT 	0x04


/* DP registers */
#define SWD_DP_REG_RO_IDCODE    0x0
#define SWD_DP_REG_WO_ABORT     0x0
#define SWD_DP_REG_RW_CTRLSTAT  0x4
#define SWD_DP_REG_RO_RESEND    0x8
#define SWD_DP_REG_WO_SELECT    0x8
#define SWD_DP_REG_RO_RDBUFF    0xC

/* Supported IDCODEs */
/* Cortex-M0 r0p0 - Conforms to DPv1 */
#define SWD_VAL_IDCODE_CM0DAP   0x0BB11477
/* Cortex-M0+ r0p0 - Conforms to DPv1 */
#define SWD_VAL_IDCODE_CM0PDAP  0x0BC11477
/* STM32F3/STM32F4 - Conforms to DPv1 */
#define SWD_VAL_IDCODE_STM32L   0x2BA01477

/* DP REG ABORT */
#define SWD_VAL_ABORT_ORUNERRCLR 0x10
#define SWD_VAL_ABORT_WDERRCLR   0x08
#define SWD_VAL_ABORT_STKERRCLR  0x04
#define SWD_VAL_ABORT_STKCMPCLR  0x02
#define SWD_VAL_ABORT_CLEAR (SWD_VAL_ABORT_ORUNERRCLR | \
			     SWD_VAL_ABORT_WDERRCLR   | \
			     SWD_VAL_ABORT_STKERRCLR  | \
			     SWD_VAL_ABORT_STKCMPCLR)

/* DP REG CTRLSTAT */
#define SWD_VAL_CTRLSTAT_CSYSPWRUPACK 31
#define SWD_VAL_CTRLSTAT_CSYSPWRUPREQ 30
#define SWD_VAL_CTRLSTAT_CDBGPWRUPACK 29
#define SWD_VAL_CTRLSTAT_CDBGPWRUPREQ 28

/* DP REG SELECT */
#define SWD_VAL_SELECT_MEMAP 0

/* MEM-AP registers */
#define SWD_MEMAP_REG_RW_CSW 0x0
#define SWD_MEMAP_REG_RW_TAR 0x4
#define SWD_MEMAP_REG_RW_DRW 0xC

/* Application Interrupt and Reset Control Register */
#define SWD_REG_AIRCR			0xE000ED0C
#define SWD_VAL_AIRCR_VECTKEY	0x05FA0000
#define SWD_VAL_AIRCR_SYSRSTRQ	0x04

/* Core Debug registers */
#define SWD_REG_DHCSR          	0xE000EDF0
#define SWD_VAL_DHCSR_DBGKEY    0xa05f0000
#define SWD_VAL_DHCSR_C_HALT    0x02
#define SWD_VAL_DHCSR_C_DEBUGEN 0x01
#define SWD_VAL_DHCSR_NOINC_32 	0x23000002

static void swd_wire_write_len(struct swd_dev_data *devdata, bool value,
			       int len);
static bool swd_mode_switch(struct swd_dev_data *devdata);

void swd_init(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	devdata->direction = SWD_DIRECTION_OUT;

	/* set swd lines to output and drive both low */
	gpio_direction_output(devdata->gpio_swdclk, 0);
	gpio_direction_output(devdata->gpio_swdio, 0);

	/* must be 150+ clocks @125kHz+ */
	swd_wire_write_len(devdata, 1, 200);
	swd_mode_switch(devdata);
}

void swd_deinit(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	/* float swd lines */
	gpio_direction_input(devdata->gpio_swdclk);
	gpio_direction_input(devdata->gpio_swdio);
}

static void swd_wire_clock(struct swd_dev_data *devdata)
{
	gpio_set_value(devdata->gpio_swdclk, SWD_CLK_RISING);
	gpio_set_value(devdata->gpio_swdclk, SWD_CLK_FALLING);
}

static inline void swd_wire_set_direction(struct swd_dev_data *devdata,
					  enum swd_direction direction)
{
	if (devdata->direction != direction) {
		if (direction == SWD_DIRECTION_OUT) {
			gpio_direction_output(devdata->gpio_swdio, 0);
		} else {
			gpio_direction_input(devdata->gpio_swdio);
		}
		devdata->direction = direction;
	}
}

static void swd_wire_write(struct swd_dev_data *devdata, bool value)
{
	swd_wire_set_direction(devdata, SWD_DIRECTION_OUT);
	gpio_set_value(devdata->gpio_swdio, value);
	swd_wire_clock(devdata);
}

static void swd_wire_write_len(struct swd_dev_data *devdata, bool value,
			       int len)
{
	int i = 0;

	swd_wire_set_direction(devdata, SWD_DIRECTION_OUT);
	gpio_set_value(devdata->gpio_swdio, value);
	for (i = 0; i < len; i++) {
		swd_wire_clock(devdata);
	}
}

static bool swd_wire_read(struct swd_dev_data *devdata)
{
	bool value = false;

	swd_wire_set_direction(devdata, SWD_DIRECTION_IN);
	value = gpio_get_value(devdata->gpio_swdio);
	swd_wire_clock(devdata);
	return value;
}

static void swd_wire_turnaround(struct swd_dev_data *devdata)
{
	swd_wire_read(devdata);
}

static bool swd_wire_header(struct swd_dev_data *devdata, bool read,
			    bool apndp, u8 reg)
{
	u8 ack = 0;
	bool address2 = !!(reg & (1<<2));
	bool address3 = !!(reg & (1<<3));
	u8 bitcount = (apndp + read + address2 + address3);
	bool parity = bitcount & 1;

	while (true) {
		swd_wire_write(devdata, SWD_VAL_START);
		swd_wire_write(devdata, apndp);
		swd_wire_write(devdata, read);
		swd_wire_write(devdata, address2);
		swd_wire_write(devdata, address3);
		swd_wire_write(devdata, parity);
		swd_wire_write(devdata, SWD_VAL_STOP);
		swd_wire_write(devdata, SWD_VAL_PARK);

		swd_wire_turnaround(devdata);

		ack = 0;
		ack |= swd_wire_read(devdata) << 0;
		ack |= swd_wire_read(devdata) << 1;
		ack |= swd_wire_read(devdata) << 2;

		if (ack == SWD_VAL_OK) {
			return true;
		}

		swd_wire_turnaround(devdata);

		if (ack == SWD_VAL_WAIT) {
			continue;
		}

		if (ack == SWD_VAL_FAULT) {
			/* TODO: Better error handling */
			printk("JMD: Bus fault condition\n");
		}

		return false;
	}
}

static void swd_dpap_write(struct swd_dev_data *devdata, bool apndp,
			   u8 reg, u32 data)
{
	int bitcount = 0;
	int i = 0;
	bool parity;

	if (!swd_wire_header(devdata, SWD_VAL_WRITE, apndp, reg)) {
		/* TODO: Better error handling */
		printk("JMD: SWD response is invalid/unknown\n");
		return;
	}
	swd_wire_turnaround(devdata);

	for (i = 0; i < 32; i++) {
		bool value = !!(data & (1<<i));
		bitcount += value;
		swd_wire_write(devdata, value);
	}
	parity = bitcount & 1;
	swd_wire_write(devdata, parity);
	swd_wire_write(devdata, 0);
}

static u32 swd_dpap_read(struct swd_dev_data *devdata, bool apndp, u8 reg)
{
	u32 data = 0;
	int bitcount = 0;
	int i = 0;
	bool parity, check;

	if (!swd_wire_header(devdata, SWD_VAL_READ, apndp, reg)) {
		/* TODO: Better error handling */
		printk("JMD: SWD response is invalid/unknown\n");
		return 0;
	}

	for (i = 0; i < 32; i++) {
		bool value = swd_wire_read(devdata);
		bitcount += value;
		data |= (value<<i);
	}
	parity = bitcount & 1;
	check = swd_wire_read(devdata);
	if (check != parity) {
		/* TODO: Better error handling */
		printk("JMD: Parity error\n");
		return 0;
	}
	swd_wire_turnaround(devdata);
	swd_wire_write(devdata, 0);

	return data;
}

static void swd_ap_write(struct swd_dev_data *devdata, u8 reg, u32 data)
{
	swd_dpap_write(devdata, SWD_VAL_AP, reg, data);
}

static u32 swd_ap_read(struct swd_dev_data *devdata, u8 reg)
{
	return swd_dpap_read(devdata, SWD_VAL_AP, reg);
}

static void swd_dp_write(struct swd_dev_data *devdata, u8 reg, u32 data)
{
	swd_dpap_write(devdata, SWD_VAL_DP, reg, data);
}

static u32 swd_dp_read(struct swd_dev_data *devdata, u8 reg)
{
	return swd_dpap_read(devdata, SWD_VAL_DP, reg);
}

void swd_memory_write(struct device *dev, u32 address, u32 data)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	swd_ap_write(devdata, SWD_MEMAP_REG_RW_TAR, address);
	swd_ap_write(devdata, SWD_MEMAP_REG_RW_DRW, data);
}

u32 swd_memory_read(struct device *dev, u32 address)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	swd_ap_write(devdata, SWD_MEMAP_REG_RW_TAR, address);
	swd_ap_read(devdata, SWD_MEMAP_REG_RW_DRW);
	return swd_dp_read(devdata, SWD_DP_REG_RO_RDBUFF);
}

static bool swd_mode_switch(struct swd_dev_data *devdata)
{
	int i = 0;
	bool status = true;
	const u16 jtag_to_swd = 0xE79E;
	u32 idcode, ack, req;

	/* JTAG to SWD */
	swd_wire_write_len(devdata, 1, 100);
	for (i = 0; i < 16; i++) {
		bool value = !!(jtag_to_swd & (1<<i));

		swd_wire_write(devdata, value);
	}
	swd_wire_write_len(devdata, 1, 100);
	swd_wire_write_len(devdata, 0, 20);

	/* IDCODE */
	status = swd_wire_header(devdata,
				 SWD_VAL_READ,
				 SWD_VAL_DP,
				 SWD_DP_REG_RO_IDCODE);
	for (i = 0; i < 34; i++) {
		swd_wire_read(devdata);
	}

	if (status) {
		idcode = swd_dp_read(devdata, 0);
		if (!(idcode == SWD_VAL_IDCODE_CM0DAP ||
		      idcode == SWD_VAL_IDCODE_CM0PDAP ||
		      idcode == SWD_VAL_IDCODE_STM32L)) {
			/* TODO: Better error handling */
			printk("JMD: SWD IDCODE is invalid (0x%x)\n", idcode);
		}

		swd_dp_write(devdata,
			     SWD_DP_REG_WO_ABORT,
			     SWD_VAL_ABORT_CLEAR);
		swd_dp_write(devdata,
			     SWD_DP_REG_WO_SELECT,
			     SWD_VAL_SELECT_MEMAP);

		ack = (1<<SWD_VAL_CTRLSTAT_CSYSPWRUPACK) |
			(1<<SWD_VAL_CTRLSTAT_CDBGPWRUPACK);
		req = (1<<SWD_VAL_CTRLSTAT_CSYSPWRUPREQ) |
			(1<<SWD_VAL_CTRLSTAT_CDBGPWRUPREQ);
		while ((swd_dp_read(devdata, SWD_DP_REG_RW_CTRLSTAT) & ack)
		       != ack) {
			swd_dp_write(devdata, SWD_DP_REG_RW_CTRLSTAT, req);
		}

		swd_dp_write(devdata,
			     SWD_DP_REG_WO_ABORT,
			     SWD_VAL_ABORT_CLEAR);
		swd_ap_write(devdata,
			     SWD_MEMAP_REG_RW_CSW,
			     SWD_VAL_DHCSR_NOINC_32);
	}

	return status;
}

void swd_halt(struct device *dev)
{
	swd_memory_write(dev,
			 SWD_REG_DHCSR,
			 SWD_VAL_DHCSR_DBGKEY | SWD_VAL_DHCSR_C_DEBUGEN);
	swd_memory_write(dev,
			 SWD_REG_DHCSR,
			 (SWD_VAL_DHCSR_DBGKEY |
			  SWD_VAL_DHCSR_C_DEBUGEN |
			  SWD_VAL_DHCSR_C_HALT));
}

void swd_reset(struct device *dev)
{
	swd_memory_write(dev,
			 SWD_REG_DHCSR,
			 (SWD_VAL_DHCSR_DBGKEY |
			  SWD_VAL_DHCSR_C_DEBUGEN |
			  SWD_VAL_DHCSR_C_HALT));
	swd_memory_write(dev,
			 SWD_REG_AIRCR,
			 (SWD_VAL_AIRCR_VECTKEY | SWD_VAL_AIRCR_SYSRSTRQ));
}
