/*
 * HND Run Time Environment ioctl.
 *
 * Copyright (C) 2022, Broadcom.
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

#ifndef _dngl_ioctl_h_
#define _dngl_ioctl_h_

/* ==== Dongle IOCTLs i.e. non-d11 IOCTLs ==== */

#ifndef _rte_ioctl_h_
/* ================================================================ */
/* These are the existing ioctls moved from src/include/rte_ioctl.h */
/* ================================================================ */

/* RTE IOCTL definitions for generic ether devices */
#define RTEIOCTLSTART		0x8901
#define RTEGHWADDR		0x8901
#define RTESHWADDR		0x8902
#define RTEGMTU			0x8903
#define RTEGSTATS		0x8904
#define RTEGALLMULTI		0x8905
#define RTESALLMULTI		0x8906
#define RTEGPROMISC		0x8907
#define RTESPROMISC		0x8908
#define RTESMULTILIST	0x8909
#define RTEGUP			0x890A
#define RTEGPERMADDR		0x890B
#define RTEDEVPWRSTCHG		0x890C	/* Device pwr state change for PCIedev */
#define RTEDEVPMETOGGLE		0x890D	/* Toggle PME# to wake up the host */
#define RTEDEVTIMESYNC		0x890E	/* Device TimeSync */
#define RTEDEVDSNOTIFY		0x890F	/* Bus DS state notification */
#define RTED11DMALPBK_INIT	0x8910	/* D11 DMA loopback init */
#define RTED11DMALPBK_UNINIT	0x8911	/* D11 DMA loopback uninit */
#define RTED11DMALPBK_RUN	0x8912	/* D11 DMA loopback run */
#define RTEDEVTSBUFPOST		0x8913	/* Async interface for tsync buffer post */
#define RTED11DMAHOSTLPBK_RUN	0x8914  /* D11 DMA host memory loopback run */
#define RTEDEVGETTSF		0x8915  /* Get device TSF */
#define RTEDURATIONUNIT		0x8916  /* Duration unit */
#define RTEWRITE_WAR_REGS	0x8917  /* write workaround regs */
#define RTEDEVRMPMK		0x8918  /* Remove PMK */
#define RTEDEVDBGVAL		0x8919  /* Set debug val */
#define RTEDEVVIFDEL		0x891A  /* Delete virtual cfgs */
#define RTEQUIESCEFORCE		0x891B	/* Force D11 core into quiesce */
#define RTEPTMENABLE		0x891C	/* Start/stop PTM time stamping in TX/RX status */
/* Ensure last RTE IOCTL define val is assigned to RTEIOCTLEND */
#define RTEIOCTLEND		0x891C  /* LAST RTE IOCTL value */

#define RTE_IOCTL_QUERY		0x00
#define RTE_IOCTL_SET		0x01
#define RTE_IOCTL_OVL_IDX_MASK	0x1e
#define RTE_IOCTL_OVL_RSV	0x20
#define RTE_IOCTL_OVL		0x40
#define RTE_IOCTL_OVL_IDX_SHIFT	1

enum hnd_ioctl_cmd {
	HND_RTE_DNGL_IS_SS = 1, /* true if device connected at super speed */

	/* PCIEDEV specific wl <--> bus ioctls */
	BUS_GET_VAR = 2,
	BUS_SET_VAR = 3,
	BUS_FLUSH_RXREORDER_Q = 4,
	BUS_SET_LTR_STATE = 5,
	BUS_FLUSH_CHAINED_PKTS = 6,
	BUS_SET_COPY_COUNT = 7,
	BUS_UPDATE_FLOW_PKTS_MAX = 8,
	BUS_UPDATE_EXTRA_TXLFRAGS = 9,
	BUS_UPDATE_FRWD_RESRV_BUFCNT = 10,
	BUS_PCIE_CONFIG_ACCESS = 11,
	BUS_HC_EVENT_MASK_UPDATE = 12,
	BUS_SET_MAC_WAKE_STATE = 13,
	BUS_FRWD_PKT_RXCMPLT = 14,
	BUS_PCIE_LATENCY_ENAB = 15, /* to enable latency feature in pcie */
	BUS_GET_MAXITEMS = 16,
	BUS_SET_BUS_CSO_CAP = 17,	/* Update the CSO cap from wl layer to bus layer */
	BUS_DUMP_RX_DMA_STALL_RELATED_INFO = 18,
	BUS_UPDATE_RESVPOOL_STATE = 19,	/* Update resvpool state */
	BUS_GET_MAX_RING_NUM = 20 /* Get the Max num of the Tx rings */
};

#define SDPCMDEV_SET_MAXTXPKTGLOM	1
#define RTE_MEMUSEINFO_VER 0x00

typedef struct memuse_info {
	uint16 ver;			/* version of this struct */
	uint16 len;			/* length in bytes of this structure */
	uint32 tot;			/* Total memory */
	uint32 text_len;	/* Size of Text segment memory */
	uint32 data_len;	/* Size of Data segment memory */
	uint32 bss_len;		/* Size of BSS segment memory */

	uint32 arena_size;	/* Total Heap size */
	uint32 arena_free;	/* Heap memory available or free */
	uint32 inuse_size;	/* Heap memory currently in use */
	uint32 inuse_hwm;	/* High watermark of memory - reclaimed memory */
	uint32 inuse_overhead;	/* tally of allocated mem_t blocks */
	uint32 inuse_total;	/* Heap in-use + Heap overhead memory  */
	uint32 free_lwm;        /* Least free size since reclaim */
	uint32 mf_count;        /* Malloc failure count */
} memuse_info_t;

#define RTE_POOLUSEINFO_VER 0x01
typedef struct pooluse_info {
	uint16 ver;				/* version of this struct */
	uint16 len;				/* length in bytes of this structure */

	uint32 shared_count;			/* pktpool_shared pkt count */
	uint32 shared_available;		/* pktpool_shared pkt left */
	uint32 shared_max_pkt_bytes;		/* pktpool_shared max pkt size */
	uint32 shared_overhead;			/* pktpool_shared with overhead */

	uint32 lfrag_count;			/* pktpool_shared_lfrag pkt count */
	uint32 lfrag_available;			/* pktpool_shared_lfrag pkt left */
	uint32 lfrag_max_pkt_bytes;		/* pktpool_shared_lfrag max pkt size */
	uint32 lfrag_overhead;			/* pktpool_shared_lfrag with overhead */

	uint32 resvlfrag_count;			/* pktpool_resv_lfrag pkt count */
	uint32 resvlfrag_available;		/* pktpool_resv_lfrag pkt left */
	uint32 resvlfrag_max_pkt_bytes;		/* pktpool_resv_lfrag max pkt size */
	uint32 resvlfrag_overhead;		/* pktpool_resv_lfrag with overhead */

	uint32 rxlfrag_count;			/* pktpool_shared_rxlfrag pkt count */
	uint32 rxlfrag_available;		/* pktpool_shared_rxlfrag pkt left */
	uint32 rxlfrag_max_pkt_bytes;		/* pktpool_shared_rxlfrag max pkt size */
	uint32 rxlfrag_overhead;		/* pktpool_shared_rxlfrag with overhead */

	uint32 rxdata_count;			/* pktpool_shared_rxdata pkt count */
	uint32 rxdata_available;		/* pktpool_shared_rxdata pkt left */
	uint32 rxdata_max_pkt_bytes;		/* pktpool_shared_rxdata max pkt size */
	uint32 rxdata_overhead;			/* pktpool_shared_rxdata with overhead */

	uint32 alfrag_count;			/* pktpool_shared_alfrag pkt count */
	uint32 alfrag_available;		/* pktpool_shared_alfrag pkt left */
	uint32 alfrag_max_pkt_bytes;		/* pktpool_shared_alfrag max pkt size */
	uint32 alfrag_overhead;			/* pktpool_shared_alfrag with overhead */

	uint32 alfragdata_count;		/* pktpool_shared_alfrag_data pkt count */
	uint32 alfragdata_available;		/* pktpool_shared_alfrag_data pkt left */
	uint32 alfragdata_max_pkt_bytes;	/* pktpool_shared_alfrag_data max pkt size */
	uint32 alfragdata_overhead;		/* pktpool_shared_alfrag_data with overhead */

	uint32 resvalfrag_count;		/* pktpool_resv_alfrag pkt count */
	uint32 resvalfrag_available;		/* pktpool_resv_alfrag pkt left */
	uint32 resvalfrag_max_pkt_bytes;	/* pktpool_resv_alfrag max pkt size */
	uint32 resvalfrag_overhead;		/* pktpool_resv_alfrag with overhead */

	uint32 resvalfragdata_count;		/* pktpool_resv_alfrag_data pkt count */
	uint32 resvalfragdata_available;	/* pktpool_resv_alfrag_data pkt left */
	uint32 resvalfragdata_max_pkt_bytes;	/* pktpool_resv_alfrag_data max pkt size */
	uint32 resvalfragdata_overhead;		/* pktpool_resv_alfrag_data with overhead */

	uint32 total_size;			/* pktpool total size in bytes */
	uint32 total_overhead;			/* pktpool total with overhead */
} pooluse_info_t;

typedef struct memuse_ext_info {
	memuse_info_t hu;	/* heap usage */
	pooluse_info_t pu;	/* pktpool usage */
} memuse_ext_info_t;

/* Different DMA loopback modes */
#define M2M_DMA_LOOPBACK	0	/* PCIE M2M mode */
#define D11_DMA_LOOPBACK	1	/* PCIE M2M and D11 mode without ucode */
#define BMC_DMA_LOOPBACK	2	/* PCIE M2M and D11 mode with ucode */
#define M2M_NON_DMA_LOOPBACK	3	/* Non DMA(indirect) mode */
#define D11_DMA_HOST_MEM_LPBK	4	/* D11 mode */
#define M2M_DMA_WRITE_TO_RAM	6	/* PCIE M2M write to specific memory mode */
#define M2M_DMA_READ_FROM_RAM	7	/* PCIE M2M read from specific memory mode */
#define D11_DMA_WRITE_TO_RAM	8	/* D11 write to specific memory mode */
#define D11_DMA_READ_FROM_RAM	9	/* D11 read from specific memory mode */

/* For D11 DMA loopback test */
typedef struct d11_dmalpbk_init_args {
	uint8 core_num;
	uint8 lpbk_mode;
} d11_dmalpbk_init_args_t;

typedef struct d11_dmalpbk_args {
	uint8 *buf;
	int32 len;
	void *p;
	uint8 core_num;
	uint8 pad[3];
} d11_dmalpbk_args_t;

typedef enum wl_config_var {
	WL_VAR_TX_PKTFETCH_INDUCE = 1,
	WL_VAR_LAST
} wl_config_var_t;

typedef struct wl_config_buf {
	wl_config_var_t var;
	uint32 val;
} wl_config_buf_t;

/* ================================================================ */
/* These are the existing ioctls moved from src/include/rte_ioctl.h */
/* ================================================================ */
#endif /* _rte_ioctl_h_ */

/* MPU test iovar version */
#define MPU_TEST_STRUCT_VER	0

/* MPU test OP */
#define MPU_TEST_OP_READ	0
#define MPU_TEST_OP_WRITE	1
#define MPU_TEST_OP_EXECUTE	2

/* Debug iovar for MPU testing */
typedef struct mpu_test_args {
	/* version control */
	uint16 ver;
	uint16 len;	/* the length of this structure */
	/* data */
	uint32 addr;
	uint8 op;	/* see MPU_TEST_OP_XXXX */
	uint8 rsvd;
	uint16 size;	/* valid for read/write */
	uint8 val[];
} mpu_test_args_t;

/* dsec command uses bcm iov framework (bcm_iov_buf_t) for iovar input/output */
/* dsec command vesion */
#define DSEC_IOV_VERSION_V1		1u

enum desc_cmd_ids {
	DSEC_CMD_ALL			= 0u,
	DSEC_CMD_TRANSUNLOCK		= 1u,
	DSEC_CMD_SBOOT			= 2u,
	DSEC_CMD_RGNLOCK		= 3u,
	DSEC_CMD_SBOOT_DATA		= 4u
};

/* TRANSIENT unlock command */
#define TRANSIENT_UNLOCK_VER_V1		1u
#define TRANSIENT_UNLOCK_KEY_SIZE	4u	/* 4 Words of 32-bit each */
#define TRANSIENT_UNLOCK_SALT_SIZE	16u	/* 16 Words of 32-bit each */

enum desc_crypto_subcmd_ids {
	DSEC_TRUNLOCK_SUBCMD_NONE	= 0u,
	DSEC_TRUNLOCK_SUBCMD_VER	= 1u,
	DSEC_TRUNLOCK_SUBCMD_STATUS	= 2u,
	DSEC_TRUNLOCK_SUBCMD_UNLOCK	= 3u
};

typedef struct {
	uint16 version;				/* cmd structure version */
	uint16 length;				/* cmd struct len */
	uint16 subcmd_id;			/* transient_unlock sub command */
	uint16 status;				/* Unlock status */
	uint32 key[TRANSIENT_UNLOCK_KEY_SIZE];	/* otp read mode */
	uint32 salt[TRANSIENT_UNLOCK_SALT_SIZE]; /* byte offset into otp to start read */
} transient_unlock_cmd_v1_t;

/* OTP Region lock command */
#define DSEC_RGN_LOCK_VER_V1		1u

/* OTP Lock Regions */
#define DSEC_LOCK_RGN_NONE		0u
#define DSEC_LOCK_RGN_WAFER_SORT	1u
#define DSEC_LOCK_RGN_HASH_DATA		2u
#define DSEC_LOCK_RGN_FINAL_TEST	3u
#define DSEC_LOCK_RGN_AUTOLOAD		4u
#define DSEC_LOCK_RGN_UPPER_GU		5u
#define DSEC_LOCK_RGN_LOWER_GU		6u
#define DSEC_LOCK_RGN_HW_SW		7u
#define DSEC_LOCK_RGN_BT		8u
#define DSEC_LOCK_RGN_SECURE		9u
#define DSEC_LOCK_RGN_SECURE_IV		10u
#define DSEC_LOCK_RGN_SECURE_V		11u
#define DSEC_LOCK_RGN_SECURE_VI_0	12u
#define DSEC_LOCK_RGN_SECURE_VI_1	13u

/* SECURE OTP command */
#define DSEC_OTP_VER_V1			1u

enum dsec_sboot_xtlv_id {
	DSEC_OTP_XTLV_NONE			= 0u,	/* Not valid otp tag */
	DSEC_OTP_XTLV_VER			= 1u,	/* OTP region type */
	/* 2u is can be used */

	/* RNG Lock Tags: */
	DSEC_OTP_XTLV_RGN			= 3u,	/* OTP region type */
	DSEC_OTP_XTLV_DATA			= 4u,	/* OTP region lock data */

	/* SBOOT Tags: */
	DSEC_OTP_XTLV_SBOOT_FW_SIG_ENABLE	= 5u,	/* FW signing enable bit */
	DSEC_OTP_XTLV_SBOOT_FW_SIG_DISABLE	= 6u,	/* FW signing disaable bit */
	DSEC_OTP_XTLV_SBOOT_ROM_PROTECT_ENABLE	= 7u,	/* ROM protect enable bit */
	DSEC_OTP_XTLV_SBOOT_ROM_PROTECT_PATCH	= 8u,	/* ROM protect from patch */
	DSEC_OTP_XTLV_SBOOT_HOST_RD_NONSEC_EN	= 9u,	/* Host read non secure enable bit */
	DSEC_OTP_XTLV_SBOOT_HOST_RD_NONSEC_DIS	= 10u,	/* Host read non secure disable bit */
	DSEC_OTP_XTLV_SBOOT_HOST_WR_NONSEC_EN	= 11u,	/* Host write non secure enable bit */
	DSEC_OTP_XTLV_SBOOT_HOST_WR_NONSEC_DIS	= 12u,	/* Host write non secure disable bit */
	DSEC_OTP_XTLV_SBOOT_DBGREGS_PROT_ENAB	= 13u,	/* ARM DBG regs protect enable bit */
	DSEC_OTP_XTLV_SBOOT_DBGREGS_PROT_DIS	= 14u,	/* ARM DBG regs protect disable bit */
	DSEC_OTP_XTLV_SBOOT_JTAG_PROTECT_ENAB	= 15u,	/* JTAG protect disable bit */
	DSEC_OTP_XTLV_SBOOT_JTAG_PROTECT_DIS	= 16u,	/* JTAG protect re-enable bit */
	DSEC_OTP_XTLV_SBOOT_TCAM_PROTECT_SIZE	= 17u,	/* TCAM protect enable size field 8 bits */
	DSEC_OTP_XTLV_SBOOT_ACTIVATE_SECURITY	= 18u,	/* Active security enable bit */
	DSEC_OTP_XTLV_SBOOT_KEY_REVOC_BITS	= 19u,	/* Key revocation Bits field 16 bits */
	DSEC_OTP_XTLV_SBOOT_CUSTOMER_PUB_KEY_1	= 20u,	/* Customer public key 1 field 257 bits */
	DSEC_OTP_XTLV_SBOOT_CUSTOMER_PUB_KEY_2	= 21u,	/* Customer public key 2 field 257 bits */
	DSEC_OTP_XTLV_SBOOT_LOT_NUM		= 22u,	/* Chip lot num low bits [0:16] 17 bits */
	DSEC_OTP_XTLV_SBOOT_WAFER_NUM		= 23u,	/* Chip wafer num 5 bits */
	DSEC_OTP_XTLV_SBOOT_WAFER_X		= 24u,	/* Chip wafer X 9 bits */
	DSEC_OTP_XTLV_SBOOT_WAFER_Y		= 25u,	/* Chip wafer Y 9 bits */
	DSEC_OTP_XTLV_SBOOT_UNLOCK_HASH_VAL	= 26u,	/* Unlock Hash Val 128 bits */
	DSEC_OTP_XTLV_SBOOT_PRODUCTION_CHIP	= 27u,	/* Production chip bit */
	DSEC_OTP_XTLV_SBOOT_ENCRYPTION_KEY	= 28u,	/* AES wrapped fw encryption key 320 bits */
	DSEC_OTP_XTLV_SBOOT_LOT_NUM_MS		= 29u,	/* Chip lot num high bits [17:47] 31 bits */
};

#define CAPEXT_INFO_VERSION_1	(1u)
#define CAPEXT_INFO_VERSION	CAPEXT_INFO_VERSION_1

/* Top structure of capext reporting. For reporting, feature ids are used as types in XTLVs */
typedef struct {
	uint16	version;	/**< see definition of CAPEXT_INFO_VERSION */
	uint16	datalen;	/**< length of data including all paddings. */
	uint8   data [];	/**< variable length payload:
				 * 1 or more bcm_xtlv_t type of tuples.
				 * each tuple is padded to multiple of 4 bytes.
				 * 'datalen' field of this structure includes all paddings.
				 */
} capext_info_t;

/* Each feature reported in capext has a feature id. Feature id is a 16-bit value.
 * The feature id namespace is split into 3 partitions. One for BUS, the second for RTE,
 * and the third for WL. All partitions are contiguous and fixed in size
 */
#define CAPEXT_FEATURE_ID_NUM_PARTITIONS	(3u)
#define CAPEXT_FEATURE_ID_PARTITION_SIZE	(1024u)
/* Feature IDs from 3072 for capext are reserved */
#define CAPEXT_RSVD_FEATURE_ID_BASE		(3072u)

/* Bus partition */
/* The features listed in the enumeration below have subfeatures.
 * If a new feature is added/updated and that feature has sub-features that need to be reported,
 * add that feature here
 */
#define CAPEXT_BUS_FEATURE_ID_BASE		(0)
enum capext_bus_feature_ids {
	CAPEXT_BUS_FEATURE_RSVD		= (CAPEXT_BUS_FEATURE_ID_BASE + 0),
	/* BUS top level feature id to hold and report bitmaps of features with and
	 * without sub-features.
	 */
	CAPEXT_BUS_FEATURE_BUS_FEATURES	= (CAPEXT_BUS_FEATURE_ID_BASE + 1),
	/* BUS feature ids below hold and report sub-feature bitmaps of some features
	 * mentioned in top level feature id bitmap
	 */
	CAPEXT_BUS_FEATURE_PKTLAT	= (CAPEXT_BUS_FEATURE_ID_BASE + 2),
	CAPEXT_BUS_FEATURE_MAX
};

/* BUS features bit positions in top level rte feature id. Features mentioned below are reported */
enum capext_bus_feature_bitpos {
	CAPEXT_BUS_FEATURE_BITPOS_HP2P		= 0,
	CAPEXT_BUS_FEATURE_BITPOS_PTM		= 1,
	CAPEXT_BUS_FEATURE_BITPOS_PKTLAT	= 2,	/* feature with sub-features */
	CAPEXT_BUS_FEATURE_BITPOS_MAX
};

/* Packet latency sub-feature bit positions. These sub-features need to be reported */
enum capext_pktlat_subfeature_bitpos {
	CAPEXT_PKTLAT_BITPOS_META	= 0,
	CAPEXT_PKTLAT_BITPOS_IPC	= 1,
	CAPEXT_PKTLAT_BITPOS_MAX
};

/* RTE partition */
/* The features listed in the enumeration below have subfeatures.
 * If a new feature is added and that feature has sub-features that need to be reported,
 * add that feature here
 */
#define CAPEXT_RTE_FEATURE_ID_BASE		(1024u)
enum capext_rte_feature_ids {
	CAPEXT_RTE_FEATURE_RSVD		= (CAPEXT_RTE_FEATURE_ID_BASE + 0),
	/* RTE top level feature id to hold and report bitmaps of features with and
	 * without sub-features.
	 */
	CAPEXT_RTE_FEATURE_RTE_FEATURES	= (CAPEXT_RTE_FEATURE_ID_BASE + 1),
	/* RTE feature ids below hold and report sub-feature bitmaps of some features
	 * mentioned in top level feature id bitmap
	 */
	CAPEXT_RTE_FEATURE_ECOUNTERS	= (CAPEXT_RTE_FEATURE_ID_BASE + 2),
	CAPEXT_RTE_FEATURE_MAX
};

/* Ecounters sub-feature bit positions. These sub-features need to be reported */
enum capext_ecounters_subfeature_bitpos {
	CAPEXT_ECOUNTERS_BITPOS_TXHIST	= 0,
	CAPEXT_ECOUNTERS_BITPOS_ADV	= 1,
	CAPEXT_ECOUNTERS_BITPOS_PHY	= 2,
	CAPEXT_ECOUNTERS_BITPOS_PHY_CAL	= 3,
	CAPEXT_ECOUNTERS_BITPOS_MAX
};

/* RTE features bit positions in top level rte feature id. Features mentioned below are reported */
enum capext_rte_feature_bitpos {
	CAPEXT_RTE_FEATURE_BITPOS_H2D_LOG_TIME_SYNC	= 0,
	CAPEXT_RTE_FEATURE_BITPOS_HWRNG			= 1,
	CAPEXT_RTE_FEATURE_BITPOS_SPMI			= 2,
	CAPEXT_RTE_FEATURE_BITPOS_ECOUNTERS		= 3,	/* feature with sub-features */
	CAPEXT_RTE_FEATURE_BITPOS_EVENT_LOG		= 4,

	CAPEXT_RTE_FEATURE_BITPOS_LOGTRACE		= 5,
	CAPEXT_RTE_FEATURE_BITPOS_HCHK			= 6,
	CAPEXT_RTE_FEATURE_BITPOS_SMD			= 7,
	CAPEXT_RTE_FEATURE_BITPOS_ETD			= 8,
	CAPEXT_RTE_FEATURE_BITPOS_MAX
};
#endif /* _dngl_ioctl_h_ */
