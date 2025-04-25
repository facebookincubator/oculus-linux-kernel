/*
 * Copyright (c) 2012-2014, 2017-2019, 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#if !defined(WLAN_HDD_IOCTL_H)
#define WLAN_HDD_IOCTL_H

#include <linux/netdevice.h>
#include <uapi/linux/if.h>
#include "wlan_hdd_main.h"

extern struct sock *cesium_nl_srv_sock;

/**
 * hdd_ioctl() - ioctl handler (wrapper) for wlan network interfaces
 * @dev: device upon which the ioctl was received
 * @ifr: ioctl request information
 * @cmd: ioctl command
 *
 * This function acts as an SSR-protecting wrapper to __hdd_ioctl()
 * which is where the ioctls are really handled.
 *
 * Return: 0 on success, non-zero on error
 */
int hdd_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);

/**
 * hdd_dev_private_ioctl() - private ioctl handler for wlan network interfaces
 * @dev: device upon which the ioctl was received
 * @ifr: ioctl request information
 * @data: pointer to the raw command data in the ioctl request
 * @cmd: ioctl command
 *
 * For Kernel 5.15+, this function acts as an SSR-protecting wrapper
 * to __hdd_ioctl(), which is where the ioctls are really handled.
 *
 * Return: 0 on success, non-zero on error
 */
int hdd_dev_private_ioctl(struct net_device *dev, struct ifreq *ifr,
			  void __user *data, int cmd);

/**
 * wlan_hdd_set_mc_rate() - Function to set MC rate.
 * @link_info: Link info pointer in HDD adapter
 * @target_rate: Target rate to set.
 *
 * The API sets the value in @target_rate for MC Tx
 *
 * Return: Non-zero value on failure.
 */
int wlan_hdd_set_mc_rate(struct wlan_hdd_link_info *link_info, int target_rate);

/**
 * hdd_update_smps_antenna_mode() - set smps and antenna mode
 * @hdd_ctx: Pointer to hdd context
 * @mode: antenna mode
 *
 * This function will set smps and antenna mode.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_update_smps_antenna_mode(struct hdd_context *hdd_ctx, int mode);

/**
 * hdd_set_antenna_mode() - SET ANTENNA MODE command handler
 * @link_info: Link info pointer in HDD adapter
 * @mode: new antenna mode
 */
int hdd_set_antenna_mode(struct wlan_hdd_link_info *link_info, int mode);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * hdd_get_roam_scan_ch_cb() - roam scan channel list callback handler
 * @hdd_handle: Pointer to hdd context
 * @roam_ch: pointer to roam scan ch event data
 * @context: cookie
 *
 * Callback function to processes roam scan chaanel list event. If
 * command response field in the response message is set that means
 * event received as a response of GETROAMSCANCHANNELS command else
 * event was raised by firmware upon disconnection.
 *
 * Return: none
 */
void hdd_get_roam_scan_ch_cb(hdd_handle_t hdd_handle,
			     struct roam_scan_ch_resp *roam_ch,
			     void *context);

/**
 * hdd_get_roam_scan_freq() - roam scan freq list
 * @adapter: Pointer to hdd adapter
 * @mac_handle: pointer to mac_handle
 * @chan_list: Pointer to hold roam scan freq list
 * @num_channels: Pointer to hold num of roam scan channels in list
 *
 * This function gets roam scan frequencies from FW if FW is capable else
 * roam scan frequencies are taken from host maintained list.
 *
 * Return: 0 on success else error value
 */
int
hdd_get_roam_scan_freq(struct hdd_adapter *adapter, mac_handle_t mac_handle,
		       uint32_t *chan_list, uint8_t *num_channels);
#else
static inline void
hdd_get_roam_scan_ch_cb(hdd_handle_t hdd_handle,
			void *roam_ch,
			void *context)
{
}

static inline int
hdd_get_roam_scan_freq(struct hdd_adapter *adapter, mac_handle_t mac_handle,
		       uint32_t *chan_list, uint8_t *num_channels)
{
	return -EFAULT;
}
#endif

/**
 * hdd_ioctl_log_buffer() - dump log buffer of a type
 * @log_id: id of what log type to be
 * @count: number of lines to be copied
 * @custom_print: custom print function pointer
 * @print_ctx: print context for custom print function
 *
 * If custom print function is NULL, will default to printk
 *
 * Return: None
 */
void hdd_ioctl_log_buffer(int log_id, uint32_t count, qdf_abstract_print
							     *custom_print,
							     void *print_ctx);
#ifdef WLAN_DUMP_LOG_BUF_CNT
/**
 * hdd_dump_log_buffer() - dump log buffer history
 * @print_ctx: print context for custom print function
 * @custom_print: custom print function pointer
 *
 * If custom print function is NULL, will default to printk
 *
 * Return: None
 */
void hdd_dump_log_buffer(void *print_ctx, qdf_abstract_print *custom_print);

#else
static inline
void hdd_dump_log_buffer(void *print_ctx, qdf_abstract_print *custom_print)
{
}

#endif
#endif /* end #if !defined(WLAN_HDD_IOCTL_H) */
