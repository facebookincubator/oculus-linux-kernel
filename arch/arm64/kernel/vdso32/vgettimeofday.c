/*
 * Copyright 2015 Mentor Graphics Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/clocksource.h>
#include <linux/compiler.h>
#include <linux/time.h>
#include <asm/unistd.h>
#include <asm/vdso_datapage.h>

#include "aarch32-barrier.h"

/*
 * We use the hidden visibility to prevent the compiler from generating a GOT
 * relocation. Not only is going through a GOT useless (the entry couldn't and
 * musn't be overridden by another library), it does not even work: the linker
 * cannot generate an absolute address to the data page.
 *
 * With the hidden visibility, the compiler simply generates a PC-relative
 * relocation (R_ARM_REL32), and this is what we need.
 */
extern const struct vdso_data _vdso_data __attribute__((visibility("hidden")));

static inline const struct vdso_data *get_vdso_data(void)
{
	const struct vdso_data *ret;
	/*
	 * This simply puts &_vdso_data into ret. The reason why we don't use
	 * `ret = &_vdso_data` is that the compiler tends to optimise this in a
	 * very suboptimal way: instead of keeping &_vdso_data in a register,
	 * it goes through a relocation almost every time _vdso_data must be
	 * accessed (even in subfunctions). This is both time and space
	 * consuming: each relocation uses a word in the code section, and it
	 * has to be loaded at runtime.
	 *
	 * This trick hides the assignment from the compiler. Since it cannot
	 * track where the pointer comes from, it will only use one relocation
	 * where get_vdso_data() is called, and then keep the result in a
	 * register.
	 */
	asm("mov %0, %1" : "=r"(ret) : "r"(&_vdso_data));
	return ret;
}

static notrace u32 __vdso_read_begin(const struct vdso_data *vdata)
{
	u32 seq;
repeat:
	seq = ACCESS_ONCE(vdata->tb_seq_count);
	if (seq & 1) {
		cpu_relax();
		goto repeat;
	}
	return seq;
}

static notrace u32 vdso_read_begin(const struct vdso_data *vdata)
{
	u32 seq;

	seq = __vdso_read_begin(vdata);

	aarch32_smp_rmb();
	return seq;
}

static notrace int vdso_read_retry(const struct vdso_data *vdata, u32 start)
{
	aarch32_smp_rmb();
	return vdata->tb_seq_count != start;
}

/*
 * Note: only AEABI is supported by the compat layer, we can assume AEABI
 * syscall conventions are used.
 */
static notrace long clock_gettime_fallback(clockid_t _clkid,
					   struct timespec *_ts)
{
	register struct timespec *ts asm("r1") = _ts;
	register clockid_t clkid asm("r0") = _clkid;
	register long ret asm ("r0");
	register long nr asm("r7") = __NR_compat_clock_gettime;

	asm volatile(
	"	svc #0\n"
	: "=r" (ret)
	: "r" (clkid), "r" (ts), "r" (nr)
	: "memory");

	return ret;
}

static notrace int do_realtime_coarse(struct timespec *ts,
				      const struct vdso_data *vdata)
{
	u32 seq;

	do {
		seq = vdso_read_begin(vdata);

		ts->tv_sec = vdata->xtime_coarse_sec;
		ts->tv_nsec = vdata->xtime_coarse_nsec;

	} while (vdso_read_retry(vdata, seq));

	return 0;
}

static notrace int do_monotonic_coarse(struct timespec *ts,
				       const struct vdso_data *vdata)
{
	struct timespec tomono;
	u32 seq;

	do {
		seq = vdso_read_begin(vdata);

		ts->tv_sec = vdata->xtime_coarse_sec;
		ts->tv_nsec = vdata->xtime_coarse_nsec;

		tomono.tv_sec = vdata->wtm_clock_sec;
		tomono.tv_nsec = vdata->wtm_clock_nsec;

	} while (vdso_read_retry(vdata, seq));

	ts->tv_sec += tomono.tv_sec;
	timespec_add_ns(ts, tomono.tv_nsec);

	return 0;
}

static notrace u64 get_ns(const struct vdso_data *vdata)
{
	u64 cycle_delta;
	u64 cycle_now;
	u64 nsec;

	/* AArch32 implementation of arch_counter_get_cntvct() */
	isb();
	asm volatile("mrrc p15, 1, %Q0, %R0, c14" : "=r" (cycle_now));

	/* The virtual counter provides 56 significant bits. */
	cycle_delta = (cycle_now - vdata->cs_cycle_last) & CLOCKSOURCE_MASK(56);

	nsec = (cycle_delta * vdata->cs_mult) + vdata->xtime_clock_nsec;
	nsec >>= vdata->cs_shift;

	return nsec;
}

static notrace int do_realtime(struct timespec *ts,
			       const struct vdso_data *vdata)
{
	u64 nsecs;
	u32 seq;

	do {
		seq = vdso_read_begin(vdata);

		if (vdata->use_syscall)
			return -EINVAL;

		ts->tv_sec = vdata->xtime_clock_sec;
		nsecs = get_ns(vdata);

	} while (vdso_read_retry(vdata, seq));

	ts->tv_nsec = 0;
	timespec_add_ns(ts, nsecs);

	return 0;
}

static notrace int do_monotonic(struct timespec *ts,
				const struct vdso_data *vdata)
{
	struct timespec tomono;
	u64 nsecs;
	u32 seq;

	do {
		seq = vdso_read_begin(vdata);

		if (vdata->use_syscall)
			return -EINVAL;

		ts->tv_sec = vdata->xtime_clock_sec;
		nsecs = get_ns(vdata);

		tomono.tv_sec = vdata->wtm_clock_sec;
		tomono.tv_nsec = vdata->wtm_clock_nsec;

	} while (vdso_read_retry(vdata, seq));

	ts->tv_sec += tomono.tv_sec;
	ts->tv_nsec = 0;
	timespec_add_ns(ts, nsecs + tomono.tv_nsec);

	return 0;
}

notrace int __vdso_clock_gettime(clockid_t clkid, struct timespec *ts)
{
	const struct vdso_data *vdata = get_vdso_data();
	int ret = -EINVAL;

	switch (clkid) {
	case CLOCK_REALTIME_COARSE:
		ret = do_realtime_coarse(ts, vdata);
		break;
	case CLOCK_MONOTONIC_COARSE:
		ret = do_monotonic_coarse(ts, vdata);
		break;
#ifdef CONFIG_ARM_ARCH_TIMER_VCT_ACCESS
	/*
	 * The following calls require userspace access to the virtual timer
	 * which is not normally granted on arm64. Without this configuration
	 * flag the calls will get trapped and an illegal operation exception
	 * will get thrown.
	 */
	case CLOCK_REALTIME:
		ret = do_realtime(ts, vdata);
		break;
	case CLOCK_MONOTONIC:
		ret = do_monotonic(ts, vdata);
		break;
#endif
	default:
		break;
	}

	if (ret)
		ret = clock_gettime_fallback(clkid, ts);

	return ret;
}

static notrace long gettimeofday_fallback(struct timeval *_tv,
					  struct timezone *_tz)
{
	register struct timezone *tz asm("r1") = _tz;
	register struct timeval *tv asm("r0") = _tv;
	register long ret asm ("r0");
	register long nr asm("r7") = __NR_compat_gettimeofday;

	asm volatile(
	"	svc #0\n"
	: "=r" (ret)
	: "r" (tv), "r" (tz), "r" (nr)
	: "memory");

	return ret;
}

notrace int __vdso_gettimeofday(struct timeval *tv, struct timezone *tz)
{
	struct timespec ts;
	const struct vdso_data *vdata = get_vdso_data();
	int ret;

	ret = do_realtime(&ts, vdata);
	if (ret)
		return gettimeofday_fallback(tv, tz);

	if (tv) {
		tv->tv_sec = ts.tv_sec;
		tv->tv_usec = ts.tv_nsec / 1000;
	}
	if (tz) {
		tz->tz_minuteswest = vdata->tz_minuteswest;
		tz->tz_dsttime = vdata->tz_dsttime;
	}

	return ret;
}

/* Avoid unresolved references emitted by GCC */

void __aeabi_unwind_cpp_pr0(void)
{
}

void __aeabi_unwind_cpp_pr1(void)
{
}

void __aeabi_unwind_cpp_pr2(void)
{
}
