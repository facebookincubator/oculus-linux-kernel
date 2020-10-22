/*
 * Misc utility routines for accessing chip-specific features
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

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmdevs.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <sbgci.h>
#ifndef BCMSDIO
#include <pcie_core.h>
#endif // endif
#ifdef BCMPCIEDEV
#include <pcieregsoffs.h>
#include <pciedev.h>
#endif /* BCMPCIEDEV */
#include <pcicfg.h>
#include <sbpcmcia.h>
#include <sbsysmem.h>
#include <sbsocram.h>
#ifdef BCMSDIO
#include <bcmsdh.h>
#include <sdio.h>
#include <sbsdio.h>
#include <sbhnddma.h>
#include <sbsdpcmdev.h>
#include <bcmsdpcm.h>
#endif /* BCMSDIO */
#include <hndpmu.h>

#ifdef BCM_SDRBL
#include <hndcpu.h>
#endif /* BCM_SDRBL */
#ifdef HNDGCI
#include <hndgci.h>
#endif /* HNDGCI */
#include <hndlhl.h>
#include <hndoobr.h>
#include <lpflags.h>
#ifdef BCM_SH_SFLASH
#include <sh_sflash.h>
#endif // endif
#ifdef BCMGCISHM
#include <hnd_gcishm.h>
#endif // endif
#include "siutils_priv.h"
#include "sbhndarm.h"
#include <hndchipc.h>
// MOG-ON: SOCI_NCI_BUS
#ifdef SOCI_NCI_BUS
#include <nci.h>
#endif /* SOCI_NCI_BUS */
// MOG-OFF: SOCI_NCI_BUS

#ifdef SECI_UART
/* Defines the set of GPIOs to be used for SECI UART if not specified in NVRAM */
/* For further details on each ppin functionality please refer to PINMUX table in
 * Top level architecture of BCMXXXX Chip
 */
#define DEFAULT_SECI_UART_PINMUX	0x08090a0b
static bool force_seci_clk = 0;
#endif /* SECI_UART */

#define XTAL_FREQ_26000KHZ		26000
#define XTAL_FREQ_59970KHZ		59970
#define WCI2_UART_RX_BUF_SIZE	64

/**
 * A set of PMU registers is clocked in the ILP domain, which has an implication on register write
 * behavior: if such a register is written, it takes multiple ILP clocks for the PMU block to absorb
 * the write. During that time the 'SlowWritePending' bit in the PMUStatus register is set.
 */
#define PMUREGS_ILP_SENSITIVE(regoff) \
	((regoff) == OFFSETOF(pmuregs_t, pmutimer) || \
	 (regoff) == OFFSETOF(pmuregs_t, pmuwatchdog) || \
	 (regoff) == OFFSETOF(pmuregs_t, res_req_timer))

#define CHIPCREGS_ILP_SENSITIVE(regoff) \
	((regoff) == OFFSETOF(chipcregs_t, pmutimer) || \
	 (regoff) == OFFSETOF(chipcregs_t, pmuwatchdog) || \
	 (regoff) == OFFSETOF(chipcregs_t, res_req_timer))

#define GCI_FEM_CTRL_WAR 0x11111111

#ifndef AXI_TO_VAL
#define AXI_TO_VAL 19
#endif	/* AXI_TO_VAL */

#ifndef AXI_TO_VAL_25
/*
 * Increase BP timeout for fast clock and short PCIe timeouts
 * New timeout: 2 ** 25 cycles
 */
#define AXI_TO_VAL_25	25
#endif /* AXI_TO_VAL_25 */

#define si_srpwr_domain_mask(rval, mask) \
	(((rval) >> SRPWR_STATUS_SHIFT) & (mask))

/* local prototypes */
int32 si_alloc_wrapper(si_info_t *sii);
static si_info_t *si_doattach(si_info_t *sii, uint devid, osl_t *osh, volatile void *regs,
                              uint bustype, void *sdh, char **vars, uint *varsz);
static bool si_buscore_prep(si_info_t *sii, uint bustype, uint devid, void *sdh);
static bool si_buscore_setup(si_info_t *sii, chipcregs_t *cc, uint bustype, uint32 savewin,
	uint *origidx, volatile const void *regs);

static bool si_pmu_is_ilp_sensitive(uint32 idx, uint regoff);

static void si_oob_war_BT_F1(si_t *sih);

#if defined(BCMLTECOEX)
static void si_wci2_rxfifo_intr_handler_process(si_t *sih, uint32 intstatus);
#endif // endif

// MOG-ON: BCMINTERNAL
// MOG-OFF: BCMINTERNAL

/* global variable to indicate reservation/release of gpio's */
static uint32 si_gpioreservation = 0;
/* global flag to prevent shared resources from being initialized multiple times in si_attach() */
static bool si_onetimeinit = FALSE;

#ifdef SR_DEBUG
static const uint32 si_power_island_test_array[] = {
	0x0000, 0x0001, 0x0010, 0x0011,
	0x0100, 0x0101, 0x0110, 0x0111,
	0x1000, 0x1001, 0x1010, 0x1011,
	0x1100, 0x1101, 0x1110, 0x1111
};
#endif /* SR_DEBUG */

/* 4360 pcie2 WAR */
int do_4360_pcie2_war = 0;

/* global kernel resource */
static si_info_t ksii;
static si_cores_info_t ksii_cores_info;

/**
 * Allocate an si handle. This function may be called multiple times.
 *
 * devid - pci device id (used to determine chip#)
 * osh - opaque OS handle
 * regs - virtual address of initial core registers
 * bustype - pci/sb/sdio/etc
 * vars - pointer to a to-be created pointer area for "environment" variables. Some callers of this
 *        function set 'vars' to NULL, making dereferencing of this parameter undesired.
 * varsz - pointer to int to return the size of the vars
 */
si_t *
si_attach(uint devid, osl_t *osh, volatile void *regs,
                       uint bustype, void *sdh, char **vars, uint *varsz)
{
	si_info_t *sii;
	si_cores_info_t *cores_info;
	/* alloc si_info_t */
	/* freed after ucode download for firmware builds */
	if ((sii = MALLOCZ_NOPERSIST(osh, sizeof(si_info_t))) == NULL) {
		SI_ERROR(("si_attach: malloc failed! malloced %d bytes\n", MALLOCED(osh)));
		return (NULL);
	}

#ifdef BCMDVFS
	if (si_dvfs_info_init((si_t *)sii, osh) == NULL) {
		SI_ERROR(("si_dvfs_info_init failed\n"));
		return (NULL);
	}
#endif /* BCMDVFS */

	/* alloc si_cores_info_t */
	if ((cores_info = (si_cores_info_t *)MALLOCZ(osh,
		sizeof(si_cores_info_t))) == NULL) {
		SI_ERROR(("si_attach: malloc failed! malloced %d bytes\n", MALLOCED(osh)));
		MFREE(osh, sii, sizeof(si_info_t));
		return (NULL);
	}
	sii->cores_info = cores_info;

	if (si_doattach(sii, devid, osh, regs, bustype, sdh, vars, varsz) == NULL) {
		MFREE(osh, sii, sizeof(si_info_t));
		MFREE(osh, cores_info, sizeof(si_cores_info_t));
		return (NULL);
	}
	sii->vars = vars ? *vars : NULL;
	sii->varsz = varsz ? *varsz : 0;

#if defined(BCM_SH_SFLASH) && !defined(BCM_SH_SFLASH_DISABLED)
	sh_sflash_attach(osh, (si_t *)sii);
#endif // endif

	return (si_t *)sii;
}

static uint32	wd_msticks;		/**< watchdog timer ticks normalized to ms */

/** Returns the backplane address of the chipcommon core for a particular chip */
uint32
si_enum_base(uint devid)
{
	return SI_ENUM_BASE_DEFAULT;
}

/** generic kernel variant of si_attach(). Is not called for Linux WLAN NIC builds. */
si_t *
si_kattach(osl_t *osh)
{
	static bool ksii_attached = FALSE;
	si_cores_info_t *cores_info;

	if (!ksii_attached) {
		void *regs = NULL;
		const uint device_id = BCM4710_DEVICE_ID; // pick an arbitrary default device_id

		regs = REG_MAP(si_enum_base(device_id), SI_CORE_SIZE); // map physical to virtual
		cores_info = (si_cores_info_t *)&ksii_cores_info;
		ksii.cores_info = cores_info;

		/* Use osh as the deciding factor if the memory management
		 * system has been initialized. Pass non-NULL vars & varsz only
		 * if memory management has been initialized. Otherwise MALLOC()
		 * will fail/crash.
		 */
		ASSERT(osh);
		if (si_doattach(&ksii, device_id, osh, regs,
		                SI_BUS, NULL,
		                osh != SI_OSH ? &(ksii.vars) : NULL,
		                osh != SI_OSH ? &(ksii.varsz) : NULL) == NULL) {
			SI_ERROR(("si_kattach: si_doattach failed\n"));
			REG_UNMAP(regs);
			return NULL;
		}
		REG_UNMAP(regs);

		/* save ticks normalized to ms for si_watchdog_ms() */
		if (PMUCTL_ENAB(&ksii.pub)) {
			/* based on 32KHz ILP clock */
			wd_msticks = 32;
		} else {
			wd_msticks = ALP_CLOCK / 1000;
		}

		ksii_attached = TRUE;
		SI_MSG(("si_kattach done. ccrev = %d, wd_msticks = %d\n",
		        CCREV(ksii.pub.ccrev), wd_msticks));
	}

	return &ksii.pub;
}

static bool
si_buscore_prep(si_info_t *sii, uint bustype, uint devid, void *sdh)
{
	BCM_REFERENCE(sdh);
	BCM_REFERENCE(devid);

#if defined(BCMSDIO) && !defined(BCMSDIOLITE)
	/* As it precedes any backplane access, can't check chipid; but may
	 * be able to qualify with devid if underlying SDIO allows.  But should
	 * be ok for all our SDIO (4318 doesn't support clock and pullup regs,
	 * but the access attempts don't seem to hurt.)  Might elimiante the
	 * the need for ALP for CIS at all if underlying SDIO uses CMD53...
	 */
	if (BUSTYPE(bustype) == SDIO_BUS) {
		int err;
		uint8 clkset;

		/* Try forcing SDIO core to do ALPAvail request only */
		clkset = SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_ALP_AVAIL_REQ;
		bcmsdh_cfg_write(sdh, SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, clkset, &err);
		if (!err) {
			uint8 clkval;

			/* If register supported, wait for ALPAvail and then force ALP */
			clkval = bcmsdh_cfg_read(sdh, SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, NULL);
			if ((clkval & ~SBSDIO_AVBITS) == clkset) {
				SPINWAIT(((clkval = bcmsdh_cfg_read(sdh, SDIO_FUNC_1,
					SBSDIO_FUNC1_CHIPCLKCSR, NULL)), !SBSDIO_ALPAV(clkval)),
					PMU_MAX_TRANSITION_DLY);
				if (!SBSDIO_ALPAV(clkval)) {
					SI_ERROR(("timeout on ALPAV wait, clkval 0x%02x\n",
						clkval));
					return FALSE;
				}
				clkset = SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_FORCE_ALP;
				bcmsdh_cfg_write(sdh, SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR,
					clkset, &err);
				OSL_DELAY(65);
			}
		}

		/* Also, disable the extra SDIO pull-ups */
		bcmsdh_cfg_write(sdh, SDIO_FUNC_1, SBSDIO_FUNC1_SDIOPULLUP, 0, NULL);
	}

#endif /* BCMSDIO && BCMDONGLEHOST && !BCMSDIOLITE */

	return TRUE;
}

/* note: this function is used by dhd */
uint32
si_get_pmu_reg_addr(si_t *sih, uint32 offset)
{
	si_info_t *sii = SI_INFO(sih);
	uint32 pmuaddr = INVALID_ADDR;
	uint origidx = 0;

	SI_MSG(("si_get_pmu_reg_addr: pmu access, offset: %x\n", offset));
	if (!(sii->pub.cccaps & CC_CAP_PMU)) {
		goto done;
	}
	if (AOB_ENAB(&sii->pub)) {
		uint pmucoreidx;
		pmuregs_t *pmu;
		SI_MSG(("si_get_pmu_reg_addr: AOBENAB: %x\n", offset));
		origidx = sii->curidx;
		pmucoreidx = si_findcoreidx(&sii->pub, PMU_CORE_ID, 0);
		pmu = si_setcoreidx(&sii->pub, pmucoreidx);
		/* note: this function is used by dhd and possible 64 bit compilation needs
		 * a cast to (unsigned long) for avoiding a compilation error.
		 */
		pmuaddr = (uint32)(uintptr)((volatile uint8*)pmu + offset);
		si_setcoreidx(sih, origidx);
	} else
		pmuaddr = SI_ENUM_BASE(sih) + offset;

done:
	printf("si_get_pmu_reg_addr: addrRET: %x\n", pmuaddr);
	return pmuaddr;
}

static bool
si_buscore_setup(si_info_t *sii, chipcregs_t *cc, uint bustype, uint32 savewin,
	uint *origidx, volatile const void *regs)
{
	const si_cores_info_t *cores_info = sii->cores_info;
	bool pci, pcie, pcie_gen2 = FALSE;
	uint i;
	uint pciidx, pcieidx, pcirev, pcierev;

#if defined(AXI_TIMEOUTS_NIC) || defined(AXI_TIMEOUTS)
	/* first, enable backplane timeouts */
	si_slave_wrapper_add(&sii->pub);
#endif // endif
	sii->curidx = 0;

	cc = si_setcoreidx(&sii->pub, SI_CC_IDX);
	ASSERT((uintptr)cc);

	/* get chipcommon rev */
	sii->pub.ccrev = (int)si_corerev(&sii->pub);

	/* get chipcommon chipstatus */
	if (CCREV(sii->pub.ccrev) >= 11)
		sii->pub.chipst = R_REG(sii->osh, &cc->chipstatus);

	/* get chipcommon capabilites */
	sii->pub.cccaps = R_REG(sii->osh, &cc->capabilities);
	/* get chipcommon extended capabilities */

	if (CCREV(sii->pub.ccrev) >= 35)
		sii->pub.cccaps_ext = R_REG(sii->osh, &cc->capabilities_ext);

	/* get pmu rev and caps */
	if (sii->pub.cccaps & CC_CAP_PMU) {
		if (AOB_ENAB(&sii->pub)) {
			uint pmucoreidx;
			pmuregs_t *pmu;

			pmucoreidx = si_findcoreidx(&sii->pub, PMU_CORE_ID, 0);
			if (!GOODIDX(pmucoreidx)) {
				SI_ERROR(("si_buscore_setup: si_findcoreidx failed\n"));
				return FALSE;
			}

			pmu = si_setcoreidx(&sii->pub, pmucoreidx);
			sii->pub.pmucaps = R_REG(sii->osh, &pmu->pmucapabilities);
			si_setcoreidx(&sii->pub, SI_CC_IDX);

			sii->pub.gcirev = si_corereg(&sii->pub, GCI_CORE_IDX(&sii->pub),
				GCI_OFFSETOF(&sii->pub, gci_corecaps0), 0, 0) & GCI_CAP0_REV_MASK;

			if (GCIREV(sii->pub.gcirev) >= 9) {
				sii->pub.lhlrev = si_corereg(&sii->pub, GCI_CORE_IDX(&sii->pub),
					OFFSETOF(gciregs_t, lhl_core_capab_adr), 0, 0) &
					LHL_CAP_REV_MASK;
			} else {
				sii->pub.lhlrev = NOREV;
			}

		} else
			sii->pub.pmucaps = R_REG(sii->osh, &cc->pmucapabilities);

		sii->pub.pmurev = sii->pub.pmucaps & PCAP_REV_MASK;
	}

	SI_MSG(("Chipc: rev %d, caps 0x%x, chipst 0x%x pmurev %d, pmucaps 0x%x\n",
		CCREV(sii->pub.ccrev), sii->pub.cccaps, sii->pub.chipst, sii->pub.pmurev,
		sii->pub.pmucaps));

	/* figure out bus/orignal core idx */
	/* note for PCI_BUS the buscoretype variable is setup in ai_scan() */
	if (BUSTYPE(sii->pub.bustype) != PCI_BUS) {
		sii->pub.buscoretype = NODEV_CORE_ID;
	}
	sii->pub.buscorerev = NOREV;
	sii->pub.buscoreidx = BADIDX;

	pci = pcie = FALSE;
	pcirev = pcierev = NOREV;
	pciidx = pcieidx = BADIDX;

	/* This loop can be optimized */
	for (i = 0; i < sii->numcores; i++) {
		uint cid, crev;

		si_setcoreidx(&sii->pub, i);
		cid = si_coreid(&sii->pub);
		crev = si_corerev(&sii->pub);

		/* Display cores found */
		if (CHIPTYPE(sii->pub.socitype) != SOCI_NCI) {
			SI_VMSG(("CORE[%d]: id 0x%x rev %d base 0x%x size:%x regs 0x%p\n",
				i, cid, crev, cores_info->coresba[i], cores_info->coresba_size[i],
				OSL_OBFUSCATE_BUF(cores_info->regs[i])));
		}

		if (BUSTYPE(bustype) == SI_BUS) {
			/* now look at the chipstatus register to figure the pacakge */
			/* this shoudl be a general change to cover all teh chips */
			/* this also shoudl validate the build where the dongle is built */
			/* for SDIO but downloaded on PCIE dev */
#ifdef BCMPCIEDEV_ENABLED
			if (cid == PCIE2_CORE_ID) {
				pcieidx = i;
				pcierev = crev;
				pcie = TRUE;
				pcie_gen2 = TRUE;
			}
#endif // endif
			/* rest fill it up here */

		} else if (BUSTYPE(bustype) == PCI_BUS) {
			if (cid == PCI_CORE_ID) {
				pciidx = i;
				pcirev = crev;
				pci = TRUE;
			} else if ((cid == PCIE_CORE_ID) || (cid == PCIE2_CORE_ID)) {
				pcieidx = i;
				pcierev = crev;
				pcie = TRUE;
				if (cid == PCIE2_CORE_ID)
					pcie_gen2 = TRUE;
			}
		}
#ifdef BCMSDIO
		else if (((BUSTYPE(bustype) == SDIO_BUS) ||
		          (BUSTYPE(bustype) == SPI_BUS)) &&
		         (cid == SDIOD_CORE_ID)) {
			sii->pub.buscorerev = (int16)crev;
			sii->pub.buscoretype = (uint16)cid;
			sii->pub.buscoreidx = (uint16)i;
		}
#endif /* BCMSDIO */

		/* find the core idx before entering this func. */
		if (CHIPTYPE(sii->pub.socitype) == SOCI_NCI) {
			if (regs == sii->curmap) {
				*origidx = i;
			}
		} else {
			/* find the core idx before entering this func. */
			if ((savewin && (savewin == cores_info->coresba[i])) ||
			(regs == cores_info->regs[i])) {
				*origidx = i;
			}
		}
	}

#if defined(PCIE_FULL_DONGLE)
	if (pcie) {
		if (pcie_gen2)
			sii->pub.buscoretype = PCIE2_CORE_ID;
		else
			sii->pub.buscoretype = PCIE_CORE_ID;
		sii->pub.buscorerev = (int16)pcierev;
		sii->pub.buscoreidx = (uint16)pcieidx;
	}
	BCM_REFERENCE(pci);
	BCM_REFERENCE(pcirev);
	BCM_REFERENCE(pciidx);
#else
	if (pci) {
		sii->pub.buscoretype = PCI_CORE_ID;
		sii->pub.buscorerev = (int16)pcirev;
		sii->pub.buscoreidx = (uint16)pciidx;
	} else if (pcie) {
		if (pcie_gen2)
			sii->pub.buscoretype = PCIE2_CORE_ID;
		else
			sii->pub.buscoretype = PCIE_CORE_ID;
		sii->pub.buscorerev = (int16)pcierev;
		sii->pub.buscoreidx = (uint16)pcieidx;
	}
#endif /* defined(PCIE_FULL_DONGLE) */

	SI_VMSG(("Buscore id/type/rev %d/0x%x/%d\n", sii->pub.buscoreidx, sii->pub.buscoretype,
	         sii->pub.buscorerev));

#if defined(BCMSDIO)
	/* Make sure any on-chip ARM is off (in case strapping is wrong), or downloaded code was
	 * already running.
	 */
	if ((BUSTYPE(bustype) == SDIO_BUS) || (BUSTYPE(bustype) == SPI_BUS)) {
		if (si_setcore(&sii->pub, ARM7S_CORE_ID, 0) ||
		    si_setcore(&sii->pub, ARMCM3_CORE_ID, 0))
			si_core_disable(&sii->pub, 0);
	}
#endif /* BCMSDIO && BCMDONGLEHOST */

	/* return to the original core */
	si_setcoreidx(&sii->pub, *origidx);

	return TRUE;
}

uint16
si_chipid(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);

	return (sii->chipnew) ? sii->chipnew : sih->chip;
}

/* CHIP_ID's being mapped here should not be used anywhere else in the code */
static void
si_chipid_fixup(si_t *sih)
{
	si_info_t *sii = SI_INFO(sih);

	ASSERT(sii->chipnew == 0);
	switch (sih->chip) {
		case BCM4377_CHIP_ID:
			sii->chipnew = sih->chip; /* save it */
			sii->pub.chip = BCM4369_CHIP_ID; /* chip class */
		break;
		case BCM4375_CHIP_ID:
			sii->chipnew = sih->chip; /* save it */
			sii->pub.chip = BCM4375_CHIP_ID; /* chip class */
		break;
		case BCM4362_CHIP_ID:
			sii->chipnew = sih->chip; /* save it */
			sii->pub.chip = BCM4362_CHIP_ID; /* chip class */
		break;
		default:
		break;
	}
}

#ifdef AXI_TIMEOUTS_NIC
uint32
si_clear_backplane_to_fast(void *sih, void *addr)
{
	si_t *_sih = DISCARD_QUAL(sih, si_t);

	if (CHIPTYPE(_sih->socitype) == SOCI_AI) {
		return ai_clear_backplane_to_fast(_sih, addr);
	}

	return 0;
}

const si_axi_error_info_t *
si_get_axi_errlog_info(const si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		return (const si_axi_error_info_t *)sih->err_info;
	}

	return NULL;
}

void
si_reset_axi_errlog_info(const si_t *sih)
{
	if (sih->err_info) {
		sih->err_info->count = 0;
	}
}
#endif /* AXI_TIMEOUTS_NIC */

int32
si_alloc_wrapper(si_info_t *sii)
{
	if (sii->osh) {
		sii->axi_wrapper = (axi_wrapper_t *)MALLOCZ(sii->osh,
			(sizeof(axi_wrapper_t) * SI_MAX_AXI_WRAPPERS));

		if (sii->axi_wrapper == NULL) {
			return BCME_NOMEM;
		}
	} else {
		sii->axi_wrapper = NULL;
		return BCME_ERROR;
	}
	return BCME_OK;
}

/**
 * Allocate an si handle. This function may be called multiple times. This function is called by
 * both si_attach() and si_kattach().
 *
 * vars - pointer to a to-be created pointer area for "environment" variables. Some callers of this
 *        function set 'vars' to NULL.
 */
static si_info_t *
si_doattach(si_info_t *sii, uint devid, osl_t *osh, volatile void *regs,
                       uint bustype, void *sdh, char **vars, uint *varsz)
{
	struct si_pub *sih = &sii->pub;
	uint32 w, savewin;
	chipcregs_t *cc;
	char *pvars = NULL;
	uint origidx;
#ifdef NVSRCX
	char *sromvars;
#endif // endif

	ASSERT(GOODREGS(regs));

	savewin = 0;

	sih->buscoreidx = BADIDX;
	sii->device_removed = FALSE;

	sii->curmap = regs;
	sii->sdh = sdh;
	sii->osh = osh;
	sii->second_bar0win = ~0x0;
	sih->enum_base = si_enum_base(devid);

#if defined(AXI_TIMEOUTS_NIC)
	sih->err_info = MALLOCZ(osh, sizeof(si_axi_error_info_t));
	if (sih->err_info == NULL) {
		SI_ERROR(("si_doattach: %zu bytes MALLOC FAILED",
			sizeof(si_axi_error_info_t)));
	}
#endif /* AXI_TIMEOUTS_NIC */

#if defined(AXI_TIMEOUTS_NIC) && defined(__linux__)
	osl_set_bpt_cb(osh, (void *)si_clear_backplane_to_fast, (void *)sih);
#endif	/* AXI_TIMEOUTS_NIC && linux */

	/* check to see if we are a si core mimic'ing a pci core */
	if ((bustype == PCI_BUS) &&
	    (OSL_PCI_READ_CONFIG(sii->osh, PCI_SPROM_CONTROL, sizeof(uint32)) == 0xffffffff)) {
		SI_ERROR(("si_doattach: incoming bus is PCI but it's a lie, switching to SI "
		          "devid:0x%x\n", devid));
		bustype = SI_BUS;
	}

	/* find Chipcommon address */
	if (bustype == PCI_BUS) {
		savewin = OSL_PCI_READ_CONFIG(sii->osh, PCI_BAR0_WIN, sizeof(uint32));
		if (!GOODCOREADDR(savewin, SI_ENUM_BASE(sih)))
			savewin = SI_ENUM_BASE(sih);
		OSL_PCI_WRITE_CONFIG(sii->osh, PCI_BAR0_WIN, 4, SI_ENUM_BASE(sih));
		if (!regs)
			return NULL;
		cc = (chipcregs_t *)regs;
#ifdef BCMSDIO
	} else if ((bustype == SDIO_BUS) || (bustype == SPI_BUS)) {
		cc = (chipcregs_t *)sii->curmap;
#endif // endif
	} else {
		cc = (chipcregs_t *)REG_MAP(SI_ENUM_BASE(sih), SI_CORE_SIZE);
	}

	sih->bustype = (uint16)bustype;
#ifdef BCMBUSTYPE
	if (bustype != BUSTYPE(bustype)) {
		SI_ERROR(("si_doattach: bus type %d does not match configured bus type %d\n",
			bustype, BUSTYPE(bustype)));
		return NULL;
	}
#endif // endif

	/* bus/core/clk setup for register access */
	if (!si_buscore_prep(sii, bustype, devid, sdh)) {
		SI_ERROR(("si_doattach: si_core_clk_prep failed %d\n", bustype));
		return NULL;
	}

	/* ChipID recognition.
	*   We assume we can read chipid at offset 0 from the regs arg.
	*   If we add other chiptypes (or if we need to support old sdio hosts w/o chipcommon),
	*   some way of recognizing them needs to be added here.
	*/
	if (!cc) {
		SI_ERROR(("si_doattach: chipcommon register space is null \n"));
		return NULL;
	}
	w = R_REG(osh, &cc->chipid);
	/* plz refer to RB:13157 */
	if ((w & 0xfffff) == 148277) w -= 65532;
	sih->socitype = (w & CID_TYPE_MASK) >> CID_TYPE_SHIFT;
	/* Might as wll fill in chip id rev & pkg */
	sih->chip = w & CID_ID_MASK;
	sih->chiprev = (w & CID_REV_MASK) >> CID_REV_SHIFT;
	sih->chippkg = (w & CID_PKG_MASK) >> CID_PKG_SHIFT;

	si_chipid_fixup(sih);

	sih->issim = IS_SIM(sih->chippkg);

	if (MULTIBP_CAP(sih)) {
		sih->_multibp_enable = TRUE;
	}

	/* scan for cores */
	if (CHIPTYPE(sii->pub.socitype) == SOCI_SB) {
		SI_MSG(("Found chip type SB (0x%08x)\n", w));
		sb_scan(&sii->pub, regs, devid);
	} else if ((CHIPTYPE(sii->pub.socitype) == SOCI_AI) ||
		(CHIPTYPE(sii->pub.socitype) == SOCI_NAI) ||
		(CHIPTYPE(sii->pub.socitype) == SOCI_DVTBUS)) {

		if (CHIPTYPE(sii->pub.socitype) == SOCI_AI)
			SI_MSG(("Found chip type AI (0x%08x)\n", w));
		else if (CHIPTYPE(sii->pub.socitype) == SOCI_NAI)
			SI_MSG(("Found chip type NAI (0x%08x)\n", w));
		else
			SI_MSG(("Found chip type DVT (0x%08x)\n", w));
		/* pass chipc address instead of original core base */
		if ((si_alloc_wrapper(sii)) != BCME_OK) {
			SI_ERROR(("si_doattach: %zu  bytes MALLOC Failed",
				(sizeof(axi_wrapper_t) * SI_MAX_AXI_WRAPPERS)));
			goto exit;
		}
		ai_scan(&sii->pub, (void *)(uintptr)cc, devid);
	} else if (CHIPTYPE(sii->pub.socitype) == SOCI_NCI) {
		sii->nci_info = nci_init(sih, (void*)(uintptr)cc, sih->bustype);
		if (sii->nci_info == NULL) {
			SI_ERROR(("si_doattach: NCI Malloc Failed"));
			goto exit;
		}

		if ((si_alloc_wrapper(sii)) != BCME_OK) {
			SI_ERROR(("si_doattach: %zu  bytes MALLOC Failed",
				(sizeof(axi_wrapper_t) * SI_MAX_AXI_WRAPPERS)));
			goto exit;
		}
		if ((sii->numcores = nci_scan(sih)) == 0u) {
			SI_ERROR(("si_doattach: NCI Scan Failed\n"));
			goto exit;
		} else {
			nci_dump_erom(sii->nci_info);
		}
	} else if (CHIPTYPE(sii->pub.socitype) == SOCI_UBUS) {
		SI_MSG(("Found chip type UBUS (0x%08x), chip id = 0x%4x\n", w, sih->chip));
		/* pass chipc address instead of original core base */
		ub_scan(&sii->pub, (void *)(uintptr)cc, devid);
	} else {
		SI_ERROR(("Found chip of unknown type (0x%08x)\n", w));
		return NULL;
	}

	/* no cores found, bail out */
	if (sii->numcores == 0) {
		SI_ERROR(("si_doattach: could not find any cores\n"));
		return NULL;
	}
	/* bus/core/clk setup */
	origidx = SI_CC_IDX;
	if (!si_buscore_setup(sii, cc, bustype, savewin, &origidx, regs)) {
		SI_ERROR(("si_doattach: si_buscore_setup failed\n"));
		goto exit;
	}

	/* Set the clkdiv2 divisor bits (2:0) to 0x4 if srom is present */
	if (bustype == SI_BUS) {
		uint32 clkdiv2, sromprsnt, capabilities, srom_supported;
		capabilities =	R_REG(osh, &cc->capabilities);
		srom_supported = capabilities & SROM_SUPPORTED;
		if (srom_supported) {
			sromprsnt = R_REG(osh, &cc->sromcontrol);
			sromprsnt = sromprsnt & SROM_PRSNT_MASK;
			if (sromprsnt) {
				/* SROM clock come from backplane clock/div2. Must <= 1Mhz */
				clkdiv2 = (R_REG(osh, &cc->clkdiv2) & ~CLKD2_SROM);
				clkdiv2 |= CLKD2_SROMDIV_192;
				W_REG(osh, &cc->clkdiv2, clkdiv2);
			}
		}
	}

	if (bustype == PCI_BUS) {

	}
#ifdef BCM_SDRBL
	/* 4360 rom bootloader in PCIE case, if the SDR is enabled, But preotection is
	 * not turned on, then we want to hold arm in reset.
	 * Bottomline: In sdrenable case, we allow arm to boot only when protection is
	 * turned on.
	 */
	if (CHIP_HOSTIF_PCIE(&(sii->pub))) {
		uint32 sflags = si_arm_sflags(&(sii->pub));

		/* If SDR is enabled but protection is not turned on
		* then we want to force arm to WFI.
		*/
		if ((sflags & (SISF_SDRENABLE | SISF_TCMPROT)) == SISF_SDRENABLE) {
			disable_arm_irq();
			while (1) {
				hnd_cpu_wait(sih);
			}
		}
	}
#endif /* BCM_SDRBL */

	pvars = NULL;
	BCM_REFERENCE(pvars);

	if (!si_onetimeinit) {

		if (CCREV(sii->pub.ccrev) >= 20) {
			uint32 gpiopullup = 0, gpiopulldown = 0;
			cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
			ASSERT(cc != NULL);

			W_REG(osh, &cc->gpiopullup, gpiopullup);
			W_REG(osh, &cc->gpiopulldown, gpiopulldown);
			si_setcoreidx(sih, origidx);
		}

#if defined(BT_WLAN_REG_ON_WAR)
	/*
	 * 4389B0/C0 - WLAN and BT turn on WAR - synchronize WLAN and BT firmware using GCI
	 * semaphore - THREAD_0_GCI_SEM_3_ID to ensure that simultaneous register accesses
	 * does not occur. The WLAN firmware will acquire the semaphore just to ensure that
	 * if BT firmware is already executing the WAR, then wait until it finishes.
	 * In BT firmware checking for WL_REG_ON status is sufficient to decide whether
	 * to apply the WAR or not (i.e, WLAN is turned ON/OFF).
	 */
	if ((hnd_gcisem_acquire(GCI_BT_WLAN_REG_ON_WAR_SEM, TRUE,
			GCI_BT_WLAN_REG_ON_WAR_SEM_TIMEOUT) != BCME_OK)) {
		SI_ERROR(("Failed to get GCI WLAN/BT REG_ON WAR semaphore...\n"));
		hnd_gcisem_set_err(GCI_BT_WLAN_REG_ON_WAR_SEM);
		goto exit;
	}
	if ((hnd_gcisem_release(GCI_BT_WLAN_REG_ON_WAR_SEM) != BCME_OK)) {
		SI_ERROR(("Failed to release GCI WLAN/BT REG_ON WAR semaphore...\n"));
		hnd_gcisem_set_err(GCI_BT_WLAN_REG_ON_WAR_SEM);
		goto exit;
	}
#endif /* BT_WLAN_REG_ON_WAR */

		/* Skip PMU initialization from the Dongle Host.
		 * Firmware will take care of it when it comes up.
		 */
	}

	/* clear any previous epidiag-induced target abort */
	ASSERT(!si_taclear(sih, FALSE));

#if defined(BCMPMU_STATS) && !defined(BCMPMU_STATS_DISABLED)
	si_pmustatstimer_init(sih);
#endif /* BCMPMU_STATS */

#ifdef BOOTLOADER_CONSOLE_OUTPUT
	/* Enable console prints */
	si_muxenab(sii, 3);
#endif // endif

	if (((PCIECOREREV(sih->buscorerev) == 66) || (PCIECOREREV(sih->buscorerev) == 68)) &&
		CST4378_CHIPMODE_BTOP(sih->chipst)) {
		/*
		 * HW4378-413 :
		 * BT oob connections for pcie function 1 seen at oob_ain[5] instead of oob_ain[1]
		 */
		si_oob_war_BT_F1(sih);
	}

	return (sii);

exit:

	return NULL;
}

/** may be called with core in reset */
void
si_detach(si_t *sih)
{
	si_info_t *sii = SI_INFO(sih);
	si_cores_info_t *cores_info = (si_cores_info_t *)sii->cores_info;
	uint idx;

#ifdef BCM_SH_SFLASH
	if (BCM_SH_SFLASH_ENAB()) {
		sh_sflash_detach(sii->osh, sih);
	}
#endif // endif
	if (BUSTYPE(sih->bustype) == SI_BUS) {
		if (CHIPTYPE(sii->pub.socitype) == SOCI_NCI) {
			if (sii->nci_info) {
				nci_uninit(sii->nci_info);
				sii->nci_info = NULL;
			}
		}
		for (idx = 0; idx < SI_MAXCORES; idx++) {
			if (cores_info->regs[idx]) {
				REG_UNMAP(cores_info->regs[idx]);
				cores_info->regs[idx] = NULL;
			}
		}
	}

#if !defined(BCMBUSTYPE) || (BCMBUSTYPE == SI_BUS)
	if (cores_info != &ksii_cores_info)
#endif	/* !BCMBUSTYPE || (BCMBUSTYPE == SI_BUS) */
		MFREE(sii->osh, cores_info, sizeof(si_cores_info_t));

#if defined(AXI_TIMEOUTS_NIC)
	if (sih->err_info) {
		MFREE(sii->osh, sih->err_info, sizeof(si_axi_error_info_t));
		sii->pub.err_info = NULL;
	}
#endif /* AXI_TIMEOUTS_NIC */

	if (sii->axi_wrapper) {
		MFREE(sii->osh, sii->axi_wrapper,
			(sizeof(axi_wrapper_t) * SI_MAX_AXI_WRAPPERS));
	}

#ifdef BCMDVFS
	si_dvfs_info_deinit(sih, sii->osh);
#endif /* BCMDVFS */

#if !defined(BCMBUSTYPE) || (BCMBUSTYPE == SI_BUS)
	if (sii != &ksii)
#endif	/* !BCMBUSTYPE || (BCMBUSTYPE == SI_BUS) */
		MFREE(sii->osh, sii, sizeof(si_info_t));
}

void *
si_osh(si_t *sih)
{
	const si_info_t *sii;

	sii = SI_INFO(sih);
	return sii->osh;
}

void
si_setosh(si_t *sih, osl_t *osh)
{
	si_info_t *sii;

	sii = SI_INFO(sih);
	if (sii->osh != NULL) {
		SI_ERROR(("osh is already set....\n"));
		ASSERT(!sii->osh);
	}
	sii->osh = osh;
}

/** register driver interrupt disabling and restoring callback functions */
void
si_register_intr_callback(si_t *sih, void *intrsoff_fn, void *intrsrestore_fn,
                          void *intrsenabled_fn, void *intr_arg)
{
	si_info_t *sii = SI_INFO(sih);
	si_cores_info_t *cores_info = (si_cores_info_t *)sii->cores_info;
	sii->intr_arg = intr_arg;
	sii->intrsoff_fn = (si_intrsoff_t)intrsoff_fn;
	sii->intrsrestore_fn = (si_intrsrestore_t)intrsrestore_fn;
	sii->intrsenabled_fn = (si_intrsenabled_t)intrsenabled_fn;
	/* save current core id.  when this function called, the current core
	 * must be the core which provides driver functions(il, et, wl, etc.)
	 */
	sii->dev_coreid = cores_info->coreid[sii->curidx];
}

void
si_deregister_intr_callback(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);
	sii->intrsoff_fn = NULL;
	sii->intrsrestore_fn = NULL;
	sii->intrsenabled_fn = NULL;
}

uint
si_intflag(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);

	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_intflag(sih);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		return R_REG(sii->osh, ((uint32 *)(uintptr)
			    (sii->oob_router + OOB_STATUSA)));
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_intflag(sih);
	else {
		ASSERT(0);
		return 0;
	}
}

uint
si_flag(si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_flag(sih);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_flag(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_flag(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_flag(sih);
	else {
		ASSERT(0);
		return 0;
	}
}

uint
si_flag_alt(const si_t *sih)
{
	if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
	(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
	(CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_flag_alt(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_flag_alt(sih);
	else {
		ASSERT(0);
		return 0;
	}
}

void
si_setint(const si_t *sih, int siflag)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		sb_setint(sih, siflag);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		ai_setint(sih, siflag);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		ub_setint(sih, siflag);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		nci_setint(sih, siflag);
	else
		ASSERT(0);
}

uint32
si_oobr_baseaddr(const si_t *sih, bool second)
{
	const si_info_t *sii = SI_INFO(sih);

	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return 0;
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		return (second ? sii->oob_router1 : sii->oob_router);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_oobr_baseaddr(sih, second);
	else {
		ASSERT(0);
		return 0;
	}
}

uint
si_coreid(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (const si_cores_info_t *)sii->cores_info;
	if (CHIPTYPE(sii->pub.socitype) == SOCI_NCI) {
		return nci_coreid(sih, sii->curidx);
	} else
	{
		return cores_info->coreid[sii->curidx];
	}
}

uint
si_coreidx(const si_t *sih)
{
	const si_info_t *sii;

	sii = SI_INFO(sih);
	return sii->curidx;
}

uint
si_get_num_cores(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	return sii->numcores;
}

volatile void *
si_d11_switch_addrbase(si_t *sih, uint coreunit)
{
	return si_setcore(sih,  D11_CORE_ID, coreunit);
}

/** return the core-type instantiation # of the current core */
uint
si_coreunit(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (const si_cores_info_t *)sii->cores_info;
	uint idx;
	uint coreid;
	uint coreunit;
	uint i;

	if (CHIPTYPE(sii->pub.socitype) == SOCI_NCI) {
		return nci_coreunit(sih);
	}

	coreunit = 0;

	idx = sii->curidx;

	ASSERT(GOODREGS(sii->curmap));
	coreid = si_coreid(sih);

	/* count the cores of our type */
	for (i = 0; i < idx; i++)
		if (cores_info->coreid[i] == coreid)
			coreunit++;

	return (coreunit);
}

uint
si_corevendor(const si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_corevendor(sih);
		else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_corevendor(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_corevendor(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_corevendor(sih);
	else {
		ASSERT(0);
		return 0;
	}
}

bool
si_backplane64(const si_t *sih)
{
	return ((sih->cccaps & CC_CAP_BKPLN64) != 0);
}

uint
si_corerev(const si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_corerev(sih);
		else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_corerev(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_corerev(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_corerev(sih);
	else {
		ASSERT(0);
		return 0;
	}
}

uint
si_corerev_minor(const si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		return ai_corerev_minor(sih);
	}
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_corerev_minor(sih);
	else {
		return 0;
	}
}

/* return index of coreid or BADIDX if not found */
uint
si_findcoreidx(const si_t *sih, uint coreid, uint coreunit)
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (const si_cores_info_t *)sii->cores_info;
	uint found;
	uint i;

	if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		return nci_findcoreidx(sih, coreid, 0u);
	}

	found = 0;

	for (i = 0; i < sii->numcores; i++) {
		if (cores_info->coreid[i] == coreid) {
			if (found == coreunit)
				return (i);
			found++;
		}
	}

	return (BADIDX);
}

/* return the coreid of the core at index */
uint
si_findcoreid(const si_t *sih, uint coreidx)
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = sii->cores_info;

	if (coreidx >= sii->numcores) {
		return NODEV_CORE_ID;
	}
	if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		return nci_coreid(sih, coreidx);
	}
	return cores_info->coreid[coreidx];
}

/** return total coreunit of coreid or zero if not found */
uint
si_numcoreunits(const si_t *sih, uint coreid)
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (si_cores_info_t *)sii->cores_info;
	uint found = 0;
	uint i;

	if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		return nci_numcoreunits(sih, coreid);
	}
	for (i = 0; i < sii->numcores; i++) {
		if (cores_info->coreid[i] == coreid) {
			found++;
		}
	}

	return found;
}

/** return total D11 coreunits */
uint
BCMRAMFN(si_numd11coreunits)(const si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		return nci_numcoreunits(sih, D11_CORE_ID);
	}
	return si_numcoreunits(sih, D11_CORE_ID);
}

/** return list of found cores */
uint
si_corelist(const si_t *sih, uint coreid[])
{
	const si_info_t *sii = SI_INFO(sih);
	const si_cores_info_t *cores_info = (const si_cores_info_t *)sii->cores_info;

	if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		return nci_corelist(sih, coreid);
	}
	memcpy_s(coreid, (sii->numcores * sizeof(uint)), cores_info->coreid,
		(sii->numcores * sizeof(uint)));
	return (sii->numcores);
}

/** return current wrapper mapping */
void *
si_wrapperregs(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);

	ASSERT(GOODREGS(sii->curwrap));

	return (sii->curwrap);
}

/** return current register mapping */
volatile void *
si_coreregs(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);

	ASSERT(GOODREGS(sii->curmap));

	return (sii->curmap);
}

/**
 * This function changes logical "focus" to the indicated core;
 * must be called with interrupts off.
 * Moreover, callers should keep interrupts off during switching out of and back to d11 core
 */
volatile void *
si_setcore(si_t *sih, uint coreid, uint coreunit)
{
	uint idx;

	idx = si_findcoreidx(sih, coreid, coreunit);
	if (!GOODIDX(idx)) {
		return (NULL);
	}

	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_setcoreidx(sih, idx);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_setcoreidx(sih, idx);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_setcoreidx(sih, idx);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_setcoreidx(sih, idx, 0u);
	else {
		ASSERT(0);
		return NULL;
	}
}

volatile void *
si_setcoreidx(si_t *sih, uint coreidx)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_setcoreidx(sih, coreidx);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_setcoreidx(sih, coreidx);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_setcoreidx(sih, coreidx);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_setcoreidx(sih, coreidx, 0u);
	else {
		ASSERT(0);
		return NULL;
	}
}

/** Turn off interrupt as required by sb_setcore, before switch core */
volatile void *
si_switch_core(si_t *sih, uint coreid, uint *origidx, bcm_int_bitmask_t *intr_val)
{
	volatile void *cc;
	si_info_t *sii = SI_INFO(sih);

	if (SI_FAST(sii)) {
		/* Overloading the origidx variable to remember the coreid,
		 * this works because the core ids cannot be confused with
		 * core indices.
		 */
		*origidx = coreid;
		if (coreid == CC_CORE_ID)
			return (volatile void *)CCREGS_FAST(sii);
		else if (coreid == BUSCORETYPE(sih->buscoretype))
			return (volatile void *)PCIEREGS(sii);
	}
	INTR_OFF(sii, intr_val);
	*origidx = sii->curidx;
	cc = si_setcore(sih, coreid, 0);
	ASSERT(cc != NULL);

	return cc;
}

/* restore coreidx and restore interrupt */
void
si_restore_core(si_t *sih, uint coreid, bcm_int_bitmask_t *intr_val)
{
	si_info_t *sii = SI_INFO(sih);

	if (SI_FAST(sii) && ((coreid == CC_CORE_ID) || (coreid == BUSCORETYPE(sih->buscoretype))))
		return;

	si_setcoreidx(sih, coreid);
	INTR_RESTORE(sii, intr_val);
}

/* Switch to particular core and get corerev */
#ifdef USE_NEW_COREREV_API
uint
si_corerev_ext(si_t *sih, uint coreid, uint coreunit)
{
	uint coreidx;
	uint corerev;

	coreidx = si_coreidx(sih);
	(void)si_setcore(sih, coreid, coreunit);

	corerev = si_corerev(sih);

	si_setcoreidx(sih, coreidx);
	return corerev;
}
#else
uint si_get_corerev(si_t *sih, uint core_id)
{
	uint corerev, orig_coreid;
	bcm_int_bitmask_t intr_val;

	si_switch_core(sih, core_id, &orig_coreid, &intr_val);
	corerev = si_corerev(sih);
	si_restore_core(sih, orig_coreid, &intr_val);
	return corerev;
}
#endif /* !USE_NEW_COREREV_API */

int
si_numaddrspaces(const si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_numaddrspaces(sih);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_numaddrspaces(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_numaddrspaces(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_numaddrspaces(sih);
	else {
		ASSERT(0);
		return 0;
	}
}

/* Return the address of the nth address space in the current core
 * Arguments:
 * sih : Pointer to struct si_t
 * spidx : slave port index
 * baidx : base address index
 */

uint32
si_addrspace(const si_t *sih, uint spidx, uint baidx)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_addrspace(sih, baidx);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_addrspace(sih, spidx, baidx);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_addrspace(sih, baidx);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_addrspace(sih, spidx, baidx);
	else {
		ASSERT(0);
		return 0;
	}
}

/* Return the size of the nth address space in the current core
 * Arguments:
 * sih : Pointer to struct si_t
 * spidx : slave port index
 * baidx : base address index
 */
uint32
si_addrspacesize(const si_t *sih, uint spidx, uint baidx)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_addrspacesize(sih, baidx);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_addrspacesize(sih, spidx, baidx);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_addrspacesize(sih, baidx);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_addrspacesize(sih, spidx, baidx);
	else {
		ASSERT(0);
		return 0;
	}
}

void
si_coreaddrspaceX(const si_t *sih, uint asidx, uint32 *addr, uint32 *size)
{
	/* Only supported for SOCI_AI */
	if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		ai_coreaddrspaceX(sih, asidx, addr, size);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		nci_coreaddrspaceX(sih, asidx, addr, size);
	else
		*size = 0;
}

uint32
si_core_cflags(const si_t *sih, uint32 mask, uint32 val)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_core_cflags(sih, mask, val);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_core_cflags(sih, mask, val);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_core_cflags(sih, mask, val);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_core_cflags(sih, mask, val);
	else {
		ASSERT(0);
		return 0;
	}
}

void
si_core_cflags_wo(const si_t *sih, uint32 mask, uint32 val)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		sb_core_cflags_wo(sih, mask, val);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		ai_core_cflags_wo(sih, mask, val);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		ub_core_cflags_wo(sih, mask, val);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		nci_core_cflags_wo(sih, mask, val);
	else
		ASSERT(0);
}

uint32
si_core_sflags(const si_t *sih, uint32 mask, uint32 val)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_core_sflags(sih, mask, val);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_core_sflags(sih, mask, val);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_core_sflags(sih, mask, val);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_core_sflags(sih, mask, val);
	else {
		ASSERT(0);
		return 0;
	}
}

void
si_commit(si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		sb_commit(sih);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		;
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		;
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		;
	else {
		ASSERT(0);
	}
}

bool
si_iscoreup(const si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_iscoreup(sih);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_iscoreup(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_iscoreup(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_iscoreup(sih);
	else {
		ASSERT(0);
		return FALSE;
	}
}

/** Caller should make sure it is on the right core, before calling this routine */
uint
si_wrapperreg(const si_t *sih, uint32 offset, uint32 mask, uint32 val)
{
	/* only for AI back plane chips */
	if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		return (ai_wrap_reg(sih, offset, mask, val));
	else if	(CHIPTYPE(sih->socitype) == SOCI_NCI)
		return (nci_get_wrap_reg(sih, offset, mask, val));
	return 0;
}
/* si_backplane_access is used to read full backplane address from host for PCIE FD
 * it uses secondary bar-0 window which lies at an offset of 16K from primary bar-0
 * Provides support for read/write of 1/2/4 bytes of backplane address
 * Can be used to read/write
 *	1. core regs
 *	2. Wrapper regs
 *	3. memory
 *	4. BT area
 * For accessing any 32 bit backplane address, [31 : 12] of backplane should be given in "region"
 * [11 : 0] should be the "regoff"
 * for reading  4 bytes from reg 0x200 of d11 core use it like below
 * : si_backplane_access(sih, 0x18001000, 0x200, 4, 0, TRUE)
 */
static int si_backplane_addr_sane(uint addr, uint size)
{
	int bcmerror = BCME_OK;

	/* For 2 byte access, address has to be 2 byte aligned */
	if (size == 2) {
		if (addr & 0x1) {
			bcmerror = BCME_ERROR;
		}
	}
	/* For 4 byte access, address has to be 4 byte aligned */
	if (size == 4) {
		if (addr & 0x3) {
			bcmerror = BCME_ERROR;
		}
	}

// MOG-ON: BCMINTERNAL
// MOG-OFF: BCMINTERNAL

	return bcmerror;
}

void
si_invalidate_second_bar0win(si_t *sih)
{
	si_info_t *sii = SI_INFO(sih);
	sii->second_bar0win = ~0x0;
}

int
si_backplane_access(si_t *sih, uint addr, uint size, uint *val, bool read)
{
	volatile uint32 *r = NULL;
	uint32 region = 0;
	si_info_t *sii = SI_INFO(sih);

	/* Valid only for pcie bus */
	if (BUSTYPE(sih->bustype) != PCI_BUS) {
		SI_ERROR(("Valid only for pcie bus \n"));
		return BCME_ERROR;
	}
	if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		return nci_backplane_access(sih, addr, size, val, read);
	}

	/* Split adrr into region and address offset */
	region = (addr & (0xFFFFF << 12));
	addr = addr & 0xFFF;

	/* check for address and size sanity */
	if (si_backplane_addr_sane(addr, size) != BCME_OK)
		return BCME_ERROR;

	/* Update window if required */
	if (sii->second_bar0win != region) {
		OSL_PCI_WRITE_CONFIG(sii->osh, PCIE2_BAR0_CORE2_WIN, 4, region);
		sii->second_bar0win = region;
	}

	/* Estimate effective address
	 * sii->curmap   : bar-0 virtual address
	 * PCI_SECOND_BAR0_OFFSET  : secondar bar-0 offset
	 * regoff : actual reg offset
	 */
	r = (volatile uint32 *)((volatile char *)sii->curmap + PCI_SECOND_BAR0_OFFSET + addr);

	SI_VMSG(("si curmap %p  region %x regaddr %x effective addr %p READ %d\n",
		(volatile char*)sii->curmap, region, addr, r, read));

	switch (size) {
		case sizeof(uint8) :
			if (read)
				*val = R_REG(sii->osh, (volatile uint8*)r);
			else
				W_REG(sii->osh, (volatile uint8*)r, *val);
			break;
		case sizeof(uint16) :
			if (read)
				*val = R_REG(sii->osh, (volatile uint16*)r);
			else
				W_REG(sii->osh, (volatile uint16*)r, *val);
			break;
		case sizeof(uint32) :
			if (read)
				*val = R_REG(sii->osh, (volatile uint32*)r);
			else
				W_REG(sii->osh, (volatile uint32*)r, *val);
			break;
		default :
			SI_ERROR(("Invalid  size %d \n", size));
			return (BCME_ERROR);
			break;
	}

	return (BCME_OK);
}

// MOG-ON: BCMINTERNAL
// MOG-OFF: BCMINTERNAL

uint
si_corereg(si_t *sih, uint coreidx, uint regoff, uint mask, uint val)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_corereg(sih, coreidx, regoff, mask, val);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_corereg(sih, coreidx, regoff, mask, val);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_corereg(sih, coreidx, regoff, mask, val);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_corereg(sih, coreidx, regoff, mask, val);
	else {
		ASSERT(0);
		return 0;
	}
}

uint
si_corereg_writeonly(si_t *sih, uint coreidx, uint regoff, uint mask, uint val)
{
	if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
		return nci_corereg_writeonly(sih, coreidx, regoff, mask, val);
	} else
	{
		return ai_corereg_writeonly(sih, coreidx, regoff, mask, val);
	}
}

/** ILP sensitive register access needs special treatment to avoid backplane stalls */
bool si_pmu_is_ilp_sensitive(uint32 idx, uint regoff)
{
	if (idx == SI_CC_IDX) {
		if (CHIPCREGS_ILP_SENSITIVE(regoff))
			return TRUE;
	} else if (PMUREGS_ILP_SENSITIVE(regoff)) {
		return TRUE;
	}

	return FALSE;
}

/** 'idx' should refer either to the chipcommon core or the PMU core */
uint
si_pmu_corereg(si_t *sih, uint32 idx, uint regoff, uint mask, uint val)
{
	int pmustatus_offset;

	/* prevent backplane stall on double write to 'ILP domain' registers in the PMU */
	if (mask != 0 && PMUREV(sih->pmurev) >= 22 &&
	    si_pmu_is_ilp_sensitive(idx, regoff)) {
		pmustatus_offset = AOB_ENAB(sih) ? OFFSETOF(pmuregs_t, pmustatus) :
			OFFSETOF(chipcregs_t, pmustatus);

		while (si_corereg(sih, idx, pmustatus_offset, 0, 0) & PST_SLOW_WR_PENDING)
			{};
	}

	return si_corereg(sih, idx, regoff, mask, val);
}

/*
 * If there is no need for fiddling with interrupts or core switches (typically silicon
 * back plane registers, pci registers and chipcommon registers), this function
 * returns the register offset on this core to a mapped address. This address can
 * be used for W_REG/R_REG directly.
 *
 * For accessing registers that would need a core switch, this function will return
 * NULL.
 */
volatile uint32 *
si_corereg_addr(si_t *sih, uint coreidx, uint regoff)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_corereg_addr(sih, coreidx, regoff);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_corereg_addr(sih, coreidx, regoff);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		return nci_corereg_addr(sih, coreidx, regoff);
	else {
		return 0;
	}
}

void
si_core_disable(const si_t *sih, uint32 bits)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		sb_core_disable(sih, bits);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		ai_core_disable(sih, bits);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		nci_core_disable(sih, bits);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		ub_core_disable(sih, bits);
}

void
si_core_reset(si_t *sih, uint32 bits, uint32 resetbits)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		sb_core_reset(sih, bits, resetbits);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI))
		ai_core_reset(sih, bits, resetbits);
	else if (CHIPTYPE(sih->socitype) == SOCI_NCI)
		nci_core_reset(sih, bits, resetbits);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		ub_core_reset(sih, bits, resetbits);
}

/** Run bist on current core. Caller needs to take care of core-specific bist hazards */
int
si_corebist(const si_t *sih)
{
	uint32 cflags;
	int result = 0;

	/* Read core control flags */
	cflags = si_core_cflags(sih, 0, 0);

	/* Set bist & fgc */
	si_core_cflags(sih, ~0, (SICF_BIST_EN | SICF_FGC));

	/* Wait for bist done */
	SPINWAIT(((si_core_sflags(sih, 0, 0) & SISF_BIST_DONE) == 0), 100000);

	if (si_core_sflags(sih, 0, 0) & SISF_BIST_ERROR)
		result = BCME_ERROR;

	/* Reset core control flags */
	si_core_cflags(sih, 0xffff, cflags);

	return result;
}

uint
si_num_slaveports(const si_t *sih, uint coreid)
{
	uint idx = si_findcoreidx(sih, coreid, 0);
	uint num = 0;

	if (idx != BADIDX) {
		if (CHIPTYPE(sih->socitype) == SOCI_AI) {
			num = ai_num_slaveports(sih, idx);
		}
		else if (CHIPTYPE(sih->socitype) == SOCI_NCI) {
			num = nci_num_slaveports(sih, idx);
		}
	}
	return num;
}

/* TODO: Check if NCI has a slave port address */
uint32
si_get_slaveport_addr(si_t *sih, uint spidx, uint baidx, uint core_id, uint coreunit)
{
	const si_info_t *sii = SI_INFO(sih);
	uint origidx = sii->curidx;
	uint32 addr = 0x0;

	if (!((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI) ||
		(CHIPTYPE(sih->socitype) == SOCI_NCI)))
		goto done;

	si_setcore(sih, core_id, coreunit);

	addr = si_addrspace(sih, spidx, baidx);

	si_setcoreidx(sih, origidx);

done:
	return addr;
}

/* TODO: Check if NCI has a d11 slave port address */
uint32
si_get_d11_slaveport_addr(si_t *sih, uint spidx, uint baidx, uint coreunit)
{
	const si_info_t *sii = SI_INFO(sih);
	uint origidx = sii->curidx;
	uint32 addr = 0x0;

	if (!((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS) ||
		(CHIPTYPE(sih->socitype) == SOCI_NAI) ||
		(CHIPTYPE(sih->socitype) == SOCI_NCI)))
		goto done;

	si_setcore(sih, D11_CORE_ID, coreunit);

	addr = si_addrspace(sih, spidx, baidx);

	si_setcoreidx(sih, origidx);

done:
	return addr;
}

static uint32
factor6(uint32 x)
{
	switch (x) {
	case CC_F6_2:	return 2;
	case CC_F6_3:	return 3;
	case CC_F6_4:	return 4;
	case CC_F6_5:	return 5;
	case CC_F6_6:	return 6;
	case CC_F6_7:	return 7;
	default:	return 0;
	}
}

/*
 * Divide the clock by the divisor with protection for
 * a zero divisor.
 */
static uint32
divide_clock(uint32 clock, uint32 div)
{
	return div ? clock / div : 0;
}

/** calculate the speed the SI would run at given a set of clockcontrol values */
uint32
si_clock_rate(uint32 pll_type, uint32 n, uint32 m)
{
	uint32 n1, n2, clock, m1, m2, m3, mc;

	n1 = n & CN_N1_MASK;
	n2 = (n & CN_N2_MASK) >> CN_N2_SHIFT;

	if (pll_type == PLL_TYPE6) {
		if (m & CC_T6_MMASK)
			return CC_T6_M1;
		else
			return CC_T6_M0;
	} else if ((pll_type == PLL_TYPE1) ||
	           (pll_type == PLL_TYPE3) ||
	           (pll_type == PLL_TYPE4) ||
	           (pll_type == PLL_TYPE7)) {
		n1 = factor6(n1);
		n2 += CC_F5_BIAS;
	} else if (pll_type == PLL_TYPE2) {
		n1 += CC_T2_BIAS;
		n2 += CC_T2_BIAS;
		ASSERT((n1 >= 2) && (n1 <= 7));
		ASSERT((n2 >= 5) && (n2 <= 23));
	} else if (pll_type == PLL_TYPE5) {
		/* 5365 */
		return (100000000);
	} else
		ASSERT(0);
	/* PLL types 3 and 7 use BASE2 (25Mhz) */
	if ((pll_type == PLL_TYPE3) ||
	    (pll_type == PLL_TYPE7)) {
		clock = CC_CLOCK_BASE2 * n1 * n2;
	} else
		clock = CC_CLOCK_BASE1 * n1 * n2;

	if (clock == 0)
		return 0;

	m1 = m & CC_M1_MASK;
	m2 = (m & CC_M2_MASK) >> CC_M2_SHIFT;
	m3 = (m & CC_M3_MASK) >> CC_M3_SHIFT;
	mc = (m & CC_MC_MASK) >> CC_MC_SHIFT;

	if ((pll_type == PLL_TYPE1) ||
	    (pll_type == PLL_TYPE3) ||
	    (pll_type == PLL_TYPE4) ||
	    (pll_type == PLL_TYPE7)) {
		m1 = factor6(m1);
		if ((pll_type == PLL_TYPE1) || (pll_type == PLL_TYPE3))
			m2 += CC_F5_BIAS;
		else
			m2 = factor6(m2);
		m3 = factor6(m3);

		switch (mc) {
		case CC_MC_BYPASS:	return (clock);
		case CC_MC_M1:		return divide_clock(clock, m1);
		case CC_MC_M1M2:	return divide_clock(clock, m1 * m2);
		case CC_MC_M1M2M3:	return divide_clock(clock, m1 * m2 * m3);
		case CC_MC_M1M3:	return divide_clock(clock, m1 * m3);
		default:		return (0);
		}
	} else {
		ASSERT(pll_type == PLL_TYPE2);

		m1 += CC_T2_BIAS;
		m2 += CC_T2M2_BIAS;
		m3 += CC_T2_BIAS;
		ASSERT((m1 >= 2) && (m1 <= 7));
		ASSERT((m2 >= 3) && (m2 <= 10));
		ASSERT((m3 >= 2) && (m3 <= 7));

		if ((mc & CC_T2MC_M1BYP) == 0)
			clock /= m1;
		if ((mc & CC_T2MC_M2BYP) == 0)
			clock /= m2;
		if ((mc & CC_T2MC_M3BYP) == 0)
			clock /= m3;

		return (clock);
	}
}

/**
 * Some chips could have multiple host interfaces, however only one will be active.
 * For a given chip. Depending pkgopt and cc_chipst return the active host interface.
 */
uint
si_chip_hostif(const si_t *sih)
{
	uint hosti = 0;

	switch (CHIPID(sih->chip)) {
	case BCM43012_CHIP_ID:
	case BCM43013_CHIP_ID:
	case BCM43014_CHIP_ID:
		hosti = CHIP_HOSTIF_SDIOMODE;
		break;
	CASE_BCM43602_CHIP:
		hosti = CHIP_HOSTIF_PCIEMODE;
		break;

	case BCM4360_CHIP_ID:
		/* chippkg bit-0 == 0 is PCIE only pkgs
		 * chippkg bit-0 == 1 has both PCIE and USB cores enabled
		 */
		if ((sih->chippkg & 0x1) && (sih->chipst & CST4360_MODE_USB))
			hosti = CHIP_HOSTIF_USBMODE;
		else
			hosti = CHIP_HOSTIF_PCIEMODE;

		break;

	case BCM4369_CHIP_GRPID:
		 if (CST4369_CHIPMODE_SDIOD(sih->chipst))
			 hosti = CHIP_HOSTIF_SDIOMODE;
		 else if (CST4369_CHIPMODE_PCIE(sih->chipst))
			 hosti = CHIP_HOSTIF_PCIEMODE;
		 break;
	case BCM4368_CHIP_GRPID:
		 hosti = CHIP_HOSTIF_PCIEMODE;
		 break;
	case BCM4376_CHIP_GRPID:
	case BCM4378_CHIP_GRPID:
	case BCM4385_CHIP_GRPID:
	case BCM4387_CHIP_GRPID:
	case BCM4388_CHIP_GRPID:
	case BCM4389_CHIP_GRPID:
		 hosti = CHIP_HOSTIF_PCIEMODE;
		 break;
	 case BCM4362_CHIP_GRPID:
		if (CST4362_CHIPMODE_SDIOD(sih->chipst)) {
			hosti = CHIP_HOSTIF_SDIOMODE;
		} else if (CST4362_CHIPMODE_PCIE(sih->chipst)) {
			hosti = CHIP_HOSTIF_PCIEMODE;
		}
		break;

	default:
		break;
	}

	return hosti;
}

/** set chip watchdog reset timer to fire in 'ticks' */
void
si_watchdog(si_t *sih, uint ticks)
{
	uint nb, maxt;
	uint pmu_wdt = 1;

	if (PMUCTL_ENAB(sih) && pmu_wdt) {
		nb = (CCREV(sih->ccrev) < 26) ? 16 : ((CCREV(sih->ccrev) >= 37) ? 32 : 24);
		/* The mips compiler uses the sllv instruction,
		 * so we specially handle the 32-bit case.
		 */
		if (nb == 32)
			maxt = 0xffffffff;
		else
			maxt = ((1 << nb) - 1);

		if (ticks == 1)
			ticks = 2;
		else if (ticks > maxt)
			ticks = maxt;
		if ((CHIPID(sih->chip) == BCM43012_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43013_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43014_CHIP_ID)) {
			PMU_REG_NEW(sih, min_res_mask, ~0, DEFAULT_43012_MIN_RES_MASK);
			PMU_REG_NEW(sih, watchdog_res_mask, ~0, DEFAULT_43012_MIN_RES_MASK);
			PMU_REG_NEW(sih, pmustatus, PST_WDRESET, PST_WDRESET);
			PMU_REG_NEW(sih, pmucontrol_ext, PCTL_EXT_FASTLPO_SWENAB, 0);
			SPINWAIT((PMU_REG(sih, pmustatus, 0, 0) & PST_ILPFASTLPO),
				PMU_MAX_TRANSITION_DLY);
		}
		pmu_corereg(sih, SI_CC_IDX, pmuwatchdog, ~0, ticks);
	} else {
		maxt = (1 << 28) - 1;
		if (ticks > maxt)
			ticks = maxt;

		if (CCREV(sih->ccrev) >= 65) {
			si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, watchdog), ~0,
				(ticks & WD_COUNTER_MASK) | WD_SSRESET_PCIE_F0_EN |
					WD_SSRESET_PCIE_ALL_FN_EN);
		} else {
			si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, watchdog), ~0, ticks);
		}
	}
}

/** trigger watchdog reset after ms milliseconds */
void
si_watchdog_ms(si_t *sih, uint32 ms)
{
	si_watchdog(sih, wd_msticks * ms);
}

uint32 si_watchdog_msticks(void)
{
	return wd_msticks;
}

bool
si_taclear(si_t *sih, bool details)
{
	return FALSE;
}

/** return the slow clock source - LPO, XTAL, or PCI */
static uint
si_slowclk_src(si_info_t *sii)
{
	chipcregs_t *cc;

	ASSERT(SI_FAST(sii) || si_coreid(&sii->pub) == CC_CORE_ID);

	if (CCREV(sii->pub.ccrev) < 6) {
		if ((BUSTYPE(sii->pub.bustype) == PCI_BUS) &&
		    (OSL_PCI_READ_CONFIG(sii->osh, PCI_GPIO_OUT, sizeof(uint32)) &
		     PCI_CFG_GPIO_SCS))
			return (SCC_SS_PCI);
		else
			return (SCC_SS_XTAL);
	} else if (CCREV(sii->pub.ccrev) < 10) {
		cc = (chipcregs_t *)si_setcoreidx(&sii->pub, sii->curidx);
		ASSERT(cc);
		return (R_REG(sii->osh, &cc->slow_clk_ctl) & SCC_SS_MASK);
	} else	/* Insta-clock */
		return (SCC_SS_XTAL);
}

/** return the ILP (slowclock) min or max frequency */
static uint
si_slowclk_freq(si_info_t *sii, bool max_freq, chipcregs_t *cc)
{
	uint32 slowclk;
	uint div;

	ASSERT(SI_FAST(sii) || si_coreid(&sii->pub) == CC_CORE_ID);

	/* shouldn't be here unless we've established the chip has dynamic clk control */
	ASSERT(R_REG(sii->osh, &cc->capabilities) & CC_CAP_PWR_CTL);

	slowclk = si_slowclk_src(sii);
	if (CCREV(sii->pub.ccrev) < 6) {
		if (slowclk == SCC_SS_PCI)
			return (max_freq ? (PCIMAXFREQ / 64) : (PCIMINFREQ / 64));
		else
			return (max_freq ? (XTALMAXFREQ / 32) : (XTALMINFREQ / 32));
	} else if (CCREV(sii->pub.ccrev) < 10) {
		div = 4 *
		        (((R_REG(sii->osh, &cc->slow_clk_ctl) & SCC_CD_MASK) >> SCC_CD_SHIFT) + 1);
		if (slowclk == SCC_SS_LPO)
			return (max_freq ? LPOMAXFREQ : LPOMINFREQ);
		else if (slowclk == SCC_SS_XTAL)
			return (max_freq ? (XTALMAXFREQ / div) : (XTALMINFREQ / div));
		else if (slowclk == SCC_SS_PCI)
			return (max_freq ? (PCIMAXFREQ / div) : (PCIMINFREQ / div));
		else
			ASSERT(0);
	} else {
		/* Chipc rev 10 is InstaClock */
		div = R_REG(sii->osh, &cc->system_clk_ctl) >> SYCC_CD_SHIFT;
		div = 4 * (div + 1);
		return (max_freq ? XTALMAXFREQ : (XTALMINFREQ / div));
	}
	return (0);
}

static void
si_clkctl_setdelay(si_info_t *sii, void *chipcregs)
{
	chipcregs_t *cc = (chipcregs_t *)chipcregs;
	uint slowmaxfreq, pll_delay, slowclk;
	uint pll_on_delay, fref_sel_delay;

	pll_delay = PLL_DELAY;

	/* If the slow clock is not sourced by the xtal then add the xtal_on_delay
	 * since the xtal will also be powered down by dynamic clk control logic.
	 */

	slowclk = si_slowclk_src(sii);
	if (slowclk != SCC_SS_XTAL)
		pll_delay += XTAL_ON_DELAY;

	/* Starting with 4318 it is ILP that is used for the delays */
	slowmaxfreq = si_slowclk_freq(sii, (CCREV(sii->pub.ccrev) >= 10) ? FALSE : TRUE, cc);

	pll_on_delay = ((slowmaxfreq * pll_delay) + 999999) / 1000000;
	fref_sel_delay = ((slowmaxfreq * FREF_DELAY) + 999999) / 1000000;

	W_REG(sii->osh, &cc->pll_on_delay, pll_on_delay);
	W_REG(sii->osh, &cc->fref_sel_delay, fref_sel_delay);
}

/** initialize power control delay registers */
void
si_clkctl_init(si_t *sih)
{
	si_info_t *sii;
	uint origidx = 0;
	chipcregs_t *cc;
	bool fast;

	if (!CCCTL_ENAB(sih))
		return;

	sii = SI_INFO(sih);
	fast = SI_FAST(sii);
	if (!fast) {
		origidx = sii->curidx;
		if ((cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0)) == NULL)
			return;
	} else if ((cc = (chipcregs_t *)CCREGS_FAST(sii)) == NULL)
		return;
	ASSERT(cc != NULL);

	/* set all Instaclk chip ILP to 1 MHz */
	if (CCREV(sih->ccrev) >= 10)
		SET_REG(sii->osh, &cc->system_clk_ctl, SYCC_CD_MASK,
		        (ILP_DIV_1MHZ << SYCC_CD_SHIFT));

	si_clkctl_setdelay(sii, (void *)(uintptr)cc);

	OSL_DELAY(20000);

	if (!fast)
		si_setcoreidx(sih, origidx);
}

/** change logical "focus" to the gpio core for optimized access */
volatile void *
si_gpiosetcore(si_t *sih)
{
	return (si_setcoreidx(sih, SI_CC_IDX));
}

/**
 * mask & set gpiocontrol bits.
 * If a gpiocontrol bit is set to 0, chipcommon controls the corresponding GPIO pin.
 * If a gpiocontrol bit is set to 1, the GPIO pin is no longer a GPIO and becomes dedicated
 *   to some chip-specific purpose.
 */
uint32
si_gpiocontrol(si_t *sih, uint32 mask, uint32 val, uint8 priority)
{
	uint regoff;

	regoff = 0;

	/* gpios could be shared on router platforms
	 * ignore reservation if it's high priority (e.g., test apps)
	 */
	if ((priority != GPIO_HI_PRIORITY) &&
	    (BUSTYPE(sih->bustype) == SI_BUS) && (val || mask)) {
		mask = priority ? (si_gpioreservation & mask) :
			((si_gpioreservation | mask) & ~(si_gpioreservation));
		val &= mask;
	}

	regoff = OFFSETOF(chipcregs_t, gpiocontrol);
	return (si_corereg(sih, SI_CC_IDX, regoff, mask, val));
}

/** mask&set gpio output enable bits */
uint32
si_gpioouten(si_t *sih, uint32 mask, uint32 val, uint8 priority)
{
	uint regoff;

	regoff = 0;

	/* gpios could be shared on router platforms
	 * ignore reservation if it's high priority (e.g., test apps)
	 */
	if ((priority != GPIO_HI_PRIORITY) &&
	    (BUSTYPE(sih->bustype) == SI_BUS) && (val || mask)) {
		mask = priority ? (si_gpioreservation & mask) :
			((si_gpioreservation | mask) & ~(si_gpioreservation));
		val &= mask;
	}

	regoff = OFFSETOF(chipcregs_t, gpioouten);
	return (si_corereg(sih, SI_CC_IDX, regoff, mask, val));
}

/** mask&set gpio output bits */
uint32
si_gpioout(si_t *sih, uint32 mask, uint32 val, uint8 priority)
{
	uint regoff;

	regoff = 0;

	/* gpios could be shared on router platforms
	 * ignore reservation if it's high priority (e.g., test apps)
	 */
	if ((priority != GPIO_HI_PRIORITY) &&
	    (BUSTYPE(sih->bustype) == SI_BUS) && (val || mask)) {
		mask = priority ? (si_gpioreservation & mask) :
			((si_gpioreservation | mask) & ~(si_gpioreservation));
		val &= mask;
	}

	regoff = OFFSETOF(chipcregs_t, gpioout);
	return (si_corereg(sih, SI_CC_IDX, regoff, mask, val));
}

/** reserve one gpio */
uint32
si_gpioreserve(const si_t *sih, uint32 gpio_bitmask, uint8 priority)
{
	/* only cores on SI_BUS share GPIO's and only applcation users need to
	 * reserve/release GPIO
	 */
	if ((BUSTYPE(sih->bustype) != SI_BUS) || (!priority)) {
		ASSERT((BUSTYPE(sih->bustype) == SI_BUS) && (priority));
		return 0xffffffff;
	}
	/* make sure only one bit is set */
	if ((!gpio_bitmask) || ((gpio_bitmask) & (gpio_bitmask - 1))) {
		ASSERT((gpio_bitmask) && !((gpio_bitmask) & (gpio_bitmask - 1)));
		return 0xffffffff;
	}

	/* already reserved */
	if (si_gpioreservation & gpio_bitmask)
		return 0xffffffff;
	/* set reservation */
	si_gpioreservation |= gpio_bitmask;

	return si_gpioreservation;
}

/**
 * release one gpio.
 *
 * releasing the gpio doesn't change the current value on the GPIO last write value
 * persists till someone overwrites it.
 */
uint32
si_gpiorelease(const si_t *sih, uint32 gpio_bitmask, uint8 priority)
{
	/* only cores on SI_BUS share GPIO's and only applcation users need to
	 * reserve/release GPIO
	 */
	if ((BUSTYPE(sih->bustype) != SI_BUS) || (!priority)) {
		ASSERT((BUSTYPE(sih->bustype) == SI_BUS) && (priority));
		return 0xffffffff;
	}
	/* make sure only one bit is set */
	if ((!gpio_bitmask) || ((gpio_bitmask) & (gpio_bitmask - 1))) {
		ASSERT((gpio_bitmask) && !((gpio_bitmask) & (gpio_bitmask - 1)));
		return 0xffffffff;
	}

	/* already released */
	if (!(si_gpioreservation & gpio_bitmask))
		return 0xffffffff;

	/* clear reservation */
	si_gpioreservation &= ~gpio_bitmask;

	return si_gpioreservation;
}

/* return the current gpioin register value */
uint32
si_gpioin(si_t *sih)
{
	uint regoff;

	regoff = OFFSETOF(chipcregs_t, gpioin);
	return (si_corereg(sih, SI_CC_IDX, regoff, 0, 0));
}

/* mask&set gpio interrupt polarity bits */
uint32
si_gpiointpolarity(si_t *sih, uint32 mask, uint32 val, uint8 priority)
{
	uint regoff;

	/* gpios could be shared on router platforms */
	if ((BUSTYPE(sih->bustype) == SI_BUS) && (val || mask)) {
		mask = priority ? (si_gpioreservation & mask) :
			((si_gpioreservation | mask) & ~(si_gpioreservation));
		val &= mask;
	}

	regoff = OFFSETOF(chipcregs_t, gpiointpolarity);
	return (si_corereg(sih, SI_CC_IDX, regoff, mask, val));
}

/* mask&set gpio interrupt mask bits */
uint32
si_gpiointmask(si_t *sih, uint32 mask, uint32 val, uint8 priority)
{
	uint regoff;

	/* gpios could be shared on router platforms */
	if ((BUSTYPE(sih->bustype) == SI_BUS) && (val || mask)) {
		mask = priority ? (si_gpioreservation & mask) :
			((si_gpioreservation | mask) & ~(si_gpioreservation));
		val &= mask;
	}

	regoff = OFFSETOF(chipcregs_t, gpiointmask);
	return (si_corereg(sih, SI_CC_IDX, regoff, mask, val));
}

uint32
si_gpioeventintmask(si_t *sih, uint32 mask, uint32 val, uint8 priority)
{
	uint regoff;
	/* gpios could be shared on router platforms */
	if ((BUSTYPE(sih->bustype) == SI_BUS) && (val || mask)) {
		mask = priority ? (si_gpioreservation & mask) :
			((si_gpioreservation | mask) & ~(si_gpioreservation));
		val &= mask;
	}
	regoff = OFFSETOF(chipcregs_t, gpioeventintmask);
	return (si_corereg(sih, SI_CC_IDX, regoff, mask, val));
}

uint32
si_gpiopull(si_t *sih, bool updown, uint32 mask, uint32 val)
{
	uint offs;

	if (CCREV(sih->ccrev) < 20)
		return 0xffffffff;

	offs = (updown ? OFFSETOF(chipcregs_t, gpiopulldown) : OFFSETOF(chipcregs_t, gpiopullup));
	return (si_corereg(sih, SI_CC_IDX, offs, mask, val));
}

uint32
si_gpioevent(si_t *sih, uint regtype, uint32 mask, uint32 val)
{
	uint offs;

	if (CCREV(sih->ccrev) < 11)
		return 0xffffffff;

	if (regtype == GPIO_REGEVT)
		offs = OFFSETOF(chipcregs_t, gpioevent);
	else if (regtype == GPIO_REGEVT_INTMSK)
		offs = OFFSETOF(chipcregs_t, gpioeventintmask);
	else if (regtype == GPIO_REGEVT_INTPOL)
		offs = OFFSETOF(chipcregs_t, gpioeventintpolarity);
	else
		return 0xffffffff;

	return (si_corereg(sih, SI_CC_IDX, offs, mask, val));
}

uint32
si_gpio_int_enable(si_t *sih, bool enable)
{
	uint offs;

	if (CCREV(sih->ccrev) < 11)
		return 0xffffffff;

	offs = OFFSETOF(chipcregs_t, intmask);
	return (si_corereg(sih, SI_CC_IDX, offs, CI_GPIO, (enable ? CI_GPIO : 0)));
}

/** Return the size of the specified SYSMEM bank */
static uint
sysmem_banksize(const si_info_t *sii, sysmemregs_t *regs, uint8 idx)
{
	uint banksize, bankinfo;
	uint bankidx = idx;

	W_REG(sii->osh, &regs->bankidx, bankidx);
	bankinfo = R_REG(sii->osh, &regs->bankinfo);
	banksize = SYSMEM_BANKINFO_SZBASE * ((bankinfo & SYSMEM_BANKINFO_SZMASK) + 1);
	return banksize;
}

/** Return the RAM size of the SYSMEM core */
uint32
si_sysmem_size(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	uint origidx;
	bcm_int_bitmask_t intr_val;

	sysmemregs_t *regs;
	bool wasup;
	uint32 coreinfo;
	uint memsize = 0;
	uint8 i;
	uint nb, nrb;

	/* Block ints and save current core */
	INTR_OFF(sii, &intr_val);
	origidx = si_coreidx(sih);

	/* Switch to SYSMEM core */
	if (!(regs = si_setcore(sih, SYSMEM_CORE_ID, 0)))
		goto done;

	/* Get info for determining size */
	if (!(wasup = si_iscoreup(sih)))
		si_core_reset(sih, 0, 0);
	coreinfo = R_REG(sii->osh, &regs->coreinfo);

	/* Number of ROM banks, SW need to skip the ROM banks. */
	nrb = (coreinfo & SYSMEM_SRCI_ROMNB_MASK) >> SYSMEM_SRCI_ROMNB_SHIFT;

	nb = (coreinfo & SYSMEM_SRCI_SRNB_MASK) >> SYSMEM_SRCI_SRNB_SHIFT;
	for (i = 0; i < nb; i++)
		memsize += sysmem_banksize(sii, regs, i + nrb);

	si_setcoreidx(sih, origidx);

done:
	INTR_RESTORE(sii, &intr_val);

	return memsize;
}

/** Return the size of the specified SOCRAM bank */
static uint
socram_banksize(const si_info_t *sii, sbsocramregs_t *regs, uint8 idx, uint8 mem_type)
{
	uint banksize, bankinfo;
	uint bankidx = idx | (mem_type << SOCRAM_BANKIDX_MEMTYPE_SHIFT);

	ASSERT(mem_type <= SOCRAM_MEMTYPE_DEVRAM);

	W_REG(sii->osh, &regs->bankidx, bankidx);
	bankinfo = R_REG(sii->osh, &regs->bankinfo);
	banksize = SOCRAM_BANKINFO_SZBASE * ((bankinfo & SOCRAM_BANKINFO_SZMASK) + 1);
	return banksize;
}

void si_socram_set_bankpda(si_t *sih, uint32 bankidx, uint32 bankpda)
{
	const si_info_t *sii = SI_INFO(sih);
	uint origidx;
	bcm_int_bitmask_t intr_val;
	sbsocramregs_t *regs;
	bool wasup;
	uint corerev;

	/* Block ints and save current core */
	INTR_OFF(sii, &intr_val);
	origidx = si_coreidx(sih);

	/* Switch to SOCRAM core */
	if (!(regs = si_setcore(sih, SOCRAM_CORE_ID, 0)))
		goto done;

	if (!(wasup = si_iscoreup(sih)))
		si_core_reset(sih, 0, 0);

	corerev = si_corerev(sih);
	if (corerev >= 16) {
		W_REG(sii->osh, &regs->bankidx, bankidx);
		W_REG(sii->osh, &regs->bankpda, bankpda);
	}

	/* Return to previous state and core */
	if (!wasup)
		si_core_disable(sih, 0);
	si_setcoreidx(sih, origidx);

done:
	INTR_RESTORE(sii, &intr_val);
}

/** Return the RAM size of the SOCRAM core */
uint32
si_socram_size(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	uint origidx;
	bcm_int_bitmask_t intr_val;

	sbsocramregs_t *regs;
	bool wasup;
	uint corerev;
	uint32 coreinfo;
	uint memsize = 0;

	/* Block ints and save current core */
	INTR_OFF(sii, &intr_val);
	origidx = si_coreidx(sih);

	/* Switch to SOCRAM core */
	if (!(regs = si_setcore(sih, SOCRAM_CORE_ID, 0)))
		goto done;

	/* Get info for determining size */
	if (!(wasup = si_iscoreup(sih)))
		si_core_reset(sih, 0, 0);
	corerev = si_corerev(sih);
	coreinfo = R_REG(sii->osh, &regs->coreinfo);

	/* Calculate size from coreinfo based on rev */
	if (corerev == 0)
		memsize = 1 << (16 + (coreinfo & SRCI_MS0_MASK));
	else if (corerev < 3) {
		memsize = 1 << (SR_BSZ_BASE + (coreinfo & SRCI_SRBSZ_MASK));
		memsize *= (coreinfo & SRCI_SRNB_MASK) >> SRCI_SRNB_SHIFT;
	} else if ((corerev <= 7) || (corerev == 12)) {
		uint nb = (coreinfo & SRCI_SRNB_MASK) >> SRCI_SRNB_SHIFT;
		uint bsz = (coreinfo & SRCI_SRBSZ_MASK);
		uint lss = (coreinfo & SRCI_LSS_MASK) >> SRCI_LSS_SHIFT;
		if (lss != 0)
			nb --;
		memsize = nb * (1 << (bsz + SR_BSZ_BASE));
		if (lss != 0)
			memsize += (1 << ((lss - 1) + SR_BSZ_BASE));
	} else {
		uint8 i;
		uint nb;
		/* length of SRAM Banks increased for corerev greater than 23 */
		if (corerev >= 23) {
			nb = (coreinfo & (SRCI_SRNB_MASK | SRCI_SRNB_MASK_EXT)) >> SRCI_SRNB_SHIFT;
		} else {
			nb = (coreinfo & SRCI_SRNB_MASK) >> SRCI_SRNB_SHIFT;
		}
		for (i = 0; i < nb; i++)
			memsize += socram_banksize(sii, regs, i, SOCRAM_MEMTYPE_RAM);
	}

	/* Return to previous state and core */
	if (!wasup)
		si_core_disable(sih, 0);
	si_setcoreidx(sih, origidx);

done:
	INTR_RESTORE(sii, &intr_val);

	return memsize;
}

/* Return true if bus MPU is present */
bool
si_is_bus_mpu_present(si_t *sih)
{
	uint origidx, newidx = NODEV_CORE_ID;
	sysmemregs_t *sysmemregs = NULL;
	cr4regs_t *cr4regs;
	const si_info_t *sii = SI_INFO(sih);
	uint ret = 0;
	bool wasup;

	origidx = si_coreidx(sih);

	cr4regs = si_setcore(sih, ARMCR4_CORE_ID, 0);
	if (cr4regs) {
		/* ARMCR4 */
		newidx = ARMCR4_CORE_ID;
	} else {
		sysmemregs = si_setcore(sih, SYSMEM_CORE_ID, 0);
		if (sysmemregs) {
			/* ARMCA7 */
			newidx = SYSMEM_CORE_ID;
		}
	}

	if (newidx != NODEV_CORE_ID) {
		if (!(wasup = si_iscoreup(sih))) {
			si_core_reset(sih, 0, 0);
		}
		if (newidx == ARMCR4_CORE_ID) {
			/* ARMCR4 */
			ret = R_REG(sii->osh, &cr4regs->corecapabilities) & CAP_MPU_MASK;
		} else {
			/* ARMCA7 */
			ret = R_REG(sii->osh, &sysmemregs->mpucapabilities) &
				ACC_MPU_REGION_CNT_MASK;
		}
		if (!wasup) {
			si_core_disable(sih, 0);
		}
	}

	si_setcoreidx(sih, origidx);

	return ret ? TRUE : FALSE;
}

/** Return the TCM-RAM size of the ARMCR4 core. */
uint32
si_tcm_size(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	uint origidx;
	bcm_int_bitmask_t intr_val;
	volatile uint8 *regs;
	bool wasup;
	uint32 corecap;
	uint memsize = 0;
	uint banku_size = 0;
	uint32 nab = 0;
	uint32 nbb = 0;
	uint32 totb = 0;
	uint32 bxinfo = 0;
	uint32 idx = 0;
	volatile uint32 *arm_cap_reg;
	volatile uint32 *arm_bidx;
	volatile uint32 *arm_binfo;

	/* Block ints and save current core */
	INTR_OFF(sii, &intr_val);
	origidx = si_coreidx(sih);

	/* Switch to CR4 core */
	if (!(regs = si_setcore(sih, ARMCR4_CORE_ID, 0)))
		goto done;

	/* Get info for determining size. If in reset, come out of reset,
	 * but remain in halt
	 */
	if (!(wasup = si_iscoreup(sih)))
		si_core_reset(sih, SICF_CPUHALT, SICF_CPUHALT);

	arm_cap_reg = (volatile uint32 *)(regs + SI_CR4_CAP);
	corecap = R_REG(sii->osh, arm_cap_reg);

	nab = (corecap & ARMCR4_TCBANB_MASK) >> ARMCR4_TCBANB_SHIFT;
	nbb = (corecap & ARMCR4_TCBBNB_MASK) >> ARMCR4_TCBBNB_SHIFT;
	totb = nab + nbb;

	arm_bidx = (volatile uint32 *)(regs + SI_CR4_BANKIDX);
	arm_binfo = (volatile uint32 *)(regs + SI_CR4_BANKINFO);
	for (idx = 0; idx < totb; idx++) {
		W_REG(sii->osh, arm_bidx, idx);

		bxinfo = R_REG(sii->osh, arm_binfo);
		if (bxinfo & ARMCR4_BUNITSZ_MASK) {
			banku_size = ARMCR4_BSZ_1K;
		} else {
			banku_size = ARMCR4_BSZ_8K;
		}
		memsize += ((bxinfo & ARMCR4_BSZ_MASK) + 1) * banku_size;
	}

	/* Return to previous state and core */
	if (!wasup)
		si_core_disable(sih, 0);
	si_setcoreidx(sih, origidx);

done:
	INTR_RESTORE(sii, &intr_val);

	return memsize;
}

bool
si_has_flops(si_t *sih)
{
	uint origidx, cr4_rev;

	/* Find out CR4 core revision */
	origidx = si_coreidx(sih);
	if (si_setcore(sih, ARMCR4_CORE_ID, 0)) {
		cr4_rev = si_corerev(sih);
		si_setcoreidx(sih, origidx);

		if (cr4_rev == 1 || cr4_rev >= 3)
			return TRUE;
	}
	return FALSE;
}

uint32
si_socram_srmem_size(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	uint origidx;
	bcm_int_bitmask_t intr_val;

	sbsocramregs_t *regs;
	bool wasup;
	uint corerev;
	uint32 coreinfo;
	uint memsize = 0;

	/* Block ints and save current core */
	INTR_OFF(sii, &intr_val);
	origidx = si_coreidx(sih);

	/* Switch to SOCRAM core */
	if (!(regs = si_setcore(sih, SOCRAM_CORE_ID, 0)))
		goto done;

	/* Get info for determining size */
	if (!(wasup = si_iscoreup(sih)))
		si_core_reset(sih, 0, 0);
	corerev = si_corerev(sih);
	coreinfo = R_REG(sii->osh, &regs->coreinfo);

	/* Calculate size from coreinfo based on rev */
	if (corerev >= 16) {
		uint8 i;
		uint nb = (coreinfo & SRCI_SRNB_MASK) >> SRCI_SRNB_SHIFT;
		for (i = 0; i < nb; i++) {
			W_REG(sii->osh, &regs->bankidx, i);
			if (R_REG(sii->osh, &regs->bankinfo) & SOCRAM_BANKINFO_RETNTRAM_MASK)
				memsize += socram_banksize(sii, regs, i, SOCRAM_MEMTYPE_RAM);
		}
	}

	/* Return to previous state and core */
	if (!wasup)
		si_core_disable(sih, 0);
	si_setcoreidx(sih, origidx);

done:
	INTR_RESTORE(sii, &intr_val);

	return memsize;
}

void
si_btcgpiowar(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	uint origidx;
	bcm_int_bitmask_t intr_val;
	chipcregs_t *cc;

	/* Make sure that there is ChipCommon core present &&
	 * UART_TX is strapped to 1
	 */
	if (!(sih->cccaps & CC_CAP_UARTGPIO))
		return;

	/* si_corereg cannot be used as we have to guarantee 8-bit read/writes */
	INTR_OFF(sii, &intr_val);

	origidx = si_coreidx(sih);

	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
	ASSERT(cc != NULL);

	W_REG(sii->osh, &cc->uart0mcr, R_REG(sii->osh, &cc->uart0mcr) | 0x04);

	/* restore the original index */
	si_setcoreidx(sih, origidx);

	INTR_RESTORE(sii, &intr_val);
}

void
si_chipcontrl_restore(si_t *sih, uint32 val)
{
	const si_info_t *sii = SI_INFO(sih);
	chipcregs_t *cc;
	uint origidx = si_coreidx(sih);

	if ((cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0)) == NULL) {
		SI_ERROR(("si_chipcontrl_restore: Failed to find CORE ID!\n"));
		return;
	}
	W_REG(sii->osh, &cc->chipcontrol, val);
	si_setcoreidx(sih, origidx);
}

uint32
si_chipcontrl_read(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	chipcregs_t *cc;
	uint origidx = si_coreidx(sih);
	uint32 val;

	if ((cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0)) == NULL) {
		SI_ERROR(("si_chipcontrl_read: Failed to find CORE ID!\n"));
		return -1;
	}
	val = R_REG(sii->osh, &cc->chipcontrol);
	si_setcoreidx(sih, origidx);
	return val;
}

/** switch muxed pins, on: SROM, off: FEMCTRL. Called for a family of ac chips, not just 4360. */
void
si_chipcontrl_srom4360(si_t *sih, bool on)
{
	const si_info_t *sii = SI_INFO(sih);
	chipcregs_t *cc;
	uint origidx = si_coreidx(sih);
	uint32 val;

	if ((cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0)) == NULL) {
		SI_ERROR(("si_chipcontrl_srom4360: Failed to find CORE ID!\n"));
		return;
	}
	val = R_REG(sii->osh, &cc->chipcontrol);

	if (on) {
		val &= ~(CCTRL4360_SECI_MODE |
			CCTRL4360_BTSWCTRL_MODE |
			CCTRL4360_EXTRA_FEMCTRL_MODE |
			CCTRL4360_BT_LGCY_MODE |
			CCTRL4360_CORE2FEMCTRL4_ON);

		W_REG(sii->osh, &cc->chipcontrol, val);
	} else {
		/* huh, nothing here? */
	}

	si_setcoreidx(sih, origidx);
}

/**
 * The SROM clock is derived from the backplane clock. For chips having a fast
 * backplane clock that requires a higher-than-POR-default clock divisor ratio for the SROM clock.
 */
void
si_srom_clk_set(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	chipcregs_t *cc;
	uint origidx = si_coreidx(sih);
	uint32 val;
	uint32 divisor = 1;

	if ((cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0)) == NULL) {
		SI_ERROR(("si_srom_clk_set: Failed to find CORE ID!\n"));
		return;
	}

	val = R_REG(sii->osh, &cc->clkdiv2);
	ASSERT(0);

	W_REG(sii->osh, &cc->clkdiv2, ((val & ~CLKD2_SROM) | divisor));
	si_setcoreidx(sih, origidx);
}

void
si_pmu_avb_clk_set(si_t *sih, osl_t *osh, bool set_flag)
{
}

void
si_btc_enable_chipcontrol(si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	chipcregs_t *cc;
	uint origidx = si_coreidx(sih);

	if ((cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0)) == NULL) {
		SI_ERROR(("si_btc_enable_chipcontrol: Failed to find CORE ID!\n"));
		return;
	}

	/* BT fix */
	W_REG(sii->osh, &cc->chipcontrol,
		R_REG(sii->osh, &cc->chipcontrol) | CC_BTCOEX_EN_MASK);

	si_setcoreidx(sih, origidx);
}

/** cache device removed state */
void si_set_device_removed(si_t *sih, bool status)
{
	si_info_t *sii = SI_INFO(sih);

	sii->device_removed = status;
}

/** check if the device is removed */
bool
si_deviceremoved(const si_t *sih)
{
	uint32 w;
	const si_info_t *sii = SI_INFO(sih);

	if (sii->device_removed) {
		return TRUE;
	}

	switch (BUSTYPE(sih->bustype)) {
	case PCI_BUS:
		ASSERT(SI_INFO(sih)->osh != NULL);
		w = OSL_PCI_READ_CONFIG(SI_INFO(sih)->osh, PCI_CFG_VID, sizeof(uint32));
		if ((w & 0xFFFF) != VENDOR_BROADCOM)
			return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

bool
si_is_warmboot(void)
{

	return FALSE;
}

bool
si_is_sprom_available(si_t *sih)
{
	if (CCREV(sih->ccrev) >= 31) {
		const si_info_t *sii;
		uint origidx;
		chipcregs_t *cc;
		uint32 sromctrl;

		if ((sih->cccaps & CC_CAP_SROM) == 0)
			return FALSE;

		sii = SI_INFO(sih);
		origidx = sii->curidx;
		cc = si_setcoreidx(sih, SI_CC_IDX);
		ASSERT(cc);
		sromctrl = R_REG(sii->osh, &cc->sromcontrol);
		si_setcoreidx(sih, origidx);
		return (sromctrl & SRC_PRESENT);
	}

	switch (CHIPID(sih->chip)) {
	case BCM4369_CHIP_GRPID:
		if (CHIPREV(sih->chiprev) == 0) {
			/* WAR for 4369a0: HW4369-1729. no sprom, default to otp always. */
			return 0;
		} else {
			return (sih->chipst & CST4369_SPROM_PRESENT) != 0;
		}
	case BCM4368_CHIP_ID:
		return FALSE;	/* revisit if sprom_present chipstatus bit becomes available */
		break;
	CASE_BCM43602_CHIP:
		return (sih->chipst & CST43602_SPROM_PRESENT) != 0;
	case BCM43012_CHIP_ID:
	case BCM43013_CHIP_ID:
	case BCM43014_CHIP_ID:
		return FALSE;
	case BCM4362_CHIP_GRPID:
		return (sih->chipst & CST4362_SPROM_PRESENT) != 0;
	case BCM4376_CHIP_GRPID:
	case BCM4378_CHIP_GRPID:
		return (sih->chipst & CST4378_SPROM_PRESENT) != 0;
	case BCM4385_CHIP_GRPID:
	case BCM4387_CHIP_GRPID:
		return (sih->chipst & CST4387_SPROM_PRESENT) != 0;
	case BCM4388_CHIP_GRPID:
	case BCM4389_CHIP_GRPID:
	case BCM4397_CHIP_GRPID:
		/* 4389 supports only OTP */
		return FALSE;
	default:
		return TRUE;
	}
}

bool
si_is_sflash_available(const si_t *sih)
{
	switch (CHIPID(sih->chip)) {
	case BCM4368_CHIP_ID:
		return (sih->chipst & CST4368_SFLASH_PRESENT) != 0;
	default:
		return FALSE;
	}
}

uint32 si_get_sromctl(si_t *sih)
{
	chipcregs_t *cc;
	uint origidx = si_coreidx(sih);
	uint32 sromctl;
	osl_t *osh = si_osh(sih);

	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT((uintptr)cc);

	sromctl = R_REG(osh, &cc->sromcontrol);

	/* return to the original core */
	si_setcoreidx(sih, origidx);
	return sromctl;
}

int si_set_sromctl(si_t *sih, uint32 value)
{
	chipcregs_t *cc;
	uint origidx = si_coreidx(sih);
	osl_t *osh = si_osh(sih);
	int ret = BCME_OK;

	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT((uintptr)cc);

	/* get chipcommon rev */
	if (si_corerev(sih) >= 32) {
		/* SpromCtrl is only accessible if CoreCapabilities.SpromSupported and
		 * SpromPresent is 1.
		 */
		if ((R_REG(osh, &cc->capabilities) & CC_CAP_SROM) != 0 &&
		     (R_REG(osh, &cc->sromcontrol) & SRC_PRESENT)) {
			W_REG(osh, &cc->sromcontrol, value);
		} else {
			ret = BCME_NODEVICE;
		}
	} else {
		ret = BCME_UNSUPPORTED;
	}

	/* return to the original core */
	si_setcoreidx(sih, origidx);

	return ret;
}

uint
si_core_wrapperreg(si_t *sih, uint32 coreidx, uint32 offset, uint32 mask, uint32 val)
{
	uint origidx;
	bcm_int_bitmask_t intr_val;
	uint ret_val;
	const si_info_t *sii = SI_INFO(sih);

	origidx = si_coreidx(sih);

	INTR_OFF(sii, &intr_val);
	/* Validate the core idx */
	si_setcoreidx(sih, coreidx);

	ret_val = si_wrapperreg(sih, offset, mask, val);

	/* return to the original core */
	si_setcoreidx(sih, origidx);
	INTR_RESTORE(sii, &intr_val);
	return ret_val;
}

/* cleanup the timer from the host when ARM is been halted
 * without a chance for ARM cleanup its resources
 * If left not cleanup, Intr from a software timer can still
 * request HT clk when ARM is halted.
 */
uint32
si_pmu_res_req_timer_clr(si_t *sih)
{
	uint32 mask;

	mask = PRRT_REQ_ACTIVE | PRRT_INTEN | PRRT_HT_REQ;
	mask <<= 14;
	/* clear mask bits */
	pmu_corereg(sih, SI_CC_IDX, res_req_timer, mask, 0);
	/* readback to ensure write completes */
	return pmu_corereg(sih, SI_CC_IDX, res_req_timer, 0, 0);
}

/** turn on/off rfldo */
void
si_pmu_rfldo(si_t *sih, bool on)
{
}

/* Caller of this function should make sure is on PCIE core
 * Used in pciedev.c.
 */
void
si_pcie_disable_oobselltr(const si_t *sih)
{
	ASSERT(si_coreid(sih) == PCIE2_CORE_ID);
	if (PCIECOREREV(sih->buscorerev) >= 23)
		si_wrapperreg(sih, AI_OOBSELIND74, ~0, 0);
	else
		si_wrapperreg(sih, AI_OOBSELIND30, ~0, 0);
}

void
si_pcie_ltr_war(const si_t *sih)
{
}

void
si_pcie_hw_LTR_war(const si_t *sih)
{
}

void
si_pciedev_reg_pm_clk_period(const si_t *sih)
{
}

void
si_pciedev_crwlpciegen2(const si_t *sih)
{
}

void
si_pcie_prep_D3(const si_t *sih, bool enter_D3)
{
}

#if defined(AXI_TIMEOUTS) || defined(AXI_TIMEOUTS_NIC)
uint32
si_clear_backplane_to_per_core(si_t *sih, uint coreid, uint coreunit, void * wrap)
{
	if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS)) {
		return ai_clear_backplane_to_per_core(sih, coreid, coreunit, wrap);
	}

	return AXI_WRAP_STS_NONE;
}
#endif /* AXI_TIMEOUTS || AXI_TIMEOUTS_NIC */

uint32
si_clear_backplane_to(si_t *sih)
{
	if ((CHIPTYPE(sih->socitype) == SOCI_AI) ||
		(CHIPTYPE(sih->socitype) == SOCI_DVTBUS)) {
		return ai_clear_backplane_to(sih);
	}

	return 0;
}

void
si_update_backplane_timeouts(const si_t *sih, bool enable, uint32 timeout_exp,
	uint32 cid)
{
#if defined(AXI_TIMEOUTS) || defined(AXI_TIMEOUTS_NIC)
	/* Enable only for AXI */
	if (CHIPTYPE(sih->socitype) != SOCI_AI) {
		return;
	}

	ai_update_backplane_timeouts(sih, enable, timeout_exp, cid);
#endif /* AXI_TIMEOUTS  || AXI_TIMEOUTS_NIC */
}

/*
 * This routine adds the AXI timeouts for
 * chipcommon, pcie and ARM slave wrappers
 */
void
si_slave_wrapper_add(si_t *sih)
{
#if defined(AXI_TIMEOUTS) || defined(AXI_TIMEOUTS_NIC)
	uint32 axi_to = 0;

	/* Enable only for AXI */
	if ((CHIPTYPE(sih->socitype) != SOCI_AI) &&
		(CHIPTYPE(sih->socitype) != SOCI_DVTBUS)) {
		return;
	}

	axi_to = AXI_TO_VAL;

	/* All required slave wrappers are added in ai_scan */
	ai_update_backplane_timeouts(sih, TRUE, axi_to, 0);

#ifdef DISABLE_PCIE2_AXI_TIMEOUT
	ai_update_backplane_timeouts(sih, FALSE, 0, PCIE_CORE_ID);
	ai_update_backplane_timeouts(sih, FALSE, 0, PCIE2_CORE_ID);
#endif // endif

#endif /* AXI_TIMEOUTS  || AXI_TIMEOUTS_NIC */

}

void
si_pll_sr_reinit(si_t *sih)
{
}

void
si_pll_closeloop(si_t *sih)
{
#if defined(SAVERESTORE)
	uint32 data;

	BCM_REFERENCE(data);

	/* disable PLL open loop operation */
	switch (CHIPID(sih->chip)) {
		case BCM4369_CHIP_GRPID:
		case BCM4362_CHIP_GRPID:
		case BCM4368_CHIP_GRPID:
		case BCM4376_CHIP_GRPID:
		case BCM4378_CHIP_GRPID:
		case BCM4385_CHIP_GRPID:
		case BCM4387_CHIP_GRPID:
		case BCM4388_CHIP_GRPID:
		case BCM4389_CHIP_GRPID:
		case BCM4397_CHIP_GRPID:
			si_pmu_chipcontrol(sih, PMU_CHIPCTL1,
				PMU_CC1_ENABLE_CLOSED_LOOP_MASK, PMU_CC1_ENABLE_CLOSED_LOOP);
			break;
		default:
			/* any unsupported chip bail */
			return;
	}
#endif // endif
}

uint32 si_findcoreidx_by_axiid(const si_t *sih, uint32 axiid)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		return ai_findcoreidx_by_axiid(sih, axiid);
	return 0;
}

void
si_wrapper_get_last_error(const si_t *sih, uint32 *error_status, uint32 *core, uint32 *lo,
        uint32 *hi, uint32 *id)
{
#if defined(AXI_TIMEOUTS_NIC) || defined(AXI_TIMEOUTS)
	if (CHIPTYPE(sih->socitype) == SOCI_AI)
		ai_wrapper_get_last_error(sih, error_status, core, lo, hi, id);
#endif /* (AXI_TIMEOUTS_NIC) || (AXI_TIMEOUTS) */
	return;
}

uint32
si_get_axi_timeout_reg(const si_t *sih)
{
#if defined(AXI_TIMEOUTS_NIC) || defined(AXI_TIMEOUTS)
	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		return ai_get_axi_timeout_reg();
	}
#endif /* (AXI_TIMEOUTS_NIC) || (AXI_TIMEOUTS) */
	return 0;
}

#if defined(BCMSRPWR) && !defined(BCMSRPWR_DISABLED)
bool _bcmsrpwr = TRUE;
#else
bool _bcmsrpwr = FALSE;
#endif // endif

#define PWRREQ_OFFSET(sih)	OFFSETOF(chipcregs_t, powerctl)

static void
si_corereg_pciefast_write(const si_t *sih, uint regoff, uint val)
{
	volatile uint32 *r = NULL;
	const si_info_t *sii = SI_INFO(sih);

	ASSERT((BUSTYPE(sih->bustype) == PCI_BUS));

	r = (volatile uint32 *)((volatile char *)sii->curmap +
		PCI_16KB0_PCIREGS_OFFSET + regoff);

	W_REG(sii->osh, r, val);
}

static uint
si_corereg_pciefast_read(const si_t *sih, uint regoff)
{
	volatile uint32 *r = NULL;
	const si_info_t *sii = SI_INFO(sih);

	ASSERT((BUSTYPE(sih->bustype) == PCI_BUS));

	r = (volatile uint32 *)((volatile char *)sii->curmap +
		PCI_16KB0_PCIREGS_OFFSET + regoff);

	return R_REG(sii->osh, r);
}

uint32
si_srpwr_request(const si_t *sih, uint32 mask, uint32 val)
{
	const si_info_t *sii = SI_INFO(sih);
	uint32 r, offset = (BUSTYPE(sih->bustype) == SI_BUS) ?
		OFFSETOF(chipcregs_t, powerctl) : PWRREQ_OFFSET(sih);
	uint32 mask2 = mask;
	uint32 val2 = val;
	volatile uint32 *fast_srpwr_addr = (volatile uint32 *)((uintptr)SI_ENUM_BASE(sih)
					 + (uintptr)offset);

	if (FWSIGN_ENAB()) {
		return 0;
	}

	if (mask || val) {
		mask <<= SRPWR_REQON_SHIFT;
		val  <<= SRPWR_REQON_SHIFT;

		/* Return if requested power request is already set */
		if (BUSTYPE(sih->bustype) == SI_BUS) {
			r = R_REG(sii->osh, fast_srpwr_addr);
		} else {
			r = si_corereg_pciefast_read(sih, offset);
		}

		if ((r & mask) == val) {
			return r;
		}

		r = (r & ~mask) | val;

		if (BUSTYPE(sih->bustype) == SI_BUS) {
			W_REG(sii->osh, fast_srpwr_addr, r);
			r = R_REG(sii->osh, fast_srpwr_addr);
		} else {
			si_corereg_pciefast_write(sih, offset, r);
			r = si_corereg_pciefast_read(sih, offset);
		}

		if (val2) {
			if ((r & (mask2 << SRPWR_STATUS_SHIFT)) ==
			(val2 << SRPWR_STATUS_SHIFT)) {
				return r;
			}
			si_srpwr_stat_spinwait(sih, mask2, val2);
		}
	} else {
		if (BUSTYPE(sih->bustype) == SI_BUS) {
			r = R_REG(sii->osh, fast_srpwr_addr);
		} else {
			r = si_corereg_pciefast_read(sih, offset);
		}
	}

	return r;
}

#ifdef CORE_PWRUP_WAR
uint32
si_srpwr_request_on_rev80(si_t *sih, uint32 mask, uint32 val, uint32 ucode_awake)
{
	const si_info_t *sii = SI_INFO(sih);
	uint32 r, offset = OFFSETOF(chipcregs_t, powerctl); /* Same 0x1e8 per core */
	uint cidx = (BUSTYPE(sih->bustype) == SI_BUS) ? SI_CC_IDX : sih->buscoreidx;
	uint32 mask2 = mask;
	uint32 val2 = val;
	volatile uint32 *fast_srpwr_addr = (volatile uint32 *)((uintptr)SI_ENUM_BASE(sih)
					 + (uintptr)offset);
	if (mask || val) {
		mask <<= SRPWR_REQON_SHIFT;
		val  <<= SRPWR_REQON_SHIFT;

		/* Return if requested power request is already set */
		if (BUSTYPE(sih->bustype) == SI_BUS) {
			r = R_REG(sii->osh, fast_srpwr_addr);
		} else {
			r = si_corereg(sih, cidx, offset, 0, 0);
		}

		if ((r & mask) == val) {
			W_REG(sii->osh, fast_srpwr_addr, r);
			return r;
		}

		r = (r & ~mask) | val;

		if (BUSTYPE(sih->bustype) == SI_BUS) {
			W_REG(sii->osh, fast_srpwr_addr, r);
			r = R_REG(sii->osh, fast_srpwr_addr);
		} else {
			r = si_corereg(sih, cidx, offset, ~0, r);
		}

		if (val2) {

			/*
			 * When ucode is not requested to be awake by FW,
			 * the power status may indicate ON due to FW or
			 * ucode's earlier power down request is not
			 * honored yet. In such case, FW will find the
			 * power status high at this stage, but as it is in
			 * transition (from ON to OFF), it may go down any
			 * time and lead to AXI slave error. Hence we need
			 * a fixed delay to cross any such transition state.
			 */
			if (ucode_awake == 0) {
				hnd_delay(SRPWR_UP_DOWN_DELAY);
			}

			if ((r & (mask2 << SRPWR_STATUS_SHIFT)) ==
			(val2 << SRPWR_STATUS_SHIFT)) {
				return r;
			}
			si_srpwr_stat_spinwait(sih, mask2, val2);
		}
	} else {
		if (BUSTYPE(sih->bustype) == SI_BUS) {
			r = R_REG(sii->osh, fast_srpwr_addr);
		} else {
			r = si_corereg(sih, cidx, offset, 0, 0);
		}
		SPINWAIT(((R_REG(sii->osh, fast_srpwr_addr) &
				(mask2 << SRPWR_REQON_SHIFT)) != 0),
				PMU_MAX_TRANSITION_DLY);
	}

	return r;
}
#endif /* CORE_PWRUP_WAR */

uint32
si_srpwr_stat_spinwait(const si_t *sih, uint32 mask, uint32 val)
{
	const si_info_t *sii = SI_INFO(sih);
	uint32 r, offset = (BUSTYPE(sih->bustype) == SI_BUS) ?
		OFFSETOF(chipcregs_t, powerctl) : PWRREQ_OFFSET(sih);
	volatile uint32 *fast_srpwr_addr = (volatile uint32 *)((uintptr)SI_ENUM_BASE(sih)
					 + (uintptr)offset);

	if (FWSIGN_ENAB()) {
		return 0;
	}
	ASSERT(mask);
	ASSERT(val);

	/* spinwait on pwrstatus */
	mask <<= SRPWR_STATUS_SHIFT;
	val <<= SRPWR_STATUS_SHIFT;

	if (BUSTYPE(sih->bustype) == SI_BUS) {
		SPINWAIT(((R_REG(sii->osh, fast_srpwr_addr) & mask) != val),
			PMU_MAX_TRANSITION_DLY);
		r = R_REG(sii->osh, fast_srpwr_addr) & mask;
		ASSERT(r == val);
	} else {
		SPINWAIT(((si_corereg_pciefast_read(sih, offset) & mask) != val),
			PMU_MAX_TRANSITION_DLY);
		r = si_corereg_pciefast_read(sih, offset) & mask;
		ASSERT(r == val);
	}

	r = (r >> SRPWR_STATUS_SHIFT) & SRPWR_DMN_ALL_MASK(sih);

	return r;
}

uint32
si_srpwr_stat(si_t *sih)
{
	uint32 r, offset = (BUSTYPE(sih->bustype) == SI_BUS) ?
		OFFSETOF(chipcregs_t, powerctl) : PWRREQ_OFFSET(sih);
	uint cidx = (BUSTYPE(sih->bustype) == SI_BUS) ? SI_CC_IDX : sih->buscoreidx;

	if (BUSTYPE(sih->bustype) == SI_BUS) {
		r = si_corereg(sih, cidx, offset, 0, 0);
	} else {
		r = si_corereg_pciefast_read(sih, offset);
	}

	r = (r >> SRPWR_STATUS_SHIFT) & SRPWR_DMN_ALL_MASK(sih);

	return r;
}

uint32
si_srpwr_domain(si_t *sih)
{
	uint32 r, offset = (BUSTYPE(sih->bustype) == SI_BUS) ?
		OFFSETOF(chipcregs_t, powerctl) : PWRREQ_OFFSET(sih);
	uint cidx = (BUSTYPE(sih->bustype) == SI_BUS) ? SI_CC_IDX : sih->buscoreidx;

	if (FWSIGN_ENAB()) {
		return 0;
	}

	if (BUSTYPE(sih->bustype) == SI_BUS) {
		r = si_corereg(sih, cidx, offset, 0, 0);
	} else {
		r = si_corereg_pciefast_read(sih, offset);
	}

	r = (r >> SRPWR_DMN_ID_SHIFT) & SRPWR_DMN_ID_MASK;

	return r;
}

uint8
si_srpwr_domain_wl(si_t *sih)
{
	return SRPWR_DMN1_ARMBPSD;
}

bool
si_srpwr_cap(si_t *sih)
{
	if (FWSIGN_ENAB()) {
		return FALSE;
	}

	/* If domain ID is non-zero, chip supports power domain control */
	return si_srpwr_domain(sih) != 0 ? TRUE : FALSE;
}

uint32
si_srpwr_domain_all_mask(const si_t *sih)
{
	uint32 mask = SRPWR_DMN0_PCIE_MASK |
	              SRPWR_DMN1_ARMBPSD_MASK |
	              SRPWR_DMN2_MACAUX_MASK |
	              SRPWR_DMN3_MACMAIN_MASK;

	if (si_scan_core_present(sih)) {
		mask |= SRPWR_DMN4_MACSCAN_MASK;
	}

	return mask;
}

uint32
si_srpwr_bt_status(si_t *sih)
{
	uint32 r;
	uint32 offset = (BUSTYPE(sih->bustype) == SI_BUS) ?
		OFFSETOF(chipcregs_t, powerctl) : PWRREQ_OFFSET(sih);
	uint32 cidx = (BUSTYPE(sih->bustype) == SI_BUS) ? SI_CC_IDX : sih->buscoreidx;

	if (BUSTYPE(sih->bustype) == SI_BUS) {
		r = si_corereg(sih, cidx, offset, 0, 0);
	} else {
		r = si_corereg_pciefast_read(sih, offset);
	}

	r = (r >> SRPWR_BT_STATUS_SHIFT) & SRPWR_BT_STATUS_MASK;

	return r;
}
/* Utility API to read/write the raw registers with absolute address.
 * This function can be invoked from either FW or host driver.
 */
uint32
si_raw_reg(const si_t *sih, uint32 reg, uint32 val, uint32 wrire_req)
{
	const si_info_t *sii = SI_INFO(sih);
	uint32 address_space = reg & ~0xFFF;
	volatile uint32 * addr = (void*)(uintptr)(reg);
	uint32 prev_value = 0;
	uint32 cfg_reg = 0;

	if (sii == NULL) {
		return 0;
	}

	/* No need to translate the absolute address on SI bus */
	if (BUSTYPE(sih->bustype) == SI_BUS) {
		goto skip_cfg;
	}

	/* This API supports only the PCI host interface */
	if (BUSTYPE(sih->bustype) != PCI_BUS) {
		return ID32_INVALID;
	}

	if (PCIE_GEN2(sii)) {
		/* Use BAR0 Secondary window is PCIe Gen2.
		 * Set the secondary BAR0 Window to current register of interest
		 */
		addr = (volatile uint32*)(((volatile uint8*)sii->curmap) +
			PCI_SEC_BAR0_WIN_OFFSET + (reg & 0xfff));
		cfg_reg = PCIE2_BAR0_CORE2_WIN;

	} else {
		/* PCIe Gen1 do not have secondary BAR0 window.
		 * reuse the BAR0 WIN2
		 */
		addr = (volatile uint32*)(((volatile uint8*)sii->curmap) +
			PCI_BAR0_WIN2_OFFSET + (reg & 0xfff));
		cfg_reg = PCI_BAR0_WIN2;
	}

	prev_value = OSL_PCI_READ_CONFIG(sii->osh, cfg_reg, 4);

	if (prev_value != address_space) {
		OSL_PCI_WRITE_CONFIG(sii->osh, cfg_reg,
			sizeof(uint32), address_space);
	} else {
		prev_value = 0;
	}

skip_cfg:
	if (wrire_req) {
		W_REG(sii->osh, addr, val);
	} else {
		val = R_REG(sii->osh, addr);
	}

	if (prev_value) {
		/* Restore BAR0 WIN2 for PCIE GEN1 devices */
		OSL_PCI_WRITE_CONFIG(sii->osh,
			cfg_reg, sizeof(uint32), prev_value);
	}

	return val;
}

uint8
si_lhl_ps_mode(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	return sii->lhl_ps_mode;
}

uint8
si_hib_ext_wakeup_isenab(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);
	return sii->hib_ext_wakeup_enab;
}

static void
si_oob_war_BT_F1(si_t *sih)
{
	uint origidx = si_coreidx(sih);
	volatile void *regs;

	if (FWSIGN_ENAB()) {
		return;
	}
	regs = si_setcore(sih, AXI2AHB_BRIDGE_ID, 0);
	ASSERT(regs);
	BCM_REFERENCE(regs);

	si_wrapperreg(sih, AI_OOBSELINA30, 0xF00, 0x300);

	si_setcoreidx(sih, origidx);
}

#ifdef UART_TRAP_DBG
void
si_dump_APB_Bridge_registers(const si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		ai_dump_APB_Bridge_registers(sih);
	}
}
#endif /* UART_TRAP_DBG */

void
si_force_clocks(const si_t *sih, uint clock_state)
{
	if (CHIPTYPE(sih->socitype) == SOCI_AI) {
		ai_force_clocks(sih, clock_state);
	}
}

/* Indicates to the siutils how the PICe BAR0 is mappend,
 * used for siutils to arrange BAR0 window management,
 * for PCI NIC driver.
 *
 * Here is the current scheme, which are all using BAR0:
 *
 * id     enum       wrapper
 * ====   =========  =========
 *    0   0000-0FFF  1000-1FFF
 *    1   4000-4FFF  5000-5FFF
 *    2   9000-9FFF  A000-AFFF
 * >= 3   not supported
 */
void
si_set_slice_id(si_t *sih, uint8 slice)
{
	si_info_t *sii = SI_INFO(sih);

	sii->slice = slice;
}

uint8
si_get_slice_id(const si_t *sih)
{
	const si_info_t *sii = SI_INFO(sih);

	return sii->slice;
}

bool
BCMRAMFN(si_scan_core_present)(const si_t *sih)
{
	return ((si_numcoreunits(sih, D11_CORE_ID) >= 2) &&
		(si_numcoreunits(sih, SR_CORE_ID) > 4));
}

void
si_jtag_udr_pwrsw_main_toggle(si_t *sih, bool on)
{
}
