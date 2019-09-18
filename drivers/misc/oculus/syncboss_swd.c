#include "syncboss_swd.h"
#include <linux/gpio.h>
#include <linux/printk.h>

////////////////////////////////////////////////////////////////
// Note: Most of this code was taken, with minimal changes, from the
//       CV1 HMD_Main firmware
////////////////////////////////////////////////////////////////

#define SYNCBOSS_SWD_DIRECTION_OUT 1
#define SYNCBOSS_SWD_DIRECTION_IN  0

#define SYNCBOSS_SWD_START   1
#define SYNCBOSS_SWD_DP      0
#define SYNCBOSS_SWD_AP      1
#define SYNCBOSS_SWD_WRITE   0
#define SYNCBOSS_SWD_READ    1
#define SYNCBOSS_SWD_STOP    0
#define SYNCBOSS_SWD_PARK    1
#define SYNCBOSS_SWD_IDLE    0

#define SYNCBOSS_SWD_FALLING 0
#define SYNCBOSS_SWD_RISING  1

#define SYNCBOSS_SWD_OK    0b001
#define SYNCBOSS_SWD_WAIT  0b010
#define SYNCBOSS_SWD_FAULT 0b100

// DP registers
#define SYNCBOSS_SWD_IDCODE   0x0
#define SYNCBOSS_SWD_ABORT    0x0
#define SYNCBOSS_SWD_CTRLSTAT 0x4
#define SYNCBOSS_SWD_RESEND   0x8
#define SYNCBOSS_SWD_SELECT   0x8
#define SYNCBOSS_SWD_RDBUFF   0xC

// ABORT
#define SYNCBOSS_SWD_ABORT_CLEAR 0x1E

// CTRLSTAT
#define SYNCBOSS_SWD_CSYSPWRUPACK 31
#define SYNCBOSS_SWD_CSYSPWRUPREQ 30
#define SYNCBOSS_SWD_CDBGPWRUPACK 29
#define SYNCBOSS_SWD_CDBGPWRUPREQ 28

// SELECT
#define SYNCBOSS_SWD_SELECT_MEMAP 0

// MEM-AP
#define SYNCBOSS_SWD_MEMAP_CSW 0x0
#define SYNCBOSS_SWD_MEMAP_TAR 0x4
#define SYNCBOSS_SWD_MEMAP_DRW 0xC

// Core
#define SYNCBOSS_SWD_DHCSR          0xE000EDF0
#define SYNCBOSS_SWD_DEBUG_KEY      0xa05f0000
#define SYNCBOSS_SWD_DEBUG_STOP     2
#define SYNCBOSS_SWD_DEBUG_START    0
#define SYNCBOSS_SWD_DEBUG_OFF      0
#define SYNCBOSS_SWD_DEBUG_ON       1
#define SYNCBOSS_SWD_DEBUG_NOINC_32 0x23000002

#define SYNCBOSS_SWD_NRF_POWER           0x40000544

void swd_wire_write_len(struct swdhandle_t* handle, bool value, int len);
bool swd_mode_switch(struct swdhandle_t* handle);

void swd_init(struct swdhandle_t* handle, int gpio_swclk, int gpio_swdio)
{
	handle->gpio_swclk = gpio_swclk;
	handle->gpio_swdio = gpio_swdio;
	handle->direction = SYNCBOSS_SWD_DIRECTION_OUT;

	// set swd lines to output and drive both low
	gpio_direction_output(handle->gpio_swclk, 0);
	gpio_direction_output(handle->gpio_swdio, 0);

	swd_wire_write_len(handle, 1, 200); // must be 150+ clocks @125kHz+
	swd_mode_switch(handle);
}

void swd_deinit(struct swdhandle_t* handle)
{
	// float swd lines
	gpio_direction_input(handle->gpio_swclk);
	gpio_direction_input(handle->gpio_swdio);
}

static void swd_wire_clock(struct swdhandle_t* handle)
{
	gpio_set_value(handle->gpio_swclk, SYNCBOSS_SWD_RISING);
	gpio_set_value(handle->gpio_swclk, SYNCBOSS_SWD_FALLING);
}

static inline void swd_wire_set_direction(struct swdhandle_t* handle, int direction)
{
	if (handle->direction != direction) {
		if (direction == SYNCBOSS_SWD_DIRECTION_OUT) {
			gpio_direction_output(handle->gpio_swdio, 0);
		} else {
			gpio_direction_input(handle->gpio_swdio);
		}
		handle->direction = direction;
	}
}

void swd_wire_write(struct swdhandle_t* handle, bool value)
{
	swd_wire_set_direction(handle, SYNCBOSS_SWD_DIRECTION_OUT);
	gpio_set_value(handle->gpio_swdio, value);
	swd_wire_clock(handle);
}

void swd_wire_write_len(struct swdhandle_t* handle, bool value, int len)
{
	int i = 0;
	swd_wire_set_direction(handle, SYNCBOSS_SWD_DIRECTION_OUT);
	gpio_set_value(handle->gpio_swdio, value);
	for (i = 0; i < len; i++) {
		swd_wire_clock(handle);
	}
}

bool swd_wire_read(struct swdhandle_t* handle)
{
	bool value = false;
	swd_wire_set_direction(handle, SYNCBOSS_SWD_DIRECTION_IN);
	value = gpio_get_value(handle->gpio_swdio);
	swd_wire_clock(handle);
	return value;
}

static void swd_wire_turnaround(struct swdhandle_t* handle)
{
	swd_wire_read(handle);
}

static bool swd_wire_header(struct swdhandle_t* handle, bool read, bool apndp, u8 reg)
{
	u8 ack = 0;
	bool address2 = !!(reg & (1<<2));
	bool address3 = !!(reg & (1<<3));
	u8 bitcount = (apndp + read + address2 + address3);
	bool parity = bitcount & 1;

	while (true) {
		swd_wire_write(handle, SYNCBOSS_SWD_START);
		swd_wire_write(handle, apndp);
		swd_wire_write(handle, read);
		swd_wire_write(handle, address2);
		swd_wire_write(handle, address3);
		swd_wire_write(handle, parity);
		swd_wire_write(handle, SYNCBOSS_SWD_STOP);
		swd_wire_write(handle, SYNCBOSS_SWD_PARK);

		swd_wire_turnaround(handle);

		ack = 0;
		ack |= swd_wire_read(handle) << 0;
		ack |= swd_wire_read(handle) << 1;
		ack |= swd_wire_read(handle) << 2;

		if (ack == SYNCBOSS_SWD_OK) {
			return true;
		}

		swd_wire_turnaround(handle);

		if (ack == SYNCBOSS_SWD_WAIT) {
			continue;
		}

		if (ack == SYNCBOSS_SWD_FAULT) {
			// TODO: Better error handling
			printk("JMD: Bus fault condition\n");
			//assert("SWD bus fault condition", false);
		}

		return false;
	}
}

static void swd_dpap_write(struct swdhandle_t* handle, bool apndp, u8 reg, u32 data)
{
	int bitcount = 0;
	int i = 0;
	bool parity;

	if (!swd_wire_header(handle, SYNCBOSS_SWD_WRITE, apndp, reg)) {
		// TODO: Better error handling
		printk("JMD: SWD response is invalid/unknown\n");
		return;
	}
	swd_wire_turnaround(handle);

	for (i = 0; i < 32; i++) {
		bool value = !!(data & (1<<i));
		bitcount += value;
		swd_wire_write(handle, value);
	}
	parity = bitcount & 1;
	swd_wire_write(handle, parity);
	swd_wire_write(handle, 0);
}

static u32 swd_dpap_read(struct swdhandle_t* handle, bool apndp, u8 reg)
{
	u32 data = 0;
	int bitcount = 0;
	int i = 0;
	bool parity, check;

	if (!swd_wire_header(handle, SYNCBOSS_SWD_READ, apndp, reg)) {
		// TODO: Better error handling
		printk("JMD: SWD response is invalid/unknown\n");
		return 0;
	}

	for (i = 0; i < 32; i++) {
		bool value = swd_wire_read(handle);
		bitcount += value;
		data |= (value<<i);
	}
	parity = bitcount & 1;
	check = swd_wire_read(handle);
	if (check != parity) {
		// TODO: Better error handling
		printk("JMD: Parity error\n");
		return 0;
	}
	swd_wire_turnaround(handle);
	swd_wire_write(handle, 0);

	return data;
}

void swd_ap_write(struct swdhandle_t* handle, u8 reg, u32 data)
{
	swd_dpap_write(handle, SYNCBOSS_SWD_AP, reg, data);
}

u32 swd_ap_read(struct swdhandle_t* handle, u8 reg)
{
	return swd_dpap_read(handle, SYNCBOSS_SWD_AP, reg);
}

void swd_dp_write(struct swdhandle_t* handle, u8 reg, u32 data)
{
	swd_dpap_write(handle, SYNCBOSS_SWD_DP, reg, data);
}

u32 swd_dp_read(struct swdhandle_t* handle, u8 reg)
{
	return swd_dpap_read(handle, SYNCBOSS_SWD_DP, reg);
}

void swd_memory_write(struct swdhandle_t* handle, u32 address, u32 data)
{
	swd_ap_write(handle, SYNCBOSS_SWD_MEMAP_TAR, address);
	swd_ap_write(handle, SYNCBOSS_SWD_MEMAP_DRW, data);
}

u32 swd_memory_read(struct swdhandle_t* handle, u32 address)
{
	swd_ap_write(handle, SYNCBOSS_SWD_MEMAP_TAR, address);
	swd_ap_read(handle, SYNCBOSS_SWD_MEMAP_DRW);
	return swd_dp_read(handle, SYNCBOSS_SWD_RDBUFF);
}

bool swd_mode_switch(struct swdhandle_t* handle)
{
	int i = 0;
	bool status = true;
	const u16 jtag_to_swd = 0xE79E;
	u32 idcode, ack, req;

	// JTAG to SWD
	swd_wire_write_len(handle, 1, 100);
	for (i = 0; i < 16; i++) {
		bool value = !!(jtag_to_swd & (1<<i));
		swd_wire_write(handle, value);
	}
	swd_wire_write_len(handle, 1, 100);
	swd_wire_write_len(handle, 0, 20);

	// IDCODE
	status = swd_wire_header(handle, SYNCBOSS_SWD_READ, SYNCBOSS_SWD_DP, SYNCBOSS_SWD_IDCODE);
	for (i = 0; i < 34; i++) {
		swd_wire_read(handle);
	}

	if (status) {
		idcode = swd_dp_read(handle, 0);
		if (!(idcode == 0x0bb11477 || idcode == 0x0bc11477 || idcode == 0x2ba01477)) {
			// TODO: Better error handling
			printk("JMD: SWD IDCODE is invalid (0x%x)\n", idcode);
		}

		swd_dp_write(handle, SYNCBOSS_SWD_ABORT, SYNCBOSS_SWD_ABORT_CLEAR);
		swd_dp_write(handle, SYNCBOSS_SWD_SELECT, SYNCBOSS_SWD_SELECT_MEMAP);

		ack = (1<<SYNCBOSS_SWD_CSYSPWRUPACK) | (1<<SYNCBOSS_SWD_CDBGPWRUPACK);
		req = (1<<SYNCBOSS_SWD_CSYSPWRUPREQ) | (1<<SYNCBOSS_SWD_CDBGPWRUPREQ);
		while ((swd_dp_read(handle, SYNCBOSS_SWD_CTRLSTAT) & ack) != ack) {
			swd_dp_write(handle, SYNCBOSS_SWD_CTRLSTAT, req);
		}

		swd_dp_write(handle, SYNCBOSS_SWD_ABORT, SYNCBOSS_SWD_ABORT_CLEAR);
		swd_ap_write(handle, SYNCBOSS_SWD_MEMAP_CSW, SYNCBOSS_SWD_DEBUG_NOINC_32);
	}

	return status;
}

void swd_halt(struct swdhandle_t* handle)
{
	swd_memory_write(handle, SYNCBOSS_SWD_DHCSR, SYNCBOSS_SWD_DEBUG_KEY | SYNCBOSS_SWD_DEBUG_ON);
	swd_memory_write(handle, SYNCBOSS_SWD_DHCSR, SYNCBOSS_SWD_DEBUG_KEY | SYNCBOSS_SWD_DEBUG_ON  | SYNCBOSS_SWD_DEBUG_STOP);
}