/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _MMRM_VM_BE_H_
#define _MMRM_VM_BE_H_

#include <linux/soc/qcom/msm_mmrm.h>
#include <mmrm_vm_interface.h>

/*
 * mmrm_vm_be_recv -- BE message receiving thread call this function
 *                       for transfer receiving packet to BE
 * @mmrm_vm: device driver info
 * @data: message pointer
 * @size: message size
 */
int mmrm_vm_be_recv(struct mmrm_vm_driver_data *mmrm_vm, void *data, size_t size);

/*
 * mmrm_vm_be_send_response -- BE message receiving thread call this function
 *                             for sending back API calling result to FE
 * @mmrm_vm: specific device driver info
 * @size: message size
 */
int mmrm_vm_be_send_response(struct mmrm_vm_driver_data *mmrm_vm, void *msg);


#endif /* _MMRM_VM_BE_H_ */


