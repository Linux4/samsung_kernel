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

//#define MSM_OIS_DEBUG

DEFINE_MSM_MUTEX(msm_ois_mutex);

#undef CDBG_FW
#ifdef MSM_OIS_FW_DEBUG
#define CDBG_FW(fmt, args...) pr_err("[OIS_FW_DBG][%d]"fmt,__LINE__, ##args)
#else
#define CDBG_FW(fmt, args...) do { } while (0)
#endif

#if defined(CONFIG_SENSOR_RETENTION)
extern bool sensor_retention_mode;
#endif

static struct v4l2_file_operations msm_ois_v4l2_subdev_fops;

#define OIS_FW_UPDATE_PACKET_SIZE 256
#define PROGCODE_SIZE (1024 * 28)

#define OIS_USER_DATA_START_ADDR  (0x7400)

#define MAX_RETRY_COUNT               (3)

#undef CDBG
#ifdef MSM_OIS_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

#define CONFIG_MSM_CAMERA_DEBUG_INFO
#undef CDBG_I
#ifdef CONFIG_MSM_CAMERA_DEBUG_INFO
#define CDBG_I(fmt, args...) pr_info(fmt, ##args)
#else
#define CDBG_I(fmt, args...) do { } while (0)
#endif

extern int16_t msm_actuator_move_for_ois_test(void);

static int32_t msm_ois_power_up(struct msm_ois_ctrl_t *a_ctrl);
static int32_t msm_ois_power_down(struct msm_ois_ctrl_t *a_ctrl);
static int32_t msm_ois_check_extclk(struct msm_ois_ctrl_t *a_ctrl) ;

static struct i2c_driver msm_ois_i2c_driver;
struct msm_ois_ctrl_t *g_msm_ois_t;
extern struct class *camera_class; /*sys/class/camera*/
#if 1//defined(SAMSUNG_OIS_RUMBA_S4)
struct msm_camera_power_ctrl_t *g_ois_power_info;
#endif

#define GPIO_LEVEL_LOW        0
#define GPIO_LEVEL_HIGH       1
#define GPIO_CAM_RESET        30

#if 1//defined(SAMSUNG_OIS_RUMBA_S4)
#define OIS_FW_STATUS_OFFSET    (0x00FC)
#define OIS_FW_STATUS_SIZE      (4)
#define OIS_HW_VERSION_OFFSET   (0x6FF1)
#define OIS_FW_VERSION_OFFSET   (0x6FED)
#define CAMERA_OIS_EXT_CLK_12MHZ 0xB71B00
#define CAMERA_OIS_EXT_CLK_17MHZ 0x1036640
#define CAMERA_OIS_EXT_CLK_19MHZ 0x124F800
#define CAMERA_OIS_EXT_CLK_24MHZ 0x16E3600
#define CAMERA_OIS_EXT_CLK_26MHZ 0x18CBA80
#define CAMERA_OIS_EXT_CLK CAMERA_OIS_EXT_CLK_24MHZ
#else
#define OIS_FW_STATUS_OFFSET    (0x0006)
#define OIS_FW_STATUS_SIZE      (1)
#define OIS_FW_VERSION_SIZE     (6)
#define OIS_HW_VERSION_OFFSET    (0x6FF2)
#endif
#define OIS_HW_VERSION_SIZE     (3)
#define OIS_DEBUG_INFO_SIZE     (40)
#define OIS_FW_PATH "/system/etc/firmware"
#define OIS_FW_DOM_NAME "ois_fw_dom.bin"
#define OIS_FW_SEC_NAME "ois_fw_sec.bin"
#define SYSFS_OIS_DEBUG_PATH  "/sys/class/camera/ois/ois_exif"

static int msm_ois_get_fw_status(struct msm_ois_ctrl_t *a_ctrl)
{
    int rc = 0;
    uint32_t i =0;
    uint8_t status_arr[OIS_FW_STATUS_SIZE];
    uint32_t status = 0;

    rc = msm_ois_i2c_seq_read(a_ctrl, OIS_FW_STATUS_OFFSET, status_arr, OIS_FW_STATUS_SIZE);
    if (rc < 0) {
        pr_err("%s : i2c read fail\n", __func__);
    }

    for (i = 0; i < OIS_FW_STATUS_SIZE; i++)
        status |= status_arr[i] << (i * 8);
    a_ctrl->is_force_update = false;

#if 1//defined(SAMSUNG_OIS_RUMBA_S4)
    // In case previous update failed, (like removing the battery during update)
    // Module itself set the 0x00FC ~ 0x00FF register as error status
    // So if previous fw update failed, 0x00FC ~ 0x00FF register value is '4451'
    if (status == 4451) { //previous fw update failed, 0x00FC ~ 0x00FF register value is 4451
        a_ctrl->is_force_update = true;
        return -1;
    }
    return 0;
#else
    // In case previous update failed, (like removing the battery during update)
    // Module itself set the 0x0006 register as error status
    // So if previous fw update failed, 0x0006 register value is not '0'
    return status;
#endif
}

static int msm_ois_set_shift(struct msm_ois_ctrl_t *a_ctrl)
{
    int rc = 0, retries = 20;
    uint16_t status = 0;
    uint16_t servoVal = 0;
    unsigned char SendData[4];

    CDBG("Enter\n");
    msm_ois_i2c_byte_read(a_ctrl, 0x0000, &servoVal);   // Read current servo status

    rc = msm_ois_i2c_byte_write(a_ctrl, 0x0000, 0x00);   // OIS servo off
    if (rc < 0) {
        pr_err("ois servo off failed, i2c fail\n");
        goto ERROR;
    }

    /* check ois status if it`s idle or not */
    /* 0x01 == IDLE State */
    do {
        msm_ois_i2c_byte_read(a_ctrl,0x01,&status);
        if (status == 0x01)
            break;
        if (--retries < 0) {
            pr_err("%s : read register fail!. status: 0x%x\n", __func__, status);
            goto ERROR;
        }
        usleep_range(10000, 11000);
    } while (status != 0x01);

    SendData[0] = 0x85;
    SendData[1] = 0xEB;
    SendData[2] = 0x51;
    SendData[3] = 0x3F;
    msm_ois_i2c_seq_write(a_ctrl, 0x0348, SendData, 4);

    rc = msm_ois_i2c_byte_write(a_ctrl, 0x0039, 0x01);   // OIS shift calibration enable
    if (rc < 0) {
        pr_err("ois shift calibration enable failed, i2c fail\n");
        goto ERROR;
    }

    a_ctrl->is_shift_enabled = true;
ERROR:
    if (servoVal == 0x01) {
        rc = msm_ois_i2c_byte_write(a_ctrl, 0x0000, 0x01);   // OIS servo on
        if (rc < 0) {
            pr_err("ois servo on failed, i2c fail\n");
        }
    }

    CDBG("Exit\n");
    return rc;
}

static int32_t msm_ois_set_debug_info(struct msm_ois_ctrl_t *a_ctrl, uint16_t mode)
{
    uint16_t    read_value;
    int         rc = 0;
    char        ois_debug_info[OIS_DEBUG_INFO_SIZE] ="";
    char        exif_tag[6]= "ssois"; //defined exif tag for ois
    int         current_mode;

    CDBG("Enter");

    if (!a_ctrl->is_set_debug_info){
        rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_read( // read Error register
            &a_ctrl->i2c_client, 0x04, &read_value, MSM_CAMERA_I2C_WORD_DATA);
        if (rc < 0)
            pr_err("get ois error register value failed, i2c fail");

        a_ctrl->debug_info.err_reg = read_value;

        if (msm_ois_i2c_byte_read(a_ctrl, 0x01, &read_value) < 0) //read Status register
            pr_err("get ois status register value failed, i2c fail");

        a_ctrl->debug_info.status_reg = read_value;

        a_ctrl->is_set_debug_info = TRUE;

        memcpy(a_ctrl->debug_info.cal_ver, &a_ctrl->cal_info.cal_ver, MSM_OIS_VER_SIZE*sizeof(char));
        memcpy(a_ctrl->debug_info.module_ver, &a_ctrl->module_ver, MSM_OIS_VER_SIZE*sizeof(char));
        memcpy(a_ctrl->debug_info.phone_ver, &a_ctrl->phone_ver, MSM_OIS_VER_SIZE*sizeof(char));
        a_ctrl->debug_info.cal_ver[MSM_OIS_VER_SIZE] = '\0';
        a_ctrl->debug_info.module_ver[MSM_OIS_VER_SIZE] = '\0';
        a_ctrl->debug_info.phone_ver[MSM_OIS_VER_SIZE] = '\0';
    }

    current_mode = mode;
    snprintf(ois_debug_info, OIS_DEBUG_INFO_SIZE, "%s%s %s %s %x %x %x\n", exif_tag,
        (a_ctrl->debug_info.module_ver[0] == '\0') ? ("ISNULL") : (a_ctrl->debug_info.module_ver),
        (a_ctrl->debug_info.phone_ver[0] == '\0') ? ("ISNULL") : (a_ctrl->debug_info.phone_ver),
        (a_ctrl->debug_info.cal_ver[0] == '\0') ? ("ISNULL") : (a_ctrl->debug_info.cal_ver),
        a_ctrl->debug_info.err_reg, a_ctrl->debug_info.status_reg, current_mode);

    CDBG("ois exif debug info %s", ois_debug_info);

    rc = msm_camera_write_sysfs(SYSFS_OIS_DEBUG_PATH, ois_debug_info, sizeof(ois_debug_info));

    if (rc < 0) {
      pr_err("msm_camera_write_sysfs failed");
      rc = -1;
    }

    CDBG("Exit");

    return rc;
}

static int32_t msm_ois_read_phone_ver(struct msm_ois_ctrl_t *a_ctrl)
{
    struct file *filp = NULL;
    mm_segment_t old_fs;
    char    char_ois_ver[MSM_OIS_VER_SIZE+1] = "";
    char    ois_bin_full_path[256] = "";
    int ret = 0, i;
    char    ois_core_ver;

    loff_t pos;

    old_fs = get_fs();
    set_fs(KERNEL_DS);

#if 1//defined(SAMSUNG_OIS_RUMBA_S4)
    ois_core_ver = a_ctrl->cal_info.cal_ver[0];
#else
    sprintf(ois_hwinfo, "%c%c%c", a_ctrl->module_ver.gyro_sensor, a_ctrl->module_ver.driver_ic, a_ctrl->module_ver.core_ver);
#endif

    switch (ois_core_ver) {
        case 'A':
        case 'C':
        case 'M':
        case 'O':
            sprintf(ois_bin_full_path, "%s/%s", OIS_FW_PATH, OIS_FW_DOM_NAME);
            break;
        case 'B':
        case 'D':
        case 'N':
        case 'P':
            sprintf(ois_bin_full_path, "%s/%s", OIS_FW_PATH, OIS_FW_SEC_NAME);
            break;
    }
    sprintf(a_ctrl->load_fw_name, ois_bin_full_path); // to use in fw_update

    pr_info("OIS FW : %s", ois_bin_full_path);

    filp = filp_open(ois_bin_full_path, O_RDONLY, 0);
    if(IS_ERR(filp)){
        pr_err("%s: No OIS FW with %c exists in the system. Load from OIS module.\n", __func__, ois_core_ver);
        set_fs(old_fs);
        return -1;
    }

#if 1//defined(SAMSUNG_OIS_RUMBA_S4)
    pos = OIS_HW_VERSION_OFFSET;
    ret = vfs_read(filp, char_ois_ver, sizeof(char) * OIS_HW_VERSION_SIZE, &pos);
    if (ret < 0) {
        pr_err("%s: Fail to read OIS FW.", __func__);
        ret = -1;
        goto ERROR;
    }

    pos = OIS_FW_VERSION_OFFSET;
    ret = vfs_read(filp, char_ois_ver + OIS_HW_VERSION_SIZE, sizeof(char) * (MSM_OIS_VER_SIZE - OIS_HW_VERSION_SIZE), &pos);
    if (ret < 0) {
        pr_err("%s: Fail to read OIS FW.", __func__);
        ret = -1;
        goto ERROR;
    }
#else
    pos = OIS_VERSION_OFFSET;
    ret = vfs_read(filp, char_ois_ver, sizeof(char_ois_ver), &pos);
    if (ret < 0) {
        pr_err("%s: Fail to read OIS FW.", __func__);
        ret = -1;
        goto ERROR;
    }
#endif

    for(i = 0; i < MSM_OIS_VER_SIZE; i ++)
    {
        if(!isalnum(char_ois_ver[i]))
        {
            pr_err("%s: %d version char (%c) is not alnum type.", __func__, __LINE__, char_ois_ver[i]);
            ret = -1;
            goto ERROR;
        }
    }

    memcpy(&a_ctrl->phone_ver, char_ois_ver, MSM_OIS_VER_SIZE * sizeof(char));
#if 1//defined(SAMSUNG_OIS_RUMBA_S4)
    pr_info("%c%c%c%c%c%c%c\n",
        a_ctrl->phone_ver.core_ver, a_ctrl->phone_ver.gyro_sensor, a_ctrl->phone_ver.driver_ic, a_ctrl->phone_ver.year,
        a_ctrl->phone_ver.month, a_ctrl->phone_ver.iteration_0, a_ctrl->phone_ver.iteration_1);
#else
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

#if 1//defined(SAMSUNG_OIS_RUMBA_S4)
    a_ctrl->i2c_client.i2c_func_tbl->i2c_read(&a_ctrl->i2c_client, 0xFB, &read_value, MSM_CAMERA_I2C_BYTE_DATA);
    a_ctrl->module_ver.core_ver = read_value & 0xFF;
    if(!isalnum(read_value&0xFF))
    {
        pr_err("%s: %d version char is not alnum type.", __func__, __LINE__);
        return -1;
    }
#endif

    a_ctrl->i2c_client.i2c_func_tbl->i2c_read(&a_ctrl->i2c_client, 0xF8, &read_value, MSM_CAMERA_I2C_WORD_DATA);
    a_ctrl->module_ver.gyro_sensor = read_value & 0xFF;
    a_ctrl->module_ver.driver_ic = (read_value >> 8) & 0xFF;
    if(!isalnum(read_value&0xFF) && !isalnum((read_value>>8)&0xFF))
    {
        pr_err("%s: %d version char is not alnum type.", __func__, __LINE__);
        return -1;
    }

    a_ctrl->i2c_client.i2c_func_tbl->i2c_read(&a_ctrl->i2c_client, 0x7C, &read_value, MSM_CAMERA_I2C_WORD_DATA);
#if 1//defined(SAMSUNG_OIS_RUMBA_S4)
    a_ctrl->module_ver.year = (read_value >> 8) & 0xFF;
#else
    a_ctrl->module_ver.core_ver = (read_value >> 8) & 0xFF;
#endif
    a_ctrl->module_ver.month = read_value & 0xFF;
    if(!isalnum(read_value&0xFF) && !isalnum((read_value>>8)&0xFF))
    {
        pr_err("%s: %d version char is not alnum type.", __func__, __LINE__);
        return -1;
    }

    a_ctrl->i2c_client.i2c_func_tbl->i2c_read(&a_ctrl->i2c_client, 0x7E, &read_value, MSM_CAMERA_I2C_WORD_DATA);
    a_ctrl->module_ver.iteration_0 = (read_value >> 8) & 0xFF;
    a_ctrl->module_ver.iteration_1 = read_value & 0xFF;
    if(!isalnum(read_value&0xFF) && !isalnum((read_value>>8)&0xFF))
    {
        pr_err("%s: %d version char is not alnum type.", __func__, __LINE__);
        return -1;
    }

#if 1//defined(SAMSUNG_OIS_RUMBA_S4)
    pr_err("%c%c%c%c%c%c%c\n", a_ctrl->module_ver.core_ver, a_ctrl->module_ver.gyro_sensor, a_ctrl->module_ver.driver_ic, a_ctrl->module_ver.year,
        a_ctrl->module_ver.month, a_ctrl->module_ver.iteration_0, a_ctrl->module_ver.iteration_1);
#else
    CDBG_I("%c%c%c%c%c%c\n", a_ctrl->module_ver.gyro_sensor, a_ctrl->module_ver.driver_ic, a_ctrl->module_ver.core_ver,
        a_ctrl->module_ver.month, a_ctrl->module_ver.iteration_0, a_ctrl->module_ver.iteration_1);
#endif

    return 0;
}

static int32_t msm_ois_set_mode(struct msm_ois_ctrl_t *a_ctrl,
                            uint16_t mode)
{
    CDBG_I("Enter\n");
    CDBG_I("[mode :: %d] \n", mode);

    if (!a_ctrl->is_shift_enabled) {
        CDBG_I("SET :: SHIFT_CALIBRATION\n");
        msm_ois_set_shift(a_ctrl);
    }

    switch(mode) {
        case OIS_MODE_ON_STILL:
            CDBG_I("SET :: OIS_MODE_ON_STILL\n");
            if(msm_ois_i2c_byte_write(a_ctrl, 0x02, 0x00) < 0)  /* OIS mode reg set - still*/
                return -1;
            if(msm_ois_i2c_byte_write(a_ctrl, 0x00, 0x01) < 0)  /* OIS ctrl reg set - ON*/
                return -1;
            break;
        case OIS_MODE_ON_ZOOM:
            CDBG_I("SET :: OIS_MODE_ON_ZOOM\n");
            if(msm_ois_i2c_byte_write(a_ctrl, 0x02, 0x13) < 0) /* OIS mode reg set - zoom*/
                return -1;
            if(msm_ois_i2c_byte_write(a_ctrl, 0x00, 0x01) < 0) /* OIS ctrl reg set - ON*/
                return -1;
            break;
        case OIS_MODE_ON_VIDEO:
            CDBG_I("SET :: OIS_MODE_ON_VIDEO\n");
            if(msm_ois_i2c_byte_write(a_ctrl, 0x02, 0x01) < 0) /* OIS mode reg set - video*/
                return -1;
            if(msm_ois_i2c_byte_write(a_ctrl, 0x00, 0x01) < 0) /* OIS ctrl reg set - ON*/
                return -1;
            break;

        case OIS_MODE_SINE_X:
            CDBG_I("SET :: OIS_MODE_SINE_X\n"); // for factory test
            msleep(100);
            if (!a_ctrl->is_shift_enabled) {
                CDBG_I("SET :: SHIFT_CALIBRATION\n");
                msm_ois_set_shift(a_ctrl);
            }
            msm_ois_i2c_byte_write(a_ctrl, 0x18, 0x01); /* OIS SIN_CTRL- X */
            msm_ois_i2c_byte_write(a_ctrl, 0x19, 0x01); /* OIS SIN_FREQ - 1 hz*/
            msm_ois_i2c_byte_write(a_ctrl, 0x1A, 0x2d); /* OIS SIN_AMP - 40 */
            msm_ois_i2c_byte_write(a_ctrl, 0x02, 0x03); /* OIS MODE - 3 ( sin )*/
            msm_ois_i2c_byte_write(a_ctrl, 0x00, 0x01); /* OIS ctrl reg set - ON*/
            break;

        case OIS_MODE_SINE_Y:
            CDBG_I("SET :: OIS_MODE_SINE_Y\n"); // for factory test
            msleep(100);
            if (!a_ctrl->is_shift_enabled) {
                CDBG_I("SET :: SHIFT_CALIBRATION\n");
                msm_ois_set_shift(a_ctrl);
            }
            msm_ois_i2c_byte_write(a_ctrl, 0x18, 0x02); /* OIS SIN_CTRL- Y */
            msm_ois_i2c_byte_write(a_ctrl, 0x19, 0x01); /* OIS SIN_FREQ - 1 hz*/
            msm_ois_i2c_byte_write(a_ctrl, 0x1A, 0x2d); /* OIS SIN_AMP - 40 */
            msm_ois_i2c_byte_write(a_ctrl, 0x02, 0x03); /* OIS MODE - 3 ( sin )*/
            msm_ois_i2c_byte_write(a_ctrl, 0x00, 0x01); /* OIS ctrl reg set - ON*/
            break;

        case OIS_MODE_CENTERING:
            CDBG_I("SET :: OIS_MODE_CENTERING (OFF) \n"); // ois compensation off
            if(msm_ois_i2c_byte_write(a_ctrl, 0x02, 0x05) < 0) /* OIS mode reg set - centering*/
                return -1;
            if(msm_ois_i2c_byte_write(a_ctrl, 0x00, 0x01) < 0) /* OIS ctrl reg set - ON*/
                return -1;
            break;
        default:
            break;
    }
    msm_ois_set_debug_info(a_ctrl, mode);
    a_ctrl->is_servo_on = TRUE;
    return 0;
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

    CDBG_FW("%s addr = 0x%x data: 0x%x\n", __func__, addr, *data);
    return rc;
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

    CDBG_FW("%s addr = 0x%x data: 0x%x\n", __func__, addr, data);
    return rc;
}

int msm_ois_i2c_seq_read(struct msm_ois_ctrl_t *a_ctrl, uint32_t addr, uint8_t *data, uint32_t size)
{
    int rc = 0;
    rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_read_seq(
            &a_ctrl->i2c_client, addr, data, size);

    if (rc < 0)
        pr_err("ois i2c seq read failed addr : 0x%x\n", addr);
    return rc;
}

int msm_ois_i2c_seq_write(struct msm_ois_ctrl_t *a_ctrl, uint32_t addr, uint8_t *data, uint32_t size)
{
    int rc = 0;
    rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write_seq(
            &a_ctrl->i2c_client, addr, data, size);

    if (rc < 0)
        pr_err("ois i2c byte write failed addr : 0x%x\n", addr);
    return rc;
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
    uint8_t read_data[72] = {0, }, shift_data[56] = {0, };
    uint8_t offset[2] = {0, };
    int rc = 0, i = 0;
    uint16_t read_status = 0;

    /* OIS Servo Off */
    if (msm_ois_i2c_byte_write(a_ctrl, 0x0000, 0) < 0)
        goto ERROR;

    for (i = MAX_RETRY_COUNT; i > 0; i--) {
        if (msm_ois_i2c_byte_read(a_ctrl, 0x0000, &read_status) < 0)
            goto ERROR;
        if (read_status == 1) /* Read Complete? */
            break;
        usleep_range(10000, 11000); // give some delay to wait
    }
    if (i < 0)
        goto ERROR;

    /* User Data Area & Address Setting - 1Page */
    offset[0] = 0x00;
    offset[1] = 0x00;
    rc = msm_ois_i2c_byte_write(a_ctrl, 0x000F, 0x12);  // DLFSSIZE_W Register(0x000F) : Size = 4byte * Value
    rc |= msm_ois_i2c_seq_write(a_ctrl, 0x0010, offset, 2); // Data Write Start Address Offset : 0x0000
    rc |= msm_ois_i2c_byte_write(a_ctrl, 0x000E, 0x04); // DFLSCMD Register(0x000E) = READ
    if (rc < 0)
        goto ERROR;

    for (i = MAX_RETRY_COUNT; i > 0; i--) {
        if (msm_ois_i2c_byte_read(a_ctrl, 0x000E, &read_status) < 0)
            goto ERROR;
        if (read_status == 0x14) /* Read Complete? */
            break;
        usleep_range(10000, 11000); // give some delay to wait
        pr_info("DFLSCMD Read command doesn't success(iter = %d)", (MAX_RETRY_COUNT - 1));
    }
    if (i < 0)
        goto ERROR;

    /* Cal-Version Read */
    /* Suppression Ratio Read */
    /* Manufacturing Info Read */
    /* OIS Hall Cal. Info Read */
    rc = msm_ois_i2c_seq_read(a_ctrl, 0x0100, read_data, 72);
    if (rc < 0)
        goto ERROR;

    /* copy Cal-Version */
    pr_info("userdata cal ver : %c %c %c %c %c %c %c\n",
            read_data[0], read_data[1], read_data[2], read_data[3],
            read_data[4], read_data[5], read_data[6]);
    memcpy(user_data, read_data, size * sizeof(uint8_t));


    /* User Data Area & Address Setting - 2Page */
    offset[0] = 0x00;
    offset[1] = 0x04;
    rc = msm_ois_i2c_byte_write(a_ctrl, 0x000F, 0x0E);  // DLFSSIZE_W Register(0x000F) : Size = 4byte * Value
    rc |= msm_ois_i2c_seq_write(a_ctrl, 0x0010, offset, 2); // Data Write Start Address Offset : 0x0400
    rc |= msm_ois_i2c_byte_write(a_ctrl, 0x000E, 0x04); // DFLSCMD Register(0x000E) = READ
    if (rc < 0)
        goto ERROR;

    for (i = MAX_RETRY_COUNT; i >= 0; i--) {
        if (msm_ois_i2c_byte_read(a_ctrl, 0x000E, &read_status) < 0)
            goto ERROR;
        if (read_status == 0x14) /* Read Complete? */
            break;
        usleep_range(10000, 11000); // give some delay to wait
        pr_info("DFLSCMD Read command doesn't success(iter = %d)", i+1);
    }
    if (i < 0)
        goto ERROR;

    /* OIS Shift Info Read */
    /* OIS Shift Calibration Read */
    rc = msm_ois_i2c_seq_read(a_ctrl, 0x0100, shift_data, 56);
    if (rc < 0)
        goto ERROR;

    msm_ois_create_shift_table(a_ctrl, shift_data);

ERROR:
    return rc;
}

int msm_ois_create_shift_table(struct msm_ois_ctrl_t *a_ctrl, uint8_t *shift_data){
    int i = 0, j = 0;
    int16_t dataX[9] = {0, }, dataY[9] = {0, };
    uint16_t tempX = 0, tempY = 0;

    if (!a_ctrl || !shift_data)
        goto ERROR;

    CDBG("Enter\n");
    for (i = 0; i < 9; i++) {
        // Shift X : 0x7808 ~ 0x7818 (2byte)
        tempX = (uint16_t)(shift_data[8 + (i * 2)] | (shift_data[8 + (i * 2) + 1] << 8));
        if (tempX > 32767)
            tempX -= 65536;
        dataX[i] = (int16_t)tempX;

        // Shift Y : 0x7820 ~ 0x7830 (2byte)
        tempY = (uint16_t)(shift_data[32 + (i * 2)] | (shift_data[32 + (i* 2) + 1] << 8));
        if (tempY > 32767)
            tempY -= 65536;
        dataY[i] = (int16_t)tempY;
    }

    for (i = 0; i< 9; i++) {
        pr_info("dataX[%d] = %5d / dataY[%d] = %5d\n", i, dataX[i], i, dataY[i]);
    }

    for (j = 0; j < 8; j++) {
        for (i = 0; i < 64; i++) {
            a_ctrl->shift_tbl.ois_shift_x[i + (j << 6)] = ((((int32_t)dataX[j + 1] - dataX[j])  * i) >> 6) + dataX[j];
            a_ctrl->shift_tbl.ois_shift_y[i + (j << 6)] = ((((int32_t)dataY[j + 1] - dataY[j])  * i) >> 6) + dataY[j];
        }
    }

    CDBG("Exit\n");
    return 0;

ERROR:
    pr_err("%s : create ois shift table fail\n", __FUNCTION__);
    return -1;
}

int msm_ois_shift_calibration(uint16_t af_position) {
    uint8_t data[4] = {0, };
    int rc = 0;

    if (!g_msm_ois_t)
        return -1;

    if (!g_msm_ois_t->is_camera_run)
        return 0;

    if (!g_msm_ois_t->is_servo_on)
        return 0;

    if (af_position >= NUM_AF_POSITION) {
        pr_err("%s : af position error %u\n", __FUNCTION__, af_position);
        return -1;
    }

    data[0] = (g_msm_ois_t->shift_tbl.ois_shift_x[af_position] & 0x00FF);
    data[1] = ((g_msm_ois_t->shift_tbl.ois_shift_x[af_position] >> 8) & 0x00FF);
    data[2] = (g_msm_ois_t->shift_tbl.ois_shift_y[af_position] & 0x00FF);
    data[3] = ((g_msm_ois_t->shift_tbl.ois_shift_y[af_position] >> 8) & 0x00FF);

    rc = msm_ois_i2c_seq_write(g_msm_ois_t,0x004C, data, 4);
    if (rc < 0)
        pr_err("%s : write ois shift calibration error\n", __FUNCTION__);

    return rc;
}

static int32_t msm_ois_read_cal_info(struct msm_ois_ctrl_t *a_ctrl)
{
    int         rc = 0;
    uint8_t     user_data[MSM_OIS_VER_SIZE+1] = {0, };
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

    if(msm_ois_read_user_data_section(a_ctrl, OIS_USER_DATA_START_ADDR, MSM_OIS_VER_SIZE, user_data) < 0) {
        pr_err(" failed to read user data \n");
        return -1;
    }

    memcpy(a_ctrl->cal_info.cal_ver, user_data, (MSM_OIS_VER_SIZE) * sizeof(uint8_t));
    a_ctrl->cal_info.cal_ver[MSM_OIS_VER_SIZE] = '\0';

    pr_info("cal version = 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x(%s)\n",
        a_ctrl->cal_info.cal_ver[0], a_ctrl->cal_info.cal_ver[1], a_ctrl->cal_info.cal_ver[2],
        a_ctrl->cal_info.cal_ver[3], a_ctrl->cal_info.cal_ver[4], a_ctrl->cal_info.cal_ver[5], a_ctrl->cal_info.cal_ver[6],
        a_ctrl->cal_info.cal_ver);

    a_ctrl->cal_info.is_different_crc = compare_crc;

    return 0;
}

static int32_t msm_ois_read_manual_cal_info(struct msm_ois_ctrl_t *a_ctrl)
{
    int         rc = 0;
    uint8_t user_data[MSM_OIS_VER_SIZE+1] = {0, };
    uint8_t version[20] = {0, };
    uint16_t val;

    version[0] = 0x21;
    version[1] = 0x43;
    version[2] = 0x65;
    version[3] = 0x87;
    version[4] = 0x23;
    version[5] = 0x01;
    version[6] = 0xEF;
    version[7] = 0xCD;
    version[8] = 0x00;
    version[9] = 0x74;
    version[10] = 0x00;
    version[11] = 0x00;
    version[12] = 0x04;
    version[13] = 0x00;
    version[14] = 0x00;
    version[15] = 0x00;
    version[16] = 0x01;
    version[17] = 0x00;
    version[18] = 0x00;
    version[19] = 0x00;

    rc = msm_ois_i2c_seq_write(a_ctrl, 0x0100, version, 0x16);
    if (rc < 0)
        pr_err("ois i2c read word failed addr : 0x%x", 0x0100);
    usleep_range(5000, 6000);

    rc |= msm_ois_i2c_byte_read(a_ctrl, 0x0118, &val); //Core version
    user_data[0] = (uint8_t)(val & 0x00FF);

    rc |= msm_ois_i2c_byte_read(a_ctrl, 0x0119, &val); //Gyro Sensor
    user_data[1] = (uint8_t)(val & 0x00FF);

    rc |= msm_ois_i2c_byte_read(a_ctrl, 0x011A, &val); //Driver IC
    user_data[2] = (uint8_t)(val & 0x00FF);
    if (rc < 0)
        pr_err("ois i2c read word failed addr : 0x%x", 0x0100);

    memcpy(a_ctrl->cal_info.cal_ver, user_data, (MSM_OIS_VER_SIZE) * sizeof(uint8_t));
    a_ctrl->cal_info.cal_ver[MSM_OIS_VER_SIZE] = '\0';

    pr_info("Core version = 0x%02x, Gyro sensor = 0x%02x, Driver IC = 0x%02x\n",
        a_ctrl->cal_info.cal_ver[0], a_ctrl->cal_info.cal_ver[1], a_ctrl->cal_info.cal_ver[2]);

    return 0;
}

static int32_t msm_ois_fw_update(struct msm_ois_ctrl_t *a_ctrl)
{
    int ret = 0;
    uint8_t SendData[OIS_FW_UPDATE_PACKET_SIZE] ="";
    uint16_t checkSum;
    int block;
    uint16_t val;
    struct file *ois_filp = NULL;
    unsigned char *buffer = NULL;
    char    bin_ver[MSM_OIS_VER_SIZE+1] = "";
    char    mod_ver[MSM_OIS_VER_SIZE+1] = "";
    uint32_t fw_size = 0;
    mm_segment_t old_fs;
    int retries = 5;

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    CDBG_FW(" ENTER \n");

    /* check ois status if it`s idle or not */
    /* OISSTS register(0x0001) 1Byte read */
    /* 0x01 == IDLE State */
    do {
        msm_ois_i2c_byte_read(a_ctrl,0x01,&val);
        if (--retries < 0) {
            pr_err("[OIS_FW_DBG] ois status is not idle \n");
            goto ERROR;
        }
        usleep_range(10000, 11000);
    } while (val != 0x01);

    /*file open */

    ois_filp = filp_open(a_ctrl->load_fw_name, O_RDONLY, 0);
    if (IS_ERR(ois_filp)) {
        pr_err("[OIS_FW_DBG] fail to open file %s \n", a_ctrl->load_fw_name);
        ret = -1;
        goto ERROR;
    }

    fw_size = ois_filp->f_path.dentry->d_inode->i_size;
    CDBG_FW("fw size %d Bytes\n", fw_size);
    buffer = vmalloc(fw_size);
    memset(buffer, 0x00, fw_size);
    ois_filp->f_pos = 0;

    ret = vfs_read(ois_filp, (char __user *)buffer, fw_size, &ois_filp->f_pos);
    if (ret != fw_size) {
        pr_err("[OIS_FW_DBG] failed to read file \n");
        ret = -1;
        goto ERROR;
    }

    if (!a_ctrl->is_force_update) {
        ret = msm_ois_check_extclk(a_ctrl);
        if (ret < 0) {
            pr_err("%s : check extclk is failed %d\n", __func__, __LINE__);
            return ret;
        }
    }

    /* update a program code */
    msm_ois_i2c_byte_write(a_ctrl,0x0C,0x75);
    msleep(55);

    /* verify checkSum */
    checkSum = msm_ois_calcchecksum(buffer, PROGCODE_SIZE);
    pr_info("[OIS_FW_DBG] ois cal checksum = %u\n", checkSum);

    /* Write UserProgram Data */
    for(block = 0; block < (PROGCODE_SIZE / OIS_FW_UPDATE_PACKET_SIZE); block++)
    {
        memcpy(SendData, buffer, OIS_FW_UPDATE_PACKET_SIZE);

        ret = msm_ois_i2c_seq_write(a_ctrl, 0x0100, SendData, OIS_FW_UPDATE_PACKET_SIZE);
        if(ret < 0)
            pr_err("[OIS_FW_DBG] i2c byte prog code write failed\n");

        buffer += OIS_FW_UPDATE_PACKET_SIZE;
        usleep_range(10000, 11000);
    }

    /* write checkSum */
    SendData[0] = (checkSum & 0x00FF);
    SendData[1] = (checkSum & 0xFF00) >> 8;
    SendData[2] = 0;
    SendData[3] = 0x80;
    msm_ois_i2c_seq_write(a_ctrl, 0x0008, SendData, 4); // FWUP_CHKSUM REG(0x0008)

    msleep(190); // RUMBA Self Reset

    a_ctrl->i2c_client.i2c_func_tbl->i2c_read( // Error Status read
    &a_ctrl->i2c_client, 0x0006, &val, MSM_CAMERA_I2C_WORD_DATA);

    if(val == 0x0000)
        CDBG_FW(" progCode update success \n ");
    else
        pr_err(" progCode update fail \n");

    /* s/w reset */
    if(msm_ois_i2c_byte_write(a_ctrl, 0x000D, 0x01) < 0)
        pr_err("[OIS_FW_DBG] s/w reset i2c write error : 0x000D\n");
    if(msm_ois_i2c_byte_write(a_ctrl, 0x000E, 0x06) < 0)
        pr_err("[OIS_FW_DBG] s/w reset i2c write error : 0x000E\n");

    msleep(50);

    /* Param init - Flash to Rumba */
    if(msm_ois_i2c_byte_write(a_ctrl, 0x0036, 0x03) < 0)
        pr_err("[OIS_FW_DBG] param init i2c write error : 0x0036\n");
    msleep(200);

    msm_ois_read_module_ver(a_ctrl);

    memcpy(bin_ver, &a_ctrl->phone_ver, MSM_OIS_VER_SIZE*sizeof(char));
    memcpy(mod_ver, &a_ctrl->module_ver, MSM_OIS_VER_SIZE*sizeof(char));
    bin_ver[MSM_OIS_VER_SIZE] = '\0';
    mod_ver[MSM_OIS_VER_SIZE] = '\0';

    pr_err("[OIS_FW_DBG] after update version : phone %s, module %s\n", bin_ver, mod_ver);
    if(strncmp(bin_ver, mod_ver, MSM_OIS_VER_SIZE) != 0)//after update phone bin ver == module ver
    {
        ret = -1;
        pr_err("[OIS_FW_DBG] module ver is not the same with phone ver , update failed\n");
        goto ERROR;
    }

    pr_err("[OIS_FW_DBG] ois fw update done\n");

ERROR:
    if (ois_filp) {
        filp_close(ois_filp, NULL);
        ois_filp = NULL;
    }
    if (buffer){
        vfree(buffer);
        buffer = NULL;
    }
    set_fs(old_fs);
    return ret;

}

static int32_t msm_ois_vreg_control(struct msm_ois_ctrl_t *a_ctrl,
                            int config)
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

#if 1//defined(SAMSUNG_OIS_RUMBA_S4)
static int32_t msm_ois_check_extclk(struct msm_ois_ctrl_t *a_ctrl)
{
    uint16_t val, pll_multi, pll_divide;
    int retries = 5;
    int ret = 0;
    uint8_t clk_arr[4];
    uint32_t cur_clk = 0, new_clk = 0;

    /* OISSTS register(0x0001) 1Byte read */
    /* 0x01 == IDLE State */
    do {
        msm_ois_i2c_byte_read(a_ctrl,0x01,&val);
        if (--retries < 0) {
            pr_err("%s : read register fail!\n", __func__);
            return -EINVAL;
        }
        usleep_range(10000, 11000);
    } while (val != 0x01);

    /* Check current EXTCLK in register(0x03F0-0x03F3) */
    ret = msm_ois_i2c_seq_read(a_ctrl, 0x03F0, clk_arr, 4);
    if (ret < 0) {
        pr_err("%s : i2c read fail\n", __func__);
    }

    cur_clk = (clk_arr[3] << 24) |(clk_arr[2] << 16) |
                (clk_arr[1] << 8) |clk_arr[0];

    pr_err("[OIS_FW_DBG] %s : cur_clk = %u\n", __func__, cur_clk);

    if (cur_clk != CAMERA_OIS_EXT_CLK) {
        new_clk = CAMERA_OIS_EXT_CLK;
        clk_arr[0] = CAMERA_OIS_EXT_CLK & 0xFF;
        clk_arr[1] = (CAMERA_OIS_EXT_CLK >> 8) & 0xFF;
        clk_arr[2] = (CAMERA_OIS_EXT_CLK >> 16) & 0xFF;
        clk_arr[3] = (CAMERA_OIS_EXT_CLK >> 24) & 0xFF;

        switch (new_clk) {
            case 0xB71B00:
                pll_multi = 0x08;
                pll_divide = 0x03;
                break;
            case 0x1036640:
                pll_multi = 0x09;
                pll_divide = 0x05;
                break;
            case 0x124F800:
                pll_multi = 0x05;
                pll_divide = 0x03;
                break;
            case 0x16E3600:
                pll_multi = 0x04;
                pll_divide = 0x03;
                break;
            case 0x18CBA80:
                pll_multi = 0x06;
                pll_divide = 0x05;
                break;
            default:
                pr_info("cur_clk: 0x%08x\n", cur_clk);
                ret = -EINVAL;
                goto error;
        }
        /* Set External Clock(0x03F0-0x03F3) Setting */
        ret = msm_ois_i2c_seq_write(a_ctrl, 0x03F0, clk_arr, 4);
        if (ret < 0) {
            pr_err("i2c write fail 0x03F0\n");
        }

        /* Set PLL Multiple(0x03F4) Setting */
        ret = msm_ois_i2c_byte_write(a_ctrl, 0x03F4, pll_multi);
        if (ret < 0) {
            pr_err("i2c write fail 0x03F4\n");
        }

        /* Set PLL Divide(0x03F5) Setting */
        ret = msm_ois_i2c_byte_write(a_ctrl, 0x03F5, pll_divide);
        if (ret < 0) {
            pr_err("i2c write fail 0x03F5\n");
        }

        /* External Clock & I2C setting write to OISDATASECTION(0x0003) */
        ret = msm_ois_i2c_byte_write(a_ctrl, 0x0003, 0x01);
        if (ret < 0) {
            pr_err("i2c write fail 0x0003\n");
        }

        /* Wait for Flash ROM Write */
        msleep(200);

        /* S/W Reset */
        /* DFLSCTRL register(0x000D) */
        ret = msm_ois_i2c_byte_write(a_ctrl ,0x000D, 0x01);
        if (ret < 0) {
            pr_err("i2c write fail 0x000D\n");
        }

        /* Set DFLSCMD register(0x000E) = 6(Reset) */
        ret = msm_ois_i2c_byte_write(a_ctrl ,0x000E, 0x06);
        if (ret < 0) {
            pr_err("i2c write fail 0x000E\n");
        }
        /* Wait for Restart */
        msleep(50);

        pr_info("Apply EXTCLK for ois %u\n", new_clk);
    }else {
        pr_info("Keep current EXTCLK %u\n", cur_clk);
    }

error:
    return  ret;
}

static int32_t msm_ois_enable_clk(struct msm_camera_power_ctrl_t *power_info, int enable) {
    int rc = 0, index = 0;
    struct msm_sensor_power_setting *power_setting = NULL;

    if (power_info == NULL) {
        pr_err("%s ois clk info is null", __func__);
        return -1;
    }

    for (index = 0; index < power_info->power_setting_size; index++) {
        power_setting = &power_info->power_setting[index];
        switch (power_setting->seq_type) {
        case SENSOR_CLK:
            if (power_setting->seq_val >= power_info->clk_info_size) {
                pr_err("%s clk index %d >= max %zu\n", __func__,
                        power_setting->seq_val,
                        power_info->clk_info_size);
            }
            if (power_setting->config_val)
                power_info->clk_info[power_setting->seq_val].
                    clk_rate = power_setting->config_val;
            rc = msm_cam_clk_enable(power_info->dev,
                                    &power_info->clk_info[0],
                                    (struct clk **)&power_setting->data[0],
                                    power_info->clk_info_size,
                                    enable);

            if (rc < 0) {
                pr_err("%s: clk enable failed\n", __func__);
                break;
            }
            break;
        default:
            break;
        }
        if (power_setting->delay > 20) {
            msleep(power_setting->delay);
        } else if (power_setting->delay) {
            usleep_range(power_setting->delay * 1000,
                        (power_setting->delay * 1000) + 1000);
        }
    }

    return rc;
}
#endif

static int32_t msm_ois_power_down(struct msm_ois_ctrl_t *a_ctrl)
{
    int32_t rc = 0;
    CDBG_I("Enter\n");
    if (a_ctrl->ois_state != OIS_POWER_DOWN) {
        gpio_set_value_cansleep(GPIO_CAM_RESET,GPIO_OUT_LOW); // for i2c comm
        usleep_range(10000, 11000);
#if 1//defined(SAMSUNG_OIS_RUMBA_S4)
        rc = msm_ois_enable_clk(g_ois_power_info, 0);
        if (rc < 0) {
            pr_err("%s : enable mclk is failed %d\n", __func__, __LINE__);
            return rc;
        }
        usleep_range(10000, 11000);
#endif
        rc = msm_ois_vreg_control(a_ctrl, 0);
        if (rc < 0) {
            pr_err("%s failed %d\n", __func__, __LINE__);
            return rc;
        }
        a_ctrl->ois_state = OIS_POWER_DOWN;
    }
    msleep(30);
    CDBG("Exit\n");
    return rc;
}

static int32_t msm_ois_config(struct msm_ois_ctrl_t *a_ctrl,
    void __user *argp)
{
    struct msm_ois_cfg_data *cdata =
        (struct msm_ois_cfg_data *)argp;
    int32_t rc = 0;
    int retries = 2;

    mutex_lock(a_ctrl->ois_mutex);
    CDBG_I("Enter\n");
    CDBG_I("%s type %d\n", __func__, cdata->cfgtype);
    switch (cdata->cfgtype) {
    case CFG_OIS_SET_MODE:
        CDBG("CFG_OIS_SET_MODE enter \n");
        CDBG("CFG_OIS_SET_MODE value :: %d\n", cdata->set_mode_value);
        do{
            rc = msm_ois_set_mode(a_ctrl, cdata->set_mode_value);
            if (rc < 0){
                if (--retries < 0)
                    break;
            }
        }while(rc);
        if (retries < 0)
            pr_err("set mode failed %d\n", rc);
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
        if(isalnum(a_ctrl->cal_info.cal_ver[0]))
        {
            rc = msm_ois_read_phone_ver(a_ctrl);
            if (rc < 0)
                pr_err("There is no OIS FW in the system. skip fw update from phone %d\n", rc);

            if (copy_to_user(cdata->version, &a_ctrl->phone_ver, sizeof(struct msm_ois_ver_t)))
                pr_err("copy to user failed \n");
        } else {
            pr_err("CFG_OIS_READ_PHONE_VER core_ver invalid \n");
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

    case CFG_OIS_READ_MANUAL_CAL_INFO:
        CDBG("CFG_OIS_READ_MANUAL_CAL_INFO enter \n");
        rc = msm_ois_read_manual_cal_info(a_ctrl);
        if (rc < 0)
            pr_err("ois read manual cal info failed %d\n", rc);

        if (copy_to_user(cdata->ois_cal_info, &a_ctrl->cal_info, sizeof(struct msm_ois_cal_info_t)))
            pr_err("copy to user failed\n");
        break;

    case CFG_OIS_FW_UPDATE:
        CDBG("CFG_OIS_FW_UPDATE enter \n");
        rc = msm_ois_fw_update(a_ctrl);
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
    CDBG_I("Enter\n");
    if (!subdev_id) {
        pr_err("failed\n");
        return -EINVAL;
    }
    if (a_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE)
        *subdev_id = a_ctrl->pdev->id;
    else
        *subdev_id = a_ctrl->subdev_id;

    CDBG("subdev_id %d\n", *subdev_id);
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
    a_ctrl->is_camera_run= TRUE;
    a_ctrl->is_set_debug_info = FALSE;
    a_ctrl->is_shift_enabled = FALSE;
    a_ctrl->is_servo_on = FALSE;

    CDBG("Exit\n");
    return rc;
}

static int msm_ois_close(struct v4l2_subdev *sd,
    struct v4l2_subdev_fh *fh) {
    int rc = 0;
    struct msm_ois_ctrl_t *a_ctrl =  v4l2_get_subdevdata(sd);
#if defined(CONFIG_SENSOR_RETENTION)
    int retries = 5;
    uint16_t val = 0;
#endif
    CDBG_I("Enter\n");
    if (!a_ctrl) {
        pr_err("failed\n");
        return -EINVAL;
    }

#if defined(CONFIG_SENSOR_RETENTION)
    if (sensor_retention_mode) {
	    CDBG_I("now sensor retention mode\n");
	    rc = msm_ois_i2c_byte_write(a_ctrl, 0x0000, 0x00); /* OIS ctrl reg set - SERVO OFF*/

	    /* check ois status if it`s idle or not */
	    /* OISSTS register(0x0001) 1Byte read */
	    /* 0x01 == IDLE State */
	    do {
	        msm_ois_i2c_byte_read(a_ctrl,0x0001,&val);
	        if (val == 0x01)
	            break;
	        if (--retries < 0) {
	            pr_err("[OIS_FW_DBG] ois status is not idle \n");
	            break;
	        }
	        usleep_range(10000, 11000);
	    } while (val != 0x01);

	    rc |= msm_ois_i2c_byte_write(a_ctrl, 0x0030, 0x03); /* Set low power mode */
	    if (rc < 0)
	        pr_err("%s: set ois low power mode failed\n", __func__);
	    else
	        CDBG_I("Entering ois low power mode(gyro sleep)\n");

	    usleep_range(2000, 3000);
    }
#endif

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
    a_ctrl->is_camera_run= FALSE;
    a_ctrl->is_shift_enabled = FALSE;
    a_ctrl->is_servo_on = FALSE;
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
    CDBG_I("Enter\n");

    switch (cmd) {
    case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
        return msm_ois_get_subdev_id(a_ctrl, argp);
    case VIDIOC_MSM_OIS_CFG:
        return msm_ois_config(a_ctrl, argp);
    case MSM_SD_SHUTDOWN:
        msm_ois_close(sd, NULL);
        return 0;
    default:
        return -ENOIOCTLCMD;
    }
}
#ifdef CONFIG_COMPAT

static long msm_ois_subdev_do_ioctl(
    struct file *file, unsigned int cmd, void *arg)
{
    long rc = 0;
    struct video_device *vdev = video_devdata(file);
    struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);
    struct msm_ois_cfg_data32 *u32 =
        (struct msm_ois_cfg_data32 *)arg;
    struct msm_ois_cfg_data ois_data;
    void *parg = arg;

    ois_data.cfgtype = u32->cfgtype;

    switch (cmd) {
    case VIDIOC_MSM_OIS_CFG32:
        cmd = VIDIOC_MSM_OIS_CFG;

        switch (u32->cfgtype) {
        case CFG_OIS_SET_MODE:
            ois_data.set_mode_value = u32->set_mode_value;
            parg = &ois_data;
            break;
        case CFG_OIS_READ_MODULE_VER:
        case CFG_OIS_READ_PHONE_VER:
            ois_data.version = compat_ptr(u32->version);
            parg = &ois_data;
            break;
        case CFG_OIS_READ_CAL_INFO:
        case CFG_OIS_READ_MANUAL_CAL_INFO:
            ois_data.ois_cal_info= compat_ptr(u32->ois_cal_info);
            parg = &ois_data;
            break;
        default:
            parg = &ois_data;
            break;
        }
    }
    rc = msm_ois_subdev_ioctl(sd, cmd, parg);
    return rc;
}

static long msm_ois_subdev_fops_ioctl(struct file *file, unsigned int cmd,
    unsigned long arg)
{
    return video_usercopy(file, cmd, arg, msm_ois_subdev_do_ioctl);
}
#endif

static int32_t msm_ois_power_up(struct msm_ois_ctrl_t *a_ctrl)
{
    int rc = 0;
    CDBG_I("Enter\n");

    if(a_ctrl->ois_state != OIS_POWER_UP)
    {
        CDBG_I("start power up\n");
        rc = msm_ois_vreg_control(a_ctrl, 1);
        if (rc < 0) {
            pr_err("%s failed %d\n", __func__, __LINE__);
            return rc;
        }

#if 1//defined(SAMSUNG_OIS_RUMBA_S4)
        rc = msm_ois_enable_clk(g_ois_power_info, 1);
        if (rc < 0) {
            pr_err("%s : enable mclk is failed %d\n", __func__, __LINE__);
            return rc;
        }
#endif

        gpio_set_value_cansleep(GPIO_CAM_RESET,GPIO_OUT_HIGH); //for i2c comm
        msleep(100); // for gyro stabilization in all factor test
        a_ctrl->ois_state = OIS_POWER_UP;
    }
    CDBG("Exit\n");
    return rc;
}

static int32_t msm_ois_power(struct v4l2_subdev *sd, int on)
{
    int rc = 0;
    struct msm_ois_ctrl_t *a_ctrl = v4l2_get_subdevdata(sd);
    CDBG_I("Enter\n");
    mutex_lock(a_ctrl->ois_mutex);
    if (on)
        rc = msm_ois_power_up(a_ctrl);
    else
        rc = msm_ois_power_down(a_ctrl);
    mutex_unlock(a_ctrl->ois_mutex);
    CDBG("Exit\n");
    return rc;
}

static struct v4l2_subdev_core_ops msm_ois_subdev_core_ops = {
    .ioctl = msm_ois_subdev_ioctl,
    .s_power = msm_ois_power,
};

static struct v4l2_subdev_ops msm_ois_subdev_ops = {
    .core = &msm_ois_subdev_core_ops,
};

static int32_t msm_ois_get_dt_gpio_req_tbl(struct device_node *of_node,
    struct msm_camera_gpio_conf *gconf, uint16_t *gpio_array,
    uint16_t gpio_array_size)
{
    int32_t rc = 0, i = 0;
    uint32_t count = 0;
    uint32_t *val_array = NULL;

    CDBG_I("Enter\n");
    if (!of_get_property(of_node, "qcom,gpio-req-tbl-num", &count))
        return 0;

    count /= sizeof(uint32_t);
    if (!count) {
        pr_err("%s qcom,gpio-req-tbl-num 0\n", __func__);
        return 0;
    }

    val_array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
    if (!val_array) {
        pr_err("%s failed %d\n", __func__, __LINE__);
        return -ENOMEM;
    }

    gconf->cam_gpio_req_tbl = kzalloc(sizeof(struct gpio) * count,
        GFP_KERNEL);
    if (!gconf->cam_gpio_req_tbl) {
        pr_err("%s failed %d\n", __func__, __LINE__);
        rc = -ENOMEM;
        goto ERROR1;
    }
    gconf->cam_gpio_req_tbl_size = count;

    rc = of_property_read_u32_array(of_node, "qcom,gpio-req-tbl-num",
        val_array, count);
    if (rc < 0) {
        pr_err("%s failed %d\n", __func__, __LINE__);
        goto ERROR2;
    }
    for (i = 0; i < count; i++) {
        if (val_array[i] >= gpio_array_size) {
            pr_err("%s gpio req tbl index %d invalid\n",
                __func__, val_array[i]);
            return -EINVAL;
        }
        gconf->cam_gpio_req_tbl[i].gpio = gpio_array[val_array[i]];
        CDBG("%s cam_gpio_req_tbl[%d].gpio = %d\n", __func__, i,
            gconf->cam_gpio_req_tbl[i].gpio);
    }

    rc = of_property_read_u32_array(of_node, "qcom,gpio-req-tbl-flags",
        val_array, count);
    if (rc < 0) {
        pr_err("%s failed %d\n", __func__, __LINE__);
        goto ERROR2;
    }
    for (i = 0; i < count; i++) {
        gconf->cam_gpio_req_tbl[i].flags = val_array[i];
        CDBG("%s cam_gpio_req_tbl[%d].flags = %ld\n", __func__, i,
            gconf->cam_gpio_req_tbl[i].flags);
    }

    for (i = 0; i < count; i++) {
        rc = of_property_read_string_index(of_node,
            "qcom,gpio-req-tbl-label", i,
            &gconf->cam_gpio_req_tbl[i].label);
        CDBG("%s cam_gpio_req_tbl[%d].label = %s\n", __func__, i,
            gconf->cam_gpio_req_tbl[i].label);
        if (rc < 0) {
            pr_err("%s failed %d\n", __func__, __LINE__);
            goto ERROR2;
        }
    }

    kfree(val_array);
    return rc;

ERROR2:
    kfree(gconf->cam_gpio_req_tbl);
ERROR1:
    kfree(val_array);
    gconf->cam_gpio_req_tbl_size = 0;
    return rc;
}

static int32_t msm_ois_get_gpio_data(struct msm_ois_ctrl_t *a_ctrl,
    struct device_node *of_node)
{
    int32_t rc = 0;
    uint32_t i = 0;
    uint16_t gpio_array_size = 0;
    uint16_t *gpio_array = NULL;

    CDBG_I("Enter\n");

    a_ctrl->gpio_conf = kzalloc(sizeof(struct msm_camera_gpio_conf),
        GFP_KERNEL);
    if (!a_ctrl->gpio_conf) {
        pr_err("%s failed %d\n", __func__, __LINE__);
        return -ENOMEM;
    }
    gpio_array_size = of_gpio_count(of_node);
    CDBG("%s gpio count %d\n", __func__, gpio_array_size);

    if (gpio_array_size) {
        gpio_array = kzalloc(sizeof(uint16_t) * gpio_array_size,
            GFP_KERNEL);
        if (!gpio_array) {
            pr_err("%s failed %d\n", __func__, __LINE__);
            rc = -ENOMEM;
            goto ERROR1;
        }
        for (i = 0; i < gpio_array_size; i++) {
            gpio_array[i] = of_get_gpio(of_node, i);
            CDBG("%s gpio_array[%d] = %d\n", __func__, i,
                gpio_array[i]);
        }

        rc = msm_ois_get_dt_gpio_req_tbl(of_node,
            a_ctrl->gpio_conf, gpio_array, gpio_array_size);
        if (rc < 0) {
            pr_err("%s failed %d\n", __func__, __LINE__);
            goto ERROR2;
        }
        kfree(gpio_array);
    }
    return 0;

ERROR2:
    kfree(gpio_array);
ERROR1:
    kfree(a_ctrl->gpio_conf);
    return rc;
}

static int32_t msm_ois_i2c_probe(struct i2c_client *client,
    const struct i2c_device_id *id)
{
    int rc = 0;
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
        if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
            pr_err("i2c_check_functionality failed\n");
            goto probe_failure;
        }

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
            __func__, check_use_gpios);
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

    ois_ctrl_t->ois_v4l2_subdev_ops = &msm_ois_subdev_ops;
    ois_ctrl_t->ois_mutex = &msm_ois_mutex;
    ois_ctrl_t->i2c_driver = &msm_ois_i2c_driver;

    //CDBG("client = %x\n", (unsigned int) client);
    ois_ctrl_t->i2c_client.client = client;
    ois_ctrl_t->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
    /* Set device type as I2C */
    ois_ctrl_t->ois_device_type = MSM_CAMERA_I2C_DEVICE;
    ois_ctrl_t->i2c_client.i2c_func_tbl = &msm_sensor_qup_func_tbl;
    ois_ctrl_t->ois_v4l2_subdev_ops = &msm_ois_subdev_ops;
    ois_ctrl_t->ois_mutex = &msm_ois_mutex;
    ois_ctrl_t->ois_state = OIS_POWER_DOWN;
    ois_ctrl_t->is_camera_run= FALSE;

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

    msm_ois_v4l2_subdev_fops = v4l2_subdev_fops;

#ifdef CONFIG_COMPAT
    msm_ois_v4l2_subdev_fops.compat_ioctl32 =
        msm_ois_subdev_fops_ioctl;
#endif
    ois_ctrl_t->msm_sd.sd.devnode->fops =
        &msm_ois_v4l2_subdev_fops;

    g_msm_ois_t = ois_ctrl_t;

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

    msm_ois_v4l2_subdev_fops = v4l2_subdev_fops;

#ifdef CONFIG_COMPAT
    msm_ois_v4l2_subdev_fops.compat_ioctl32 =
        msm_ois_subdev_fops_ioctl;
#endif
    msm_ois_t->msm_sd.sd.devnode->fops =
        &msm_ois_v4l2_subdev_fops;

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

// check ois version to see if it is available for selftest or not
void msm_is_ois_version(void)
{
    int ret = 0;
    uint16_t val_c = 0, val_d = 0;
    uint16_t version = 0;

    ret = g_msm_ois_t->i2c_client.i2c_func_tbl->i2c_read( //Read 0x00FC value
        &g_msm_ois_t->i2c_client, 0xFC, &val_c,
        MSM_CAMERA_I2C_BYTE_DATA);

    if (ret < 0) {
        pr_err("i2c read fail\n");
    }

    ret = g_msm_ois_t->i2c_client.i2c_func_tbl->i2c_read( //Read 0x00FD value
        &g_msm_ois_t->i2c_client, 0xFD, &val_d,
        MSM_CAMERA_I2C_BYTE_DATA);

    if (ret < 0) {
        pr_err("i2c read fail\n");
    }
    version = (val_d << 8) | val_c;

    pr_info("OIS version = 0x%04x , after 11AE version , fw supoort selftest\n", version);
    pr_info("msm_is_ois_version : End \n");
}

bool msm_is_ois_sine_wavecheck(int threshold, int *sinx, int *siny, int *result)
{
    uint16_t buf = 0, val = 0;
    int ret = 0, retries = 10;
    int sinx_count = 0, siny_count = 0;
    uint16_t u16_sinx_count = 0, u16_siny_count = 0;
    uint16_t u16_sinx = 0, u16_siny = 0;

    ret = msm_ois_i2c_byte_write(g_msm_ois_t, 0x0052, (uint16_t)threshold); /* error threshold level. */
    ret |= msm_ois_i2c_byte_write(g_msm_ois_t, 0x0053, 0x0); /* count value for error judgement level. */
    ret |= msm_ois_i2c_byte_write(g_msm_ois_t, 0x0054, 0x05); /* frequency level for measurement. */
    ret |= msm_ois_i2c_byte_write(g_msm_ois_t, 0x0055, 0x3A); /* amplitude level for measurement. */
    ret |= msm_ois_i2c_byte_write(g_msm_ois_t, 0x0057, 0x02); /* vyvle level for measurement. */
    ret |= msm_ois_i2c_byte_write(g_msm_ois_t, 0x0050, 0x01); /* start sine wave check operation */

    if (ret < 0) {
        pr_err("i2c write fail\n");
        return false;
    }

    retries = 10;
    do {
        ret = msm_ois_i2c_byte_read(g_msm_ois_t, 0x0050, &val);
        if (ret < 0) {
            pr_err("i2c read fail\n");
            break;
        }

        msleep(100);

        if (--retries < 0) {
            pr_err("sine wave operation fail.\n");
            break;
        }
    } while (val);

    ret = msm_ois_i2c_byte_read(g_msm_ois_t, 0x0051, &buf);
    if (ret < 0) {
        pr_err("i2c read fail\n");
    }

    *result = (int)buf;
    pr_info("MCERR(0x51)=%d\n", buf);

    ret = g_msm_ois_t->i2c_client.i2c_func_tbl->i2c_read(
                &g_msm_ois_t->i2c_client, 0x00E4, &u16_sinx_count, MSM_CAMERA_I2C_WORD_DATA);
    sinx_count = ((u16_sinx_count & 0xFF00) >> 8) | ((u16_sinx_count &  0x00FF) << 8);
    if (sinx_count > 0x7FFF) {
        sinx_count = -((sinx_count ^ 0xFFFF) + 1);
    }
    ret |= g_msm_ois_t->i2c_client.i2c_func_tbl->i2c_read(
                &g_msm_ois_t->i2c_client, 0x00E6, &u16_siny_count, MSM_CAMERA_I2C_WORD_DATA);
    siny_count = ((u16_siny_count & 0xFF00) >> 8) | ((u16_siny_count &  0x00FF) << 8);
    if (siny_count > 0x7FFF) {
        siny_count = -((siny_count ^ 0xFFFF) + 1);
    }
    ret |= g_msm_ois_t->i2c_client.i2c_func_tbl->i2c_read(
                &g_msm_ois_t->i2c_client, 0x00E8, &u16_sinx, MSM_CAMERA_I2C_WORD_DATA);
    *sinx = ((u16_sinx & 0xFF00) >> 8) | ((u16_sinx &  0x00FF) << 8);
    if (*sinx > 0x7FFF) {
        *sinx = -((*sinx ^ 0xFFFF) + 1);
    }
    ret |= g_msm_ois_t->i2c_client.i2c_func_tbl->i2c_read(
                &g_msm_ois_t->i2c_client, 0x00EA, &u16_siny, MSM_CAMERA_I2C_WORD_DATA);
    *siny = ((u16_siny & 0xFF00) >> 8) | ((u16_siny &  0x00FF) << 8);
    if (*siny > 0x7FFF) {
        *siny = -((*siny ^ 0xFFFF) + 1);
    }
    if (ret < 0) {
        pr_err("i2c read fail\n");
    }

    pr_info("threshold = %d, sinx = %d, siny = %d, sinx_count = %d, siny_count = %d\n",
    threshold, *sinx, *siny, sinx_count, siny_count);

    if (buf == 0x0) {
        return true;
    } else {
        return false;
    }
}

/* get offset from module for line test */

void msm_is_ois_offset_test(long *raw_data_x, long *raw_data_y, bool is_need_cal)
{
    int i = 0;
    uint16_t val;
    uint16_t x = 0, y = 0;
    int x_sum = 0, y_sum = 0, sum = 0;
    int retries = 0, avg_count = 20;

    if(is_need_cal) // with calibration , offset value will be renewed.
    {
        if(msm_ois_i2c_byte_write( g_msm_ois_t, 0x14, 0x01) < 0)
            pr_err("i2c write fail\n");

        retries = avg_count;
        do {
            msm_ois_i2c_byte_read(g_msm_ois_t, 0x14, &val);

            CDBG("[read_val_0x0014::0x%04x]\n", val);

            usleep_range(10000, 11000);

            if (--retries < 0) {
                pr_err("Read register failed!, data 0x0014 val = 0x%04x\n", val);
                break;
            }
        } while (val);
    }

    retries = avg_count;
    for (i = 0; i < retries; retries--) {

        msm_ois_i2c_byte_read(g_msm_ois_t, 0x0248, &val);
        x = val;
        msm_ois_i2c_byte_read(g_msm_ois_t, 0x0249, &val);
        x_sum = (val << 8) | x;

        if (x_sum > 0x7FFF) {
            x_sum = -((x_sum ^ 0xFFFF) + 1);
        }
        sum += x_sum;
    }
    sum = sum * 10 /avg_count;
    *raw_data_x = sum * 1000 /131 /10;

    sum = 0;

    retries = avg_count;
    for (i = 0; i < retries; retries--) {
        msm_ois_i2c_byte_read(g_msm_ois_t, 0x024A, &val);
        y = val;
        msm_ois_i2c_byte_read(g_msm_ois_t, 0x024B, &val);
        y_sum = (val << 8) | y;

        if (y_sum > 0x7FFF) {
            y_sum = -((y_sum ^ 0xFFFF) + 1);
        }
        sum += y_sum;
    }
    sum = sum * 10 /avg_count;
    *raw_data_y = sum *1000 /131 /10;

    pr_err("msm_is_ois_offset_test : end \n");

    msm_is_ois_version();
    return;
}

/* ois module itselt has selftest fuction for line test.  */
/* it excutes by setting register and return the result */

u8 msm_is_ois_self_test()
{
    int ret = 0;
    uint16_t val;
    int retries = 30;
    u16 reg_val = 0;
    u8 x = 0, y = 0;
    u16 x_gyro_log = 0, y_gyro_log = 0;

    pr_info("msm_is_ois_self_test : start \n");

    ret = g_msm_ois_t->i2c_client.i2c_func_tbl->i2c_write(  //Write
    &g_msm_ois_t->i2c_client, 0x14, 0x08,
    MSM_CAMERA_I2C_BYTE_DATA);

    if (ret < 0) {
        pr_err("i2c write fail\n");
    }

    do {
        g_msm_ois_t->i2c_client.i2c_func_tbl->i2c_read( //Read 0x0014 value
        &g_msm_ois_t->i2c_client, 0x14, &val,
        MSM_CAMERA_I2C_BYTE_DATA);

        CDBG("[read_val_0x0014::%x]\n", val);

        usleep_range(10000, 11000);
        if (--retries < 0) {
            pr_err("Read register failed!, data 0x0014 val = 0x%04x\n", val);
            break;
        }
    } while (val);

    g_msm_ois_t->i2c_client.i2c_func_tbl->i2c_read( //Read 0x0004 value
    &g_msm_ois_t->i2c_client, 0x04, &val,
    MSM_CAMERA_I2C_BYTE_DATA);

    msm_ois_i2c_byte_read(g_msm_ois_t, 0x00EC, &reg_val);
    x = reg_val;
    msm_ois_i2c_byte_read(g_msm_ois_t, 0x00ED, &reg_val);
    x_gyro_log = (reg_val << 8) | x;
    msm_ois_i2c_byte_read(g_msm_ois_t, 0x00EE, &reg_val);
    y = reg_val;
    msm_ois_i2c_byte_read(g_msm_ois_t, 0x00EF, &reg_val);
    y_gyro_log = (reg_val << 8) | y;
    pr_err("%s(GSTLOG0=%d, GSTLOG1=%d)\n", __FUNCTION__, x_gyro_log, y_gyro_log);

    pr_err("msm_is_ois_self_test : test done, reg: 0x04 data : val = 0x%04x\n", val);
    pr_err("msm_is_ois_self_test : end \n");
    return val;
}
/*Rumba Gyro sensor selftest sysfs*/
static ssize_t gyro_selftest_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int result_total = 0;
    bool result_offset = 0, result_selftest = 0;
    unsigned char selftest_ret = 0;
    long raw_data_x = 0, raw_data_y = 0;

    msm_is_ois_offset_test(&raw_data_x, &raw_data_y, 1);
    msleep(50);
    selftest_ret = msm_is_ois_self_test();

    if (selftest_ret == 0x0) {
        result_selftest = true;
    } else {
        result_selftest = false;
    }
    if (abs(raw_data_x) > 30000 || abs(raw_data_y) > 30000){
        result_offset = false;
    } else {
        result_offset = true;
    }

    if (result_offset && result_selftest) {
        result_total = 0;
    } else if (!result_offset && !result_selftest) {
        result_total = 3;
    } else if (!result_offset) {
        result_total = 1;
    } else if (!result_selftest) {
        result_total = 2;
    }

    pr_info("Result : 0 (success), 1 (offset fail), 2 (selftest fail) , 3 (both fail) \n");
    sprintf(buf, "Result : %d, result x = %ld.%03ld, result y = %ld.%03ld\n", result_total , raw_data_x /1000, abs(raw_data_x % 1000),
        raw_data_y /1000, abs(raw_data_y % 1000));
    pr_info("%s", buf);

    if (raw_data_x < 0 && raw_data_y < 0) {
        return sprintf(buf, "%d,-%ld.%03ld,-%ld.%03ld\n", result_total, abs(raw_data_x /1000), abs(raw_data_x % 1000),
            abs(raw_data_y /1000), abs(raw_data_y % 1000));
    } else if (raw_data_x < 0) {
        return sprintf(buf, "%d,-%ld.%03ld,%ld.%03ld\n", result_total, abs(raw_data_x /1000), abs(raw_data_x % 1000),
            raw_data_y /1000, raw_data_y % 1000);
    } else if (raw_data_y < 0) {
        return sprintf(buf, "%d,%ld.%03ld,-%ld.%03ld\n", result_total, raw_data_x /1000, raw_data_x % 1000,
            abs(raw_data_y /1000), abs(raw_data_y % 1000));
    } else {
        return sprintf(buf, "%d,%ld.%03ld,%ld.%03ld\n", result_total, raw_data_x /1000, raw_data_x % 1000,
            raw_data_y /1000, raw_data_y % 1000);
    }
}

/*Rumba gyro get offset rawdata (without cal, except that same with selftest) sysfs*/
static ssize_t gyro_rawdata_test_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    long raw_data_x = 0, raw_data_y = 0;

    msm_is_ois_offset_test(&raw_data_x, &raw_data_y, 0);

    pr_info(" raw data x = %ld.%03ld, raw data y = %ld.%03ld\n", raw_data_x /1000, raw_data_x % 1000,
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

/* Rumba hall calibration test */
/* get diff between y,x min and max after callibration */

static ssize_t ois_hall_cal_test_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    bool result = 0;
    int retries= 20, vaild_diff =1100 ,ret = 0;
    uint16_t val,tmp_val;
    uint16_t xhmax, xhmin, yhmax, yhmin, diff_x ,diff_y;
    unsigned char SendData[2];

    msm_actuator_move_for_ois_test();
// add satat, 141008
    msleep(30);

    if(msm_ois_i2c_byte_write(g_msm_ois_t, 0x02, 0x02) < 0)  /* OIS mode reg set - Fixed mode*/
        pr_err("i2c failed to set fixed mode ");
    if(msm_ois_i2c_byte_write(g_msm_ois_t, 0x00, 0x01) < 0)  /* OIS ctrl reg set - SERVO ON*/
        pr_err("i2c failed to set ctrl on");

    g_msm_ois_t->i2c_client.i2c_func_tbl->i2c_read(
        &g_msm_ois_t->i2c_client, 0x021A, &val, MSM_CAMERA_I2C_WORD_DATA);
    SendData[0] = (val & 0xFF00) >> 8;
    SendData[1] = (val & 0x00FF);

    ret = msm_ois_i2c_seq_write(g_msm_ois_t, 0x0022, SendData, 2);
    if(ret < 0)
        pr_err("i2c error in setting target to move");

    g_msm_ois_t->i2c_client.i2c_func_tbl->i2c_read(
        &g_msm_ois_t->i2c_client, 0x021C, &val, MSM_CAMERA_I2C_WORD_DATA);
    SendData[0] = (val & 0xFF00) >> 8;
    SendData[1] = (val & 0x00FF);

    ret = msm_ois_i2c_seq_write(g_msm_ois_t, 0x0024, SendData, 2);
    if(ret < 0)
        pr_err("i2c error in setting target to move");

    msleep(400);
    pr_info("OIS Postion = Center");
// add end, 141008


    msm_ois_i2c_byte_write( g_msm_ois_t, 0x00, 0x00);

    do {
        msm_ois_i2c_byte_read(g_msm_ois_t, 0x01, &val);
        pr_err("to check if ois has entered to idle mode [read_val_0x0001::0x%04x]\n", val);
        usleep_range(10000, 11000);
        if (--retries < 0) {
            pr_err("return to idle mode failed!, data 0x0014 val = 0x%04x\n", val);
            break;
        }
    } while (val != 1);

// add start, 140902
    msm_ois_i2c_byte_write(g_msm_ois_t, 0x230, 0x64);
    msm_ois_i2c_byte_write(g_msm_ois_t, 0x231, 0x00);
    msm_ois_i2c_byte_write(g_msm_ois_t, 0x232, 0x64);
    msm_ois_i2c_byte_write(g_msm_ois_t, 0x233, 0x00);
// add end, 140902

// add start debug code, 140831
    msm_ois_i2c_byte_read(g_msm_ois_t, 0x230, &val);
    pr_info("OIS [read_val_0x0230::0x%04x]\n", val);
    msm_ois_i2c_byte_read(g_msm_ois_t, 0x231, &val);
    pr_info("OIS [read_val_0x0231::0x%04x]\n", val);
    msm_ois_i2c_byte_read(g_msm_ois_t, 0x232, &val);
    pr_info("OIS [read_val_0x0232::0x%04x]\n", val);
    msm_ois_i2c_byte_read(g_msm_ois_t, 0x233, &val);
    pr_info("OIS [read_val_0x0233::0x%04x]\n", val);

// add end, 140831

    msm_ois_i2c_byte_write(g_msm_ois_t, 0x020E, 0x6E);
    msm_ois_i2c_byte_write(g_msm_ois_t, 0x020F, 0x6E);
    msm_ois_i2c_byte_write(g_msm_ois_t, 0x0210, 0x1E);
    msm_ois_i2c_byte_write(g_msm_ois_t, 0x0211, 0x1E);
    msm_ois_i2c_byte_write(g_msm_ois_t, 0x0013, 0x01);

    val =1;
    retries = 120; // 12 secs, 140831 1500->120

    while (val == 1)
    {
        msm_ois_i2c_byte_read(g_msm_ois_t, 0x0013, &val);
        msleep(100); // 140831 10ms->100ms
        if(--retries < 0){
            pr_err("callibration is not done or not [read_val_0x0013::0x%04x]\n", val);
            break;
        }
    }

    msm_ois_i2c_byte_read(g_msm_ois_t, 0x0212, &val);
    tmp_val = val;
    msm_ois_i2c_byte_read(g_msm_ois_t, 0x0213, &val);
    xhmax = (val << 8) | tmp_val;
    pr_info("xhmax (byte) : %d \n", xhmax);

    msm_ois_i2c_byte_read(g_msm_ois_t, 0x0214, &val);
    tmp_val = val;
    msm_ois_i2c_byte_read(g_msm_ois_t, 0x0215, &val);
    xhmin = (val << 8) | tmp_val ;
    pr_info("xhmin (byte) : %d \n", xhmin);

    msm_ois_i2c_byte_read(g_msm_ois_t, 0x0216, &val);
    tmp_val = val;
    msm_ois_i2c_byte_read(g_msm_ois_t, 0x0217, &val);
    yhmax = (val << 8) | tmp_val ;
    pr_info("yhmin (byte) : %d \n", yhmax);

    msm_ois_i2c_byte_read(g_msm_ois_t, 0x0218, &val);
    tmp_val = val;
    msm_ois_i2c_byte_read(g_msm_ois_t, 0x0219, &val);
    yhmin = (val << 8) | tmp_val ;
    pr_info("yhmin (byte) : %d \n", yhmin);

    diff_x = xhmax - xhmin;
    diff_y = yhmax - yhmin;

    if((diff_x) > vaild_diff && (diff_y)> vaild_diff)
        result = 0; // 0 (success) 1(fail)
    else
        result = 1;

// add satat, 141008

    g_msm_ois_t->i2c_client.i2c_func_tbl->i2c_read(
        &g_msm_ois_t->i2c_client, 0x021A, &val, MSM_CAMERA_I2C_WORD_DATA);
    SendData[0] = (val & 0xFF00) >> 8;
    SendData[1] = (val & 0x00FF);

    ret = msm_ois_i2c_seq_write(g_msm_ois_t, 0x0022, SendData, 2);
    if(ret < 0)
        pr_err("i2c error in setting target to move");

    g_msm_ois_t->i2c_client.i2c_func_tbl->i2c_read(
        &g_msm_ois_t->i2c_client, 0x021C, &val, MSM_CAMERA_I2C_WORD_DATA);
    SendData[0] = (val & 0xFF00) >> 8;
    SendData[1] = (val & 0x00FF);

    ret = msm_ois_i2c_seq_write(g_msm_ois_t, 0x0024, SendData, 2);
    if(ret < 0)
        pr_err("i2c error in setting target to move");

    if(msm_ois_i2c_byte_write(g_msm_ois_t, 0x02, 0x02) < 0)  /* OIS mode reg set - Fixed mode*/
        pr_err("i2c failed to set fixed mode ");
    if(msm_ois_i2c_byte_write(g_msm_ois_t, 0x00, 0x01) < 0)  /* OIS ctrl reg set -SERVO ON*/
        pr_err("i2c failed to set ctrl on ");

    msleep(400);
    if(msm_ois_i2c_byte_write(g_msm_ois_t, 0x00, 0x00) < 0)  /* OIS ctrl reg set - SERVO OFF*/
        pr_err("i2c failed to set ctrl off");

// add end, 141008

    pr_info("result : %d (success : 0), diff_x : %d , diff_y : %d\n" ,result, diff_x, diff_y );
    return sprintf(buf, "%d,%d,%d\n", result, diff_x, diff_y);
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
            pr_info("ois_power_store : power down \n");
            break;
        case '1' :
            msm_ois_power_up(g_msm_ois_t);
            pr_info("ois_power_store : power up \n");
            break;

        default:
            break;
        }
    }
    return size;
}

char ois_fw_full[40] = "NULL NULL\n";
static ssize_t ois_fw_full_show(struct device *dev,
                     struct device_attribute *attr, char *buf)
{
    CDBG("[FW_DBG] OIS_fw_ver : %s\n", ois_fw_full);
    return snprintf(buf, sizeof(ois_fw_full), "%s", ois_fw_full);
}

static ssize_t ois_fw_full_store(struct device *dev,
                      struct device_attribute *attr, const char *buf, size_t size)
{
    CDBG("[FW_DBG] buf : %s\n", buf);
    snprintf(ois_fw_full, sizeof(ois_fw_full), "%s", buf);

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

uint32_t ois_autotest_threshold = 0;
static ssize_t ois_autotest_show(struct device *dev,
                     struct device_attribute *attr, char *buf)
{
    bool ret = false;
    int sin_x = 0, sin_y = 0, result = 0;
    bool x_result = true, y_result = true;
    int cnt = 0;

    msm_actuator_move_for_ois_test();
    msleep(100);

    ret = msm_is_ois_sine_wavecheck(ois_autotest_threshold, &sin_x , &sin_y, &result);
    if (ret) {
        x_result = y_result = true;
    } else {
        if((result&0x03)== 0x01)
        {
            //X Axis Fail
            x_result = false;
        }
        else if((result&0x03)== 0x02)
        {
            //Y Axis Fail
            y_result = false;
        }
        else
        {
            //X Axis Fail
            x_result = false;
            //Y Axis Fail
            y_result = false;
        }
    }

    cnt = sprintf(buf, "%s, %d, %s, %d", (x_result ? "pass" : "fail"), (x_result ? 0 : sin_x), (y_result ? "pass" : "fail"), (y_result ? 0 : sin_y));
    pr_info("result : %s\n", buf);
    return cnt;
}

static ssize_t ois_autotest_store(struct device *dev,
                      struct device_attribute *attr, const char *buf, size_t size)
{
    uint32_t value = 0;

    if(buf==NULL || kstrtouint(buf, 10, &value))
        return -1;
    ois_autotest_threshold = value;
    return size;
}

static DEVICE_ATTR(selftest, S_IRUGO, gyro_selftest_show, NULL);
static DEVICE_ATTR(ois_rawdata, S_IRUGO, gyro_rawdata_test_show, NULL);
static DEVICE_ATTR(ois_diff, S_IRUGO, ois_hall_cal_test_show, NULL);
static DEVICE_ATTR(ois_power, S_IWUSR, NULL, ois_power_store);
static DEVICE_ATTR(oisfw, S_IRUGO|S_IWUSR|S_IWGRP, ois_fw_full_show, ois_fw_full_store);
static DEVICE_ATTR(ois_exif, S_IRUGO|S_IWUSR|S_IWGRP, ois_exif_show, ois_exif_store);
static DEVICE_ATTR(autotest, S_IRUGO|S_IWUSR|S_IWGRP, ois_autotest_show, ois_autotest_store);

static int __init msm_ois_init_module(void)
{
    int32_t rc = 0;
    struct device *cam_ois;

    CDBG_I("Enter\n");

    rc = platform_driver_probe(&msm_ois_platform_driver,
        msm_ois_platform_probe);
    if (rc < 0) {
        pr_err("%s:%d platform driver probe rc %d\n",
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

    if (rc >= 0 && !IS_ERR(camera_class)) { //for sysfs
        cam_ois = device_create(camera_class, NULL, 0, NULL, "ois");
        if (!cam_ois) {
            pr_err("Failed to create device(ois) in camera_class!\n");
            rc = -ENOENT;
        }

        if (device_create_file(cam_ois, &dev_attr_selftest) < 0) {
            pr_err("failed to create device file, %s\n",
            dev_attr_selftest.attr.name);
            rc = -ENOENT;
        }
        if (device_create_file(cam_ois, &dev_attr_ois_power) < 0) {
            pr_err("failed to create device file, %s\n",
            dev_attr_ois_power.attr.name);
            rc = -ENOENT;
        }
        if (device_create_file(cam_ois, &dev_attr_ois_rawdata) < 0) {
            pr_err("failed to create device file, %s\n",
            dev_attr_ois_rawdata.attr.name);
            rc = -ENOENT;
        }
        if (device_create_file(cam_ois, &dev_attr_ois_diff) < 0) {
            pr_err("failed to create device file, %s\n",
            dev_attr_ois_diff.attr.name);
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
        if (device_create_file(cam_ois, &dev_attr_autotest) < 0) {
            printk("Failed to create device file!(%s)!\n",
                dev_attr_autotest.attr.name);
            rc = -ENODEV;
        }
    } else {
    pr_err("Failed to create device(ois) because of no camera class!\n");
        rc = -EINVAL;
    }

    return rc;
}

module_init(msm_ois_init_module);
MODULE_DESCRIPTION("MSM OIS");
MODULE_LICENSE("GPL v2");
