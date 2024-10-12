/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for oncell ic core functions
 *
 *  Copyright (C) 2019 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "himax_ic_core.h"

int HX_TOUCH_INFO_POINT_CNT;

struct himax_on_core_command_operation *g_on_core_cmd_op;
struct on_ic_operation *on_pic_op;
struct on_fw_operation *on_pfw_op;
struct on_flash_operation *on_pflash_op;
struct on_sram_operation *on_psram_op;
struct on_driver_operation *on_pdriver_op;
EXPORT_SYMBOL(on_pdriver_op);

void (*himax_mcu_cmd_struct_free)(void);
uint32_t dbg_reg_ary[4] = {fw_addr_fw_dbg_msg_addr, fw_addr_chk_fw_status,
	fw_addr_chk_dd_status, fw_addr_flag_reset_event};

/* CORE_IC */
/* IC side start*/
static void himax_mcu_burst_enable(uint8_t auto_add_4_byte)
{
	uint8_t tmp_data[DATA_LEN_4];
	int ret = 0;
	/*I("%s: Entering\n", __func__);*/
	tmp_data[0] = on_pic_op->data_conti[0];

	ret = himax_bus_write(on_pic_op->addr_conti[0],
			tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	tmp_data[0] = (on_pic_op->data_incr4[0] | auto_add_4_byte);

	ret = himax_bus_write(on_pic_op->addr_incr4[0],
			tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}
}

static int himax_mcu_register_read(uint8_t *read_addr, uint32_t read_length,
		uint8_t *read_data, uint8_t cfg_flag)
{
	struct on_ic_operation *on_pic_op = g_on_core_cmd_op->ic_op;
	uint8_t tmp_data[DATA_LEN_4];
	int i = 0;
	int address = 0;
	int ret = 0;

	/*I("%s: Entering\n", __func__);*/

	if (cfg_flag == 0 || cfg_flag == 2) {
		if (read_length > FLASH_RW_MAX_LEN) {
			E("%s: read len over %d!\n", __func__,
				FLASH_RW_MAX_LEN);
			return LENGTH_FAIL;
		}

		/*disable idle mode to read register*/
		if (cfg_flag == 2)
			g_core_fp.fp_idle_mode(1);

		if (read_length > DATA_LEN_4)
			g_core_fp.fp_burst_enable(1);
		else
			g_core_fp.fp_burst_enable(0);

		address = (read_addr[3] << 24)
				+ (read_addr[2] << 16)
				+ (read_addr[1] << 8)
				+ read_addr[0];

		i = address;
		tmp_data[0] = (uint8_t)i;
		tmp_data[1] = (uint8_t)(i >> 8);
		tmp_data[2] = (uint8_t)(i >> 16);
		tmp_data[3] = (uint8_t)(i >> 24);

		ret = himax_bus_write(on_pic_op->addr_ahb_addr_byte_0[0],
			tmp_data, sizeof(tmp_data), HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			goto i2c_rw_fail;
		}

		tmp_data[0] = on_pic_op->data_ahb_access_direction_read[0];

		ret = himax_bus_write(on_pic_op->addr_ahb_access_direction[0],
				tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			goto i2c_rw_fail;
		}

		ret = himax_bus_read(on_pic_op->addr_ahb_rdata_byte_0[0],
				read_data, read_length, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			goto i2c_rw_fail;
		}

		/*if (read_length > DATA_LEN_4)*/
			/*g_core_fp.fp_burst_enable(0);*/

/*i2c_rw_fail:*/
		/*if (cfg_flag == 2)*/
			/*g_core_fp.fp_idle_mode(0);*/
			/*enble idle mode after read register*/

	} else if (cfg_flag == 1) {
		ret = himax_bus_read(read_addr[0], read_data,
				read_length, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return I2C_FAIL;
		}
	} else {
		E("%s: cfg_flag = %d, value is wrong!\n", __func__, cfg_flag);
		return ERR_WORK_OUT;
	}
	return NO_ERR;

i2c_rw_fail:
	/*enble idle mode after read register*/
	if (cfg_flag == 2)
		g_core_fp.fp_idle_mode(0);

	return I2C_FAIL;
}

/*
 *static int himax_mcu_flash_write_burst(uint8_t *reg_byte,
 *		uint8_t *write_data)
 *{
 *	uint8_t data_byte[FLASH_WRITE_BURST_SZ];
 *	int i = 0, j = 0;
 *	int data_byte_sz = sizeof(data_byte);
 *
 *	for (i = 0; i < ADDR_LEN_4; i++) {
 *		data_byte[i] = reg_byte[i];
 *	}
 *
 *	for (j = ADDR_LEN_4; j < data_byte_sz; j++) {
 *		data_byte[j] = write_data[j - ADDR_LEN_4];
 *	}
 *
 *	if (himax_bus_write(on_pic_op->addr_ahb_addr_byte_0[0], data_byte,
 *	data_byte_sz, HIMAX_I2C_RETRY_TIMES) < 0) {
 *		E("%s: i2c access fail!\n", __func__);
 *		return I2C_FAIL;
 *	}
 *	return NO_ERR;
 *}
 */

static void himax_mcu_flash_write_burst_lenth(uint8_t *reg_byte,
		uint8_t *write_data, uint32_t length)
{
	uint8_t *data_byte;
	int i = 0, j = 0;
	int ret = 0;

	/*if (length + ADDR_LEN_4 > FLASH_RW_MAX_LEN) {*/
		/*E("%s: write len over %d!\n", __func__, FLASH_RW_MAX_LEN);*/
		/*return;*/
	/*}*/

	data_byte = kzalloc(sizeof(uint8_t)*(length + 4), GFP_KERNEL);

	if (data_byte != NULL) {

		for (i = 0; i < ADDR_LEN_4; i++)
			data_byte[i] = reg_byte[i];

		for (j = ADDR_LEN_4; j < length + ADDR_LEN_4; j++)
			data_byte[j] = write_data[j - ADDR_LEN_4];

		ret = himax_bus_write(on_pic_op->addr_ahb_addr_byte_0[0],
			data_byte,
			length + ADDR_LEN_4,
			HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			/*kfree(data_byte);*/
			/*return;*/
		}

		kfree(data_byte);
	}
}

static int himax_mcu_register_write(uint8_t *write_addr, uint32_t write_length,
		uint8_t *write_data, uint8_t cfg_flag)
{
	int ret = 0;
	/*I("%s: Entering\n", __func__);*/

	if (cfg_flag == 0 || cfg_flag == 2) {

		if (cfg_flag == 2)
			g_core_fp.fp_idle_mode(1);

		if (write_length > DATA_LEN_4)
			g_core_fp.fp_burst_enable(1);
		else
			g_core_fp.fp_burst_enable(0);

		himax_mcu_flash_write_burst_lenth(write_addr, write_data,
				write_length);

		/*if (write_length > DATA_LEN_4)*/
			/*g_core_fp.fp_burst_enable(0);*/

		if (cfg_flag == 2)
			g_core_fp.fp_idle_mode(0);

	} else if (cfg_flag == 1) {
		ret = himax_bus_write(write_addr[0], write_data,
				write_length, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return I2C_FAIL;
		}
	} else {
		E("%s: cfg_flag = %d, value is wrong!\n", __func__, cfg_flag);
		return ERR_WORK_OUT;
	}

	return NO_ERR;
}

static int himax_write_read_reg(uint8_t *tmp_addr, uint8_t *tmp_data,
		uint8_t hb, uint8_t lb)
{
	int cnt = 0;
	uint8_t r_data[ADDR_LEN_4] = {0};

	do {
		if (r_data[1] != lb || r_data[0] != hb)
			g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4,
			tmp_data, 0);
		usleep_range(10000, 11000);
		g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, r_data, 0);
		/* I("%s:Now r_data[0]=0x%02X,[1]=0x%02X,
		 *	[2]=0x%02X,[3]=0x%02X\n",
		 *	__func__, r_data[0],
		 *	r_data[1], r_data[2], r_data[3]);
		 */
	} while ((r_data[1] != hb || r_data[0] != lb) && cnt++ < 100);

	if (cnt >= 100) {
		g_core_fp.fp_read_FW_status();
		return HX_RW_REG_FAIL;
	}

	return NO_ERR;
}

static void himax_mcu_interface_on(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t tmp_data2[DATA_LEN_4];
	int cnt = 0;
	int ret = 0;

	/*Read a dummy register to wake up I2C.*/
	ret = himax_bus_read(on_pic_op->addr_ahb_rdata_byte_0[0], tmp_data,
			sizeof(tmp_data), HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	do {
		tmp_data[0] = on_pic_op->data_conti[0];

		ret = himax_bus_write(on_pic_op->addr_conti[0],
				tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}

		tmp_data[0] = on_pic_op->data_incr4[0];

		ret = himax_bus_write(on_pic_op->addr_incr4[0],
				tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}

		/*Check cmd*/
		himax_bus_read(on_pic_op->addr_conti[0], tmp_data, 1,
				HIMAX_I2C_RETRY_TIMES);
		himax_bus_read(on_pic_op->addr_incr4[0], tmp_data2, 1,
				HIMAX_I2C_RETRY_TIMES);

		if (tmp_data[0] == on_pic_op->data_conti[0]
		&& tmp_data2[0] == on_pic_op->data_incr4[0])
			break;

		/*msleep(1);*/
		usleep_range(1000, 1100);
	} while (++cnt < 10);

	if (cnt > 0)
		I("%s: Polling burst mode %d times\n", __func__, cnt);
}

static bool himax_mcu_wait_wip(int Timing)
{
	uint8_t tmp_data[DATA_LEN_4] = {0};
	int retry_cnt = 0;

	do {
		g_core_fp.fp_register_read(on_pflash_op->addr_ctrl_auto,
				sizeof(tmp_data), tmp_data, 0);

		if (retry_cnt++ > 100) {
			E("%s: Wait wip error!\n", __func__);
			E("%s:tmp_data[0] = 0x%02X, tmp_data[1] = 0x%02X\n",
				__func__, tmp_data[0], tmp_data[1]);
			return false;
		}

		if (tmp_data[1] == on_pflash_op->data_auto[0]
		&& (tmp_data[0] == on_pflash_op->data_main_read[0]
		|| tmp_data[0] == on_pflash_op->data_sfr_read[0]
		|| tmp_data[0] == on_pflash_op->data_spp_read[0])) {
			E("%s: Operation function is read.\n", __func__);
			return false;
		}

		/*msleep(timing);*/

		/*I("%s: Wait wip cnt = %d\n", __func__, retry_cnt);*/
	} while (tmp_data[1] != 0x00 || tmp_data[0] != 0x00);

	return true;
}

static void himax_mcu_sense_on(uint8_t FlashMode)
{
	uint8_t tmp_data[DATA_LEN_4];
	int ret = 0;

	private_ts->notouch_frame = private_ts->ic_notouch_frame;
	tmp_data[0] = on_pic_op->cmd_mcu_on[0];

	ret = himax_bus_write(on_pic_op->adr_mcu_ctrl[0],
			tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0)
		E("%s: i2c access fail!\n", __func__);
}

static void himax_mcu_tcon_on(void)
{
	g_core_fp.fp_register_write(on_pic_op->adr_tcon_ctrl,
		sizeof(on_pic_op->adr_tcon_ctrl),
		on_pic_op->cmd_tcon_on, 0);
}

static bool himax_mcu_watch_dog_off(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t retry = 0;
	bool ret = false;

	g_core_fp.fp_burst_enable(0);

	do {
		g_core_fp.fp_register_write(on_pic_op->adr_wdg_ctrl,
				sizeof(on_pic_op->adr_wdg_ctrl),
				on_pic_op->cmd_wdg_psw, 0);

		g_core_fp.fp_register_read(on_pic_op->adr_wdg_ctrl,
				sizeof(tmp_data), tmp_data, 0);

		I("%s: Read status from IC = %X,%X\n", __func__,
				tmp_data[0], tmp_data[1]);

	} while ((tmp_data[1] != on_pic_op->cmd_wdg_psw[1]
		|| tmp_data[0] != on_pic_op->cmd_wdg_psw[0])
		&& retry++ < 5);

	if (retry >= 5) {
		I("%s: Turn off watch dog failed\n", __func__);
		ret = false;
	} else {
		I("%s: Turn off watch dog ok\n", __func__);

		g_core_fp.fp_register_write(on_pic_op->adr_wdg_cnt_ctrl,
				sizeof(on_pic_op->adr_wdg_cnt_ctrl),
				on_pic_op->cmd_wdg_cnt_clr, 0);

		ret = true;
	}
	return ret;
}

static bool himax_mcu_sense_off(bool check_en)
{
	bool ret = false;
	int reval = 0;

	g_core_fp.fp_idle_mode(1); /*disable idle mode*/

	if (g_core_fp.fp_watch_dog_off()) {
		reval = himax_bus_write(on_pic_op->adr_mcu_ctrl[0],
				on_pic_op->cmd_mcu_off,
				1,
				HIMAX_I2C_RETRY_TIMES);

		if (reval < 0) {
			E("%s: i2c access fail!\n", __func__);
			ret = false;
		} else {
			g_core_fp.fp_tcon_on();
			ret = true;
		}
	} else {
		E("%s: Sense off fail!\n", __func__);
		ret = false;
	}

	if (ret == false)
		g_core_fp.fp_idle_mode(0); /*enable idle mode*/

	return ret;
}

static void himax_mcu_sleep_in(void)
{
	int ret = 0;
	/*tmp_data[0] = 0x80;*/
	ret = himax_bus_write(on_pic_op->adr_sleep_ctrl[0],
			on_pic_op->cmd_sleep_in,
			1,
			HIMAX_I2C_RETRY_TIMES);
	if (ret < 0)
		E("%s: i2c access fail!\n", __func__);
}

static void himax_mcu_resume_ic_action(void)
{
	g_core_fp.fp_sense_on(1);
}

static void himax_mcu_suspend_ic_action(void)
{
	g_core_fp.fp_sleep_in();
}

static void himax_mcu_power_on_init(void)
{
#if 0
	I("%s\n", __func__);
	g_core_fp.fp_touch_information();
	g_core_fp.fp_calc_touch_data_size();
	/*g_core_fp.fp_sense_on(0x00);*/
#endif
	I("%s, oncell Entering!\n", __func__);
	g_core_fp.fp_sense_on(0x00);
	I("%s, oncell End!\n", __func__);

}

/* IC side end*/
/* CORE_IC */

/* CORE_FW */
/* FW side start*/
static void diag_mcu_parse_raw_data(struct himax_report_data *hx_touch_data,
		int mul_num, int self_num, uint8_t diag_cmd,
		int32_t *mutual_data, int32_t *self_data)
{
	int RawDataLen_word;
	int index = 0;
	int temp1;
	int temp2;
	int i;

	if (hx_touch_data->hx_rawdata_buf[0]
	== on_pfw_op->data_rawdata_ready_lb[0]
	&& hx_touch_data->hx_rawdata_buf[1]
	== on_pfw_op->data_rawdata_ready_hb[0]
	&& hx_touch_data->hx_rawdata_buf[2] > 0
	&& hx_touch_data->hx_rawdata_buf[3] == diag_cmd) {
		RawDataLen_word = hx_touch_data->rawdata_size / 2;
		index = (hx_touch_data->hx_rawdata_buf[2] - 1)
			* RawDataLen_word;

		/*I("Header[%d]: %x, %x, %x, %x, mutual: %d, self: %d\n", index,
		 *	buf[56], buf[57], buf[58], buf[59], mul_num,self_num);
		 */
		/*I("RawDataLen=%d, RawDataLen_word=%d,hx_touch_info_size=%d\n",
		 *	RawDataLen, RawDataLen_word, hx_touch_info_size);
		 */
		for (i = 0; i < RawDataLen_word; i++) {
			temp1 = index + i;

			if (temp1 < mul_num) {
				/*4: RawData Header, 1:HSB*/
				mutual_data[index + i] =
					hx_touch_data->hx_rawdata_buf[i * 2
					+ 4 + 1] * 256
					+ hx_touch_data->hx_rawdata_buf[i * 2
					+ 4];
			} else {
				temp1 = i + index;
				temp2 = self_num + mul_num;

				if (temp1 >= temp2)
					break;

				/*4: RawData Header*/
				self_data[i + index - mul_num] =
					(hx_touch_data->hx_rawdata_buf[i * 2
					+ 4 + 1] << 8)
					+ hx_touch_data->hx_rawdata_buf[i * 2
					+ 4];
			}
		}
	}
}

static int himax_mcu_Calculate_CRC_with_AP(unsigned char *FW_content,
		int CRC_from_FW, int mode)
{
	return true;
}

static uint32_t himax_mcu_check_CRC(uint8_t *start_addr, int reload_length)
{
	uint32_t result = 0;
	uint8_t tmp_data[DATA_LEN_4];
	int cnt = 0;
	int ret = 0;

	ret = g_core_fp.fp_register_write(on_pfw_op->addr_flash_checksum,
			sizeof(on_pfw_op->addr_flash_checksum),
			on_pfw_op->data_flash_checksum, 0);
	if (ret < NO_ERR) {
		E("%s: i2c access fail!\n", __func__);
		return HW_CRC_FAIL;
	}
	/*msleep(15);*/
	usleep_range(15000, 15100);

	do {
		ret = g_core_fp.fp_register_read(on_pfw_op->addr_flash_checksum,
				DATA_LEN_4, tmp_data, 0);
		if (ret < NO_ERR) {
			E("%s: i2c access fail!\n", __func__);
			return HW_CRC_FAIL;
		}
	} while (tmp_data[0] != 0x90 && cnt++ < 50);

	ret = g_core_fp.fp_register_read(on_pfw_op->addr_crc_value,
			DATA_LEN_4, tmp_data, 0); /*50*/
	if (ret < NO_ERR) {
		E("%s: i2c access fail!\n", __func__);
		return HW_CRC_FAIL;
	}

	I("%s: tmp_data[3]=%X,tmp_data[2]=%X,tmp_data[1]=%X,tmp_data[0]=%X\n",
			__func__, tmp_data[3],
			tmp_data[2], tmp_data[1], tmp_data[0]);

	result = ((tmp_data[3] << 24)
			+ (tmp_data[2] << 16)
			+ (tmp_data[1] << 8)
			+ tmp_data[0]);

	return result;
}

static void himax_mcu_set_SMWP_enable(uint8_t SMWP_enable, bool suspended)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t back_data[DATA_LEN_4];
	uint8_t retry_cnt = 0;

	do {
		if (SMWP_enable == 0x01 && suspended) {
			tmp_data[0] = SMWP_enable;
			himax_bus_write(on_pfw_op->addr_smwp_enable[0],
					tmp_data, 1, HIMAX_I2C_RETRY_TIMES);

			back_data[0] = SMWP_enable;
			I("%s: SMWP enable\n", __func__);
		} else {
			tmp_data[0] = 0x00;
			himax_bus_write(on_pfw_op->addr_smwp_enable[0],
					tmp_data, 1, HIMAX_I2C_RETRY_TIMES);

			back_data[0] = 0x00;
			I("%s: SMWP disable\n", __func__);
		}
		himax_bus_read(on_pfw_op->addr_smwp_enable[0],
				tmp_data, 1, HIMAX_I2C_RETRY_TIMES);

		retry_cnt++;
	} while ((tmp_data[0] != back_data[0])
		&& retry_cnt < HIMAX_REG_RETRY_TIMES);
}

static void himax_mcu_set_HSEN_enable(uint8_t HSEN_enable, bool suspended)
{
	/*not support now*/
}

static void himax_mcu_usb_detect_set(uint8_t *cable_config)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t back_data[DATA_LEN_4];
	uint8_t retry_cnt = 0;

	do {
		if (cable_config[1] == 0x01) {
			tmp_data[0] = cable_config[1];
			himax_bus_write(on_pfw_op->addr_usb_detect[0],
					tmp_data, 1, HIMAX_I2C_RETRY_TIMES);

			back_data[0] = cable_config[1];
			I("%s: USB detect status IN!\n", __func__);
		} else {
			tmp_data[0] = 0x00;
			himax_bus_write(on_pfw_op->addr_usb_detect[0],
					tmp_data, 1, HIMAX_I2C_RETRY_TIMES);

			back_data[0] = 0x00;
			I("%s: USB detect status OUT!\n", __func__);
		}
		himax_bus_read(on_pfw_op->addr_usb_detect[0],
				tmp_data, 1, HIMAX_I2C_RETRY_TIMES);

		/*I("%s: tmp_data[0]=%d, USB detect=%d, retry_cnt=%d\n",
		 *	__func__, tmp_data[0], cable_config[1], retry_cnt);
		 */
		retry_cnt++;
	} while ((tmp_data[0] != back_data[0])
		&& retry_cnt < HIMAX_REG_RETRY_TIMES);
}

static void himax_mcu_diag_register_set(uint8_t diag_command,
		uint8_t storage_type, bool is_dirly)
{
	uint8_t tmp_data[DATA_LEN_4];
	int ret = 0;

	I("%s: diag_cmd = %d, storage_type = %d\n",
			__func__, diag_command, storage_type);
	tmp_data[0] = diag_command;

	ret = himax_bus_write(on_pfw_op->addr_raw_out_sel[0],
			tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}
}

#if defined(HX_TP_SELF_TEST_DRIVER)
static void himax_mcu_control_reK(bool enable)
{
	uint8_t tmp_data[DATA_LEN_4];

	g_core_fp.fp_register_read(on_pfw_op->addr_reK_crtl,
			DATA_LEN_4, tmp_data, 0);

	if (enable)
		tmp_data[0] |= on_pfw_op->data_reK_en[0];
	else
		tmp_data[0] &= on_pfw_op->data_reK_dis[0];

	g_core_fp.fp_register_write(on_pfw_op->addr_reK_crtl,
			DATA_LEN_4, tmp_data, 0);
}

static void himax_8bit_to_16bit(u8 *info_data, int len_8, u16 *out_data)
{
	int i = 0;

	for (i = 0; i < len_8; i = i + 2) {
		out_data[i / 2] = (info_data[i + 1] << 8) + info_data[i];
		/*I("data16[%d] = %d\n", i / 2, out_data[i / 2]);*/
	}
}

static int himax_dc_compare(u8 *thx_data, u16 *info_data16)
{
	uint8_t x_num = ic_data->HX_RX_NUM;
	uint8_t y_num = ic_data->HX_TX_NUM;
	uint16_t mul_dc_delta, mul_dc_upper, mul_dc_lower;
	uint16_t slf_dc_delta, slf_dc_upper, slf_dc_lower;
	int i = 0;
	uint16_t mul_fail_cnt = 0;
	uint8_t slf_fail_cnt = 0;
	uint8_t result = on_pfw_op->data_rst_init[0];/*0xF0*/
	uint16_t mul_dc_ava = (uint16_t)thx_data[0]
			| ((uint16_t)thx_data[1] << 8);
	uint16_t slf_dc_ava = (uint16_t)thx_data[3]
			| ((uint16_t)thx_data[4] << 8);
	uint8_t mul_dc_bound = thx_data[2];
	uint8_t slf_dc_bound = thx_data[5];

	if (mul_dc_bound > 100)
		mul_dc_bound = 100;
	if (slf_dc_bound > 100)
		slf_dc_bound = 100;

	mul_dc_delta = ((uint32_t)mul_dc_ava * mul_dc_bound + 50) / 100;
	slf_dc_delta = ((uint32_t)slf_dc_ava * slf_dc_bound + 50) / 100;

	/*absolute upper/lower*/
	mul_dc_upper = mul_dc_ava + mul_dc_delta;
	mul_dc_lower = mul_dc_ava - mul_dc_delta;
	slf_dc_upper = slf_dc_ava + slf_dc_delta;
	slf_dc_lower = slf_dc_ava - slf_dc_delta;

	/*mutual*/
	for (i = 0; i < x_num * y_num; i++) {
		if (info_data16[i] > mul_dc_upper
		|| info_data16[i] < mul_dc_lower) {
			++mul_fail_cnt;
			result |= MASK_BIT_0;
		}
	}
	/*self*/
	for (i = x_num * y_num; i < x_num * y_num + x_num + y_num; i++) {
		if (info_data16[i] > slf_dc_upper
		|| info_data16[i] < slf_dc_lower) {
			++slf_fail_cnt;
			result |= MASK_BIT_1;
		}
	}

	I("%s: mul_fail_cnt = %d, slf_dc_fail_cnt = %d!\n",
	 __func__, mul_fail_cnt, slf_fail_cnt);
	return result;
}

static int himax_bank_compare(u8 *thx_data, u16 *info_data16)
{
	uint8_t x_num = ic_data->HX_RX_NUM;
	uint8_t y_num = ic_data->HX_TX_NUM;
	int i = 0;
	uint8_t slf_fail_cnt = 0;
	uint8_t result = on_pfw_op->data_rst_init[0];/*0xF0*/
	uint8_t slf_up_bound = thx_data[6];
	uint8_t slf_low_bound = thx_data[7];

	if (slf_up_bound > 63)
		slf_up_bound = 63;
	if (slf_low_bound > 63)
		slf_low_bound = 0;

	for (i = x_num * y_num; i < x_num * y_num + x_num + y_num; i++) {
		if (info_data16[i] > slf_up_bound
		|| info_data16[i] < slf_low_bound) {
			++slf_fail_cnt;
			result |= MASK_BIT_2;
		}
	}

	I("%s: slf_bank_fail_cnt = %d!\n", __func__, slf_fail_cnt);
	return result;
}
#endif

static int himax_mcu_chip_self_test(struct seq_file *s, void *v)
{
	uint8_t data[DATA_LEN_8] = {0};
	int i = 0;
	int flag = 0;
	uint8_t result = 0x00;
#if defined(HX_TP_SELF_TEST_DRIVER)
	uint8_t x_num = ic_data->HX_RX_NUM;
	uint8_t y_num = ic_data->HX_TX_NUM;
	int mutual_size = x_num * y_num * 2;
	int self_size = (x_num + y_num) * 2;
	int total_size = mutual_size + self_size;
	uint8_t *info_data = NULL;
	uint16_t *info_data16 = NULL;
#else
	uint8_t cnt = 0;
#endif
	/*data = {avg_mul_DC_LSB, avg_mul_DC_MSB, mul_DC_up_low_bound
	 *		avg_slf_DC_LSB, avg_slf_DC_MSB, slf_DC_up_low_bound
	 *		slf_bank_up, self_bank_low}
	 */
	/*read test criteria*/
	himax_bus_read(on_pfw_op->addr_criteria_addr[0], data,
			DATA_LEN_8, HIMAX_I2C_RETRY_TIMES);

	for (i = 0; i < DATA_LEN_8; i++) {
		if (data[i] == 0x00)
			flag++;
	}

	if (flag == DATA_LEN_8) {
		data[0] = on_pfw_op->data_thx_avg_mul_dc_lsb[0];
		data[1] = on_pfw_op->data_thx_avg_mul_dc_msb[0];
		/*2850 = 0xB22 , 100%*/
		data[2] = on_pfw_op->data_thx_mul_dc_up_low_bud[0];
		data[3] = on_pfw_op->data_thx_avg_slf_dc_lsb[0];
		data[4] = on_pfw_op->data_thx_avg_slf_dc_msb[0];
		/*1300 = 0x514 , 100%*/
		data[5] = on_pfw_op->data_thx_slf_dc_up_low_bud[0];
		data[6] = on_pfw_op->data_thx_slf_bank_up[0];
		data[7] = on_pfw_op->data_thx_slf_bank_low[0];
		I("%s: Use default THX setting!\n", __func__);
	}

#if defined(HX_TP_SELF_TEST_DRIVER)
	/*info_data = kzalloc(sizeof(uint8_t) * total_size, GFP_KERNEL);*/
	/*info_data16 = kzalloc(sizeof(uint16_t) * total_size / 2,
	 *	GFP_KERNEL);
	 */
	info_data = kzalloc(total_size, GFP_KERNEL);
	info_data16 = kzalloc(total_size, GFP_KERNEL);

	if (info_data != NULL && info_data16 != NULL) {
		/*Get DC from SRAM and compare*/
		g_core_fp.fp_control_reK(0); /*Disable reK*/
		g_core_fp.fp_diag_register_set(on_pfw_op->data_dc_set[0],
			0x01, false);
		g_core_fp.fp_get_DSRAM_data(info_data, 0);
		himax_8bit_to_16bit(info_data, total_size, info_data16);
		result |= himax_dc_compare(data, info_data16);
		/*Get Bank from SRAM and compare*/
		g_core_fp.fp_diag_register_set(on_pfw_op->data_bank_set[0],
			0x01, false);
		g_core_fp.fp_get_DSRAM_data(info_data, 0);
		himax_8bit_to_16bit(info_data, total_size, info_data16);
		result |= himax_bank_compare(data, info_data16);
		I("%s: result = %02X!\n", __func__, result);
		g_core_fp.fp_diag_register_set(0, 0, false);

		/*output result*/
		g_core_fp.fp_control_reK(1);/*Enable reK*/

		kfree(info_data);
		kfree(info_data16);
	} else {
		E("%s: allocate memory fail\n", __func__);
		if (info_data != NULL)
			kfree(info_data);

		if (info_data16 != NULL)
			kfree(info_data16);
	}

	if (result == on_pfw_op->data_rst_init[0])/*0xF0*/
		result = on_pfw_op->data_selftest_pass[0];
#else
	data[0] = on_pfw_op->data_selftest_request[0];
	himax_bus_write(on_pfw_op->addr_selftest_addr_en[0], data, 1,
			HIMAX_I2C_RETRY_TIMES);

	do {
		himax_bus_read(on_pfw_op->addr_selftest_result_addr[0], data,
				DATA_LEN_8, HIMAX_I2C_RETRY_TIMES);
		msleep(20);
		cnt++;
	} while (data[0] == 0x00 && cnt < 50);

	result = data[0];

	if (result != on_pfw_op->data_selftest_pass[0]) {/*Debug log*/
		I("%s: result = 0x%2.2X, cnt = %d\n", __func__, result, cnt);
		for (i = 0; i < DATA_LEN_8; i++)
			I("%s: [%d] = %2.2X\n", __func__, i, data[i]);
	}

	/*return g_core_fp.fp_chip_self_test();*/
#endif

	if (result == on_pfw_op->data_selftest_pass[0]) {
		I("%s: self-test pass\n", __func__);
		seq_puts(s, "Self_Test Pass:\n");
		/*return 0x00;*/
		i = 0;
	} else {
		I("%s: self-test fail, result = %2X\n", __func__, result);
		seq_puts(s, "Self_Test Fail:\n");
		/*return 0x01;*/
		i = 1;
	}

	return i;
}

static void himax_mcu_idle_mode(int disable)
{
	int ret = 0;

	if (!disable) {
		ret = himax_bus_write(on_pfw_op->addr_fw_mode_status[0],
				on_pfw_op->data_idle_en_pwd, 1,
				HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}
	} else {
		ret = himax_bus_write(on_pfw_op->addr_fw_mode_status[0],
				on_pfw_op->data_idle_dis_pwd, 1,
				HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}
	}
	msleep(40);
}

static void himax_mcu_reload_disable(int disable)
{
}

static bool himax_mcu_check_chip_version(void)
{
	return true;
}

static int himax_mcu_read_ic_trigger_type(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	int trigger_type = false;

	g_core_fp.fp_register_read(on_pdriver_op->addr_fw_define_int_is_edge,
			DATA_LEN_4, tmp_data, 0);

	if ((tmp_data[1] & 0x01) == 1)
		trigger_type = true;

	return trigger_type;
}

static int himax_mcu_read_i2c_status(void)
{
	return i2c_error_count;
}

static void himax_mcu_read_FW_ver(void)
{
	uint8_t data[DATA_LEN_8];
	int i = 0;
	int ret = 0;

	for (i = 0; i < 6; i++) {
		ret = himax_bus_read(on_pfw_op->addr_fw_ver_start[0] + i,
				&data[i], 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			E("Maybe NOT have FW in chipset\n");
			E("Maybe Wrong FW in chipset\n");
		}
	}

	ic_data->vendor_fw_ver = data[0] << 8 | data[1];
	/*I("PANEL_VER : %X\n", ic_data->vendor_panel_ver);*/
	I("FW_VER : %X\n", ic_data->vendor_fw_ver);
	ic_data->vendor_cid_maj_ver = data[2];
	ic_data->vendor_cid_min_ver = data[3];
	ic_data->vendor_config_ver = data[4];
	I("CID_VER : %X\n", (ic_data->vendor_cid_maj_ver << 8
						| ic_data->vendor_cid_min_ver));
	I("CFG_VER : %X\n", ic_data->vendor_config_ver);

	ic_data->vendor_touch_cfg_ver = data[4];
	I("TOUCH_VER : %X\n", ic_data->vendor_touch_cfg_ver);
	ic_data->vendor_display_cfg_ver = data[5];
	I("DISPLAY_VER : %X\n", ic_data->vendor_display_cfg_ver);
}

static bool himax_mcu_read_event_stack(uint8_t *buf, uint8_t length)
{
	uint8_t cmd[DATA_LEN_4];
	int ret = 0;
	/* AHB_I2C Burst Read Off */
	cmd[0] = on_pfw_op->data_ahb_dis[0];

	ret = himax_bus_write(on_pfw_op->addr_ahb_addr[0],
			cmd, 1, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}

	himax_bus_read(on_pfw_op->addr_event_addr[0], buf,
			length, HIMAX_I2C_RETRY_TIMES);
	/*  AHB_I2C Burst Read On */
	cmd[0] = on_pfw_op->data_ahb_en[0];

	ret = himax_bus_write(on_pfw_op->addr_ahb_addr[0],
			cmd, 1, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}

	return 1;
}

static void himax_mcu_return_event_stack(void)
{
	int retry = 20;
	int i;
	uint8_t tmp_data[DATA_LEN_4];

	I("%s\n", __func__);

	do {
		I("now %d times!\n", retry);

		for (i = 0; i < DATA_LEN_4; i++)
			tmp_data[i] = on_psram_op->addr_rawdata_end[i];

		g_core_fp.fp_register_write(on_psram_op->addr_rawdata_addr,
				DATA_LEN_4, tmp_data, 0);
		g_core_fp.fp_register_read(on_psram_op->addr_rawdata_addr,
				DATA_LEN_4, tmp_data, 0);
		retry--;
		/*msleep(10);*/
		usleep_range(10000, 10100);
	} while ((tmp_data[1] != on_psram_op->addr_rawdata_end[1]
		&& tmp_data[0] != on_psram_op->addr_rawdata_end[0])
		&& retry > 0);

	I("%s: End of setting!\n", __func__);
}

static bool himax_mcu_calculateChecksum(bool change_iref, uint32_t size)
{
	uint32_t CRC_result = 0;
	uint8_t tmp_data[DATA_LEN_4];
	bool ret = 0;

	if (!g_core_fp.fp_ahb_squit())
		return false;

	CRC_result = g_core_fp.fp_check_CRC(tmp_data, FW_SIZE_64k);
	msleep(50);
	I("%s: CRC_result = %8X!\n", __func__, CRC_result);

	ret = (CRC_result == 0) ? true : false;

	g_core_fp.fp_init_auto_func();
	return ret;
}

static void himax_mcu_read_FW_status(void)
{
	/*not support in on-cell*/
}

static void himax_mcu_irq_switch(int switch_on)
{
	int ret = 0;

	if (switch_on) {
		if (private_ts->use_irq)
			himax_int_enable(switch_on);
		else
			hrtimer_start(&private_ts->timer, ktime_set(1, 0),
					HRTIMER_MODE_REL);
	} else {
		if (private_ts->use_irq) {
			himax_int_enable(switch_on);
		} else {
			hrtimer_cancel(&private_ts->timer);
			ret = cancel_work_sync(&private_ts->work);
		}
	}
}

static uint8_t himax_mcu_read_DD_status(uint8_t *cmd_set, uint8_t *tmp_data)
{
	/*not support in on-cell*/
	return NO_ERR;
}
/* FW side end*/
/* CORE_FW */

/* CORE_FLASH */
/* FLASH side start*/
static void himax_mcu_chip_erase(void)
{
	g_core_fp.fp_register_write(on_pflash_op->addr_ctrl_auto, DATA_LEN_4,
			on_pflash_op->data_main_erase, 0);
}

static bool himax_mcu_block_erase(int start_addr, int length)
{
	return true;
}

static bool himax_mcu_sector_erase(int start_addr)
{
	return true;
}

static bool himax_mcu_ahb_squit(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t retry_cnt = 0;

	g_core_fp.fp_register_write(on_pflash_op->addr_ahb_ctrl, DATA_LEN_4,
			on_pflash_op->data_ahb_squit, 0);
	/*msleep(1);*/
	usleep_range(1000, 1100);

	do {
		if (retry_cnt++ > 100) {
			E("%s: ahb squit error!\n", __func__);
			return false;
		}
		/*msleep(1);*/
		usleep_range(1000, 1100);
		g_core_fp.fp_register_read(on_pflash_op->addr_ahb_ctrl,
				DATA_LEN_4, tmp_data, 0);
	} while (tmp_data[0] != 0x00);

	return true;
}

bool himax_mcu_sfr_rw(uint8_t *addr, int length, uint8_t *data,
		uint8_t rw_ctrl) /*r:0, w:1*/
{
	uint8_t tmp_data[DATA_LEN_4];
	int i = 0;

	I("%s: Enter rw_ctrl = %d\n", __func__, rw_ctrl);

	tmp_data[3] = (length / 4 - 1) >> 8; /*access_unit*/
	tmp_data[2] = length / 4 - 1; /*access_unit*/
	tmp_data[1] = on_pflash_op->data_auto[0];/*0xA5;*/
	tmp_data[0] = on_pflash_op->data_sfr_read[0] + rw_ctrl; /*r:14, w:15*/
	g_core_fp.fp_register_write(on_pflash_op->addr_ctrl_auto,
			DATA_LEN_4, tmp_data, 0);

	/*msleep(1);*/
	usleep_range(1000, 1100);

	tmp_data[1] = 0x00;
	tmp_data[0] = 0x00;
	if (!rw_ctrl) {
		g_core_fp.fp_register_read(addr, length, data, 0);
		I("%s:SFR!data0=0x%2X,data1=0x%2X,data2=0x%2X,data3=0x%2X\n",
			__func__, data[0], data[1], data[2], data[3]);
	} else {
		g_core_fp.fp_register_write(addr, length, data, 0);
		/*msleep(1);*/
		usleep_range(1000, 1100);
		if (g_core_fp.fp_wait_wip(5)) {
			g_core_fp.fp_sfr_rw(addr, DATA_LEN_4, tmp_data, 0);
			for (i = 0; i < DATA_LEN_4; i++) {
				if (tmp_data[i] != data[i]) {
					g_core_fp.fp_ahb_squit();
					E("%s:Write SFR data fail\n", __func__);
					E("%s:data[%d]=0x%02X\n",
						__func__, i, data[i]);
					E("%s:read_data[%d] = 0x%02X\n",
						__func__, i, tmp_data[i]);
					return false;
				}
			}
			if (!g_core_fp.fp_ahb_squit())
				return false;
			else
				return true;
		}
	}

	return true;
}

bool himax_mcu_unlock_flash(void)
{
	if (!g_core_fp.fp_ahb_squit()) /*switch to SFR Read*/
		return false;

	if (!g_core_fp.fp_sfr_rw(on_pflash_op->addr_unlock_0,
	DATA_LEN_4, on_pflash_op->data_cmd0, 1))
		return false;

	if (!g_core_fp.fp_sfr_rw(on_pflash_op->addr_unlock_4,
	DATA_LEN_4, on_pflash_op->data_cmd1, 1))
		return false;

	if (!g_core_fp.fp_sfr_rw(on_pflash_op->addr_unlock_8,
	DATA_LEN_4, on_pflash_op->data_cmd2, 1))
		return false;

	if (!g_core_fp.fp_sfr_rw(on_pflash_op->addr_unlock_c,
	DATA_LEN_4, on_pflash_op->data_cmd3, 1))
		return false;

	return true;
}

bool himax_mcu_lock_flash(void)
{
	uint8_t tmp_data[DATA_LEN_4] = {0};

	if (!g_core_fp.fp_sfr_rw(on_pflash_op->addr_unlock_c,
	DATA_LEN_4, tmp_data, 0))
		return false;

	if (!g_core_fp.fp_ahb_squit())
		return false;

	tmp_data[3] |= on_pflash_op->data_lock[3];/*0x03;*/
	tmp_data[2] |= on_pflash_op->data_lock[2];/*0x40;*/

	if (!g_core_fp.fp_sfr_rw(on_pflash_op->addr_unlock_c,
	DATA_LEN_4, tmp_data, 1))
		return false;

	return true;
}

static void himax_mcu_init_auto_func(void)
{
	uint8_t tmp_data[DATA_LEN_4] = {0};

	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = on_pflash_op->data_auto[0]; /*0xA5*/
	tmp_data[0] = on_pflash_op->data_main_read[0];/*0x03*/

	g_core_fp.fp_register_write(on_pflash_op->addr_ctrl_auto,
			DATA_LEN_4, tmp_data, 0);
}

static void himax_mcu_flash_programming(uint8_t *FW_content, int FW_Size)
{
	uint8_t tmp_addr[DATA_LEN_4];
	uint8_t tmp_data[FLASH_RW_MAX_LEN] = {0};
	unsigned char *ImageBuffer = FW_content;
	int s_idx = 0;
	int p_idx = 0;
	int p_tmp_idx = 0;
	int p_end_idx = 0;
	int srart_addr = 0x00;
	int end_addr = srart_addr + FW_Size;
	int page_size = FW_PAGE_SZ;
	int page_per_sector = FW_PAGE_PER_SECTOR;
	int sector_byte = page_per_sector * page_size;
	int prog_tmp_idx = 0;

	int start_sector_idx = srart_addr / sector_byte;
	int start_page_idx = srart_addr % sector_byte;
	int end_sector_idx = end_addr / sector_byte;
	int end_page_idx = end_addr % sector_byte;

	if (!g_core_fp.fp_wait_wip(5)) {
		if (!g_core_fp.fp_ahb_squit()) {
			E("%s: Wait wip error!\n", __func__);
			return;
		}
	}

	g_core_fp.fp_burst_enable(1);

	for (s_idx = start_sector_idx; s_idx < end_sector_idx; s_idx++) {
		I("%s: Programing Sector %d\n", __func__, s_idx);
		if (s_idx == start_sector_idx) {
			p_tmp_idx = start_page_idx;
			p_end_idx = page_per_sector;
		} else if (s_idx == end_sector_idx) {
			p_tmp_idx = 0;
			p_end_idx = end_page_idx;
		} else {
			p_tmp_idx = 0;
			p_end_idx = page_per_sector;
		}

		for (p_idx = p_tmp_idx ; p_idx < p_end_idx ; p_idx++) {
			prog_tmp_idx = s_idx * sector_byte + p_idx * page_size;
			memcpy(tmp_data, &ImageBuffer[prog_tmp_idx],
				page_size * sizeof(uint8_t));
			/*I("%s: Sector idx = %d, Page idx = %d,
			 *	prog_tmp_idx = %X\n",
			 *	__func__, s_idx, p_idx, prog_tmp_idx);
			 */
			tmp_addr[3] = 0x00;
			tmp_addr[2] = 0x00;
			tmp_addr[1] = (s_idx << 3) + (p_idx >> 3);
			tmp_addr[0] = (p_idx % 0x08) << 5;
			g_core_fp.fp_flash_page_write(tmp_addr,
				page_size, tmp_data);
		}
	}
}

static void himax_mcu_flash_page_write(uint8_t *write_addr, int length,
		uint8_t *write_data)
{
	uint8_t tmp_addr[DATA_LEN_4] = {0};
	uint8_t tmp_data[DATA_LEN_4] = {0};

	g_core_fp.fp_register_write(on_pflash_op->addr_ctrl_base,
			DATA_LEN_4, write_addr, 0);

	tmp_data[3] = (length / 4 - 1) >> 8; /*access_unit*/
	tmp_data[2] = length / 4 - 1; /*access_unit*/
	tmp_data[1] = on_pflash_op->data_auto[0];
	tmp_data[0] = on_pflash_op->data_page_write[0];
	g_core_fp.fp_register_write(on_pflash_op->addr_ctrl_auto,
			DATA_LEN_4, tmp_data, 0);

	g_core_fp.fp_register_write(tmp_addr, length, write_data, 0);

	/*msleep(10);*/

	g_core_fp.fp_wait_wip(5);
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_32k(unsigned char *fw,
		int len, bool change_iref)
{
	/* Not use */
	return 0;
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_60k(unsigned char *fw,
		int len, bool change_iref)
{
	/* Not use */
	return 0;
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_64k(unsigned char *fw,
		int len, bool change_iref)
{
	int ret = 0;
	uint32_t CRC_result = 0xFFFF;

	if (len != FW_SIZE_64k) {
		E("%s: The file size is not 64K bytes\n", __func__);
		return ret;
	}

	if (!g_core_fp.fp_sense_off(true)) {
		E("%s: sense_off failed!\n", __func__);
		goto Failed;
	}

	if (!g_core_fp.fp_unlock_flash()) {
		E("%s: himax_unlock_flash failed!\n", __func__);
		goto Failed;
	}

	g_core_fp.fp_chip_erase();
	g_core_fp.fp_flash_programming(fw, len);

	if (!g_core_fp.fp_lock_flash()) {
		E("%s: himax_lock_flash failed!\n", __func__);
		goto Failed;
	}

	CRC_result = g_core_fp.fp_check_CRC(on_pfw_op->addr_program_reload_from,
			len);

	if (CRC_result != 0) {
		E("%s: check_CRC failed! CRC_result = 0x%X\n", __func__,
			CRC_result);
	} else {
		I("%s: CRC checksum pass!\n", __func__);
		ret = 1;
	}
	g_core_fp.fp_init_auto_func();

Failed:
	return ret;
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_124k(unsigned char *fw,
		int len, bool change_iref)
{
	/* Not use */
	return 0;
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_128k(unsigned char *fw,
		int len, bool change_iref)
{
	/* Not use */
	return 0;
}

static void himax_mcu_flash_read(uint8_t *r_data, int start_addr,
		int length)
{
	uint8_t tmp_addr[DATA_LEN_4];
	uint8_t tmp_data[FLASH_RW_MAX_LEN] = {0};
	unsigned char *r_buf = r_data;
	int i = 0, addr = 0;

	tmp_data[3] = (FW_PAGE_SZ / 4 - 1) >> 8; /*access_unit*/
	tmp_data[2] = FW_PAGE_SZ / 4 - 1; /*access_unit*/
	tmp_data[1] = on_pflash_op->data_auto[0];/*0xA5;*/
	tmp_data[0] = on_pflash_op->data_main_read[0];/*0x03;*/
	g_core_fp.fp_register_write(on_pflash_op->addr_ctrl_auto,
			DATA_LEN_4, tmp_data, 0);

	for (addr = start_addr; addr < start_addr + length;
	addr = addr + FW_PAGE_SZ) {
		I("%s: addr = %X, i = %d\n", __func__, addr, i);
		tmp_addr[0] = (uint8_t)addr;
		tmp_addr[1] = (uint8_t)(addr >> 8);
		tmp_addr[2] = (uint8_t)(addr >> 16);
		tmp_addr[3] = (uint8_t)(addr >> 24);
		g_core_fp.fp_register_read(tmp_addr, FW_PAGE_SZ, tmp_data, 0);
		memcpy(&r_buf[i], tmp_data, FW_PAGE_SZ * sizeof(uint8_t));
		i = i + FW_PAGE_SZ;
	}
	I("%s: Finished!\n", __func__);
}

static void himax_mcu_flash_dump_func(uint8_t local_flash_command,
		int Flash_Size, uint8_t *flash_buffer)
{
	int page_prog_start = 0;

	g_core_fp.fp_sense_off(true);
	g_core_fp.fp_flash_read(flash_buffer, page_prog_start, Flash_Size);
	g_core_fp.fp_sense_on(0x01);
}
static bool himax_mcu_flash_lastdata_check(uint32_t size)
{
	return 0;
}
static int hx_mcu_diff_overlay_flash(void)
{
	return 0;
}


/* FLASH side end*/
/* CORE_FLASH */

/* CORE_SRAM */
/* SRAM side start*/
static void himax_mcu_sram_write(uint8_t *FW_content)
{
}

static bool himax_mcu_sram_verify(uint8_t *FW_File, int FW_Size)
{
	return true;
}

static bool himax_mcu_get_DSRAM_data(uint8_t *info_data, bool DSRAM_Flag)
{
	int i = 0;
	unsigned char tmp_addr[ADDR_LEN_4];
	unsigned char tmp_data[DATA_LEN_4];
	uint8_t max_i2c_size = MAX_I2C_TRANS_SZ;
	uint8_t x_num = ic_data->HX_RX_NUM;
	uint8_t y_num = ic_data->HX_TX_NUM;
	int m_key_num = 0;
	int total_size = (x_num * y_num + x_num + y_num) * 2 + 4;
	int total_size_temp;
	int mutual_data_size = x_num * y_num * 2;
	int self_data_size = (x_num + y_num) * 2;
	int total_read_times = 0;
	int address = 0;
	uint8_t *temp_info_data; /*max mkey size = 8*/
	uint16_t check_sum_cal = 0;
	int fw_run_flag = -1;
	bool ret_data = false;

	temp_info_data = kzalloc(sizeof(uint8_t) * (total_size + 8),
		GFP_KERNEL);
	if (temp_info_data == NULL) {
		E("%s: allocate memory fail!!!\n", __func__);
		return false;
	}
	/*1. Read number of MKey R100070E8H to determin data size*/
	m_key_num = ic_data->HX_BT_NUM;
	/* I("%s,m_key_num=%d\n",__func__ ,m_key_num);*/
	total_size += m_key_num * 2;

	g_core_fp.fp_idle_mode(1);

	/* 2. Start DSRAM Rawdata and Wait Data Ready */
	tmp_data[3] = 0x00; tmp_data[2] = 0x00;

	tmp_data[1] = on_psram_op->passwrd_start[1];
	tmp_data[0] = on_psram_op->passwrd_start[0];

	fw_run_flag = himax_write_read_reg(on_psram_op->addr_rawdata_addr,
			tmp_data, on_psram_op->passwrd_end[1],
			on_psram_op->passwrd_end[0]);

	if (fw_run_flag < 0) {
		I("%s Data NOT ready => bypass\n", __func__);
		kfree(temp_info_data);
		return false;
	}

	/* 3. Read RawData */
	total_size_temp = total_size;

	tmp_addr[0] = on_psram_op->addr_rawdata_addr[0];
	tmp_addr[1] = on_psram_op->addr_rawdata_addr[1];
	tmp_addr[2] = on_psram_op->addr_rawdata_addr[2];
	tmp_addr[3] = on_psram_op->addr_rawdata_addr[3];

	if (total_size % max_i2c_size == 0)
		total_read_times = total_size / max_i2c_size;
	else
		total_read_times = total_size / max_i2c_size + 1;

	for (i = 0; i < total_read_times; i++) {
		address = (on_psram_op->addr_rawdata_addr[3] << 24)
			+ (on_psram_op->addr_rawdata_addr[2] << 16)
			+ (on_psram_op->addr_rawdata_addr[1] << 8)
			+ on_psram_op->addr_rawdata_addr[0]
			+ i * max_i2c_size;

		tmp_addr[3] = (uint8_t)((address >> 24) & 0x00FF);
		tmp_addr[2] = (uint8_t)((address >> 16) & 0x00FF);
		tmp_addr[1] = (uint8_t)((address >> 8) & 0x00FF);
		tmp_addr[0] = (uint8_t)((address) & 0x00FF);

		if (total_size_temp >= max_i2c_size) {
			g_core_fp.fp_register_read(tmp_addr, max_i2c_size,
					&temp_info_data[i * max_i2c_size], 0);
			total_size_temp = total_size_temp - max_i2c_size;
		} else {
			/*I("last total_size_temp=%d\n",total_size_temp);*/
			g_core_fp.fp_register_read(tmp_addr,
					total_size_temp % max_i2c_size,
					&temp_info_data[i * max_i2c_size],
					0);
		}
	}

	/* 4. FW stop outputing */
	/*I("DSRAM_Flag=%d\n",DSRAM_Flag);*/
	if (DSRAM_Flag == false) {
		/*I("Return to Event Stack!\n");*/
		g_core_fp.fp_register_write(on_psram_op->addr_rawdata_addr,
				DATA_LEN_4, on_psram_op->data_fin, 0);
	} else {
		/*I("Continue to SRAM!\n");*/
		g_core_fp.fp_register_write(on_psram_op->addr_rawdata_addr,
				DATA_LEN_4, on_psram_op->data_conti, 0);
	}

	g_core_fp.fp_idle_mode(0);

	/* 5. Data Checksum Check */
	for (i = 2; i < total_size; i += 2) { /* 2:PASSWORD NOT included */
		check_sum_cal += (temp_info_data[i + 1] * 256
			+ temp_info_data[i]);
	}

	if (check_sum_cal % 0x10000 == 0) {
		/*I("%s: checksum PASS\n", __func__);*/
		memcpy(info_data, &temp_info_data[4],
				mutual_data_size * sizeof(uint8_t));
		memcpy(&info_data[mutual_data_size],
				&temp_info_data[mutual_data_size + 4],
				self_data_size * sizeof(uint8_t));
		ret_data = true;
	} else {
		I("%s: checksum FAIL=%2X\n", __func__, check_sum_cal);
	}

	kfree(temp_info_data);
	return ret_data;
}
/* SRAM side end*/
/* CORE_SRAM */

/* CORE_DRIVER */
static void himax_mcu_init_ic(void)
{
	I("%s: use default oncell init.\n", __func__);
}

#if defined(HX_BOOT_UPGRADE) || defined(HX_ZERO_FLASH)
static int himax_mcu_fw_ver_bin(void)
{
	I("%s: use default oncell address.\n", __func__);
	if (hxfw != NULL) {
		I("Catch fw version in bin file!\n");
		g_i_FW_VER = (hxfw->data[FW_VER_MAJ_FLASH_ADDR] << 8)
				| hxfw->data[FW_VER_MIN_FLASH_ADDR];
		g_i_CFG_VER = hxfw->data[CFG_VER_MAJ_FLASH_ADDR];
		g_i_CID_MAJ = hxfw->data[CID_VER_MAJ_FLASH_ADDR];
		g_i_CID_MIN = hxfw->data[CID_VER_MIN_FLASH_ADDR];
	} else {
		I("%s: FW data is null!\n", __func__);
		return 1;
	}
	return NO_ERR;
}
#endif

#if defined(HX_RST_PIN_FUNC)
static void himax_mcu_pin_reset(void)
{
	I("%s: Now reset the Touch chip.\n", __func__);
	himax_rst_gpio_set(private_ts->rst_gpio, 0);
	usleep_range(RST_LOW_PERIOD_S, RST_LOW_PERIOD_E);
	himax_rst_gpio_set(private_ts->rst_gpio, 1);
	usleep_range(RST_HIGH_PERIOD_S, RST_HIGH_PERIOD_E);
}

static void himax_mcu_ic_reset(uint8_t loadconfig, uint8_t int_off)
{
	struct himax_ts_data *ts = private_ts;

	HX_HW_RESET_ACTIVATE = 1;

	I("%s,status: loadconfig=%d,int_off=%d\n", __func__,
		loadconfig, int_off);

	if (ts->rst_gpio >= 0) {
		if (int_off)
			g_core_fp.fp_irq_switch(0);

		g_core_fp.fp_pin_reset();

		/* if (loadconfig)
			g_core_fp.fp_reload_config(); */

		if (int_off)
			g_core_fp.fp_irq_switch(1);
	}
}
#endif

#if !defined(HX_FIX_TOUCH_INFO)
static uint8_t himax_mcu_tp_info_check(void)
{
	uint8_t rx = on_pdriver_op->data_df_rx[0];
	uint8_t tx = on_pdriver_op->data_df_tx[0];
	uint8_t pt = on_pdriver_op->data_df_pt[0];
	uint8_t err_cnt = 0;

	if (ic_data->HX_RX_NUM < (rx / 8)
	|| ic_data->HX_RX_NUM > (rx * 3 / 2)) {
		ic_data->HX_RX_NUM = rx;
		err_cnt |= 0x01;
	}

	if (ic_data->HX_TX_NUM < (tx / 8)
	|| ic_data->HX_TX_NUM > (tx * 3 / 2)) {
		ic_data->HX_TX_NUM = tx;
		err_cnt |= 0x02;
	}

	if (ic_data->HX_MAX_PT < (pt / 8)
	|| ic_data->HX_MAX_PT > (pt * 3 / 2)) {
		ic_data->HX_MAX_PT = pt;
		err_cnt |= 0x04;
	}

	return err_cnt;
}
#endif

static void himax_mcu_touch_information(void)
{
#if !defined(HX_FIX_TOUCH_INFO)
	uint8_t data[DATA_LEN_8] = {0};
	uint8_t err_cnt = 0;

#if !defined(HX_NEW_EVENT_STACK_FORMAT)
	/*cmd[3] = 0x08; cmd[2] = 0x00; cmd[1] = 0x00; cmd[0] = 0x1C;*/
	g_core_fp.fp_register_read(on_pdriver_op->addr_fw_rx_tx_maxpt_num,
			DATA_LEN_4, data, 0);

	ic_data->HX_RX_NUM = data[0];
	ic_data->HX_TX_NUM = data[1];
	ic_data->HX_BT_NUM = data[2];
	ic_data->HX_MAX_PT = data[3];
	/*I("%s : HX_RX_NUM=%d,HX_TX_NUM=%d, HX_BT_NUM=%d,HX_MAX_PT=%d\n",
	 *		__func__, ic_data->HX_RX_NUM, ic_data->HX_TX_NUM,
	 *		ic_data->HX_BT_NUM,ic_data->HX_MAX_PT);
	 */

	/*cmd[3] = 0x08; cmd[2] = 0x00; cmd[1] = 0x00; cmd[0] = 0x0C;*/
	g_core_fp.fp_register_read(on_pdriver_op->addr_fw_xy_rev_int_edge,
			DATA_LEN_4, data, 0);
	/*0x0800000E, bit[4]*/
	if ((data[2] & 0x10) == 0x10)
		ic_data->HX_XY_REVERSE = true;
	else
		ic_data->HX_XY_REVERSE = false;
	/*I("%s : HX_XY_REVERSE=0x%2.2X\n",__func__,ic_data->HX_XY_REVERSE);*/

	/*0x0800000D, bit[0]*/
	if ((data[1] & 0x01) == 1)
		ic_data->HX_INT_IS_EDGE = true;
	else
		ic_data->HX_INT_IS_EDGE = false;
	/*I("%s : HX_INT_IS_EDGE=0x%2.2X\n",__func__,ic_data->HX_INT_IS_EDGE);*/
#else
	g_core_fp.fp_register_read(on_pdriver_op->addr_fw_rx_tx_maxpt_num,
			DATA_LEN_4, data, 0);
	ic_data->HX_RX_NUM = data[0];
	ic_data->HX_TX_NUM = data[1];
	g_core_fp.fp_register_read(on_pdriver_op->addr_fw_maxpt_bt_num,
			DATA_LEN_4, data, 0);
	ic_data->HX_MAX_PT = data[0];
	ic_data->HX_BT_NUM = data[1];

	g_core_fp.fp_register_read(on_pdriver_op->addr_fw_xy_rev_int_edge,
			DATA_LEN_4, data, 0);
	/*0x0800000E, bit[4]*/
	if ((data[1] & 0x40) == 0x40)
		ic_data->HX_XY_REVERSE = true;
	else
		ic_data->HX_XY_REVERSE = false;
	/*I("%s : HX_XY_REVERSE=0x%2.2X\n", __func__, ic_data->HX_XY_REVERSE);*/

	/*0x0800000D, bit[0]*/
	if ((data[1] & 0x01) == 1)
		ic_data->HX_INT_IS_EDGE = true;
	else
		ic_data->HX_INT_IS_EDGE = false;
	/*I("%s : HX_INT_IS_EDGE=0x%2.2X\n", __func__,*/
	/*ic_data->HX_INT_IS_EDGE);*/
#endif
	ic_data->HX_PEN_FUNC = false; /*Not support pen in on-cell*/
	err_cnt = himax_mcu_tp_info_check();
	if (err_cnt > 0)
		E("TP Info from IC is wrong, err_cnt = 0x%X\n", err_cnt);
#else
	ic_data->HX_RX_NUM      = FIX_HX_RX_NUM;
	ic_data->HX_TX_NUM      = FIX_HX_TX_NUM;
	ic_data->HX_BT_NUM      = FIX_HX_BT_NUM;
	ic_data->HX_MAX_PT      = FIX_HX_MAX_PT;
	ic_data->HX_XY_REVERSE  = FIX_HX_XY_REVERSE;
	ic_data->HX_INT_IS_EDGE = FIX_HX_INT_IS_EDGE;
	ic_data->HX_PEN_FUNC    = FIX_HX_PEN_FUNC;
#endif
	ic_data->HX_Y_RES = private_ts->pdata->screenHeight;
	ic_data->HX_X_RES = private_ts->pdata->screenWidth;
	I("%s:HX_RX_NUM =%d,HX_TX_NUM =%d\n", __func__,
		ic_data->HX_RX_NUM, ic_data->HX_TX_NUM);
	I("%s:HX_MAX_PT=%d,HX_XY_REVERSE =%d\n", __func__,
		ic_data->HX_MAX_PT, ic_data->HX_XY_REVERSE);
	I("%s:HX_Y_RES=%d,HX_X_RES =%d\n", __func__,
		ic_data->HX_Y_RES, ic_data->HX_X_RES);
	I("%s:HX_INT_IS_EDGE =%d,HX_PEN_FUNC = %d\n", __func__,
	ic_data->HX_INT_IS_EDGE, ic_data->HX_PEN_FUNC);
}

static void himax_mcu_calcTouchDataSize(void)
{
	struct himax_ts_data *ts_data = private_ts;

	ts_data->x_channel = ic_data->HX_RX_NUM;
	ts_data->y_channel = ic_data->HX_TX_NUM;
	ts_data->nFinger_support = ic_data->HX_MAX_PT;
#if 0
	ts_data->coord_data_size = 4 * ts_data->nFinger_support;
	ts_data->area_data_size = ((ts_data->nFinger_support / 4)
				+ (ts_data->nFinger_support % 4 ? 1 : 0)) * 4;
	ts_data->coordInfoSize = ts_data->coord_data_size
				+ ts_data->area_data_size + 4;
	ts_data->raw_data_frame_size = 128 - ts_data->coord_data_size
				- ts_data->area_data_size - 4 - 4 - 2;

	if (ts_data->raw_data_frame_size == 0) {
		E("%s: could NOT calculate!\n", __func__);
		return;
	}

	ts_data->raw_data_nframes = ((uint32_t)ts_data->x_channel
					* ts_data->y_channel
					+ ts_data->x_channel
					+ ts_data->y_channel)
					/ ts_data->raw_data_frame_size
					+ (((uint32_t)ts_data->x_channel
					* ts_data->y_channel
					+ ts_data->x_channel
					+ ts_data->y_channel)
					% ts_data->raw_data_frame_size) ? 1 : 0;

	I("%s: coord_dsz:%d,area_dsz:%d,raw_data_fsz:%d,raw_data_nframes:%d",
		__func__,
		ts_data->coord_data_size,
		ts_data->area_data_size,
		ts_data->raw_data_frame_size,
		ts_data->raw_data_nframes);
#endif
	HX_TOUCH_INFO_POINT_CNT = ic_data->HX_MAX_PT * 4;
	if ((ic_data->HX_MAX_PT % 4) == 0)
		HX_TOUCH_INFO_POINT_CNT +=
			(ic_data->HX_MAX_PT / 4) * 4;
	else
		HX_TOUCH_INFO_POINT_CNT +=
			((ic_data->HX_MAX_PT / 4) + 1) * 4;

	if (himax_report_data_init())
		E("%s: allocate data fail\n", __func__);

}

#if 0
static void himax_mcu_reload_config(void)
{
	if (himax_report_data_init())
		E("%s: allocate data fail\n", __func__);

	g_core_fp.fp_sense_on(0x00);
}
#endif

static int himax_mcu_get_touch_data_size(void)
{
	return HIMAX_TOUCH_DATA_SIZE;
}

static int himax_mcu_hand_shaking(void)
{
	/* 0:Running, 1:Stop, 2:I2C Fail */
	int result = 0;
	return result;
}

static int himax_mcu_determin_diag_rawdata(int diag_command)
{
	return diag_command % 10;
}

static int himax_mcu_determin_diag_storage(int diag_command)
{
	return diag_command / 10;
}

static int himax_mcu_cal_data_len(int raw_cnt_rmd, int HX_MAX_PT,
		int raw_cnt_max)
{
	int RawDataLen;
	/* rawdata checksum is 2 bytes */
	if (raw_cnt_rmd != 0x00)
		RawDataLen = MAX_I2C_TRANS_SZ
			- ((HX_MAX_PT + raw_cnt_max + 3) * 4) - 2;
	else
		RawDataLen = MAX_I2C_TRANS_SZ
			- ((HX_MAX_PT + raw_cnt_max + 2) * 4) - 2;

	return RawDataLen;
}

static bool himax_mcu_diag_check_sum(struct himax_report_data *hx_touch_data)
{
	uint16_t check_sum_cal = 0;
	int i;

	/* Check 128th byte CRC */
	for (i = 0, check_sum_cal = 0;
	i < (hx_touch_data->touch_all_size
	- hx_touch_data->touch_info_size);
	i += 2) {
		check_sum_cal += (hx_touch_data->hx_rawdata_buf[i + 1]
				* FLASH_RW_MAX_LEN
				+ hx_touch_data->hx_rawdata_buf[i]);
	}

	if (check_sum_cal % HX64K != 0) {
		I("%s: checksum FAIL=%2X\n", __func__, check_sum_cal);
		return 0;
	}

	return 1;
}

static void himax_mcu_diag_parse_raw_data(
		struct himax_report_data *hx_touch_data,
		int mul_num, int self_num,
		uint8_t diag_cmd,
		int32_t *mutual_data,
		int32_t *self_data)
{
	diag_mcu_parse_raw_data(hx_touch_data, mul_num, self_num,
			diag_cmd, mutual_data, self_data);
}

#if defined(HX_EXCP_RECOVERY)
static int himax_mcu_ic_excp_recovery(uint32_t hx_excp_event,
		uint32_t hx_zero_event, uint32_t length)
{
	if (g_zero_event_count > 5) {
		g_zero_event_count = 0;
		I("%s: EXCEPTION event checked - ALL Zero.\n", __func__);
		/*goto checksum_fail;*/
		return HX_EXCP_EVENT;
	}

	if (hx_excp_event == length) {
		g_zero_event_count = 0;
		/*goto checksum_fail;*/
		return HX_EXCP_EVENT;
	} else if (hx_zero_event == length) {
		g_zero_event_count++;
		I("%s: ALL Zero event is %d times.\n", __func__,
			g_zero_event_count);
		/*goto err_workqueue_out;*/
		return HX_ZERO_EVENT_COUNT;
	}

	return NO_ERR;

/*checksum_fail:*/
	/*return CHECKSUM_FAIL;*/
/*err_workqueue_out:*/
	/*return WORK_OUT;*/
}

static void himax_mcu_excp_ic_reset(void)
{
	HX_EXCP_RESET_ACTIVATE = 1;
	I("%s: Now reset the Touch chip.\n", __func__);
#if defined(HX_RST_PIN_FUNC)
	himax_mcu_pin_reset();
#endif
}
#endif

#if defined(HX_SMART_WAKEUP)\
	|| defined(HX_HIGH_SENSE)\
	|| defined(HX_USB_DETECT_GLOBAL)
static void himax_mcu_resend_cmd_func(bool suspended)
{
#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE)
	struct himax_ts_data *ts = private_ts;
#endif

	if (!suspended) {
		/* if entering resume need to sense off first*/
		himax_int_enable(0);
#if defined(HX_RESUME_HW_RESET)
		g_core_fp.fp_ic_reset(false, false);
#else
		g_core_fp.fp_sense_off(true);
#endif
	}

#if defined(HX_SMART_WAKEUP)
	g_core_fp.fp_set_SMWP_enable(ts->SMWP_enable, suspended);
#endif
#if defined(HX_HIGH_SENSE)
	g_core_fp.fp_set_HSEN_enable(ts->HSEN_enable, suspended);
#endif
#if defined(HX_USB_DETECT_GLOBAL)
	himax_cable_detect_func(true);
#endif
}
#endif
/* CORE_DRIVER */

/* CORE_INIT */
/* init start */
static void himax_mcu_fp_init(void)
{
/* CORE_IC */
	g_core_fp.fp_burst_enable = himax_mcu_burst_enable;
	g_core_fp.fp_register_read = himax_mcu_register_read;
	/*g_core_fp.fp_flash_write_burst = himax_mcu_flash_write_burst;*/
	/*g_core_fp.fp_flash_write_burst_lenth =
	 *	himax_mcu_flash_write_burst_lenth;
	 */
	g_core_fp.fp_register_write = himax_mcu_register_write;
	g_core_fp.fp_interface_on = himax_mcu_interface_on;
	g_core_fp.fp_sense_on = himax_mcu_sense_on;
	g_core_fp.fp_tcon_on = himax_mcu_tcon_on;
	g_core_fp.fp_watch_dog_off = himax_mcu_watch_dog_off;
	g_core_fp.fp_sense_off = himax_mcu_sense_off;
	g_core_fp.fp_sleep_in = himax_mcu_sleep_in;
	g_core_fp.fp_wait_wip = himax_mcu_wait_wip;
	g_core_fp.fp_resume_ic_action = himax_mcu_resume_ic_action;
	g_core_fp.fp_suspend_ic_action = himax_mcu_suspend_ic_action;
	g_core_fp.fp_power_on_init = himax_mcu_power_on_init;
/* CORE_IC */
/* CORE_FW */
	g_core_fp.fp_Calculate_CRC_with_AP = himax_mcu_Calculate_CRC_with_AP;
	g_core_fp.fp_check_CRC = himax_mcu_check_CRC;
	g_core_fp.fp_set_SMWP_enable = himax_mcu_set_SMWP_enable;
	g_core_fp.fp_set_HSEN_enable = himax_mcu_set_HSEN_enable;
	g_core_fp.fp_usb_detect_set = himax_mcu_usb_detect_set;
	g_core_fp.fp_diag_register_set = himax_mcu_diag_register_set;
#if defined(HX_TP_SELF_TEST_DRIVER)
	g_core_fp.fp_control_reK = himax_mcu_control_reK;
#endif
	g_core_fp.fp_chip_self_test = himax_mcu_chip_self_test;
	g_core_fp.fp_idle_mode = himax_mcu_idle_mode;
	g_core_fp.fp_reload_disable = himax_mcu_reload_disable;
	g_core_fp.fp_check_chip_version = himax_mcu_check_chip_version;
	g_core_fp.fp_read_ic_trigger_type = himax_mcu_read_ic_trigger_type;
	g_core_fp.fp_read_i2c_status = himax_mcu_read_i2c_status;
	g_core_fp.fp_read_FW_ver = himax_mcu_read_FW_ver;
	g_core_fp.fp_read_event_stack = himax_mcu_read_event_stack;
	g_core_fp.fp_return_event_stack = himax_mcu_return_event_stack;
	g_core_fp.fp_calculateChecksum = himax_mcu_calculateChecksum;
	g_core_fp.fp_read_FW_status = himax_mcu_read_FW_status;
	g_core_fp.fp_irq_switch = himax_mcu_irq_switch;
	g_core_fp.fp_read_DD_status = himax_mcu_read_DD_status;
/* CORE_FW */
/* CORE_FLASH */
	g_core_fp.fp_chip_erase = himax_mcu_chip_erase;
	g_core_fp.fp_block_erase = himax_mcu_block_erase;
	g_core_fp.fp_sector_erase = himax_mcu_sector_erase;
	g_core_fp.fp_flash_programming = himax_mcu_flash_programming;
	g_core_fp.fp_flash_page_write = himax_mcu_flash_page_write;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_32k =
			himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_32k;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_60k =
			himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_60k;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_64k =
			himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_64k;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_124k =
			himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_124k;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_128k =
			himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_128k;
	g_core_fp.fp_flash_dump_func = himax_mcu_flash_dump_func;
	g_core_fp.fp_flash_lastdata_check = himax_mcu_flash_lastdata_check;
	g_core_fp._diff_overlay_flash = hx_mcu_diff_overlay_flash;
	g_core_fp.fp_ahb_squit = himax_mcu_ahb_squit;
	g_core_fp.fp_flash_read = himax_mcu_flash_read;
	g_core_fp.fp_sfr_rw = himax_mcu_sfr_rw;
	g_core_fp.fp_unlock_flash = himax_mcu_unlock_flash;
	g_core_fp.fp_lock_flash = himax_mcu_lock_flash;
	g_core_fp.fp_init_auto_func = himax_mcu_init_auto_func;
/* CORE_FLASH */
/* CORE_SRAM */
	g_core_fp.fp_sram_write = himax_mcu_sram_write;
	g_core_fp.fp_sram_verify = himax_mcu_sram_verify;
	g_core_fp.fp_get_DSRAM_data = himax_mcu_get_DSRAM_data;
/* CORE_SRAM */
/* CORE_DRIVER */
	g_core_fp.fp_chip_init = himax_mcu_init_ic;
#if defined(HX_BOOT_UPGRADE) || defined(HX_ZERO_FLASH)
	g_core_fp.fp_fw_ver_bin = himax_mcu_fw_ver_bin;
#endif
#if defined(HX_RST_PIN_FUNC)
	g_core_fp.fp_pin_reset = himax_mcu_pin_reset;
	g_core_fp.fp_ic_reset = himax_mcu_ic_reset;
#endif
	g_core_fp.fp_touch_information = himax_mcu_touch_information;
	g_core_fp.fp_calc_touch_data_size = himax_mcu_calcTouchDataSize;
	/*g_core_fp.fp_reload_config = himax_mcu_reload_config;*/
	g_core_fp.fp_get_touch_data_size = himax_mcu_get_touch_data_size;
	g_core_fp.fp_hand_shaking = himax_mcu_hand_shaking;
	g_core_fp.fp_determin_diag_rawdata = himax_mcu_determin_diag_rawdata;
	g_core_fp.fp_determin_diag_storage = himax_mcu_determin_diag_storage;
	g_core_fp.fp_cal_data_len = himax_mcu_cal_data_len;
	g_core_fp.fp_diag_check_sum = himax_mcu_diag_check_sum;
	g_core_fp.fp_diag_parse_raw_data = himax_mcu_diag_parse_raw_data;
#if defined(HX_EXCP_RECOVERY)
	g_core_fp.fp_ic_excp_recovery = himax_mcu_ic_excp_recovery;
	g_core_fp.fp_excp_ic_reset = himax_mcu_excp_ic_reset;
#endif
#if defined(HX_SMART_WAKEUP)\
	|| defined(HX_HIGH_SENSE)\
	|| defined(HX_USB_DETECT_GLOBAL)
	g_core_fp.fp_resend_cmd_func = himax_mcu_resend_cmd_func;
#endif
/* CORE_DRIVER */
}

int himax_mcu_on_cmd_struct_init(void)
{
	I("%s: Entering!\n", __func__);
	himax_mcu_cmd_struct_free = himax_mcu_on_cmd_struct_free;

	g_on_core_cmd_op =
		kzalloc(sizeof(struct himax_on_core_command_operation),
		GFP_KERNEL);
	if (g_on_core_cmd_op == NULL)
		goto err_g_on_core_cmd_op_fail;

	g_on_core_cmd_op->ic_op = kzalloc(sizeof(struct on_ic_operation),
			GFP_KERNEL);
	if (g_on_core_cmd_op->ic_op == NULL)
		goto err_g_on_core_cmd_op_ic_op_fail;

	g_on_core_cmd_op->fw_op = kzalloc(sizeof(struct on_fw_operation),
			GFP_KERNEL);
	if (g_on_core_cmd_op->fw_op == NULL)
		goto err_g_on_core_cmd_op_fw_op_fail;

	g_on_core_cmd_op->flash_op = kzalloc(sizeof(struct on_flash_operation),
			GFP_KERNEL);
	if (g_on_core_cmd_op->flash_op == NULL)
		goto err_g_on_core_cmd_op_flash_op_fail;

	g_on_core_cmd_op->sram_op = kzalloc(sizeof(struct on_sram_operation),
			GFP_KERNEL);
	if (g_on_core_cmd_op->sram_op == NULL)
		goto err_g_on_core_cmd_op_sram_op_fail;

	g_on_core_cmd_op->driver_op =
		kzalloc(sizeof(struct on_driver_operation),
		GFP_KERNEL);
	if (g_on_core_cmd_op->driver_op == NULL)
		goto err_g_on_core_cmd_op_driver_op_fail;

	on_pic_op = g_on_core_cmd_op->ic_op;
	on_pfw_op = g_on_core_cmd_op->fw_op;
	on_pflash_op = g_on_core_cmd_op->flash_op;
	on_psram_op = g_on_core_cmd_op->sram_op;
	on_pdriver_op = g_on_core_cmd_op->driver_op;

	himax_mcu_fp_init();

	return NO_ERR;

err_g_on_core_cmd_op_driver_op_fail:
	kfree(g_on_core_cmd_op->sram_op);
	g_on_core_cmd_op->sram_op = NULL;
err_g_on_core_cmd_op_sram_op_fail:
	kfree(g_on_core_cmd_op->flash_op);
	g_on_core_cmd_op->flash_op = NULL;
err_g_on_core_cmd_op_flash_op_fail:
	kfree(g_on_core_cmd_op->fw_op);
	g_on_core_cmd_op->fw_op = NULL;
err_g_on_core_cmd_op_fw_op_fail:
	kfree(g_on_core_cmd_op->ic_op);
	g_on_core_cmd_op->ic_op = NULL;
err_g_on_core_cmd_op_ic_op_fail:
	kfree(g_on_core_cmd_op);
	g_on_core_cmd_op = NULL;
err_g_on_core_cmd_op_fail:

	E("%s: Allocate memory FAIL!!!\n", __func__);

	return -ENOMEM;
}
EXPORT_SYMBOL(himax_mcu_on_cmd_struct_init);

void himax_mcu_on_cmd_struct_free(void)
{
	on_pic_op = NULL;
	on_pfw_op = NULL;
	on_pflash_op = NULL;
	on_psram_op = NULL;
	on_pdriver_op = NULL;
	kfree(g_on_core_cmd_op->driver_op);
	g_on_core_cmd_op->driver_op = NULL;
	kfree(g_on_core_cmd_op->sram_op);
	g_on_core_cmd_op->sram_op = NULL;
	kfree(g_on_core_cmd_op->flash_op);
	g_on_core_cmd_op->flash_op = NULL;
	kfree(g_on_core_cmd_op->fw_op);
	g_on_core_cmd_op->fw_op = NULL;
	kfree(g_on_core_cmd_op->ic_op);
	g_on_core_cmd_op->ic_op = NULL;
	kfree(g_on_core_cmd_op);
	g_on_core_cmd_op = NULL;

	I("%s: release completed\n", __func__);
}

void himax_mcu_on_cmd_init(void)
{
	I("%s: Entering!\n", __func__);
/* CORE_IC */
	himax_parse_assign_cmd(on_ic_adr_ahb_addr_byte_0,
		on_pic_op->addr_ahb_addr_byte_0,
		sizeof(on_pic_op->addr_ahb_addr_byte_0));
	himax_parse_assign_cmd(on_ic_adr_ahb_rdata_byte_0,
		on_pic_op->addr_ahb_rdata_byte_0,
		sizeof(on_pic_op->addr_ahb_rdata_byte_0));
	himax_parse_assign_cmd(on_ic_adr_ahb_access_direction,
		on_pic_op->addr_ahb_access_direction,
		sizeof(on_pic_op->addr_ahb_access_direction));
	himax_parse_assign_cmd(on_ic_adr_conti,
		on_pic_op->addr_conti,
		sizeof(on_pic_op->addr_conti));
	himax_parse_assign_cmd(on_ic_adr_incr4,
		on_pic_op->addr_incr4,
		sizeof(on_pic_op->addr_incr4));
	himax_parse_assign_cmd(on_ic_cmd_ahb_access_direction_read,
		on_pic_op->data_ahb_access_direction_read,
		sizeof(on_pic_op->data_ahb_access_direction_read));
	himax_parse_assign_cmd(on_ic_cmd_conti,
		on_pic_op->data_conti,
		sizeof(on_pic_op->data_conti));
	himax_parse_assign_cmd(on_ic_cmd_incr4,
		on_pic_op->data_incr4,
		sizeof(on_pic_op->data_incr4));
	himax_parse_assign_cmd(on_ic_adr_mcu_ctrl,
		on_pic_op->adr_mcu_ctrl,
		sizeof(on_pic_op->adr_mcu_ctrl));
	himax_parse_assign_cmd(on_ic_cmd_mcu_on,
		on_pic_op->cmd_mcu_on,
		sizeof(on_pic_op->cmd_mcu_on));
	himax_parse_assign_cmd(on_ic_cmd_mcu_off,
		on_pic_op->cmd_mcu_off,
		sizeof(on_pic_op->cmd_mcu_off));
	himax_parse_assign_cmd(on_ic_adr_sleep_ctrl,
		on_pic_op->adr_sleep_ctrl,
		sizeof(on_pic_op->adr_sleep_ctrl));
	himax_parse_assign_cmd(on_ic_cmd_sleep_in,
		on_pic_op->cmd_sleep_in,
		sizeof(on_pic_op->cmd_sleep_in));
	himax_parse_assign_cmd(on_ic_adr_tcon_ctrl,
		on_pic_op->adr_tcon_ctrl,
		sizeof(on_pic_op->adr_tcon_ctrl));
	himax_parse_assign_cmd(on_ic_cmd_tcon_on,
		on_pic_op->cmd_tcon_on,
		sizeof(on_pic_op->cmd_tcon_on));
	himax_parse_assign_cmd(on_ic_adr_wdg_ctrl,
		on_pic_op->adr_wdg_ctrl,
		sizeof(on_pic_op->adr_wdg_ctrl));
	himax_parse_assign_cmd(on_ic_cmd_wdg_psw,
		on_pic_op->cmd_wdg_psw,
		sizeof(on_pic_op->cmd_wdg_psw));
	himax_parse_assign_cmd(on_ic_adr_wdg_cnt_ctrl,
		on_pic_op->adr_wdg_cnt_ctrl,
		sizeof(on_pic_op->adr_wdg_cnt_ctrl));
	himax_parse_assign_cmd(on_ic_cmd_wdg_cnt_clr,
		on_pic_op->cmd_wdg_cnt_clr,
		sizeof(on_pic_op->cmd_wdg_cnt_clr));
/* CORE_IC */
/* CORE_FW */
	himax_parse_assign_cmd(on_fw_addr_smwp_enable,
		on_pfw_op->addr_smwp_enable,
		sizeof(on_pfw_op->addr_smwp_enable));
	himax_parse_assign_cmd(on_fw_addr_program_reload_from,
		on_pfw_op->addr_program_reload_from,
		sizeof(on_pfw_op->addr_program_reload_from));
	himax_parse_assign_cmd(on_fw_addr_raw_out_sel,
		on_pfw_op->addr_raw_out_sel,
		sizeof(on_pfw_op->addr_raw_out_sel));
	himax_parse_assign_cmd(on_fw_addr_flash_checksum,
		on_pfw_op->addr_flash_checksum,
		sizeof(on_pfw_op->addr_flash_checksum));
	himax_parse_assign_cmd(on_fw_data_flash_checksum,
		on_pfw_op->data_flash_checksum,
		sizeof(on_pfw_op->data_flash_checksum));
	himax_parse_assign_cmd(on_fw_addr_crc_value,
		on_pfw_op->addr_crc_value,
		sizeof(on_pfw_op->addr_crc_value));
	himax_parse_assign_cmd(on_fw_addr_fw_mode_status,
		on_pfw_op->addr_fw_mode_status,
		sizeof(on_pfw_op->addr_fw_mode_status));
	himax_parse_assign_cmd(on_fw_addr_icid_addr,
		on_pfw_op->addr_icid_addr,
		sizeof(on_pfw_op->addr_icid_addr));
	himax_parse_assign_cmd(on_fw_addr_fw_ver_start,
		on_pfw_op->addr_fw_ver_start,
		sizeof(on_pfw_op->addr_fw_ver_start));
	himax_parse_assign_cmd(on_fw_data_safe_mode_release_pw_active,
		on_pfw_op->data_safe_mode_release_pw_active,
		sizeof(on_pfw_op->data_safe_mode_release_pw_active));
	himax_parse_assign_cmd(on_fw_addr_criteria_addr,
		on_pfw_op->addr_criteria_addr,
		sizeof(on_pfw_op->addr_criteria_addr));
	himax_parse_assign_cmd(on_fw_data_selftest_pass,
		on_pfw_op->data_selftest_pass,
		sizeof(on_pfw_op->data_selftest_pass));
	himax_parse_assign_cmd(on_fw_addr_reK_crtl,
		on_pfw_op->addr_reK_crtl,
		sizeof(on_pfw_op->addr_reK_crtl));
	himax_parse_assign_cmd(on_fw_data_reK_en,
		on_pfw_op->data_reK_en,
		sizeof(on_pfw_op->data_reK_en));
	himax_parse_assign_cmd(on_fw_data_reK_dis,
		on_pfw_op->data_reK_dis,
		sizeof(on_pfw_op->data_reK_dis));
	himax_parse_assign_cmd(on_fw_data_rst_init,
		on_pfw_op->data_rst_init,
		sizeof(on_pfw_op->data_rst_init));
	himax_parse_assign_cmd(on_fw_data_dc_set,
		on_pfw_op->data_dc_set,
		sizeof(on_pfw_op->data_dc_set));
	himax_parse_assign_cmd(on_fw_data_bank_set,
		on_pfw_op->data_bank_set,
		sizeof(on_pfw_op->data_bank_set));
	himax_parse_assign_cmd(on_fw_addr_selftest_addr_en,
		on_pfw_op->addr_selftest_addr_en,
		sizeof(on_pfw_op->addr_selftest_addr_en));
	himax_parse_assign_cmd(on_fw_addr_selftest_result_addr,
		on_pfw_op->addr_selftest_result_addr,
		sizeof(on_pfw_op->addr_selftest_result_addr));
	himax_parse_assign_cmd(on_fw_data_selftest_request,
		on_pfw_op->data_selftest_request,
		sizeof(on_pfw_op->data_selftest_request));
	himax_parse_assign_cmd(on_fw_data_thx_avg_mul_dc_lsb,
		on_pfw_op->data_thx_avg_mul_dc_lsb,
		sizeof(on_pfw_op->data_thx_avg_mul_dc_lsb));
	himax_parse_assign_cmd(on_fw_data_thx_avg_mul_dc_msb,
		on_pfw_op->data_thx_avg_mul_dc_msb,
		sizeof(on_pfw_op->data_thx_avg_mul_dc_msb));
	himax_parse_assign_cmd(on_fw_data_thx_mul_dc_up_low_bud,
		on_pfw_op->data_thx_mul_dc_up_low_bud,
		sizeof(on_pfw_op->data_thx_mul_dc_up_low_bud));
	himax_parse_assign_cmd(on_fw_data_thx_avg_slf_dc_lsb,
		on_pfw_op->data_thx_avg_slf_dc_lsb,
		sizeof(on_pfw_op->data_thx_avg_slf_dc_lsb));
	himax_parse_assign_cmd(on_fw_data_thx_avg_slf_dc_msb,
		on_pfw_op->data_thx_avg_slf_dc_msb,
		sizeof(on_pfw_op->data_thx_avg_slf_dc_msb));
	himax_parse_assign_cmd(on_fw_data_thx_slf_dc_up_low_bud,
		on_pfw_op->data_thx_slf_dc_up_low_bud,
		sizeof(on_pfw_op->data_thx_slf_dc_up_low_bud));
	himax_parse_assign_cmd(on_fw_data_thx_slf_bank_up,
		on_pfw_op->data_thx_slf_bank_up,
		sizeof(on_pfw_op->data_thx_slf_bank_up));
	himax_parse_assign_cmd(on_fw_data_thx_slf_bank_low,
		on_pfw_op->data_thx_slf_bank_low,
		sizeof(on_pfw_op->data_thx_slf_bank_low));
	himax_parse_assign_cmd(on_fw_data_idle_dis_pwd,
		on_pfw_op->data_idle_dis_pwd,
		sizeof(on_pfw_op->data_idle_dis_pwd));
	himax_parse_assign_cmd(on_fw_data_idle_en_pwd,
		on_pfw_op->data_idle_en_pwd,
		sizeof(on_pfw_op->data_idle_en_pwd));
	himax_parse_assign_cmd(on_fw_data_rawdata_ready_hb,
		on_pfw_op->data_rawdata_ready_hb,
		sizeof(on_pfw_op->data_rawdata_ready_hb));
	himax_parse_assign_cmd(on_fw_data_rawdata_ready_lb,
		on_pfw_op->data_rawdata_ready_lb,
		sizeof(on_pfw_op->data_rawdata_ready_lb));
	himax_parse_assign_cmd(on_fw_addr_ahb_addr,
		on_pfw_op->addr_ahb_addr,
		sizeof(on_pfw_op->addr_ahb_addr));
	himax_parse_assign_cmd(on_fw_data_ahb_dis,
		on_pfw_op->data_ahb_dis,
		sizeof(on_pfw_op->data_ahb_dis));
	himax_parse_assign_cmd(on_fw_data_ahb_en,
		on_pfw_op->data_ahb_en,
		sizeof(on_pfw_op->data_ahb_en));
	himax_parse_assign_cmd(on_fw_addr_event_addr,
		on_pfw_op->addr_event_addr,
		sizeof(on_pfw_op->addr_event_addr));
	himax_parse_assign_cmd(on_fw_usb_detect_addr,
		on_pfw_op->addr_usb_detect,
		sizeof(on_pfw_op->addr_usb_detect));
/* CORE_FW */
/* CORE_FLASH */
	himax_parse_assign_cmd(on_flash_addr_ctrl_base,
		on_pflash_op->addr_ctrl_base,
		sizeof(on_pflash_op->addr_ctrl_base));
	himax_parse_assign_cmd(on_flash_addr_ctrl_auto,
		on_pflash_op->addr_ctrl_auto,
		sizeof(on_pflash_op->addr_ctrl_auto));
	himax_parse_assign_cmd(on_flash_data_main_erase,
		on_pflash_op->data_main_erase,
		sizeof(on_pflash_op->data_main_erase));
	himax_parse_assign_cmd(on_flash_data_auto,
		on_pflash_op->data_auto,
		sizeof(on_pflash_op->data_auto));
	himax_parse_assign_cmd(on_flash_data_main_read,
		on_pflash_op->data_main_read,
		sizeof(on_pflash_op->data_main_read));
	himax_parse_assign_cmd(on_flash_data_page_write,
		on_pflash_op->data_page_write,
		sizeof(on_pflash_op->data_page_write));
	himax_parse_assign_cmd(on_flash_data_sfr_read,
		on_pflash_op->data_sfr_read,
		sizeof(on_pflash_op->data_sfr_read));
	himax_parse_assign_cmd(on_flash_data_spp_read,
		on_pflash_op->data_spp_read,
		sizeof(on_pflash_op->data_spp_read));
	himax_parse_assign_cmd(on_flash_addr_ahb_ctrl,
		on_pflash_op->addr_ahb_ctrl,
		sizeof(on_pflash_op->addr_ahb_ctrl));
	himax_parse_assign_cmd(on_flash_data_ahb_squit,
		on_pflash_op->data_ahb_squit,
		sizeof(on_pflash_op->data_ahb_squit));
	himax_parse_assign_cmd(on_flash_addr_unlock_0,
		on_pflash_op->addr_unlock_0,
		sizeof(on_pflash_op->addr_unlock_0));
	himax_parse_assign_cmd(on_flash_addr_unlock_4,
		on_pflash_op->addr_unlock_4,
		sizeof(on_pflash_op->addr_unlock_4));
	himax_parse_assign_cmd(on_flash_addr_unlock_8,
		on_pflash_op->addr_unlock_8,
		sizeof(on_pflash_op->addr_unlock_8));
	himax_parse_assign_cmd(on_flash_addr_unlock_c,
		on_pflash_op->addr_unlock_c,
		sizeof(on_pflash_op->addr_unlock_c));
	himax_parse_assign_cmd(on_flash_data_cmd0,
		on_pflash_op->data_cmd0,
		sizeof(on_pflash_op->data_cmd0));
	himax_parse_assign_cmd(on_flash_data_cmd1,
		on_pflash_op->data_cmd1,
		sizeof(on_pflash_op->data_cmd1));
	himax_parse_assign_cmd(on_flash_data_cmd2,
		on_pflash_op->data_cmd2,
		sizeof(on_pflash_op->data_cmd2));
	himax_parse_assign_cmd(on_flash_data_cmd3,
		on_pflash_op->data_cmd3,
		sizeof(on_pflash_op->data_cmd3));
	himax_parse_assign_cmd(on_flash_data_lock,
		on_pflash_op->data_lock,
		sizeof(on_pflash_op->data_lock));
/* CORE_FLASH */
/* CORE_SRAM */
	/* sram start*/
	himax_parse_assign_cmd(on_sram_adr_rawdata_addr,
		on_psram_op->addr_rawdata_addr,
		sizeof(on_psram_op->addr_rawdata_addr));
	himax_parse_assign_cmd(on_sram_adr_rawdata_end,
		on_psram_op->addr_rawdata_end,
		sizeof(on_psram_op->addr_rawdata_end));
	himax_parse_assign_cmd(on_sram_cmd_conti,
		on_psram_op->data_conti,
		sizeof(on_psram_op->data_conti));
	himax_parse_assign_cmd(on_sram_cmd_fin,
		on_psram_op->data_fin,
		sizeof(on_psram_op->data_fin));
	himax_parse_assign_cmd(on_sram_passwrd_start,
		on_psram_op->passwrd_start,
		sizeof(on_psram_op->passwrd_start));
	himax_parse_assign_cmd(on_sram_passwrd_end,
		on_psram_op->passwrd_end,
		sizeof(on_psram_op->passwrd_end));
	/* sram end*/
/* CORE_SRAM */
/* CORE_DRIVER */
	himax_parse_assign_cmd(on_driver_addr_fw_define_int_is_edge,
		on_pdriver_op->addr_fw_define_int_is_edge,
		sizeof(on_pdriver_op->addr_fw_define_int_is_edge));
	himax_parse_assign_cmd(on_driver_addr_fw_rx_tx_maxpt_num,
		on_pdriver_op->addr_fw_rx_tx_maxpt_num,
		sizeof(on_pdriver_op->addr_fw_rx_tx_maxpt_num));
#if defined(HX_NEW_EVENT_STACK_FORMAT)
	himax_parse_assign_cmd(on_driver_addr_fw_maxpt_bt_num,
		on_pdriver_op->addr_fw_maxpt_bt_num,
		sizeof(on_pdriver_op->addr_fw_rx_tx_maxpt_num));
#endif
	himax_parse_assign_cmd(on_driver_addr_fw_xy_rev_int_edge,
		on_pdriver_op->addr_fw_xy_rev_int_edge,
		sizeof(on_pdriver_op->addr_fw_xy_rev_int_edge));
	himax_parse_assign_cmd(on_driver_addr_fw_define_x_y_res,
		on_pdriver_op->addr_fw_define_x_y_res,
		sizeof(on_pdriver_op->addr_fw_define_x_y_res));
	himax_parse_assign_cmd(on_driver_data_df_rx,
		on_pdriver_op->data_df_rx,
		sizeof(on_pdriver_op->data_df_rx));
	himax_parse_assign_cmd(on_driver_data_df_tx,
		on_pdriver_op->data_df_tx,
		sizeof(on_pdriver_op->data_df_tx));
	himax_parse_assign_cmd(on_driver_data_df_pt,
		on_pdriver_op->data_df_pt,
		sizeof(on_pdriver_op->data_df_pt));
/* CORE_DRIVER */
}
EXPORT_SYMBOL(himax_mcu_on_cmd_init);
/* init end*/
/* CORE_INIT */
