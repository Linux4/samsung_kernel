/*
 * include/linux/sec_param.h
 *
 * COPYRIGHT(C) 2011-2016 Samsung Electronics Co., Ltd. All Right Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */



struct fiemap_extent_p {
         unsigned long fe_logical;  /* logical offset in bytes for the start of
                             * the extent from the beginning of the file */
         unsigned long fe_physical; /* physical offset in bytes for the start
                             * of the extent from the beginning of the disk */
         unsigned long fe_length;   /* length in bytes for this extent */
         unsigned long fe_reserved64[2];
         unsigned int fe_flags;    /* FIEMAP_EXTENT_* flags for this extent */
         unsigned int fe_reserved[3];
};

struct fiemap_p {
         unsigned long fm_start;         /* logical offset (inclusive) at
                                  * which to start mapping (in) */
         unsigned long fm_length;        /* logical length of mapping which
                                  * userspace wants (in) */
         unsigned int fm_flags;         /* FIEMAP_FLAG_* flags for request (in/out) */
         unsigned int fm_mapped_extents;/* number of extents that were mapped (out) */
         unsigned int fm_extent_count;  /* size of fm_extents array (in) */
         unsigned int fm_reserved;
         struct fiemap_extent_p fm_extents[128]; /* array of mapped extents (out) */
};

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
	unsigned int sapa[3];
#else
	unsigned int reserved1[3];
#endif
#ifdef CONFIG_SEC_MONITOR_BATTERY_REMOVAL
	unsigned int normal_poweroff;
#else
	unsigned int reserved2;
#endif
#ifdef CONFIG_WIRELESS_IC_PARAM
	unsigned int wireless_ic;
#else
	unsigned int reserved3;
#endif
	char reserved4[80];
	char reserved5[80];
	char reserved6[80];
	char reserved7[80];
	char reserved8[80];
#ifdef CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE
	unsigned int wireless_charging_mode;
#else
	unsigned int reserved5;
#endif
#if defined(CONFIG_MUIC_HV) || defined(CONFIG_SUPPORT_QC30)
	unsigned int afc_disable;
#else
	unsigned int reserved6;
#endif
	unsigned int cp_reserved_mem;
	char reserved9[4];
 	char reserved10[4];
	char param_lcd_resolution[8]; // Variable LCD resolution
	char reserved11[16];
	unsigned int api_gpio_test;
	char api_gpio_test_result[256];
	char reboot_recovery_cause[256];
	unsigned int user_partition_flashed;
	unsigned int force_upload_flag;
	unsigned int cp_reserved_mem_backup;
	unsigned int FMM_lock;
	unsigned int dump_sink;
	unsigned int fiemap_update;
	struct fiemap_p fiemap_result;
};

struct sec_param_data_s {
	struct work_struct sec_param_work;
	struct completion work;
	void *value;
	unsigned int offset;
	unsigned int size;
	unsigned int direction;
	bool success;
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
	param_index_sapa,
#endif
#ifdef CONFIG_SEC_MONITOR_BATTERY_REMOVAL
	param_index_normal_poweroff,
#endif
#ifdef CONFIG_WIRELESS_IC_PARAM
	param_index_wireless_ic,
#endif
#ifdef CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE
	param_index_wireless_charging_mode,
#endif
#if defined(CONFIG_MUIC_HV) || defined(CONFIG_SUPPORT_QC30)
	param_index_afc_disable,
#endif
	param_index_cp_reserved_mem,
	param_index_lcd_resolution,
	param_index_api_gpio_test,
	param_index_api_gpio_test_result,
	param_index_reboot_recovery_cause,
	param_index_user_partition_flashed,
	param_index_force_upload_flag,
	param_index_cp_reserved_mem_backup,
	param_index_FMM_lock,
	param_index_dump_sink,
	param_index_fiemap_update,
	param_index_fiemap_result,
#ifdef CONFIG_SEC_QUEST
	param_index_quest,
	param_index_quest_ddr_result,
#endif
	param_index_max_sec_param_data,
};

extern bool sec_get_param(enum sec_param_index index, void *value);
extern bool sec_set_param(enum sec_param_index index, void *value);
extern bool sales_code_is(char*);

#define SEC_PARAM_FILE_SIZE	(0xA00000)	/* 10MB */
#define SEC_PARAM_FILE_OFFSET	(SEC_PARAM_FILE_SIZE - 0x100000)
#define SECTOR_UNIT_SIZE	(4096)		/* UFS */

#define FMMLOCK_MAGIC_NUM	0x464D4F4E

#ifdef CONFIG_SEC_QUEST
/* SEC QUEST region in PARAM partition */
#define SEC_PARAM_QUEST_OFFSET			(SEC_PARAM_FILE_OFFSET - 0x100000)
#define SEC_PARAM_QUEST_SIZE				(0x2000) /* 8KB */
#define SEC_PARAM_QUEST_DDR_RESULT_OFFSET	(SEC_PARAM_QUEST_OFFSET + SEC_PARAM_QUEST_SIZE) /* 8MB + 8KB */
#endif
