// SPDX-License-Identifier: GPL-2.0
/* (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary. */

#include <linux/init.h>
#include <linux/log_gpu_error.h>
#include <linux/module.h>
#include <linux/sysctl.h>

#define MAX_GPU_ERRORS 10

static unsigned char gpu_error_msg[MAX_GPU_ERROR_LEN + 1];
static unsigned char gpu_additional_error_msgs[MAX_GPU_ERRORS][MAX_GPU_ERROR_LEN + 1];

static int gpu_error_pid = -1;
static int gpu_additional_error_pids[MAX_GPU_ERRORS];

static unsigned char gpu_error_process_name[MAX_GPU_ERROR_LEN + 1];
static unsigned char gpu_additional_error_process_names[MAX_GPU_ERRORS][MAX_GPU_ERROR_LEN + 1];

static unsigned int count_of_gpu_additional_errors;
static unsigned int current_gpu_error_index;

static unsigned int data_written;
static unsigned int data_read;

static struct ctl_table_header *ctl_table_hdr;

static spinlock_t gpu_error_log_lock;

static int gpu_error_data_read_handler(struct ctl_table *table, int write,
	void __user *buffer, size_t *lenp,
	loff_t *ppos);

static struct ctl_table gpu_error_table[] = {
	{
		.procname = "error",
		.data = &gpu_error_msg,
		.maxlen = MAX_GPU_ERROR_LEN,
		.mode = 0664,
		.proc_handler = proc_dostring,
	},
	{
		.procname = "pid",
		.data = &gpu_error_pid,
		.maxlen = sizeof(int),
		.mode = 0664,
		.proc_handler = proc_dointvec,
	},
	{
		.procname = "process_name",
		.data = &gpu_error_process_name,
		.maxlen = MAX_GPU_ERROR_LEN,
		.mode = 0664,
		.proc_handler = proc_dostring,
	},
	{
		.procname = "read",
		.data = &data_read,
		.maxlen = sizeof(unsigned int),
		.mode = 0666,
		.proc_handler = gpu_error_data_read_handler,
		.extra1 = SYSCTL_ZERO,
		.extra2 = SYSCTL_ONE,
	},
	{
		.procname = "written",
		.data = &data_written,
		.maxlen = sizeof(unsigned int),
		.mode = 0664,
		.proc_handler = proc_douintvec_minmax,
		.extra1 = SYSCTL_ZERO,
		.extra2 = SYSCTL_ONE,
	}, {}
};

static struct ctl_table gpu_error_base_table[] = {
	{
		.procname = "gpu_error",
		.mode = 0664,
		.child = gpu_error_table
	}, {}
};

static void reset_locked(void)
{
	/* Do not lock gpu_error_log_lock */

	if (count_of_gpu_additional_errors != current_gpu_error_index) {
		memcpy(gpu_error_msg,
			gpu_additional_error_msgs[current_gpu_error_index],
			MAX_GPU_ERROR_LEN);
		memset(gpu_additional_error_msgs[current_gpu_error_index], 0,
			MAX_GPU_ERROR_LEN + 1);

		gpu_error_pid =
			gpu_additional_error_pids[current_gpu_error_index];
		gpu_additional_error_pids[current_gpu_error_index] = -1;

		memcpy(gpu_error_process_name,
			gpu_additional_error_process_names[current_gpu_error_index],
			MAX_GPU_ERROR_LEN);
		memset(gpu_additional_error_process_names[current_gpu_error_index],
			0, MAX_GPU_ERROR_LEN + 1);

		data_written = 1;

		current_gpu_error_index += 1;

		if (count_of_gpu_additional_errors == current_gpu_error_index) {
			count_of_gpu_additional_errors = 0;
			current_gpu_error_index = 0;
		}
	} else {
		memset(gpu_error_msg, 0, MAX_GPU_ERROR_LEN + 1);
		gpu_error_pid = -1;
		memset(gpu_error_process_name, 0, MAX_GPU_ERROR_LEN + 1);

		data_written = 0;
	}

	/* Do not lock gpu_error_log_lock */
}

static int gpu_error_data_read_handler(struct ctl_table *table, int write,
	void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret = 0;
	unsigned long irq_flags = 0;

	spin_lock_irqsave(&gpu_error_log_lock, irq_flags);

	ret = proc_douintvec_minmax(table, write, buffer, lenp, ppos);

	if (!ret && write)
		reset_locked();

	spin_unlock_irqrestore(&gpu_error_log_lock, irq_flags);

	return ret;
}

static int __init log_gpu_init(void)
{
	int i = 0;

	memset(gpu_error_msg, 0, MAX_GPU_ERROR_LEN + 1);
	gpu_error_pid = -1;
	memset(gpu_error_process_name, 0, MAX_GPU_ERROR_LEN + 1);
	for (i = 0; i < MAX_GPU_ERRORS; ++i) {
		memset(gpu_additional_error_msgs[i], 0, MAX_GPU_ERROR_LEN + 1);
		gpu_additional_error_pids[i] = -1;
		memset(gpu_additional_error_process_names[i], 0, MAX_GPU_ERROR_LEN + 1);
	}

	count_of_gpu_additional_errors = 0;
	current_gpu_error_index = 0;
	data_written = 0;
	data_read = 0;

	ctl_table_hdr = register_sysctl_table(gpu_error_base_table);

	spin_lock_init(&gpu_error_log_lock);

	reset_locked();

	return 0;
}

static void __exit log_gpu_uninit(void)
{
	if (ctl_table_hdr != NULL)
		unregister_sysctl_table(ctl_table_hdr);
}

void log_gpu_error(const unsigned char *error_msg, const size_t error_msg_len,
	const int pid, const unsigned char *process_name, const size_t process_name_len)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&gpu_error_log_lock, irq_flags);

	if (data_written == 0) {
		if (error_msg != NULL) {
			memcpy(gpu_error_msg, error_msg,
				error_msg_len < MAX_GPU_ERROR_LEN ?
				error_msg_len : MAX_GPU_ERROR_LEN);
		}

		gpu_error_pid = pid;

		if (process_name != NULL) {
			memcpy(gpu_error_process_name, process_name,
				process_name_len < MAX_GPU_ERROR_LEN ?
				process_name_len : MAX_GPU_ERROR_LEN);
		}

		data_written = 1;
	} else if (count_of_gpu_additional_errors < MAX_GPU_ERRORS) {
		if (error_msg != NULL) {
			memcpy(gpu_additional_error_msgs[count_of_gpu_additional_errors],
				error_msg, error_msg_len < MAX_GPU_ERROR_LEN ?
				error_msg_len : MAX_GPU_ERROR_LEN);
		}

		gpu_additional_error_pids[count_of_gpu_additional_errors] = pid;

		if (process_name != NULL) {
			memcpy(gpu_additional_error_process_names[count_of_gpu_additional_errors],
				process_name, process_name_len < MAX_GPU_ERROR_LEN ?
				process_name_len : MAX_GPU_ERROR_LEN);
		}

		count_of_gpu_additional_errors += 1;
	}

	spin_unlock_irqrestore(&gpu_error_log_lock, irq_flags);
}
EXPORT_SYMBOL(log_gpu_error);

module_init(log_gpu_init);
module_exit(log_gpu_uninit);

MODULE_DESCRIPTION("Log GPU Errors Driver");
MODULE_LICENSE("GPL v2");
