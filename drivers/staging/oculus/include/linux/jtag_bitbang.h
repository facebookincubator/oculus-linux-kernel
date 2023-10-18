/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This driver manages one or more jtag chains controlled by host pins.
 * Jtag chains must be defined during setup using jtag_state structs.
 * All operations must be done from user programs using ioctls to /dev/jtag
 * Typical operation sequence is:
 * - open() the device (normally /dev/jtag)
 * - ioctl JTAG_GET_DEVICES reads how many devices in the chain
 * (repeat for each chip in the chain)
 * - ioctl JTAG_GET_ID identifies the chip
 * - ioctl JTAG_SET_IR_LENGTH sets the instruction register length
 * Before accessing the data registers, instruction registers' lenghtes
 *  MUST be programmed for all chips.
 * After this initialization, you can execute JTAG_IR_WR, JTAG_DR_RD, JTAG_DR_WR
 *  commands in any sequence.
 */

#ifndef __JTAG_BITBANG_H__
#define __JTAG_BITBANG_H__

#include <linux/firmware.h>
#include <linux/gpio/consumer.h>
#include <linux/limits.h>
#include <linux/sysfs.h>

#define JTAG_MAX_DEVICES 16
#define JTAG_CABLE_TYPE_INVALID 0
#define JTAG_CABLE_TYPE_START_INDEX 1
#define JTAG_TMS_BUFFER_SIZE 128

#define GOWIN_FS_MAX_LINES 1024u
#define GOWIN_FS_MAX_LINE_LENGTH 2296u
#define GOWIN_FS_MAX_BUF_SIZE (GOWIN_FS_MAX_LINES * GOWIN_FS_MAX_LINE_LENGTH)
#define GOWIN_FS_MIN_LINE_LENGTH ((size_t)8u * 8u)

enum jtag_cable_type { GOWIN = JTAG_CABLE_TYPE_START_INDEX };

enum jtag_tap_state {
  TEST_LOGIC_RESET = 0,
  RUN_TEST_IDLE = 1,
  SELECT_DR_SCAN = 2,
  CAPTURE_DR = 3,
  SHIFT_DR = 4,
  EXIT1_DR = 5,
  PAUSE_DR = 6,
  EXIT2_DR = 7,
  UPDATE_DR = 8,
  SELECT_IR_SCAN = 9,
  CAPTURE_IR = 10,
  SHIFT_IR = 11,
  EXIT1_IR = 12,
  PAUSE_IR = 13,
  EXIT2_IR = 14,
  UPDATE_IR = 15,
  UNKNOWN = 999
};

typedef int jtag_device_index;
typedef int jtag_device_id;
typedef u16 jtag_fpga_manufacturer_id;

struct jtag_fpga_manufacturer {
  jtag_fpga_manufacturer_id id;
  const char *manufacturer;
};

struct jtag_fpga_model {
  jtag_device_id id;
  const char *manufacturer;
  const char *part;
  const char *desc;
  int irlength;
};

struct jtag_state {
  struct mutex lock;
  struct list_head device_entry;
  struct class *jtag_class;

  enum jtag_tap_state tap_state;

  char filename[128];
  u8 tms_buffer[JTAG_TMS_BUFFER_SIZE];
  int tms_buffer_size;
  int num_tms;
  int device_index;
  enum jtag_cable_type cable_type;

  void *cable;
  struct device *dev;

  u32 device_count;
  u32 irlen_count;

  s16 irlen[JTAG_MAX_DEVICES]; /* [devices] */

  jtag_device_id device_ids[JTAG_MAX_DEVICES];
};

struct gowin_jtag_codes {
  int user_code;
  int status_reg;
};

struct gowin_jtag_fw_bitstream_header_data {
  // 0x06
  u32 id;
  // 0x10 0xff & (val >> 16)
  u8 bit_rate;
  // 0x10 0x01 & (val >> 13)
  bool compressed;
  // 0x10 0x01 & (val >> 12)
  bool done_bypass;
  // 0x0b bool
  bool security_bit;
  // 0x0A 0xFFFF & val
  u16 checksum;
  // 0x52
  u32 spi_addr;
  // 0x3B 0x01 & (val >> 23)
  bool crc_check;
  // 0x3B  0xffff & val
  u16 conf_data_length;
};

struct gowin_jtag_fw_bitstream_data {
  bool parsed;
  struct gowin_jtag_fw_bitstream_header_data headers;
  u16 header_done_line_index;
  bool header_done_found;
  char *line_data;
  u16 line_count; // Base on the ID code
  char **line_ptrs;
  //[GOWIN_FS_MAX_LINES]
  u32 address_count;
  u32 data_size;
  u32 data_bits_size;
  u8 *data;
};

struct gowin_jtag_fw_update_request {
  const struct firmware *fw;
  struct gowin_jtag_fw_bitstream_data bitstream;
};

extern int gowin_jtag_fw_update(struct jtag_state *state,
                                struct gowin_jtag_fw_update_request *request);
extern int gowin_jtag_read_codes(struct jtag_state *state,
                                 struct gowin_jtag_codes *codes);
extern int gowin_jtag_get_id(struct jtag_state *state, int *id);
extern int gowin_jtag_tck_test(struct jtag_state *state, int *arg);

extern int gowin_jtag_read_id_code(struct jtag_state *state, int *code);
extern int gowin_jtag_read_user_code(struct jtag_state *state, int *code);
extern int gowin_jtag_read_status_reg(struct jtag_state *state, int *code);

#endif
