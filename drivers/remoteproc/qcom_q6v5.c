// SPDX-License-Identifier: GPL-2.0
/*
 * Qualcomm Peripheral Image Loader for Q6V5
 *
 * Copyright (C) 2016-2018 Linaro Ltd.
 * Copyright (C) 2014 Sony Mobile Communications AB
 * Copyright (c) 2012-2013, 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/soc/qcom/smem.h>
#include <linux/soc/qcom/smem_state.h>
#include <linux/remoteproc.h>
#include <linux/delay.h>
#include <linux/rbtree.h>
#include <linux/kthread.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#include "qcom_common.h"
#include "qcom_q6v5.h"
#include <trace/events/rproc_qcom.h>

#define Q6V5_PANIC_DELAY_MS	200
#define MAX_FW_FILE_SIZE (4 * 1024)
#define NAME_LEN 64
#define LINE_LEN 128
#define UUID_LEN 36
#define SMEM_BUFFER_LEN 4096

#ifdef CONFIG_QCOM_CRASH_SYMBOL_MATCH
struct symbol_entry {
	struct rb_node node;
	u32 addr;
	const char *name;
};

static struct rb_root symbol_tree = RB_ROOT;
#endif
/**
 * qcom_q6v5_prepare() - reinitialize the qcom_q6v5 context before start
 * @q6v5:	reference to qcom_q6v5 context to be reinitialized
 *
 * Return: 0 on success, negative errno on failure
 */
int qcom_q6v5_prepare(struct qcom_q6v5 *q6v5)
{
	reinit_completion(&q6v5->start_done);
	reinit_completion(&q6v5->stop_done);

	q6v5->running = true;
	q6v5->handover_issued = false;

	enable_irq(q6v5->handover_irq);

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_q6v5_prepare);

/**
 * qcom_q6v5_unprepare() - unprepare the qcom_q6v5 context after stop
 * @q6v5:	reference to qcom_q6v5 context to be unprepared
 *
 * Return: 0 on success, 1 if handover hasn't yet been called
 */
int qcom_q6v5_unprepare(struct qcom_q6v5 *q6v5)
{
	disable_irq(q6v5->handover_irq);

	return !q6v5->handover_issued;
}
EXPORT_SYMBOL_GPL(qcom_q6v5_unprepare);

void qcom_q6v5_register_ssr_subdev(struct qcom_q6v5 *q6v5, struct rproc_subdev *ssr_subdev)
{
	q6v5->ssr_subdev = ssr_subdev;
}
EXPORT_SYMBOL(qcom_q6v5_register_ssr_subdev);

static void qcom_q6v5_crash_handler_work(struct work_struct *work)
{
	struct qcom_q6v5 *q6v5 = container_of(work, struct qcom_q6v5, crash_handler);
	struct rproc *rproc = q6v5->rproc;
	struct rproc_subdev *subdev;
	int votes;

	mutex_lock(&rproc->lock);

	rproc->state = RPROC_CRASHED;

	votes = atomic_xchg(&rproc->power, 0);
	/* if votes are zero, rproc has already been shutdown */
	if (votes == 0) {
		mutex_unlock(&rproc->lock);
		return;
	}

	list_for_each_entry_reverse(subdev, &rproc->subdevs, node) {
		if (subdev->stop)
			subdev->stop(subdev, true);
	}

	mutex_unlock(&rproc->lock);

	/*
	 * Temporary workaround until ramdump userspace application calls
	 * sync() and fclose() on attempting the dump.
	 */
	msleep(100);
	panic("Panicking, remoteproc %s crashed\n", q6v5->rproc->name);
}

#ifdef CONFIG_QCOM_CRASH_SYMBOL_MATCH
static char *read_symbol_file(struct qcom_q6v5 *q6v5, const char *path, size_t *size_out)
{
	char *buf;
	int ret;
	const struct firmware *symtab = NULL;

	ret = request_firmware(&symtab, path, q6v5->dev);
	if (ret < 0) {
		dev_err(q6v5->dev, "request_firmware failed: %s (%ld)\n", path, ret);
		return ERR_PTR(ret);
	}
	buf = kvzalloc(symtab->size + 1, GFP_KERNEL);
	if (!buf) {
		release_firmware(symtab);
		return ERR_PTR(-ENOMEM);
	}
	if (symtab->data < 0) {
		dev_err(q6v5->dev, "Firmware is empty or invalid: %zd\n", symtab->data);
		kvfree(buf);
		release_firmware(symtab);
		return ERR_PTR(-EINVAL);
	}
	memcpy(buf, symtab->data, symtab->size);
	*size_out = symtab->size;
	release_firmware(symtab);
	return buf;
}

static int parse_symbols(struct qcom_q6v5 *q6v5, const char *buf, size_t size)
{
	const char *cur = buf;
	const char *end = buf + size;
	const char *line_start;
	char line[LINE_LEN];
	char name[NAME_LEN];
	uint32_t addr;
	int32_t len;
	struct symbol_entry *entry, *this;
	struct rb_node **new;
	struct rb_node *parent;

	while (cur < end) {
		len = 0;
		line_start = cur;

		while (cur < end && *cur != '\n')
			cur++;

		len = min((int)(cur - line_start), (int)(sizeof(line) - 1));
		memcpy(line, line_start, len);
		line[len] = '\0';

		if (cur < end)
			cur++;

		if (sscanf(line, "%x %63s", &addr, name) == 2) {
			entry = kzalloc(sizeof(*entry), GFP_KERNEL);
			if (!entry)
				return -ENOMEM;
			entry->addr = addr;
			entry->name = kstrdup(name, GFP_KERNEL);
			if (!entry->name) {
				kfree(entry);
				return -ENOMEM;
			}
			new = &symbol_tree.rb_node;
			parent = NULL;
			while (*new) {
				this = rb_entry(*new, struct symbol_entry, node);
				parent = *new;
				if (addr < this->addr)
					new = &(*new)->rb_left;
				else
					new = &(*new)->rb_right;
			}
			rb_link_node(&entry->node, parent, new);
			rb_insert_color(&entry->node, &symbol_tree);
		}
	}
	return 0;
}

static const char *match_function(u32 addr)
{
	struct rb_node *node = symbol_tree.rb_node;
	const char *closest = "none";

	while (node) {
		struct symbol_entry *entry = rb_entry(node, struct symbol_entry, node);

		if (entry->addr == addr)
			return entry->name;
		else if (entry->addr < addr) {
			closest = entry->name;
			node = node->rb_right;
		} else {
			node = node->rb_left;
		}
	}
	return closest;
}

static void symbol_loader_work(struct work_struct *work)
{
	size_t len;
	char *buf, *cur, *msg, *end, *token, *callstack_entry, *addr_start;
	const char *func;
	char uuid[UUID_LEN + 1];
	char path[LINE_LEN];
	size_t size;
	uint32_t addr;
	int ret;
	struct qcom_q6v5 *q6v5;

	q6v5 = container_of(work, struct qcom_q6v5, symbol_loader);
	msg = qcom_smem_get(q6v5->smem_host_id, q6v5->crash_stack, &len);
	if (IS_ERR(msg) || len < UUID_LEN) {
		dev_err(q6v5->dev, "Failed to get UUID from crash_stack\n");
		return;
	}

	if (len < (UUID_LEN + 1)) {
		dev_err(q6v5->dev, "Not enough data for UUID: %zu\n", len);
		return;
	}

	memcpy(uuid, msg + (len - (UUID_LEN + 1)), UUID_LEN);
	uuid[UUID_LEN] = '\0';

	snprintf(path, sizeof(path), "%s_symtab.txt", uuid);
	buf = read_symbol_file(q6v5, path, &size);
	if (IS_ERR(buf))
		return;
	ret = parse_symbols(q6v5, buf, size);
	kvfree(buf);
	if (ret) {
		dev_err(q6v5->dev, "Failed to parse symbols\n");
		return;
	}

	if (q6v5->crash_stack) {
		cur = msg;
		end = msg + len;
		token = strsep(&cur, "|");	/* do this once to get rid of the header */
		dev_err(q6v5->dev, "Stack Trace:\n");
		while (cur && cur < end) {
			callstack_entry = strsep(&cur, "|");
			if (!callstack_entry)
				break;
			addr_start = strpbrk(callstack_entry, ")");
			if (!addr_start)
				break;
			token = addr_start + 1;
			if (token[0] == '\0')
				continue;

			if (kstrtou32(token, 16, &addr) == 0) {
				func = match_function(addr);
				dev_err(q6v5->dev, "%s (0x%08x)\n", func, addr);
			}
		}
	}
}
#endif

static irqreturn_t q6v5_wdog_interrupt(int irq, void *data)
{
	struct qcom_q6v5 *q6v5 = data;
	struct qcom_rproc_ssr *ssr;
	size_t len;
	char *msg;

	/* Sometimes the stop triggers a watchdog rather than a stop-ack */
	if (!q6v5->running) {
		dev_info(q6v5->dev, "received wdog irq while q6 is offline\n");
		complete(&q6v5->stop_done);
		return IRQ_HANDLED;
	}

	msg = qcom_smem_get(QCOM_SMEM_HOST_ANY, q6v5->crash_reason, &len);
	if (!IS_ERR(msg) && len > 0 && msg[0])
		dev_err(q6v5->dev, "watchdog received: %s\n", msg);
	else
		dev_err(q6v5->dev, "watchdog without message\n");

	q6v5->running = false;
	trace_rproc_qcom_event(dev_name(q6v5->dev), "q6v5_wdog", msg);
	if (q6v5->rproc->recovery_disabled) {
		schedule_work(&q6v5->crash_handler);
	} else {
		if (q6v5->ssr_subdev) {
			qcom_notify_early_ssr_clients(q6v5->ssr_subdev);
			ssr = container_of(q6v5->ssr_subdev, struct qcom_rproc_ssr, subdev);
			ssr->is_notified = true;
		}

		rproc_report_crash(q6v5->rproc, RPROC_WATCHDOG);
	}

	return IRQ_HANDLED;
}

static irqreturn_t q6v5_fatal_interrupt(int irq, void *data)
{
	struct qcom_q6v5 *q6v5 = data;
	struct qcom_rproc_ssr *ssr;
	size_t len;
	char *msg;

	if (!q6v5->running) {
		dev_info(q6v5->dev, "received fatal irq while q6 is offline\n");
		return IRQ_HANDLED;
	}

	msg = qcom_smem_get(QCOM_SMEM_HOST_ANY, q6v5->crash_reason, &len);
	if (!IS_ERR(msg) && len > 0 && msg[0])
		dev_err(q6v5->dev, "fatal error received: %s\n", msg);
	else
		dev_err(q6v5->dev, "fatal error without message\n");

#ifdef CONFIG_QCOM_CRASH_SYMBOL_MATCH
	if (queue_work(system_freezable_wq, &q6v5->symbol_loader)) {
		dev_info(q6v5->dev, "Symbol loader work started\n");
		flush_work(&q6v5->symbol_loader);
	} else {
		dev_err(q6v5->dev, "Failed to queue symbol loader work\n");
	}
#endif
	q6v5->running = false;
	trace_rproc_qcom_event(dev_name(q6v5->dev), "q6v5_fatal", msg);
	if (q6v5->rproc->recovery_disabled) {
		schedule_work(&q6v5->crash_handler);
	} else {
		if (q6v5->ssr_subdev) {
			qcom_notify_early_ssr_clients(q6v5->ssr_subdev);
			ssr = container_of(q6v5->ssr_subdev, struct qcom_rproc_ssr, subdev);
			ssr->is_notified = true;
		}

		rproc_report_crash(q6v5->rproc, RPROC_FATAL_ERROR);
	}

	return IRQ_HANDLED;
}

static irqreturn_t q6v5_ready_interrupt(int irq, void *data)
{
	struct qcom_q6v5 *q6v5 = data;

	complete(&q6v5->start_done);

	return IRQ_HANDLED;
}

/**
 * qcom_q6v5_wait_for_start() - wait for remote processor start signal
 * @q6v5:	reference to qcom_q6v5 context
 * @timeout:	timeout to wait for the event, in jiffies
 *
 * qcom_q6v5_unprepare() should not be called when this function fails.
 *
 * Return: 0 on success, -ETIMEDOUT on timeout
 */
int qcom_q6v5_wait_for_start(struct qcom_q6v5 *q6v5, int timeout)
{
	int ret;

	ret = wait_for_completion_timeout(&q6v5->start_done, timeout);
	if (!ret)
		disable_irq(q6v5->handover_irq);

	return !ret ? -ETIMEDOUT : 0;
}
EXPORT_SYMBOL_GPL(qcom_q6v5_wait_for_start);

static irqreturn_t q6v5_handover_interrupt(int irq, void *data)
{
	struct qcom_q6v5 *q6v5 = data;

	if (q6v5->handover)
		q6v5->handover(q6v5);

	q6v5->handover_issued = true;

	return IRQ_HANDLED;
}

static irqreturn_t q6v5_stop_interrupt(int irq, void *data)
{
	struct qcom_q6v5 *q6v5 = data;

	complete(&q6v5->stop_done);

	return IRQ_HANDLED;
}

/**
 * qcom_q6v5_request_stop() - request the remote processor to stop
 * @q6v5:	reference to qcom_q6v5 context
 * @sysmon:	reference to the remote's sysmon instance, or NULL
 *
 * Return: 0 on success, negative errno on failure
 */
int qcom_q6v5_request_stop(struct qcom_q6v5 *q6v5, struct qcom_sysmon *sysmon)
{
	int ret;

	q6v5->running = false;

	/* Don't perform SMP2P dance if sysmon already shut
	 * down the remote or if it isn't running
	 */
	if (q6v5->rproc->state != RPROC_RUNNING || qcom_sysmon_shutdown_acked(sysmon))
		return 0;

	qcom_smem_state_update_bits(q6v5->state,
				    BIT(q6v5->stop_bit), BIT(q6v5->stop_bit));

	ret = wait_for_completion_timeout(&q6v5->stop_done, 5 * HZ);

	qcom_smem_state_update_bits(q6v5->state, BIT(q6v5->stop_bit), 0);

	return ret == 0 ? -ETIMEDOUT : 0;
}
EXPORT_SYMBOL_GPL(qcom_q6v5_request_stop);

/**
 * qcom_q6v5_panic() - panic handler to invoke a stop on the remote
 * @q6v5:	reference to qcom_q6v5 context
 *
 * Set the stop bit and sleep in order to allow the remote processor to flush
 * its caches etc for post mortem debugging.
 *
 * Return: 200ms
 */
unsigned long qcom_q6v5_panic(struct qcom_q6v5 *q6v5)
{
	qcom_smem_state_update_bits(q6v5->state,
				    BIT(q6v5->stop_bit), BIT(q6v5->stop_bit));

	return Q6V5_PANIC_DELAY_MS;
}
EXPORT_SYMBOL_GPL(qcom_q6v5_panic);

/**
 * qcom_q6v5_init() - initializer of the q6v5 common struct
 * @q6v5:	handle to be initialized
 * @pdev:	platform_device reference for acquiring resources
 * @rproc:	associated remoteproc instance
 * @crash_reason: SMEM id for crash reason string, or 0 if none
 * @handover:	function to be called when proxy resources should be released
 *
 * Return: 0 on success, negative errno on failure
 */
int qcom_q6v5_init(struct qcom_q6v5 *q6v5, struct platform_device *pdev,
		struct rproc *rproc, int crash_reason, int crash_stack, unsigned int smem_host_id,
		   void (*handover)(struct qcom_q6v5 *q6v5))
{
	int ret;

	q6v5->rproc = rproc;
	q6v5->dev = &pdev->dev;
	q6v5->crash_reason = crash_reason;
	q6v5->crash_stack = crash_stack;
	q6v5->smem_host_id = smem_host_id;
	q6v5->handover = handover;
	q6v5->ssr_subdev = NULL;

	init_completion(&q6v5->start_done);
	init_completion(&q6v5->stop_done);

	q6v5->wdog_irq = platform_get_irq_byname(pdev, "wdog");
	if (q6v5->wdog_irq < 0)
		return q6v5->wdog_irq;

	ret = devm_request_threaded_irq(&pdev->dev, q6v5->wdog_irq,
					NULL, q6v5_wdog_interrupt,
					IRQF_ONESHOT,
					"q6v5 wdog", q6v5);
	if (ret) {
		dev_err(&pdev->dev, "failed to acquire wdog IRQ\n");
		return ret;
	}

	q6v5->fatal_irq = platform_get_irq_byname(pdev, "fatal");
	if (q6v5->fatal_irq < 0)
		return q6v5->fatal_irq;

	ret = devm_request_threaded_irq(&pdev->dev, q6v5->fatal_irq,
					NULL, q6v5_fatal_interrupt,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"q6v5 fatal", q6v5);
	if (ret) {
		dev_err(&pdev->dev, "failed to acquire fatal IRQ\n");
		return ret;
	}

	q6v5->ready_irq = platform_get_irq_byname(pdev, "ready");
	if (q6v5->ready_irq < 0)
		return q6v5->ready_irq;

	ret = devm_request_threaded_irq(&pdev->dev, q6v5->ready_irq,
					NULL, q6v5_ready_interrupt,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"q6v5 ready", q6v5);
	if (ret) {
		dev_err(&pdev->dev, "failed to acquire ready IRQ\n");
		return ret;
	}

	q6v5->handover_irq = platform_get_irq_byname(pdev, "handover");
	if (q6v5->handover_irq < 0)
		return q6v5->handover_irq;

	ret = devm_request_threaded_irq(&pdev->dev, q6v5->handover_irq,
					NULL, q6v5_handover_interrupt,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"q6v5 handover", q6v5);
	if (ret) {
		dev_err(&pdev->dev, "failed to acquire handover IRQ\n");
		return ret;
	}
	disable_irq(q6v5->handover_irq);

	q6v5->stop_irq = platform_get_irq_byname(pdev, "stop-ack");
	if (q6v5->stop_irq < 0)
		return q6v5->stop_irq;

	ret = devm_request_threaded_irq(&pdev->dev, q6v5->stop_irq,
					NULL, q6v5_stop_interrupt,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"q6v5 stop", q6v5);
	if (ret) {
		dev_err(&pdev->dev, "failed to acquire stop-ack IRQ\n");
		return ret;
	}

	q6v5->state = qcom_smem_state_get(&pdev->dev, "stop", &q6v5->stop_bit);
	if (IS_ERR(q6v5->state)) {
		dev_err(&pdev->dev, "failed to acquire stop state\n");
		return PTR_ERR(q6v5->state);
	}

	INIT_WORK(&q6v5->crash_handler, qcom_q6v5_crash_handler_work);

#ifdef CONFIG_QCOM_CRASH_SYMBOL_MATCH
	INIT_WORK(&q6v5->symbol_loader, symbol_loader_work);
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(qcom_q6v5_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Peripheral Image Loader for Q6V5");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
