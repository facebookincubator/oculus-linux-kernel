/*
 * Copyright © 2016 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include <linux/kthread.h>

#include "../i915_selftest.h"
#include "i915_random.h"
#include "igt_flush_test.h"
#include "igt_wedge_me.h"

#include "mock_context.h"
#include "mock_drm.h"

#define IGT_IDLE_TIMEOUT 50 /* ms; time to wait after flushing between tests */

struct hang {
	struct drm_i915_private *i915;
	struct drm_i915_gem_object *hws;
	struct drm_i915_gem_object *obj;
	struct i915_gem_context *ctx;
	u32 *seqno;
	u32 *batch;
};

static int hang_init(struct hang *h, struct drm_i915_private *i915)
{
	void *vaddr;
	int err;

	memset(h, 0, sizeof(*h));
	h->i915 = i915;

	h->ctx = kernel_context(i915);
	if (IS_ERR(h->ctx))
		return PTR_ERR(h->ctx);

	h->hws = i915_gem_object_create_internal(i915, PAGE_SIZE);
	if (IS_ERR(h->hws)) {
		err = PTR_ERR(h->hws);
		goto err_ctx;
	}

	h->obj = i915_gem_object_create_internal(i915, PAGE_SIZE);
	if (IS_ERR(h->obj)) {
		err = PTR_ERR(h->obj);
		goto err_hws;
	}

	i915_gem_object_set_cache_level(h->hws, I915_CACHE_LLC);
	vaddr = i915_gem_object_pin_map(h->hws, I915_MAP_WB);
	if (IS_ERR(vaddr)) {
		err = PTR_ERR(vaddr);
		goto err_obj;
	}
	h->seqno = memset(vaddr, 0xff, PAGE_SIZE);

	vaddr = i915_gem_object_pin_map(h->obj,
					HAS_LLC(i915) ? I915_MAP_WB : I915_MAP_WC);
	if (IS_ERR(vaddr)) {
		err = PTR_ERR(vaddr);
		goto err_unpin_hws;
	}
	h->batch = vaddr;

	return 0;

err_unpin_hws:
	i915_gem_object_unpin_map(h->hws);
err_obj:
	i915_gem_object_put(h->obj);
err_hws:
	i915_gem_object_put(h->hws);
err_ctx:
	kernel_context_close(h->ctx);
	return err;
}

static u64 hws_address(const struct i915_vma *hws,
		       const struct i915_request *rq)
{
	return hws->node.start + offset_in_page(sizeof(u32)*rq->fence.context);
}

static int emit_recurse_batch(struct hang *h,
			      struct i915_request *rq)
{
	struct drm_i915_private *i915 = h->i915;
	struct i915_address_space *vm =
		rq->gem_context->ppgtt ?
		&rq->gem_context->ppgtt->vm :
		&i915->ggtt.vm;
	struct i915_vma *hws, *vma;
	unsigned int flags;
	u32 *batch;
	int err;

	vma = i915_vma_instance(h->obj, vm, NULL);
	if (IS_ERR(vma))
		return PTR_ERR(vma);

	hws = i915_vma_instance(h->hws, vm, NULL);
	if (IS_ERR(hws))
		return PTR_ERR(hws);

	err = i915_vma_pin(vma, 0, 0, PIN_USER);
	if (err)
		return err;

	err = i915_vma_pin(hws, 0, 0, PIN_USER);
	if (err)
		goto unpin_vma;

	err = i915_vma_move_to_active(vma, rq, 0);
	if (err)
		goto unpin_hws;

	if (!i915_gem_object_has_active_reference(vma->obj)) {
		i915_gem_object_get(vma->obj);
		i915_gem_object_set_active_reference(vma->obj);
	}

	err = i915_vma_move_to_active(hws, rq, 0);
	if (err)
		goto unpin_hws;

	if (!i915_gem_object_has_active_reference(hws->obj)) {
		i915_gem_object_get(hws->obj);
		i915_gem_object_set_active_reference(hws->obj);
	}

	batch = h->batch;
	if (INTEL_GEN(i915) >= 8) {
		*batch++ = MI_STORE_DWORD_IMM_GEN4;
		*batch++ = lower_32_bits(hws_address(hws, rq));
		*batch++ = upper_32_bits(hws_address(hws, rq));
		*batch++ = rq->fence.seqno;
		*batch++ = MI_ARB_CHECK;

		memset(batch, 0, 1024);
		batch += 1024 / sizeof(*batch);

		*batch++ = MI_ARB_CHECK;
		*batch++ = MI_BATCH_BUFFER_START | 1 << 8 | 1;
		*batch++ = lower_32_bits(vma->node.start);
		*batch++ = upper_32_bits(vma->node.start);
	} else if (INTEL_GEN(i915) >= 6) {
		*batch++ = MI_STORE_DWORD_IMM_GEN4;
		*batch++ = 0;
		*batch++ = lower_32_bits(hws_address(hws, rq));
		*batch++ = rq->fence.seqno;
		*batch++ = MI_ARB_CHECK;

		memset(batch, 0, 1024);
		batch += 1024 / sizeof(*batch);

		*batch++ = MI_ARB_CHECK;
		*batch++ = MI_BATCH_BUFFER_START | 1 << 8;
		*batch++ = lower_32_bits(vma->node.start);
	} else if (INTEL_GEN(i915) >= 4) {
		*batch++ = MI_STORE_DWORD_IMM_GEN4 | MI_USE_GGTT;
		*batch++ = 0;
		*batch++ = lower_32_bits(hws_address(hws, rq));
		*batch++ = rq->fence.seqno;
		*batch++ = MI_ARB_CHECK;

		memset(batch, 0, 1024);
		batch += 1024 / sizeof(*batch);

		*batch++ = MI_ARB_CHECK;
		*batch++ = MI_BATCH_BUFFER_START | 2 << 6;
		*batch++ = lower_32_bits(vma->node.start);
	} else {
		*batch++ = MI_STORE_DWORD_IMM | MI_MEM_VIRTUAL;
		*batch++ = lower_32_bits(hws_address(hws, rq));
		*batch++ = rq->fence.seqno;
		*batch++ = MI_ARB_CHECK;

		memset(batch, 0, 1024);
		batch += 1024 / sizeof(*batch);

		*batch++ = MI_ARB_CHECK;
		*batch++ = MI_BATCH_BUFFER_START | 2 << 6;
		*batch++ = lower_32_bits(vma->node.start);
	}
	*batch++ = MI_BATCH_BUFFER_END; /* not reached */
	i915_gem_chipset_flush(h->i915);

	flags = 0;
	if (INTEL_GEN(vm->i915) <= 5)
		flags |= I915_DISPATCH_SECURE;

	err = rq->engine->emit_bb_start(rq, vma->node.start, PAGE_SIZE, flags);

unpin_hws:
	i915_vma_unpin(hws);
unpin_vma:
	i915_vma_unpin(vma);
	return err;
}

static struct i915_request *
hang_create_request(struct hang *h, struct intel_engine_cs *engine)
{
	struct i915_request *rq;
	int err;

	if (i915_gem_object_is_active(h->obj)) {
		struct drm_i915_gem_object *obj;
		void *vaddr;

		obj = i915_gem_object_create_internal(h->i915, PAGE_SIZE);
		if (IS_ERR(obj))
			return ERR_CAST(obj);

		vaddr = i915_gem_object_pin_map(obj,
						HAS_LLC(h->i915) ? I915_MAP_WB : I915_MAP_WC);
		if (IS_ERR(vaddr)) {
			i915_gem_object_put(obj);
			return ERR_CAST(vaddr);
		}

		i915_gem_object_unpin_map(h->obj);
		i915_gem_object_put(h->obj);

		h->obj = obj;
		h->batch = vaddr;
	}

	rq = i915_request_alloc(engine, h->ctx);
	if (IS_ERR(rq))
		return rq;

	err = emit_recurse_batch(h, rq);
	if (err) {
		i915_request_add(rq);
		return ERR_PTR(err);
	}

	return rq;
}

static u32 hws_seqno(const struct hang *h, const struct i915_request *rq)
{
	return READ_ONCE(h->seqno[rq->fence.context % (PAGE_SIZE/sizeof(u32))]);
}

static void hang_fini(struct hang *h)
{
	*h->batch = MI_BATCH_BUFFER_END;
	i915_gem_chipset_flush(h->i915);

	i915_gem_object_unpin_map(h->obj);
	i915_gem_object_put(h->obj);

	i915_gem_object_unpin_map(h->hws);
	i915_gem_object_put(h->hws);

	kernel_context_close(h->ctx);

	igt_flush_test(h->i915, I915_WAIT_LOCKED);
}

static bool wait_until_running(struct hang *h, struct i915_request *rq)
{
	return !(wait_for_us(i915_seqno_passed(hws_seqno(h, rq),
					       rq->fence.seqno),
			     10) &&
		 wait_for(i915_seqno_passed(hws_seqno(h, rq),
					    rq->fence.seqno),
			  1000));
}

static int igt_hang_sanitycheck(void *arg)
{
	struct drm_i915_private *i915 = arg;
	struct i915_request *rq;
	struct intel_engine_cs *engine;
	enum intel_engine_id id;
	struct hang h;
	int err;

	/* Basic check that we can execute our hanging batch */

	mutex_lock(&i915->drm.struct_mutex);
	err = hang_init(&h, i915);
	if (err)
		goto unlock;

	for_each_engine(engine, i915, id) {
		long timeout;

		if (!intel_engine_can_store_dword(engine))
			continue;

		rq = hang_create_request(&h, engine);
		if (IS_ERR(rq)) {
			err = PTR_ERR(rq);
			pr_err("Failed to create request for %s, err=%d\n",
			       engine->name, err);
			goto fini;
		}

		i915_request_get(rq);

		*h.batch = MI_BATCH_BUFFER_END;
		i915_gem_chipset_flush(i915);

		i915_request_add(rq);

		timeout = i915_request_wait(rq,
					    I915_WAIT_LOCKED,
					    MAX_SCHEDULE_TIMEOUT);
		i915_request_put(rq);

		if (timeout < 0) {
			err = timeout;
			pr_err("Wait for request failed on %s, err=%d\n",
			       engine->name, err);
			goto fini;
		}
	}

fini:
	hang_fini(&h);
unlock:
	mutex_unlock(&i915->drm.struct_mutex);
	return err;
}

static void global_reset_lock(struct drm_i915_private *i915)
{
	struct intel_engine_cs *engine;
	enum intel_engine_id id;

	pr_debug("%s: current gpu_error=%08lx\n",
		 __func__, i915->gpu_error.flags);

	while (test_and_set_bit(I915_RESET_BACKOFF, &i915->gpu_error.flags))
		wait_event(i915->gpu_error.reset_queue,
			   !test_bit(I915_RESET_BACKOFF,
				     &i915->gpu_error.flags));

	for_each_engine(engine, i915, id) {
		while (test_and_set_bit(I915_RESET_ENGINE + id,
					&i915->gpu_error.flags))
			wait_on_bit(&i915->gpu_error.flags,
				    I915_RESET_ENGINE + id,
				    TASK_UNINTERRUPTIBLE);
	}
}

static void global_reset_unlock(struct drm_i915_private *i915)
{
	struct intel_engine_cs *engine;
	enum intel_engine_id id;

	for_each_engine(engine, i915, id)
		clear_bit(I915_RESET_ENGINE + id, &i915->gpu_error.flags);

	clear_bit(I915_RESET_BACKOFF, &i915->gpu_error.flags);
	wake_up_all(&i915->gpu_error.reset_queue);
}

static int igt_global_reset(void *arg)
{
	struct drm_i915_private *i915 = arg;
	unsigned int reset_count;
	int err = 0;

	/* Check that we can issue a global GPU reset */

	global_reset_lock(i915);
	set_bit(I915_RESET_HANDOFF, &i915->gpu_error.flags);

	mutex_lock(&i915->drm.struct_mutex);
	reset_count = i915_reset_count(&i915->gpu_error);

	i915_reset(i915, ALL_ENGINES, NULL);

	if (i915_reset_count(&i915->gpu_error) == reset_count) {
		pr_err("No GPU reset recorded!\n");
		err = -EINVAL;
	}
	mutex_unlock(&i915->drm.struct_mutex);

	GEM_BUG_ON(test_bit(I915_RESET_HANDOFF, &i915->gpu_error.flags));
	global_reset_unlock(i915);

	if (i915_terminally_wedged(&i915->gpu_error))
		err = -EIO;

	return err;
}

static bool wait_for_idle(struct intel_engine_cs *engine)
{
	return wait_for(intel_engine_is_idle(engine), IGT_IDLE_TIMEOUT) == 0;
}

static int __igt_reset_engine(struct drm_i915_private *i915, bool active)
{
	struct intel_engine_cs *engine;
	enum intel_engine_id id;
	struct hang h;
	int err = 0;

	/* Check that we can issue an engine reset on an idle engine (no-op) */

	if (!intel_has_reset_engine(i915))
		return 0;

	if (active) {
		mutex_lock(&i915->drm.struct_mutex);
		err = hang_init(&h, i915);
		mutex_unlock(&i915->drm.struct_mutex);
		if (err)
			return err;
	}

	for_each_engine(engine, i915, id) {
		unsigned int reset_count, reset_engine_count;
		IGT_TIMEOUT(end_time);

		if (active && !intel_engine_can_store_dword(engine))
			continue;

		if (!wait_for_idle(engine)) {
			pr_err("%s failed to idle before reset\n",
			       engine->name);
			err = -EIO;
			break;
		}

		reset_count = i915_reset_count(&i915->gpu_error);
		reset_engine_count = i915_reset_engine_count(&i915->gpu_error,
							     engine);

		set_bit(I915_RESET_ENGINE + id, &i915->gpu_error.flags);
		do {
			u32 seqno = intel_engine_get_seqno(engine);

			if (active) {
				struct i915_request *rq;

				mutex_lock(&i915->drm.struct_mutex);
				rq = hang_create_request(&h, engine);
				if (IS_ERR(rq)) {
					err = PTR_ERR(rq);
					mutex_unlock(&i915->drm.struct_mutex);
					break;
				}

				i915_request_get(rq);
				i915_request_add(rq);
				mutex_unlock(&i915->drm.struct_mutex);

				if (!wait_until_running(&h, rq)) {
					struct drm_printer p = drm_info_printer(i915->drm.dev);

					pr_err("%s: Failed to start request %x, at %x\n",
					       __func__, rq->fence.seqno, hws_seqno(&h, rq));
					intel_engine_dump(engine, &p,
							  "%s\n", engine->name);

					i915_request_put(rq);
					err = -EIO;
					break;
				}

				GEM_BUG_ON(!rq->global_seqno);
				seqno = rq->global_seqno - 1;
				i915_request_put(rq);
			}

			err = i915_reset_engine(engine, NULL);
			if (err) {
				pr_err("i915_reset_engine failed\n");
				break;
			}

			if (i915_reset_count(&i915->gpu_error) != reset_count) {
				pr_err("Full GPU reset recorded! (engine reset expected)\n");
				err = -EINVAL;
				break;
			}

			reset_engine_count += active;
			if (i915_reset_engine_count(&i915->gpu_error, engine) !=
			    reset_engine_count) {
				pr_err("%s engine reset %srecorded!\n",
				       engine->name, active ? "not " : "");
				err = -EINVAL;
				break;
			}

			if (!wait_for_idle(engine)) {
				struct drm_printer p =
					drm_info_printer(i915->drm.dev);

				pr_err("%s failed to idle after reset\n",
				       engine->name);
				intel_engine_dump(engine, &p,
						  "%s\n", engine->name);

				err = -EIO;
				break;
			}
		} while (time_before(jiffies, end_time));
		clear_bit(I915_RESET_ENGINE + id, &i915->gpu_error.flags);

		if (err)
			break;

		err = igt_flush_test(i915, 0);
		if (err)
			break;
	}

	if (i915_terminally_wedged(&i915->gpu_error))
		err = -EIO;

	if (active) {
		mutex_lock(&i915->drm.struct_mutex);
		hang_fini(&h);
		mutex_unlock(&i915->drm.struct_mutex);
	}

	return err;
}

static int igt_reset_idle_engine(void *arg)
{
	return __igt_reset_engine(arg, false);
}

static int igt_reset_active_engine(void *arg)
{
	return __igt_reset_engine(arg, true);
}

struct active_engine {
	struct task_struct *task;
	struct intel_engine_cs *engine;
	unsigned long resets;
	unsigned int flags;
};

#define TEST_ACTIVE	BIT(0)
#define TEST_OTHERS	BIT(1)
#define TEST_SELF	BIT(2)
#define TEST_PRIORITY	BIT(3)

static int active_request_put(struct i915_request *rq)
{
	int err = 0;

	if (!rq)
		return 0;

	if (i915_request_wait(rq, 0, 5 * HZ) < 0) {
		GEM_TRACE("%s timed out waiting for completion of fence %llx:%d, seqno %d.\n",
			  rq->engine->name,
			  rq->fence.context,
			  rq->fence.seqno,
			  i915_request_global_seqno(rq));
		GEM_TRACE_DUMP();

		i915_gem_set_wedged(rq->i915);
		err = -EIO;
	}

	i915_request_put(rq);

	return err;
}

static int active_engine(void *data)
{
	I915_RND_STATE(prng);
	struct active_engine *arg = data;
	struct intel_engine_cs *engine = arg->engine;
	struct i915_request *rq[8] = {};
	struct i915_gem_context *ctx[ARRAY_SIZE(rq)];
	struct drm_file *file;
	unsigned long count = 0;
	int err = 0;

	file = mock_file(engine->i915);
	if (IS_ERR(file))
		return PTR_ERR(file);

	for (count = 0; count < ARRAY_SIZE(ctx); count++) {
		mutex_lock(&engine->i915->drm.struct_mutex);
		ctx[count] = live_context(engine->i915, file);
		mutex_unlock(&engine->i915->drm.struct_mutex);
		if (IS_ERR(ctx[count])) {
			err = PTR_ERR(ctx[count]);
			while (--count)
				i915_gem_context_put(ctx[count]);
			goto err_file;
		}
	}

	while (!kthread_should_stop()) {
		unsigned int idx = count++ & (ARRAY_SIZE(rq) - 1);
		struct i915_request *old = rq[idx];
		struct i915_request *new;

		mutex_lock(&engine->i915->drm.struct_mutex);
		new = i915_request_alloc(engine, ctx[idx]);
		if (IS_ERR(new)) {
			mutex_unlock(&engine->i915->drm.struct_mutex);
			err = PTR_ERR(new);
			break;
		}

		if (arg->flags & TEST_PRIORITY)
			ctx[idx]->sched.priority =
				i915_prandom_u32_max_state(512, &prng);

		rq[idx] = i915_request_get(new);
		i915_request_add(new);
		mutex_unlock(&engine->i915->drm.struct_mutex);

		err = active_request_put(old);
		if (err)
			break;

		cond_resched();
	}

	for (count = 0; count < ARRAY_SIZE(rq); count++) {
		int err__ = active_request_put(rq[count]);

		/* Keep the first error */
		if (!err)
			err = err__;
	}

err_file:
	mock_file_free(engine->i915, file);
	return err;
}

static int __igt_reset_engines(struct drm_i915_private *i915,
			       const char *test_name,
			       unsigned int flags)
{
	struct intel_engine_cs *engine, *other;
	enum intel_engine_id id, tmp;
	struct hang h;
	int err = 0;

	/* Check that issuing a reset on one engine does not interfere
	 * with any other engine.
	 */

	if (!intel_has_reset_engine(i915))
		return 0;

	if (flags & TEST_ACTIVE) {
		mutex_lock(&i915->drm.struct_mutex);
		err = hang_init(&h, i915);
		mutex_unlock(&i915->drm.struct_mutex);
		if (err)
			return err;

		if (flags & TEST_PRIORITY)
			h.ctx->sched.priority = 1024;
	}

	for_each_engine(engine, i915, id) {
		struct active_engine threads[I915_NUM_ENGINES] = {};
		unsigned long global = i915_reset_count(&i915->gpu_error);
		unsigned long count = 0, reported;
		IGT_TIMEOUT(end_time);

		if (flags & TEST_ACTIVE &&
		    !intel_engine_can_store_dword(engine))
			continue;

		if (!wait_for_idle(engine)) {
			pr_err("i915_reset_engine(%s:%s): failed to idle before reset\n",
			       engine->name, test_name);
			err = -EIO;
			break;
		}

		memset(threads, 0, sizeof(threads));
		for_each_engine(other, i915, tmp) {
			struct task_struct *tsk;

			threads[tmp].resets =
				i915_reset_engine_count(&i915->gpu_error,
							other);

			if (!(flags & TEST_OTHERS))
				continue;

			if (other == engine && !(flags & TEST_SELF))
				continue;

			threads[tmp].engine = other;
			threads[tmp].flags = flags;

			tsk = kthread_run(active_engine, &threads[tmp],
					  "igt/%s", other->name);
			if (IS_ERR(tsk)) {
				err = PTR_ERR(tsk);
				goto unwind;
			}

			threads[tmp].task = tsk;
			get_task_struct(tsk);
		}

		set_bit(I915_RESET_ENGINE + id, &i915->gpu_error.flags);
		do {
			u32 seqno = intel_engine_get_seqno(engine);
			struct i915_request *rq = NULL;

			if (flags & TEST_ACTIVE) {
				mutex_lock(&i915->drm.struct_mutex);
				rq = hang_create_request(&h, engine);
				if (IS_ERR(rq)) {
					err = PTR_ERR(rq);
					mutex_unlock(&i915->drm.struct_mutex);
					break;
				}

				i915_request_get(rq);
				i915_request_add(rq);
				mutex_unlock(&i915->drm.struct_mutex);

				if (!wait_until_running(&h, rq)) {
					struct drm_printer p = drm_info_printer(i915->drm.dev);

					pr_err("%s: Failed to start request %x, at %x\n",
					       __func__, rq->fence.seqno, hws_seqno(&h, rq));
					intel_engine_dump(engine, &p,
							  "%s\n", engine->name);

					i915_request_put(rq);
					err = -EIO;
					break;
				}

				GEM_BUG_ON(!rq->global_seqno);
				seqno = rq->global_seqno - 1;
			}

			err = i915_reset_engine(engine, NULL);
			if (err) {
				pr_err("i915_reset_engine(%s:%s): failed, err=%d\n",
				       engine->name, test_name, err);
				break;
			}

			count++;

			if (rq) {
				i915_request_wait(rq, 0, MAX_SCHEDULE_TIMEOUT);
				i915_request_put(rq);
			}

			if (!(flags & TEST_SELF) && !wait_for_idle(engine)) {
				struct drm_printer p =
					drm_info_printer(i915->drm.dev);

				pr_err("i915_reset_engine(%s:%s):"
				       " failed to idle after reset\n",
				       engine->name, test_name);
				intel_engine_dump(engine, &p,
						  "%s\n", engine->name);

				err = -EIO;
				break;
			}
		} while (time_before(jiffies, end_time));
		clear_bit(I915_RESET_ENGINE + id, &i915->gpu_error.flags);
		pr_info("i915_reset_engine(%s:%s): %lu resets\n",
			engine->name, test_name, count);

		reported = i915_reset_engine_count(&i915->gpu_error, engine);
		reported -= threads[engine->id].resets;
		if (reported != (flags & TEST_ACTIVE ? count : 0)) {
			pr_err("i915_reset_engine(%s:%s): reset %lu times, but reported %lu, expected %lu reported\n",
			       engine->name, test_name, count, reported,
			       (flags & TEST_ACTIVE ? count : 0));
			if (!err)
				err = -EINVAL;
		}

unwind:
		for_each_engine(other, i915, tmp) {
			int ret;

			if (!threads[tmp].task)
				continue;

			ret = kthread_stop(threads[tmp].task);
			if (ret) {
				pr_err("kthread for other engine %s failed, err=%d\n",
				       other->name, ret);
				if (!err)
					err = ret;
			}
			put_task_struct(threads[tmp].task);

			if (other != engine &&
			    threads[tmp].resets !=
			    i915_reset_engine_count(&i915->gpu_error, other)) {
				pr_err("Innocent engine %s was reset (count=%ld)\n",
				       other->name,
				       i915_reset_engine_count(&i915->gpu_error,
							       other) -
				       threads[tmp].resets);
				if (!err)
					err = -EINVAL;
			}
		}

		if (global != i915_reset_count(&i915->gpu_error)) {
			pr_err("Global reset (count=%ld)!\n",
			       i915_reset_count(&i915->gpu_error) - global);
			if (!err)
				err = -EINVAL;
		}

		if (err)
			break;

		err = igt_flush_test(i915, 0);
		if (err)
			break;
	}

	if (i915_terminally_wedged(&i915->gpu_error))
		err = -EIO;

	if (flags & TEST_ACTIVE) {
		mutex_lock(&i915->drm.struct_mutex);
		hang_fini(&h);
		mutex_unlock(&i915->drm.struct_mutex);
	}

	return err;
}

static int igt_reset_engines(void *arg)
{
	static const struct {
		const char *name;
		unsigned int flags;
	} phases[] = {
		{ "idle", 0 },
		{ "active", TEST_ACTIVE },
		{ "others-idle", TEST_OTHERS },
		{ "others-active", TEST_OTHERS | TEST_ACTIVE },
		{
			"others-priority",
			TEST_OTHERS | TEST_ACTIVE | TEST_PRIORITY
		},
		{
			"self-priority",
			TEST_OTHERS | TEST_ACTIVE | TEST_PRIORITY | TEST_SELF,
		},
		{ }
	};
	struct drm_i915_private *i915 = arg;
	typeof(*phases) *p;
	int err;

	for (p = phases; p->name; p++) {
		if (p->flags & TEST_PRIORITY) {
			if (!(i915->caps.scheduler & I915_SCHEDULER_CAP_PRIORITY))
				continue;
		}

		err = __igt_reset_engines(arg, p->name, p->flags);
		if (err)
			return err;
	}

	return 0;
}

static u32 fake_hangcheck(struct i915_request *rq, u32 mask)
{
	struct i915_gpu_error *error = &rq->i915->gpu_error;
	u32 reset_count = i915_reset_count(error);

	error->stalled_mask = mask;

	/* set_bit() must be after we have setup the backchannel (mask) */
	smp_mb__before_atomic();
	set_bit(I915_RESET_HANDOFF, &error->flags);

	wake_up_all(&error->wait_queue);

	return reset_count;
}

static int igt_reset_wait(void *arg)
{
	struct drm_i915_private *i915 = arg;
	struct i915_request *rq;
	unsigned int reset_count;
	struct hang h;
	long timeout;
	int err;

	if (!intel_engine_can_store_dword(i915->engine[RCS]))
		return 0;

	/* Check that we detect a stuck waiter and issue a reset */

	global_reset_lock(i915);

	mutex_lock(&i915->drm.struct_mutex);
	err = hang_init(&h, i915);
	if (err)
		goto unlock;

	rq = hang_create_request(&h, i915->engine[RCS]);
	if (IS_ERR(rq)) {
		err = PTR_ERR(rq);
		goto fini;
	}

	i915_request_get(rq);
	i915_request_add(rq);

	if (!wait_until_running(&h, rq)) {
		struct drm_printer p = drm_info_printer(i915->drm.dev);

		pr_err("%s: Failed to start request %x, at %x\n",
		       __func__, rq->fence.seqno, hws_seqno(&h, rq));
		intel_engine_dump(rq->engine, &p, "%s\n", rq->engine->name);

		i915_gem_set_wedged(i915);

		err = -EIO;
		goto out_rq;
	}

	reset_count = fake_hangcheck(rq, ALL_ENGINES);

	timeout = i915_request_wait(rq, I915_WAIT_LOCKED, 10);
	if (timeout < 0) {
		pr_err("i915_request_wait failed on a stuck request: err=%ld\n",
		       timeout);
		err = timeout;
		goto out_rq;
	}

	GEM_BUG_ON(test_bit(I915_RESET_HANDOFF, &i915->gpu_error.flags));
	if (i915_reset_count(&i915->gpu_error) == reset_count) {
		pr_err("No GPU reset recorded!\n");
		err = -EINVAL;
		goto out_rq;
	}

out_rq:
	i915_request_put(rq);
fini:
	hang_fini(&h);
unlock:
	mutex_unlock(&i915->drm.struct_mutex);
	global_reset_unlock(i915);

	if (i915_terminally_wedged(&i915->gpu_error))
		return -EIO;

	return err;
}

struct evict_vma {
	struct completion completion;
	struct i915_vma *vma;
};

static int evict_vma(void *data)
{
	struct evict_vma *arg = data;
	struct i915_address_space *vm = arg->vma->vm;
	struct drm_i915_private *i915 = vm->i915;
	struct drm_mm_node evict = arg->vma->node;
	int err;

	complete(&arg->completion);

	mutex_lock(&i915->drm.struct_mutex);
	err = i915_gem_evict_for_node(vm, &evict, 0);
	mutex_unlock(&i915->drm.struct_mutex);

	return err;
}

static int __igt_reset_evict_vma(struct drm_i915_private *i915,
				 struct i915_address_space *vm)
{
	struct drm_i915_gem_object *obj;
	struct task_struct *tsk = NULL;
	struct i915_request *rq;
	struct evict_vma arg;
	struct hang h;
	int err;

	if (!intel_engine_can_store_dword(i915->engine[RCS]))
		return 0;

	/* Check that we can recover an unbind stuck on a hanging request */

	global_reset_lock(i915);

	mutex_lock(&i915->drm.struct_mutex);
	err = hang_init(&h, i915);
	if (err)
		goto unlock;

	obj = i915_gem_object_create_internal(i915, PAGE_SIZE);
	if (IS_ERR(obj)) {
		err = PTR_ERR(obj);
		goto fini;
	}

	arg.vma = i915_vma_instance(obj, vm, NULL);
	if (IS_ERR(arg.vma)) {
		err = PTR_ERR(arg.vma);
		goto out_obj;
	}

	rq = hang_create_request(&h, i915->engine[RCS]);
	if (IS_ERR(rq)) {
		err = PTR_ERR(rq);
		goto out_obj;
	}

	err = i915_vma_pin(arg.vma, 0, 0,
			   i915_vma_is_ggtt(arg.vma) ? PIN_GLOBAL : PIN_USER);
	if (err)
		goto out_obj;

	err = i915_vma_move_to_active(arg.vma, rq, EXEC_OBJECT_WRITE);
	i915_vma_unpin(arg.vma);

	i915_request_get(rq);
	i915_request_add(rq);
	if (err)
		goto out_rq;

	mutex_unlock(&i915->drm.struct_mutex);

	if (!wait_until_running(&h, rq)) {
		struct drm_printer p = drm_info_printer(i915->drm.dev);

		pr_err("%s: Failed to start request %x, at %x\n",
		       __func__, rq->fence.seqno, hws_seqno(&h, rq));
		intel_engine_dump(rq->engine, &p, "%s\n", rq->engine->name);

		i915_gem_set_wedged(i915);
		goto out_reset;
	}

	init_completion(&arg.completion);

	tsk = kthread_run(evict_vma, &arg, "igt/evict_vma");
	if (IS_ERR(tsk)) {
		err = PTR_ERR(tsk);
		tsk = NULL;
		goto out_reset;
	}

	wait_for_completion(&arg.completion);

	if (wait_for(waitqueue_active(&rq->execute), 10)) {
		struct drm_printer p = drm_info_printer(i915->drm.dev);

		pr_err("igt/evict_vma kthread did not wait\n");
		intel_engine_dump(rq->engine, &p, "%s\n", rq->engine->name);

		i915_gem_set_wedged(i915);
		goto out_reset;
	}

out_reset:
	fake_hangcheck(rq, intel_engine_flag(rq->engine));

	if (tsk) {
		struct igt_wedge_me w;

		/* The reset, even indirectly, should take less than 10ms. */
		igt_wedge_on_timeout(&w, i915, HZ / 10 /* 100ms timeout*/)
			err = kthread_stop(tsk);
	}

	mutex_lock(&i915->drm.struct_mutex);
out_rq:
	i915_request_put(rq);
out_obj:
	i915_gem_object_put(obj);
fini:
	hang_fini(&h);
unlock:
	mutex_unlock(&i915->drm.struct_mutex);
	global_reset_unlock(i915);

	if (i915_terminally_wedged(&i915->gpu_error))
		return -EIO;

	return err;
}

static int igt_reset_evict_ggtt(void *arg)
{
	struct drm_i915_private *i915 = arg;

	return __igt_reset_evict_vma(i915, &i915->ggtt.vm);
}

static int igt_reset_evict_ppgtt(void *arg)
{
	struct drm_i915_private *i915 = arg;
	struct i915_gem_context *ctx;
	int err;

	mutex_lock(&i915->drm.struct_mutex);
	ctx = kernel_context(i915);
	mutex_unlock(&i915->drm.struct_mutex);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	err = 0;
	if (ctx->ppgtt) /* aliasing == global gtt locking, covered above */
		err = __igt_reset_evict_vma(i915, &ctx->ppgtt->vm);

	kernel_context_close(ctx);
	return err;
}

static int wait_for_others(struct drm_i915_private *i915,
			   struct intel_engine_cs *exclude)
{
	struct intel_engine_cs *engine;
	enum intel_engine_id id;

	for_each_engine(engine, i915, id) {
		if (engine == exclude)
			continue;

		if (!wait_for_idle(engine))
			return -EIO;
	}

	return 0;
}

static int igt_reset_queue(void *arg)
{
	struct drm_i915_private *i915 = arg;
	struct intel_engine_cs *engine;
	enum intel_engine_id id;
	struct hang h;
	int err;

	/* Check that we replay pending requests following a hang */

	global_reset_lock(i915);

	mutex_lock(&i915->drm.struct_mutex);
	err = hang_init(&h, i915);
	if (err)
		goto unlock;

	for_each_engine(engine, i915, id) {
		struct i915_request *prev;
		IGT_TIMEOUT(end_time);
		unsigned int count;

		if (!intel_engine_can_store_dword(engine))
			continue;

		prev = hang_create_request(&h, engine);
		if (IS_ERR(prev)) {
			err = PTR_ERR(prev);
			goto fini;
		}

		i915_request_get(prev);
		i915_request_add(prev);

		count = 0;
		do {
			struct i915_request *rq;
			unsigned int reset_count;

			rq = hang_create_request(&h, engine);
			if (IS_ERR(rq)) {
				err = PTR_ERR(rq);
				goto fini;
			}

			i915_request_get(rq);
			i915_request_add(rq);

			/*
			 * XXX We don't handle resetting the kernel context
			 * very well. If we trigger a device reset twice in
			 * quick succession while the kernel context is
			 * executing, we may end up skipping the breadcrumb.
			 * This is really only a problem for the selftest as
			 * normally there is a large interlude between resets
			 * (hangcheck), or we focus on resetting just one
			 * engine and so avoid repeatedly resetting innocents.
			 */
			err = wait_for_others(i915, engine);
			if (err) {
				pr_err("%s(%s): Failed to idle other inactive engines after device reset\n",
				       __func__, engine->name);
				i915_request_put(rq);
				i915_request_put(prev);

				GEM_TRACE_DUMP();
				i915_gem_set_wedged(i915);
				goto fini;
			}

			if (!wait_until_running(&h, prev)) {
				struct drm_printer p = drm_info_printer(i915->drm.dev);

				pr_err("%s(%s): Failed to start request %x, at %x\n",
				       __func__, engine->name,
				       prev->fence.seqno, hws_seqno(&h, prev));
				intel_engine_dump(engine, &p,
						  "%s\n", engine->name);

				i915_request_put(rq);
				i915_request_put(prev);

				i915_gem_set_wedged(i915);

				err = -EIO;
				goto fini;
			}

			reset_count = fake_hangcheck(prev, ENGINE_MASK(id));

			i915_reset(i915, ENGINE_MASK(id), NULL);

			GEM_BUG_ON(test_bit(I915_RESET_HANDOFF,
					    &i915->gpu_error.flags));

			if (prev->fence.error != -EIO) {
				pr_err("GPU reset not recorded on hanging request [fence.error=%d]!\n",
				       prev->fence.error);
				i915_request_put(rq);
				i915_request_put(prev);
				err = -EINVAL;
				goto fini;
			}

			if (rq->fence.error) {
				pr_err("Fence error status not zero [%d] after unrelated reset\n",
				       rq->fence.error);
				i915_request_put(rq);
				i915_request_put(prev);
				err = -EINVAL;
				goto fini;
			}

			if (i915_reset_count(&i915->gpu_error) == reset_count) {
				pr_err("No GPU reset recorded!\n");
				i915_request_put(rq);
				i915_request_put(prev);
				err = -EINVAL;
				goto fini;
			}

			i915_request_put(prev);
			prev = rq;
			count++;
		} while (time_before(jiffies, end_time));
		pr_info("%s: Completed %d resets\n", engine->name, count);

		*h.batch = MI_BATCH_BUFFER_END;
		i915_gem_chipset_flush(i915);

		i915_request_put(prev);

		err = igt_flush_test(i915, I915_WAIT_LOCKED);
		if (err)
			break;
	}

fini:
	hang_fini(&h);
unlock:
	mutex_unlock(&i915->drm.struct_mutex);
	global_reset_unlock(i915);

	if (i915_terminally_wedged(&i915->gpu_error))
		return -EIO;

	return err;
}

static int igt_handle_error(void *arg)
{
	struct drm_i915_private *i915 = arg;
	struct intel_engine_cs *engine = i915->engine[RCS];
	struct hang h;
	struct i915_request *rq;
	struct i915_gpu_state *error;
	int err;

	/* Check that we can issue a global GPU and engine reset */

	if (!intel_has_reset_engine(i915))
		return 0;

	if (!engine || !intel_engine_can_store_dword(engine))
		return 0;

	mutex_lock(&i915->drm.struct_mutex);

	err = hang_init(&h, i915);
	if (err)
		goto err_unlock;

	rq = hang_create_request(&h, engine);
	if (IS_ERR(rq)) {
		err = PTR_ERR(rq);
		goto err_fini;
	}

	i915_request_get(rq);
	i915_request_add(rq);

	if (!wait_until_running(&h, rq)) {
		struct drm_printer p = drm_info_printer(i915->drm.dev);

		pr_err("%s: Failed to start request %x, at %x\n",
		       __func__, rq->fence.seqno, hws_seqno(&h, rq));
		intel_engine_dump(rq->engine, &p, "%s\n", rq->engine->name);

		i915_gem_set_wedged(i915);

		err = -EIO;
		goto err_request;
	}

	mutex_unlock(&i915->drm.struct_mutex);

	/* Temporarily disable error capture */
	error = xchg(&i915->gpu_error.first_error, (void *)-1);

	i915_handle_error(i915, ENGINE_MASK(engine->id), 0, NULL);

	xchg(&i915->gpu_error.first_error, error);

	mutex_lock(&i915->drm.struct_mutex);

	if (rq->fence.error != -EIO) {
		pr_err("Guilty request not identified!\n");
		err = -EINVAL;
		goto err_request;
	}

err_request:
	i915_request_put(rq);
err_fini:
	hang_fini(&h);
err_unlock:
	mutex_unlock(&i915->drm.struct_mutex);
	return err;
}

int intel_hangcheck_live_selftests(struct drm_i915_private *i915)
{
	static const struct i915_subtest tests[] = {
		SUBTEST(igt_global_reset), /* attempt to recover GPU first */
		SUBTEST(igt_hang_sanitycheck),
		SUBTEST(igt_reset_idle_engine),
		SUBTEST(igt_reset_active_engine),
		SUBTEST(igt_reset_engines),
		SUBTEST(igt_reset_queue),
		SUBTEST(igt_reset_wait),
		SUBTEST(igt_reset_evict_ggtt),
		SUBTEST(igt_reset_evict_ppgtt),
		SUBTEST(igt_handle_error),
	};
	bool saved_hangcheck;
	int err;

	if (!intel_has_gpu_reset(i915))
		return 0;

	if (i915_terminally_wedged(&i915->gpu_error))
		return -EIO; /* we're long past hope of a successful reset */

	intel_runtime_pm_get(i915);
	saved_hangcheck = fetch_and_zero(&i915_modparams.enable_hangcheck);

	err = i915_subtests(tests, i915);

	mutex_lock(&i915->drm.struct_mutex);
	igt_flush_test(i915, I915_WAIT_LOCKED);
	mutex_unlock(&i915->drm.struct_mutex);

	i915_modparams.enable_hangcheck = saved_hangcheck;
	intel_runtime_pm_put(i915);

	return err;
}
