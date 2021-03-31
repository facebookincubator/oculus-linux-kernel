#ifndef _SWD_REGISTERS_NRF_H
#define _SWD_REGISTERS_NRF_H

#define SWD_NRF_NVMC_READY          0x4001E400
#define SWD_NRF_NVMC_CONFIG         0x4001E504
#define SWD_NRF_NVMC_ERASEPAGE      0x4001E508
#define SWD_NRF_NVMC_ERASEALL       0x4001E50C
#define SWD_NRF_NVMC_ERASEUICR      0x4001E514
#define SWD_NRF_POWER               0x40000544

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

#endif
