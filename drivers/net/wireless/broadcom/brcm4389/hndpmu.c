/*
 * Misc utility routines for accessing PMU corerev specific features
 * of the SiliconBackplane-based Broadcom chips.
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

/**
 * @file
 * Note: this file contains PLL/FLL related functions. A chip can contain multiple PLLs/FLLs.
 * However, in the context of this file the baseband ('BB') PLL/FLL is referred to.
 *
 * Throughout this code, the prefixes 'pmu1_' and 'pmu2_' are used.
 * They refer to different revisions of the PMU (which is at revision 18 @ Apr 25, 2012)
 * pmu1_ marks the transition from PLL to ADFLL (Digital Frequency Locked Loop). It supports
 * fractional frequency generation. pmu2_ does not support fractional frequency generation.
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmdevs.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <hndchipc.h>
#include <hndpmu.h>
#include <hndlhl.h>
#include <sbgci.h>
#ifdef EVENT_LOG_COMPILE
#include <event_log.h>
#endif // endif
#include <sbgci.h>
#include <lpflags.h>

#include "siutils_priv.h"

#ifdef BCM_AVS
#include <bcm_avs.h>
#endif // endif

#define	PMU_ERROR(args)

#ifdef BCMDBG_PMU
#define	PMU_MSG(args)	printf args
#else
#define	PMU_MSG(args)
#endif	/* BCMDBG_MPU */

/* To check in verbose debugging messages not intended
 * to be on except on private builds.
 */
#define	PMU_NONE(args)
#define flags_shift	14

/** contains resource bit positions for a specific chip */
struct rsc_per_chip {
	uint8 ht_avail;
	uint8 macphy_clkavail;
	uint8 ht_start;
	uint8 otp_pu;
	uint8 macphy_aux_clkavail;
	uint8 macphy_scan_clkavail;
	uint8 cb_ready;
	uint8 dig_ready;
};

typedef struct rsc_per_chip rsc_per_chip_t;

#if defined(BCMPMU_STATS) && !defined(BCMPMU_STATS_DISABLED)
bool	_pmustatsenab = TRUE;
#else
bool	_pmustatsenab = FALSE;
#endif /* BCMPMU_STATS */

/* 1MHz lpo enable */
/* PLEASE USE THIS MACRO IN ATTACH PATH ONLY! */
#if defined(BCM_FASTLPO) && !defined(BCM_FASTLPO_DISABLED)
	#define FASTLPO_ENAB()	(TRUE)
#else
	#define FASTLPO_ENAB()	(FALSE)
#endif // endif

/* Disable the power optimization feature */
bool	_bcm_pwr_opt_dis = FALSE;

/**
 * Reads/writes a chipcontrol reg. Performes core switching if required, at function exit the
 * original core is restored. Depending on chip type, read/writes to chipcontrol regs in CC core
 * (older chips) or to chipcontrol regs in PMU core (later chips).
 */
uint32
si_pmu_chipcontrol(si_t *sih, uint reg, uint32 mask, uint32 val)
{
	pmu_corereg(sih, SI_CC_IDX, chipcontrol_addr, ~0, reg);
	return pmu_corereg(sih, SI_CC_IDX, chipcontrol_data, mask, val);
}

/**
 * Reads/writes a voltage regulator (vreg) register. Performes core switching if required, at
 * function exit the original core is restored. Depending on chip type, writes to regulator regs
 * in CC core (older chips) or to regulator regs in PMU core (later chips).
 */
uint32
si_pmu_vreg_control(si_t *sih, uint reg, uint32 mask, uint32 val)
{
	pmu_corereg(sih, SI_CC_IDX, regcontrol_addr, ~0, reg);
	return pmu_corereg(sih, SI_CC_IDX, regcontrol_data, mask, val);
}

/**
 * Reads/writes a PLL control register. Performes core switching if required, at function exit the
 * original core is restored. Depending on chip type, writes to PLL control regs in CC core (older
 * chips) or to PLL control regs in PMU core (later chips).
 */
uint32
si_pmu_pllcontrol(si_t *sih, uint reg, uint32 mask, uint32 val)
{
	pmu_corereg(sih, SI_CC_IDX, pllcontrol_addr, ~0, reg);
	return pmu_corereg(sih, SI_CC_IDX, pllcontrol_data, mask, val);
}

/**
 * Balance between stable SDIO operation and power consumption is achieved using this function.
 * Note that each drive strength table is for a specific VDDIO of the SDIO pads, ideally this
 * function should read the VDDIO itself to select the correct table. For now it has been solved
 * with the 'BCM_SDIO_VDDIO' preprocessor constant.
 *
 * 'drivestrength': desired pad drive strength in mA. Drive strength of 0 requests tri-state (if
 *		    hardware supports this), if no hw support drive strength is not programmed.
 */
void
si_sdiod_drive_strength_init(si_t *sih, osl_t *osh, uint32 drivestrength)
{
	/*
	 * Note:
	 * This function used to set the SDIO drive strength via PMU_CHIPCTL1 for the
	 * 43143, 4330, 4334, 4336, 43362 chips.  These chips are now no longer supported, so
	 * the code has been deleted.
	 * Newer chips have the SDIO drive strength setting via a GCI Chip Control register,
	 * but the bit definitions are chip-specific.  We are keeping this function available
	 * (accessed via DHD 'sdiod_drive' IOVar) in case these newer chips need to provide access.
	 */
	UNUSED_PARAMETER(sih);
	UNUSED_PARAMETER(osh);
	UNUSED_PARAMETER(drivestrength);
}

uint32
si_pmu_wake_bit_offset(si_t *sih)
{
	uint32 wakebit;

	switch (CHIPID(sih->chip)) {
	case BCM4369_CHIP_GRPID:
		wakebit = PMU_CC2_GCI2_WAKE;
		break;
	case BCM4368_CHIP_GRPID:
	case BCM4376_CHIP_GRPID:
	case BCM4378_CHIP_GRPID:
		wakebit = CC2_4378_GCI2WAKE_MASK;
		break;
	case BCM4385_CHIP_GRPID:
	case BCM4387_CHIP_GRPID:
		wakebit = CC2_4387_GCI2WAKE_MASK;
		break;
	case BCM4388_CHIP_GRPID:
	case BCM4389_CHIP_GRPID:
	case BCM4397_CHIP_GRPID:
		wakebit = CC2_4389_GCI2WAKE_MASK;
		break;
	default:
		wakebit = 0;
		ASSERT(0);
		break;
	}

	return wakebit;
}

void si_pmu_set_min_res_mask(si_t *sih, osl_t *osh, uint min_res_mask)
{
	pmuregs_t *pmu;
	uint origidx;

	/* Remember original core before switch to chipc/pmu */
	origidx = si_coreidx(sih);
	if (AOB_ENAB(sih)) {
		pmu = si_setcore(sih, PMU_CORE_ID, 0);
	}
	else {
		pmu = si_setcoreidx(sih, SI_CC_IDX);
	}
	ASSERT(pmu != NULL);

	W_REG(osh, &pmu->min_res_mask, min_res_mask);
	OSL_DELAY(100);

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

bool
si_pmu_cap_fast_lpo(si_t *sih)
{
	return (PMU_REG(sih, core_cap_ext, 0, 0) & PCAP_EXT_USE_MUXED_ILP_CLK_MASK) ? TRUE : FALSE;
}

int
si_pmu_fast_lpo_disable(si_t *sih)
{
	if (!si_pmu_cap_fast_lpo(sih)) {
		PMU_ERROR(("si_pmu_fast_lpo_disable: No Fast LPO capability\n"));
		return BCME_ERROR;
	}

	PMU_REG(sih, pmucontrol_ext,
		PCTL_EXT_FASTLPO_ENAB |
		PCTL_EXT_FASTLPO_SWENAB |
		PCTL_EXT_FASTLPO_PCIE_SWENAB,
		0);
	OSL_DELAY(1000);
	return BCME_OK;
}

/*
* 4389B0/C0 - WL and BT turn on WAR,
* set below bits in PMU chip control 6
* - global bit[195] / bit[3] - enable legacy pmu_wakeup to make
* domain 1 (WL) power request
* - global bit[206] / bit[14] - perst_wake_en
*/
void
si_pmu_dmn1_perst_wakeup(si_t *sih, bool set)
{
	if (PMUREV(sih->pmurev) == 40) {
		if (set) {
			si_pmu_chipcontrol(sih, PMU_CHIPCTL6,
				(PMU_CC6_ENABLE_DMN1_WAKEUP |
				PMU_CC6_ENABLE_PMU_WAKEUP_PERST),
				(PMU_CC6_ENABLE_DMN1_WAKEUP |
				PMU_CC6_ENABLE_PMU_WAKEUP_PERST));
		} else {
			si_pmu_chipcontrol(sih, PMU_CHIPCTL6,
				(PMU_CC6_ENABLE_DMN1_WAKEUP |
				PMU_CC6_ENABLE_PMU_WAKEUP_PERST),
				0);
		}
	}
}

#ifdef BCMPMU_STATS
/*
 * 8 pmu statistics timer default map
 *
 * for CORE_RDY_AUX measure, set as below for timer 6 and 7 instead of CORE_RDY_MAIN.
 *	//core-n active duration : pmu_rsrc_state(CORE_RDY_AUX)
 *	{ SRC_CORE_RDY_AUX, FALSE, TRUE, LEVEL_HIGH},
 *	//core-n active duration : pmu_rsrc_state(CORE_RDY_AUX)
 *	{ SRC_CORE_RDY_AUX, FALSE, TRUE, EDGE_RISE}
 */
static pmu_stats_timer_t pmustatstimer[] = {
	{ SRC_LINK_IN_L12, FALSE, TRUE, PMU_STATS_LEVEL_HIGH},	//link_in_l12
	{ SRC_LINK_IN_L23, FALSE, TRUE, PMU_STATS_LEVEL_HIGH},	//link_in_l23
	{ SRC_PM_ST_IN_D0, FALSE, TRUE, PMU_STATS_LEVEL_HIGH},	//pm_st_in_d0
	{ SRC_PM_ST_IN_D3, FALSE, TRUE, PMU_STATS_LEVEL_HIGH},	//pm_st_in_d3
	//deep-sleep duration : pmu_rsrc_state(XTAL_PU)
	{ SRC_XTAL_PU, FALSE, TRUE, PMU_STATS_LEVEL_LOW},
	//deep-sleep entry count : pmu_rsrc_state(XTAL_PU)
	{ SRC_XTAL_PU, FALSE, TRUE, PMU_STATS_EDGE_FALL},
	//core-n active duration : pmu_rsrc_state(CORE_RDY_MAIN)
	{ SRC_CORE_RDY_MAIN, FALSE, TRUE, PMU_STATS_LEVEL_HIGH},
	//core-n active duration : pmu_rsrc_state(CORE_RDY_MAIN)
	{ SRC_CORE_RDY_MAIN, FALSE, TRUE, PMU_STATS_EDGE_RISE}
};

static void
si_pmustatstimer_update(osl_t *osh, pmuregs_t *pmu, uint8 timerid)
{
	uint32 stats_timer_ctrl;

	W_REG(osh, &pmu->pmu_statstimer_addr, timerid);
	stats_timer_ctrl =
		((pmustatstimer[timerid].src_num << PMU_ST_SRC_SHIFT) &
			PMU_ST_SRC_MASK) |
		((pmustatstimer[timerid].cnt_mode << PMU_ST_CNT_MODE_SHIFT) &
			PMU_ST_CNT_MODE_MASK) |
		((pmustatstimer[timerid].enable << PMU_ST_EN_SHIFT) & PMU_ST_EN_MASK) |
		((pmustatstimer[timerid].int_enable << PMU_ST_INT_EN_SHIFT) & PMU_ST_INT_EN_MASK);
	W_REG(osh, &pmu->pmu_statstimer_ctrl, stats_timer_ctrl);
	W_REG(osh, &pmu->pmu_statstimer_N, 0);
}

void
si_pmustatstimer_int_enable(si_t *sih)
{
	pmuregs_t *pmu;
	uint origidx;
	osl_t *osh = si_osh(sih);

	/* Remember original core before switch to chipc/pmu */
	origidx = si_coreidx(sih);
	if (AOB_ENAB(sih)) {
		pmu = si_setcore(sih, PMU_CORE_ID, 0);
	} else {
		pmu = si_setcoreidx(sih, SI_CC_IDX);
	}
	ASSERT(pmu != NULL);

	OR_REG(osh, &pmu->pmuintmask0, PMU_INT_STAT_TIMER_INT_MASK);

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

void
si_pmustatstimer_int_disable(si_t *sih)
{
	pmuregs_t *pmu;
	uint origidx;
	osl_t *osh = si_osh(sih);

	/* Remember original core before switch to chipc/pmu */
	origidx = si_coreidx(sih);
	if (AOB_ENAB(sih)) {
		pmu = si_setcore(sih, PMU_CORE_ID, 0);
	} else {
		pmu = si_setcoreidx(sih, SI_CC_IDX);
	}
	ASSERT(pmu != NULL);

	AND_REG(osh, &pmu->pmuintmask0, ~PMU_INT_STAT_TIMER_INT_MASK);

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

void
si_pmustatstimer_init(si_t *sih)
{
	pmuregs_t *pmu;
	uint origidx;
	osl_t *osh = si_osh(sih);
	uint32 core_cap_ext;
	uint8 max_stats_timer_num;
	int8 i;

	/* Remember original core before switch to chipc/pmu */
	origidx = si_coreidx(sih);
	if (AOB_ENAB(sih)) {
		pmu = si_setcore(sih, PMU_CORE_ID, 0);
	} else {
		pmu = si_setcoreidx(sih, SI_CC_IDX);
	}
	ASSERT(pmu != NULL);

	core_cap_ext = R_REG(osh, &pmu->core_cap_ext);

	max_stats_timer_num = ((core_cap_ext & PCAP_EXT_ST_NUM_MASK) >> PCAP_EXT_ST_NUM_SHIFT) + 1;

	for (i = 0; i < max_stats_timer_num; i++) {
		si_pmustatstimer_update(osh, pmu, i);
	}

	OR_REG(osh, &pmu->pmuintmask0, PMU_INT_STAT_TIMER_INT_MASK);

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

void
si_pmustatstimer_dump(si_t *sih)
{
	pmuregs_t *pmu;
	uint origidx;
	osl_t *osh = si_osh(sih);
	uint32 core_cap_ext, pmucapabilities, AlpPeriod, ILPPeriod, pmuintmask0, pmuintstatus;
	uint8 max_stats_timer_num, max_stats_timer_src_num;
	uint32 stat_timer_ctrl, stat_timer_N;
	uint8 i;
	uint32 current_time_ms = OSL_SYSUPTIME();

	/* Remember original core before switch to chipc/pmu */
	origidx = si_coreidx(sih);
	if (AOB_ENAB(sih)) {
		pmu = si_setcore(sih, PMU_CORE_ID, 0);
	} else {
		pmu = si_setcoreidx(sih, SI_CC_IDX);
	}
	ASSERT(pmu != NULL);

	pmucapabilities = R_REG(osh, &pmu->pmucapabilities);
	core_cap_ext = R_REG(osh, &pmu->core_cap_ext);
	AlpPeriod = R_REG(osh, &pmu->slowclkperiod);
	ILPPeriod = R_REG(osh, &pmu->ILPPeriod);

	max_stats_timer_num = ((core_cap_ext & PCAP_EXT_ST_NUM_MASK) >>
		PCAP_EXT_ST_NUM_SHIFT) + 1;
	max_stats_timer_src_num = ((core_cap_ext & PCAP_EXT_ST_SRC_NUM_MASK) >>
		PCAP_EXT_ST_SRC_NUM_SHIFT) + 1;

	pmuintstatus = R_REG(osh, &pmu->pmuintstatus);
	pmuintmask0 = R_REG(osh, &pmu->pmuintmask0);

	PMU_ERROR(("si_pmustatstimer_dump : TIME %d\n", current_time_ms));

	PMU_ERROR(("\tMAX Timer Num %d, MAX Source Num %d\n",
		max_stats_timer_num, max_stats_timer_src_num));
	PMU_ERROR(("\tpmucapabilities 0x%8x, core_cap_ext 0x%8x, AlpPeriod 0x%8x, ILPPeriod 0x%8x, "
		"pmuintmask0 0x%8x, pmuintstatus 0x%8x, pmurev %d\n",
		pmucapabilities, core_cap_ext, AlpPeriod, ILPPeriod,
		pmuintmask0, pmuintstatus, PMUREV(sih->pmurev)));

	for (i = 0; i < max_stats_timer_num; i++) {
		W_REG(osh, &pmu->pmu_statstimer_addr, i);
		stat_timer_ctrl = R_REG(osh, &pmu->pmu_statstimer_ctrl);
		stat_timer_N = R_REG(osh, &pmu->pmu_statstimer_N);
		PMU_ERROR(("\t Timer %d : control 0x%8x, %d\n",
			i, stat_timer_ctrl, stat_timer_N));
	}

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

void
si_pmustatstimer_start(si_t *sih, uint8 timerid)
{
	pmuregs_t *pmu;
	uint origidx;
	osl_t *osh = si_osh(sih);

	/* Remember original core before switch to chipc/pmu */
	origidx = si_coreidx(sih);
	if (AOB_ENAB(sih)) {
		pmu = si_setcore(sih, PMU_CORE_ID, 0);
	} else {
		pmu = si_setcoreidx(sih, SI_CC_IDX);
	}
	ASSERT(pmu != NULL);

	pmustatstimer[timerid].enable = TRUE;

	W_REG(osh, &pmu->pmu_statstimer_addr, timerid);
	OR_REG(osh, &pmu->pmu_statstimer_ctrl, PMU_ST_ENAB << PMU_ST_EN_SHIFT);

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

void
si_pmustatstimer_stop(si_t *sih, uint8 timerid)
{
	pmuregs_t *pmu;
	uint origidx;
	osl_t *osh = si_osh(sih);

	/* Remember original core before switch to chipc/pmu */
	origidx = si_coreidx(sih);
	if (AOB_ENAB(sih)) {
		pmu = si_setcore(sih, PMU_CORE_ID, 0);
	} else {
		pmu = si_setcoreidx(sih, SI_CC_IDX);
	}
	ASSERT(pmu != NULL);

	pmustatstimer[timerid].enable = FALSE;

	W_REG(osh, &pmu->pmu_statstimer_addr, timerid);
	AND_REG(osh, &pmu->pmu_statstimer_ctrl, ~(PMU_ST_ENAB << PMU_ST_EN_SHIFT));

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

void
si_pmustatstimer_clear(si_t *sih, uint8 timerid)
{
	pmuregs_t *pmu;
	uint origidx;
	osl_t *osh = si_osh(sih);

	/* Remember original core before switch to chipc/pmu */
	origidx = si_coreidx(sih);
	if (AOB_ENAB(sih)) {
		pmu = si_setcore(sih, PMU_CORE_ID, 0);
	} else {
		pmu = si_setcoreidx(sih, SI_CC_IDX);
	}
	ASSERT(pmu != NULL);

	W_REG(osh, &pmu->pmu_statstimer_addr, timerid);
	W_REG(osh, &pmu->pmu_statstimer_N, 0);

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

void
si_pmustatstimer_clear_overflow(si_t *sih)
{
	uint8 i;
	uint32 core_cap_ext;
	uint8 max_stats_timer_num;
	uint32 timerN;
	pmuregs_t *pmu;
	uint origidx;
	osl_t *osh = si_osh(sih);

	/* Remember original core before switch to chipc/pmu */
	origidx = si_coreidx(sih);
	if (AOB_ENAB(sih)) {
		pmu = si_setcore(sih, PMU_CORE_ID, 0);
	} else {
		pmu = si_setcoreidx(sih, SI_CC_IDX);
	}
	ASSERT(pmu != NULL);

	core_cap_ext = R_REG(osh, &pmu->core_cap_ext);
	max_stats_timer_num = ((core_cap_ext & PCAP_EXT_ST_NUM_MASK) >> PCAP_EXT_ST_NUM_SHIFT) + 1;

	for (i = 0; i < max_stats_timer_num; i++) {
		W_REG(osh, &pmu->pmu_statstimer_addr, i);
		timerN = R_REG(osh, &pmu->pmu_statstimer_N);
		if (timerN == 0xFFFFFFFF) {
			PMU_ERROR(("pmustatstimer overflow clear - timerid : %d\n", i));
			si_pmustatstimer_clear(sih, i);
		}
	}

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

uint32
si_pmustatstimer_read(si_t *sih, uint8 timerid)
{
	pmuregs_t *pmu;
	uint origidx;
	osl_t *osh = si_osh(sih);
	uint32 stats_timer_N;

	/* Remember original core before switch to chipc/pmu */
	origidx = si_coreidx(sih);
	if (AOB_ENAB(sih)) {
		pmu = si_setcore(sih, PMU_CORE_ID, 0);
	} else {
		pmu = si_setcoreidx(sih, SI_CC_IDX);
	}
	ASSERT(pmu != NULL);

	W_REG(osh, &pmu->pmu_statstimer_addr, timerid);
	stats_timer_N = R_REG(osh, &pmu->pmu_statstimer_N);

	/* Return to original core */
	si_setcoreidx(sih, origidx);

	return stats_timer_N;
}

void
si_pmustatstimer_cfg_src_num(si_t *sih, uint8 src_num, uint8 timerid)
{
	pmuregs_t *pmu;
	uint origidx;
	osl_t *osh = si_osh(sih);

	/* Remember original core before switch to chipc/pmu */
	origidx = si_coreidx(sih);
	if (AOB_ENAB(sih)) {
		pmu = si_setcore(sih, PMU_CORE_ID, 0);
	} else {
		pmu = si_setcoreidx(sih, SI_CC_IDX);
	}
	ASSERT(pmu != NULL);

	pmustatstimer[timerid].src_num = src_num;
	si_pmustatstimer_update(osh, pmu, timerid);

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

void
si_pmustatstimer_cfg_cnt_mode(si_t *sih, uint8 cnt_mode, uint8 timerid)
{
	pmuregs_t *pmu;
	uint origidx;
	osl_t *osh = si_osh(sih);

	/* Remember original core before switch to chipc/pmu */
	origidx = si_coreidx(sih);
	if (AOB_ENAB(sih)) {
		pmu = si_setcore(sih, PMU_CORE_ID, 0);
	} else {
		pmu = si_setcoreidx(sih, SI_CC_IDX);
	}
	ASSERT(pmu != NULL);

	pmustatstimer[timerid].cnt_mode = cnt_mode;
	si_pmustatstimer_update(osh, pmu, timerid);

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}
#endif /* BCMPMU_STATS */

/* query the # of mac resource request timers */
uint
si_pmu_get_mac_rsrc_req_tmr_cnt(si_t *sih)
{
	if (PMUREV(sih->pmurev) >= 26) {
		uint32 core_cap_ext = PMU_REG(sih, core_cap_ext, 0, 0);
		uint mac_rsrc_cnt =
		        ((core_cap_ext & PCAP_EXT_MAC_RSRC_REQ_TMR_CNT_MASK) >>
		         PCAP_EXT_MAC_RSRC_REQ_TMR_CNT_SHIFT) + 1;
		return mac_rsrc_cnt;
	}

	return si_numd11coreunits(sih);
}

/* query the # of pmu interrupt recevier */
uint
si_pmu_get_pmu_interrupt_rcv_cnt(si_t *sih)
{
	if (PMUREV(sih->pmurev) >= 26) {
		uint32 core_cap_ext = PMU_REG(sih, core_cap_ext, 0, 0);
		uint pmu_intr_rcvr_cnt =
		        ((core_cap_ext & PCAP_EXT_PMU_INTR_RCVR_CNT_MASK) >>
		         PCAP_EXT_PMU_INTR_RCVR_CNT_SHIFT) + 1;
		return pmu_intr_rcvr_cnt;
	}

	return si_numd11coreunits(sih);
}

int
si_pmu_res_state_pwrsw_main_wait(si_t *sih)
{
	int ret = BCME_OK;

	switch (CHIPID(sih->chip)) {
	case BCM4387_CHIP_GRPID:
		if (PMU_REG(sih, res_state, 0, 0) & PMURES_BIT(RES4387_PWRSW_MAIN)) {
			SPINWAIT((PMU_REG(sih, res_state, 0, 0) &
				PMURES_BIT(RES4387_PWRSW_MAIN)), 10000);
			OSL_DELAY(1000);
		}
		ret = (PMU_REG(sih, res_state, 0, 0) & PMURES_BIT(RES4387_PWRSW_MAIN)) ?
			BCME_ERROR : BCME_OK;
		break;
	default:
		PMU_ERROR(("si_pmu_res_state_pwrsw_main_wait: add support for this chip!\n"));
		OSL_SYS_HALT();
		break;
	}

	return ret;
}

#if defined(BT_WLAN_REG_ON_WAR)
void
si_pmu_reg_on_war_ext_wake_perst_set(si_t *sih)
{
	uint origidx = si_coreidx(sih);
	pmuregs_t *pmu = si_setcore(sih, PMU_CORE_ID, 0);
	osl_t *osh = si_osh(sih);

	if (PMUREV(sih->pmurev) == 40) {
		/*
		 * set PCIEPerstReq (bit-5) as a wake-up source in
		 * ExtWakeMask0 (0x760) register
		 */
		W_REG(osh, &pmu->extwakemask0, PMU_EXT_WAKE_MASK_0_PCIE_PERST);

		/*
		 * configure the wakemask as "common backplane" resources to
		 * be up during wake-up in ExtWakeReqMask0 (0x770) register
		 */
		W_REG(osh, &pmu->extwakereqmask[0], REG_ON_WAR_PMU_EXT_WAKE_REQ_MASK0_VAL);
	}

	si_setcoreidx(sih, origidx);
}

void
si_pmu_reg_on_war_ext_wake_perst_clear(si_t *sih)
{
	uint32 val = 0;
	uint origidx = si_coreidx(sih);
	pmuregs_t *pmu = si_setcore(sih, PMU_CORE_ID, 0);
	osl_t *osh = si_osh(sih);

	if (PMUREV(sih->pmurev) == 40) {
		/* clear all set bits in ExtWakeupStatus (0x744) register */
		val = R_REG(osh, &pmu->extwakeupstatus);
		W_REG(osh, &pmu->extwakeupstatus, val);
	}

	si_setcoreidx(sih, origidx);
}
#endif /* BT_WLAN_REG_ON_WAR */
