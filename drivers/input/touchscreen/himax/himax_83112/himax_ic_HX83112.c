/*  Himax Android Driver Sample Code for HX83112 chipset

	Copyright (C) 2018 Himax Corporation.

	This software is licensed under the terms of the GNU General Public
	License version 2,  as published by the Free Software Foundation,  and
	may be copied,  distributed,  and modified under those terms.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

*/

#include "himax_ic_HX83112.h"
extern struct himax_ts_data *g_ts;
extern struct himax_core_fp g_core_fp;
extern struct ic_operation *pic_op;
extern struct fw_operation *pfw_op;
#ifdef HX_ZERO_FLASH
extern struct zf_operation *pzf_op;
#endif
#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
#if defined(HX_EN_DYNAMIC_NAME)
/*	extern char *i_CTPM_firmware_name;*/
extern struct himax_ic_data *ic_data;
#endif
#endif
extern struct himax_chip_detect *g_core_chip_dt;

extern uint8_t g_hx_ic_dt_num;
extern int i2c_error_count;
extern unsigned char IC_TYPE;
extern unsigned char IC_CHECKSUM;
extern bool DSRAM_Flag;

extern unsigned long FW_VER_MAJ_FLASH_ADDR;
extern unsigned long FW_VER_MIN_FLASH_ADDR;
extern unsigned long CFG_VER_MAJ_FLASH_ADDR;
extern unsigned long CFG_VER_MIN_FLASH_ADDR;
extern unsigned long CID_VER_MAJ_FLASH_ADDR;
extern unsigned long CID_VER_MIN_FLASH_ADDR;

extern unsigned long FW_VER_MAJ_FLASH_LENG;
extern unsigned long FW_VER_MIN_FLASH_LENG;
extern unsigned long CFG_VER_MAJ_FLASH_LENG;
extern unsigned long CFG_VER_MIN_FLASH_LENG;
extern unsigned long CID_VER_MAJ_FLASH_LENG;
extern unsigned long CID_VER_MIN_FLASH_LENG;

#ifdef HX_AUTO_UPDATE_FW
extern int g_i_FW_VER;
extern int g_i_CFG_VER;
extern int g_i_CID_MAJ;
extern int g_i_CID_MIN;
extern unsigned char *i_CTPM_FW;
#endif

#ifdef HX_TP_PROC_2T2R
extern bool Is_2T2R;
#endif

#ifdef HX_USB_DETECT_GLOBAL
extern void himax_cable_detect_func(bool force_renew);
#endif

static void hx83112_chip_init(void)
{

	g_ts->chip_cell_type = CHIP_IS_IN_CELL;
	input_info(true, g_ts->dev, "%s: IC cell type = %d\n", __func__, g_ts->chip_cell_type);
	IC_CHECKSUM = HX_TP_BIN_CHECKSUM_CRC;
/*Himax: Set FW and CFG Flash Address*/
	FW_VER_MAJ_FLASH_ADDR = 49157;	/*0x00C005 */
	FW_VER_MAJ_FLASH_LENG = 1;
	FW_VER_MIN_FLASH_ADDR = 49158;	/*0x00C006 */
	FW_VER_MIN_FLASH_LENG = 1;
	CFG_VER_MAJ_FLASH_ADDR = 49408;	/*0x00C100 */
	CFG_VER_MAJ_FLASH_LENG = 1;
	CFG_VER_MIN_FLASH_ADDR = 49409;	/*0x00C101 */
	CFG_VER_MIN_FLASH_LENG = 1;
	CID_VER_MAJ_FLASH_ADDR = 49154;	/*0x00C002 */
	CID_VER_MAJ_FLASH_LENG = 1;
	CID_VER_MIN_FLASH_ADDR = 49155;	/*0x00C003 */
	CID_VER_MIN_FLASH_LENG = 1;
}

static bool hx83112_sense_off(bool check_en)
{
	uint8_t cnt = 0;
	uint8_t tmp_data[DATA_LEN_4];

	do {
		if (cnt == 0 || (tmp_data[0] != 0xA5
			&& tmp_data[0] != 0x00 && tmp_data[0] != 0x87)) {
			g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr,
						DATA_LEN_4, pfw_op->data_fw_stop, 0);
		}
		msleep(20);

/* check fw status */
		g_core_fp.fp_register_read(pic_op->addr_cs_central_state, ADDR_LEN_4, tmp_data, 0);

		if (tmp_data[0] != 0x05) {
			input_info(true, g_ts->dev, "%s %s: Do not need wait FW, Status = 0x%02X!\n",
					HIMAX_LOG_TAG, __func__, tmp_data[0]);
			break;
		}

		g_core_fp.fp_register_read(pfw_op->addr_ctrl_fw_isr, 4,
						tmp_data, false);
		input_info(true, g_ts->dev, "%s %s: cnt = %d, data[0] = 0x%02X!\n",
				HIMAX_LOG_TAG, __func__, cnt, tmp_data[0]);
	} while (tmp_data[0] != 0x87 && (++cnt < 10) && check_en == true);

	cnt = 0;

	do {
		/*===========================================
		I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		===========================================*/
		tmp_data[0] = pic_op->data_i2c_psw_lb[0];

		if (himax_bus_write
			(pic_op->adr_i2c_psw_lb[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_err(true, g_ts->dev, "%s %s: i2c access fail!\n", HIMAX_LOG_TAG, __func__);
			return false;
		}

		/*===========================================
		I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		===========================================*/
		tmp_data[0] = pic_op->data_i2c_psw_ub[0];

		if (himax_bus_write(pic_op->adr_i2c_psw_ub[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_err(true, g_ts->dev, "%s %s: i2c access fail!\n", HIMAX_LOG_TAG, __func__);
			return false;
		}

		/*===========================================
		I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x00
		===========================================*/
		tmp_data[0] = 0x00;

		if (himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_err(true, g_ts->dev, "%s %s: i2c access fail!\n", HIMAX_LOG_TAG, __func__);
			return false;
		}

		/*===========================================
		I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		===========================================*/
		tmp_data[0] = pic_op->data_i2c_psw_lb[0];

		if (himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_err(true, g_ts->dev, "%s %s: i2c access fail!\n", HIMAX_LOG_TAG, __func__);
			return false;
		}

		/*===========================================
		I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		===========================================*/
		tmp_data[0] = pic_op->data_i2c_psw_ub[0];

		if (himax_bus_write(pic_op->adr_i2c_psw_ub[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES) < 0) {
			input_err(true, g_ts->dev, "%s %s: i2c access fail!\n", HIMAX_LOG_TAG, __func__);
			return false;
		}

		/* ======================
		Check enter_save_mode
		======================*/
		g_core_fp.fp_register_read(pic_op->addr_cs_central_state,
						ADDR_LEN_4, tmp_data, 0);
		input_info(true, g_ts->dev, "%s %s: Check enter_save_mode data[0]=%X \n",
				HIMAX_LOG_TAG, __func__, tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
		/*=====================================
		Reset TCON
		=====================================*/
			g_core_fp.fp_register_write(pic_op->addr_tcon_on_rst,
							DATA_LEN_4,
							pic_op->data_rst, 0);
			msleep(1);
			tmp_data[3] = pic_op->data_rst[3];
			tmp_data[2] = pic_op->data_rst[2];
			tmp_data[1] = pic_op->data_rst[1];
			tmp_data[0] = pic_op->data_rst[0] | 0x01;
			g_core_fp.fp_register_write(pic_op->addr_tcon_on_rst,
							DATA_LEN_4, tmp_data, 0);
		/*=====================================
		Reset ADC
		=====================================*/
			g_core_fp.fp_register_write(pic_op->addr_adc_on_rst,
							DATA_LEN_4,
							pic_op->data_rst, 0);
			msleep(1);
			tmp_data[3] = pic_op->data_rst[3];
			tmp_data[2] = pic_op->data_rst[2];
			tmp_data[1] = pic_op->data_rst[1];
			tmp_data[0] = pic_op->data_rst[0] | 0x01;
			g_core_fp.fp_register_write(pic_op->addr_adc_on_rst,
							DATA_LEN_4, tmp_data, 0);
			return true;
		} else {
			msleep(10);
#ifdef HX_RST_PIN_FUNC
			g_core_fp.fp_ic_reset(false, false);
#endif
		}
	} while (cnt++ < 15);

	return false;
}

#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
#if defined(HX_EN_DYNAMIC_NAME)
static void hx83112a_read_ic_ver(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t tmp_addr[DATA_LEN_4];

	/* Enable read DD */
	himax_in_parse_assign_cmd(hx83112_ic_osc_en, tmp_addr,
					sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
	tmp_data[0] = 0x01;
	g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, false);

	himax_in_parse_assign_cmd(hx83112_ic_osc_pw, tmp_addr,
					sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
	tmp_data[0] = 0xA5;
	g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, false);

	tmp_data[0] = 0x00;
	tmp_data[1] = 0x83;
	tmp_data[2] = 0x11;
	tmp_data[3] = 0x2A;
	himax_in_parse_assign_cmd(hx83112_ic_b9_en, tmp_addr, sizeof(tmp_addr));
	g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, false);

	tmp_data[0] = 0x00;
	tmp_data[1] = 0x55;
	tmp_data[2] = 0xAA;
	tmp_data[3] = 0x00;
	himax_in_parse_assign_cmd(hx83112_ic_eb_en, tmp_addr, sizeof(tmp_addr));
	g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, false);

	/* Read DD data */
	himax_in_parse_assign_cmd(hx83112_cb_ic_fw, tmp_addr, sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
	input_info(true, g_ts->dev, "%s %s: CB=%X\n",
			HIMAX_LOG_TAG, __func__, tmp_data[0]);
	ic_data->vendor_old_ic_ver = tmp_data[0];

	himax_in_parse_assign_cmd(hx83112_e8_ic_fw, tmp_addr, sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
	input_info(true, g_ts->dev, "%s %s: E8=%X\n",
			HIMAX_LOG_TAG, __func__, tmp_data[0]);
	ic_data->vendor_ic_ver = tmp_data[0];

	/* Disable read DD */
	himax_in_parse_assign_cmd(hx83112_ic_osc_en, tmp_addr,
					sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
	tmp_data[0] = 0x00;
	g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, false);
}

static void hx83112b_read_ic_ver(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t tmp_addr[DATA_LEN_4];

	/* Enable read DD */
	himax_in_parse_assign_cmd(hx83112_ic_osc_pw, tmp_addr,
					sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
	tmp_data[0] = 0xA5;
	g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, false);

	tmp_data[0] = 0x00;
	tmp_data[1] = 0x83;
	tmp_data[2] = 0x11;
	tmp_data[3] = 0x2B;
	himax_in_parse_assign_cmd(hx83112_ic_b9_en, tmp_addr, sizeof(tmp_addr));
	g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, false);

	/* Read DD data */
	himax_in_parse_assign_cmd(hx83112_cb_ic_fw, tmp_addr, sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
	input_info(true, g_ts->dev, "%s %s: CB=%X\n",
			HIMAX_LOG_TAG, __func__, tmp_data[0]);
	ic_data->vendor_old_ic_ver = tmp_data[0];

	himax_in_parse_assign_cmd(hx83112_e8_ic_fw, tmp_addr, sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
	input_info(true, g_ts->dev, "%s %s: E8=%X\n",
			HIMAX_LOG_TAG, __func__, tmp_data[0]);
	ic_data->vendor_ic_ver = tmp_data[0];
}

static void hx83112e_read_ic_ver(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t tmp_addr[DATA_LEN_4];

	/* Enable read DD */
	himax_in_parse_assign_cmd(hx83112_ic_osc_pw, tmp_addr,
					sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
	tmp_data[0] = 0xA5;
	g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, false);

	tmp_data[0] = 0x00;
	tmp_data[1] = 0x83;
	tmp_data[2] = 0x11;
	tmp_data[3] = 0x2E;
	himax_in_parse_assign_cmd(hx83112_ic_b9_en, tmp_addr, sizeof(tmp_addr));
	g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, false);

	/* Read DD data */
	himax_in_parse_assign_cmd(hx83112_cb_ic_fw, tmp_addr, sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
	input_info(true, g_ts->dev, "%s %s: CB=%X\n",
			HIMAX_LOG_TAG, __func__, tmp_data[0]);
	ic_data->vendor_old_ic_ver = tmp_data[0];

	himax_in_parse_assign_cmd(hx83112_e8_ic_fw, tmp_addr, sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
	input_info(true, g_ts->dev, "%s %s: E8=%X\n",
			HIMAX_LOG_TAG, __func__, tmp_data[0]);
	ic_data->vendor_ic_ver = tmp_data[0];
}

static void hx83112_dynamic_fw_name(uint8_t ic_name)
{
	char firmware_name[64];

	if (ic_name == 0x2a) {
		hx83112a_read_ic_ver();
	} else if (ic_name == 0x2b) {
		hx83112b_read_ic_ver();
	} else {
		hx83112e_read_ic_ver();
	}

	if (g_ts->pdata->i_CTPM_firmware_name != NULL) {
		kfree(g_ts->pdata->i_CTPM_firmware_name);
		g_ts->pdata->i_CTPM_firmware_name = NULL;
	}
	memset(firmware_name, 0x00, sizeof(firmware_name));

	if ((ic_data->vendor_ic_ver == 0x13) || (ic_data->vendor_ic_ver == 0x14)
		|| (ic_data->vendor_ic_ver == 0x15)
		|| (ic_data->vendor_ic_ver == 0x16)
		|| (ic_data->vendor_ic_ver == 0x23)) {
		ic_data->vendor_semifac = 2;
	} else if ((ic_data->vendor_old_ic_ver == 0x44)
			|| (ic_data->vendor_old_ic_ver == 0x77)
			|| (ic_data->vendor_ic_ver == 0x03)
			|| (ic_data->vendor_ic_ver == 0x04)) {
		ic_data->vendor_semifac = 1;
	} else {
		ic_data->vendor_semifac = 0;
	}
	memcpy(firmware_name, "Himax_firmware.bin",
			sizeof(char) * strlen("Himax_firmware.bin"));

	g_ts->pdata->i_CTPM_firmware_name =
		kzalloc((sizeof(char) * (strlen(firmware_name) + 1)), GFP_KERNEL);
	if (g_ts->pdata->i_CTPM_firmware_name != NULL) {
		memcpy(g_ts->pdata->i_CTPM_firmware_name, firmware_name,
				(sizeof(char) * (strlen(firmware_name) + 1)));
	}
	input_info(true, g_ts->dev,
			"%s %s: i_CTPM_firmware_name : %s \n", HIMAX_LOG_TAG,
			__func__, g_ts->pdata->i_CTPM_firmware_name);
}
#endif
#endif

#if defined(HX_ZERO_FLASH)
static void himax_hx83112f_reload_to_active(void)
{
	uint8_t addr[DATA_LEN_4] = {0};
	uint8_t data[DATA_LEN_4] = {0};
	uint8_t retry_cnt = 0;

	addr[3] = 0x90;
	addr[2] = 0x00;
	addr[1] = 0x00;
	addr[0] = 0x48;

	do {
		data[3] = 0x00;
		data[2] = 0x00;
		data[1] = 0x00;
		data[0] = 0xEC;
		g_core_fp.fp_register_write(addr, DATA_LEN_4, data, 0);
		usleep_range(1000, 1100);
		g_core_fp.fp_register_read(addr, DATA_LEN_4, data, 0);
		I("%s: data[1]=%d, data[0]=%d, retry_cnt=%d\n", __func__,
				data[1], data[0], retry_cnt);
		retry_cnt++;
	} while ((data[1] != 0x01
		|| data[0] != 0xEC)
		&& retry_cnt < HIMAX_REG_RETRY_TIMES);
}

static void himax_hx83112f_resume_ic_action(void)
{
#if !defined(HX_RESUME_HW_RESET)
	himax_hx83112f_reload_to_active();
#endif
}

static void himax_hx83112f_sense_on(uint8_t FlashMode)
{
	uint8_t tmp_data[DATA_LEN_4];
	int retry = 0;
	int ret = 0;

	I("Enter %s\n", __func__);

	g_core_fp.fp_interface_on();
	g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr,
		sizeof(pfw_op->data_clear), pfw_op->data_clear, 0);
	/*msleep(20);*/
	usleep_range(10000, 10001);
	if (!FlashMode) {
#if defined(HX_RST_PIN_FUNC)
		g_core_fp.fp_ic_reset(false, false);
#else
		g_core_fp.fp_system_reset();
#endif
		himax_hx83112f_reload_to_active();
	} else {
		himax_hx83112f_reload_to_active();
		do {
			g_core_fp.fp_register_write(
				pfw_op->addr_safe_mode_release_pw,
				sizeof(pfw_op->
				data_safe_mode_release_pw_active),
				pfw_op->data_safe_mode_release_pw_active,
				0);

			g_core_fp.fp_register_read(
				pfw_op->addr_flag_reset_event,
				DATA_LEN_4, tmp_data, 0);
			I("%s:Read status from IC = %X,%X\n", __func__,
					tmp_data[0], tmp_data[1]);
		} while ((tmp_data[1] != 0x01
			|| tmp_data[0] != 0x00)
			&& retry++ < 5);

		if (retry >= 5) {
			E("%s: Fail:\n", __func__);
#if defined(HX_RST_PIN_FUNC)
			g_core_fp.fp_ic_reset(false, false);
#else
			g_core_fp.fp_system_reset();
#endif
			himax_hx83112f_reload_to_active();
		} else {
			I("%s:OK and Read status from IC = %X,%X\n", __func__,
				tmp_data[0], tmp_data[1]);
			/* reset code*/
			tmp_data[0] = 0x00;

			ret = himax_bus_write(
				pic_op->adr_i2c_psw_lb[0],
				tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
			if (ret < 0)
				E("%s: i2c access fail!\n", __func__);

				ret = himax_bus_write(
					pic_op->adr_i2c_psw_ub[0],
					tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
			if (ret < 0)
				E("%s: i2c access fail!\n", __func__);

			g_core_fp.fp_register_write(
				pfw_op->addr_safe_mode_release_pw,
				sizeof(pfw_op->
				data_safe_mode_release_pw_reset),
				pfw_op->data_safe_mode_release_pw_reset,
				0);
		}
	}
}

#endif

static void hx83112f_func_re_init(void)
{
#if defined(HX_ZERO_FLASH)
	g_core_fp.fp_resume_ic_action = himax_hx83112f_resume_ic_action;
	g_core_fp.fp_sense_on = himax_hx83112f_sense_on;
	g_core_fp.fp_0f_reload_to_active = himax_hx83112f_reload_to_active;
#endif
}

static void hx83112_func_re_init(void)
{
	g_core_fp.fp_sense_off = hx83112_sense_off;
	g_core_fp.fp_chip_init = hx83112_chip_init;
}

static void hx83112_reg_re_init(void)
{
}

static void hx83112f_reg_re_init(void)
{
	pfw_op->addr_raw_out_sel[3] = 0x10;
	pfw_op->addr_raw_out_sel[2] = 0x00;
	pfw_op->addr_raw_out_sel[1] = 0x72;
	pfw_op->addr_raw_out_sel[0] = 0xEC;
//	himax_parse_assign_cmd(hx83112f_fw_addr_raw_out_sel,
//		pfw_op->addr_raw_out_sel,
//		sizeof(pfw_op->addr_raw_out_sel));
}

static bool hx83112_chip_detect(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	bool ret_data = false;
	int ret = 0;
	int i = 0;
	ret = himax_mcu_in_cmd_struct_init();
	if (ret < 0) {
		ret_data = false;
		input_err(true, g_ts->dev, "%s %s:cmd_struct_init Fail:\n", HIMAX_LOG_TAG, __func__);
		return ret_data;
	}
	himax_mcu_in_cmd_init();

	hx83112_reg_re_init();
	hx83112_func_re_init();

	g_core_fp.fp_sense_off(false);

	for (i = 0; i < 5; i++) {
		g_core_fp.fp_register_read(pfw_op->addr_icid_addr, DATA_LEN_4,
						tmp_data, false);
		input_info(true, g_ts->dev, "%s:Read driver IC ID = %X, %X, %X\n",
				__func__, tmp_data[3], tmp_data[2], tmp_data[1]);

		if ((tmp_data[3] == 0x83) && (tmp_data[2] == 0x11)
			&& ((tmp_data[1] == 0x2a) || (tmp_data[1] == 0x2b)
			|| (tmp_data[1] == 0x2e || (tmp_data[1] == 0x2f)))) {
			if (tmp_data[1] == 0x2a) {
				strlcpy(g_ts->chip_name,
					HX_83112A_SERIES_PWON, 30);
			} else if (tmp_data[1] == 0x2b) {
				strlcpy(g_ts->chip_name,
					HX_83112B_SERIES_PWON, 30);
			} else if (tmp_data[1] == 0x2f) {
				strlcpy(g_ts->chip_name,
					HX_83112F_SERIES_PWON, 30);
//				ic_data->ic_adc_num =
//					hx83112f_data_adc_num;
				hx83112f_reg_re_init();
				hx83112f_func_re_init();
			}

			input_info(true, g_ts->dev, "%s:IC name = %s\n",
						__func__, g_ts->chip_name);

			input_info(true, g_ts->dev, "%s: Himax IC package %x%x%x in\n",
						__func__, tmp_data[3], tmp_data[2], tmp_data[1]);
			ret_data = true;
			break;
		} else {
			ret_data = false;
			input_err(true, g_ts->dev,
					"%s %s:Read driver ID register Fail:\n",
					HIMAX_LOG_TAG, __func__);
			input_err(true, g_ts->dev,
					"%s %s: Could NOT find Himax Chipset \n",
					HIMAX_LOG_TAG, __func__);
			input_err(true, g_ts->dev,
					"%s %s Please check 1.VCCD,VCCA,VSP,VSN \n",
					HIMAX_LOG_TAG, __func__);
			input_err(true, g_ts->dev,
					"%s %s 2. LCM_RST,TP_RST \n",
					HIMAX_LOG_TAG, __func__);
			input_err(true, g_ts->dev,
					"%s %s 3. Power On Sequence \n",
					HIMAX_LOG_TAG, __func__);
		}
	}

#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
#if defined(HX_EN_DYNAMIC_NAME)
	hx83112_dynamic_fw_name(tmp_data[1]);
#endif
#endif

	return ret_data;
}

static int himax_hx83112_probe(void)
{
	pr_info("[sec_input] %s:Enter\n", __func__);
	if (g_core_chip_dt == NULL) {
		g_core_chip_dt = kzalloc(sizeof(struct himax_chip_detect) * HX_DRIVER_MAX_IC_NUM, GFP_KERNEL);
		pr_info("[sec_input] %s:1st alloc g_core_chip_dt \n", __func__);
	}

	if (g_hx_ic_dt_num < HX_DRIVER_MAX_IC_NUM) {
		g_core_chip_dt[g_hx_ic_dt_num].fp_chip_detect = hx83112_chip_detect;
		g_hx_ic_dt_num++;
	}

	return 0;
}

static int himax_hx83112_remove(void)
{
	g_core_chip_dt[g_hx_ic_dt_num].fp_chip_detect = NULL;
	g_core_fp.fp_chip_init = NULL;
	return 0;
}

static int __init himax_hx83112_init(void)
{
	int ret = 0;

	pr_info("[sec_input] %s\n", __func__);
	ret = himax_hx83112_probe();
	return 0;
}

static void __exit himax_hx83112_exit(void)
{
	himax_hx83112_remove();
}

module_init(himax_hx83112_init);
module_exit(himax_hx83112_exit);

MODULE_DESCRIPTION("HIMAX HX83112 touch driver");
MODULE_LICENSE("GPL");
