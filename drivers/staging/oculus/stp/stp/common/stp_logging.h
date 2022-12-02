#ifndef STL_LOGGING_H
#define STL_LOGGING_H

#include <linux/string.h>
#include <linux/printk.h>

#if !defined(RELEASE_BUILD)
#define STP_LOG_ERROR pr_err
#define STP_LOG_INFO pr_info
#define STP_LOG_DEBUG pr_debug

#define STP_LOG_ERROR_RATE_LIMIT(fmt, ...) printk_ratelimited(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define STP_LOG_INFO_RATE_LIMIT(fmt, ...) printk_ratelimited(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#define STP_LOG_DEBUG_RATE_LIMIT(fmt, ...) printk_ratelimited(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)

#else
#define STP_DONT_LOG(...)
#define STP_LOG_ERROR pr_err
#define STP_LOG_INFO STP_DONT_LOG
#define STP_LOG_DEBUG STP_DONT_LOG

#define STP_LOG_ERROR_RATE_LIMIT(fmt, ...) printk_ratelimited(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define STP_LOG_INFO_RATE_LIMIT STP_DONT_LOG
#define STP_LOG_DEBUG_RATE_LIMIT STP_DONT_LOG
#endif

#define STP_LOG_RX_DATA(buffer) pr_info("[STP] RX_DATA: %s\n", buffer)
#define STP_LOG_TX_DATA(buffer) pr_info("[STP] TX_DATA: %s\n", buffer)

#endif
