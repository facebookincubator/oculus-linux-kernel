#ifndef STL_LOGGING_H
#define STL_LOGGING_H

#include <linux/printk.h>
#include <linux/string.h>

#if !defined(RELEASE_BUILD)
#define STP_LOG_ERROR(m, ...) pr_err("[STPLib] (%s:%d) " m "\n", __func__, __LINE__, ##__VA_ARGS__)
#define STP_LOG_INFO(m, ...) pr_info("[STPLib] (%s:%d) " m "\n", __func__, __LINE__, ##__VA_ARGS__)
#define STP_LOG_DEBUG(m, ...)                                                                      \
    pr_debug("[STPLib] (%s:%d) " m "\n", __func__, __LINE__, ##__VA_ARGS__)

#define STP_LOG_ERROR_RATE_LIMIT(m, ...)                                                           \
    pr_err_ratelimited("[STPLib] (%s:%d) " m "\n", __func__, __LINE__, ##__VA_ARGS__)
#define STP_LOG_INFO_RATE_LIMIT(m, ...)                                                            \
    pr_info_ratelimited("[STPLib] (%s:%d) " m "\n", __func__, __LINE__, ##__VA_ARGS__)
#define STP_LOG_DEBUG_RATE_LIMIT(m, ...)                                                           \
    pr_debug_ratelimited("[STPLib] (%s:%d) " m "\n", __func__, __LINE__, ##__VA_ARGS__)

#else
#define STP_DONT_LOG(...)
#define STP_LOG_ERROR(m, ...) pr_err("[STPLib] (%s:%d) " m "\n", __func__, __LINE__, ##__VA_ARGS__)
#define STP_LOG_INFO STP_DONT_LOG
#define STP_LOG_DEBUG STP_DONT_LOG

#define STP_LOG_ERROR_RATE_LIMIT(m, ...)                                                           \
    pr_err_ratelimited("[STPLib] (%s:%d) " m "\n", __func__, __LINE__, ##__VA_ARGS__)
#define STP_LOG_INFO_RATE_LIMIT STP_DONT_LOG
#define STP_LOG_DEBUG_RATE_LIMIT STP_DONT_LOG
#endif

#define STP_LOG_RX_DATA(buffer) pr_info("[STP] RX_DATA: %s\n", buffer)
#define STP_LOG_TX_DATA(buffer) pr_info("[STP] TX_DATA: %s\n", buffer)

#endif
