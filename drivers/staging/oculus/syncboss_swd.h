#ifndef _SYNCBOSS_SWD_H
#define _SYNCBOSS_SWD_H

#include <linux/types.h>

struct swdhandle_t
{
	int gpio_swclk;
	int gpio_swdio;
	int direction;
};

// Initialize the SWD handle
void swd_init(struct swdhandle_t* handle, int gpio_swclk, int gpio_swdio);

// Deinit the SWD handle
void swd_deinit(struct swdhandle_t* handle);

// Stop the target
void swd_halt(struct swdhandle_t* handle);

// Read 4 bytes of flash memory from a given address
u32 swd_memory_read(struct swdhandle_t* handle, u32 address);

// Write 4 bytes of flash memory at a given address
void swd_memory_write(struct swdhandle_t* handle, u32 address, u32 data);

#endif
