/*
 * HND SiliconBackplane PMU support.
 *
 * Copyright (C) 2020, Broadcom.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *
 * <<Broadcom-WL-IPTag/Dual:>>
 */

#ifndef _hndpmu_h_
#define _hndpmu_h_

#include <typedefs.h>
#include <osl_decl.h>
#include <siutils.h>
#include <sbchipc.h>
#if defined(BTOVERPCIE) || defined(BT_WLAN_REG_ON_WAR)
#include <hnd_gcisem.h>
#endif /* BTOVERPCIE || BT_WLAN_REG_ON_WAR */

#if defined(EDV)
extern uint32 si_pmu_get_backplaneclkspeed(si_t *sih);
extern void si_pmu_update_backplane_clock(si_t *sih, osl_t *osh, uint reg, uint32 mask, uint32 val);
#endif // endif

extern uint32 si_pmu_rsrc_macphy_clk_deps(si_t *sih, osl_t *osh, int maccore_index);
extern uint32 si_pmu_rsrc_ht_avail_clk_deps(si_t *sih, osl_t *osh);
extern uint32 si_pmu_rsrc_cb_ready_deps(si_t *sih, osl_t *osh);

extern void si_pmu_otp_power(si_t *sih, osl_t *osh, bool on, uint32* min_res_mask);
extern void si_sdiod_drive_strength_init(si_t *sih, osl_t *osh, uint32 drivestrength);

extern void si_pmu_slow_clk_reinit(si_t *sih, osl_t *osh);
extern void si_pmu_avbtimer_enable(si_t *sih, osl_t *osh, bool set_flag);
extern uint32 si_pmu_dump_pmucap_binary(si_t *sih, uchar *p);
extern uint32 si_pmu_dump_buf_size_pmucap(si_t *sih);
extern int si_pmu_wait_for_steady_state(si_t *sih, osl_t *osh, pmuregs_t *pmu);
extern uint32 si_pmu_wake_bit_offset(si_t *sih);
extern uint32 si_pmu_get_pmutimer(si_t *sih);
extern void si_pmu_set_min_res_mask(si_t *sih, osl_t *osh, uint min_res_mask);
extern void si_pmu_set_mac_rsrc_req(si_t *sih, int macunit);
extern void si_pmu_set_mac_rsrc_req_sc(si_t *sih, osl_t *osh);
extern bool si_pmu_fast_lpo_enable_pcie(si_t *sih);
extern bool si_pmu_fast_lpo_enable_pmu(si_t *sih);
extern uint32 si_cur_pmu_time(si_t *sih);
extern bool si_pmu_cap_fast_lpo(si_t *sih);
extern int si_pmu_fast_lpo_disable(si_t *sih);
extern void si_pmu_dmn1_perst_wakeup(si_t *sih, bool set);
#ifdef BCMPMU_STATS
extern void si_pmustatstimer_init(si_t *sih);
extern void si_pmustatstimer_dump(si_t *sih);
extern void si_pmustatstimer_start(si_t *sih, uint8 timerid);
extern void si_pmustatstimer_stop(si_t *sih, uint8 timerid);
extern void si_pmustatstimer_clear(si_t *sih, uint8 timerid);
extern void si_pmustatstimer_clear_overflow(si_t *sih);
extern uint32 si_pmustatstimer_read(si_t *sih, uint8 timerid);
extern void si_pmustatstimer_cfg_src_num(si_t *sih, uint8 src_num, uint8 timerid);
extern void si_pmustatstimer_cfg_cnt_mode(si_t *sih, uint8 cnt_mode, uint8 timerid);
extern void si_pmustatstimer_int_enable(si_t *sih);
extern void si_pmustatstimer_int_disable(si_t *sih);
#endif /* BCMPMU_STATS */
extern int si_pmu_min_res_set(si_t *sih, osl_t *osh, uint min_mask, bool set);
extern void si_pmu_disable_intr_pwrreq(si_t *sih);

extern void si_pmu_fis_setup(si_t *sih);

extern uint si_pmu_get_mac_rsrc_req_tmr_cnt(si_t *sih);
extern uint si_pmu_get_pmu_interrupt_rcv_cnt(si_t *sih);

extern bool _bcm_pwr_opt_dis;
#define BCM_PWR_OPT_ENAB()	(FALSE)

extern int si_pmu_mem_pwr_off(si_t *sih, int core_idx);
extern int si_pmu_mem_pwr_on(si_t *sih);

#if defined(BT_WLAN_REG_ON_WAR)
#define REG_ON_WAR_PMU_EXT_WAKE_REQ_MASK0_VAL 0x060000CDu

extern void si_pmu_reg_on_war_ext_wake_perst_set(si_t *sih);
extern void si_pmu_reg_on_war_ext_wake_perst_clear(si_t *sih);
#endif /* BT_WLAN_REG_ON_WAR */
#endif /* _hndpmu_h_ */
