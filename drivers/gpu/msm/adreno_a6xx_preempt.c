// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include "adreno.h"
#include "adreno_a6xx.h"
#include "adreno_pm4types.h"
#include "adreno_trace.h"

#define PREEMPT_RECORD(_field) \
		offsetof(struct a6xx_cp_preemption_record, _field)

enum {
	SET_PSEUDO_REGISTER_SAVE_REGISTER_SMMU_INFO = 0,
	SET_PSEUDO_REGISTER_SAVE_REGISTER_PRIV_NON_SECURE_SAVE_ADDR,
	SET_PSEUDO_REGISTER_SAVE_REGISTER_PRIV_SECURE_SAVE_ADDR,
	SET_PSEUDO_REGISTER_SAVE_REGISTER_NON_PRIV_SAVE_ADDR,
	SET_PSEUDO_REGISTER_SAVE_REGISTER_COUNTER,
};

static void _update_wptr(struct adreno_device *adreno_dev, bool reset_timer)
{
	struct adreno_ringbuffer *rb = adreno_dev->cur_rb;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&rb->preempt_lock, flags);

	if (in_interrupt() == 0) {
		/*
		 * We might have skipped updating the wptr in case we are in
		 * dispatcher context. Do it now.
		 */
		if (rb->skip_inline_wptr) {

			ret = adreno_gmu_fenced_write(adreno_dev,
				ADRENO_REG_CP_RB_WPTR, rb->wptr,
				FENCE_STATUS_WRITEDROPPED0_MASK);

			reset_timer = true;
			rb->skip_inline_wptr = false;
		}
	} else {
		unsigned int wptr;

		adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_WPTR, &wptr);
		if (wptr != rb->wptr) {
			adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_WPTR,
				rb->wptr);
			reset_timer = true;
		}
	}

	if (reset_timer)
		rb->dispatch_q.expires = jiffies +
			msecs_to_jiffies(adreno_drawobj_timeout);

	spin_unlock_irqrestore(&rb->preempt_lock, flags);

	if (in_interrupt() == 0) {
		/* If WPTR update fails, set the fault and trigger recovery */
		if (ret) {
			adreno_set_gpu_fault(adreno_dev, ADRENO_GMU_FAULT);
			adreno_dispatcher_schedule(KGSL_DEVICE(adreno_dev));
		}
	}
}

static void _a6xx_update_active_time(struct adreno_device *adreno_dev)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_context *drawctxt;
	struct kgsl_thread_private *thread;
	struct adreno_drawobj_profile_entry *entry;
	void *hostptr;

	volatile uint64_t *activated_ptr;
	uint64_t active_ticks = 0, preempt_ticks = 0;
	unsigned int profile_index;

	/*
	 * Grab the profiling buffer and thread pointers. Bail if either
	 * pointer is invalid.
	 */
	hostptr = adreno_dev->profile_kptr;
	drawctxt = adreno_dev->cur_rb->drawctxt_active;
	thread = (drawctxt != NULL) ? drawctxt->base.thread_priv : NULL;
	if (hostptr == NULL || IS_ERR_OR_NULL(thread))
		return;

	/* Read out the always-on counter ticks. Bail out on failure. */
	preempt_ticks = gpudev->read_alwayson(adreno_dev);
	if (preempt_ticks == 0)
		return;

	/*
	 * Grab the latest value of the current RB's profile index. Skip over
	 * it if it's out-of-bounds.
	 */
	smp_rmb();
	profile_index = *(volatile unsigned int *)
		(hostptr + RB_PROFILE_INDEX_OFFSET(adreno_dev->cur_rb));
	if (profile_index < ADRENO_DRAWOBJ_PROFILE_COUNT) {
		/* Grab the entry pointer associated with this profile index. */
		entry = (struct adreno_drawobj_profile_entry *)
			(hostptr + (profile_index * sizeof(*entry)) +
			ADRENO_DRAWOBJ_PROFILE_BASE);
		activated_ptr = (volatile uint64_t *)&entry->activated;

		/*
		 * Read out when this cmdobj last became active. Nothing needs
		 * to be done if the cmdobj has already retired or is already
		 * inactive.
		 */
		active_ticks = *activated_ptr;
		if (entry->retired > 0 || active_ticks == 0)
			goto handle_next_rb;
		*activated_ptr = 0;

		/* Make sure the update to entry->activated flushes. */
		smp_wmb();

		/*
		 * Add the time since this entry last activated to the cmdobj's
		 * active time and then update the activated time entry to
		 * match preempt_ticks.
		 */
		thread->stats[KGSL_THREADSTATS_ACTIVE_TIME] +=
			(preempt_ticks - active_ticks) * 10000 / 192;
	}

handle_next_rb:
	/*
	 * Now grab the latest value for the next RB's profile index. If it
	 * is valid, then that ringbuffer is being reactivated, so we need
	 * to update its 'activated' ticks. This happens when the GPU returns
	 * to working on a preempted cmdobj, for example.
	 */
	smp_rmb();
	profile_index = *(volatile unsigned int *)
		(hostptr + RB_PROFILE_INDEX_OFFSET(adreno_dev->next_rb));
	if (profile_index < ADRENO_DRAWOBJ_PROFILE_COUNT) {
		/* Grab the entry pointer associated with this profile index. */
		entry = (struct adreno_drawobj_profile_entry *)
			(hostptr + (profile_index * sizeof(*entry)) +
			ADRENO_DRAWOBJ_PROFILE_BASE);
		activated_ptr = (volatile uint64_t *)&entry->activated;

		/* Mark the next RB's cmdobj as active. */
		*activated_ptr = preempt_ticks;

		/* Make sure the update to entry->activated flushes. */
		smp_wmb();
	}
}

static void _a6xx_preemption_done(struct adreno_device *adreno_dev)
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

	if (status & 0x1) {
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

	adreno_dev->preempt.count++;

	del_timer_sync(&adreno_dev->preempt.timer);

	_a6xx_update_active_time(adreno_dev);

	adreno_readreg(adreno_dev, ADRENO_REG_CP_PREEMPT_LEVEL_STATUS, &status);

	trace_adreno_preempt_done(adreno_dev->cur_rb, adreno_dev->next_rb,
		status);

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

static void _a6xx_preemption_fault(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int status;

	/*
	 * If the power is on check the preemption status one more time - if it
	 * was successful then just transition to the complete state
	 */
	if (kgsl_state_is_awake(device)) {
		adreno_readreg(adreno_dev, ADRENO_REG_CP_PREEMPT, &status);

		if (!(status & 0x1)) {
			adreno_set_preempt_state(adreno_dev,
				ADRENO_PREEMPT_COMPLETE);

			adreno_dispatcher_schedule(device);
			return;
		}
	}

	dev_err(device->dev,
		     "Preemption Fault: cur=%d R/W=0x%x/0x%x, next=%d R/W=0x%x/0x%x\n",
		     adreno_dev->cur_rb->id,
		     adreno_get_rptr(adreno_dev->cur_rb),
		     adreno_dev->cur_rb->wptr,
		     adreno_dev->next_rb->id,
		     adreno_get_rptr(adreno_dev->next_rb),
		     adreno_dev->next_rb->wptr);

	adreno_set_gpu_fault(adreno_dev, ADRENO_PREEMPT_FAULT);
	adreno_dispatcher_schedule(device);
}

static void _a6xx_preemption_worker(struct work_struct *work)
{
	struct adreno_preemption *preempt = container_of(work,
		struct adreno_preemption, work);
	struct adreno_device *adreno_dev = container_of(preempt,
		struct adreno_device, preempt);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/* Need to take the mutex to make sure that the power stays on */
	mutex_lock(&device->mutex);

	if (adreno_in_preempt_state(adreno_dev, ADRENO_PREEMPT_FAULTED))
		_a6xx_preemption_fault(adreno_dev);

	mutex_unlock(&device->mutex);
}

/* Find the highest priority active ringbuffer */
static struct adreno_ringbuffer *a6xx_next_ringbuffer(
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

void a6xx_preemption_trigger(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);
	struct adreno_ringbuffer *next;
	uint64_t ttbr0, gpuaddr;
	unsigned int contextidr, cntl;
	unsigned long flags;
	struct adreno_preemption *preempt = &adreno_dev->preempt;
	struct a6xx_cp_smmu_info *smmu_info =
			(struct a6xx_cp_smmu_info *)iommu->kptr;
	struct a6xx_cp_preemption_record *preempt_record;
	struct adreno_ringbuffer_pagetable_info *pt_info;

	/* Put ourselves into a possible trigger state */
	if (!adreno_move_preempt_state(adreno_dev,
		ADRENO_PREEMPT_NONE, ADRENO_PREEMPT_START))
		return;

	/* Get the next ringbuffer to preempt in */
	next = a6xx_next_ringbuffer(adreno_dev);

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

	preempt_record = (struct a6xx_cp_preemption_record *)
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

	/* Make sure any pending writes to the scratch buffer complete first. */
	smp_rmb();
	gpuaddr = *(uint64_t *)(preempt->scratch_kptr + next->id * sizeof(u64));

	/*
	 * Set a keepalive bit before the first preemption register write.
	 * This is required since while each individual write to the context
	 * switch registers will wake the GPU from collapse, it will not in
	 * itself cause GPU activity. Thus, the GPU could technically be
	 * re-collapsed between subsequent register writes leading to a
	 * prolonged preemption sequence. The keepalive bit prevents any
	 * further power collapse while it is set.
	 * It is more efficient to use a keepalive+wake-on-fence approach here
	 * rather than an OOB. Both keepalive and the fence are effectively
	 * free when the GPU is already powered on, whereas an OOB requires an
	 * unconditional handshake with the GMU.
	 */
	if (gmu_core_isenabled(device))
		gmu_core_regrmw(device, A6XX_GMU_AO_SPARE_CNTL, 0x0, 0x2);

	/*
	 * Fenced writes on this path will make sure the GPU is woken up
	 * in case it was power collapsed by the GMU.
	 */
	if (adreno_gmu_fenced_write(adreno_dev,
		ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_NON_SECURE_RESTORE_ADDR_LO,
		lower_32_bits(next->preemption_desc->gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK))
		goto err;

	/*
	 * Above fence writes will make sure GMU comes out of
	 * IFPC state if its was in IFPC state but it doesn't
	 * guarantee that GMU FW actually moved to ACTIVE state
	 * i.e. wake-up from IFPC is complete.
	 * Wait for GMU to move to ACTIVE state before triggering
	 * preemption. This is require to make sure CP doesn't
	 * interrupt GMU during wake-up from IFPC.
	 */
	if (gmu_core_dev_wait_for_active_transition(device))
		goto err;

	if (adreno_gmu_fenced_write(adreno_dev,
		ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_NON_SECURE_RESTORE_ADDR_HI,
		upper_32_bits(next->preemption_desc->gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK))
		goto err;

	if (adreno_gmu_fenced_write(adreno_dev,
		ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_LO,
		lower_32_bits(next->secure_preemption_desc->gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK))
		goto err;

	if (adreno_gmu_fenced_write(adreno_dev,
		ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_HI,
		upper_32_bits(next->secure_preemption_desc->gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK))
		goto err;

	if (adreno_gmu_fenced_write(adreno_dev,
		ADRENO_REG_CP_CONTEXT_SWITCH_NON_PRIV_RESTORE_ADDR_LO,
		lower_32_bits(gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK))
		goto err;

	if (adreno_gmu_fenced_write(adreno_dev,
		ADRENO_REG_CP_CONTEXT_SWITCH_NON_PRIV_RESTORE_ADDR_HI,
		upper_32_bits(gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK))
		goto err;

	adreno_dev->next_rb = next;

	/* Start the timer to detect a stuck preemption */
	mod_timer(&adreno_dev->preempt.timer,
		jiffies + msecs_to_jiffies(ADRENO_PREEMPT_TIMEOUT));

	cntl = (preempt->preempt_level << 6) | 0x01;

	/* Skip save/restore during L1 preemption */
	if (preempt->skipsaverestore)
		cntl |= (1 << 9);

	/* Enable GMEM save/restore across preemption */
	if (preempt->usesgmem)
		cntl |= (1 << 8);

	trace_adreno_preempt_trigger(adreno_dev->cur_rb, adreno_dev->next_rb,
		cntl);

	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_TRIGGERED);

	/* Trigger the preemption */
	if (adreno_gmu_fenced_write(adreno_dev, ADRENO_REG_CP_PREEMPT, cntl,
					FENCE_STATUS_WRITEDROPPED1_MASK)) {
		adreno_dev->next_rb = NULL;
		del_timer(&adreno_dev->preempt.timer);
		goto err;
	}

	return;
err:
	/* If fenced write fails, take inline snapshot and trigger recovery */
	if (!in_interrupt()) {
		gmu_core_snapshot(device);
		adreno_set_gpu_fault(adreno_dev,
			ADRENO_GMU_FAULT_SKIP_SNAPSHOT);
	} else {
		adreno_set_gpu_fault(adreno_dev, ADRENO_GMU_FAULT);
	}
	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);
	adreno_dispatcher_schedule(device);
	/* Clear the keep alive */
	if (gmu_core_isenabled(device))
		gmu_core_regrmw(device, A6XX_GMU_AO_SPARE_CNTL, 0x2, 0x0);
}

void a6xx_preemption_callback(struct adreno_device *adreno_dev, int bit)
{
	unsigned int status;

	if (!adreno_move_preempt_state(adreno_dev,
		ADRENO_PREEMPT_TRIGGERED, ADRENO_PREEMPT_PENDING))
		return;

	adreno_readreg(adreno_dev, ADRENO_REG_CP_PREEMPT, &status);

	if (status & 0x1) {
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

	adreno_dev->preempt.count++;

	/*
	 * We can now safely clear the preemption keepalive bit, allowing
	 * power collapse to resume its regular activity.
	 */
	if (gmu_core_isenabled(KGSL_DEVICE(adreno_dev)))
		gmu_core_regrmw(KGSL_DEVICE(adreno_dev),
				A6XX_GMU_AO_SPARE_CNTL, 0x2, 0x0);

	del_timer(&adreno_dev->preempt.timer);

	_a6xx_update_active_time(adreno_dev);

	adreno_readreg(adreno_dev, ADRENO_REG_CP_PREEMPT_LEVEL_STATUS, &status);

	trace_adreno_preempt_done(adreno_dev->cur_rb, adreno_dev->next_rb,
		status);

	adreno_dev->prev_rb = adreno_dev->cur_rb;
	adreno_dev->cur_rb = adreno_dev->next_rb;
	adreno_dev->next_rb = NULL;

	/* Update the wptr if it changed while preemption was ongoing */
	_update_wptr(adreno_dev, true);

	/* Update the dispatcher timer for the new command queue */
	mod_timer(&adreno_dev->dispatcher.timer,
		adreno_dev->cur_rb->dispatch_q.expires);

	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);

	a6xx_preemption_trigger(adreno_dev);
}

void a6xx_preemption_schedule(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!adreno_is_preemption_enabled(adreno_dev))
		return;

	mutex_lock(&device->mutex);

	if (adreno_in_preempt_state(adreno_dev, ADRENO_PREEMPT_COMPLETE))
		_a6xx_preemption_done(adreno_dev);

	a6xx_preemption_trigger(adreno_dev);

	mutex_unlock(&device->mutex);
}

unsigned int a6xx_preemption_pre_ibsubmit(
		struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb,
		unsigned int *cmds, struct kgsl_context *context)
{
	unsigned int *cmds_orig = cmds;
	uint64_t gpuaddr = 0;

	if (context) {
		gpuaddr = context->user_ctxt_record->memdesc.gpuaddr;
		*cmds++ = cp_type7_packet(CP_SET_PSEUDO_REGISTER, 15);
	} else {
		*cmds++ = cp_type7_packet(CP_SET_PSEUDO_REGISTER, 12);
	}

	/* NULL SMMU_INFO buffer - we track in KMD */
	*cmds++ = SET_PSEUDO_REGISTER_SAVE_REGISTER_SMMU_INFO;
	cmds += cp_gpuaddr(adreno_dev, cmds, 0x0);

	*cmds++ = SET_PSEUDO_REGISTER_SAVE_REGISTER_PRIV_NON_SECURE_SAVE_ADDR;
	cmds += cp_gpuaddr(adreno_dev, cmds, rb->preemption_desc->gpuaddr);

	*cmds++ = SET_PSEUDO_REGISTER_SAVE_REGISTER_PRIV_SECURE_SAVE_ADDR;
	cmds += cp_gpuaddr(adreno_dev, cmds,
			rb->secure_preemption_desc->gpuaddr);

	if (context) {

		*cmds++ = SET_PSEUDO_REGISTER_SAVE_REGISTER_NON_PRIV_SAVE_ADDR;
		cmds += cp_gpuaddr(adreno_dev, cmds, gpuaddr);
	}

	/*
	 * There is no need to specify this address when we are about to
	 * trigger preemption. This is because CP internally stores this
	 * address specified here in the CP_SET_PSEUDO_REGISTER payload to
	 * the context record and thus knows from where to restore
	 * the saved perfcounters for the new ringbuffer.
	 */
	*cmds++ = SET_PSEUDO_REGISTER_SAVE_REGISTER_COUNTER;
	cmds += cp_gpuaddr(adreno_dev, cmds,
			rb->perfcounter_save_restore_desc->gpuaddr);

	if (context) {
		struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
		struct adreno_ringbuffer *rb = drawctxt->rb;
		uint64_t dest = adreno_dev->preempt.scratch->gpuaddr +
			sizeof(u64) * rb->id;

		*cmds++ = cp_mem_packet(adreno_dev, CP_MEM_WRITE, 2, 2);
		cmds += cp_gpuaddr(adreno_dev, cmds, dest);
		*cmds++ = lower_32_bits(gpuaddr);
		*cmds++ = upper_32_bits(gpuaddr);
	}

	return (unsigned int) (cmds - cmds_orig);
}

unsigned int a6xx_preemption_post_ibsubmit(struct adreno_device *adreno_dev,
	unsigned int *cmds)
{
	unsigned int *cmds_orig = cmds;
	struct adreno_ringbuffer *rb = adreno_dev->cur_rb;

	if (rb) {
		uint64_t dest = adreno_dev->preempt.scratch->gpuaddr +
			sizeof(u64) * rb->id;

		*cmds++ = cp_mem_packet(adreno_dev, CP_MEM_WRITE, 2, 2);
		cmds += cp_gpuaddr(adreno_dev, cmds, dest);
		*cmds++ = 0;
		*cmds++ = 0;
	}

	*cmds++ = cp_type7_packet(CP_CONTEXT_SWITCH_YIELD, 4);
	cmds += cp_gpuaddr(adreno_dev, cmds, 0x0);
	*cmds++ = 1;
	*cmds++ = 0;

	return (unsigned int) (cmds - cmds_orig);
}

void a6xx_preemption_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);
	struct a6xx_cp_smmu_info *smmu_info =
			(struct a6xx_cp_smmu_info *)iommu->kptr;
	struct a6xx_cp_preemption_record *preempt_record;
	struct adreno_ringbuffer *rb;
	unsigned int i;

	if (!adreno_is_preemption_enabled(adreno_dev))
		return;

	/* Force the state to be clear */
	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);

	/* smmu_info is allocated and mapped in a6xx_preemption_iommu_init */
	smmu_info->magic = A6XX_CP_SMMU_INFO_MAGIC_REF;
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
		preempt_record = (struct a6xx_cp_preemption_record *)
				rb->preemption_kptr;
		preempt_record->rptr = 0;
		preempt_record->wptr = 0;
		/* Make sure the writes are posted before continuing */
		smp_wmb();

		adreno_ringbuffer_set_pagetable(rb,
			device->mmu.defaultpagetable);
	}
}

static int a6xx_preemption_ringbuffer_init(struct adreno_device *adreno_dev,
	struct adreno_ringbuffer *rb)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_cp_preemption_record *preempt_record;
	unsigned int page_count = 0;

	/*
	 * Reserve CP context record size as
	 * GMEM size + GPU HW state size i.e 0x110000
	 */
	if (IS_ERR_OR_NULL(rb->preemption_desc)) {
		rb->preemption_desc = kgsl_allocate_global(device,
			adreno_dev->gpucore->gmem_size + 0x110000, 0,
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

	/*
	 * Reserve CP context record size as
	 * GMEM size + GPU HW state size i.e 0x110000
	 */
	if (IS_ERR_OR_NULL(rb->secure_preemption_desc))
		rb->secure_preemption_desc = kgsl_allocate_global(device,
			adreno_dev->gpucore->gmem_size + 0x110000,
			KGSL_MEMFLAGS_SECURE, KGSL_MEMDESC_PRIVILEGED,
			"secure_preemption_desc");

	if (IS_ERR(rb->secure_preemption_desc))
		return PTR_ERR(rb->secure_preemption_desc);

	if (IS_ERR_OR_NULL(rb->perfcounter_save_restore_desc))
		rb->perfcounter_save_restore_desc = kgsl_allocate_global(device,
			A6XX_CP_PERFCOUNTER_SAVE_RESTORE_SIZE, 0,
			KGSL_MEMDESC_PRIVILEGED,
			"perfcounter_save_restore_desc");
	if (IS_ERR(rb->perfcounter_save_restore_desc))
		return PTR_ERR(rb->perfcounter_save_restore_desc);

	/* Reset the state of the preemption record. */
	if (!IS_ERR_OR_NULL(rb->preemption_kptr)) {
		preempt_record = (struct a6xx_cp_preemption_record *)
				rb->preemption_kptr;
		preempt_record->magic = A6XX_CP_CTXRECORD_MAGIC_REF;
		preempt_record->info = 0;
		preempt_record->data = 0;
		preempt_record->cntl = gpudev->cp_rb_cntl;
		preempt_record->rptr = 0;
		preempt_record->wptr = 0;
		preempt_record->rptr_addr = SCRATCH_RPTR_GPU_ADDR(device, rb->id);
		preempt_record->rbase = rb->buffer_desc->gpuaddr;
		preempt_record->counter = 0;
		/* Make sure the writes are posted before continuing */
		smp_wmb();
	}

	return 0;
}

int a6xx_preemption_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);
	struct adreno_preemption *preempt = &adreno_dev->preempt;
	struct adreno_ringbuffer *rb;
	int ret;
	unsigned int i;
	unsigned int page_count = 0;

	/* We are dependent on IOMMU to make preemption go on the CP side */
	if (kgsl_mmu_get_mmutype(device) != KGSL_MMU_TYPE_IOMMU)
		return -ENODEV;

	INIT_WORK(&preempt->work, _a6xx_preemption_worker);

	if (IS_ERR_OR_NULL(preempt->scratch)) {
		preempt->scratch = kgsl_allocate_global(device, PAGE_SIZE, 0, 0,
				"preemption_scratch");
		if (IS_ERR_OR_NULL(preempt->scratch))
			return (!preempt->scratch) ? -EINVAL :
					PTR_ERR(preempt->scratch);

		preempt->scratch_kptr = kgsl_sharedmem_vm_map_readonly(
				preempt->scratch, 0, PAGE_SIZE, &page_count);

		if (IS_ERR_OR_NULL(preempt->scratch_kptr))
			return (!preempt->scratch_kptr) ? -EINVAL :
					PTR_ERR(preempt->scratch_kptr);
		else if (WARN(page_count != 1,
			 "preemption scratch mapping size does not match buffer size\n"))
			return -EINVAL;
	}

	/* Allocate mem for storing preemption switch record */
	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		ret = a6xx_preemption_ringbuffer_init(adreno_dev, rb);
		if (ret)
			return ret;
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

void a6xx_preemption_context_destroy(struct kgsl_context *context)
{
	struct kgsl_device *device = context->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION) ||
			context->user_ctxt_record == NULL)
		return;

	gpumem_free_entry(context->user_ctxt_record);

	/* Put the extra ref from gpumem_alloc_entry() */
	kgsl_mem_entry_put_deferred(context->user_ctxt_record);

	/*
	 * kgsl_mem_entry_put_deferred holds its own reference to the
	 * memory entry that is has deferred freeing, so it's safe to
	 * NULL the pointer here to mark it as destroyed.
	 */
	context->user_ctxt_record = NULL;
}

int a6xx_preemption_context_init(struct kgsl_context *context)
{
	struct kgsl_device *device = context->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	uint64_t flags = 0;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION) ||
			context->user_ctxt_record != NULL)
		return 0;

	if (context->flags & KGSL_CONTEXT_SECURE)
		flags |= KGSL_MEMFLAGS_SECURE;

	if (context->flags & KGSL_CONTEXT_COMPAT_TASK)
		flags |= KGSL_MEMFLAGS_FORCE_32BIT;

	/*
	 * gpumem_alloc_entry takes an extra refcount. Put it only when
	 * destroying the context to keep the context record valid
	 */
	context->user_ctxt_record = gpumem_alloc_entry(context->dev_priv,
			A6XX_CP_CTXRECORD_USER_RESTORE_SIZE, flags,
			KGSL_MEMDESC_LAZY_ALLOCATION);
	if (IS_ERR(context->user_ctxt_record)) {
		int ret = PTR_ERR(context->user_ctxt_record);

		context->user_ctxt_record = NULL;
		return ret;
	}

	return 0;
}
