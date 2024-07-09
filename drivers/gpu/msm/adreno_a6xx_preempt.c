// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "adreno.h"
#include "adreno_a6xx.h"
#include "adreno_pm4types.h"
#include "adreno_trace.h"

#define PREEMPT_RECORD(_field) \
		offsetof(struct a6xx_cp_preemption_record, _field)

#define PREEMPT_SMMU_RECORD(_field) \
		offsetof(struct a6xx_cp_smmu_info, _field)

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
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	spin_lock_irqsave(&rb->preempt_lock, flags);

	if (in_interrupt() == 0) {
		/*
		 * We might have skipped updating the wptr in case we are in
		 * dispatcher context. Do it now.
		 */
		if (rb->skip_inline_wptr) {
			/*
			 * There could be a situation where GPU comes out of
			 * ifpc after a fenced write transaction but before
			 * reading AHB_FENCE_STATUS from KMD, it goes back to
			 * ifpc due to inactivity (kernel scheduler plays a
			 * role here). Thus, the GPU could technically be
			 * re-collapsed between subsequent register writes
			 * leading to a prolonged preemption sequence. The
			 * keepalive bit prevents any further power collapse
			 * while it is set.
			 */
			if (gmu_core_isenabled(device))
				gmu_core_regrmw(device, A6XX_GMU_AO_SPARE_CNTL,
					0x0, 0x2);

			ret = adreno_gmu_fenced_write(adreno_dev,
				ADRENO_REG_CP_RB_WPTR, rb->wptr,
				FENCE_STATUS_WRITEDROPPED0_MASK);

			/* Clear the keep alive */
			if (gmu_core_isenabled(device))
				gmu_core_regrmw(device, A6XX_GMU_AO_SPARE_CNTL,
					0x2, 0x0);

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

static inline bool adreno_move_preempt_state(struct adreno_device *adreno_dev,
	enum adreno_preempt_states old, enum adreno_preempt_states new)
{
	return (atomic_cmpxchg(&adreno_dev->preempt.state, old, new) == old);
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

static void _a6xx_preemption_timer(struct timer_list *t)
{
	struct adreno_preemption *preempt = from_timer(preempt, t, timer);
	struct adreno_device *adreno_dev = container_of(preempt,
						struct adreno_device, preempt);

	/* We should only be here from a triggered state */
	if (!adreno_move_preempt_state(adreno_dev,
		ADRENO_PREEMPT_TRIGGERED, ADRENO_PREEMPT_FAULTED))
		return;

	/* Schedule the worker to take care of the details */
	queue_work(system_unbound_wq, &adreno_dev->preempt.work);
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

	cntl = (((preempt->preempt_level << 6) & 0xC0) |
		((preempt->skipsaverestore << 9) & 0x200) |
		((preempt->usesgmem << 8) & 0x100) | 0x1);

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
	kgsl_sharedmem_readq(&next->pagetable_desc, &ttbr0,
		PT_INFO_OFFSET(ttbr0));
	kgsl_sharedmem_readl(&next->pagetable_desc, &contextidr,
		PT_INFO_OFFSET(contextidr));

	kgsl_sharedmem_writel(device, &next->preemption_desc,
		PREEMPT_RECORD(wptr), next->wptr);

	spin_unlock_irqrestore(&next->preempt_lock, flags);

	/* And write it to the smmu info */
	kgsl_sharedmem_writeq(device, &iommu->smmu_info,
		PREEMPT_SMMU_RECORD(ttbr0), ttbr0);
	kgsl_sharedmem_writel(device, &iommu->smmu_info,
		PREEMPT_SMMU_RECORD(context_idr), contextidr);

	kgsl_sharedmem_readq(&preempt->scratch, &gpuaddr,
			next->id * sizeof(u64));

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
		lower_32_bits(next->preemption_desc.gpuaddr),
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
		upper_32_bits(next->preemption_desc.gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK))
		goto err;

	if (adreno_gmu_fenced_write(adreno_dev,
		ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_LO,
		lower_32_bits(next->secure_preemption_desc.gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK))
		goto err;

	if (adreno_gmu_fenced_write(adreno_dev,
		ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_HI,
		upper_32_bits(next->secure_preemption_desc.gpuaddr),
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
	cmds += cp_gpuaddr(adreno_dev, cmds, rb->preemption_desc.gpuaddr);

	*cmds++ = SET_PSEUDO_REGISTER_SAVE_REGISTER_PRIV_SECURE_SAVE_ADDR;
	cmds += cp_gpuaddr(adreno_dev, cmds,
			rb->secure_preemption_desc.gpuaddr);

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
			rb->perfcounter_save_restore_desc.gpuaddr);

	if (context) {
		struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
		struct adreno_ringbuffer *rb = drawctxt->rb;
		uint64_t dest = PREEMPT_SCRATCH_ADDR(adreno_dev, rb->id);

		*cmds++ = cp_mem_packet(adreno_dev, CP_MEM_WRITE, 2, 2);
		cmds += cp_gpuaddr(adreno_dev, cmds, dest);
		*cmds++ = lower_32_bits(gpuaddr);
		*cmds++ = upper_32_bits(gpuaddr);

		/*
		 * Add a KMD post amble to clear the perf counters during
		 * preemption
		 */
		if (!adreno_dev->perfcounter) {
			u64 kmd_postamble_addr = SCRATCH_POSTAMBLE_ADDR
						(KGSL_DEVICE(adreno_dev));

			*cmds++ = cp_type7_packet(CP_SET_AMBLE, 3);
			*cmds++ = lower_32_bits(kmd_postamble_addr);
			*cmds++ = upper_32_bits(kmd_postamble_addr);
			*cmds++ = ((CP_KMD_AMBLE_TYPE << 20) | GENMASK(22, 20))
			| (adreno_dev->preempt.postamble_len | GENMASK(19, 0));
		}
	}

	return (unsigned int) (cmds - cmds_orig);
}

unsigned int a6xx_preemption_post_ibsubmit(struct adreno_device *adreno_dev,
	unsigned int *cmds)
{
	unsigned int *cmds_orig = cmds;
	struct adreno_ringbuffer *rb = adreno_dev->cur_rb;

	if (rb) {
		uint64_t dest = PREEMPT_SCRATCH_ADDR(adreno_dev, rb->id);

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
	struct adreno_ringbuffer *rb;
	unsigned int i;

	if (!adreno_is_preemption_enabled(adreno_dev))
		return;

	/* Force the state to be clear */
	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);

	/* smmu_info is allocated and mapped in a6xx_preemption_iommu_init */
	kgsl_sharedmem_writel(device, &iommu->smmu_info,
		PREEMPT_SMMU_RECORD(magic), A6XX_CP_SMMU_INFO_MAGIC_REF);
	kgsl_sharedmem_writeq(device, &iommu->smmu_info,
		PREEMPT_SMMU_RECORD(ttbr0), MMU_DEFAULT_TTBR0(device));

	/* The CP doesn't use the asid record, so poison it */
	kgsl_sharedmem_writel(device, &iommu->smmu_info,
		PREEMPT_SMMU_RECORD(asid), 0xDECAFBAD);
	kgsl_sharedmem_writel(device, &iommu->smmu_info,
		PREEMPT_SMMU_RECORD(context_idr),
		MMU_DEFAULT_CONTEXTIDR(device));

	adreno_writereg64(adreno_dev,
		ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_LO,
		ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_HI,
		iommu->smmu_info.gpuaddr);

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		/*
		 * preemption_desc is allocated and mapped at init time,
		 * so no need to check sharedmem_writel return value
		 */
		kgsl_sharedmem_writel(device, &rb->preemption_desc,
			PREEMPT_RECORD(rptr), 0);
		kgsl_sharedmem_writel(device, &rb->preemption_desc,
			PREEMPT_RECORD(wptr), 0);

		adreno_ringbuffer_set_pagetable(rb,
			device->mmu.defaultpagetable);
	}
}

static int a6xx_preemption_ringbuffer_init(struct adreno_device *adreno_dev,
	struct adreno_ringbuffer *rb)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	/*
	 * Reserve CP context record size as
	 * GMEM size + GPU HW state size i.e 0x110000
	 */
	ret = kgsl_allocate_global(device, &rb->preemption_desc,
		adreno_dev->gpucore->gmem_size + 0x110000,
		0, KGSL_MEMDESC_PRIVILEGED,
		"preemption_desc");
	if (ret)
		return ret;

	/*
	 * Reserve CP context record size as
	 * GMEM size + GPU HW state size i.e 0x110000
	 */
	ret = kgsl_allocate_user(device, &rb->secure_preemption_desc,
		adreno_dev->gpucore->gmem_size + 0x110000,
		KGSL_MEMFLAGS_SECURE | KGSL_MEMDESC_PRIVILEGED);
	if (ret)
		return ret;

	ret = kgsl_iommu_map_global_secure_pt_entry(device,
				&rb->secure_preemption_desc);
	if (ret)
		return ret;

	ret = kgsl_allocate_global(device, &rb->perfcounter_save_restore_desc,
		A6XX_CP_PERFCOUNTER_SAVE_RESTORE_SIZE, 0,
		KGSL_MEMDESC_PRIVILEGED, "perfcounter_save_restore_desc");
	if (ret)
		return ret;

	kgsl_sharedmem_writel(device, &rb->preemption_desc,
		PREEMPT_RECORD(magic), A6XX_CP_CTXRECORD_MAGIC_REF);
	kgsl_sharedmem_writel(device, &rb->preemption_desc,
		PREEMPT_RECORD(info), 0);
	kgsl_sharedmem_writel(device, &rb->preemption_desc,
		PREEMPT_RECORD(data), 0);
	kgsl_sharedmem_writel(device, &rb->preemption_desc,
		PREEMPT_RECORD(cntl), gpudev->cp_rb_cntl);
	kgsl_sharedmem_writel(device, &rb->preemption_desc,
		PREEMPT_RECORD(rptr), 0);
	kgsl_sharedmem_writel(device, &rb->preemption_desc,
		PREEMPT_RECORD(wptr), 0);
	kgsl_sharedmem_writeq(device, &rb->preemption_desc,
		PREEMPT_RECORD(rptr_addr), SCRATCH_RPTR_GPU_ADDR(device,
		rb->id));
	kgsl_sharedmem_writeq(device, &rb->preemption_desc,
		PREEMPT_RECORD(rbase), rb->buffer_desc.gpuaddr);
	kgsl_sharedmem_writeq(device, &rb->preemption_desc,
		PREEMPT_RECORD(counter), 0);

	return 0;
}

#ifdef CONFIG_QCOM_KGSL_IOMMU
static int a6xx_preemption_iommu_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);

	/* Allocate mem for storing preemption smmu record */
	return kgsl_allocate_global(device, &iommu->smmu_info, PAGE_SIZE,
		KGSL_MEMFLAGS_GPUREADONLY, KGSL_MEMDESC_PRIVILEGED,
		"smmu_info");
}

static void a6xx_preemption_iommu_close(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);

	kgsl_free_global(device, &iommu->smmu_info);
}
#else
static int a6xx_preemption_iommu_init(struct adreno_device *adreno_dev)
{
	return -ENODEV;
}

static void a6xx_preemption_iommu_close(struct adreno_device *adreno_dev)
{
}
#endif

static void _preemption_close(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_preemption *preempt = &adreno_dev->preempt;
	struct adreno_ringbuffer *rb;
	unsigned int i;

	del_timer(&preempt->timer);
	kgsl_free_global(device, &preempt->scratch);
	a6xx_preemption_iommu_close(adreno_dev);

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		kgsl_free_global(device, &rb->preemption_desc);
		kgsl_free_global(device, &rb->perfcounter_save_restore_desc);
		kgsl_iommu_unmap_global_secure_pt_entry(device,
				&rb->secure_preemption_desc);
		kgsl_sharedmem_free(&rb->secure_preemption_desc);
	}
}

void a6xx_preemption_close(struct adreno_device *adreno_dev)
{
	if (!test_bit(ADRENO_DEVICE_PREEMPTION, &adreno_dev->priv))
		return;

	_preemption_close(adreno_dev);
}

int a6xx_preemption_init(struct adreno_device *adreno_dev)
{
	u32 flags = ADRENO_FEATURE(adreno_dev, ADRENO_APRIV) ?
			KGSL_MEMDESC_PRIVILEGED : 0;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_preemption *preempt = &adreno_dev->preempt;
	struct adreno_ringbuffer *rb;
	int ret;
	unsigned int i;

	/* We are dependent on IOMMU to make preemption go on the CP side */
	if (kgsl_mmu_get_mmutype(device) != KGSL_MMU_TYPE_IOMMU)
		return -ENODEV;

	INIT_WORK(&preempt->work, _a6xx_preemption_worker);

	timer_setup(&preempt->timer, _a6xx_preemption_timer, 0);

	ret = kgsl_allocate_global(device, &preempt->scratch, PAGE_SIZE, 0,
			flags, "preemption_scratch");

	/* Allocate mem for storing preemption switch record */
	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		ret = a6xx_preemption_ringbuffer_init(adreno_dev, rb);
		if (ret)
			goto err;
	}

	/*
	 * First 28 dwords of the device scratch buffer are used to store
	 * shadow rb data. Reserve 11 dwords in the device scratch buffer
	 * from SCRATCH_POSTAMBLE_OFFSET for KMD postamble pm4 packets.
	 * This should be in *device->scratch* so that userspace cannot
	 * access it.
	 */
	if (!adreno_dev->perfcounter) {
		u32 *postamble = device->scratch.hostptr +
				SCRATCH_POSTAMBLE_OFFSET;
		u32 count = 0;

		postamble[count++] = cp_type7_packet(CP_REG_RMW, 3);
		postamble[count++] = A6XX_RBBM_PERFCTR_SRAM_INIT_CMD;
		postamble[count++] = 0x0;
		postamble[count++] = 0x1;

		postamble[count++] = cp_type7_packet(CP_WAIT_REG_MEM, 6);
		postamble[count++] = 0x3;
		postamble[count++] = A6XX_RBBM_PERFCTR_SRAM_INIT_STATUS;
		postamble[count++] = 0x0;
		postamble[count++] = 0x1;
		postamble[count++] = 0x1;
		postamble[count++] = 0x0;

		preempt->postamble_len = count;
	}

	ret = a6xx_preemption_iommu_init(adreno_dev);

err:
	if (ret)
		_preemption_close(adreno_dev);

	return ret;
}

void a6xx_preemption_context_destroy(struct kgsl_context *context)
{
	struct kgsl_device *device = context->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION))
		return;

	gpumem_free_entry(context->user_ctxt_record);

	/* Put the extra ref from gpumem_alloc_entry() */
	kgsl_mem_entry_put_deferred(context->user_ctxt_record);
}

int a6xx_preemption_context_init(struct kgsl_context *context)
{
	struct kgsl_device *device = context->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	uint64_t flags = 0;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION))
		return 0;

	if (context->flags & KGSL_CONTEXT_SECURE)
		flags |= KGSL_MEMFLAGS_SECURE;

	if (kgsl_is_compat_task())
		flags |= KGSL_MEMFLAGS_FORCE_32BIT;

	/*
	 * gpumem_alloc_entry takes an extra refcount. Put it only when
	 * destroying the context to keep the context record valid
	 */
	context->user_ctxt_record = gpumem_alloc_entry(context->dev_priv,
			A6XX_CP_CTXRECORD_USER_RESTORE_SIZE, flags);
	if (IS_ERR(context->user_ctxt_record)) {
		int ret = PTR_ERR(context->user_ctxt_record);

		context->user_ctxt_record = NULL;
		return ret;
	}

	return 0;
}
