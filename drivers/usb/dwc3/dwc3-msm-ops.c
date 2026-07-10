// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/sched.h>
#include <linux/usb/dwc3-msm.h>
#include <linux/usb/composite.h>
#include "core.h"
#include "debug-ipc.h"
#include "gadget.h"

struct kprobe_data {
	struct dwc3 *dwc;
	int xi0;
};

static int entry_dwc3_gadget_init_in_endpoint(struct kretprobe_instance *ri,
					      struct pt_regs *regs)
{
	return 0;
}

static int exit_dwc3_gadget_init_in_endpoint(struct kretprobe_instance *ri,
					     struct pt_regs *regs)
{
	return 0;
}

static int entry_dwc3_gadget_init_out_endpoint(struct kretprobe_instance *ri,
					       struct pt_regs *regs)
{
	return 0;
}

static int exit_dwc3_gadget_init_out_endpoint(struct kretprobe_instance *ri,
					      struct pt_regs *regs)
{
	return 0;
}

static int entry_dwc3_gadget_run_stop(struct kretprobe_instance *ri,
				      struct pt_regs *regs)
{
	return 0;
}

static int entry_dwc3_send_gadget_ep_cmd(struct kretprobe_instance *ri,
					 struct pt_regs *regs)
{
	return 0;
}

static int entry_dwc3_gadget_reset_interrupt(struct kretprobe_instance *ri,
					     struct pt_regs *regs)
{
	return 0;
}

static int entry_dwc3_gadget_conndone_interrupt(struct kretprobe_instance *ri,
						struct pt_regs *regs)
{
	return 0;
}

static int exit_dwc3_gadget_conndone_interrupt(struct kretprobe_instance *ri,
					       struct pt_regs *regs)
{
	return 0;
}

static int entry_dwc3_gadget_pullup(struct kretprobe_instance *ri,
				    struct pt_regs *regs)
{
	return 0;
}

static int exit_dwc3_gadget_pullup(struct kretprobe_instance *ri,
				   struct pt_regs *regs)
{
	return 0;
}

static int entry___dwc3_gadget_start(struct kretprobe_instance *ri,
				     struct pt_regs *regs)
{
	return 0;
}

static int entry_trace_dwc3_ctrl_req(struct kretprobe_instance *ri,
				     struct pt_regs *regs)
{
	struct usb_ctrlrequest *ctrl = (struct usb_ctrlrequest *)regs->regs[0];

	dbg_trace_ctrl_req(ctrl);

	return 0;
}

static int entry_trace_dwc3_ep_queue(struct kretprobe_instance *ri,
				     struct pt_regs *regs)
{
	struct dwc3_request *req = (struct dwc3_request *)regs->regs[0];

	dbg_trace_ep_queue(req);

	return 0;
}

static int entry_trace_dwc3_ep_dequeue(struct kretprobe_instance *ri,
				       struct pt_regs *regs)
{
	struct dwc3_request *req = (struct dwc3_request *)regs->regs[0];

	dbg_trace_ep_dequeue(req);

	return 0;
}

static int entry_trace_dwc3_gadget_giveback(struct kretprobe_instance *ri,
					    struct pt_regs *regs)
{
	struct dwc3_request *req = (struct dwc3_request *)regs->regs[0];

	dbg_trace_gadget_giveback(req);

	return 0;
}

static int entry_trace_dwc3_gadget_ep_cmd(struct kretprobe_instance *ri,
				   struct pt_regs *regs)
{
	struct dwc3_ep *dep = (struct dwc3_ep *)regs->regs[0];
	unsigned int cmd = regs->regs[1];
	struct dwc3_gadget_ep_cmd_params *param = (struct dwc3_gadget_ep_cmd_params *)regs->regs[2];
	int cmd_status = regs->regs[3];

	dbg_trace_gadget_ep_cmd(dep, cmd, param, cmd_status);

	return 0;
}

static int entry_trace_dwc3_prepare_trb(struct kretprobe_instance *ri,
				   struct pt_regs *regs)
{
	struct dwc3_ep *dep = (struct dwc3_ep *)regs->regs[0];
	struct dwc3_trb *trb = (struct dwc3_trb *)regs->regs[1];

	dbg_trace_trb_prepare(dep, trb);

	return 0;
}

static int entry_trace_dwc3_event(struct kretprobe_instance *ri,
				   struct pt_regs *regs)
{
	u32 event = regs->regs[0];
	struct dwc3 *dwc = (struct dwc3 *)regs->regs[1];

	dbg_trace_event(event, dwc);

	return 0;
}

#define ENTRY_EXIT(name) {\
	.handler = exit_##name,\
	.entry_handler = entry_##name,\
	.data_size = sizeof(struct kprobe_data),\
	.maxactive = 8,\
	.kp.symbol_name = #name,\
}

#define ENTRY(name) {\
	.entry_handler = entry_##name,\
	.data_size = sizeof(struct kprobe_data),\
	.maxactive = 8,\
	.kp.symbol_name = #name,\
}

static struct kretprobe dwc3_msm_probes[] = {
	ENTRY(dwc3_gadget_run_stop),
	ENTRY(dwc3_send_gadget_ep_cmd),
	ENTRY(dwc3_gadget_reset_interrupt),
	ENTRY_EXIT(dwc3_gadget_conndone_interrupt),
	ENTRY_EXIT(dwc3_gadget_pullup),
	ENTRY(__dwc3_gadget_start),
	ENTRY(trace_dwc3_ctrl_req),
	ENTRY(trace_dwc3_ep_queue),
	ENTRY(trace_dwc3_ep_dequeue),
	ENTRY(trace_dwc3_gadget_giveback),
	ENTRY(trace_dwc3_gadget_ep_cmd),
	ENTRY(trace_dwc3_prepare_trb),
	ENTRY(trace_dwc3_event),
	ENTRY_EXIT(dwc3_gadget_init_in_endpoint),
	ENTRY_EXIT(dwc3_gadget_init_out_endpoint),
};


int dwc3_msm_kretprobe_init(void)
{
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(dwc3_msm_probes) ; i++) {
		ret = register_kretprobe(&dwc3_msm_probes[i]);
		if (ret < 0) {
			pr_err("register_kretprobe failed, returned %d\n", ret);
			return ret;
		}
	}

	return 0;
}

void dwc3_msm_kretprobe_exit(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dwc3_msm_probes); i++)
		unregister_kretprobe(&dwc3_msm_probes[i]);
}

