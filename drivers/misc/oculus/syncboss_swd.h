#ifndef _SYNCBOSS_SWD_H
#define _SYNCBOSS_SWD_H

#include <linux/types.h>

#define SYNCBOSS_SWD_NRF_NVMC_READY      0x4001E400
#define SYNCBOSS_SWD_NRF_NVMC_CONFIG     0x4001E504
#define SYNCBOSS_SWD_NRF_NVMC_ERASEPAGE  0x4001E508
#define SYNCBOSS_SWD_NRF_NVMC_ERASEALL   0x4001E50C

#define SYNCBOSS_SWD_NVMC_CONFIG_REN     0
#define SYNCBOSS_SWD_NVMC_CONFIG_WEN     (1<<0)
#define SYNCBOSS_SWD_NVMC_CONFIG_EEN     (1<<1)

#define SYNCBOSS_SWD_NVMC_READY          (1<<0)
#define SYNCBOSS_SWD_NVMC_READY_Busy     (0<<0)
#define SYNCBOSS_SWD_NVMC_READY_Ready    (1<<0)

#define SYNCBOSS_SWD_NVMC_ERASEALL_NOP   0
#define SYNCBOSS_SWD_NVMC_ERASEALL_ERASE 1

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
