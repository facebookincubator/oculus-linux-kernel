/* SPDX-License-Identifier: GPL-2.0 */
#ifndef SWD_REGISTERS_NRF52XXX_H
#define SWD_REGISTERS_NRF52XXX_H

#define SWD_NRF_NVMC_READY          0x4001E400
#define SWD_NRF_NVMC_CONFIG         0x4001E504
#define SWD_NRF_NVMC_ERASEPAGE      0x4001E508
#define SWD_NRF_NVMC_ERASEALL       0x4001E50C
#define SWD_NRF_NVMC_ERASEUICR      0x4001E514

#define SWD_NRF_NVMC_CONFIG_REN     0
#define SWD_NRF_NVMC_CONFIG_WEN     (1<<0)
#define SWD_NRF_NVMC_CONFIG_EEN     (1<<1)

#define SWD_NRF_NVMC_READY_BM       (1<<0)
#define SWD_NRF_NVMC_READY_Busy     (0<<0)
#define SWD_NRF_NVMC_READY_Ready    (1<<0)

#define SWD_NRF_NVMC_ERASEUICR_Start (1<<0)

#define SWD_NRF_UICR_BASE            0x10001000
#define SWD_NRF_UICR_SIZE            528
#define SWD_NRF_UICR_CUSTOMER_OFFSET 0x080

/* DP APSEL values to choose the Access Port */
#define SWD_NRF_APSEL_MEMAP             0x00
#define SWD_NRF_APSEL_CTRLAP            0x01

/* CTRL-AP registers */
#define SWD_NRF_APREG_RESET                     0x000
#define SWD_NRF_APREG_RESET_Reset               (1)
#define SWD_NRF_APREG_RESET_NoReset             (0)
#define SWD_NRF_APREG_ERASEALL                  0x004
#define SWD_NRF_APREG_ERASEALL_Start            (1)
#define SWD_NRF_APREG_ERASEALL_NoOperation      (0)
#define SWD_NRF_APREG_ERASEALLSTATUS            0x008
#define SWD_NRF_APREG_ERASEALLSTATUS_Ready      (0)
#define SWD_NRF_APREG_APPROTECTSTATUS           0x00C
#define SWD_NRF_APREG_APPROTECTSTATUS_Disabled  (1)

/* APPROTECT Disable for new nRF */
#define SWD_NRF_UICR_APPROTECT          0x10001208
#define SWD_NRF_UICR_APPROTECT_Disable  (0x5a)

#define SWD_NRF_FICR_PART    0x10000100
#define SWD_NRF_FICR_VARIANT 0x10000104

/* nRF52832-specific registers */
#define SWD_NRF52832_BPROT_DISABLEINDEBUG 0x40000608
#define SWD_NRF52832_BPROT_DISABLEINDEBUG_DisableInDebug (1<<0)

#endif
