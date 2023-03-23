/*
 * create in 2021/1/7.
 * create emmc node in  /proc/bootdevice
 */
#ifndef _SPRD_PROC_BOOTDEVICE_H_
#define _SPRD_PROC_BOOTDEVICE_H_
#include <linux/proc_fs.h>
#include <linux/proc_ns.h>
#include <linux/mmc/mmc.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/mmc/card.h>
#define MAX_NAME_LEN 32

struct __bootdevice {
	const struct device *dev;
//	struct mmc_cid cid;
	u32 raw_cid[4];
	u32 raw_csd[4];
	char product_name[MAX_NAME_LEN + 1];
	u8 life_time_est_typ_a;
	u8 life_time_est_typ_b;
	u8 rev;
	unsigned int manfid;
	unsigned short oemid;
	unsigned char hwrev;
	u8 fwrev[MMC_FIRMWARE_LEN];
	u8 pre_eol_info;
	unsigned long long enhanced_area_offset;
	unsigned int enhanced_area_size;
	unsigned int serial;
	unsigned char prv;
	u32	ocr;
	unsigned int erase_size;
	unsigned int size;
	u8 type;
	const char *name;
	u8 ext_csd[512];
};

int sprd_create_bootdevice_entry(void);
void set_bootdevice_cid(u32 *cid);
void set_bootdevice_csd(u32 *csd);
void set_bootdevice_serial(unsigned int serial);
void set_bootdevice_product_name(char *product_name);
void set_bootdevice_manfid(unsigned int manfid);
void set_bootdevice_oemid(unsigned short oemid);
void set_bootdevice_fwrev(u8 *fwrev);
void set_bootdevice_ife_time_est_typ(u8 life_time_est_typ_a, u8 life_time_est_typ_b);
void set_bootdevice_rev(u8 rev);
void set_bootdevice_pre_eol_info(u8 pre_eol_info);
void set_bootdevice_enhanced_area_offset(unsigned long long enhanced_area_offset);
void set_bootdevice_enhanced_area_size(unsigned int enhanced_area_size);
void set_bootdevice_prv(unsigned char prv);
void set_bootdevice_hwrev(unsigned char hwrev);
void set_bootdevice_ocr(u32	ocr);
void set_bootdevice_erase_size(unsigned int erase_size);
void set_bootdevice_size(unsigned int size);
void set_bootdevice_type(void);
void set_bootdevice_name(const char *name);
void set_bootdevice_ext_csd(u8 *ext_csd);
#endif
