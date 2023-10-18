/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_SENSOR_LITE_CMN_HEADER_
#define _CAM_SENSOR_LITE_CMN_HEADER_

#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/list.h>

#include <dt-bindings/msm-camera.h>
#include <media/cam_sensor.h>
#include <media/cam_req_mgr.h>
#include "cam_rpmsg.h"

#define MAX_POWER_CONFIG    12

#define MAX_SENSOR_VIRTUAL_CHANNEL_COUNT 2
#define MAX_SETTINGS 200
#define MAX_STREAMS  4

#define MAX_CSIPHY_SETTINGS 200

typedef uint32_t virtual_channel;

enum sensor_stream_type {
	STREAM_BLOB             = 0,
	STREAM_IMAGE            = 1,
	STREAM_PDAF             = 2,
	STREAM_HDR              = 3,
	STREAM_META             = 4,
	STREAM_IMAGE_SHORT      = 5,
	STREAM_IMAGE_MIDDLE     = 6,
	STREAM_IMAGE_1          = 7,
	STREAM_IMAGE_2          = 8,
	STREAM_IMAGE_3          = 9,
	STREAM_MAX              = 10
};

enum mode_identifier {
	IRIS,
	EYE
};

enum version_type {
	VERSION0,
	VERSION1,
	VERSION2,
	VERSIONMAX
};

enum probe_status {
	PROBE_SUCCESS,
	PROBE_FAILURE
};

enum phy_type {
	CPHY,
	DPHY
};

enum camera_power_seq_type {
	SEQ_TYPE_SENSOR_MCLK,
	SEQ_TYPE_SENSOR_VANA,
	SEQ_TYPE_SENSOR_VDIG,
	SEQ_TYPE_SENSOR_VIO,
	SEQ_TYPE_SENSOR_VAF,
	SEQ_TYPE_SENSOR_VAF_PWDM,
	SEQ_TYPE_SENSOR_CUSTOM_REG1,
	SEQ_TYPE_SENSOR_CUSTOM_REG2,
	SEQ_TYPE_SENSOR_RESET,
	SEQ_TYPE_SENSOR_STANDBY,
	SEQ_TYPE_SENSOR_CUSTOM_GPIO1,
	SEQ_TYPE_SENSOR_CUSTOM_GPIO2,
	SEQ_TYPE_SENSOR_VANA1,
	SEQ_TYPE_SENSOR_SEQ_TYPE_MAX
};

enum operation_type {
	OPERATION_TYPE_WRITE,
	OPERATION_TYPE_WRITE_BURST,
	OPERATION_TYPE_WRITE_SEQ,
	OPERATION_TYPE_READ
};

enum i2c_command_type {
	I2C_CMD_TYPE_READ,
	I2C_CMD_TYPE_WRITE,
	I2C_CMD_TYPE_POLL,
	I2C_CMD_TYPE_READ_BURST,
	I2C_CMD_TYPE_WRITE_BURST
};

/**
 * struct i2c_commands - i2c command structure
 *
 * @cmd_type         :    type of i2c command
 * @reg_addr         :    address of register
 * @reg_value        :    value in the register
 * @delay            :    delay in us
 */
struct i2c_commands {
	uint16_t                     cmd_type;
	uint16_t                     reg_addr;
	uint16_t                     reg_value;
	uint16_t                     delay;
} __packed;

/**
 * struct probe_command - Probe command structure
 *
 * @command_tag         :    tag of the command
 * @reserved            :
 * @num_commands        :    number of i2c commands
 * @i2c_commands        :    i2c command configuration
 */
struct probe_command {
	uint16_t                   command_tag;
	uint16_t                   reserved;
	uint16_t                   num_commands;
	uint16_t                   reserved1;
	struct   i2c_commands      i2c_command;
} __packed;

/**
 * struct cam_camera_slave_info - Slave camera information
 *
 * @sensor_slave_addr          :    address of the slave sensor
 * @sensor_id_reg_addr         :    address of id register
 * @sensor_id                  :    chip id
 * @sensor_id_mask             :    id mask
 */
struct camera_slave_info {
	uint16_t sensor_slave_addr;
	uint16_t sensor_id_reg_addr;
	uint16_t sensor_id;
	uint16_t sensor_id_mask;
} __packed;

/**
 * struct sensor_power_setting - Power settings
 *
 * @seq_type                    :    type of camera power sequence
 * @seq_val                     :    value of sequence
 * @config_val                  :
 * @delay                       :    delay in us
 * @data                        :
 */
struct sensor_power_setting {
	uint32_t                    seq_type;
	uint32_t                    seq_val;
	uint32_t                    config_val;
	uint32_t                    delay;
	uint32_t                    data[10];
} __packed;


/**
 * sensor_lite_header - common header for all the payloads
 *
 * @hpkt_header       : packet header for helios
 *                    : version    7 bit
 *                    : direction  1 bit
 *                    : num_packet 8 bit
 *                    : packet_sz  16 bit
 * @hpkt_preamble     : preable header for helios
 *                    : type : 8 bit
 *                    : size : 16 bit
 *                    : reserved : 8 bits
 * @version           : version of this packet
 * @tag               : tag to identify the packet type
 * @is_pkt_active     : whether pkt is active
 * @reserved          : for future use
 * @size              : size of entire packet
 */
struct sensor_lite_header {
	struct cam_slave_pkt_hdr            hpkt_header;
	struct cam_rpmsg_slave_payload_desc hpkt_preamble;
	uint8_t                             version;
	uint8_t                             tag;
	uint16_t                            is_pkt_active : 1;
	uint16_t                            reserved      : 15;
	uint32_t                            size;
} __packed;

/**
 * struct probe_payload - Explains about the payload packet configuration
 *
 * @header                      : common header
 * @slave_info                  : slave info
 * @sensor_physical_id          : sensor_physical camera id
 * @power_up_settings_offset    : offset of power up settings command array
 * @power_up_settings_size      : size of power up settings command array
 * @power_down_settings_offset  : offset of power down settings command array
 * @power_down_settings_size    : size of power down settings command array
 */
struct probe_payload_v2 {
	struct sensor_lite_header    header;
	struct camera_slave_info     slave_info;
	uint32_t                     sensor_physical_id;
	uint32_t                     power_up_settings_offset;
	uint32_t                     power_up_settings_size;
	uint32_t                     power_down_settings_offset;
	uint32_t                     power_down_settings_size;
}  __packed;


/**
 * phy_info - Explains about the remote phy information
 *
 * @phy_id              :  Phy HW id
 * @lane_assign         :  Assignment of phy lanes
 * @lane_count          :  Number of phy lanes
 * @combo_mode          :  Is phy connected in combo mode
 * @phy_type            :  Phy type (CPHY/DPHY)
 * @sensor_physical_id  :  Sensor physical id for this phy
 */
struct phy_info {
	uint32_t    phy_id;
	uint16_t    lane_assign;
	uint16_t    lane_count;
	uint32_t    combo_mode;
	uint16_t    phy_type;
	uint16_t    sensor_physical_id;
};

/**
 * phy_reg_setting - Explains about the PHY register info
 *
 * @reg_addr  :  Register address
 * @reg_data  :  Register value
 * @delay     :  Delay in us
 */
struct phy_reg_setting {
	uint32_t reg_addr;
	uint32_t reg_data;
	uint32_t delay;
};

/**
 * phy_reg_config - Explains about the PHY register settings sequence
 *
 * @num_settings  :  Number of reg settings
 * @offset        :  Offset of reg settings
 */
struct phy_reg_config {
	uint32_t                num_settings;
	uint32_t                offset;
};

/**
 * phy_header - Header for the remote phy payload
 *
 * @hpkt_header    :  Packet header for helios
 * @hpkt_preamble  :  Preamble header for helios
 * @version        :  Packet version
 * @size           :  Packet size
 */
struct phy_header {
	struct cam_slave_pkt_hdr             hpkt_header;
	struct cam_rpmsg_slave_payload_desc  hpkt_preamble;
	uint32_t                             version;
	uint32_t                             size;
};

/**
 * phy_payload - Remote phy payload
 *
 * @phy_header          :  Header
 * @phy_id              :  Phy HW id
 * @sensor_physical_id  :  Sensor physical id
 * @phy_lane_en_config  :  Phy lane enable reg config
 * @phy_lane_config     :  Phy lane reg config
 * @phy_reset_config    :  Phy reset config
 */
struct phy_payload {
	struct phy_header     header;
	uint32_t              phy_id;
	uint32_t              sensor_physical_id;
	struct phy_reg_config phy_lane_en_config;
	struct phy_reg_config phy_lane_config;
	struct phy_reg_config phy_reset_config;
};

struct phy_reg_settings {
	uint32_t                  num_lane_en_settings;
	struct   phy_reg_setting* lane_en;
	uint32_t                  num_lane_settings;
	struct   phy_reg_setting* lane;
	uint32_t                  num_reset_settings;
	struct   phy_reg_setting* reset;
};

/**
 * phy_acq_payload - Remote phy dummy payload
 *
 * @phy_header          :  Header
 * @phy_id              :  Phy HW id
 * @sensor_physical_id  :  Sensor physical id
 */
struct phy_acq_payload {
	struct phy_header header;
	uint32_t          phy_id;
	uint32_t          sensor_physical_id;
};

struct phy_probe_info {
	uint16_t vt;
	uint16_t phy_index;
	uint16_t num_lanes;
	uint8_t  pt;
	uint8_t  combo_mode;
	uint16_t lane_mask;
	uint16_t reserved;
} __packed;

/**
 * struct sensor_probe_response - Explains about the sensor probe entry
 *
 * @vt                          :    version
 * @sensor_id                   :    id of the sensor probed by slave
 * @status                      :    probe status
 * @phy_info                    :    the phy information
 */
struct sensor_probe_response {
	uint16_t                        vt;
	uint8_t                         sensor_id;
	uint8_t                         status;
	struct   phy_probe_info         phy_info;
};

/**
 * struct probe_response_packet - Explains the configuration of probe response packet
 *
 * @vt                          :    version
 * @probe_entry                 :    sensor probe entry configuration
 */
struct probe_response_packet {
	uint16_t                                vt;
	uint16_t                                reserved;
	struct   sensor_probe_response          probe_response;
};

/**
 * struct reg_setting - Explains about the registers information
 *
 * @slave_addr                  :    address of the slave
 * @reg_addr                    :    address of the register
 * @reg_data                    :    data in the register
 * @reg_data_type               :    type of data in register
 * @reg_addr_type               :    type of register address
 * @delay                       :    delay in us
 * @operation                   :    type of operation
 */
struct reg_setting {
	uint32_t    slave_addr;
	uint32_t    reg_addr;
	uint32_t    reg_data;
	uint32_t    reg_data_type;
	uint32_t    reg_addr_type;
	uint32_t    delay;
	uint32_t    operation;
};

/**
 * struct reg_settings - Explains about the array of registers information
 *
 * @num_settings                :    number of settings
 * @reg_set                     :    register information
 */
struct reg_settings {
	uint32_t                num_settings;
	struct  reg_setting     reg_set[MAX_SETTINGS];
};

/**
 * struct trigger_type - Explains about the trigger camera type
 *
 * @fsin_setting                :    fsin settings
 */
struct trigger_type {
	struct reg_settings     fsin_setting;
};

/**
 * struct streaming_type - Explains about the streaming camera type
 *
 * @stream_on_settings          :    the stream on settings
 * @stream_off_settings         :    the stream off settings
 * @group_hold_on_settings      :    group hold on settings
 * @group_hold_off_settings     :    group hold off settings
 */
struct streaming_type {
	struct reg_settings     stream_on_settings;
	struct reg_settings     stream_off_settings;
	struct reg_settings     group_hold_on_settings;
	struct reg_settings     group_hold_off_settings;
};

/**
 * struct camera_type - Explains about the type of camera
 *
 * @streaming_camera            :    streaming type camera configuration
 * @trigger_camera              :    trigger type camera configuration
 */
struct camera_type {
	union {
		struct streaming_type   streaming_camera;
		struct trigger_type     trigger_camera;
	};
};

/**
 * struct exposure_info - Explains about the exposure information
 *
 * @frame_length_lines_addr          :    address of register containing frame length lines
 * @line_length_pixel_clock_addr     :    address of register containing line length pixel clock
 * @coarse_int_gtime_addr            :    address of register containing coarse int gtime
 * @global_gain_addr                 :    address of register containing global gain
 * @max_analog_gain                  :    maximum analog gain
 * @max_digital_gain                 :    maximum digital gain
 * @vertical_offset                  :    vertical offset
 * @max_line_count                   :    maximum line count
 */
struct exposure_info {
	uint32_t                    frame_length_lines_addr;
	uint32_t                    line_length_pixel_clock_addr;
	uint32_t                    coarse_int_gtime_addr;
	uint32_t                    global_gain_addr;
	double                      max_analog_gain;
	double                      max_digital_gain;
	uint32_t                    vertical_offset;
	uint32_t                    max_line_count;
};

/**
 * struct frame_dimension - Explains about the dimension of the frame
 *
 * Frame dimension: contains xStart, yStart, width and height
 */
struct frame_dimension {
	uint32_t x_start;
	uint32_t y_start;
	uint32_t width;
	uint32_t height;
};

/**
 * struct sensor_aggregator_map - Explains about the sensor aggregator mapping
 *
 * @sensor                     :    sensor id
 * @aggregator                 :    aggregator id
 */
struct sensor_aggregator_map {
	virtual_channel sensor;
	virtual_channel aggregator;
};

enum sensorlite_mode_identifier {
	SENSOR_LITE_IRIS,
	SENSOR_LITE_EYE
};


/**
 * @brief sensorlite_stream_configuration
 *
 * @vc: virtual channel
 * @dt: data type
 * @bit_width: bits per pixel
 * @stream_type: type of stream
 * @stream_configuration_id: id of stream configuration
 */
struct sensorlite_stream_configuration {
	uint32_t                         vc;
	uint32_t                         dt;
	struct frame_dimension           fd;
	uint32_t                         bit_width;
	uint32_t                         stream_type;
	uint32_t                         stream_configuration_id;
} __packed;

/**
 * @brief sensorlite_stream_information
 * @stream_configuration_count : number of stream configurations
 * @stream_config              : array of stream configurations
 */
struct sensorlite_stream_information {
	uint32_t                               stream_configuration_count;
	struct sensorlite_stream_configuration stream_config[MAX_STREAMS];
} __packed;

/**
 * @brief sensorlite_resolution_cmd
 *
 * @header : common header
 * @stream_info : stream information
 * @frame_rate  : frame rate
 * @mode_id     : mode index
 * @mode_settings_offset: offset of mode settings
 * @mode_settings_count : number of mode settings
 */
struct sensorlite_resolution_cmd {
	struct sensor_lite_header            header;
	uint32_t                             sensor_id;
	struct sensorlite_stream_information stream_info;
	double                               frame_rate;
	uint32_t                             mode_id;
	uint32_t                             mode_settings_offset;
	uint32_t                             mode_settings_count;
} __packed;


/**
 * struct vc_mapping - vc mapping
 *
 * @version                     :    version of this packet
 * @reserved0                   :    starting offset of init settings in payload area
 * @reserved1                   :    reserved for future use
 * @sensor_vc                   :    virtual channel of sensor
 * @aggregator_vc               :    virtual channel of aggrigator
 */
struct vc_mapping {
	uint8_t       version;
	uint8_t       reserved0;
	uint16_t      reserved1;
	uint32_t      sensor_vc;
	uint32_t      aggregator_vc;
} __packed;

struct sensor_lite_exposure_info {
	uint32_t                        frameLengthLinesAddr;
	uint32_t                        lineLengthPixelClockAddr;
	uint32_t                        coarseIntTimeAddr;
	uint32_t                        globalGainAddr;
	double                          maxAnalogGain;
	double                          maxDigitalGain;
	uint32_t                        verticalOffset;
	uint32_t                        maxLineCount;
} __packed;

struct sensor_lite_setting_groupt_cmd {
	struct sensor_lite_header        header;
	uint32_t                         cmd_type;
	uint32_t                         time_offset;
	uint32_t                         group_regsettings_count;
	uint32_t                         group_regsettings_offset;
} __packed;

struct sensor_lite_settings_stream_cmd {
	struct sensor_lite_header        header;
	uint32_t                         channel_id;
	uint32_t                         tracker_id;
	uint32_t                         sensor_id;
	uint32_t                         sof_to_apply_min_time;
	uint32_t                         eof_to_trigger_min_time;
	uint32_t                         numcmd_groups;
	uint32_t                         settings_groupoffset;
} __packed;

struct sensor_lite_perframe_cmd {
	struct sensor_lite_header        header;
	uint32_t                         settings_id;
	uint64_t                         timestamp;
	uint32_t                         num_streams;
	uint32_t                         stream_offset;
} __packed;

struct sensor_lite_exp_ctrl_cmd {
	struct sensor_lite_header          header;
	uint32_t                           sensor_id;
	uint32_t                           exp_ctrl_setting_offset;
	uint32_t                           exp_ctrl_setting_count;
} __packed;

/**
 * @brief sensor_lite_acquire_cmd - Explains about the acquire acket
 */
struct sensor_lite_acquire_cmd {
	struct sensor_lite_header        header;
	uint32_t                         sensor_id;
	uint32_t                         power_settings_offset;
	uint32_t                         power_settings_size;
} __packed;

struct sensor_lite_release_cmd {
	struct sensor_lite_header        header;
	uint32_t                         sensor_id;
	uint32_t                         power_settings_offset;
	uint32_t                         power_settings_size;
} __packed;

struct sensor_lite_start_stop_cmd {
	struct sensor_lite_header        header;
	uint32_t                         sensor_id;
	uint32_t                         start_stop_settings_offset;
	uint32_t                         start_stop_settings_size;
} __packed;

struct slave_dest_camera_init_payload {
	struct sensor_lite_header        header;
	uint32_t                         sensor_physical_id;
	struct sensor_lite_exposure_info expInfo;
	uint32_t                         resolution_data_offset;
	uint32_t                         resolution_data_count;
	uint32_t                         init_settings_offset;
	uint32_t                         init_settings_count;
	uint32_t                         streamon_settings_offset;
	uint32_t                         streamon_settings_count;
	uint32_t                         streamoff_settings_offset;
	uint32_t                         streamoff_settings_count;
	uint32_t                         groupholdon_settings_offset;
	uint32_t                         groupholdon_settings_count;
	uint32_t                         groupholdoff_settings_offset;
	uint32_t                         groupholdoff_settings_count;
	uint32_t                         fsin_settings_offset;
	uint32_t                         fsin_settings_count;
} __packed;


/**
 * struct host_dest_camera_init_payload - Explains about the host camera sesnor information
 *
 * @header                      :    common header
 * @sensor_physical_id          :    sensor id
 * @vc_map                      :    the vc mapping information
 * @init_setting_offset         :    starting offset of init settings in payload area
 *					array of regisettings of type (reg_setting)
 * @init_setting_count          :    number of init settings in payload area
 */
struct host_dest_camera_init_payload_v2 {
	struct sensor_lite_header    header;
	uint32_t                     sensor_physical_id;
	struct  vc_mapping           vc_map;
	uint32_t                     init_setting_offset;
	uint32_t                     init_setting_count;
} __packed;

struct host_dest_camera_init_payload_v3 {
	struct sensor_lite_header    header;
	uint32_t                     sensor_physical_id;
	struct  vc_mapping           vc_map[4];
	uint32_t                     init_setting_offset;
	uint32_t                     init_setting_count;
} __packed;

enum {
	SENSORLITE_CMD_TYPE_INVALID         = 0,
	SENSORLITE_CMD_TYPE_PROBE           = 1,
	SENSORLITE_CMD_TYPE_SLAVEDESTINIT   = 2,
	SENSORLITE_CMD_TYPE_HOSTDESTINIT    = 3,
	SENSORLITE_CMD_TYPE_EXPOSUREUPDATE  = 4,
	SENSORLITE_CMD_TYPE_RESOLUTIONINFO  = 5,
	SENSORLITE_CMD_TYPE_DEBUG           = 6,
	SENSORLITE_CMD_TYPE_PERFRAME        = 7,
	SENSORLITE_CMD_TYPE_START           = 8,
	SENSORLITE_CMD_TYPE_STOP            = 9,
	SENSORLITE_CMD_TYPE_MAX             = 0xA,
};

#endif
