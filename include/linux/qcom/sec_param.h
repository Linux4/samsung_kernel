/*
 * include/linux/sec_param.h
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

struct sec_param_data {
	unsigned int debuglevel;
	unsigned int uartsel;
	unsigned int rory_control;
	unsigned int movinand_checksum_done;
	unsigned int movinand_checksum_pass;
	unsigned int cp_debuglevel;
#ifdef CONFIG_GSM_MODEM_SPRD6500
	unsigned int update_cp_bin;
#endif
#ifdef CONFIG_RTC_AUTO_PWRON_PARAM
	unsigned int boot_alarm_set;
	unsigned int boot_alarm_value_l;
	unsigned int boot_alarm_value_h;
#endif
#ifdef CONFIG_SEC_MONITOR_BATTERY_REMOVAL
	unsigned int normal_poweroff;
#endif
#ifdef CONFIG_RESTART_REASON_SEC_PARAM
	unsigned int param_restart_reason;
#endif
#ifdef CONFIG_SKU_THEME
	char param_skutheme_info[32];
#endif
#ifdef CONFIG_BARCODE_PAINTER
	char param_barcode_info[1024];
#else
        char reserved4[1024];
#endif

	char edit_cmdline[128];
	unsigned int afc_disable;
};

struct sec_param_data_s {
	struct work_struct sec_param_work;
	struct completion work;
	void *value;
	unsigned int offset;
	unsigned int size;
	unsigned int direction;
};

enum sec_param_index {
	param_index_debuglevel,
	param_index_uartsel,
	param_rory_control,
	param_index_movinand_checksum_done,
	param_index_movinand_checksum_pass,
	param_cp_debuglevel,
#ifdef CONFIG_GSM_MODEM_SPRD6500
	param_update_cp_bin,
#endif
#ifdef CONFIG_RTC_AUTO_PWRON_PARAM
	param_index_boot_alarm_set,
	param_index_boot_alarm_value_l,
	param_index_boot_alarm_value_h,
#endif
#ifdef CONFIG_SEC_MONITOR_BATTERY_REMOVAL
	param_index_normal_poweroff,
#endif
#ifdef CONFIG_RESTART_REASON_SEC_PARAM
	param_index_restart_reason,
#endif
#ifdef CONFIG_SKU_THEME
	param_index_skutheme_info,
#endif
#ifdef CONFIG_BARCODE_PAINTER
	param_index_barcode_info,
#endif
	param_index_edit_cmdline,
	param_index_afc_disable,
	param_index_max_sec_param_data,
#ifdef CONFIG_USER_RESET_DEBUG
	param_index_reset_ex_info,
#endif
};

extern bool sec_get_param(enum sec_param_index index, void *value);
extern bool sec_set_param(enum sec_param_index index, void *value);
#define SEC_PARAM_FILE_SIZE	(0xA00000)		/* 10MB */
#define SEC_PARAM_FILE_OFFSET (SEC_PARAM_FILE_SIZE - 0x100000)
#define SECTOR_UNIT_SIZE (4096) /* UFS */

#ifdef CONFIG_USER_RESET_DEBUG 
#define SEC_PARAM_EX_INFO_OFFSET (SEC_PARAM_FILE_SIZE - (3 * SECTOR_UNIT_SIZE))
#endif

