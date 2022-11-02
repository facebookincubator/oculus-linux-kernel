/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * cec-notifier.h - notify CEC drivers of physical address changes
 *
 * Copyright 2016 Russell King <rmk+kernel@arm.linux.org.uk>
 * Copyright 2016-2017 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#ifndef LINUX_CEC_NOTIFIER_H
#define LINUX_CEC_NOTIFIER_H

#include <linux/types.h>
#include <media/cec.h>

struct device;
struct edid;
struct cec_adapter;
struct cec_notifier;

#if IS_REACHABLE(CONFIG_CEC_CORE) && IS_ENABLED(CONFIG_CEC_NOTIFIER)

/**
 * cec_notifier_get_conn - find or create a new cec_notifier for the given
 * device and connector tuple.
 * @dev: device that sends the events.
 * @conn: the connector name from which the event occurs
 *
 * If a notifier for device @dev already exists, then increase the refcount
 * and return that notifier.
 *
 * If it doesn't exist, then allocate a new notifier struct and return a
 * pointer to that new struct.
 *
 * Return NULL if the memory could not be allocated.
 */
struct cec_notifier *cec_notifier_get_conn(struct device *dev,
					   const char *conn);

/**
 * cec_notifier_put - decrease refcount and delete when the refcount reaches 0.
 * @n: notifier
 */
void cec_notifier_put(struct cec_notifier *n);

/**
 * cec_notifier_set_phys_addr - set a new physical address.
 * @n: the CEC notifier
 * @pa: the CEC physical address
 *
 * Set a new CEC physical address.
 * Does nothing if @n == NULL.
 */
void cec_notifier_set_phys_addr(struct cec_notifier *n, u16 pa);

/**
 * cec_notifier_set_phys_addr_from_edid - set parse the PA from the EDID.
 * @n: the CEC notifier
 * @edid: the struct edid pointer
 *
 * Parses the EDID to obtain the new CEC physical address and set it.
 * Does nothing if @n == NULL.
 */
void cec_notifier_set_phys_addr_from_edid(struct cec_notifier *n,
					  const struct edid *edid);

/**
 * cec_notifier_register - register a callback with the notifier
 * @n: the CEC notifier
 * @adap: the CEC adapter, passed as argument to the callback function
 * @callback: the callback function
 */
void cec_notifier_register(struct cec_notifier *n,
			   struct cec_adapter *adap,
			   void (*callback)(struct cec_adapter *adap, u16 pa));

/**
 * cec_notifier_unregister - unregister the callback from the notifier.
 * @n: the CEC notifier
 */
void cec_notifier_unregister(struct cec_notifier *n);

/**
 * cec_register_cec_notifier - register the notifier with the cec adapter.
 * @adap: the CEC adapter
 * @notifier: the CEC notifier
 */
void cec_register_cec_notifier(struct cec_adapter *adap,
			       struct cec_notifier *notifier);

#else
static inline struct cec_notifier *cec_notifier_get_conn(struct device *dev,
							 const char *conn)
{
	/* A non-NULL pointer is expected on success */
	return (struct cec_notifier *)0xdeadfeed;
}

static inline void cec_notifier_put(struct cec_notifier *n)
{
}

static inline void cec_notifier_set_phys_addr(struct cec_notifier *n, u16 pa)
{
}

static inline void cec_notifier_set_phys_addr_from_edid(struct cec_notifier *n,
							const struct edid *edid)
{
}

static inline void cec_notifier_register(struct cec_notifier *n,
			 struct cec_adapter *adap,
			 void (*callback)(struct cec_adapter *adap, u16 pa))
{
}

static inline void cec_notifier_unregister(struct cec_notifier *n)
{
}

static inline void cec_register_cec_notifier(struct cec_adapter *adap,
					     struct cec_notifier *notifier)
{
}
#endif

/**
 * cec_notifier_get - find or create a new cec_notifier for the given device.
 * @dev: device that sends the events.
 *
 * If a notifier for device @dev already exists, then increase the refcount
 * and return that notifier.
 *
 * If it doesn't exist, then allocate a new notifier struct and return a
 * pointer to that new struct.
 *
 * Return NULL if the memory could not be allocated.
 */
static inline struct cec_notifier *cec_notifier_get(struct device *dev)
{
	return cec_notifier_get_conn(dev, NULL);
}

/**
 * cec_notifier_phys_addr_invalidate() - set the physical address to INVALID
 *
 * @n: the CEC notifier
 *
 * This is a simple helper function to invalidate the physical
 * address. Does nothing if @n == NULL.
 */
static inline void cec_notifier_phys_addr_invalidate(struct cec_notifier *n)
{
	cec_notifier_set_phys_addr(n, CEC_PHYS_ADDR_INVALID);
}

#endif
