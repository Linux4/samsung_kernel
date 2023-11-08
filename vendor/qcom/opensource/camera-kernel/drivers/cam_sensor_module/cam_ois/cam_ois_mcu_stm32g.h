/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#ifndef _CAM_OIS_MCU_STM32_H_
#define _CAM_OIS_MCU_STM32_H_

#include "cam_ois_dev.h"

#define MAX_MODULE_NUM      (3)
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
#define CUR_MODULE_NUM      (3)
#elif defined(CONFIG_SAMSUNG_REAR_DUAL) && defined(CONFIG_SEC_B5Q_PROJECT)
#define CUR_MODULE_NUM      (1)
#else
#define CUR_MODULE_NUM      (2)
#endif

#define INIT_X_TARGET		(800)
#define STEP_VALUE			(300)
#define STEP_COUNT			(10)
#define RUMBA_WRITE_UILD	(0x48)
#define RUMBA_READ_UILD 	(0x49)

#define AKM_W_X_WRITE_UCLD	(0x1C)
#define AKM_W_X_READ_UCLD 	(0x1D)
#define AKM_W_Y_WRITE_UCLD	(0x9C)
#define AKM_W_Y_READ_UCLD 	(0x9D)
#define AKM_T_X_WRITE_UCLD	(0xE8)
#define AKM_T_X_READ_UCLD 	(0xE9)
#define AKM_T_Y_WRITE_UCLD	(0x68)
#define AKM_T_Y_READ_UCLD 	(0x69)
#define HALL_CAL_COUNT		(8)

#define CAMERA_OIS_EXT_CLK_12MHZ 0xB71B00
#define CAMERA_OIS_EXT_CLK_17MHZ 0x1036640
#define CAMERA_OIS_EXT_CLK_19P2MHZ 0x124F800
#define CAMERA_OIS_EXT_CLK_24MHZ   0x16E3600
#define CAMERA_OIS_EXT_CLK_26MHZ   0x18CBA80

#define MAX_EFS_DATA_LENGTH     (30)

struct cam_ois_sinewave_t
{
    int sin_x;
    int sin_y;
};


int cam_ois_offset_test(struct cam_ois_ctrl_t *o_ctrl,
	long *raw_data_x, long *raw_data_y, long *raw_data_z, bool is_need_cal);
int cam_ois_parsing_raw_data(struct cam_ois_ctrl_t *o_ctrl,
	uint8_t *buf, uint32_t buf_size, long *raw_data_x, long *raw_data_y, long *raw_data_z);
int cam_ois_gyro_sensor_calibration(struct cam_ois_ctrl_t *o_ctrl,
	long *raw_data_x, long *raw_data_y,long *raw_data_z);
int cam_ois_gyro_sensor_noise_check(struct cam_ois_ctrl_t *o_ctrl,
	long *stdev_data_x, long *stdev_data_y);
uint32_t cam_ois_self_test(struct cam_ois_ctrl_t *o_ctrl);
bool cam_ois_sine_wavecheck(struct cam_ois_ctrl_t *o_ctrl, int threshold,
	char* buf, uint32_t module_mask);
int cam_ois_check_fw(struct cam_ois_ctrl_t *o_ctrl);
int cam_ois_wait_idle(struct cam_ois_ctrl_t *o_ctrl, int retries);
int cam_ois_init(struct cam_ois_ctrl_t *o_ctrl);
int cam_ois_i2c_write(struct cam_ois_ctrl_t *o_ctrl,
	uint32_t addr, uint32_t data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type);
int cam_ois_shift_calibration(struct cam_ois_ctrl_t *o_ctrl, uint16_t af_position, uint16_t subdev_id);
int32_t cam_ois_set_debug_info(struct cam_ois_ctrl_t *o_ctrl, uint16_t mode);
int cam_ois_set_servo_ctrl(struct cam_ois_ctrl_t *o_ctrl, uint32_t en);
int cam_ois_get_ois_mode(struct cam_ois_ctrl_t *o_ctrl, uint16_t *mode);
int cam_ois_set_ois_mode(struct cam_ois_ctrl_t *o_ctrl, uint16_t mode);
int cam_ois_set_shift(struct cam_ois_ctrl_t *o_ctrl);
int cam_ois_set_angle_for_compensation(struct cam_ois_ctrl_t *o_ctrl);
int cam_ois_set_ggfadeup(struct cam_ois_ctrl_t *o_ctrl, uint16_t value);
int cam_ois_set_ggfadedown(struct cam_ois_ctrl_t *o_ctrl, uint16_t value);
int cam_ois_fixed_aperture(struct cam_ois_ctrl_t *o_ctrl);
int cam_ois_write_xgg_ygg(struct cam_ois_ctrl_t *o_ctrl);
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
int cam_ois_write_dual_cal(struct cam_ois_ctrl_t *o_ctrl);
#endif
int cam_ois_write_gyro_orientation(struct cam_ois_ctrl_t *o_ctrl);
int cam_ois_mcu_init(struct cam_ois_ctrl_t *o_ctrl);
void cam_ois_reset(void *ctrl);
int cam_ois_read_hall_position(struct cam_ois_ctrl_t *o_ctrl,
	uint32_t* targetPosition, uint32_t* hallPosition);
#if defined(CONFIG_SAMSUNG_OIS_TAMODE_CONTROL)
int ps_notifier_cb(struct notifier_block *nb, unsigned long event, void *data);
int cam_ois_add_tamode_msg(struct cam_ois_ctrl_t *o_ctrl);
int cam_ois_set_ta_mode(struct cam_ois_ctrl_t *o_ctrl);
#endif
int cam_ois_check_tele_cross_talk(struct cam_ois_ctrl_t *o_ctrl, uint16_t *result);
int cam_ois_check_ois_valid_show(struct cam_ois_ctrl_t *o_ctrl, uint16_t *result);
uint32_t cam_ois_check_ext_clk(struct cam_ois_ctrl_t *o_ctrl);
int32_t cam_ois_set_ext_clk(struct cam_ois_ctrl_t *o_ctrl, uint32_t clk);

int cam_ois_read_hall_cal(struct cam_ois_ctrl_t *o_ctrl, uint16_t subdev_id, uint16_t *result);
#if defined(CONFIG_SAMSUNG_OIS_ADC_TEMPERATURE_SUPPORT)
int get_ois_adc_value(struct cam_ois_ctrl_t *o_ctrl, uint32_t *result);
#endif

#define OISCTRL				(0x0000) // OIS Control Register
#define OISSTS				(0x0001) // OIS Status Register
#define OISMODE				(0x0002) // OIS Mode Select Register
#define OISERR				(0x0004) // OIS Error Register
#define FWUPERR				(0x0006) // Actuator Driver's FW Update Error
#define FWUPINDEX			(0x0007) // FW Update Index
#define FWUPCHKSUM			(0x0008) // FW Checksum Data
#define FWSIZE				(0x000A) // FW Update Size
#define FWUPCTRL			(0x000C) // FW Update Control Register
#define DFLSCTRL			(0x000D) // DFSCTRL
#define DFLSCMD				(0x000E) // DFLSCMD
#define DFLSSIZE_W			(0x000F) // DFLSSIZE_W
#define DFLSADR				(0x0010) // DFLSADR
#define GCCTRL				(0x0014) // Gyro Calibration Control Register
#define XTARGET				(0x0022) // X axis Fixed Mode Target
#define YTARGET				(0x0024) // Y axis Fixed Mode Target
#define ByPassCtrl			(0x0028) // By Pass Mode Control
#define GN_MSRCTRL			(0x0029) // Gyro Noise Measure Contorl Register
#define TACTRL				(0x0035) // TA Mode Control
#define CACTRL				(0x0039) // OIS Center Shift Compensation Control Register
#define CAAFPOSM1			(0x003A) // AF Position for Module1
#define CAAFPOSM2			(0x003B) // AF Position for Module2
#define AFTARGET_M1			(0x003C) // Target Position for Wide AF
#define AFTARGET_M2			(0x003E) // Target Position for Tele AF
#define AFHALL_M1			(0x0040) // Hall Position for Wide AF
#define AFHALL_M2			(0x0042) // Hall Position for Tele AF
#define AFTARGET_M3			(0x0044) // Target Position for Tele2 AF
#define AFHALL_M3			(0x0046) // Hall Position for Tele2 AF
#define CAAFPOSM3			(0x0048) // AF Position for Module3
#define MCERR_W				(0x004C) // Module Test Error Register
#define MCSTH_M3			(0x004E) // Sinewave Check Error Decision Threshold Setting M3
#define MCCTRL				(0x0050) // Module Check Control Register
#define MCERR_B				(0x0051) // Module Test Error Register
#define MCSTH_M1			(0x0052) // Sinewave Check Error Decision Threshold Setting
#define MCSERRC				(0x0053) // Sinewave Check Error Decision Count Setting
#define MCSFREQ				(0x0054) // Sinewave Operation Frequency Register
#define MCSAMP				(0x0055) // Sinewave Operation Amplitude Setting Register
#define MCSSKIPNUM			(0x0056) // Sinewave Measurement Skip Frequency Setting Register
#define MCSNUM				(0x0057) // Sinewave Measurement Skip Frequency Setting Register
#define MCSTH_M2			(0x005B) // Sinewave Check Error Decision Threshold Setting M2
#if defined(CONFIG_SAMSUNG_OIS_ADC_TEMPERATURE_SUPPORT)
#define GETADC			(0x0070) // GETADC Information
#endif
#define VDRINFO				(0x007C) // Vendor Information
#define FWINFO_CTRL			(0x0080) // F/W Internal Information Update Register
#define X_GYRO_CALC_M1		(0x0086) // X Target M1
#define Y_GYRO_CALC_M1		(0x0088) // Y Target M1
#define HAX_OUT_M1			(0x008E) // X Hall M1
#define HAY_OUT_M1			(0x0090) // Y Hall M1
#define X_GYRO_CALC_M3		(0x009E) // X Target M3
#define Y_GYRO_CALC_M3		(0x00A0) // Y Target M3
#define HAX_OUT_M3			(0x00A6) // X Hall M3
#define HAY_OUT_M3			(0x00A8) // Y Hall M3
#define X_GYRO_CALC_M2		(0x00AC) // X Target M2
#define Y_GYRO_CALC_M2		(0x00AE) // Y Target M2
#define HAX_OUT_M2			(0x00B4) // X Hall M2
#define HAY_OUT_M2			(0x00B6) // Y Hall M2
#define OISSEL				(0x00BE) // OIS Driver Output Select Register
#define LGMCRES0_M1			(0x00C0) // LoopGain ModuleCheck M1 result1
#define LGMCRES1_M1			(0x00C2) // LoopGain ModuleCheck M1 result2
#define LGMCRES2_M1			(0x00C4) // LoopGain ModuleCheck M1 result3
#define LGMCRES3_M1			(0x00C6) // LoopGain ModuleCheck M1 result4
#define LGMCRES0_M3			(0x00D8) // LoopGain ModuleCheck M3 result0
#define LGMCRES1_M3			(0x00DA) // LoopGain ModuleCheck M3 result1
#define LGMCRES2_M3			(0x00DC) // LoopGain ModuleCheck M3 result2
#define LGMCRES3_M3			(0x00DE) // LoopGain ModuleCheck M3 result3
#define LGMCRES0_M2			(0x00E4) // LoopGain ModuleCheck M2 result0
#define LGMCRES1_M2			(0x00E6) // LoopGain ModuleCheck M2 result1
#define LGMCRES2_M2			(0x00E8) // LoopGain ModuleCheck M2 result2
#define LGMCRES3_M2			(0x00EA) // LoopGain ModuleCheck M2 result3
#define GSTLOG0				(0x00EC) // Gyro SelfTest X Result
#define GSTLOG1				(0x00EE) // Gyro SelfTest Y Result
#define GSTLOG2				(0x00F0) // Gyro SelfTest Z Result
#define HWVER				(0x00F8) // HW Version
#define FLS_DATA			(0x0100) // Code Flash Data Buffer
#define XCENTER_M1			(0x021A) // X Hall Center M1
#define YCENTER_M1			(0x021C) // Y Hall Center M1
#define GGFADEUP			(0x0238) // Gyro Gain Fade Up Time Setting
#define GGFADEDOWN			(0x023A) // Gyro Gain Fade Down Time Setting
#define GYRO_POLA_X_M1		(0x0240) // X Gyro Pola M1
#define GYRO_POLA_Y_M1		(0x0241) // Y Gyro Pola M1
#define GYRO_ORIENT			(0x0242) // Gyro Cal. running time
#define XGZERO				(0x0248) // X axis Gyro 0 Point Offset Setting Register
#define YGZERO				(0x024A) // Y axis Gyro 0 Point Offset Setting Register
#define ZGZERO				(0x024C) // Z axis Gyro 0 Point Offset Setting Register
#define XGN_STDEV			(0x024E) // X axis Gyro Noise Standard Deviation Result
#define YGN_STDEV			(0x0250) // Y axis Gyro Noise Standard Deviation Result
#define XGG_M1				(0x0254) // X axis Gyro Gain Coefficient Setting Module#1 Register
#define YGG_M1				(0x0258) // Y axis Gyro Gain Coefficient Setting Module#1 Register
#define COCTRL				(0x0440) // Dual Cal. Center Offset Enable
#define XCOFFSET_M1			(0x0442) // Dual Cal. Offset X M1
#define YCOFFSET_M1			(0x0444) // Dual Cal. Offset Y M1
#define XCOFFSET_M2			(0x0446) // Dual Cal. Offset X M2
#define YCOFFSET_M2			(0x0448) // Dual Cal. Offset Y M2
#define XCOFFSET_M3			(0x044A) // Dual Cal. Offset X M3
#define YCOFFSET_M3			(0x044C) // Dual Cal. Offset Y M3
#define XGG_M3				(0x0514) // X axis Gyro Gain Coefficient Setting Module#3 Register
#define YGG_M3				(0x0518) // Y axis Gyro Gain Coefficient Setting Module#3 Register
#define GYRO_POLA_X_M2		(0x0552) // X Gyro Pola M2
#define GYRO_POLA_Y_M2		(0x0553) // Y Gyro Pola M2
#define XGG_M2				(0x0554) // X axis Gyro Gain Coefficient Setting Module#2 Register
#define YGG_M2				(0x0558) // Y axis Gyro Gain Coefficient Setting Module#2 Register
#define GYRO_POLA_X_M3		(0x054E) // X Gyro Pola M3
#define GYRO_POLA_Y_M3		(0x054F) // Y Gyro Pola M3

/*
*Below code add for MCU sysboot cmd operation
*/
typedef struct
{
    uint32_t page;
    uint32_t count;
} sysboot_erase_param_type;

/* Target specific definitions
 */
#define BOOT_I2C_STARTUP_DELAY          (sysboot_i2c_startup_delay) /* msecs */
#define BOOT_I2C_TARGET_PID             (product_id)
#define BOOT_I2C_ADDR                   (sysboot_i2c_slave_address << 1) /* it used directly as parameter of I2C HAL API */

#define BOOT_I2C_HANDLE                 (hi2c1)
#define BOOT_I2C_LPHANDLE               (&(BOOT_I2C_HANDLE))

/* Protocol specific definitions
 *  NOTE: timeout interval unit: msec
 */

#define BOOT_I2C_INTER_PKT_FRONT_INTVL  (1)
#define BOOT_I2C_INTER_PKT_BACK_INTVL   (1)

#define BOOT_I2C_SYNC_RETRY_COUNT       (3)
#define BOOT_I2C_SYNC_RETRY_INTVL       (50)

#define BOOT_I2C_CMD_TMOUT              (30)
#define BOOT_I2C_WRITE_TMOUT            (flash_prog_time)
#define BOOT_I2C_FULL_ERASE_TMOUT       (flash_full_erase_time)
#define BOOT_I2C_PAGE_ERASE_TMOUT(n)    (flash_page_erase_time * n)
#define BOOT_I2C_WAIT_RESP_TMOUT        (30)
#define BOOT_I2C_WAIT_MAX_RESP_TMOUT    (1500)
#define BOOT_I2C_WAIT_RESP_POLL_TMOUT   (500)
#define BOOT_I2C_WAIT_RESP_POLL_INTVL   (3)
#define BOOT_I2C_WAIT_RESP_POLL_RETRY   (BOOT_I2C_WAIT_RESP_POLL_TMOUT / BOOT_I2C_WAIT_RESP_POLL_INTVL)
#define BOOT_I2C_XMIT_TMOUT(count)      (5 + (1 * count))
#define BOOT_I2C_RECV_TMOUT(count)      BOOT_I2C_XMIT_TMOUT(count)

/* Payload length info. */

#define BOOT_I2C_CMD_LEN                (1)
#define BOOT_I2C_ADDRESS_LEN            (4)
#define BOOT_I2C_NUM_READ_LEN           (1)
#define BOOT_I2C_NUM_WRITE_LEN          (1)
#define BOOT_I2C_NUM_ERASE_LEN          (2)
#define BOOT_I2C_CHECKSUM_LEN           (1)

#define BOOT_I2C_MAX_WRITE_LEN          (256)  /* Protocol limitation */
#define BOOT_I2C_MAX_ERASE_PARAM_LEN    (4096) /* In case of erase parameter with 2048 pages */
#define BOOT_I2C_MAX_PAYLOAD_LEN        (BOOT_I2C_MAX_ERASE_PARAM_LEN) /* Larger one between write and erase., */

#define BOOT_I2C_REQ_CMD_LEN            (BOOT_I2C_CMD_LEN + BOOT_I2C_CHECKSUM_LEN)
#define BOOT_I2C_REQ_ADDRESS_LEN        (BOOT_I2C_ADDRESS_LEN + BOOT_I2C_CHECKSUM_LEN)
#define BOOT_I2C_READ_PARAM_LEN         (BOOT_I2C_NUM_READ_LEN + BOOT_I2C_CHECKSUM_LEN)
#define BOOT_I2C_WRITE_PARAM_LEN(len)   (BOOT_I2C_NUM_WRITE_LEN + len + BOOT_I2C_CHECKSUM_LEN)
#define BOOT_I2C_ERASE_PARAM_LEN(len)   (len + BOOT_I2C_CHECKSUM_LEN)

#define BOOT_I2C_RESP_GET_VER_LEN       (0x01) /* bootloader version(1) */
#define BOOT_I2C_RESP_GET_ID_LEN        (0x03) /* number of bytes - 1(1) + product ID(2) */

/* Commands and Response */

#define BOOT_I2C_CMD_GET                (0x00)
#define BOOT_I2C_CMD_GET_VER            (0x01)
#define BOOT_I2C_CMD_GET_ID             (0x02)
#define BOOT_I2C_CMD_READ               (0x11)
#define BOOT_I2C_CMD_GO                 (0x21)
#define BOOT_I2C_CMD_WRITE              (0x31)
#define BOOT_I2C_CMD_ERASE              (0x44)
#define BOOT_I2C_CMD_WRITE_UNPROTECT    (0x73)
#define BOOT_I2C_CMD_READ_UNPROTECT     (0x92)
#define BOOT_I2C_CMD_SYNC               (0xFF)

#define BOOT_I2C_RESP_ACK               (0x79)
#define BOOT_I2C_RESP_NACK              (0x1F)
#define BOOT_I2C_RESP_BUSY              (0x76)

/* Exported functions ------------------------------------------------------- */
int sysboot_i2c_sync(struct cam_ois_ctrl_t *o_ctrl, uint8_t *cmd);
int sysboot_i2c_info(struct cam_ois_ctrl_t *o_ctrl);
int sysboot_i2c_read(struct cam_ois_ctrl_t *o_ctrl, uint32_t address, uint8_t *dst, size_t len);
int sysboot_i2c_write(struct cam_ois_ctrl_t *o_ctrl, uint32_t address, uint8_t *src, size_t len);
int sysboot_i2c_erase(struct cam_ois_ctrl_t *o_ctrl, uint32_t address, size_t len);
int sysboot_i2c_go(struct cam_ois_ctrl_t *o_ctrl, uint32_t address);
int sysboot_i2c_write_unprotect(struct cam_ois_ctrl_t *o_ctrl);
int sysboot_i2c_read_unprotect(struct cam_ois_ctrl_t *o_ctrl);

/* Private definitaions ----------------------------------------------------- */
#define BOOT_NRST_PULSE_INTVL           (2) /* msec */

/* Utility MACROs */

#ifndef NTOHL
#define NTOHL(x)                        ((((x) & 0xFF000000U) >> 24) | \
                                         (((x) & 0x00FF0000U) >>  8) | \
                                         (((x) & 0x0000FF00U) <<  8) | \
                                         (((x) & 0x000000FFU) << 24))
#endif
#ifndef HTONL
#define HTONL(x)                        NTOHL(x)
#endif

#ifndef NTOHS
#define NTOHS(x)                        (((x >> 8) & 0x00FF) | ((x << 8) & 0xFF00))
#endif
#ifndef HTONS
#define HTONS(x)                        NTOHS(x)
#endif

/* ERROR definitions -------------------------------------------------------- */

enum
{
	/* BASE ERROR ------------------------------------------------------------- */
	BOOT_ERR_BASE                         = -999, /* -9xx */
	BOOT_ERR_INVALID_PROTOCOL_GET_INFO,
	BOOT_ERR_INVALID_PROTOCOL_SYNC,
	BOOT_ERR_INVALID_PROTOCOL_READ,
	BOOT_ERR_INVALID_PROTOCOL_WRITE,
	BOOT_ERR_INVALID_PROTOCOL_ERASE,
	BOOT_ERR_INVALID_PROTOCOL_GO,
	BOOT_ERR_INVALID_PROTOCOL_WRITE_UNPROTECT,
	BOOT_ERR_INVALID_PROTOCOL_READ_UNPROTECT,
	BOOT_ERR_INVALID_MAX_WRITE_BYTES,

	/* I2C ERROR -------------------------------------------------------------- */
	BOOT_ERR_I2C_BASE                     = -899, /* -8xx */
	BOOT_ERR_I2C_RESP_NACK,
	BOOT_ERR_I2C_RESP_UNKNOWN,
	BOOT_ERR_I2C_RESP_API_FAIL,
	BOOT_ERR_I2C_XMIT_API_FAIL,
	BOOT_ERR_I2C_RECV_API_FAIL,

	/* SPI ERROR -------------------------------------------------------------- */
	BOOT_ERR_SPI_BASE                     = -799, /* -7xx */

	/* UART ERROR ------------------------------------------------------------- */
	BOOT_ERR_UART_BASE                    = -699, /* -6xx */

	/* DEVICE ERROR ----------------------------------------------------------- */
	BOOT_ERR_DEVICE_MEMORY_MAP            = -599, /* -5xx */
	BOOT_ERR_DEVICE_PAGE_SIZE_NOT_FOUND,

	/* API ERROR (OFFSET) ----------------------------------------------------- */
	BOOT_ERR_API_GET                      = -1000,
	BOOT_ERR_API_GET_ID                   = -2000,
	BOOT_ERR_API_GET_VER                  = -3000,
	BOOT_ERR_API_SYNC                     = -4000,
	BOOT_ERR_API_READ                     = -5000,
	BOOT_ERR_API_WRITE                    = -6000,
	BOOT_ERR_API_ERASE                    = -7000,
	BOOT_ERR_API_GO                       = -8000,
	BOOT_ERR_API_WRITE_UNPROTECT          = -9000,
	BOOT_ERR_API_READ_UNPROTECT           = -10000,
	BOOT_ERR_API_SAVE_CONTENTS            = -11000,
	BOOT_ERR_API_RESTORE_CONTENTS         = -12000,
};
#endif/* _CAM_OIS_MCU_STM32_H_ */
