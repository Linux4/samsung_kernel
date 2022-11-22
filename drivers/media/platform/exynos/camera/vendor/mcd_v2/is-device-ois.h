/*
 * Samsung Exynos5 SoC series IS OIS driver
 *
 * exynos5 is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

struct is_ois_gpio {
	char *sda;
	char *scl;
	char *pinname;
};

struct is_device_ois {
	struct v4l2_device				v4l2_dev;
	struct platform_device				*pdev;
	unsigned long					state;
	struct exynos_platform_is_sensor		*pdata;
	struct i2c_client				*client;
	struct is_ois_ops				*ois_ops;
	int ois_en;
	bool ois_hsi2c_status;
	bool not_crc_bin;
};

struct is_ois_exif {
	int error_data;
	int status_data;
};

struct is_ois_info {
	char	header_ver[7];
	char	load_fw_name[50];
	u8 checksum;
	u8 caldata;
};

#define OIS_FW_NAME_SEC		"ois_fw_sec.bin"
#define OIS_FW_NAME_SEC_2		"ois_fw_sec_2.bin"
#define OIS_FW_NAME_DOM		"ois_fw_dom.bin"
#define IS_OIS_SDCARD_PATH		"/data/media/0/"
#define OIS_BOOT_FW_SIZE	(1024 * 4)
#define OIS_PROG_FW_SIZE	(1024 * 24)

void is_ois_i2c_config(struct i2c_client *client, bool onoff);
int is_ois_i2c_read(struct i2c_client *client, u16 addr, u8 *data);
int is_ois_i2c_write(struct i2c_client *client ,u16 addr, u8 data);
int is_ois_i2c_write_multi(struct i2c_client *client ,u16 addr, u8 *data, size_t size);
int is_ois_i2c_read_multi(struct i2c_client *client, u16 addr, u8 *data, size_t size);
void is_ois_enable(struct is_core *core);
void is_ois_offset_test(struct is_core *core, long *raw_data_x, long *raw_data_y);
int is_ois_self_test(struct is_core *core);
bool is_ois_check_sensor(struct is_core *core);
int is_ois_gpio_on(struct is_core *core);
int is_ois_gpio_off(struct is_core *core);
int is_ois_get_module_version(struct is_ois_info **minfo);
int is_ois_get_phone_version(struct is_ois_info **minfo);
int is_ois_get_user_version(struct is_ois_info **uinfo);
bool is_ois_version_compare(char *fw_ver1, char *fw_ver2, char *fw_ver3);
bool is_ois_version_compare_default(char *fw_ver1, char *fw_ver2);
void is_ois_get_offset_data(struct is_core *core, long *raw_data_x, long *raw_data_y);
bool is_ois_diff_test(struct is_core *core, int *x_diff, int *y_diff);
bool is_ois_crc_check(struct is_core *core, char *buf);
u16 is_ois_calc_checksum(u8 *data, int size);

void is_ois_exif_data(struct is_core *core);
int is_ois_get_exif_data(struct is_ois_exif **exif_info);
void is_ois_fw_status(struct is_core *core);
void is_ois_fw_update(struct is_core *core);
bool is_ois_check_fw(struct is_core *core);
bool is_ois_auto_test(struct is_core *core,
	                int threshold, bool *x_result, bool *y_result, int *sin_x, int *sin_y);
void is_ois_gyro_sleep(struct is_core *core);
