/*
 * Copyright (C) 2016-2018 Netronome Systems, Inc.
 *
 * This software is dual licensed under the GNU General License Version 2,
 * June 1991 as shown in the file COPYING in the top-level directory of this
 * source tree or the BSD 2-Clause License provided below.  You have the
 * option to license this software under the complete terms of either license.
 *
 * The BSD 2-Clause License:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      1. Redistributions of source code must retain the above
 *         copyright notice, this list of conditions and the following
 *         disclaimer.
 *
 *      2. Redistributions in binary form must reproduce the above
 *         copyright notice, this list of conditions and the following
 *         disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/bpf.h>
#include <linux/bpf_verifier.h>
#include <linux/kernel.h>
#include <linux/pkt_cls.h>

#include "../nfp_app.h"
#include "../nfp_main.h"
#include "fw.h"
#include "main.h"

#define pr_vlog(env, fmt, ...)	\
	bpf_verifier_log_write(env, "[nfp] " fmt, ##__VA_ARGS__)

struct nfp_insn_meta *
nfp_bpf_goto_meta(struct nfp_prog *nfp_prog, struct nfp_insn_meta *meta,
		  unsigned int insn_idx, unsigned int n_insns)
{
	unsigned int forward, backward, i;

	backward = meta->n - insn_idx;
	forward = insn_idx - meta->n;

	if (min(forward, backward) > n_insns - insn_idx - 1) {
		backward = n_insns - insn_idx - 1;
		meta = nfp_prog_last_meta(nfp_prog);
	}
	if (min(forward, backward) > insn_idx && backward > insn_idx) {
		forward = insn_idx;
		meta = nfp_prog_first_meta(nfp_prog);
	}

	if (forward < backward)
		for (i = 0; i < forward; i++)
			meta = nfp_meta_next(meta);
	else
		for (i = 0; i < backward; i++)
			meta = nfp_meta_prev(meta);

	return meta;
}

static void
nfp_record_adjust_head(struct nfp_app_bpf *bpf, struct nfp_prog *nfp_prog,
		       struct nfp_insn_meta *meta,
		       const struct bpf_reg_state *reg2)
{
	unsigned int location =	UINT_MAX;
	int imm;

	/* Datapath usually can give us guarantees on how much adjust head
	 * can be done without the need for any checks.  Optimize the simple
	 * case where there is only one adjust head by a constant.
	 */
	if (reg2->type != SCALAR_VALUE || !tnum_is_const(reg2->var_off))
		goto exit_set_location;
	imm = reg2->var_off.value;
	/* Translator will skip all checks, we need to guarantee min pkt len */
	if (imm > ETH_ZLEN - ETH_HLEN)
		goto exit_set_location;
	if (imm > (int)bpf->adjust_head.guaranteed_add ||
	    imm < -bpf->adjust_head.guaranteed_sub)
		goto exit_set_location;

	if (nfp_prog->adjust_head_location) {
		/* Only one call per program allowed */
		if (nfp_prog->adjust_head_location != meta->n)
			goto exit_set_location;

		if (meta->arg2.reg.var_off.value != imm)
			goto exit_set_location;
	}

	location = meta->n;
exit_set_location:
	nfp_prog->adjust_head_location = location;
}

static int
nfp_bpf_stack_arg_ok(const char *fname, struct bpf_verifier_env *env,
		     const struct bpf_reg_state *reg,
		     struct nfp_bpf_reg_state *old_arg)
{
	s64 off, old_off;

	if (reg->type != PTR_TO_STACK) {
		pr_vlog(env, "%s: unsupported ptr type %d\n",
			fname, reg->type);
		return false;
	}
	if (!tnum_is_const(reg->var_off)) {
		pr_vlog(env, "%s: variable pointer\n", fname);
		return false;
	}

	off = reg->var_off.value + reg->off;
	if (-off % 4) {
		pr_vlog(env, "%s: unaligned stack pointer %lld\n", fname, -off);
		return false;
	}

	/* Rest of the checks is only if we re-parse the same insn */
	if (!old_arg)
		return true;

	old_off = old_arg->reg.var_off.value + old_arg->reg.off;
	old_arg->var_off |= off != old_off;

	return true;
}

static bool
nfp_bpf_map_call_ok(const char *fname, struct bpf_verifier_env *env,
		    struct nfp_insn_meta *meta,
		    u32 helper_tgt, const struct bpf_reg_state *reg1)
{
	if (!helper_tgt) {
		pr_vlog(env, "%s: not supported by FW\n", fname);
		return false;
	}

	return true;
}

static int
nfp_bpf_check_call(struct nfp_prog *nfp_prog, struct bpf_verifier_env *env,
		   struct nfp_insn_meta *meta)
{
	const struct bpf_reg_state *reg1 = cur_regs(env) + BPF_REG_1;
	const struct bpf_reg_state *reg2 = cur_regs(env) + BPF_REG_2;
	const struct bpf_reg_state *reg3 = cur_regs(env) + BPF_REG_3;
	struct nfp_app_bpf *bpf = nfp_prog->bpf;
	u32 func_id = meta->insn.imm;

	switch (func_id) {
	case BPF_FUNC_xdp_adjust_head:
		if (!bpf->adjust_head.off_max) {
			pr_vlog(env, "adjust_head not supported by FW\n");
			return -EOPNOTSUPP;
		}
		if (!(bpf->adjust_head.flags & NFP_BPF_ADJUST_HEAD_NO_META)) {
			pr_vlog(env, "adjust_head: FW requires shifting metadata, not supported by the driver\n");
			return -EOPNOTSUPP;
		}

		nfp_record_adjust_head(bpf, nfp_prog, meta, reg2);
		break;

	case BPF_FUNC_xdp_adjust_tail:
		if (!bpf->adjust_tail) {
			pr_vlog(env, "adjust_tail not supported by FW\n");
			return -EOPNOTSUPP;
		}
		break;

	case BPF_FUNC_map_lookup_elem:
		if (!nfp_bpf_map_call_ok("map_lookup", env, meta,
					 bpf->helpers.map_lookup, reg1) ||
		    !nfp_bpf_stack_arg_ok("map_lookup", env, reg2,
					  meta->func_id ? &meta->arg2 : NULL))
			return -EOPNOTSUPP;
		break;

	case BPF_FUNC_map_update_elem:
		if (!nfp_bpf_map_call_ok("map_update", env, meta,
					 bpf->helpers.map_update, reg1) ||
		    !nfp_bpf_stack_arg_ok("map_update", env, reg2,
					  meta->func_id ? &meta->arg2 : NULL) ||
		    !nfp_bpf_stack_arg_ok("map_update", env, reg3, NULL))
			return -EOPNOTSUPP;
		break;

	case BPF_FUNC_map_delete_elem:
		if (!nfp_bpf_map_call_ok("map_delete", env, meta,
					 bpf->helpers.map_delete, reg1) ||
		    !nfp_bpf_stack_arg_ok("map_delete", env, reg2,
					  meta->func_id ? &meta->arg2 : NULL))
			return -EOPNOTSUPP;
		break;

	case BPF_FUNC_get_prandom_u32:
		if (bpf->pseudo_random)
			break;
		pr_vlog(env, "bpf_get_prandom_u32(): FW doesn't support random number generation\n");
		return -EOPNOTSUPP;

	case BPF_FUNC_perf_event_output:
		BUILD_BUG_ON(NFP_BPF_SCALAR_VALUE != SCALAR_VALUE ||
			     NFP_BPF_MAP_VALUE != PTR_TO_MAP_VALUE ||
			     NFP_BPF_STACK != PTR_TO_STACK ||
			     NFP_BPF_PACKET_DATA != PTR_TO_PACKET);

		if (!bpf->helpers.perf_event_output) {
			pr_vlog(env, "event_output: not supported by FW\n");
			return -EOPNOTSUPP;
		}

		/* Force current CPU to make sure we can report the event
		 * wherever we get the control message from FW.
		 */
		if (reg3->var_off.mask & BPF_F_INDEX_MASK ||
		    (reg3->var_off.value & BPF_F_INDEX_MASK) !=
		    BPF_F_CURRENT_CPU) {
			char tn_buf[48];

			tnum_strn(tn_buf, sizeof(tn_buf), reg3->var_off);
			pr_vlog(env, "event_output: must use BPF_F_CURRENT_CPU, var_off: %s\n",
				tn_buf);
			return -EOPNOTSUPP;
		}

		/* Save space in meta, we don't care about arguments other
		 * than 4th meta, shove it into arg1.
		 */
		reg1 = cur_regs(env) + BPF_REG_4;

		if (reg1->type != SCALAR_VALUE /* NULL ptr */ &&
		    reg1->type != PTR_TO_STACK &&
		    reg1->type != PTR_TO_MAP_VALUE &&
		    reg1->type != PTR_TO_PACKET) {
			pr_vlog(env, "event_output: unsupported ptr type: %d\n",
				reg1->type);
			return -EOPNOTSUPP;
		}

		if (reg1->type == PTR_TO_STACK &&
		    !nfp_bpf_stack_arg_ok("event_output", env, reg1, NULL))
			return -EOPNOTSUPP;

		/* Warn user that on offload NFP may return success even if map
		 * is not going to accept the event, since the event output is
		 * fully async and device won't know the state of the map.
		 * There is also FW limitation on the event length.
		 *
		 * Lost events will not show up on the perf ring, driver
		 * won't see them at all.  Events may also get reordered.
		 */
		dev_warn_once(&nfp_prog->bpf->app->pf->pdev->dev,
			      "bpf: note: return codes and behavior of bpf_event_output() helper differs for offloaded programs!\n");
		pr_vlog(env, "warning: return codes and behavior of event_output helper differ for offload!\n");

		if (!meta->func_id)
			break;

		if (reg1->type != meta->arg1.type) {
			pr_vlog(env, "event_output: ptr type changed: %d %d\n",
				meta->arg1.type, reg1->type);
			return -EINVAL;
		}
		break;

	default:
		pr_vlog(env, "unsupported function id: %d\n", func_id);
		return -EOPNOTSUPP;
	}

	meta->func_id = func_id;
	meta->arg1 = *reg1;
	meta->arg2.reg = *reg2;

	return 0;
}

static int
nfp_bpf_check_exit(struct nfp_prog *nfp_prog,
		   struct bpf_verifier_env *env)
{
	const struct bpf_reg_state *reg0 = cur_regs(env) + BPF_REG_0;
	u64 imm;

	if (nfp_prog->type == BPF_PROG_TYPE_XDP)
		return 0;

	if (!(reg0->type == SCALAR_VALUE && tnum_is_const(reg0->var_off))) {
		char tn_buf[48];

		tnum_strn(tn_buf, sizeof(tn_buf), reg0->var_off);
		pr_vlog(env, "unsupported exit state: %d, var_off: %s\n",
			reg0->type, tn_buf);
		return -EINVAL;
	}

	imm = reg0->var_off.value;
	if (nfp_prog->type == BPF_PROG_TYPE_SCHED_CLS &&
	    imm <= TC_ACT_REDIRECT &&
	    imm != TC_ACT_SHOT && imm != TC_ACT_STOLEN &&
	    imm != TC_ACT_QUEUED) {
		pr_vlog(env, "unsupported exit state: %d, imm: %llx\n",
			reg0->type, imm);
		return -EINVAL;
	}

	return 0;
}

static int
nfp_bpf_check_stack_access(struct nfp_prog *nfp_prog,
			   struct nfp_insn_meta *meta,
			   const struct bpf_reg_state *reg,
			   struct bpf_verifier_env *env)
{
	s32 old_off, new_off;

	if (!tnum_is_const(reg->var_off)) {
		pr_vlog(env, "variable ptr stack access\n");
		return -EINVAL;
	}

	if (meta->ptr.type == NOT_INIT)
		return 0;

	old_off = meta->ptr.off + meta->ptr.var_off.value;
	new_off = reg->off + reg->var_off.value;

	meta->ptr_not_const |= old_off != new_off;

	if (!meta->ptr_not_const)
		return 0;

	if (old_off % 4 == new_off % 4)
		return 0;

	pr_vlog(env, "stack access changed location was:%d is:%d\n",
		old_off, new_off);
	return -EINVAL;
}

static const char *nfp_bpf_map_use_name(enum nfp_bpf_map_use use)
{
	static const char * const names[] = {
		[NFP_MAP_UNUSED]	= "unused",
		[NFP_MAP_USE_READ]	= "read",
		[NFP_MAP_USE_WRITE]	= "write",
		[NFP_MAP_USE_ATOMIC_CNT] = "atomic",
	};

	if (use >= ARRAY_SIZE(names) || !names[use])
		return "unknown";
	return names[use];
}

static int
nfp_bpf_map_mark_used_one(struct bpf_verifier_env *env,
			  struct nfp_bpf_map *nfp_map,
			  unsigned int off, enum nfp_bpf_map_use use)
{
	if (nfp_map->use_map[off / 4] != NFP_MAP_UNUSED &&
	    nfp_map->use_map[off / 4] != use) {
		pr_vlog(env, "map value use type conflict %s vs %s off: %u\n",
			nfp_bpf_map_use_name(nfp_map->use_map[off / 4]),
			nfp_bpf_map_use_name(use), off);
		return -EOPNOTSUPP;
	}

	nfp_map->use_map[off / 4] = use;

	return 0;
}

static int
nfp_bpf_map_mark_used(struct bpf_verifier_env *env, struct nfp_insn_meta *meta,
		      const struct bpf_reg_state *reg,
		      enum nfp_bpf_map_use use)
{
	struct bpf_offloaded_map *offmap;
	struct nfp_bpf_map *nfp_map;
	unsigned int size, off;
	int i, err;

	if (!tnum_is_const(reg->var_off)) {
		pr_vlog(env, "map value offset is variable\n");
		return -EOPNOTSUPP;
	}

	off = reg->var_off.value + meta->insn.off + reg->off;
	size = BPF_LDST_BYTES(&meta->insn);
	offmap = map_to_offmap(reg->map_ptr);
	nfp_map = offmap->dev_priv;

	if (off + size > offmap->map.value_size) {
		pr_vlog(env, "map value access out-of-bounds\n");
		return -EINVAL;
	}

	for (i = 0; i < size; i += 4 - (off + i) % 4) {
		err = nfp_bpf_map_mark_used_one(env, nfp_map, off + i, use);
		if (err)
			return err;
	}

	return 0;
}

static int
nfp_bpf_check_ptr(struct nfp_prog *nfp_prog, struct nfp_insn_meta *meta,
		  struct bpf_verifier_env *env, u8 reg_no)
{
	const struct bpf_reg_state *reg = cur_regs(env) + reg_no;
	int err;

	if (reg->type != PTR_TO_CTX &&
	    reg->type != PTR_TO_STACK &&
	    reg->type != PTR_TO_MAP_VALUE &&
	    reg->type != PTR_TO_PACKET) {
		pr_vlog(env, "unsupported ptr type: %d\n", reg->type);
		return -EINVAL;
	}

	if (reg->type == PTR_TO_STACK) {
		err = nfp_bpf_check_stack_access(nfp_prog, meta, reg, env);
		if (err)
			return err;
	}

	if (reg->type == PTR_TO_MAP_VALUE) {
		if (is_mbpf_load(meta)) {
			err = nfp_bpf_map_mark_used(env, meta, reg,
						    NFP_MAP_USE_READ);
			if (err)
				return err;
		}
		if (is_mbpf_store(meta)) {
			pr_vlog(env, "map writes not supported\n");
			return -EOPNOTSUPP;
		}
		if (is_mbpf_xadd(meta)) {
			err = nfp_bpf_map_mark_used(env, meta, reg,
						    NFP_MAP_USE_ATOMIC_CNT);
			if (err)
				return err;
		}
	}

	if (meta->ptr.type != NOT_INIT && meta->ptr.type != reg->type) {
		pr_vlog(env, "ptr type changed for instruction %d -> %d\n",
			meta->ptr.type, reg->type);
		return -EINVAL;
	}

	meta->ptr = *reg;

	return 0;
}

static int
nfp_bpf_check_store(struct nfp_prog *nfp_prog, struct nfp_insn_meta *meta,
		    struct bpf_verifier_env *env)
{
	const struct bpf_reg_state *reg = cur_regs(env) + meta->insn.dst_reg;

	if (reg->type == PTR_TO_CTX) {
		if (nfp_prog->type == BPF_PROG_TYPE_XDP) {
			/* XDP ctx accesses must be 4B in size */
			switch (meta->insn.off) {
			case offsetof(struct xdp_md, rx_queue_index):
				if (nfp_prog->bpf->queue_select)
					goto exit_check_ptr;
				pr_vlog(env, "queue selection not supported by FW\n");
				return -EOPNOTSUPP;
			}
		}
		pr_vlog(env, "unsupported store to context field\n");
		return -EOPNOTSUPP;
	}
exit_check_ptr:
	return nfp_bpf_check_ptr(nfp_prog, meta, env, meta->insn.dst_reg);
}

static int
nfp_bpf_check_xadd(struct nfp_prog *nfp_prog, struct nfp_insn_meta *meta,
		   struct bpf_verifier_env *env)
{
	const struct bpf_reg_state *sreg = cur_regs(env) + meta->insn.src_reg;
	const struct bpf_reg_state *dreg = cur_regs(env) + meta->insn.dst_reg;

	if (dreg->type != PTR_TO_MAP_VALUE) {
		pr_vlog(env, "atomic add not to a map value pointer: %d\n",
			dreg->type);
		return -EOPNOTSUPP;
	}
	if (sreg->type != SCALAR_VALUE) {
		pr_vlog(env, "atomic add not of a scalar: %d\n", sreg->type);
		return -EOPNOTSUPP;
	}

	meta->xadd_over_16bit |=
		sreg->var_off.value > 0xffff || sreg->var_off.mask > 0xffff;
	meta->xadd_maybe_16bit |=
		(sreg->var_off.value & ~sreg->var_off.mask) <= 0xffff;

	return nfp_bpf_check_ptr(nfp_prog, meta, env, meta->insn.dst_reg);
}

static int
nfp_bpf_check_alu(struct nfp_prog *nfp_prog, struct nfp_insn_meta *meta,
		  struct bpf_verifier_env *env)
{
	const struct bpf_reg_state *sreg =
		cur_regs(env) + meta->insn.src_reg;
	const struct bpf_reg_state *dreg =
		cur_regs(env) + meta->insn.dst_reg;

	meta->umin_src = min(meta->umin_src, sreg->umin_value);
	meta->umax_src = max(meta->umax_src, sreg->umax_value);
	meta->umin_dst = min(meta->umin_dst, dreg->umin_value);
	meta->umax_dst = max(meta->umax_dst, dreg->umax_value);

	/* NFP supports u16 and u32 multiplication.
	 *
	 * For ALU64, if either operand is beyond u32's value range, we reject
	 * it. One thing to note, if the source operand is BPF_K, then we need
	 * to check "imm" field directly, and we'd reject it if it is negative.
	 * Because for ALU64, "imm" (with s32 type) is expected to be sign
	 * extended to s64 which NFP mul doesn't support.
	 *
	 * For ALU32, it is fine for "imm" be negative though, because the
	 * result is 32-bits and there is no difference on the low halve of
	 * the result for signed/unsigned mul, so we will get correct result.
	 */
	if (is_mbpf_mul(meta)) {
		if (meta->umax_dst > U32_MAX) {
			pr_vlog(env, "multiplier is not within u32 value range\n");
			return -EINVAL;
		}
		if (mbpf_src(meta) == BPF_X && meta->umax_src > U32_MAX) {
			pr_vlog(env, "multiplicand is not within u32 value range\n");
			return -EINVAL;
		}
		if (mbpf_class(meta) == BPF_ALU64 &&
		    mbpf_src(meta) == BPF_K && meta->insn.imm < 0) {
			pr_vlog(env, "sign extended multiplicand won't be within u32 value range\n");
			return -EINVAL;
		}
	}

	/* NFP doesn't have divide instructions, we support divide by constant
	 * through reciprocal multiplication. Given NFP support multiplication
	 * no bigger than u32, we'd require divisor and dividend no bigger than
	 * that as well.
	 *
	 * Also eBPF doesn't support signed divide and has enforced this on C
	 * language level by failing compilation. However LLVM assembler hasn't
	 * enforced this, so it is possible for negative constant to leak in as
	 * a BPF_K operand through assembly code, we reject such cases as well.
	 */
	if (is_mbpf_div(meta)) {
		if (meta->umax_dst > U32_MAX) {
			pr_vlog(env, "dividend is not within u32 value range\n");
			return -EINVAL;
		}
		if (mbpf_src(meta) == BPF_X) {
			if (meta->umin_src != meta->umax_src) {
				pr_vlog(env, "divisor is not constant\n");
				return -EINVAL;
			}
			if (meta->umax_src > U32_MAX) {
				pr_vlog(env, "divisor is not within u32 value range\n");
				return -EINVAL;
			}
		}
		if (mbpf_src(meta) == BPF_K && meta->insn.imm < 0) {
			pr_vlog(env, "divide by negative constant is not supported\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int
nfp_verify_insn(struct bpf_verifier_env *env, int insn_idx, int prev_insn_idx)
{
	struct nfp_prog *nfp_prog = env->prog->aux->offload->dev_priv;
	struct nfp_insn_meta *meta = nfp_prog->verifier_meta;

	meta = nfp_bpf_goto_meta(nfp_prog, meta, insn_idx, env->prog->len);
	nfp_prog->verifier_meta = meta;

	if (!nfp_bpf_supported_opcode(meta->insn.code)) {
		pr_vlog(env, "instruction %#02x not supported\n",
			meta->insn.code);
		return -EINVAL;
	}

	if (meta->insn.src_reg >= MAX_BPF_REG ||
	    meta->insn.dst_reg >= MAX_BPF_REG) {
		pr_vlog(env, "program uses extended registers - jit hardening?\n");
		return -EINVAL;
	}

	if (meta->insn.code == (BPF_JMP | BPF_CALL))
		return nfp_bpf_check_call(nfp_prog, env, meta);
	if (meta->insn.code == (BPF_JMP | BPF_EXIT))
		return nfp_bpf_check_exit(nfp_prog, env);

	if (is_mbpf_load(meta))
		return nfp_bpf_check_ptr(nfp_prog, meta, env,
					 meta->insn.src_reg);
	if (is_mbpf_store(meta))
		return nfp_bpf_check_store(nfp_prog, meta, env);

	if (is_mbpf_xadd(meta))
		return nfp_bpf_check_xadd(nfp_prog, meta, env);

	if (is_mbpf_alu(meta))
		return nfp_bpf_check_alu(nfp_prog, meta, env);

	return 0;
}

const struct bpf_prog_offload_ops nfp_bpf_analyzer_ops = {
	.insn_hook = nfp_verify_insn,
};
