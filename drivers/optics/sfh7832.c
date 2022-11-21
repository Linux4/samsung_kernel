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

#include "sfh7832.h"

#define VENDOR				"OSRAM"
#define SFH7831_CHIP_NAME	"SFH7831"
#define SFH7832_CHIP_NAME	"SFH7832"
#define SFHXXXX_CHIP_NAME	"SFHXXXX"

#define VERSION				"1"
#define SUB_VERSION			"28"
#define VENDOR_VERSION		"o"

#define MODULE_NAME_HRM		"hrm_sensor"
#define DEFAULT_THRESHOLD	2097151
#define SLAVE_ADDR_SFH		0x5B

int hrm_debug = 1;
int hrm_info;

module_param(hrm_debug, int, S_IRUGO | S_IWUSR);
module_param(hrm_info, int, S_IRUGO | S_IWUSR);

static struct sfh7832_device_data *sfh7832_data;

/* #define DEBUG_HRMSENSOR */
#ifdef DEBUG_HRMSENSOR
static void sfh7832_debug_var(struct sfh7832_device_data *data)
{
	HRM_dbg("===== %s =====\n", __func__);
	HRM_dbg("%s client %p slave_addr 0x%x\n", __func__,
		data->client, data->client->addr);
	HRM_dbg("%s dev %p\n", __func__, data->dev);
	HRM_dbg("%s hrm_input_dev %p\n", __func__, data->hrm_input_dev);
	HRM_dbg("%s hrm_pinctrl %p\n", __func__, data->hrm_pinctrl);
	HRM_dbg("%s pins_sleep %p\n", __func__, data->pins_sleep);
	HRM_dbg("%s pins_idle %p\n", __func__, data->pins_idle);
	HRM_dbg("%s vdd_1p8 %s\n", __func__, data->vdd_1p8);
	HRM_dbg("%s i2c_1p8 %s\n", __func__, data->i2c_1p8);
	HRM_dbg("%s enabled_mode %d\n", __func__, data->enabled_mode);
	HRM_dbg("%s hrm_sampling_rate %d\n", __func__, data->hrm_sampling_rate);
	HRM_dbg("%s regulator_state %d\n", __func__, data->regulator_state);
	HRM_dbg("%s pin_hrm_int %d\n", __func__, data->pin_hrm_int);
	HRM_dbg("%s pin_hrm_en %d\n", __func__, data->pin_hrm_en);
	HRM_dbg("%s hrm_irq %d\n", __func__, data->hrm_irq);
	HRM_dbg("%s irq_state %d\n", __func__, atomic_read(&sfh7832_data->irq_state));
	HRM_dbg("%s led_current %d\n", __func__, data->led_current);
	HRM_dbg("%s xtalk_code %d\n", __func__, data->xtalk_code);
	HRM_dbg("%s hrm_threshold %d\n", __func__, data->hrm_threshold);
	HRM_dbg("%s eol_test_is_enable %d\n", __func__,
		data->eol_test_is_enable);
	HRM_dbg("%s eol_test_status %d\n", __func__, data->eol_test_status);
	HRM_dbg("%s lib_ver %s\n", __func__, data->lib_ver);
	HRM_dbg("===== %s =====\n", __func__);
}
#endif

int sfh7832_write_reg(u8 reg_addr, u32 data)
{
	int err;
	int tries = 0;
	int num = 1;
	u8 buffer[4] = { 0, };

	struct i2c_msg msgs[] = {
		{
		 .addr = sfh7832_data->client->addr,
		 .flags = sfh7832_data->client->flags & I2C_M_TEN,
		 .len = 4,
		 .buf = buffer,
		 },
	};

	buffer[0] = reg_addr;
	buffer[1] = (data >> 16) & 0xff;
	buffer[2] = (data >> 8) & 0xff;
	buffer[3] = data & 0xff;

	if (!sfh7832_data->pm_state || sfh7832_data->regulator_state == 0) {
		HRM_err("%s - write error, pm suspend or reg_state %d\n",
				__func__, sfh7832_data->regulator_state);
		err = -EFAULT;
		return err;
	}
	mutex_lock(&sfh7832_data->suspendlock);

	do {
		err = i2c_transfer(sfh7832_data->client->adapter, msgs, num);
		if (err != num)
			msleep_interruptible(AFE4420_I2C_RETRY_DELAY);
		if (err < 0)
			HRM_err("%s - i2c_transfer error = %d\n", __func__, err);
	} while ((err != num) && (++tries < AFE4420_I2C_MAX_RETRIES));

	mutex_unlock(&sfh7832_data->suspendlock);

	if (err != num) {
		HRM_err("%s -write transfer error:%d\n", __func__, err);
		err = -EIO;
		sfh7832_data->i2c_err_cnt++;
		return err;
	}

	return 0;
}

int sfh7832_read_reg(u8 reg, u32 *val)
{
	int err = -1;
	int tries = 0;		/* # of attempts to read the device */

	int num = 2;
	u8 buffer[3] = { 0, };

	struct i2c_msg msgs[] = {
		{
		 .addr = sfh7832_data->client->addr,
		 .flags = sfh7832_data->client->flags & I2C_M_TEN,
		 .len = 1,
		 .buf = &reg,
		 },
		{
		 .addr = sfh7832_data->client->addr,
		 .flags = (sfh7832_data->client->flags & I2C_M_TEN) | I2C_M_RD,
		 .len = 3,
		 .buf = buffer,
		 },
	};

	if (!sfh7832_data->pm_state || sfh7832_data->regulator_state == 0) {
		HRM_err("%s - read error, pm suspend or reg_state %d\n",
				__func__, sfh7832_data->regulator_state);
		err = -EFAULT;
		return err;
	}
	mutex_lock(&sfh7832_data->suspendlock);

	do {
		err = i2c_transfer(sfh7832_data->client->adapter, msgs, num);

		if (err != num)
			msleep_interruptible(AFE4420_I2C_RETRY_DELAY);
		if (err < 0)
			HRM_err("%s - i2c_transfer error = %d\n", __func__,
				err);
	} while ((err != num) && (++tries < AFE4420_I2C_MAX_RETRIES));

	mutex_unlock(&sfh7832_data->suspendlock);

	if (err != num) {
		HRM_err("%s -read transfer error:%d\n", __func__, err);
		err = -EIO;
		sfh7832_data->i2c_err_cnt++;
	} else
		err = 0;

	if (err >= 0)
		*val = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];

	return err;
}

int sfh7832_read_burst_reg(u8 reg, u8 *buffer, u8 size)
{
	int err = -1;
	int tries = 0;		/* # of attempts to read the device */
	int num = 2;

	struct i2c_msg msgs[] = {
		{
		 .addr = sfh7832_data->client->addr,
		 .flags = sfh7832_data->client->flags & I2C_M_TEN,
		 .len = 1,
		 .buf = &reg,
		 },
		{
		 .addr = sfh7832_data->client->addr,
		 .flags = (sfh7832_data->client->flags & I2C_M_TEN) | I2C_M_RD,
		 .len = size,
		 .buf = buffer,
		 },
	};

	if (!sfh7832_data->pm_state || sfh7832_data->regulator_state == 0) {
		HRM_err("%s - read error, pm suspend or reg_state %d\n",
				__func__, sfh7832_data->regulator_state);
		err = -EFAULT;
		return err;
	}
	mutex_lock(&sfh7832_data->suspendlock);

	do {
		err = i2c_transfer(sfh7832_data->client->adapter, msgs, num);

		if (err != num)
			msleep_interruptible(AFE4420_I2C_RETRY_DELAY);
		if (err < 0)
			HRM_err("%s - i2c_transfer error = %d\n", __func__,
				err);
	} while ((err != num) && (++tries < AFE4420_I2C_MAX_RETRIES));

	mutex_unlock(&sfh7832_data->suspendlock);

	if (err != num) {
		HRM_err("%s -read transfer error:%d\n", __func__, err);
		err = -EIO;
		sfh7832_data->i2c_err_cnt++;
	} else
		err = 0;

	return err;
}

int sfh7832_print_reg_status(void)
{
	int i, err;
	u32 value;

	for (i = 0; i < 232 ; i++) {
		err = sfh7832_read_reg(i, &value);
		if (err != 0) {
			HRM_err("%s - error sfh7832_read_reg!\n", __func__);
			return err;
		}
		HRM_dbg("DUMP -  : reg : %02x, value : %06x\n", i, value);
	}
	return 0;
}

int sfh7832_get_chipid(u32 *chip_id)
{
	int err = 0;
	/* AFE4420_DESIGNID is 0xB */
	u32 recvData;

	err = sfh7832_read_reg(AFE4420_DESIGN_ID, &recvData);
	if (err != 0)
		return err;

	*chip_id = ((recvData >> 7) & 0x1FFF);

	if (*chip_id != AFE4420_DESIGNID && *chip_id != AFE4420_DESIGNID_REV2)
		err = -ENXIO;

	HRM_info("%s - Device ID = 0x%x\n", __func__, *chip_id);

	return err;
}


int sfh7832_get_part_type(u16 *part_type)
{
	int err = 0;

	sfh7832_data->part_type = PART_TYPE_SFH7832;
	*part_type = sfh7832_data->part_type;

	return err;
}

int sfh7832_set_ioffset(s8 ir, s8 red, s8 green, s8 blue)
{
	int err = 0;

	u32 OffDAC_InterimCode1 = 0;
	u32 OffDAC_InterimCode2 = 0;
	u32 OffDAC_InterimCode3 = 0;
	u32 OffDAC_InterimCode4 = 0;
	/* u32 OffDAC_dummy; */ /* for d5 */
	/* u32 OffDAC_ambient; */ /* for d6 */

	u8 pol[4];

	HRM_dbg("%s - irgb : %d %d %d %d\n", __func__, ir, red, green, blue);

	sfh7832_data->ioffset_led1 = ir;	/* IR */
	sfh7832_data->ioffset_led2 = red;	/* red */
	sfh7832_data->ioffset_led3 = green;	/* green */
	sfh7832_data->ioffset_led4 = blue;	/* blue */
	/* sfh7832_data->ioffset_led1 = d5;
	 * sfh7832_data->ioffset_led4 = d5;
	 */

	sfh7832_data->min_base_offset[AGC_IR] = ioff_step[sfh7832_data->ioff_dac] * (long long) ir * gain[sfh7832_data->rfgain_led1] * CONVERTER;
	sfh7832_data->min_base_offset[AGC_RED] = ioff_step[sfh7832_data->ioff_dac] * (long long) red * gain[sfh7832_data->rfgain_led2] * CONVERTER;
	sfh7832_data->min_base_offset[AGC_GREEN] = ioff_step[sfh7832_data->ioff_dac] * (long long) green * gain[sfh7832_data->rfgain_led3] * CONVERTER;
	sfh7832_data->min_base_offset[AGC_BLUE] = ioff_step[sfh7832_data->ioff_dac] * (long long) blue * gain[sfh7832_data->rfgain_led4] * CONVERTER;

	HRM_dbg("%s - min base : %lld %lld %lld %lld\n", __func__, sfh7832_data->min_base_offset[AGC_IR], sfh7832_data->min_base_offset[AGC_RED],
		sfh7832_data->min_base_offset[AGC_GREEN], sfh7832_data->min_base_offset[AGC_BLUE]);

	sfh7832_data->reached_thresh[AGC_IR] = 0;
	sfh7832_data->reached_thresh[AGC_RED] = 0;
	sfh7832_data->reached_thresh[AGC_GREEN] = 0;
	sfh7832_data->reached_thresh[AGC_BLUE] = 0;

	/* Spliting LED2 data input */
	if (green >= 0)
		pol[2] = 1;
	else {
		pol[2] = 0;
		green = -green;
	}
	OffDAC_InterimCode3 = ((pol[2] << 7) | (green & 0x7F));

	/* Spliting LED3 data input */
	if (blue >= 0)
		pol[3] = 1;
	else {
		pol[3] = 0;
		blue = -blue;
	}
	OffDAC_InterimCode4 = ((pol[3] << 7) | (blue & 0x7F));

	/* Spliting LED1 data input */
	if (ir >= 0)
		pol[0] = 1;
	else {
		pol[0] = 0;
		ir = -ir;
	}
	OffDAC_InterimCode1 = ((pol[0] << 7) | (ir & 0x7F));

	/* Spliting LED4 data input */
	if (red >= 0)
		pol[1] = 1;
	else {
		pol[1] = 0;
		red = -red;
	}
	OffDAC_InterimCode2 = ((pol[1] << 7) | (red & 0x7F));

	afe4420_phase13_reg[PHASE5_INDEX + 1][1] &= IOFFDAC_MASK;
	afe4420_phase13_reg[PHASE7_INDEX + 1][1] &= IOFFDAC_MASK;
	afe4420_phase13_reg[PHASE11_INDEX + 1][1] &= IOFFDAC_MASK;
	afe4420_phase13_reg[PHASE13_INDEX + 1][1] &= IOFFDAC_MASK;

	afe4420_phase13_reg[PHASE5_INDEX + 1][1] |=
	    (OffDAC_InterimCode1 << IOFFDAC_BASE);
	afe4420_phase13_reg[PHASE7_INDEX + 1][1] |=
	    (OffDAC_InterimCode2 << IOFFDAC_BASE);
	afe4420_phase13_reg[PHASE11_INDEX + 1][1] |=
	    (OffDAC_InterimCode3 << IOFFDAC_BASE);
	afe4420_phase13_reg[PHASE13_INDEX + 1][1] |=
	    (OffDAC_InterimCode4 << IOFFDAC_BASE);

	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE5_INDEX + 1][0],
				afe4420_phase13_reg[PHASE5_INDEX + 1][1]);
	if (err != 0)
		return err;
	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE7_INDEX + 1][0],
				afe4420_phase13_reg[PHASE7_INDEX + 1][1]);
	if (err != 0)
		return err;
	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE11_INDEX + 1][0],
				afe4420_phase13_reg[PHASE11_INDEX + 1][1]);
	if (err != 0)
		return err;
	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE13_INDEX + 1][0],
				afe4420_phase13_reg[PHASE13_INDEX + 1][1]);
	return err;
}

int sfh7832_get_rf(u8 *g1, u8 *g2, u8 *g3, u8 *g4)
{
	int err = 0;
	u32 get_rf = 0;

	err = sfh7832_read_reg(AFE4420_PHASECNTRL1(PHASE5), &get_rf);
	if (err != 0)
		return err;
	*g1 = (get_rf & 0xF0) >> TIA_GAIN_RF;

	err = sfh7832_read_reg(AFE4420_PHASECNTRL1(PHASE7), &get_rf);
	if (err != 0)
		return err;
	*g2 = (get_rf & 0xF0) >> TIA_GAIN_RF;

	err = sfh7832_read_reg(AFE4420_PHASECNTRL1(PHASE11), &get_rf);
	if (err != 0)
		return err;
	*g3 = (get_rf & 0xF0) >> TIA_GAIN_RF;

	err = sfh7832_read_reg(AFE4420_PHASECNTRL1(PHASE13), &get_rf);
	if (err != 0)
		return err;
	*g4 = (get_rf & 0xF0) >> TIA_GAIN_RF;

	return err;

}

int sfh7832_set_rf(u8 g1, u8 g2, u8 g3, u8 g4)
{
	int err = 0;

	sfh7832_data->rfgain_amb1 = g1;	/* ambient */
	sfh7832_data->rfgain_led1 = g1;	/* IR */
	sfh7832_data->rfgain_led2 = g2;	/* RED */
	sfh7832_data->rfgain_amb2 = g3;	/* ambient */
	sfh7832_data->rfgain_led3 = g3;	/* GREEN */
	sfh7832_data->rfgain_led4 = g4;	/* BLUE */

	sfh7832_data->min_base_offset[AGC_IR] = ioff_step[sfh7832_data->ioff_dac] * (long long)sfh7832_data->ioffset_led1 * gain[sfh7832_data->rfgain_led1] * CONVERTER;
	sfh7832_data->min_base_offset[AGC_RED] = ioff_step[sfh7832_data->ioff_dac] * (long long)sfh7832_data->ioffset_led2 * gain[sfh7832_data->rfgain_led2] * CONVERTER;
	sfh7832_data->min_base_offset[AGC_GREEN] = ioff_step[sfh7832_data->ioff_dac] * (long long)sfh7832_data->ioffset_led3 * gain[sfh7832_data->rfgain_led3] * CONVERTER;
	sfh7832_data->min_base_offset[AGC_BLUE] = ioff_step[sfh7832_data->ioff_dac] * (long long)sfh7832_data->ioffset_led4 * gain[sfh7832_data->rfgain_led4] * CONVERTER;

	sfh7832_data->reached_thresh[AGC_IR] = 0;
	sfh7832_data->reached_thresh[AGC_RED] = 0;
	sfh7832_data->reached_thresh[AGC_GREEN] = 0;
	sfh7832_data->reached_thresh[AGC_BLUE] = 0;

	afe4420_phase13_reg[PHASE3_INDEX + 1][1] &= TIA_RF_MASK;
	afe4420_phase13_reg[PHASE5_INDEX + 1][1] &= TIA_RF_MASK;
	afe4420_phase13_reg[PHASE7_INDEX + 1][1] &= TIA_RF_MASK;
	afe4420_phase13_reg[PHASE9_INDEX + 1][1] &= TIA_RF_MASK;
	afe4420_phase13_reg[PHASE11_INDEX + 1][1] &= TIA_RF_MASK;
	afe4420_phase13_reg[PHASE13_INDEX + 1][1] &= TIA_RF_MASK;

	afe4420_phase13_reg[PHASE3_INDEX + 1][1] |=
	    (sfh7832_data->rfgain_amb1 << TIA_GAIN_RF);
	afe4420_phase13_reg[PHASE5_INDEX + 1][1] |=
	    (sfh7832_data->rfgain_led1 << TIA_GAIN_RF);
	afe4420_phase13_reg[PHASE7_INDEX + 1][1] |=
	    (sfh7832_data->rfgain_led2 << TIA_GAIN_RF);
	afe4420_phase13_reg[PHASE9_INDEX + 1][1] |=
	    (sfh7832_data->rfgain_amb2 << TIA_GAIN_RF);
	afe4420_phase13_reg[PHASE11_INDEX + 1][1] |=
	    (sfh7832_data->rfgain_led3 << TIA_GAIN_RF);
	afe4420_phase13_reg[PHASE13_INDEX + 1][1] |=
	    (sfh7832_data->rfgain_led4 << TIA_GAIN_RF);

	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE3_INDEX + 1][0],
				afe4420_phase13_reg[PHASE3_INDEX + 1][1]);
	if (err != 0)
		return err;
	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE5_INDEX + 1][0],
				afe4420_phase13_reg[PHASE5_INDEX + 1][1]);
	if (err != 0)
		return err;
	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE7_INDEX + 1][0],
				afe4420_phase13_reg[PHASE7_INDEX + 1][1]);
	if (err != 0)
		return err;
	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE9_INDEX + 1][0],
				afe4420_phase13_reg[PHASE9_INDEX + 1][1]);
	if (err != 0)
		return err;
	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE11_INDEX + 1][0],
				afe4420_phase13_reg[PHASE11_INDEX + 1][1]);
	if (err != 0)
		return err;
	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE13_INDEX + 1][0],
				afe4420_phase13_reg[PHASE13_INDEX + 1][1]);
	return err;
}

int sfh7832_get_cf(u8 *g1)
{
	int err = 0;
	u32 get_cf = 0;

	err = sfh7832_read_reg(AFE4420_PHASECNTRL1(PHASE3), &get_cf);
	if (err != 0)
		return err;

	*g1 = (get_cf & 0x1C00) >> TIA_GAIN_CF;

	return err;
}

int sfh7832_set_cf(s8 c1)
{
	int err = 0;

	sfh7832_data->cfgain_amb1 = c1;	/* ambient */
	sfh7832_data->cfgain_led1 = c1;	/* ir */
	sfh7832_data->cfgain_led2 = c1;	/* red */
	sfh7832_data->cfgain_amb2 = c1;	/*  ambient */
	sfh7832_data->cfgain_led3 = c1;	/* green */
	sfh7832_data->cfgain_led4 = c1;	/* blue */

	afe4420_phase13_reg[PHASE3_INDEX + 1][1] &= TIA_CF_MASK;
	afe4420_phase13_reg[PHASE5_INDEX + 1][1] &= TIA_CF_MASK;
	afe4420_phase13_reg[PHASE7_INDEX + 1][1] &= TIA_CF_MASK;
	afe4420_phase13_reg[PHASE9_INDEX + 1][1] &= TIA_CF_MASK;
	afe4420_phase13_reg[PHASE11_INDEX + 1][1] &= TIA_CF_MASK;
	afe4420_phase13_reg[PHASE13_INDEX + 1][1] &= TIA_CF_MASK;

	afe4420_phase13_reg[PHASE3_INDEX + 1][1] |=
	    (sfh7832_data->cfgain_amb1 << TIA_GAIN_CF);
	afe4420_phase13_reg[PHASE5_INDEX + 1][1] |=
	    (sfh7832_data->cfgain_led1 << TIA_GAIN_CF);
	afe4420_phase13_reg[PHASE7_INDEX + 1][1] |=
	    (sfh7832_data->cfgain_led2 << TIA_GAIN_CF);
	afe4420_phase13_reg[PHASE9_INDEX + 1][1] |=
	    (sfh7832_data->cfgain_amb2 << TIA_GAIN_CF);
	afe4420_phase13_reg[PHASE11_INDEX + 1][1] |=
	    (sfh7832_data->cfgain_led3 << TIA_GAIN_CF);
	afe4420_phase13_reg[PHASE13_INDEX + 1][1] |=
	    (sfh7832_data->cfgain_led4 << TIA_GAIN_CF);

	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE3_INDEX + 1][0],
				afe4420_phase13_reg[PHASE3_INDEX + 1][1]);
	if (err != 0)
		return err;
	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE5_INDEX + 1][0],
				afe4420_phase13_reg[PHASE5_INDEX + 1][1]);
	if (err != 0)
		return err;
	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE7_INDEX + 1][0],
				afe4420_phase13_reg[PHASE7_INDEX + 1][1]);
	if (err != 0)
		return err;
	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE9_INDEX + 1][0],
				afe4420_phase13_reg[PHASE9_INDEX + 1][1]);
	if (err != 0)
		return err;
	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE11_INDEX + 1][0],
				afe4420_phase13_reg[PHASE11_INDEX + 1][1]);
	if (err != 0)
		return err;
	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE13_INDEX + 1][0],
				afe4420_phase13_reg[PHASE13_INDEX + 1][1]);

	return err;
}

int sfh7832_get_numav(u8 *num_avr)
{
	int err = 0;
	u32 numav = 0;

	err = sfh7832_read_reg(AFE4420_PHASECNTRL1(PHASE3), &numav);
	if (err != 0)
		return err;

	*num_avr = (numav & 0xF);
	return err;
}

int sfh7832_set_numav(s32 num_avr)
{
	int err = 0;

	if (num_avr > 0xf)
		num_avr = 0xf;

	sfh7832_data->numav = num_avr - 1;

	afe4420_phase13_reg[PHASE3_INDEX + 1][1] &= NUMAV_MASK;
	afe4420_phase13_reg[PHASE5_INDEX + 1][1] &= NUMAV_MASK;
	afe4420_phase13_reg[PHASE7_INDEX + 1][1] &= NUMAV_MASK;
	afe4420_phase13_reg[PHASE9_INDEX + 1][1] &= NUMAV_MASK;
	afe4420_phase13_reg[PHASE11_INDEX + 1][1] &= NUMAV_MASK;
	afe4420_phase13_reg[PHASE13_INDEX + 1][1] &= NUMAV_MASK;

	afe4420_phase13_reg[PHASE3_INDEX + 1][1] |= sfh7832_data->numav;
	afe4420_phase13_reg[PHASE5_INDEX + 1][1] |= sfh7832_data->numav;
	afe4420_phase13_reg[PHASE7_INDEX + 1][1] |= sfh7832_data->numav;
	afe4420_phase13_reg[PHASE9_INDEX + 1][1] |= sfh7832_data->numav;
	afe4420_phase13_reg[PHASE11_INDEX + 1][1] |= sfh7832_data->numav;
	afe4420_phase13_reg[PHASE13_INDEX + 1][1] |= sfh7832_data->numav;

	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE3_INDEX + 1][0],
				afe4420_phase13_reg[PHASE3_INDEX + 1][1]);
	if (err != 0)
		return err;
	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE5_INDEX + 1][0],
				afe4420_phase13_reg[PHASE5_INDEX + 1][1]);
	if (err != 0)
		return err;
	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE7_INDEX + 1][0],
				afe4420_phase13_reg[PHASE7_INDEX + 1][1]);
	if (err != 0)
		return err;
	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE9_INDEX + 1][0],
				afe4420_phase13_reg[PHASE9_INDEX + 1][1]);
	if (err != 0)
		return err;
	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE11_INDEX + 1][0],
				afe4420_phase13_reg[PHASE11_INDEX + 1][1]);
	if (err != 0)
		return err;
	err = sfh7832_write_reg(afe4420_phase13_reg[PHASE13_INDEX + 1][0],
				afe4420_phase13_reg[PHASE13_INDEX + 1][1]);

	return err;
}

int sfh7832_get_ioff_dac(u8 *dac)
{
	int err = 0;
	u32 val;

	err = sfh7832_read_reg(AFE4420_CONTROL1, &val);
	if (err != 0)
		return err;

	val = (val >> 10) & 7;
	*dac = val;

	return err;
}

int sfh7832_set_ioff_dac(u8 dac)
{
	int err = 0;
	u32 val;
	u32 ioff_dac;

	sfh7832_data->ioff_dac = dac;

	ioff_dac = ((u32) dac & 7) << 10;

	err = sfh7832_read_reg(AFE4420_CONTROL1, &val);
	if (err != 0)
		return err;

	val &= 0x868200;

	val |= ioff_dac;

	HRM_dbg("%s - val : 0x%x\n", __func__, val);

	err = sfh7832_write_reg(AFE4420_CONTROL1, val);
	if (err != 0)
		return err;

	return err;
}

int sfh7832_get_aacm_calib(u8 *calib_data)
{
	int err = 0;

	return err;
}

int sfh7832_get_callib_aacm_pd(u8 mode, u8 rf)
{
	return cal_aacm_pd_table[mode][rf];
}

void current_comb(u8 cur1, u8 cur2, u8 lednum, u32 *regval, u32 *leddrv1,
		  u32 *leddrv2)
{
	int curtoreg1 = 0;
	int curtoreg2 = 0;

	if ((cur1 > 0) && (cur1 <= 100)) {
		curtoreg1 = ((255 * cur1) / 100);
		*leddrv1 &= ~(1 << (lednum + 7));
	} else if ((cur1 > 100) && (cur1 <= 200)) {
		curtoreg1 = ((255 * cur1) / 200);
		*leddrv1 |= (1 << (lednum + 7));
	}

	if ((cur2 > 0) && (cur2 <= 100)) {
		curtoreg2 = ((255 * cur2) / 100);
		*leddrv2 &= ~(1 << (lednum + 8));
	} else if ((cur2 > 100) && (cur2 <= 200)) {
		curtoreg2 = ((255 * cur2) / 200);
		*leddrv2 |= (1 << (lednum + 8));
	}

	*regval = (((curtoreg2 << ILED2_SHIFT) & ILED2_MASK) | curtoreg1);
}

int sfh7832_set_current_upto_200mA(u8 d1, u8 d2, u8 d3, u8 d4)
{
/* example code for set the current  upto 200mA with step mA */
	int err = 0;
	int i = 0;
	u32 LEDInterimCode2_1 = 0;
	u32 LEDInterimCode4_3 = 0;

	/* d1:LED 2:green,
	 * d2:LED 3:blue,
	 * d3:LED 1:IR,
	 * d4:LED 4:red
	 */

	sfh7832_data->iled2 = d1;	/* set green */
	sfh7832_data->iled3 = d2;	/* set blue */
	sfh7832_data->iled1 = d3;	/* set ir; */
	sfh7832_data->iled4 = d4;	/* set red */

	HRM_info("%s - %d %d %d %d\n", __func__, sfh7832_data->iled2,
		 sfh7832_data->iled3, sfh7832_data->iled1, sfh7832_data->iled4);

	afe4420_phase13_reg[PHASE5_INDEX][1] &= ~(1 << PCNTRL0_LED_DRV2_TX1);
	afe4420_phase13_reg[PHASE7_INDEX][1] &= ~(1 << PCNTRL0_LED_DRV2_TX4);
	afe4420_phase13_reg[PHASE11_INDEX][1] &= ~(1 << PCNTRL0_LED_DRV2_TX2);
	afe4420_phase13_reg[PHASE13_INDEX][1] &= ~(1 << PCNTRL0_LED_DRV2_TX3);

	for (i = 4; i < 13 ; i = i + 2) {
		err = sfh7832_write_reg(afe4420_phase13_reg[i * 3][0],
					afe4420_phase13_reg[i * 3][1]);
		if (err != 0)
			return err;
	}

	current_comb(d3, d1, 1, &LEDInterimCode2_1,
				&afe4420_phase13_reg[PHASE5_INDEX][1],
				&afe4420_phase13_reg[PHASE7_INDEX][1]);

	current_comb(d2, d4, 3, &LEDInterimCode4_3,
				&afe4420_phase13_reg[PHASE11_INDEX][1],
				&afe4420_phase13_reg[PHASE13_INDEX][1]);

	err = sfh7832_write_reg(AFE4420_ILED_TX2_1, LEDInterimCode2_1);
	if (err != 0)
		return err;

	err = sfh7832_write_reg(AFE4420_ILED_TX4_3, LEDInterimCode4_3);
	if (err != 0)
		return err;

	for (i = 4; i < 13; i = i + 2) {
		err = sfh7832_write_reg(afe4420_phase13_reg[i * 3][0],
					afe4420_phase13_reg[i * 3][1]);
		if (err != 0)
			return err;
	}

	HRM_info("%s - 0x%x 0x%x\n", __func__, LEDInterimCode2_1,
		 LEDInterimCode4_3);
	return err;
}

int sfh7832_set_current(u8 d1, u8 d2, u8 d3, u8 d4, u8 channel)
{
	int err = 0;
	u32 LEDInterimCode2_1 = 0;
	u32 LEDInterimCode4_3 = 0;

	s32 c1 = 0;
	s32 c2 = 0;
	s32 c3 = 0;
	s32 c4 = 0;

	switch (channel) {
	case AGC_IR:
		sfh7832_data->iled1 = d1; /* set ir; */
		c1 = (u32) sfh7832_data->iled1 * 250 / 255; /* convert into mA domain */
		break;
	case AGC_RED:
		sfh7832_data->iled4 = d2; /* set red */
		c4 = (u32) sfh7832_data->iled4 * 250 / 255;
		break;
	case AGC_GREEN:
		sfh7832_data->iled2 = d3; /* set green */
		c2 = (u32) sfh7832_data->iled2 * 250 / 255;
		break;
	case AGC_BLUE:
		sfh7832_data->iled3 = d4; /* set blue */
		c3 = (u32) sfh7832_data->iled3 * 250 / 255;
		break;
	default:
		sfh7832_data->iled1 = d1; /* set ir; */
		sfh7832_data->iled4 = d2; /* set red */
		sfh7832_data->iled2 = d3; /* set green */
		sfh7832_data->iled3 = d4; /* set blue */

		c1 = (u32) sfh7832_data->iled1 * 250 / 255;
		c4 = (u32) sfh7832_data->iled4 * 250 / 255;
		c2 = (u32) sfh7832_data->iled2 * 250 / 255;
		c3 = (u32) sfh7832_data->iled3 * 250 / 255;
		break;
	}

	if (sfh7832_data->trim_version) {	/* 21bit OTP trim */
		switch (channel) {
		case AGC_IR:
			sfh7832_data->trim_led1 = c1 * 1020 / ((1000 + (s32) sfh7832_data->trmled1 * TRM_1 * 10) 
										+ c1 * ((s32)sfh7832_data->trmled1 * TRM_1 * 10) / TRM_B1);
			break;
		case AGC_RED:
			sfh7832_data->trim_led4 = c4 * 1020 / ((1000 + (s32) sfh7832_data->trmled4 * TRM_4 * 10)
										+ c4 * ((s32)sfh7832_data->trmled4 * TRM_4 * 10) / TRM_B4);
			break;
		case AGC_GREEN:
			sfh7832_data->trim_led2 = c2 * 1020 / ((1000 + (s32) sfh7832_data->trmled2 * TRM_2 * 10)
										+ c2 * ((s32)sfh7832_data->trmled2 * TRM_2 * 10) / TRM_B2);
			break;
		case AGC_BLUE:
			sfh7832_data->trim_led3 = c3 * 1020 / ((1000 + (s32) sfh7832_data->trmled3 * TRM_3 * 10)
										+ c3 * ((s32)sfh7832_data->trmled3 * TRM_3 * 10) / TRM_B3);
			break;
		default:
			sfh7832_data->trim_led1 = c1 * 1020 / ((1000 + (s32) sfh7832_data->trmled1 * TRM_1 * 10)
										+ c1 * ((s32)sfh7832_data->trmled1 * TRM_1 * 10) / TRM_B1);
			sfh7832_data->trim_led2 = c2 * 1020 / ((1000 + (s32) sfh7832_data->trmled2 * TRM_2 * 10)
										+ c2 * ((s32)sfh7832_data->trmled2 * TRM_2 * 10) / TRM_B2);
			sfh7832_data->trim_led3 = c3 * 1020 / ((1000 + (s32) sfh7832_data->trmled3 * TRM_3 * 10)
										+ c3 * ((s32)sfh7832_data->trmled3 * TRM_3 * 10) / TRM_B3);
			sfh7832_data->trim_led4 = c4 * 1020 / ((1000 + (s32) sfh7832_data->trmled4 * TRM_4 * 10)
										+ c4 * ((s32)sfh7832_data->trmled4 * TRM_4 * 10) / TRM_B4);
			break;
		}
	} else {						/* 18bit OTP trim */
		switch (channel) {
		case AGC_IR:
			sfh7832_data->trim_led1 = c1 * 1020 / ((1000 + ((s32)sfh7832_data->trmglobal * TRM_GLOBAL
				* 10 + (s32)sfh7832_data->trmled1 * TRM_1 * 10)) + c1 * ((s32)sfh7832_data->trmglobal * TRM_GLOBAL * 10 + (s32)sfh7832_data->trmled1 * TRM_1*10) / TRM_B1);
			break;
		case AGC_RED:
			sfh7832_data->trim_led4 = c4 * 1020 / ((1000 + ((s32)sfh7832_data->trmglobal * TRM_GLOBAL
				* 10 + (s32)sfh7832_data->trmled4 * TRM_4 * 10)) + c4 * ((s32)sfh7832_data->trmglobal * TRM_GLOBAL * 10 + (s32)sfh7832_data->trmled4 * TRM_4*10) / TRM_B4);
			break;
		case AGC_GREEN:
			sfh7832_data->trim_led2 = c2 * 1020 / ((1000 + ((s32)sfh7832_data->trmglobal * TRM_GLOBAL
				* 10 + (s32)sfh7832_data->trmled2 * TRM_2 * 10)) + c2 * ((s32)sfh7832_data->trmglobal * TRM_GLOBAL * 10 + (s32)sfh7832_data->trmled2 * TRM_2*10) / TRM_B2);
			break;
		case AGC_BLUE:
			sfh7832_data->trim_led3 = c3 * 1020 / ((1000 + ((s32)sfh7832_data->trmglobal * TRM_GLOBAL
				* 10 + (s32)sfh7832_data->trmled3 * TRM_3 * 10)) + c3 * ((s32)sfh7832_data->trmglobal * TRM_GLOBAL * 10 + (s32)sfh7832_data->trmled3 * TRM_3*10) / TRM_B3);
			break;
		default:
			sfh7832_data->trim_led1 = c1 * 1020 / ((1000 + ((s32)sfh7832_data->trmglobal * TRM_GLOBAL
				* 10 + (s32)sfh7832_data->trmled1 * TRM_1 * 10)) + c1  * ((s32)sfh7832_data->trmglobal * TRM_GLOBAL * 10 + (s32)sfh7832_data->trmled1 * TRM_1*10) / TRM_B1);
			sfh7832_data->trim_led2 = c2 * 1020 / ((1000 + ((s32)sfh7832_data->trmglobal * TRM_GLOBAL
				* 10 + (s32)sfh7832_data->trmled2 * TRM_2 * 10)) + c2  * ((s32)sfh7832_data->trmglobal * TRM_GLOBAL * 10 + (s32)sfh7832_data->trmled2 * TRM_2*10) / TRM_B2);
			sfh7832_data->trim_led3 = c3 * 1020 / ((1000 + ((s32)sfh7832_data->trmglobal * TRM_GLOBAL
				* 10 + (s32)sfh7832_data->trmled3 * TRM_3 * 10)) + c3  * ((s32)sfh7832_data->trmglobal * TRM_GLOBAL * 10 + (s32)sfh7832_data->trmled3 * TRM_3*10) / TRM_B3);
			sfh7832_data->trim_led4 = c4 * 1020 / ((1000 + ((s32)sfh7832_data->trmglobal * TRM_GLOBAL
				* 10 + (s32)sfh7832_data->trmled4 * TRM_4 * 10)) + c4  * ((s32)sfh7832_data->trmglobal * TRM_GLOBAL * 10 + (s32)sfh7832_data->trmled4 * TRM_4*10) / TRM_B4);
			break;
		}
	}

	/* code for limiting to 200mA in E2x mode  */
	if (sfh7832_data->trim_led1 > LED_MAX_CODE)
		sfh7832_data->trim_led1 = LED_MAX_CODE;

	if (sfh7832_data->trim_led2 > LED_MAX_CODE)
		sfh7832_data->trim_led2 = LED_MAX_CODE;

	if (sfh7832_data->trim_led3 > LED_MAX_CODE)
		sfh7832_data->trim_led3 = LED_MAX_CODE;

	if (sfh7832_data->trim_led4 > LED_MAX_CODE)
		sfh7832_data->trim_led4 = LED_MAX_CODE;

	HRM_info("%s - INPUT I:0x%x R:0x%x G:0x%x B:0x%x OUTPUT I:0x%x R:0x%x G:0x%x B:0x%x\n",
			__func__, sfh7832_data->iled1, sfh7832_data->iled4, sfh7832_data->iled2, sfh7832_data->iled3,
			sfh7832_data->trim_led1, sfh7832_data->trim_led4, sfh7832_data->trim_led2, sfh7832_data->trim_led3);

	/* Current for LED1,LED2 */
	LEDInterimCode2_1 = (u8) sfh7832_data->trim_led2;
	LEDInterimCode2_1 = (((LEDInterimCode2_1 << ILED2_SHIFT) & ILED2_MASK) | (u8) sfh7832_data->trim_led1);

	/* Current for LED3,LED4 */
	LEDInterimCode4_3 = (u8) sfh7832_data->trim_led4;
	LEDInterimCode4_3 = (((LEDInterimCode4_3 << ILED4_SHIFT) & ILED4_MASK) | (u8) sfh7832_data->trim_led3);

	HRM_info("%s - 0x%x 0x%x\n", __func__, LEDInterimCode2_1, LEDInterimCode4_3);

	/*Set current for ILED 1,2 */
	err = sfh7832_write_reg(AFE4420_ILED_TX2_1, LEDInterimCode2_1);
	if (err != 0)
		return err;
	/*Set current for ILED 3,4 */
	err = sfh7832_write_reg(AFE4420_ILED_TX4_3, LEDInterimCode4_3);

	return err;
}

static int sfh7832_update_led_current(int new_led_uA, int led_num)
{
	int err = 0;
	u8 led_reg_val;

	if (new_led_uA > sfh7832_data->max_current) {
		HRM_dbg("%s - Tried to set LED%d to %duA. Max is %duA\n",
			__func__, led_num, new_led_uA, sfh7832_data->max_current);
		new_led_uA = sfh7832_data->max_current;
	} else if (new_led_uA < SFH7832_MIN_CURRENT) {
		HRM_dbg("%s - Tried to set LED%d to %duA. Min is %duA\n",
			__func__, led_num, new_led_uA, SFH7832_MIN_CURRENT);
		new_led_uA = SFH7832_MIN_CURRENT;
	}
	led_reg_val = new_led_uA / SFH7832_CURRENT_PER_STEP;
	/* d1:LED 2:green
	 * d2:LED 3:blue
	 * d3:LED 1:IR
	 * d4:LED 4:red
	 */

	switch (led_num) {
	case AGC_IR:
		sfh7832_data->iled1 = led_reg_val;	/* set ir */
		break;
	case AGC_RED:
		sfh7832_data->iled4 = led_reg_val;	/* set red */
		break;
	case AGC_GREEN:
		sfh7832_data->iled2 = led_reg_val;	/* set green */
		break;
	case AGC_BLUE:
		sfh7832_data->iled3 = led_reg_val;	/* set blue */
		break;
	}

	HRM_dbg("%s - Setting IR, Red, Green, Blue : %d, led_reg_val : %02x",
		__func__, led_num, led_reg_val);

	err = sfh7832_set_current(sfh7832_data->iled1, sfh7832_data->iled4, sfh7832_data->iled2, sfh7832_data->iled3, led_num);

	return err;
}

/* convert two's complement to signed integer in a symmetric fashion, i.e. with +0 and -0 */
int8_t _decode(uint8_t reg, uint8_t size)
{
	int8_t res;

	if (reg & (1 << (size-1)))
		/* res = -(~reg & ((1 << size) - 1) ); */
		res = -((~reg & ((1 << size) - 1))+1);
	else
		res = reg & ((1 << size) - 1);

	return res;
}

int sfh7832_reset_sdk_agc_var(int channel)
{
	int err = 0;

	switch (channel) {
	case 0:
		sfh7832_data->iled1 = 0x0;
		sfh7832_data->reached_thresh[AGC_IR] = 0;
		sfh7832_data->agc_sum[AGC_IR] = 0;
		sfh7832_data->agc_sample_cnt[AGC_IR] = 0;
		sfh7832_data->agc_current[AGC_IR] =
		    (sfh7832_data->iled1 * SFH7832_CURRENT_PER_STEP);
		break;
	case 1:
		sfh7832_data->iled4 = 0x0;
		sfh7832_data->reached_thresh[AGC_RED] = 0;
		sfh7832_data->agc_sum[AGC_RED] = 0;
		sfh7832_data->agc_sample_cnt[AGC_RED] = 0;
		sfh7832_data->agc_current[AGC_RED] =
		    (sfh7832_data->iled4 * SFH7832_CURRENT_PER_STEP);
		break;
	case 2:
		sfh7832_data->iled2 = 0x0;
		sfh7832_data->reached_thresh[AGC_GREEN] = 0;
		sfh7832_data->agc_sum[AGC_GREEN] = 0;
		sfh7832_data->agc_sample_cnt[AGC_GREEN] = 0;
		sfh7832_data->agc_current[AGC_GREEN] =
		    (sfh7832_data->iled2 * SFH7832_CURRENT_PER_STEP);
		break;
	case 3:
		sfh7832_data->iled3 = 0x0;
		sfh7832_data->reached_thresh[AGC_BLUE] = 0;
		sfh7832_data->agc_sum[AGC_BLUE] = 0;
		sfh7832_data->agc_sample_cnt[AGC_BLUE] = 0;
		sfh7832_data->agc_current[AGC_BLUE] =
		    (sfh7832_data->iled3 * SFH7832_CURRENT_PER_STEP);
		break;
	}
	err = sfh7832_set_current(sfh7832_data->iled1,
						sfh7832_data->iled4,
						sfh7832_data->iled2,
						sfh7832_data->iled3,
						channel);
	if (err != 0)
		return err;

	return err;

}

int linear_search_agc(int ppg_average, int led_num)
{
	int xtalk_adc = 0;
	int adc_full = 0;
	int target_adc = 0;
	long long min_base = SFH7832_MIN_DIODE_VAL + sfh7832_data->min_base_offset[led_num];
	long long target_current;
	int agc_range = SFH7832_AGC_ERROR_RANGE1;

	HRM_info("%s - start AGC\n", __func__);

	if (min_base > SFH7832_MAX_DIODE_VAL) {
		HRM_err("%s - error! min val : %lld", __func__, min_base);
		return CONSTRAINT_VIOLATION;
	}

	adc_full = SFH7832_MAX_DIODE_VAL - min_base;

	HRM_info("%s -(%d) min : %lld\n", __func__, led_num, min_base);

	target_adc = adc_full * sfh7832_data->agc_led_out_percent / 100 + min_base;

	HRM_info("%s, (%d) ppg_average : %d\n", __func__, led_num, ppg_average);

	if (ppg_average >= SFH7832_MAX_DIODE_VAL) {
		if (sfh7832_data->prev_agc_current[led_num] >=
		    sfh7832_data->agc_current[led_num])
			target_current = sfh7832_data->agc_current[led_num] / 2;
		else
			target_current =
			    (sfh7832_data->agc_current[led_num] +
			     sfh7832_data->prev_agc_current[led_num]) / 2;

		if (target_current > sfh7832_data->max_current)
			target_current = sfh7832_data->max_current;
		else if (target_current < SFH7832_MIN_CURRENT)
			target_current = SFH7832_MIN_CURRENT;

		sfh7832_data->prev_agc_current[led_num] = sfh7832_data->agc_current[led_num];
		sfh7832_data->change_led_by_absolute_count[led_num] =
		    target_current - sfh7832_data->agc_current[led_num];
		sfh7832_data->agc_current[led_num] = target_current;
		return 0;
	} else if (ppg_average <= min_base) {
		if (sfh7832_data->prev_agc_current[led_num] < sfh7832_data->agc_current[led_num])
			sfh7832_data->prev_agc_current[led_num] = sfh7832_data->max_current;
		target_current = (sfh7832_data->agc_current[led_num]
				  + sfh7832_data->prev_agc_current[led_num] +
				  SFH7832_CURRENT_PER_STEP) / 2;

		if (target_current > sfh7832_data->max_current)
			target_current = sfh7832_data->max_current;
		else if (target_current < SFH7832_MIN_CURRENT)
			target_current = SFH7832_MIN_CURRENT;

		sfh7832_data->prev_agc_current[led_num] = sfh7832_data->agc_current[led_num];
		sfh7832_data->change_led_by_absolute_count[led_num] =
		    target_current - sfh7832_data->agc_current[led_num];
		sfh7832_data->agc_current[led_num] = target_current;
		return 0;
	}

	if (sfh7832_data->reached_thresh[led_num]) {
		agc_range = SFH7832_AGC_ERROR_RANGE2;
		if (ppg_average <= (sfh7832_data->prev_agc_average[led_num] * 4 / 10)) {
			sfh7832_data->remove_cnt[led_num]++;
			if (sfh7832_data->remove_cnt[led_num] > 10) {
				sfh7832_data->remove_cnt[led_num] = 0;
				sfh7832_data->reached_thresh[led_num] = 0;
			}
		} else
			sfh7832_data->remove_cnt[led_num] = 0;
	}

	HRM_info("%s, (%d) target : %d, ppg_average : %d prev_average : %d reach_thd : %d\n",
		__func__, led_num, target_adc, ppg_average,
		sfh7832_data->prev_agc_average[led_num], sfh7832_data->reached_thresh[led_num]);

	HRM_info("%s - (%d) range (%d): %lld, %lld\n", __func__, led_num, agc_range,
		adc_full * (sfh7832_data->agc_led_out_percent + agc_range) / 100
			+ min_base,
			adc_full * (sfh7832_data->agc_led_out_percent - agc_range) / 100
			+ min_base);

	if (ppg_average <
	    adc_full * (sfh7832_data->agc_led_out_percent + agc_range) / 100
			+ min_base
		&& ppg_average >
		adc_full * (sfh7832_data->agc_led_out_percent - agc_range) / 100
			+ min_base) {
		sfh7832_data->reached_thresh[led_num] = 1;
		sfh7832_data->change_led_by_absolute_count[led_num] = 0;
		sfh7832_data->prev_agc_average[led_num] = ppg_average;
		return 0;
	}

	HRM_info("%s, (%d) target2 : %d\n", __func__, led_num, target_adc);

	target_current = target_adc - (ppg_average + xtalk_adc);

	if (sfh7832_data->agc_current[led_num] > sfh7832_data->prev_current[led_num])
		target_current *=
		    (sfh7832_data->agc_current[led_num] - sfh7832_data->prev_current[led_num]);
	else if (sfh7832_data->agc_current[led_num] < sfh7832_data->prev_current[led_num])
		target_current *=
		    (sfh7832_data->prev_current[led_num] - sfh7832_data->agc_current[led_num]);

	if (ppg_average + xtalk_adc > sfh7832_data->prev_ppg[led_num])
		target_current /= (ppg_average + xtalk_adc -
				   sfh7832_data->prev_ppg[led_num]);
	else if (ppg_average + xtalk_adc < sfh7832_data->prev_ppg[led_num])
		target_current /= (sfh7832_data->prev_ppg[led_num] -
				   (ppg_average + xtalk_adc));

	target_current += sfh7832_data->agc_current[led_num];

	if (target_current > sfh7832_data->max_current)
		target_current = sfh7832_data->max_current;
	else if (target_current < SFH7832_MIN_CURRENT)
		target_current = SFH7832_MIN_CURRENT;

	sfh7832_data->change_led_by_absolute_count[led_num] =
	    target_current - sfh7832_data->agc_current[led_num];

	sfh7832_data->prev_current[led_num] = sfh7832_data->agc_current[led_num];
	sfh7832_data->agc_current[led_num] = target_current;
	sfh7832_data->prev_ppg[led_num] = ppg_average + xtalk_adc;

	HRM_info("%s, sfh7832_data->agc_current[%d] : %u, %lld\n",
		 __func__, led_num, sfh7832_data->agc_current[led_num], target_current);

	return 0;

}

int sfh7832_agc(int counts, int led_num)
{
	int err = 0;
	int avg = 0;

	/* HRM_info("%s - led_num(%d) = %d, %d\n", __func__, led_num,
	 * counts, sfh7832_data->agc_sample_cnt[led_num]);
	 */
	if (led_num < AGC_IR || led_num > AGC_BLUE)
		return -EIO;

	sfh7832_data->agc_sum[led_num] += counts;
	sfh7832_data->agc_sample_cnt[led_num]++;

	if (sfh7832_data->agc_sample_cnt[led_num] < sfh7832_data->agc_min_num_samples)
		return 0;

	sfh7832_data->agc_sample_cnt[led_num] = 0;

	avg = sfh7832_data->agc_sum[led_num] / sfh7832_data->agc_min_num_samples;
	sfh7832_data->agc_sum[led_num] = 0;
	/* HRM_info("%s - led_num(%d) = %d, %d\n", __func__, led_num, counts, avg); */

	err = linear_search_agc(avg, led_num);
	if (err != 0)
		return err;

	/* HRM_info("%s - %d-a:%d\n", __func__, led_num,
	 * sfh7832_data->change_led_by_absolute_count[led_num]);
	 */

	if (sfh7832_data->change_led_by_absolute_count[led_num] == 0) {
		/*
		 * HRM_info("%s - ir_curr = 0x%2x, red_curr = 0x%2x, ir_adc = %d, red_adc = %d\n",
		 *      __func__, data->ir_curr, data->red_curr, data->ir_adc, data->red_adc);
		 */
		return 0;
	}

	err = sfh7832_data->update_led(sfh7832_data->agc_current[led_num], led_num);
	if (err)
		HRM_err("%s failed\n", __func__);

	/* skip 2 sample after changing current */
	sfh7832_data->agc_enabled = 0;
	sfh7832_data->sample_cnt = AGC_SKIP_CNT - 2;

	return err;
}

static int sfh7832_cal_agc(int ir, int red, int green, int blue)
{
	int err = 0;
	int led_num0, led_num1, led_num2, led_num3;

	led_num0 = AGC_IR;
	led_num1 = AGC_RED;
	led_num2 = AGC_GREEN;
	led_num3 = AGC_BLUE;

	if (sfh7832_data->agc_mode == M_HRM) {
		if (ir >= sfh7832_data->hrm_threshold) {	/* Detect */
			err = sfh7832_agc(ir, led_num0);	/* IR */
			if (err != 0)
				HRM_dbg("%s failed sfh7832_agc\n", __func__);

			if (sfh7832_data->agc_current[led_num1] !=
			    SFH7832_MIN_CURRENT) {
				err = sfh7832_agc(red, led_num1);	/* RED */
				if (err != 0)
					HRM_dbg("%s failed sfh7832_agc\n", __func__);
			} else {
				/* init */
				sfh7832_data->agc_current[led_num1]
				    = sfh7832_data->init_iled[AGC_RED] *
				    SFH7832_CURRENT_PER_STEP;
				sfh7832_data->agc_enabled = 0;
				sfh7832_data->sample_cnt = 0;
				sfh7832_data->reached_thresh[led_num1] = 0;
				sfh7832_data->prev_current[led_num1] = 0;
				sfh7832_data->prev_agc_current[led_num1] =
				    sfh7832_data->max_current;
				sfh7832_data->prev_ppg[led_num1] = 0;
				sfh7832_data->agc_sum[led_num1] = 0;
				sfh7832_data->agc_sample_cnt[led_num1] = 0;
				err = sfh7832_data->update_led(sfh7832_data->agc_current[led_num1],
							led_num1);
				if (err != 0)
					HRM_dbg("%s failed update_led\n", __func__);
			}
		} else {
			if (sfh7832_data->agc_current[led_num1] != SFH7832_MIN_CURRENT) {	/* Disable RED */
				/* init */
				sfh7832_data->agc_current[led_num1] =
				    SFH7832_MIN_CURRENT;
				sfh7832_data->reached_thresh[led_num1] = 0;
				sfh7832_data->prev_current[led_num1] = 0;
				sfh7832_data->prev_agc_current[led_num1] =
				    sfh7832_data->max_current;
				sfh7832_data->prev_ppg[led_num1] = 0;
				sfh7832_data->agc_sum[led_num1] = 0;
				sfh7832_data->agc_sample_cnt[led_num1] = 0;
				err = sfh7832_data->update_led(sfh7832_data->agc_current[led_num1],
							led_num1);
				if (err != 0)
					HRM_dbg("%s failed update_led\n", __func__);

				/* init */
				sfh7832_data->agc_current[led_num0]
				    = sfh7832_data->init_iled[AGC_IR] *
				    SFH7832_CURRENT_PER_STEP;
				sfh7832_data->agc_enabled = 0;
				sfh7832_data->sample_cnt = 0;
				sfh7832_data->reached_thresh[led_num0] = 0;
				sfh7832_data->prev_current[led_num0] = 0;
				sfh7832_data->prev_agc_current[led_num0] =
				    sfh7832_data->max_current;
				sfh7832_data->prev_ppg[led_num0] = 0;
				sfh7832_data->agc_sum[led_num0] = 0;
				sfh7832_data->agc_sample_cnt[led_num0] = 0;
				err = sfh7832_data->update_led(sfh7832_data->agc_current[led_num0],
							led_num0);
				if (err != 0)
					HRM_dbg("%s failed update_led\n", __func__);
			}
		}
	} else if (sfh7832_data->agc_mode == M_SDK) {
		if (sfh7832_data->mode_sdk_enabled & 0x01) {	/* IR channel */
			if (sfh7832_data->agc_current[led_num0] !=
			    SFH7832_MIN_CURRENT) {
				err = sfh7832_agc(ir, led_num0);
				if (err != 0)
					HRM_dbg("%s failed sfh7832_agc\n", __func__);
			} else {
				sfh7832_data->agc_current[led_num0]
				    = sfh7832_data->init_iled[AGC_IR] *
				    SFH7832_CURRENT_PER_STEP;
				sfh7832_data->agc_enabled = 0;
				sfh7832_data->sample_cnt = 0;
				sfh7832_data->reached_thresh[led_num0] = 0;
				sfh7832_data->prev_current[led_num0] = 0;
				sfh7832_data->prev_agc_current[led_num0] =
				    sfh7832_data->max_current;
				sfh7832_data->prev_ppg[led_num0] = 0;
				sfh7832_data->agc_sum[led_num0] = 0;
				sfh7832_data->agc_sample_cnt[led_num0] = 0;
				err = sfh7832_data->update_led(sfh7832_data->agc_current[led_num0],
							led_num0);
				if (err != 0)
					HRM_dbg("%s failed update_led\n", __func__);
			}
		}
		if (sfh7832_data->mode_sdk_enabled & 0x02) {	/* RED channel */
			if (sfh7832_data->agc_current[led_num1] !=
			    SFH7832_MIN_CURRENT) {
				err = sfh7832_agc(red, led_num1);
				if (err != 0)
					HRM_dbg("%s failed sfh7832_agc\n", __func__);
			} else {
				sfh7832_data->agc_current[led_num1]
				    = sfh7832_data->init_iled[AGC_RED] *
				    SFH7832_CURRENT_PER_STEP;
				sfh7832_data->agc_enabled = 0;
				sfh7832_data->sample_cnt = 0;
				sfh7832_data->reached_thresh[led_num1] = 0;
				sfh7832_data->prev_current[led_num1] = 0;
				sfh7832_data->prev_agc_current[led_num1] =
				    sfh7832_data->max_current;
				sfh7832_data->prev_ppg[led_num1] = 0;
				sfh7832_data->agc_sum[led_num1] = 0;
				sfh7832_data->agc_sample_cnt[led_num1] = 0;
				err = sfh7832_data->update_led(sfh7832_data->agc_current[led_num1],
							led_num1);
				if (err != 0)
					HRM_dbg("%s failed update_led\n", __func__);
			}
		}
		if (sfh7832_data->mode_sdk_enabled & 0x04) {	/* Green channel */
			if (sfh7832_data->agc_current[led_num2] !=
			    SFH7832_MIN_CURRENT) {
				err = sfh7832_agc(green, led_num2);
				if (err != 0)
					HRM_dbg("%s failed sfh7832_agc\n", __func__);
			} else {
				sfh7832_data->agc_current[led_num2]
				    = sfh7832_data->init_iled[AGC_GREEN] *
				    SFH7832_CURRENT_PER_STEP;
				sfh7832_data->agc_enabled = 0;
				sfh7832_data->sample_cnt = 0;
				sfh7832_data->reached_thresh[led_num2] = 0;
				sfh7832_data->prev_current[led_num2] = 0;
				sfh7832_data->prev_agc_current[led_num2] =
				    sfh7832_data->max_current;
				sfh7832_data->prev_ppg[led_num2] = 0;
				sfh7832_data->agc_sum[led_num2] = 0;
				sfh7832_data->agc_sample_cnt[led_num2] = 0;
				err = sfh7832_data->update_led(sfh7832_data->agc_current[led_num2],
							led_num2);
				if (err != 0)
					HRM_dbg("%s failed update_led\n", __func__);
			}
		}
		if (sfh7832_data->mode_sdk_enabled & 0x08) {	/* BLUE channel */
			if (sfh7832_data->agc_current[led_num3] !=
			    SFH7832_MIN_CURRENT) {
				err = sfh7832_agc(blue, led_num3);
				if (err != 0)
					HRM_dbg("%s failed sfh7832_agc\n", __func__);
			} else {
				sfh7832_data->agc_current[led_num3]
				    = sfh7832_data->init_iled[AGC_BLUE] *
				    SFH7832_CURRENT_PER_STEP;
				sfh7832_data->agc_enabled = 0;
				sfh7832_data->sample_cnt = 0;
				sfh7832_data->reached_thresh[led_num3] = 0;
				sfh7832_data->prev_current[led_num3] = 0;
				sfh7832_data->prev_agc_current[led_num3] =
				    sfh7832_data->max_current;
				sfh7832_data->prev_ppg[led_num3] = 0;
				sfh7832_data->agc_sum[led_num3] = 0;
				sfh7832_data->agc_sample_cnt[led_num3] = 0;
				err = sfh7832_data->update_led(sfh7832_data->agc_current[led_num3],
							led_num3);
				if (err != 0)
					HRM_dbg("%s failed update_led\n", __func__);
			}
		}
	}

	return err;
}

int sfh7832_sw_reset(void)
{
	int err = 0;

	err = sfh7832_write_reg(AFE4420_CONTROL0, 1 << SW_RESET);	/* SW_RESET */
	if (err != 0)
		return err;
	usleep_range(1000, 1100);
	err = sfh7832_write_reg(AFE4420_INT_MUX, (0x1 << 5) |
		((sfh7832_data->fifo_depth * sfh7832_data->n_phase - 1) << 6));
	if (err != 0)
		return err;

	err = sfh7832_write_reg(AFE4420_CONTROL0, 1 << TM_COUNT_RST);
	if (err != 0)
		return err;

	err = sfh7832_write_reg(AFE4420_TE_PRPCT, 10);
	if (err != 0)
		return err;

	err = sfh7832_write_reg(AFE4420_CONTROL0, 0 << TM_COUNT_RST);
	if (err != 0)
		return err;
	usleep_range(100, 110);

	err = sfh7832_write_reg(AFE4420_CONTROL0, 1 << TM_COUNT_RST);

	return err;
}

int sfh7832_reg_init(void)
{
	int err = 0;
	int i = 0;
	u32 recvData;

	err = sfh7832_sw_reset();
	if (err != 0) {
		HRM_dbg("%s failed sfh7832_sw_reset\n", __func__);
		return err;
	}

	err = sfh7832_write_reg(AFE4420_TE_PRPCT, sfh7832_data->prf_count);	/* PRPCT (PRF value) */
	if (err != 0)
		return err;

	/* adc noise workaround */
	err = sfh7832_read_reg(AFE4420_BIAS_OTP, &recvData);
	if (err != 0)
		return err;
	if (!((recvData >> 7) & 1)) {
		HRM_dbg("%s 0x27 : 0x%x\n", __func__, recvData);
		err = sfh7832_write_reg(AFE4420_BIAS_REG, 0x10);
		if (err != 0)
			return err;
	}

	while (afe4420_reg[i][0] != AFE4420_MAX_REG) {
		err = sfh7832_write_reg(afe4420_reg[i][0], afe4420_reg[i][1]);
		if (err != 0)
			return err;
		i++;

	}
	return err;
}

void sfh7832_reset_numav(void)
{
	afe4420_phase13_reg[PHASE3_INDEX + 1][1] &= NUMAV_MASK;
	afe4420_phase13_reg[PHASE5_INDEX + 1][1] &= NUMAV_MASK;
	afe4420_phase13_reg[PHASE7_INDEX + 1][1] &= NUMAV_MASK;
	afe4420_phase13_reg[PHASE9_INDEX + 1][1] &= NUMAV_MASK;
	afe4420_phase13_reg[PHASE11_INDEX + 1][1] &= NUMAV_MASK;
	afe4420_phase13_reg[PHASE13_INDEX + 1][1] &= NUMAV_MASK;
}

void sfh7832_reset_fifo_data_ctl(void)
{
	afe4420_phase13_reg[PHASE3_INDEX + 2][1] |=
		(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);
	afe4420_phase13_reg[PHASE5_INDEX + 2][1] |=
		(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);
	afe4420_phase13_reg[PHASE7_INDEX + 2][1] |=
		(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);
	afe4420_phase13_reg[PHASE9_INDEX + 2][1] |=
		(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);
	afe4420_phase13_reg[PHASE11_INDEX + 2][1] |=
		(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);
	afe4420_phase13_reg[PHASE13_INDEX + 2][1] |=
		(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);
}

void sfh7832_reset_tia_gain(void)
{
	afe4420_phase13_reg[PHASE3_INDEX + 1][1] &= TIA_RF_MASK;
	afe4420_phase13_reg[PHASE5_INDEX + 1][1] &= TIA_RF_MASK;
	afe4420_phase13_reg[PHASE7_INDEX + 1][1] &= TIA_RF_MASK;
	afe4420_phase13_reg[PHASE9_INDEX + 1][1] &= TIA_RF_MASK;
	afe4420_phase13_reg[PHASE11_INDEX + 1][1] &= TIA_RF_MASK;
	afe4420_phase13_reg[PHASE13_INDEX + 1][1] &= TIA_RF_MASK;
}

int sfh7832_timer_rst_fifo_en(void)
{
	int err = 0;

	/* TM_COUNT_RST = 0, FIFO_EN = 1 */
	err = sfh7832_write_reg(AFE4420_CONTROL0, 1 << FIFO_EN);

	return err;
}

int sfh7832_set_phase_num(u8 num)
{
	int err = 0;

	if (num <= 0)
		num = 1;

	err = sfh7832_write_reg(AFE4420_NR_RESET_NUMPHASE, num - 1);

	return err;
}

int sfh7832_AACM_enable(void)
{
	int err = 0;
	u32 aacm_reg = 0;

	/*EN_AACM_GBL enable */
	err = sfh7832_read_reg(AFE4420_CONTROL1, &aacm_reg);
	if (err != 0)
		return err;

	aacm_reg |= (1 << EN_AACM_GBL);

	err = sfh7832_write_reg(AFE4420_CONTROL1, aacm_reg);
	if (err != 0)
		return err;

	/*PD1 EN_AACM_PD1 enable */
	err = sfh7832_read_reg(AFE4420_PDCNTRL0(0), &aacm_reg);
	if (err != 0)
		return err;

	aacm_reg |= 0x1;

	err = sfh7832_write_reg(AFE4420_PDCNTRL0(0), aacm_reg);
	if (err != 0)
		return err;

	return err;

}

int sfh7832_AACM_disable(void)
{
	int err = 0;
	u32 aacm_reg = 0;

	/*EN_AACM_GBL disable */
	err = sfh7832_read_reg(AFE4420_CONTROL1, &aacm_reg);
	if (err != 0)
		return err;

	aacm_reg &= ~(1 << EN_AACM_GBL);

	err = sfh7832_write_reg(AFE4420_CONTROL1, aacm_reg);
	if (err != 0)
		return err;


	/*PD1 EN_AACM_PD1 disable */
	err = sfh7832_read_reg(AFE4420_PDCNTRL0(0), &aacm_reg);
	if (err != 0)
		return err;

	aacm_reg &= ~(0x1);

	err = sfh7832_write_reg(AFE4420_PDCNTRL0(0), aacm_reg);
	if (err != 0)
		return err;

	return err;
}

int sfh7832_update_scan_mode(enum op_mode mode)
{

	int err = 0, i = 0, phase_num = 0;

	err = sfh7832_set_ioff_dac(sfh7832_data->ioff_dac);
	if (err != 0) {
		HRM_dbg("%s -sfh7832_set_ioff_dac fail : %d\n", __func__, err);
		return err;
	}
	err = sfh7832_AACM_enable();
	if (err != 0) {
		HRM_dbg("%s -sfh7832_AACM_enable fail : %d\n", __func__, err);
		return err;
	}

	/* default phase = 13 */
	if (mode == MODE_HRM) {
		afe4420_phase13_reg[PHASE3_INDEX][1] = LED_AMBIENT;
		afe4420_phase13_reg[PHASE5_INDEX][1] = LED_IR_200MA_DRV;
		afe4420_phase13_reg[PHASE7_INDEX][1] = LED_RED_200MA_DRV;
		afe4420_phase13_reg[PHASE9_INDEX][1] = LED_AMBIENT;

		sfh7832_reset_numav();
		afe4420_phase13_reg[PHASE3_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);
		afe4420_phase13_reg[PHASE5_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);
		afe4420_phase13_reg[PHASE7_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);
		afe4420_phase13_reg[PHASE9_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);

		sfh7832_reset_tia_gain();
		afe4420_phase13_reg[PHASE3_INDEX + 1][1] |=
			(sfh7832_data->rfgain_amb1 << TIA_GAIN_RF);
		afe4420_phase13_reg[PHASE5_INDEX + 1][1] |=
			(sfh7832_data->rfgain_led1 << TIA_GAIN_RF);
		afe4420_phase13_reg[PHASE7_INDEX + 1][1] |=
			(sfh7832_data->rfgain_led2 << TIA_GAIN_RF);
		afe4420_phase13_reg[PHASE9_INDEX + 1][1] |=
			(sfh7832_data->rfgain_amb1 << TIA_GAIN_RF);

		sfh7832_reset_fifo_data_ctl();

		afe4420_phase13_reg[PHASE3_INDEX + 2][1] &=
		    ~(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);
		afe4420_phase13_reg[PHASE5_INDEX + 2][1] &=
		    ~(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);
		afe4420_phase13_reg[PHASE7_INDEX + 2][1] &=
		    ~(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);
		afe4420_phase13_reg[PHASE9_INDEX + 2][1] &=
		    ~(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);

		phase_num = 9;
	} else if (mode == MODE_PROX) {	/* dummy, IR */
		/*turn off LED4(Red), LED2(Green), LED3(Blue) */
		afe4420_phase13_reg[PHASE3_INDEX][1] = LED_AMBIENT;
		afe4420_phase13_reg[PHASE5_INDEX][1] = LED_IR_200MA_DRV;

		sfh7832_reset_numav();
		afe4420_phase13_reg[PHASE3_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);
		afe4420_phase13_reg[PHASE5_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);

		sfh7832_reset_tia_gain();
		afe4420_phase13_reg[PHASE3_INDEX + 1][1] |=
			(sfh7832_data->rfgain_amb1 << TIA_GAIN_RF);
		afe4420_phase13_reg[PHASE5_INDEX + 1][1] |=
			(sfh7832_data->rfgain_led1 << TIA_GAIN_RF);

		sfh7832_reset_fifo_data_ctl();
		afe4420_phase13_reg[PHASE3_INDEX + 2][1] &=
		    ~(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);
		afe4420_phase13_reg[PHASE5_INDEX + 2][1] &=
		    ~(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);

		phase_num = 5;
	} else if (mode == MODE_SDK_IR) {	/* dummy,IR, Red, Green, Blue */
		if (sfh7832_data->eol_test_is_enable != 0) {
			err = sfh7832_AACM_disable();
			if (err != 0) {
				HRM_dbg("%s - sfh7832_AACM_disable fail\n", __func__);
				return err;
			}
		}

		/*turn on LED1(IR), LED4(Red), LED2(Green), LED3(Blue) */
		afe4420_phase13_reg[PHASE3_INDEX][1] = LED_AMBIENT;
		afe4420_phase13_reg[PHASE5_INDEX][1] = LED_IR_200MA_DRV;
		afe4420_phase13_reg[PHASE7_INDEX][1] = LED_RED_200MA_DRV;
		afe4420_phase13_reg[PHASE9_INDEX][1] = LED_AMBIENT;
		afe4420_phase13_reg[PHASE11_INDEX][1] = LED_GREEN_200MA_DRV;
		afe4420_phase13_reg[PHASE13_INDEX][1] = LED_BLUE_200MA_DRV;

		sfh7832_reset_numav();
		afe4420_phase13_reg[PHASE3_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);
		afe4420_phase13_reg[PHASE5_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);
		afe4420_phase13_reg[PHASE7_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);
		afe4420_phase13_reg[PHASE9_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);
		afe4420_phase13_reg[PHASE11_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);
		afe4420_phase13_reg[PHASE13_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);

		sfh7832_reset_tia_gain();
		afe4420_phase13_reg[PHASE3_INDEX + 1][1] |=
			(sfh7832_data->rfgain_amb1 << TIA_GAIN_RF);
		afe4420_phase13_reg[PHASE5_INDEX + 1][1] |=
			(sfh7832_data->rfgain_led1 << TIA_GAIN_RF);
		afe4420_phase13_reg[PHASE7_INDEX + 1][1] |=
			(sfh7832_data->rfgain_led2 << TIA_GAIN_RF);
		afe4420_phase13_reg[PHASE9_INDEX + 1][1] |=
			(sfh7832_data->rfgain_amb2 << TIA_GAIN_RF);
		afe4420_phase13_reg[PHASE11_INDEX + 1][1] |=
			(sfh7832_data->rfgain_led3 << TIA_GAIN_RF);
		afe4420_phase13_reg[PHASE13_INDEX + 1][1] |=
			(sfh7832_data->rfgain_led4 << TIA_GAIN_RF);

		sfh7832_reset_fifo_data_ctl();
		afe4420_phase13_reg[PHASE3_INDEX + 2][1] &=
		    ~(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);
		afe4420_phase13_reg[PHASE5_INDEX + 2][1] &=
		    ~(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);
		afe4420_phase13_reg[PHASE7_INDEX + 2][1] &=
		    ~(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);
		afe4420_phase13_reg[PHASE9_INDEX + 2][1] &=
		    ~(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);
		afe4420_phase13_reg[PHASE11_INDEX + 2][1] &=
		    ~(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);
		afe4420_phase13_reg[PHASE13_INDEX + 2][1] &=
		    ~(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);

		phase_num = 13;
	} else if (mode == MODE_SVC_IR) {
		afe4420_phase13_reg[PHASE3_INDEX][1] = LED_AMBIENT;
		afe4420_phase13_reg[PHASE5_INDEX][1] = LED_IR_200MA_DRV;
		afe4420_phase13_reg[PHASE7_INDEX][1] = LED_RED_200MA_DRV;
		afe4420_phase13_reg[PHASE9_INDEX][1] = LED_AMBIENT;
		afe4420_phase13_reg[PHASE11_INDEX][1] = LED_GREEN_200MA_DRV;
		afe4420_phase13_reg[PHASE13_INDEX][1] = LED_BLUE_200MA_DRV;
	
		sfh7832_reset_numav();
		afe4420_phase13_reg[PHASE3_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);
		afe4420_phase13_reg[PHASE5_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);
		afe4420_phase13_reg[PHASE7_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);
		afe4420_phase13_reg[PHASE9_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);
		afe4420_phase13_reg[PHASE11_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);
		afe4420_phase13_reg[PHASE13_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);
	
		sfh7832_reset_tia_gain();
		afe4420_phase13_reg[PHASE3_INDEX + 1][1] |=
			(sfh7832_data->rfgain_amb1 << TIA_GAIN_RF);
		afe4420_phase13_reg[PHASE5_INDEX + 1][1] |=
			(sfh7832_data->rfgain_led1 << TIA_GAIN_RF);
		afe4420_phase13_reg[PHASE7_INDEX + 1][1] |=
			(sfh7832_data->rfgain_led2 << TIA_GAIN_RF);
		afe4420_phase13_reg[PHASE9_INDEX + 1][1] |=
			(sfh7832_data->rfgain_amb2 << TIA_GAIN_RF);
		afe4420_phase13_reg[PHASE11_INDEX + 1][1] |=
			(sfh7832_data->rfgain_led3 << TIA_GAIN_RF);
		afe4420_phase13_reg[PHASE13_INDEX + 1][1] |=
			(sfh7832_data->rfgain_led4 << TIA_GAIN_RF);
		sfh7832_reset_fifo_data_ctl();

		phase_num = 13;
	} else if (mode == MODE_AMBIENT) {	/* dummy, Ambient */
		err = sfh7832_AACM_disable();
		if (err != 0) {
			HRM_dbg("%s - sfh7832_AACM_disable fail\n", __func__);
			return err;
		}

		afe4420_phase13_reg[PHASE3_INDEX][1] = LED_AMBIENT;

		sfh7832_reset_numav();
		afe4420_phase13_reg[PHASE3_INDEX + 1][1] |= (sfh7832_data->numav & 0xf);

		sfh7832_reset_tia_gain();
		afe4420_phase13_reg[PHASE3_INDEX + 1][1] |=
			(sfh7832_data->rfgain_amb1 << TIA_GAIN_RF);

		sfh7832_reset_fifo_data_ctl();
		afe4420_phase13_reg[PHASE3_INDEX + 2][1] &=
		    ~(NO_DATA << PCNTRL2_FIFO_DATA_CTRL);

		phase_num = 3;
	} else {
		HRM_dbg("%s - MODE_UNKNOWN\n", __func__);
		return -1;
	}

	while (afe4420_phase13_reg[i][0] != AFE4420_MAX_REG) {
		err = sfh7832_write_reg(afe4420_phase13_reg[i][0],
				    afe4420_phase13_reg[i][1]);
		if (err != 0)
			return err;
		i++;
	}

	err = sfh7832_set_phase_num(phase_num);
	if (err != 0) {
		HRM_dbg("%s - error setting sfh7832_set_phase_num!\n", __func__);
		return err;
	}

	err = sfh7832_set_ioffset(sfh7832_data->ioffset_led1, sfh7832_data->ioffset_led2,
				sfh7832_data->ioffset_led3, sfh7832_data->ioffset_led4);
	if (err != 0) {
		HRM_dbg("%s - error setting sfh7832_set_ioffset!\n", __func__);
		return err;
	}

	err = sfh7832_set_cf(sfh7832_data->cfgain_amb1);
	if (err != 0) {
		HRM_dbg("%s - error setting sfh7832_set_cf!\n", __func__);
		return err;
	}

	return err;

}

void sfh7832_set_param_hrm(void)
{
	int i = 0;

	sfh7832_data->agc_enabled = 0;
	sfh7832_data->prf_count = SFH7832_PRF_100HZ;
	sfh7832_data->fifo_depth = 1;
	sfh7832_data->rfgain_amb1 = TIA_GAIN_25K;
	sfh7832_data->rfgain_amb2 = TIA_GAIN_50K;
	sfh7832_data->rfgain_led1 = TIA_GAIN_25K;
	sfh7832_data->rfgain_led2 = TIA_GAIN_25K;
	sfh7832_data->rfgain_led3 = TIA_GAIN_50K;
	sfh7832_data->rfgain_led4 = TIA_GAIN_50K;

	sfh7832_data->sample_cnt = 0;
	sfh7832_data->iled1 = sfh7832_data->init_iled[AGC_IR];	/* IR */
	sfh7832_data->iled2 = 0x00;	/* GREEN */
	sfh7832_data->iled3 = 0x00;	/* BLUE */
	sfh7832_data->iled4 = 0x00;	/* RED */
	sfh7832_data->trim_led1 = 0x00;
	sfh7832_data->trim_led2 = 0x00;
	sfh7832_data->trim_led3 = 0x00;
	sfh7832_data->trim_led4 = 0x00;

	/* AGC control */
	sfh7832_data->agc_current[AGC_IR] = (sfh7832_data->iled1 * SFH7832_CURRENT_PER_STEP);
	sfh7832_data->agc_current[AGC_RED] = (sfh7832_data->iled4 * SFH7832_CURRENT_PER_STEP);
	sfh7832_data->agc_current[AGC_GREEN] = 0;
	sfh7832_data->agc_current[AGC_BLUE] = 0;

	for (i = 0; i < 2; i++) {
		sfh7832_data->reached_thresh[i] = 0;
		sfh7832_data->agc_sum[i] = 0;
		sfh7832_data->prev_current[i] = 0;
		sfh7832_data->prev_agc_current[i] = sfh7832_data->max_current;
		sfh7832_data->prev_ppg[i] = 0;
	}
}

void sfh7832_set_param_sdk(void)
{
	int i = 0;

	sfh7832_data->agc_mode = M_SDK;
	sfh7832_data->agc_enabled = 1;

	sfh7832_data->prf_count = SFH7832_PRF_100HZ;
	sfh7832_data->fifo_depth = 1;
	sfh7832_data->n_phase = 6;
	sfh7832_data->rfgain_amb1 = TIA_GAIN_25K;
	sfh7832_data->rfgain_amb2 = TIA_GAIN_50K;
	sfh7832_data->rfgain_led1 = TIA_GAIN_25K;
	sfh7832_data->rfgain_led2 = TIA_GAIN_25K;
	sfh7832_data->rfgain_led3 = TIA_GAIN_50K;
	sfh7832_data->rfgain_led4 = TIA_GAIN_50K;

	sfh7832_data->sample_cnt = 0;
	sfh7832_data->iled1 = 0x00;	/* IR */
	sfh7832_data->iled2 = 0x00;	/* green */
	sfh7832_data->iled3 = 0x00;	/* blue */
	sfh7832_data->iled4 = 0x00;	/* Red */
	sfh7832_data->trim_led1 = 0x00;
	sfh7832_data->trim_led2 = 0x00;
	sfh7832_data->trim_led3 = 0x00;
	sfh7832_data->trim_led4 = 0x00;
	sfh7832_data->num_samples = 4;

	/* AGC control */
	sfh7832_data->agc_current[AGC_IR] = (sfh7832_data->iled1 * SFH7832_CURRENT_PER_STEP);
	sfh7832_data->agc_current[AGC_GREEN] = (sfh7832_data->iled2 * SFH7832_CURRENT_PER_STEP);
	sfh7832_data->agc_current[AGC_BLUE] = (sfh7832_data->iled3 * SFH7832_CURRENT_PER_STEP);
	sfh7832_data->agc_current[AGC_RED] = (sfh7832_data->iled4 * SFH7832_CURRENT_PER_STEP);

	for (i = 0; i < sfh7832_data->num_samples; i++) {
		sfh7832_data->reached_thresh[i] = 0;
		sfh7832_data->agc_sum[i] = 0;
		sfh7832_data->prev_current[i] = 0;
		sfh7832_data->prev_agc_current[i] = sfh7832_data->max_current;
		sfh7832_data->prev_ppg[i] = 0;
	}
}

void sfh7832_set_param_ambient(void)
{
	sfh7832_data->agc_mode = M_NONE;
	sfh7832_data->agc_enabled = 0;

	sfh7832_data->prf_count = SFH7832_PRF_400HZ;
	sfh7832_data->fifo_depth = 20;
	sfh7832_data->n_phase = 1;
	sfh7832_data->rfgain_amb1 = TIA_GAIN_50K;
	sfh7832_data->rfgain_amb2 = TIA_GAIN_50K;
	sfh7832_data->rfgain_led1 = TIA_GAIN_50K;
	sfh7832_data->rfgain_led2 = TIA_GAIN_50K;
	sfh7832_data->rfgain_led3 = TIA_GAIN_50K;
	sfh7832_data->rfgain_led4 = TIA_GAIN_50K;

	sfh7832_data->sample_cnt = 0;
	sfh7832_data->flicker_data_cnt = 0;
	sfh7832_data->iled1 = 0x00;	/* IR */
	sfh7832_data->iled2 = 0x00;	/* green */
	sfh7832_data->iled3 = 0x00;	/* blue */
	sfh7832_data->iled4 = 0x00;	/* Red */
	sfh7832_data->trim_led1 = 0x00;
	sfh7832_data->trim_led2 = 0x00;
	sfh7832_data->trim_led3 = 0x00;
	sfh7832_data->trim_led4 = 0x00;
}

int sfh7832_hrm_enable(void)
{
	int err = 0;

	sfh7832_set_param_hrm();
	sfh7832_data->n_phase = 4;
	sfh7832_data->agc_mode = M_HRM;

	err = sfh7832_reg_init();
	if (err != 0) {
		HRM_dbg("%s - sfh7832_reg_init fail!\n", __func__);
		return err;
	}
	err = sfh7832_update_scan_mode(MODE_HRM);
	if (err != 0) {
		HRM_dbg("%s - sfh7832_update_scan_mode fail!\n", __func__);
		return err;
	}

	err = sfh7832_timer_rst_fifo_en();
	if (err != 0) {
		HRM_dbg("%s - sfh7832_timer_rst_fifo_en fail!\n", __func__);
		return err;
	}

	err = sfh7832_set_current(sfh7832_data->iled1, sfh7832_data->iled4, sfh7832_data->iled2, sfh7832_data->iled3, LED_ALL);
	if (err != 0)
		HRM_dbg("%s - sfh7832_set_current fail!\n", __func__);

	return err;
}

int sfh7832_prox_enable(void)
{
	int err = 0;

	sfh7832_set_param_hrm();
	sfh7832_data->n_phase = 2;
	sfh7832_data->agc_mode = M_NONE;

	err = sfh7832_reg_init();
	if (err != 0) {
		HRM_dbg("%s - sfh7832_reg_init fail!\n", __func__);
		return err;
	}
	err = sfh7832_update_scan_mode(MODE_PROX);
	if (err != 0) {
		HRM_dbg("%s - sfh7832_update_scan_mode fail!\n", __func__);
		return err;
	}
	err = sfh7832_timer_rst_fifo_en();
	if (err != 0) {
		HRM_dbg("%s - sfh7832_timer_rst_fifo_en fail!\n", __func__);
		return err;
	}

	err = sfh7832_set_current(0xff, 0, 0, 0, LED_ALL);
	if (err != 0)
		HRM_dbg("%s - sfh7832_set_current fail!\n", __func__);

	return err;

}

int sfh7832_sdk_enable(void)
{
	int err = 0;
	
	sfh7832_set_param_sdk();

	err = sfh7832_reg_init();
	if (err != 0) {
		HRM_dbg("%s - sfh7832_reg_init fail!\n", __func__);
		return err;
	}
	err = sfh7832_update_scan_mode(MODE_SDK_IR);
	if (err != 0) {
		HRM_dbg("%s - sfh7832_update_scan_mode fail!\n", __func__);
		return err;
	}

	err = sfh7832_timer_rst_fifo_en();
	if (err != 0) {
		HRM_dbg("%s - sfh7832_timer_rst_fifo_en fail!\n", __func__);
		return err;
	}

	err = sfh7832_set_current(sfh7832_data->iled1, sfh7832_data->iled4, sfh7832_data->iled2, sfh7832_data->iled3, LED_ALL);
	if (err != 0)
		HRM_dbg("%s - sfh7832_set_current fail!\n", __func__);

	return err;
}

int sfh7832_svc_led_enable(void)
{
	int err = 0;
	
	sfh7832_set_param_sdk();
	sfh7832_data->agc_mode = M_NONE;

	err = sfh7832_reg_init();
	if (err != 0) {
		HRM_dbg("%s - sfh7832_reg_init fail!\n", __func__);
		return err;
	}
	err = sfh7832_update_scan_mode(MODE_SVC_IR);
	if (err != 0) {
		HRM_dbg("%s - sfh7832_update_scan_mode fail!\n", __func__);
		return err;
	}

	err = sfh7832_timer_rst_fifo_en();
	if (err != 0) {
		HRM_dbg("%s - sfh7832_timer_rst_fifo_en fail!\n", __func__);
		return err;
	}

	err = sfh7832_set_current(sfh7832_data->iled1, sfh7832_data->iled4, sfh7832_data->iled2, sfh7832_data->iled3, LED_ALL);
	if (err != 0)
		HRM_dbg("%s - sfh7832_set_current fail!\n", __func__);

	return err;
}

int sfh7832_ambient_enable(void)
{
	int err = 0;

	sfh7832_set_param_ambient();

	err = sfh7832_reg_init();
	if (err != 0) {
		HRM_dbg("%s - sfh7832_reg_init fail!\n", __func__);
		return err;
	}
	err = sfh7832_update_scan_mode(MODE_AMBIENT);
	if (err != 0) {
		HRM_dbg("%s - sfh7832_update_scan_mode fail!\n", __func__);
		return err;
	}
	err = sfh7832_timer_rst_fifo_en();
	if (err != 0) {
		HRM_dbg("%s - sfh7832_timer_rst_fifo_en fail!\n", __func__);
		return err;
	}

	err = sfh7832_set_current(sfh7832_data->iled1, sfh7832_data->iled4, sfh7832_data->iled2, sfh7832_data->iled3, LED_ALL);
	if (err != 0)
		HRM_dbg("%s - sfh7832_set_current fail!\n", __func__);

	return err;
}

static int sfh7832_enable(enum op_mode mode)
{
	int err = -1;

	HRM_dbg("%s - enable_m : %d cur_m : %d", __func__, mode, sfh7832_data->enabled_mode);

	if (mode == MODE_HRM)
		err = sfh7832_hrm_enable();
	else if (mode == MODE_PROX)
		err = sfh7832_prox_enable();
	else if (mode == MODE_SDK_IR)
		err = sfh7832_sdk_enable();
	else if (mode == MODE_SVC_IR)
		err = sfh7832_svc_led_enable();
	else if (mode == MODE_AMBIENT)
		err = sfh7832_ambient_enable();
	else
		HRM_dbg("%s - MODE_UNKNOWN\n", __func__);

	if (err != 0)
		HRM_err("%s fail err = %d\n", __func__, err);

	return err;
}

static int sfh7832_enable_eol_flicker(void)
{
	int err = 0;

	sfh7832_set_param_ambient();
	sfh7832_data->num_samples = 1;
	sfh7832_data->enabled_mode = MODE_AMBIENT;

	err = sfh7832_reg_init();
	if (err != 0) {
		HRM_dbg("%s - sfh7832_reg_init fail!\n", __func__);
		return err;
	}
	err = sfh7832_update_scan_mode(MODE_AMBIENT);
	if (err != 0) {
		HRM_dbg("%s - sfh7832_update_scan_mode fail!\n", __func__);
		return err;
	}

	err = sfh7832_timer_rst_fifo_en();
	if (err != 0) {
		HRM_dbg("%s - sfh7832_timer_rst_fifo_en fail!\n", __func__);
		return err;
	}

	/* set registers with proper values LED off */
	err = sfh7832_set_current(0x0, 0x0, 0x0, 0x0, LED_ALL);
	if (err != 0) {
		HRM_dbg("%s - sfh7832_set_current fail!\n", __func__);
		return err;
	}

	sfh7832_data->eol_data.state = _EOL_STATE_TYPE_FLICKER_INIT;
	return err;

}

static int sfh7832_enable_eol_dc(void)
{
	int err = 0;

	sfh7832_set_param_sdk();
	sfh7832_data->num_samples = 0;
	sfh7832_data->rfgain_amb1 = TIA_GAIN_25K;
	sfh7832_data->rfgain_led1 = TIA_GAIN_25K;
	sfh7832_data->rfgain_led2 = TIA_GAIN_25K;
	sfh7832_data->rfgain_amb2 = TIA_GAIN_25K;
	sfh7832_data->rfgain_led3 = TIA_GAIN_25K;
	sfh7832_data->rfgain_led4 = TIA_GAIN_25K;

	err = sfh7832_reg_init();
	if (err != 0) {
		HRM_dbg("%s - sfh7832_reg_init fail!\n", __func__);
		return err;
	}
	err = sfh7832_update_scan_mode(MODE_SDK_IR);
	if (err != 0) {
		HRM_dbg("%s - sfh7832_update_scan_mode fail!\n", __func__);
		return err;
	}

	err = sfh7832_timer_rst_fifo_en();
	if (err != 0) {
		HRM_dbg("%s - sfh7832_timer_rst_fifo_en fail!\n", __func__);
		return err;
	}

	sfh7832_data->enabled_mode = MODE_SDK_IR;

	err = sfh7832_set_current(EOL_DC_MID_LED_IR_CURRENT,
					   EOL_DC_MID_LED_RED_CURRENT,
					   EOL_DC_MID_LED_GREEN_CURRENT,
					   EOL_DC_MID_LED_BLUE_CURRENT,
					   LED_ALL);
	if (err != 0) {
		HRM_dbg("%s - sfh7832_set_current fail!\n", __func__);
		return err;
	}
	sfh7832_data->eol_data.state = _EOL_STATE_TYPE_DC_MODE_MID;
	return 0;

}

void sfh7832_init_eol_data(struct sfh7832_eol_data *eol_data)
{
	memset(eol_data, 0, sizeof(struct sfh7832_eol_data));
	do_gettimeofday(&eol_data->start_time);
}

u32 sfh7832_eol_sqrt(u64 sqsum)
{
	u64 sq_rt;
	u64 g0, g1, g2, g3, g4;
	unsigned int seed;
	unsigned int next;
	unsigned int step;

	g4 = sqsum / 100000000;
	g3 = (sqsum - g4 * 100000000) / 1000000;
	g2 = (sqsum - g4 * 100000000 - g3 * 1000000) / 10000;
	g1 = (sqsum - g4 * 100000000 - g3 * 1000000 - g2 * 10000) / 100;
	g0 = (sqsum - g4 * 100000000 - g3 * 1000000 - g2 * 10000 - g1 * 100);

	next = g4;
	step = 0;
	seed = 0;
	while (((seed + 1) * (step + 1)) <= next) {
		step++;
		seed++;
	}

	sq_rt = (u64) seed * 10000;
	next = (next - (seed * step)) * 100 + g3;

	step = 0;
	seed = 2 * seed * 10;
	while (((seed + 1) * (step + 1)) <= next) {
		step++;
		seed++;
	}

	sq_rt = sq_rt + step * 1000;
	next = (next - seed * step) * 100 + g2;
	seed = (seed + step) * 10;
	step = 0;
	while (((seed + 1) * (step + 1)) <= next) {
		step++;
		seed++;
	 }

	sq_rt = sq_rt + step * 100;
	next = (next - seed * step) * 100 + g1;
	seed = (seed + step) * 10;
	step = 0;

	while (((seed + 1) * (step + 1)) <= next) {
		step++;
		seed++;
	}

	sq_rt = sq_rt + step * 10;
	next = (next - seed * step) * 100 + g0;
	seed = (seed + step) * 10;
	step = 0;

	while (((seed + 1) * (step + 1)) <= next) {
		step++;
		seed++;
	}

	sq_rt = sq_rt + step;

	return (u32) sq_rt;
}

void sfh7832_eol_std_sum(struct sfh7832_eol_hrm_data *eol_hrm_data)
{
	int ch_i = 0, std_i = 0;
	int buf_i = 0, buf_idx = 0;
	s64 buf[10] = {0, };
	s64 avg = 0, sum = 0, sum_tot = 0;

	for (ch_i = 0; ch_i < SELF_MAX_CH_NUM; ch_i++) {
		sum_tot = 0;
		buf_idx = 0;
		for (std_i = 0; std_i < EOL_STD2_BLOCK_SIZE; std_i++) {
			sum = 0;
			avg = 0;
			for (buf_i = 0; buf_i < EOL_STD2_SIZE; buf_i++) {
				buf[buf_i] = eol_hrm_data->buf[ch_i][buf_idx];
				avg += buf[buf_i];

				buf_idx++;
			}
			do_div(avg, EOL_STD2_SIZE);

			for (buf_i = 0; buf_i < EOL_STD2_SIZE; buf_i++)
				sum += (buf[buf_i] - avg)*(buf[buf_i] - avg);

			HRM_dbg("%s - sum %d %d : %lld, %lld\n", __func__, ch_i, std_i, avg, sum);

			sum_tot += sum;
		}

		sum_tot = div64_s64(sum_tot, EOL_DC_HIGH_SIZE);

		eol_hrm_data->std_sum2[ch_i] = sfh7832_eol_sqrt(sum_tot);
	}
}

void sfh7832_eol_hrm_data(int ir_data, int red_data, int green_data,
			    int blue_data, struct sfh7832_eol_data *eol_data,
			    u32 array_pos, u32 eol_skip_cnt, u32 eol_step_size)
{
	int i = 0;
	int hrm_eol_count = eol_data->eol_hrm_data[array_pos].count;
	int hrm_eol_index = eol_data->eol_hrm_data[array_pos].index;
	int ir_current = eol_data->eol_hrm_data[array_pos].ir_current;
	int red_current = eol_data->eol_hrm_data[array_pos].red_current;
	int green_current = eol_data->eol_hrm_data[array_pos].green_current;
	int blue_current = eol_data->eol_hrm_data[array_pos].blue_current;
	s64 average[4];
	s64 std[4];
	u32 hrm_eol_skip_cnt = eol_skip_cnt;
	u32 hrm_eol_size = eol_step_size;

	if (!hrm_eol_count) {
		HRM_dbg("%s - STEP [%d], [%d] started IR : %d , RED : %d, GREEN : %d, BLUE : %d,eol_skip_cnt : %d, eol_step_size : %d\n",
				__func__, array_pos, array_pos + 2,
				ir_current, red_current, green_current, blue_current,
				eol_skip_cnt, eol_step_size);
	}
	if (eol_step_size > EOL_HRM_SIZE) {
		HRM_dbg("%s - FATAL ERROR !!!! the buffer size should be smaller than EOL_HRM_SIZE\n",
			 __func__);
		goto hrm_eol_exit;
	}
	if (hrm_eol_count < hrm_eol_skip_cnt)
		goto hrm_eol_exit;

	if (hrm_eol_index < hrm_eol_size) {
		eol_data->eol_hrm_data[array_pos].buf[SELF_IR_CH][hrm_eol_index] = (s64)ir_data;
		eol_data->eol_hrm_data[array_pos].buf[SELF_RED_CH][hrm_eol_index] = (s64)red_data;
		eol_data->eol_hrm_data[array_pos].buf[SELF_GREEN_CH][hrm_eol_index] = (s64)green_data;
		eol_data->eol_hrm_data[array_pos].buf[SELF_BLUE_CH][hrm_eol_index] = (s64)blue_data;
		eol_data->eol_hrm_data[array_pos].sum[SELF_IR_CH]
			+= eol_data->eol_hrm_data[array_pos].buf[SELF_IR_CH][hrm_eol_index];
		eol_data->eol_hrm_data[array_pos].sum[SELF_RED_CH]
			+= eol_data->eol_hrm_data[array_pos].buf[SELF_RED_CH][hrm_eol_index];
		eol_data->eol_hrm_data[array_pos].sum[SELF_GREEN_CH]
			+= eol_data->eol_hrm_data[array_pos].buf[SELF_GREEN_CH][hrm_eol_index];
		eol_data->eol_hrm_data[array_pos].sum[SELF_BLUE_CH]
			+= eol_data->eol_hrm_data[array_pos].buf[SELF_BLUE_CH][hrm_eol_index];
		eol_data->eol_hrm_data[array_pos].index++;
	} else if (hrm_eol_index == hrm_eol_size && eol_data->eol_hrm_data[array_pos].done != 1) {
		eol_data->eol_hrm_data[array_pos].average[SELF_IR_CH]
			= div64_s64(eol_data->eol_hrm_data[array_pos].sum[SELF_IR_CH], hrm_eol_size);
		eol_data->eol_hrm_data[array_pos].average[SELF_RED_CH]
			= div64_s64(eol_data->eol_hrm_data[array_pos].sum[SELF_RED_CH], hrm_eol_size);
		eol_data->eol_hrm_data[array_pos].average[SELF_GREEN_CH]
			= div64_s64(eol_data->eol_hrm_data[array_pos].sum[SELF_GREEN_CH], hrm_eol_size);
		eol_data->eol_hrm_data[array_pos].average[SELF_BLUE_CH]
			= div64_s64(eol_data->eol_hrm_data[array_pos].sum[SELF_BLUE_CH], hrm_eol_size);

		average[SELF_IR_CH] = eol_data->eol_hrm_data[array_pos].average[SELF_IR_CH];
		average[SELF_RED_CH] = eol_data->eol_hrm_data[array_pos].average[SELF_RED_CH];
		average[SELF_GREEN_CH] = eol_data->eol_hrm_data[array_pos].average[SELF_GREEN_CH];
		average[SELF_BLUE_CH] = eol_data->eol_hrm_data[array_pos].average[SELF_BLUE_CH];

		for (i = 0; i < hrm_eol_size; i++) {
			eol_data->eol_hrm_data[array_pos].std_sum[SELF_IR_CH]
				+= (eol_data->eol_hrm_data[array_pos].buf[SELF_IR_CH][i] - average[SELF_IR_CH])
				* (eol_data->eol_hrm_data[array_pos].buf[SELF_IR_CH][i] - average[SELF_IR_CH]);
			eol_data->eol_hrm_data[array_pos].std_sum[SELF_RED_CH]
				+= (eol_data->eol_hrm_data[array_pos].buf[SELF_RED_CH][i] - average[SELF_RED_CH])
				* (eol_data->eol_hrm_data[array_pos].buf[SELF_RED_CH][i] - average[SELF_RED_CH]);
			eol_data->eol_hrm_data[array_pos].std_sum[SELF_GREEN_CH]
				+= (eol_data->eol_hrm_data[array_pos].buf[SELF_GREEN_CH][i] - average[SELF_GREEN_CH])
				* (eol_data->eol_hrm_data[array_pos].buf[SELF_GREEN_CH][i] - average[SELF_GREEN_CH]);
			eol_data->eol_hrm_data[array_pos].std_sum[SELF_BLUE_CH]
				+= (eol_data->eol_hrm_data[array_pos].buf[SELF_BLUE_CH][i] - average[SELF_BLUE_CH])
				* (eol_data->eol_hrm_data[array_pos].buf[SELF_BLUE_CH][i] - average[SELF_BLUE_CH]);
		}

		do_div(eol_data->eol_hrm_data[array_pos].std_sum[SELF_IR_CH], hrm_eol_size);
		eol_data->eol_hrm_data[array_pos].std[SELF_IR_CH]
			= sfh7832_eol_sqrt(eol_data->eol_hrm_data[array_pos].std_sum[SELF_IR_CH]);
		std[SELF_IR_CH] =  eol_data->eol_hrm_data[array_pos].std[SELF_IR_CH];

		do_div(eol_data->eol_hrm_data[array_pos].std_sum[SELF_RED_CH], hrm_eol_size);
		eol_data->eol_hrm_data[array_pos].std[SELF_RED_CH]
			= sfh7832_eol_sqrt(eol_data->eol_hrm_data[array_pos].std_sum[SELF_RED_CH]);
		std[SELF_RED_CH] =	eol_data->eol_hrm_data[array_pos].std[SELF_RED_CH];

		do_div(eol_data->eol_hrm_data[array_pos].std_sum[SELF_GREEN_CH], hrm_eol_size);
		eol_data->eol_hrm_data[array_pos].std[SELF_GREEN_CH]
			= sfh7832_eol_sqrt(eol_data->eol_hrm_data[array_pos].std_sum[SELF_GREEN_CH]);
		std[SELF_GREEN_CH] =  eol_data->eol_hrm_data[array_pos].std[SELF_GREEN_CH];

		do_div(eol_data->eol_hrm_data[array_pos].std_sum[SELF_BLUE_CH], hrm_eol_size);
		eol_data->eol_hrm_data[array_pos].std[SELF_BLUE_CH]
			= sfh7832_eol_sqrt(eol_data->eol_hrm_data[array_pos].std_sum[SELF_BLUE_CH]);
		std[SELF_BLUE_CH] =  eol_data->eol_hrm_data[array_pos].std[SELF_BLUE_CH];

		if (array_pos == EOL_MODE_DC_HIGH)
			sfh7832_eol_std_sum(&eol_data->eol_hrm_data[array_pos]);

		eol_data->eol_hrm_data[array_pos].done = 1;
		eol_data->eol_hrm_flag |= 1 << array_pos;
		HRM_dbg("%s - STEP [%d], [%d], [%x] Done\n",
			__func__, array_pos, array_pos + 2, eol_data->eol_hrm_flag);
	}
hrm_eol_exit:
	eol_data->eol_hrm_data[array_pos].count++;

}

void sfh7832_eol_flicker_data(int *ir_data, struct sfh7832_eol_data *eol_data)
{
	int i = 0;
	int eol_flicker_count = eol_data->eol_flicker_data.count;
	int eol_flicker_index = eol_data->eol_flicker_data.index;
	s64 data[2];
	s64 average[2];
	u64 freq_start_time = 0;
	u64 freq_work_time = 0;
	u64 freq_float_value = 400000;

	if (!eol_flicker_count)
		HRM_dbg("%s - EOL FLICKER STEP 1 STARTED !!\n", __func__);

	if (eol_flicker_count < EOL_FLICKER_SKIP_CNT)
		goto eol_flicker_exit;

	if (eol_flicker_index < EOL_FLICKER_SIZE) {
		for (i = 0; i < sfh7832_data->fifo_depth * sfh7832_data->n_phase; i++) {
			eol_data->eol_flicker_data.buf[SELF_IR_CH][eol_flicker_index + i] = (s64)ir_data[i];
			eol_data->eol_flicker_data.sum[SELF_IR_CH]
				+= eol_data->eol_flicker_data.buf[SELF_IR_CH][eol_flicker_index + i];

			if (eol_data->eol_flicker_data.max < eol_data->eol_flicker_data.buf[SELF_IR_CH][eol_flicker_index + i])
				eol_data->eol_flicker_data.max = eol_data->eol_flicker_data.buf[SELF_IR_CH][eol_flicker_index + i];
		}
		eol_data->eol_flicker_data.index += 20;

		if (eol_flicker_index == 0)
			do_gettimeofday(&eol_data->eol_flicker_data.start_time_t);

		} else if (eol_flicker_index == EOL_FLICKER_SIZE
				&& eol_data->eol_flicker_data.done != 1) {
			do_gettimeofday(&eol_data->eol_flicker_data.work_time_t);
			eol_data->eol_freq_data.done = 1;

			eol_data->eol_flicker_data.average[SELF_IR_CH]
				= div64_s64(eol_data->eol_flicker_data.sum[SELF_IR_CH], EOL_FLICKER_SIZE);
			average[SELF_IR_CH] = eol_data->eol_flicker_data.average[SELF_IR_CH];

			for (i = 0; i < EOL_FLICKER_SIZE; i++) {
				data[SELF_IR_CH] = eol_data->eol_flicker_data.buf[SELF_IR_CH][i];
				eol_data->eol_flicker_data.std_sum[SELF_IR_CH]
					+= (data[SELF_IR_CH] - average[SELF_IR_CH])
					* (data[SELF_IR_CH] - average[SELF_IR_CH]);
			}

			freq_start_time = (sfh7832_data->eol_data.eol_flicker_data.start_time_t.tv_sec * 1000)
				+ (sfh7832_data->eol_data.eol_flicker_data.start_time_t.tv_usec / 1000);

			freq_work_time = (sfh7832_data->eol_data.eol_flicker_data.work_time_t.tv_sec * 1000)
				+ (sfh7832_data->eol_data.eol_flicker_data.work_time_t.tv_usec / 1000);
			freq_work_time -= freq_start_time;

			sfh7832_data->eol_data.eol_freq_data.frequency = (freq_float_value/freq_work_time);

			do_div(eol_data->eol_flicker_data.std_sum[SELF_IR_CH], EOL_FLICKER_SIZE);
			eol_data->eol_flicker_data.std[SELF_IR_CH] =
				sfh7832_eol_sqrt(eol_data->eol_flicker_data.std_sum[SELF_IR_CH]);

			eol_data->eol_flicker_data.done = 1;
			HRM_dbg("%s - EOL FLICKER STEP 1 Done\n", __func__);
		}

eol_flicker_exit:
	eol_data->eol_flicker_data.count += 20;

}

void sfh7832_eol_xtalk_data(int ir_data, struct sfh7832_eol_data *eol_data)
{
	int eol_xtalk_count = eol_data->eol_xtalk_data.count;
	int eol_xtalk_index = eol_data->eol_xtalk_data.index;

	if (!eol_xtalk_count)
		HRM_dbg("%s - EOL XTALK STEP 1 STARTED !!\n", __func__);

	if (eol_xtalk_count < EOL_XTALK_SKIP_CNT)
		goto xtalk_eol_exit;

	if (eol_xtalk_index < EOL_XTALK_SIZE) {
		eol_data->eol_xtalk_data.buf[eol_xtalk_index] = (s64)ir_data;
		eol_data->eol_xtalk_data.sum += eol_data->eol_xtalk_data.buf[eol_xtalk_index];
		eol_data->eol_xtalk_data.index++;
	} else {
		eol_data->eol_xtalk_data.average
			= div64_s64(eol_data->eol_xtalk_data.sum, EOL_XTALK_SIZE);
		eol_data->eol_xtalk_data.done = EOL_SUCCESS;
		HRM_dbg("%s - EOL XTALK STEP 1 Done\n", __func__);
	}

xtalk_eol_exit:
	eol_data->eol_xtalk_data.count++;

}

void sfh7832_make_eol_item(struct sfh7832_eol_data *eol_data)
{
	switch (sfh7832_data->eol_test_type) {
	case EOL_15_MODE:
		snprintf(eol_data->eol_item_data.test_type, 5, "%s", "MAIN");
		eol_data->eol_item_data.system_noise[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_spec[0];
		eol_data->eol_item_data.system_noise[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_spec[1];
		eol_data->eol_item_data.ir_mid[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_spec[2];
		eol_data->eol_item_data.ir_mid[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_spec[3];
		eol_data->eol_item_data.red_mid[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_spec[4];
		eol_data->eol_item_data.red_mid[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_spec[5];
		eol_data->eol_item_data.green_mid[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_spec[6];
		eol_data->eol_item_data.green_mid[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_spec[7];
		eol_data->eol_item_data.blue_mid[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_spec[8];
		eol_data->eol_item_data.blue_mid[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_spec[9];
		eol_data->eol_item_data.ir_high[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_spec[10];
		eol_data->eol_item_data.ir_high[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_spec[11];
		eol_data->eol_item_data.red_high[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_spec[12];
		eol_data->eol_item_data.red_high[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_spec[13];
		eol_data->eol_item_data.green_high[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_spec[14];
		eol_data->eol_item_data.green_high[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_spec[15];
		eol_data->eol_item_data.blue_high[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_spec[16];
		eol_data->eol_item_data.blue_high[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_spec[17];
		eol_data->eol_item_data.ir_offdac[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_spec[18];
		eol_data->eol_item_data.ir_offdac[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_spec[19];
		eol_data->eol_item_data.red_offdac[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_spec[20];
		eol_data->eol_item_data.red_offdac[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_spec[21];
		eol_data->eol_item_data.green_offdac[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_spec[22];
		eol_data->eol_item_data.green_offdac[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_spec[23];
		eol_data->eol_item_data.blue_offdac[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_spec[24];
		eol_data->eol_item_data.blue_offdac[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_spec[25];
		eol_data->eol_item_data.ir_noise[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_spec[26];
		eol_data->eol_item_data.ir_noise[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_spec[27];
		eol_data->eol_item_data.red_noise[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_spec[28];
		eol_data->eol_item_data.red_noise[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_spec[29];
		eol_data->eol_item_data.green_noise[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_spec[30];
		eol_data->eol_item_data.green_noise[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_spec[31];
		eol_data->eol_item_data.blue_noise[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_spec[32];
		eol_data->eol_item_data.blue_noise[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_spec[33];
		eol_data->eol_item_data.frequency[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_spec[34];
		eol_data->eol_item_data.frequency[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_spec[35];
		break;
	case EOL_SEMI_FT:
		snprintf(eol_data->eol_item_data.test_type, 5, "%s", "SEMI");
		eol_data->eol_item_data.system_noise[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_semi_spec[0];
		eol_data->eol_item_data.system_noise[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_semi_spec[1];
		eol_data->eol_item_data.ir_mid[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_semi_spec[2];
		eol_data->eol_item_data.ir_mid[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_semi_spec[3];
		eol_data->eol_item_data.red_mid[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_semi_spec[4];
		eol_data->eol_item_data.red_mid[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_semi_spec[5];
		eol_data->eol_item_data.green_mid[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_semi_spec[6];
		eol_data->eol_item_data.green_mid[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_semi_spec[7];
		eol_data->eol_item_data.blue_mid[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_semi_spec[8];
		eol_data->eol_item_data.blue_mid[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_semi_spec[9];
		eol_data->eol_item_data.ir_high[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_semi_spec[10];
		eol_data->eol_item_data.ir_high[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_semi_spec[11];
		eol_data->eol_item_data.red_high[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_semi_spec[12];
		eol_data->eol_item_data.red_high[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_semi_spec[13];
		eol_data->eol_item_data.green_high[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_semi_spec[14];
		eol_data->eol_item_data.green_high[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_semi_spec[15];
		eol_data->eol_item_data.blue_high[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_semi_spec[16];
		eol_data->eol_item_data.blue_high[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_semi_spec[17];
		eol_data->eol_item_data.ir_offdac[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_semi_spec[18];
		eol_data->eol_item_data.ir_offdac[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_semi_spec[19];
		eol_data->eol_item_data.red_offdac[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_semi_spec[20];
		eol_data->eol_item_data.red_offdac[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_semi_spec[21];
		eol_data->eol_item_data.green_offdac[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_semi_spec[22];
		eol_data->eol_item_data.green_offdac[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_semi_spec[23];
		eol_data->eol_item_data.blue_offdac[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_semi_spec[24];
		eol_data->eol_item_data.blue_offdac[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_semi_spec[25];
		eol_data->eol_item_data.ir_noise[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_semi_spec[26];
		eol_data->eol_item_data.ir_noise[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_semi_spec[27];
		eol_data->eol_item_data.red_noise[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_semi_spec[28];
		eol_data->eol_item_data.red_noise[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_semi_spec[29];
		eol_data->eol_item_data.green_noise[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_semi_spec[30];
		eol_data->eol_item_data.green_noise[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_semi_spec[31];
		eol_data->eol_item_data.blue_noise[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_semi_spec[32];
		eol_data->eol_item_data.blue_noise[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_semi_spec[33];
		eol_data->eol_item_data.frequency[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_semi_spec[34];
		eol_data->eol_item_data.frequency[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_semi_spec[35];
		break;
	case EOL_XTALK:
		snprintf(eol_data->eol_item_data.test_type, 6, "%s", "XTALK");
		eol_data->eol_item_data.xtalk[EOL_SPEC_MIN] = (s32) sfh7832_data->eol_xtalk_spec[0];
		eol_data->eol_item_data.xtalk[EOL_SPEC_MAX] = (s32) sfh7832_data->eol_xtalk_spec[1];
		break;
	default:
		break;
	}

	switch (sfh7832_data->eol_test_type) {
	case EOL_15_MODE:
	case EOL_SEMI_FT:
		eol_data->eol_item_data.system_noise[EOL_MEASURE] = (s32) eol_data->eol_flicker_data.std[SELF_IR_CH];
		eol_data->eol_item_data.ir_mid[EOL_MEASURE] = (s32) eol_data->eol_hrm_data[EOL_MODE_DC_MID].average[SELF_IR_CH];
		eol_data->eol_item_data.red_mid[EOL_MEASURE] = (s32) eol_data->eol_hrm_data[EOL_MODE_DC_MID].average[SELF_RED_CH];
		eol_data->eol_item_data.green_mid[EOL_MEASURE] = (s32) eol_data->eol_hrm_data[EOL_MODE_DC_MID].average[SELF_GREEN_CH];
		eol_data->eol_item_data.blue_mid[EOL_MEASURE] = (s32) eol_data->eol_hrm_data[EOL_MODE_DC_MID].average[SELF_BLUE_CH];
		eol_data->eol_item_data.ir_high[EOL_MEASURE] = (s32) eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].average[SELF_IR_CH];
		eol_data->eol_item_data.red_high[EOL_MEASURE] = (s32) eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].average[SELF_RED_CH];
		eol_data->eol_item_data.green_high[EOL_MEASURE] = (s32) eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].average[SELF_GREEN_CH];
		eol_data->eol_item_data.blue_high[EOL_MEASURE] = (s32) eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].average[SELF_BLUE_CH];
		eol_data->eol_item_data.ir_offdac[EOL_MEASURE] = (s32) eol_data->eol_hrm_data[EOL_MODE_DC_IOFFDAC].average[SELF_IR_CH];
		eol_data->eol_item_data.red_offdac[EOL_MEASURE] = (s32) eol_data->eol_hrm_data[EOL_MODE_DC_IOFFDAC].average[SELF_RED_CH];
		eol_data->eol_item_data.green_offdac[EOL_MEASURE] = (s32) eol_data->eol_hrm_data[EOL_MODE_DC_IOFFDAC].average[SELF_GREEN_CH];
		eol_data->eol_item_data.blue_offdac[EOL_MEASURE] = (s32) eol_data->eol_hrm_data[EOL_MODE_DC_IOFFDAC].average[SELF_BLUE_CH];
		eol_data->eol_item_data.ir_noise[EOL_MEASURE] = (s32) eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].std_sum2[SELF_IR_CH];
		eol_data->eol_item_data.red_noise[EOL_MEASURE] = (s32) eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].std_sum2[SELF_RED_CH];
		eol_data->eol_item_data.green_noise[EOL_MEASURE] = (s32) eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].std_sum2[SELF_GREEN_CH];
		eol_data->eol_item_data.blue_noise[EOL_MEASURE] = (s32) eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].std_sum2[SELF_BLUE_CH];
		eol_data->eol_item_data.frequency[EOL_MEASURE] = (s32) eol_data->eol_freq_data.frequency;

		if (eol_data->eol_item_data.system_noise[EOL_MEASURE] >= eol_data->eol_item_data.system_noise[EOL_SPEC_MIN]
			&& eol_data->eol_item_data.system_noise[EOL_MEASURE] <= eol_data->eol_item_data.system_noise[EOL_SPEC_MAX])
			eol_data->eol_item_data.system_noise[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.system_noise[EOL_PASS_FAIL] = false;

		if (eol_data->eol_item_data.ir_mid[EOL_MEASURE] >= eol_data->eol_item_data.ir_mid[EOL_SPEC_MIN]
			&& eol_data->eol_item_data.ir_mid[EOL_MEASURE] <= eol_data->eol_item_data.ir_mid[EOL_SPEC_MAX])
			eol_data->eol_item_data.ir_mid[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.ir_mid[EOL_PASS_FAIL] = false;

		if (eol_data->eol_item_data.red_mid[EOL_MEASURE] >= eol_data->eol_item_data.red_mid[EOL_SPEC_MIN]
			&& eol_data->eol_item_data.red_mid[EOL_MEASURE] <= eol_data->eol_item_data.red_mid[EOL_SPEC_MAX])
			eol_data->eol_item_data.red_mid[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.red_mid[EOL_PASS_FAIL] = false;

		if (eol_data->eol_item_data.green_mid[EOL_MEASURE] >= eol_data->eol_item_data.green_mid[EOL_SPEC_MIN]
			&& eol_data->eol_item_data.green_mid[EOL_MEASURE] <= eol_data->eol_item_data.green_mid[EOL_SPEC_MAX])
			eol_data->eol_item_data.green_mid[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.green_mid[EOL_PASS_FAIL] = false;

		if (eol_data->eol_item_data.blue_mid[EOL_MEASURE] >= eol_data->eol_item_data.blue_mid[EOL_SPEC_MIN]
			&& eol_data->eol_item_data.blue_mid[EOL_MEASURE] <= eol_data->eol_item_data.blue_mid[EOL_SPEC_MAX])
			eol_data->eol_item_data.blue_mid[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.blue_mid[EOL_PASS_FAIL] = false;

		if (eol_data->eol_item_data.ir_high[EOL_MEASURE] >= eol_data->eol_item_data.ir_high[EOL_SPEC_MIN]
			&& eol_data->eol_item_data.ir_high[EOL_MEASURE] <= eol_data->eol_item_data.ir_high[EOL_SPEC_MAX])
			eol_data->eol_item_data.ir_high[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.ir_high[EOL_PASS_FAIL] = false;

		if (eol_data->eol_item_data.red_high[EOL_MEASURE] >= eol_data->eol_item_data.red_high[EOL_SPEC_MIN]
			&& eol_data->eol_item_data.red_high[EOL_MEASURE] <= eol_data->eol_item_data.red_high[EOL_SPEC_MAX])
			eol_data->eol_item_data.red_high[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.red_high[EOL_PASS_FAIL] = false;

		if (eol_data->eol_item_data.green_high[EOL_MEASURE] >= eol_data->eol_item_data.green_high[EOL_SPEC_MIN]
			&& eol_data->eol_item_data.green_high[EOL_MEASURE] <= eol_data->eol_item_data.green_high[EOL_SPEC_MAX])
			eol_data->eol_item_data.green_high[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.green_high[EOL_PASS_FAIL] = false;

		if (eol_data->eol_item_data.blue_high[EOL_MEASURE] >= eol_data->eol_item_data.blue_high[EOL_SPEC_MIN]
			&& eol_data->eol_item_data.blue_high[EOL_MEASURE] <= eol_data->eol_item_data.blue_high[EOL_SPEC_MAX])
			eol_data->eol_item_data.blue_high[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.blue_high[EOL_PASS_FAIL] = false;

		if (eol_data->eol_item_data.ir_offdac[EOL_MEASURE] >= eol_data->eol_item_data.ir_offdac[EOL_SPEC_MIN]
			&& eol_data->eol_item_data.ir_offdac[EOL_MEASURE] <= eol_data->eol_item_data.ir_offdac[EOL_SPEC_MAX])
			eol_data->eol_item_data.ir_offdac[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.ir_offdac[EOL_PASS_FAIL] = false;

		if (eol_data->eol_item_data.red_offdac[EOL_MEASURE] >= eol_data->eol_item_data.red_offdac[EOL_SPEC_MIN]
			&& eol_data->eol_item_data.red_offdac[EOL_MEASURE] <= eol_data->eol_item_data.red_offdac[EOL_SPEC_MAX])
			eol_data->eol_item_data.red_offdac[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.red_offdac[EOL_PASS_FAIL] = false;

		if (eol_data->eol_item_data.green_offdac[EOL_MEASURE] >= eol_data->eol_item_data.green_offdac[EOL_SPEC_MIN]
			&& eol_data->eol_item_data.green_offdac[EOL_MEASURE] <= eol_data->eol_item_data.green_offdac[EOL_SPEC_MAX])
			eol_data->eol_item_data.green_offdac[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.green_offdac[EOL_PASS_FAIL] = false;

		if (eol_data->eol_item_data.blue_offdac[EOL_MEASURE] >= eol_data->eol_item_data.blue_offdac[EOL_SPEC_MIN]
			&& eol_data->eol_item_data.blue_offdac[EOL_MEASURE] <= eol_data->eol_item_data.blue_offdac[EOL_SPEC_MAX])
			eol_data->eol_item_data.blue_offdac[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.blue_offdac[EOL_PASS_FAIL] = false;

		if (eol_data->eol_item_data.ir_noise[EOL_MEASURE] >= eol_data->eol_item_data.ir_noise[EOL_SPEC_MIN]
			&& eol_data->eol_item_data.ir_noise[EOL_MEASURE] <= eol_data->eol_item_data.ir_noise[EOL_SPEC_MAX])
			eol_data->eol_item_data.ir_noise[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.ir_noise[EOL_PASS_FAIL] = false;

		if (eol_data->eol_item_data.red_noise[EOL_MEASURE] >= eol_data->eol_item_data.red_noise[EOL_SPEC_MIN]
			&& eol_data->eol_item_data.red_noise[EOL_MEASURE] <= eol_data->eol_item_data.red_noise[EOL_SPEC_MAX])
			eol_data->eol_item_data.red_noise[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.red_noise[EOL_PASS_FAIL] = false;

		if (eol_data->eol_item_data.green_noise[EOL_MEASURE] >= eol_data->eol_item_data.green_noise[EOL_SPEC_MIN]
			&& eol_data->eol_item_data.green_noise[EOL_MEASURE] <= eol_data->eol_item_data.green_noise[EOL_SPEC_MAX])
			eol_data->eol_item_data.green_noise[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.green_noise[EOL_PASS_FAIL] = false;

		if (eol_data->eol_item_data.blue_noise[EOL_MEASURE] >= eol_data->eol_item_data.blue_noise[EOL_SPEC_MIN]
			&& eol_data->eol_item_data.blue_noise[EOL_MEASURE] <= eol_data->eol_item_data.blue_noise[EOL_SPEC_MAX])
			eol_data->eol_item_data.blue_noise[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.blue_noise[EOL_PASS_FAIL] = false;

		if (eol_data->eol_item_data.frequency[EOL_MEASURE] >= eol_data->eol_item_data.frequency[EOL_SPEC_MIN]
			&& eol_data->eol_item_data.frequency[EOL_MEASURE] <= eol_data->eol_item_data.frequency[EOL_SPEC_MAX])
			eol_data->eol_item_data.frequency[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.frequency[EOL_PASS_FAIL] = false;
		break;
	case EOL_XTALK:
		eol_data->eol_item_data.xtalk[EOL_MEASURE] = (s32) eol_data->eol_xtalk_data.average;

		if (eol_data->eol_item_data.xtalk[EOL_MEASURE] >= eol_data->eol_item_data.xtalk[EOL_SPEC_MIN]
		&& eol_data->eol_item_data.xtalk[EOL_MEASURE] <= eol_data->eol_item_data.xtalk[EOL_SPEC_MAX])
			eol_data->eol_item_data.xtalk[EOL_PASS_FAIL] = true;
		else
			eol_data->eol_item_data.xtalk[EOL_PASS_FAIL] = false;
		break;
	default:
		break;
	}
}

void sfh7832_eol_check_done(struct sfh7832_eol_data *eol_data)
{
	int i = 0;
	int start_time;
	int end_time;

	if (sfh7832_data->eol_test_result != NULL)
		kfree(sfh7832_data->eol_test_result);

	sfh7832_data->eol_test_result = kzalloc(sizeof(char) * SFH_EOL_RESULT, GFP_KERNEL);
	if (sfh7832_data->eol_test_result == NULL) {
		HRM_err("%s - couldn't eol_test_result allocate memory\n", __func__);
		return;
	}

	if ((eol_data->eol_flicker_data.done)
		&& (eol_data->eol_hrm_flag == EOL_HRM_COMPLETE_ALL_STEP)
		&& (eol_data->eol_freq_data.done)) {
		do_gettimeofday(&eol_data->end_time);
		start_time = (eol_data->start_time.tv_sec * 1000) + (eol_data->start_time.tv_usec / 1000);
		end_time = (eol_data->end_time.tv_sec * 1000) + (eol_data->end_time.tv_usec / 1000);
		HRM_dbg("%s - EOL Tested Time :  %d ms Tested Count : %d\n",
			__func__, end_time - start_time, sfh7832_data->sample_cnt);

		HRM_dbg("%s - SETTING CUREENT %x, %x\n",
			__func__, EOL_DC_HIGH_LED_IR_CURRENT, EOL_DC_HIGH_LED_RED_CURRENT);

		for (i = 0; i < EOL_HRM_ARRY_SIZE; i++)
			HRM_dbg("%s - DOUBLE CHECK STEP[%d], done : %d\n",
				__func__, i + 2,
				eol_data->eol_hrm_data[i].done);

		for (i = 0; i < EOL_FLICKER_SIZE; i++)
			HRM_dbg("%s - EOL_FLICKER_MODE EOLRAW,%lld,%d\n",
				 __func__, eol_data->eol_flicker_data.buf[SELF_IR_CH][i], 0);

		for (i = 0; i < EOL_DC_MIDDLE_SIZE; i++)
			HRM_dbg("%s - EOL_DC_MIDDLE_MODE EOLRAW,%lld,%lld,%lld,%lld\n", __func__,
					eol_data->eol_hrm_data[EOL_MODE_DC_MID].buf[SELF_IR_CH][i],
					eol_data->eol_hrm_data[EOL_MODE_DC_MID].buf[SELF_RED_CH][i],
					eol_data->eol_hrm_data[EOL_MODE_DC_MID].buf[SELF_GREEN_CH][i],
					eol_data->eol_hrm_data[EOL_MODE_DC_MID].buf[SELF_BLUE_CH][i]);

		for (i = 0; i < EOL_DC_HIGH_SIZE; i++)
			HRM_dbg("%s - EOL_DC_HIGH_MODE EOLRAW,%lld,%lld,%lld,%lld\n", __func__,
					eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].buf[SELF_IR_CH][i],
					eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].buf[SELF_RED_CH][i],
					eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].buf[SELF_GREEN_CH][i],
					eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].buf[SELF_BLUE_CH][i]);

		for (i = 0; i < EOL_DC_IOFFDAC_SIZE; i++)
			HRM_dbg("%s - EOL_DC_IOFFDAC EOLRAW,%lld,%lld,%lld,%lld\n", __func__,
					eol_data->eol_hrm_data[EOL_MODE_DC_IOFFDAC].buf[SELF_IR_CH][i],
					eol_data->eol_hrm_data[EOL_MODE_DC_IOFFDAC].buf[SELF_RED_CH][i],
					eol_data->eol_hrm_data[EOL_MODE_DC_IOFFDAC].buf[SELF_GREEN_CH][i],
					eol_data->eol_hrm_data[EOL_MODE_DC_IOFFDAC].buf[SELF_BLUE_CH][i]);

		HRM_dbg("%s - EOL_FLICKER_MODE RESULT,%lld,%lld\n", __func__,
				eol_data->eol_flicker_data.average[SELF_IR_CH],
				eol_data->eol_flicker_data.std_sum[SELF_IR_CH]);
		HRM_dbg("%s - EOL_DC_MIDDLE_MODE RESULT,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld\n",
				__func__, eol_data->eol_hrm_data[EOL_MODE_DC_MID].average[SELF_IR_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_MID].std_sum[SELF_IR_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_MID].average[SELF_RED_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_MID].std_sum[SELF_RED_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_MID].average[SELF_GREEN_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_MID].std_sum[SELF_GREEN_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_MID].average[SELF_BLUE_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_MID].std_sum[SELF_BLUE_CH]);
		HRM_dbg("%s - EOL_DC_HIGH_MODE RESULT,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld\n",
				__func__, eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].average[SELF_IR_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].std_sum[SELF_IR_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].average[SELF_RED_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].std_sum[SELF_RED_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].average[SELF_GREEN_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].std_sum[SELF_GREEN_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].average[SELF_BLUE_CH],
			eol_data->eol_hrm_data[EOL_MODE_DC_HIGH].std_sum[SELF_BLUE_CH]);
		HRM_dbg("%s - EOL_DC_XTALK RESULT,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld\n",
				__func__, eol_data->eol_hrm_data[EOL_MODE_DC_IOFFDAC].average[SELF_IR_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_IOFFDAC].std_sum[SELF_IR_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_IOFFDAC].average[SELF_RED_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_IOFFDAC].std_sum[SELF_RED_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_IOFFDAC].average[SELF_GREEN_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_IOFFDAC].std_sum[SELF_GREEN_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_IOFFDAC].average[SELF_BLUE_CH],
				eol_data->eol_hrm_data[EOL_MODE_DC_IOFFDAC].std_sum[SELF_BLUE_CH]);

		HRM_dbg("%s - EOL FREQ RESULT,%d\n", __func__, eol_data->eol_freq_data.count);

		sfh7832_make_eol_item(eol_data);

		snprintf(sfh7832_data->eol_test_result, SFH_EOL_RESULT,
			"%s,%d,"
			"%s,%d,%d,%d,%d,"
			"%s,%d,%d,%d,%d,"
			"%s,%d,%d,%d,%d,"
			"%s,%d,%d,%d,%d,"
			"%s,%d,%d,%d,%d,"
			"%s,%d,%d,%d,%d,"
			"%s,%d,%d,%d,%d,"
			"%s,%d,%d,%d,%d,"
			"%s,%d,%d,%d,%d,"
			"%s,%d,%d,%d,%d,"
			"%s,%d,%d,%d,%d,"
			"%s,%d,%d,%d,%d,"
			"%s,%d,%d,%d,%d,"
			"%s,%d,%d,%d,%d,"
			"%s,%d,%d,%d,%d,"
			"%s,%d,%d,%d,%d,"
			"%s,%d,%d,%d,%d,"
			"%s,%d,%d,%d,%d",
			eol_data->eol_item_data.test_type, EOL_ITEM_CNT,
			EOL_SYSTEM_NOISE,
			eol_data->eol_item_data.system_noise[EOL_PASS_FAIL],
			eol_data->eol_item_data.system_noise[EOL_MEASURE],
			eol_data->eol_item_data.system_noise[EOL_SPEC_MIN],
			eol_data->eol_item_data.system_noise[EOL_SPEC_MAX],
			EOL_IR_MID,
			eol_data->eol_item_data.ir_mid[EOL_PASS_FAIL],
			eol_data->eol_item_data.ir_mid[EOL_MEASURE],
			eol_data->eol_item_data.ir_mid[EOL_SPEC_MIN],
			eol_data->eol_item_data.ir_mid[EOL_SPEC_MAX],
			EOL_RED_MID,
			eol_data->eol_item_data.red_mid[EOL_PASS_FAIL],
			eol_data->eol_item_data.red_mid[EOL_MEASURE],
			eol_data->eol_item_data.red_mid[EOL_SPEC_MIN],
			eol_data->eol_item_data.red_mid[EOL_SPEC_MAX],
			EOL_GREEN_MID,
			eol_data->eol_item_data.green_mid[EOL_PASS_FAIL],
			eol_data->eol_item_data.green_mid[EOL_MEASURE],
			eol_data->eol_item_data.green_mid[EOL_SPEC_MIN],
			eol_data->eol_item_data.green_mid[EOL_SPEC_MAX],
			EOL_BLUE_MID,
			eol_data->eol_item_data.blue_mid[EOL_PASS_FAIL],
			eol_data->eol_item_data.blue_mid[EOL_MEASURE],
			eol_data->eol_item_data.blue_mid[EOL_SPEC_MIN],
			eol_data->eol_item_data.blue_mid[EOL_SPEC_MAX],
			EOL_IR_HIGH,
			eol_data->eol_item_data.ir_high[EOL_PASS_FAIL],
			eol_data->eol_item_data.ir_high[EOL_MEASURE],
			eol_data->eol_item_data.ir_high[EOL_SPEC_MIN],
			eol_data->eol_item_data.ir_high[EOL_SPEC_MAX],
			EOL_RED_HIGH,
			eol_data->eol_item_data.red_high[EOL_PASS_FAIL],
			eol_data->eol_item_data.red_high[EOL_MEASURE],
			eol_data->eol_item_data.red_high[EOL_SPEC_MIN],
			eol_data->eol_item_data.red_high[EOL_SPEC_MAX],
			EOL_GREEN_HIGH,
			eol_data->eol_item_data.green_high[EOL_PASS_FAIL],
			eol_data->eol_item_data.green_high[EOL_MEASURE],
			eol_data->eol_item_data.green_high[EOL_SPEC_MIN],
			eol_data->eol_item_data.green_high[EOL_SPEC_MAX],
			EOL_BLUE_HIGH,
			eol_data->eol_item_data.blue_high[EOL_PASS_FAIL],
			eol_data->eol_item_data.blue_high[EOL_MEASURE],
			eol_data->eol_item_data.blue_high[EOL_SPEC_MIN],
			eol_data->eol_item_data.blue_high[EOL_SPEC_MAX],
			EOL_IR_OFFDAC,
			eol_data->eol_item_data.ir_offdac[EOL_PASS_FAIL],
			eol_data->eol_item_data.ir_offdac[EOL_MEASURE],
			eol_data->eol_item_data.ir_offdac[EOL_SPEC_MIN],
			eol_data->eol_item_data.ir_offdac[EOL_SPEC_MAX],
			EOL_RED_OFFDAC,
			eol_data->eol_item_data.red_offdac[EOL_PASS_FAIL],
			eol_data->eol_item_data.red_offdac[EOL_MEASURE],
			eol_data->eol_item_data.red_offdac[EOL_SPEC_MIN],
			eol_data->eol_item_data.red_offdac[EOL_SPEC_MAX],
			EOL_GREEN_OFFDAC,
			eol_data->eol_item_data.green_offdac[EOL_PASS_FAIL],
			eol_data->eol_item_data.green_offdac[EOL_MEASURE],
			eol_data->eol_item_data.green_offdac[EOL_SPEC_MIN],
			eol_data->eol_item_data.green_offdac[EOL_SPEC_MAX],
			EOL_BLUE_OFFDAC,
			eol_data->eol_item_data.blue_offdac[EOL_PASS_FAIL],
			eol_data->eol_item_data.blue_offdac[EOL_MEASURE],
			eol_data->eol_item_data.blue_offdac[EOL_SPEC_MIN],
			eol_data->eol_item_data.blue_offdac[EOL_SPEC_MAX],
			EOL_IR_NOISE,
			eol_data->eol_item_data.ir_noise[EOL_PASS_FAIL],
			eol_data->eol_item_data.ir_noise[EOL_MEASURE],
			eol_data->eol_item_data.ir_noise[EOL_SPEC_MIN],
			eol_data->eol_item_data.ir_noise[EOL_SPEC_MAX],
			EOL_RED_NOISE,
			eol_data->eol_item_data.red_noise[EOL_PASS_FAIL],
			eol_data->eol_item_data.red_noise[EOL_MEASURE],
			eol_data->eol_item_data.red_noise[EOL_SPEC_MIN],
			eol_data->eol_item_data.red_noise[EOL_SPEC_MAX],
			EOL_GREEN_NOISE,
			eol_data->eol_item_data.green_noise[EOL_PASS_FAIL],
			eol_data->eol_item_data.green_noise[EOL_MEASURE],
			eol_data->eol_item_data.green_noise[EOL_SPEC_MIN],
			eol_data->eol_item_data.green_noise[EOL_SPEC_MAX],
			EOL_BLUE_NOISE,
			eol_data->eol_item_data.blue_noise[EOL_PASS_FAIL],
			eol_data->eol_item_data.blue_noise[EOL_MEASURE],
			eol_data->eol_item_data.blue_noise[EOL_SPEC_MIN],
			eol_data->eol_item_data.blue_noise[EOL_SPEC_MAX],
			EOL_FREQUENCY,
			eol_data->eol_item_data.frequency[EOL_PASS_FAIL],
			eol_data->eol_item_data.frequency[EOL_MEASURE],
			eol_data->eol_item_data.frequency[EOL_SPEC_MIN],
			eol_data->eol_item_data.frequency[EOL_SPEC_MAX]);

		HRM_dbg("%s result - %s\n", __func__, sfh7832_data->eol_test_result);

		sfh7832_data->eol_test_status = 1;
	}
	if (eol_data->eol_xtalk_data.done) {
		for (i = 0; i < EOL_XTALK_SIZE; i++)
			HRM_dbg("%s - EOL_XTALK_MODE EOLRAW,%lld\n",
				 __func__, eol_data->eol_xtalk_data.buf[i]);

		sfh7832_make_eol_item(eol_data);

		snprintf(sfh7832_data->eol_test_result, SFH_EOL_RESULT,
			"%s,%d,%s,%d,%d,%d,%d",
			eol_data->eol_item_data.test_type, 1,
			EOL_HRM_XTALK,
			eol_data->eol_item_data.xtalk[EOL_PASS_FAIL],
			eol_data->eol_item_data.xtalk[EOL_MEASURE],
			eol_data->eol_item_data.xtalk[EOL_SPEC_MIN],
			eol_data->eol_item_data.xtalk[EOL_SPEC_MAX]);

		HRM_dbg("%s result - %s\n", __func__, sfh7832_data->eol_test_result);
		sfh7832_data->eol_test_status = 1;
	}
}

int sfh7832_eol_read_data(u32 *data)
{
	int err = 0;
	int i = 0;
	int value = 0;
	u8 *recvData = sfh7832_data->recvData;
	u32 rw_diff;

	/* reset hrm_data_cnt for 1 fifo depth */
	sfh7832_data->hrm_data_cnt = 0;
	sfh7832_data->flicker_data_cnt = 0;

	err = sfh7832_read_reg(AFE4420_POINTER_DIFF, &rw_diff);
	if (err != 0)
		return err;

	if (rw_diff < (sfh7832_data->fifo_depth * sfh7832_data->n_phase) - 1) {
		HRM_err("%s unexpected interrupt signal 0x6D : (%d), size : (%d)\n",
			__func__, rw_diff, (sfh7832_data->fifo_depth * sfh7832_data->n_phase) - 1);
		return -1;
	}
	HRM_dbg("%s rw_diff 0x6D : (%d) SIZE : (%d)\n",
		__func__, rw_diff, (sfh7832_data->fifo_depth * sfh7832_data->n_phase) - 1);

	switch (sfh7832_data->enabled_mode) {
	case MODE_AMBIENT:
		/* EOL fliker */
		err = sfh7832_read_burst_reg(AFE4420_FIFO_REG, &recvData[0],
							sfh7832_data->fifo_depth * sfh7832_data->n_phase * NUM_BYTES_PER_SAMPLE);
		if (err != 0)
			return err;

		for (i = 0; i < sfh7832_data->fifo_depth * sfh7832_data->n_phase; i++) {
			value = ((recvData[i * 3] << 16) |
				(recvData[i * 3 + 1] << 8) | recvData[i * 3 + 2]);

			if (value & 1 << 21)
				value = ((~value & 0x1FFFFF) + 1);
			else
				value = -(value & 0x1FFFFF);

			data[i] = sfh7832_data->flicker_data[sfh7832_data->flicker_data_cnt++] = value;
		}

		sfh7832_data->sample_cnt++;
		sfh7832_data->eol_data.eol_count += 20;
		break;
	case MODE_HRM:
		err = sfh7832_read_burst_reg(AFE4420_FIFO_REG, &recvData[0],
						sfh7832_data->fifo_depth * sfh7832_data->n_phase * NUM_BYTES_PER_SAMPLE);
			if (err != 0)
				return err;

		for (i = 0; i < sfh7832_data->fifo_depth * sfh7832_data->n_phase; i++) {
			value = ((recvData[i * 3] << 16) |
				(recvData[i * 3 + 1] << 8) | recvData[i * 3 + 2]);

			if (value & 1 << 21)
				value = ((~value & 0x1FFFFF) + 1);
			else
				value = -(value & 0x1FFFFF);

			sfh7832_data->hrm_data[sfh7832_data->hrm_data_cnt++] = value;
		}
		data[0] = sfh7832_data->hrm_data[1] - sfh7832_data->hrm_data[0];
		data[1] = sfh7832_data->hrm_data[2] - sfh7832_data->hrm_data[3];

		sfh7832_data->sample_cnt++;
		sfh7832_data->eol_data.eol_count++;
		break;
	case MODE_SDK_IR:
		err = sfh7832_read_burst_reg(AFE4420_FIFO_REG, &recvData[0],
						sfh7832_data->fifo_depth * sfh7832_data->n_phase * NUM_BYTES_PER_SAMPLE);
			if (err != 0)
				return err;

		for (i = 0; i < sfh7832_data->fifo_depth * sfh7832_data->n_phase; i++) {
			value = ((recvData[i * 3] << 16) |
				(recvData[i * 3 + 1] << 8) | recvData[i * 3 + 2]);

			if (value & 1 << 21)
				value = ((~value & 0x1FFFFF) + 1);
			else
				value = -(value & 0x1FFFFF);

			sfh7832_data->hrm_data[sfh7832_data->hrm_data_cnt++] = value;
		}
		data[0] = sfh7832_data->hrm_data[1] - sfh7832_data->hrm_data[0];
		data[1] = sfh7832_data->hrm_data[2] - sfh7832_data->hrm_data[0];
		data[2] = sfh7832_data->hrm_data[4] - sfh7832_data->hrm_data[3];
		data[3] = sfh7832_data->hrm_data[5] - sfh7832_data->hrm_data[3];

		sfh7832_data->sample_cnt++;
		sfh7832_data->eol_data.eol_count++;
		break;
	default:
		break;
	}

	switch (sfh7832_data->eol_data.state) {
	case _EOL_STATE_TYPE_INIT:
		HRM_dbg("%s _EOL_STATE_TYPE_INIT\n", __func__);
		err = 1;
		if (sfh7832_data->eol_data.eol_count >= EOL_START_SKIP_CNT) {
			sfh7832_data->eol_data.state = _EOL_STATE_TYPE_DC_MODE_MID;
			sfh7832_data->eol_data.eol_count = 0;
		}
		break;
	case _EOL_STATE_TYPE_DC_MODE_MID:
		sfh7832_eol_hrm_data(data[0], data[1], data[2], data[3],
								&sfh7832_data->eol_data, EOL_MODE_DC_MID,
								EOL_DC_MIDDLE_SKIP_CNT,
								EOL_DC_MIDDLE_SIZE);

		if (sfh7832_data->eol_data.eol_hrm_data[EOL_MODE_DC_MID].done == EOL_SUCCESS) {
			HRM_dbg("%s FINISH : _EOL_STATE_TYPE_DC_MODE_MID\n", __func__);

			err = sfh7832_set_current
				(EOL_DC_HIGH_LED_IR_CURRENT,
				EOL_DC_HIGH_LED_RED_CURRENT,
				EOL_DC_HIGH_LED_GREEN_CURRENT,
				EOL_DC_HIGH_LED_BLUE_CURRENT,
				LED_ALL);
			if (err != 0) {
				HRM_dbg("%s - error sfh7832_set_current!\n", __func__);
				return err;
			}

			sfh7832_data->eol_data.state = _EOL_STATE_TYPE_DC_MODE_HIGH;
			sfh7832_data->eol_data.eol_count = 0;
		}
		break;
	case _EOL_STATE_TYPE_DC_MODE_HIGH:
		sfh7832_eol_hrm_data(data[0], data[1], data[2], data[3],
								&sfh7832_data->eol_data, EOL_MODE_DC_HIGH,
								EOL_DC_HIGH_SKIP_CNT,
								EOL_DC_HIGH_SIZE);

		if (sfh7832_data->eol_data.eol_hrm_data[EOL_MODE_DC_HIGH].done == EOL_SUCCESS) {
			HRM_dbg("%s FINISH : _EOL_STATE_TYPE_DC_MODE_HIGH\n", __func__);

			err = sfh7832_set_ioffset(-12, -12, -6, -6); /* 6uA, 3uA */
			if (err != 0) {
				HRM_dbg("%s - error sfh7832_set_ioffset!\n", __func__);
				return err;
			}

			sfh7832_data->eol_data.state = _EOL_STATE_TYPE_DC_IOFFDAC;
			sfh7832_data->eol_data.eol_count = 0;
		}
		break;
	case _EOL_STATE_TYPE_DC_IOFFDAC:
		sfh7832_eol_hrm_data(data[0], data[1], data[2], data[3],
								&sfh7832_data->eol_data, EOL_MODE_DC_IOFFDAC,
								EOL_DC_IOFFDAC_SKIP_CNT,
								EOL_DC_IOFFDAC_SIZE);

		if (sfh7832_data->eol_data.eol_hrm_data[EOL_MODE_DC_IOFFDAC].done == EOL_SUCCESS) {
			HRM_dbg("%s FINISH : _EOL_STATE_TYPE_DC_IOFFDAC\n", __func__);
			err = sfh7832_set_ioffset(0x00, 0x00, 0x00, 0x00);
			if (err != 0) {
				HRM_dbg("%s - error sfh7832_set_ioffset!\n", __func__);
				return err;
			}
			sfh7832_enable_eol_flicker();
			if (err != 0) {
				HRM_err("%s - err sfh7832_enable_eol_flicker!\n", __func__);
				return err;
			}
			sfh7832_data->eol_data.eol_count = 0;
		}
		break;
	case _EOL_STATE_TYPE_FLICKER_INIT:
		if (sfh7832_data->eol_data.eol_count > EOL_START_SKIP_CNT) {
			HRM_dbg("%s FINISH : _EOL_STATE_TYPE_FLICKER_INIT\n", __func__);
			sfh7832_data->eol_data.state = _EOL_STATE_TYPE_FLICKER_MODE;
			sfh7832_data->eol_data.eol_count = 0;
		}
		break;
	case _EOL_STATE_TYPE_FLICKER_MODE:
		sfh7832_eol_flicker_data(&data[0], &sfh7832_data->eol_data);

		if (sfh7832_data->eol_data.eol_flicker_data.done == EOL_SUCCESS) {
			HRM_dbg("%s FINISH : _EOL_STATE_TYPE_FLICKER_MODE : %d, %d\n",
					__func__, sfh7832_data->eol_data.eol_flicker_data.index,
					sfh7832_data->eol_data.eol_flicker_data.done);

			sfh7832_data->eol_data.state = _EOL_STATE_TYPE_END;
			sfh7832_data->eol_data.eol_count = 0;
		}
		break;
	case _EOL_STATE_TYPE_END:
		sfh7832_eol_check_done(&sfh7832_data->eol_data);
		sfh7832_data->eol_data.state = _EOL_STATE_TYPE_STOP;
		break;
	case _EOL_STATE_TYPE_STOP:
		sfh7832_eol_set_mode(PWR_OFF);
		sfh7832_data->enabled_mode = MODE_NONE;
		break;
	case _EOL_STATE_TYPE_XTALK_MODE:
		sfh7832_eol_xtalk_data(data[0], &sfh7832_data->eol_data);
	
		if (sfh7832_data->eol_data.eol_xtalk_data.done == EOL_SUCCESS) {
			HRM_dbg("%s FINISH : _EOL_STATE_TYPE_XTALK_MODE\n", __func__);
	
			sfh7832_data->eol_data.state = _EOL_STATE_TYPE_END;
			sfh7832_data->eol_data.eol_count = 0;
		}
		break;
	default:
		break;
	}

	return err;
}

int sfh7832_hrm_read_data(s32 *data)
{
	int err = 0;
	int i = 0;
	int value = 0;
	u32 rw_diff;

	/* default PHASE_PER_SAMPLES =5 */
	u8 *recvData = sfh7832_data->recvData;

	/* reset hrm_data_cnt for 1 fifo depth */
	sfh7832_data->hrm_data_cnt = 0;

	err = sfh7832_read_reg(AFE4420_POINTER_DIFF, &rw_diff);
	if (err != 0)
		return err;

	if (rw_diff < (sfh7832_data->fifo_depth * sfh7832_data->n_phase) - 1) {
		HRM_info("%s unexpected interrupt signal 0x6D : (%d), size : (%d)\n",
			__func__, rw_diff, (sfh7832_data->fifo_depth * sfh7832_data->n_phase) - 1);
		return -1;
	}
	HRM_info("%s rw_diff 0x6D : (%d) SIZE : (%d)\n",
		__func__, rw_diff, (sfh7832_data->fifo_depth * sfh7832_data->n_phase) - 1);

	err = sfh7832_read_burst_reg(AFE4420_FIFO_REG,
				     &recvData[0],
				     sfh7832_data->fifo_depth *
				     sfh7832_data->n_phase *
				     NUM_BYTES_PER_SAMPLE);
	if (err != 0)
		return err;

	for (i = 0; i < sfh7832_data->fifo_depth * sfh7832_data->n_phase; i++) {
		value = ((recvData[i * 3] << 16) | (recvData[i * 3 + 1] << 8) | recvData[i * 3 + 2]);

		if (value & 1 << 21) /* 23 */
			value = ((~value & 0x1FFFFF) + 1);
		else
			value = -(value & 0x1FFFFF);

		sfh7832_data->hrm_data[sfh7832_data->hrm_data_cnt++] = value;
	}

	switch (sfh7832_data->enabled_mode) {
	case MODE_HRM:
		HRM_info("HRM SIZE(%d)AMB(%d)IR(%d)RED(%d)AMB(%d)\n", sfh7832_data->fifo_depth,
			sfh7832_data->hrm_data[0],
			sfh7832_data->hrm_data[1],
			sfh7832_data->hrm_data[2],
			sfh7832_data->hrm_data[3]);
		data[0] = sfh7832_data->hrm_data[1];
		data[1] = sfh7832_data->hrm_data[2];
		data[2] = sfh7832_data->hrm_data[0];	/* amb */
		data[3] = sfh7832_data->hrm_data[3];	/* amb */
		break;

	case MODE_PROX:
		HRM_info("PROX AMB(%d)IR(%d)\n", sfh7832_data->hrm_data[0],
			sfh7832_data->hrm_data[1]);
		data[0] = sfh7832_data->hrm_data[1];
		data[1] = sfh7832_data->hrm_data[0];	/* amb */
		data[2] = 0;
		data[3] = 0;
		data[4] = 0;
		break;

	case MODE_SDK_IR:
		HRM_info("SDK AMB(%d)IR(%d)RED(%d)AMB(%d)GREEN(%d)BLUE(%d)\n",
			sfh7832_data->hrm_data[0],
			sfh7832_data->hrm_data[1],
			sfh7832_data->hrm_data[2],
			sfh7832_data->hrm_data[3],
			sfh7832_data->hrm_data[4],
			sfh7832_data->hrm_data[5]);
		data[0] = sfh7832_data->hrm_data[1];
		data[1] = sfh7832_data->hrm_data[2];
		data[2] = sfh7832_data->hrm_data[4];
		data[3] = sfh7832_data->hrm_data[5];
		data[4] = sfh7832_data->hrm_data[0];	/* amb */
		data[5] = sfh7832_data->hrm_data[3];	/* amb */
		break;

	case MODE_AMBIENT:
		data[0] = 0;
		data[1] = 0;
		data[2] = 0;
		data[3] = 0;
		data[4] = sfh7832_data->hrm_data[0];
		break;

	default:
		break;
	}

	sfh7832_data->sample_cnt++;
	return err;

}

int sfh7832_awb_flicker_read_data(s32 *data)
{
	int err = 0;
	int i = 0;
	int value = 0;
	u32 rw_diff;
	/* please change the PHASE_PER_SAMPLES for different phase depend on mode */
	u8 *recvData = sfh7832_data->recvData;

	err = sfh7832_read_reg(AFE4420_POINTER_DIFF, &rw_diff);
	if (err != 0)
		return err;

	if (rw_diff < (sfh7832_data->fifo_depth * sfh7832_data->n_phase) - 1) {
		HRM_info("%s unexpected interrupt signal 0x6D : (%d), size : (%d)\n",
			__func__, rw_diff, (sfh7832_data->fifo_depth * sfh7832_data->n_phase) - 1);
		return -2;
	}
	HRM_info("%s rw_diff 0x6D : (%d) SIZE : (%d)\n",
		__func__, rw_diff, (sfh7832_data->fifo_depth * sfh7832_data->n_phase) - 1);

	err = sfh7832_read_burst_reg(AFE4420_FIFO_REG, recvData,
				     sfh7832_data->fifo_depth *
				     sfh7832_data->n_phase *
				     NUM_BYTES_PER_SAMPLE);
	if (err != 0)
		return err;

	for (i = 0; i < sfh7832_data->fifo_depth * sfh7832_data->n_phase; i++) {
		value = ((recvData[i * 3] << 16) | (recvData[i * 3 + 1] << 8) | recvData[i * 3 + 2]);

		if (value & 1 << 21)
			value = ((~value & 0x1FFFFF) + 1);
		else
			value = -(value & 0x1FFFFF);

		data[0] = sfh7832_data->flicker_data[sfh7832_data->flicker_data_cnt++] = value + FULL_SCALE;
	}

	return err;
}

long sfh7832_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;

	struct sfh7832_device_data *data = container_of(file->private_data,
							struct
							sfh7832_device_data,
							miscdev);

	HRM_info("%s - ioctl start\n", __func__);
	mutex_lock(&data->flickerdatalock);

	switch (cmd) {
	case AFE4420_IOCTL_READ_FLICKER:
		ret = copy_to_user(argp,
				   data->flicker_data,
				   sizeof(int) * FLICKER_DATA_CNT);
		if (unlikely(ret))
			goto ioctl_error;

		break;

	default:
		HRM_err("%s - invalid cmd\n", __func__);
		break;
	}

	mutex_unlock(&data->flickerdatalock);
	return ret;

ioctl_error:
	mutex_unlock(&data->flickerdatalock);
	HRM_err("%s - read flicker data err(%d)\n", __func__, ret);
	return -ret;
}

static const struct file_operations sfh7832_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.unlocked_ioctl = sfh7832_ioctl,
};


int sfh7832_read_data(struct output_data *data)
{
	int err = 0;

	data->mode = sfh7832_data->enabled_mode;

	if (sfh7832_data->eol_test_is_enable == 1) {
		err = sfh7832_eol_read_data(data->data_main);
		if (err != 0) {
			HRM_err("%s - sfh7832_eol_read_data err : %d\n", __func__, err);
			return err;
		}
	} else {
		if (sfh7832_data->enabled_mode == MODE_HRM) {
			data->main_num = 2;
			err = sfh7832_hrm_read_data(data->data_main);
			if (err != 0) {
				HRM_err("%s - sfh7832_hrm_read_data err : %d\n", __func__, err);
				return err;
			}
		} else if (sfh7832_data->enabled_mode == MODE_SDK_IR) {
			data->main_num = 4;
			err = sfh7832_hrm_read_data(data->data_main);
			if (err != 0) {
				HRM_err("%s - sfh7832_hrm_read_data err : %d\n", __func__, err);
				return err;
			}
		} else if (sfh7832_data->enabled_mode == MODE_PROX) {
			data->main_num = 1;
			err = sfh7832_hrm_read_data(data->data_main);
			if (err != 0) {
				HRM_err("%s - sfh7832_hrm_read_data err : %d\n", __func__, err);
				return err;
			}
		} else if (sfh7832_data->enabled_mode == MODE_AMBIENT) {
			data->main_num = 0;
			if (sfh7832_data->flicker_data_cnt < FLICKER_DATA_CNT) {
				err = sfh7832_awb_flicker_read_data(data->data_main);
				if (err == -2) {
					HRM_info("%s - sfh7832 unexpected interrupt signal\n", __func__);
					return err;
				} else if (err != 0) {
					HRM_err("%s - sfh7832_awb_flicker_read_data err : %d\n", __func__, err);
					return err;
				}
			}
			if (sfh7832_data->flicker_data_cnt % AWB_DATA_CNT == 0) {
				data->main_num = 2;
				if (sfh7832_data->flicker_data_cnt == FLICKER_DATA_CNT) {
					data->data_main[1] = -2 + FULL_SCALE;
					sfh7832_data->flicker_data_cnt = 0;
				}
			}
			return err;
		}

		if (!sfh7832_data->agc_enabled || sfh7832_data->enabled_mode == MODE_PROX) {
			if (sfh7832_data->sample_cnt > AGC_SKIP_CNT)
				sfh7832_data->agc_enabled = 1;
			else
				data->main_num = 0;
		}

		if (sfh7832_data->agc_mode != M_NONE
		    && agc_is_enabled && sfh7832_data->agc_enabled) {
			err = sfh7832_cal_agc(data->data_main[0], data->data_main[1],
					data->data_main[2], data->data_main[3]);
			if (err != 0)
				HRM_err("%s - sfh7832_awb_flicker_read_data err : %d\n", __func__, err);
		}
		if (sfh7832_data->enabled_mode == MODE_HRM) {
			data->data_main[0] = (data->data_main[0] - data->data_main[2]) + FULL_SCALE;
			data->data_main[1] = (data->data_main[1] - data->data_main[3]) + FULL_SCALE;
		} else if (sfh7832_data->enabled_mode == MODE_SDK_IR) {
			data->data_main[0] = (data->data_main[0] - data->data_main[4]) + FULL_SCALE;
			data->data_main[1] = (data->data_main[1] - data->data_main[4]) + FULL_SCALE;
			data->data_main[2] = (data->data_main[2] - data->data_main[5]) + FULL_SCALE;
			data->data_main[3] = (data->data_main[3] - data->data_main[5]) + FULL_SCALE;
		} else if (sfh7832_data->enabled_mode == MODE_PROX) {
			data->data_main[0] = (data->data_main[0] - data->data_main[1]) + FULL_SCALE;
		}
	}

	return err;

}

static void sfh7832_irq_set_state(int irq_enable)
{
	int irq_cnt = atomic_read(&sfh7832_data->irq_state);
	HRM_info("%s - irq_enable : %d, irq_state : %d\n",
		 __func__, irq_enable, irq_cnt);

	if (irq_enable) {
 		if (atomic_inc_return(&sfh7832_data->irq_state) == 1)
			enable_irq(sfh7832_data->hrm_irq);
 		else
 			atomic_dec(&sfh7832_data->irq_state);
	} else {
 		if (atomic_dec_return(&sfh7832_data->irq_state) == 0)
			disable_irq_nosync(sfh7832_data->hrm_irq);
 		else
 			atomic_inc(&sfh7832_data->irq_state);
	}
}

static int sfh7832_power_always_ctrl(struct sfh7832_device_data *data, int onoff)
{
	int rc = 0;
	struct regulator *regulator_vdd_1p8;

	regulator_vdd_1p8 = regulator_get(&data->client->dev, data->vdd_1p8);
	if (IS_ERR(regulator_vdd_1p8) || regulator_vdd_1p8 == NULL) {
		HRM_err("%s - vdd_1p8 regulator_get fail\n", __func__);
		rc = PTR_ERR(regulator_vdd_1p8);
		return rc;
	}

	if (onoff) {
		rc = regulator_enable(regulator_vdd_1p8);
		if (rc != 0) {
			HRM_err("%s - enable vdd_1p8 failed, rc=%d\n", __func__, rc);
			return rc;
		}
		sfh7832_data->regulator_state++;
	} else {
		rc = regulator_disable(regulator_vdd_1p8);
		if (rc != 0) {
			HRM_err("%s - disable vdd_1p8 failed, rc=%d\n",
				__func__, rc);
			return rc;
		}
		sfh7832_data->regulator_state--;
	}
	usleep_range(1000, 1100);
	return rc;
}

static int sfh7832_power_down_ctrl(int onoff)
{
	int rc = 0;

	if (onoff == PWR_ON) {
		rc = sfh7832_write_reg(AFE4420_CONTROL0, 1 << SW_RESET);
		if (rc != 0)
			return rc;

		usleep_range(1000, 1100);

		rc = gpio_direction_output(sfh7832_data->pin_hrm_en, 1);
		if (rc) {
			HRM_err("%s - gpio direction output failed, rc=%d\n",
				__func__, rc);
		}
		usleep_range(2000, 2100);
	} else {
		rc = sfh7832_write_reg(AFE4420_CONTROL1, 1 << PDNAFE);
		if (rc != 0)
			return rc;

		gpio_set_value(sfh7832_data->pin_hrm_en, 0);
		rc = gpio_direction_input(sfh7832_data->pin_hrm_en);
		if (rc) {
			HRM_err("%s - gpio direction input failed, rc=%d\n",
				__func__, rc);
		}
	}
	return rc;
}

static int sfh7832_power_ctrl(int onoff)
{
	int rc = 0;
	static int i2c_1p8_enable;
	struct regulator *regulator_vdd_1p8;
	struct regulator *regulator_i2c_1p8;

	HRM_dbg("%s - onoff : %d, state : %d\n",
		__func__, onoff, sfh7832_data->regulator_state);

	if (onoff == PWR_ON) {
		if (sfh7832_data->regulator_state != 0) {
			HRM_dbg("%s - duplicate regulator : %d\n",
				__func__, onoff);
			sfh7832_data->regulator_state++;
			return 0;
		}
		sfh7832_data->regulator_state++;
		sfh7832_data->pm_state = PM_RESUME;
	} else {
		if (sfh7832_data->regulator_state == 0) {
			HRM_dbg("%s - already off the regulator : %d\n",
				__func__, onoff);
			return 0;
		} else if (sfh7832_data->regulator_state != 1) {
			HRM_dbg("%s - duplicate regulator : %d\n",
				__func__, onoff);
			sfh7832_data->regulator_state--;
			return 0;
		}
		sfh7832_data->regulator_state--;
	}

	if (sfh7832_data->i2c_1p8 != NULL) {
		regulator_i2c_1p8 = regulator_get(&sfh7832_data->client->dev, sfh7832_data->i2c_1p8);
		if (IS_ERR(regulator_i2c_1p8) || regulator_i2c_1p8 == NULL) {
			HRM_err("%s - i2c_1p8 regulator_get fail\n", __func__);
			rc = PTR_ERR(regulator_i2c_1p8);
			regulator_i2c_1p8 = NULL;
			goto get_i2c_1p8_failed;
		}
	}

	regulator_vdd_1p8 = regulator_get(&sfh7832_data->client->dev, sfh7832_data->vdd_1p8);
	if (IS_ERR(regulator_vdd_1p8) || regulator_vdd_1p8 == NULL) {
		HRM_err("%s - vdd_1p8 regulator_get fail\n", __func__);
		rc = PTR_ERR(regulator_vdd_1p8);
		regulator_vdd_1p8 = NULL;
		goto get_vdd_1p8_failed;
	}

	HRM_dbg("%s - onoff = %d\n", __func__, onoff);

	if (onoff == PWR_ON) {
		if (sfh7832_data->i2c_1p8 != NULL && i2c_1p8_enable == 0) {
			rc = regulator_enable(regulator_i2c_1p8);
			i2c_1p8_enable = 1;
			if (rc) {
				HRM_err("enable i2c_1p8 failed, rc=%d\n", rc);
				goto enable_i2c_1p8_failed;
			}
		}

		rc = regulator_enable(regulator_vdd_1p8);
		if (rc) {
			HRM_err("%s - enable vdd_1p8 failed, rc=%d\n",
				__func__, rc);
			goto enable_vdd_1p8_failed;
		}
		usleep_range(1000, 1100);

		rc = sfh7832_write_reg(AFE4420_CONTROL0, 1 << SW_RESET);
		if (rc != 0)
			HRM_dbg("%s - error setting SW_RESET\n", __func__);
		usleep_range(1000, 1100);

		rc = gpio_direction_output(sfh7832_data->pin_hrm_en, 1);
		if (rc) {
			HRM_err("%s - gpio direction output failed, rc=%d\n",
				__func__, rc);
			goto gpio_direction_output_failed;
		}
		usleep_range(2000, 2100);
	} else {
		rc = regulator_disable(regulator_vdd_1p8);
		if (rc) {
			HRM_err("%s - disable vdd_1p8 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}

		gpio_set_value(sfh7832_data->pin_hrm_en, 0);
		rc = gpio_direction_input(sfh7832_data->pin_hrm_en);
		if (rc) {
			HRM_err("%s - gpio direction input failed, rc=%d\n",
				__func__, rc);
		}
	}

	goto done;

gpio_direction_output_failed:
	regulator_disable(regulator_vdd_1p8);
enable_vdd_1p8_failed:
	if (sfh7832_data->i2c_1p8 != NULL) {
		regulator_disable(regulator_i2c_1p8);
		i2c_1p8_enable = 0;
	}
enable_i2c_1p8_failed:
done:
	regulator_put(regulator_vdd_1p8);
get_vdd_1p8_failed:
	if (sfh7832_data->i2c_1p8 != NULL)
		regulator_put(regulator_i2c_1p8);
get_i2c_1p8_failed:
	return rc;

}

static int sfh7832_eol_set_mode(int onoff)
{
	int err = 0;

	if (onoff == PWR_ON) {
		sfh7832_data->eol_test_is_enable = 1;
		if (sfh7832_data->always_on)
			err = sfh7832_power_down_ctrl(PWR_ON);
		else
			err = sfh7832_power_ctrl(PWR_ON);

		if (err < 0)
			HRM_err("%s hrm_regulator_on fail err = %d\n",
				__func__, err);
		sfh7832_pin_control(PWR_ON);
		sfh7832_irq_set_state(PWR_ON);

		sfh7832_data->agc_mode = M_NONE;
		sfh7832_init_eol_data(&sfh7832_data->eol_data);

		if (sfh7832_data->eol_test_type == EOL_XTALK) {
			sfh7832_data->enabled_mode = MODE_HRM;
			sfh7832_data->eol_data.state = _EOL_STATE_TYPE_XTALK_MODE;
			sfh7832_data->ioffset_led1 = 0;
			sfh7832_data->ioffset_led2 = 0;
			sfh7832_data->ioffset_led3 = 0;
			sfh7832_data->ioffset_led4 = 0;
			err = sfh7832_hrm_enable();
		} else {	/* EOL */
			sfh7832_data->eol_data.state = _EOL_STATE_TYPE_INIT;
			err = sfh7832_enable_eol_dc();
		}
		if (err != 0) {
			sfh7832_data->eol_test_is_enable = 0;

			sfh7832_irq_set_state(PWR_OFF);
			sfh7832_pin_control(PWR_OFF);
			
			if (sfh7832_data->always_on)
				err = sfh7832_power_down_ctrl(PWR_OFF);
			else
				err = sfh7832_power_ctrl(PWR_OFF);
			if (err < 0)
				HRM_err("%s hrm_regulator_on fail err = %d\n",
					__func__, err);

			HRM_err("%s err : %d\n", __func__, err);
			return err;
		}
	} else {
		sfh7832_data->eol_test_is_enable = 0;

		sfh7832_irq_set_state(PWR_OFF);
		sfh7832_pin_control(PWR_OFF);

		if (sfh7832_data->always_on)
			err = sfh7832_power_down_ctrl(PWR_OFF);
		else
			err = sfh7832_power_ctrl(PWR_OFF);

		if (err < 0)
			HRM_err("%s hrm_regulator_off fail err = %d\n",
				__func__, err);
	}
	HRM_info("%s - onoff = %d\n", __func__, onoff);

	return err;
}

void sfh7832_set_mode(int onoff, enum op_mode mode)
{
	int err;

	if (onoff == PWR_ON) {
		if (sfh7832_data->always_on)
			err = sfh7832_power_down_ctrl(PWR_ON);
		else
			err = sfh7832_power_ctrl(PWR_ON);

		if (err < 0)
			HRM_err("%s hrm_regulator_on fail err = %d\n", __func__, err);

		sfh7832_pin_control(PWR_ON);
		sfh7832_irq_set_state(PWR_ON);

		err = sfh7832_enable(mode);
		if (err == 0)
			sfh7832_data->enabled_mode = mode;
		else {
			sfh7832_data->enabled_mode = MODE_NONE;
			sfh7832_irq_set_state(PWR_OFF);
			sfh7832_pin_control(PWR_OFF);

			if (sfh7832_data->always_on)
				err = sfh7832_power_down_ctrl(PWR_OFF);
			else
				err = sfh7832_power_ctrl(PWR_OFF);
			if (err < 0)
				HRM_err("%s hrm_regulator_on fail err = %d\n", __func__, err);

			HRM_dbg("%s - enable err : %d\n", __func__, err);
		}

		if (err < 0 && mode == MODE_AMBIENT) {
			input_report_rel(sfh7832_data->hrm_input_dev, REL_Y, -5 + 1);	/* F_ERR_I2C -5 detected i2c error */
			input_sync(sfh7832_data->hrm_input_dev);
			HRM_err("%s - awb mode enable error\n", __func__);
		}
	} else {
		if (sfh7832_data->regulator_state == 0) {
			HRM_dbg("%s - already power off - disable skip\n",
				__func__);
			return;
		}

		sfh7832_data->enabled_mode = 0;
		sfh7832_data->mode_sdk_enabled = 0;
		sfh7832_data->mode_svc_enabled = 0;
		sfh7832_data->ioffset_led1 = 0;
		sfh7832_data->ioffset_led2 = 0;
		sfh7832_data->ioffset_led3 = 0;
		sfh7832_data->ioffset_led4 = 0;
		sfh7832_irq_set_state(PWR_OFF);
		sfh7832_pin_control(PWR_OFF);

		if (sfh7832_data->always_on)
			err = sfh7832_power_down_ctrl(PWR_OFF);
		else
			err = sfh7832_power_ctrl(PWR_OFF);

		if (err < 0)
			HRM_err("%s hrm_regulator_off fail err = %d\n",
				__func__, err);
	}
	HRM_dbg("%s - onoff = %d m : %d c : %d\n",
		__func__, onoff, mode, sfh7832_data->enabled_mode);

}

/* hrm input enable/disable sysfs */
static ssize_t sfh7832_enable_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return snprintf(buf, strlen(buf), "%d\n", sfh7832_data->enabled_mode);
}

static ssize_t sfh7832_enable_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	int on_off;
	enum op_mode mode;

	mutex_lock(&sfh7832_data->activelock);
	if (sysfs_streq(buf, "0")) {
		on_off = PWR_OFF;
		mode = MODE_NONE;
	} else if (sysfs_streq(buf, "1")) {
		on_off = PWR_ON;
		mode = MODE_HRM;
		sfh7832_data->mode_cnt.hrm_cnt++;
	} else if (sysfs_streq(buf, "2")) {
		on_off = PWR_ON;
		mode = MODE_AMBIENT;
		sfh7832_data->mode_cnt.amb_cnt++;
	} else if (sysfs_streq(buf, "3")) {
		on_off = PWR_ON;
		mode = MODE_PROX;
		sfh7832_data->mode_cnt.prox_cnt++;
	} else if (sysfs_streq(buf, "10")) {
		on_off = PWR_ON;
		mode = MODE_SDK_IR;
		sfh7832_data->mode_cnt.sdk_cnt++;
	} else if (sysfs_streq(buf, "11")) {
		on_off = PWR_ON;
		mode = MODE_SDK_RED;
	} else if (sysfs_streq(buf, "12")) {
		on_off = PWR_ON;
		mode = MODE_SDK_GREEN;
	} else if (sysfs_streq(buf, "13")) {
		on_off = PWR_ON;
		mode = MODE_SDK_BLUE;
	} else if (sysfs_streq(buf, "-10")) {
		on_off = PWR_OFF;
		mode = MODE_SDK_IR;
	} else if (sysfs_streq(buf, "-11")) {
		on_off = PWR_OFF;
		mode = MODE_SDK_RED;
	} else if (sysfs_streq(buf, "-12")) {
		on_off = PWR_OFF;
		mode = MODE_SDK_GREEN;
	} else if (sysfs_streq(buf, "-13")) {
		on_off = PWR_OFF;
		mode = MODE_SDK_BLUE;
	} else if (sysfs_streq(buf, "15")) {
		on_off = PWR_ON;
		mode = MODE_SVC_RED;
	} else if (sysfs_streq(buf, "16")) {
		on_off = PWR_ON;
		mode = MODE_SVC_GREEN;
	} else if (sysfs_streq(buf, "17")) {
		on_off = PWR_ON;
		mode = MODE_SVC_BLUE;
	} else if (sysfs_streq(buf, "-15")) {
		on_off = PWR_OFF;
		mode = MODE_SVC_RED;
	} else if (sysfs_streq(buf, "-16")) {
		on_off = PWR_OFF;
		mode = MODE_SVC_GREEN;
	} else if (sysfs_streq(buf, "-17")) {
		on_off = PWR_OFF;
		mode = MODE_SVC_BLUE;
	} else {
		HRM_err("%s - invalid value %d\n", __func__, *buf);
		sfh7832_data->mode_cnt.unkn_cnt++;
		mutex_unlock(&sfh7832_data->activelock);
		return -EINVAL;
	}

	if (mode == MODE_SDK_IR || mode == MODE_SDK_RED
	    || mode == MODE_SDK_GREEN || mode == MODE_SDK_BLUE) {
		HRM_dbg("%s - Check SDK enable %d, %d, %d\n", __func__, on_off, mode, sfh7832_data->mode_sdk_enabled);
		if (on_off == PWR_ON) {
			if (sfh7832_data->mode_sdk_enabled & (1 << (mode - MODE_SDK_IR))) {	/* already enabled */
				/* Do Nothing */
				HRM_dbg("%s - SDK mode already enabled %d\n", __func__, mode);
				mutex_unlock(&sfh7832_data->activelock);
				return count;
			}
			if (sfh7832_data->mode_sdk_enabled == 0) {
				sfh7832_data->mode_sdk_enabled |= (1 << (mode - MODE_SDK_IR));
				mode = MODE_SDK_IR;
				sfh7832_data->mode_cnt.sdk_cnt++;
			} else {
				sfh7832_data->mode_sdk_enabled |= (1 << (mode - MODE_SDK_IR));
				mutex_unlock(&sfh7832_data->activelock);
				return count;
			}
		} else {
			if ((sfh7832_data->mode_sdk_enabled & (1 << (mode - MODE_SDK_IR))) == 0) {	/* Not Enabled */
				/* Do Nothing */
				HRM_dbg("%s - SDK mode not enabled %d\n", __func__, mode);
				mutex_unlock(&sfh7832_data->activelock);
				return count;
			} else {
				/* Oring disable mode */
				sfh7832_data->mode_sdk_enabled &= ~(1 << (mode - MODE_SDK_IR));
				sfh7832_reset_sdk_agc_var(mode - MODE_SDK_IR);

				if (sfh7832_data->mode_sdk_enabled == 0)
					mode = MODE_NONE;
				else {
					mutex_unlock(&sfh7832_data->activelock);
					return count;
				}
			}
		}
		HRM_dbg("%s - Current SDK Mode %02x\n", __func__, sfh7832_data->mode_sdk_enabled);
	} else if (mode == MODE_SVC_IR || mode == MODE_SVC_RED
		|| mode == MODE_SVC_GREEN || mode == MODE_SVC_BLUE) {
		HRM_dbg("%s - SVC en : %d m : %d c : %d\n", __func__, on_off, mode, sfh7832_data->mode_svc_enabled);
		if (on_off == PWR_ON) {
			if (sfh7832_data->mode_svc_enabled & (1<<(mode - MODE_SVC_IR))) { /* already enabled */
				/* Do Nothing */
				HRM_dbg("%s - SVC %d mode already enabled\n", __func__, mode);
				mutex_unlock(&sfh7832_data->activelock);
				return count;
			}
			if (sfh7832_data->mode_svc_enabled == 0) {
				sfh7832_data->mode_svc_enabled |= (1<<(mode - MODE_SVC_IR));
				mode = MODE_SVC_IR;
				sfh7832_data->mode_cnt.unkn_cnt++;
			} else {
				sfh7832_data->mode_svc_enabled |= (1<<(mode - MODE_SVC_IR));
				mutex_unlock(&sfh7832_data->activelock);
				return count;
			}
		} else {
			if ((sfh7832_data->mode_svc_enabled & (1<<(mode - MODE_SVC_IR))) == 0) { /* Not Enabled */
				/* Do Nothing */
				HRM_dbg("%s - SVC %d mode not enabled\n", __func__, mode);
				mutex_unlock(&sfh7832_data->activelock);
				return count;
			}
			/* Oring disable mode */
			sfh7832_data->mode_svc_enabled &= ~(1<<(mode - MODE_SVC_IR));
	
			if (sfh7832_data->mode_svc_enabled == 0) {
				mode = MODE_NONE;
			} else {
				mutex_unlock(&sfh7832_data->activelock);
				return count;
			}
		}
	}

	HRM_dbg("%s en : %d m : %d c : %d\n", __func__, on_off, mode, sfh7832_data->enabled_mode);
	sfh7832_set_mode(on_off, mode);
	mutex_unlock(&sfh7832_data->activelock);

	return count;
}

static ssize_t sfh7832_poll_delay_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	u32 sampling_period_ns = 0;

	switch (sfh7832_data->prf_count) {
	case SFH7832_PRF_100HZ:
		sampling_period_ns = 10000;
		break;
	case SFH7832_PRF_200HZ:
		sampling_period_ns = 5000;
		break;
	case SFH7832_PRF_300HZ:
		sampling_period_ns = 3333;
		break;
	case SFH7832_PRF_400HZ:
		sampling_period_ns = 2500;
		break;
	default:
		sampling_period_ns = 10000;
		break;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", sampling_period_ns);
}

static ssize_t sfh7832_poll_delay_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	u32 sampling_period_ns = 0;
	int err = 0;

	mutex_lock(&sfh7832_data->activelock);

	err = kstrtoint(buf, 10, &sampling_period_ns);

	if (err < 0) {
		HRM_dbg("%s - kstrtoint failed.(%d)\n", __func__, err);
		mutex_unlock(&sfh7832_data->activelock);
		return err;
	}

	switch (sampling_period_ns) {
	case 10000000:
		sfh7832_data->prf_count = SFH7832_PRF_100HZ;
		break;
	case 5000000:
		sfh7832_data->prf_count = SFH7832_PRF_200HZ;
		break;
	case 3333000:
		sfh7832_data->prf_count = SFH7832_PRF_300HZ;
		break;
	case 2500000:
		sfh7832_data->prf_count = SFH7832_PRF_400HZ;
		break;
	default:
		sfh7832_data->prf_count = SFH7832_PRF_100HZ;
		break;
	}

	HRM_dbg("%s - hrm sensor sampling rate is setted as %dns\n", __func__,
		sampling_period_ns);

	mutex_unlock(&sfh7832_data->activelock);

	return size;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		   sfh7832_enable_show, sfh7832_enable_store);
static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   sfh7832_poll_delay_show, sfh7832_poll_delay_store);

static struct attribute *hrm_sysfs_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static struct attribute_group hrm_attribute_group = {
	.attrs = hrm_sysfs_attrs,
};

static ssize_t sfh7832_name_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	char chip_name[NAME_LEN];

	if (sfh7832_data->part_type == PART_TYPE_SFH7831)
		strlcpy(chip_name, SFH7831_CHIP_NAME, strlen(SFH7831_CHIP_NAME) + 1);
	else if (sfh7832_data->part_type == PART_TYPE_SFH7832)
		strlcpy(chip_name, SFH7832_CHIP_NAME, strlen(SFH7832_CHIP_NAME) + 1);
	else
		strlcpy(chip_name, SFHXXXX_CHIP_NAME, strlen(SFHXXXX_CHIP_NAME) + 1);

	return snprintf(buf, PAGE_SIZE, "%s\n", chip_name);
}

static ssize_t sfh7832_vendor_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	char vendor[NAME_LEN];

	strlcpy(vendor, VENDOR, strlen(VENDOR) + 1);

	return snprintf(buf, PAGE_SIZE, "%s\n", vendor);
}

static ssize_t sfh7832_led_current_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	int err;
	int led1, led2, led3, led4;

	mutex_lock(&sfh7832_data->activelock);
	err = sscanf(buf, "%8x", &sfh7832_data->led_current);
	if (err < 0) {
		HRM_err("%s - failed, err = %x\n", __func__, err);
		mutex_unlock(&sfh7832_data->activelock);
		return err;
	}
	led1 = 0x000000ff & sfh7832_data->led_current;
	led2 = (0x0000ff00 & sfh7832_data->led_current) >> 8;
	led3 = (0x00ff0000 & sfh7832_data->led_current) >> 16;
	led4 = (0xff000000 & sfh7832_data->led_current) >> 24;
	HRM_info("%s 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", __func__, led1, led2,
		 led3, led4);

	err = sfh7832_set_current(led1, led2, led3, led4, LED_ALL);
	if (err < 0) {
		HRM_err("%s - failed, err = %x\n", __func__, err);
		mutex_unlock(&sfh7832_data->activelock);
		return err;
	}
	mutex_unlock(&sfh7832_data->activelock);

	return size;
}

static ssize_t sfh7832_led_current_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	u8 led1, led2, led3, led4;

	mutex_lock(&sfh7832_data->activelock);

	led1 = sfh7832_data->iled1; /* get ir */
	led2 = sfh7832_data->iled4; /* get red */
	led3 = sfh7832_data->iled2; /* get green */
	led4 = sfh7832_data->iled3; /* get blue */

	sfh7832_data->led_current = (led1 & 0xff) | ((led2 & 0xff) << 8)
	    | ((led3 & 0xff) << 16) | ((led4 & 0xff) << 24);

	mutex_unlock(&sfh7832_data->activelock);
	HRM_info("%s led1 0x%02x, led2 0x%02x, led3 0x%02x, led4 0x%02x\n",
		 __func__, led1, led2, led3, led4);

	return snprintf(buf, PAGE_SIZE, "%08X\n", sfh7832_data->led_current);
}

static ssize_t sfh7832_flush_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t size)
{
	int ret = 0;
	u8 handle = 0;

	mutex_lock(&sfh7832_data->activelock);
	ret = kstrtou8(buf, 10, &handle);
	if (ret < 0) {
		HRM_err("%s - kstrtou8 failed.(%d)\n", __func__, ret);
		mutex_unlock(&sfh7832_data->activelock);
		return ret;
	}
	HRM_dbg("%s - handle = %d\n", __func__, handle);
	mutex_unlock(&sfh7832_data->activelock);

	input_report_rel(sfh7832_data->hrm_input_dev, REL_MISC, handle);

	return size;
}

static ssize_t sfh7832_int_pin_check_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	/* need to check if this should be implemented */
	HRM_dbg("%s - not implement\n", __func__);
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t sfh7832_lib_ver_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	size_t buf_len;

	mutex_lock(&sfh7832_data->activelock);
	buf_len = strlen(buf) + 1;
	if (buf_len > NAME_LEN)
		buf_len = NAME_LEN;

	if (sfh7832_data->lib_ver != NULL)
		kfree(sfh7832_data->lib_ver);

	sfh7832_data->lib_ver = kzalloc(sizeof(char) * buf_len, GFP_KERNEL);
	if (sfh7832_data->lib_ver == NULL) {
		HRM_err("%s - couldn't allocate memory\n", __func__);
		mutex_unlock(&sfh7832_data->activelock);
		return -ENOMEM;
	}
	strlcpy(sfh7832_data->lib_ver, buf, buf_len);

	HRM_info("%s - lib_ver = %s\n", __func__, sfh7832_data->lib_ver);
	mutex_unlock(&sfh7832_data->activelock);

	return size;
}

static ssize_t sfh7832_lib_ver_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (sfh7832_data->lib_ver == NULL) {
		HRM_err("%s - data->lib_ver is NULL\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s\n", "NULL");
	}
	HRM_info("%s - lib_ver = %s\n", __func__, sfh7832_data->lib_ver);
	return snprintf(buf, PAGE_SIZE, "%s\n", sfh7832_data->lib_ver);
}

static ssize_t sfh7832_threshold_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	int err = 0;

	mutex_lock(&sfh7832_data->activelock);
	err = kstrtoint(buf, 10, &sfh7832_data->hrm_threshold);
	if (err < 0) {
		HRM_err("%s - kstrtoint failed.(%d)\n", __func__, err);
		mutex_unlock(&sfh7832_data->activelock);
		return err;
	}
	HRM_info("%s - threshold = %d\n",
		__func__, sfh7832_data->hrm_threshold);

	mutex_unlock(&sfh7832_data->activelock);
	return size;

}

static ssize_t sfh7832_threshold_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	if (sfh7832_data->hrm_threshold) {
		HRM_info("%s - threshold = %d\n",
			__func__, sfh7832_data->hrm_threshold);
		return snprintf(buf, PAGE_SIZE, "%d\n", sfh7832_data->hrm_threshold);
	} else {
		HRM_info("%s - threshold = 0\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
}

static ssize_t sfh7832_prox_thd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;

	mutex_lock(&sfh7832_data->activelock);
	err = kstrtoint(buf, 10, &sfh7832_data->prox_threshold);
	if (err < 0) {
		HRM_err("%s - kstrtoint failed.(%d)\n", __func__, err);
		mutex_unlock(&sfh7832_data->activelock);
		return err;
	}
	HRM_info("%s - prox threshold = %d\n", __func__, sfh7832_data->prox_threshold);

	mutex_unlock(&sfh7832_data->activelock);
	return size;
}

static ssize_t sfh7832_prox_thd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (sfh7832_data->prox_threshold) {
		HRM_info("%s - prox threshold = %d\n",
			__func__, sfh7832_data->prox_threshold);
		return snprintf(buf, PAGE_SIZE, "%d\n", sfh7832_data->prox_threshold);
	}

	HRM_info("%s - prox threshold = 0\n", __func__);
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t sfh7832_eol_test_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	int test_onoff;

	mutex_lock(&sfh7832_data->activelock);
	if (sysfs_streq(buf, "1")) {	/* eol_test start */
		test_onoff = 1;
		sfh7832_data->eol_test_type = EOL_15_MODE;
	} else if (sysfs_streq(buf, "2")) {	/* semi ft test start */
		test_onoff = 1;
		sfh7832_data->eol_test_type = EOL_SEMI_FT;
	} else if (sysfs_streq(buf, "3")) {	/* xtalk test start */
		test_onoff = 1;
		sfh7832_data->eol_test_type = EOL_XTALK;
	} else if (sysfs_streq(buf, "0")) {	/* eol_test stop */
		test_onoff = 0;
	} else {
		HRM_err("%s: invalid value %d\n", __func__, *buf);
		mutex_unlock(&sfh7832_data->activelock);
		return -EINVAL;
	}
	HRM_dbg("%s: %d\n", __func__, test_onoff);
	if (sfh7832_data->eol_test_is_enable == test_onoff) {
		HRM_err("%s: invalid eol status Pre: %d, AF : %d\n", __func__,
			sfh7832_data->eol_test_is_enable, test_onoff);
		mutex_unlock(&sfh7832_data->activelock);
		return -EINVAL;
	}
	if (sfh7832_eol_set_mode(test_onoff) < 0)
		sfh7832_data->eol_test_is_enable = 0;

	mutex_unlock(&sfh7832_data->activelock);
	return size;
}

static ssize_t sfh7832_eol_test_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", sfh7832_data->eol_test_is_enable);
}

static ssize_t sfh7832_eol_test_result_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	mutex_lock(&sfh7832_data->activelock);

	if (sfh7832_data->eol_test_status == 0) {
		HRM_err("%s - data->eol_test_status is NULL\n", __func__);
		sfh7832_data->eol_test_status = 0;
		mutex_unlock(&sfh7832_data->activelock);
		return snprintf(buf, PAGE_SIZE, "%s\n", "NO_EOL_TEST");
	}
	HRM_dbg("%s - result = %d\n", __func__, sfh7832_data->eol_test_status);
	sfh7832_data->eol_test_status = 0;

	mutex_unlock(&sfh7832_data->activelock);

	return snprintf(buf, PAGE_SIZE, "%s\n", sfh7832_data->eol_test_result);
}

static ssize_t sfh7832_eol_test_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", sfh7832_data->eol_test_status);
}

static ssize_t sfh7832_read_reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	HRM_info("%s - val=0x%06x\n", __func__, sfh7832_data->reg_read_buf);

	return snprintf(buf, PAGE_SIZE, "%06x\n", sfh7832_data->reg_read_buf);
}

static ssize_t sfh7832_read_reg_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	int err = -1;
	unsigned int cmd = 0;
	u32 val = 0;

	mutex_lock(&sfh7832_data->i2clock);
	if (sfh7832_data->regulator_state == 0) {
		HRM_err("%s - need to power on\n", __func__);
		mutex_unlock(&sfh7832_data->i2clock);
		return size;
	}
	err = sscanf(buf, "%8x", &cmd);
	if (err == 0) {
		HRM_err("%s - sscanf fail\n", __func__);
		mutex_unlock(&sfh7832_data->i2clock);
		return size;
	}

	err = sfh7832_read_reg((u8) cmd, &val);
	if (err != 0) {
		HRM_err("%s err=%d, val=0x%06x\n", __func__, err, val);
		mutex_unlock(&sfh7832_data->i2clock);
		return size;
	}
	sfh7832_data->reg_read_buf = val;
	mutex_unlock(&sfh7832_data->i2clock);

	return size;
}

static ssize_t sfh7832_write_reg_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	int err = -1;
	unsigned int cmd = 0;
	unsigned int val = 0;

	mutex_lock(&sfh7832_data->i2clock);
	if (sfh7832_data->regulator_state == 0) {
		HRM_err("%s - need to power on.\n", __func__);
		mutex_unlock(&sfh7832_data->i2clock);
		return size;
	}
	err = sscanf(buf, "%8x, %8x", &cmd, &val);
	if (err == 0) {
		HRM_err("%s - sscanf fail %s\n", __func__, buf);
		mutex_unlock(&sfh7832_data->i2clock);
		return size;
	}

	err = sfh7832_write_reg((u8) cmd, (u32) val);
	if (err < 0) {
		HRM_err("%s fail err = %d\n", __func__, err);
		mutex_unlock(&sfh7832_data->i2clock);
		return err;
	}
	mutex_unlock(&sfh7832_data->i2clock);

	return size;
}

static ssize_t sfh7832_debug_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	HRM_info("%s - debug mode = %u\n", __func__, sfh7832_data->debug_mode);

	if (sfh7832_data->debug_mode == DEBUG_SET_TIA_GAIN)
		return snprintf(buf, PAGE_SIZE, "%04x\n", sfh7832_data->debug_value);
	else if (sfh7832_data->debug_mode == DEBUG_GET_LED_TRIM)
		return snprintf(buf, PAGE_SIZE, "%08x\n", sfh7832_data->debug_value);
	else		
		return snprintf(buf, PAGE_SIZE, "%d\n", sfh7832_data->debug_value);
}

static ssize_t sfh7832_debug_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t size)
{
	int err;
	s32 mode;
	int val = -1;
	u8 buff[4];

	mutex_lock(&sfh7832_data->activelock);

	err = sscanf(buf, "%d, %04x", &mode, &val);
	if (err < 0) {
		HRM_err("%s - failed, err = %x\n", __func__, err);
		mutex_unlock(&sfh7832_data->activelock);
		return err;
	}
	sfh7832_data->debug_mode = (u8) mode;
	HRM_info("%s - mode = %d %d\n", __func__, mode, val);

	switch (sfh7832_data->debug_mode) {
	case DEBUG_REG_STATUS:
		sfh7832_print_reg_status();
		break;
	case DEBUG_ENABLE_AGC:
		agc_is_enabled = 1;
		sfh7832_data->debug_value = (u32)sfh7832_data->debug_mode;
		break;
	case DEBUG_DISABLE_AGC:
		agc_is_enabled = 0;
		sfh7832_data->debug_value = (u32)sfh7832_data->debug_mode;
		break;
	case DEBUG_SET_TIA_GAIN:
		if (val < 0) {
			err = sfh7832_get_rf(&buff[0], &buff[1], &buff[2], &buff[3]);
			if (err < 0) {
				HRM_err("%s - failed, err = %x\n", __func__, err);
				mutex_unlock(&sfh7832_data->activelock);
				return err;
			}

			sfh7832_data->debug_value = (buff[0] & 0xf) | ((buff[1] & 0xf) << 4)
			    | ((buff[2] & 0xf) << 8) | ((buff[3] & 0xf) << 12);
		} else {
			buff[0] = 0x000f & val;
			buff[1] = (0x00f0 & val) >> 4;
			buff[2] = (0x0f00 & val) >> 8;
			buff[3] = (0xf000 & val) >> 12;

			err = sfh7832_set_rf(buff[0], buff[1], buff[2], buff[3]);
			if (err < 0) {
				HRM_err("%s - failed, err = %x\n", __func__, err);
				mutex_unlock(&sfh7832_data->activelock);
				return err;
			}
		}
		break;
	case DEBUG_SET_TIA_CF:
		if (val < 0) {
			err = sfh7832_get_cf(&buff[0]);
			if (err < 0) {
				HRM_err("%s - failed, err = %x\n", __func__, err);
				mutex_unlock(&sfh7832_data->activelock);
				return err;
			}
			sfh7832_data->debug_value = (u32)buff[0];
		} else {
			err = sfh7832_set_cf(val);
			if (err < 0) {
				HRM_err("%s - failed, err = %x\n", __func__, err);
				mutex_unlock(&sfh7832_data->activelock);
				return err;
			}
		}
		break;
	case DEBUG_SET_IOFF_DAC:
		if (val < 0) {
			err = sfh7832_get_ioff_dac(&buff[0]);
			if (err < 0) {
				HRM_err("%s - failed, err = %x\n", __func__, err);
				mutex_unlock(&sfh7832_data->activelock);
				return err;
			}
			sfh7832_data->debug_value = (u32)buff[0];
		} else  {
			err = sfh7832_set_ioff_dac(val);
			if (err < 0) {
				HRM_err("%s - failed, err = %x\n", __func__, err);
				mutex_unlock(&sfh7832_data->activelock);
				return err;
			}
		}
		break;
	case DEBUG_SET_CURRENT_RANGE:
		switch (val) {
		case -1:
			sfh7832_data->debug_value = (u32)sfh7832_data->max_current;
			break;
		case 0:
			sfh7832_data->max_current = 50000;
			break;
		case 1:
			sfh7832_data->max_current = 100000;
			break;
		case 2:
			sfh7832_data->max_current = 150000;
			break;
		case 3:
			sfh7832_data->max_current = 200000;
			break;
		case 4:
			sfh7832_data->max_current = 250000;
			break;
		default:
			sfh7832_data->max_current = 250000;
			break;
		}
		break;
	case DEBUG_SET_NUMAV:
		if (val < 0) {
			err = sfh7832_get_numav(&buff[0]);
			if (err < 0) {
				HRM_err("%s - failed, err = %x\n", __func__, err);
				mutex_unlock(&sfh7832_data->activelock);
				return err;
			}
			sfh7832_data->debug_value = (u32)buff[0];
		} else {
			err = sfh7832_set_numav(val);
			if (err < 0) {
				HRM_err("%s - failed, err = %x\n", __func__, err);
				mutex_unlock(&sfh7832_data->activelock);
				return err;
			}
		}
		break;
	case DEBUG_GET_LED_TRIM:
		sfh7832_data->debug_value = (sfh7832_data->trmled1 & 0xff) | ((sfh7832_data->trmled4 & 0xff) << 8)
	    | ((sfh7832_data->trmled2 & 0xff) << 16) | ((sfh7832_data->trmled3 & 0xff) << 24);
		break;
	default:
		break;
	}

	mutex_unlock(&sfh7832_data->activelock);

	return size;
}

static ssize_t sfh7832_device_id_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int err;
	u32 device_id = 0;

	if (sfh7832_data->always_on)
		err = sfh7832_power_down_ctrl(PWR_ON);
	else
		err = sfh7832_power_ctrl(PWR_ON);

	if (err < 0)
		HRM_err("%s hrm_regulator_on fail err = %d\n", __func__, err);

	mutex_lock(&sfh7832_data->activelock);

	err = sfh7832_get_chipid(&device_id);
	if (err != 0)
		HRM_err("%s sfh7832_get_chipid fail err = %d\n", __func__, err);

	mutex_unlock(&sfh7832_data->activelock);

	if (sfh7832_data->always_on)
		err = sfh7832_power_down_ctrl(PWR_OFF);
	else
		err = sfh7832_power_ctrl(PWR_OFF);

	if (err < 0)
		HRM_err("%s hrm_regulator_off fail err = %d\n", __func__, err);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", device_id);
}

static ssize_t sfh7832_part_type_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	u16 part_type = 0;

	mutex_lock(&sfh7832_data->activelock);

	sfh7832_get_part_type(&part_type);

	mutex_unlock(&sfh7832_data->activelock);

	return snprintf(buf, PAGE_SIZE, "%d\n", part_type);
}

static ssize_t sfh7832_i2c_err_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sfh7832_data->i2c_err_cnt);
}

static ssize_t sfh7832_i2c_err_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	sfh7832_data->i2c_err_cnt = 0;

	return size;
}

static ssize_t sfh7832_curr_adc_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			"\"HRIC\":\"%d\",\"HRRC\":\"%d\",\"HRIA\":\"%d\",\"HRRA\":\"%d\"\n",
			sfh7832_data->iled1, sfh7832_data->iled4, sfh7832_data->ir_adc, sfh7832_data->red_adc);
}

static ssize_t sfh7832_curr_adc_store(struct device *dev,
			      struct device_attribute *attr, const char *buf,
			      size_t size)
{
	sfh7832_data->iled1 = 0;
	sfh7832_data->iled4 = 0;
	sfh7832_data->ir_adc = 0;
	sfh7832_data->red_adc = 0;

	return size;
}

static ssize_t sfh7832_mode_cnt_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			"\"CNT_HRM\":\"%d\",\"CNT_AMB\":\"%d\",\"CNT_PROX\":\"%d\",\"CNT_SDK\":\"%d\",\"CNT_CGM\":\"%d\",\"CNT_UNKN\":\"%d\"\n",
			sfh7832_data->mode_cnt.hrm_cnt, sfh7832_data->mode_cnt.amb_cnt,
			sfh7832_data->mode_cnt.prox_cnt, sfh7832_data->mode_cnt.sdk_cnt,
			sfh7832_data->mode_cnt.cgm_cnt, sfh7832_data->mode_cnt.unkn_cnt);
}

static ssize_t sfh7832_mode_cnt_store(struct device *dev,
			      struct device_attribute *attr, const char *buf,
			      size_t size)
{
	sfh7832_data->mode_cnt.hrm_cnt = 0;
	sfh7832_data->mode_cnt.amb_cnt = 0;
	sfh7832_data->mode_cnt.prox_cnt = 0;
	sfh7832_data->mode_cnt.sdk_cnt = 0;
	sfh7832_data->mode_cnt.cgm_cnt = 0;
	sfh7832_data->mode_cnt.unkn_cnt = 0;

	return size;
}

static ssize_t sfh7832_factory_cmd_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	static char cmd_result[MAX_BUF_LEN];

	mutex_lock(&sfh7832_data->activelock);

	if (sfh7832_data->isTrimmed)
		snprintf(cmd_result, MAX_BUF_LEN, "%d", 1);
	else
		snprintf(cmd_result, MAX_BUF_LEN, "%d", 0);

	HRM_dbg("%s cmd_result = %s\n", __func__, cmd_result);

	mutex_unlock(&sfh7832_data->activelock);

	return snprintf(buf, PAGE_SIZE, "%s\n", cmd_result);
}

static ssize_t sfh7832_version_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	HRM_info("%s - cmd_result = %s.%s.%s%s\n", __func__,
		VERSION, SUB_VERSION, HEADER_VERSION, VENDOR_VERSION);

	return snprintf(buf, PAGE_SIZE, "%s.%s.%s%s\n",
		VERSION, SUB_VERSION, HEADER_VERSION, VENDOR_VERSION);
}

static ssize_t sfh7832_sensor_info_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	HRM_dbg("%s - sensor_info_data not support\n", __func__);

	return snprintf(buf, PAGE_SIZE, "NOT SUPPORT\n");
}

static ssize_t sfh7832_xtalk_code_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	int err;
	int xtalk1, xtalk2, xtalk3, xtalk4;

	mutex_lock(&sfh7832_data->activelock);
	err = sscanf(buf, "%8x", &sfh7832_data->xtalk_code);
	if (err < 0) {
		HRM_err("%s - failed, err = %x\n", __func__, err);
		mutex_unlock(&sfh7832_data->activelock);
		return err;
	}
	xtalk1 = 0x000000ff & sfh7832_data->xtalk_code;
	xtalk2 = (0x0000ff00 & sfh7832_data->xtalk_code) >> 8;
	xtalk3 = (0x00ff0000 & sfh7832_data->xtalk_code) >> 16;
	xtalk4 = (0xff000000 & sfh7832_data->xtalk_code) >> 24;
	HRM_dbg("%s 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", __func__, xtalk1,
		 xtalk2, xtalk3, xtalk4);

	err = sfh7832_set_ioffset(xtalk1, xtalk2, xtalk3, xtalk4);
	if (err < 0) {
		HRM_err("%s - failed, err = %x\n", __func__, err);
		mutex_unlock(&sfh7832_data->activelock);
		return err;
	}
	mutex_unlock(&sfh7832_data->activelock);

	return size;
}

static ssize_t sfh7832_xtalk_code_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	u32 xtalk_code;

	mutex_lock(&sfh7832_data->activelock);

	xtalk_code = (sfh7832_data->ioffset_led1 & 0xff) | ((sfh7832_data->ioffset_led2 & 0xff) << 8)
	    | ((sfh7832_data->ioffset_led3 & 0xff) << 16) | ((sfh7832_data->ioffset_led4 & 0xff) << 24);

	mutex_unlock(&sfh7832_data->activelock);
	HRM_info
	    ("%s xtalk1 0x%02x, xtalk2 0x%02x, xtalk3 0x%02x, xtalk4 0x%02x\n",
	     __func__, sfh7832_data->ioffset_led1, sfh7832_data->ioffset_led2, sfh7832_data->ioffset_led2, sfh7832_data->ioffset_led4);

	return snprintf(buf, PAGE_SIZE, "%08X\n", xtalk_code);
}

static DEVICE_ATTR(name, S_IRUGO, sfh7832_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, sfh7832_vendor_show, NULL);
static DEVICE_ATTR(led_current, S_IRUGO | S_IWUSR | S_IWGRP,
		   sfh7832_led_current_show, sfh7832_led_current_store);
static DEVICE_ATTR(hrm_flush, S_IWUSR | S_IWGRP, NULL, sfh7832_flush_store);
static DEVICE_ATTR(int_pin_check, S_IRUGO, sfh7832_int_pin_check_show, NULL);
static DEVICE_ATTR(lib_ver, S_IRUGO | S_IWUSR | S_IWGRP,
		   sfh7832_lib_ver_show, sfh7832_lib_ver_store);
static DEVICE_ATTR(threshold, S_IRUGO | S_IWUSR | S_IWGRP,
		   sfh7832_threshold_show, sfh7832_threshold_store);
static DEVICE_ATTR(prox_thd, S_IRUGO | S_IWUSR | S_IWGRP,
	sfh7832_prox_thd_show, sfh7832_prox_thd_store);
static DEVICE_ATTR(eol_test, S_IRUGO | S_IWUSR | S_IWGRP,
		   sfh7832_eol_test_show, sfh7832_eol_test_store);
static DEVICE_ATTR(eol_test_result, S_IRUGO, sfh7832_eol_test_result_show, NULL);
static DEVICE_ATTR(eol_test_status, S_IRUGO, sfh7832_eol_test_status_show, NULL);
static DEVICE_ATTR(read_reg, S_IRUGO | S_IWUSR | S_IWGRP,
		   sfh7832_read_reg_show, sfh7832_read_reg_store);
static DEVICE_ATTR(write_reg, S_IWUSR | S_IWGRP, NULL, sfh7832_write_reg_store);
static DEVICE_ATTR(hrm_debug, S_IRUGO | S_IWUSR | S_IWGRP,
		   sfh7832_debug_show, sfh7832_debug_store);
static DEVICE_ATTR(device_id, S_IRUGO, sfh7832_device_id_show, NULL);
static DEVICE_ATTR(part_type, S_IRUGO, sfh7832_part_type_show, NULL);
static DEVICE_ATTR(i2c_err_cnt, S_IRUGO | S_IWUSR | S_IWGRP, sfh7832_i2c_err_show,
		   sfh7832_i2c_err_store);
static DEVICE_ATTR(curr_adc, S_IRUGO | S_IWUSR | S_IWGRP, sfh7832_curr_adc_show,
		   sfh7832_curr_adc_store);
static DEVICE_ATTR(mode_cnt, S_IRUGO | S_IWUSR | S_IWGRP, sfh7832_mode_cnt_show,
		   sfh7832_mode_cnt_store);
static DEVICE_ATTR(hrm_factory_cmd, S_IRUGO, sfh7832_factory_cmd_show, NULL);
static DEVICE_ATTR(hrm_version, S_IRUGO, sfh7832_version_show, NULL);
static DEVICE_ATTR(sensor_info, S_IRUGO, sfh7832_sensor_info_show, NULL);
static DEVICE_ATTR(xtalk_code, S_IRUGO | S_IWUSR | S_IWGRP,
		   sfh7832_xtalk_code_show, sfh7832_xtalk_code_store);

static struct device_attribute *sfh7832_sensor_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_led_current,
	&dev_attr_hrm_flush,
	&dev_attr_int_pin_check,
	&dev_attr_lib_ver,
	&dev_attr_threshold,
	&dev_attr_prox_thd,
	&dev_attr_eol_test,
	&dev_attr_eol_test_result,
	&dev_attr_eol_test_status,
	&dev_attr_read_reg,
	&dev_attr_write_reg,
	&dev_attr_device_id,
	&dev_attr_part_type,
	&dev_attr_i2c_err_cnt,
	&dev_attr_curr_adc,
	&dev_attr_mode_cnt,
	&dev_attr_hrm_debug,
	&dev_attr_hrm_factory_cmd,
	&dev_attr_hrm_version,
	&dev_attr_sensor_info,
	&dev_attr_xtalk_code,
	NULL,
};

irqreturn_t sfh7832_irq_handler(int hrm_irq, void *device)
{
	int err;
	struct output_data read_data;
	int i;
	static unsigned int sample_cnt;

	memset(&read_data, 0, sizeof(struct output_data));

	if (sfh7832_data->regulator_state == 0 || sfh7832_data->enabled_mode == 0 || sfh7832_data->mode_svc_enabled != 0) {
		HRM_dbg("%s - stop irq handler (reg_state : %d, enabled_mode : %d, rear_led : 0x%x)\n",
				__func__, sfh7832_data->regulator_state, sfh7832_data->enabled_mode, sfh7832_data->mode_svc_enabled);
		return IRQ_HANDLED;
	}
#ifdef CONFIG_ARCH_QCOM
		pm_qos_add_request(&sfh7832_data->pm_qos_req_fpm, PM_QOS_CPU_DMA_LATENCY,
			PM_QOS_DEFAULT_VALUE);
#endif

	err = sfh7832_read_data(&read_data);

	if (err == 0) {
		if (sample_cnt++ > 100) {
			HRM_dbg
				("%s mode:0x%x main:%d,%d,%d,%d\n",
				 __func__, read_data.mode, read_data.data_main[0],
				 read_data.data_main[1], read_data.data_main[2],
				 read_data.data_main[3]);
			sample_cnt = 0;
		}

		if (sfh7832_data->hrm_input_dev == NULL) {
			HRM_err("%s - hrm_input_dev is NULL\n", __func__);
		} else if (sfh7832_data->enabled_mode == 0) {
			HRM_err("%s - enabled_mode %d\n", __func__,
				sfh7832_data->enabled_mode);
		} else {
			for (i = 0; i < read_data.main_num; i++)
				input_report_rel(sfh7832_data->hrm_input_dev,
						 REL_X + i,
						 read_data.data_main[i]
						 + 1);

			if (read_data.main_num)
				input_sync(sfh7832_data->hrm_input_dev);
		}
	}

#ifdef CONFIG_ARCH_QCOM
		pm_qos_remove_request(&sfh7832_data->pm_qos_req_fpm);
#endif
	return IRQ_HANDLED;
}

static int sfh7832_setup_irq(void)
{
	int errorno = -EIO;
	int status;

	errorno = request_threaded_irq(sfh7832_data->hrm_irq, NULL,
				       sfh7832_irq_handler,
				       IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				       "hrm_sensor_irq", sfh7832_data);

	if (errorno < 0) {
		HRM_err("%s - failed for setup hrm_irq errono= %d\n",
			__func__, errorno);
		errorno = -ENODEV;
		return errorno;
	}
	disable_irq_nosync(sfh7832_data->hrm_irq);

	if (!IS_ERR_OR_NULL(sfh7832_data->pins_sleep)) {
		status = pinctrl_select_state(sfh7832_data->hrm_pinctrl,
						  sfh7832_data->pins_sleep);
		if (status)
			HRM_err("%s: can't set pin sleep state\n",
			__func__);
	}

	return errorno;
}

static void sfh7832_init_var1(void)
{
	sfh7832_data->client = NULL;
	sfh7832_data->dev = NULL;
	sfh7832_data->hrm_input_dev = NULL;
	sfh7832_data->hrm_pinctrl = NULL;
	sfh7832_data->pins_sleep = NULL;
	sfh7832_data->pins_idle = NULL;
	sfh7832_data->vdd_1p8 = NULL;
	sfh7832_data->i2c_1p8 = NULL;
	sfh7832_data->enabled_mode = 0;
	sfh7832_data->mode_sdk_enabled = 0;
	sfh7832_data->mode_svc_enabled = 0;
	sfh7832_data->regulator_state = 0;
	sfh7832_data->hrm_threshold = DEFAULT_THRESHOLD;
	sfh7832_data->eol_test_is_enable = 0;
	sfh7832_data->eol_test_status = 0;
	sfh7832_data->eol_test_type = EOL_15_MODE;
	sfh7832_data->reg_read_buf = 0;
	sfh7832_data->lib_ver = NULL;
	sfh7832_data->pm_state = PM_RESUME;
	atomic_set(&sfh7832_data->irq_state, 0);
}

int sfh7832_init_var2(void)
{
	int err = 0;
	u32 device_id = 0;

	err = sfh7832_get_chipid(&device_id);
	if (err) {
		HRM_err("%s SFH7832 WHOAMI read fail\n", __func__);
		return -EINVAL;
	}

	sfh7832_data->part_type = PART_TYPE_SFH7832;

	sfh7832_data->i2c_err_cnt = 0;

	sfh7832_data->fifo_depth = 1;
	sfh7832_data->n_phase = 1;

	sfh7832_data->numav = SFH7832_DEFAULT_NUMAV;
	sfh7832_data->ioff_dac = TIA_IFS_OFFDAC_63_5uA;
	sfh7832_data->max_current = SFH7832_MAX_CURRENT;

	sfh7832_data->ioffset_led1 = 0;
	sfh7832_data->ioffset_led2 = 0;
	sfh7832_data->ioffset_led3 = 0;
	sfh7832_data->ioffset_led4 = 0;

	sfh7832_data->rfgain_amb1 = TIA_GAIN_25K;
	sfh7832_data->rfgain_amb2 = TIA_GAIN_50K;
	sfh7832_data->rfgain_led1 = TIA_GAIN_25K;
	sfh7832_data->rfgain_led2 = TIA_GAIN_25K;
	sfh7832_data->rfgain_led3 = TIA_GAIN_50K;
	sfh7832_data->rfgain_led4 = TIA_GAIN_50K;

	sfh7832_data->cfgain_amb1 = TIA_CAP_25P;
	sfh7832_data->cfgain_amb2 = TIA_CAP_25P;
	sfh7832_data->cfgain_led1 = TIA_CAP_25P;
	sfh7832_data->cfgain_led2 = TIA_CAP_25P;
	sfh7832_data->cfgain_led3 = TIA_CAP_25P;
	sfh7832_data->cfgain_led4 = TIA_CAP_25P;

	if (sfh7832_data->init_iled[AGC_IR] == 0) {
		sfh7832_data->init_iled[AGC_IR] = SFH7832_IR_INIT_CURRENT;
		sfh7832_data->init_iled[AGC_RED] = SFH7832_RED_INIT_CURRENT;
		sfh7832_data->init_iled[AGC_GREEN] = SFH7832_GREEN_INIT_CURRENT;
		sfh7832_data->init_iled[AGC_BLUE] = SFH7832_BLUE_INIT_CURRENT;
	}

	/* AGC */
	sfh7832_data->update_led = sfh7832_update_led_current;
	sfh7832_data->agc_led_out_percent = SFH7832_AGC_DEFAULT_LED_OUT_RANGE;
	sfh7832_data->agc_min_num_samples = SFH7832_AGC_DEFAULT_MIN_NUM_PERCENT;
	sfh7832_data->min_base_offset[AGC_IR] = 0;
	sfh7832_data->min_base_offset[AGC_RED] = 0;
	sfh7832_data->min_base_offset[AGC_GREEN] = 0;
	sfh7832_data->min_base_offset[AGC_BLUE] = 0;

	sfh7832_data->remove_cnt[AGC_IR] = 0;
	sfh7832_data->remove_cnt[AGC_RED] = 0;
	sfh7832_data->remove_cnt[AGC_GREEN] = 0;
	sfh7832_data->remove_cnt[AGC_BLUE] = 0;

	return err;

}

static int sfh7832_parse_dt(void)
{
	struct device *dev = &sfh7832_data->client->dev;
	struct device_node *dNode = dev->of_node;
	enum of_gpio_flags flags;
	u32 threshold[2];
	int i;

	if (dNode == NULL)
		return -ENODEV;

	sfh7832_data->pin_hrm_int = of_get_named_gpio_flags(dNode,
						"hrmsensor,hrm_int-gpio", 0,
						&flags);
	if (sfh7832_data->pin_hrm_int < 0) {
		HRM_err("%s - get pin_hrm_int error\n", __func__);
		return -ENODEV;
	}

	sfh7832_data->pin_hrm_en = of_get_named_gpio_flags(dNode,
					       "hrmsensor,hrm_boost_en-gpio", 0,
					       &flags);
	if (sfh7832_data->pin_hrm_en < 0) {
		HRM_err("%s - get hrm_en error\n", __func__);
		return -ENODEV;
	}

	if (of_property_read_string(dNode, "hrmsensor,vdd_1p8",
				    (char const **)&sfh7832_data->vdd_1p8) < 0)
		HRM_dbg("%s - vdd_1p8 doesn`t exist\n", __func__);

	if (of_property_read_string(dNode, "hrmsensor,i2c_1p8",
				    (char const **)&sfh7832_data->i2c_1p8) < 0)
		HRM_dbg("%s -  i2c_1p8 doesn`t exist\n", __func__);

	sfh7832_data->always_on = of_property_read_bool(dev->of_node, "hrmsensor,always");
	if (sfh7832_data->always_on)
		HRM_dbg("%s - VDD_1p8 always on\n", __func__);

	sfh7832_data->hrm_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(sfh7832_data->hrm_pinctrl)) {
		HRM_err("%s: failed pinctrl_get (%li)\n",
			__func__, PTR_ERR(sfh7832_data->hrm_pinctrl));
		sfh7832_data->hrm_pinctrl = NULL;
		return -EINVAL;
	}

	sfh7832_data->pins_sleep = pinctrl_lookup_state(sfh7832_data->hrm_pinctrl, "sleep");
	if (IS_ERR_OR_NULL(sfh7832_data->pins_sleep)) {
		HRM_dbg("%s : could not get pins sleep_state (%li)\n",
			__func__, PTR_ERR(sfh7832_data->pins_sleep));

		devm_pinctrl_put(sfh7832_data->hrm_pinctrl);

		sfh7832_data->pins_sleep = NULL;
		return -EINVAL;
	}

	sfh7832_data->pins_idle = pinctrl_lookup_state(sfh7832_data->hrm_pinctrl, "idle");
	if (IS_ERR_OR_NULL(sfh7832_data->pins_idle)) {
		HRM_err("%s : could not get pins idle_state (%li)\n",
			__func__, PTR_ERR(sfh7832_data->pins_idle));

		devm_pinctrl_put(sfh7832_data->hrm_pinctrl);

		sfh7832_data->pins_idle = NULL;
		return -EINVAL;
	}

	if (of_property_read_u32_array(dNode, "hrmsensor,thd",
		threshold, ARRAY_SIZE(threshold)) < 0) {
		HRM_err("%s - get threshold error\n", __func__);
	} else {
		sfh7832_data->hrm_threshold = threshold[0];
		sfh7832_data->prox_threshold = threshold[1];
	}
	HRM_dbg("%s - hrm_threshold = %d, prox_threshold = %d\n",
		__func__, sfh7832_data->hrm_threshold, sfh7832_data->prox_threshold);

	if (of_property_read_u32_array(dNode, "hrmsensor,init_curr",
		sfh7832_data->init_iled, ARRAY_SIZE(sfh7832_data->init_iled)) < 0) {
		HRM_err("%s - get init_curr error\n", __func__);

		sfh7832_data->init_iled[AGC_IR] = 0;
		sfh7832_data->init_iled[AGC_RED] = 0;
		sfh7832_data->init_iled[AGC_GREEN] = 0;
		sfh7832_data->init_iled[AGC_BLUE] = 0;
	}
	HRM_dbg("%s - init_curr = 0x%.2x 0x%.2x 0x%.2x 0x%.2x\n", __func__,
		sfh7832_data->init_iled[AGC_IR], sfh7832_data->init_iled[AGC_RED],
		sfh7832_data->init_iled[AGC_GREEN], sfh7832_data->init_iled[AGC_BLUE]);

	if (of_property_read_u32_array(dNode, "hrmsensor,eol",
		 sfh7832_data->eol_spec, ARRAY_SIZE(sfh7832_data->eol_spec)) < 0) {
		HRM_err("%s - get eol_spec error\n", __func__);
	}
	for (i = 0 ; i < (u32) ARRAY_SIZE(sfh7832_data->eol_spec) ; i += 2)
		HRM_info("%s - eol_spec : min : %d, max %d\n", __func__, sfh7832_data->eol_spec[i], sfh7832_data->eol_spec[i+1]);

	if (of_property_read_u32_array(dNode, "hrmsensor,eol_semi",
		 sfh7832_data->eol_semi_spec, ARRAY_SIZE(sfh7832_data->eol_semi_spec)) < 0) {
		HRM_err("%s - get eol_semi_spec error\n", __func__);
	}
	for (i = 0 ; i < (u32) ARRAY_SIZE(sfh7832_data->eol_semi_spec) ; i += 2)
		HRM_info("%s - eol_semi_spec : min : %d, max %d\n",
			__func__, sfh7832_data->eol_semi_spec[i], sfh7832_data->eol_semi_spec[i+1]);

	if (of_property_read_u32_array(dNode, "hrmsensor,eol_xtalk",
		 sfh7832_data->eol_xtalk_spec, ARRAY_SIZE(sfh7832_data->eol_xtalk_spec)) < 0) {
		HRM_err("%s - get eol_xtalk_spec error\n", __func__);
	}
	HRM_info("%s - eol_xtalk_spec : min : %d, max %d\n", __func__, sfh7832_data->eol_spec[0], sfh7832_data->eol_spec[1]);

	return 0;
}

static int sfh7832_setup_gpio(void)
{
	int errorno = -EIO;

	errorno = gpio_request(sfh7832_data->pin_hrm_int, "hrm_int");
	if (errorno) {
		HRM_err("%s - failed to request hrm_int\n", __func__);
		return errorno;
	}

	errorno = gpio_direction_input(sfh7832_data->pin_hrm_int);
	if (errorno) {
		HRM_err("%s - failed to set hrm_int as input\n", __func__);
		goto err_gpio_direction_input;
	}
	sfh7832_data->hrm_irq = gpio_to_irq(sfh7832_data->pin_hrm_int);

	errorno = gpio_request(sfh7832_data->pin_hrm_en, "hrm_en");
	if (errorno) {
		HRM_err("%s - failed to request hrm_en\n", __func__);
		goto err_gpio_direction_input;
	}
	goto done;

err_gpio_direction_input:
	gpio_free(sfh7832_data->pin_hrm_int);
done:
	return errorno;
}

int sfh7832_otps_ok(void)
{
	int err = 0;
	u32 r41;
	u32 r28;
	u32 r38;
	uint8_t otps_ok;

	/* test whether OTPs have been read out correctly */
	/* TI proposed registers 41h/D16, 28h/D20, 38h/D15, 38h/D5, 38h/D4 which seem to always be written */
	err = sfh7832_read_reg(0x41, &r41);
	if (err != 0)
		return err;
	err = sfh7832_read_reg(0x28, &r28);
	if (err != 0)
		return err;
	err = sfh7832_read_reg(0x38, &r38);
	if (err != 0)
		return err;
	HRM_dbg("R41: 0x%06x / R28: 0x%06x / R38: 0x%06x\n", r41, r28, r38);

	otps_ok = (r41 & (1 << 16)) && (r28 & (1 << 20)) && (r38 & (1 << 15)) && (r38 & (1 << 5)) && (r38 & (1 << 4));

	return otps_ok;
}
/*
* CRC Checksum stuff to provide a way to identify AFEs based on their OTP readings
* stackoverflow.com/questions/10564491/function_to_calculate_a_crc16_checksum
*
*/

u16 crc_16(const u8 *data, u16 length)
{
	u16 i, crc = 0;

	if (data == NULL)
		return 0;

	while (length--) {
		crc ^= *data++;

		for (i = 0; i < 8; i++)
			crc = crc & 1 ? (crc >> 1) ^ 0xa001 : crc >> 1;
	}

	return crc & 0xFFFF;
}

/*
* calculates the crc16 checksum of three OTPs that are written by TI to provide some kind of device identification
*  to prevent mix up
*/
u16 sfh7832_get_checksum(void)
{
	uint32_t reg;
	u8 buffer[9];
	int err = 0;

	err = sfh7832_read_reg(0x27, &reg);
	buffer[0] = (reg >> 16) & 0xFF;
	buffer[1] = (reg >> 8) & 0xFF;
	buffer[2] = reg & 0xFF;
	HRM_dbg("R27: 0x%06x, ", reg);

	err = sfh7832_read_reg(0x41, &reg);
	buffer[3] = (reg >> 16) & 0xFF;
	buffer[4] = (reg >> 8) & 0xFF;
	buffer[5] = reg & 0xFF;
	HRM_dbg("R41: 0x%06x, ", reg);

	err = sfh7832_read_reg(0x38, &reg);
	buffer[6] = (reg >> 16) & 0xFF;
	buffer[7] = (reg >> 8) & 0xFF;
	buffer[8] = reg & 0xFF;
	HRM_dbg("R38: 0x%06x\n", reg);

	return crc_16(buffer, 9);
}

/* read OTPs for trimming and extract the parameters */
void sfh7832_read_trimming_parameters(uint8_t *done, int8_t *trm0,
						int8_t *trm1, int8_t *trm2, int8_t *trm3, int8_t *trm4)
{
	int err = 0;
	uint32_t r41;
	uint32_t r38;

	/* read the two relevant registers */
	err = sfh7832_read_reg(TRM_DONE_REG, &r38);
	if (err != 0)
		return;
	err = sfh7832_read_reg(TRM_LED1_REG, &r41);
	if (err != 0)
		return;

/* the trimming done bit in the 21 OTP version is located on the three additional OTPs -> can be used to determine the trimming version */
	sfh7832_data->trim_version = (uint8_t) (r38 >> 7) & 1;

	if (sfh7832_data->trim_version) {  /* 21 OTPs */
		/* extract information */
		*done = (uint8_t) (r38 >> 7) & 1;											/* trimming done */
		*trm1 = _decode((r41 >> 8) & 0x1F, 5);										/* IR */
		*trm2 = _decode((((r41 >> 2) & 0x1) << 4) | ((r38 >> 19) & 0x0F), 5);		/* green */
		*trm3 = _decode((((r38 >> 16) & 0x07) << 2) | ((r38 >> 13) & 0x03), 5);	/* blue */
		*trm4 = _decode((r41 >> 3) & 0x1F, 5);										/* red */
	} else {	/* 18 OTPs */
		/* extract information */
		*done = (uint8_t) (r38 >> 16) & 1;										/* trimming done */
		*trm0 = _decode((r41 >> 2) & 0x1F, 5);									/* global */
		*trm1 = _decode((r41 >> 10) & 0x07, 3);									/* IR */
		*trm2 = _decode((r38 >> 20) & 0x07, 3);									/* green */
		*trm3 = _decode((r38 >> 17) & 0x07, 3);									/* blue */
		*trm4 = _decode((r41 >> 7) & 0x07, 3);									/* red */
	}
	HRM_dbg("part is ");
	if (!*done) {
		HRM_dbg("untrimmed\n");
	} else if (sfh7832_data->trim_version) {
		HRM_dbg("trimmed with 21 OTPs\n");
	} else {
		HRM_dbg("trimmed with 18 OTPs\n");
	}

	HRM_dbg("trimming parameters: %d, %d, %d, %d, %d\n", *trm0, *trm1, *trm2, *trm3, *trm4);
}

int sfh7832_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = -ENODEV;
	struct sfh7832_device_data *data;

	HRM_dbg("%s - start\n", __func__);

	/* check to make sure that the adapter supports I2C */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		HRM_err("%s - I2C_FUNC_I2C not supported\n", __func__);
		return -ENODEV;
	}

	/* allocate some memory for the device */
	data = kzalloc(sizeof(struct sfh7832_device_data), GFP_KERNEL);
	if (data == NULL) {
		HRM_err("%s - couldn't allocate memory\n", __func__);
		return -ENOMEM;
	}

	data->hrm_data = kzalloc(sizeof(int) * HRM_DATA_CNT * 10, GFP_KERNEL);
	if (data->hrm_data == NULL) {
		HRM_err("%s - couldn't allocate hrm memory\n", __func__);
		goto err_hrm_alloc_fail;
	}

	data->flicker_data = kzalloc(sizeof(int)*FLICKER_DATA_CNT, GFP_KERNEL);
	if (data->flicker_data == NULL) {
		HRM_err("%s - couldn't allocate flicker data memory\n", __func__);
		goto err_flicker_alloc_fail;
	}

	data->recvData = kzalloc(sizeof(u8)*FLICKER_DATA_CNT * NUM_BYTES_PER_SAMPLE, GFP_KERNEL);
	if (data->recvData == NULL) {
		HRM_err("%s - couldn't allocate recvData data memory\n", __func__);
		goto err_recvData_alloc_fail;
	}

	sfh7832_data = data;
	sfh7832_init_var1();

	data->client = client;
	data->miscdev.minor = MISC_DYNAMIC_MINOR;
	data->miscdev.name = "max_hrm";
	data->miscdev.fops = &sfh7832_fops;
	data->miscdev.mode = S_IRUGO;
	i2c_set_clientdata(client, data);
	HRM_info("%s client  = %p\n", __func__, client);

	err = misc_register(&data->miscdev);
	if (err < 0) {
		HRM_err("%s - failed to register Device\n", __func__);
		goto err_misc_register;
	}

	mutex_init(&data->i2clock);
	mutex_init(&data->activelock);
	mutex_init(&data->suspendlock);
	mutex_init(&data->flickerdatalock);

	err = sfh7832_parse_dt();
	if (err < 0) {
		HRM_err("[SENSOR] %s - of_node error\n", __func__);
		err = -ENODEV;
		goto err_parse_dt;
	}

	err = sfh7832_setup_gpio();
	if (err) {
		HRM_err("%s - could not initialize resources\n", __func__);
		goto err_setup_gpio;
	}

	if (data->always_on) {
		err = sfh7832_power_always_ctrl(data, PWR_ON);
		if (err != 0) {
			HRM_err("%s sfh7832_power_always_on fail(%d)\n", __func__, err);
			goto err_power_on;
		}
		err = sfh7832_power_down_ctrl(PWR_ON);
		if (err != 0) {
			HRM_err("%s sfh7832 i2c fail or scan mode enter!!(%d)\n", __func__, err);
			/* WA for POR fail */
			err = sfh7832_power_always_ctrl(data, PWR_OFF);
			if (err != 0) {
				HRM_err("%s sfh7832_power_always_on fail(%d)\n", __func__, err);
				goto err_power_on;
			}
			err = sfh7832_power_always_ctrl(data, PWR_ON);
			if (err != 0) {
				HRM_err("%s sfh7832_power_always_on fail(%d)\n", __func__, err);
				goto err_power_on;
			}
			err = sfh7832_power_down_ctrl(PWR_ON);
			if (err != 0) {
				HRM_err("%s sfh7832 i2c fail (%d)\n", __func__, err);
				goto err_init_fail;
			}
		}
	} else {
		err = sfh7832_power_ctrl(PWR_ON);
		if (err != 0) {
			HRM_err("%s sfh7832_power_ctrl fail(%d, %d)\n", __func__,
				err, PWR_ON);
			goto err_power_on;
		}
	}

	if (data->client->addr != SLAVE_ADDR_SFH) {
		err = -EIO;
		HRM_err("%s - slave address error, 0x%02x\n", __func__, data->client->addr);
		goto err_init_fail;
	}

	err = sfh7832_sw_reset();
	if (err != 0) {
		HRM_dbg("%s failed sfh7832_sw_reset\n", __func__);
		goto err_init_fail;
	}

	err = sfh7832_init_var2();
	if (err < 0) {
		HRM_err("%s - failed to init_var2\n", __func__);
		goto err_init_fail;
	}

	if (!sfh7832_otps_ok()) {
		HRM_dbg("WARNING OTPs could not be initialized - retry..\n");
		err = sfh7832_sw_reset();
		if (err != 0) {
			HRM_dbg("%s failed sfh7832_sw_reset\n", __func__);
			goto err_init_fail;
		}

		/* test again */
		if (!sfh7832_otps_ok())
			HRM_dbg("ERROR OTPs could not be initialized\n");
		/*  return -1; */
	}

	sfh7832_read_trimming_parameters(&data->isTrimmed, &data->trmglobal,
							&data->trmled1, &data->trmled2,
							&data->trmled3, &data->trmled4);

	data->hrm_input_dev = input_allocate_device();
	if (!data->hrm_input_dev) {
		HRM_err("%s - could not allocate input device\n", __func__);
		goto err_input_allocate_device;
	}
	data->hrm_input_dev->name = MODULE_NAME_HRM;
	input_set_drvdata(data->hrm_input_dev, data);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_X);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_Y);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_Z);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_RX);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_MISC);
	input_set_capability(data->hrm_input_dev, EV_ABS, ABS_X);
	input_set_capability(data->hrm_input_dev, EV_ABS, ABS_Y);
	input_set_capability(data->hrm_input_dev, EV_ABS, ABS_Z);
	input_set_capability(data->hrm_input_dev, EV_ABS, ABS_RX);
	input_set_capability(data->hrm_input_dev, EV_ABS, ABS_RY);
	input_set_capability(data->hrm_input_dev, EV_ABS, ABS_RZ);
	input_set_capability(data->hrm_input_dev, EV_ABS, ABS_THROTTLE);
	input_set_capability(data->hrm_input_dev, EV_ABS, ABS_RUDDER);

	err = input_register_device(data->hrm_input_dev);
	if (err < 0) {
		input_free_device(data->hrm_input_dev);
		HRM_err("%s - could not register input device\n", __func__);
		goto err_input_register_device;
	}

#ifdef CONFIG_ARCH_QCOM
	err = sensors_create_symlink(&data->hrm_input_dev->dev.kobj,
				data->hrm_input_dev->name);
#else
	err = sensors_create_symlink(data->hrm_input_dev);
#endif
	if (err < 0) {
		HRM_err("%s - create_symlink error\n", __func__);
		goto err_sensors_create_symlink;
	}
	err = sysfs_create_group(&data->hrm_input_dev->dev.kobj,
				 &hrm_attribute_group);
	if (err) {
		HRM_err("%s - could not create sysfs group\n", __func__);
		goto err_sysfs_create_group;
	}

#ifdef CONFIG_ARCH_QCOM
	err = sensors_register(&data->dev, data, sfh7832_sensor_attrs,
				MODULE_NAME_HRM);
#else
	err = sensors_register(data->dev, data, sfh7832_sensor_attrs,
			       MODULE_NAME_HRM);
#endif
	if (err) {
		HRM_err("%s - cound not register hrm_sensor(%d).\n",
			__func__, err);
		goto hrm_sensor_register_failed;
	}

	err = sfh7832_setup_irq();
	if (err) {
		HRM_err("%s - could not setup hrm_irq\n", __func__);
		goto err_setup_irq;
	}
	if (data->always_on)
		err = sfh7832_power_down_ctrl(PWR_OFF);
	else
		err = sfh7832_power_ctrl(PWR_OFF);

	if (err < 0) {
		HRM_err("%s sfh7832_power_ctrl fail(%d, %d)\n", __func__,
			err, PWR_OFF);
		goto dev_set_drvdata_failed;
	}
	HRM_dbg("%s success\n", __func__);

	goto done;

dev_set_drvdata_failed:
	free_irq(data->hrm_irq, data);
err_setup_irq:
	sensors_unregister(data->dev, sfh7832_sensor_attrs);
hrm_sensor_register_failed:
	sysfs_remove_group(&data->hrm_input_dev->dev.kobj,
			   &hrm_attribute_group);
err_sysfs_create_group:
#ifdef CONFIG_ARCH_QCOM
	sensors_remove_symlink(&data->hrm_input_dev->dev.kobj,
				data->hrm_input_dev->name);
#else
	sensors_remove_symlink(data->hrm_input_dev);
#endif
err_sensors_create_symlink:
	input_unregister_device(data->hrm_input_dev);
err_input_register_device:
err_input_allocate_device:
err_init_fail:
	sfh7832_power_ctrl(PWR_OFF);
err_power_on:
	gpio_free(data->pin_hrm_int);
	gpio_free(data->pin_hrm_en);

err_setup_gpio:
err_parse_dt:
	if (data->hrm_pinctrl) {
		devm_pinctrl_put(data->hrm_pinctrl);
		data->hrm_pinctrl = NULL;
	}
	if (data->pins_idle)
		data->pins_idle = NULL;
	if (data->pins_sleep)
		data->pins_sleep = NULL;

	mutex_destroy(&data->i2clock);
	mutex_destroy(&data->activelock);
	mutex_destroy(&data->suspendlock);
	mutex_destroy(&data->flickerdatalock);
	misc_deregister(&data->miscdev);
err_misc_register:
	kfree(data->recvData);
err_recvData_alloc_fail:
	kfree(data->flicker_data);
err_flicker_alloc_fail:
	kfree(data->hrm_data);
err_hrm_alloc_fail:
	kfree(data);
	HRM_err("%s failed\n", __func__);
done:
	return err;
}

int sfh7832_remove(struct i2c_client *client)
{
	HRM_dbg("%s\n", __func__);

	sfh7832_power_ctrl(PWR_OFF);

	sensors_unregister(sfh7832_data->dev, sfh7832_sensor_attrs);
	sysfs_remove_group(&sfh7832_data->hrm_input_dev->dev.kobj,
			   &hrm_attribute_group);

#ifdef CONFIG_ARCH_QCOM
	sensors_remove_symlink(&sfh7832_data->hrm_input_dev->dev.kobj,
				sfh7832_data->hrm_input_dev->name);
#else

	sensors_remove_symlink(sfh7832_data->hrm_input_dev);
#endif
	input_unregister_device(sfh7832_data->hrm_input_dev);

	if (sfh7832_data->hrm_pinctrl) {
		devm_pinctrl_put(sfh7832_data->hrm_pinctrl);
		sfh7832_data->hrm_pinctrl = NULL;
	}
	if (sfh7832_data->pins_idle)
		sfh7832_data->pins_idle = NULL;
	if (sfh7832_data->pins_sleep)
		sfh7832_data->pins_sleep = NULL;
	disable_irq(sfh7832_data->hrm_irq);
	free_irq(sfh7832_data->hrm_irq, sfh7832_data);
	gpio_free(sfh7832_data->pin_hrm_int);
	gpio_free(sfh7832_data->pin_hrm_en);
	mutex_destroy(&sfh7832_data->i2clock);
	mutex_destroy(&sfh7832_data->activelock);
	mutex_destroy(&sfh7832_data->suspendlock);
	mutex_destroy(&sfh7832_data->flickerdatalock);
	misc_deregister(&sfh7832_data->miscdev);

	kfree(sfh7832_data->recvData);
	kfree(sfh7832_data->flicker_data);
	kfree(sfh7832_data->lib_ver);
	if (sfh7832_data->eol_test_result != NULL)
		kfree(sfh7832_data->eol_test_result);
	kfree(sfh7832_data);
	i2c_set_clientdata(client, NULL);

	sfh7832_data = NULL;
	return 0;
}

static void sfh7832_shutdown(struct i2c_client *client)
{
	HRM_dbg("%s\n", __func__);
}

void sfh7832_pin_control(bool pin_set)
{
	int status = 0;

	if (!sfh7832_data->hrm_pinctrl) {
		HRM_err("%s hrm_pinctrl is null\n", __func__);
		return;
	}
	if (pin_set) {
		if (!IS_ERR_OR_NULL(sfh7832_data->pins_idle)) {
			status = pinctrl_select_state(sfh7832_data->hrm_pinctrl,
						      sfh7832_data->pins_idle);
			if (status)
				HRM_err("%s: can't set pin default state\n",
					__func__);
			HRM_info("%s idle\n", __func__);
		}
	} else {
		if (!IS_ERR_OR_NULL(sfh7832_data->pins_sleep)) {
			status = pinctrl_select_state(sfh7832_data->hrm_pinctrl,
						      sfh7832_data->pins_sleep);
			if (status)
				HRM_dbg("%s: can't set pin sleep state\n",
					__func__);
			HRM_info("%s sleep\n", __func__);
		}
	}
}

#ifdef CONFIG_PM
static int sfh7832_pm_suspend(struct device *dev)
{
	int err = 0;

	HRM_dbg("%s - %d\n", __func__, sfh7832_data->enabled_mode);

	if (sfh7832_data->mode_svc_enabled)
		return err;

	if (sfh7832_data->enabled_mode != 0 || sfh7832_data->regulator_state != 0) {
		mutex_lock(&sfh7832_data->activelock);
		sfh7832_irq_set_state(PWR_OFF);
		sfh7832_pin_control(PWR_OFF);

		if (sfh7832_data->always_on)
			err = sfh7832_power_down_ctrl(PWR_OFF);
		else
			err = sfh7832_power_ctrl(PWR_OFF);

		if (err < 0)
			HRM_err("%s - hrm_regulator_off fail err = %d\n",
				__func__, err);
		mutex_unlock(&sfh7832_data->activelock);
	}
	mutex_lock(&sfh7832_data->suspendlock);
	sfh7832_data->pm_state = PM_SUSPEND;
	mutex_unlock(&sfh7832_data->suspendlock);

	return err;
}

static int sfh7832_pm_resume(struct device *dev)
{
	int err = 0;

	HRM_dbg("%s - %d\n", __func__, sfh7832_data->enabled_mode);

	if (sfh7832_data->mode_svc_enabled)
		return err;

	mutex_lock(&sfh7832_data->suspendlock);
	sfh7832_data->pm_state = PM_RESUME;
	mutex_unlock(&sfh7832_data->suspendlock);

	if (sfh7832_data->enabled_mode != 0) {
		mutex_lock(&sfh7832_data->activelock);

		if (sfh7832_data->always_on)
			err = sfh7832_power_down_ctrl(PWR_ON);
		else
			err = sfh7832_power_ctrl(PWR_ON);

		if (err < 0)
			HRM_err("%s - hrm_regulator_on fail err = %d\n",
				__func__, err);

		sfh7832_pin_control(PWR_ON);
		sfh7832_irq_set_state(PWR_ON);

		err = sfh7832_enable(sfh7832_data->enabled_mode);
		if (err != 0)
			HRM_err("%s - enable err : %d\n", __func__, err);

		if (err < 0 && sfh7832_data->enabled_mode == MODE_AMBIENT) {
			input_report_rel(sfh7832_data->hrm_input_dev,
				REL_Y, -5 + 1); /* F_ERR_I2C -5 detected i2c error */
			input_sync(sfh7832_data->hrm_input_dev);
			HRM_err("%s - awb mode enable error : %d\n", __func__, err);
		}

		mutex_unlock(&sfh7832_data->activelock);
	}
	return err;

}


static const struct dev_pm_ops sfh7832_pm_ops = {
	.suspend = sfh7832_pm_suspend,
	.resume = sfh7832_pm_resume
};
#endif

static const struct of_device_id sfh7832_match_table[] = {
	{.compatible = "hrm_os",},
	{},
};

static const struct i2c_device_id sfh7832_device_id[] = {
	{"sfh7832", 0},
	{}
};

/* descriptor of the hrmsensor I2C driver */
static struct i2c_driver sfh7832_i2c_driver = {
	.driver = {
		   .name = "sfh7832",
		   .owner = THIS_MODULE,
#if defined(CONFIG_PM)
		   .pm = &sfh7832_pm_ops,
#endif
		   .of_match_table = sfh7832_match_table,
		   },
	.probe = sfh7832_probe,
	.remove = sfh7832_remove,
	.shutdown = sfh7832_shutdown,
	.id_table = sfh7832_device_id,
};

/* initialization and exit functions */
static int __init sfh7832_init(void)
{
	if (!lpcharge)
		return i2c_add_driver(&sfh7832_i2c_driver);
	else
		return 0;
}

static void __exit sfh7832_exit(void)
{
	i2c_del_driver(&sfh7832_i2c_driver);
}

module_init(sfh7832_init);
module_exit(sfh7832_exit);

MODULE_DESCRIPTION("sfh7832 Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
