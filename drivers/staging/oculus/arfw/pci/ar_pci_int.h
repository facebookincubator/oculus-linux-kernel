/* SPDX-License-Identifier: GPL+																							*/
/*******************************************************************************
 * @file ar_pci_int.h
 *
 * @brief Internal header for PCI AR accelorator driver
 *
 * @details
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef AR_PCI_INT_H
#define AR_PCI_INT_H

#include <linux/arfw_io_interface.h>
#include <linux/atomic.h>
#include <linux/gpio/consumer.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/hrtimer.h>

#include <ar_atomics.h>
#include <arfw_config.h>
#include <arfw_ops.h>

#include "fb_pcie_drvr_shared.h"
#include "fb_pcie_drvr_shared_compat.h"

#ifdef CONFIG_ARFIRMWARE_PCI_DEBUG
#include "test/pci_debug.h"
#endif // CONFIG_ARFIRMWARE_PCI_DEBUG

#define AR_DRIVER_NAME "ar_pci"

#define PCIE_AP_SRC_CTRL_RING_SIZE (32)
#define PCIE_AP_DST_CTRL_RING_SIZE (32)

#define ARP_DEFAULT_NUM_BUF_PER_SESSION 4

#define ARFW_COL_DEVID_LEN 32
#define ARFW_COL_DEVID_PREFIX "AR-COL-PCI"

struct ar_pci_cmd_reply_promise;
struct ar_client_queue;
struct ar_future;
struct ar_firmware_device;
typedef struct ar_pci_bar_data_ring ar_pci_bar_data_ring_t;
typedef struct ar_pci_control_ring_context ar_pci_control_ring_context_t;

typedef struct arfw_io_request ar_firmware_io_request_t;

typedef struct {
	struct list_head list_node;
	struct mutex lock;
	bool released;
} ar_pci_client_list_t;

typedef struct {
	bool valid;
	uint16_t rcv_ring_pend_buff_count_max;
	char devid[ARFW_COL_DEVID_LEN];
} arp_info_t;

typedef enum {
	AR_PCI_DEV_POWER_STATE_NONE,
	AR_PCI_DEV_POWER_STATE_OFF,
	AR_PCI_DEV_POWER_STATE_ON,
} ar_pci_dev_power_state_t;

// Old BAR layout used by FW versions 4.0 and earlier. The queue indexes are
// placed in the BAR space. Need to support it for the backwards
// compatibility. For instance, need to be able to update FW even after the
// factory reset.
// The fields below makes sense only for the BAR queue indexes layout.
typedef struct {
	// This is a cached copy of the bar memory area captured during start up.
	pcie_bar_memory_area_t bar_memory_area;
	// Cached copies of ARP updated BAR rd/wr indexes
	void *arp_update_wr_index_cached;
	void *arp_update_rd_index_cached;
	// Cached copies of the AP rd/wr indexes
	void *ap_wr_index_cached;
	void *ap_rd_index_cached;
} ar_pci_bar_layout_t;

// Aperture max size is 384 MB
#define APERTURE_MAX_SIZE (384 * ARFW_APERTURE_ALLOC_CHUNK_SIZE)

// which functions are allowed to use the aperture
#define APERTURE_TEST_FUNC 0
#define APERTURE_TEST_FUNC_ID 1
#define APERTURE_FDLA_FUNC 3
#define APERTURE_FDLA_FUNC_ID 2

// size of the aperture for the functions
#define APERTURE_TEST_FUNC_SIZE (4 * ARFW_APERTURE_ALLOC_CHUNK_SIZE)
#define APERTURE_FDLA_FUNC_SIZE (100 * ARFW_APERTURE_ALLOC_CHUNK_SIZE)

/**
 * A PCI aperture region description. Holds allocation information for the aperture
 * itself as well as carveouts for aperture buffers.
 */
struct ar_pci_device_aperture_region {
	/// id of the aperture
	uint32_t id;
	/// kernel va of the aperture
	void *addr;
	/// dma address of the aperture
	dma_addr_t dma_addr;
	/// size of the aperture in bytes
	size_t size;
	/// size of the aperture in ARFW_APERTURE_ALLOC_CHUNK_SIZEs.
	size_t bits;
	/// allocation bitmap for aperture buffers, locked for concurrent accesses
	struct {
		spinlock_t lock;
		DECLARE_BITMAP(bitmap, APERTURE_MAX_SIZE /
					       ARFW_APERTURE_ALLOC_CHUNK_SIZE);
	} locked_allocs;
};

struct arfw_pci_proto_ops {
	// Different firmware versions have different payloads and message types.
	bool (*arfw_pci_proto_set_payload)(
		ar_firmware_message_header_t *ar_ipc_header, void *ring_entry);
	void (*arfw_pci_proto_service_ring_entry)(void *ring_entry);
};

/**
 * Callback invoked on boot interrupt
 */
typedef int (*ar_pci_handshake_irq_handler_t)(struct pci_dev *dev);

/**
 * This is the primary pci data structure.
 */
typedef struct {
	struct pci_dev *dev;
	const struct pci_device_id *dev_id;

	// Track the link stability to bypass firmware communications.
	// This field is set to true on protocol initialization finish and
	// to false on linkdown events.
	bool link_is_up;

	// callback invoked on function boot interrupt
	ar_pci_handshake_irq_handler_t handshake_irq_handler;
	int handshake_irq_index;

	// callbacks into the client
	const struct arfw_client_ops *client_ops;

	// Callbacks for the PCI functionality to support different
	// versions of firmware.
	const struct arfw_pci_proto_ops *proto_ops;

	// Used to track all registered pci device driver objects.
	struct list_head list;

	// These are the mapped virtual address of both BARs
	// The memory bar is used to store ring and status info
	void *mmio_memory_bar;
	// The interrupt bar is used as a doorbell after transfers and reads
	void *mmio_interrupt_bar;

	// Number of MSI interrupts used by device.
	int msi_int_num;

	// Bar sizes
	size_t mmio_memory_bar_size;
	size_t mmio_interrupt_bar_size;

	// Bar regions
	int mmio_memory_bar_region;
	// The interrupt region may be set to -1 if not used
	int mmio_interrupt_bar_region;

	// Offset of the doorbell interrupt
	uint32_t ar_pcie_doorbell_offset;

	// This ring is used to send command messages from the AP to the ARP
	ar_pci_control_ring_context_t *ap_src_control_ring;
	// This ring is used to receive command messages from the ARP to the AP
	ar_pci_control_ring_context_t *ap_dst_control_ring;

	ar_pci_control_ring_context_t *ap_src_buffer_control_ring;

	// This is a cached copy of the bar memory area captured during start up. This is
	// a new BAR layout, where the queue indexes are placed in the RAM space.
	pcie_bar_data_t bar_data;

	// Location of the ctrl and data queue indexes in memory. It could be in BAR or RAM.
	void *ap_update_block_addr;
	dma_addr_t ap_update_block_dma;
	size_t ap_update_block_size;
	void *cp_update_block_addr;
	dma_addr_t cp_update_block_dma;
	size_t cp_update_block_size;

	// The max number of the src and dst queues.
	uint16_t ap_src_data_ring_max;
	uint16_t ap_dst_data_ring_max;

	// Deprecated field to allocate memory in case the old FW is used (4.0 or earlier).
	// If this is the case then the queue indexes will be be allocated in the BAR
	// space and not in RAM. This is used to support some old FW versions, which could
	// be run after factory reset. At some point we will be able to remove it.
	ar_pci_bar_layout_t *deprecated_bar_layout;
	// True if indexes are placed in the BAR space. False for RAM.
	bool queue_indexes_in_bar;

	// These are the data rings used by the individual services
	struct mutex data_ring_lock;
	ar_pci_bar_data_ring_t *send_data_rings;
	ar_pci_bar_data_ring_t *rcv_data_rings;

	// Doorbell deferring
	int doorbell_pending;
	ktime_t last_doorbell;
	bool doorbell_timer_active;
	struct hrtimer doorbell_timer;
	spinlock_t doorbell_lock;

	/**
	 * Map for associated replies with commands on the control queues.
	 * Sequence numbers will be mapped to futures. Those futures will get
	 * completed with an error or ar_pci_cmd_reply_t.
	 */
	struct list_head cmd_reply_list;
	spinlock_t cmd_reply_lock;

	/**
	 * Active client queue lists for send and receive
	 * These could be broken out by priority later if necessary
	 */
	ar_pci_client_list_t client_send_queues;
	ar_pci_client_list_t client_rcv_queues;

	atomic_t next_seq_num;
	atomic_t data_irq_count;
	atomic_t handshake_irq_count;

	struct {
		struct mutex lock;
		ar_pci_dev_power_state_t state;
	} power;

#ifdef CONFIG_ARFIRMWARE_MSM_PCIE
	struct msm_pcie_register_event *link_event;
	// Protect against concurrent aspm ops
	struct mutex aspm_lock;
#endif // CONFIG_ARFIRMWARE_MSM_PCIE

#ifdef CONFIG_ARFIRMWARE_PCI_DEBUG
	pci_debug_data_t debug_data;
#endif // CONFIG_ARFIRMWARE_PCI_DEBUG

	// ARP info struct
	arp_info_t arp_info;

	// AP protocol version
	pcie_protocol_version_t ap_ver;

	// ARP protocol version
	pcie_protocol_version_t arp_ver;

#ifdef CONFIG_ARFIRMWARE_PCI_GPIO_DOORBELL
	// Acro doesn't support in-band doorbell so we use a GPIO as a workaround
	struct gpio_desc *acro_doorbell_gpio;
#endif // CONFIG_ARFIRMWARE_PCI_GPIO_DOORBELL
	struct ar_pci_device_aperture_region aperture;

	struct {
		// lock for adjusting duty cycle
		struct mutex lock;
		// current frequency for this duty cycle
		int freq_hz;
		// Set when the firmware has accepted the duty cycle
		atomic_t enabled;
	} duty_cycle;

	// Set when the firmware support the command for delete all data rings
	bool group_shutdown_supported;
	bool sys_name_supported;

	// Backs the link_state sysfs node. The dirent is cached because the down
	// edge is signalled from hard IRQ, where sysfs_notify() may sleep.
	struct {
		atomic_t up;
		struct kernfs_node *kn;
	} link_state;
} ar_pci_driver_t;

typedef struct ar_client_queue {
	// refernce to arfw client who owns the queue
	arfw_client_queue_t arfw_queue;
	// refernce to the pci driver
	ar_pci_driver_t *driver;

	// Set when the queue is ready to serve the fw
	ar_atomic_t ready;
	// Set when the queue was corrupted and released
	bool released;

	// List node for active ring list
	struct list_head node;
	// pointer to the ring list mutex
	struct mutex *lock;

	// parameters of the queue
	struct arfw_client_queue_create_params queue_params;
	// bar space elements of the ring
	ar_pci_bar_data_ring_t *data_ring;

	// The max number of elements a scatter gather list can support
	uint32_t max_num_sg_entries;

	uint16_t seq_num;
	uint16_t send_queue_write_index;
} ar_client_queue_t;

typedef struct ar_pci_queue_data {
	// DMA address, shared with FW.
	dma_addr_t dma_addr;
} ar_pci_queue_data_t;

#endif // !AR_PCI_INT_H
