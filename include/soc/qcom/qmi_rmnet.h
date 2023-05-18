/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _QMI_RMNET_H
#define _QMI_RMNET_H

#include <linux/netdevice.h>
#include <linux/skbuff.h>

struct qmi_rmnet_ps_ind {
	void (*ps_on_handler)(void *port);
	void (*ps_off_handler)(void *port);
	struct list_head list;
};


#ifdef CONFIG_QCOM_QMI_RMNET
void qmi_rmnet_qmi_exit(void *qmi_pt, void *port);
void qmi_rmnet_change_link(struct net_device *dev, void *port, void *tcm_pt);
void qmi_rmnet_enable_all_flows(struct net_device *dev);
bool qmi_rmnet_all_flows_enabled(struct net_device *dev);
void qmi_rmnet_prepare_ps_bearers(struct net_device *dev, u8 *num_bearers,
				  u8 *bearer_id);
#else
static inline void qmi_rmnet_qmi_exit(void *qmi_pt, void *port)
{
}

static inline void
qmi_rmnet_change_link(struct net_device *dev, void *port, void *tcm_pt)
{
}

static inline void
qmi_rmnet_enable_all_flows(struct net_device *dev)
{
}

static inline bool
qmi_rmnet_all_flows_enabled(struct net_device *dev)
{
	return true;
}

static inline void qmi_rmnet_prepare_ps_bearers(struct net_device *dev,
						u8 *num_bearers, u8 *bearer_id)
{
	if (num_bearers)
		*num_bearers = 0;
}

#endif

#ifdef CONFIG_QCOM_QMI_DFC
void *qmi_rmnet_qos_init(struct net_device *real_dev,
			 struct net_device *vnd_dev, u8 mux_id);
void qmi_rmnet_qos_exit_pre(void *qos);
void qmi_rmnet_qos_exit_post(void);
bool qmi_rmnet_get_flow_state(struct net_device *dev, struct sk_buff *skb,
			      bool *drop);
void qmi_rmnet_burst_fc_check(struct net_device *dev,
			      int ip_type, u32 mark, unsigned int len);
int qmi_rmnet_get_queue(struct net_device *dev, struct sk_buff *skb);
#else
static inline void *
qmi_rmnet_qos_init(struct net_device *real_dev,
		   struct net_device *vnd_dev, u8 mux_id)
{
	return NULL;
}

static inline void qmi_rmnet_qos_exit_pre(void *qos)
{
}

static inline void qmi_rmnet_qos_exit_post(void)
{
}

static inline bool qmi_rmnet_get_flow_state(struct net_device *dev,
					    struct sk_buff *skb,
					    bool *drop)
{
	return false;
}

static inline void
qmi_rmnet_burst_fc_check(struct net_device *dev,
			 int ip_type, u32 mark, unsigned int len)
{
}

static inline int qmi_rmnet_get_queue(struct net_device *dev,
				       struct sk_buff *skb)
{
	return 0;
}
#endif

#ifdef CONFIG_QCOM_QMI_POWER_COLLAPSE
int qmi_rmnet_set_powersave_mode(void *port, uint8_t enable);
void qmi_rmnet_work_init(void *port);
void qmi_rmnet_work_exit(void *port);
void qmi_rmnet_work_maybe_restart(void *port);
void qmi_rmnet_set_dl_msg_active(void *port);
bool qmi_rmnet_ignore_grant(void *port);

int qmi_rmnet_ps_ind_register(void *port,
			      struct qmi_rmnet_ps_ind *ps_ind);
int qmi_rmnet_ps_ind_deregister(void *port,
				struct qmi_rmnet_ps_ind *ps_ind);
void qmi_rmnet_ps_off_notify(void *port);
void qmi_rmnet_ps_on_notify(void *port);

#else
static inline int qmi_rmnet_set_powersave_mode(void *port, uint8_t enable)
{
	return 0;
}
static inline void qmi_rmnet_work_init(void *port)
{
}
static inline void qmi_rmnet_work_exit(void *port)
{
}
static inline void qmi_rmnet_work_maybe_restart(void *port)
{

}
static inline void qmi_rmnet_set_dl_msg_active(void *port)
{
}
static inline bool qmi_rmnet_ignore_grant(void *port)
{
	return false;
}

static inline int qmi_rmnet_ps_ind_register(struct rmnet_port *port,
				     struct qmi_rmnet_ps_ind *ps_ind)
{
	return 0;
}
static inline int qmi_rmnet_ps_ind_deregister(struct rmnet_port *port,
				       struct qmi_rmnet_ps_ind *ps_ind)
{
	return 0;
}

static inline void qmi_rmnet_ps_off_notify(struct rmnet_port *port)
{

}

static inline void qmi_rmnet_ps_on_notify(struct rmnet_port *port)
{

}
#endif
#endif /*_QMI_RMNET_H*/
