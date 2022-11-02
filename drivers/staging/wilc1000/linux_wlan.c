// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2012 - 2018 Microchip Technology Inc., and its subsidiaries.
 * All rights reserved.
 */

#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/firmware.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>

#include "wilc_wfi_cfgoperations.h"

bool wilc_enable_ps = true;

static int dev_state_ev_handler(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	struct in_ifaddr *dev_iface = ptr;
	struct wilc_priv *priv;
	struct host_if_drv *hif_drv;
	struct net_device *dev;
	u8 *ip_addr_buf;
	struct wilc_vif *vif;
	u8 null_ip[4] = {0};
	char wlan_dev_name[5] = "wlan0";

	if (!dev_iface || !dev_iface->ifa_dev || !dev_iface->ifa_dev->dev)
		return NOTIFY_DONE;

	if (memcmp(dev_iface->ifa_label, "wlan0", 5) &&
	    memcmp(dev_iface->ifa_label, "p2p0", 4))
		return NOTIFY_DONE;

	dev  = (struct net_device *)dev_iface->ifa_dev->dev;
	if (!dev->ieee80211_ptr || !dev->ieee80211_ptr->wiphy)
		return NOTIFY_DONE;

	priv = wiphy_priv(dev->ieee80211_ptr->wiphy);
	if (!priv)
		return NOTIFY_DONE;

	hif_drv = (struct host_if_drv *)priv->hif_drv;
	vif = netdev_priv(dev);
	if (!vif || !hif_drv)
		return NOTIFY_DONE;

	switch (event) {
	case NETDEV_UP:
		if (vif->iftype == STATION_MODE || vif->iftype == CLIENT_MODE) {
			hif_drv->ifc_up = 1;
			wilc_optaining_ip = false;
			del_timer(&wilc_during_ip_timer);
		}

		if (wilc_enable_ps)
			wilc_set_power_mgmt(vif, 1, 0);

		netdev_dbg(dev, "[%s] Up IP\n", dev_iface->ifa_label);

		ip_addr_buf = (char *)&dev_iface->ifa_address;
		netdev_dbg(dev, "IP add=%d:%d:%d:%d\n",
			   ip_addr_buf[0], ip_addr_buf[1],
			   ip_addr_buf[2], ip_addr_buf[3]);
		wilc_setup_ipaddress(vif, ip_addr_buf, vif->idx);

		break;

	case NETDEV_DOWN:
		if (vif->iftype == STATION_MODE || vif->iftype == CLIENT_MODE) {
			hif_drv->ifc_up = 0;
			wilc_optaining_ip = false;
		}

		if (memcmp(dev_iface->ifa_label, wlan_dev_name, 5) == 0)
			wilc_set_power_mgmt(vif, 0, 0);

		wilc_resolve_disconnect_aberration(vif);

		netdev_dbg(dev, "[%s] Down IP\n", dev_iface->ifa_label);

		ip_addr_buf = null_ip;
		netdev_dbg(dev, "IP add=%d:%d:%d:%d\n",
			   ip_addr_buf[0], ip_addr_buf[1],
			   ip_addr_buf[2], ip_addr_buf[3]);

		wilc_setup_ipaddress(vif, ip_addr_buf, vif->idx);

		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}

static irqreturn_t isr_uh_routine(int irq, void *user_data)
{
	struct net_device *dev = user_data;
	struct wilc_vif *vif = netdev_priv(dev);
	struct wilc *wilc = vif->wilc;

	if (wilc->close) {
		netdev_err(dev, "Can't handle UH interrupt\n");
		return IRQ_HANDLED;
	}
	return IRQ_WAKE_THREAD;
}

static irqreturn_t isr_bh_routine(int irq, void *userdata)
{
	struct net_device *dev = userdata;
	struct wilc_vif *vif = netdev_priv(userdata);
	struct wilc *wilc = vif->wilc;

	if (wilc->close) {
		netdev_err(dev, "Can't handle BH interrupt\n");
		return IRQ_HANDLED;
	}

	wilc_handle_isr(wilc);

	return IRQ_HANDLED;
}

static int init_irq(struct net_device *dev)
{
	int ret = 0;
	struct wilc_vif *vif = netdev_priv(dev);
	struct wilc *wl = vif->wilc;

	ret = gpiod_direction_input(wl->gpio_irq);
	if (ret) {
		netdev_err(dev, "could not obtain gpio for WILC_INTR\n");
		return ret;
	}

	wl->dev_irq_num = gpiod_to_irq(wl->gpio_irq);

	ret = request_threaded_irq(wl->dev_irq_num, isr_uh_routine,
				   isr_bh_routine,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   "WILC_IRQ", dev);
	if (ret < 0)
		netdev_err(dev, "Failed to request IRQ\n");
	else
		netdev_dbg(dev, "IRQ request succeeded IRQ-NUM= %d\n",
			   wl->dev_irq_num);

	return ret;
}

static void deinit_irq(struct net_device *dev)
{
	struct wilc_vif *vif = netdev_priv(dev);
	struct wilc *wilc = vif->wilc;

	/* Deinitialize IRQ */
	if (wilc->dev_irq_num)
		free_irq(wilc->dev_irq_num, wilc);
}

void wilc_mac_indicate(struct wilc *wilc)
{
	int status;

	wilc_wlan_cfg_get_val(WID_STATUS, (unsigned char *)&status, 4);
	if (wilc->mac_status == MAC_STATUS_INIT) {
		wilc->mac_status = status;
		complete(&wilc->sync_event);
	} else {
		wilc->mac_status = status;
	}
}

static struct net_device *get_if_handler(struct wilc *wilc, u8 *mac_header)
{
	u8 *bssid, *bssid1;
	int i = 0;

	bssid = mac_header + 10;
	bssid1 = mac_header + 4;

	for (i = 0; i < wilc->vif_num; i++) {
		if (wilc->vif[i]->mode == STATION_MODE)
			if (ether_addr_equal_unaligned(bssid,
						       wilc->vif[i]->bssid))
				return wilc->vif[i]->ndev;
		if (wilc->vif[i]->mode == AP_MODE)
			if (ether_addr_equal_unaligned(bssid1,
						       wilc->vif[i]->bssid))
				return wilc->vif[i]->ndev;
	}

	return NULL;
}

int wilc_wlan_set_bssid(struct net_device *wilc_netdev, u8 *bssid, u8 mode)
{
	struct wilc_vif *vif = netdev_priv(wilc_netdev);

	memcpy(vif->bssid, bssid, 6);
	vif->mode = mode;

	return 0;
}

int wilc_wlan_get_num_conn_ifcs(struct wilc *wilc)
{
	u8 i = 0;
	u8 null_bssid[6] = {0};
	u8 ret_val = 0;

	for (i = 0; i < wilc->vif_num; i++)
		if (memcmp(wilc->vif[i]->bssid, null_bssid, 6))
			ret_val++;

	return ret_val;
}

static int linux_wlan_txq_task(void *vp)
{
	int ret;
	u32 txq_count;
	struct net_device *dev = vp;
	struct wilc_vif *vif = netdev_priv(dev);
	struct wilc *wl = vif->wilc;

	complete(&wl->txq_thread_started);
	while (1) {
		wait_for_completion(&wl->txq_event);

		if (wl->close) {
			complete(&wl->txq_thread_started);

			while (!kthread_should_stop())
				schedule();
			break;
		}
		do {
			ret = wilc_wlan_handle_txq(dev, &txq_count);
			if (txq_count < FLOW_CONTROL_LOWER_THRESHOLD) {
				if (netif_queue_stopped(wl->vif[0]->ndev))
					netif_wake_queue(wl->vif[0]->ndev);
				if (netif_queue_stopped(wl->vif[1]->ndev))
					netif_wake_queue(wl->vif[1]->ndev);
			}
		} while (ret == WILC_TX_ERR_NO_BUF && !wl->close);
	}
	return 0;
}

static int wilc_wlan_get_firmware(struct net_device *dev)
{
	struct wilc_vif *vif = netdev_priv(dev);
	struct wilc *wilc = vif->wilc;
	int chip_id, ret = 0;
	const struct firmware *wilc_firmware;
	char *firmware;

	chip_id = wilc_get_chipid(wilc, false);

	if (chip_id < 0x1003a0)
		firmware = FIRMWARE_1002;
	else
		firmware = FIRMWARE_1003;

	netdev_info(dev, "loading firmware %s\n", firmware);

	if (!(&vif->ndev->dev))
		goto fail;

	if (request_firmware(&wilc_firmware, firmware, wilc->dev) != 0) {
		netdev_err(dev, "%s - firmware not available\n", firmware);
		ret = -1;
		goto fail;
	}
	wilc->firmware = wilc_firmware;

fail:

	return ret;
}

static int linux_wlan_start_firmware(struct net_device *dev)
{
	struct wilc_vif *vif = netdev_priv(dev);
	struct wilc *wilc = vif->wilc;
	int ret = 0;

	ret = wilc_wlan_start(wilc);
	if (ret < 0)
		return ret;

	if (!wait_for_completion_timeout(&wilc->sync_event,
					 msecs_to_jiffies(5000)))
		return -ETIME;

	return 0;
}

static int wilc1000_firmware_download(struct net_device *dev)
{
	struct wilc_vif *vif = netdev_priv(dev);
	struct wilc *wilc = vif->wilc;
	int ret = 0;

	if (!wilc->firmware) {
		netdev_err(dev, "Firmware buffer is NULL\n");
		return -ENOBUFS;
	}

	ret = wilc_wlan_firmware_download(wilc, wilc->firmware->data,
					  wilc->firmware->size);
	if (ret < 0)
		return ret;

	release_firmware(wilc->firmware);
	wilc->firmware = NULL;

	netdev_dbg(dev, "Download Succeeded\n");

	return 0;
}

static int linux_wlan_init_test_config(struct net_device *dev,
				       struct wilc_vif *vif)
{
	unsigned char c_val[64];
	struct wilc *wilc = vif->wilc;
	struct wilc_priv *priv;
	struct host_if_drv *hif_drv;

	netdev_dbg(dev, "Start configuring Firmware\n");
	priv = wiphy_priv(dev->ieee80211_ptr->wiphy);
	hif_drv = (struct host_if_drv *)priv->hif_drv;
	netdev_dbg(dev, "Host = %p\n", hif_drv);
	wilc_get_chipid(wilc, false);

	*(int *)c_val = 1;

	if (!wilc_wlan_cfg_set(vif, 1, WID_SET_DRV_HANDLER, c_val, 4, 0, 0))
		goto fail;

	c_val[0] = 0;
	if (!wilc_wlan_cfg_set(vif, 0, WID_PC_TEST_MODE, c_val, 1, 0, 0))
		goto fail;

	c_val[0] = INFRASTRUCTURE;
	if (!wilc_wlan_cfg_set(vif, 0, WID_BSS_TYPE, c_val, 1, 0, 0))
		goto fail;

	c_val[0] = AUTORATE;
	if (!wilc_wlan_cfg_set(vif, 0, WID_CURRENT_TX_RATE, c_val, 1, 0, 0))
		goto fail;

	c_val[0] = G_MIXED_11B_2_MODE;
	if (!wilc_wlan_cfg_set(vif, 0, WID_11G_OPERATING_MODE, c_val, 1, 0,
			       0))
		goto fail;

	c_val[0] = 1;
	if (!wilc_wlan_cfg_set(vif, 0, WID_CURRENT_CHANNEL, c_val, 1, 0, 0))
		goto fail;

	c_val[0] = G_SHORT_PREAMBLE;
	if (!wilc_wlan_cfg_set(vif, 0, WID_PREAMBLE, c_val, 1, 0, 0))
		goto fail;

	c_val[0] = AUTO_PROT;
	if (!wilc_wlan_cfg_set(vif, 0, WID_11N_PROT_MECH, c_val, 1, 0, 0))
		goto fail;

	c_val[0] = ACTIVE_SCAN;
	if (!wilc_wlan_cfg_set(vif, 0, WID_SCAN_TYPE, c_val, 1, 0, 0))
		goto fail;

	c_val[0] = SITE_SURVEY_OFF;
	if (!wilc_wlan_cfg_set(vif, 0, WID_SITE_SURVEY, c_val, 1, 0, 0))
		goto fail;

	*((int *)c_val) = 0xffff;
	if (!wilc_wlan_cfg_set(vif, 0, WID_RTS_THRESHOLD, c_val, 2, 0, 0))
		goto fail;

	*((int *)c_val) = 2346;
	if (!wilc_wlan_cfg_set(vif, 0, WID_FRAG_THRESHOLD, c_val, 2, 0, 0))
		goto fail;

	c_val[0] = 0;
	if (!wilc_wlan_cfg_set(vif, 0, WID_BCAST_SSID, c_val, 1, 0, 0))
		goto fail;

	c_val[0] = 1;
	if (!wilc_wlan_cfg_set(vif, 0, WID_QOS_ENABLE, c_val, 1, 0, 0))
		goto fail;

	c_val[0] = NO_POWERSAVE;
	if (!wilc_wlan_cfg_set(vif, 0, WID_POWER_MANAGEMENT, c_val, 1, 0, 0))
		goto fail;

	c_val[0] = NO_SECURITY; /* NO_ENCRYPT, 0x79 */
	if (!wilc_wlan_cfg_set(vif, 0, WID_11I_MODE, c_val, 1, 0, 0))
		goto fail;

	c_val[0] = OPEN_SYSTEM;
	if (!wilc_wlan_cfg_set(vif, 0, WID_AUTH_TYPE, c_val, 1, 0, 0))
		goto fail;

	strcpy(c_val, "123456790abcdef1234567890");
	if (!wilc_wlan_cfg_set(vif, 0, WID_WEP_KEY_VALUE, c_val,
			       (strlen(c_val) + 1), 0, 0))
		goto fail;

	strcpy(c_val, "12345678");
	if (!wilc_wlan_cfg_set(vif, 0, WID_11I_PSK, c_val, (strlen(c_val)), 0,
			       0))
		goto fail;

	strcpy(c_val, "password");
	if (!wilc_wlan_cfg_set(vif, 0, WID_1X_KEY, c_val, (strlen(c_val) + 1),
			       0, 0))
		goto fail;

	c_val[0] = 192;
	c_val[1] = 168;
	c_val[2] = 1;
	c_val[3] = 112;
	if (!wilc_wlan_cfg_set(vif, 0, WID_1X_SERV_ADDR, c_val, 4, 0, 0))
		goto fail;

	c_val[0] = 3;
	if (!wilc_wlan_cfg_set(vif, 0, WID_LISTEN_INTERVAL, c_val, 1, 0, 0))
		goto fail;

	c_val[0] = 3;
	if (!wilc_wlan_cfg_set(vif, 0, WID_DTIM_PERIOD, c_val, 1, 0, 0))
		goto fail;

	c_val[0] = NORMAL_ACK;
	if (!wilc_wlan_cfg_set(vif, 0, WID_ACK_POLICY, c_val, 1, 0, 0))
		goto fail;

	c_val[0] = 0;
	if (!wilc_wlan_cfg_set(vif, 0, WID_USER_CONTROL_ON_TX_POWER, c_val, 1,
			       0, 0))
		goto fail;

	c_val[0] = 48;
	if (!wilc_wlan_cfg_set(vif, 0, WID_TX_POWER_LEVEL_11A, c_val, 1, 0,
			       0))
		goto fail;

	c_val[0] = 28;
	if (!wilc_wlan_cfg_set(vif, 0, WID_TX_POWER_LEVEL_11B, c_val, 1, 0,
			       0))
		goto fail;

	*((int *)c_val) = 100;
	if (!wilc_wlan_cfg_set(vif, 0, WID_BEACON_INTERVAL, c_val, 2, 0, 0))
		goto fail;

	c_val[0] = REKEY_DISABLE;
	if (!wilc_wlan_cfg_set(vif, 0, WID_REKEY_POLICY, c_val, 1, 0, 0))
		goto fail;

	*((int *)c_val) = 84600;
	if (!wilc_wlan_cfg_set(vif, 0, WID_REKEY_PERIOD, c_val, 4, 0, 0))
		goto fail;

	*((int *)c_val) = 500;
	if (!wilc_wlan_cfg_set(vif, 0, WID_REKEY_PACKET_COUNT, c_val, 4, 0,
			       0))
		goto fail;

	c_val[0] = 1;
	if (!wilc_wlan_cfg_set(vif, 0, WID_SHORT_SLOT_ALLOWED, c_val, 1, 0,
			       0))
		goto fail;

	c_val[0] = G_SELF_CTS_PROT;
	if (!wilc_wlan_cfg_set(vif, 0, WID_11N_ERP_PROT_TYPE, c_val, 1, 0, 0))
		goto fail;

	c_val[0] = 1;
	if (!wilc_wlan_cfg_set(vif, 0, WID_11N_ENABLE, c_val, 1, 0, 0))
		goto fail;

	c_val[0] = HT_MIXED_MODE;
	if (!wilc_wlan_cfg_set(vif, 0, WID_11N_OPERATING_MODE, c_val, 1, 0,
			       0))
		goto fail;

	c_val[0] = 1;
	if (!wilc_wlan_cfg_set(vif, 0, WID_11N_TXOP_PROT_DISABLE, c_val, 1, 0,
			       0))
		goto fail;

	c_val[0] = DETECT_PROTECT_REPORT;
	if (!wilc_wlan_cfg_set(vif, 0, WID_11N_OBSS_NONHT_DETECTION, c_val, 1,
			       0, 0))
		goto fail;

	c_val[0] = RTS_CTS_NONHT_PROT;
	if (!wilc_wlan_cfg_set(vif, 0, WID_11N_HT_PROT_TYPE, c_val, 1, 0, 0))
		goto fail;

	c_val[0] = 0;
	if (!wilc_wlan_cfg_set(vif, 0, WID_11N_RIFS_PROT_ENABLE, c_val, 1, 0,
			       0))
		goto fail;

	c_val[0] = MIMO_MODE;
	if (!wilc_wlan_cfg_set(vif, 0, WID_11N_SMPS_MODE, c_val, 1, 0, 0))
		goto fail;

	c_val[0] = 7;
	if (!wilc_wlan_cfg_set(vif, 0, WID_11N_CURRENT_TX_MCS, c_val, 1, 0,
			       0))
		goto fail;

	c_val[0] = 1;
	if (!wilc_wlan_cfg_set(vif, 0, WID_11N_IMMEDIATE_BA_ENABLED, c_val, 1,
			       1, 1))
		goto fail;

	return 0;

fail:
	return -1;
}

static int wlan_deinit_locks(struct net_device *dev)
{
	struct wilc_vif *vif = netdev_priv(dev);
	struct wilc *wilc = vif->wilc;

	mutex_destroy(&wilc->hif_cs);
	mutex_destroy(&wilc->rxq_cs);
	mutex_destroy(&wilc->txq_add_to_head_cs);

	return 0;
}

static void wlan_deinitialize_threads(struct net_device *dev)
{
	struct wilc_vif *vif = netdev_priv(dev);
	struct wilc *wl = vif->wilc;

	wl->close = 1;

	complete(&wl->txq_event);

	if (wl->txq_thread) {
		kthread_stop(wl->txq_thread);
		wl->txq_thread = NULL;
	}
}

static void wilc_wlan_deinitialize(struct net_device *dev)
{
	struct wilc_vif *vif = netdev_priv(dev);
	struct wilc *wl = vif->wilc;

	if (!wl) {
		netdev_err(dev, "wl is NULL\n");
		return;
	}

	if (wl->initialized) {
		netdev_info(dev, "Deinitializing wilc1000...\n");

		if (!wl->dev_irq_num &&
		    wl->hif_func->disable_interrupt) {
			mutex_lock(&wl->hif_cs);
			wl->hif_func->disable_interrupt(wl);
			mutex_unlock(&wl->hif_cs);
		}
		complete(&wl->txq_event);

		wlan_deinitialize_threads(dev);
		deinit_irq(dev);

		wilc_wlan_stop(wl);
		wilc_wlan_cleanup(dev);
		wlan_deinit_locks(dev);

		wl->initialized = false;

		netdev_dbg(dev, "wilc1000 deinitialization Done\n");
	} else {
		netdev_dbg(dev, "wilc1000 is not initialized\n");
	}
}

static int wlan_init_locks(struct net_device *dev)
{
	struct wilc_vif *vif = netdev_priv(dev);
	struct wilc *wl = vif->wilc;

	mutex_init(&wl->hif_cs);
	mutex_init(&wl->rxq_cs);

	spin_lock_init(&wl->txq_spinlock);
	mutex_init(&wl->txq_add_to_head_cs);

	init_completion(&wl->txq_event);

	init_completion(&wl->cfg_event);
	init_completion(&wl->sync_event);
	init_completion(&wl->txq_thread_started);

	return 0;
}

static int wlan_initialize_threads(struct net_device *dev)
{
	struct wilc_vif *vif = netdev_priv(dev);
	struct wilc *wilc = vif->wilc;

	wilc->txq_thread = kthread_run(linux_wlan_txq_task, (void *)dev,
				       "K_TXQ_TASK");
	if (IS_ERR(wilc->txq_thread)) {
		netdev_err(dev, "couldn't create TXQ thread\n");
		wilc->close = 0;
		return PTR_ERR(wilc->txq_thread);
	}
	wait_for_completion(&wilc->txq_thread_started);

	return 0;
}

static int wilc_wlan_initialize(struct net_device *dev, struct wilc_vif *vif)
{
	int ret = 0;
	struct wilc *wl = vif->wilc;

	if (!wl->initialized) {
		wl->mac_status = MAC_STATUS_INIT;
		wl->close = 0;

		wlan_init_locks(dev);

		ret = wilc_wlan_init(dev);
		if (ret < 0) {
			ret = -EIO;
			goto fail_locks;
		}

		ret = wlan_initialize_threads(dev);
		if (ret < 0) {
			ret = -EIO;
			goto fail_wilc_wlan;
		}

		if (wl->gpio_irq && init_irq(dev)) {
			ret = -EIO;
			goto fail_threads;
		}

		if (!wl->dev_irq_num &&
		    wl->hif_func->enable_interrupt &&
		    wl->hif_func->enable_interrupt(wl)) {
			ret = -EIO;
			goto fail_irq_init;
		}

		if (wilc_wlan_get_firmware(dev)) {
			ret = -EIO;
			goto fail_irq_enable;
		}

		ret = wilc1000_firmware_download(dev);
		if (ret < 0) {
			ret = -EIO;
			goto fail_irq_enable;
		}

		ret = linux_wlan_start_firmware(dev);
		if (ret < 0) {
			ret = -EIO;
			goto fail_irq_enable;
		}

		if (wilc_wlan_cfg_get(vif, 1, WID_FIRMWARE_VERSION, 1, 0)) {
			int size;
			char firmware_ver[20];

			size = wilc_wlan_cfg_get_val(WID_FIRMWARE_VERSION,
						     firmware_ver,
						     sizeof(firmware_ver));
			firmware_ver[size] = '\0';
			netdev_dbg(dev, "Firmware Ver = %s\n", firmware_ver);
		}
		ret = linux_wlan_init_test_config(dev, vif);

		if (ret < 0) {
			netdev_err(dev, "Failed to configure firmware\n");
			ret = -EIO;
			goto fail_fw_start;
		}

		wl->initialized = true;
		return 0;

fail_fw_start:
		wilc_wlan_stop(wl);

fail_irq_enable:
		if (!wl->dev_irq_num &&
		    wl->hif_func->disable_interrupt)
			wl->hif_func->disable_interrupt(wl);
fail_irq_init:
		if (wl->dev_irq_num)
			deinit_irq(dev);
fail_threads:
		wlan_deinitialize_threads(dev);
fail_wilc_wlan:
		wilc_wlan_cleanup(dev);
fail_locks:
		wlan_deinit_locks(dev);
		netdev_err(dev, "WLAN initialization FAILED\n");
	} else {
		netdev_dbg(dev, "wilc1000 already initialized\n");
	}
	return ret;
}

static int mac_init_fn(struct net_device *ndev)
{
	netif_start_queue(ndev);
	netif_stop_queue(ndev);

	return 0;
}

static int wilc_mac_open(struct net_device *ndev)
{
	struct wilc_vif *vif = netdev_priv(ndev);
	struct wilc *wl = vif->wilc;
	unsigned char mac_add[ETH_ALEN] = {0};
	int ret = 0;
	int i = 0;

	if (!wl || !wl->dev) {
		netdev_err(ndev, "device not ready\n");
		return -ENODEV;
	}

	netdev_dbg(ndev, "MAC OPEN[%p]\n", ndev);

	ret = wilc_init_host_int(ndev);
	if (ret < 0)
		return ret;

	ret = wilc_wlan_initialize(ndev, vif);
	if (ret < 0) {
		wilc_deinit_host_int(ndev);
		return ret;
	}

	for (i = 0; i < wl->vif_num; i++) {
		if (ndev == wl->vif[i]->ndev) {
			wilc_set_wfi_drv_handler(vif, wilc_get_vif_idx(vif),
						 vif->iftype, vif->ifc_id);
			wilc_set_operation_mode(vif, vif->iftype);
			break;
		}
	}

	wilc_get_mac_address(vif, mac_add);
	netdev_dbg(ndev, "Mac address: %pM\n", mac_add);
	memcpy(wl->vif[i]->src_addr, mac_add, ETH_ALEN);
	memcpy(ndev->dev_addr, wl->vif[i]->src_addr, ETH_ALEN);

	if (!is_valid_ether_addr(ndev->dev_addr)) {
		netdev_err(ndev, "Wrong MAC address\n");
		wilc_deinit_host_int(ndev);
		wilc_wlan_deinitialize(ndev);
		return -EINVAL;
	}

	wilc_mgmt_frame_register(vif->ndev->ieee80211_ptr->wiphy,
				 vif->ndev->ieee80211_ptr,
				 vif->frame_reg[0].type,
				 vif->frame_reg[0].reg);
	wilc_mgmt_frame_register(vif->ndev->ieee80211_ptr->wiphy,
				 vif->ndev->ieee80211_ptr,
				 vif->frame_reg[1].type,
				 vif->frame_reg[1].reg);
	netif_wake_queue(ndev);
	wl->open_ifcs++;
	vif->mac_opened = 1;
	return 0;
}

static struct net_device_stats *mac_stats(struct net_device *dev)
{
	struct wilc_vif *vif = netdev_priv(dev);

	return &vif->netstats;
}

static void wilc_set_multicast_list(struct net_device *dev)
{
	struct netdev_hw_addr *ha;
	struct wilc_vif *vif = netdev_priv(dev);
	int i = 0;

	if (dev->flags & IFF_PROMISC)
		return;

	if (dev->flags & IFF_ALLMULTI ||
	    dev->mc.count > WILC_MULTICAST_TABLE_SIZE) {
		wilc_setup_multicast_filter(vif, false, 0);
		return;
	}

	if (dev->mc.count == 0) {
		wilc_setup_multicast_filter(vif, true, 0);
		return;
	}

	netdev_for_each_mc_addr(ha, dev) {
		memcpy(wilc_multicast_mac_addr_list[i], ha->addr, ETH_ALEN);
		netdev_dbg(dev, "Entry[%d]: %x:%x:%x:%x:%x:%x\n", i,
			   wilc_multicast_mac_addr_list[i][0],
			   wilc_multicast_mac_addr_list[i][1],
			   wilc_multicast_mac_addr_list[i][2],
			   wilc_multicast_mac_addr_list[i][3],
			   wilc_multicast_mac_addr_list[i][4],
			   wilc_multicast_mac_addr_list[i][5]);
		i++;
	}

	wilc_setup_multicast_filter(vif, true, (dev->mc.count));
}

static void linux_wlan_tx_complete(void *priv, int status)
{
	struct tx_complete_data *pv_data = priv;

	dev_kfree_skb(pv_data->skb);
	kfree(pv_data);
}

netdev_tx_t wilc_mac_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct wilc_vif *vif = netdev_priv(ndev);
	struct wilc *wilc = vif->wilc;
	struct tx_complete_data *tx_data = NULL;
	int queue_count;
	char *udp_buf;
	struct iphdr *ih;
	struct ethhdr *eth_h;

	if (skb->dev != ndev) {
		netdev_err(ndev, "Packet not destined to this device\n");
		return 0;
	}

	tx_data = kmalloc(sizeof(*tx_data), GFP_ATOMIC);
	if (!tx_data) {
		dev_kfree_skb(skb);
		netif_wake_queue(ndev);
		return 0;
	}

	tx_data->buff = skb->data;
	tx_data->size = skb->len;
	tx_data->skb  = skb;

	eth_h = (struct ethhdr *)(skb->data);
	if (eth_h->h_proto == cpu_to_be16(0x8e88))
		netdev_dbg(ndev, "EAPOL transmitted\n");

	ih = (struct iphdr *)(skb->data + sizeof(struct ethhdr));

	udp_buf = (char *)ih + sizeof(struct iphdr);
	if ((udp_buf[1] == 68 && udp_buf[3] == 67) ||
	    (udp_buf[1] == 67 && udp_buf[3] == 68))
		netdev_dbg(ndev, "DHCP Message transmitted, type:%x %x %x\n",
			   udp_buf[248], udp_buf[249], udp_buf[250]);

	vif->netstats.tx_packets++;
	vif->netstats.tx_bytes += tx_data->size;
	tx_data->bssid = wilc->vif[vif->idx]->bssid;
	queue_count = wilc_wlan_txq_add_net_pkt(ndev, (void *)tx_data,
						tx_data->buff, tx_data->size,
						linux_wlan_tx_complete);

	if (queue_count > FLOW_CONTROL_UPPER_THRESHOLD) {
		netif_stop_queue(wilc->vif[0]->ndev);
		netif_stop_queue(wilc->vif[1]->ndev);
	}

	return 0;
}

static int wilc_mac_close(struct net_device *ndev)
{
	struct wilc_priv *priv;
	struct wilc_vif *vif = netdev_priv(ndev);
	struct host_if_drv *hif_drv;
	struct wilc *wl;

	if (!vif || !vif->ndev || !vif->ndev->ieee80211_ptr ||
	    !vif->ndev->ieee80211_ptr->wiphy)
		return 0;

	priv = wiphy_priv(vif->ndev->ieee80211_ptr->wiphy);
	wl = vif->wilc;

	if (!priv)
		return 0;

	hif_drv = (struct host_if_drv *)priv->hif_drv;

	netdev_dbg(ndev, "Mac close\n");

	if (!wl)
		return 0;

	if (!hif_drv)
		return 0;

	if (wl->open_ifcs > 0)
		wl->open_ifcs--;
	else
		return 0;

	if (vif->ndev) {
		netif_stop_queue(vif->ndev);

		wilc_deinit_host_int(vif->ndev);
	}

	if (wl->open_ifcs == 0) {
		netdev_dbg(ndev, "Deinitializing wilc1000\n");
		wl->close = 1;
		wilc_wlan_deinitialize(ndev);
		wilc_wfi_deinit_mon_interface();
	}

	vif->mac_opened = 0;

	return 0;
}

void wilc_frmw_to_linux(struct wilc *wilc, u8 *buff, u32 size, u32 pkt_offset)
{
	unsigned int frame_len = 0;
	int stats;
	unsigned char *buff_to_send = NULL;
	struct sk_buff *skb;
	struct net_device *wilc_netdev;
	struct wilc_vif *vif;

	if (!wilc)
		return;

	wilc_netdev = get_if_handler(wilc, buff);
	if (!wilc_netdev)
		return;

	buff += pkt_offset;
	vif = netdev_priv(wilc_netdev);

	if (size > 0) {
		frame_len = size;
		buff_to_send = buff;

		skb = dev_alloc_skb(frame_len);
		if (!skb)
			return;

		skb->dev = wilc_netdev;

		skb_put_data(skb, buff_to_send, frame_len);

		skb->protocol = eth_type_trans(skb, wilc_netdev);
		vif->netstats.rx_packets++;
		vif->netstats.rx_bytes += frame_len;
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		stats = netif_rx(skb);
		netdev_dbg(wilc_netdev, "netif_rx ret value is: %d\n", stats);
	}
}

void wilc_wfi_mgmt_rx(struct wilc *wilc, u8 *buff, u32 size)
{
	int i = 0;
	struct wilc_vif *vif;

	for (i = 0; i < wilc->vif_num; i++) {
		vif = netdev_priv(wilc->vif[i]->ndev);
		if (vif->monitor_flag) {
			wilc_wfi_monitor_rx(buff, size);
			return;
		}
	}

	vif = netdev_priv(wilc->vif[1]->ndev);
	if ((buff[0] == vif->frame_reg[0].type && vif->frame_reg[0].reg) ||
	    (buff[0] == vif->frame_reg[1].type && vif->frame_reg[1].reg))
		wilc_wfi_p2p_rx(wilc->vif[1]->ndev, buff, size);
}

static struct notifier_block g_dev_notifier = {
	.notifier_call = dev_state_ev_handler
};

void wilc_netdev_cleanup(struct wilc *wilc)
{
	int i;

	if (wilc && (wilc->vif[0]->ndev || wilc->vif[1]->ndev))
		unregister_inetaddr_notifier(&g_dev_notifier);

	if (wilc && wilc->firmware) {
		release_firmware(wilc->firmware);
		wilc->firmware = NULL;
	}

	if (wilc && (wilc->vif[0]->ndev || wilc->vif[1]->ndev)) {
		for (i = 0; i < NUM_CONCURRENT_IFC; i++)
			if (wilc->vif[i]->ndev)
				if (wilc->vif[i]->mac_opened)
					wilc_mac_close(wilc->vif[i]->ndev);

		for (i = 0; i < NUM_CONCURRENT_IFC; i++) {
			unregister_netdev(wilc->vif[i]->ndev);
			wilc_free_wiphy(wilc->vif[i]->ndev);
			free_netdev(wilc->vif[i]->ndev);
		}
	}

	kfree(wilc);
}
EXPORT_SYMBOL_GPL(wilc_netdev_cleanup);

static const struct net_device_ops wilc_netdev_ops = {
	.ndo_init = mac_init_fn,
	.ndo_open = wilc_mac_open,
	.ndo_stop = wilc_mac_close,
	.ndo_start_xmit = wilc_mac_xmit,
	.ndo_get_stats = mac_stats,
	.ndo_set_rx_mode  = wilc_set_multicast_list,
};

int wilc_netdev_init(struct wilc **wilc, struct device *dev, int io_type,
		     const struct wilc_hif_func *ops)
{
	int i, ret;
	struct wilc_vif *vif;
	struct net_device *ndev;
	struct wilc *wl;

	wl = kzalloc(sizeof(*wl), GFP_KERNEL);
	if (!wl)
		return -ENOMEM;

	*wilc = wl;
	wl->io_type = io_type;
	wl->hif_func = ops;
	INIT_LIST_HEAD(&wl->txq_head.list);
	INIT_LIST_HEAD(&wl->rxq_head.list);

	register_inetaddr_notifier(&g_dev_notifier);

	for (i = 0; i < NUM_CONCURRENT_IFC; i++) {
		struct wireless_dev *wdev;

		ndev = alloc_etherdev(sizeof(struct wilc_vif));
		if (!ndev)
			return -ENOMEM;

		vif = netdev_priv(ndev);
		memset(vif, 0, sizeof(struct wilc_vif));

		if (i == 0) {
			strcpy(ndev->name, "wlan%d");
			vif->ifc_id = 1;
		} else {
			strcpy(ndev->name, "p2p%d");
			vif->ifc_id = 0;
		}
		vif->wilc = *wilc;
		vif->ndev = ndev;
		wl->vif[i] = vif;
		wl->vif_num = i + 1;
		vif->idx = i;

		ndev->netdev_ops = &wilc_netdev_ops;

		wdev = wilc_create_wiphy(ndev, dev);

		if (dev)
			SET_NETDEV_DEV(ndev, dev);

		if (!wdev) {
			netdev_err(ndev, "Can't register WILC Wiphy\n");
			return -1;
		}

		vif->ndev->ieee80211_ptr = wdev;
		vif->ndev->ml_priv = vif;
		wdev->netdev = vif->ndev;
		vif->netstats.rx_packets = 0;
		vif->netstats.tx_packets = 0;
		vif->netstats.rx_bytes = 0;
		vif->netstats.tx_bytes = 0;

		ret = register_netdev(ndev);
		if (ret)
			return ret;

		vif->iftype = STATION_MODE;
		vif->mac_opened = 0;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(wilc_netdev_init);

MODULE_LICENSE("GPL");
