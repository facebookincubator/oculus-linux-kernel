// // SPDX-License-Identifier: GPL-2.0-only
// /*
 // * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 // * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 // */

// #include <linux/kernel.h>
// #include <linux/notifier.h>
// #include <linux/msm_kgsl.h>

// #include "cvp_hfi_helper.h"
// #include "cvp_hfi_api.h"
// #include "msm_cvp_internal.h"
// #include "msm_cvp_debug.h"
// #include "msm_cvp_common.h"
// #include "cvp_core_hfi.h"
// #include "msm_cvp_resources.h"
// #include "msm_cvp.h"
// #include "msm_gpu_eva.h"


// static int msm_eva_notify_gpu_status ( u32 status );
// static void gpu_ssr_delay_handler(struct work_struct *work);
// static DECLARE_DELAYED_WORK(gpu_eva_work, gpu_ssr_delay_handler);
// struct workqueue_struct *gpu_eva_workq;

// static struct notifier_block kgsl_eva_notifier_blk = {
	// .notifier_call = kgsl_eva_notifier_callback,
	// .priority = INT_MAX,
// };


// static void gpu_ssr_delay_handler(struct work_struct *work)
// {
	// struct msm_cvp_core *core;
	// struct msm_cvp_inst *inst = NULL;
	// uint32_t i = 0;
	// unsigned long flags = 0;

	// core = list_first_entry(&cvp_driver->cores, struct msm_cvp_core, list);
	// if (core) {
		// dprintk(CVP_INFO, "Valid Core Identified\n");
	// }
	// else {
		// dprintk(CVP_ERR, "InValid Core \n");
		// return;
	// }
	// list_for_each_entry(inst, &core->instances, list) {
		// if( (inst->state != MSM_CVP_CORE_INVALID ) && (inst->prop.type == HFI_SESSION_LSR ) ) {
			// dprintk(CVP_INFO, " find active LSR session\n");
			// if (cvp_clean_session_queues(inst))
				// dprintk(CVP_ERR, "Failed to clean fences\n");
			// for (i = 0; i < ARRAY_SIZE(inst->completions); i++)
				// complete(&inst->completions[i]);
			// spin_lock_irqsave(&inst->event_handler.lock, flags);
			// inst->event_handler.event = CVP_SSR_EVENT;
			// spin_unlock_irqrestore(
				// &inst->event_handler.lock, flags);
			// wake_up_all(&inst->event_handler.wq);
		// }
	// }
// }

// static int msm_eva_notify_gpu_status( u32 status )
// {
	// int rc = 0;
	// u32 res_msg_id = 0;
	// struct msm_cvp_core *core;

	// core = list_first_entry(&cvp_driver->cores, struct msm_cvp_core, list);
	// if (core){
		// dprintk(CVP_INFO, "Valid Core Identified\n");
	// } else {
		// dprintk(CVP_ERR, "InValid Core \n");
		// return -EINVAL;
	// }
	// if ( status == HFI_CMD_SYS_STOP_GMU_CMD ) {
		// res_msg_id = HAL_SYS_GMU_STOP_DONE;
	// }
	// else if ( status == HFI_CMD_SYS_START_GMU_CMD ) {
		// res_msg_id = HAL_SYS_GMU_START_DONE;
	// }
	// rc = call_hfi_op(core->device, notify_gpu_status,
		// core->device->hfi_device_data, status );
	// if (rc) {
		// dprintk(CVP_ERR,"notify gpu status failed\n");
		// return -EINVAL;
	// }
	// else {
		// rc = wait_for_completion_timeout(
			// &core->completions[SYS_MSG_INDEX(res_msg_id)],
			// msecs_to_jiffies(
			// core->resources.msm_cvp_hw_rsp_timeout));

		// if (!rc) {
			// dprintk(CVP_ERR, "Wait timed out for HFI_CMD_SYS_GMU_CMD: %d\n",
			// SYS_MSG_INDEX(res_msg_id));
			// rc = -ETIMEDOUT;
		// }
	// }
	// return rc;
// }

// int kgsl_eva_notifier_callback( struct notifier_block *this, unsigned long event, void *ptr )
// {
	// int rc = 0;
	// enum kgsl_srcu_events msg = ( enum kgsl_srcu_events )(event);
	// switch(msg)
	// {
		// case GPU_SSR_BEGIN:
			// dprintk(CVP_INFO, "Received GPU_SSR_BEGIN");
			// rc = msm_eva_notify_gpu_status( HFI_CMD_SYS_STOP_GMU_CMD );
			// if (rc){
				// dprintk(CVP_INFO, "Failed to notify gpu status");
			// }
			 // if (!queue_delayed_work(gpu_eva_workq,
				// &gpu_eva_work,
				// msecs_to_jiffies(400))) {
				// dprintk(CVP_PWR,"EVA-GPU work already scheduled\n");
			// }
			// break;
		// case GPU_GMU_READY:
		// case GPU_SSR_END:
			// dprintk(CVP_INFO, "Received GPU_GMU_READY/GPU_SSR_END");;
			// cancel_delayed_work(&gpu_eva_work);
			// rc = msm_eva_notify_gpu_status( HFI_CMD_SYS_START_GMU_CMD );
			// if( rc ) {
				// dprintk(CVP_ERR, "EVA_GPU_SSR_END_ACK/EVA_GMU_READY_ACK failed \n");
			// }
			// break;
		// case GPU_SSR_FATAL:
			// dprintk(CVP_INFO, "Received GPU_SSR_FATAL %d", msg);
			// gpu_ssr_delay_handler((struct work_struct *)gpu_eva_workq);
			// break;
		// default:
			// dprintk(CVP_ERR, "Invalid message \n");
			// break;
	// }
	// return rc;
// }

// int __interface_gpu_init(void)
// {
	// int rc = 0;
	// rc = kgsl_add_rcu_notifier( &kgsl_eva_notifier_blk );
	// if( rc ){
		// dprintk(CVP_ERR, "kgsl_add_rcu_notifier failed \n");
	// }
	// gpu_eva_workq  =  create_singlethread_workqueue("eva-gpu-workerq");
	// if (!gpu_eva_workq) {
		// dprintk(CVP_ERR, "create gpu_eva_workq failed\n");
		// rc = -EINVAL;
	// }
	// return rc;
// }
// int __interface_gpu_deinit(void)
// {
	// int rc = 0;
	// rc = kgsl_del_rcu_notifier(&kgsl_eva_notifier_blk);
	// if(rc) {
		// dprintk(CVP_ERR, "kgsl_del_rcu_notifier failed \n");
	// }
	// cancel_delayed_work(&gpu_eva_work);
	// destroy_workqueue(gpu_eva_workq);
	// return rc;
// }
