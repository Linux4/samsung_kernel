// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

/* OIS Workqueue */
// #include <linux/hrtimer.h>
// #include <linux/init.h>
// #include <linux/ktime.h>
/* ------------------------- */

#include "hf_manager.h"

#define DRIVER_NAME "dw9781c"

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define DW9781C_NAME				"dw9781c"

// #define FOR_DEBUG
// #define FW_UPDATE

#ifdef FW_UPDATE
#include "VIVO_FW_v0B.F0_230601_set_FW_Word_Format.h"
#endif

#define DW9781C_CTRL_DELAY_US			5000

#define DW9781OIS_I2C_SLAVE_ADDR 0x14
#define DW9781C_REG_CHIP_CTRL              0xD000
#define DW9781C_REG_OIS_DSP_CTRL           0xD001
#define DW9781C_REG_OIS_LOGIC_RESET        0xD002
#define DW9781C_REG_OIS_USER_WRITE_PROTECT 0xEBF1
#define DW9781C_REG_OIS_PANTILT_DERGEEX    0x725C   // Pan-tilt Limit value X
#define DW9781C_REG_OIS_PANTILT_DERGEEY    0x725D   // Pan-tilt Limit value Y
#define DW9781C_REG_IMU_SELECT             0x7012   // Select the gyro filter, depending on the gyro sensor model
#define DW9781C_REG_GYRO_INIT              0x7018
#define DW9781C_REG_OIS_CTRL               0x7015
#define DW9781C_REG_OIS_MODE               0x7014
#define DW9781C_REG_OIS_CMD_STATUS         0x73E5
#define DW9781C_REG_OIS_GYROX              0x70F0   // X_GYRO
#define DW9781C_REG_OIS_GYROY              0x70F1   // Y_GYRO
#define DW9781C_REG_OIS_TARGETX            0x7274   // X_TARGET
#define DW9781C_REG_OIS_TARGETY            0x7275   // Y_TARGET
#define DW9781C_REG_OIS_LENS_POSX          0x7049   // X_LENS_POS
#define DW9781C_REG_OIS_LENS_POSY          0x704A   // Y_LENS_POS
#define DW9781C_REG_OIS_CL_TARGETX         0x7025
#define DW9781C_REG_OIS_CL_TARGETY         0x7026
#define OIS_ON                   0x0000   // OIS ON/SERVO ON
#define SERVO_ON                 0x0001   // OIS OFF/SERVO ON
#define SERVO_OFF                0x0002   // OIS OFF/SERVO OFF
#define ON                       0x0001
#define OFF                      0x0000
#define USER_WRITE_EN            0x56FA
#define REG_DEAFULT_ON           0x8000

#define REGULATOR_MAXSIZE			16
#define PINCTRL_MAXSIZE				16
#define CLK_MAXSIZE				16

static const char * const ldo_names[] = {
	"vdd",
	"rst",
};

/* power  on stage : idx = 0, 2, 4, ... */
/* power off stage : idx = 1, 3, 5, ... */
static const char * const pio_names[] = {
	//"mclk_4mA",
	//"mclk_off",
	//"oisen_off",
	//"oisen_on",
	// "vdd_on",
	// "vdd_off",
	// "rst_low",
	// "rst_high",
};

/* set TG   : idx = 0, 2, 4, ... */
/* set Freq : idx = 1, 3, 5, ... */
static const char * const clk_names[] = {
	// "mclk",
	// "24",
};

/* dw9781c device structure */
struct dw9781c_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *ldo[REGULATOR_MAXSIZE];
	struct pinctrl *pio;
	struct pinctrl_state *pio_state[PINCTRL_MAXSIZE];
	struct clk *clk[CLK_MAXSIZE];
	struct hf_device hf_dev;
};

#define OIS_DATA_NUMBER 32
struct OisInfo {
	int32_t is_ois_supported;
	int32_t data_mode;  /* ON/OFF */
	int32_t samples;
	int32_t x_shifts[OIS_DATA_NUMBER];
	int32_t y_shifts[OIS_DATA_NUMBER];
	int64_t timestamps[OIS_DATA_NUMBER];
};

struct mtk_ois_pos_info {
	struct OisInfo *p_ois_info;
};

enum DW9781C_IMU {
	ICM20690 = 0x0000,
	LSM6DSM  = 0x0002,
	LSM6DSOQ = 0x0004,
	ICM42631 = 0x0005,
	BMI260   = 0x0006,
	ICM42602 = 0x0007,
	ICM42631_AUX2 = 0x0008,
};
enum DW9781C_mode {
	DW9781C_STILL_MODE       = 0x8000,
	// OIS locks to center position
	DW9781C_CENTERING_MODE   = 0x8003,
};

static struct i2c_client *m_client;
static int32_t fixmode00;

/* OIS Workqueue */
static struct workqueue_struct *ois_init_wq;
static struct work_struct ois_init_work;
static int32_t ois_init_done;
static struct dw9781c_device *g_dw9781c;

/* Control commnad */
#define VIDIOC_MTK_S_OIS_MODE _IOW('V', BASE_VIDIOC_PRIVATE + 2, int32_t)

#define VIDIOC_MTK_G_OIS_POS_INFO _IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct mtk_ois_pos_info)

#define VIDIOC_MTK_S_OIS_INIT _IOWR('V', BASE_VIDIOC_PRIVATE + 7, int32_t)
#define VIDIOC_MTK_S_OIS_UNINIT _IOWR('V', BASE_VIDIOC_PRIVATE + 8, int32_t)

static inline struct dw9781c_device *to_dw9781c_ois(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct dw9781c_device, ctrls);
}

static inline struct dw9781c_device *sd_to_dw9781c_ois(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct dw9781c_device, sd);
}

static int ois_i2c_rd_u16(struct i2c_client *i2c_client, u16 reg, u16 *val)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msg[2];
	u16 addr = i2c_client->addr;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	msg[0].addr = addr;
	msg[0].flags = i2c_client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);
	msg[1].addr  = addr;
	msg[1].flags = i2c_client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 2;
	ret = i2c_transfer(i2c_client->adapter, msg, 2);
	if (ret < 0) {
		LOG_INF("i2c transfer failed (%d)\n", ret);
		return ret;
	}
	*val = ((u16)buf[0] << 8) | buf[1];

	return 0;
}

static int ois_i2c_rd_s16(struct i2c_client *i2c_client, u16 reg, s16 *val)
{
	int ret = 0;
	u16 uval;

	ret = ois_i2c_rd_u16(m_client, reg, &uval);
	if (ret < 0) {
		LOG_INF("i2c transfer failed (%d)\n", ret);
		return ret;
	}

	*val = (s16)uval;

	return ret;
}

static int ois_i2c_wr_u16(struct i2c_client *i2c_client, u16 reg, u16 val)
{
	int ret;
	u8 buf[4];
	struct i2c_msg msg;
	u16 addr = i2c_client->addr;

	LOG_INF("OIS write 0x%x: 0x%x (%d)\n", reg, val, val);

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val >> 8;
	buf[3] = val & 0xff;
	msg.addr = addr;
	msg.flags = i2c_client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);
	ret = i2c_transfer(i2c_client->adapter, &msg, 1);
	if (ret < 0) {
		LOG_INF("i2c transfer failed (%d)\n", ret);
		return ret;
	}
	return 0;
}

static void I2C_OPERATION_CHECK(int val)
{
	if (val) {
		LOG_INF("%s I2C failure: %d",
			__func__, val);
	}
}

static void readhall(s16 *gyro_x, s16 *gyro_y, s16 *target_x, s16 *target_y, s16 *len_x, s16 *len_y)
{
	int ret = 0;
	u16 nRet = 0;
	unsigned int nTargetAddressX = 0, nTargetAddressY = 0;

	// get x gyro
	ret = ois_i2c_rd_s16(m_client, DW9781C_REG_OIS_GYROX, gyro_x);
	if (ret < 0) {
		LOG_INF("Get x GYRO fail\n");
		return;
	}

	// get y gyro
	ret = ois_i2c_rd_s16(m_client, DW9781C_REG_OIS_GYROY, gyro_y);
	if (ret < 0) {
		LOG_INF("Get y GYRO fail\n");
		return;
	}

	// get OIS_CTRL register value
	ret = ois_i2c_rd_u16(m_client, DW9781C_REG_OIS_CTRL, &nRet);
	if (ret < 0) {
		LOG_INF("Get OIS_CTRL fail\n");
		return;
	}

	// According to OIS_CTRL register value to decide what target address to use
	if (nRet == OIS_ON) {
		nTargetAddressX = DW9781C_REG_OIS_TARGETX;
		nTargetAddressY = DW9781C_REG_OIS_TARGETY;
	} else {
		nTargetAddressX = DW9781C_REG_OIS_CL_TARGETX;
		nTargetAddressY = DW9781C_REG_OIS_CL_TARGETY;
	}

#ifdef FOR_DEBUG
	LOG_INF("DW9781C_REG_OIS_CTRL: (%d), nTargetAddressX/Y: (0x%x/0x%x)\n",
		nRet, nTargetAddressX, nTargetAddressY);
#endif

	// get x target
	ret = ois_i2c_rd_s16(m_client, nTargetAddressX, target_x);
	if (ret < 0) {
		LOG_INF("Get x target fail\n");
		return;
	}

	// get y target
	ret = ois_i2c_rd_s16(m_client, nTargetAddressY, target_y);
	if (ret < 0) {
		LOG_INF("Get y target fail\n");
		return;
	}

	// get x len position
	ret = ois_i2c_rd_s16(m_client, DW9781C_REG_OIS_LENS_POSX, len_x);
	if (ret < 0) {
		LOG_INF("Get lens x position fail\n");
		return;
	}

	// get y len position
	ret = ois_i2c_rd_s16(m_client, DW9781C_REG_OIS_LENS_POSY, len_y);
	if (ret < 0) {
		LOG_INF("Get lens y position fail\n");
		return;
	}
#ifdef FOR_DEBUG
	LOG_INF("Gyro, Target, Lens position (x/y) : (%d/%d), (%d/%d), (%d/%d)\n",
			*gyro_x, *gyro_y,
			*target_x, *target_y,
			*len_x, *len_y);
#endif
}

static void fixmode(u16 code_x, u16 code_y)
{
	int ret = 0;

	// set ic servo on / ois off
	ret = ois_i2c_wr_u16(m_client, DW9781C_REG_OIS_CTRL, SERVO_ON);
	LOG_INF("targetX (%d), targetY (%d)", code_x, code_y);

	// set targetX
	ret = ois_i2c_wr_u16(m_client, DW9781C_REG_OIS_CL_TARGETX, code_x);
	I2C_OPERATION_CHECK(ret);

	// set targetY
	ret = ois_i2c_wr_u16(m_client, DW9781C_REG_OIS_CL_TARGETY, code_y);
	I2C_OPERATION_CHECK(ret);
}

static int dw9781c_release(struct dw9781c_device *dw9781c)
{
	int ret = 0;
	struct i2c_client *client = v4l2_get_subdevdata(&dw9781c->sd);

	ret = ois_i2c_wr_u16(client, DW9781C_REG_OIS_CTRL, SERVO_OFF);
	I2C_OPERATION_CHECK(ret);
	mdelay(4);
	ret = ois_i2c_wr_u16(client, DW9781C_REG_CHIP_CTRL, OFF);
	I2C_OPERATION_CHECK(ret);
	mdelay(10);

	return 0;
}

#ifdef FW_UPDATE
static void set_mode_for_FW_download(void)
{
	u16 FMCret = 0;
	int ret = 0;

	ret = ois_i2c_wr_u16(m_client, 0xd001, 0x0000);
	I2C_OPERATION_CHECK(ret);

	LOG_INF("[dw9781c_all_protection_release] execution\r\n");
	/* release all protection */
	ret = ois_i2c_wr_u16(m_client, 0xFAFA, 0x98AC);
	I2C_OPERATION_CHECK(ret);
	mdelay(1);

	ret = ois_i2c_wr_u16(m_client, 0xF053, 0x70BD);
	I2C_OPERATION_CHECK(ret);
	mdelay(1);

	//wait 5ms
	ois_i2c_rd_u16(m_client, 0xDE01, &FMCret);
	LOG_INF("FMC register (%d)", FMCret);
	mdelay(5);
}

static void erase_FW_flash_sector(void)
{
	LOG_INF("[dw9781c_erase_mtp] start erasing firmware flash\r\n");
	/* 12c level adjust */
	ois_i2c_wr_u16(m_client, 0xd005, 0x0001);
	ois_i2c_wr_u16(m_client, 0xdd03, 0x0002);
	ois_i2c_wr_u16(m_client, 0xdd04, 0x0002);

	/* 4k Sector_0 */
	ois_i2c_wr_u16(m_client, 0xde03, 0x0000);
	/* 4k Sector Erase */
	ois_i2c_wr_u16(m_client, 0xde04, 0x0002);
	mdelay(10);
	/* 4k Sector_1 */
	ois_i2c_wr_u16(m_client, 0xde03, 0x0008);
	/* 4k Sector Erase */
	ois_i2c_wr_u16(m_client, 0xde04, 0x0002);
	mdelay(10);
	/* 4k Sector_2 */
	ois_i2c_wr_u16(m_client, 0xde03, 0x0010);
	/* 4k Sector Erase */
	ois_i2c_wr_u16(m_client, 0xde04, 0x0002);
	mdelay(10);
	/* 4k Sector_3 */
	ois_i2c_wr_u16(m_client, 0xde03, 0x0018);
	/* 4k Sector Erase */
	ois_i2c_wr_u16(m_client, 0xde04, 0x0002);
	mdelay(10);
	/* 4k Sector_4 */
	ois_i2c_wr_u16(m_client, 0xde03, 0x0020);
	/* 4k Sector Erase */
	ois_i2c_wr_u16(m_client, 0xde04, 0x0002);
	mdelay(10);
	LOG_INF("[dw9781c_erase_mtp] complete erasing firmware flash\r\n");
}

static void ois_reset(void)
{
	ois_i2c_wr_u16(m_client, 0xD002, 0x0001);
	mdelay(4);

	ois_i2c_wr_u16(m_client, 0xD001, 0x0001);
	mdelay(25);

	ois_i2c_wr_u16(m_client, 0xEBF1, 0x56FA);
}

//#define DATPKT_SIZE 256
#define DATPKT_SIZE 1
#define MTP_START_ADDRESS 0x8000

struct FirmwareContex {
	unsigned int driverIc;
	unsigned int size;
	unsigned short *fwContentPtr;
	unsigned short version;
};

struct FirmwareContex g_firmwareContext;

void GenerateFirmwareContexts(void)
{
	g_firmwareContext.version = 0x0624;
	g_firmwareContext.size = 10240; /* size: word */
	g_firmwareContext.driverIc = 0x9781;
	g_firmwareContext.fwContentPtr = DW9781_Flash_Buf;
}

unsigned short buf_temp[10240];

static int FlashDownload_Seq(void)
{
	unsigned short addr;
	int i = 0;
	int ret = 0;

	memset(buf_temp, 0, g_firmwareContext.size * sizeof(unsigned short));
	GenerateFirmwareContexts();

	// set mode for FW download
	set_mode_for_FW_download();

	// Erase FW flash sector
	erase_FW_flash_sector();

	LOG_INF("[dw9781c_download_fw] start firmware download\r\n");

	// Write FW data
	for (i = 0; i < g_firmwareContext.size; i += DATPKT_SIZE) {
		addr = MTP_START_ADDRESS + i;
		ois_i2c_wr_u16(m_client, addr, *(g_firmwareContext.fwContentPtr + i));
		// i2c_block_write_reg(addr, g_firmwareContext.fwContentPtr + i, DATPKT_SIZE);

	}
	LOG_INF("[dw9781c_download_fw] write firmware to flash\r\n");

	// Read FW data
	for (i = 0; i <  g_firmwareContext.size; i += DATPKT_SIZE) {
		addr = MTP_START_ADDRESS + i;
		ois_i2c_rd_u16(m_client, addr, buf_temp + i);
		// i2c_block_read_reg(addr, buf_temp + i, DATPKT_SIZE);
	}
	LOG_INF("[dw9781c_download_fw] read firmware from flash");

	// Verify FW data
	for (i = 0; i < g_firmwareContext.size; i++) {
		if (g_firmwareContext.fwContentPtr[i] != buf_temp[i]) {
			LOG_INF("firmware verify NG!!! ADDR:%04X, firmware:%04x, READ:%04x\n",
				MTP_START_ADDRESS+i,
				g_firmwareContext.fwContentPtr[i],
				buf_temp[i]);
			// shutdown mode
			ret = ois_i2c_wr_u16(m_client, DW9781C_REG_CHIP_CTRL, OFF);
			I2C_OPERATION_CHECK(ret);
			return -1;
		}
	}

	// IC reset
	ois_reset();

	return 0;
}
#endif

static int dw9781c_init(struct dw9781c_device *dw9781c)
{
	// Initial parameters
	int ret = 0;
	struct i2c_client *client = v4l2_get_subdevdata(&dw9781c->sd);
	u16 i2c_readvalue = 0;
	u16 i2cret_ver = 0, i2cret_date = 0;

	m_client = client;

	client->addr = DW9781OIS_I2C_SLAVE_ADDR >> 1;
	// set chip enable
	ret = ois_i2c_wr_u16(client, DW9781C_REG_CHIP_CTRL, ON);
	I2C_OPERATION_CHECK(ret);
	mdelay(10);
	// OIS RESET
	ret = ois_i2c_wr_u16(client, DW9781C_REG_OIS_LOGIC_RESET, ON);
	I2C_OPERATION_CHECK(ret);
	mdelay(4);
	ret = ois_i2c_wr_u16(client, DW9781C_REG_OIS_DSP_CTRL, ON);
	I2C_OPERATION_CHECK(ret);
	mdelay(25);
	ret = ois_i2c_wr_u16(client, DW9781C_REG_OIS_USER_WRITE_PROTECT, USER_WRITE_EN);
	mdelay(10);

	// -------------------------------- read firmware version -------------------------
	ret = ois_i2c_rd_u16(client, 0x7001, &i2cret_ver);
	ret = ois_i2c_rd_u16(client, 0x7002, &i2cret_date);
	LOG_INF("FW_VER (0x%x), FW_DATE (0x%x)", i2cret_ver, i2cret_date);

#ifdef FW_UPDATE
	// firmware update
	if (i2cret_date != 0x0601) {
		// Uodate firmware
		if (FlashDownload_Seq() < 0) {
			LOG_INF("FlashDownload_Seq fail!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
			return -1;
		}
		LOG_INF("FlashDownload_Seq Pass!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	}

	ret = ois_i2c_rd_u16(client, 0x7001, &i2cret_ver);
	ret = ois_i2c_rd_u16(client, 0x7002, &i2cret_date);
	LOG_INF("after update, FW_VER (0x%x), FW_DATE (0x%x)", i2cret_ver, i2cret_date);
#endif

	// check is master or intercept
	ret = ois_i2c_rd_u16(client, DW9781C_REG_GYRO_INIT, &i2c_readvalue);
	LOG_INF("after reset, REG_GYRO_INIT = 0x%x\n", i2c_readvalue);

	// if the default value isn't intercept mode, modify to intercept mode
	if (i2c_readvalue != 0x0001) {
		// open gyro data reading and set intercept
		ret = ois_i2c_wr_u16(client, DW9781C_REG_GYRO_INIT, 0x0001);
		I2C_OPERATION_CHECK(ret);
		mdelay(2);

		// Store start =======================================
		LOG_INF("do store\n");
		ret = ois_i2c_wr_u16(client, 0x7015, 0x0002);
		I2C_OPERATION_CHECK(ret);
		mdelay(10);

		ret = ois_i2c_wr_u16(client, 0xFD00, 0x5252);
		I2C_OPERATION_CHECK(ret);
		mdelay(1);

		ret = ois_i2c_wr_u16(client, 0x7011, 0x00AA);
		I2C_OPERATION_CHECK(ret);
		mdelay(20);

		ret = ois_i2c_wr_u16(client, 0x7010, 0x8000);
		I2C_OPERATION_CHECK(ret);
		mdelay(200);
		// Store end =======================================

		// OIS RESET
		LOG_INF("do reset\n");
		ret = ois_i2c_wr_u16(client, DW9781C_REG_OIS_LOGIC_RESET, ON);
		I2C_OPERATION_CHECK(ret);
		mdelay(4);
		ret = ois_i2c_wr_u16(client, DW9781C_REG_OIS_DSP_CTRL, ON);
		I2C_OPERATION_CHECK(ret);
		mdelay(25);
		ret = ois_i2c_wr_u16(client, DW9781C_REG_OIS_USER_WRITE_PROTECT, USER_WRITE_EN);
		mdelay(10);
	}

	// set pantilt limit, (1100 is equal to 1.1 degree)
	ret = ois_i2c_wr_u16(client, DW9781C_REG_OIS_PANTILT_DERGEEX, 1100);
	I2C_OPERATION_CHECK(ret);
	mdelay(1);
	ret = ois_i2c_wr_u16(client, DW9781C_REG_OIS_PANTILT_DERGEEY, 1100);
	I2C_OPERATION_CHECK(ret);
	mdelay(1);
	// set gyro select ICM42631_AUX2
	ret = ois_i2c_wr_u16(client, DW9781C_REG_IMU_SELECT, ICM42631_AUX2);
	I2C_OPERATION_CHECK(ret);
	mdelay(1);

	// open gyro data reading
	ret = ois_i2c_wr_u16(client, DW9781C_REG_GYRO_INIT, 0x0001);
	I2C_OPERATION_CHECK(ret);
	usleep_range(1900, 2000);

	// check is master or intercept
	ret = ois_i2c_rd_u16(client, DW9781C_REG_GYRO_INIT, &i2c_readvalue);
	LOG_INF("after open gyro data reading, REG_GYRO_INIT = 0x%x\n", i2c_readvalue);

	// lock OIS, because DW hw limitation (can't get gyro data)
	fixmode(0, 0);
	fixmode00 = 1;

	// set ois mode
	ret = ois_i2c_wr_u16(client, DW9781C_REG_OIS_MODE, DW9781C_STILL_MODE);
	I2C_OPERATION_CHECK(ret);
	mdelay(1);
	ret = ois_i2c_wr_u16(client, DW9781C_REG_OIS_CMD_STATUS, REG_DEAFULT_ON);
	I2C_OPERATION_CHECK(ret);
	mdelay(1);
	LOG_INF("stream on, servo on dw9781c\n");

	return ret;
}

/* Power handling */
static int dw9781c_power_off(struct dw9781c_device *dw9781c)
{
	int ret;
	int i;
	int hw_nums;

	LOG_INF("+\n");

	hw_nums = ARRAY_SIZE(pio_names);
	if (hw_nums > PINCTRL_MAXSIZE)
		hw_nums = PINCTRL_MAXSIZE;
	for (i = 0; i < hw_nums; i += 2) {
		int idx = 1 + i;

		if (dw9781c->pio && dw9781c->pio_state[idx]) {
			ret = pinctrl_select_state(dw9781c->pio,
						dw9781c->pio_state[idx]);
			if (ret < 0)
				LOG_INF("cannot enable %d pintctrl\n", idx);
		}
	}
	usleep_range(1000, 1100);

	hw_nums = ARRAY_SIZE(ldo_names);
	if (hw_nums > REGULATOR_MAXSIZE)
		hw_nums = REGULATOR_MAXSIZE;
	for (i = 0; i < hw_nums; i++) {
		if (dw9781c->ldo[i]) {
			ret = regulator_disable(dw9781c->ldo[i]);
			if (ret < 0)
				LOG_INF("cannot enable %d regulator\n", i);
		}
	}

	hw_nums = ARRAY_SIZE(clk_names);
	if (hw_nums > CLK_MAXSIZE)
		hw_nums = CLK_MAXSIZE;
	for (i = 0; i < hw_nums; i += 2) {
		struct clk *mclk = dw9781c->clk[i];
		struct clk *mclk_src = dw9781c->clk[i + 1];

		if (mclk && mclk_src) {
			clk_disable_unprepare(mclk_src);
			clk_disable_unprepare(mclk);
		}
	}
	LOG_INF("-\n");

	return ret;
}

static int dw9781c_power_on(struct dw9781c_device *dw9781c)
{
	int ret;
	int i;
	int hw_nums;

	LOG_INF("+\n");

	hw_nums = ARRAY_SIZE(clk_names);
	if (hw_nums > CLK_MAXSIZE)
		hw_nums = CLK_MAXSIZE;
	for (i = 0; i < hw_nums; i += 2) {
		struct clk *mclk = dw9781c->clk[i];
		struct clk *mclk_src = dw9781c->clk[i + 1];

		if (mclk && mclk_src) {
			ret = clk_prepare_enable(mclk);
			if (ret) {
				LOG_INF("cannot enable %d mclk\n", i);
			} else {
				ret = clk_prepare_enable(mclk_src);
				if (ret) {
					LOG_INF("cannot enable %d mclk_src\n", i);
				} else {
					ret = clk_set_parent(mclk, mclk_src);
					if (ret)
						LOG_INF("cannot set %d mclk's parent'\n", i);
				}
			}
		}
	}

	hw_nums = ARRAY_SIZE(ldo_names);
	if (hw_nums > REGULATOR_MAXSIZE)
		hw_nums = REGULATOR_MAXSIZE;
	for (i = 0; i < hw_nums; i++) {
		if (dw9781c->ldo[i]) {
			ret = regulator_enable(dw9781c->ldo[i]);
			if (ret < 0)
				LOG_INF("cannot enable %d regulator\n", i);
		}
	}
	usleep_range(1000, 1100);

	hw_nums = ARRAY_SIZE(pio_names);
	if (hw_nums > PINCTRL_MAXSIZE)
		hw_nums = PINCTRL_MAXSIZE;
	for (i = 0; i < hw_nums; i += 2) {
		int idx = i;

		if (dw9781c->pio && dw9781c->pio_state[idx]) {
			ret = pinctrl_select_state(dw9781c->pio,
						dw9781c->pio_state[idx]);
			if (ret < 0)
				LOG_INF("cannot enable %d pintctrl\n", idx);
		}
	}

	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(DW9781C_CTRL_DELAY_US, DW9781C_CTRL_DELAY_US + 100);

	LOG_INF("-\n");

	return ret;
}

static int dw9781c_set_ctrl(struct v4l2_ctrl *ctrl)
{
	/* struct dw9781c_device *dw9781c = to_dw9781c_ois(ctrl); */

	return 0;
}

static const struct v4l2_ctrl_ops dw9781c_ois_ctrl_ops = {
	.s_ctrl = dw9781c_set_ctrl,
};

static int dw9781c_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;

	ret = pm_runtime_get_sync(sd->dev);
	if (ret < 0) {
		pm_runtime_put_noidle(sd->dev);
		return ret;
	}

	return 0;
}

static int dw9781c_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	pm_runtime_put(sd->dev);
	return 0;
}

static long dw9781c_ops_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;

	switch (cmd) {

	case VIDIOC_MTK_S_OIS_MODE:
	{
		int *ois_mode = arg;

		if (*ois_mode)
			LOG_INF("VIDIOC_MTK_S_OIS_MODE Enable\n");
		else
			LOG_INF("VIDIOC_MTK_S_OIS_MODE Disable\n");
	}
	break;

	case VIDIOC_MTK_G_OIS_POS_INFO:
	{
		struct mtk_ois_pos_info *info = arg;
		struct OisInfo pos_info;

		memset(&pos_info, 0, sizeof(struct OisInfo));

		/* To Do */

		if (copy_to_user((void *)info->p_ois_info, &pos_info, sizeof(pos_info)))
			ret = -EFAULT;
	}
	break;
	case VIDIOC_MTK_S_OIS_INIT:
	{
		struct dw9781c_device *dw9781c = sd_to_dw9781c_ois(sd);

		dw9781c_init(dw9781c);
		break;
	}
	case VIDIOC_MTK_S_OIS_UNINIT:
	{
		struct dw9781c_device *dw9781c = sd_to_dw9781c_ois(sd);

		dw9781c_release(dw9781c);
		break;
	}
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

/* OIS Workqueue */
static void ois_init_fx(struct work_struct *data)
{
	int err = 0;

	LOG_INF("+\n");
	// power on OIS pmic
	err = dw9781c_power_on(g_dw9781c);
	if (err < 0) {
		LOG_INF("OIS power on fail!\n");
		ois_init_done = 1;
		return;
	}

	// OIS init
	err = dw9781c_init(g_dw9781c);
	if (err < 0)
		LOG_INF("OIS init fail!\n");

	ois_init_done = 1;
	LOG_INF("-\n");
}

static int dw9781c_enable(struct hf_device *hfdev, int sensor_type, int en)
{
	int err = 0;
	struct device *dev = hf_device_get_private_data(hfdev);
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9781c_device *dw9781c = sd_to_dw9781c_ois(sd);
	int wait_loop = 0;

	LOG_INF("id:%d en:%d\n", sensor_type, en);
	if (en) {
		/* OIS Workqueue */
		if (ois_init_wq == NULL) {
			ois_init_wq =
				create_singlethread_workqueue("ois_init_work");
			if (!ois_init_wq) {
				LOG_INF("create_singlethread_workqueue fail\n");
				return -ENOMEM;
			}

			/* init work queue */
			INIT_WORK(&ois_init_work, ois_init_fx);
			ois_init_done = 0;
			queue_work(ois_init_wq, &ois_init_work);
		}

	} else {
		wait_loop = 1000;

		while (wait_loop > 0) {
			if (ois_init_done)
				break;
			mdelay(1);
			wait_loop--;
		}
		/* OIS Workqueue */
		if (ois_init_wq) {
			/* flush work queue */
			flush_work(&ois_init_work);

			flush_workqueue(ois_init_wq);
			destroy_workqueue(ois_init_wq);
			ois_init_wq = NULL;
		}

		// OIS uninit
		err = dw9781c_release(dw9781c);
		if (err < 0)
			LOG_INF("dw9781c release failed!\n");

		// power off OIS pmic
		err = dw9781c_power_off(dw9781c);
		if (err < 0) {
			LOG_INF("OIS power on fail!\n");
			return err;
		}
	}

	return err;
}

static int dw9781c_batch(struct hf_device *hfdev, int sensor_type,
							int64_t delay, int64_t latency)
{
	LOG_INF("id:%d delay:%lld latency:%lld\n", sensor_type,
				delay, latency);
	return 0;
}

static int dw9781c_sample(struct hf_device *hfdev)
{
	struct device *dev = hf_device_get_private_data(hfdev);
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9781c_device *dw9781c = sd_to_dw9781c_ois(sd);
	struct hf_manager *manager = dw9781c->hf_dev.manager;
	struct hf_manager_event event;

	s16 gyro_x = 0, gyro_y = 0;
	s16 target_x = 0, target_y = 0;
	s16 len_x = 0, len_y = 0;

	if (fixmode00 == 0)
		readhall(&gyro_x, &gyro_y, &target_x, &target_y, &len_x, &len_y);

	memset(&event, 0, sizeof(struct hf_manager_event));
	event.timestamp = get_interrupt_timestamp(manager);
	event.sensor_type = dw9781c->hf_dev.support_list[0].sensor_type;
	event.accurancy = SENSOR_ACCURANCY_HIGH;
	event.action = DATA_ACTION;
	// unit transform, full scale is +-250dps, 65536/500=131(code/dps)
	event.word[0] = gyro_x * 1000 / 131 * 1000;	// ois gyro x
	event.word[1] = gyro_y * 1000 / 131 * 1000;	// ois gyro y
	event.word[2] = target_x * 1000000;	// target x
	event.word[3] = target_y * 1000000;	// target y
	event.word[4] = len_x * 1000000;	// HALL_X
	event.word[5] = len_y * 1000000;	// HALL_Y
	manager->report(manager, &event);
	manager->complete(manager);

	return 0;
}

static int dw9781c_custom_cmd(struct hf_device *hfdev, int sensor_type,
		struct custom_cmd *cust_cmd)
{
	int ret = 0;

#ifdef FOR_DEBUG
	LOG_INF("command%d, data%d\n", cust_cmd->command, cust_cmd->data[0]);
#endif
	// OIS_MODE_CONFIG
	if (cust_cmd->command == 1 || cust_cmd->command == 6) {
		if (cust_cmd->data[0] == 0) {
#ifdef FOR_DEBUG
			LOG_INF("unlock\n");
#endif
			ret = ois_i2c_wr_u16(m_client, DW9781C_REG_OIS_CTRL, OIS_ON); // OIS ON/SERVO ON
			fixmode00 = 0;
			I2C_OPERATION_CHECK(ret);
		} else {
#ifdef FOR_DEBUG
			LOG_INF("lock!\n");
#endif
			fixmode(0, 0);
			fixmode00 = 1;
		}

	// OIS_POSTURE_CONFIG
	} else if (cust_cmd->command == 3) {
		// manual control
		LOG_INF("manual control, targetX (%d), targetY (%d)",
			cust_cmd->data[0] >> 16,
			cust_cmd->data[0] & 0xFFFF);

		// set ic servo on / ois off
		ret = ois_i2c_wr_u16(m_client, DW9781C_REG_OIS_CTRL, SERVO_ON);
		I2C_OPERATION_CHECK(ret);

		// set targetX
		ret = ois_i2c_wr_u16(m_client, DW9781C_REG_OIS_CL_TARGETX,
			(cust_cmd->data[0] >> 16));
		I2C_OPERATION_CHECK(ret);

		// set targetY
		ret = ois_i2c_wr_u16(m_client, DW9781C_REG_OIS_CL_TARGETY,
			(cust_cmd->data[0] & 0xFFFF));
		I2C_OPERATION_CHECK(ret);
	}

	return 0;
}

static struct sensor_info support_sensors[] = {
	{
		.sensor_type = SENSOR_TYPE_OIS1,
		.gain = 1000000,
		.name = {'o', 'i', 's', '1'},
		.vendor = {'m', 't', 'k'},
	}
};

static const struct v4l2_subdev_internal_ops dw9781c_int_ops = {
	.open = dw9781c_open,
	.close = dw9781c_close,
};

static const struct v4l2_subdev_core_ops dw9781c_ops_core = {
	.ioctl = dw9781c_ops_core_ioctl,
};

static const struct v4l2_subdev_ops dw9781c_ops = {
	.core = &dw9781c_ops_core,
};

static void dw9781c_subdev_cleanup(struct dw9781c_device *dw9781c)
{
	v4l2_async_unregister_subdev(&dw9781c->sd);
	v4l2_ctrl_handler_free(&dw9781c->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&dw9781c->sd.entity);
#endif
}

static int dw9781c_init_controls(struct dw9781c_device *dw9781c)
{
	struct v4l2_ctrl_handler *hdl = &dw9781c->ctrls;
	/* const struct v4l2_ctrl_ops *ops = &dw9781c_ois_ctrl_ops; */

	v4l2_ctrl_handler_init(hdl, 1);

	if (hdl->error)
		return hdl->error;

	dw9781c->sd.ctrl_handler = hdl;

	return 0;
}

static int dw9781c_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct dw9781c_device *dw9781c;
	int ret;
	int i;
	int hw_nums;

	LOG_INF("+\n");

	dw9781c = devm_kzalloc(dev, sizeof(*dw9781c), GFP_KERNEL);
	if (!dw9781c)
		return -ENOMEM;

	hw_nums = ARRAY_SIZE(clk_names);
	if (hw_nums > CLK_MAXSIZE)
		hw_nums = CLK_MAXSIZE;
	for (i = 0; i < hw_nums; i++) {
		dw9781c->clk[i] = devm_clk_get(dev, clk_names[i]);
		if (IS_ERR(dw9781c->clk[i])) {
			dw9781c->clk[i] = NULL;
			LOG_INF("cannot get %s clk\n", clk_names[i]);
		}
	}

	hw_nums = ARRAY_SIZE(ldo_names);
	if (hw_nums > REGULATOR_MAXSIZE)
		hw_nums = REGULATOR_MAXSIZE;
	for (i = 0; i < hw_nums; i++) {
		dw9781c->ldo[i] = devm_regulator_get(dev, ldo_names[i]);
		if (IS_ERR(dw9781c->ldo[i])) {
			LOG_INF("cannot get %s regulator\n", ldo_names[i]);
			dw9781c->ldo[i] = NULL;
		}
	}

	hw_nums = ARRAY_SIZE(pio_names);
	if (hw_nums > PINCTRL_MAXSIZE)
		hw_nums = PINCTRL_MAXSIZE;
	dw9781c->pio = devm_pinctrl_get(dev);
	if (IS_ERR(dw9781c->pio)) {
		ret = PTR_ERR(dw9781c->pio);
		dw9781c->pio = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		for (i = 0; i < hw_nums; i++) {
			dw9781c->pio_state[i] = pinctrl_lookup_state(
				dw9781c->pio, pio_names[i]);

			if (IS_ERR(dw9781c->pio_state[i])) {
				ret = PTR_ERR(dw9781c->pio_state[i]);
				dw9781c->pio_state[i] = NULL;
				LOG_INF("cannot get %s pinctrl\n", pio_names[i]);
			}
		}
	}

	client->addr = (DW9781OIS_I2C_SLAVE_ADDR) >> 1;
	m_client = client;

	// hf manager porting
	dw9781c->hf_dev.dev_name = DW9781C_NAME;
	dw9781c->hf_dev.device_poll = HF_DEVICE_IO_POLLING;
	dw9781c->hf_dev.device_bus = HF_DEVICE_IO_SYNC;
	dw9781c->hf_dev.support_list = support_sensors;
	dw9781c->hf_dev.support_size = ARRAY_SIZE(support_sensors);
	dw9781c->hf_dev.enable = dw9781c_enable;
	dw9781c->hf_dev.batch = dw9781c_batch;
	dw9781c->hf_dev.sample = dw9781c_sample;
	dw9781c->hf_dev.custom_cmd = dw9781c_custom_cmd;
	hf_device_set_private_data(&dw9781c->hf_dev, dev);
	ret = hf_device_register_manager_create(&dw9781c->hf_dev);
	if (ret < 0) {
		LOG_INF("%s hf_manager_create fail\n", __func__);
		ret = -1;
		goto err_cleanup;
	}

	v4l2_i2c_subdev_init(&dw9781c->sd, client, &dw9781c_ops);
	dw9781c->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dw9781c->sd.internal_ops = &dw9781c_int_ops;

	ret = dw9781c_init_controls(dw9781c);
	if (ret)
		goto err_cleanup;

#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&dw9781c->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	dw9781c->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&dw9781c->sd);
	if (ret < 0)
		goto err_cleanup;

	pm_runtime_enable(dev);

	g_dw9781c = dw9781c;

	LOG_INF("-\n");

	return 0;

err_cleanup:
	hf_device_unregister_manager_destroy(&dw9781c->hf_dev);
	dw9781c_subdev_cleanup(dw9781c);
	return ret;
}

static void dw9781c_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9781c_device *dw9781c = sd_to_dw9781c_ois(sd);

	LOG_INF("+\n");

	hf_device_unregister_manager_destroy(&dw9781c->hf_dev);
	dw9781c_subdev_cleanup(dw9781c);
	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		dw9781c_power_off(dw9781c);
	pm_runtime_set_suspended(&client->dev);

	LOG_INF("-\n");
}

static int __maybe_unused dw9781c_ois_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9781c_device *dw9781c = sd_to_dw9781c_ois(sd);

	return dw9781c_power_off(dw9781c);
}

static int __maybe_unused dw9781c_ois_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dw9781c_device *dw9781c = sd_to_dw9781c_ois(sd);

	return dw9781c_power_on(dw9781c);
}

static const struct i2c_device_id dw9781c_id_table[] = {
	{ DW9781C_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, dw9781c_id_table);

static const struct of_device_id dw9781c_of_table[] = {
	{ .compatible = "mediatek,dw9781c" },
	{ },
};
MODULE_DEVICE_TABLE(of, dw9781c_of_table);

static const struct dev_pm_ops dw9781c_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(dw9781c_ois_suspend, dw9781c_ois_resume, NULL)
};

static struct i2c_driver dw9781c_i2c_driver = {
	.driver = {
		.name = DW9781C_NAME,
		.pm = &dw9781c_pm_ops,
		.of_match_table = dw9781c_of_table,
	},
	.probe_new  = dw9781c_probe,
	.remove = dw9781c_remove,
	.id_table = dw9781c_id_table,
};

module_i2c_driver(dw9781c_i2c_driver);

MODULE_AUTHOR("Po-Hao Huang <Po-Hao.Huang@mediatek.com>");
MODULE_DESCRIPTION("DW9781C OIS driver");
MODULE_LICENSE("GPL");
