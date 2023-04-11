/************************************************************************
 * Copyright (c) 2012-2020, Focaltech Systems (R)£¬All Rights Reserved.
 *
 * File Name: focaltech_flash.h
 *
 * Author: Focaltech Driver Team
 *
 * Created: 2016-08-07
 *
 * Abstract:
 *
 ************************************************************************/
#ifndef __LINUX_FOCALTECH_FLASH_H__
#define __LINUX_FOCALTECH_FLASH_H__

/*****************************************************************************
 * 1.Included header files
 *****************************************************************************/
#include "focaltech_core.h"

/*****************************************************************************
 * Private constant and macro definitions using #define
 *****************************************************************************/

#define TSP_PATH_EXTERNAL_FW		"/sdcard/Firmware/TSP/tsp.bin"
#define TSP_PATH_EXTERNAL_FW_SIGNED	"/sdcard/Firmware/TSP/tsp_signed.bin"


enum FW_STATUS {
	FTS_RUN_IN_ERROR,
	FTS_RUN_IN_APP,
	FTS_RUN_IN_ROM,
	FTS_RUN_IN_PRAM,
	FTS_RUN_IN_BOOTLOADER,
};

enum FW_FLASH_MODE {
	FLASH_MODE_APP,
	FLASH_MODE_LIC,
	FLASH_MODE_PARAM,
	FLASH_MODE_ALL,
};

enum ECC_CHECK_MODE {
	ECC_CHECK_MODE_XOR,
	ECC_CHECK_MODE_CRC16,
};

enum UPGRADE_SPEC {
	UPGRADE_SPEC_V_1_0 = 0x0100,
};

/*****************************************************************************
 * Private enumerations, structures and unions using typedef
 *****************************************************************************/
/* IC info */
struct upgrade_func {
	u16 ctype[FTS_MAX_COMPATIBLE_TYPE];
	u32 fwveroff;
	u32 fwcfgoff;
	u32 appoff;
	u32 licoff;
	u32 paramcfgoff;
	u32 paramcfgveroff;
	u32 paramcfg2off;
	int pram_ecc_check_mode;
	int fw_ecc_check_mode;
	int upgspec_version;
	bool new_return_value_from_ic;
	bool appoff_handle_in_ic;
	bool is_reset_register_BC;
	bool read_boot_id_need_reset;
	bool hid_supported;
	bool pramboot_supported;
	u8 *pramboot;
	u32 pb_length;
	int (*init)(u8 *, u32);
	int (*write_pramboot_private)(void);
	int (*upgrade)(u8 *, u32);
	int (*get_hlic_ver)(u8 *);
	int (*lic_upgrade)(u8 *, u32);
	int (*param_upgrade)(u8 *, u32);
	int (*force_upgrade)(u8 *, u32);
};

struct upgrade_setting_nf {
	u8 rom_idh;
	u8 rom_idl;
	u16 reserved;
	u32 app2_offset;
	u32 ecclen_max;
	u8 eccok_val;
	u8 upgsts_boot;
	u8 delay_init;
	bool spi_pe;
	bool half_length;
	bool fd_check;
	bool drwr_support;
};

struct upgrade_module {
	int id;
	char vendor_name[MAX_MODULE_VENDOR_NAME_LEN];
	u8 *fw_file;
	u32 fw_len;
};

struct fts_upgrade {
	struct fts_ts_data *ts_data;
	struct upgrade_module *module_info;
	struct upgrade_func *func;
	struct upgrade_setting_nf *setting_nf;
	int module_id;
	bool fw_from_request;
	u8 *fw;
	u32 fw_length;
	u8 *lic;
	u32 lic_length;
};

/*****************************************************************************
 * Global variable or extern global variabls/functions
 *****************************************************************************/


/*****************************************************************************
 * Static function prototypes
 *****************************************************************************/
#endif
