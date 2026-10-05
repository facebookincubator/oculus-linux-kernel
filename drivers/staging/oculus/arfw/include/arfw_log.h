/* SPDX-License-Identifier: GPL+                                              */
/*******************************************************************************
 * @file arfw_log.h
 *
 * @brief Log macros for the arfirmware driver
 *
 * @details
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef ARFW_LOG_H
#define ARFW_LOG_H

#include <linux/string.h>

#define __FILENAME__ \
	(strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define AR_LOG_MODULE_ARFW "[ARFW]"
#define AR_LOG_MODULE_PCI "[ARFW-PCI]"
#define AR_LOG_MODULE_TEST "[ARFW-TEST]"
#define AR_LOG_MODULE_USER "[ARFW-LOOPBACK]"
#define AR_LOG_MODULE_USB "[ARFW-USB]"

// Top level AR operations
#define AR_LOG_INIT "[Init]"
#define AR_LOG_CTRL "[Ctrl]"
#define AR_LOG_SHUTDOWN "[Shutdown]"
#define AR_LOG_SUSPEND "[Suspend]"
#define AR_LOG_RESUME "[Resume]"
#define AR_LOG_POWER "[Power]"
#define AR_LOG_DEV_REG "[Device_Register]"
#define AR_LOG_DEV_UNREG "[Device_Unregister]"
#define AR_LOG_DEV_QUERY "[Device_Query]"
#define AR_LOG_CREATE_QUEUE "[Create_Queue]"
#define AR_LOG_DESTROY_QUEUE "[Destroy_Queue]"
#define AR_LOG_QUERY_QUEUE "[Query_Queue]"
#define AR_LOG_BIND_QUEUE "[Bind_Queue]"
#define AR_LOG_BIND_REGION "[Bind_Region]"
#define AR_LOG_CREATE_CTRL_QUEUE "[Create_Control_Queue]"
#define AR_LOG_DESTORY_CTRL_QUEUE "[Destroy_Control_Queue]"
#define AR_LOG_READ "[Read]"
#define AR_LOG_WRITE "[Write]"
#define AR_LOG_PEND "[Pend]"
#define AR_LOG_READ_CTRL "[Control_Read]"
#define AR_LOG_WRITE_CTRL "[Control_Write]"
#define AR_LOG_REG_MEM "[Register_Mem]"
#define AR_LOG_UNREG_MEM "[Unregister_Mem]"
#define AR_LOG_REF_MEM "[Inc_Reference_Mem]"
#define AR_LOG_DEREF_MEM "[Dec_Reference_Mem]"
#define AR_LOG_MMAP "[Mmap]"
#define AR_LOG_APERTURE_ALLOC "[Aperture_Alloc]"
#define AR_LOG_APERTURE_MMAP "[Aperture_Mmap]"
#define AR_LOG_APERTURE_FREE "[Aperture_Free]"
#define AR_LOG_SHIM "[Shim]"
#define AR_LOG_INTERRUPT "[Interrupt]"
#define AR_LOG_SYSFS_READ "[Sysfs_Read]"
#define AR_LOG_SYSFS_WRITE "[Sysfs_Write]"

// Internal AR operations
#define AR_LOG_VMAP_MEM "[Vmap_Mem]"
#define AR_LOG_DMA_MAP_MEM "[Dma_map_Mem]"
#define AR_LOG_MMAP_MEM "[Mmap_Mem]"
#define AR_LOG_QUEUE_PRODUCE "[Queue_Produce]"
#define AR_LOG_QUEUE_RESERVE "[Queue_Reserve]"
#define AR_LOG_INDEX_CONSUMED "[Index_Consumed]"
#define AR_LOG_CLIENT_LOCK "[Lock_Client]"
#define AR_LOG_DEBUGFS_QUEUE "[DebugFS_Queue]"
#define AR_LOG_DEBUGFS_DEVICE "[DebugFS_Device]"

#ifdef DEBUG

#define AR_LOG_ERR(module, action, str, ...)                          \
	pr_err("%s%s[%s][%s:%d] " str "\n", module, action, __func__, \
	       __FILENAME__, __LINE__, ##__VA_ARGS__)
#define AR_LOG_WARN(module, action, str, ...)                          \
	pr_warn("%s%s[%s][%s:%d] " str "\n", module, action, __func__, \
		__FILENAME__, __LINE__, ##__VA_ARGS__)
#define AR_LOG_INFO(module, action, str, ...)                          \
	pr_info("%s%s[%s][%s:%d] " str "\n", module, action, __func__, \
		__FILENAME__, __LINE__, ##__VA_ARGS__)

#define AR_LOG_DEV_ERR(dev, module, action, str, ...)                       \
	dev_err(dev, "%s%s[%s][%s:%d] " str "\n", module, action, __func__, \
		__FILENAME__, __LINE__, ##__VA_ARGS__)
#define AR_LOG_DEV_WARN(dev, module, action, str, ...)                       \
	dev_warn(dev, "%s%s[%s][%s:%d] " str "\n", module, action, __func__, \
		 __FILENAME__, __LINE__, ##__VA_ARGS__)
#define AR_LOG_DEV_INFO(dev, module, action, str, ...)                       \
	dev_info(dev, "%s%s[%s][%s:%d] " str "\n", module, action, __func__, \
		 __FILENAME__, __LINE__, ##__VA_ARGS__)

#else // !DEBUG

#define AR_LOG_ERR(module, action, str, ...) \
	pr_err("%s%s[%s] " str "\n", module, action, __func__, ##__VA_ARGS__)
#define AR_LOG_WARN(module, action, str, ...) \
	pr_warn("%s%s[%s] " str "\n", module, action, __func__, ##__VA_ARGS__)
#define AR_LOG_INFO(module, action, str, ...) \
	pr_info("%s%s[%s] " str "\n", module, action, __func__, ##__VA_ARGS__)

#define AR_LOG_DEV_ERR(dev, module, action, str, ...)                \
	dev_err(dev, "%s%s[%s] " str "\n", module, action, __func__, \
		##__VA_ARGS__)
#define AR_LOG_DEV_WARN(dev, module, action, str, ...)                \
	dev_warn(dev, "%s%s[%s] " str "\n", module, action, __func__, \
		 ##__VA_ARGS__)
#define AR_LOG_DEV_INFO(dev, module, action, str, ...)                \
	dev_info(dev, "%s%s[%s] " str "\n", module, action, __func__, \
		 ##__VA_ARGS__)

#endif // DEBUG

#define AR_LOG_DBG(module, action, str, ...)                            \
	pr_debug("%s%s[%s][%s:%d] " str "\n", module, action, __func__, \
		 __FILENAME__, __LINE__, ##__VA_ARGS__)
#define AR_LOG_DEV_DBG(dev, module, action, str, ...)                       \
	dev_dbg(dev, "%s%s[%s][%s:%d] " str "\n", module, action, __func__, \
		__FILENAME__, __LINE__, ##__VA_ARGS__)

#define AR_LOG_PCI_ERR(action, str, ...) \
	AR_LOG_ERR(AR_LOG_MODULE_PCI, action, str, ##__VA_ARGS__)
#define AR_LOG_PCI_INFO(action, str, ...) \
	AR_LOG_INFO(AR_LOG_MODULE_PCI, action, str, ##__VA_ARGS__)
#define AR_LOG_PCI_DBG(action, str, ...) \
	AR_LOG_DBG(AR_LOG_MODULE_PCI, action, str, ##__VA_ARGS__)

#define AR_LOG_PCI_DEV_ERR(dev, action, str, ...) \
	AR_LOG_DEV_ERR(dev, AR_LOG_MODULE_PCI, action, str, ##__VA_ARGS__)
#define AR_LOG_PCI_DEV_WARN(dev, action, str, ...) \
	AR_LOG_DEV_WARN(dev, AR_LOG_MODULE_PCI, action, str, ##__VA_ARGS__)
#define AR_LOG_PCI_DEV_INFO(dev, action, str, ...) \
	AR_LOG_DEV_INFO(dev, AR_LOG_MODULE_PCI, action, str, ##__VA_ARGS__)
#define AR_LOG_PCI_DEV_DBG(dev, action, str, ...) \
	AR_LOG_DEV_DBG(dev, AR_LOG_MODULE_PCI, action, str, ##__VA_ARGS__)

#define AR_LOG_PCI_QUEUE_PARAMS_ERR(dev, queue_params, action, str, ...) \
	AR_LOG_PCI_DEV_ERR(dev, action, "[pci queue: 0x%x%s0x%x] " str,  \
			   (queue_params)->hlos_endpoint_id,             \
			   (queue_params)->queue_direction ==            \
					   AR_QUEUE_HLOS_TO_FW ?         \
					 "->" :                                \
					 "<-",                                 \
			   (queue_params)->fw_endpoint_id, ##__VA_ARGS__)
#define AR_LOG_PCI_QUEUE_PARAMS_INFO(dev, queue_params, action, str, ...) \
	AR_LOG_PCI_DEV_INFO(dev, action, "[pci queue: 0x%x%s0x%x] " str,  \
			    (queue_params)->hlos_endpoint_id,             \
			    (queue_params)->queue_direction ==            \
					    AR_QUEUE_HLOS_TO_FW ?         \
					  "->" :                                \
					  "<-",                                 \
			    (queue_params)->fw_endpoint_id, ##__VA_ARGS__)
#define AR_LOG_PCI_QUEUE_PARAMS_DBG(dev, queue_params, action, str, ...) \
	AR_LOG_PCI_DEV_DBG(dev, action, "[pci queue: 0x%x%s0x%x] " str,  \
			   (queue_params)->hlos_endpoint_id,             \
			   (queue_params)->queue_direction ==            \
					   AR_QUEUE_HLOS_TO_FW ?         \
					 "->" :                                \
					 "<-",                                 \
			   (queue_params)->fw_endpoint_id, ##__VA_ARGS__)

#define AR_LOG_PCI_QUEUE_ERR(queue, action, str, ...)                  \
	AR_LOG_PCI_QUEUE_PARAMS_ERR(&queue->driver->dev->dev,          \
				    &queue->queue_params, action, str, \
				    ##__VA_ARGS__)
#define AR_LOG_PCI_QUEUE_INFO(queue, action, str, ...)                  \
	AR_LOG_PCI_QUEUE_PARAMS_INFO(&queue->driver->dev->dev,          \
				     &queue->queue_params, action, str, \
				     ##__VA_ARGS__)
#define AR_LOG_PCI_QUEUE_DBG(queue, action, str, ...)                  \
	AR_LOG_PCI_QUEUE_PARAMS_DBG(&queue->driver->dev->dev,          \
				    &queue->queue_params, action, str, \
				    ##__VA_ARGS__)

#define AR_LOG_ARFW_ERR(action, str, ...) \
	AR_LOG_ERR(AR_LOG_MODULE_ARFW, action, str, ##__VA_ARGS__)
#define AR_LOG_ARFW_WARN(action, str, ...) \
	AR_LOG_WARN(AR_LOG_MODULE_ARFW, action, str, ##__VA_ARGS__)
#define AR_LOG_ARFW_INFO(action, str, ...) \
	AR_LOG_INFO(AR_LOG_MODULE_ARFW, action, str, ##__VA_ARGS__)
#define AR_LOG_ARFW_DBG(action, str, ...) \
	AR_LOG_DBG(AR_LOG_MODULE_ARFW, action, str, ##__VA_ARGS__)

#define AR_LOG_ARFW_DEV_ERR(dev, action, str, ...) \
	AR_LOG_DEV_ERR(dev, AR_LOG_MODULE_ARFW, action, str, ##__VA_ARGS__)
#define AR_LOG_ARFW_DEV_WARN(dev, action, str, ...) \
	AR_LOG_DEV_WARN(dev, AR_LOG_MODULE_ARFW, action, str, ##__VA_ARGS__)
#define AR_LOG_ARFW_DEV_INFO(dev, action, str, ...) \
	AR_LOG_DEV_INFO(dev, AR_LOG_MODULE_ARFW, action, str, ##__VA_ARGS__)
#define AR_LOG_ARFW_DEV_DBG(dev, action, str, ...) \
	AR_LOG_DEV_DBG(dev, AR_LOG_MODULE_ARFW, action, str, ##__VA_ARGS__)

#define AR_LOG_ARFW_QUEUE_ERR(queue, action, msg, argmsg, ...)           \
	AR_LOG_ARFW_DEV_ERR(                                             \
		queue->dev, action,                                      \
		msg " [arfw queue:%s 0x%x%s0x%x, " argmsg "]",           \
		(queue->mirror ? "M" : " "), queue->hlos_endpoint_id,    \
		(queue->direction == AR_QUEUE_HLOS_TO_FW ? "->" : "<-"), \
		queue->fw_endpoint_id, ##__VA_ARGS__)
#define AR_LOG_ARFW_QUEUE_WARN(queue, action, msg, argmsg, ...)          \
	AR_LOG_ARFW_DEV_WARN(                                            \
		queue->dev, action,                                      \
		msg " [arfw queue:%s 0x%x%s0x%x, " argmsg "]",           \
		(queue->mirror ? "M" : " "), queue->hlos_endpoint_id,    \
		(queue->direction == AR_QUEUE_HLOS_TO_FW ? "->" : "<-"), \
		queue->fw_endpoint_id, ##__VA_ARGS__)
#define AR_LOG_ARFW_QUEUE_INFO(queue, action, msg, argmsg, ...)          \
	AR_LOG_ARFW_DEV_INFO(                                            \
		queue->dev, action,                                      \
		msg " [arfw queue:%s 0x%x%s0x%x, " argmsg "]",           \
		(queue->mirror ? "M" : " "), queue->hlos_endpoint_id,    \
		(queue->direction == AR_QUEUE_HLOS_TO_FW ? "->" : "<-"), \
		queue->fw_endpoint_id, ##__VA_ARGS__)
#define AR_LOG_ARFW_QUEUE_INFO0(queue, action, msg)                      \
	AR_LOG_ARFW_DEV_INFO(                                            \
		queue->dev, action, msg " [arfw queue:%s 0x%x%s0x%x]",   \
		(queue->mirror ? "M" : " "), queue->hlos_endpoint_id,    \
		(queue->direction == AR_QUEUE_HLOS_TO_FW ? "->" : "<-"), \
		queue->fw_endpoint_id)
#define AR_LOG_ARFW_QUEUE_DBG(queue, action, msg, argmsg, ...)           \
	AR_LOG_ARFW_DEV_DBG(                                             \
		queue->dev, action,                                      \
		msg " [arfw queue:%s 0x%x%s0x%x, " argmsg "]",           \
		(queue->mirror ? "M" : " "), queue->hlos_endpoint_id,    \
		(queue->direction == AR_QUEUE_HLOS_TO_FW ? "->" : "<-"), \
		queue->fw_endpoint_id, ##__VA_ARGS__)
#define AR_LOG_ARFW_QUEUE_DBG0(queue, action, msg)                       \
	AR_LOG_ARFW_DEV_DBG(                                             \
		queue->dev, action, msg " [arfw queue:%s 0x%x%s0x%x]",   \
		(queue->mirror ? "M" : " "), queue->hlos_endpoint_id,    \
		(queue->direction == AR_QUEUE_HLOS_TO_FW ? "->" : "<-"), \
		queue->fw_endpoint_id)

#define AR_LOG_USER_ERR(action, str, ...) \
	AR_LOG_ERR(AR_LOG_MODULE_USER, action, str, ##__VA_ARGS__)
#define AR_LOG_USER_INFO(action, str, ...) \
	AR_LOG_INFO(AR_LOG_MODULE_USER, action, str, ##__VA_ARGS__)
#define AR_LOG_USER_DBG(action, str, ...) \
	AR_LOG_DBG(AR_LOG_MODULE_USER, action, str, ##__VA_ARGS__)

#define AR_LOG_USER_DEV_ERR(dev, action, str, ...) \
	AR_LOG_DEV_ERR(dev, AR_LOG_MODULE_USER, action, str, ##__VA_ARGS__)
#define AR_LOG_USER_DEV_INFO(dev, action, str, ...) \
	AR_LOG_DEV_INFO(dev, AR_LOG_MODULE_USER, action, str, ##__VA_ARGS__)
#define AR_LOG_USER_DEV_DBG(dev, action, str, ...) \
	AR_LOG_DEV_DBG(dev, AR_LOG_MODULE_USER, action, str, ##__VA_ARGS__)

#define AR_LOG_USER_QUEUE_ERR(queue, action, str, ...)                        \
	AR_LOG_DEV_ERR(                                                       \
		&queue->dev->dev, AR_LOG_MODULE_USER, action,                 \
		"%s 0x%x%s0x%x: " str, (queue->info.mirror ? "M" : " "),      \
		queue->info.hlos_endpoint,                                    \
		(queue->info.direction == AR_QUEUE_HLOS_TO_FW) ? "->" : "<-", \
		queue->info.fw_endpoint, ##__VA_ARGS__)
#define AR_LOG_USER_QUEUE_INFO(queue, action, str, ...)                       \
	AR_LOG_DEV_INFO(                                                      \
		&queue->dev->dev, AR_LOG_MODULE_USER, action,                 \
		"%s 0x%x%s0x%x: " str, (queue->info.mirror ? "M" : " "),      \
		queue->info.hlos_endpoint,                                    \
		(queue->info.direction == AR_QUEUE_HLOS_TO_FW) ? "->" : "<-", \
		queue->info.fw_endpoint, ##__VA_ARGS__)
#define AR_LOG_USER_QUEUE_DBG(queue, action, str, ...)                        \
	AR_LOG_DEV_DBG(                                                       \
		&queue->dev->dev, AR_LOG_MODULE_USER, action,                 \
		"%s 0x%x%s0x%x: " str, (queue->info.mirror ? "M" : " "),      \
		queue->info.hlos_endpoint,                                    \
		(queue->info.direction == AR_QUEUE_HLOS_TO_FW) ? "->" : "<-", \
		queue->info.fw_endpoint, ##__VA_ARGS__)

#define AR_LOG_USB_ERR(action, str, ...) \
	AR_LOG_ERR(AR_LOG_MODULE_USB, action, str, ##__VA_ARGS__)
#define AR_LOG_USB_INFO(action, str, ...) \
	AR_LOG_INFO(AR_LOG_MODULE_USB, action, str, ##__VA_ARGS__)
#define AR_LOG_USB_DBG(action, str, ...) \
	AR_LOG_DBG(AR_LOG_MODULE_USB, action, str, ##__VA_ARGS__)

#define AR_LOG_USB_DEV_ERR(dev, action, str, ...) \
	AR_LOG_DEV_ERR(dev, AR_LOG_MODULE_USB, action, str, ##__VA_ARGS__)
#define AR_LOG_USB_DEV_INFO(dev, action, str, ...) \
	AR_LOG_DEV_INFO(dev, AR_LOG_MODULE_USB, action, str, ##__VA_ARGS__)
#define AR_LOG_USB_DEV_DBG(dev, action, str, ...) \
	AR_LOG_DEV_DBG(dev, AR_LOG_MODULE_USB, action, str, ##__VA_ARGS__)

#define AR_LOG_USB_EP_ERR(ep, action, str, ...)                       \
	AR_LOG_USB_DEV_ERR(                                           \
		&ep->driver->intf->dev, action, "[%s:%s:%d] " str,    \
		ep->ctrl ? "c" : "d",                                 \
		usb_endpoint_dir_in(&ep->driver->intf->cur_altsetting \
					     ->endpoint[ep->offset]   \
					     .desc) ?                 \
			      "in" :                                        \
			      "out",                                        \
		ep->num, ##__VA_ARGS__)
#define AR_LOG_USB_EP_INFO(ep, action, str, ...)                      \
	AR_LOG_USB_DEV_INFO(                                          \
		&ep->driver->intf->dev, action, "[%s:%s:%d] " str,    \
		ep->ctrl ? "c" : "d",                                 \
		usb_endpoint_dir_in(&ep->driver->intf->cur_altsetting \
					     ->endpoint[ep->offset]   \
					     .desc) ?                 \
			      "in" :                                        \
			      "out",                                        \
		ep->num, ##__VA_ARGS__)
#define AR_LOG_USB_EP_DBG(ep, action, str, ...)                       \
	AR_LOG_USB_DEV_DBG(                                           \
		&ep->driver->intf->dev, action, "[%s:%s:%d] " str,    \
		ep->ctrl ? "c" : "d",                                 \
		usb_endpoint_dir_in(&ep->driver->intf->cur_altsetting \
					     ->endpoint[ep->offset]   \
					     .desc) ?                 \
			      "in" :                                        \
			      "out",                                        \
		ep->num, ##__VA_ARGS__)

#define AR_LOG_USB_QUEUE_PARAMS_ERR(ep, params, action, str, ...)             \
	AR_LOG_USB_DEV_ERR(                                                   \
		&ep->driver->intf->dev, action, "[%s:%s:%d|0x%x%s0x%x] " str, \
		ep->ctrl ? "c" : "d",                                         \
		usb_endpoint_dir_in(&ep->driver->intf->cur_altsetting         \
					     ->endpoint[ep->offset]           \
					     .desc) ?                         \
			      "in" :                                                \
			      "out",                                                \
		ep->num, params->hlos_endpoint_id,                            \
		params->queue_direction == AR_QUEUE_HLOS_TO_FW ? "->" : "<-", \
		params->fw_endpoint_id, ##__VA_ARGS__)
#define AR_LOG_USB_QUEUE_PARAMS_INFO(ep, params, action, str, ...)            \
	AR_LOG_USB_DEV_INFO(                                                  \
		&ep->driver->intf->dev, action, "[%s:%s:%d|0x%x%s0x%x] " str, \
		ep->ctrl ? "c" : "d",                                         \
		usb_endpoint_dir_in(&ep->driver->intf->cur_altsetting         \
					     ->endpoint[ep->offset]           \
					     .desc) ?                         \
			      "in" :                                                \
			      "out",                                                \
		ep->num, params->hlos_endpoint_id,                            \
		params->queue_direction == AR_QUEUE_HLOS_TO_FW ? "->" : "<-", \
		params->fw_endpoint_id, ##__VA_ARGS__)
#define AR_LOG_USB_QUEUE_PARAMS_DBG(ep, params, action, str, ...)             \
	AR_LOG_USB_DEV_DBG(                                                   \
		&ep->driver->intf->dev, action, "[%s:%s:%d|0x%x%s0x%x] " str, \
		ep->ctrl ? "c" : "d",                                         \
		usb_endpoint_dir_in(&ep->driver->intf->cur_altsetting         \
					     ->endpoint[ep->offset]           \
					     .desc) ?                         \
			      "in" :                                                \
			      "out",                                                \
		ep->num, params->hlos_endpoint_id,                            \
		params->queue_direction == AR_QUEUE_HLOS_TO_FW ? "->" : "<-", \
		params->fw_endpoint_id, ##__VA_ARGS__)

#define AR_LOG_USB_QUEUE_ERR(ep, queue, action, str, ...)              \
	AR_LOG_USB_QUEUE_PARAMS_ERR(ep, (&queue->params), action, str, \
				    ##__VA_ARGS__)
#define AR_LOG_USB_QUEUE_INFO(ep, queue, action, str, ...)              \
	AR_LOG_USB_QUEUE_PARAMS_INFO(ep, (&queue->params), action, str, \
				     ##__VA_ARGS__)
#define AR_LOG_USB_QUEUE_DBG(ep, queue, action, str, ...)              \
	AR_LOG_USB_QUEUE_PARAMS_DBG(ep, (&queue->params), action, str, \
				    ##__VA_ARGS__)

#endif // !ARFW_LOG_H
