/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DPU_RM_H__
#define __DPU_RM_H__

#include <linux/list.h>

#include "msm_kms.h"
#include "dpu_hw_top.h"

/**
 * enum dpu_rm_topology_name - HW resource use case in use by connector
 * @DPU_RM_TOPOLOGY_NONE:                 No topology in use currently
 * @DPU_RM_TOPOLOGY_SINGLEPIPE:           1 LM, 1 PP, 1 INTF/WB
 * @DPU_RM_TOPOLOGY_DUALPIPE:             2 LM, 2 PP, 2 INTF/WB
 * @DPU_RM_TOPOLOGY_DUALPIPE_3DMERGE:     2 LM, 2 PP, 3DMux, 1 INTF/WB
 */
enum dpu_rm_topology_name {
	DPU_RM_TOPOLOGY_NONE = 0,
	DPU_RM_TOPOLOGY_SINGLEPIPE,
	DPU_RM_TOPOLOGY_DUALPIPE,
	DPU_RM_TOPOLOGY_DUALPIPE_3DMERGE,
	DPU_RM_TOPOLOGY_MAX,
};

/**
 * enum dpu_rm_topology_control - HW resource use case in use by connector
 * @DPU_RM_TOPCTL_RESERVE_LOCK: If set, in AtomicTest phase, after a successful
 *                              test, reserve the resources for this display.
 *                              Normal behavior would not impact the reservation
 *                              list during the AtomicTest phase.
 * @DPU_RM_TOPCTL_RESERVE_CLEAR: If set, in AtomicTest phase, before testing,
 *                               release any reservation held by this display.
 *                               Normal behavior would not impact the
 *                               reservation list during the AtomicTest phase.
 * @DPU_RM_TOPCTL_DS  : Require layer mixers with DS capabilities
 */
enum dpu_rm_topology_control {
	DPU_RM_TOPCTL_RESERVE_LOCK,
	DPU_RM_TOPCTL_RESERVE_CLEAR,
	DPU_RM_TOPCTL_DS,
};

/**
 * struct dpu_rm - DPU dynamic hardware resource manager
 * @dev: device handle for event logging purposes
 * @rsvps: list of hardware reservations by each crtc->encoder->connector
 * @hw_blks: array of lists of hardware resources present in the system, one
 *	list per type of hardware block
 * @hw_mdp: hardware object for mdp_top
 * @lm_max_width: cached layer mixer maximum width
 * @rsvp_next_seq: sequence number for next reservation for debugging purposes
 * @rm_lock: resource manager mutex
 */
struct dpu_rm {
	struct drm_device *dev;
	struct list_head rsvps;
	struct list_head hw_blks[DPU_HW_BLK_MAX];
	struct dpu_hw_mdp *hw_mdp;
	uint32_t lm_max_width;
	uint32_t rsvp_next_seq;
	struct mutex rm_lock;
};

/**
 *  struct dpu_rm_hw_blk - resource manager internal structure
 *	forward declaration for single iterator definition without void pointer
 */
struct dpu_rm_hw_blk;

/**
 * struct dpu_rm_hw_iter - iterator for use with dpu_rm
 * @hw: dpu_hw object requested, or NULL on failure
 * @blk: dpu_rm internal block representation. Clients ignore. Used as iterator.
 * @enc_id: DRM ID of Encoder client wishes to search for, or 0 for Any Encoder
 * @type: Hardware Block Type client wishes to search for.
 */
struct dpu_rm_hw_iter {
	void *hw;
	struct dpu_rm_hw_blk *blk;
	uint32_t enc_id;
	enum dpu_hw_blk_type type;
};

/**
 * dpu_rm_init - Read hardware catalog and create reservation tracking objects
 *	for all HW blocks.
 * @rm: DPU Resource Manager handle
 * @cat: Pointer to hardware catalog
 * @mmio: mapped register io address of MDP
 * @dev: device handle for event logging purposes
 * @Return: 0 on Success otherwise -ERROR
 */
int dpu_rm_init(struct dpu_rm *rm,
		struct dpu_mdss_cfg *cat,
		void __iomem *mmio,
		struct drm_device *dev);

/**
 * dpu_rm_destroy - Free all memory allocated by dpu_rm_init
 * @rm: DPU Resource Manager handle
 * @Return: 0 on Success otherwise -ERROR
 */
int dpu_rm_destroy(struct dpu_rm *rm);

/**
 * dpu_rm_reserve - Given a CRTC->Encoder->Connector display chain, analyze
 *	the use connections and user requirements, specified through related
 *	topology control properties, and reserve hardware blocks to that
 *	display chain.
 *	HW blocks can then be accessed through dpu_rm_get_* functions.
 *	HW Reservations should be released via dpu_rm_release_hw.
 * @rm: DPU Resource Manager handle
 * @drm_enc: DRM Encoder handle
 * @crtc_state: Proposed Atomic DRM CRTC State handle
 * @conn_state: Proposed Atomic DRM Connector State handle
 * @topology: Pointer to topology info for the display
 * @test_only: Atomic-Test phase, discard results (unless property overrides)
 * @Return: 0 on Success otherwise -ERROR
 */
int dpu_rm_reserve(struct dpu_rm *rm,
		struct drm_encoder *drm_enc,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state,
		struct msm_display_topology topology,
		bool test_only);

/**
 * dpu_rm_reserve - Given the encoder for the display chain, release any
 *	HW blocks previously reserved for that use case.
 * @rm: DPU Resource Manager handle
 * @enc: DRM Encoder handle
 * @Return: 0 on Success otherwise -ERROR
 */
void dpu_rm_release(struct dpu_rm *rm, struct drm_encoder *enc);

/**
 * dpu_rm_get_mdp - Retrieve HW block for MDP TOP.
 *	This is never reserved, and is usable by any display.
 * @rm: DPU Resource Manager handle
 * @Return: Pointer to hw block or NULL
 */
struct dpu_hw_mdp *dpu_rm_get_mdp(struct dpu_rm *rm);

/**
 * dpu_rm_init_hw_iter - setup given iterator for new iteration over hw list
 *	using dpu_rm_get_hw
 * @iter: iter object to initialize
 * @enc_id: DRM ID of Encoder client wishes to search for, or 0 for Any Encoder
 * @type: Hardware Block Type client wishes to search for.
 */
void dpu_rm_init_hw_iter(
		struct dpu_rm_hw_iter *iter,
		uint32_t enc_id,
		enum dpu_hw_blk_type type);
/**
 * dpu_rm_get_hw - retrieve reserved hw object given encoder and hw type
 *	Meant to do a single pass through the hardware list to iteratively
 *	retrieve hardware blocks of a given type for a given encoder.
 *	Initialize an iterator object.
 *	Set hw block type of interest. Set encoder id of interest, 0 for any.
 *	Function returns first hw of type for that encoder.
 *	Subsequent calls will return the next reserved hw of that type in-order.
 *	Iterator HW pointer will be null on failure to find hw.
 * @rm: DPU Resource Manager handle
 * @iter: iterator object
 * @Return: true on match found, false on no match found
 */
bool dpu_rm_get_hw(struct dpu_rm *rm, struct dpu_rm_hw_iter *iter);

/**
 * dpu_rm_check_property_topctl - validate property bitmask before it is set
 * @val: user's proposed topology control bitmask
 * @Return: 0 on success or error
 */
int dpu_rm_check_property_topctl(uint64_t val);

/**
 * dpu_rm_get_topology_name - returns the name of the the given topology
 *                            definition
 * @topology: topology definition
 * @Return: name of the topology
 */
enum dpu_rm_topology_name
dpu_rm_get_topology_name(struct msm_display_topology topology);

#endif /* __DPU_RM_H__ */
