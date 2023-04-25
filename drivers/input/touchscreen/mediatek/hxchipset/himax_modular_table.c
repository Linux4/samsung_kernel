#include "himax_modular_table.h"

#if !defined(__HIMAX_HX852xH_MOD__) && !defined(__HIMAX_HX852xG_MOD__)
#if !defined(__HIMAX_HX852xJ_MOD__)
struct fw_operation **kp_pfw_op;
EXPORT_SYMBOL(kp_pfw_op);

struct ic_operation **kp_pic_op;
EXPORT_SYMBOL(kp_pic_op);

struct flash_operation **kp_pflash_op;
EXPORT_SYMBOL(kp_pflash_op);

struct driver_operation **kp_pdriver_op;
EXPORT_SYMBOL(kp_pdriver_op);
#endif
#endif

#if defined(HX_ZERO_FLASH) && defined(CONFIG_TOUCHSCREEN_HIMAX_INCELL)
struct zf_operation **kp_pzf_op;
EXPORT_SYMBOL(kp_pzf_op);

int *kp_G_POWERONOF;
EXPORT_SYMBOL(kp_G_POWERONOF);
#endif

unsigned char *kp_IC_CHECKSUM;
EXPORT_SYMBOL(kp_IC_CHECKSUM);

#if defined(HX_EXCP_RECOVERY)
u8 *kp_HX_EXCP_RESET_ACTIVATE;
EXPORT_SYMBOL(kp_HX_EXCP_RESET_ACTIVATE);
#endif

#if defined(HX_ZERO_FLASH) && defined(HX_CODE_OVERLAY)
#if defined(CONFIG_TOUCHSCREEN_HIMAX_INCELL)
uint8_t **kp_ovl_idx;
EXPORT_SYMBOL(kp_ovl_idx);
#endif
#endif

unsigned long *kp_FW_VER_MAJ_FLASH_ADDR;
EXPORT_SYMBOL(kp_FW_VER_MAJ_FLASH_ADDR);

unsigned long *kp_FW_VER_MIN_FLASH_ADDR;
EXPORT_SYMBOL(kp_FW_VER_MIN_FLASH_ADDR);

unsigned long *kp_CFG_VER_MAJ_FLASH_ADDR;
EXPORT_SYMBOL(kp_CFG_VER_MAJ_FLASH_ADDR);

unsigned long *kp_CFG_VER_MIN_FLASH_ADDR;
EXPORT_SYMBOL(kp_CFG_VER_MIN_FLASH_ADDR);

unsigned long *kp_CID_VER_MAJ_FLASH_ADDR;
EXPORT_SYMBOL(kp_CID_VER_MAJ_FLASH_ADDR);

unsigned long *kp_CID_VER_MIN_FLASH_ADDR;
EXPORT_SYMBOL(kp_CID_VER_MIN_FLASH_ADDR);

uint32_t *kp_CFG_TABLE_FLASH_ADDR;
EXPORT_SYMBOL(kp_CFG_TABLE_FLASH_ADDR);

uint32_t *kp_CFG_TABLE_FLASH_ADDR_T;
EXPORT_SYMBOL(kp_CFG_TABLE_FLASH_ADDR_T);

#if defined(HX_BOOT_UPGRADE) || defined(HX_ZERO_FLASH)
	int *kp_g_i_FW_VER;
EXPORT_SYMBOL(kp_g_i_FW_VER);

	int *kp_g_i_CFG_VER;
EXPORT_SYMBOL(kp_g_i_CFG_VER);

	int *kp_g_i_CID_MAJ;
EXPORT_SYMBOL(kp_g_i_CID_MAJ);

	int *kp_g_i_CID_MIN;
EXPORT_SYMBOL(kp_g_i_CID_MIN);

	const struct firmware **kp_hxfw;
EXPORT_SYMBOL(kp_hxfw);
#endif

#if defined(HX_TP_PROC_2T2R)
	bool *kp_Is_2T2R;
EXPORT_SYMBOL(kp_Is_2T2R);
#endif

#if defined(HX_USB_DETECT_GLOBAL)
	void (*kp_himax_cable_detect_func)(bool force_renew);
EXPORT_SYMBOL(kp_himax_cable_detect_func);
#endif

#if defined(HX_RST_PIN_FUNC)
	void (*kp_himax_rst_gpio_set)(int pinnum, uint8_t value);
EXPORT_SYMBOL(kp_himax_rst_gpio_set);
#endif

struct himax_ts_data **kp_private_ts;
EXPORT_SYMBOL(kp_private_ts);

struct himax_core_fp *kp_g_core_fp;
EXPORT_SYMBOL(kp_g_core_fp);

struct himax_ic_data **kp_ic_data;
EXPORT_SYMBOL(kp_ic_data);

#if !defined(__HIMAX_HX852xH_MOD__) && !defined(__HIMAX_HX852xG_MOD__)
	#if !defined(__HIMAX_HX852xJ_MOD__)
		void (*kp_himax_mcu_in_cmd_init)(void);
EXPORT_SYMBOL(kp_himax_mcu_in_cmd_init);

	int (*kp_himax_mcu_in_cmd_struct_init)(void);
EXPORT_SYMBOL(kp_himax_mcu_in_cmd_struct_init);
#else
	struct on_driver_operation **kp_on_pdriver_op;
EXPORT_SYMBOL(kp_on_pdriver_op);

	struct on_flash_operation **kp_on_pflash_op;
EXPORT_SYMBOL(kp_on_pflash_op);

	void (*kp_himax_mcu_on_cmd_init)(void);
EXPORT_SYMBOL(kp_himax_mcu_on_cmd_init);

	int (*kp_himax_mcu_on_cmd_struct_init)(void);
EXPORT_SYMBOL(kp_himax_mcu_on_cmd_struct_init);
	#endif
#else
struct on_driver_operation **kp_on_pdriver_op;
EXPORT_SYMBOL(kp_on_pdriver_op);

struct on_flash_operation **kp_on_pflash_op;
EXPORT_SYMBOL(kp_on_pflash_op);

void (*kp_himax_mcu_on_cmd_init)(void);
EXPORT_SYMBOL(kp_himax_mcu_on_cmd_init);

int (*kp_himax_mcu_on_cmd_struct_init)(void);
EXPORT_SYMBOL(kp_himax_mcu_on_cmd_struct_init);
#endif
void (*kp_himax_parse_assign_cmd)(uint32_t addr, uint8_t *cmd,
	int len);
EXPORT_SYMBOL(kp_himax_parse_assign_cmd);

int (*kp_himax_bus_read)(uint8_t command, uint8_t *data,
		uint32_t length, uint8_t toRetry);
EXPORT_SYMBOL(kp_himax_bus_read);

int (*kp_himax_bus_write)(uint8_t command, uint8_t *data,
		uint32_t length, uint8_t toRetry);
EXPORT_SYMBOL(kp_himax_bus_write);

void (*kp_himax_int_enable)(int enable);
EXPORT_SYMBOL(kp_himax_int_enable);

int (*kp_himax_ts_register_interrupt)(void);
EXPORT_SYMBOL(kp_himax_ts_register_interrupt);

uint8_t (*kp_himax_int_gpio_read)(int pinnum);
EXPORT_SYMBOL(kp_himax_int_gpio_read);

int (*kp_himax_gpio_power_config)(struct himax_i2c_platform_data *pdata);
EXPORT_SYMBOL(kp_himax_gpio_power_config);


#if !defined(HX_USE_KSYM)
struct himax_chip_entry hx_self_sym_lookup;
EXPORT_SYMBOL(hx_self_sym_lookup);
#else
struct himax_chip_entry *hx_self_sym_lookup;
EXPORT_SYMBOL(hx_self_sym_lookup);
#endif

#if defined(HX_USE_KSYM)

struct himax_chip_entry *hx_init_chip_entry(void)
{
	// int i = 0;
	if (hx_self_sym_lookup == NULL) {
		hx_self_sym_lookup =
			kzalloc(sizeof(struct himax_chip_entry), GFP_KERNEL);
		hx_self_sym_lookup->hx_ic_dt_num =
			sizeof(himax_ksym_lookup) / sizeof(char *);
		hx_self_sym_lookup->core_chip_dt =
			kzalloc(sizeof(struct himax_chip_detect) *
				hx_self_sym_lookup->hx_ic_dt_num,
				GFP_KERNEL);
	}
	return hx_self_sym_lookup;
}
EXPORT_SYMBOL(hx_init_chip_entry);

void hx_release_chip_entry(void)
{
	int i = 0, idx = -1;

	idx = himax_get_ksym_idx();
	if (idx >= 0)
		I("%s: there are sine IC!\n", __func__);

	if (hx_self_sym_lookup != NULL) {
		for (i = 0; i < hx_self_sym_lookup->hx_ic_dt_num; i++) {
			if (hx_self_sym_lookup->core_chip_dt[i].fp_chip_detect
				!= NULL) {
				kfree(
					hx_self_sym_lookup->core_chip_dt[i].
					fp_chip_detect);
				hx_self_sym_lookup->core_chip_dt[i].
					fp_chip_detect
					= NULL;
			}
		}
		hx_self_sym_lookup->hx_ic_dt_num = 0;
		kfree(hx_self_sym_lookup);
		hx_self_sym_lookup = NULL;
		I("%s, release hx_self_sym_lookup OK!\n", __func__);
	} else {
		I("%s, no need to release hx_self_sym_lookup!\n", __func__);
	}

}
EXPORT_SYMBOL(hx_release_chip_entry);

#else
void hx_release_chip_entry(void)
{
	int i, j, idx;
	struct himax_chip_entry *entry;

	idx = himax_get_ksym_idx();
	if (idx >= 0) {
		if (isEmpty(idx) != 0) {
			I("%s: no chip registered or entry clean up\n",
			__func__);
			return;
		}
		entry = get_chip_entry_by_index(idx);

		for (i = 0; i < entry->hx_ic_dt_num; i++) {
			if (entry->core_chip_dt
				[i].fp_chip_detect != NULL) {
				if (i == (entry->hx_ic_dt_num - 1)) {
					entry->core_chip_dt
						[i].fp_chip_detect = NULL;
					entry->hx_ic_dt_num = 0;
				} else {
					for (j = i; j < entry
						->hx_ic_dt_num; j++)
						entry->core_chip_dt
							[i].fp_chip_detect =
						entry->core_chip_dt
							[j].fp_chip_detect;

					entry->core_chip_dt
						[j].fp_chip_detect = NULL;
					entry->hx_ic_dt_num--;
				}
			}
		}
		if (entry->hx_ic_dt_num == 0) {
			kfree(entry->core_chip_dt);
			entry->core_chip_dt = NULL;
		}
	}
}

#endif




