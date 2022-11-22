#include "jadard_common.h"
#include "jadard_platform.h"
#include "jadard_module.h"

extern struct jadard_module_fp g_module_fp;
extern struct jadard_ts_data *pjadard_ts_data;
extern struct jadard_ic_data *pjadard_ic_data;
struct jadard_common_variable g_common_variable;

#if defined(JD_AUTO_UPGRADE_FW) || defined(JD_ZERO_FLASH)
extern char *jd_i_CTPM_firmware_name;
extern uint8_t *g_jadard_fw;
extern uint32_t g_jadard_fw_len;
#if defined(JD_ZERO_FLASH)
bool jd_g_f_0f_update = false;
/*hs03s_NM code for SR-AL5625-01-642 by yuli at 2022/5/16 start*/
bool jadard_fw_ready = false;
/*hs03s_NM code for SR-AL5625-01-642 by yuli at 2022/5/16 end*/
#endif
#endif

#ifdef JD_RST_PIN_FUNC
/*
 * Parameter signification:
 * reloadcfg int_off_on
 *   true      true : Reload config & HW reset & INT off->on
 *   true      false: Reload config & HW reset
 *   false     true : HW reset & INT off->on
 *   false     false: HW reset
*/
static void jadard_mcu_ic_reset(bool reload_cfg, bool int_off_on)
{
	JD_I("%s: reload_cfg = %d, int_off_on = %d\n", __func__, reload_cfg, int_off_on);
	pjadard_ts_data->rst_active = true;

	if (pjadard_ts_data->rst_gpio >= 0) {
		if (int_off_on) {
			jadard_int_enable(false);
		}

		g_module_fp.fp_pin_reset();

		if (reload_cfg) {
			g_module_fp.fp_report_data_reinit();
		}

		if (int_off_on) {
			jadard_int_enable(true);
		}
	}
}
#endif

static void jadard_mcu_ic_soft_reset(void)
{
	pjadard_ts_data->rst_active = true;
	jadard_int_enable(false);
	g_module_fp.fp_soft_reset();
	jadard_int_enable(true);
}

static uint8_t jadard_mcu_dd_register_write(uint8_t cmd, uint8_t *par, uint8_t par_len)
{
	return jadard_DbicWriteDDReg(cmd, par, par_len);
}

static uint8_t jadard_mcu_dd_register_read(uint8_t cmd, uint8_t *par, uint8_t par_len)
{
	return jadard_DbicReadDDReg(cmd, par, par_len);
}

static void jadard_mcu_touch_info_set(void)
{
	/* Data set by jadard_parse_dt() */
	JD_I("%s: JD_X_NUM = %d, JD_Y_NUM = %d, JD_MAX_PT = %d\n", __func__,
		pjadard_ic_data->JD_X_NUM, pjadard_ic_data->JD_Y_NUM, pjadard_ic_data->JD_MAX_PT);
	JD_I("%s: JD_X_RES = %d, JD_Y_RES = %d\n", __func__, pjadard_ic_data->JD_X_RES, pjadard_ic_data->JD_Y_RES);
	JD_I("%s: JD_INT_EDGE = %d\n", __func__, pjadard_ic_data->JD_INT_EDGE);
}

static void jadard_mcu_report_data_reinit(void)
{
	if (jadard_report_data_init()) {
		JD_E("%s: allocate data fail\n", __func__);
	}
}

static void jadard_mcu_report_points(struct jadard_ts_data *ts)
{
	jadard_report_points(ts);
}

static int jadard_mcu_parse_report_data(struct jadard_ts_data *ts, int irq_event, int ts_status)
{
	return jadard_parse_report_data(ts, irq_event, ts_status);
}

static int jadard_mcu_distribute_touch_data(struct jadard_ts_data *ts, uint8_t *buf, int irq_event, int ts_status)
{
	return jadard_distribute_touch_data(ts, buf, irq_event, ts_status);
}

static void jadard_mcu_log_touch_state(void)
{
	jadard_log_touch_state();
}

#if defined(JD_SMART_WAKEUP) || defined(JD_USB_DETECT_GLOBAL)|| defined(JD_HIGH_SENSITIVITY)
static void jadard_mcu_resume_set_func(bool suspended)
{
#ifdef JD_SMART_WAKEUP
	g_module_fp.fp_set_SMWP_enable(pjadard_ts_data->SMWP_enable);
#endif
#ifdef JD_USB_DETECT_GLOBAL
	jadard_cable_detect(true);
#endif
#ifdef JD_HIGH_SENSITIVITY
	g_module_fp.fp_set_high_sensitivity(pjadard_ts_data->high_sensitivity_enable);
#endif
}
#endif

#ifdef JD_ZERO_FLASH
int jadard_mcu_0f_upgrade_fw(char *file_name)
{
	int err = JD_NO_ERR;
	const struct firmware *fw = NULL;

/*hs03s  code for DEVAL5625-1915 by wangdeyan at 20210706 start*/
	jadard_int_enable(false);

	JD_I("file name = %s\n", file_name);

	err = request_firmware(&fw, file_name, pjadard_ts_data->dev);

	if (err < 0) {
		JD_E("%s: Open file fail(ret:%d)\n", __func__, err);
		return err;
	} else {
		if (g_jadard_fw_len > fw->size) {
			memcpy(g_jadard_fw, fw->data, sizeof(uint8_t) * fw->size);
		} else {
			JD_E("%s: Memory overflow\n", __func__);
			err = JD_WRITE_OVERFLOW;
			goto fw_upgrade_fail;
		}
	}
/*hs03s  code for DEVAL5625-1915 by wangdeyan at 20210706 end*/

	if (jd_g_f_0f_update) {
		JD_W("%s: Other thread is upgrade now\n", __func__);
		err = JD_UPGRADE_CONFLICT;
	} else {
		JD_I("%s: Entering upgrade Flow\n", __func__);
		jd_g_f_0f_update = true;

		JD_I("FW size = %d\n", (int)fw->size);
		err = g_module_fp.fp_ram_write(0, g_jadard_fw, fw->size);
		jd_g_f_0f_update = false;

		/*hs03s_NM code for SR-AL5625-01-642 by yuli at 2022/5/16 start*/
		if (err >= 0) {
			jadard_fw_ready = true;
		}
		/*hs03s_NM code for SR-AL5625-01-642 by yuli at 2022/5/16 end*/
	}
fw_upgrade_fail:
	release_firmware(fw);

	return err;
}

void jadard_mcu_0f_operation(struct work_struct *work)
{
	/*hs03s  code for SR-AL5625-01-96 by wangdeyan at 20210519 start*/
	int err = g_module_fp.fp_0f_upgrade_fw(pjadard_ts_data->firmware_name);
	/*hs03s  code for SR-AL5625-01-96 by wangdeyan at 20210519 end*/

	if (err >= 0) {
		g_module_fp.fp_read_fw_ver();
	}
	jadard_int_enable(true);
}
#endif

static void jadard_mcu_fp_init(void)
{
#ifdef JD_RST_PIN_FUNC
	g_module_fp.fp_ic_reset              = jadard_mcu_ic_reset;
#endif
	g_module_fp.fp_ic_soft_reset         = jadard_mcu_ic_soft_reset;
	g_module_fp.fp_dd_register_read      = jadard_mcu_dd_register_read;
	g_module_fp.fp_dd_register_write     = jadard_mcu_dd_register_write;
	g_module_fp.fp_touch_info_set        = jadard_mcu_touch_info_set;
	g_module_fp.fp_report_data_reinit    = jadard_mcu_report_data_reinit;
	g_module_fp.fp_report_points         = jadard_mcu_report_points;
	g_module_fp.fp_parse_report_data     = jadard_mcu_parse_report_data;
	g_module_fp.fp_distribute_touch_data = jadard_mcu_distribute_touch_data;
	g_module_fp.fp_get_freq_band         = NULL; /* Not all chip support */
	g_module_fp.fp_log_touch_state       = jadard_mcu_log_touch_state;
#if defined(JD_SMART_WAKEUP) || defined(JD_USB_DETECT_GLOBAL)|| defined(JD_HIGH_SENSITIVITY)
	g_module_fp.fp_resume_set_func       = jadard_mcu_resume_set_func;
#endif
#ifdef JD_ZERO_FLASH
	g_module_fp.fp_0f_operation          = jadard_mcu_0f_operation;
	g_module_fp.fp_0f_upgrade_fw         = jadard_mcu_0f_upgrade_fw;
#endif
}

void jadard_mcu_cmd_struct_init(void)
{
	JD_D("%s: Entering!\n", __func__);

	jadard_mcu_fp_init();
}
