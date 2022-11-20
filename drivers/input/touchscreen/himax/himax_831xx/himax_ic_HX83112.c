/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for HX83112 chipset
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

#include "himax_ic_HX83112.h"
#include "himax_modular.h"

static int32_t himax_ic_setup_external_symbols(void)
{
	int32_t ret = 0;

#if !defined(__HIMAX_HX852xH_MOD__) && !defined(__HIMAX_HX852xG_MOD__)
	assert_on_symbol(pfw_op);
	assert_on_symbol(pic_op);
	assert_on_symbol(pdriver_op);
#endif
	assert_on_symbol(g_core_chip_dt);

	assert_on_symbol(private_ts);
	assert_on_symbol(g_core_fp);
	assert_on_symbol(ic_data);

#if defined(__HIMAX_HX852xH_MOD__) || defined(__HIMAX_HX852xG_MOD__)
#if defined(__HIMAX_HX852xG_MOD__)
	assert_on_symbol(on_pdriver_op);
#endif
	assert_on_symbol_func(himax_mcu_on_cmd_init);
	assert_on_symbol_func(himax_mcu_on_cmd_struct_init);
	assert_on_symbol_func(himax_on_parse_assign_cmd);
#else
	assert_on_symbol_func(himax_mcu_in_cmd_init);
	assert_on_symbol_func(himax_mcu_in_cmd_struct_init);
	assert_on_symbol_func(himax_in_parse_assign_cmd);
#endif

	assert_on_symbol_func(himax_bus_read);
	assert_on_symbol_func(himax_bus_write);
	assert_on_symbol_func(himax_bus_write_command);
	assert_on_symbol_func(himax_bus_master_write);
	assert_on_symbol_func(himax_int_enable);
	assert_on_symbol_func(himax_ts_register_interrupt);
	assert_on_symbol_func(himax_int_gpio_read);
	assert_on_symbol_func(himax_gpio_power_config);

#if defined(HX_ZERO_FLASH)
	assert_on_symbol(pzf_op);
	assert_on_symbol(G_POWERONOF);
#endif

#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
#if defined(HX_EN_DYNAMIC_NAME)
	assert_on_symbol(i_CTPM_firmware_name);
#endif
#endif
	assert_on_symbol(IC_CHECKSUM);

	assert_on_symbol(FW_VER_MAJ_FLASH_ADDR);
	assert_on_symbol(FW_VER_MIN_FLASH_ADDR);
	assert_on_symbol(CFG_VER_MAJ_FLASH_ADDR);
	assert_on_symbol(CFG_VER_MIN_FLASH_ADDR);
	assert_on_symbol(CID_VER_MAJ_FLASH_ADDR);
	assert_on_symbol(CID_VER_MIN_FLASH_ADDR);
	assert_on_symbol(CFG_TABLE_FLASH_ADDR);
	assert_on_symbol(ADDR_VER_IC_NAME);
	assert_on_symbol(PANEL_VERSION_ADDR);

	assert_on_symbol(FW_VER_MAJ_FLASH_LENG);
	assert_on_symbol(FW_VER_MIN_FLASH_LENG);
	assert_on_symbol(CFG_VER_MAJ_FLASH_LENG);
	assert_on_symbol(CFG_VER_MIN_FLASH_LENG);
	assert_on_symbol(CID_VER_MAJ_FLASH_LENG);
	assert_on_symbol(CID_VER_MIN_FLASH_LENG);
	assert_on_symbol(PANEL_VERSION_LENG);

#ifdef HX_AUTO_UPDATE_FW
	assert_on_symbol(g_i_FW_VER);
	assert_on_symbol(g_i_CFG_VER);
	assert_on_symbol(g_i_CID_MAJ);
	assert_on_symbol(g_i_CID_MIN);
	assert_on_symbol(i_CTPM_FW);
#endif

#ifdef HX_TP_PROC_2T2R
	assert_on_symbol(Is_2T2R);
#endif

#ifdef HX_USB_DETECT_GLOBAL
	assert_on_symbol_func(himax_cable_detect_func);
#endif

#ifdef HX_RST_PIN_FUNC
	assert_on_symbol_func(himax_rst_gpio_set);
#endif

#ifdef HX_ESD_RECOVERY
	assert_on_symbol(HX_ESD_RESET_ACTIVATE);
#endif
	return ret;
}

static void hx83112_chip_init(void)
{
	(*kp_private_ts)->chip_cell_type = CHIP_IS_IN_CELL;
	I("%s:IC cell type = %d\n",  __func__,
		(*kp_private_ts)->chip_cell_type);
	(*kp_IC_CHECKSUM)	=	HX_TP_BIN_CHECKSUM_CRC;
	/*Himax: Set FW and CFG Flash Address*/
	// (FW_VER_MAJ_FLASH_ADDR) = 49157;  /*0x00C005*/
	// (FW_VER_MIN_FLASH_ADDR) = 49158;  /*0x00C006*/
	// (CFG_VER_MAJ_FLASH_ADDR) = 49408;  /*0x00C100*/
	// (CFG_VER_MIN_FLASH_ADDR) = 49409;  /*0x00C101*/
	// (CID_VER_MAJ_FLASH_ADDR) = 49154;  /*0x00C002*/
	// (CID_VER_MIN_FLASH_ADDR) = 49155;  /*0x00C003*/
	// (CFG_TABLE_FLASH_ADDR) = 0x10000;
	// (CFG_TABLE_FLASH_ADDR_T) = (CFG_TABLE_FLASH_ADDR);

	CID_VER_MAJ_FLASH_ADDR	= 49154;  /*0x00C002*/
	CID_VER_MAJ_FLASH_LENG	= 1;
	CID_VER_MIN_FLASH_ADDR	= 49155;  /*0x00C003*/
	CID_VER_MIN_FLASH_LENG	= 1;
	PANEL_VERSION_ADDR		= 0x012404;
	PANEL_VERSION_LENG		= 1;
	FW_VER_MAJ_FLASH_ADDR		= 49157;  /*0x00C005*/
	FW_VER_MAJ_FLASH_LENG		= 1;
	FW_VER_MIN_FLASH_ADDR		= 49158;  /*0x00C006*/
	FW_VER_MIN_FLASH_LENG		= 1;
	CFG_VER_MAJ_FLASH_ADDR	= 49408;  /*0x00C100*/
	CFG_VER_MAJ_FLASH_LENG	= 1;
	CFG_VER_MIN_FLASH_ADDR	= 49409;  /*0x00C101*/
	CFG_VER_MIN_FLASH_LENG	= 1;
	ADDR_VER_IC_NAME	= 0x12450;
}

static bool hx83112_sense_off(bool check_en)
{
	uint8_t cnt = 0;
	uint8_t tmp_data[DATA_LEN_4];
	int ret = 0;

	do {
		if (atomic_read(&(*kp_private_ts)->suspend_mode) == HIMAX_STATE_POWER_OFF) {
			I("%s:It had power-off-suspended!\n", __func__);
			break;
		}

		if (cnt == 0
		|| (tmp_data[0] != 0xA5
		&& tmp_data[0] != 0x00
		&& tmp_data[0] != 0x87))
			g_core_fp.fp_register_write(
				(*kp_pfw_op)->addr_ctrl_fw_isr,
				DATA_LEN_4,
				(*kp_pfw_op)->data_fw_stop,
				0);

		/*msleep(20);*/
		usleep_range(10000, 10001);
		/* check fw status */
		g_core_fp.fp_register_read(
			(*kp_pic_op)->addr_cs_central_state,
			ADDR_LEN_4, tmp_data, 0);

		if (tmp_data[0] != 0x05) {
			I("%s: Do not need wait FW, Status = 0x%02X!\n",
					__func__, tmp_data[0]);
			break;
		}

		g_core_fp.fp_register_read((*kp_pfw_op)->addr_ctrl_fw_isr,
			4, tmp_data, false);
		I("%s: cnt = %d, data[0] = 0x%02X!\n", __func__,
			cnt, tmp_data[0]);
	} while (tmp_data[0] != 0x87 && (++cnt < 10) && check_en == true);

	cnt = 0;

	do {
		if (atomic_read(&(*kp_private_ts)->suspend_mode) == HIMAX_STATE_POWER_OFF) {
			I("%s:It had power-off-suspended!\n", __func__);
			break;
		}
		/**
		 * I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 */
		tmp_data[0] = (*kp_pic_op)->data_i2c_psw_lb[0];

		ret = himax_bus_write((*kp_pic_op)->adr_i2c_psw_lb[0],
				tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/**
		 * I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 */
		tmp_data[0] = (*kp_pic_op)->data_i2c_psw_ub[0];

		ret = himax_bus_write((*kp_pic_op)->adr_i2c_psw_ub[0],
				tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/**
		 * Check enter_save_mode
		 */
		g_core_fp.fp_register_read(
			(*kp_pic_op)->addr_cs_central_state,
			ADDR_LEN_4,
			tmp_data,
			0);
		I("%s: Check enter_save_mode data[0]=%X\n", __func__,
			tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
			/**
			 * Reset TCON
			 */
			g_core_fp.fp_register_write(
				(*kp_pic_op)->addr_tcon_on_rst,
				DATA_LEN_4,
				(*kp_pic_op)->data_rst,
				0);
			usleep_range(1000, 1001);
			/**
			 * Reset ADC
			 */
			g_core_fp.fp_register_write(
				(*kp_pic_op)->addr_adc_on_rst,
				DATA_LEN_4,
				(*kp_pic_op)->data_rst,
				0);
			usleep_range(1000, 1001);
			tmp_data[3] = (*kp_pic_op)->data_rst[3];
			tmp_data[2] = (*kp_pic_op)->data_rst[2];
			tmp_data[1] = (*kp_pic_op)->data_rst[1];
			tmp_data[0] = (*kp_pic_op)->data_rst[0] | 0x01;
			g_core_fp.fp_register_write(
				(*kp_pic_op)->addr_adc_on_rst,
				DATA_LEN_4,
				tmp_data,
				0);
			goto SUCCEED;
		} else {
			/*msleep(10);*/
#if defined(HX_RST_PIN_FUNC)
			g_core_fp.fp_ic_reset(false, false);
#else
			g_core_fp.fp_system_reset();
#endif
		}
	} while (cnt++ < 5);

	return false;
SUCCEED:
	return true;
}

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
		if (atomic_read(&(*kp_private_ts)->suspend_mode) == HIMAX_STATE_POWER_OFF) {
			I("%s:It had power-off-suspended!\n", __func__);
			break;
		}
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
	if (atomic_read(&(*kp_private_ts)->suspend_mode) == HIMAX_STATE_POWER_OFF) {
		I("%s:It had power-off-suspended!\n", __func__);
		return;
	}
	// (*kp_private_ts)->notouch_frame = (*kp_private_ts)->ic_notouch_frame;
	g_core_fp.fp_interface_on();
	g_core_fp.fp_register_write((*kp_pfw_op)->addr_ctrl_fw_isr,
		sizeof((*kp_pfw_op)->data_clear), (*kp_pfw_op)->data_clear, 0);
	/*msleep(20);*/
	usleep_range(10000, 10001);
	if (!FlashMode) {
#if defined(HX_RST_PIN_FUNC)
		g_core_fp.fp_ic_reset(false, false);
#else
		g_core_fp.fp_system_reset();
#endif
	} else {
		do {
			if (atomic_read(&(*kp_private_ts)->suspend_mode) == HIMAX_STATE_POWER_OFF) {
				I("%s:It had power-off-suspended!\n", __func__);
				break;
			}
			g_core_fp.fp_register_read(
				(*kp_pfw_op)->addr_flag_reset_event,
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
		} else {
			I("%s:OK and Read status from IC = %X,%X\n", __func__,
				tmp_data[0], tmp_data[1]);
			/* reset code*/
			tmp_data[0] = 0x00;

			ret = himax_bus_write(
				(*kp_pic_op)->adr_i2c_psw_lb[0],
				tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
			if (ret < 0) {
				E("%s: cmd=%x i2c access fail!\n",
				__func__,
				(*kp_pic_op)->adr_i2c_psw_lb[0]);
			}

				ret = himax_bus_write(
					(*kp_pic_op)->adr_i2c_psw_ub[0],
					tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
			if (ret < 0) {
				E("%s: cmd=%x i2c access fail!\n",
				__func__,
				(*kp_pic_op)->adr_i2c_psw_ub[0]);
			}
		}
	}
	himax_hx83112f_reload_to_active();
}

static void himax_hx83112f_system_reset(void)
{
	int ret = 0;
	uint8_t tmp_data[DATA_LEN_4];
	int retry = 0;

	I("%s: Entering!\n", __func__);
	g_core_fp.fp_interface_on();
	g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr,
		sizeof(pfw_op->data_clear), pfw_op->data_clear, false);
	do {
		/* reset code*/
		/**
		 * I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 */
		tmp_data[0] = pic_op->data_i2c_psw_lb[0];

		ret = himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0)
			E("%s: bus access fail!\n", __func__);

		/**
		 * I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 */
		tmp_data[0] = pic_op->data_i2c_psw_ub[0];

		ret = himax_bus_write(pic_op->adr_i2c_psw_ub[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0)
			E("%s: bus access fail!\n", __func__);

		/**
		 * I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x00
		 */
		tmp_data[0] = 0x00;

		ret = himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0)
			E("%s: bus access fail!\n", __func__);

		usleep_range(10000, 11000);

		g_core_fp.fp_register_read(pfw_op->addr_flag_reset_event,
			DATA_LEN_4, tmp_data, false);
		I("%s:Read status from IC = %X,%X\n", __func__,
				tmp_data[0], tmp_data[1]);
	} while ((tmp_data[1] != 0x02 || tmp_data[0] != 0x00) && retry++ < 5);

	I("%s: End!\n", __func__);
}

#endif

static void hx83112f_dd_en_read(int en)
{
	uint8_t addr_osc[] = {0x9C, 0x00, 0x00, 0x90};
	uint8_t addr_osc_pw[] = {0x80, 0x02, 0x00, 0x90};
	uint8_t addr_pwd2[] = {0x00, 0xB0, 0x0E, 0x30};

	uint8_t en_val_osc[] = {0xDD, 0x00, 0x00, 0x00};
	uint8_t en_pwd_val_osc[] = {0xA5, 0x00, 0x00, 0x00};
	uint8_t en_pwd2[] = {0x00, 0x55, 0x66, 0xCC};

	uint8_t dis_val_osc[] = {0x00, 0x00, 0x00, 0x00};
	uint8_t dis_pwd_val_osc[] = {0x00, 0x00, 0x00, 0x00};


	if (en) {
		// I("%s: EN!\n", __func__);
		g_core_fp.fp_register_write(addr_osc, DATA_LEN_4, en_val_osc, false);
		usleep_range(1000, 1001);
		g_core_fp.fp_register_write(addr_osc_pw, DATA_LEN_4, en_pwd_val_osc, false);
		usleep_range(1000, 1001);
		g_core_fp.fp_register_write(addr_pwd2, DATA_LEN_4, en_pwd2, false);


	} else {
		// I("%s: DIS!\n", __func__);
		g_core_fp.fp_register_write(addr_osc_pw, DATA_LEN_4, dis_val_osc, false);
		usleep_range(1000, 1001);
		g_core_fp.fp_register_write(addr_osc_pw, DATA_LEN_4, dis_pwd_val_osc, false);
	}
}

void hx83112f_read_proxy1b(void)
{
	uint8_t tmp_addr[] = {0x18, 0x7F, 0x00, 0x10};
	uint8_t tmp_data[4] = {0};

	g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);
	(*kp_private_ts)->proxy_1b_en = tmp_data[2] & 0x02;
	I("%s now proxy_1b_en= %d\n", __func__, (*kp_private_ts)->proxy_1b_en);
}

static void hx83112f_proximity_display_off(void)
{
	uint8_t addr_display_off[] = {0x00, 0x80, 0x02, 0x30};
	uint8_t enter_cmd[] = {0x00, 0x00, 0x00, 0x00};

	I("%s: Entering\n", __func__);
	g_core_fp._dd_en_read(1);
	g_core_fp.fp_register_write(addr_display_off, DATA_LEN_4, enter_cmd, false);
	g_core_fp._dd_en_read(0);
	I("%s: End\n", __func__);

}
static void hx83112f_proximity_sleep_in(void)
{
	uint8_t addr_sleep_in[] = {0x00, 0x00, 0x01, 0x30};
	uint8_t enter_cmd[] = {0x00, 0x00, 0x00, 0x00};

	uint8_t addr_db0h[] = {0x04, 0x00, 0x0B, 0x30};
	uint8_t off_db0h[] = {0x0E, 0x00, 0x00, 0x00};

	uint8_t addr_bist[] = {0x00, 0xF0, 0x0C, 0x30};
	uint8_t off_bist[] = {0x00, 0x00, 0x14, 0x00};

	uint8_t read_db0h[DATA_LEN_4] = {0};
	int retry = 30;
	I("%s: Entering\n", __func__);

	memset(read_db0h, 0x78, sizeof(read_db0h));
	while (read_db0h[0] != off_db0h[0] && retry > 0) {
		g_core_fp._dd_en_read(1);
		usleep_range(1000, 1001);
		g_core_fp.fp_register_write(addr_db0h, DATA_LEN_4, off_db0h, false);
		usleep_range(10000, 10001);
		g_core_fp.fp_register_read(addr_db0h, DATA_LEN_4, read_db0h, 0);
		I("%s: read_data[0]=0x%02X, off_db0h[0]=0x%02X, retry_=%d\n", __func__,
				read_db0h[0], off_db0h[0], retry--);
	}
	usleep_range(1000, 1001);

	memset(read_db0h, 0x78, sizeof(read_db0h));
	retry = 30;
	while (read_db0h[1] != off_bist[1] && retry > 0) {
		g_core_fp._dd_en_read(1);
		usleep_range(1000, 1001);
		g_core_fp.fp_register_write(addr_bist, DATA_LEN_4, off_bist, false);
		usleep_range(10000, 10001);
		g_core_fp.fp_register_read(addr_bist, DATA_LEN_4, read_db0h, 0);
		I("%s: read_data[1]=0x%02X, off_bist[1]=0x%02X, retry_=%d\n", __func__,
				read_db0h[1], off_bist[1], retry--);
	}
	usleep_range(1000, 1001);

	g_core_fp._dd_en_read(1);
	g_core_fp.fp_register_write(addr_sleep_in, DATA_LEN_4, enter_cmd, false);
	g_core_fp._dd_en_read(0);

	if ((*kp_private_ts)->SMWP_enable == 1 && (*kp_private_ts)->proxy_1b_en == 0) {
		g_core_fp.fp_set_SMWP_enable(0, (*kp_private_ts)->suspended);
	}
	I("%s: Delay\n", __func__);
	msleep(70);
	I("%s: End\n", __func__);
}

static void hx83112f_proximity_display_on(void)
{
	uint8_t addr_display_on[] = {0x00, 0x90, 0x02, 0x30};
	uint8_t enter_cmd[] = {0x00, 0x00, 0x00, 0x00};

	I("%s: Entering\n", __func__);

	g_core_fp._dd_en_read(1);
	g_core_fp.fp_register_write(addr_display_on, DATA_LEN_4, enter_cmd, false);
	g_core_fp._dd_en_read(0);
	I("%s: End\n", __func__);
}

static void hx83112f_proximity_sleep_out(void)
{
	uint8_t addr_sleep_out[] = {0x00, 0x10, 0x01, 0x30};
	uint8_t enter_cmd[] = {0x00, 0x01, 0x00, 0x00};

	uint8_t addr_wakeup[] = {0x33, 0x00, 0x00, 0x00};
	uint8_t on_wakeup[] = {0x11, 0x00, 0x00, 0x00};

	I("%s: Entering\n", __func__);

	himax_bus_write(addr_wakeup[0], on_wakeup, 1, HIMAX_I2C_RETRY_TIMES);
	msleep(10);

	g_core_fp._dd_en_read(1);
	g_core_fp.fp_register_write(addr_sleep_out, DATA_LEN_4, enter_cmd, false);
	usleep_range(20000, 20001);


	I("%s: End\n", __func__);
}

static void hx83112_func_re_init(void)
{
	g_core_fp.fp_sense_off = hx83112_sense_off;
	g_core_fp.fp_chip_init = hx83112_chip_init;
	g_core_fp._dd_en_read = hx83112f_dd_en_read;
	g_core_fp._proximity_display_off = hx83112f_proximity_display_off;
	g_core_fp._proximity_sleep_in = hx83112f_proximity_sleep_in;
	g_core_fp._proximity_display_on =  hx83112f_proximity_display_on;
	g_core_fp._proximity_sleep_out = hx83112f_proximity_sleep_out;
#if defined(HX_ZERO_FLASH)
	g_core_fp.fp_system_reset = himax_hx83112f_system_reset;
#endif
}

static void hx83112_reg_re_init(void)
{
	// (*kp_private_ts)->ic_notouch_frame = hx83112_notouch_frame;
}


static void hx83112f_func_re_init(void)
{
#if defined(HX_ZERO_FLASH)
	g_core_fp.fp_resume_ic_action = himax_hx83112f_resume_ic_action;
	g_core_fp.fp_sense_on = himax_hx83112f_sense_on;
	g_core_fp.fp_0f_reload_to_active = himax_hx83112f_reload_to_active;
#endif
	g_core_fp._read_proxy_1b = hx83112f_read_proxy1b;
}

static void hx83112f_reg_re_init(void)
{
	kp_himax_in_parse_assign_cmd(hx83112f_fw_addr_raw_out_sel,
		(*kp_pfw_op)->addr_raw_out_sel,
		sizeof((*kp_pfw_op)->addr_raw_out_sel));
	// (*kp_private_ts)->ic_notouch_frame = hx83112f_notouch_frame;
	kp_himax_in_parse_assign_cmd(hx83112f_addr_ic_ver_name,
		(*kp_pfw_op)->addr_ver_ic_name,
		sizeof((*kp_pfw_op)->addr_ver_ic_name));
}

static bool hx83112_chip_detect(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	bool ret_data = false;
	int ret = 0;
	int i = 0;

	if (himax_ic_setup_external_symbols())
		return false;


	ret = himax_mcu_in_cmd_struct_init();
	if (ret < 0) {
		ret_data = false;
		E("%s:cmd_struct_init Fail:\n", __func__);
		return ret_data;
	}

	himax_mcu_in_cmd_init();

	hx83112_reg_re_init();
	hx83112_func_re_init();

	ret = himax_bus_read((*kp_pic_op)->addr_conti[0],
			tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return false;
	}

	if (g_core_fp.fp_sense_off(false) == false) {
		ret_data = false;
		E("%s:fp_sense_off Fail:\n", __func__);
		return ret_data;
	}
	for (i = 0; i < 5; i++) {
		ret = g_core_fp.fp_register_read(
			(*kp_pfw_op)->addr_icid_addr,
			DATA_LEN_4,
			tmp_data,
			false);

		if (ret != 0) {
			ret_data = false;
			E("%s:fp_register_read Fail:\n", __func__);
			return ret_data;
		}
		I("%s:Read driver IC ID = %X, %X, %X\n", __func__,
				tmp_data[3], tmp_data[2], tmp_data[1]);

		if ((tmp_data[3] == 0x83)
		&& (tmp_data[2] == 0x11)
		&& ((tmp_data[1] == 0x2f))) {
			strlcpy((*kp_private_ts)->chip_name,
				HX_83112F_SERIES_PWON, 30);
			// (*kp_ic_data)->ic_adc_num =
			// 	hx83112f_data_adc_num;
#if defined(HX_ZERO_FLASH) && defined(HX_OPT_HW_CRC)
			g_zf_opt_crc.fw_addr = -1;
#endif
			(*kp_ic_data)->dsram_size = hx83112f_dsram_size;
			(*kp_ic_data)->isram_size = hx83112f_isram_size;
			(*kp_ic_data)->dsram_addr = hx83112f_dsram_addr;
			(*kp_ic_data)->isram_addr = hx83112f_isram_addr;

			hx83112f_reg_re_init();
			hx83112f_func_re_init();


			I("%s:IC name = %s\n", __func__,
				(*kp_private_ts)->chip_name);

			I("Himax IC package %x%x%x in\n", tmp_data[3],
					tmp_data[2], tmp_data[1]);
			ret_data = true;
			goto FINAL;
		} else {
			ret_data = false;
			E("%s:Read driver ID register Fail:\n", __func__);
			E("Could NOT find Himax Chipset\n");
			E("Please check 1.VCCD,VCCA,VSP,VSN\n");
			E("2. LCM_RST,TP_RST\n");
			E("3. Power On Sequence\n");
		}
	}
FINAL:

	return ret_data;
}

DECLARE(HX_MOD_KSYM_HX83112);

#if 0
static int himax_hx83112_probe(void)
{
	I("%s:Enter\n", __func__);
	himax_add_chip_dt(hx83112_chip_detect);

	return 0;
}

static int himax_hx83112_remove(void)
{
	free_chip_dt_table();
	return 0;
}

static int __init himax_hx83112_init(void)
{
	int ret = 0;

	I("%s\n", __func__);
	ret = himax_hx83112_probe();
	return 0;
}

static void __exit himax_hx83112_exit(void)
{
	himax_hx83112_remove();
}
#endif

bool _hx83112_init(void)
{
	bool ret = false;

	I("%s\n", __func__);
	ret = hx83112_chip_detect();
	return ret;
}


