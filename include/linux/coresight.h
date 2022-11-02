/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2012,2017-2018 The Linux Foundation. All rights reserved.
 */

#ifndef _LINUX_CORESIGHT_H
#define _LINUX_CORESIGHT_H

#include <linux/device.h>
#include <linux/perf_event.h>
#include <linux/sched.h>

/* Peripheral id registers (0xFD0-0xFEC) */
#define CORESIGHT_PERIPHIDR4	0xfd0
#define CORESIGHT_PERIPHIDR5	0xfd4
#define CORESIGHT_PERIPHIDR6	0xfd8
#define CORESIGHT_PERIPHIDR7	0xfdC
#define CORESIGHT_PERIPHIDR0	0xfe0
#define CORESIGHT_PERIPHIDR1	0xfe4
#define CORESIGHT_PERIPHIDR2	0xfe8
#define CORESIGHT_PERIPHIDR3	0xfeC
/* Component id registers (0xFF0-0xFFC) */
#define CORESIGHT_COMPIDR0	0xff0
#define CORESIGHT_COMPIDR1	0xff4
#define CORESIGHT_COMPIDR2	0xff8
#define CORESIGHT_COMPIDR3	0xffC

#define ETM_ARCH_V3_3		0x23
#define ETM_ARCH_V3_5		0x25
#define PFT_ARCH_V1_0		0x30
#define PFT_ARCH_V1_1		0x31

#define CORESIGHT_UNLOCK	0xc5acce55

extern struct bus_type coresight_bustype;

enum coresight_dev_type {
	CORESIGHT_DEV_TYPE_NONE,
	CORESIGHT_DEV_TYPE_SINK,
	CORESIGHT_DEV_TYPE_LINK,
	CORESIGHT_DEV_TYPE_LINKSINK,
	CORESIGHT_DEV_TYPE_SOURCE,
	CORESIGHT_DEV_TYPE_HELPER,
};

enum coresight_dev_subtype_sink {
	CORESIGHT_DEV_SUBTYPE_SINK_NONE,
	CORESIGHT_DEV_SUBTYPE_SINK_PORT,
	CORESIGHT_DEV_SUBTYPE_SINK_BUFFER,
};

enum coresight_dev_subtype_link {
	CORESIGHT_DEV_SUBTYPE_LINK_NONE,
	CORESIGHT_DEV_SUBTYPE_LINK_MERG,
	CORESIGHT_DEV_SUBTYPE_LINK_SPLIT,
	CORESIGHT_DEV_SUBTYPE_LINK_FIFO,
};

enum coresight_dev_subtype_source {
	CORESIGHT_DEV_SUBTYPE_SOURCE_NONE,
	CORESIGHT_DEV_SUBTYPE_SOURCE_PROC,
	CORESIGHT_DEV_SUBTYPE_SOURCE_BUS,
	CORESIGHT_DEV_SUBTYPE_SOURCE_SOFTWARE,
};

enum coresight_dev_subtype_helper {
	CORESIGHT_DEV_SUBTYPE_HELPER_NONE,
	CORESIGHT_DEV_SUBTYPE_HELPER_CATU,
};

/**
 * union coresight_dev_subtype - further characterisation of a type
 * @sink_subtype:	type of sink this component is, as defined
 *			by @coresight_dev_subtype_sink.
 * @link_subtype:	type of link this component is, as defined
 *			by @coresight_dev_subtype_link.
 * @source_subtype:	type of source this component is, as defined
 *			by @coresight_dev_subtype_source.
 * @helper_subtype:	type of helper this component is, as defined
 *			by @coresight_dev_subtype_helper.
 */
union coresight_dev_subtype {
	/* We have some devices which acts as LINK and SINK */
	struct {
		enum coresight_dev_subtype_sink sink_subtype;
		enum coresight_dev_subtype_link link_subtype;
	};
	enum coresight_dev_subtype_source source_subtype;
	enum coresight_dev_subtype_helper helper_subtype;
};

/**
 * struct coresight_reg_clk - regulators and clocks need by coresight
 * @nr_reg:	number of regulators
 * @nr_clk:	number of clocks
 * @reg:	regulator list
 * @clk:	clock list
 */
struct coresight_reg_clk {
	int nr_reg;
	int nr_clk;
	struct regulator **reg;
	struct clk **clk;
};

/**
 * struct coresight_platform_data - data harvested from the DT specification
 * @cpu:	the CPU a source belongs to. Only applicable for ETM/PTMs.
 * @name:	name of the component as shown under sysfs.
 * @nr_inport:	number of input ports for this component.
 * @outports:	list of remote endpoint port number.
 * @source_names:name of all source components connected to this device.
 * @child_names:name of all child components connected to this device.
 * @child_ports:child component port number the current component is
		connected  to.
 * @nr_outport:	number of output ports for this component.
 * @clk:	The clock this component is associated to.
 * @reg_clk:	as defined by @coresight_reg_clk.
 */
struct coresight_platform_data {
	int cpu;
	const char *name;
	int nr_inport;
	int *outports;
	const char **source_names;
	const char **child_names;
	int *child_ports;
	int nr_outport;
	struct clk *clk;
	struct coresight_reg_clk *reg_clk;
};

/**
 * struct coresight_desc - description of a component required from drivers
 * @type:	as defined by @coresight_dev_type.
 * @subtype:	as defined by @coresight_dev_subtype.
 * @ops:	generic operations for this component, as defined
		by @coresight_ops.
 * @pdata:	platform data collected from DT.
 * @dev:	The device entity associated to this component.
 * @groups:	operations specific to this component. These will end up
		in the component's sysfs sub-directory.
 */
struct coresight_desc {
	enum coresight_dev_type type;
	union coresight_dev_subtype subtype;
	const struct coresight_ops *ops;
	struct coresight_platform_data *pdata;
	struct device *dev;
	const struct attribute_group **groups;
};

/**
 * struct coresight_connection - representation of a single connection
 * @outport:	a connection's output port number.
 * @source_name:source component's name.
 * @chid_name:	remote component's name.
 * @child_port:	remote component's port number @output is connected to.
 * @child_dev:	a @coresight_device representation of the component
		connected to @outport.
 */
struct coresight_connection {
	int outport;
	const char *source_name;
	const char *child_name;
	int child_port;
	struct coresight_device *child_dev;
};

/**
 * struct coresight_device - representation of a device as used by the framework
 * @conns:	array of coresight_connections associated to this component.
 * @nr_inport:	number of input port associated to this component.
 * @nr_outport:	number of output port associated to this component.
 * @type:	as defined by @coresight_dev_type.
 * @subtype:	as defined by @coresight_dev_subtype.
 * @ops:	generic operations for this component, as defined
		by @coresight_ops.
 * @dev:	The device entity associated to this component.
 * @refcnt:	keep track of what is in use.
 * @orphan:	true if the component has connections that haven't been linked.
 * @enable:	'true' if component is currently part of an active path.
 * @activated:	'true' only if a _sink_ has been activated.  A sink can be
		activated but not yet enabled.  Enabling for a _sink_
		happens when a source has been selected for that it.
 * @abort:     captures sink trace on abort.
 * @reg_clk:	as defined by @coresight_reg_clk.
 */
struct coresight_device {
	struct coresight_connection *conns;
	int nr_inport;
	int nr_outport;
	enum coresight_dev_type type;
	union coresight_dev_subtype subtype;
	const struct coresight_ops *ops;
	struct device dev;
	atomic_t *refcnt;
	struct coresight_path *node;
	bool orphan;
	bool enable;	/* true only if configured as part of a path */
	bool activated;	/* true only if a sink is part of a path */
	struct coresight_reg_clk *reg_clk;
};

#define to_coresight_device(d) container_of(d, struct coresight_device, dev)

#define source_ops(csdev)	csdev->ops->source_ops
#define sink_ops(csdev)		csdev->ops->sink_ops
#define link_ops(csdev)		csdev->ops->link_ops
#define helper_ops(csdev)	csdev->ops->helper_ops

/**
 * struct coresight_ops_sink - basic operations for a sink
 * Operations available for sinks
 * @enable:		enables the sink.
 * @disable:		disables the sink.
 * @alloc_buffer:	initialises perf's ring buffer for trace collection.
 * @free_buffer:	release memory allocated in @get_config.
 * @set_buffer:		initialises buffer mechanic before a trace session.
 * @reset_buffer:	finalises buffer mechanic after a trace session.
 * @update_buffer:	update buffer pointers after a trace session.
 */
struct coresight_ops_sink {
	int (*enable)(struct coresight_device *csdev, u32 mode);
	void (*disable)(struct coresight_device *csdev);
	void *(*alloc_buffer)(struct coresight_device *csdev, int cpu,
			      void **pages, int nr_pages, bool overwrite);
	void (*free_buffer)(void *config);
	int (*set_buffer)(struct coresight_device *csdev,
			  struct perf_output_handle *handle,
			  void *sink_config);
	unsigned long (*reset_buffer)(struct coresight_device *csdev,
				      struct perf_output_handle *handle,
				      void *sink_config);
	void (*update_buffer)(struct coresight_device *csdev,
			      struct perf_output_handle *handle,
			      void *sink_config);
};

/**
 * struct coresight_ops_link - basic operations for a link
 * Operations available for links.
 * @enable:	enables flow between iport and oport.
 * @disable:	disables flow between iport and oport.
 */
struct coresight_ops_link {
	int (*enable)(struct coresight_device *csdev, int iport, int oport);
	void (*disable)(struct coresight_device *csdev, int iport, int oport);
};

/**
 * struct coresight_ops_source - basic operations for a source
 * Operations available for sources.
 * @cpu_id:	returns the value of the CPU number this component
 *		is associated to.
 * @trace_id:	returns the value of the component's trace ID as known
 *		to the HW.
 * @enable:	enables tracing for a source.
 * @disable:	disables tracing for a source.
 */
struct coresight_ops_source {
	int (*cpu_id)(struct coresight_device *csdev);
	int (*trace_id)(struct coresight_device *csdev);
	int (*enable)(struct coresight_device *csdev,
		      struct perf_event *event,  u32 mode);
	void (*disable)(struct coresight_device *csdev,
			struct perf_event *event);
};

/**
 * struct coresight_ops_helper - Operations for a helper device.
 *
 * All operations could pass in a device specific data, which could
 * help the helper device to determine what to do.
 *
 * @enable	: Enable the device
 * @disable	: Disable the device
 */
struct coresight_ops_helper {
	int (*enable)(struct coresight_device *csdev, void *data);
	int (*disable)(struct coresight_device *csdev, void *data);
};

struct coresight_ops {
	const struct coresight_ops_sink *sink_ops;
	const struct coresight_ops_link *link_ops;
	const struct coresight_ops_source *source_ops;
	const struct coresight_ops_helper *helper_ops;
};

#ifdef CONFIG_CORESIGHT
extern struct coresight_device *
coresight_register(struct coresight_desc *desc);
extern void coresight_unregister(struct coresight_device *csdev);
extern int coresight_enable(struct coresight_device *csdev);
extern void coresight_disable(struct coresight_device *csdev);
extern int coresight_timeout(void __iomem *addr, u32 offset,
			     int position, int value);
extern void coresight_abort(void);
extern void coresight_disable_reg_clk(struct coresight_device *csdev);
extern int coresight_enable_reg_clk(struct coresight_device *csdev);
static inline bool coresight_link_late_disable(void)
{
	if (IS_ENABLED(CONFIG_CORESIGHT_LINK_LATE_DISABLE))
		return true;
	else
		return false;
}
extern void coresight_disable_all_source_link(void);
extern void coresight_enable_all_source_link(void);
#else
static inline struct coresight_device *
coresight_register(struct coresight_desc *desc) { return NULL; }
static inline void coresight_unregister(struct coresight_device *csdev) {}
static inline int
coresight_enable(struct coresight_device *csdev) { return -ENOSYS; }
static inline void coresight_disable(struct coresight_device *csdev) {}
static inline int coresight_timeout(void __iomem *addr, u32 offset,
				     int position, int value) { return 1; }
static inline void coresight_abort(void) {}
static inline void coresight_disable_reg_clk(struct coresight_device *csdev) {}
static inline int coresight_enable_reg_clk(struct coresight_device *csdev)
{ return -EINVAL; }
static void coresight_disable_all_source_link(void) {};
static void coresight_enable_all_source_link(void) {};
#endif

#if defined(CONFIG_OF) && defined(CONFIG_CORESIGHT)
extern int of_coresight_get_cpu(const struct device_node *node);
extern struct coresight_platform_data *
of_get_coresight_platform_data(struct device *dev,
			       const struct device_node *node);
extern int of_get_coresight_csr_name(struct device_node *node,
				const char **csr_name);

extern struct coresight_cti_data *of_get_coresight_cti_data(
				struct device *dev, struct device_node *node);
#else
static inline int of_coresight_get_cpu(const struct device_node *node)
{ return 0; }
static inline struct coresight_platform_data *of_get_coresight_platform_data(
	struct device *dev, const struct device_node *node) { return NULL; }
static inline int of_get_coresight_csr_name(struct device_node *node,
		const char **csr_name){ return -EINVAL; }
static inline struct coresight_cti_data *of_get_coresight_cti_data(
		struct device *dev, struct device_node *node) { return NULL; }
#endif

#endif
