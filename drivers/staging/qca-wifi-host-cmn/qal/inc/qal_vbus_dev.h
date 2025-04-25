/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: qal_vbus_dev
 * QCA abstraction layer (QAL) virtual bus management APIs
 */

#if !defined(__QAL_VBUS_DEV_H)
#define __QAL_VBUS_DEV_H

/* Include Files */
#include <qdf_types.h>
#include <i_qal_vbus_dev.h>

struct qdf_vbus_resource;
struct qdf_vbus_rstctl;
struct qdf_dev_clk;
struct qdf_pfm_hndl;
struct qdf_pfm_drv;
struct qdf_device_node;
typedef __qdf_of_gpio_flags qdf_of_gpio_flags;
#ifdef ENHANCED_OS_ABSTRACTION
/**
 * qal_vbus_get_iorsc() - acquire io resource
 * @devnum: Device Number
 * @flag: Property bitmap for the io resource
 * @devname: Device name string
 *
 * This function will allocate the io resource for a device
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qal_vbus_get_iorsc(int devnum, uint32_t flag, char *devname);

/**
 * qal_vbus_release_iorsc() - release io resource
 * @devnum: Device Number
 *
 * This function will release the io resource attached to a device
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qal_vbus_release_iorsc(int devnum);

/**
 * qal_vbus_allocate_iorsc() - allocate io resource
 * @pinnum: pin Number
 * @label: pin name string
 *
 * This function will allocate the io resource for a device
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qal_vbus_allocate_iorsc(unsigned int pinnum, const char *label);

/**
 * qal_vbus_iorsc_dir_output() - set pin dirction to output
 * @pin: pin Number
 * @val: value
 *
 * This function set the gpio pin direction to output
 *
 * Return: 0 on success, error no on failure
 */
QDF_STATUS
qal_vbus_iorsc_dir_output(unsigned int pin, int val);

/**
 * qal_vbus_iorsc_set_value() - set pin direction
 * @pin: pin Number
 * @val: value
 *
 * This function set the gpio pin direction based on value
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qal_vbus_iorsc_set_value(unsigned int pin, int val);

/**
 * qal_vbus_iorsc_toirq() - set irq number to gpio
 * @pin: pin Number
 *
 * This function set the irq number to gpio pin
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qal_vbus_iorsc_toirq(unsigned int pin);

/**
 * qal_vbus_request_irq() - set interrupt handler
 * @irqnum: irq Number
 * @handler: function handler to be called
 * @flags: irq flags
 * @dev_name: device name
 * @ctx: pointer to device context
 * This function set up the handling of the interrupt
 *
 * Return: QDF_STATUS_SUCCESS on success, Error code on failure
 */
QDF_STATUS
qal_vbus_request_irq(unsigned int irqnum,
		     irqreturn_t (*handler)(int irq, void *arg),
		     unsigned long flags, const char *dev_name,
		     void *ctx);

/**
 * __qal_vbus_free_irq() - free irq
 * @irqnum: irq Number
 * @ctx: pointer to device context
 *
 * This function free the irq number set to gpio pin
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static inline QDF_STATUS
__qal_vbus_free_irq(unsigned int irqnum, void *ctx);

/**
 * qal_vbus_enable_devclk() - enable device clock
 * @clk: Device clock
 *
 * This function will enable the clock for a device
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qal_vbus_enable_devclk(struct qdf_dev_clk *clk);

/**
 * qal_vbus_disable_devclk() - disable device clock
 * @clk: Device clock
 *
 * This function will disable the clock for a device
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qal_vbus_disable_devclk(struct qdf_dev_clk *clk);

/**
 * qal_vbus_get_dev_rstctl() - get device reset control
 * @pfhndl: Device handle
 * @state: Device state information
 * @rstctl: Device reset control handle
 *
 * This function will acquire the control to reset the device
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qal_vbus_get_dev_rstctl(struct qdf_pfm_hndl *pfhndl, const char *state,
			struct qdf_vbus_rstctl **rstctl);

/**
 * qal_vbus_release_dev_rstctl() - release device reset control
 * @pfhndl: Device handle
 * @rstctl: Device reset control handle
 *
 * This function will release the control to reset the device
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qal_vbus_release_dev_rstctl(struct qdf_pfm_hndl *pfhndl,
			    struct qdf_vbus_rstctl *rstctl);

/**
 * qal_vbus_activate_dev_rstctl() - activate device reset control
 * @pfhndl: Device handle
 * @rstctl: Device reset control handle
 *
 * This function will activate the reset control for the device
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qal_vbus_activate_dev_rstctl(struct qdf_pfm_hndl *pfhndl,
			     struct qdf_vbus_rstctl *rstctl);

/**
 * qal_vbus_deactivate_dev_rstctl() - deactivate device reset control
 * @pfhndl: Device handle
 * @rstctl: Device reset control handle
 *
 * This function will deactivate the reset control for the device
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qal_vbus_deactivate_dev_rstctl(struct qdf_pfm_hndl *pfhndl,
			       struct qdf_vbus_rstctl *rstctl);

/**
 * qal_vbus_get_resource() - get resource
 * @pfhndl: Device handle
 * @rsc: Resource handle
 * @restype: Resource type
 * @residx: Resource index
 *
 * This function will acquire a particular resource and attach it to the device
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qal_vbus_get_resource(struct qdf_pfm_hndl *pfhndl,
		      struct qdf_vbus_resource **rsc, uint32_t restype,
		      uint32_t residx);

/**
 * qal_vbus_get_irq() - get irq
 * @pfhndl: Device handle
 * @str: Device identifier
 * @irq: irq number
 *
 * This function will acquire an irq for the device
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qal_vbus_get_irq(struct qdf_pfm_hndl *pfhndl, const char *str, int *irq);

/**
 * qal_vbus_register_driver() - register driver
 * @pfdev: Device handle
 *
 * This function will initialize a device
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qal_vbus_register_driver(struct qdf_pfm_drv *pfdev);

/**
 * qal_vbus_deregister_driver() - deregister driver
 * @pfdev: Device handle
 *
 * This function will deregister the driver for a device
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qal_vbus_deregister_driver(struct qdf_pfm_drv *pfdev);

/**
 * qal_vbus_gpio_set_value_cansleep() - assign a gpio's value
 * @gpio: gpio whose value will be assigned
 * @value: value to assign
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qal_vbus_gpio_set_value_cansleep(unsigned int gpio, int value);

/**
 * qal_vbus_rcu_read_lock() - mark the beginning of an RCU read-side critical
 *                            section
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qal_vbus_rcu_read_lock(void);

/**
 * qal_vbus_rcu_read_unlock() - mark the end of an RCU read-side critical
 *                              section
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qal_vbus_rcu_read_unlock(void);

/**
 * qal_vbus_of_get_named_gpio_flags() - Get a GPIO descriptor and flags
 * for GPIO API
 * @np: device node to get GPIO from
 * @list_name: property name containing gpio specifier(s)
 * @index: index of the GPIO
 * @flags: a flags pointer to fill in
 *
 * The global GPIO number for the GPIO specified by its descriptor.
 */
int
qal_vbus_of_get_named_gpio_flags(struct qdf_device_node *np,
				 const char *list_name,
				 int index, qdf_of_gpio_flags *flags);
#else
static inline QDF_STATUS
qal_vbus_get_iorsc(int devnum, uint32_t flag, char *devname)
{
	return __qal_vbus_get_iorsc(devnum, flag, devname);
}

static inline QDF_STATUS
qal_vbus_allocate_iorsc(unsigned int pinnum, const char *label)
{
	return __qal_vbus_allocate_iorsc(pinnum, label);
}

static inline QDF_STATUS
qal_vbus_iorsc_dir_output(unsigned int pin, int val)
{
	return __qal_vbus_iorsc_dir_output(pin, val);
}

static inline QDF_STATUS
qal_vbus_iorsc_set_value(unsigned int pin, int val)
{
	return __qal_vbus_iorsc_set_value(pin, val);
}

static inline QDF_STATUS
qal_vbus_release_iorsc(int devnum)
{
	return __qal_vbus_release_iorsc(devnum);
}

static inline QDF_STATUS
qal_vbus_iorsc_toirq(unsigned int pin)
{
	return __qal_vbus_iorsc_toirq(pin);
}

static inline QDF_STATUS
qal_vbus_request_irq(unsigned int irqnum,
		     irqreturn_t (*handler)(int irq, void *arg),
		     unsigned long flags, const char *dev_name, void *ctx)
{
	return __qal_vbus_request_irq(irqnum, handler, flags, dev_name, ctx);
}

static inline QDF_STATUS
qal_vbus_free_irq(unsigned int irqnum, void *ctx)
{
	return __qal_vbus_free_irq(irqnum, ctx);
}

static inline QDF_STATUS
qal_vbus_enable_devclk(struct qdf_dev_clk *clk)
{
	return __qal_vbus_enable_devclk(clk);
}

static inline QDF_STATUS
qal_vbus_disable_devclk(struct qdf_dev_clk *clk)
{
	return __qal_vbus_disable_devclk(clk);
}

static inline QDF_STATUS
qal_vbus_get_dev_rstctl(struct qdf_pfm_hndl *pfhndl, const char *state,
			struct qdf_vbus_rstctl **rstctl)
{
	return __qal_vbus_get_dev_rstctl(pfhndl, state, rstctl);
}

static inline QDF_STATUS
qal_vbus_release_dev_rstctl(struct qdf_pfm_hndl *pfhndl,
			    struct qdf_vbus_rstctl *rstctl)
{
	return __qal_vbus_release_dev_rstctl(pfhndl, rstctl);
}

static inline QDF_STATUS
qal_vbus_activate_dev_rstctl(struct qdf_pfm_hndl *pfhndl,
			     struct qdf_vbus_rstctl *rstctl)
{
	return __qal_vbus_activate_dev_rstctl(pfhndl, rstctl);
}

static inline QDF_STATUS
qal_vbus_deactivate_dev_rstctl(struct qdf_pfm_hndl *pfhndl,
			       struct qdf_vbus_rstctl *rstctl)
{
	return __qal_vbus_deactivate_dev_rstctl(pfhndl, rstctl);
}

static inline QDF_STATUS
qal_vbus_get_resource(struct qdf_pfm_hndl *pfhndl,
		      struct qdf_vbus_resource **rsc, uint32_t restype,
		      uint32_t residx)
{
	return __qal_vbus_get_resource(pfhndl, rsc, restype, residx);
}

static inline QDF_STATUS
qal_vbus_get_irq(struct qdf_pfm_hndl *pfhndl, const char *str, int *irq)
{
	return __qal_vbus_get_irq(pfhndl, str, irq);
}

static inline QDF_STATUS
qal_vbus_register_driver(struct qdf_pfm_drv *pfdev)
{
	return __qal_vbus_register_driver(pfdev);
}

static inline QDF_STATUS
qal_vbus_deregister_driver(struct qdf_pfm_drv *pfdev)
{
	return __qal_vbus_deregister_driver(pfdev);
}

static inline QDF_STATUS
qal_vbus_gpio_set_value_cansleep(unsigned int gpio, int value)
{
	return __qal_vbus_gpio_set_value_cansleep(gpio, value);
}

static inline QDF_STATUS
qal_vbus_rcu_read_lock(void)
{
	return __qal_vbus_rcu_read_lock();
}

static inline QDF_STATUS
qal_vbus_rcu_read_unlock(void)
{
	return __qal_vbus_rcu_read_unlock();
}

static inline int
qal_vbus_of_get_named_gpio_flags(struct qdf_device_node *np,
				 const char *list_name,
				 int index, qdf_of_gpio_flags *flags)
{
	return __qal_vbus_of_get_named_gpio_flags(np, list_name, index, flags);
}
#endif
#endif  /* __QAL_VBUS_DEV_H */
