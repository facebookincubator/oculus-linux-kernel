/*
 *
 * FocalTech fts TouchScreen driver.
 *
 * Copyright (c) 2012-2019, Focaltech Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*****************************************************************************
*
* File Name: focaltech_upgrade_ft5422u.c
*
* Author: Focaltech Driver Team
*
* Created: 2017-07-22
*
* Abstract:
*
* Reference:
*
*****************************************************************************/
/*****************************************************************************
* 1.Included header files
*****************************************************************************/
#include "../focaltech_flash.h"

/*****************************************************************************
* Static function prototypes
*****************************************************************************/

u8 pb_file_ft2388[] = {
#include "../include/pramboot/FT2388_Pramboot_20191230.i"
};




static int fts_pram_ecc_cal_algo_ft2388(
    u32 start_addr,
    u32 ecc_length)
{
    int ret = 0;
    int i = 0;
    int ecc = 0;
    u8 val[2] = { 0 };
    u8 tmp = 0;
    u8 cmd[FTS_ROMBOOT_CMD_ECC_NEW_LEN] = { 0 };

    FTS_INFO("read out pramboot checksum");

    cmd[0] = FTS_ROMBOOT_CMD_ECC;
    cmd[1] = BYTE_OFF_16(start_addr);
    cmd[2] = BYTE_OFF_8(start_addr);
    cmd[3] = BYTE_OFF_0(start_addr);
    cmd[4] = BYTE_OFF_16(ecc_length);
    cmd[5] = BYTE_OFF_8(ecc_length);
    cmd[6] = BYTE_OFF_0(ecc_length);
    ret = fts_write(cmd, FTS_ROMBOOT_CMD_ECC_NEW_LEN);
    if (ret < 0) {
        FTS_ERROR("write pramboot ecc cal cmd fail");
        return ret;
    }

    cmd[0] = FTS_ROMBOOT_CMD_ECC_FINISH;
    for (i = 0; i < FTS_ECC_FINISH_TIMEOUT; i++) {
        msleep(1);
        ret = fts_read(cmd, 1, val, 1);
        if (ret < 0) {
            FTS_ERROR("ecc_finish read cmd fail");
            return ret;
        }
        tmp = FTS_ROMBOOT_CMD_ECC_FINISH_OK_A5;
        if (tmp == val[0])
            break;
    }
    if (i >= FTS_ECC_FINISH_TIMEOUT) {
        FTS_ERROR("wait ecc finish fail");
        return -EIO;
    }

    cmd[0] = FTS_ROMBOOT_CMD_ECC_READ;
    ret = fts_read(cmd, 1, val, 2);
    if (ret < 0) {
        FTS_ERROR("read pramboot ecc fail");
        return ret;
    }

    ecc = ((u16)(val[0] << 8) + val[1]) & 0x0000FFFF;
    return ecc;
}


/************************************************************************
* Name: fts_fwupg_get_boot_state_ft2388
* Brief: read boot id(rom/pram/bootloader), confirm boot environment
* Input:
* Output:
* Return: return 0 if success, otherwise return error code
***********************************************************************/
static int fts_fwupg_get_boot_state_ft2388(
    enum FW_STATUS *fw_sts)
{
    int ret = 0;
    u8 cmd[4] = { 0 };
    u32 cmd_len = 0;
    u8 val[2] = { 0 };

    FTS_INFO("**********read boot id**********");

    cmd[0] = FTS_CMD_START1;
    cmd[1] = FTS_CMD_START2;
    cmd_len = 2;
    ret = fts_write(cmd, cmd_len);
    if (ret < 0) {
        FTS_ERROR("write 55 cmd fail");
        return ret;
    }

    msleep(FTS_CMD_START_DELAY);
    cmd[0] = FTS_CMD_READ_ID;
    cmd[1] = cmd[2] = cmd[3] = 0x00;
    cmd_len = FTS_CMD_READ_ID_LEN_INCELL;

    ret = fts_read(cmd, cmd_len, val, 2);
    if (ret < 0) {
        FTS_ERROR("write 90 cmd fail");
        return ret;
    }
    FTS_INFO("read boot id:0x%02x%02x", val[0], val[1]);

    if ((val[0] == 0x23) && (val[1] == 0x88)) {
        FTS_INFO("tp run in romboot");
        *fw_sts = FTS_RUN_IN_ROM;
    } else if ((val[0] == 0x23) && (val[1] == 0xA8)) {
        FTS_INFO("tp run in pramboot");
        *fw_sts = FTS_RUN_IN_PRAM;
    } else if ((val[0] == 0) && (val[1] == 0)) {
        FTS_INFO("tp run in bootloader");
        *fw_sts = FTS_RUN_IN_BOOTLOADER;
    }

    return 0;
}


static int fts_pram_start_ft2388(void)
{
    u8 cmd = FTS_ROMBOOT_CMD_START_APP;
    int ret = 0;

    FTS_INFO("remap to start pramboot");

    ret = fts_write(&cmd, 1);
    if (ret < 0) {
        FTS_ERROR("write start pram cmd fail");
        return ret;
    }
    msleep(FTS_DELAY_PRAMBOOT_START);

    return 0;
}



/************************************************************************
* Name: fts_fwupg_check_state_ft2388
* Brief: confirm tp run in which mode: romboot/pramboot/bootloader
* Input:
* Output:
* Return: return true if state is match, otherwise return false
***********************************************************************/
static bool fts_fwupg_check_state_ft2388(enum FW_STATUS rstate)
{
    int ret = 0;
    int i = 0;
    enum FW_STATUS cstate = FTS_RUN_IN_ERROR;

    for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
        ret = fts_fwupg_get_boot_state_ft2388(&cstate);
        /* FTS_DEBUG("fw state=%d, retries=%d", cstate, i); */
        if (cstate == rstate)
            return true;
        msleep(FTS_DELAY_READ_ID);
    }

    return false;
}


static int fts_pram_write_buf_ft2388(u8 *buf, u32 len)
{
    int ret = 0;
    int ecc_flag = 0;
    u32 i = 0;
    u32 j = 0;
    u32 offset = 0;
    u32 remainder = 0;
    u32 packet_number;
    u32 packet_len = 0;
    u8 packet_buf[FTS_FLASH_PACKET_LENGTH + FTS_CMD_WRITE_LEN] = { 0 };
    u16 ecc_new = 0;
    u16 ecc_buf = 0;
    int ecc_in_host = 0;
    u32 cmdlen = 0;

    FTS_INFO("write pramboot to pram");

    FTS_INFO("pramboot len=%d", len);
    if ((len < PRAMBOOT_MIN_SIZE) || (len > PRAMBOOT_MAX_SIZE)) {
        FTS_ERROR("pramboot length(%d) fail", len);
        return -EINVAL;
    }

    packet_number = len / FTS_FLASH_PACKET_LENGTH;
    remainder = len % FTS_FLASH_PACKET_LENGTH;
    if (remainder > 0)
        packet_number++;
    packet_len = FTS_FLASH_PACKET_LENGTH;

    for (i = 0; i < packet_number; i++) {
        offset = i * FTS_FLASH_PACKET_LENGTH;
        /* last packet */
        if ((i == (packet_number - 1)) && remainder)
            packet_len = remainder;

        packet_buf[0] = FTS_ROMBOOT_CMD_WRITE;
        packet_buf[1] = BYTE_OFF_16(offset);
        packet_buf[2] = BYTE_OFF_8(offset);
        packet_buf[3] = BYTE_OFF_0(offset);

        packet_buf[4] = BYTE_OFF_8(packet_len);
        packet_buf[5] = BYTE_OFF_0(packet_len);
        cmdlen = 6;

        for (j = 0; j < packet_len; j++) {
            packet_buf[cmdlen + j] = buf[offset + j];
            ecc_flag++;
            if (ecc_flag == 1)
                ecc_buf = ((ecc_buf | buf[offset + j]) << 8);
            if (ecc_flag == 2) {
                ecc_buf += buf[offset + j];
                ecc_new ^= ecc_buf;
                ecc_buf = 0;
                ecc_flag = 0;
            }
        }

        ret = fts_write(packet_buf, packet_len + cmdlen);
        if (ret < 0) {
            FTS_ERROR("pramboot write data(%d) fail", i);
            return ret;
        }
    }

    ecc_in_host = (int)ecc_new;

    return ecc_in_host;
}



static int fts_pram_write_remap_ft2388(void)
{
    int ret = 0;
    int ecc_in_host = 0;
    int ecc_in_tp = 0;
    u8 *pb_buf = NULL;
    u32 pb_len = 0;

    FTS_INFO("write pram and remap");

    if (upgrade_func_ft2388.pb_length < FTS_MIN_LEN) {
        FTS_ERROR("pramboot length(%d) fail", upgrade_func_ft2388.pb_length);
        return -EINVAL;
    }

    pb_buf = upgrade_func_ft2388.pramboot;
    pb_len = upgrade_func_ft2388.pb_length;
    /* write pramboot to pram */
    ecc_in_host = fts_pram_write_buf_ft2388(pb_buf, pb_len);
    if (ecc_in_host < 0) {
        FTS_ERROR( "write pramboot fail");
        return ecc_in_host;
    }
    /* read out checksum */
    ecc_in_tp = fts_pram_ecc_cal_algo_ft2388(0, pb_len);
    if (ecc_in_tp < 0) {
        FTS_ERROR( "read pramboot ecc fail");
        return ecc_in_tp;
    }

    FTS_INFO("pram ecc in tp:%x, host:%x", ecc_in_tp, ecc_in_host);
    /*  pramboot checksum != fw checksum, upgrade fail */
    if (ecc_in_host != ecc_in_tp) {
        FTS_ERROR("pramboot ecc check fail");
        return -EIO;
    }

    /*start pram*/
    ret = fts_pram_start_ft2388();
    if (ret < 0) {
        FTS_ERROR("pram start fail");
        return ret;
    }

    return 0;
}


/************************************************************************
* Name: fts_fwupg_reset_to_romboot_ft2388
* Brief: reset to romboot, to load pramboot
* Input:
* Output:
* Return: return 0 if success, otherwise return error code
***********************************************************************/
static int fts_fwupg_reset_to_romboot_ft2388(void)
{
    int ret = 0;
    int i = 0;
    u8 cmd = FTS_CMD_RESET;
    enum FW_STATUS state = FTS_RUN_IN_ERROR;

    ret = fts_write(&cmd, 1);
    if (ret < 0) {
        FTS_ERROR("pram/rom/bootloader reset cmd write fail");
        return ret;
    }
    mdelay(10);

    for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
        ret = fts_fwupg_get_boot_state_ft2388(&state);
        if (FTS_RUN_IN_ROM == state)
            break;
        mdelay(5);
    }
    if (i >= FTS_UPGRADE_LOOP) {
        FTS_ERROR("reset to romboot fail");
        return -EIO;
    }

    return 0;
}



static int fts_pram_init_ft2388(void)
{
    int ret = 0;
    u8 reg_val = 0;
    u8 wbuf[3] = { 0 };

    FTS_INFO("pramboot initialization");

    /* read flash ID */
    wbuf[0] = FTS_CMD_FLASH_TYPE;
    ret = fts_read(wbuf, 1, &reg_val, 1);
    if (ret < 0) {
        FTS_ERROR("read flash type fail");
        return ret;
    }

    /* set flash clk */
    wbuf[0] = FTS_CMD_FLASH_TYPE;
    wbuf[1] = reg_val;
    wbuf[2] = 0x00;
    ret = fts_write(wbuf, 3);
    if (ret < 0) {
        FTS_ERROR("write flash type fail");
        return ret;
    }

    return 0;
}


static int fts_pram_write_init_ft2388(void)
{
    int ret = 0;
    bool state = 0;
    enum FW_STATUS status = FTS_RUN_IN_ERROR;

    FTS_INFO("**********pram write and init**********");

    FTS_DEBUG("check whether tp is in romboot or not ");
    /* need reset to romboot when non-romboot state */
    ret = fts_fwupg_get_boot_state_ft2388(&status);
    if (status != FTS_RUN_IN_ROM) {
        if (FTS_RUN_IN_PRAM == status) {
            FTS_INFO("tp is in pramboot, need send reset cmd before upgrade");
            ret = fts_pram_init_ft2388();
            if (ret < 0) {
                FTS_ERROR("pramboot(before) init fail");
                return ret;
            }
        }

        FTS_INFO("tp isn't in romboot, need send reset to romboot");
        ret = fts_fwupg_reset_to_romboot_ft2388();
        if (ret < 0) {
            FTS_ERROR("reset to romboot fail");
            return ret;
        }
    }

    /* check the length of the pramboot */
    ret = fts_pram_write_remap_ft2388();
    if (ret < 0) {
        FTS_ERROR("pram write fail, ret=%d", ret);
        return ret;
    }

    FTS_DEBUG("after write pramboot, confirm run in pramboot");
    state = fts_fwupg_check_state_ft2388(FTS_RUN_IN_PRAM);
    if (!state) {
        FTS_ERROR("not in pramboot");
        return -EIO;
    }

    ret = fts_pram_init_ft2388();
    if (ret < 0) {
        FTS_ERROR("pramboot init fail");
        return ret;
    }

    return 0;
}



static int fts_fwupg_reset_to_boot_ft2388(void)
{
    int ret = 0;
    u8 reg = FTS_REG_UPGRADE;

    ret = fts_write_reg(reg, FTS_UPGRADE_AA);
    if (ret < 0) {
        FTS_ERROR("write FC=0xAA fail");
        return ret;
    }
    msleep(FTS_DELAY_UPGRADE_AA);

    ret = fts_write_reg(reg, FTS_UPGRADE_55);
    if (ret < 0) {
        FTS_ERROR("write FC=0x55 fail");
        return ret;
    }

    msleep(FTS_DELAY_UPGRADE_RESET);
    return 0;
}


static bool fts_fwupg_check_fw_valid_ft2388(void)
{
    int ret = 0;

    ret = fts_wait_tp_to_valid();
    if (ret < 0) {
        FTS_INFO("tp fw invaild");
        return false;
    }

    FTS_INFO("tp fw vaild");
    return true;
}


/************************************************************************
* Name: fts_fwupg_enter_into_boot_ft2388
* Brief: enter into boot environment, ready for upgrade
* Input:
* Output:
* Return: return 0 if success, otherwise return error code
***********************************************************************/
static int fts_fwupg_enter_into_boot_ft2388(void)
{
    int ret = 0;
    bool fwvalid = false;

    FTS_INFO("***********enter into pramboot/bootloader***********");

    fwvalid = fts_fwupg_check_fw_valid_ft2388();
    if (fwvalid) {
        ret = fts_fwupg_reset_to_boot_ft2388();
        if (ret < 0) {
            FTS_ERROR("enter into romboot/bootloader fail");
            return ret;
        }
    }
    FTS_INFO("pram supported, write pramboot and init");
    /* pramboot */
    ret = fts_pram_write_init_ft2388();
    if (ret < 0) {
        FTS_ERROR("pram write_init fail");
        return ret;
    }

    return 0;
}


/************************************************************************
* Name: fts_ft2388_upgrade
* Brief:
* Input:
* Output:
* Return: return 0 if success, otherwise return error code
***********************************************************************/
static int fts_ft2388_upgrade_mode(enum FW_FLASH_MODE mode, u8 *buf, u32 len)
{
    int ret = 0;
    u32 start_addr = 0;
    u8 cmd[4] = { 0 };
    u32 delay = 0;
    int ecc_in_host = 0;
    int ecc_in_tp = 0;

    if ((NULL == buf) || (len < FTS_MIN_LEN)) {
        FTS_ERROR("buffer/len(%x) is invalid", len);
        return -EINVAL;
    }

    /* enter into upgrade environment */
    ret = fts_fwupg_enter_into_boot_ft2388();
    if (ret < 0) {
        FTS_ERROR("enter into pramboot/bootloader fail,ret=%d", ret);
        goto fw_reset;
    }

    cmd[0] = FTS_CMD_FLASH_MODE;
    cmd[1] = FLASH_MODE_UPGRADE_VALUE;
    if (upgrade_func_ft2388.appoff_handle_in_ic) {
        start_addr = 0; /* offset handle in pramboot */
    } else {
        start_addr = upgrade_func_ft2388.appoff;
    }
    FTS_INFO("flash mode:0x%02x, start addr=0x%04x", cmd[1], start_addr);

    ret = fts_write(cmd, 2);
    if (ret < 0) {
        FTS_ERROR("upgrade mode(09) cmd write fail");
        goto fw_reset;
    }

    cmd[0] = FTS_CMD_APP_DATA_LEN_INCELL;
    cmd[1] = BYTE_OFF_16(len);
    cmd[2] = BYTE_OFF_8(len);
    cmd[3] = BYTE_OFF_0(len);
    ret = fts_write(cmd, FTS_CMD_DATA_LEN_LEN);
    if (ret < 0) {
        FTS_ERROR("data len cmd write fail");
        goto fw_reset;
    }

    delay = FTS_ERASE_SECTOR_DELAY * (len / FTS_MAX_LEN_SECTOR);
    ret = fts_fwupg_erase(delay);
    if (ret < 0) {
        FTS_ERROR("erase cmd write fail");
        goto fw_reset;
    }

    /* write app */
    ecc_in_host = fts_flash_write_buf(start_addr, buf, len, 1);
    if (ecc_in_host < 0 ) {
        FTS_ERROR("lcd initial code write fail");
        goto fw_reset;
    }

    /* ecc */
    ecc_in_tp = fts_fwupg_ecc_cal(start_addr, len);
    if (ecc_in_tp < 0 ) {
        FTS_ERROR("ecc read fail");
        goto fw_reset;
    }

    FTS_INFO("ecc in tp:%x, host:%x", ecc_in_tp, ecc_in_host);
    if (ecc_in_tp != ecc_in_host) {
        FTS_ERROR("ecc check fail");
        goto fw_reset;
    }

    FTS_INFO("upgrade success, reset to normal boot");
    ret = fts_fwupg_reset_in_boot();
    if (ret < 0) {
        FTS_ERROR("reset to normal boot fail");
    }

    msleep(400);
    return 0;

fw_reset:
    return -EIO;
}

/************************************************************************
* Name: fts_ft2388_upgrade
* Brief:
* Input:
* Output:
* Return: return 0 if success, otherwise return error code
***********************************************************************/
static int fts_ft2388_upgrade(u8 *buf, u32 len)
{
    int ret = 0;
    u8 *tmpbuf = NULL;
    u32 app_len = 0;

    FTS_INFO("fw app upgrade...");
    if (NULL == buf) {
        FTS_ERROR("fw buf is null");
        return -EINVAL;
    }

    if ((len < FTS_MIN_LEN) || (len > FTS_MAX_LEN_FILE)) {
        FTS_ERROR("fw buffer len(%x) fail", len);
        return -EINVAL;
    }

    app_len = len - upgrade_func_ft2388.appoff;
    tmpbuf = buf + upgrade_func_ft2388.appoff;
    ret = fts_ft2388_upgrade_mode(FLASH_MODE_APP, tmpbuf, app_len);
    if (ret < 0) {
        FTS_INFO("fw upgrade fail,reset to normal boot");
        if (fts_fwupg_reset_in_boot() < 0) {
            FTS_ERROR("reset to normal boot fail");
        }
        return ret;
    }

    return 0;
}


struct upgrade_func upgrade_func_ft2388 = {
    .ctype = {0x1E},
    .fwveroff = 0x010E,
    .fwcfgoff = 0x0128,
    .appoff = 0x0000,
    .appoff_handle_in_ic = true,
    .new_return_value_from_ic = true,
    .pramboot_supported = true,
    .pramboot = pb_file_ft2388,
    .pb_length = sizeof(pb_file_ft2388),
    .hid_supported = false,
    .upgrade = fts_ft2388_upgrade,
};
