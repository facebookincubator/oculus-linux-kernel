/*
 * Copyright (c) 2017-2018, 2020 The Linux Foundation. All rights reserved.
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

#ifndef _WLAN_HDD_SYSFS_H_
#define _WLAN_HDD_SYSFS_H_

/**
 * struct hdd_sysfs_print_ctx - keeps track of sysfs buffer printing
 * @buf: pointer to sysfs char buffer
 * @idx: current position in char buffer
 * @new_line: if set true, newline will be added at end of each print
 */
struct hdd_sysfs_print_ctx {
	char *buf;
	int idx;
	bool new_line;
};

#ifdef WLAN_SYSFS

#define MAX_SYSFS_USER_COMMAND_SIZE_LENGTH (32)
#define MAX_CMD_INPUT (512)

/**
 * hdd_sysfs_validate_and_copy_buf() - validate sysfs input buf and copy into
 *                                     destination buffer
 * @dest_buf: pointer to destination buffer where data should be copied
 * @dest_buf_size: size of destination buffer
 * @src_buf: pointer to constant sysfs source buffer
 * @src_buf_size: size of source buffer
 *
 * Return: 0 for success and error code for failure
 */
int
hdd_sysfs_validate_and_copy_buf(char *dest_buf, size_t dest_buf_size,
				char const *src_buf, size_t src_buf_size);

/**
 * hdd_create_sysfs_files() - create sysfs files
 * @hdd_ctx: pointer to hdd context
 *
 * Return: none
 */
void hdd_create_sysfs_files(struct hdd_context *hdd_ctx);

/**
 * hdd_destroy_sysfs_files() - destroy sysfs files
 *
 * Return: none
 */
void hdd_destroy_sysfs_files(void);

/**
 * hdd_create_adapter_sysfs_files - create adapter sysfs files
 * @adapter: pointer to adapter
 *
 * Return: none
 */
void hdd_create_adapter_sysfs_files(struct hdd_adapter *adapter);

/**
 * hdd_destroy_adapter_sysfs_files - destroy adapter sysfs files
 * @adapter: pointer to adapter
 *
 * Return: none
 */
void hdd_destroy_adapter_sysfs_files(struct hdd_adapter *adapter);

/**
 * hdd_create_wifi_feature_interface_sysfs_file - Create wifi feature interface
 * sysfs file
 *
 * Return: none
 */
void hdd_create_wifi_feature_interface_sysfs_file(void);

/**
 * hdd_destroy_wifi_feature_interface_sysfs_file - Destroy wifi feature
 * interface sysfs file
 *
 * Return: none
 */
void hdd_destroy_wifi_feature_interface_sysfs_file(void);

/**
 * hdd_sysfs_create_wifi_root_obj() - create wifi root kobj
 *
 * Return: none
 */
void hdd_sysfs_create_wifi_root_obj(void);

/**
 * hdd_sysfs_destroy_wifi_root_obj() - Destroy wifi root kobj
 *
 * Return: none
 */
void hdd_sysfs_destroy_wifi_root_obj(void);

/*
 * hdd_sysfs_print() - print to sysfs char buffer
 * @ctx: pointer to struct hdd_sysfs_print_ctx
 * @fmt: string format
 *
 * To use this function, create a hdd_sysfs_print_ctx variable, set
 * idx to 0, set buf to the outbuffer and set new_line to true or false.
 * Pass this context struct to every function call.
 * Using this function will then write the data to the outbuffer and
 * increment the counter.
 *
 * The context pointer is void to be compatible with qdf_abstract_print,
 * to allow for abstract printing.
 *
 * Return: Number of characters written
 */
int hdd_sysfs_print(void *ctx, const char *fmt, ...);

#else
static inline int
hdd_sysfs_validate_and_copy_buf(char *dest_buf, size_t dest_buf_size,
				char const *src_buf, size_t src_buf_size)
{
	return -EPERM;
}

static inline void hdd_create_sysfs_files(struct hdd_context *hdd_ctx)
{
}

static inline void hdd_destroy_sysfs_files(void)
{
}

static inline void hdd_create_adapter_sysfs_files(struct hdd_adapter *adapter)
{
}

static inline void hdd_destroy_adapter_sysfs_files(struct hdd_adapter *adapter)
{
}

static inline void hdd_create_wifi_feature_interface_sysfs_file(void)
{
}

static inline void hdd_destroy_wifi_feature_interface_sysfs_file(void)
{
}

static inline void hdd_sysfs_create_wifi_root_obj(void)
{
}

static inline void hdd_sysfs_destroy_wifi_root_obj(void)
{
}

#endif /* End of WLAN SYSFS*/

#endif /* End of _WLAN_HDD_SYSFS_H_ */
