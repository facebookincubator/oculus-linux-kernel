// SPDX-License-Identifier: GPL-2.0-only
/**
 * Driver to expose MPM sleep_clock counter as a POSIX clock
 * For reference, see drivers/soc/qcom/boot_stats.c
 */

#include <linux/alarmtimer.h>
#include <linux/hrtimer.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/posix-clock.h>
#include <linux/slab.h>
#include <linux/suspend.h>

#define TIMER_TICKS_PER_SEC 32768
#define US_PER_SEC (1000000)
#define NS_PER_SEC (1000000000)
#define NS_PER_10SEC (10LL * NS_PER_SEC)

/* refresh once an hour, or every half roll-over period when suspended */
#define HALF_ROLLOVER_PERIOD (uint32_t)(0x100000000LL / TIMER_TICKS_PER_SEC / 2)

/* for debug, use shorter refresh intervals */
#define MPM_SLEEP_CLOCK_DEBUG 0

#if (MPM_SLEEP_CLOCK_DEBUG)
#define REFR_INTERVAL ktime_set(10, 0)
#define REFR_INTERVAL_SUSPEND ktime_set(30, 0)
#else
#define REFR_INTERVAL ktime_set(60 * 60, 0)
#define REFR_INTERVAL_SUSPEND ktime_set(HALF_ROLLOVER_PERIOD, 0)
#endif

struct sleep_clk_data {
	struct platform_device *pdev; /* platform device pointer */
	struct device cdev; /* character device for posix clock */
	struct posix_clock clock; /* posix clock registration */
	dev_t cdevid; /* character device ID */
	struct hrtimer hr_timer; /* periodic refresh timer */
	struct alarm alarm_timer; /* refresh timer when soc suspended */
	spinlock_t lock; /* structure spinlock */
	uint64_t sclk_rollover; /* rollover count */
	uint32_t sclk_ticks_prev; /* previous 32-bit tick count */
	struct notifier_block pm_nb; /* PM notifier block */
	void __iomem *mpm_counter_base; /* MPM counter base pointer */
};

/* read the raw counter value in ticks */
static uint64_t sleep_clock_ticks64(struct sleep_clk_data *sleep_clk)
{
	uint32_t ticks;
	uint64_t total_ticks;
	void __iomem *sclk_tick;

	if (!sleep_clk)
		return 0;

	sclk_tick = sleep_clk->mpm_counter_base;

	if (!sclk_tick)
		return 0;

	spin_lock(&sleep_clk->lock);
	ticks = __raw_readl(sclk_tick);
	if (ticks < sleep_clk->sclk_ticks_prev) {
		pr_info("sleep_clock: rollover: prev=%u cur=%u\n",
			sleep_clk->sclk_ticks_prev, ticks);
		sleep_clk->sclk_rollover += 0x100000000LL;
	}

	sleep_clk->sclk_ticks_prev = ticks;
	total_ticks = ticks + sleep_clk->sclk_rollover;
	spin_unlock(&sleep_clk->lock);

	return total_ticks;
}

/* read the timer and convert to uS - internal use only */
static uint64_t sleep_clock_us(struct sleep_clk_data *sleep_clk)
{
	uint64_t ticks;

	ticks = sleep_clock_ticks64(sleep_clk);

	return div_u64(ticks, TIMER_TICKS_PER_SEC) * US_PER_SEC;
}

static int sleep_clock_getres(struct posix_clock *pc, struct timespec64 *tp)
{
	tp->tv_sec = 0;
	tp->tv_nsec = NS_PER_SEC / TIMER_TICKS_PER_SEC;
	return 0;
}

static int sleep_clock_settime(struct posix_clock *pc,
			       const struct timespec64 *tp)
{
	return -EOPNOTSUPP;
}

static int sleep_clock_gettime(struct posix_clock *__always_unused pc,
			       struct timespec64 *tp)
{
	uint64_t ticks;
	uint32_t rem_ticks;

	struct sleep_clk_data *sleep_clk =
		container_of(pc, struct sleep_clk_data, clock);
	if (!sleep_clk)
		return -EINVAL;

	ticks = sleep_clock_ticks64(sleep_clk);

	if (tp) {
		tp->tv_sec =
			div_u64_rem(ticks, TIMER_TICKS_PER_SEC, &rem_ticks);
		tp->tv_nsec = (uint32_t)div_u64(
			rem_ticks * (uint64_t)NS_PER_SEC, TIMER_TICKS_PER_SEC);
	}

	return 0;
}

static int sleep_clock_adjtime(struct posix_clock *pc,
			       struct __kernel_timex *tx)
{
	return -EOPNOTSUPP;
}

static void sleep_clock_release(struct device *dev)
{
}

static struct posix_clock_operations sleep_clock_ops = {
	.owner = THIS_MODULE,
	.clock_adjtime = sleep_clock_adjtime,
	.clock_gettime = sleep_clock_gettime,
	.clock_getres = sleep_clock_getres,
	.clock_settime = sleep_clock_settime,
	.ioctl = NULL,
	.open = NULL,
	.poll = NULL,
	.read = NULL
};

static enum hrtimer_restart hr_timer_handler(struct hrtimer *timer)
{
	uint64_t ts;
	struct sleep_clk_data *sleep_clk =
		container_of(timer, struct sleep_clk_data, hr_timer);

	// refresh the clock and restart the timer
	ts = sleep_clock_us(sleep_clk);
	pr_info("sleep_clock: @ %lld: hr_timer", ts);
	hrtimer_forward_now(timer, REFR_INTERVAL);
	return HRTIMER_RESTART;
}

enum alarmtimer_restart alarm_timer_handler(struct alarm *alarm, ktime_t t)
{
	uint64_t ts;
	struct sleep_clk_data *sleep_clk =
		container_of(alarm, struct sleep_clk_data, alarm_timer);

	// refresh the clock and restart the timer
	ts = sleep_clock_us(sleep_clk);
	pr_info("sleep_clock: @ %lld: alarm_timer", ts);
	alarm_forward_now(alarm, REFR_INTERVAL_SUSPEND);
	return ALARMTIMER_RESTART;
}

static int sleep_clock_pm_notifier(struct notifier_block *nb,
				   unsigned long action, void *data)
{
	uint64_t ts;
	struct sleep_clk_data *sleep_clk =
		container_of(nb, struct sleep_clk_data, pm_nb);
	if (!sleep_clk)
		return NOTIFY_BAD;

	/* refresh the clock */
	ts = sleep_clock_us(sleep_clk);

	/* on suspend, schedule the wake-up alarm for maximum delay */
	/* on resume, cancel the alarm - hrtimer refresh takes over */
	switch (action) {
	case PM_SUSPEND_PREPARE:
		pr_info("sleep_clock: @ %lld: pm_suspend", ts);
		alarm_start_relative(&sleep_clk->alarm_timer,
				     REFR_INTERVAL_SUSPEND);
		break;
	case PM_POST_SUSPEND:
		pr_info("sleep_clock: @ %lld: post_suspend", ts);
		alarm_try_to_cancel(&sleep_clk->alarm_timer);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static int sleep_clock_probe(struct platform_device *pdev)
{
	struct sleep_clk_data *sleep_clk;
	int rc = 0;
	int index = 0, major;
	dev_t sleep_clock_devt;
	struct class *sleep_clock_class;
	struct resource *res;

	pr_info("sleep_clock: probe");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		pr_err("sleep_clock: can't get IO resources\n");
		return -ENODEV;
	}

	sleep_clk = devm_kzalloc(&pdev->dev, sizeof(struct sleep_clk_data),
				 GFP_KERNEL);
	if (!sleep_clk)
		return -ENOMEM;

	/* get base pointer to the counter */
	sleep_clk->mpm_counter_base =
		devm_ioremap(&pdev->dev, res->start, resource_size(res));

	if (!sleep_clk->mpm_counter_base) {
		pr_err("sleep_clock: cant map counter base\n");
		rc = -ENODEV;
		goto no_remap;
	}

	spin_lock_init(&sleep_clk->lock);

	sleep_clock_class = class_create(THIS_MODULE, "sleep_clk");
	if (IS_ERR(sleep_clock_class)) {
		pr_err("sleep_clock: failed to allocate class\n");
		rc = -ENOMEM;
		goto no_class;
	}

	rc = alloc_chrdev_region(&sleep_clock_devt, 0, 1, "sleep_clk");
	if (rc < 0) {
		pr_err("sleep_clock: failed to allocate cdev region\n");
		goto no_region;
	}

	major = MAJOR(sleep_clock_devt);

	sleep_clk->pdev = pdev;
	sleep_clk->clock.ops = sleep_clock_ops;
	sleep_clk->cdevid = MKDEV(major, index);

	/* Initialize a new character device for the clock */
	device_initialize(&sleep_clk->cdev);
	sleep_clk->cdev.devt = sleep_clk->cdevid;
	sleep_clk->cdev.class = sleep_clock_class;
	sleep_clk->cdev.parent = NULL;
	sleep_clk->cdev.groups = NULL;
	sleep_clk->cdev.release = sleep_clock_release;
	dev_set_drvdata(&sleep_clk->cdev, &sleep_clk);
	dev_set_name(&sleep_clk->cdev, "sleep_clk%d", 0);

	/* save data to the platform driver */
	platform_set_drvdata(pdev, sleep_clk);

	/* create a posix clock and link it to the device. */
	rc = posix_clock_register(&sleep_clk->clock, &sleep_clk->cdev);
	if (rc) {
		pr_err("sleep_clock: failed to create posix clock\n");
		goto no_clock;
	}

	/* start a periodic refresh timer  to prevent rollovers */
	hrtimer_init(&sleep_clk->hr_timer, CLOCK_BOOTTIME, HRTIMER_MODE_REL);
	sleep_clk->hr_timer.function = &hr_timer_handler;
	hrtimer_start(&sleep_clk->hr_timer, REFR_INTERVAL, HRTIMER_MODE_REL);

	/* initialize a suspend refresh timer */
	alarm_init(&sleep_clk->alarm_timer, ALARM_REALTIME,
		   alarm_timer_handler);

	/* register suspend/resume notifier */
	sleep_clk->pm_nb.notifier_call = sleep_clock_pm_notifier;
	rc = register_pm_notifier(&sleep_clk->pm_nb);
	if (rc) {
		pr_err("sleep_clock: failed to register PM notifier\n");
	}

	pr_info("sleep_clock: probe success, refr_susp=%d sec\n",
		(uint32_t)(REFR_INTERVAL_SUSPEND / 1E9L));

	return 0;

no_clock:
	unregister_chrdev_region(MAJOR(sleep_clk->cdevid), 1);
no_region:
	class_destroy(sleep_clk->cdev.class);
no_class:
no_remap:
	devm_kfree(&pdev->dev, sleep_clk);

	return rc;
}

static int sleep_clock_remove(struct platform_device *pdev)
{
	struct sleep_clk_data *sleep_clk = platform_get_drvdata(pdev);
	if (!sleep_clk)
		return -ENODEV;

	unregister_pm_notifier(&sleep_clk->pm_nb);
	alarm_cancel(&sleep_clk->alarm_timer);
	hrtimer_cancel(&sleep_clk->hr_timer);
	posix_clock_unregister(&sleep_clk->clock);

	unregister_chrdev_region(MAJOR(sleep_clk->cdevid), 1);
	class_destroy(sleep_clk->cdev.class);
	devm_kfree(&pdev->dev, sleep_clk);

	pr_info("sleep_clock: removed");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sleep_clock_of_match[] = {
	{ .compatible = "qcom,mpm2-sleep-counter" },
	{},
};
MODULE_DEVICE_TABLE(of, sleep_clock_of_match);
#endif

static struct platform_driver sleep_clock_driver = {
	.driver = {
		.name = "mpm_sleep_clock",
		.of_match_table = of_match_ptr(sleep_clock_of_match),
	},
	.probe = sleep_clock_probe,
	.remove = sleep_clock_remove
};

static int __init sleep_clock_driver_init(void)
{
	int rc = 0;

	rc = platform_driver_register(&sleep_clock_driver);
	if (rc) {
		pr_err("sleep_clock: unable to register driver: %d\n", rc);
		return rc;
	}

	return rc;
}

static void __exit sleep_clock_driver_exit(void)
{
	platform_driver_unregister(&sleep_clock_driver);
}

module_init(sleep_clock_driver_init);
module_exit(sleep_clock_driver_exit);

MODULE_AUTHOR("Maxim Adelman <imax@fb.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Sleep clock driver, based on Qualcomm MPM sleep counter");
