/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "[%s::%d] " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/crc32.h>
#include "msm_sd.h"
#include "msm_ois.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#include "msm_camera_dt_util.h"


DEFINE_MSM_MUTEX(msm_ois_mutex);

#define OIS_FW_UPDATE_PACKET_SIZE 256
#define BOOTCODE_SIZE (1024 * 4)
#define PROGCODE_SIZE (1024 * 24)

#define OIS_CAL_DATA_INFO_SIZE        (6)
#define OIS_CAL_DATA_INFO_START_ADDR  (0x7400)

#define MAX_RETRY_COUNT               (3)

#undef CDBG
#ifdef CONFIG_MSM_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

#undef CDBG_I
#define CONFIG_MSM_CAMERA_DEBUG_INFO
#ifdef CONFIG_MSM_CAMERA_DEBUG_INFO
#define CDBG_I(fmt, args...) pr_info(fmt, ##args)
#else
#define CDBG_I(fmt, args...) do { } while (0)
#endif


#define OIS_PINCTRL_STATE_DEFAULT "ois_default"
#define OIS_PINCTRL_STATE_SLEEP "ois_suspend"

int msm_ois_i2c_byte_read(struct msm_ois_ctrl_t *a_ctrl, uint32_t addr, uint16_t *data);
int msm_ois_i2c_byte_write(struct msm_ois_ctrl_t *a_ctrl, uint32_t addr, uint16_t data);

static int32_t msm_ois_power_up(struct msm_ois_ctrl_t *a_ctrl);
static int32_t msm_ois_power_down(struct msm_ois_ctrl_t *a_ctrl);

static struct i2c_driver msm_ois_i2c_driver;
struct msm_ois_ctrl_t *g_msm_ois_t;
extern struct class *camera_class; /*sys/class/camera*/
extern int16_t msm_actuator_move_for_ois_test(void);


#define GPIO_LEVEL_LOW        0
#define GPIO_LEVEL_HIGH       1
#define GPIO_CAM_RESET        36

#define OIS_VERSION_OFFSET    (0x6FF2)
#define OIS_FW_VER_SIZE       (6)
#define OIS_HW_VER_SIZE       (3)
#define OIS_DEBUG_INFO_SIZE   (40)
#define SYSFS_OIS_DEBUG_PATH  "/sys/class/camera/ois/ois_exif"

extern void *msm_get_eeprom_data_base(int id, uint32_t *size);

#define I2C_RETRY_COUNT     3

#define FW_GYRO_SENSOR      0
#define FW_DRIVER_IC        1
#define FW_CORE_VERSION     2
#define FW_RELEASE_YEAR     3
#define FW_RELEASE_MONTH    4
#define FW_RELEASE_COUNT    5

#define MODULE_OIS_TOTAL_SIZE                   0x171B
#define MODULE_OIS_START_ADDR                   0x1200

#define MODULE_OIS_HDR_OFFSET                   0x0
#define MODULE_OIS_HDR_CHK_SUM_OFFSET           0x80

#define MODULE_OIS_CAL_DATA_OFFSET              0x84
#define MODULE_OIS_CAL_DATA_CHK_SUM_OFFSET      0xE4
#define MODULE_OIS_CAL_DATA_MAP_SIZE            0x60

#define MODULE_OIS_SHIFT_DATA_OFFSET            0xE8
#define MODULE_OIS_SHIFT_DATA_CHK_SUM_OFFSET    0x117

#define MODULE_OIS_FW_OFFSET                    0x11B
#define MODULE_OIS_FW_CHK_SUM_OFFSET            0x517
#define MODULE_OIS_FW_MAP_SIZE                  0x3FC

#define MODULE_OIS_FW_FACTORY_OFFSET            0x51B
#define MODULE_OIS_FW_FACTORY_CHK_SUM_OFFSET    0x1717
#define MODULE_OIS_FW_FACTORY_MAP_SIZE          0x11FC

#define OIS_FW_PHONE_BASE_ADDR                  0x0
#define OIS_SET_FW_PHONE_ADDR_OFFSET            0x0

#define OIS_SET_FW_D1_OFFSET_ADDR_OFFSET        0x04
#define OIS_SET_FW_D1_SIZE_ADDR_OFFSET          0x06
#define OIS_SET_FW_D1_TARGET_ADDR_OFFSET        0x08

#define OIS_SET_FW_D2_OFFSET_ADDR_OFFSET        0x0A
#define OIS_SET_FW_D2_SIZE_ADDR_OFFSET          0x0C
#define OIS_SET_FW_D2_TARGET_ADDR_OFFSET        0x0E

#define OIS_SET_FW_D3_OFFSET_ADDR_OFFSET        0x10
#define OIS_SET_FW_D3_SIZE_ADDR_OFFSET          0x12
#define OIS_SET_FW_D3_TARGET_ADDR_OFFSET        0x14

#define MOCULE_OIS_CAL_DATA_OFFSET_OFFSET       0x04
#define MOCULE_OIS_CAL_DATA_SIZE_OFFSET         0x06
#define MOCULE_OIS_CAL_DATA_TRGT_ADDR_OFFSET    0x08


#define OIS_GYRO_SCALE_FACTOR_V003              262
#define OIS_GYRO_SCALE_FACTOR_V004              131


static u8 *ois_phone_buf = NULL;
static int ois_fw_base_addr = MODULE_OIS_START_ADDR;

static u8 *msm_ois_base(int id)
{
    u32 size = 0;
    u8 *ebase = (u8 *)msm_get_eeprom_data_base(id, &size);
    if (!ebase)
        return NULL;

    if (size < MODULE_OIS_TOTAL_SIZE) {
        pr_err("%s eeprom size[%d] expected size[%d]\n",
                __func__, size, MODULE_OIS_TOTAL_SIZE);
        return NULL;
    }
    return (ebase + MODULE_OIS_START_ADDR);
}

/* OIS_A7_START */

static int msm_ois_verify_sum(const char *mem, uint32_t size, uint32_t sum)
{
    uint32_t crc = ~0UL;

    /* check overflow */
    if (size > crc - sizeof(uint32_t))
        return -EINVAL;

    crc = crc32_le(crc, mem, size);
    if (~crc != sum) {
        pr_err("%s: expect 0x%x, result 0x%x\n", __func__, sum, ~crc);
        return -EINVAL;
    }
    CDBG("%s: checksum pass 0x%x\n", __func__, sum);
    return 0;
}


/* Byte swap short - change big-endian to little-endian */
uint16_t swap_uint16( uint16_t val )
{
    return (val << 8) | ((val >> 8) & 0xFF);
}

uint32_t swap_uint32( uint32_t val )
{
    val = ((val << 8) & 0xFF00FF00 ) | ((val >> 8) & 0xFF00FF );
    return (val << 16) | (val >> 16);
}

char ois_fw_full[40] = {0,};
void msm_ois_get_phone_fw(struct msm_ois_ctrl_t *a_ctrl)
{
    a_ctrl->phone_ver.core_ver = 0;
    a_ctrl->phone_ver.gyro_sensor = 0;
    a_ctrl->phone_ver.driver_ic = 0;
    a_ctrl->phone_ver.year = 0;
    a_ctrl->phone_ver.month = 0;
    a_ctrl->phone_ver.iteration_0 = 0;
    a_ctrl->phone_ver.iteration_1 = 0;
}

void msm_ois_get_module_fw(struct msm_ois_ctrl_t *a_ctrl)
{
    u8 *buf = msm_ois_base(0);
    if (!buf) {
        pr_err("%s eeprom base is NULL!\n", __func__);
        return;
    }

    a_ctrl->module_ver.core_ver = buf[0x40];
    a_ctrl->module_ver.gyro_sensor = buf[0x41];
    a_ctrl->module_ver.driver_ic = buf[0x42];
    a_ctrl->module_ver.year = buf[0x43];
    a_ctrl->module_ver.month = buf[0x44];
    a_ctrl->module_ver.iteration_0 = buf[0x45];
    a_ctrl->module_ver.iteration_1 = buf[0x46];

    CDBG("%c%c%c%c%c%c%c\n", a_ctrl->module_ver.core_ver,
        a_ctrl->module_ver.gyro_sensor, a_ctrl->module_ver.driver_ic,
        a_ctrl->module_ver.year, a_ctrl->module_ver.month,
        a_ctrl->module_ver.iteration_0, a_ctrl->module_ver.iteration_1);

}

void msm_ois_get_fw_version(struct msm_ois_ctrl_t *a_ctrl)
{
    char P_FW[10] = {0,}, M_FW[10] = {0,}, L_FW[10] = {0,};
    msm_ois_get_phone_fw(a_ctrl);
    msm_ois_get_module_fw(a_ctrl);

    if (a_ctrl->module_ver.core_ver == 0 && a_ctrl->module_ver.gyro_sensor == 0)
        sprintf(M_FW, "%s", "NULL");
    else
        sprintf(M_FW, "%c%c%c%c%c%c%c", a_ctrl->module_ver.core_ver,
            a_ctrl->module_ver.gyro_sensor, a_ctrl->module_ver.driver_ic,
            a_ctrl->module_ver.year, a_ctrl->module_ver.month,
            a_ctrl->module_ver.iteration_0, a_ctrl->module_ver.iteration_1);

    if (a_ctrl->phone_ver.core_ver == 0 && a_ctrl->phone_ver.gyro_sensor == 0)
        sprintf(P_FW, "%s", "NULL");
    else
        sprintf(P_FW, "%c%c%c%c%c%c%c", a_ctrl->phone_ver.core_ver,
            a_ctrl->phone_ver.gyro_sensor, a_ctrl->phone_ver.driver_ic,
            a_ctrl->phone_ver.year, a_ctrl->phone_ver.month,
            a_ctrl->phone_ver.iteration_0, a_ctrl->phone_ver.iteration_1);

    strncpy(L_FW, M_FW, 10);
    sprintf(ois_fw_full, "%s %s %s\n", M_FW, P_FW, L_FW);
    CDBG_I("ois fw[%s]\n", ois_fw_full);
}

int msm_ois_cal_revision(char *cal_ver)
{
    int revision = 0;

    if( cal_ver[0] != 'V'
        || cal_ver[1] < '0' || cal_ver[1] > '9'
        || cal_ver[2] < '0' || cal_ver[2] > '9'
        || cal_ver[3] < '0' || cal_ver[3] > '9' ) {
        return 0;
    }

    revision = ((cal_ver[1] - '0') * 100) + ((cal_ver[2] - '0') * 10) + (cal_ver[3] - '0');

    CDBG_I(": %d\n", revision);

    return revision;
}

void msm_ois_get_cal_version(struct msm_ois_ctrl_t *a_ctrl)
{
    u8 *buf = msm_ois_base(0);
    if (!buf) {
        pr_err("%s eeprom base is NULL!\n", __func__);
        return;
    }

    memcpy(a_ctrl->cal_info.cal_ver, &buf[0x48], 4);
}

u8 i2c_write_buf[5000] = {0,};
int msm_ois_i2c_write_multi(struct msm_ois_ctrl_t *a_ctrl, u16 addr, u8 *data, size_t size)
{
    int ret = 0, err = 0;
    int retries = I2C_RETRY_COUNT;
    ulong i = 0;

    struct i2c_client *client = a_ctrl->i2c_client.client;
    uint16_t saddr = client->addr >> 1;

    struct i2c_msg msg = {
        .addr   = saddr,
        .flags  = 0,
        .len    = size + 2,
        .buf    = i2c_write_buf,
    };

    i2c_write_buf[0] = (addr & 0xFF00) >> 8;
    i2c_write_buf[1] = addr & 0xFF;

    for (i = 0; i < size; i++) {
        i2c_write_buf[i + 2] = *(data + i);
    }
    CDBG("OISLOG %s : W(0x%02X%02X, start:%02X, end:%02X)\n", __func__,
        i2c_write_buf[0], i2c_write_buf[1], i2c_write_buf[2], i2c_write_buf[i + 1]);

    do {
        ret = i2c_transfer(client->adapter, &msg, 1);
        if (likely(ret == 1))
        break;

        usleep_range(10000,11000);
        err = ret;
    } while (--retries > 0);

    /* Retry occured */
    if (unlikely(retries < I2C_RETRY_COUNT)) {
        pr_err("i2c_write: error %d, write (%04X, %04X), retry %d\n",
            err, addr, *data, I2C_RETRY_COUNT - retries);
    }

    if (unlikely(ret != 1)) {
        pr_err("I2C does not work\n\n");
        return -EIO;
    }

    return 0;
}

static int msm_ois_i2c_read_multi(struct msm_ois_ctrl_t *a_ctrl, u16 addr, u8 *data, size_t size)
{
    int err;
    u8 rxbuf[256], txbuf[2];
    struct i2c_msg msg[2];
    struct i2c_client *client = a_ctrl->i2c_client.client;
    uint16_t saddr = client->addr >> 1;

    txbuf[0] = (addr & 0xff00) >> 8;
    txbuf[1] = (addr & 0xff);

    msg[0].addr = saddr;
    msg[0].flags = 0;
    msg[0].len = 2;
    msg[0].buf = txbuf;

    msg[1].addr = saddr;
    msg[1].flags = I2C_M_RD;
    msg[1].len = size;
    msg[1].buf = rxbuf;

    err = i2c_transfer(client->adapter, msg, 2);
    if (unlikely(err != 2)) {
        pr_err("%s: register read fail", __func__);
        return -EIO;
    }

    memcpy(data, rxbuf, size);
    return 0;
}

int msm_ois_fw_revision(char *fw_ver)
{
    int revision = 0;
    revision = revision + ((int)fw_ver[FW_RELEASE_YEAR] - 64) * 1000;
    revision = revision + ((int)fw_ver[FW_RELEASE_MONTH] - 64) * 100;
    revision = revision + ((int)fw_ver[FW_RELEASE_COUNT] - 48) * 10;
    revision = revision + (int)fw_ver[FW_RELEASE_COUNT + 1] - 48;

    return revision;
}

bool msm_ois_version_compare(char *fw_ver1, char *fw_ver2)
{
    int revision1, revision2;

    revision1 = msm_ois_fw_revision(fw_ver1);
    revision2 = msm_ois_fw_revision(fw_ver2);

    CDBG_I("msm_ois_version_compare (eeprom: %d vs phone : %d)\n", revision1, revision2);

    if (revision1 >= revision2) /* Download EEPROM OIS FW */
        ois_fw_base_addr = MODULE_OIS_FW_OFFSET;
    else /* Dwonload Phone OIS FW */
        ois_fw_base_addr = OIS_FW_PHONE_BASE_ADDR;

    return true;
}

u32 msm_ois_read_cal_checksum(struct msm_ois_ctrl_t *a_ctrl)
{
    int ret = 0;
    u8 read_data[4] = {0,};
    u32 checksum = 0;

    ret = msm_ois_i2c_read_multi(a_ctrl, 0xF008, read_data, 4);
    if (ret) {
        pr_err("i2c read fail\n");
    }

    checksum = (read_data[0] << 24) | (read_data[1] << 16) | (read_data[2] << 8) | (read_data[3]);

    CDBG("%s : R(0x%02X%02X%02X%02X)\n",
        __func__, read_data[0], read_data[1], read_data[2], read_data[3]);

    return checksum;
}



int msm_ois_download_factory_fw_set(struct msm_ois_ctrl_t *a_ctrl)
{
    int ret = 0;
    u8 *buf = NULL;

    uint16_t ois_set_fw_addr = 0;
    uint16_t ois_set_fw_target_addr1;
    uint16_t ois_set_fw_offset1 = 0;
    uint16_t ois_set_fw_size1 = 0;

    uint16_t ois_set_fw_target_addr2;
    uint16_t ois_set_fw_offset2 = 0;
    uint16_t ois_set_fw_size2 = 0;

    uint16_t ois_set_fw_target_addr3;
    uint16_t ois_set_fw_offset3 = 0;
    uint16_t ois_set_fw_size3 = 0;

    u32 checksum_fw = 0;
    u32 checksum_module = 0;

    CDBG_I("E");
    /* Access to "OIS FW DL" */
    ret = msm_ois_i2c_byte_write(a_ctrl, 0xF010, 0x00);
    if (ret < 0) {
        pr_err("i2c write fail\n");
        ret = -EINVAL;
    }

    /* set fw address from eeprom fw buf or phone fw buf */
    if (ois_fw_base_addr == MODULE_OIS_START_ADDR) { /* EEPROM SET FW */
        buf = msm_ois_base(0);
        if (!buf) {
            pr_err("%s eeprom base is NULL!\n", __func__);
            return 0; /* returning success: as withouth OIS camera must work */
        }
        ois_set_fw_addr = MODULE_OIS_FW_FACTORY_OFFSET;
        CDBG_I("OIS SET FW Loading from EEPROM.\n");
    } else { /* PHONE SET FW */
        buf = ois_phone_buf;
        ois_set_fw_addr = OIS_SET_FW_PHONE_ADDR_OFFSET;
        CDBG_I("OIS SET FW Loading from PHONE.\n");
    }

    ret = msm_ois_verify_sum(&buf[ois_set_fw_addr],\
          MODULE_OIS_FW_FACTORY_MAP_SIZE, *(int *)(buf + MODULE_OIS_FW_FACTORY_CHK_SUM_OFFSET));
    if (ret < 0) {
        pr_err("%s: OIS_FW check sum verification failed! checksum[%p] fw[%p]\n", __func__,
            &buf[MODULE_OIS_FW_FACTORY_CHK_SUM_OFFSET], &buf[ois_set_fw_addr]);
        return -EINVAL;
    }

    ois_set_fw_target_addr1 =
        swap_uint16(*((uint16_t *)&buf[ois_set_fw_addr+OIS_SET_FW_D1_TARGET_ADDR_OFFSET]));
    ois_set_fw_offset1 =
        swap_uint16(*((uint16_t *)&buf[ois_set_fw_addr+OIS_SET_FW_D1_OFFSET_ADDR_OFFSET]));
    ois_set_fw_size1 =
        swap_uint16(*((uint16_t *)&buf[ois_set_fw_addr+OIS_SET_FW_D1_SIZE_ADDR_OFFSET]));
    if (ois_set_fw_size1 > 0)
        ret = msm_ois_i2c_write_multi(a_ctrl, ois_set_fw_target_addr1,
                            &buf[ois_set_fw_addr+ois_set_fw_offset1], ois_set_fw_size1);

    ois_set_fw_target_addr2 =
        swap_uint16(*((uint16_t *)&buf[ois_set_fw_addr+OIS_SET_FW_D2_TARGET_ADDR_OFFSET]));
    ois_set_fw_offset2 =
        swap_uint16(*((uint16_t *)&buf[ois_set_fw_addr+OIS_SET_FW_D2_OFFSET_ADDR_OFFSET]));
    ois_set_fw_size2 =
        swap_uint16(*((uint16_t *)&buf[ois_set_fw_addr+OIS_SET_FW_D2_SIZE_ADDR_OFFSET]));
    if (ois_set_fw_size2 > 0)
        ret = msm_ois_i2c_write_multi(a_ctrl, ois_set_fw_target_addr2,
                            &buf[ois_set_fw_addr+ois_set_fw_offset2], ois_set_fw_size2);

    ois_set_fw_target_addr3 =
        swap_uint16(*((uint16_t *)&buf[ois_set_fw_addr+OIS_SET_FW_D3_TARGET_ADDR_OFFSET]));
    ois_set_fw_offset3 =
        swap_uint16(*((uint16_t *)&buf[ois_set_fw_addr+OIS_SET_FW_D3_OFFSET_ADDR_OFFSET]));
    ois_set_fw_size3 =
        swap_uint16(*((uint16_t *)&buf[ois_set_fw_addr+OIS_SET_FW_D3_SIZE_ADDR_OFFSET]));
    if (ois_set_fw_size3 > 0)
        ret = msm_ois_i2c_write_multi(a_ctrl, ois_set_fw_target_addr3,
                            &buf[ois_set_fw_addr+ois_set_fw_offset3], ois_set_fw_size3);

    checksum_fw = swap_uint32(*((int32_t *)&buf[ois_set_fw_addr]));
    checksum_module = msm_ois_read_cal_checksum(a_ctrl);

    if (checksum_fw != checksum_module) {
        pr_err("%s: OIS FW check sum mismatch!\n", __func__);
        ret = -EINVAL;
    }

    CDBG_I("Success X\n");

    CDBG("OIS SET FW addr = 0x%04X\n", ois_set_fw_addr);
    CDBG("OIS SET FW #1 target reg. = 0x%04X, offset = 0x%04X, size = 0x%04X\n",
        ois_set_fw_target_addr1, ois_set_fw_offset1, ois_set_fw_size1);
    CDBG("OIS SET FW #2 target reg. = 0x%04X, offset = 0x%04X, size = 0x%04X\n",
        ois_set_fw_target_addr2, ois_set_fw_offset2, ois_set_fw_size2);
    CDBG("OIS SET FW #3 target reg. = 0x%04X, offset = 0x%04X, size = 0x%04X\n",
        ois_set_fw_target_addr3, ois_set_fw_offset3, ois_set_fw_size3);
    CDBG("OIS SET FW checksum = ( Phone-0x%08X : Module-0x%08X)\n",
        checksum_fw, checksum_module);
    return ret;
}




int msm_ois_download_fw_set(struct msm_ois_ctrl_t *a_ctrl)
{
    int ret = 0;
    u8 *buf = NULL;

    uint16_t ois_set_fw_addr = 0;
    uint16_t ois_set_fw_target_addr1;
    uint16_t ois_set_fw_offset1 = 0;
    uint16_t ois_set_fw_size1 = 0;

    uint16_t ois_set_fw_target_addr2;
    uint16_t ois_set_fw_offset2 = 0;
    uint16_t ois_set_fw_size2 = 0;

    uint16_t ois_set_fw_target_addr3;
    uint16_t ois_set_fw_offset3 = 0;
    uint16_t ois_set_fw_size3 = 0;

    u32 checksum_fw = 0;
    u32 checksum_module = 0;

    /* Access to "OIS FW DL" */
    ret = msm_ois_i2c_byte_write(a_ctrl, 0xF010, 0x00);
    if (ret < 0) {
        pr_err("i2c write fail\n");
        ret = -EINVAL;
    }

    /* set fw address from eeprom fw buf or phone fw buf */
    if (ois_fw_base_addr == MODULE_OIS_START_ADDR) { /* EEPROM SET FW */
        buf = msm_ois_base(0);
        if (!buf) {
            pr_err("%s eeprom base is NULL!\n", __func__);
            return 0; /* returning success: as withouth OIS camera must work */
        }
        ois_set_fw_addr = MODULE_OIS_FW_OFFSET;
        CDBG_I("OIS SET FW Loading from EEPROM.\n");
    } else { /* PHONE SET FW */
        buf = ois_phone_buf;
        ois_set_fw_addr = OIS_SET_FW_PHONE_ADDR_OFFSET;
        CDBG_I("OIS SET FW Loading from PHONE.\n");
    }

    ret = msm_ois_verify_sum(&buf[ois_set_fw_addr],\
          MODULE_OIS_FW_MAP_SIZE, *(int *)(buf + MODULE_OIS_FW_CHK_SUM_OFFSET));
    if (ret < 0) {
        pr_err("OIS_FW check sum verification failed! checksum[%p] fw[%p]\n", &buf[MODULE_OIS_FW_CHK_SUM_OFFSET],\
            &buf[ois_set_fw_addr]);
        return -EINVAL;
    }

    ois_set_fw_target_addr1 =
        swap_uint16(*((uint16_t *)&buf[ois_set_fw_addr+OIS_SET_FW_D1_TARGET_ADDR_OFFSET]));
    ois_set_fw_offset1 =
        swap_uint16(*((uint16_t *)&buf[ois_set_fw_addr+OIS_SET_FW_D1_OFFSET_ADDR_OFFSET]));
    ois_set_fw_size1 =
        swap_uint16(*((uint16_t *)&buf[ois_set_fw_addr+OIS_SET_FW_D1_SIZE_ADDR_OFFSET]));
    if (ois_set_fw_size1 > 0)
        ret = msm_ois_i2c_write_multi(a_ctrl, ois_set_fw_target_addr1,
                            &buf[ois_set_fw_addr+ois_set_fw_offset1], ois_set_fw_size1);

    ois_set_fw_target_addr2 =
        swap_uint16(*((uint16_t *)&buf[ois_set_fw_addr+OIS_SET_FW_D2_TARGET_ADDR_OFFSET]));
    ois_set_fw_offset2 =
        swap_uint16(*((uint16_t *)&buf[ois_set_fw_addr+OIS_SET_FW_D2_OFFSET_ADDR_OFFSET]));
    ois_set_fw_size2 =
        swap_uint16(*((uint16_t *)&buf[ois_set_fw_addr+OIS_SET_FW_D2_SIZE_ADDR_OFFSET]));
    if (ois_set_fw_size2 > 0)
        ret = msm_ois_i2c_write_multi(a_ctrl, ois_set_fw_target_addr2,
                            &buf[ois_set_fw_addr+ois_set_fw_offset2], ois_set_fw_size2);

    ois_set_fw_target_addr3 =
        swap_uint16(*((uint16_t *)&buf[ois_set_fw_addr+OIS_SET_FW_D3_TARGET_ADDR_OFFSET]));
    ois_set_fw_offset3 =
        swap_uint16(*((uint16_t *)&buf[ois_set_fw_addr+OIS_SET_FW_D3_OFFSET_ADDR_OFFSET]));
    ois_set_fw_size3 =
        swap_uint16(*((uint16_t *)&buf[ois_set_fw_addr+OIS_SET_FW_D3_SIZE_ADDR_OFFSET]));
    if (ois_set_fw_size3 > 0)
        ret = msm_ois_i2c_write_multi(a_ctrl, ois_set_fw_target_addr3,
                            &buf[ois_set_fw_addr+ois_set_fw_offset3], ois_set_fw_size3);

    checksum_fw = swap_uint32(*((int32_t *)&buf[ois_set_fw_addr]));
    checksum_module = msm_ois_read_cal_checksum(a_ctrl);

    if (checksum_fw != checksum_module) {
        pr_err("OIS FW check sum mismatch!\n");
        ret = -EINVAL;
    }

    CDBG_I("Success X\n");

    CDBG("OIS SET FW addr = 0x%04X\n", ois_set_fw_addr);
    CDBG("OIS SET FW #1 target reg. = 0x%04X, offset = 0x%04X, size = 0x%04X\n",
        ois_set_fw_target_addr1, ois_set_fw_offset1, ois_set_fw_size1);
    CDBG("OIS SET FW #2 target reg. = 0x%04X, offset = 0x%04X, size = 0x%04X\n",
        ois_set_fw_target_addr2, ois_set_fw_offset2, ois_set_fw_size2);
    CDBG("OIS SET FW #3 target reg. = 0x%04X, offset = 0x%04X, size = 0x%04X\n",
        ois_set_fw_target_addr3, ois_set_fw_offset3, ois_set_fw_size3);
    CDBG("OIS SET FW checksum = ( Phone-0x%08X : Module-0x%08X)\n",
        checksum_fw, checksum_module);
    return ret;
}

int msm_ois_download_cal_data(struct msm_ois_ctrl_t *a_ctrl, int mode)
{
    int ret = 0;
    u8 *buf = NULL;

    uint16_t ois_cal_start_addr = MODULE_OIS_CAL_DATA_OFFSET;
    uint16_t ois_cal_target_addr;
    uint16_t ois_cal_offset = 0;
    uint16_t ois_cal_size = 0;

    u32 checksum_fw = 0;
    u32 checksum_cal = 0;
    u32 checksum_module = 0;

    buf = msm_ois_base(0);
    if (!buf) {
        pr_err("%s eeprom base is NULL!\n", __func__);
        return 0; /* returning success: as without OIS camera must work */
    }

    ret = msm_ois_verify_sum(&buf[ois_cal_start_addr],\
          MODULE_OIS_CAL_DATA_MAP_SIZE, *(int *)(buf + MODULE_OIS_CAL_DATA_CHK_SUM_OFFSET));
    if (ret < 0) {
        pr_err("OIS_CAL check sum verification failed! checksum[%p] fw[%p]\n", &buf[MODULE_OIS_CAL_DATA_CHK_SUM_OFFSET],\
            &buf[ois_cal_start_addr]);
        return -EINVAL;
    }

    ois_cal_target_addr = swap_uint16(*((uint16_t *)&buf[ois_cal_start_addr + MOCULE_OIS_CAL_DATA_TRGT_ADDR_OFFSET]));
    ois_cal_offset = swap_uint16(*((uint16_t *)&buf[ois_cal_start_addr + MOCULE_OIS_CAL_DATA_OFFSET_OFFSET]));
    ois_cal_size = swap_uint16(*((uint16_t *)&buf[ois_cal_start_addr + MOCULE_OIS_CAL_DATA_SIZE_OFFSET]));
    if (ois_cal_size > 0)
        ret = msm_ois_i2c_write_multi(a_ctrl, ois_cal_target_addr,
                    &buf[ois_cal_start_addr + ois_cal_offset], ois_cal_size);


    if (ois_fw_base_addr == MODULE_OIS_START_ADDR) {
        if (mode == 0)
            checksum_fw = swap_uint32(*((int32_t *)&buf[MODULE_OIS_FW_OFFSET]));
        else
            checksum_fw = swap_uint32(*((int32_t *)&buf[MODULE_OIS_FW_FACTORY_OFFSET]));
    } else {
        checksum_fw = swap_uint32(*((int32_t *)&buf[OIS_SET_FW_PHONE_ADDR_OFFSET]));
    }

    checksum_cal = swap_uint32(*((int32_t *)&buf[ois_cal_start_addr]));
    checksum_module = msm_ois_read_cal_checksum(a_ctrl);

    /* OIS Download Complete */
    ret = msm_ois_i2c_byte_write(a_ctrl, 0xF006, 0x00);
    if (ret) {
        pr_err("i2c write fail\n");
        ret = -EINVAL;
    }


    if (checksum_fw + checksum_cal != checksum_module) {
        pr_err("%s:%d OIS CAL check sum mismatch! checksum_fw[%x] + checksum_fw[%x] != module_checksum[%x]\n",
                __func__, __LINE__, checksum_fw, checksum_cal, checksum_module);
        ret = -EINVAL;
   }

    if (ret == 0)
        CDBG_I("Success!");

    CDBG("OIS Cal FW addr = 0x%04X\n", ois_cal_start_addr);
    CDBG("OIS Cal target Reg. = 0x%04X, offset = 0x%04X, size = 0x%04X\n",
        ois_cal_target_addr, ois_cal_offset, ois_cal_size);
    CDBG("OIS Cal FW checksum = ( 0x%08X : 0x%08X)\n", checksum_fw, checksum_module);

    return ret;
}


int msm_ois_read_status(struct msm_ois_ctrl_t *a_ctrl)
{
    int ret = 0;
    u16 status = 0;
    int wait_ready_cnt = 0;

    do {
        ret = msm_ois_i2c_byte_read(a_ctrl, 0x6024, &status);
        if (ret < 0) {
            pr_err("i2c read fail\n");
        }
        msleep(20);
        wait_ready_cnt ++;
    } while((status == 0) && (wait_ready_cnt < 100));

    if (status) {
        CDBG_I("ois status ready(%d), wait(%d ms)\n", status, wait_ready_cnt * 20);
    } else {
        pr_err("ois status NOT ready(%d), wait(%d ms)\n", status, wait_ready_cnt * 20);
        ret = -EINVAL;
    }

    return ret;
}

int msm_ois_fw_update(struct msm_ois_ctrl_t *a_ctrl, int mode)
{
    int ret = 0;
    int retry_cnt = 0;

    CDBG_I("E\n");

    /* Select FW (PHONE or EEPROM) */
    //msm_ois_check_fw(core);

    /* Download FW (SET or FACTORY) */
    retry_cnt = 3;
    do {
        if (mode == 0)
            ret = msm_ois_download_fw_set(a_ctrl);
        else
            ret = msm_ois_download_factory_fw_set(a_ctrl);
        if (ret) {
            pr_err("%s:%d ois fw write fail, ret(%d)\n", __func__, __LINE__, ret);
            goto out;
        }
        retry_cnt--;
    }while( ret && (retry_cnt > 0));

    /* Dwonload OIS Cal data */
    ret = msm_ois_download_cal_data(a_ctrl, mode);
    if (ret) {
        pr_err("%s:%d ois caldata write fail\n", __func__, __LINE__);
        goto out;
    }

    /* OIS Status Read */
    mdelay(20);
    ret = msm_ois_read_status(a_ctrl);
    if (ret) {
        pr_err("%s:%d OIS Active Failed!\n", __func__, __LINE__);
        goto out;
    }

    ret = msm_ois_i2c_byte_write(a_ctrl, 0x6020, 0x01); /* OIS Servo On/ OIS Off */
    ret |= msm_ois_i2c_byte_write(a_ctrl, 0x6023, 0x00); /* Gyro On*/

    ret |= msm_ois_read_status(a_ctrl);


#if defined(OIS_TEST_WITHOUT_FW)
    /* OIS Servo On, OIS Off, Gyro On */
    ret = msm_ois_i2c_byte_write(a_ctrl, 0x6020, 0x01);
    ret |= msm_ois_i2c_byte_write(a_ctrl, 0x6023, 0x00);

    /* OIS Servo On / OIS Off */
    ret |= msm_ois_i2c_byte_write(a_ctrl, 0x6020, 0x01);
    ret |= msm_ois_read_status(a_ctrl);

    /* OIS Mode */
    ret = msm_ois_i2c_byte_write(a_ctrl, 0x6021, 0x7B);
    ret |= msm_ois_read_status(a_ctrl);

    /* Compensation angle */
    ret |= msm_ois_i2c_byte_write(a_ctrl, 0x6025, 0x40);
    ret |= msm_ois_read_status(a_ctrl);

    /* OIS On */
    ret |= msm_ois_i2c_byte_write(a_ctrl, 0x6020, 0x02);
    ret |= msm_ois_read_status(a_ctrl);

    if (ret) {
        pr_err("OIS Test fail\n");
    }
#endif
out:
    CDBG_I("Status[%s] X\n", ret ? "Failed!":"Success!");

    return ret;
}

int msm_ois_set_mode_still(struct msm_ois_ctrl_t *a_ctrl)
{
    int ret;

    ret = msm_ois_i2c_byte_write(a_ctrl, 0x6020, 0x01); /* Servo On/ OIS Off */
    ret |= msm_ois_read_status(a_ctrl);

    ret |= msm_ois_i2c_byte_write(a_ctrl, 0x6021, 0x7B); /*OIS Mode Still*/
    ret |= msm_ois_read_status(a_ctrl);

    ret |= msm_ois_i2c_byte_write(a_ctrl, 0x6025, 0x40); /*revision angle*/
    ret |= msm_ois_read_status(a_ctrl);

    ret = msm_ois_i2c_byte_write(a_ctrl, 0x6020, 0x02); /* Servo Off/ OIS On */
    ret |= msm_ois_read_status(a_ctrl);

    return ret;
}

int msm_ois_set_mode_recording(struct msm_ois_ctrl_t *a_ctrl)
{
    int ret;

    ret = msm_ois_i2c_byte_write(a_ctrl, 0x6020, 0x01); /* Servo On/ OIS Off */
    ret |= msm_ois_read_status(a_ctrl);

    ret |= msm_ois_i2c_byte_write(a_ctrl, 0x6021, 0x61); /*OIS Mode Still*/
    ret |= msm_ois_read_status(a_ctrl);

    ret |= msm_ois_i2c_byte_write(a_ctrl, 0x6025, 0xE0); /*revision angle*/
    ret |= msm_ois_read_status(a_ctrl);

    ret = msm_ois_i2c_byte_write(a_ctrl, 0x6020, 0x02); /* Servo Off/ OIS On */
    ret |= msm_ois_read_status(a_ctrl);

    return ret;
}


static int msm_ois_set_mode_sin_x(struct msm_ois_ctrl_t *a_ctrl)
{
    int rc;
    rc  = msm_ois_i2c_byte_write(a_ctrl, 0x6130, 0x00);     // Sin Mode Off
    rc |= msm_ois_i2c_byte_write(a_ctrl, 0x6020, 0x01);     // Servo_ON
    rc |= msm_ois_i2c_byte_write(a_ctrl, 0x6131, 0x01);     // Frequency = 1Hz
    rc |= msm_ois_i2c_byte_write(a_ctrl, 0x6132, 0x50);     // AMP = 80
    rc |= msm_ois_i2c_byte_write(a_ctrl, 0x6020, 0x03);     // Manual Mode
    rc |= msm_ois_i2c_byte_write(a_ctrl, 0x6130, 0x01);     // X축 Sin_Mode 동작

    return rc;
}

static int msm_ois_set_mode_sin_y(struct msm_ois_ctrl_t *a_ctrl)
{
    int rc;
    rc  = msm_ois_i2c_byte_write(a_ctrl, 0x6130, 0x00);     // Sin Mode Off
    rc |= msm_ois_i2c_byte_write(a_ctrl, 0x6020, 0x01);     // Servo_ON
    rc |= msm_ois_i2c_byte_write(a_ctrl, 0x6131, 0x01);     // Frequency = 1Hz
    rc |= msm_ois_i2c_byte_write(a_ctrl, 0x6132, 0x50);     // AMP = 80
    rc |= msm_ois_i2c_byte_write(a_ctrl, 0x6020, 0x03);     // Manual Mode
    rc |= msm_ois_i2c_byte_write(a_ctrl, 0x6130, 0x02);     // Y축 Sin_Mode 동작

    return rc;
}

/* OIS_A7_END */

static int msm_ois_get_fw_status(struct msm_ois_ctrl_t *a_ctrl)
{
    int rc = 0;
    int ret = 0;
    uint16_t read_value = 0;

    rc = msm_ois_i2c_byte_read(a_ctrl, 0x0006, &read_value);
    if (rc < 0) {
        pr_err("%s:%d i2c read fail , addr 0x0006 \n", __func__, __LINE__);
    }
    ret = read_value;
    return ret;
}

static int32_t msm_ois_read_phone_ver(struct msm_ois_ctrl_t *a_ctrl)
{
    struct file *filp = NULL;
    mm_segment_t old_fs;
    char    char_ois_ver[OIS_FW_VER_SIZE] = "";
    char    ois_path[256] = "/system/cameradata";
    char    ois_bin_full_path[256] = "";
    char    ois_hwinfo[OIS_HW_VER_SIZE+1] = "";
    int ret = 0, i;

    loff_t pos;

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    memcpy(ois_hwinfo, &a_ctrl->module_ver, OIS_HW_VER_SIZE*sizeof(char));

    ois_hwinfo[OIS_HW_VER_SIZE] = 0;
    sprintf(ois_bin_full_path, "%s/OIS_%s.bin", ois_path, ois_hwinfo);
    sprintf(a_ctrl->load_fw_name, ois_bin_full_path); // to use in fw_update

    pr_info("OIS FW : %s", ois_bin_full_path);

    filp = filp_open(ois_bin_full_path, O_RDONLY, 0);
    if(IS_ERR(filp)){
        pr_err("%s: No OIS FW with %s exists in the system. Load from OIS module.\n", __func__, ois_hwinfo);
        set_fs(old_fs);
        return -1;
    }

    pos = OIS_VERSION_OFFSET;
    ret = vfs_read(filp, char_ois_ver, sizeof(char_ois_ver), &pos);
    if (ret < 0) {
        pr_err("%s: Fail to read OIS FW.", __func__);
        ret = -1;
        goto ERROR;
    }

    for(i = 0; i < OIS_FW_VER_SIZE; i ++)
    {
        if(!isalnum(char_ois_ver[i]))
        {
            pr_err("%s: %d version char (%x) is not alnum type.", __func__, __LINE__, char_ois_ver[i]);
            ret = -1;
            goto ERROR;
        }
    }

    memcpy(&a_ctrl->phone_ver, char_ois_ver, sizeof(struct msm_ois_ver_t));
#if 0
    pr_info("%c%c%c%c%c%c\n",
        a_ctrl->phone_ver.gyro_sensor, a_ctrl->phone_ver.driver_ic, a_ctrl->phone_ver.core_ver,
        a_ctrl->phone_ver.month, a_ctrl->phone_ver.iteration_0, a_ctrl->phone_ver.iteration_1);
#endif

ERROR:
    if (filp) {
        filp_close(filp, NULL);
        filp = NULL;
    }
    set_fs(old_fs);
    return ret;
}

static int32_t msm_ois_read_module_ver(struct msm_ois_ctrl_t *a_ctrl)
{
    uint16_t read_value;

    a_ctrl->i2c_client.i2c_func_tbl->i2c_read(&a_ctrl->i2c_client, 0xF8, &read_value, MSM_CAMERA_I2C_WORD_DATA);
    a_ctrl->module_ver.gyro_sensor = read_value & 0xFF;
    a_ctrl->module_ver.driver_ic = (read_value >> 8) & 0xFF;
    if(!isalnum(read_value&0xFF) && !isalnum((read_value>>8)&0xFF))
    {
        pr_err("%s: %d version char is not alnum type.", __func__, __LINE__);
        return -1;
    }

    a_ctrl->i2c_client.i2c_func_tbl->i2c_read(&a_ctrl->i2c_client, 0x7C, &read_value, MSM_CAMERA_I2C_WORD_DATA);
    a_ctrl->module_ver.core_ver = (read_value >> 8) & 0xFF;
    a_ctrl->module_ver.month = read_value & 0xFF;
    if(!isalnum(read_value&0xFF) && !isalnum((read_value>>8)&0xFF))
    {
        pr_err("%s: %d version char is not alnum type.", __func__, __LINE__);
        return -1;
    }

    a_ctrl->i2c_client.i2c_func_tbl->i2c_read(&a_ctrl->i2c_client, 0x7E, &read_value, MSM_CAMERA_I2C_WORD_DATA);
#if 0
    a_ctrl->module_ver.iteration_0 = (read_value >> 8) & 0xFF;
    a_ctrl->module_ver.iteration_1 = read_value & 0xFF;
#endif
    if(!isalnum(read_value&0xFF) && !isalnum((read_value>>8)&0xFF))
    {
        pr_err("%s: %d version char is not alnum type.", __func__, __LINE__);
        return -1;
    }

    CDBG("%c%c%c%c%c%c\n", a_ctrl->module_ver.gyro_sensor, a_ctrl->module_ver.driver_ic, a_ctrl->module_ver.core_ver,
        a_ctrl->module_ver.month, a_ctrl->module_ver.iteration_0, a_ctrl->module_ver.iteration_1);

    return 0;
}

static int32_t msm_ois_set_mode(struct msm_ois_ctrl_t *a_ctrl,
                            uint16_t mode)
{
    int rc = 0;
    switch(mode) {
        case OIS_MODE_ON_STILL:
            if (a_ctrl->ois_mode != OIS_MODE_ON_STILL) {
                CDBG_I("SET :: OIS_MODE_ON_STILL\n");
                rc = msm_ois_set_mode_still(a_ctrl);
                a_ctrl->ois_mode = OIS_MODE_ON_STILL;
            }
            break;
        case OIS_MODE_ON_ZOOM:
            CDBG_I("SET :: OIS_MODE_ON_ZOOM\n");
            rc = -EINVAL;
            break;
        case OIS_MODE_ON_VIDEO:
            if (a_ctrl->ois_mode != OIS_MODE_ON_VIDEO) {
                CDBG_I("SET :: OIS_MODE_ON_VIDEO\n");
                rc = msm_ois_set_mode_recording(a_ctrl);
                a_ctrl->ois_mode = OIS_MODE_ON_VIDEO;
            }
            break;
        case OIS_MODE_SINE_X:
            CDBG_I("SET :: OIS_MODE_SINE_X\n");
            rc = msm_ois_set_mode_sin_x(a_ctrl);
            break;

        case OIS_MODE_SINE_Y:
            CDBG_I("SET :: OIS_MODE_SINE_Y\n");
            rc = msm_ois_set_mode_sin_y(a_ctrl);
            break;

        case OIS_MODE_CENTERING:
            CDBG_I("SET :: OIS_MODE_CENTERING\n");
            rc = -EINVAL;
            break;
        default:
            rc = msm_ois_set_mode_still(a_ctrl);
            break;
    }
    return rc;
}

int msm_ois_i2c_byte_read(struct msm_ois_ctrl_t *a_ctrl, uint32_t addr, uint16_t *data)
{
    int rc = 0;
    rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_read(
        &a_ctrl->i2c_client, addr, data, MSM_CAMERA_I2C_BYTE_DATA);

    if (rc < 0) {
        pr_err("ois i2c byte read failed addr : 0x%x data : 0x%x ", addr, *data);
        return rc;
    }

    CDBG("%s addr = 0x%x data: 0x%x\n", __func__, addr, *data);
    return 0;
}

int msm_ois_i2c_byte_write(struct msm_ois_ctrl_t *a_ctrl, uint32_t addr, uint16_t data)
{
    int rc = 0;
    rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
    &a_ctrl->i2c_client, addr, data, MSM_CAMERA_I2C_BYTE_DATA);

    if (rc < 0) {
        pr_err("ois i2c byte write failed addr : 0x%x data : 0x%x ", addr, data);
        return rc;
    }

    CDBG("%s addr = 0x%x data: 0x%x\n", __func__, addr, data);
    return 0;
}

uint16_t msm_ois_calcchecksum(unsigned char *data, int size)
{
    int i = 0;
    uint16_t result = 0;

    for( i = 0; i < size; i += 2) {
        result = result  + (0xFFFF & (((*(data + i + 1)) << 8) | (*(data + i))));
    }
    return result;
}

static int32_t msm_ois_read_user_data_section(struct msm_ois_ctrl_t *a_ctrl, uint16_t addr, int size, uint8_t *user_data)
{
    int rc = 0, i, read_complete = 0;
    uint16_t read_status;
    uint32_t size_word;

    //  useful range in user data section is 0x7400 ~ 0x7417(max size is 24 bytes).
    //  currently we read only first 8 bytes for cal ver.
    uint8_t read_data[OIS_CAL_DATA_INFO_SIZE] = {0, };

    size_word = (size+3) >> 2;
    /* Configure Data Read Size */
    /* DFLSSIZE_W register(0x000F) 1Byte Send */
    /* unit of size is word(=4byte here) */
    if(msm_ois_i2c_byte_write(a_ctrl, 0x0F, size_word) < 0)
        goto ERROR;

    /* DFLSADR register(0x0010-11) 2Byte Send */
    /* Data Read Start Address offset from 0x7400(start address of User Data Section) */
    rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
        &a_ctrl->i2c_client, 0x10, addr - OIS_CAL_DATA_INFO_START_ADDR, MSM_CAMERA_I2C_WORD_DATA);
    if (rc < 0) {
        pr_err("ois i2c write word failed addr : 0x%x, data : 0x%x ", 0x10, addr);
        goto ERROR;
    }

    /* DFLSCMD reg(0x000E) 1byte send */
    /* 0x04 : READ command */
    if(msm_ois_i2c_byte_write(a_ctrl, 0x0E, 0x04) < 0)
        goto ERROR;

    for(i = 0; i < MAX_RETRY_COUNT; i ++)
    {
        /* DFLSCMD Read, check if read success(0x14) */
        msm_ois_i2c_byte_read(a_ctrl, 0x0E, &read_status);
        if( read_status == 0x14 ) /* Read Complete? */
        {
            read_complete = 1;
            break;
        }
        msleep(20); // give some delay to wait
        pr_info("DFLSCMD Read command doesn't success(iter = %d)", i+1);
    }

    if(read_complete == 1)
    {
        /* FLS_DATA register(0x0100-0x01FF) */
        rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_read_seq(
            &a_ctrl->i2c_client, 0x100, read_data, OIS_CAL_DATA_INFO_SIZE);
        if (rc < 0) {
            pr_err("ois i2c read seq failed addr : 0x%x", 0x100);
            goto ERROR;
        }

        pr_info("userdata cal ver : %02x %02x %02x %02x %02x %02x\n",
            read_data[0], read_data[1], read_data[2], read_data[3],
            read_data[4], read_data[5]);

        memcpy(user_data, read_data, size * sizeof(uint8_t));
    } else {
        pr_err("DFLSCMD Read command doesn't success(0x%x)", read_status);
        goto ERROR;
    }

    return 0;

ERROR:
    return -1;
}

static int32_t msm_ois_read_cal_info(struct msm_ois_ctrl_t *a_ctrl)
{
    int         rc = 0;
    uint8_t     user_data[OIS_CAL_DATA_INFO_SIZE];
    uint16_t    checksum_rumba = 0xFFFF;
    uint16_t    checksum_line = 0xFFFF;
    uint16_t    compare_crc = 0xFFFF;

    rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_read(
        &a_ctrl->i2c_client, 0x7A, &checksum_rumba, MSM_CAMERA_I2C_WORD_DATA); // OIS Driver IC cal checksum
    if (rc < 0) {
        pr_err("ois i2c read word failed addr : 0x%x", 0x7A);
    }

    rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_read(
        &a_ctrl->i2c_client, 0x021E, &checksum_line, MSM_CAMERA_I2C_WORD_DATA); // Line cal checksum
    if (rc < 0) {
        pr_err("ois i2c read word failed addr : 0x%x", 0x021E);
    }

    rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_read(
        &a_ctrl->i2c_client, 0x0004, &compare_crc, MSM_CAMERA_I2C_WORD_DATA);
    if (rc < 0) {
        pr_err("ois i2c read word failed addr : 0x%x", 0x0004);
    }
    a_ctrl->cal_info.cal_checksum_rumba = checksum_rumba;
    a_ctrl->cal_info.cal_checksum_line = checksum_line;
    pr_info("cal checksum(rumba : %d, line : %d), compare_crc = %d", a_ctrl->cal_info.cal_checksum_rumba, a_ctrl->cal_info.cal_checksum_line, compare_crc);

    if(msm_ois_read_user_data_section(a_ctrl, OIS_CAL_DATA_INFO_START_ADDR, OIS_CAL_DATA_INFO_SIZE, user_data) < 0) {
        pr_err(" failed to read user data \n");
        return -1;
    }

    memcpy(a_ctrl->cal_info.cal_ver, user_data, (MSM_OIS_VER_SIZE)*sizeof(uint8_t));

    pr_info("cal version = 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x (%s)\n",
        a_ctrl->cal_info.cal_ver[0], a_ctrl->cal_info.cal_ver[1], a_ctrl->cal_info.cal_ver[2],
        a_ctrl->cal_info.cal_ver[3], a_ctrl->cal_info.cal_ver[4], a_ctrl->cal_info.cal_ver[5],
        a_ctrl->cal_info.cal_ver);

    a_ctrl->cal_info.is_different_crc = compare_crc;

    return 0;
}

#if 0
static int32_t msm_ois_vreg_control(struct msm_ois_ctrl_t *a_ctrl, int config)
{
    int rc = 0, i, cnt;
    int idx = 0;
    struct msm_ois_vreg *vreg_cfg;

    CDBG_I("Enter\n");
    vreg_cfg = &a_ctrl->vreg_cfg;
    cnt = vreg_cfg->num_vreg;
    if (!cnt){
        pr_err("failed\n");
        return 0;
    }
    CDBG("[num_vreg::%d]", cnt);

    if (cnt >= MSM_OIS_MAX_VREGS) {
        pr_err("%s failed %d cnt %d\n", __func__, __LINE__, cnt);
        return -EINVAL;
    }

    for (i = 0; i < cnt; i++) {
        if(config) {
            idx = i;
        }
        else {
            idx = cnt - (i + 1);
        }

        if (a_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
            rc = msm_camera_config_single_vreg(&(a_ctrl->pdev->dev),
                &vreg_cfg->cam_vreg[idx],
                (struct regulator **)&vreg_cfg->data[idx],
                config);
        } else {
            rc = msm_camera_config_single_vreg(&(a_ctrl->i2c_client.client->dev),
                &vreg_cfg->cam_vreg[idx],
                (struct regulator **)&vreg_cfg->data[idx],
                config);
        }

    }
    return rc;
}
#endif

static int32_t msm_ois_config(struct msm_ois_ctrl_t *a_ctrl,
    void __user *argp)
{
    struct msm_ois_cfg_data *cdata =
        (struct msm_ois_cfg_data *)argp;
    int32_t rc = 0;
    int retries = 2;

    mutex_lock(a_ctrl->ois_mutex);
    CDBG_I("%s type [%d] E\n", __func__, cdata->cfgtype);
    switch (cdata->cfgtype) {
    case CFG_OIS_SET_MODE:
        CDBG("CFG_OIS_SET_MODE value :: %d\n", cdata->set_mode_value);
        do{
            rc = msm_ois_set_mode(a_ctrl, cdata->set_mode_value);
            if (rc){
                pr_err("set mode failed %d\n", rc);
                if (--retries < 0)
                    break;
            }
        }while(rc);
        break;
    case CFG_OIS_READ_MODULE_VER:
        CDBG("CFG_OIS_READ_MODULE_VER enter \n");
        rc = msm_ois_read_module_ver(a_ctrl);
        if (rc < 0)
            pr_err("read module version failed, skip fw update from phone %d\n", rc);

        if (copy_to_user(cdata->version, &a_ctrl->module_ver, sizeof(struct msm_ois_ver_t)))
            pr_err("copy to user failed \n");
        break;

    case CFG_OIS_READ_PHONE_VER:
        CDBG("CFG_OIS_READ_PHONE_VER enter \n");
        if(isalnum(a_ctrl->module_ver.gyro_sensor))
        {
            rc = msm_ois_read_phone_ver(a_ctrl);
            if (rc < 0)
                pr_err("There is no OIS FW in the system. skip fw update from phone %d\n", rc);

            if (copy_to_user(cdata->version, &a_ctrl->phone_ver, sizeof(struct msm_ois_ver_t)))
                pr_err("copy to user failed \n");
        }
        break;

    case CFG_OIS_READ_CAL_INFO:
        CDBG("CFG_OIS_READ_CAL_INFO enter \n");
         rc = msm_ois_read_cal_info(a_ctrl);
        if (rc < 0)
            pr_err("ois read user data failed %d\n", rc);

        if (copy_to_user(cdata->ois_cal_info, &a_ctrl->cal_info, sizeof(struct msm_ois_cal_info_t)))
            pr_err("copy to user failed\n");
        break;

    case CFG_OIS_FW_UPDATE:
        CDBG("CFG_OIS_FW_UPDATE enter \n");
        rc = msm_ois_fw_update(a_ctrl, 0);
        if (rc < 0)
            pr_err("ois fw update failed %d\n", rc);
        break;
    case CFG_OIS_GET_FW_STATUS:
        CDBG("CFG_OIS_GET_FW_STATUS enter \n");
        rc = msm_ois_get_fw_status(a_ctrl);
        if (rc)
            pr_err("previous fw update failed , force update will be done %d\n", rc);
        break;

    case CFG_OIS_POWERDOWN:
        rc = msm_ois_power_down(a_ctrl);
        if (rc < 0)
            pr_err("msm_ois_power_down failed %d\n", rc);
        break;

    case CFG_OIS_POWERUP:
        rc = msm_ois_power_up(a_ctrl);
        if (rc < 0)
            pr_err("Failed ois power up%d\n", rc);
        break;

    default:
        break;
    }
    mutex_unlock(a_ctrl->ois_mutex);
    CDBG("Exit\n");
    return rc;
}

static int32_t msm_ois_get_subdev_id(struct msm_ois_ctrl_t *a_ctrl,
    void *arg)
{
    uint32_t *subdev_id = (uint32_t *)arg;
    CDBG("Enter\n");
    if (!subdev_id) {
        pr_err("failed\n");
        return -EINVAL;
    }
    if (a_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE)
        *subdev_id = a_ctrl->pdev->id;
    else
        *subdev_id = a_ctrl->subdev_id;

    CDBG_I("subdev_id %d\n", *subdev_id);
    CDBG("Exit\n");
    return 0;
}

static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
    .i2c_read = msm_camera_cci_i2c_read,
    .i2c_read_seq = msm_camera_cci_i2c_read_seq,
    .i2c_write = msm_camera_cci_i2c_write,
    .i2c_write_table = msm_camera_cci_i2c_write_table,
    .i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
    .i2c_write_table_w_microdelay =
        msm_camera_cci_i2c_write_table_w_microdelay,
    .i2c_util = msm_sensor_cci_i2c_util,
    .i2c_poll =  msm_camera_cci_i2c_poll,
};

static struct msm_camera_i2c_fn_t msm_sensor_qup_func_tbl = {
    .i2c_read = msm_camera_qup_i2c_read,
    .i2c_read_seq = msm_camera_qup_i2c_read_seq,
    .i2c_write = msm_camera_qup_i2c_write,
    .i2c_write_seq = msm_camera_qup_i2c_write_seq,
    .i2c_write_table = msm_camera_qup_i2c_write_table,
    .i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
    .i2c_write_table_w_microdelay =
        msm_camera_qup_i2c_write_table_w_microdelay,
    .i2c_poll = msm_camera_qup_i2c_poll,
};

static int msm_ois_open(struct v4l2_subdev *sd,
    struct v4l2_subdev_fh *fh) {
    int rc = 0;
    struct msm_ois_ctrl_t *a_ctrl =  v4l2_get_subdevdata(sd);
    CDBG_I("Enter\n");
    if (!a_ctrl) {
        pr_err("failed\n");
        return -EINVAL;
    }
    if (a_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
        rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_util(
            &a_ctrl->i2c_client, MSM_CCI_INIT);
        if (rc < 0)
            pr_err("cci_init failed\n");
    }
    if (a_ctrl->gpio_conf && a_ctrl->gpio_conf->cam_gpio_req_tbl) {
        CDBG("%s:%d request gpio\n", __func__, __LINE__);
        rc = msm_camera_request_gpio_table(
            a_ctrl->gpio_conf->cam_gpio_req_tbl,
            a_ctrl->gpio_conf->cam_gpio_req_tbl_size, 1);
        if (rc < 0) {
            pr_err("%s: request gpio failed\n", __func__);
            return rc;
        }
    }
    a_ctrl->is_camera_run = TRUE;
    a_ctrl->is_set_debug_info = FALSE;
    CDBG("Exit\n");
    return rc;
}

static int msm_ois_close(struct v4l2_subdev *sd,
    struct v4l2_subdev_fh *fh) {
    int rc = 0;
    struct msm_ois_ctrl_t *a_ctrl =  v4l2_get_subdevdata(sd);
    CDBG_I("Enter\n");
    if (!a_ctrl) {
        pr_err("failed\n");
        return -EINVAL;
    }
    if (a_ctrl->gpio_conf && a_ctrl->gpio_conf->cam_gpio_req_tbl) {
        CDBG("%s:%d release gpio\n", __func__, __LINE__);
        msm_camera_request_gpio_table(
            a_ctrl->gpio_conf->cam_gpio_req_tbl,
            a_ctrl->gpio_conf->cam_gpio_req_tbl_size, 0);
    }
    if (a_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
        rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_util(
            &a_ctrl->i2c_client, MSM_CCI_RELEASE);
        if (rc < 0)
            pr_err("cci_init failed\n");
    }
    a_ctrl->is_camera_run = FALSE;
    CDBG("Exit\n");
    return rc;
}

static const struct v4l2_subdev_internal_ops msm_ois_internal_ops = {
    .open = msm_ois_open,
    .close = msm_ois_close,
};

static long msm_ois_subdev_ioctl(struct v4l2_subdev *sd,
            unsigned int cmd, void *arg)
{
    struct msm_ois_ctrl_t *a_ctrl = v4l2_get_subdevdata(sd);
    void __user *argp = (void __user *)arg;
    CDBG_I("%s:%d a_ctrl %p argp %p\n", __func__, __LINE__, a_ctrl, argp);
    switch (cmd) {
    case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
        return msm_ois_get_subdev_id(a_ctrl, argp);
    case VIDIOC_MSM_OIS_IO_CFG:
        return msm_ois_config(a_ctrl, argp);
    case MSM_SD_SHUTDOWN:
        msm_ois_close(sd, NULL);
        return 0;
    default:
        return -ENOIOCTLCMD;
    }
}

static int32_t msm_ois_power(struct v4l2_subdev *sd, int on)
{
    int rc = 0;
    struct msm_ois_ctrl_t *a_ctrl = v4l2_get_subdevdata(sd);
    CDBG("Enter\n");
    mutex_lock(a_ctrl->ois_mutex);
    if (on)
        rc = msm_ois_power_up(a_ctrl);
    else
        rc = msm_ois_power_down(a_ctrl);
    mutex_unlock(a_ctrl->ois_mutex);
    CDBG_I("Exit Power[%s] Status[%d]\n", on? "ON" : "OFF", rc);
    return rc;
}

static struct v4l2_subdev_core_ops msm_ois_subdev_core_ops = {
    .ioctl = msm_ois_subdev_ioctl,
    .s_power = msm_ois_power,
};

static struct v4l2_subdev_ops msm_ois_subdev_ops = {
    .core = &msm_ois_subdev_core_ops,
};

static int msm_ois_power_up(struct msm_ois_ctrl_t *ctrl)
{
    int rc;
    CDBG("%s:%d E\n", __func__, __LINE__);

    if (ctrl->is_camera_run == FALSE && ctrl->ois_state == OIS_POWER_DOWN) {

        CDBG("[%p] [%p]\n", ctrl->pinctrl_info.pinctrl,
            ctrl->pinctrl_info.gpio_state_active);

        pinctrl_select_state(ctrl->pinctrl_info.pinctrl,
            ctrl->pinctrl_info.gpio_state_active);

        rc = gpio_request(ctrl->ois_en, "ois_en");
        if (rc) {
            pr_err("%s:%d Failed rc[%d]\n", __func__, __LINE__, rc);
            gpio_free(ctrl->ois_en);
            goto ERR;
        }

        rc = gpio_request(ctrl->ois_reset, "ois_reset");
        if (rc) {
            pr_err("%s:%d Failed rc[%d]\n", __func__, __LINE__, rc);
            gpio_free(ctrl->ois_en);
            gpio_free(ctrl->ois_reset);
            goto ERR;
        }

        gpio_direction_output(ctrl->ois_en, 1);
        gpio_direction_output(ctrl->ois_reset, 1);
        msleep(10);
        CDBG_I("GPIO status En[%d] Reset[%d]\n", gpio_get_value(ctrl->ois_en), gpio_get_value(ctrl->ois_reset));

        ctrl->ois_state = OIS_POWER_UP;

        msm_ois_fw_update(ctrl, 1);
    }

    CDBG("%s:%d X\n", __func__, __LINE__);
    return 0;
ERR:
    pr_err("%s:%d Failed!\n", __func__, __LINE__);
    return -EINVAL;
}

static int msm_ois_power_down(struct msm_ois_ctrl_t *ctrl)
{
    CDBG("%s:%d E\n", __func__, __LINE__);

    if (ctrl->is_camera_run == FALSE && ctrl->ois_state == OIS_POWER_UP) {

        CDBG("[%p] [%p]\n", ctrl->pinctrl_info.pinctrl,
            ctrl->pinctrl_info.gpio_state_suspend);

        gpio_direction_output(ctrl->ois_en, 0);
        gpio_direction_output(ctrl->ois_reset, 0);
        gpio_free(ctrl->ois_en);
        gpio_free(ctrl->ois_reset);

        pinctrl_select_state(ctrl->pinctrl_info.pinctrl,
            ctrl->pinctrl_info.gpio_state_suspend);

        ctrl->ois_state = OIS_POWER_DOWN;
    }

    CDBG_I("X\n");
    return 0;
}

static int msm_ois_pinctrl_init(struct msm_ois_ctrl_t *ctrl,
    struct device_node *of_node)
{
    struct msm_pinctrl_info *ois_pctrl = NULL;
    ois_pctrl = &ctrl->pinctrl_info;
    ois_pctrl->pinctrl = devm_pinctrl_get(ctrl->dev);
    if (IS_ERR_OR_NULL(ois_pctrl->pinctrl)) {
        pr_err("%s:%d Getting pinctrl handle failed\n",
            __func__, __LINE__);
        return -EINVAL;
    }

    ois_pctrl->gpio_state_active =
        pinctrl_lookup_state(ois_pctrl->pinctrl,
                OIS_PINCTRL_STATE_DEFAULT);
    if (IS_ERR_OR_NULL(ois_pctrl->gpio_state_active)) {
        pr_err("%s:%d Failed to get the active state pinctrl handle\n",
            __func__, __LINE__);
        return -EINVAL;
    }

    ois_pctrl->gpio_state_suspend
        = pinctrl_lookup_state(ois_pctrl->pinctrl,
                OIS_PINCTRL_STATE_SLEEP);
    if (IS_ERR_OR_NULL(ois_pctrl->gpio_state_suspend)) {
        pr_err("%s:%d Failed to get the suspend state pinctrl handle\n",
                __func__, __LINE__);
        return -EINVAL;
    }

    return 0;
}



static int32_t msm_ois_get_gpio_data(struct msm_ois_ctrl_t *ctrl,
    struct device_node *of_node)
{
    int rc = 0;

    ctrl->ois_en = of_get_named_gpio(of_node, "ois,ois-en", 0);
    if (ctrl->ois_en < 0) {
        pr_err("%s:%d failed!\n", __func__, __LINE__);
        return -EINVAL;
    }

    ctrl->ois_reset = of_get_named_gpio(of_node, "ois,ois-reset", 0);
    if (ctrl->ois_reset < 0) {
        pr_err("%s:%d failed!\n", __func__, __LINE__);
        return -EINVAL;
    }

    rc = msm_ois_pinctrl_init(ctrl, of_node);

    if (rc < 0) {
        pr_err("%s:%d failed!\n", __func__, __LINE__);
        return -EINVAL;
    }

    pinctrl_select_state(ctrl->pinctrl_info.pinctrl,
        ctrl->pinctrl_info.gpio_state_suspend);


    return 0;
}

static int32_t msm_ois_i2c_probe(struct i2c_client *client,
    const struct i2c_device_id *id)
{
    int rc = 0;
    uint32_t temp;
    struct msm_ois_ctrl_t *ois_ctrl_t = NULL;
    struct msm_ois_vreg *vreg_cfg;
    bool check_use_gpios;

    CDBG_I("Enter\n");

    if (client == NULL) {
        pr_err("msm_ois_i2c_probe: client is null\n");
        rc = -EINVAL;
        goto probe_failure;
    }

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        pr_err("i2c_check_functionality failed\n");
        goto probe_failure;
    }

    if (!client->dev.of_node) {
        ois_ctrl_t = (struct msm_ois_ctrl_t *)(id->driver_data);
    } else {
        ois_ctrl_t = kzalloc(sizeof(struct msm_ois_ctrl_t),
            GFP_KERNEL);
        if (!ois_ctrl_t) {
            pr_err("%s:%d no memory\n", __func__, __LINE__);
            return -ENOMEM;
        }
        ois_ctrl_t->dev = &client->dev;

        CDBG("client = 0x%p\n",  client);

        rc = of_property_read_u32(client->dev.of_node, "cell-index",
            &ois_ctrl_t->subdev_id);
        CDBG("cell-index %d, rc %d\n", ois_ctrl_t->subdev_id, rc);
        ois_ctrl_t->cam_name = ois_ctrl_t->subdev_id;
        if (rc < 0) {
            pr_err("failed rc %d\n", rc);
            kfree(ois_ctrl_t);//prevent
            return rc;
        }
        check_use_gpios = of_property_read_bool(client->dev.of_node, "unuse-gpios");
        CDBG("%s: check unuse-gpio flag(%d)\n",
            __FUNCTION__, check_use_gpios);
        if (!check_use_gpios) {
            rc = msm_ois_get_gpio_data(ois_ctrl_t,
                client->dev.of_node);
        }
    }

    if (of_find_property(client->dev.of_node,
            "qcom,cam-vreg-name", NULL)) {
        vreg_cfg = &ois_ctrl_t->vreg_cfg;
        rc = msm_camera_get_dt_vreg_data(client->dev.of_node,
            &vreg_cfg->cam_vreg, &vreg_cfg->num_vreg);
        if (rc < 0) {
            kfree(ois_ctrl_t);
            pr_err("failed rc %d\n", rc);
            return rc;
        }
    }

    rc = of_property_read_u32(client->dev.of_node, "ois,slave-addr",
            &temp);
    if (rc < 0) {
            pr_err("%s failed rc %d\n", __func__, rc);
            kfree(ois_ctrl_t);
            return rc;
    }
    client->addr = temp;
    CDBG("Slave ID[0x%x]\n", client->addr);

    ois_ctrl_t->ois_v4l2_subdev_ops = &msm_ois_subdev_ops;
    ois_ctrl_t->ois_mutex = &msm_ois_mutex;
    ois_ctrl_t->i2c_driver = &msm_ois_i2c_driver;

    CDBG("client = %x\n", (unsigned int) client);
    ois_ctrl_t->i2c_client.client = client;
    ois_ctrl_t->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
    /* Set device type as I2C */
    ois_ctrl_t->ois_device_type = MSM_CAMERA_I2C_DEVICE;
    ois_ctrl_t->i2c_client.i2c_func_tbl = &msm_sensor_qup_func_tbl;
    ois_ctrl_t->ois_v4l2_subdev_ops = &msm_ois_subdev_ops;
    ois_ctrl_t->ois_mutex = &msm_ois_mutex;
    ois_ctrl_t->ois_state = OIS_POWER_DOWN;
    ois_ctrl_t->is_camera_run = FALSE;
    ois_ctrl_t->ois_mode = OIS_MODE_OFF;

    ois_ctrl_t->cam_name = ois_ctrl_t->subdev_id;
    CDBG("ois_ctrl_t->cam_name: %d", ois_ctrl_t->cam_name);
    /* Assign name for sub device */
    snprintf(ois_ctrl_t->msm_sd.sd.name, sizeof(ois_ctrl_t->msm_sd.sd.name),
        "%s", ois_ctrl_t->i2c_driver->driver.name);

    /* Initialize sub device */
    v4l2_i2c_subdev_init(&ois_ctrl_t->msm_sd.sd,
        ois_ctrl_t->i2c_client.client,
        ois_ctrl_t->ois_v4l2_subdev_ops);
    v4l2_set_subdevdata(&ois_ctrl_t->msm_sd.sd, ois_ctrl_t);
    ois_ctrl_t->msm_sd.sd.internal_ops = &msm_ois_internal_ops;
    ois_ctrl_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
    media_entity_init(&ois_ctrl_t->msm_sd.sd.entity, 0, NULL, 0);
    ois_ctrl_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
    ois_ctrl_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_OIS;
    ois_ctrl_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0xB;
    msm_sd_register(&ois_ctrl_t->msm_sd);
    //g_ois_i2c_client.client = ois_ctrl_t->i2c_client.client;
    g_msm_ois_t = ois_ctrl_t;

    msm_ois_get_fw_version(ois_ctrl_t);
    msm_ois_get_cal_version(ois_ctrl_t);

    CDBG("Succeded Exit\n");
    return rc;
probe_failure:
    if (ois_ctrl_t)
        kfree(ois_ctrl_t);
    return rc;
}

static int32_t msm_ois_platform_probe(struct platform_device *pdev)
{
    int32_t rc = 0;
    struct msm_camera_cci_client *cci_client = NULL;
    struct msm_ois_ctrl_t *msm_ois_t = NULL;
    struct msm_ois_vreg *vreg_cfg;

    CDBG_I("Enter\n");

    if (!pdev->dev.of_node) {
        pr_err("of_node NULL\n");
        return -EINVAL;
    }

    msm_ois_t = kzalloc(sizeof(struct msm_ois_ctrl_t),
        GFP_KERNEL);
    if (!msm_ois_t) {
        pr_err("%s:%d failed no memory\n", __func__, __LINE__);
        return -ENOMEM;
    }
    rc = of_property_read_u32((&pdev->dev)->of_node, "cell-index",
        &pdev->id);
    CDBG("cell-index %d, rc %d\n", pdev->id, rc);
    if (rc < 0) {
        kfree(msm_ois_t);
        pr_err("failed rc %d\n", rc);
        return rc;
    }
    msm_ois_t->subdev_id = pdev->id;
    rc = of_property_read_u32((&pdev->dev)->of_node, "qcom,cci-master",
        &msm_ois_t->cci_master);
    CDBG("qcom,cci-master %d, rc %d\n", msm_ois_t->cci_master, rc);
    if (rc < 0) {
        kfree(msm_ois_t);
        pr_err("failed rc %d\n", rc);
        return rc;
    }
    if (of_find_property((&pdev->dev)->of_node,
            "qcom,cam-vreg-name", NULL)) {
        vreg_cfg = &msm_ois_t->vreg_cfg;
        rc = msm_camera_get_dt_vreg_data((&pdev->dev)->of_node,
            &vreg_cfg->cam_vreg, &vreg_cfg->num_vreg);
        if (rc < 0) {
            kfree(msm_ois_t);
            pr_err("failed rc %d\n", rc);
            return rc;
        }
    }

    msm_ois_t->ois_v4l2_subdev_ops = &msm_ois_subdev_ops;
    msm_ois_t->ois_mutex = &msm_ois_mutex;
    msm_ois_t->cam_name = pdev->id;

    /* Set platform device handle */
    msm_ois_t->pdev = pdev;
    /* Set device type as platform device */
    msm_ois_t->ois_device_type = MSM_CAMERA_PLATFORM_DEVICE;
    msm_ois_t->i2c_client.i2c_func_tbl = &msm_sensor_cci_func_tbl;
    msm_ois_t->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
    msm_ois_t->i2c_client.cci_client = kzalloc(sizeof(
        struct msm_camera_cci_client), GFP_KERNEL);
    if (!msm_ois_t->i2c_client.cci_client) {
        kfree(msm_ois_t->vreg_cfg.cam_vreg);
        kfree(msm_ois_t);
        pr_err("failed no memory\n");
        return -ENOMEM;
    }
    msm_ois_t->is_camera_run= FALSE;

    cci_client = msm_ois_t->i2c_client.cci_client;
    cci_client->cci_subdev = msm_cci_get_subdev();
    cci_client->cci_i2c_master = MASTER_MAX;
    v4l2_subdev_init(&msm_ois_t->msm_sd.sd,
        msm_ois_t->ois_v4l2_subdev_ops);
    v4l2_set_subdevdata(&msm_ois_t->msm_sd.sd, msm_ois_t);
    msm_ois_t->msm_sd.sd.internal_ops = &msm_ois_internal_ops;
    msm_ois_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
    snprintf(msm_ois_t->msm_sd.sd.name,
        ARRAY_SIZE(msm_ois_t->msm_sd.sd.name), "msm_ois");
    media_entity_init(&msm_ois_t->msm_sd.sd.entity, 0, NULL, 0);
    msm_ois_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
    msm_ois_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_OIS;
    msm_ois_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0xB;
    rc = msm_sd_register(&msm_ois_t->msm_sd);
    //g_ois_i2c_client.cci_client = msm_ois_t->i2c_client.cci_client;
    g_msm_ois_t = msm_ois_t;

    CDBG("Exit[rc::%d]\n", rc);
    return rc;
}

static const struct i2c_device_id msm_ois_i2c_id[] = {
    { "msm_ois", (kernel_ulong_t)NULL },
    { }
};
static const struct of_device_id msm_ois_dt_match[] = {
    {.compatible = "qcom,ois", .data = NULL},
    {}
};

static struct i2c_driver msm_ois_i2c_driver = {
    .id_table = msm_ois_i2c_id,
    .probe  = msm_ois_i2c_probe,
    .remove = __exit_p(msm_ois_i2c_remove),
    .driver = {
        .name = "msm_ois",
        .owner = THIS_MODULE,
        .of_match_table = msm_ois_dt_match,
    },
};

MODULE_DEVICE_TABLE(of, msm_ois_dt_match);

static struct platform_driver msm_ois_platform_driver = {
    .driver = {
        .name = "qcom,ois",
        .owner = THIS_MODULE,
        .of_match_table = msm_ois_dt_match,
    },
};

static bool msm_ois_diff_test(struct msm_ois_ctrl_t *a_ctrl, int *x_diff, int *y_diff)
{
    int ret;
    int X_Max, X_Min, Y_Max, Y_Min;
    int default_diff = 1100;
    u8 m_val[2] = {0,};

    ret = msm_ois_i2c_byte_write(a_ctrl, 0x6020, 0x01);

    if (ret < 0) {
        return false;
    }
    msm_ois_read_status(a_ctrl);

    msm_ois_i2c_byte_write(a_ctrl, 0x6020, 0x04);      // Calibration Mode

    m_val[0] = 0x05;
    m_val[1] = 0x00;
    msm_ois_i2c_write_multi(a_ctrl, 0x6064, m_val, 2);
    msleep(100);
    msm_ois_i2c_byte_write(a_ctrl, 0x6060, 0x00);
    msm_ois_i2c_read_multi(a_ctrl, 0x6062, m_val, 2);
    X_Max = (m_val[0] << 8) + m_val[1];

    m_val[0] = 0xFB;
    m_val[1] = 0x00;
    msm_ois_i2c_write_multi(a_ctrl, 0x6064, m_val, 2);
    msleep(100);
    msm_ois_i2c_byte_write(a_ctrl, 0x6060, 0x00);
    msm_ois_i2c_read_multi(a_ctrl, 0x6062, m_val, 2);
    X_Min = (m_val[0] << 8) + m_val[1];


    m_val[0] = 0x00;
    m_val[1] = 0x00;
    msm_ois_i2c_write_multi(a_ctrl, 0x6064, m_val, 2);
    msleep(100);

    m_val[0] = 0x05;
    m_val[1] = 0x00;
    msm_ois_i2c_write_multi(a_ctrl, 0x6066, m_val, 2);
    msleep(100);
    msm_ois_i2c_byte_write(a_ctrl, 0x6060, 0x01);
    msm_ois_i2c_read_multi(a_ctrl, 0x6062, m_val, 2);
    Y_Max = (m_val[0] << 8) + m_val[1];

    m_val[0] = 0xFB;
    m_val[1] = 0x00;
    msm_ois_i2c_write_multi(a_ctrl, 0x6066, m_val, 2);
    msleep(100);
    msm_ois_i2c_byte_write(a_ctrl, 0x6060, 0x01);
    msm_ois_i2c_read_multi(a_ctrl, 0x6062, m_val, 2);
    Y_Min = (m_val[0] << 8) + m_val[1];

    m_val[0] = 0x00;
    m_val[1] = 0x00;
    msm_ois_i2c_write_multi(a_ctrl, 0x6066, m_val, 2);
    msleep(100);

    msm_ois_i2c_byte_write(a_ctrl, 0x6020, 0x00);     // Standby Mode

    *x_diff = abs(X_Max - X_Min);
    *y_diff = abs(Y_Max - Y_Min);


    if (*x_diff > default_diff && *y_diff > default_diff) {
        return true;
    } else {
        return false;
    }

}

static int msm_ois_gyro_selftest(struct msm_ois_ctrl_t *a_ctrl)
 {
    u16 status = 0;
    msm_ois_i2c_byte_write(a_ctrl, 0x6020, 0x04);          // Calibration Mode
    msleep(10);
    msm_ois_i2c_byte_write(a_ctrl, 0x6023, 0x02);
    msleep(10);
    msm_ois_i2c_byte_write(a_ctrl, 0x6138, 0x00);
    msm_ois_read_status(a_ctrl);

    //msleep(50);

    msm_ois_i2c_byte_read(a_ctrl, 0x6139, &status);
    CDBG_I("status[%d]\n", status);
    return status == 3 ? 0 : 1;  //OK = 3  ,  NG = 0~2
}

static int msm_is_ois_get_offset(struct msm_ois_ctrl_t *a_ctrl, long *raw_data_x, long *raw_data_y)
{
    char *buf = NULL;
    uint16_t x_temp = 0, y_temp = 0;
    int16_t x_gyro = 0, y_gyro = 0;

    CDBG("E\n");

    buf = msm_ois_base(0);
    if (!buf)
        return -1;

    x_temp = (buf [MODULE_OIS_CAL_DATA_OFFSET + 0x18] << 8)
            + buf [MODULE_OIS_CAL_DATA_OFFSET + 0x19];
    x_gyro = *((int16_t *)(&x_temp));

    y_temp = (buf [MODULE_OIS_CAL_DATA_OFFSET + 0x1A] << 8)
            + buf [MODULE_OIS_CAL_DATA_OFFSET + 0x1B];
    y_gyro = *((int16_t *)(&y_temp));

    *raw_data_x = x_gyro;
    *raw_data_y = y_gyro;


    CDBG("X\n");
    return 0;
}

static int msm_ois_gyro_offset_adjustment(struct msm_ois_ctrl_t *a_ctrl, long *gyro_offset_x, long *gyro_offset_y)
{
    int i;
    int ret;
    int scale_factor;
    u16 x_sum = 0, y_sum = 0, sum = 0;
    u16 x_gyro = 0, y_gyro = 0;
    u8 status[2] = {0};
    u8 avg_cnt = 10;

    CDBG_I("E\n");

    *gyro_offset_x = 0;
    *gyro_offset_y = 0;

    if( msm_ois_cal_revision(a_ctrl->cal_info.cal_ver) < 4 )
        scale_factor = OIS_GYRO_SCALE_FACTOR_V003;
    else
        scale_factor = OIS_GYRO_SCALE_FACTOR_V004;

    ret = msm_ois_i2c_byte_write(a_ctrl, 0x6020, 0x01); /* Servo On/ OIS Off */
    msleep(100);
    ret |= msm_ois_read_status(a_ctrl);
    if (ret)
        goto out;

    ret |= msm_ois_i2c_byte_write(a_ctrl, 0x6023, 0x00); /* Gyro On*/
    if (ret)
        goto out;

    for(i = 1; i <= avg_cnt; i++) {
        ret |= msm_ois_i2c_byte_write(a_ctrl, 0x6088, 0x00);
        ret |= msm_ois_read_status(a_ctrl);

        msm_ois_i2c_read_multi(a_ctrl, 0x608A, status, 2);
        x_gyro = (status[0] << 8) + status[1];
        sum = *((int16_t *)(&x_gyro));
        y_sum = y_sum + sum;
        x_sum = x_sum + x_gyro;

        CDBG_I("i[%d] x_offset[%d]\n", i, x_gyro);
        ret |= msm_ois_i2c_byte_write(a_ctrl, 0x6088, 0x01);
        ret |= msm_ois_read_status(a_ctrl);

        msm_ois_i2c_read_multi(a_ctrl, 0x608A, status, 2);
        y_gyro = (status[0] << 8) + status[1];
        sum = *((int16_t *)(&y_gyro));
        y_sum = y_sum + sum;
        CDBG_I("i[%d] y_offset[%d]\n", i, y_gyro);
    }

    x_sum = x_sum * 1000 / avg_cnt;
    y_sum = y_sum * 1000 / avg_cnt;

    *gyro_offset_x = x_sum / scale_factor;
    *gyro_offset_y = y_sum / scale_factor;

    CDBG_I("X Offset[%ld], Y Offset[%ld]\n", *gyro_offset_x, *gyro_offset_y);

    return 0;
out:
    pr_err("%s:%d Failed!\n", __func__, __LINE__);
    return -EINVAL;
}



static ssize_t gyro_selftest_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int result_total = 0;
    bool result_offset = true, result_selftest = false;
    unsigned char selftest_ret = 0;
    long raw_data_x = 0, raw_data_y = 0;
    int final_x1, final_x2, final_y1, final_y2;

    msm_ois_gyro_offset_adjustment(g_msm_ois_t, &raw_data_x, &raw_data_y);
    msleep(50);
    selftest_ret = msm_ois_gyro_selftest(g_msm_ois_t);

    if (selftest_ret == 0x0)
        result_selftest = true;

    if (abs(raw_data_x) > 35000 || abs(raw_data_y) > 35000)
        result_offset = false;

    if (result_offset && result_selftest)
        result_total = 0;
    else if (!result_offset && !result_selftest)
        result_total = 3;
    else if (!result_offset)
        result_total = 1;
    else if (!result_selftest)
        result_total = 2;

    final_x1 = abs(raw_data_x / 1000);
    final_y1 = abs(raw_data_y / 1000);
    final_x2 = abs(raw_data_x % 1000);
    final_y2 = abs(raw_data_y % 1000);

    pr_err("Result : 0 (success), 1 (offset fail), 2 (selftest fail) , 3 (both fail) \n");
    pr_err("Result : %d, result x = %d.%03d, result y = %d.%03d\n",
        result_total , final_x1, final_x2, final_y1, final_y2);

    if (raw_data_x < 0 && raw_data_y < 0)
        return sprintf(buf, "%d,-%d.%03d,-%d.%03d\n", result_total, final_x1, final_x2, final_y1, final_y2);
    else if (raw_data_x < 0)
        return sprintf(buf, "%d,-%d.%03d,%d.%03d\n", result_total, final_x1, final_x2, final_y1, final_y2);
    else if (raw_data_y < 0)
        return sprintf(buf, "%d,%d.%03d,-%d.%03d\n", result_total, final_x1, final_x2, final_y1, final_y2);
    else
        return sprintf(buf, "%d,%d.%03d,%d.%03d\n", result_total, final_x1, final_x2, final_y1, final_y2);
}

static ssize_t ois_hall_cal_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int result;
    int x_diff = 0, y_diff = 0;
    //msm_actuator_move_for_ois_test();
    result = msm_ois_diff_test(g_msm_ois_t, &x_diff, &y_diff);
    return sprintf(buf, "%d,%d,%d\n", result == true ? 0 : 1, x_diff, y_diff);
}


static ssize_t gyro_rawdata_test_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    long raw_data_x = 0, raw_data_y = 0;

    msm_is_ois_get_offset(g_msm_ois_t, &raw_data_x, &raw_data_y);

    CDBG_I("raw data x = %ld.%03ld, raw data y = %ld.%03ld\n", raw_data_x /1000, raw_data_x % 1000,
        raw_data_y /1000, raw_data_y % 1000);

    if (raw_data_x < 0 && raw_data_y < 0) {
        return sprintf(buf, "-%ld.%03ld,-%ld.%03ld\n", abs(raw_data_x /1000), abs(raw_data_x % 1000),
            abs(raw_data_y /1000), abs(raw_data_y % 1000));
    } else if (raw_data_x < 0) {
        return sprintf(buf, "-%ld.%03ld,%ld.%03ld\n", abs(raw_data_x /1000), abs(raw_data_x % 1000),
            raw_data_y /1000, raw_data_y % 1000);
    } else if (raw_data_y < 0) {
        return sprintf(buf, "%ld.%03ld,-%ld.%03ld\n", raw_data_x /1000, raw_data_x % 1000,
            abs(raw_data_y /1000), abs(raw_data_y % 1000));
    } else {
        return sprintf(buf, "%ld.%03ld,%ld.%03ld\n", raw_data_x /1000, raw_data_x % 1000,
            raw_data_y /1000, raw_data_y % 1000);
    }
}

static ssize_t ois_fw_full_show(struct device *dev,
                     struct device_attribute *attr, char *buf)
{
    CDBG("[FW_DBG] OIS_fw_ver : %s\n", ois_fw_full);
    return sprintf(buf, "%s", ois_fw_full);
}

static ssize_t ois_fw_full_store(struct device *dev,
                      struct device_attribute *attr, const char *buf, size_t size)
{
    CDBG("[FW_DBG] buf : %s\n", buf);
    snprintf(ois_fw_full, sizeof(ois_fw_full), "%s", buf);

    return size;
}


static ssize_t ois_power_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    bool is_camera_run = g_msm_ois_t->is_camera_run;

    if((g_msm_ois_t->i2c_client.client==NULL)&&(g_msm_ois_t->pdev==NULL)) {
        return size;
    }

    if(!is_camera_run){
        switch (buf[0]) {
        case '0' :
            msm_ois_power_down(g_msm_ois_t);
            pr_err("ois_power_store : power down \n");
            break;
        case '1' :
            msm_ois_power_up(g_msm_ois_t);
            pr_err("ois_power_store : power up \n");
            break;
        default:
            break;
        }
    }
    return size;
}


char ois_debug[40] = "NULL NULL NULL\n";
static ssize_t ois_exif_show(struct device *dev,
                     struct device_attribute *attr, char *buf)
{
    CDBG("[FW_DBG] ois_debug : %s\n", ois_debug);
    return snprintf(buf, sizeof(ois_debug), "%s", ois_debug);
}

static ssize_t ois_exif_store(struct device *dev,
                      struct device_attribute *attr, const char *buf, size_t size)
{
    CDBG("[FW_DBG] buf: %s\n", buf);
    snprintf(ois_debug, sizeof(ois_debug), "%s", buf);

    return size;
}


static DEVICE_ATTR(selftest, S_IRUGO, gyro_selftest_show, NULL);
static DEVICE_ATTR(ois_diff, S_IRUGO, ois_hall_cal_test_show, NULL);
static DEVICE_ATTR(ois_rawdata, S_IRUGO, gyro_rawdata_test_show, NULL);
static DEVICE_ATTR(ois_power, S_IWUGO, NULL, ois_power_store);
static DEVICE_ATTR(oisfw, S_IRUGO|S_IWUSR|S_IWGRP, ois_fw_full_show, ois_fw_full_store);
static DEVICE_ATTR(ois_exif, S_IRUGO|S_IWUSR|S_IWGRP, ois_exif_show, ois_exif_store);


static int __init msm_ois_init_module(void)
{
    int32_t rc = 0;
    struct device *cam_ois;

    CDBG_I("Enter\n");

    if (!IS_ERR(camera_class)) { //for sysfs
        cam_ois = device_create(camera_class, NULL, 0, NULL, "ois");
        if (IS_ERR(cam_ois)) {
            pr_err("Failed to create device(ois) in camera_class!\n");
            rc = -ENOENT;
        }
        if (device_create_file(cam_ois, &dev_attr_ois_power) < 0) {
            pr_err("failed to create device file, %s\n",
            dev_attr_ois_power.attr.name);
            rc = -ENOENT;
        }
        if (device_create_file(cam_ois, &dev_attr_selftest) < 0) {
            pr_err("failed to create device file, %s\n",
            dev_attr_selftest.attr.name);
            rc = -ENOENT;
        }
        if (device_create_file(cam_ois, &dev_attr_ois_diff) < 0) {
            pr_err("failed to create device file, %s\n",
            dev_attr_ois_diff.attr.name);
            rc = -ENOENT;
        }
        if (device_create_file(cam_ois, &dev_attr_ois_rawdata) < 0) {
            pr_err("failed to create device file, %s\n",
            dev_attr_ois_rawdata.attr.name);
            rc = -ENOENT;
        }
        if (device_create_file(cam_ois, &dev_attr_oisfw) < 0) {
            printk("Failed to create device file!(%s)!\n",
                dev_attr_oisfw.attr.name);
            rc = -ENODEV;
        }
        if (device_create_file(cam_ois, &dev_attr_ois_exif) < 0) {
            printk("Failed to create device file!(%s)!\n",
                dev_attr_ois_exif.attr.name);
            rc = -ENODEV;
        }

    } else {
    pr_err("Failed to create device(ois) because of no camera class!\n");
        rc = -EINVAL;
    }

    rc = platform_driver_probe(&msm_ois_platform_driver,
        msm_ois_platform_probe);
    if (rc < 0) {
        pr_err("%s:%d failed platform driver probe rc %d\n",
            __func__, __LINE__, rc);
    } else {
        CDBG("%s:%d platform_driver_probe rc %d\n", __func__, __LINE__, rc);
    }
    rc = i2c_add_driver(&msm_ois_i2c_driver);
    if (rc < 0)
        pr_err("%s:%d failed i2c driver probe rc %d\n",
            __func__, __LINE__, rc);
    else
        CDBG("%s:%d i2c_add_driver rc %d\n", __func__, __LINE__, rc);

    return rc;
}

module_init(msm_ois_init_module);
MODULE_DESCRIPTION("MSM OIS");
MODULE_LICENSE("GPL v2");
