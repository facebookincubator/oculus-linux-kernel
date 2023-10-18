#ifndef STP_LINUX_H
#define STP_LINUX_H

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/crc-ccitt.h>

/**
 * Mutex.
 */
#define STP_LOCK_TYPE struct mutex
#define STP_LOCK_INIT(m) mutex_init(&(m))
#define STP_LOCK_DEINIT(m)
#define STP_LOCK(m) mutex_lock(&(m))
#define STP_UNLOCK(m) mutex_unlock(&(m))

/**
 * Sleeps current thread for a specified duration.
 * @param m Duration in milliseconds.
 */
#define STP_MSLEEP(m) msleep_interruptible(m)

/**
 * Asserts that a condition is true and panics otherwise.
 * @param c Condition.
 * @param m Panic message.
 */
#define STP_ASSERT(c, m)                                                                           \
    do                                                                                             \
    {                                                                                              \
        if (likely(c))                                                                             \
            break;                                                                                 \
        pr_err("STP Assert " m);                                                                   \
        BUG();                                                                                     \
    } while (0)

#endif

/**
 * Compute CRC
 * @param buf buffer
 * @param len buffer length
 * @return CRC
 */
#define STP_CRC_COMPUTE(buf, len) crc_ccitt(0xFFFF, buf, len)
