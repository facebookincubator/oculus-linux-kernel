/*
 * Copyright (c) 2015  Hauke Mehrtens <hauke@hauke-m.de>
 * Copyright (c) 2015 - 2016 Intel Deutschland GmbH
 *
 * Backport functionality introduced in Linux 4.3.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/version.h>
#include <linux/seq_file.h>
#include <linux/export.h>
#include <linux/printk.h>
#include <linux/thermal.h>
#include <linux/slab.h>

#ifdef CONFIG_THERMAL
#if LINUX_VERSION_IS_GEQ(3,8,0)
struct backport_thermal_ops_wrapper {
	old_thermal_zone_device_ops_t ops;
	struct thermal_zone_device_ops *driver_ops;
};

static int backport_thermal_get_temp(struct thermal_zone_device *dev,
				     unsigned long *temp)
{
	struct backport_thermal_ops_wrapper *wrapper =
		container_of(dev->ops, struct backport_thermal_ops_wrapper, ops);
	int _temp, ret;

	ret = wrapper->driver_ops->get_temp(dev, &_temp);
	if (!ret)
		*temp = (unsigned long)_temp;

	return ret;
}

static int backport_thermal_get_trip_temp(struct thermal_zone_device *dev,
					  int i, unsigned long *temp)
{
	struct backport_thermal_ops_wrapper *wrapper =
		container_of(dev->ops, struct backport_thermal_ops_wrapper, ops);
	int _temp, ret;

	ret = wrapper->driver_ops->get_trip_temp(dev, i,  &_temp);
	if (!ret)
		*temp = (unsigned long)_temp;

	return ret;
}

static int backport_thermal_set_trip_temp(struct thermal_zone_device *dev,
					  int i, unsigned long temp)
{
	struct backport_thermal_ops_wrapper *wrapper =
		container_of(dev->ops, struct backport_thermal_ops_wrapper, ops);

	return wrapper->driver_ops->set_trip_temp(dev, i, (int)temp);
}

static int backport_thermal_get_trip_hyst(struct thermal_zone_device *dev,
					  int i, unsigned long *temp)
{
	struct backport_thermal_ops_wrapper *wrapper =
		container_of(dev->ops, struct backport_thermal_ops_wrapper, ops);
	int _temp, ret;

	ret = wrapper->driver_ops->get_trip_hyst(dev, i, &_temp);
	if (!ret)
		*temp = (unsigned long)_temp;

	return ret;
}

static int backport_thermal_set_trip_hyst(struct thermal_zone_device *dev,
					  int i, unsigned long temp)
{
	struct backport_thermal_ops_wrapper *wrapper =
		container_of(dev->ops, struct backport_thermal_ops_wrapper, ops);

	return wrapper->driver_ops->set_trip_hyst(dev, i, (int)temp);
}

static int backport_thermal_get_crit_temp(struct thermal_zone_device *dev,
					  unsigned long *temp)
{
	struct backport_thermal_ops_wrapper *wrapper =
		container_of(dev->ops, struct backport_thermal_ops_wrapper, ops);
	int _temp, ret;

	ret = wrapper->driver_ops->get_crit_temp(dev, &_temp);
	if (!ret)
		*temp = (unsigned long)_temp;

	return ret;
}

#if LINUX_VERSION_IS_GEQ(3, 19, 0)
static int backport_thermal_set_emul_temp(struct thermal_zone_device *dev,
					  unsigned long temp)
{
	struct backport_thermal_ops_wrapper *wrapper =
		container_of(dev->ops, struct backport_thermal_ops_wrapper, ops);

	return wrapper->driver_ops->set_emul_temp(dev, (int)temp);
}
#endif /* LINUX_VERSION_IS_GEQ(3, 19, 0) */

struct thermal_zone_device *backport_thermal_zone_device_register(
	const char *type, int trips, int mask, void *devdata,
	struct thermal_zone_device_ops *ops,
	const struct thermal_zone_params *tzp,
	int passive_delay, int polling_delay)
{
	struct backport_thermal_ops_wrapper *wrapper = kzalloc(sizeof(*wrapper), GFP_KERNEL);
	struct thermal_zone_device *ret;

	if (!wrapper)
		return NULL;

	wrapper->driver_ops = ops;

#define copy(_op)		\
	wrapper->ops._op = ops->_op

	copy(bind);
	copy(unbind);
	copy(get_mode);
	copy(set_mode);
	copy(get_trip_type);
	copy(get_trend);
	copy(notify);

	/* Assign the backport ops to the old struct to get the
	 * correct types.  But only assign if the registrant defined
	 * the ops.
	 */
#define assign_ops(_op)		\
	if (ops->_op)		\
		wrapper->ops._op = backport_thermal_##_op

	assign_ops(get_temp);
	assign_ops(get_trip_temp);
	assign_ops(set_trip_temp);
	assign_ops(get_trip_hyst);
	assign_ops(set_trip_hyst);
	assign_ops(get_crit_temp);
#if LINUX_VERSION_IS_GEQ(3, 19, 0)
	assign_ops(set_emul_temp);
#endif /* LINUX_VERSION_IS_GEQ(3, 19, 0) */
#undef assign_ops

	ret = old_thermal_zone_device_register(type, trips, mask, devdata,
					       &wrapper->ops, tzp, passive_delay,
					       polling_delay);
	if (!ret)
		kfree(wrapper);
	return ret;
}
EXPORT_SYMBOL_GPL(backport_thermal_zone_device_register);

void backport_thermal_zone_device_unregister(struct thermal_zone_device *dev)
{
	struct backport_thermal_ops_wrapper *wrapper =
		container_of(dev->ops, struct backport_thermal_ops_wrapper, ops);

	old_thermal_zone_device_unregister(dev);
	kfree(wrapper);
}
EXPORT_SYMBOL_GPL(backport_thermal_zone_device_unregister);

#endif /* >= 3.8.0 */
#endif /* CONFIG_THERMAL */

static void seq_set_overflow(struct seq_file *m)
{
	m->count = m->size;
}

/* A complete analogue of print_hex_dump() */
void seq_hex_dump(struct seq_file *m, const char *prefix_str, int prefix_type,
		  int rowsize, int groupsize, const void *buf, size_t len,
		  bool ascii)
{
	const u8 *ptr = buf;
	int i, linelen, remaining = len;
	int ret;

	if (rowsize != 16 && rowsize != 32)
		rowsize = 16;

	for (i = 0; i < len && !seq_has_overflowed(m); i += rowsize) {
		linelen = min(remaining, rowsize);
		remaining -= rowsize;

		switch (prefix_type) {
		case DUMP_PREFIX_ADDRESS:
			seq_printf(m, "%s%p: ", prefix_str, ptr + i);
			break;
		case DUMP_PREFIX_OFFSET:
			seq_printf(m, "%s%.8x: ", prefix_str, i);
			break;
		default:
			seq_printf(m, "%s", prefix_str);
			break;
		}

		ret = hex_dump_to_buffer(ptr + i, linelen, rowsize, groupsize,
					 m->buf + m->count, m->size - m->count,
					 ascii);
		if (ret >= m->size - m->count) {
			seq_set_overflow(m);
		} else {
			m->count += ret;
			seq_putc(m, '\n');
		}
	}
}
EXPORT_SYMBOL_GPL(seq_hex_dump);

ssize_t strscpy(char *dest, const char *src, size_t count)
{
	long res = 0;

	if (count == 0)
		return -E2BIG;

	while (count) {
		char c;

		c = src[res];
		dest[res] = c;
		if (!c)
			return res;
		res++;
		count--;
	}

	/* Hit buffer length without finding a NUL; force NUL-termination. */
	if (res)
		dest[res-1] = '\0';

	return -E2BIG;
}
EXPORT_SYMBOL_GPL(strscpy);
