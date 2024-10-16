// link to datasheet:
// http://www.mouser.com/ds/2/588/AS5510_Datasheet_EN_v3-272817.pdf

#ifndef _AS5510_H__
#define _AS5510_H__

// AS5510 regs
#define DATA1_REG 0x0
#define DATA2_REG 0x1
#define CONFIG_REG 0x2
#define OFFSET1_REG 0x3
#define OFFSET2_REG 0x4
#define SENSITIVITY_REG 0xB

// definitions for better code readability
#define DATA_BITS 10

// value in SENSITIVITY_REG register
#define SENSITIVITY_COARSE 0 // 50 mT (default)
#define SENSITIVITY_FINE 2   // 12.5 mT

// third bit in CONFIG_REG
#define DO_NOT_AVERAGE 0 // default
#define DO_AVERAGE 1

// second bit in CONFIG_REG
#define POSITIVE_POLARITY 0 // default
#define NEGATIVE_POLARITY 1

// first bit in CONFIG_REG
#define POWER_ON 0 // default
#define POWER_LOW 1

// macros to convert regs to their values
#define DATA_REGS_TO_VAL(DATA1, DATA2)                                         \
((int)(((((int)(DATA2)) & 0x3) << 8) | (((int)(DATA1)) & 0xFF)))
#define DATA2_GET_PARITY(DATA2) (((DATA2) >> 2) & 1)
#define DATA2_IS_COMPENSATING(DATA2) (((DATA2) >> 3) & 1)

#define SENSITIVITY_REG_TO_VAL(SENSITIVITY) ((int)((SENSITIVITY)&0x3))
#define IS_VALID_SENSITIVITY(SENSITIVITY) ((unsigned int)(SENSITIVITY) < 4)

#define CONFIG_REG_TO_VAL(CONFIG) ((int)((CONFIG)&0x7))
#define IS_VALID_CONFIG(CONFIG) ((unsigned int)(CONFIG) < 8)
#define CONFIG_VAL_TO_REG(AVERAGE, POLARITY, POWER)                            \
((AVERAGE << 2) | (POLARITY << 1) | POWER)

#endif // _AS5510_H__
