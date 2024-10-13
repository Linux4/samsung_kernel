/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for HX83122 chipset
 *
 *  Copyright (C) 2022 Himax Corporation.
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

#include "himax_ic_HX83122.h"
#include "himax_modular.h"

static void hx83122a_chip_init(void)
{
	private_ts->chip_cell_type = CHIP_IS_IN_CELL;
	I("%s:IC cell type = %d\n", __func__, private_ts->chip_cell_type);
	(IC_CHECKSUM) = HX_TP_BIN_CHECKSUM_CRC;
	/*Himax: Set FW and CFG Flash Address*/

	CID_VER_MAJ_FLASH_ADDR	= 0x21002;  /*0x00C002*/
	CID_VER_MAJ_FLASH_LENG	= 1;
	CID_VER_MIN_FLASH_ADDR	= 0x21003;  /*0x00C003*/
	CID_VER_MIN_FLASH_LENG	= 1;
	PANEL_VERSION_ADDR		= 0x21004;  /*0x00E804*/
	PANEL_VERSION_LENG		= 1;
	FW_VER_MAJ_FLASH_ADDR		= 0x21000;  /*0x00C005*/
	FW_VER_MAJ_FLASH_LENG		= 1;
	FW_VER_MIN_FLASH_ADDR		= 0x21001;  /*0x00C006*/
	FW_VER_MIN_FLASH_LENG		= 1;
	CFG_VER_MAJ_FLASH_ADDR	= 0x21005;  /*0x00C100*/
	CFG_VER_MAJ_FLASH_LENG	= 1;
	CFG_VER_MIN_FLASH_ADDR	= 0x21006;  /*0x00C101*/
	CFG_VER_MIN_FLASH_LENG	= 1;
	ADDR_VER_IC_NAME	= 0x21050;
}

void hx83122_burst_enable(uint8_t auto_add_4_byte)
{
	uint8_t tmp_data[4];

	tmp_data[0] = 0x31;
	if (himax_bus_write(0x13, tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	tmp_data[0] = (0x10 | auto_add_4_byte);
	if (himax_bus_write(0x0D, tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}
}

int hx83122_flash_write_burst(uint8_t *reg_byte, uint8_t *write_data)
{
	uint8_t data_byte[8];
	int i = 0, j = 0;

	for (i = 0; i < 4; i++)
		data_byte[i] = reg_byte[i];
	for (j = 4; j < 8; j++)
		data_byte[j] = write_data[j-4];

	if (himax_bus_write(0x00, data_byte, 8, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return I2C_FAIL;
	}
	return 0;
}

static int hx83122_register_read(uint8_t *read_addr, int read_length, uint8_t *read_data)
{
	uint8_t tmp_data[4];
	int i = 0;
	int address = 0;

	if (read_length > 256) {
		E("%s: read len over 256!\n", __func__);
		return LENGTH_FAIL;
	}
	if (read_length > 4)
		hx83122_burst_enable(1);
	else
		hx83122_burst_enable(0);

	address = (read_addr[3] << 24) + (read_addr[2] << 16) + (read_addr[1] << 8) + read_addr[0];
	i = address;
	tmp_data[0] = (uint8_t)i;
	tmp_data[1] = (uint8_t)(i >> 8);
	tmp_data[2] = (uint8_t)(i >> 16);
	tmp_data[3] = (uint8_t)(i >> 24);
	if (himax_bus_write(0x00, tmp_data, 4, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return I2C_FAIL;
	}
	tmp_data[0] = 0x00;
	if (himax_bus_write(0x0C, tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return I2C_FAIL;
	}

	if (himax_bus_read(0x08, read_data, read_length, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return I2C_FAIL;
	}
	if (read_length > 4)
		hx83122_burst_enable(0);

	return 0;
}

#if defined(HX_RST_PIN_FUNC)
#define RST_LOW_PERIOD_S 5000
#define RST_LOW_PERIOD_E 5100
#if defined(HX_ZERO_FLASH)
#define RST_HIGH_PERIOD_S 5000
#define RST_HIGH_PERIOD_E 5100
#else
#define RST_HIGH_PERIOD_S 50000
#define RST_HIGH_PERIOD_E 50100
#endif
static void hx83122_pin_reset(void)
{
	I("%s: Now reset the Touch chip.\n", __func__);
	himax_rst_gpio_set(private_ts->rst_gpio, 0);
	usleep_range(RST_LOW_PERIOD_S, RST_LOW_PERIOD_E);
	himax_rst_gpio_set(private_ts->rst_gpio, 1);
	usleep_range(RST_HIGH_PERIOD_S, RST_HIGH_PERIOD_E);
}
#endif

static bool hx83122_sense_off(bool check_en)
{
	uint8_t cnt = 0;
	uint8_t tmp_addr[DATA_LEN_4];
	uint8_t tmp_writ[DATA_LEN_4];
	uint8_t tmp_data[DATA_LEN_4];

	do {
		if (cnt == 0 || (tmp_data[0] != 0xA5 && tmp_data[0] != 0x00 && tmp_data[0] != 0x87)) {
			tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x5C;
			tmp_writ[3] = 0x00; tmp_writ[2] = 0x00; tmp_writ[1] = 0x00; tmp_writ[0] = 0xA5;
			hx83122_flash_write_burst(tmp_addr, tmp_writ);
		}
		msleep(20);

		/* check fw status */
		tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0xA8;
		hx83122_register_read(tmp_addr, DATA_LEN_4, tmp_data);

		if (tmp_data[0] != 0x05) {
			I("%s: Do not need wait FW, Status = 0x%02X!\n",
				__func__, tmp_data[0]);
			break;
		}

		tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x5C;
		hx83122_register_read(tmp_addr, DATA_LEN_4, tmp_data);
		I("%s: cnt = %d, data[0] = 0x%02X!\n", __func__,
			cnt, tmp_data[0]);
	} while (tmp_data[0] != 0x87 && (++cnt < 50) && check_en == true);

	cnt = 0;

	do {
		/**
		 *I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 */
		tmp_data[0] = 0x27;
		if (himax_bus_write(0x31, tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/**
		 *I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 */
		tmp_data[0] = 0x95;
		if (himax_bus_write(0x32, tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: bus access fail!\n", __func__);
			return false;
		}

		/**
		 *Check enter_save_mode
		 */
		tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0xA8;
		hx83122_register_read(tmp_addr, ADDR_LEN_4, tmp_data);
		I("%s: Check enter_save_mode data[0]=%X\n",
			__func__, tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
			I("%s: Now had enter safe mode!\n", __func__);
			return true;
		}
		usleep_range(10000, 10001);
#if defined(HX_RST_PIN_FUNC)
		hx83122_pin_reset();
#endif

	} while (cnt++ < 15);

	return false;
}

static void hx83122a_sense_on(uint8_t FlashMode)
{
	uint8_t tmp_data[DATA_LEN_4] = {0};

	I("Enter %s\n", __func__);
	g_core_fp.fp_interface_on();
	g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr,
		sizeof(pfw_op->data_clear), pfw_op->data_clear, 0);
	usleep_range(20000, 21000);
	if (!FlashMode) {
#ifdef HX_RST_PIN_FUNC
		g_core_fp.fp_ic_reset(false, false);
#else
		g_core_fp.fp_system_reset();
#endif
	} else {
		if (himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0)
			E("%s: cmd=%x bus access fail!\n", __func__, pic_op->adr_i2c_psw_lb[0]);

		if (himax_bus_write(pic_op->adr_i2c_psw_ub[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0)
			E("%s: cmd=%x bus access fail!\n", __func__, pic_op->adr_i2c_psw_ub[0]);
	}
}

static bool hx83122a_sense_off(bool check_en)
{
	uint8_t cnt = 0;
	uint8_t tmp_data[DATA_LEN_4];

	I("Enter %s\n", __func__);

	do {
		if (cnt == 0 || (tmp_data[0] != 0xA5 && tmp_data[0] != 0x00 && tmp_data[0] != 0x87))
			g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr, DATA_LEN_4, pfw_op->data_fw_stop, 0);
		/*msleep(20);*/
		usleep_range(10000, 10001);

		/* check fw status */
		g_core_fp.fp_register_read(pic_op->addr_cs_central_state, ADDR_LEN_4, tmp_data, 0);

		if (tmp_data[0] != 0x05) {
			break;
		}

		g_core_fp.fp_register_read(pfw_op->addr_ctrl_fw_isr, 4, tmp_data, false);
		KI("%s: cnt = %d, data[0] = 0x%02X!\n", __func__, cnt, tmp_data[0]);
	} while (tmp_data[0] != 0x87 && (++cnt < 10) && check_en == true);

	cnt = 0;

	do {
		/*===========================================
		 *I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 *===========================================
		 */
		tmp_data[0] = 0x27;
		if (himax_bus_write(0x31, tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: bus access fail!\n", __func__);
			return false;
		}

		/*===========================================
		 *I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 *===========================================
		 */
		tmp_data[0] = 0x95;
		if (himax_bus_write(0x32, tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: bus access fail!\n", __func__);
			return false;
		}

		/* ======================
		 *Check enter_save_mode
		 *======================
		 */
		g_core_fp.fp_register_read(pic_op->addr_cs_central_state, 4, tmp_data, false);
		I("%s: Check enter_save_mode data[0]=%X\n", __func__, tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
			I("%s: Now had enter safe mode!\n", __func__);
			return true;
		}
		/*msleep(10);*/
		usleep_range(5000, 5001);
#ifdef HX_RST_PIN_FUNC
		himax_rst_gpio_set(private_ts->rst_gpio, 0);
		msleep(20);
		himax_rst_gpio_set(private_ts->rst_gpio, 1);
		msleep(50);
#endif

	} while (cnt++ < 5);

	return false;
}

static bool hx83122a_read_event_stack(uint8_t *buf, uint8_t length)
{
	unsigned long long t_start, t_end, t_delta;
	unsigned long nano_start, nano_end, nano_delta;
	int len = length;
	int i2c_speed = 0;

	if (private_ts->debug_log_level & BIT(2)) {
		t_start = local_clock();
		nano_start = do_div(t_start, 1000000000);
	}

	himax_bus_read(pfw_op->addr_event_addr[0], buf, length, HIMAX_I2C_RETRY_TIMES);

	if (private_ts->debug_log_level & BIT(2)) {
		t_end = local_clock();
		nano_end = do_div(t_end, 1000000000);
		t_delta  = t_start - t_end;
		nano_delta = (nano_start - nano_end) / 1000;
		i2c_speed = (len * 9 * 1000000 / (int)nano_delta) * 13 / 10;
		private_ts->bus_speed = (int)i2c_speed;
	}
	return 1;
}

void himax_hx83122a_cmd_init(void)
{
	I("%s: Entering!\n", __func__);
#ifdef CORE_IC
	himax_in_parse_assign_cmd(ic_adr_ahb_addr_byte_0, pic_op->addr_ahb_addr_byte_0, sizeof(pic_op->addr_ahb_addr_byte_0));
	himax_in_parse_assign_cmd(ic_adr_ahb_rdata_byte_0, pic_op->addr_ahb_rdata_byte_0, sizeof(pic_op->addr_ahb_rdata_byte_0));
	himax_in_parse_assign_cmd(ic_adr_ahb_access_direction, pic_op->addr_ahb_access_direction, sizeof(pic_op->addr_ahb_access_direction));
	himax_in_parse_assign_cmd(ic_adr_conti, pic_op->addr_conti, sizeof(pic_op->addr_conti));
	himax_in_parse_assign_cmd(ic_adr_incr4, pic_op->addr_incr4, sizeof(pic_op->addr_incr4));
	himax_in_parse_assign_cmd(ic_adr_i2c_psw_lb, pic_op->adr_i2c_psw_lb, sizeof(pic_op->adr_i2c_psw_lb));
	himax_in_parse_assign_cmd(ic_adr_i2c_psw_ub, pic_op->adr_i2c_psw_ub, sizeof(pic_op->adr_i2c_psw_ub));
	himax_in_parse_assign_cmd(ic_cmd_ahb_access_direction_read, pic_op->data_ahb_access_direction_read, sizeof(pic_op->data_ahb_access_direction_read));
	himax_in_parse_assign_cmd(ic_cmd_conti, pic_op->data_conti, sizeof(pic_op->data_conti));
	himax_in_parse_assign_cmd(hx122a_ic_cmd_incr4, pic_op->data_incr4, sizeof(pic_op->data_incr4));
	himax_in_parse_assign_cmd(ic_cmd_i2c_psw_lb, pic_op->data_i2c_psw_lb, sizeof(pic_op->data_i2c_psw_lb));
	himax_in_parse_assign_cmd(ic_cmd_i2c_psw_ub, pic_op->data_i2c_psw_ub, sizeof(pic_op->data_i2c_psw_ub));
	himax_in_parse_assign_cmd(ic_adr_tcon_on_rst, pic_op->addr_tcon_on_rst, sizeof(pic_op->addr_tcon_on_rst));
	himax_in_parse_assign_cmd(ic_addr_adc_on_rst, pic_op->addr_adc_on_rst, sizeof(pic_op->addr_adc_on_rst));
	himax_in_parse_assign_cmd(ic_adr_psl, pic_op->addr_psl, sizeof(pic_op->addr_psl));
	himax_in_parse_assign_cmd(ic_adr_cs_central_state, pic_op->addr_cs_central_state, sizeof(pic_op->addr_cs_central_state));
	himax_in_parse_assign_cmd(ic_cmd_rst, pic_op->data_rst, sizeof(pic_op->data_rst));
	himax_in_parse_assign_cmd(ic_adr_osc_en, pic_op->adr_osc_en, sizeof(pic_op->adr_osc_en));
	himax_in_parse_assign_cmd(ic_adr_osc_pw, pic_op->adr_osc_pw, sizeof(pic_op->adr_osc_pw));
#endif
#ifdef CORE_FW
	himax_in_parse_assign_cmd(fw_addr_system_reset, pfw_op->addr_system_reset, sizeof(pfw_op->addr_system_reset));
//	himax_in_parse_assign_cmd(fw_addr_safe_mode_release_pw, pfw_op->addr_safe_mode_release_pw, sizeof(pfw_op->addr_safe_mode_release_pw));
	himax_in_parse_assign_cmd(fw_addr_ctrl_fw, pfw_op->addr_ctrl_fw_isr, sizeof(pfw_op->addr_ctrl_fw_isr));
	himax_in_parse_assign_cmd(fw_addr_flag_reset_event, pfw_op->addr_flag_reset_event, sizeof(pfw_op->addr_flag_reset_event));
	himax_in_parse_assign_cmd(hx122a_fw_addr_hsen_enable, pfw_op->addr_hsen_enable, sizeof(pfw_op->addr_hsen_enable));
	himax_in_parse_assign_cmd(hx122a_fw_addr_smwp_enable, pfw_op->addr_smwp_enable, sizeof(pfw_op->addr_smwp_enable));
	himax_in_parse_assign_cmd(fw_addr_program_reload_from, pfw_op->addr_program_reload_from, sizeof(pfw_op->addr_program_reload_from));
//	himax_in_parse_assign_cmd(fw_addr_program_reload_to, pfw_op->addr_program_reload_to, sizeof(pfw_op->addr_program_reload_to));
//	himax_in_parse_assign_cmd(fw_addr_program_reload_page_write, pfw_op->addr_program_reload_page_write, sizeof(pfw_op->addr_program_reload_page_write));
	himax_in_parse_assign_cmd(hx122a_fw_addr_raw_out_sel, pfw_op->addr_raw_out_sel, sizeof(pfw_op->addr_raw_out_sel));
	himax_in_parse_assign_cmd(fw_addr_reload_status, pfw_op->addr_reload_status, sizeof(pfw_op->addr_reload_status));
	himax_in_parse_assign_cmd(fw_addr_reload_crc32_result, pfw_op->addr_reload_crc32_result, sizeof(pfw_op->addr_reload_crc32_result));
	himax_in_parse_assign_cmd(fw_addr_reload_addr_from, pfw_op->addr_reload_addr_from, sizeof(pfw_op->addr_reload_addr_from));
	himax_in_parse_assign_cmd(fw_addr_reload_addr_cmd_beat, pfw_op->addr_reload_addr_cmd_beat, sizeof(pfw_op->addr_reload_addr_cmd_beat));
	himax_in_parse_assign_cmd(hx122a_fw_addr_selftest_addr_en, pfw_op->addr_selftest_addr_en, sizeof(pfw_op->addr_selftest_addr_en));
	himax_in_parse_assign_cmd(hx122a_fw_addr_criteria_addr, pfw_op->addr_criteria_addr, sizeof(pfw_op->addr_criteria_addr));
	himax_in_parse_assign_cmd(hx122a_fw_addr_set_frame_addr, pfw_op->addr_set_frame_addr, sizeof(pfw_op->addr_set_frame_addr));
	himax_in_parse_assign_cmd(hx122a_fw_addr_selftest_result_addr, pfw_op->addr_selftest_result_addr, sizeof(pfw_op->addr_selftest_result_addr));
	himax_in_parse_assign_cmd(hx122a_fw_addr_gesture_en, pfw_op->addr_gesture_en, sizeof(pfw_op->addr_gesture_en));
	himax_in_parse_assign_cmd(hx122a_fw_addr_sorting_mode_en, pfw_op->addr_sorting_mode_en, sizeof(pfw_op->addr_sorting_mode_en));
	himax_in_parse_assign_cmd(hx122a_fw_addr_fw_mode_status, pfw_op->addr_fw_mode_status, sizeof(pfw_op->addr_fw_mode_status));
	himax_in_parse_assign_cmd(fw_addr_icid_addr, pfw_op->addr_icid_addr, sizeof(pfw_op->addr_icid_addr));
//	himax_in_parse_assign_cmd(fw_addr_trigger_addr, pfw_op->addr_trigger_addr, sizeof(pfw_op->addr_trigger_addr));
	himax_in_parse_assign_cmd(hx122a_fw_addr_fw_ver_addr, pfw_op->addr_fw_ver_addr, sizeof(pfw_op->addr_fw_ver_addr));
	himax_in_parse_assign_cmd(hx122a_fw_addr_fw_cfg_addr, pfw_op->addr_fw_cfg_addr, sizeof(pfw_op->addr_fw_cfg_addr));
	himax_in_parse_assign_cmd(hx122a_fw_addr_fw_vendor_addr, pfw_op->addr_fw_vendor_addr, sizeof(pfw_op->addr_fw_vendor_addr));
	himax_in_parse_assign_cmd(hx122a_fw_addr_cus_info, pfw_op->addr_cus_info, sizeof(pfw_op->addr_cus_info));
	himax_in_parse_assign_cmd(hx122a_fw_addr_proj_info, pfw_op->addr_proj_info, sizeof(pfw_op->addr_proj_info));
//	himax_in_parse_assign_cmd(fw_addr_fw_state_addr, pfw_op->addr_fw_state_addr, sizeof(pfw_op->addr_fw_state_addr));
	// himax_in_parse_assign_cmd(fw_addr_ver_ic_name, pfw_op->addr_ver_ic_name, sizeof(pfw_op->addr_ver_ic_name));
	himax_in_parse_assign_cmd(hx122a_fw_addr_fw_dbg_msg_addr, pfw_op->addr_fw_dbg_msg_addr, sizeof(pfw_op->addr_fw_dbg_msg_addr));
	himax_in_parse_assign_cmd(fw_addr_chk_fw_status, pfw_op->addr_chk_fw_status, sizeof(pfw_op->addr_chk_fw_status));
	himax_in_parse_assign_cmd(fw_addr_chk_dd_status, pfw_op->addr_chk_dd_status, sizeof(pfw_op->addr_chk_dd_status));
	himax_in_parse_assign_cmd(fw_addr_dd_handshak_addr, pfw_op->addr_dd_handshak_addr, sizeof(pfw_op->addr_dd_handshak_addr));
	himax_in_parse_assign_cmd(hx122a_fw_addr_dd_data_addr, pfw_op->addr_dd_data_addr, sizeof(pfw_op->addr_dd_data_addr));
//	himax_in_parse_assign_cmd(hx122a_fw_addr_clr_fw_record_dd_sts, pfw_op->addr_clr_fw_record_dd_sts, sizeof(pfw_op->addr_clr_fw_record_dd_sts));
	himax_in_parse_assign_cmd(hx122a_fw_addr_ap_notify_fw_sus, pfw_op->addr_ap_notify_fw_sus, sizeof(pfw_op->addr_ap_notify_fw_sus));
	himax_in_parse_assign_cmd(fw_data_ap_notify_fw_sus_en, pfw_op->data_ap_notify_fw_sus_en, sizeof(pfw_op->data_ap_notify_fw_sus_en));
	himax_in_parse_assign_cmd(fw_data_ap_notify_fw_sus_dis, pfw_op->data_ap_notify_fw_sus_dis, sizeof(pfw_op->data_ap_notify_fw_sus_dis));
	himax_in_parse_assign_cmd(fw_data_system_reset, pfw_op->data_system_reset, sizeof(pfw_op->data_system_reset));
//	himax_in_parse_assign_cmd(fw_data_safe_mode_release_pw_active, pfw_op->data_safe_mode_release_pw_active, sizeof(pfw_op->data_safe_mode_release_pw_active));
	himax_in_parse_assign_cmd(fw_data_clear, pfw_op->data_clear, sizeof(pfw_op->data_clear));
//	himax_in_parse_assign_cmd(fw_data_clear, pfw_op->data_clear, sizeof(pfw_op->data_clear));
	himax_in_parse_assign_cmd(fw_data_fw_stop, pfw_op->data_fw_stop, sizeof(pfw_op->data_fw_stop));
//	himax_in_parse_assign_cmd(fw_data_safe_mode_release_pw_reset, pfw_op->data_safe_mode_release_pw_reset, sizeof(pfw_op->data_safe_mode_release_pw_reset));
//	himax_in_parse_assign_cmd(fw_data_program_reload_start, pfw_op->data_program_reload_start, sizeof(pfw_op->data_program_reload_start));
//	himax_in_parse_assign_cmd(fw_data_program_reload_compare, pfw_op->data_program_reload_compare, sizeof(pfw_op->data_program_reload_compare));
//	himax_in_parse_assign_cmd(fw_data_program_reload_break, pfw_op->data_program_reload_break, sizeof(pfw_op->data_program_reload_break));
	himax_in_parse_assign_cmd(fw_data_selftest_request, pfw_op->data_selftest_request, sizeof(pfw_op->data_selftest_request));
	himax_in_parse_assign_cmd(fw_data_criteria_aa_top, pfw_op->data_criteria_aa_top, sizeof(pfw_op->data_criteria_aa_top));
	himax_in_parse_assign_cmd(fw_data_criteria_aa_bot, pfw_op->data_criteria_aa_bot, sizeof(pfw_op->data_criteria_aa_bot));
	himax_in_parse_assign_cmd(fw_data_criteria_key_top, pfw_op->data_criteria_key_top, sizeof(pfw_op->data_criteria_key_top));
	himax_in_parse_assign_cmd(fw_data_criteria_key_bot, pfw_op->data_criteria_key_bot, sizeof(pfw_op->data_criteria_key_bot));
	himax_in_parse_assign_cmd(fw_data_criteria_avg_top, pfw_op->data_criteria_avg_top, sizeof(pfw_op->data_criteria_avg_top));
	himax_in_parse_assign_cmd(fw_data_criteria_avg_bot, pfw_op->data_criteria_avg_bot, sizeof(pfw_op->data_criteria_avg_bot));
	himax_in_parse_assign_cmd(fw_data_set_frame, pfw_op->data_set_frame, sizeof(pfw_op->data_set_frame));
	himax_in_parse_assign_cmd(fw_data_selftest_ack_hb, pfw_op->data_selftest_ack_hb, sizeof(pfw_op->data_selftest_ack_hb));
	himax_in_parse_assign_cmd(fw_data_selftest_ack_lb, pfw_op->data_selftest_ack_lb, sizeof(pfw_op->data_selftest_ack_lb));
	himax_in_parse_assign_cmd(fw_data_selftest_pass, pfw_op->data_selftest_pass, sizeof(pfw_op->data_selftest_pass));
//	himax_in_parse_assign_cmd(fw_data_normal_cmd, pfw_op->data_normal_cmd, sizeof(pfw_op->data_normal_cmd));
//	himax_in_parse_assign_cmd(fw_data_normal_status, pfw_op->data_normal_status, sizeof(pfw_op->data_normal_status));
//	himax_in_parse_assign_cmd(fw_data_sorting_cmd, pfw_op->data_sorting_cmd, sizeof(pfw_op->data_sorting_cmd));
//	himax_in_parse_assign_cmd(fw_data_sorting_status, pfw_op->data_sorting_status, sizeof(pfw_op->data_sorting_status));
	himax_in_parse_assign_cmd(fw_data_dd_request, pfw_op->data_dd_request, sizeof(pfw_op->data_dd_request));
	himax_in_parse_assign_cmd(fw_data_dd_ack, pfw_op->data_dd_ack, sizeof(pfw_op->data_dd_ack));
	himax_in_parse_assign_cmd(fw_data_idle_dis_pwd, pfw_op->data_idle_dis_pwd, sizeof(pfw_op->data_idle_dis_pwd));
	himax_in_parse_assign_cmd(fw_data_idle_en_pwd, pfw_op->data_idle_en_pwd, sizeof(pfw_op->data_idle_en_pwd));
	himax_in_parse_assign_cmd(fw_data_rawdata_ready_hb, pfw_op->data_rawdata_ready_hb, sizeof(pfw_op->data_rawdata_ready_hb));
	himax_in_parse_assign_cmd(fw_data_rawdata_ready_lb, pfw_op->data_rawdata_ready_lb, sizeof(pfw_op->data_rawdata_ready_lb));
	himax_in_parse_assign_cmd(fw_addr_ahb_addr, pfw_op->addr_ahb_addr, sizeof(pfw_op->addr_ahb_addr));
	himax_in_parse_assign_cmd(fw_data_ahb_dis, pfw_op->data_ahb_dis, sizeof(pfw_op->data_ahb_dis));
	himax_in_parse_assign_cmd(fw_data_ahb_en, pfw_op->data_ahb_en, sizeof(pfw_op->data_ahb_en));
	himax_in_parse_assign_cmd(fw_addr_event_addr, pfw_op->addr_event_addr, sizeof(pfw_op->addr_event_addr));
	himax_in_parse_assign_cmd(hx122a_fw_usb_detect_addr, pfw_op->addr_usb_detect, sizeof(pfw_op->addr_usb_detect));
#ifdef HX_P_SENSOR
	himax_in_parse_assign_cmd(fw_addr_ulpm_33, pfw_op->addr_ulpm_33, sizeof(pfw_op->addr_ulpm_33));
	himax_in_parse_assign_cmd(fw_addr_ulpm_34, pfw_op->addr_ulpm_34, sizeof(pfw_op->addr_ulpm_34));
	himax_in_parse_assign_cmd(fw_data_ulpm_11, pfw_op->data_ulpm_11, sizeof(pfw_op->data_ulpm_11));
	himax_in_parse_assign_cmd(fw_data_ulpm_22, pfw_op->data_ulpm_22, sizeof(pfw_op->data_ulpm_22));
	himax_in_parse_assign_cmd(fw_data_ulpm_33, pfw_op->data_ulpm_33, sizeof(pfw_op->data_ulpm_33));
	himax_in_parse_assign_cmd(fw_data_ulpm_aa, pfw_op->data_ulpm_aa, sizeof(pfw_op->data_ulpm_aa));
#endif
	himax_in_parse_assign_cmd(hx122a_fw_addr_ctrl_mpap_ovl, pfw_op->addr_ctrl_mpap_ovl, sizeof(pfw_op->addr_ctrl_mpap_ovl));
	himax_in_parse_assign_cmd(fw_data_ctrl_mpap_ovl_on, pfw_op->data_ctrl_mpap_ovl_on, sizeof(pfw_op->data_ctrl_mpap_ovl_on));

	himax_in_parse_assign_cmd(hx122a_fw_addr_normal_noise_thx, pfw_op->addr_normal_noise_thx, sizeof(pfw_op->addr_normal_noise_thx));
	himax_in_parse_assign_cmd(hx122a_fw_addr_lpwug_noise_thx, pfw_op->addr_lpwug_noise_thx, sizeof(pfw_op->addr_lpwug_noise_thx));
	himax_in_parse_assign_cmd(hx122a_fw_addr_noise_scale, pfw_op->addr_noise_scale, sizeof(pfw_op->addr_noise_scale));
	himax_in_parse_assign_cmd(hx122a_fw_addr_recal_thx, pfw_op->addr_recal_thx, sizeof(pfw_op->addr_recal_thx));
	himax_in_parse_assign_cmd(hx122a_fw_addr_palm_num, pfw_op->addr_palm_num, sizeof(pfw_op->addr_palm_num));
	himax_in_parse_assign_cmd(hx122a_fw_addr_weight_sup, pfw_op->addr_weight_sup, sizeof(pfw_op->addr_weight_sup));
	himax_in_parse_assign_cmd(hx122a_fw_addr_normal_weight_a, pfw_op->addr_normal_weight_a, sizeof(pfw_op->addr_normal_weight_a));
	himax_in_parse_assign_cmd(hx122a_fw_addr_lpwug_weight_a, pfw_op->addr_lpwug_weight_a, sizeof(pfw_op->addr_lpwug_weight_a));
	himax_in_parse_assign_cmd(hx122a_fw_addr_weight_b, pfw_op->addr_weight_b, sizeof(pfw_op->addr_weight_b));
	himax_in_parse_assign_cmd(hx122a_fw_addr_max_dc, pfw_op->addr_max_dc, sizeof(pfw_op->addr_max_dc));
	himax_in_parse_assign_cmd(hx122a_fw_addr_skip_frame, pfw_op->addr_skip_frame, sizeof(pfw_op->addr_skip_frame));
	himax_in_parse_assign_cmd(hx122a_fw_addr_neg_noise_sup, pfw_op->addr_neg_noise_sup, sizeof(pfw_op->addr_neg_noise_sup));
	himax_in_parse_assign_cmd(fw_data_neg_noise, pfw_op->data_neg_noise, sizeof(pfw_op->data_neg_noise));
	// himax_in_parse_assign_cmd(hx122a_fw_addr_mpfw_option, pfw_op->addr_mpfw_option, sizeof(pfw_op->addr_mpfw_option));
	// himax_in_parse_assign_cmd(hx122a_fw_addr_mpfw_mode_status, pfw_op->addr_mpfw_mode_status, sizeof(pfw_op->addr_mpfw_mode_status));
	// himax_in_parse_assign_cmd(hx122a_fw_addr_leave_idle, pfw_op->addr_leave_idle, sizeof(pfw_op->addr_leave_idle));
#ifdef SEC_FACTORY_MODE
	himax_in_parse_assign_cmd(HX122A_FW_EDGE_BORDER_ADDR, pfw_op->addr_edge_border, sizeof(pfw_op->addr_edge_border));
//	himax_in_parse_assign_cmd(FW_CAL_ADDR, pfw_op->addr_cal, sizeof(pfw_op->addr_cal));
	himax_in_parse_assign_cmd(HX122A_FW_RPORT_THRSH_ADDR, pfw_op->addr_rport_thrsh, sizeof(pfw_op->addr_rport_thrsh));

	himax_in_parse_assign_cmd(hx122a_fw_addr_rotate, pfw_op->addr_rotative_mode,
		sizeof(pfw_op->addr_rotative_mode));
	himax_in_parse_assign_cmd(DATA_PORTRAIT,
		pfw_op->data_portrait, sizeof(pfw_op->data_portrait));
	himax_in_parse_assign_cmd(DATA_LANDSCAPE, pfw_op->data_landscape,
		sizeof(pfw_op->data_landscape));

	pfw_op->addr_grip_zone = hx122a_fw_addr_grip_zone;
	pfw_op->addr_reject_zone = hx122a_fw_addr_reject_zone;
	pfw_op->addr_reject_zone_boud = hx122a_fw_addr_reject_zone_bound;
	pfw_op->addr_except_zone = hx122a_fw_addr_except_zone;

	himax_in_parse_assign_cmd(hx83122a_addr_snr_measurement, pfw_op->addr_snr_measurement,
		sizeof(pfw_op->addr_snr_measurement));

	himax_in_parse_assign_cmd(hx122a_fw_addr_proximity, pfw_op->addr_proximity_mode,
		sizeof(pfw_op->addr_proximity_mode));

	himax_in_parse_assign_cmd(hx122a_func_base, pfw_op->addr_mode_base,
		sizeof(pfw_op->addr_mode_base));

	himax_in_parse_assign_cmd(hx83122a_addr_osr_ctrl, pfw_op->addr_osr_ctrl,
		sizeof(pfw_op->addr_osr_ctrl));

	himax_in_parse_assign_cmd(hx83122a_addr_reject_idle, pfw_op->addr_reject_idle,
		sizeof(pfw_op->addr_reject_idle));

	himax_in_parse_assign_cmd(ADDR_PROXIMITY_DEBUG, pfw_op->addr_proximity_debug,
		sizeof(pfw_op->addr_proximity_debug));
#endif
#endif
#ifdef CORE_FLASH
	himax_in_parse_assign_cmd(flash_addr_spi200_trans_fmt, pflash_op->addr_spi200_trans_fmt, sizeof(pflash_op->addr_spi200_trans_fmt));
	himax_in_parse_assign_cmd(flash_addr_spi200_trans_ctrl, pflash_op->addr_spi200_trans_ctrl, sizeof(pflash_op->addr_spi200_trans_ctrl));
	himax_in_parse_assign_cmd(flash_addr_spi200_cmd, pflash_op->addr_spi200_cmd, sizeof(pflash_op->addr_spi200_cmd));
	himax_in_parse_assign_cmd(flash_addr_spi200_addr, pflash_op->addr_spi200_addr, sizeof(pflash_op->addr_spi200_addr));
	himax_in_parse_assign_cmd(flash_addr_spi200_data, pflash_op->addr_spi200_data, sizeof(pflash_op->addr_spi200_data));
	himax_in_parse_assign_cmd(flash_addr_spi200_flash_speed, pflash_op->addr_spi200_flash_speed, sizeof(pflash_op->addr_spi200_flash_speed));
	himax_in_parse_assign_cmd(flash_addr_spi200_bt_num, pflash_op->addr_spi200_bt_num, sizeof(pflash_op->addr_spi200_bt_num));
	himax_in_parse_assign_cmd(flash_data_spi200_trans_fmt, pflash_op->data_spi200_trans_fmt, sizeof(pflash_op->data_spi200_trans_fmt));
	himax_in_parse_assign_cmd(flash_data_spi200_trans_ctrl_1, pflash_op->data_spi200_trans_ctrl_1, sizeof(pflash_op->data_spi200_trans_ctrl_1));
	himax_in_parse_assign_cmd(flash_data_spi200_trans_ctrl_2, pflash_op->data_spi200_trans_ctrl_2, sizeof(pflash_op->data_spi200_trans_ctrl_2));
	himax_in_parse_assign_cmd(flash_data_spi200_trans_ctrl_3, pflash_op->data_spi200_trans_ctrl_3, sizeof(pflash_op->data_spi200_trans_ctrl_3));
	himax_in_parse_assign_cmd(flash_data_spi200_trans_ctrl_4, pflash_op->data_spi200_trans_ctrl_4, sizeof(pflash_op->data_spi200_trans_ctrl_4));
	himax_in_parse_assign_cmd(flash_data_spi200_trans_ctrl_5, pflash_op->data_spi200_trans_ctrl_5, sizeof(pflash_op->data_spi200_trans_ctrl_5));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_1, pflash_op->data_spi200_cmd_1, sizeof(pflash_op->data_spi200_cmd_1));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_2, pflash_op->data_spi200_cmd_2, sizeof(pflash_op->data_spi200_cmd_2));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_3, pflash_op->data_spi200_cmd_3, sizeof(pflash_op->data_spi200_cmd_3));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_4, pflash_op->data_spi200_cmd_4, sizeof(pflash_op->data_spi200_cmd_4));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_5, pflash_op->data_spi200_cmd_5, sizeof(pflash_op->data_spi200_cmd_5));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_6, pflash_op->data_spi200_cmd_6, sizeof(pflash_op->data_spi200_cmd_6));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_7, pflash_op->data_spi200_cmd_7, sizeof(pflash_op->data_spi200_cmd_7));
	himax_in_parse_assign_cmd(flash_data_spi200_addr, pflash_op->data_spi200_addr, sizeof(pflash_op->data_spi200_addr));
#endif
#ifdef CORE_SRAM
	/* sram start*/
	himax_in_parse_assign_cmd(hx122a_sram_adr_mkey, psram_op->addr_mkey, sizeof(psram_op->addr_mkey));
	himax_in_parse_assign_cmd(hx122a_sram_adr_rawdata_addr, psram_op->addr_rawdata_addr, sizeof(psram_op->addr_rawdata_addr));
	// himax_in_parse_assign_cmd(sram_data_rawdata_end, psram_op->data_rawdata_end, sizeof(psram_op->data_rawdata_end));
	himax_in_parse_assign_cmd(sram_passwrd_start, psram_op->passwrd_start, sizeof(psram_op->passwrd_start));
	himax_in_parse_assign_cmd(sram_passwrd_end, psram_op->passwrd_end, sizeof(psram_op->passwrd_end));
	/* sram end*/
#endif
#ifdef CORE_DRIVER
	himax_in_parse_assign_cmd(hx122a_driver_addr_fw_define_flash_reload, pdriver_op->addr_fw_define_flash_reload, sizeof(pdriver_op->addr_fw_define_flash_reload));
	himax_in_parse_assign_cmd(hx122a_driver_addr_fw_define_2nd_flash_reload, pdriver_op->addr_fw_define_2nd_flash_reload, sizeof(pdriver_op->addr_fw_define_2nd_flash_reload));
	himax_in_parse_assign_cmd(hx122a_driver_addr_fw_define_int_is_edge, pdriver_op->addr_fw_define_int_is_edge, sizeof(pdriver_op->addr_fw_define_int_is_edge));
	himax_in_parse_assign_cmd(hx122a_driver_addr_fw_define_rxnum_txnum, pdriver_op->addr_fw_define_rxnum_txnum_maxpt, sizeof(pdriver_op->addr_fw_define_rxnum_txnum_maxpt));
	himax_in_parse_assign_cmd(hx122a_driver_addr_fw_define_maxpt_xyrvs, pdriver_op->addr_fw_define_xy_res_enable, sizeof(pdriver_op->addr_fw_define_xy_res_enable));
	himax_in_parse_assign_cmd(hx122a_driver_addr_fw_define_x_y_res, pdriver_op->addr_fw_define_x_y_res, sizeof(pdriver_op->addr_fw_define_x_y_res));
	himax_in_parse_assign_cmd(hx122a_driver_data_df_rx, pdriver_op->data_df_rx, sizeof(pdriver_op->data_df_rx));
	himax_in_parse_assign_cmd(hx122a_driver_data_df_tx, pdriver_op->data_df_tx, sizeof(pdriver_op->data_df_tx));
	himax_in_parse_assign_cmd(hx122a_driver_data_df_pt, pdriver_op->data_df_pt, sizeof(pdriver_op->data_df_pt));
	himax_in_parse_assign_cmd(driver_data_fw_define_flash_reload_dis, pdriver_op->data_fw_define_flash_reload_dis, sizeof(pdriver_op->data_fw_define_flash_reload_dis));
	himax_in_parse_assign_cmd(driver_data_fw_define_flash_reload_en, pdriver_op->data_fw_define_flash_reload_en, sizeof(pdriver_op->data_fw_define_flash_reload_en));
	himax_in_parse_assign_cmd(driver_data_fw_define_rxnum_txnum_maxpt_sorting, pdriver_op->data_fw_define_rxnum_txnum_maxpt_sorting, sizeof(pdriver_op->data_fw_define_rxnum_txnum_maxpt_sorting));
	himax_in_parse_assign_cmd(driver_data_fw_define_rxnum_txnum_maxpt_normal, pdriver_op->data_fw_define_rxnum_txnum_maxpt_normal, sizeof(pdriver_op->data_fw_define_rxnum_txnum_maxpt_normal));
#endif
#ifdef HX_ZERO_FLASH
	himax_in_parse_assign_cmd(zf_data_dis_flash_reload, pzf_op->data_dis_flash_reload, sizeof(pzf_op->data_dis_flash_reload));
	himax_in_parse_assign_cmd(zf_addr_activ_relod, pzf_op->addr_activ_relod, sizeof(pzf_op->addr_activ_relod));
	himax_in_parse_assign_cmd(zf_data_activ_in, pzf_op->data_activ_in, sizeof(pzf_op->data_activ_in));
#if defined(HX_CODE_OVERLAY)
	himax_in_parse_assign_cmd(hx122a_zf_addr_ovl_handshake, pzf_op->addr_ovl_handshake, sizeof(pzf_op->addr_ovl_handshake));
#endif

#endif
	himax_in_parse_assign_cmd(hx83122a_addr_ic_ver_name,
		pfw_op->addr_ver_ic_name,
		sizeof(pfw_op->addr_ver_ic_name));
	himax_in_parse_assign_cmd(hx83122a_fw_addr_gesture_history,
		pfw_op->addr_gesture_history,
		sizeof(pfw_op->addr_gesture_history));
}

static void himax_hx83122a_func_re_init(void)
{
	I("%s:Entering!\n", __func__);

	g_core_fp.fp_chip_init = hx83122a_chip_init;
	g_core_fp.fp_sense_on = hx83122a_sense_on;
	g_core_fp.fp_sense_off = hx83122a_sense_off;
	g_core_fp.fp_read_event_stack = hx83122a_read_event_stack;
	g_core_fp.fp_burst_enable = hx83122_burst_enable;
}

static bool hx83122_chip_detect(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t tmp_addr[DATA_LEN_4];
	bool ret_data = false;
	int ret = 0;
	int i = 0;

#if defined(HX_RST_PIN_FUNC)
	hx83122_pin_reset();
#endif

	if (himax_bus_read(0x13, tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: bus access fail!\n", __func__);
		return false;
	}

#if defined(HX_ZERO_FLASH)
	if (hx83122_sense_off(false) == false) {
#else
	if (hx83122_sense_off(true) == false) {
#endif
		ret_data = false;
		E("%s:hx83122_sense_off Fail:\n", __func__);
		return ret_data;
	}

	for (i = 0; i < 5; i++) {
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xD0;
		if (hx83122_register_read(tmp_addr, DATA_LEN_4, tmp_data) != 0) {
			ret_data = false;
			E("%s:hx83122_register_read Fail:\n", __func__);
			return ret_data;
		}
		I("%s:Read driver IC ID = %X,%X,%X\n", __func__, tmp_data[3],
			tmp_data[2], tmp_data[1]); /*83,10,2X*/

		if (tmp_data[3] == 0x83 && tmp_data[2] == 0x12
		&& (tmp_data[1] == 0x2a)) {
			strlcpy(private_ts->chip_name,
				HX_83122A_SERIES_PWON,
				30);

			ic_data->flash_size = HX83122A_FLASH_SIZE;

			ic_data->dsram_size = hx83122a_dsram_size;
			ic_data->isram_size = hx83122a_isram_size;
			ic_data->dsram_addr = hx83122a_dsram_addr;
			ic_data->isram_addr = hx83122a_isram_addr;

			I("%s:detect IC HX83122A successfully\n", __func__);

			ret = himax_mcu_in_cmd_struct_init();
			if (ret < 0) {
				ret_data = false;
				E("%s:cmd_struct_init Fail:\n", __func__);
				return ret_data;
			}

			himax_hx83122a_cmd_init();
			himax_hx83122a_func_re_init();

			ret_data = true;
			break;
		}

		ret_data = false;
		E("%s:Read driver ID register Fail:\n", __func__);
		E("Could NOT find Himax Chipset\n");
		E("Please check 1.VCCD,VCCA,VSP,VSN\n");
		E("2. LCM_RST,TP_RST\n");
		E("3. Power On Sequence\n");

	}

	return ret_data;
}

bool _hx83122_init(void)
{
	bool ret = false;

	I("%s\n", __func__);
	ret = hx83122_chip_detect();
	return ret;
}
