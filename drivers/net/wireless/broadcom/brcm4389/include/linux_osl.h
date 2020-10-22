/*
 * Linux OS Independent Layer
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

#ifndef _linux_osl_h_
#define _linux_osl_h_

#include <typedefs.h>
#define DECLSPEC_ALIGN(x)	__attribute__ ((aligned(x)))

/* Linux Kernel: File Operations: start */
extern void * osl_os_open_image(char * filename);
extern int osl_os_get_image_block(char * buf, int len, void * image);
extern void osl_os_close_image(void * image);
extern int osl_os_image_size(void *image);
/* Linux Kernel: File Operations: end */

#ifdef BCMDRIVER

/* OSL initialization */
extern osl_t *osl_attach(void *pdev, uint bustype, bool pkttag);

extern void osl_detach(osl_t *osh);
extern int osl_static_mem_init(osl_t *osh, void *adapter);
extern int osl_static_mem_deinit(osl_t *osh, void *adapter);
extern void osl_set_bus_handle(osl_t *osh, void *bus_handle);
extern void* osl_get_bus_handle(osl_t *osh);
#ifdef DHD_MAP_LOGGING
extern void osl_dma_map_dump(osl_t *osh);
#define OSL_DMA_MAP_DUMP(osh)	osl_dma_map_dump(osh)
#else
#define OSL_DMA_MAP_DUMP(osh)	do {} while (0)
#endif /* DHD_MAP_LOGGING */

/* Global ASSERT type */
extern uint32 g_assert_type;

#ifdef CONFIG_PHYS_ADDR_T_64BIT
#define PRI_FMT_x       "llx"
#define PRI_FMT_X       "llX"
#define PRI_FMT_o       "llo"
#define PRI_FMT_d       "lld"
#else
#define PRI_FMT_x       "x"
#define PRI_FMT_X       "X"
#define PRI_FMT_o       "o"
#define PRI_FMT_d       "d"
#endif /* CONFIG_PHYS_ADDR_T_64BIT */
/* ASSERT */
#ifndef ASSERT
#if defined(BCMASSERT_LOG) && !defined(BINCMP)
	#define ASSERT(exp) \
		do { if (!(exp)) osl_assert(#exp, __FILE__, __LINE__); } while (0)
extern void osl_assert(const char *exp, const char *file, int line);
#else
#ifdef __GNUC__
	#define GCC_VERSION \
		(__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if GCC_VERSION > 30100
	#define ASSERT(exp)	do {} while (0)
#else
	/* ASSERT could cause segmentation fault on GCC3.1, use empty instead */
	#define ASSERT(exp)
#endif /* GCC_VERSION > 30100 */
#endif /* __GNUC__ */
#endif // endif
#endif /* ASSERT */

#define ASSERT_FP(exp) ASSERT(exp)

/* microsecond delay */
#define	OSL_DELAY(usec)		osl_delay(usec)
extern void osl_delay(uint usec);

#define OSL_SLEEP(ms)			osl_sleep(ms)
extern void osl_sleep(uint ms);

/* PCI configuration space access macros */
#define	OSL_PCI_READ_CONFIG(osh, offset, size) \
	osl_pci_read_config((osh), (offset), (size))
#define	OSL_PCI_WRITE_CONFIG(osh, offset, size, val) \
	osl_pci_write_config((osh), (offset), (size), (val))
extern uint32 osl_pci_read_config(osl_t *osh, uint offset, uint size);
extern void osl_pci_write_config(osl_t *osh, uint offset, uint size, uint val);

/* PCI device bus # and slot # */
#define OSL_PCI_BUS(osh)	osl_pci_bus(osh)
#define OSL_PCI_SLOT(osh)	osl_pci_slot(osh)
#define OSL_PCIE_DOMAIN(osh)	osl_pcie_domain(osh)
#define OSL_PCIE_BUS(osh)	osl_pcie_bus(osh)
extern uint osl_pci_bus(osl_t *osh);
extern uint osl_pci_slot(osl_t *osh);
extern uint osl_pcie_domain(osl_t *osh);
extern uint osl_pcie_bus(osl_t *osh);
extern struct pci_dev *osl_pci_device(osl_t *osh);

// MOG-ON: BCMINTERNAL
// MOG-OFF: BCMINTERNAL

#define OSL_ACP_COHERENCE		(1<<1L)
#define OSL_FWDERBUF			(1<<2L)

/* Pkttag flag should be part of public information */
typedef struct {
	bool pkttag;
	bool mmbus;		/**< Bus supports memory-mapped register accesses */
	pktfree_cb_fn_t tx_fn;  /**< Callback function for PKTFREE */
	void *tx_ctx;		/**< Context to the callback function */
	void	*unused[3];	/**< temp fix for USBAP cftpool handle currption */
	void (*rx_fn)(void *rx_ctx, void *p);
	void *rx_ctx;
} osl_pubinfo_t;

extern void osl_flag_set(osl_t *osh, uint32 mask);
extern void osl_flag_clr(osl_t *osh, uint32 mask);
extern bool osl_is_flag_set(osl_t *osh, uint32 mask);

#define PKTFREESETCB(osh, _tx_fn, _tx_ctx)		\
	do {						\
	   ((osl_pubinfo_t*)osh)->tx_fn = _tx_fn;	\
	   ((osl_pubinfo_t*)osh)->tx_ctx = _tx_ctx;	\
	} while (0)

#define PKTFREESETRXCB(osh, _rx_fn, _rx_ctx)		\
	do {						\
	   ((osl_pubinfo_t*)osh)->rx_fn = _rx_fn;	\
	   ((osl_pubinfo_t*)osh)->rx_ctx = _rx_ctx;	\
	} while (0)

/* host/bus architecture-specific byte swap */
#define BUS_SWAP32(v)		(v)

	#define MALLOC(osh, size)	osl_malloc((osh), (size))
	#define MALLOCZ(osh, size)	osl_mallocz((osh), (size))
	#define MFREE(osh, addr, size) ({osl_mfree((osh), ((void *)addr), (size));(addr) = NULL;})
	#define VMALLOC(osh, size)	osl_vmalloc((osh), (size))
	#define VMALLOCZ(osh, size)	osl_vmallocz((osh), (size))
	#define VMFREE(osh, addr, size)	osl_vmfree((osh), (addr), (size))
	#define MALLOCED(osh)		osl_malloced((osh))
	#define MEMORY_LEFTOVER(osh) osl_check_memleak(osh)
	extern void *osl_malloc(osl_t *osh, uint size);
	extern void *osl_mallocz(osl_t *osh, uint size);
	extern void osl_mfree(osl_t *osh, void *addr, uint size);
	extern void *osl_vmalloc(osl_t *osh, uint size);
	extern void *osl_vmallocz(osl_t *osh, uint size);
	extern void osl_vmfree(osl_t *osh, void *addr, uint size);
	extern uint osl_malloced(osl_t *osh);
	extern uint osl_check_memleak(osl_t *osh);

extern int memcpy_s(void *dest, size_t destsz, const void *src, size_t n);
extern int memset_s(void *dest, size_t destsz, int c, size_t n);
#define	MALLOC_FAILED(osh)	osl_malloc_failed((osh))
extern uint osl_malloc_failed(osl_t *osh);

/* allocate/free shared (dma-able) consistent memory */
#define	DMA_CONSISTENT_ALIGN	osl_dma_consistent_align()
#define	DMA_ALLOC_CONSISTENT(osh, size, align, tot, pap, dmah) \
	osl_dma_alloc_consistent((osh), (size), (align), (tot), (pap))
#define	DMA_FREE_CONSISTENT(osh, va, size, pa, dmah) \
	osl_dma_free_consistent((osh), (void*)(va), (size), (pa))

#define	DMA_ALLOC_CONSISTENT_FORCE32(osh, size, align, tot, pap, dmah) \
	osl_dma_alloc_consistent((osh), (size), (align), (tot), (pap))
#define	DMA_FREE_CONSISTENT_FORCE32(osh, va, size, pa, dmah) \
	osl_dma_free_consistent((osh), (void*)(va), (size), (pa))

extern uint osl_dma_consistent_align(void);
extern void *osl_dma_alloc_consistent(osl_t *osh, uint size, uint16 align,
	uint *tot, dmaaddr_t *pap);
extern void osl_dma_free_consistent(osl_t *osh, void *va, uint size, dmaaddr_t pa);

/* map/unmap direction */
#define DMA_NO	0	/* Used to skip cache op */
#define	DMA_TX	1	/* TX direction for DMA */
#define	DMA_RX	2	/* RX direction for DMA */

/* map/unmap shared (dma-able) memory */
#define	DMA_UNMAP(osh, pa, size, direction, p, dmah) \
	osl_dma_unmap((osh), (pa), (size), (direction))
extern void osl_dma_flush(osl_t *osh, void *va, uint size, int direction, void *p,
	hnddma_seg_map_t *txp_dmah);
extern dmaaddr_t osl_dma_map(osl_t *osh, void *va, uint size, int direction, void *p,
	hnddma_seg_map_t *txp_dmah);
extern void osl_dma_unmap(osl_t *osh, dmaaddr_t pa, uint size, int direction);

#ifndef PHYS_TO_VIRT
#define	PHYS_TO_VIRT(pa)	osl_phys_to_virt(pa)
#endif // endif
#ifndef VIRT_TO_PHYS
#define	VIRT_TO_PHYS(va)	osl_virt_to_phys(va)
#endif // endif
extern void * osl_phys_to_virt(void * pa);
extern void * osl_virt_to_phys(void * va);

/* API for DMA addressing capability */
#define OSL_DMADDRWIDTH(osh, addrwidth) ({BCM_REFERENCE(osh); BCM_REFERENCE(addrwidth);})

#define OSL_SMP_WMB()	smp_wmb()

/* API for CPU relax */
extern void osl_cpu_relax(void);
#define OSL_CPU_RELAX() osl_cpu_relax()

extern void osl_preempt_disable(osl_t *osh);
extern void osl_preempt_enable(osl_t *osh);
#define OSL_DISABLE_PREEMPTION(osh)	osl_preempt_disable(osh)
#define OSL_ENABLE_PREEMPTION(osh)	osl_preempt_enable(osh)

#if defined(__ARM_ARCH_7A__)

	extern void osl_cache_flush(void *va, uint size);
	extern void osl_cache_inv(void *va, uint size);
	extern void osl_prefetch(const void *ptr);
	#define OSL_CACHE_FLUSH(va, len)	osl_cache_flush((void *)(va), len)
	#define OSL_CACHE_INV(va, len)		osl_cache_inv((void *)(va), len)
	#define OSL_PREFETCH(ptr)			osl_prefetch(ptr)
#else  /* !__ARM_ARCH_7A__ */
	#define OSL_CACHE_FLUSH(va, len)	BCM_REFERENCE(va)
	#define OSL_CACHE_INV(va, len)		BCM_REFERENCE(va)
	#define OSL_PREFETCH(ptr)		BCM_REFERENCE(ptr)
#endif /* !__ARM_ARCH_7A__ */

#ifdef AXI_TIMEOUTS_NIC
extern void osl_set_bpt_cb(osl_t *osh, void *bpt_cb, void *bpt_ctx);
extern void osl_bpt_rreg(osl_t *osh, ulong addr, volatile void *v, uint size);
#endif /* AXI_TIMEOUTS_NIC */

/* register access macros */
#if defined(BCMSDIO)
	#include <bcmsdh.h>
	#define OSL_WRITE_REG(osh, r, v) (bcmsdh_reg_write(osl_get_bus_handle(osh), \
		(uintptr)(r), sizeof(*(r)), (v)))
	#define OSL_READ_REG(osh, r) (bcmsdh_reg_read(osl_get_bus_handle(osh), \
		(uintptr)(r), sizeof(*(r))))
#elif defined(AXI_TIMEOUTS_NIC)
#define OSL_READ_REG(osh, r) \
	({\
		__typeof(*(r)) __osl_v; \
		osl_bpt_rreg(osh, (uintptr)(r), &__osl_v, sizeof(*(r))); \
		__osl_v; \
	})
#endif // endif

#if defined(AXI_TIMEOUTS_NIC)
	#define SELECT_BUS_WRITE(osh, mmap_op, bus_op) ({BCM_REFERENCE(osh); mmap_op;})
	#define SELECT_BUS_READ(osh, mmap_op, bus_op) ({BCM_REFERENCE(osh); bus_op;})
#else /* !AXI_TIMEOUTS_NIC */
#if defined(BCMSDIO)
	#define SELECT_BUS_WRITE(osh, mmap_op, bus_op) if (((osl_pubinfo_t*)(osh))->mmbus) \
		mmap_op else bus_op
	#define SELECT_BUS_READ(osh, mmap_op, bus_op) (((osl_pubinfo_t*)(osh))->mmbus) ? \
		mmap_op : bus_op
#else
	#define SELECT_BUS_WRITE(osh, mmap_op, bus_op) ({BCM_REFERENCE(osh); mmap_op;})
	#define SELECT_BUS_READ(osh, mmap_op, bus_op) ({BCM_REFERENCE(osh); mmap_op;})
#endif /* defined(BCMSDIO) */
#endif /* AXI_TIMEOUTS_NIC */

#define OSL_ERROR(bcmerror)	osl_error(bcmerror)
extern int osl_error(int bcmerror);

/* the largest reasonable packet buffer driver uses for ethernet MTU in bytes */
#define	PKTBUFSZ	2048   /* largest reasonable packet buffer, driver uses for ethernet MTU */

#define OSH_NULL   NULL

/*
 * BINOSL selects the slightly slower function-call-based binary compatible osl.
 * Macros expand to calls to functions defined in linux_osl.c .
 */
#include <linuxver.h>           /* use current 2.4.x calling conventions */
#include <linux/kernel.h>       /* for vsn/printf's */
#include <linux/string.h>       /* for mem*, str* */
extern uint64 osl_sysuptime_us(void);
#define OSL_SYSUPTIME()		((uint32)jiffies_to_msecs(jiffies))
#define OSL_SYSUPTIME_US()	osl_sysuptime_us()
extern uint64 osl_localtime_ns(void);
extern void osl_get_localtime(uint64 *sec, uint64 *usec);
extern uint64 osl_systztime_us(void);
#define OSL_LOCALTIME_NS()	osl_localtime_ns()
#define OSL_GET_LOCALTIME(sec, usec)	osl_get_localtime((sec), (usec))
#define OSL_SYSTZTIME_US()	osl_systztime_us()
#define	printf(fmt, args...)	printk(fmt , ## args)
#include <linux/kernel.h>	/* for vsn/printf's */
#include <linux/string.h>	/* for mem*, str* */
/* bcopy's: Linux kernel doesn't provide these (anymore) */
#define	bcopy_hw(src, dst, len)		memcpy((dst), (src), (len))
#define	bcopy_hw_async(src, dst, len)	memcpy((dst), (src), (len))
#define	bcopy_hw_poll_for_completion()
#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define	bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
#define	bzero(b, len)		memset((b), '\0', (len))

#if defined(CONFIG_SOC_EXYNOS9830)
extern int exynos_pcie_l1_exit(int ch_num);
#endif /* CONFIG_SOC_EXYNOS9830	*/

/* register access macros */

#ifdef CONFIG_64BIT
/* readq is defined only for 64 bit platform */
#if defined(CONFIG_SOC_EXYNOS9830)
#define R_REG(osh, r) (\
	SELECT_BUS_READ(osh, \
		({ \
			__typeof(*(r)) __osl_v = 0; \
			exynos_pcie_l1_exit(0); \
			BCM_REFERENCE(osh);	\
			switch (sizeof(*(r))) { \
				case sizeof(uint8):	__osl_v = \
					readb((volatile uint8*)(r)); break; \
				case sizeof(uint16):	__osl_v = \
					readw((volatile uint16*)(r)); break; \
				case sizeof(uint32):	__osl_v = \
					readl((volatile uint32*)(r)); break; \
				case sizeof(uint64):	__osl_v = \
					readq((volatile uint64*)(r)); break; \
			} \
			__osl_v; \
		}), \
		OSL_READ_REG(osh, r)) \
)
#else
#define R_REG(osh, r) (\
	SELECT_BUS_READ(osh, \
		({ \
			__typeof(*(r)) __osl_v = 0; \
			BCM_REFERENCE(osh);	\
			switch (sizeof(*(r))) { \
				case sizeof(uint8):	__osl_v = \
					readb((volatile uint8*)(r)); break; \
				case sizeof(uint16):	__osl_v = \
					readw((volatile uint16*)(r)); break; \
				case sizeof(uint32):	__osl_v = \
					readl((volatile uint32*)(r)); break; \
				case sizeof(uint64):	__osl_v = \
					readq((volatile uint64*)(r)); break; \
			} \
			__osl_v; \
		}), \
		OSL_READ_REG(osh, r)) \
)
#endif /* CONFIG_SOC_EXYNOS9830 */
#else /* !CONFIG_64BIT */
#define R_REG(osh, r) (\
	SELECT_BUS_READ(osh, \
		({ \
			__typeof(*(r)) __osl_v = 0; \
			switch (sizeof(*(r))) { \
				case sizeof(uint8):	__osl_v = \
					readb((volatile uint8*)(r)); break; \
				case sizeof(uint16):	__osl_v = \
					readw((volatile uint16*)(r)); break; \
				case sizeof(uint32):	__osl_v = \
					readl((volatile uint32*)(r)); break; \
			} \
			__osl_v; \
		}), \
		OSL_READ_REG(osh, r)) \
)
#endif /* CONFIG_64BIT */

#ifdef CONFIG_64BIT
/* writeq is defined only for 64 bit platform */
#if defined(CONFIG_SOC_EXYNOS9830)
#define W_REG(osh, r, v) do { \
	SELECT_BUS_WRITE(osh, \
		({ \
			exynos_pcie_l1_exit(0); \
			switch (sizeof(*(r))) { \
				case sizeof(uint8):	writeb((uint8)(v), \
						(volatile uint8*)(r)); break; \
				case sizeof(uint16):	writew((uint16)(v), \
						(volatile uint16*)(r)); break; \
				case sizeof(uint32):	writel((uint32)(v), \
						(volatile uint32*)(r)); break; \
				case sizeof(uint64):	writeq((uint64)(v), \
						(volatile uint64*)(r)); break; \
			} \
		 }), \
		(OSL_WRITE_REG(osh, r, v))); \
	} while (0)
#else
#define W_REG(osh, r, v) do { \
	SELECT_BUS_WRITE(osh, \
		switch (sizeof(*(r))) { \
			case sizeof(uint8):	writeb((uint8)(v), (volatile uint8*)(r)); break; \
			case sizeof(uint16):	writew((uint16)(v), (volatile uint16*)(r)); break; \
			case sizeof(uint32):	writel((uint32)(v), (volatile uint32*)(r)); break; \
			case sizeof(uint64):	writeq((uint64)(v), (volatile uint64*)(r)); break; \
		}, \
		(OSL_WRITE_REG(osh, r, v))); \
	} while (0)
#endif /* CONFIG_SOC_EXYNOS9830 */
#else /* !CONFIG_64BIT */
#define W_REG(osh, r, v) do { \
	SELECT_BUS_WRITE(osh, \
		switch (sizeof(*(r))) { \
			case sizeof(uint8):	writeb((uint8)(v), (volatile uint8*)(r)); break; \
			case sizeof(uint16):	writew((uint16)(v), (volatile uint16*)(r)); break; \
			case sizeof(uint32):	writel((uint32)(v), (volatile uint32*)(r)); break; \
		}, \
		(OSL_WRITE_REG(osh, r, v))); \
	} while (0)
#endif /* CONFIG_64BIT */

#define	AND_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) & (v))
#define	OR_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) | (v))

/* bcopy, bcmp, and bzero functions */
#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define	bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
#define	bzero(b, len)		memset((b), '\0', (len))

/* uncached/cached virtual address */
#define OSL_UNCACHED(va)	((void *)va)
#define OSL_CACHED(va)		((void *)va)

#define OSL_PREF_RANGE_LD(va, sz) BCM_REFERENCE(va)
#define OSL_PREF_RANGE_ST(va, sz) BCM_REFERENCE(va)

/* get processor cycle count */
#if defined(__i386__)
#define	OSL_GETCYCLES(x)	rdtscl((x))
#else
#define OSL_GETCYCLES(x)	((x) = 0)
#endif /* __i386__ */

/* dereference an address that may cause a bus exception */
#define	BUSPROBE(val, addr)	({ (val) = R_REG(NULL, (addr)); 0; })

/* map/unmap physical to virtual I/O */
#if !defined(CONFIG_MMC_MSM7X00A)
#define	REG_MAP(pa, size)	ioremap_nocache((unsigned long)(pa), (unsigned long)(size))
#else
#define REG_MAP(pa, size)       (void *)(0)
#endif /* !defined(CONFIG_MMC_MSM7X00A */
#define	REG_UNMAP(va)		iounmap((va))

/* shared (dma-able) memory access macros */
#define	R_SM(r)			*(r)
#define	W_SM(r, v)		(*(r) = (v))
#define	OR_SM(r, v)		(*(r) |= (v))
#define	BZERO_SM(r, len)	memset((r), '\0', (len))

/* Because the non BINOSL implemenation of the PKT OSL routines are macros (for
 * performance reasons),  we need the Linux headers.
 */
#include <linuxver.h>		/* use current 2.4.x calling conventions */

#define OSL_RAND()		osl_rand()
extern uint32 osl_rand(void);

#define	DMA_FLUSH(osh, va, size, direction, p, dmah) \
	osl_dma_flush((osh), (va), (size), (direction), (p), (dmah))
#define	DMA_MAP(osh, va, size, direction, p, dmah) \
	osl_dma_map((osh), (va), (size), (direction), (p), (dmah))

#else /* ! BCMDRIVER */

/* ASSERT */
	#define ASSERT(exp)	do {} while (0)

#define ASSERT_FP(exp) ASSERT(exp)

/* MALLOC and MFREE */
#define MALLOC(o, l) malloc(l)
#define MFREE(o, p, l) free(p)
#include <stdlib.h>

/* str* and mem* functions */
#include <string.h>

/* *printf functions */
#include <stdio.h>

/* bcopy, bcmp, and bzero */
extern void bcopy(const void *src, void *dst, size_t len);
extern int bcmp(const void *b1, const void *b2, size_t len);
extern void bzero(void *b, size_t len);
#endif /* ! BCMDRIVER */

typedef struct sk_buff_head PKT_LIST;
#define PKTLIST_INIT(x)		skb_queue_head_init((x))
#define PKTLIST_ENQ(x, y)	skb_queue_head((struct sk_buff_head *)(x), (struct sk_buff *)(y))
#define PKTLIST_DEQ(x)		skb_dequeue((struct sk_buff_head *)(x))
#define PKTLIST_UNLINK(x, y)	skb_unlink((struct sk_buff *)(y), (struct sk_buff_head *)(x))
#define PKTLIST_FINI(x)		skb_queue_purge((struct sk_buff_head *)(x))

#ifndef _linuxver_h_
typedef struct timer_list_compat timer_list_compat_t;
#endif /* _linuxver_h_ */
typedef struct osl_timer {
	timer_list_compat_t *timer;
	bool   set;
} osl_timer_t;

typedef void (*linux_timer_fn)(ulong arg);

extern osl_timer_t * osl_timer_init(osl_t *osh, const char *name, void (*fn)(void *arg), void *arg);
extern void osl_timer_add(osl_t *osh, osl_timer_t *t, uint32 ms, bool periodic);
extern void osl_timer_update(osl_t *osh, osl_timer_t *t, uint32 ms, bool periodic);
extern bool osl_timer_del(osl_t *osh, osl_timer_t *t);

#ifdef BCMDRIVER
typedef atomic_t osl_atomic_t;
#define OSL_ATOMIC_SET(osh, v, x)	atomic_set(v, x)
#define OSL_ATOMIC_INIT(osh, v)		atomic_set(v, 0)
#define OSL_ATOMIC_INC(osh, v)		atomic_inc(v)
#define OSL_ATOMIC_INC_RETURN(osh, v)	atomic_inc_return(v)
#define OSL_ATOMIC_DEC(osh, v)		atomic_dec(v)
#define OSL_ATOMIC_DEC_RETURN(osh, v)	atomic_dec_return(v)
#define OSL_ATOMIC_READ(osh, v)		atomic_read(v)
#define OSL_ATOMIC_ADD(osh, v, x)	atomic_add(v, x)

#ifndef atomic_set_mask
#define OSL_ATOMIC_OR(osh, v, x)	atomic_or(x, v)
#define OSL_ATOMIC_AND(osh, v, x)	atomic_and(x, v)
#else
#define OSL_ATOMIC_OR(osh, v, x)	atomic_set_mask(x, v)
#define OSL_ATOMIC_AND(osh, v, x)	atomic_clear_mask(~x, v)
#endif // endif
#endif /* BCMDRIVER */

extern void *osl_spin_lock_init(osl_t *osh);
extern void osl_spin_lock_deinit(osl_t *osh, void *lock);
extern unsigned long osl_spin_lock(void *lock);
extern void osl_spin_unlock(void *lock, unsigned long flags);
extern unsigned long osl_spin_lock_irq(void *lock);
extern void osl_spin_unlock_irq(void *lock, unsigned long flags);
extern unsigned long osl_spin_lock_bh(void *lock);
extern void osl_spin_unlock_bh(void *lock, unsigned long flags);

extern void *osl_mutex_lock_init(osl_t *osh);
extern void osl_mutex_lock_deinit(osl_t *osh, void *lock);
extern unsigned long osl_mutex_lock(void *lock);
void osl_mutex_unlock(void *lock, unsigned long flags);
#endif	/* _linux_osl_h_ */
