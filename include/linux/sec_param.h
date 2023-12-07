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
#else
	unsigned int reserved0;
#endif
#ifdef CONFIG_RTC_AUTO_PWRON_PARAM
	unsigned int boot_alarm_set;
	unsigned int boot_alarm_value_l;
	unsigned int boot_alarm_value_h;
#else
	unsigned int reserved1[3];
#endif
#ifdef CONFIG_SEC_MONITOR_BATTERY_REMOVAL
	unsigned int normal_poweroff;
#else
	unsigned int reserved2;
#endif
	unsigned int reserved3;
#ifdef CONFIG_BARCODE_PAINTER
/* [000] : SEC_IMEI= ... */
/* [080] : SEC_MEID= ... */
/* [160] : SEC_SN= ... */
/* [240] : SEC_PR_DATE= ... */
/* [320] : SEC_SKU= ... */
	char param_barcode_info[1024];
#else
	char reserved4[1024];
#endif
#ifdef CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE
	unsigned int wireless_charging_mode;
#else
	unsigned int reserved5;
#endif
#if defined(CONFIG_MUIC_HV) || defined(CONFIG_MUIC_UNIVERSAL_SM5705_AFC)
	unsigned int afc_disable;
#else
	unsigned int reserved6;
#endif
	unsigned int cp_reserved_mem;
	char param_carrierid[4]; //only use 3digits, 1 for null
	char param_sales[4]; //only use 3digits, 1 for null
	char param_lcd_resolution[8]; // Variable LCD resolution
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
#ifdef CONFIG_BARCODE_PAINTER
	param_index_barcode_info,
#endif
#ifdef CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE
	param_index_wireless_charging_mode,
#endif
#if defined(CONFIG_MUIC_HV) || defined(CONFIG_MUIC_UNIVERSAL_SM5705_AFC)
	param_index_afc_disable,
#endif	
	param_index_cp_reserved_mem,
	param_index_lcd_resolution,
	param_index_max_sec_param_data,
#ifdef CONFIG_USER_RESET_DEBUG
	param_index_reset_ex_info,
	param_index_reset_summary_info,
	param_index_reset_summary,
	param_index_reset_klog_info,
	param_index_reset_klog,
#endif
};

extern bool sec_get_param(enum sec_param_index index, void *value);
extern bool sec_set_param(enum sec_param_index index, void *value);

#define SEC_PARAM_FILE_SIZE	(0xA00000)		/* 10MB */
#define SEC_PARAM_FILE_OFFSET (SEC_PARAM_FILE_SIZE - 0x100000)
#define SECTOR_UNIT_SIZE (4096) /* UFS */

#ifdef CONFIG_USER_RESET_DEBUG 
#define SEC_PARAM_DEBUG_HEADER_OFFSET	(4*1024*1024 - 4*1024)
#define SEC_PARAM_KLOG_OFFSET		(4*1024*1024)
#define SEC_PARAM_DUMP_SUMMARY_OFFSET	(6*1024*1024)
#define SEC_PARAM_KLOG_SIZE		(2*1024*1024)
#define SEC_PARAM_DUMP_SUMMARY_SIZE	(2*1024*1024)
#define SEC_PARAM_EX_INFO_OFFSET (SEC_PARAM_FILE_SIZE - (3 * SECTOR_UNIT_SIZE))
#endif

