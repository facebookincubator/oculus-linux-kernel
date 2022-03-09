#ifndef __BACKPORT_TIMEKEEPING_H
#define __BACKPORT_TIMEKEEPING_H
#include <linux/version.h>
#include <linux/types.h>

#if LINUX_VERSION_IS_GEQ(3,17,0)
#include_next <linux/timekeeping.h>
#endif

#if LINUX_VERSION_IS_LESS(3,17,0)
#define ktime_get_ns LINUX_BACKPORT(ktime_get_ns)
extern ktime_t ktime_get(void);
#define ktime_get_ns LINUX_BACKPORT(ktime_get_ns)
static inline u64 ktime_get_ns(void)
{
	return ktime_to_ns(ktime_get());
}

extern ktime_t ktime_get_boottime(void);
#define ktime_get_boot_ns LINUX_BACKPORT(ktime_get_boot_ns)
static inline u64 ktime_get_boot_ns(void)
{
	return ktime_to_ns(ktime_get_boottime());
}
#endif /* < 3.17 */

#if LINUX_VERSION_IS_LESS(3,19,0)
static inline time64_t ktime_get_seconds(void)
{
	struct timespec t;

	ktime_get_ts(&t);

	return t.tv_sec;
}

static inline time64_t ktime_get_real_seconds(void)
{
	struct timeval tv;

	do_gettimeofday(&tv);

	return tv.tv_sec;
}
#endif

#if LINUX_VERSION_IS_LESS(3,17,0)
static inline void ktime_get_ts64(struct timespec64 *ts)
{
	ktime_get_ts(ts);
}
#endif

#endif /* __BACKPORT_TIMEKEEPING_H */
