#ifndef _STP_DEVICE_LOGGING_H_
#define _STP_DEVICE_LOGGING_H_

#include <linux/kernel.h>

// Enables all opt-in STP Device logs
#define STP_DRV_VERBOSE_LOGS 0

// Logs all STP->Linux error conversions
#define STP_DEBUG_ALL_ERR_CODES 1

// Prints the RX and TX headers at the SPI transaction
#define STP_DEBUG_MSG_HEADERS 0

#if defined(STP_DRV_VERBOSE_LOGS) && (STP_DRV_VERBOSE_LOGS == 1)

#undef STP_DEBUG_ALL_ERR_CODES
#define STP_DEBUG_ALL_ERR_CODES 1

#undef STP_DEBUG_MSG_HEADERS
#define STP_DEBUG_MSG_HEADERS 1
#endif

#define STP_DRV_LOG_ERR(m, ...)                                                \
	pr_err("[STP] (%s:%d) " m "\n", __func__, __LINE__, ##__VA_ARGS__)

#define STP_DRV_LOG_INFO(m, ...)                                               \
	pr_info("[STP] (%s:%d) " m "\n", __func__, __LINE__, ##__VA_ARGS__)

#define STP_DEV_LOG_ERR(d, m, ...)                                             \
	dev_err(d, "[STP] (%s:%d) " m "\n", __func__, __LINE__, ##__VA_ARGS__)

#define STP_DEBUG_BUFFER_LEN 256

#endif
