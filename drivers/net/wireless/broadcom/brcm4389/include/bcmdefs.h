/*
 * Misc system wide definitions
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

#ifndef	_bcmdefs_h_
#define	_bcmdefs_h_

/*
 * One doesn't need to include this file explicitly, gets included automatically if
 * typedefs.h is included.
 */

/* Use BCM_REFERENCE to suppress warnings about intentionally-unused function
 * arguments or local variables.
 */
#define BCM_REFERENCE(data)	((void)(data))

/* Allow for suppressing unused variable warnings. */
#ifdef __GNUC__
#define UNUSED_VAR     __attribute__ ((unused))
#else
#define UNUSED_VAR
#endif // endif

/* GNU GCC 4.6+ supports selectively turning off a warning.
 * Define these diagnostic macros to help suppress cast-qual warning
 * until all the work can be done to fix the casting issues.
 */
#if (defined(__GNUC__) && defined(STRICT_GCC_WARNINGS) && (__GNUC__ > 4 || (__GNUC__ == \
	4 && __GNUC_MINOR__ >= 6)) || defined(__clang__))
#define GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST()              \
	_Pragma("GCC diagnostic push")			 \
	_Pragma("GCC diagnostic ignored \"-Wcast-qual\"")
#define GCC_DIAGNOSTIC_PUSH_SUPPRESS_NULL_DEREF()	 \
	_Pragma("GCC diagnostic push")			 \
	_Pragma("GCC diagnostic ignored \"-Wnull-dereference\"")
#define GCC_DIAGNOSTIC_POP()                             \
	_Pragma("GCC diagnostic pop")
#else
#define GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST()
#define GCC_DIAGNOSTIC_PUSH_SUPPRESS_NULL_DEREF()
#define GCC_DIAGNOSTIC_POP()
#endif   /* Diagnostic macros not defined */

/* Macros to allow Coverity modeling contructs in source code */
#if defined(__COVERITY__)

/* Coverity Doc:
 * Indicates to the TAINTED_SCALAR checker and the INTEGER_OVERFLOW checker
 * that a function taints its argument
 */
#define COV_TAINTED_DATA_ARG(arg)  __coverity_tainted_data_argument__(arg)

/* Coverity Doc:
 * Indicates to the TAINTED_SCALAR checker and the INTEGER_OVERFLOW checker
 * that a function is a tainted data sink for an argument.
 */
#define COV_TAINTED_DATA_SINK(arg) __coverity_tainted_data_sink__(arg)

/* Coverity Doc:
 * Models a function that cannot take a negative number as an argument. Used in
 * conjunction with other models to indicate that negative arguments are invalid.
 */
#define COV_NEG_SINK(arg)          __coverity_negative_sink__(arg)

#else

#define COV_TAINTED_DATA_ARG(arg)  do { } while (0)
#define COV_TAINTED_DATA_SINK(arg) do { } while (0)
#define COV_NEG_SINK(arg)          do { } while (0)

#endif /* __COVERITY__ */

/* Compile-time assert can be used in place of ASSERT if the expression evaluates
 * to a constant at compile time.
 */
#define STATIC_ASSERT(expr) { \
	/* Make sure the expression is constant. */ \
	typedef enum { _STATIC_ASSERT_NOT_CONSTANT = (expr) } _static_assert_e UNUSED_VAR; \
	/* Make sure the expression is true. */ \
	typedef char STATIC_ASSERT_FAIL[(expr) ? 1 : -1] UNUSED_VAR; \
}

/* Reclaiming text and data :
 * The following macros specify special linker sections that can be reclaimed
 * after a system is considered 'up'.
 * BCMATTACHFN is also used for detach functions (it's not worth having a BCMDETACHFN,
 * as in most cases, the attach function calls the detach function to clean up on error).
 */
#if defined(BCM_RECLAIM)

extern bool bcm_reclaimed;
extern bool bcm_attach_part_reclaimed;
extern bool bcm_preattach_part_reclaimed;
extern bool bcm_postattach_part_reclaimed;

#define RECLAIMED()			(bcm_reclaimed)
#define ATTACH_PART_RECLAIMED()		(bcm_attach_part_reclaimed)
#define PREATTACH_PART_RECLAIMED()	(bcm_preattach_part_reclaimed)
#define POSTATTACH_PART_RECLAIMED()	(bcm_postattach_part_reclaimed)

/* Place _fn/_data symbols in various reclaimed output sections */
#define _data	__attribute__ ((__section__ (".dataini2." #_data))) _data
#define _fn	__attribute__ ((__section__ (".textini2." #_fn), noinline)) _fn
#define BCMPREATTACHDATA(_data)	__attribute__ ((__section__ (".dataini3." #_data))) _data
#define BCMPREATTACHFN(_fn)	__attribute__ ((__section__ (".textini3." #_fn), noinline)) _fn
#define BCMPOSTATTACHDATA(_data)	__attribute__ ((__section__ (".dataini5." #_data))) _data
#define BCMPOSTATTACHFN(_fn)	__attribute__ ((__section__ (".textini5." #_fn), noinline)) _fn

/* Relocate attach symbols to save-restore region to increase pre-reclaim heap size. */
#define BCM_SRM_ATTACH_DATA(_data)    __attribute__ ((__section__ (".datasrm." #_data))) _data
#define BCM_SRM_ATTACH_FN(_fn)        __attribute__ ((__section__ (".textsrm." #_fn), noinline)) _fn

/* Explicitly place data in .rodata section so it can be write-protected after attach */
#define BCMRODATA(_data)	__attribute__ ((__section__ (".shrodata." #_data))) _data

#ifdef BCMDBG_SR
/*
 * Don't reclaim so we can compare SR ASM
 */
#define BCMPREATTACHDATASR(_data)	_data
#define BCMPREATTACHFNSR(_fn)		_fn
#define BCMATTACHDATASR(_data)		_data
#define BCMATTACHFNSR(_fn)		_fn
#else
#define BCMPREATTACHDATASR(_data)	BCMPREATTACHDATA(_data)
#define BCMPREATTACHFNSR(_fn)		BCMPREATTACHFN(_fn)
#define BCMATTACHDATASR(_data)		_data
#define BCMATTACHFNSR(_fn)		_fn
#endif // endif

#define _data	_data
#define _fn		_fn
#ifndef CONST
#define CONST	const
#endif // endif

/* Non-manufacture or internal attach function/dat */
#define	BCMNMIATTACHFN(_fn)	_fn
#define	BCMNMIATTACHDATA(_data)	_data

#if defined(BCM_CISDUMP_NO_RECLAIM)
#define	BCMCISDUMPATTACHFN(_fn)		_fn
#define	BCMCISDUMPATTACHDATA(_data)	_data
#else
#define	BCMCISDUMPATTACHFN(_fn)		BCMNMIATTACHFN(_fn)
#define	BCMCISDUMPATTACHDATA(_data)	BCMNMIATTACHDATA(_data)
#endif // endif

/* SROM with OTP support */
#if defined(BCMOTPSROM)
#define	BCMSROMATTACHFN(_fn)		_fn
#define	BCMSROMATTACHDATA(_data)	_data
#else
#define	BCMSROMATTACHFN(_fn)		BCMNMIATTACHFN(_fn)
#define	BCMSROMATTACHDATA(_data)	BCMNMIATTACHFN(_data)
#endif	/* BCMOTPSROM */

#if defined(BCM_CISDUMP_NO_RECLAIM)
#define	BCMSROMCISDUMPATTACHFN(_fn)	_fn
#define	BCMSROMCISDUMPATTACHDATA(_data)	_data
#else
#define	BCMSROMCISDUMPATTACHFN(_fn)	BCMSROMATTACHFN(_fn)
#define	BCMSROMCISDUMPATTACHDATA(_data)	BCMSROMATTACHDATA(_data)
#endif /* BCM_CISDUMP_NO_RECLAIM */

#define _fn	_fn

#else /* BCM_RECLAIM */

#define bcm_reclaimed			(1)
#define bcm_attach_part_reclaimed	(1)
#define bcm_preattach_part_reclaimed	(1)
#define bcm_postattach_part_reclaimed	(1)
#define _data		_data
#define _fn		_fn
#define BCM_SRM_ATTACH_DATA(_data)	_data
#define BCM_SRM_ATTACH_FN(_fn)		_fn
/* BCMRODATA data is written into at attach time so it cannot be in .rodata */
#define BCMRODATA(_data)	__attribute__ ((__section__ (".data." #_data))) _data
#define BCMPREATTACHDATA(_data)		_data
#define BCMPREATTACHFN(_fn)		_fn
#define BCMPOSTATTACHDATA(_data)	_data
#define BCMPOSTATTACHFN(_fn)		_fn
#define _data		_data
#define _fn			_fn
#define _fn		_fn
#define	BCMNMIATTACHFN(_fn)		_fn
#define	BCMNMIATTACHDATA(_data)		_data
#define	BCMSROMATTACHFN(_fn)		_fn
#define	BCMSROMATTACHDATA(_data)	_data
#define BCMPREATTACHFNSR(_fn)		_fn
#define BCMPREATTACHDATASR(_data)	_data
#define BCMATTACHFNSR(_fn)		_fn
#define BCMATTACHDATASR(_data)		_data
#define	BCMSROMATTACHFN(_fn)		_fn
#define	BCMSROMATTACHDATA(_data)	_data
#define	BCMCISDUMPATTACHFN(_fn)		_fn
#define	BCMCISDUMPATTACHDATA(_data)	_data
#define	BCMSROMCISDUMPATTACHFN(_fn)	_fn
#define	BCMSROMCISDUMPATTACHDATA(_data)	_data
#define CONST				const

#define RECLAIMED()			(bcm_reclaimed)
#define ATTACH_PART_RECLAIMED()		(bcm_attach_part_reclaimed)
#define PREATTACH_PART_RECLAIMED()	(bcm_preattach_part_reclaimed)
#define POSTATTACH_PART_RECLAIMED()	(bcm_postattach_part_reclaimed)

#endif /* BCM_RECLAIM */

#define BCMUCODEDATA(_data)		_data

#if defined(BCM_AQM_DMA_DESC) && !defined(BCM_AQM_DMA_DESC_DISABLED)
#define BCMUCODEFN(_fn)			_fn
#else
#define BCMUCODEFN(_fn)			_fn
#endif /* BCM_AQM_DMA_DESC */

/* This feature is for dongle builds only.
 * In Rom build use BCMFASTPATH() to mark functions that will excluded from ROM bits if
 * BCMFASTPATH_EXCLUDE_FROM_ROM flag is defined (defined by default).
 * In romoffload or ram builds all functions that marked by BCMFASTPATH() will be placed
 * in "text_fastpath" section and will be used by trap handler.
 */
#ifndef BCMFASTPATH
	#define BCMFASTPATH(_fn)	_fn
#endif /* BCMFASTPATH */

/* Use the BCMRAMFN/BCMRAMDATA() macros to tag functions/data in source that must be included in RAM
 * (excluded from ROM). This should eliminate the need to manually specify these functions/data in
 * the ROM config file. It should only be used in special cases where the function must be in RAM
 * for *all* ROM-based chips.
 */
	#define BCMRAMFN(_fn)		_fn
	#define BCMRAMDATA(_data)	_data

/* Use BCMSPECSYM() macro to tag symbols going to a special output section in the binary. */
#define BCMSPECSYM(_sym)	__attribute__ ((__section__ (".special." #_sym))) _sym

#define STATIC	static

/* functions that do not examine any values except their arguments, and have no effects except
 * the return value should use this keyword. Note that a function that has pointer arguments
 * and examines the data pointed to must not be declared as BCMCONSTFN.
 */
#ifdef __GNUC__
#define BCMCONSTFN(_fn)	__attribute__ ((const)) _fn
#else
#define BCMCONSTFN(_fn)	_fn
#endif /* __GNUC__ */

/* Bus types */
#define	SI_BUS			0	/* SOC Interconnect */
#define	PCI_BUS			1	/* PCI target */
#define	PCMCIA_BUS		2	/* PCMCIA target */
#define SDIO_BUS		3	/* SDIO target */
#define JTAG_BUS		4	/* JTAG */
#define USB_BUS			5	/* USB (does not support R/W REG) */
#define SPI_BUS			6	/* gSPI target */
#define RPC_BUS			7	/* RPC target */

/* Allows size optimization for single-bus image */
#ifdef BCMBUSTYPE
#define BUSTYPE(bus)	(BCMBUSTYPE)
#else
#define BUSTYPE(bus)	(bus)
#endif // endif

#ifdef BCMBUSCORETYPE
#define BUSCORETYPE(ct)		(BCMBUSCORETYPE)
#else
#define BUSCORETYPE(ct)		(ct)
#endif // endif

/* Allows size optimization for single-backplane image */
#ifdef BCMCHIPTYPE
#define CHIPTYPE(bus)	(BCMCHIPTYPE)
#else
#define CHIPTYPE(bus)	(bus)
#endif // endif

/* Allows size optimization for SPROM support */
#if defined(BCMSPROMBUS)
#define SPROMBUS	(BCMSPROMBUS)
#else
#define SPROMBUS	(PCI_BUS)
#endif // endif

/* Allows size optimization for single-chip image */
/* These macros are NOT meant to encourage writing chip-specific code.
 * Use them only when it is appropriate for example in PMU PLL/CHIP/SWREG
 * controls and in chip-specific workarounds.
 */
#ifdef BCMCHIPID
#define CHIPID(chip)	(BCMCHIPID)
#else
#define CHIPID(chip)	(chip)
#endif // endif

#ifdef BCMCHIPREV
#define CHIPREV(rev)	(BCMCHIPREV)
#else
#define CHIPREV(rev)	(rev)
#endif // endif

#ifdef BCMPCIEREV
#define PCIECOREREV(rev)	(BCMPCIEREV)
#else
#define PCIECOREREV(rev)	(rev)
#endif // endif

#ifdef HWA
#ifdef BCMHWAREV
#define HWACOREREV(rev)		(BCMHWAREV)
#else
#define HWACOREREV(rev)		(rev)
#endif /* BCMHWAREV */
#else /* !HWA */
#define HWA_ENAB(submodule)		(0)
#define HWA_MANAGED_FIFO(fifo)		(0)
#define HWA_NEXTPACKET(p)		(NULL)
#define HWACOREREV(rev)			(0)

/* HWA sub-module enab checks */
#define HWA_SUBMODULES_TXPOST_ENAB()		(0)
#define HWA_SUBMODULES_RXPOSTFILL_ENAB()	(0)
#define HWA_SUBMODULES_TXDMA_ENAB()		(0)
#define HWA_SUBMODULES_TXS_ENAB()		(0)
#define HWA_SUBMODULES_CPL_ENAB()		(0)
#define HWA_SUBMODULES_RXS_ENAB()		(0)

#endif /* HWA */

#ifdef BCMPMUREV
#define PMUREV(rev)	(BCMPMUREV)
#else
#define PMUREV(rev)	(rev)
#endif // endif

#ifdef BCMCCREV
#define CCREV(rev)	(BCMCCREV)
#else
#define CCREV(rev)	(rev)
#endif // endif

#ifdef BCMGCIREV
#define GCIREV(rev)	(BCMGCIREV)
#else
#define GCIREV(rev)	(rev)
#endif // endif

#ifdef BCMCR4REV
#define CR4REV(rev)	(BCMCR4REV)
#define CR4REV_GE(rev, val)	((BCMCR4REV) >= (val))
#else
#define CR4REV(rev)	(rev)
#define CR4REV_GE(rev, val)	((rev) >= (val))
#endif // endif

#ifdef BCMLHLREV
#define LHLREV(rev)	(BCMLHLREV)
#else
#define LHLREV(rev)	(rev)
#endif // endif

#ifdef BCMSPMISREV
#define SPMISREV(rev)	(BCMSPMISREV)
#else
#define	SPMISREV(rev)	(rev)
#endif // endif

/* Defines for DMA Address Width - Shared between OSL and HNDDMA */
#define DMADDR_MASK_32 0x0		/* Address mask for 32-bits */
#define DMADDR_MASK_30 0xc0000000	/* Address mask for 30-bits */
#define DMADDR_MASK_26 0xFC000000	/* Address maks for 26-bits */
#define DMADDR_MASK_0  0xffffffff	/* Address mask for 0-bits (hi-part) */

#define	DMADDRWIDTH_26  26 /* 26-bit addressing capability */
#define	DMADDRWIDTH_30  30 /* 30-bit addressing capability */
#define	DMADDRWIDTH_32  32 /* 32-bit addressing capability */
#define	DMADDRWIDTH_63  63 /* 64-bit addressing capability */
#define	DMADDRWIDTH_64  64 /* 64-bit addressing capability */

typedef struct {
	uint32 loaddr;
	uint32 hiaddr;
} dma64addr_t;

#define PHYSADDR64HI(_pa) ((_pa).hiaddr)
#define PHYSADDR64HISET(_pa, _val) \
	do { \
		(_pa).hiaddr = (_val);		\
	} while (0)
#define PHYSADDR64LO(_pa) ((_pa).loaddr)
#define PHYSADDR64LOSET(_pa, _val) \
	do { \
		(_pa).loaddr = (_val);		\
	} while (0)

#ifdef BCMDMA64OSL
typedef dma64addr_t dmaaddr_t;
#define PHYSADDRHI(_pa) PHYSADDR64HI(_pa)
#define PHYSADDRHISET(_pa, _val) PHYSADDR64HISET(_pa, _val)
#define PHYSADDRLO(_pa)  PHYSADDR64LO(_pa)
#define PHYSADDRLOSET(_pa, _val) PHYSADDR64LOSET(_pa, _val)
#define PHYSADDRTOULONG(_pa, _ulong) \
	do { \
		_ulong = ((unsigned long long)(_pa).hiaddr << 32) | ((_pa).loaddr); \
	} while (0)

#else
typedef uint32 dmaaddr_t;
#define PHYSADDRHI(_pa) (0u)
#define PHYSADDRHISET(_pa, _val)
#define PHYSADDRLO(_pa) ((_pa))
#define PHYSADDRLOSET(_pa, _val) \
	do { \
		(_pa) = (_val);			\
	} while (0)
#endif /* BCMDMA64OSL */

#define PHYSADDRISZERO(_pa) (PHYSADDRLO(_pa) == 0 && PHYSADDRHI(_pa) == 0)

/* One physical DMA segment */
typedef struct  {
	dmaaddr_t addr;
	uint32	  length;
} hnddma_seg_t;

#if defined(__linux__)
#define MAX_DMA_SEGS 8
#else
#define MAX_DMA_SEGS 4
#endif // endif

typedef struct {
	void *oshdmah; /* Opaque handle for OSL to store its information */
	uint origsize; /* Size of the virtual packet */
	uint nsegs;
	hnddma_seg_t segs[MAX_DMA_SEGS];
} hnddma_seg_map_t;

/* packet headroom necessary to accommodate the largest header in the system, (i.e TXOFF).
 * By doing, we avoid the need  to allocate an extra buffer for the header when bridging to WL.
 * There is a compile time check in wlc.c which ensure that this value is at least as big
 * as TXOFF. This value is used in dma_rxfill (hnddma.c).
 */

#ifndef BCMEXTRAHDROOM
#define BCMEXTRAHDROOM 204
#endif // endif

/* Packet alignment for most efficient SDIO (can change based on platform) */
#ifndef SDALIGN
#define SDALIGN	32
#endif // endif

/* Headroom required for dongle-to-host communication.  Packets allocated
 * locally in the dongle (e.g. for CDC ioctls or RNDIS messages) should
 * leave this much room in front for low-level message headers which may
 * be needed to get across the dongle bus to the host.  (These messages
 * don't go over the network, so room for the full WL header above would
 * be a waste.).
*/
/*
 * set the numbers to be MAX of all the devices, to avoid problems with ROM builds
 * USB BCMDONGLEHDRSZ and BCMDONGLEPADSZ is 0
 * SDIO BCMDONGLEHDRSZ 12 and BCMDONGLEPADSZ 16
*/
#define BCMDONGLEHDRSZ 12
#define BCMDONGLEPADSZ 16

#define BCMDONGLEOVERHEAD	(BCMDONGLEHDRSZ + BCMDONGLEPADSZ)

#if defined(NO_BCMDBG_ASSERT)
	#undef BCMDBG_ASSERT
	#undef BCMASSERT_LOG
#endif // endif

#if defined(BCMASSERT_LOG)
#define BCMASSERT_SUPPORT
#endif // endif

/* Macros for doing definition and get/set of bitfields
 * Usage example, e.g. a three-bit field (bits 4-6):
 *    #define <NAME>_M	BITFIELD_MASK(3)
 *    #define <NAME>_S	4
 * ...
 *    regval = R_REG(osh, &regs->regfoo);
 *    field = GFIELD(regval, <NAME>);
 *    regval = SFIELD(regval, <NAME>, 1);
 *    W_REG(osh, &regs->regfoo, regval);
 */
#define BITFIELD_MASK(width) \
		(((unsigned)1 << (width)) - 1)
#define GFIELD(val, field) \
		(((val) >> field ## _S) & field ## _M)
#define SFIELD(val, field, bits) \
		(((val) & (~(field ## _M << field ## _S))) | \
		 ((unsigned)(bits) << field ## _S))

/* define BCMSMALL to remove misc features for memory-constrained environments */
#ifdef BCMSMALL
#undef	BCMSPACE
#define bcmspace	FALSE	/* if (bcmspace) code is discarded */
#else
#define	BCMSPACE
#define bcmspace	TRUE	/* if (bcmspace) code is retained */
#endif // endif

/* ROM_ENAB_RUNTIME_CHECK may be set based upon the #define below (for ROM builds). It may also
 * be defined via makefiles (e.g. ROM auto abandon unoptimized compiles).
 */

#ifdef BCM_SH_SFLASH
	extern bool _bcm_sh_sflash;
	#define BCM_SH_SFLASH_ENAB() (_bcm_sh_sflash)
#else
	#define BCM_SH_SFLASH_ENAB()   (0)
#endif	/* BCM_SH_SFLASH */

#ifdef BCM_DELAY_ON_LTR
	extern bool _bcm_delay_on_ltr;
	#define BCM_DELAY_ON_LTR_ENAB() (_bcm_delay_on_ltr)
#else
	#define BCM_DELAY_ON_LTR_ENAB()		(0)
#endif	/* BCM_DELAY_ON_LTR */

/* Max. nvram variable table size */
#ifndef MAXSZ_NVRAM_VARS
#ifdef LARGE_NVRAM_MAXSZ
#define MAXSZ_NVRAM_VARS	(LARGE_NVRAM_MAXSZ * 2)
#else
#define LARGE_NVRAM_MAXSZ	8192
#define MAXSZ_NVRAM_VARS	(LARGE_NVRAM_MAXSZ * 2)
#endif /* LARGE_NVRAM_MAXSZ */
#endif /* !MAXSZ_NVRAM_VARS */

#ifdef BCMLFRAG /* BCMLFRAG support enab macros  */
	extern bool _bcmlfrag;
	#define BCMLFRAG_ENAB() (_bcmlfrag)
#else
	#define BCMLFRAG_ENAB()		(0)
#endif /* BCMLFRAG_ENAB */

#ifdef BCMPCIEDEV /* BCMPCIEDEV support enab macros */
extern bool _pciedevenab;
#if defined(ROM_ENAB_RUNTIME_CHECK)
	#define BCMPCIEDEV_ENAB() (_pciedevenab)
#elif defined(BCMPCIEDEV_ENABLED)
	#define BCMPCIEDEV_ENAB()	1
#else
	#define BCMPCIEDEV_ENAB()	0
#endif // endif
#else
	#define BCMPCIEDEV_ENAB()	0
#endif /* BCMPCIEDEV */

#ifdef BCMRESVFRAGPOOL /* BCMRESVFRAGPOOL support enab macros */
extern bool _resvfragpool_enab;
	#define  BCMRESVFRAGPOOL_ENAB() (_resvfragpool_enab)
#else
	#define BCMRESVFRAGPOOL_ENAB()	0
#endif /* BCMPCIEDEV */

#ifdef BCMD11HDRPOOL /* BCMD11HDRPOOL support enab macros */
extern bool _d11hdrpool_enab;
	#define  D11HDRPOOL_ENAB()	(_d11hdrpool_enab)
#else
	#define D11HDRPOOL_ENAB()	0
#endif /* BCMD11HDRPOOL */

	#define BCMSDIODEV_ENAB()	0

#ifdef BCMSPMIS
extern bool _bcmspmi_enab;
	#define	BCMSPMIS_ENAB()		(_bcmspmi_enab)
#else
	#define	BCMSPMIS_ENAB()		0
#endif /* BCMSPMIS */

/* Max size for reclaimable NVRAM array */
#ifdef DL_NVRAM
#define NVRAM_ARRAY_MAXSIZE	DL_NVRAM
#else
#define NVRAM_ARRAY_MAXSIZE	MAXSZ_NVRAM_VARS
#endif /* DL_NVRAM */

extern uint32 gFWID;

#ifdef BCMFRWDPKT /* BCMFRWDPKT support enab macros  */
	extern bool _bcmfrwdpkt;
	#define BCMFRWDPKT_ENAB() (_bcmfrwdpkt)
#else
	#define BCMFRWDPKT_ENAB()		(0)
#endif /* BCMFRWDPKT */

#ifdef BCMFRWDPOOLREORG /* BCMFRWDPOOLREORG support enab macros  */
	extern bool _bcmfrwdpoolreorg;
	#define BCMFRWDPOOLREORG_ENAB() (_bcmfrwdpoolreorg)
#else
	#define BCMFRWDPOOLREORG_ENAB()		(0)
#endif /* BCMFRWDPOOLREORG */

#ifdef BCMPOOLRECLAIM /* BCMPOOLRECLAIM support enab macros  */
	extern bool _bcmpoolreclaim;
	#define BCMPOOLRECLAIM_ENAB() (_bcmpoolreclaim)
#else
	#define BCMPOOLRECLAIM_ENAB()		(0)
#endif /* BCMPOOLRECLAIM */

/* Chip related low power flags (lpflags) */

#ifndef PAD
#define _PADLINE(line)  pad ## line
#define _XSTR(line)     _PADLINE(line)
#define PAD             _XSTR(__LINE__)
#endif // endif

#define MODULE_DETACH(var, detach_func)\
	if (var) { \
		detach_func(var); \
		(var) = NULL; \
	}
#define MODULE_DETACH_2(var1, var2, detach_func) detach_func(var1, var2)
#define MODULE_DETACH_TYPECASTED(var, detach_func) detach_func(var)

/* When building ROML image use runtime conditional to cause the compiler
 * to compile everything but not to complain "defined but not used"
 * as #ifdef would cause at the callsites.
 * In the end functions called under if (0) {} will not be linked
 * into the final binary if they're not called from other places either.
 */
#define BCM_ATTACH_REF_DECL()
#define BCM_ATTACH_REF()	(1)

/* For ROM builds, keep it in const section so that it gets ROMmed. If abandoned, move it to
 * RO section but before ro region start so that FATAL log buf doesn't use this.
 */
	#define BCMRODATA_ONTRAP(_data)	_data

typedef struct bcm_rng * bcm_rng_handle_t;

/* Use BCM_FUNC_PTR() to tag function pointers for ASLR code implementation. It will perform
 * run-time relocation of a function pointer by translating it from a physical to virtual address.
 *
 * BCM_FUNC_PTR() should only be used where the function name is referenced (corresponding to the
 * relocation entry for that symbol). It should not be used when the function pointer is invoked.
 */
void* BCM_ASLR_CODE_FNPTR_RELOCATOR(void *func_ptr);
#if defined(BCM_ASLR_CODE_FNPTR_RELOC)
	/* 'func_ptr_err_chk' performs a compile time error check to ensure that only a constant
	 * function name is passed as an argument to BCM_FUNC_PTR(). This ensures that the macro is
	 * only used for function pointer references, and not for function pointer invocations.
	 */
	#define BCM_FUNC_PTR(func) \
		({ static void *func_ptr_err_chk __attribute__ ((unused)) = (func); \
		BCM_ASLR_CODE_FNPTR_RELOCATOR(func); })
#else
	#define BCM_FUNC_PTR(func)         (func)
#endif /* BCM_ASLR_CODE_FNPTR_RELOC */

/*
 * Timestamps have this tag appended following a null byte which
 * helps comparison/hashing scripts find and ignore them.
 */
#define TIMESTAMP_SUFFIX "<TIMESTAMP>"

#ifdef STK_PRT_MMU
/* MMU main thread stack data */
#define BCM_MMU_MTH_STK_DATA(_data) __attribute__ ((__section__ (".mmu_mth_stack." #_data))) _data
#endif /* STK_PRT_MMU */

/* Special section for MMU page-tables. */
#define BCM_MMU_PAGE_TABLE_DATA(_data) \
	__attribute__ ((__section__ (".mmu_pagetable." #_data))) _data

/* Some phy initialization code/data can't be reclaimed in dualband mode */
#if defined(DBAND)
#define WLBANDINITDATA(_data)	_data
#define WLBANDINITFN(_fn)	_fn
#else
#define WLBANDINITDATA(_data)	_data
#define WLBANDINITFN(_fn)	_fn
#endif // endif

/* Tag struct members to make it explicitly clear that they are physical addresses. These are
 * typically used in data structs shared by the firmware and host code (or off-line utilities). The
 * use of the macro avoids customer visible API/name changes.
 */
#if defined(BCM_PHYS_ADDR_NAME_CONVERSION)
	#define PHYS_ADDR_N(name) name ## _phys
#else
	#define PHYS_ADDR_N(name) name
#endif // endif

#endif /* _bcmdefs_h_ */
