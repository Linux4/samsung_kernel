/*
 * Copyright (C) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "tcs3407.h"
#include <linux/sensor/sensors_core.h>
#include <linux/kthread.h>

#define VENDOR				"AMS"

#define TCS3407_CHIP_NAME	"TCS3407"
#define TCS3408_CHIP_NAME	"TCS3408"

#define VERSION				"2"
#define SUB_VERSION			"0"
#define VENDOR_VERSION		"a"

#define MODULE_NAME		"light_sensor"

#define TCS3407_SLAVE_I2C_ADDR_REVID_V0 0x39
#define TCS3407_SLAVE_I2C_ADDR_REVID_V1 0x29

#define AMSDRIVER_I2C_RETRY_DELAY	10
#define AMSDRIVER_I2C_MAX_RETRIES	3


#define REL_DELTA                       REL_DIAL
#define REL_2ND_MIN                     REL_HWHEEL
#define REL_2ND_MAX                     REL_WHEEL
#define REL_LUX                         REL_MISC


#define AMS_ROUND_SHFT_VAL				4
#define AMS_ROUND_ADD_VAL				(1 << (AMS_ROUND_SHFT_VAL - 1))
#define AMS_ALS_GAIN_FACTOR				1000
#define CPU_FRIENDLY_FACTOR_1024		1
#define AMS_ALS_Cc						(118 * CPU_FRIENDLY_FACTOR_1024)
#define AMS_ALS_Rc						(112 * CPU_FRIENDLY_FACTOR_1024)
#define AMS_ALS_Gc						(172 * CPU_FRIENDLY_FACTOR_1024)
#define AMS_ALS_Bc						(180 * CPU_FRIENDLY_FACTOR_1024)
#define AMS_ALS_Wbc						(111 * CPU_FRIENDLY_FACTOR_1024)

#define AMS_ALS_FACTOR					1000

#ifdef CONFIG_AMS_OPTICAL_SENSOR_259x
#define AMS_ALS_TIMEBASE				(100000) /* in uSec, see data sheet */
#define AMS_ALS_ADC_MAX_COUNT			(37888) /* see data sheet */
#else
#define AMS_ALS_TIMEBASE				(2780) /* in uSec, see data sheet */
#define AMS_ALS_ADC_MAX_COUNT			(1024) /* see data sheet */
#endif
#define AMS_ALS_THRESHOLD_LOW			(5) /* in % */
#define AMS_ALS_THRESHOLD_HIGH			(5) /* in % */

#define AMS_ALS_ATIME					(50000)

#define WIDEBAND_CONST	4
#define CLEAR_CONST		3

/* REENABLE only enables those that were on record as being enabled */
#define AMS_REENABLE(ret)				{ret = ams_setByte(ctx->client, DEVREG_ENABLE, ctx->shadowEnableReg); }
#define AMS_PON(ret)					{ret = ams_setByte(ctx->client, DEVREG_ENABLE, 0x01); }

#define AMS_SMUX_EN(ret)				{ret = ams_setByte(ctx->client, DEVREG_ENABLE, 0x11); }

/* DISABLE_ALS disables ALS w/o recording that as its new state */
#define AMS_DISABLE_ALS(ret)			{ret = ams_setField(ctx->client, DEVREG_ENABLE, LOW, (MASK_AEN)); }
#define AMS_REENABLE_ALS(ret)			{ret = ams_setField(ctx->client, DEVREG_ENABLE, HIGH, (MASK_AEN)); }

#define AMS_SET_ALS_TIME(uSec, ret)		{ret = ams_setByte(ctx->client, DEVREG_ATIME,   alsTimeUsToReg(uSec)); }
#define AMS_GET_ALS_TIME(uSec, ret)		{ret = ams_getByte(ctx->client, DEVREG_ATIME,   alsTimeUsToReg(uSec)); }

#define AMS_GET_ALS_GAIN(scaledGain, gain, ret)	{ret = ams_getByte(ctx->client, DEVREG_ASTATUS, &(gain)); \
	scaledGain = alsGain_conversion[(gain) & 0x0f]; }

#define AMS_SET_ALS_STEP_TIME(uSec, ret)		{ret = ams_setWord(ctx->client, DEVREG_ASTEPL, alsTimeUsToReg(uSec * 1000)); }

#define AMS_SET_ALS_GAIN(mGain, ret)	{ret = ams_setField(ctx->client, DEVREG_CFG1, alsGainToReg(mGain), MASK_AGAIN); }
#define AMS_SET_ALS_PERS(persCode, ret)	{ret = ams_setField(ctx->client, DEVREG_PERS, (persCode), MASK_APERS); }
#define AMS_CLR_ALS_INT(ret)			{ret = ams_setByte(ctx->client, DEVREG_STATUS, (AINT | ASAT_FDSAT)); }
#define AMS_SET_ALS_THRS_LOW(x, ret)	{ret = ams_setWord(ctx->client, DEVREG_AILTL, (x)); }
#define AMS_SET_ALS_THRS_HIGH(x, ret)	{ret = ams_setWord(ctx->client, DEVREG_AIHTL, (x)); }
#define AMS_SET_ALS_AUTOGAIN(x, ret)	{ret = ams_setField(ctx->client, DEVREG_CFG8, (x), MASK_AUTOGAIN); }
#define AMS_SET_ALS_AGC_LOW_HYST(x)		{ams_setField(ctx->client, DEVREG_CFG10, ((x)<<4), MASK_AGC_LOW_HYST); }
#define AMS_SET_ALS_AGC_HIGH_HYST(x) {ams_setField(ctx->client, DEVREG_CFG10, ((x)<<6), MASK_AGC_HIGH_HYST); }

/* Get CRGB and whatever Wideband it may have */
#define AMS_ALS_GET_CRGB_W(x, ret)		{ret = ams_getBuf(ctx->client, DEVREG_ADATA0L, (uint8_t *) (x), 10); }



/* SMUX Default */
//14 25 23 41 33 12 14 24 53 23 15 14 32 44 21 23 13 54 00 76
// static uint8_t smux_default_data[] = {
//     0x14, 0x25, 0x23, 0x41,
//     0x33, 0x12, 0x14, 0x24,
//     0x53, 0x23, 0x15, 0x14,
//     0x32, 0x44, 0x21, 0x23,
//     0x13, 0x54, 0x00, 0x77
// };

/* 4x3 PD area*/
//14 20 03 41 00 00 00 00 00 23 10 04 32 00 00 00 00 00 00 66
static uint8_t smux_custom_data_43[] = {
    0x14, 0x20, 0x03, 0x41,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x23, 0x10, 0x04,
    0x32, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x66
};
/* 4x2 PD area*/
//14 20 03 00 00 00 00 00 00 23 10 04 00 00 00 00 00 00 00 66
// static uint8_t smux_custom_data_42[] = {
//     0x14, 0x20, 0x03, 0x00,
//     0x00, 0x00, 0x00, 0x00,
//     0x00, 0x23, 0x10, 0x04,
//     0x00, 0x00, 0x00, 0x00,
//     0x00, 0x00, 0x00, 0x66
// };

#define AMS_READ_S_MUX(ret)		{ret = ams_setField(ctx->client, DEVREG_CFG6, ((1)<<3), MASK_SMUX_CMD); }
#define AMS_WRITE_S_MUX(ret)		{ret = ams_setField(ctx->client, DEVREG_CFG6, ((2)<<3), MASK_SMUX_CMD); }
#define AMS_CLOSE_S_MUX(ret)		{ret = ams_setField(ctx->client, DEVREG_CFG6, 0x00, MASK_SMUX_CMD); }


typedef struct{
	uint8_t deviceId;
	uint8_t deviceIdMask;
	uint8_t deviceRef;
	uint8_t deviceRefMask;
	ams_deviceIdentifier_e device;
} ams_deviceIdentifier_t;



#define AMS_PORT_LOG_CRGB_W(dataset) \
		SENSOR_INFO("C, R,G,B = %u, %u,%u,%u; WB = %u\n"\
			, dataset.AdcClear \
			, dataset.AdcRed \
			, dataset.AdcGreen \
			, dataset.AdcBlue \
			, dataset.AdcWb	\
			)

/*
static ams_deviceIdentifier_t deviceIdentifier[] = {
	{AMS_DEVICE_ID, AMS_DEVICE_ID_MASK, AMS_REV_ID, AMS_REV_ID_MASK, AMS_TCS3407},
	{AMS_DEVICE_ID, AMS_DEVICE_ID_MASK, AMS_REV_ID_UNTRIM, AMS_REV_ID_MASK, AMS_TCS3407_UNTRIM},
	{AMS_DEVICE_ID2, AMS_DEVICE_ID2_MASK, AMS_REV_ID2, AMS_REV_ID2_MASK, AMS_TCS3408},
	{AMS_DEVICE_ID2, AMS_DEVICE_ID2_MASK, AMS_REV_ID2_UNTRIM, AMS_REV_ID2_MASK, AMS_TCS3408_UNTRIM},
	{0, 0, 0, 0, AMS_LAST_DEVICE}
};*/

deviceRegisterTable_t deviceRegisterDefinition[DEVREG_REG_MAX] = {
	{ 0x80, 0x00 },          /* DEVREG_ENABLE */
	{ 0x81, 0x00 },          /* DEVREG_ATIME */
	{ 0x82, 0x00 },          /* DEVREG_PTIME */
	{ 0x83, 0x00 },          /* DEVREG_WTIME */
	{ 0x84, 0x00 },          /* DEVREG_AILTL */
	{ 0x85, 0x00 },          /* DEVREG_AILTH */
	{ 0x86, 0x00 },          /* DEVREG_AIHTL */
	{ 0x87, 0x00 },          /* DEVREG_AIHTH */

	{ 0x90, 0x00 },          /* DEVREG_AUXID */
	{ 0x91, AMS_REV_ID },    /* DEVREG_REVID */
	{ 0x92, AMS_DEVICE_ID }, /* DEVREG_ID */
	{ 0x93, 0x00 },          /* DEVREG_STATUS */
	{ 0x94, 0x00 },          /* DEVREG_ASTATUS */
	{ 0x95, 0x00 },          /* DEVREG_ADATAOL */
	{ 0x96, 0x00 },          /* DEVREG_ADATAOH */
	{ 0x97, 0x00 },          /* DEVREG_ADATA1L */
	{ 0x98, 0x00 },          /* DEVREG_ADATA1H */
	{ 0x99, 0x00 },          /* DEVREG_ADATA2L */
	{ 0x9A, 0x00 },          /* DEVREG_ADATA2H */
	{ 0x9B, 0x00 },          /* DEVREG_ADATA3L */
	{ 0x9C, 0x00 },          /* DEVREG_ADATA3H */
	{ 0x9D, 0x00 },          /* DEVREG_ADATA4L */
	{ 0x9E, 0x00 },          /* DEVREG_ADATA4H */
	{ 0x9F, 0x00 },          /* DEVREG_ADATA5L */

	{ 0xA0, 0x00 },          /* DEVREG_ADATA5H */
	{ 0xA3, 0x00 },          /* DEVREG_STATUS2 */
	{ 0xA4, 0x00 },          /* DEVREG_STATUS3 */
	{ 0xA6, 0x00 },          /* DEVREG_STATUS5 */
	{ 0xA7, 0x00 },          /* DEVREG_STATUS4 */
	/* 0xA8 Reserved */
	{ 0xA9, 0x40 },          /* DEVREG_CFG0 */
	{ 0xAA, 0x09 },          /* DEVREG_CFG1 */
	{ 0xAC, 0x0C },          /* DEVREG_CFG3 */
	{ 0xAD, 0x00 },          /* DEVREG_CFG4 */
	{ 0xAF, 0x00 },          /* DEVREG_CFG6 */

	{ 0xB1, 0x98 },          /* DEVREG_CFG8 */
	{ 0xB2, 0x00 },          /* DEVREG_CFG9 */
	{ 0xB3, 0xF2 },          /* DEVREG_CFG10 */
	{ 0xB4, 0x4D },          /* DEVREG_CFG11 */
	{ 0xB5, 0x00 },          /* DEVREG_CFG12 */
	{ 0xBD, 0x00 },          /* DEVREG_PERS */
	{ 0xBE, 0x02 },          /* DEVREG_GPIO */

	{ 0xCA, 0xE7 },          /* DEVREG_ASTEPL */
	{ 0xCB, 0x03 },          /* DEVREG_ASTEPH */
	{ 0xCF, 0X97 },          /* DEVREG_AGC_GAIN_MAX */
	/*0x97, extended max fd_gain to 1024x as 0xA7, again to 2048x onTCS3408 0xBC*/

	{ 0xD6, 0xFf },          /* DEVREG_AZ_CONFIG */
	{ 0xD7, 0x21},           /*DEVREG_FD_CFG0*/
	{ 0xD8, 0x68},           /*DEVREG_FD_CFG1*/
	{ 0xD9, 0x64},           /*DEVREG_FD_CFG2*/
	{ 0xDA, 0x91 },           /*DEVREG_FD_CFG3*/
	{ 0xDB, 0x00 },          /* DEVREG_FD_STATUS */
	/* 0xEF-0xF8 Reserved */
	{ 0xF9, 0x00 },          /* DEVREG_INTENAB */
	{ 0xFA, 0x00 },          /* DEVREG_CONTROL */
	{ 0xFC, 0x00 },          /* DEVREG_FIFO_MAP */
	{ 0xFD, 0x00 },          /* DEVREG_FIFO_STATUS */
	{ 0xFE, 0x00 },          /* DEVREG_FDATAL */
	{ 0xFF, 0x00 },          /* DEVREG_FDATAH */
	{ 0x6f, 0x00 },          /* DEVREG_FLKR_WA_RAMLOC_1 */
	{ 0x71, 0x00 },          /* DEVREG_FLKR_WA_RAMLOC_2 */
	{ 0xF3, 0x00 },          /* DEVREG_SOFT_RESET */

	{ 0x00, 0x00 },			/* DEVREG_RAM_START */
};

uint32_t alsGain_conversion[] = {
	1000 / 2,
	1 * 1000,
	2 * 1000,
	4 * 1000,
	8 * 1000,
	16 * 1000,
	32 * 1000,
	64 * 1000,
	128 * 1000,
	256 * 1000,
	512 * 1000,
	1024 * 1000,
	2048 * 1000,
};

static const struct of_device_id tcs3407_match_table[] = {
	{ .compatible = "ams,tcs3407",},
	{},
};

static int tcs3407_write_reg_bulk(struct i2c_client *client, u8 reg_addr, u8* data, u8 length)
{
	int err = -1;
	int tries = 0;
	int num = 1;
	u8* buffer = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags & I2C_M_TEN,
			.len = (length+1),
			.buf = buffer,
		},
	};
	struct tcs3407_data *chip = i2c_get_clientdata(client);

	buffer = kzalloc(length+1, GFP_KERNEL); // address + data ;

	if (!buffer) {
		err = -ENOMEM;
		SENSOR_ERR("no memory\n");
		return -ENOMEM;
	}

	msgs[0].buf = buffer;
	buffer[0] = reg_addr;
	//&buffer[1] = &data[0];
	memcpy(&buffer[1] , &data[0], length);

// #ifndef AMS_BUILD
// 	if (!device->pm_state || device->regulator_state == 0) {
// 		ALS_err("%s - write error, pm suspend or reg_state %d\n",
// 				__func__, device->regulator_state);
// 		err = -EFAULT;
// 		return err;
// 	}
// #endif

	mutex_lock(&chip->i2clock);
	do {
		err = i2c_transfer(client->adapter, msgs, num);
		if (err != num)
			msleep_interruptible(AMSDRIVER_I2C_RETRY_DELAY);
		if (err < 0)
			SENSOR_ERR("%s - i2c_transfer error = %d\n", __func__, err);
	} while ((err != num) && (++tries < AMSDRIVER_I2C_MAX_RETRIES));
	mutex_unlock(&chip->i2clock);

	if (err != num) {
		SENSOR_ERR("%s -write transfer error:%d\n", __func__, err);
		err = -EIO;
		kfree(buffer);
		buffer = 0;
		return err;
	}

	kfree(buffer);
	buffer = 0;
	return 0;
}

static int tcs3407_write_reg(struct i2c_client *client,
	u8 reg_addr, u8 data)
{
	int err = -1;
	int tries = 0;
	int num = 1;
	u8 buffer[2] = { reg_addr, data };
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags & I2C_M_TEN,
			.len = 2,
			.buf = buffer,
		},
	};
	struct tcs3407_data *chip = i2c_get_clientdata(client);
	mutex_lock(&chip->i2clock);
	do {
		err = i2c_transfer(client->adapter, msgs, num);
		if (err != num)
			msleep_interruptible(AMSDRIVER_I2C_RETRY_DELAY);
		if (err < 0)
			SENSOR_ERR("err %d\n", err);
	} while ((err != num) && (++tries < AMSDRIVER_I2C_MAX_RETRIES));
	mutex_unlock(&chip->i2clock);
	
	if (err != num) {
		SENSOR_ERR("err %d\n", err);
		err = -EIO;
		return err;
	}

	return 0;
}

static int tcs3407_read_reg(struct i2c_client *client,
	u8 reg_addr, u8 *buffer, int length)
{
	int err = -1;
	int tries = 0; /* # of attempts to read the device */
	int num = 2;
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags & I2C_M_TEN,
			.len = 1,
			.buf = buffer,
		},
		{
			.addr = client->addr,
			.flags = (client->flags & I2C_M_TEN) | I2C_M_RD,
			.len = length,
			.buf = buffer,
		},
	};
	struct tcs3407_data *data = i2c_get_clientdata(client);
	
	mutex_lock(&data->i2clock);
	do {
		buffer[0] = reg_addr;
		err = i2c_transfer(client->adapter, msgs, num);
		if (err != num)
			msleep_interruptible(AMSDRIVER_I2C_RETRY_DELAY);
		if (err < 0)
			SENSOR_ERR("err %d\n", err);
	} while ((err != num) && (++tries < AMSDRIVER_I2C_MAX_RETRIES));
	mutex_unlock(&data->i2clock);

	if (err != num) {
		SENSOR_ERR("err %d\n", err);
		err = -EIO;
	} else
		err = 0;

	return err;
}

static uint8_t alsGainToReg(uint32_t x)
{
	int i;

	for (i = sizeof(alsGain_conversion)/sizeof(uint32_t)-1; i != 0; i--) {
		if (x >= alsGain_conversion[i])
			break;
	}
	return (i << 0);
}

static uint16_t alsTimeUsToReg(uint32_t x)
{
	uint16_t regValue;

	regValue = (x / 2816);

	return regValue;
}

static int ams_getByte(struct i2c_client *portHndl, ams_deviceRegister_t reg, uint8_t *readData)
{
	int err = 0;
	uint8_t length = 1;

	/* Sanity check input param */
	if (reg >= DEVREG_REG_MAX)
		return 0;

	err = tcs3407_read_reg(portHndl, deviceRegisterDefinition[reg].address, readData, length);

	return err;
}

static int ams_setByte(struct i2c_client *portHndl, ams_deviceRegister_t reg, uint8_t setData)
{
	int err = 0;

	/* Sanity check input param */
	if (reg >= DEVREG_REG_MAX)
		return 0;

	err = tcs3407_write_reg(portHndl, deviceRegisterDefinition[reg].address, setData);

	return err;
}

static int ams_getBuf(struct i2c_client *portHndl, ams_deviceRegister_t reg, uint8_t *readData, uint8_t length)
{
	int err = 0;

	/* Sanity check input param */
	if (reg >= DEVREG_REG_MAX)
		return 0;

	err = tcs3407_read_reg(portHndl, deviceRegisterDefinition[reg].address, readData, length);

	return err;
}

int ams_setBuf(struct i2c_client *portHndl, ams_deviceRegister_t reg, uint8_t *setData, uint8_t length)
{
	int err = 0;

	/* Sanity check input param */
	if (reg >= DEVREG_REG_MAX)
		return 0;

	// err = tcs3407_write_reg(portHndl, deviceRegisterDefinition[reg].address, *setData);
	err = tcs3407_write_reg_bulk(portHndl, deviceRegisterDefinition[reg].address, setData, length);

	return err;
}

int ams_getWord(struct i2c_client *portHndl, ams_deviceRegister_t reg, uint16_t *readData)
{
	int err = 0;
	uint8_t length = sizeof(uint16_t);
	uint8_t buffer[sizeof(uint16_t)];

	/* Sanity check input param */
	if (reg >= DEVREG_REG_MAX)
		return 0;

	err = tcs3407_read_reg(portHndl, deviceRegisterDefinition[reg].address, buffer, length);

	*readData = ((buffer[0] << AMS_ENDIAN_1) + (buffer[1] << AMS_ENDIAN_2));

	return err;
}

static int ams_setWord(struct i2c_client *portHndl, ams_deviceRegister_t reg, uint16_t setData)
{
	int err = 0;
	uint8_t buffer[sizeof(uint16_t)];

	/* Sanity check input param */
	if (reg >= (DEVREG_REG_MAX - 1))
		return 0;

	buffer[0] = ((setData >> AMS_ENDIAN_1) & 0xff);
	buffer[1] = ((setData >> AMS_ENDIAN_2) & 0xff);

	err = tcs3407_write_reg(portHndl, deviceRegisterDefinition[reg].address, buffer[0]);
	err = tcs3407_write_reg(portHndl, deviceRegisterDefinition[reg + 1].address, buffer[1]);

	return err;
}

int ams_getField(struct i2c_client *portHndl, ams_deviceRegister_t reg, uint8_t *setData, ams_regMask_t mask)
{
	int err = 0;
	uint8_t length = 1;

	/* Sanity check input param */
	if (reg >= DEVREG_REG_MAX)
		return 0;

	err = tcs3407_read_reg(portHndl, deviceRegisterDefinition[reg].address, setData, length);

	*setData &= mask;

	return err;
}

static int ams_setField(struct i2c_client *portHndl, ams_deviceRegister_t reg, uint8_t setData, ams_regMask_t mask)
{
	int err = 1;
	uint8_t length = 1;
	uint8_t original_data;
	uint8_t new_data;

	/* Sanity check input param */
	if (reg >= DEVREG_REG_MAX)
		return 0;

	err = tcs3407_read_reg(portHndl, deviceRegisterDefinition[reg].address, &original_data, length);
	if (err < 0)
		return err;

	new_data = original_data & ~mask;
	new_data |= (setData & mask);

	if (new_data != original_data)
		err = tcs3407_write_reg(portHndl, deviceRegisterDefinition[reg].address, new_data);

	return err;
}

static void als_getDefaultCalibrationData(ams_ccb_als_calibration_t *data)
{
	if (data != NULL) {
		data->Time_base = AMS_ALS_TIMEBASE;
		data->thresholdLow = AMS_ALS_THRESHOLD_LOW;
		data->thresholdHigh = AMS_ALS_THRESHOLD_HIGH;
		data->calibrationFactor = 1000;
	}
}

static void als_update_statics(amsAlsContext_t *ctx);
static void als_compute_data (amsAlsContext_t * ctx, amsAlsDataSet_t * inputData);
static void als_ave_LUX (amsAlsContext_t * ctx);
static amsAlsAdaptive_t als_adaptive(amsAlsContext_t * ctx, amsAlsDataSet_t * inputData);
static void als_calc_LUX_CCT (amsAlsContext_t * ctx,
                                uint16_t clearADC,
                                uint16_t redADC,
                                uint16_t greenADC,
                                uint16_t blueADC);

static void als_calc_LUX_CCT (amsAlsContext_t * ctx,
                                uint16_t clearADC,
                                uint16_t redADC,
                                uint16_t greenADC,
                                uint16_t blueADC) {
    int64_t lux = 0;

    lux = AMS_ALS_C_COEF * clearADC
            + AMS_ALS_R_COEF * redADC
            + AMS_ALS_G_COEF * greenADC
            + AMS_ALS_B_COEF * blueADC;

    /* int64_t is a long long on most platforms but only a long in cygwin64.
     * Promote to long long for portability (on cyg64 they're same size but
     * compiler complains if you use %ll for a long).
     *
     * Regarding 64-bit division in the linux kernel on an ARM:
     * ARM 32 bit toolchains require a function implementation called __aeabi_uldivmod
     * to do the division.  This function is missing. 64-bit division below is done
     * with do_div() for linux kernel builds.
     */
    SENSOR_INFO("tempLux=%lld, cpl=%u\n", (long long int)lux, ctx->cpl);

    lux <<= AMS_ROUND_SHFT_VAL;
    if (lux < (LONG_MAX / ctx->calibration.calibrationFactor)) {
#ifdef __KERNEL__
        lux = lux * ctx->calibration.calibrationFactor;
        do_div(lux, ctx->cpl);
#else
        lux = (lux * ctx->calibration.calibrationFactor) / ctx->cpl;
#endif
    } else {
#ifdef __KERNEL__
        do_div(lux, ctx->cpl);
        lux *= ctx->calibration.calibrationFactor;
#else
        lux = (lux / ctx->cpl ) * ctx->calibration.calibrationFactor;
#endif
    }
    lux += AMS_ROUND_ADD_VAL;
    lux >>= AMS_ROUND_SHFT_VAL;
    ctx->results.mLux = (uint32_t)lux;

    if (redADC == 0 ) redADC = 1;
    ctx->results.CCT = ((AMS_ALS_CT_COEF * blueADC) / redADC) + AMS_ALS_CT_OFFSET;
}

void als_compute_data (amsAlsContext_t * ctx, amsAlsDataSet_t * inputData) {

    if (inputData->datasetArray->clearADC < (uint16_t)USHRT_MAX){
        ctx->results.IR = 0;

        /* Compute irradiances in uW/cm^2 */
        ctx->results.rawClear = inputData->datasetArray->clearADC;
        ctx->results.rawRed = inputData->datasetArray->redADC;
        ctx->results.rawGreen = inputData->datasetArray->greenADC;
        ctx->results.rawBlue = inputData->datasetArray->blueADC;
        ctx->results.irrRed = (inputData->datasetArray->redADC) / ctx->cpl;
        ctx->results.irrClear = (inputData->datasetArray->clearADC) / ctx->cpl;
        ctx->results.irrBlue = (inputData->datasetArray->blueADC) / ctx->cpl;
        ctx->results.irrGreen = (inputData->datasetArray->greenADC) / ctx->cpl;
        ctx->results.irrWideband = (inputData->datasetArray->widebandADC) / ctx->cpl;
        SENSOR_INFO("iC %u iR %u iG %u iB %u iW %u\n"
                        , ctx->results.irrClear , ctx->results.irrRed
                        , ctx->results.irrGreen, ctx->results.irrBlue
                        , ctx->results.irrWideband);

        als_calc_LUX_CCT(ctx
            , inputData->datasetArray->clearADC
            , inputData->datasetArray->redADC
            , inputData->datasetArray->greenADC
            , inputData->datasetArray->blueADC
            );
    } else {
        /* measurement is saturated */
        SENSOR_INFO("saturated\n");
    }
}

void als_ave_LUX (amsAlsContext_t * ctx) {

    /* if average queue isn't full (at startup), fill it */
    if(ctx->ave_lux[AMS_LUX_AVERAGE_COUNT - 1] == 0) {
        ctx->results.mLux_ave = 0;
        ctx->ave_lux_index = 0;
        do {
            ctx->ave_lux[ctx->ave_lux_index++] = ctx->results.mLux;
            ctx->results.mLux_ave += ctx->results.mLux;
        } while (ctx->ave_lux_index < AMS_LUX_AVERAGE_COUNT);
        ctx->ave_lux_index = 1;
    } else {
        /* replace the oldest LUX value with the new LUX value */
        ctx->results.mLux_ave -= ctx->ave_lux[ctx->ave_lux_index];
        ctx->ave_lux[ctx->ave_lux_index] = ctx->results.mLux;
        ctx->results.mLux_ave += ctx->ave_lux[ctx->ave_lux_index];
        ctx->ave_lux_index ++;
    }

    if (ctx->ave_lux_index == AMS_LUX_AVERAGE_COUNT) {
        ctx->ave_lux_index = 0;
    }
}

amsAlsAdaptive_t als_adaptive(amsAlsContext_t * ctx, amsAlsDataSet_t * inputData){
#ifdef CONFIG_AMS_OPTICAL_SENSOR_ALS_RGB
    if (inputData->status & ALS_STATUS_OVFL) {
        ctx->results.adaptive = ADAPTIVE_ALS_TIME_DEC_REQUEST;
    } else {
        if (inputData->datasetArray->clearADC == (uint16_t)USHRT_MAX){
            if (ctx->gain != 0){
                return ADAPTIVE_ALS_GAIN_DEC_REQUEST;
            } else {
                return ADAPTIVE_ALS_TIME_DEC_REQUEST;
            }
        } else {
            if (ctx->gain != 0) {
                if (inputData->datasetArray->clearADC >= ctx->saturation){
                    return ADAPTIVE_ALS_GAIN_DEC_REQUEST;
                }
            } else {
                if (inputData->datasetArray->clearADC >= ctx->saturation){
                    return ADAPTIVE_ALS_TIME_DEC_REQUEST;
                }
            }
            if (inputData->datasetArray->clearADC <= 0xff){
                return ADAPTIVE_ALS_GAIN_INC_REQUEST;
            }
        }
    }
#else
    ctx++; inputData++; /* avoid compiler warning */
#endif
    return ADAPTIVE_ALS_NO_REQUEST;
}

static int amsAlg_als_processData(amsAlsContext_t *ctx, amsAlsDataSet_t *inputData)
{
    int ret = 0;

    if (inputData->status & ALS_STATUS_RDY){
        ctx->previousLux = ctx->results.mLux;
        if (ctx->previousGain != ctx->gain){
            als_update_statics(ctx);
        } else {
            ctx->notStableMeasurement = false;
        }
        als_compute_data(ctx, inputData);
        als_ave_LUX(ctx);
    }
    if (ctx->adaptive){
        if (inputData->status & ALS_STATUS_OVFL){
            ctx->results.adaptive = ADAPTIVE_ALS_GAIN_DEC_REQUEST;
        } else {
            ctx->results.adaptive = als_adaptive(ctx, inputData);
        }
    } else {
        ctx->results.adaptive = ADAPTIVE_ALS_NO_REQUEST;
    }

    if (ctx->notStableMeasurement) {
        ctx->results.mLux = ctx->previousLux;
    }
    return ret;
}


uint32_t ams_getResult(struct tcs3407_data *ctx)
{
	uint32_t returnValue = ctx->updateAvailable;

	ctx->updateAvailable = 0;

	return returnValue;
}

static int amsAlg_als_initAlg(amsAlsContext_t *ctx, amsAlsInitData_t *initData);
static int amsAlg_als_getAlgInfo(amsAlsAlgoInfo_t *info);
static int amsAlg_als_processData(amsAlsContext_t *ctx, amsAlsDataSet_t *inputData);

static int ccb_alsInit(struct tcs3407_data *ctx, ams_ccb_als_init_t *initData)
{
	ams_ccb_als_ctx_t *ccbCtx = &ctx->ccbAlsCtx;
	amsAlsInitData_t initAlsData;
	amsAlsAlgoInfo_t infoAls;
	int ret = 0;


	if (initData)
		memcpy(&ccbCtx->initData, initData, sizeof(ams_ccb_als_init_t));
	else
		ccbCtx->initData.calibrate = false;

	initAlsData.adaptive = false;
	initAlsData.irRejection = false;
	initAlsData.gain = ccbCtx->initData.configData.gain;
#ifdef AMS_ALS_GAIN_V2
	ctx->alsGain = initAlsData.gain;
#endif
	initAlsData.time_us = ccbCtx->initData.configData.uSecTime;
	initAlsData.calibration.adcMaxCount = ccbCtx->initData.calibrationData.adcMaxCount;
	initAlsData.calibration.calibrationFactor = ccbCtx->initData.calibrationData.calibrationFactor;
	initAlsData.calibration.Time_base = ccbCtx->initData.calibrationData.Time_base;
	initAlsData.calibration.thresholdLow = ccbCtx->initData.calibrationData.thresholdLow;
	initAlsData.calibration.thresholdHigh = ccbCtx->initData.calibrationData.thresholdHigh;
	//initAlsData.calibration.calibrationFactor = ccbCtx->initData.calibrationData.calibrationFactor;
	amsAlg_als_getAlgInfo(&infoAls);

//#ifdef CONFIG_AMS_OPTICAL_SENSOR_3407
	/* get gain from HW register if so configured */
	if (ctx->ccbAlsCtx.initData.autoGain) {
		uint32_t scaledGain;
		uint8_t gain;

		AMS_GET_ALS_GAIN(scaledGain, gain, ret);
		if (ret < 0) {
			SENSOR_ERR("failed to AMS_GET_ALS_GAIN(err %d)\n", ret);
			return ret;
		}
		ctx->ccbAlsCtx.ctxAlgAls.gain = scaledGain;
	}
//#endif
	amsAlg_als_initAlg(&ccbCtx->ctxAlgAls, &initAlsData);

	AMS_SET_ALS_TIME(ccbCtx->initData.configData.uSecTime, ret);
	if (ret < 0) {
		SENSOR_ERR("failed to AMS_SET_ALS_TIME(err %d)\n", ret);
		return ret;
	}
	AMS_SET_ALS_PERS(0x01, ret);
	if (ret < 0) {
		SENSOR_ERR("failed to AMS_SET_ALS_PERS(err %d)\n", ret);
		return ret;
	}
	AMS_SET_ALS_GAIN(ctx->ccbAlsCtx.initData.configData.gain, ret);
	if (ret < 0) {
		SENSOR_ERR("failed to AMS_SET_ALS_GAIN(err %d)\n", ret);
		return ret;
	}

	ccbCtx->shadowAiltReg = 0xffff;
	ccbCtx->shadowAihtReg = 0;
	AMS_SET_ALS_THRS_LOW(ccbCtx->shadowAiltReg, ret);
	if (ret < 0) {
		SENSOR_ERR("failed to AMS_SET_ALS_THRS_LOW(err %d)\n", ret);
		return ret;
	}
	AMS_SET_ALS_THRS_HIGH(ccbCtx->shadowAihtReg, ret);
	if (ret < 0) {
		SENSOR_ERR("failed to AMS_SET_ALS_THRS_HIGH(err %d)\n", ret);
		return ret;
	}

	ccbCtx->state = AMS_CCB_ALS_RGB;

	return ret;
}

static void ccb_alsInfo(ams_ccb_als_info_t *infoData)
{
	if (infoData != NULL) {
		infoData->algName = "ALS";
		infoData->contextMemSize = sizeof(ams_ccb_als_ctx_t);
		infoData->scratchMemSize = 0;
		infoData->defaultCalibrationData.calibrationFactor = 1000;
		//infoData->defaultCalibrationData.luxTarget = CONFIG_ALS_CAL_TARGET;
		//infoData->defaultCalibrationData.luxTargetError = CONFIG_ALS_CAL_TARGET_TOLERANCE;
		als_getDefaultCalibrationData(&infoData->defaultCalibrationData);
	}
}

static void ccb_alsSetConfig(struct tcs3407_data *data, ams_ccb_als_config_t *configData)
{
	ams_ccb_als_ctx_t *ccbCtx = &data->ccbAlsCtx;

	ccbCtx->initData.configData.threshold = configData->threshold;
}


static int ccb_alsHandle(struct tcs3407_data *ctx, ams_ccb_als_dataSet_t *alsData)
{
	ams_ccb_als_ctx_t *ccbCtx = &ctx->ccbAlsCtx;
	amsAlsDataSet_t inputDataAls;
	static adcDataSet_t adcData; /* QC - is this really needed? */
	int ret = 0;
/*
 *	uint64_t temp;
 *	uint32_t recommendedGain;
 *	uint32_t adcObjective;
 *
 *	adcDataSet_t tmpAdcData;
 *	amsAlsContext_t tmp;
 *	amsAlsDataSet_t tmpInputDataAls;
 */
	//ALS_info(AMS_DEBUG, "ccb_alsHandle: case = %d\n", ccbCtx->state);
	/* get gain from HW register if so configured */
	if (ctx->ccbAlsCtx.initData.autoGain) {
		uint32_t scaledGain;

		uint8_t gain;

		AMS_GET_ALS_GAIN(scaledGain, gain, ret);
		if (ret < 0) {
			SENSOR_ERR("failed at AMS_GET_ALS_GAIN(err: %d)\n", ret);
			return ret;
		}
		ctx->ccbAlsCtx.ctxAlgAls.gain = scaledGain;
	}

	switch (ccbCtx->state) {
	case AMS_CCB_ALS_INIT:
		AMS_DISABLE_ALS(ret);
		if (ret < 0) {
			SENSOR_ERR("failed at AMS_DISABLE_ALS(err: %d)\n", ret);
			return ret;
		}
		AMS_SET_ALS_TIME(ccbCtx->initData.configData.uSecTime, ret);
		if (ret < 0) {
			SENSOR_ERR("failed at AMS_SET_ALS_TIME(err: %d)\n", ret);
			return ret;
		}
		AMS_SET_ALS_PERS(0x01, ret);
		if (ret < 0) {
			SENSOR_ERR("failed at AMS_SET_ALS_PERS(err: %d)\n", ret);
			return ret;
		}

		AMS_SET_ALS_GAIN(ctx->ccbAlsCtx.initData.configData.gain, ret);
		if (ret < 0) {
			SENSOR_ERR("failed at AMS_SET_ALS_GAIN(err: %d)\n", ret);
			return ret;
		}
		/* force interrupt */
		ccbCtx->shadowAiltReg = 0xffff;
		ccbCtx->shadowAihtReg = 0;
		AMS_SET_ALS_THRS_LOW(ccbCtx->shadowAiltReg, ret);
		if (ret < 0) {
			SENSOR_ERR("failed at AMS_SET_ALS_THRS_LOW(err: %d)\n", ret);
			return ret;
		}
		AMS_SET_ALS_THRS_HIGH(ccbCtx->shadowAihtReg, ret);
		if (ret < 0) {
			SENSOR_ERR("failed at AMS_SET_ALS_THRS_HIGH(err: %d)\n", ret);
			return ret;
		}
		ccbCtx->state = AMS_CCB_ALS_RGB;
		AMS_REENABLE_ALS(ret);
		if (ret < 0) {
			SENSOR_ERR("failed at AMS_REENABLE_ALS(err: %d)\n", ret);
			return ret;
		}
		break;
	case AMS_CCB_ALS_RGB: /* state to measure RGB */
#ifdef HAVE_OPTION__ALWAYS_READ
		if ((alsData->statusReg & (AINT)) || ctx->alwaysReadAls)
#else
		if (alsData->statusReg & (AINT))
#endif
		{
			AMS_ALS_GET_CRGB_W(&adcData, ret);
			if (ret < 0) {
				SENSOR_ERR("failed at AMS_ALS_GET_CRGB_W(err: %d)\n", ret);
				return ret;
			}
			inputDataAls.status = ALS_STATUS_RDY;
			inputDataAls.datasetArray = (alsData_t *)&adcData;
			AMS_PORT_LOG_CRGB_W(adcData);

			amsAlg_als_processData(&ctx->ccbAlsCtx.ctxAlgAls, &inputDataAls);

			if (ctx->mode & MODE_ALS_LUX)
				ctx->updateAvailable |= (1 << AMS_AMBIENT_SENSOR);

			ccbCtx->state = AMS_CCB_ALS_RGB;
		}
		break;

	default:
		ccbCtx->state = AMS_CCB_ALS_RGB;
		break;
	}
	return false;
}

static void ccb_alsGetResult(struct tcs3407_data *data, ams_ccb_als_result_t *exportData)
{
	ams_ccb_als_ctx_t *ccbCtx = &data->ccbAlsCtx;

	/* export data */
	exportData->mLux = ccbCtx->ctxAlgAls.results.mLux;
	exportData->clear = ccbCtx->ctxAlgAls.results.irrClear;
	exportData->red = ccbCtx->ctxAlgAls.results.irrRed;
	exportData->green = ccbCtx->ctxAlgAls.results.irrGreen;
	exportData->blue = ccbCtx->ctxAlgAls.results.irrBlue;
	exportData->ir = ccbCtx->ctxAlgAls.results.irrWideband;
	exportData->time_us = ccbCtx->ctxAlgAls.time_us;
	exportData->gain = ccbCtx->ctxAlgAls.gain;
	exportData->rawClear = ccbCtx->ctxAlgAls.results.rawClear;
	exportData->rawRed = ccbCtx->ctxAlgAls.results.rawRed;
	exportData->rawGreen = ccbCtx->ctxAlgAls.results.rawGreen;
	exportData->rawBlue = ccbCtx->ctxAlgAls.results.rawBlue;
	exportData->rawWideband = ccbCtx->ctxAlgAls.results.rawWideband;
}

#ifdef CONFIG_AMS_OPTICAL_SENSOR_ALS_CCB
static bool _3407_alsSetThreshold(struct tcs3407_data *ctx, int32_t threshold)
{
	ams_ccb_als_config_t configData;

	configData.threshold = threshold;
	ccb_alsSetConfig(ctx, &configData);

	return false;
}
#endif


static int ams_deviceSetConfig(struct tcs3407_data *ctx, ams_configureFeature_t feature, deviceConfigOptions_t option, uint32_t data)
{
	int ret = 0;

	if (feature == AMS_CONFIG_ALS_LUX) {
		SENSOR_INFO("AMS_CONFIG_ALS_LUX\n");
		switch (option)	{
			case AMS_CONFIG_ENABLE: /* ON / OFF */
			SENSOR_INFO("AMS_CONFIG_ENABLE(%u), mode: %d\n",data,ctx->mode);
			if (data == 0) {
				if (ctx->mode == MODE_ALS_LUX) {
					/* if no other active features, turn off device */
					ctx->shadowEnableReg = 0;
					ctx->shadowIntenabReg = 0;
					ctx->mode = MODE_OFF;
				} else {
					if ((ctx->mode & MODE_ALS_ALL) == MODE_ALS_LUX) {
						ctx->shadowEnableReg &= ~MASK_AEN;
						ctx->shadowIntenabReg &= ~MASK_ALS_INT_ALL;
					}
					ctx->mode &= ~(MODE_ALS_LUX);
				}
			} else {
				if ((ctx->mode & MODE_ALS_ALL) == 0) {
					ret = ccb_alsInit(ctx, &ctx->ccbAlsCtx.initData);
					if (ret < 0) {
						SENSOR_ERR("failed at ccb_alsInit(err %d)\n", ret);
						return ret;
					}
					ctx->shadowEnableReg |= (AEN | PON);
					ctx->shadowIntenabReg |= AIEN;
				} else {
					/* force interrupt */
					ret = ams_setWord(ctx->client, DEVREG_AIHTL, 0x00);
					if (ret < 0) {
						SENSOR_ERR("failed at DEVREG_AIHTL(err %d)\n", ret);
						return ret;
					}
				}
				ctx->mode |= MODE_ALS_LUX;
			}
			break;
		case AMS_CONFIG_THRESHOLD: /* set threshold */
			SENSOR_INFO("AMS_CONFIG_THRESHOLD, %d\n", data);
			_3407_alsSetThreshold(ctx, data);
			break;
		default:
			break;
		}
	}

	ret = ams_setByte(ctx->client, DEVREG_ENABLE, ctx->shadowEnableReg);
	if (ret < 0) {
		SENSOR_ERR("failed at DEVREG_ENABLE(err %d)\n", ret);
		return ret;
	}
	ret = ams_setByte(ctx->client, DEVREG_INTENAB, ctx->shadowIntenabReg);
	if (ret < 0) {
		SENSOR_ERR("failed at DEVREG_INTENAB(err %d)\n", ret);
		return ret;
	}

	return 0;
}

#define STAR_ATIME  50 //50 msec
#define STAR_D_FACTOR  2266

static void als_update_statics(amsAlsContext_t *ctx)
{
	uint64_t tempCpl;
	uint64_t tempTime_us = ctx->time_us;
	uint64_t tempGain = ctx->gain;

	/* test for the potential of overflowing */
	uint32_t maxOverFlow;
#ifdef __KERNEL__
	u64 tmpTerm1;
	u64 tmpTerm2;
#endif
#ifdef __KERNEL__
	u64 tmp = ULLONG_MAX;

	do_div(tmp, ctx->time_us);

	maxOverFlow = (uint32_t)tmp;
#else
	maxOverFlow = (uint64_t)ULLONG_MAX / ctx->time_us;
#endif

	if (maxOverFlow < ctx->gain) {
		/* TODO: need to find use-case to test */
#ifdef __KERNEL__
		tmpTerm1 = tempTime_us;
		do_div(tmpTerm1, 2);
		tmpTerm2 = tempGain;
		do_div(tmpTerm2, 2);
		tempCpl = tmpTerm1 * tmpTerm2;
		do_div(tempCpl, (AMS_ALS_GAIN_FACTOR/4));
#else
		tempCpl = ((tempTime_us / 2) * (tempGain / 2)) / (AMS_ALS_GAIN_FACTOR/4);
#endif

	} else {
#ifdef __KERNEL__
		tempCpl = (tempTime_us * tempGain);
		do_div(tempCpl, AMS_ALS_GAIN_FACTOR);
#else
		tempCpl = (tempTime_us * tempGain) / AMS_ALS_GAIN_FACTOR;
#endif
	}
	if (tempCpl > (uint32_t)ULONG_MAX) {
		/* if we get here, we have a problem */
		//AMS_PORT_log_Msg_1(AMS_ERROR, "als_update_statics: overflow, setting cpl=%u\n", (uint32_t)ULONG_MAX);
		tempCpl = (uint32_t)ULONG_MAX;
	}

#ifdef __KERNEL__
    tmpTerm1 = tempCpl;
    do_div(tmpTerm1, AMS_ALS_D_FACTOR);
    ctx->cpl = (uint32_t)tmpTerm1;
#else
    ctx->cpl = tempCpl / AMS_ALS_D_FACTOR;
#endif

    {
        uint32_t max_count = ((ctx->calibration.adcMaxCount) * ctx->time_us) / ctx->calibration.Time_base;
        if(max_count > (uint32_t)ULONG_MAX) {
            ctx->saturation = (uint16_t)USHRT_MAX;
        } else {
            ctx->saturation = (uint16_t)max_count; /* TODO: need to validate more all devices */
        }
    }
    ctx->previousGain = ctx->gain;
    {
        /* just changed settings, lux readings could be jumpy */
        ctx->notStableMeasurement = true;
    }
    SENSOR_INFO("time=%d, gain=%d, cpl=%u\n", ctx->time_us, ctx->gain, ctx->cpl);
}

static int amsAlg_als_setConfig(amsAlsContext_t *ctx, amsAlsConf_t *inputData)
{
	int ret = 0;

	if (inputData != NULL) {
		ctx->gain = inputData->gain;
		ctx->time_us = inputData->time_us;
	}
	//als_update_statics(ctx);

	return ret;
}

/*
 * getConfig: is used to quarry the algorithm's configuration
 */
static int amsAlg_als_getConfig(amsAlsContext_t *ctx, amsAlsConf_t *outputData)
{
	int ret = 0;

	outputData->gain = ctx->gain;
	outputData->time_us = ctx->time_us;

	return ret;
}

static int amsAlg_als_getResult(amsAlsContext_t *ctx, amsAlsResult_t *outData)
{
	int ret = 0;

	outData->rawClear  = ctx->results.rawClear;
	outData->rawRed  = ctx->results.rawRed;
	outData->rawGreen  = ctx->results.rawGreen;
	outData->rawBlue  = ctx->results.rawBlue;
	outData->irrBlue  = ctx->results.irrBlue;
	outData->irrClear = ctx->results.irrClear;
	outData->irrGreen = ctx->results.irrGreen;
	outData->irrRed   = ctx->results.irrRed;
	outData->irrWideband = ctx->results.irrWideband;
	outData->mLux_ave  = ctx->results.mLux_ave / AMS_LUX_AVERAGE_COUNT;
	outData->IR  = ctx->results.IR;
	outData->CCT = ctx->results.CCT;
	outData->adaptive = ctx->results.adaptive;

	if (ctx->notStableMeasurement)
		ctx->notStableMeasurement = false;

	outData->mLux = ctx->results.mLux;

	return ret;
}

static int amsAlg_als_initAlg(amsAlsContext_t *ctx, amsAlsInitData_t *initData)
{
	int ret = 0;

	memset(ctx, 0, sizeof(amsAlsContext_t));

	if (initData != NULL) {
		ctx->calibration.Time_base = initData->calibration.Time_base;
		ctx->calibration.thresholdLow = initData->calibration.thresholdLow;
		ctx->calibration.thresholdHigh = initData->calibration.thresholdHigh;
		ctx->calibration.calibrationFactor = initData->calibration.calibrationFactor;
	}

	if (initData != NULL) {
		ctx->gain = initData->gain;
		ctx->time_us = initData->time_us;
		ctx->adaptive = initData->adaptive;
	}

	als_update_statics(ctx);
	return ret;
}

static int amsAlg_als_getAlgInfo(amsAlsAlgoInfo_t *info)
{
	int ret = 0;

	info->algName = "AMS_ALS";
	info->contextMemSize = sizeof(amsAlsContext_t);
	info->scratchMemSize = 0;

	info->initAlg = &amsAlg_als_initAlg;
	info->processData = &amsAlg_als_processData;
	info->getResult = &amsAlg_als_getResult;
	info->setConfig = &amsAlg_als_setConfig;
	info->getConfig = &amsAlg_als_getConfig;

	return ret;
}

static int ams_smux_set(struct tcs3407_data *ctx)
{
    int ret = 0;

    /* 1. PON */
    AMS_PON(ret);
    if (ret < 0) {
        SENSOR_ERR("%s - failed to AMS_PON\n", __func__);
        return ret;
    }

    /* 2. CFG6(0xAF) => 0x10 :  write smuxconfiguration from RAM to smux chain*/
    AMS_WRITE_S_MUX(ret);
    if (ret < 0) {
        SENSOR_ERR("%s - failed to AMS_WRITE_S_MUX\n", __func__);
        return ret;
    }

    /* 3. Write s-mux configuration data to RAM*/
    // smux_default_data  : Default RGBC configuration
    // smux_custom_data_43
    // smux_custom_data_42
    ret = ams_setBuf(ctx->client, DEVREG_RAM_START, &smux_custom_data_43[0], sizeof(smux_custom_data_43));

    if (ret < 0) {
        SENSOR_ERR("%s - failed to write s-mux to ram \n", __func__);
        return ret;
    }
    /* 4. Execute s-mux */
    AMS_SMUX_EN(ret);
    if (ret < 0) {
        SENSOR_ERR("%s - failed to AMS_SMUX_EN \n", __func__);
        return ret;
    }

    /* 5. Delay 1msec  */
    udelay(200); //Now 0x80 needs to be read back until SMUXEN has been cleared , wait 200 us

    /* 6. CFG6(0xAF) => 0x00 :  ROM code initialization of smux */
    AMS_CLOSE_S_MUX(ret);
    if (ret < 0) {
        SENSOR_ERR("%s - failed to AMS_CLOSE_S_MUX \n", __func__);
        return ret;
    }
    /* 7. Recover enalbe register */
    AMS_REENABLE(ret);
    if (ret < 0) {
        SENSOR_ERR("%s - failed to AMS_REENABLE \n", __func__);
        return ret;
    }
    return ret;
}

static bool ams_deviceGetAls(struct tcs3407_data *ctx, ams_apiAls_t *exportData);


static ssize_t als_raw_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ams_apiAls_t outData;
	struct tcs3407_data *chip = dev_get_drvdata(dev);

	ams_deviceGetAls(chip, &outData);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,(%u)\n",
		outData.rawRed, outData.rawGreen, outData.rawBlue,
		outData.rawClear, outData.gain, outData.time_us);
}

static ssize_t als_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ams_apiAls_t outData;
	struct tcs3407_data *chip = dev_get_drvdata(dev);

	ams_deviceGetAls(chip, &outData);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d\n",
		outData.red, outData.green, outData.blue,
		outData.clear, outData.time_us, outData.gain);
}


static size_t als_enable_set(struct tcs3407_data *chip, uint8_t valueToSet)
{
	int rc = 0;

#ifdef CONFIG_AMS_OPTICAL_SENSOR_ALS_CCB
	rc = ams_deviceSetConfig(chip, AMS_CONFIG_ALS_LUX, AMS_CONFIG_ENABLE, valueToSet);
	if (rc < 0) {
		SENSOR_ERR("ams_deviceSetConfig ALS_LUX fail, rc=%d\n",  rc);
		return rc;
	}
	SENSOR_INFO("ams_deviceSetConfig ALS_LUX  set\n");
#endif
	return 0;
}

/* als input enable/disable sysfs */
static ssize_t tcs3407_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcs3407_data *data = dev_get_drvdata(dev);
	SENSOR_INFO("%d\n", data->enabled);
	return snprintf(buf, strlen(buf), "%d\n", data->enabled);
}

static int ams_deviceInit(struct tcs3407_data *ctx, AMS_PORT_portHndl *portHndl, ams_calibrationData_t *calibrationData);

static void tcs3407_als_enable(struct tcs3407_data *data)
{
	int ret = 0;
	SENSOR_INFO("\n");

	mutex_lock(&data->activelock);
	ret = ams_deviceInit(data, data->client, NULL);
	if (ret  < 0) {
		mutex_unlock(&data->activelock);
		return;
	}
	
	ret = als_enable_set(data, AMSDRIVER_ALS_ENABLE);
	if (ret  < 0) {
		mutex_unlock(&data->activelock);
		return;
	}
	// TIMER START 
	hrtimer_start(&data->timer, data->light_poll_delay, HRTIMER_MODE_REL);  
	data->mode_cnt.amb_cnt++;
	mutex_unlock(&data->activelock);
}

static void tcs3407_als_disable(struct tcs3407_data *data)
{
	int ret = 0;
	SENSOR_INFO("\n");


	mutex_lock(&data->activelock);
	ret = als_enable_set(data, AMSDRIVER_ALS_DISABLE);	
	if (ret < 0)
		SENSOR_ERR("fail AMSDRIVER_ALS_DISABLE= %d\n", ret);
	data->mode_cnt.amb_cnt--;
	hrtimer_cancel(&data->timer);
	cancel_work_sync(&data->work_light);
	mutex_unlock(&data->activelock);
	
}


static ssize_t tcs3407_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcs3407_data *data = dev_get_drvdata(dev);
	u8 enable;
	int ret = 0;
	
	ret = kstrtou8(buf, 0, &enable);
	if (ret) {
		SENSOR_ERR("Invalid Argument %d\n", ret);
		return ret;
	}

	SENSOR_INFO("cur: %d, new: %d\n", data->enabled, enable);
	
	
	if (enable && !data->enabled)
		tcs3407_als_enable(data);
	else if (!enable && data->enabled)
		tcs3407_als_disable(data);

	mutex_lock(&data->activelock);
	data->enabled = enable;
	mutex_unlock(&data->activelock);

	return count;
}

static ssize_t tcs3407_poll_delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcs3407_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->delay);
}

static ssize_t tcs3407_poll_delay_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct tcs3407_data *data = dev_get_drvdata(dev);
	int sampling_period_ns = 0;
	int ret = 0;
	
	ret = kstrtoint(buf, 10, &sampling_period_ns);
	if (ret) {
		SENSOR_ERR("Invalid Argument %d\n", ret);
		return ret;
	}
	SENSOR_INFO("set: %d ns\n", sampling_period_ns);
	data->delay = sampling_period_ns;
	data->light_poll_delay = ns_to_ktime(sampling_period_ns);
	return size;
}

static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
	tcs3407_enable_show, tcs3407_enable_store);
static DEVICE_ATTR(poll_delay, S_IRUGO|S_IWUSR|S_IWGRP,
	tcs3407_poll_delay_show, tcs3407_poll_delay_store);

static struct attribute *als_sysfs_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static struct attribute_group als_attribute_group = {
	.attrs = als_sysfs_attrs,
};

/* als_sensor sysfs */
static ssize_t tcs3407_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcs3407_data *data = dev_get_drvdata(dev);
	char chip_name[NAME_LEN];

	switch (data->device_id) {
	case AMS_TCS3407:
	case AMS_TCS3407_UNTRIM:
		strlcpy(chip_name, TCS3407_CHIP_NAME, strlen(TCS3407_CHIP_NAME) + 1);
		break;
	case AMS_TCS3408:
	case AMS_TCS3408_UNTRIM:
		strlcpy(chip_name, TCS3408_CHIP_NAME, strlen(TCS3408_CHIP_NAME) + 1);
		break;
	default:
		strlcpy(chip_name, TCS3407_CHIP_NAME, strlen(TCS3407_CHIP_NAME) + 1);
		break;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", chip_name);
}

static ssize_t tcs3407_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char vendor[NAME_LEN];

	strlcpy(vendor, VENDOR, strlen(VENDOR) + 1);

	return snprintf(buf, PAGE_SIZE, "%s\n", vendor);
}

static ssize_t tcs3407_reg_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcs3407_data *chip = dev_get_drvdata(dev);
	u8 reg = 0;
	int offset = 0;
	u8 val = 0;

	for (reg = 0; reg < DEVREG_REG_MAX; reg++) {
		tcs3407_read_reg(chip->client, deviceRegisterDefinition[reg].address, &val, 1);
		SENSOR_INFO("[0x%2x 0x%2x]\n", reg, val);
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"[0x%2x 0x%2x]\n", reg, val);
	}

	return offset;
}


static ssize_t tcs3407_reg_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcs3407_data *chip = dev_get_drvdata(dev);
	int reg, val, ret;

	if (sscanf(buf, "%2x,%4x", &reg, &val) != 2) {
		SENSOR_ERR("invalid value\n");
		return count;
	}

	ret = tcs3407_write_reg(chip->client, (u8)reg, (u8)val);
	if (!ret)
		SENSOR_INFO("Register(0x%2x) data(0x%4x)\n", reg, val);
	else
		SENSOR_ERR("failed %d\n", ret);

	return count;
}


static DEVICE_ATTR(name, S_IRUGO, tcs3407_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, tcs3407_vendor_show, NULL);
static DEVICE_ATTR(reg_data, S_IRUGO,
		tcs3407_reg_data_show, tcs3407_reg_data_store);
static DEVICE_ATTR(lux, S_IRUGO, als_lux_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, als_raw_data_show, NULL);


static struct device_attribute *tcs3407_sensor_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_reg_data,
	&dev_attr_lux,
	&dev_attr_raw_data,
	NULL,
};

static int _3407_handleAlsEvent(struct tcs3407_data *ctx);

/*
#define EVENT_TYPE_LIGHT		EV_REL
161	#define EVENT_TYPE_LIGHT_R		REL_HWHEEL
162	#define EVENT_TYPE_LIGHT_G		REL_DIAL
163	#define EVENT_TYPE_LIGHT_B		REL_WHEEL
164	#define EVENT_TYPE_LIGHT_W		REL_MISC
165	#define EVENT_TYPE_LIGHT_C              EVENT_TYPE_LIGHT_W
166	#define EVENT_TYPE_LIGHT_TIME_HI        REL_X
167	#define EVENT_TYPE_LIGHT_TIME_LO        REL_Y
168	#define EVENT_TYPE_LIGHT_GAIN           REL_Z
169	#define EVENT_TYPE_LIGHT_IR             EVENT_TYPE_LIGHT_GAIN
170	
171	#define EVENT_TYPE_LIGHT_DELTA          REL_DIAL
172	#define EVENT_TYPE_LIGHT_2ND_MIN        REL_HWHEEL
173	#define EVENT_TYPE_LIGHT_2ND_MAX        REL_WHEEL
174	
175	#define EVENT_TYPE_LIGHT_LUX		REL_MISC
176	#define EVENT_TYPE_LIGHT_CCT		REL_WHEEL

177	
178	#define EVENT_TYPE_LIGHT_ALSDATA	REL_DIAL
179	#define EVENT_TYPE_LIGHT_CDATA		REL_WHEEL*/

static void tcs3407_report(struct tcs3407_data *chip)
{
	ams_apiAls_t outData;
	struct timespec ts = ktime_to_timespec(ktime_get_boottime());
	u64 timestamp_new = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
	int time_hi, time_lo;

	time_hi = (int)((timestamp_new & TIME_HI_MASK) >> TIME_HI_SHIFT);
	time_lo = (int)(timestamp_new & TIME_LO_MASK);

	if (chip->input_dev) {
		ams_deviceGetAls(chip, &outData);
        SENSOR_INFO("R:%d, G:%d, B:%d, C:%d TIME:%d, GAIN:%d, LUX: %d\n",
              outData.rawRed, outData.rawGreen, outData.rawBlue, outData.rawClear, outData.time_us, outData.gain, outData.mLux);

		input_report_rel(chip->input_dev, REL_X, time_hi);
		input_report_rel(chip->input_dev, REL_Y, time_lo);
		input_report_rel(chip->input_dev, REL_MISC, outData.mLux);
		input_report_rel(chip->input_dev, REL_WHEEL, outData.mLux);
		input_sync(chip->input_dev);

		chip->user_ir_data = outData.ir;
	}
}

static int tcs3407_deviceEventHandler(struct tcs3407_data *data)
{
	int ret = 0;
	uint8_t status5 = 0;

	ret = ams_getByte(data->client, DEVREG_STATUS, &data->shadowStatus1Reg);
	if (ret < 0) {
		SENSOR_ERR("failed at DEVREG_STATUS(err: %d)\n", ret);
		return ret;
	}

	if (data->shadowStatus1Reg & SINT) {
		ret = ams_getByte(data->client, DEVREG_STATUS5, &status5);
		if (ret < 0) {
			SENSOR_ERR("failed at DEVREG_STATUS5(err: %d)\n", ret);
			return ret;
		}
		SENSOR_INFO("data->shadowStatus1Reg %x, status5 %x, mode %x", data->shadowStatus1Reg, status5, data->mode);
	}

	if (data->shadowStatus1Reg != 0) {
		/* this clears interrupt(s) and STATUS5 */
		ret = ams_setByte(data->client, DEVREG_STATUS, data->shadowStatus1Reg);
		if (ret < 0) {
			SENSOR_ERR("failed at DEVREG_STATUS(err: %d)\n", ret);
			return ret;
		}
	} else {
		SENSOR_ERR("failed at DEVREG_STATUS(err: %d)\n", data->shadowStatus1Reg);
		return ret;
	}

loop:
	//SENSOR_INFO("DCB 0x%02x, STATUS 0x%02x, STATUS5 0x%02x\n", data->mode, data->shadowStatus1Reg, status5);

	if ((data->shadowStatus1Reg & ALS_INT_ALL) /*|| data->alwaysReadAls*/) {
		if ((data->mode & MODE_ALS_ALL) && (!(data->mode & MODE_IRBEAM))) {
			ret = _3407_handleAlsEvent(data);
			if (ret < 0) {
				SENSOR_ERR("failed at D_3407_handleAlsEvent\n");
				return ret;
			}
		}
	}

	ret = ams_getByte(data->client, DEVREG_STATUS, &data->shadowStatus1Reg);
	if (ret < 0) {
		SENSOR_ERR("failed at DEVREG_STATUS(err: %d)\n", ret);
		return ret;
	}

	if (data->shadowStatus1Reg & SINT) {
		ret = ams_getByte(data->client, DEVREG_STATUS5, &status5);
		if (ret < 0) {
			SENSOR_ERR("failed at DEVREG_STATUS5(err: %d)\n", ret);
			return ret;
		}
	} else {
		status5 = 0;
	}

	if (data->shadowStatus1Reg != 0) {
		/* this clears interrupt(s) and STATUS5 */
		ret = ams_setByte(data->client, DEVREG_STATUS, data->shadowStatus1Reg);
		if (ret < 0) {
			SENSOR_ERR("failed at DEVREG_STATUS(err: %d)\n", ret);
			return ret;
		}
#ifdef CONFIG_AMS_OPTICAL_SENSOR_FIFO
		if (!(data->shadowStatus1Reg & (PSAT)))/*changed to update event data during saturation*/
#else
		if (!(data->shadowStatus1Reg & (ASAT_FDSAT | PSAT)))/*changed to update event data during saturation*/
#endif
		{
			// AMS_PORT_log_1( "ams_devEventHd loop:go loop shadowStatus1Reg %x!!!!!!!!!!\n",data->shadowStatus1Reg);
			SENSOR_ERR("go_loop DCB 0x%02x, STATUS 0x%02x, STATUS2 0x%02x\n",
			data->mode, data->shadowStatus1Reg, data->shadowStatus2Reg);

			goto loop;
		}
	}

/*
 *	the individual handlers may have temporarily disabled things
 *	AMS_REENABLE(ret);
 *	if (ret < 0) {
 *		ALS_err("%s - failed to AMS_REENABLE\n", __func__);
 *		return ret;
 *	}
 */
	return ret;
}

static void tcs3407_work_func_light(struct work_struct *work)
{
	int err;
	int pollingHandled = 0;

	struct tcs3407_data *data
		= container_of(work, struct tcs3407_data, work_light);

	if (data->enabled == 0) {
		SENSOR_INFO("(enabled : 0)\n");

		ams_setByte(data->client, DEVREG_STATUS, (AINT | ASAT_FDSAT));
		return;
	}

	err = tcs3407_deviceEventHandler(data);
	pollingHandled = ams_getResult(data);

	if (err == 0) {
#ifdef CONFIG_AMS_OPTICAL_SENSOR_ALS
		if (pollingHandled & (1 << AMS_AMBIENT_SENSOR))
				tcs3407_report(data);
#endif

	} else {
		SENSOR_ERR("ams_deviceEventHandler failed\n");
	}
}

static enum hrtimer_restart tcs3407_timer_func(struct hrtimer *timer)
{
	struct tcs3407_data *data = container_of(timer, struct tcs3407_data, timer);
	queue_work(data->wq, &data->work_light);
	hrtimer_forward_now(&data->timer, data->light_poll_delay);
	return HRTIMER_RESTART;
}


static int tcs3407_parse_dt(struct tcs3407_data *data)
{
	struct device *dev = &data->client->dev;
	struct device_node *dNode = dev->of_node;


	if (dNode == NULL)
		return -ENODEV;

	return 0;
}

static int _3407_resetAllRegisters(struct i2c_client *client)
{
	int err = 0;
/*
 *	ams_deviceRegister_t i;
 *
 *	for (i = DEVREG_ENABLE; i <= DEVREG_CFG1; i++) {
 *		ams_setByte(portHndl, i, deviceRegisterDefinition[i].resetValue);
 *	}
 *	for (i = DEVREG_STATUS; i < DEVREG_REG_MAX; i++) {
 *		ams_setByte(portHndl, i, deviceRegisterDefinition[i].resetValue);
 *	}
 */
	// To prevent SIDE EFFECT , below register should be written
	err = ams_setByte(client, DEVREG_CFG6, deviceRegisterDefinition[DEVREG_CFG6].resetValue);
	if (err < 0) {
		SENSOR_ERR("DEVREG_CFG6(err %d)\n",err);
		return err;
	}
	err = ams_setByte(client, DEVREG_AGC_GAIN_MAX, deviceRegisterDefinition[DEVREG_AGC_GAIN_MAX].resetValue);
	if (err < 0) {
		SENSOR_ERR("DEVREG_AGC_GAIN_MAX(err %d)\n",err);
		return err;
	}

	err = ams_setByte(client, DEVREG_FD_CFG3, deviceRegisterDefinition[DEVREG_FD_CFG3].resetValue);
	if (err < 0) {
		SENSOR_ERR("DEVREG_FD_CFG3(err %d)\n",err);
		return err;
	}

	return err;
}

#ifdef CONFIG_AMS_OPTICAL_SENSOR_ALS_CCB
static int _3407_alsInit(struct tcs3407_data *ctx, ams_calibrationData_t *calibrationData)
{
	int ret = 0;

	if (calibrationData == NULL) {
		ams_ccb_als_info_t infoData;

		SENSOR_INFO("calibrationData is null\n");
		ccb_alsInfo(&infoData);
	   // ctx->ccbAlsCtx.initData.calibrationData.luxTarget = infoData.defaultCalibrationData.luxTarget;
	   // ctx->ccbAlsCtx.initData.calibrationData.luxTargetError = infoData.defaultCalibrationData.luxTargetError;
		ctx->ccbAlsCtx.initData.calibrationData.calibrationFactor = infoData.defaultCalibrationData.calibrationFactor;
		ctx->ccbAlsCtx.initData.calibrationData.Time_base = infoData.defaultCalibrationData.Time_base;
		ctx->ccbAlsCtx.initData.calibrationData.thresholdLow = infoData.defaultCalibrationData.thresholdLow;
		ctx->ccbAlsCtx.initData.calibrationData.thresholdHigh = infoData.defaultCalibrationData.thresholdHigh;
		ctx->ccbAlsCtx.initData.calibrationData.calibrationFactor = infoData.defaultCalibrationData.calibrationFactor;
	} else {
		SENSOR_INFO("calibrationData is non-null\n");
		//ctx->ccbAlsCtx.initData.calibrationData.luxTarget = calibrationData->alsCalibrationLuxTarget;
		//ctx->ccbAlsCtx.initData.calibrationData.luxTargetError = calibrationData->alsCalibrationLuxTargetError;
		ctx->ccbAlsCtx.initData.calibrationData.calibrationFactor = calibrationData->alsCalibrationFactor;
		ctx->ccbAlsCtx.initData.calibrationData.Time_base = calibrationData->timeBase_us;
		ctx->ccbAlsCtx.initData.calibrationData.thresholdLow = calibrationData->alsThresholdLow;
		ctx->ccbAlsCtx.initData.calibrationData.thresholdHigh = calibrationData->alsThresholdHigh;
		//ctx->ccbAlsCtx.initData.calibrationData.calibrationFactor = calibrationData->alsCalibrationFactor;
	}
	ctx->ccbAlsCtx.initData.calibrate = false;
	ctx->ccbAlsCtx.initData.configData.gain = 64000;//AGAIN
	ctx->ccbAlsCtx.initData.configData.uSecTime = AMS_ALS_ATIME; /*ALS Inegration time 50msec*/

	ctx->alwaysReadAls = false;
	ctx->alwaysReadFlicker = false;
	ctx->ccbAlsCtx.initData.autoGain = true; //AutoGainCtrol on
	ctx->ccbAlsCtx.initData.hysteresis = 0x02; /*Lower threshold for adata in AGC */
	if (ctx->ccbAlsCtx.initData.autoGain) {
		AMS_SET_ALS_AUTOGAIN(HIGH, ret);
		if (ret < 0) {
			SENSOR_ERR("failed to AMS_SET_ALS_AUTOGAIN\n");
			return ret;
		}
/*******************************/
/*
 * - ALS_AGC_LOW_HYST -
 * 0 -> 12.5 %
 * 1 -> 25 %
 * 2 -> 37.5 %
 * 3 -> 50 %
 *
 * - ALS_AGC_HIGH_HYST -
 * 0 -> 50 %
 * 1 -> 62.5 %
 * 2 -> 75 %
 * 3 -> 87.5 %
 */
/*******************************/
		AMS_SET_ALS_AGC_LOW_HYST(0);		// Low HYST -> 12.5 %
		AMS_SET_ALS_AGC_HIGH_HYST(3);		//  High HYST -> 87.5 %
	}
	return ret;
}

static bool ams_deviceGetAls(struct tcs3407_data *ctx, ams_apiAls_t *exportData)
{
	ams_ccb_als_result_t result;

	ccb_alsGetResult(ctx, &result);
	exportData->mLux = result.mLux;
	exportData->clear		= result.clear;
	exportData->red         = result.red;
	exportData->green       = result.green;
	exportData->blue        = result.blue;
	exportData->ir          = result.ir;
	exportData->time_us		= result.time_us;
	exportData->gain		= result.gain;
//	exportData->wideband    = result.wideband;
	exportData->rawClear    = result.rawClear;
	exportData->rawRed      = result.rawRed;
	exportData->rawGreen    = result.rawGreen;
	exportData->rawBlue     = result.rawBlue;
	exportData->rawWideband = result.rawWideband;
	return false;
}

static int _3407_handleAlsEvent(struct tcs3407_data *data)
{
	int ret = 0;
	ams_ccb_als_dataSet_t ccbAlsData;

	ccbAlsData.statusReg = data->shadowStatus1Reg;
	ret = ccb_alsHandle(data, &ccbAlsData);

	return ret;
}

#endif

static int ams_deviceSoftReset(struct tcs3407_data *ctx)
{
	int err = 0;

	SENSOR_INFO("Start\n");

	// Before S/W reset, the PON has to be asserted
	err = ams_setByte(ctx->client, DEVREG_ENABLE, PON);
	if (err < 0) {
		SENSOR_ERR("failed at DEVREG_ENABLE(err %d)\n", err);
		return err;
	}

	err = ams_setField(ctx->client, DEVREG_SOFT_RESET, HIGH, MASK_SOFT_RESET);
	if (err < 0) {
		SENSOR_ERR("failed at DEVREG_SOFT_RESET(err %d)\n", err);
		return err;
	}
	// Need 1 msec delay
	usleep_range(1000, 1100);

	// Recover the previous enable setting
	err = ams_setByte(ctx->client, DEVREG_ENABLE, ctx->shadowEnableReg);
	if (err < 0) {
		SENSOR_ERR("failed at DEVREG_ENABLE(err %d)\n", err);
		return err;
	}

	return err;
}

static int ams_deviceInit(struct tcs3407_data *ctx, AMS_PORT_portHndl *portHndl, ams_calibrationData_t *calibrationData)
{
	int ret = 0;

	ctx->mode = MODE_OFF;
	ctx->systemCalibrationData = calibrationData;
	ctx->shadowEnableReg = deviceRegisterDefinition[DEVREG_ENABLE].resetValue;
	ret = ams_deviceSoftReset(ctx);
	if (ret < 0) {
		SENSOR_ERR("failed at ams_deviceSoftReset(err %d)\n", ret);
		return ret;
	}

	ret = _3407_resetAllRegisters(ctx->client);
	if (ret < 0) {
		SENSOR_ERR("failed at _3407_resetAllRegisters(err %d)\n", ret);
		return ret;
	}

#ifdef CONFIG_AMS_OPTICAL_SENSOR_ALS_CCB
	ret = _3407_alsInit(ctx, calibrationData);
	if (ret < 0) {
		SENSOR_ERR("failed at _3407_alsInit(err %d)\n", ret);
		return ret;
	}
#endif
    ret = ams_smux_set(ctx);

    if (ret < 0) {
		SENSOR_ERR("%s - failed to ams_smux_set\n", __func__);
		return ret;
	}
/*
 *	ret = ams_setByte(ctx->portHndl, DEVREG_ENABLE, ctx->shadowEnableReg);
 *	if (ret < 0) {
 *		ALS_err("%s - failed to set DEVREG_ENABLE\n", __func__);
 *		return ret;
 *	}
 */
	return ret;
}

static void ams_getDeviceInfo(ams_deviceInfo_t *info, ams_deviceIdentifier_e deviceId)
{
	memset(info, 0, sizeof(ams_deviceInfo_t));

	info->defaultCalibrationData.timeBase_us = AMS_USEC_PER_TICK;
	info->numberOfSubSensors = 0;

	switch (deviceId) {
	case AMS_TCS3407:
	case AMS_TCS3407_UNTRIM:
		info->deviceModel = "TCS3407";
		break;
	case AMS_TCS3408:
	case AMS_TCS3408_UNTRIM:
		info->deviceModel = "TCS3408";
		break;
	default:
		info->deviceModel = "UNKNOWN";
		break;
	}

	memcpy(info->defaultCalibrationData.deviceName, info->deviceModel, sizeof(info->defaultCalibrationData.deviceName));
	info->deviceName  = "ALS";
	info->driverVersion = "Alpha";
#ifdef CONFIG_AMS_OPTICAL_SENSOR_ALS_CCB
	{
		/* TODO */
		ams_ccb_als_info_t infoData;

		ccb_alsInfo(&infoData);
		info->tableSubSensors[info->numberOfSubSensors] = AMS_AMBIENT_SENSOR;
		info->numberOfSubSensors++;

		info->alsSensor.driverName = infoData.algName;
		info->alsSensor.adcBits = 8;
		info->alsSensor.maxPolRate = 50;
		info->alsSensor.activeCurrent_uA = 100;
		info->alsSensor.standbyCurrent_uA = 5;
		info->alsSensor.rangeMax = 1;
		info->alsSensor.rangeMin = 0;

		info->defaultCalibrationData.alsCalibrationFactor = infoData.defaultCalibrationData.calibrationFactor;
//		info->defaultCalibrationData.alsCalibrationLuxTarget = infoData.defaultCalibrationData.luxTarget;
//		info->defaultCalibrationData.alsCalibrationLuxTargetError = infoData.defaultCalibrationData.luxTargetError;
#if defined(CONFIG_AMS_ALS_CRWBI) || defined(CONFIG_AMS_ALS_CRGBW)
		info->tableSubSensors[info->numberOfSubSensors] = AMS_WIDEBAND_ALS_SENSOR;
		info->numberOfSubSensors++;
#endif
	}
#endif
}

static int tcs3407_init_input_device(struct tcs3407_data *chip)
{
	int ret = 0;
	struct input_dev *dev;

	/* allocate lightsensor input_device */
	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;

	dev->name = MODULE_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_REL, REL_MISC);  //lux
	input_set_capability(dev, EV_REL, REL_WHEEL);  //cct
	input_set_capability(dev, EV_REL, REL_X);  //time_h
	input_set_capability(dev, EV_REL, REL_Y);  //time_l

	input_set_drvdata(dev, chip);

	ret = input_register_device(dev);
	if (ret < 0) {
		input_free_device(dev);
		goto err_register_input_dev;
	}

	ret = sensors_create_symlink(&dev->dev.kobj, dev->name);
	if (ret < 0)
		goto err_create_sensor_symlink;

	ret = sysfs_create_group(&dev->dev.kobj, &als_attribute_group);
	if (ret < 0)
		goto err_create_sysfs_group;

	chip->input_dev = dev;

	return 0;

err_create_sysfs_group:
	sensors_remove_symlink(&dev->dev.kobj, dev->name);
err_create_sensor_symlink:
	input_unregister_device(dev);
err_register_input_dev:
	dev = NULL;
	return ret;
}


static int tcs3407_getid(struct tcs3407_data *chip)
{
	u8 chipId;
	u8 revId;
	u8 auxId;
	//u8 i = 0;
	int err = 0;


	err = ams_getByte(chip->client, DEVREG_ID, &chipId);
	if (err < 0) {
		SENSOR_ERR("failed at DEVREG_ID(errno: %d)\n", err);
		return -1;
	}
	err = ams_getByte(chip->client, DEVREG_REVID, &revId);
	if (err < 0) {
		SENSOR_ERR("failed at DEVREG_REVID(errno: %d)\n", err);
		return -1;
	}

	err = ams_getByte(chip->client, DEVREG_AUXID, &auxId);
	if (err < 0) {
		SENSOR_ERR("failed at DEVREG_AUXID(errno: %d)\n", err);
		return -1;
	}
	SENSOR_INFO("DEVREG_ID:%02x DEVREG_REVID:%02x DEVREG_AUXID:%02x\n",chipId, revId, auxId);

	if (chipId != AMS_DEVICE_ID) {
		SENSOR_ERR("device id:%02x fail!\n", chipId);
		return -1;
	}
	
	return 0;
}


int tcs3407_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	
	struct tcs3407_data *data = 0;
	ams_deviceInfo_t amsDeviceInfo;
	ams_deviceIdentifier_e deviceId;

	SENSOR_INFO("start\n");
	if (!i2c_check_functionality(client->adapter,
		I2C_FUNC_SMBUS_BYTE_DATA)) {
		SENSOR_ERR("fail i2c_check_functionality\n");
		return -EOPNOTSUPP;
	}

	data = kzalloc(sizeof(struct tcs3407_data), GFP_KERNEL);

	if (!data) {
		err = -ENOMEM;
		SENSOR_ERR("no memory\n");
		return -ENOMEM;
	}
	
	data->client = client;
	i2c_set_clientdata(client, data);
	mutex_init(&data->i2clock);
	mutex_init(&data->activelock);

	err = tcs3407_getid(data);
	if (err < 0) {
		goto err_i2c_fail;
	}
	data->device_id = AMS_TCS3407;

	err = tcs3407_parse_dt(data);
	if (err < 0) {
		SENSOR_ERR("fail: parse dt\n");
		err = -ENODEV;
		goto err_parse_dt;
	}
	err = tcs3407_init_input_device(data);
	if (err < 0) {
		SENSOR_ERR("fail: init_input_device\n");
		err = -ENODEV;
		goto err_parse_dt;
	}

	ams_getDeviceInfo(&amsDeviceInfo, deviceId);
	SENSOR_INFO("name: %s, model: %s, ver:%s, addr: 0x%x\n", 
				amsDeviceInfo.deviceName, amsDeviceInfo.deviceModel, 
				amsDeviceInfo.driverVersion, data->client->addr);

	err = sensors_register(&data->dev, data, tcs3407_sensor_attrs, MODULE_NAME);
	if (err) {
		err = -ENOMEM;
		SENSOR_ERR("%s - cound not register als_sensor(%d).\n", __func__, err);
		goto err_id_failed;
	}

	err = ams_deviceInit(data, data->client, NULL);
	if (err < 0) {
		SENSOR_ERR("fail: ams_deviceInit\n");
		goto err_device_init;
	}

#ifdef CONFIG_AMS_OPTICAL_SENSOR_POLLING
	hrtimer_init(&data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    data->light_poll_delay = ns_to_ktime(60 * NSEC_PER_MSEC);  //60 msec Polling time, 50msec ATIME 
	data->timer.function = tcs3407_timer_func;

	data->wq = create_singlethread_workqueue("tcs3407_wq");
	if (!data->wq) {
		err = -ENOMEM;
		SENSOR_ERR("could not create workqueue\n");
		goto err_id_failed;
	}
	INIT_WORK(&data->work_light, tcs3407_work_func_light);
#endif  //CONFIG_AMS_OPTICAL_SENSOR_POLLING

	SENSOR_INFO("success\n");
	goto done;
	
err_device_init:
	sensors_unregister(data->dev, tcs3407_sensor_attrs);

err_id_failed:
	sysfs_remove_group(&data->input_dev->dev.kobj,
				&als_attribute_group);
	sensors_remove_symlink(&data->input_dev->dev.kobj,
			data->input_dev->name);

	input_unregister_device(data->input_dev);
err_parse_dt:
err_i2c_fail:
	mutex_destroy(&data->i2clock);
	mutex_destroy(&data->activelock);
	kfree(data);
	SENSOR_ERR("failed\n");
done:
	return err;
}

static int tcs3407_remove(struct i2c_client *client)
{
	struct tcs3407_data *data = i2c_get_clientdata(client);

	SENSOR_INFO("\n");
	sensors_unregister(data->dev, tcs3407_sensor_attrs);
	sysfs_remove_group(&data->input_dev->dev.kobj,
			   &als_attribute_group);
	sensors_remove_symlink(&data->input_dev->dev.kobj, data->input_dev->name);
	input_unregister_device(data->input_dev);

	mutex_destroy(&data->i2clock);
	mutex_destroy(&data->activelock);

	i2c_set_clientdata(client, NULL);

	kfree(data);
	data = NULL;
	return 0;
}

static void tcs3407_shutdown(struct i2c_client *client)
{
	struct tcs3407_data *data = i2c_get_clientdata(client);

	SENSOR_INFO("\n");
	if (data->enabled != 0) {
		tcs3407_als_disable(data);
	}
}


#ifdef CONFIG_PM
static int tcs3407_suspend(struct device *dev)
{
	struct tcs3407_data *data = dev_get_drvdata(dev);

	SENSOR_INFO("%d\n", data->enabled);

	if (data->enabled != 0) {
		tcs3407_als_disable(data);
	}
	return 0;
}

static int tcs3407_resume(struct device *dev)
{
	struct tcs3407_data *data = dev_get_drvdata(dev);

	SENSOR_INFO("%d\n", data->enabled);

	if (data->enabled != 0) {
		tcs3407_als_enable(data);
	}
	return 0;
}

static const struct dev_pm_ops tcs3407_pm_ops = {
	.suspend = tcs3407_suspend,
	.resume = tcs3407_resume
};
#endif

static const struct i2c_device_id tcs3407_idtable[] = {
	{ "tcs3407", 0 },
	{ }
};
/* descriptor of the tcs3407 I2C driver */
static struct i2c_driver tcs3407_driver = {
	.driver = {
		.name = "tcs3407",
		.owner = THIS_MODULE,
#if defined(CONFIG_PM)
		.pm = &tcs3407_pm_ops,
#endif
		.of_match_table = tcs3407_match_table,
	},
	.probe = tcs3407_probe,
	.remove = tcs3407_remove,
	.shutdown = tcs3407_shutdown,
	.id_table = tcs3407_idtable,
};

/* initialization and exit functions */
static int __init tcs3407_init(void)
{
	if (!lpcharge)
		return i2c_add_driver(&tcs3407_driver);
	else
		return 0;
}

static void __exit tcs3407_exit(void)
{
	i2c_del_driver(&tcs3407_driver);
}

module_init(tcs3407_init);
module_exit(tcs3407_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("TCS3407 ALS Driver");
MODULE_LICENSE("GPL");
