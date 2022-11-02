// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2017,2019-2020 The Linux Foundation. All rights reserved.
 */

#include "adreno.h"
#include "adreno_a5xx.h"
#include "adreno_pm4types.h"
#include "adreno_trace.h"

#define PREEMPT_RECORD(_field) \
		offsetof(struct a5xx_cp_preemption_record, _field)

static void _update_wptr(struct adreno_device *adreno_dev, bool reset_timer)
{
	struct adreno_ringbuffer *rb = adreno_dev->cur_rb;
	unsigned int wptr;
	unsigned long flags;

	spin_lock_irqsave(&rb->preempt_lock, flags);

	adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_WPTR, &wptr);

	if (wptr != rb->wptr) {
		adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_WPTR,
			rb->wptr);
		/*
		 * In case something got submitted while preemption was on
		 * going, reset the timer.
		 */
		reset_timer = true;
	}

	if (reset_timer)
		rb->dispatch_q.expires = jiffies +
			msecs_to_jiffies(adreno_drawobj_timeout);

	spin_unlock_irqrestore(&rb->preempt_lock, flags);
}

static void _a5xx_preemption_done(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int status;

	/*
	 * In the very unlikely case that the power is off, do nothing - the
	 * state will be reset on power up and everybody will be happy
	 */

	if (!kgsl_state_is_awake(device))
		return;

	adreno_readreg(adreno_dev, ADRENO_REG_CP_PREEMPT, &status);

	if (status != 0) {
		dev_err(device->dev,
			     "Preemption not complete: status=%X cur=%d R/W=%X/%X next=%d R/W=%X/%X\n",
			     status, adreno_dev->cur_rb->id,
			     adreno_get_rptr(adreno_dev->cur_rb),
			     adreno_dev->cur_rb->wptr,
			     adreno_dev->next_rb->id,
			     adreno_get_rptr(adreno_dev->next_rb),
			     adreno_dev->next_rb->wptr);

		/* Set a fault and restart */
		adreno_set_gpu_fault(adreno_dev, ADRENO_PREEMPT_FAULT);
		adreno_dispatcher_schedule(device);

		return;
	}

	del_timer_sync(&adreno_dev->preempt.timer);

	trace_adreno_preempt_done(adreno_dev->cur_rb, adreno_dev->next_rb, 0);

	/* Clean up all the bits */
	adreno_dev->prev_rb = adreno_dev->cur_rb;
	adreno_dev->cur_rb = adreno_dev->next_rb;
	adreno_dev->next_rb = NULL;

	/* Update the wptr for the new command queue */
	_update_wptr(adreno_dev, true);

	/* Update the dispatcher timer for the new command queue */
	mod_timer(&adreno_dev->dispatcher.timer,
		adreno_dev->cur_rb->dispatch_q.expires);

	/* Clear the preempt state */
	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);
}

static void _a5xx_preemption_fault(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int status;

	/*
	 * If the power is on check the preemption status one more time - if it
	 * was successful then just transition to the complete state
	 */
	if (kgsl_state_is_awake(device)) {
		adreno_readreg(adreno_dev, ADRENO_REG_CP_PREEMPT, &status);

		if (status == 0) {
			adreno_set_preempt_state(adreno_dev,
				ADRENO_PREEMPT_COMPLETE);

			adreno_dispatcher_schedule(device);
			return;
		}
	}

	dev_err(device->dev,
		     "Preemption timed out: cur=%d R/W=%X/%X, next=%d R/W=%X/%X\n",
		     adreno_dev->cur_rb->id,
		     adreno_get_rptr(adreno_dev->cur_rb),
		     adreno_dev->cur_rb->wptr,
		     adreno_dev->next_rb->id,
		     adreno_get_rptr(adreno_dev->next_rb),
		     adreno_dev->next_rb->wptr);

	adreno_set_gpu_fault(adreno_dev, ADRENO_PREEMPT_FAULT);
	adreno_dispatcher_schedule(device);
}

static void _a5xx_preemption_worker(struct work_struct *work)
{
	struct adreno_preemption *preempt = container_of(work,
		struct adreno_preemption, work);
	struct adreno_device *adreno_dev = container_of(preempt,
		struct adreno_device, preempt);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/* Need to take the mutex to make sure that the power stays on */
	mutex_lock(&device->mutex);

	if (adreno_in_preempt_state(adreno_dev, ADRENO_PREEMPT_FAULTED))
		_a5xx_preemption_fault(adreno_dev);

	mutex_unlock(&device->mutex);
}

/* Find the highest priority active ringbuffer */
static struct adreno_ringbuffer *a5xx_next_ringbuffer(
		struct adreno_device *adreno_dev)
{
	struct adreno_ringbuffer *rb;
	unsigned long flags;
	unsigned int i;

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		bool empty;

		spin_lock_irqsave(&rb->preempt_lock, flags);
		empty = adreno_rb_empty(rb);
		spin_unlock_irqrestore(&rb->preempt_lock, flags);

		if (!empty)
			return rb;
	}

	return NULL;
}

void a5xx_preemption_trigger(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);
	struct a5xx_cp_smmu_info *smmu_info =
			(struct a5xx_cp_smmu_info *)iommu->kptr;
	struct adreno_ringbuffer *next;
	struct a5xx_cp_preemption_record *preempt_record;
	struct adreno_ringbuffer_pagetable_info *pt_info;
	uint64_t ttbr0;
	unsigned int contextidr;
	unsigned long flags;

	/* Put ourselves into a possible trigger state */
	if (!adreno_move_preempt_state(adreno_dev,
		ADRENO_PREEMPT_NONE, ADRENO_PREEMPT_START))
		return;

	/* Get the next ringbuffer to preempt in */
	next = a5xx_next_ringbuffer(adreno_dev);

	/*
	 * Nothing to do if every ringbuffer is empty or if the current
	 * ringbuffer is the only active one
	 */
	if (next == NULL || next == adreno_dev->cur_rb) {
		/*
		 * Update any critical things that might have been skipped while
		 * we were looking for a new ringbuffer
		 */

		if (next != NULL) {
			_update_wptr(adreno_dev, false);

			mod_timer(&adreno_dev->dispatcher.timer,
				adreno_dev->cur_rb->dispatch_q.expires);
		}

		adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);
		return;
	}

	/* Turn off the dispatcher timer */
	del_timer(&adreno_dev->dispatcher.timer);

	/*
	 * This is the most critical section - we need to take care not to race
	 * until we have programmed the CP for the switch
	 */

	spin_lock_irqsave(&next->preempt_lock, flags);

	/*
	 * Get the pagetable from the pagetable info.
	 * The pagetable_desc is allocated and mapped at probe time, and
	 * preemption_desc at init time, so no need to check if
	 * sharedmem accesses to these memdescs succeed.
	 */
	pt_info = (struct adreno_ringbuffer_pagetable_info *)next->pagetable_kptr;
	/* Make sure any pending writes to the pagetable info complete first. */
	smp_rmb();
	ttbr0 = pt_info->ttbr0;
	contextidr = pt_info->contextidr;

	preempt_record = (struct a5xx_cp_preemption_record *)
			next->preemption_kptr;
	preempt_record->wptr = next->wptr;
	/* Make sure the wptr write is posted before continuing */
	smp_wmb();

	spin_unlock_irqrestore(&next->preempt_lock, flags);

	/* And write it to the smmu info */
	smmu_info->ttbr0 = ttbr0;
	smmu_info->context_idr = contextidr;
	/* Make sure the writes are posted before continuing */
	smp_wmb();

	kgsl_regwrite(device, A5XX_CP_CONTEXT_SWITCH_RESTORE_ADDR_LO,
		lower_32_bits(next->preemption_desc->gpuaddr));
	kgsl_regwrite(device, A5XX_CP_CONTEXT_SWITCH_RESTORE_ADDR_HI,
		upper_32_bits(next->preemption_desc->gpuaddr));

	adreno_dev->next_rb = next;

	/* Start the timer to detect a stuck preemption */
	mod_timer(&adreno_dev->preempt.timer,
		jiffies + msecs_to_jiffies(ADRENO_PREEMPT_TIMEOUT));

	trace_adreno_preempt_trigger(adreno_dev->cur_rb, adreno_dev->next_rb,
		1);

	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_TRIGGERED);

	/* Trigger the preemption */
	adreno_writereg(adreno_dev, ADRENO_REG_CP_PREEMPT, 1);
}

void a5xx_preempt_callback(struct adreno_device *adreno_dev, int bit)
{
	unsigned int status;

	if (!adreno_move_preempt_state(adreno_dev,
		ADRENO_PREEMPT_TRIGGERED, ADRENO_PREEMPT_PENDING))
		return;

	adreno_readreg(adreno_dev, ADRENO_REG_CP_PREEMPT, &status);

	if (status != 0) {
		dev_err(KGSL_DEVICE(adreno_dev)->dev,
			     "preempt interrupt with non-zero status: %X\n",
			     status);

		/*
		 * Under the assumption that this is a race between the
		 * interrupt and the register, schedule the worker to clean up.
		 * If the status still hasn't resolved itself by the time we get
		 * there then we have to assume something bad happened
		 */
		adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_COMPLETE);
		adreno_dispatcher_schedule(KGSL_DEVICE(adreno_dev));
		return;
	}

	del_timer(&adreno_dev->preempt.timer);

	trace_adreno_preempt_done(adreno_dev->cur_rb, adreno_dev->next_rb, 0);

	adreno_dev->prev_rb = adreno_dev->cur_rb;
	adreno_dev->cur_rb = adreno_dev->next_rb;
	adreno_dev->next_rb = NULL;

	/* Update the wptr if it changed while preemption was ongoing */
	_update_wptr(adreno_dev, true);

	/* Update the dispatcher timer for the new command queue */
	mod_timer(&adreno_dev->dispatcher.timer,
		adreno_dev->cur_rb->dispatch_q.expires);

	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);

	a5xx_preemption_trigger(adreno_dev);
}

void a5xx_preemption_schedule(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!adreno_is_preemption_enabled(adreno_dev))
		return;

	mutex_lock(&device->mutex);

	if (adreno_in_preempt_state(adreno_dev, ADRENO_PREEMPT_COMPLETE))
		_a5xx_preemption_done(adreno_dev);

	a5xx_preemption_trigger(adreno_dev);

	mutex_unlock(&device->mutex);
}

unsigned int a5xx_preemption_pre_ibsubmit(
			struct adreno_device *adreno_dev,
			struct adreno_ringbuffer *rb,
			unsigned int *cmds, struct kgsl_context *context)
{
	unsigned int *cmds_orig = cmds;
	uint64_t gpuaddr = rb->preemption_desc->gpuaddr;
	unsigned int preempt_style = 0;

	if (context) {
		/*
		 * Preemption from secure to unsecure needs Zap shader to be
		 * run to clear all secure content. CP does not know during
		 * preemption if it is switching between secure and unsecure
		 * contexts so restrict Secure contexts to be preempted at
		 * ringbuffer level.
		 */
		if (context->flags & KGSL_CONTEXT_SECURE)
			preempt_style = KGSL_CONTEXT_PREEMPT_STYLE_RINGBUFFER;
		else
			preempt_style = ADRENO_PREEMPT_STYLE(context->flags);
	}

	/*
	 * CP_PREEMPT_ENABLE_GLOBAL(global preemption) can only be set by KMD
	 * in ringbuffer.
	 * 1) set global preemption to 0x0 to disable global preemption.
	 *    Only RB level preemption is allowed in this mode
	 * 2) Set global preemption to defer(0x2) for finegrain preemption.
	 *    when global preemption is set to defer(0x2),
	 *    CP_PREEMPT_ENABLE_LOCAL(local preemption) determines the
	 *    preemption point. Local preemption
	 *    can be enabled by both UMD(within IB) and KMD.
	 */
	*cmds++ = cp_type7_packet(CP_PREEMPT_ENABLE_GLOBAL, 1);
	*cmds++ = ((preempt_style == KGSL_CONTEXT_PREEMPT_STYLE_FINEGRAIN)
				? 2 : 0);

	/* Turn CP protection OFF */
	cmds += cp_protected_mode(adreno_dev, cmds, 0);

	/*
	 * CP during context switch will save context switch info to
	 * a5xx_cp_preemption_record pointed by CONTEXT_SWITCH_SAVE_ADDR
	 */
	*cmds++ = cp_type4_packet(A5XX_CP_CONTEXT_SWITCH_SAVE_ADDR_LO, 1);
	*cmds++ = lower_32_bits(gpuaddr);
	*cmds++ = cp_type4_packet(A5XX_CP_CONTEXT_SWITCH_SAVE_ADDR_HI, 1);
	*cmds++ = upper_32_bits(gpuaddr);

	/* Turn CP protection ON */
	cmds += cp_protected_mode(adreno_dev, cmds, 1);

	/*
	 * Enable local preemption for finegrain preemption in case of
	 * a misbehaving IB
	 */
	if (preempt_style == KGSL_CONTEXT_PREEMPT_STYLE_FINEGRAIN) {
		*cmds++ = cp_type7_packet(CP_PREEMPT_ENABLE_LOCAL, 1);
		*cmds++ = 1;
	} else {
		*cmds++ = cp_type7_packet(CP_PREEMPT_ENABLE_LOCAL, 1);
		*cmds++ = 0;
	}

	/* Enable CP_CONTEXT_SWITCH_YIELD packets in the IB2s */
	*cmds++ = cp_type7_packet(CP_YIELD_ENABLE, 1);
	*cmds++ = 2;

	return (unsigned int) (cmds - cmds_orig);
}

int a5xx_preemption_yield_enable(unsigned int *cmds)
{
	/*
	 * SRM -- set render mode (ex binning, direct render etc)
	 * SRM is set by UMD usually at start of IB to tell CP the type of
	 * preemption.
	 * KMD needs to set SRM to NULL to indicate CP that rendering is
	 * done by IB.
	 */
	*cmds++ = cp_type7_packet(CP_SET_RENDER_MODE, 5);
	*cmds++ = 0;
	*cmds++ = 0;
	*cmds++ = 0;
	*cmds++ = 0;
	*cmds++ = 0;

	*cmds++ = cp_type7_packet(CP_YIELD_ENABLE, 1);
	*cmds++ = 1;

	return 8;
}

unsigned int a5xx_preemption_post_ibsubmit(struct adreno_device *adreno_dev,
	unsigned int *cmds)
{
	int dwords = 0;

	cmds[dwords++] = cp_type7_packet(CP_CONTEXT_SWITCH_YIELD, 4);
	/* Write NULL to the address to skip the data write */
	dwords += cp_gpuaddr(adreno_dev, &cmds[dwords], 0x0);
	cmds[dwords++] = 1;
	/* generate interrupt on preemption completion */
	cmds[dwords++] = 1;

	return dwords;
}

void a5xx_preemption_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);
	struct adreno_ringbuffer *rb;
	struct a5xx_cp_smmu_info *smmu_info =
			(struct a5xx_cp_smmu_info *)iommu->kptr;
	struct a5xx_cp_preemption_record *preempt_record;
	unsigned int i;

	if (!adreno_is_preemption_enabled(adreno_dev))
		return;

	/* Force the state to be clear */
	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);

	/* smmu_info is allocated and mapped in a5xx_preemption_iommu_init */
	smmu_info->magic = A5XX_CP_SMMU_INFO_MAGIC_REF;
	smmu_info->ttbr0 = MMU_DEFAULT_TTBR0(device);
	/* The CP doesn't use the asid record, so poison it */
	smmu_info->asid = 0xDECAFBAD;
	smmu_info->context_idr = MMU_DEFAULT_CONTEXTIDR(device);
	/* Make sure the writes are posted before continuing */
	smp_wmb();

	adreno_writereg64(adreno_dev,
			ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_LO,
			ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_HI,
			iommu->smmu_info->gpuaddr);

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		/*
		 * preemption_kptr is mapped at ringbuffer init time, so no
		 * need to check that it is valid: if ringbuffer creation or
		 * mapping failed then we wouldn't get here in the first place.
		 */
		preempt_record = (struct a5xx_cp_preemption_record *)
				rb->preemption_kptr;
		preempt_record->rptr = 0;
		preempt_record->wptr = 0;
		/* Make sure the writes are posted before continuing */
		smp_wmb();

		adreno_ringbuffer_set_pagetable(rb,
			device->mmu.defaultpagetable);
	}

}

static int a5xx_preemption_ringbuffer_init(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb, uint64_t counteraddr)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a5xx_cp_preemption_record *preempt_record;
	unsigned int page_count = 0;

	if (IS_ERR_OR_NULL(rb->preemption_desc)) {
		rb->preemption_desc = kgsl_allocate_global(device,
			A5XX_CP_CTXRECORD_SIZE_IN_BYTES, 0,
			KGSL_MEMDESC_PRIVILEGED, "preemption_desc");
		if (IS_ERR_OR_NULL(rb->preemption_desc))
			return (!rb->preemption_desc) ? -EINVAL :
					PTR_ERR(rb->preemption_desc);

		/*
		 * We only need/want to map the first page of the preemption
		 * buffer into the kernel since that's where we store shared
		 * CPU/GPU state.
		 */
		rb->preemption_kptr = kgsl_sharedmem_vm_map_readwrite(
				rb->preemption_desc, 0, PAGE_SIZE, &page_count);

		if (IS_ERR_OR_NULL(rb->preemption_kptr))
			return (!rb->preemption_kptr) ? -EINVAL :
					PTR_ERR(rb->preemption_kptr);
		else if (WARN(page_count != 1,
			 "preemption record mapping size is incorrect\n"))
			return -EINVAL;
	}

	/* Reset the state of the preemption record. */
	if (!IS_ERR_OR_NULL(rb->preemption_kptr)) {
		preempt_record = (struct a5xx_cp_preemption_record *)
				rb->preemption_kptr;
		preempt_record->magic = A5XX_CP_CTXRECORD_MAGIC_REF;
		preempt_record->info = 0;
		preempt_record->data = 0;
		preempt_record->cntl = A5XX_CP_RB_CNTL_DEFAULT;
		preempt_record->rptr = 0;
		preempt_record->wptr = 0;
		preempt_record->rptr_addr = SCRATCH_RPTR_GPU_ADDR(device, rb->id);
		preempt_record->rbase = rb->buffer_desc->gpuaddr;
		preempt_record->counter = counteraddr;
		/* Make sure the writes are posted before continuing */
		smp_wmb();
	}

	return 0;
}

int a5xx_preemption_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);
	struct adreno_preemption *preempt = &adreno_dev->preempt;
	struct adreno_ringbuffer *rb;
	int ret;
	unsigned int i;
	unsigned int page_count = 0;
	uint64_t addr;

	/* We are dependent on IOMMU to make preemption go on the CP side */
	if (kgsl_mmu_get_mmutype(device) != KGSL_MMU_TYPE_IOMMU)
		return -ENODEV;

	INIT_WORK(&preempt->work, _a5xx_preemption_worker);

	/* Allocate mem for storing preemption counters */
	if (IS_ERR_OR_NULL(preempt->scratch)) {
		preempt->scratch = kgsl_allocate_global(device,
			adreno_dev->num_ringbuffers *
			A5XX_CP_CTXRECORD_PREEMPTION_COUNTER_SIZE, 0, 0,
			"preemption_counters");
		if (IS_ERR_OR_NULL(preempt->scratch))
			return (!preempt->scratch) ? -EINVAL :
					PTR_ERR(preempt->scratch);

		preempt->scratch_kptr = kgsl_sharedmem_vm_map_readonly(
				preempt->scratch, 0, preempt->scratch->size,
				&page_count);

		if (IS_ERR_OR_NULL(preempt->scratch_kptr))
			return (!preempt->scratch_kptr) ? -EINVAL :
					PTR_ERR(preempt->scratch_kptr);
		else if (WARN(page_count != (preempt->scratch->size >> PAGE_SHIFT),
			 "preemption scratch mapping size does not match buffer size\n"))
			return -EINVAL;
	}

	addr = preempt->scratch->gpuaddr;

	/* Allocate mem for storing preemption switch record */
	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		ret = a5xx_preemption_ringbuffer_init(adreno_dev, rb, addr);
		if (ret)
			return ret;

		addr += A5XX_CP_CTXRECORD_PREEMPTION_COUNTER_SIZE;
	}

	/* Allocate mem for storing preemption smmu record */
	if (IS_ERR_OR_NULL(iommu->smmu_info)) {
		iommu->smmu_info = kgsl_allocate_global(device, PAGE_SIZE,
			KGSL_MEMFLAGS_GPUREADONLY, KGSL_MEMDESC_PRIVILEGED,
			"smmu_info");
		if (IS_ERR_OR_NULL(iommu->smmu_info))
			return (!iommu->smmu_info) ? -EINVAL :
					PTR_ERR(iommu->smmu_info);

		iommu->kptr = kgsl_sharedmem_vm_map_readwrite(iommu->smmu_info,
				0, iommu->smmu_info->size, &page_count);

		if (IS_ERR_OR_NULL(iommu->kptr))
			return (!iommu->kptr) ? -EINVAL : PTR_ERR(iommu->kptr);
		else if (WARN(page_count != 1,
			 "smmu_info mapping size does not match buffer size\n"))
			return -EINVAL;
	}

	set_bit(ADRENO_DEVICE_PREEMPTION, &adreno_dev->priv);
	return 0;
}
