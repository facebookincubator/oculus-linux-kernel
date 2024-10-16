/* SPDX-License-Identifier: GPL-2.0 */
/* (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary. */

#ifndef _LOG_GPU_ERROR_H_
#define _LOG_GPU_ERROR_H_

#define MAX_GPU_ERROR_LEN 255

void log_gpu_error(const unsigned char *error_msg, const size_t error_msg_len,
	const int pid, const unsigned char *process_name,
	const size_t process_name_len);

#endif /* _LOG_GPU_ERROR_H_ */
