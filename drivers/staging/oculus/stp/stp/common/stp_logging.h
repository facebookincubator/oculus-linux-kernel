#ifndef STL_LOGGING_H
#define STL_LOGGING_H

#include <linux/string.h>
#include <linux/printk.h>

#if !defined(RELEASE_BUILD)
#define STP_LOG_ERROR pr_err
#define STP_LOG_INFO pr_info
#define STP_LOG_DEBUG pr_debug
#else
#define STP_DONT_LOG(...)
#define STP_LOG_ERROR pr_err
#define STP_LOG_INFO STP_DONT_LOG
#define STP_LOG_DEBUG STP_DONT_LOG
#endif

#define STP_LOG_RX_DATA(buffer) pr_info("[STP] RX_DATA: %s\n", buffer)
#define STP_LOG_TX_DATA(buffer) pr_info("[STP] TX_DATA: %s\n", buffer)

#endif
