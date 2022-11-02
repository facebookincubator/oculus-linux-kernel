/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 - 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _IPA_WDI3_H_
#define _IPA_WDI3_H_

#include <linux/ipa.h>

#define IPA_HW_WDI3_TCL_DATA_CMD_ER_DESC_SIZE 32
#define IPA_HW_WDI3_IPA2FW_ER_DESC_SIZE 8

#define IPA_HW_WDI3_MAX_ER_DESC_SIZE \
	(((IPA_HW_WDI3_TCL_DATA_CMD_ER_DESC_SIZE) > \
	(IPA_HW_WDI3_IPA2FW_ER_DESC_SIZE)) ?  \
	(IPA_HW_WDI3_TCL_DATA_CMD_ER_DESC_SIZE) : \
	(IPA_HW_WDI3_IPA2FW_ER_DESC_SIZE))

#define IPA_WDI_MAX_SUPPORTED_SYS_PIPE 3

enum ipa_wdi_version {
	IPA_WDI_1,
	IPA_WDI_2,
	IPA_WDI_3
};

#define IPA_WDI3_TX_DIR 1
#define IPA_WDI3_RX_DIR 2

/**
 * struct ipa_wdi_init_in_params - wdi init input parameters
 *
 * @wdi_version: wdi version
 * @notify: uc ready callback
 * @priv: uc ready callback cookie
 */
struct ipa_wdi_init_in_params {
	enum ipa_wdi_version wdi_version;
	ipa_uc_ready_cb notify;
	void *priv;
#ifdef IPA_WAN_MSG_IPv6_ADDR_GW_LEN
	ipa_wdi_meter_notifier_cb wdi_notify;
#endif
};

/**
 * struct ipa_wdi_init_out_params - wdi init output parameters
 *
 * @is_uC_ready: is uC ready. No API should be called until uC
    is ready.
 * @is_smmu_enable: is smmu enabled
 * @is_over_gsi: is wdi over GSI or uC
 */
struct ipa_wdi_init_out_params {
	bool is_uC_ready;
	bool is_smmu_enabled;
	bool is_over_gsi;
};

/**
 * struct ipa_wdi_hdr_info - Header to install on IPA HW
 *
 * @hdr: header to install on IPA HW
 * @hdr_len: length of header
 * @dst_mac_addr_offset: destination mac address offset
 * @hdr_type: layer two header type
 */
struct ipa_wdi_hdr_info {
	u8 *hdr;
	u8 hdr_len;
	u8 dst_mac_addr_offset;
	enum ipa_hdr_l2_type hdr_type;
};

/**
 * struct ipa_wdi_reg_intf_in_params - parameters for uC offload
 *	interface registration
 *
 * @netdev_name: network interface name
 * @hdr_info: header information
 * @is_meta_data_valid: if meta data is valid
 * @meta_data: meta data if any
 * @meta_data_mask: meta data mask
 */
struct ipa_wdi_reg_intf_in_params {
	const char *netdev_name;
	struct ipa_wdi_hdr_info hdr_info[IPA_IP_MAX];
	enum ipa_client_type alt_dst_pipe;
	u8 is_meta_data_valid;
	u32 meta_data;
	u32 meta_data_mask;
};

/**
 * struct  ipa_wdi_pipe_setup_info - WDI TX/Rx configuration
 * @ipa_ep_cfg: ipa endpoint configuration
 * @client: type of "client"
 * @transfer_ring_base_pa:  physical address of the base of the transfer ring
 * @transfer_ring_size:  size of the transfer ring
 * @transfer_ring_doorbell_pa:  physical address of the doorbell that
	IPA uC will update the tailpointer of the transfer ring
 * @is_txr_rn_db_pcie_addr: Bool indicated txr ring DB is pcie or not
 * @event_ring_base_pa:  physical address of the base of the event ring
 * @event_ring_size:  event ring size
 * @event_ring_doorbell_pa:  physical address of the doorbell that IPA uC
	will update the headpointer of the event ring
 * @is_evt_rn_db_pcie_addr: Bool indicated evt ring DB is pcie or not
 * @num_pkt_buffers:  Number of pkt buffers allocated. The size of the event
	ring and the transfer ring has to be atleast ( num_pkt_buffers + 1)
 * @pkt_offset: packet offset (wdi header length)
 * @desc_format_template[IPA_HW_WDI3_MAX_ER_DESC_SIZE]:  Holds a cached
	template of the desc format
 */
struct ipa_wdi_pipe_setup_info {
	struct ipa_ep_cfg ipa_ep_cfg;
	enum ipa_client_type client;
	phys_addr_t  transfer_ring_base_pa;
	u32  transfer_ring_size;
	phys_addr_t  transfer_ring_doorbell_pa;
	bool is_txr_rn_db_pcie_addr;

	phys_addr_t  event_ring_base_pa;
	u32  event_ring_size;
	phys_addr_t  event_ring_doorbell_pa;
	bool is_evt_rn_db_pcie_addr;
	u16  num_pkt_buffers;

	u16 pkt_offset;

	u32  desc_format_template[IPA_HW_WDI3_MAX_ER_DESC_SIZE];
};

/**
 * struct  ipa_wdi_pipe_setup_info_smmu - WDI TX/Rx configuration
 * @ipa_ep_cfg: ipa endpoint configuration
 * @client: type of "client"
 * @transfer_ring_base_pa:  physical address of the base of the transfer ring
 * @transfer_ring_size:  size of the transfer ring
 * @transfer_ring_doorbell_pa:  physical address of the doorbell that
	IPA uC will update the tailpointer of the transfer ring
 * @is_txr_rn_db_pcie_addr: Bool indicated  txr ring DB is pcie or not
 * @event_ring_base_pa:  physical address of the base of the event ring
 * @event_ring_size:  event ring size
 * @event_ring_doorbell_pa:  physical address of the doorbell that IPA uC
	will update the headpointer of the event ring
 * @is_evt_rn_db_pcie_addr: Bool indicated evt ring DB is pcie or not
 * @num_pkt_buffers:  Number of pkt buffers allocated. The size of the event
	ring and the transfer ring has to be atleast ( num_pkt_buffers + 1)
 * @pkt_offset: packet offset (wdi header length)
 * @desc_format_template[IPA_HW_WDI3_MAX_ER_DESC_SIZE]:  Holds a cached
	template of the desc format
 */
struct ipa_wdi_pipe_setup_info_smmu {
	struct ipa_ep_cfg ipa_ep_cfg;
	enum ipa_client_type client;
	struct sg_table  transfer_ring_base;
	u32  transfer_ring_size;
	phys_addr_t  transfer_ring_doorbell_pa;
	bool is_txr_rn_db_pcie_addr;

	struct sg_table  event_ring_base;
	u32  event_ring_size;
	phys_addr_t  event_ring_doorbell_pa;
	bool is_evt_rn_db_pcie_addr;
	u16  num_pkt_buffers;

	u16 pkt_offset;

	u32  desc_format_template[IPA_HW_WDI3_MAX_ER_DESC_SIZE];
};

/**
 * struct  ipa_wdi_conn_in_params - information provided by
 *		uC offload client
 * @notify: client callback function
 * @priv: client cookie
 * @is_smmu_enabled: if smmu is enabled
 * @num_sys_pipe_needed: number of sys pipe needed
 * @sys_in: parameters to setup sys pipe in mcc mode
 * @tx: parameters to connect TX pipe(from IPA to WLAN)
 * @tx_smmu: smmu parameters to connect TX pipe(from IPA to WLAN)
 * @rx: parameters to connect RX pipe(from WLAN to IPA)
 * @rx_smmu: smmu parameters to connect RX pipe(from WLAN to IPA)
 */
struct ipa_wdi_conn_in_params {
	ipa_notify_cb notify;
	void *priv;
	bool is_smmu_enabled;
	u8 num_sys_pipe_needed;
	struct ipa_sys_connect_params sys_in[IPA_WDI_MAX_SUPPORTED_SYS_PIPE];
	union {
		struct ipa_wdi_pipe_setup_info tx;
		struct ipa_wdi_pipe_setup_info_smmu tx_smmu;
	} u_tx;
	union {
		struct ipa_wdi_pipe_setup_info rx;
		struct ipa_wdi_pipe_setup_info_smmu rx_smmu;
	} u_rx;
};

/**
 * struct  ipa_wdi_conn_out_params - information provided
 *				to WLAN driver
 * @tx_uc_db_pa: physical address of IPA uC doorbell for TX
 * @rx_uc_db_pa: physical address of IPA uC doorbell for RX
 */
struct ipa_wdi_conn_out_params {
	phys_addr_t tx_uc_db_pa;
	phys_addr_t rx_uc_db_pa;
};

/**
 * struct  ipa_wdi_perf_profile - To set BandWidth profile
 *
 * @client: type of client
 * @max_supported_bw_mbps: maximum bandwidth needed (in Mbps)
 */
struct ipa_wdi_perf_profile {
	enum ipa_client_type client;
	u32 max_supported_bw_mbps;
};

#if defined CONFIG_IPA || defined CONFIG_IPA3

/**
 * ipa_wdi_init - Client should call this function to
 * init WDI IPA offload data path
 *
 * Note: Should not be called from atomic context and only
 * after checking IPA readiness using ipa_register_ipa_ready_cb()
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi_init(struct ipa_wdi_init_in_params *in,
	struct ipa_wdi_init_out_params *out);

/**
 * ipa_wdi_cleanup - Client should call this function to
 * clean up WDI IPA offload data path
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi_cleanup(void);

/**
 * ipa_wdi_reg_intf - Client should call this function to
 * register interface
 *
 * Note: Should not be called from atomic context
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi_reg_intf(
	struct ipa_wdi_reg_intf_in_params *in);

/**
 * ipa_wdi_dereg_intf - Client Driver should call this
 * function to deregister before unload and after disconnect
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi_dereg_intf(const char *netdev_name);

/**
 * ipa_wdi_conn_pipes - Client should call this
 * function to connect pipes
 *
 * @in:	[in] input parameters from client
 * @out: [out] output params to client
 *
 * Note: Should not be called from atomic context
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi_conn_pipes(struct ipa_wdi_conn_in_params *in,
	struct ipa_wdi_conn_out_params *out);

/**
 * ipa_wdi_disconn_pipes() - Client should call this
 *		function to disconnect pipes
 *
 * Note: Should not be called from atomic context
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wdi_disconn_pipes(void);

/**
 * ipa_wdi_enable_pipes() - Client should call this
 *		function to enable IPA offload data path
 *
 * Note: Should not be called from atomic context
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wdi_enable_pipes(void);

/**
 * ipa_wdi_disable_pipes() - Client should call this
 *		function to disable IPA offload data path
 *
 * Note: Should not be called from atomic context
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wdi_disable_pipes(void);

/**
 * ipa_wdi_set_perf_profile() - Client should call this function to
 *		set IPA clock bandwidth based on data rates
 *
 * @profile: [in] BandWidth profile to use
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wdi_set_perf_profile(struct ipa_wdi_perf_profile *profile);

/**
 * ipa_wdi_create_smmu_mapping() - Create smmu mapping
 *
 * @num_buffers: number of buffers
 *
 * @info: wdi buffer info
 */
int ipa_wdi_create_smmu_mapping(u32 num_buffers,
	struct ipa_wdi_buffer_info *info);

/**
 * ipa_wdi_release_smmu_mapping() - Release smmu mapping
 *
 * @num_buffers: number of buffers
 *
 * @info: wdi buffer info
 */
int ipa_wdi_release_smmu_mapping(u32 num_buffers,
	struct ipa_wdi_buffer_info *info);

/**
 * ipa_wdi_get_stats() - Query WDI statistics
 * @stats:	[inout] stats blob from client populated by driver
 *
 * Returns:	0 on success, negative on failure
 *
 * @note Cannot be called from atomic context
 *
 */
int ipa_wdi_get_stats(struct IpaHwStatsWDIInfoData_t *stats);


/**
 * ipa_wdi_bw_monitor() - set wdi BW monitoring
 * @info:	[inout] info blob from client populated by driver
 *
 * Returns:	0 on success, negative on failure
 *
 * @note Cannot be called from atomic context
 *
 */
int ipa_wdi_bw_monitor(struct ipa_wdi_bw_info *info);

/**
 * ipa_wdi_sw_stats() - set wdi BW monitoring
 * @info:	[inout] info blob from client populated by driver
 *
 * Returns:	0 on success, negative on failure
 *
 * @note Cannot be called from atomic context
 *
 */
int ipa_wdi_sw_stats(struct ipa_wdi_tx_info *info);

#else /* (CONFIG_IPA || CONFIG_IPA3) */

static inline int ipa_wdi_init(struct ipa_wdi_init_in_params *in,
	struct ipa_wdi_init_out_params *out)
{
	return -EPERM;
}

static inline int ipa_wdi_cleanup(void)
{
	return -EPERM;
}

static inline int ipa_wdi_reg_intf(
	struct ipa_wdi_reg_intf_in_params *in)
{
	return -EPERM;
}

static inline int ipa_wdi_dereg_intf(const char *netdev_name)
{
	return -EPERM;
}

static inline int ipa_wdi_conn_pipes(struct ipa_wdi_conn_in_params *in,
	struct ipa_wdi_conn_out_params *out)
{
	return -EPERM;
}

static inline int ipa_wdi_disconn_pipes(void)
{
	return -EPERM;
}

static inline int ipa_wdi_enable_pipes(void)
{
	return -EPERM;
}

static inline int ipa_wdi_disable_pipes(void)
{
	return -EPERM;
}

static inline int ipa_wdi_set_perf_profile(
	struct ipa_wdi_perf_profile *profile)
{
	return -EPERM;
}

static inline int ipa_wdi_create_smmu_mapping(u32 num_buffers,
	struct ipa_wdi_buffer_info *info)
{
	return -EPERM;
}

static inline int ipa_wdi_release_smmu_mapping(u32 num_buffers,
	struct ipa_wdi_buffer_info *info)
{
	return -EPERM;
}

static inline int ipa_wdi_get_stats(struct IpaHwStatsWDIInfoData_t *stats)
{
	return -EPERM;
}

static inline int ipa_wdi_bw_monitor(struct ipa_wdi_bw_info *info)
{
	return -EPERM;
}

static inline int ipa_wdi_sw_stats(struct ipa_wdi_tx_info *info)
{
	return -EPERM;
}

#endif /* CONFIG_IPA3 */

#endif /* _IPA_WDI3_H_ */
