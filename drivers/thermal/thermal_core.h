/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  thermal_core.h
 *
 *  Copyright (C) 2012  Intel Corp
 *  Author: Durgadoss R <durgadoss.r@intel.com>
 */

#ifndef __THERMAL_CORE_H__
#define __THERMAL_CORE_H__

#include <linux/device.h>
#include <linux/thermal.h>

/* Initial state of a cooling device during binding */
#define THERMAL_NO_TARGET -1UL

/*
 * This structure is used to describe the behavior of
 * a certain cooling device on a certain trip point
 * in a certain thermal zone
 */
struct thermal_instance {
	int id;
	char name[THERMAL_NAME_LENGTH];
	struct thermal_zone_device *tz;
	struct thermal_cooling_device *cdev;
	int trip;
	bool initialized;
	unsigned long upper;	/* Highest cooling state for this trip point */
	unsigned long lower;	/* Lowest cooling state for this trip point */
	unsigned long target;	/* expected cooling state */
	char attr_name[THERMAL_NAME_LENGTH];
	struct device_attribute attr;
	char weight_attr_name[THERMAL_NAME_LENGTH];
	struct device_attribute weight_attr;
	char upper_attr_name[THERMAL_NAME_LENGTH];
	struct device_attribute upper_attr;
	char lower_attr_name[THERMAL_NAME_LENGTH];
	struct device_attribute lower_attr;
	struct list_head tz_node; /* node in tz->thermal_instances */
	struct list_head cdev_node; /* node in cdev->thermal_instances */
	unsigned int weight; /* The weight of the cooling device */
};

#define to_thermal_zone(_dev) \
	container_of(_dev, struct thermal_zone_device, device)

#define to_cooling_device(_dev)	\
	container_of(_dev, struct thermal_cooling_device, device)

int thermal_register_governor(struct thermal_governor *);
void thermal_unregister_governor(struct thermal_governor *);
void thermal_zone_device_rebind_exception(struct thermal_zone_device *,
					  const char *, size_t);
void thermal_zone_device_unbind_exception(struct thermal_zone_device *,
					  const char *, size_t);
int thermal_zone_device_set_policy(struct thermal_zone_device *, char *);
int thermal_build_list_of_policies(char *buf);

/* sysfs I/F */
int thermal_zone_create_device_groups(struct thermal_zone_device *, int);
void thermal_zone_destroy_device_groups(struct thermal_zone_device *);
void thermal_cooling_device_setup_sysfs(struct thermal_cooling_device *);
void thermal_cooling_device_destroy_sysfs(struct thermal_cooling_device *cdev);
/* used only at binding time */
ssize_t trip_point_show(struct device *, struct device_attribute *, char *);
ssize_t trip_point_store(struct device *, struct device_attribute *,
			 const char *, size_t);
ssize_t weight_show(struct device *, struct device_attribute *, char *);
ssize_t lower_limit_show(struct device *dev, struct device_attribute *attr,
			char *buf);
ssize_t upper_limit_show(struct device *dev, struct device_attribute *attr,
			char *buf);
ssize_t weight_store(struct device *, struct device_attribute *, const char *,
		     size_t);
ssize_t lower_limit_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count);
ssize_t upper_limit_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count);

#ifdef CONFIG_THERMAL_STATISTICS
void thermal_cooling_device_stats_update(struct thermal_cooling_device *cdev,
					 unsigned long new_state);
#else
static inline void
thermal_cooling_device_stats_update(struct thermal_cooling_device *cdev,
				    unsigned long new_state) {}
#endif /* CONFIG_THERMAL_STATISTICS */

#ifdef CONFIG_THERMAL_GOV_STEP_WISE
int thermal_gov_step_wise_register(void);
void thermal_gov_step_wise_unregister(void);
#else
static inline int thermal_gov_step_wise_register(void) { return 0; }
static inline void thermal_gov_step_wise_unregister(void) {}
#endif /* CONFIG_THERMAL_GOV_STEP_WISE */

#ifdef CONFIG_THERMAL_GOV_FAIR_SHARE
int thermal_gov_fair_share_register(void);
void thermal_gov_fair_share_unregister(void);
#else
static inline int thermal_gov_fair_share_register(void) { return 0; }
static inline void thermal_gov_fair_share_unregister(void) {}
#endif /* CONFIG_THERMAL_GOV_FAIR_SHARE */

#ifdef CONFIG_THERMAL_GOV_BANG_BANG
int thermal_gov_bang_bang_register(void);
void thermal_gov_bang_bang_unregister(void);
#else
static inline int thermal_gov_bang_bang_register(void) { return 0; }
static inline void thermal_gov_bang_bang_unregister(void) {}
#endif /* CONFIG_THERMAL_GOV_BANG_BANG */

#ifdef CONFIG_THERMAL_GOV_USER_SPACE
int thermal_gov_user_space_register(void);
void thermal_gov_user_space_unregister(void);
#else
static inline int thermal_gov_user_space_register(void) { return 0; }
static inline void thermal_gov_user_space_unregister(void) {}
#endif /* CONFIG_THERMAL_GOV_USER_SPACE */

#ifdef CONFIG_THERMAL_GOV_POWER_ALLOCATOR
int thermal_gov_power_allocator_register(void);
void thermal_gov_power_allocator_unregister(void);
#else
static inline int thermal_gov_power_allocator_register(void) { return 0; }
static inline void thermal_gov_power_allocator_unregister(void) {}
#endif /* CONFIG_THERMAL_GOV_POWER_ALLOCATOR */

#ifdef CONFIG_THERMAL_GOV_LOW_LIMITS
int thermal_gov_low_limits_register(void);
void thermal_gov_low_limits_unregister(void);
#else
static inline int thermal_gov_low_limits_register(void) { return 0; }
static inline void thermal_gov_low_limits_unregister(void) {}
#endif /* CONFIG_THERMAL_GOV_LOW_LIMITS */

/* device tree support */
#ifdef CONFIG_THERMAL_OF
int of_parse_thermal_zones(void);
void of_thermal_destroy_zones(void);
int of_thermal_get_ntrips(struct thermal_zone_device *);
bool of_thermal_is_trip_valid(struct thermal_zone_device *, int);
const struct thermal_trip *
of_thermal_get_trip_points(struct thermal_zone_device *);
int of_thermal_aggregate_trip(struct thermal_zone_device *tz,
			      enum thermal_trip_type type,
			      int *low, int *high);
void of_thermal_handle_trip(struct thermal_zone_device *tz);
void of_thermal_handle_trip_temp(struct thermal_zone_device *tz,
					int trip_temp);
#else
static inline int of_parse_thermal_zones(void) { return 0; }
static inline void of_thermal_destroy_zones(void) { }
static inline int of_thermal_get_ntrips(struct thermal_zone_device *tz)
{
	return 0;
}
static inline bool of_thermal_is_trip_valid(struct thermal_zone_device *tz,
					    int trip)
{
	return false;
}
static inline const struct thermal_trip *
of_thermal_get_trip_points(struct thermal_zone_device *tz)
{
	return NULL;
}
static inline int of_thermal_aggregate_trip(struct thermal_zone_device *tz,
					    enum thermal_trip_type type,
					    int *low, int *high)
{
	return -ENODEV;
}
static inline
void of_thermal_handle_trip(struct thermal_zone_device *tz)
{ }
static inline
void of_thermal_handle_trip_temp(struct thermal_zone_device *tz,
					int trip_temp)
{ }
#endif

#endif /* __THERMAL_CORE_H__ */
