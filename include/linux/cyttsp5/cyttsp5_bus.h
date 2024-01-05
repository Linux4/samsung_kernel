/*
 * cyttsp5_bus.h
 * Cypress TrueTouch(TM) Standard Product V5 Bus Driver.
 * For use with Cypress Txx5xx parts.
 * Supported parts include:
 * TMA5XX
 *
 * Copyright (C) 2012-2013 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#ifndef _LINUX_CYTTSP5_BUS_H
#define _LINUX_CYTTSP5_BUS_H

#include <linux/device.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/limits.h>

/* TTHE Tuner Support */
#define TTHE_TUNER_SUPPORT

extern struct bus_type cyttsp5_bus_type;

struct cyttsp5_driver;
struct cyttsp5_device;
struct cyttsp5_adapter;

enum cyttsp5_atten_type {
	CY_ATTEN_IRQ,
	CY_ATTEN_STARTUP,
	CY_ATTEN_EXCLUSIVE,
	CY_ATTEN_NUM_ATTEN,
};

typedef int (*cyttsp5_atten_func) (struct cyttsp5_device *);

struct cyttsp5_ops {
	int (*read_default)(struct cyttsp5_adapter *adap,
		void *buf, int size);
	int (*read_default_nosize)(struct cyttsp5_adapter *adap,
		void *buf, int max);
	int (*write_read_specific)(struct cyttsp5_adapter *adap, u8 write_len,
		u8 *write_buf, u8 *read_buf);
};

struct cyttsp5_adapter {
	struct list_head node;
	char id[NAME_MAX];
	struct device *dev;
	int (*read_default)(struct cyttsp5_adapter *adap,
		void *buf, int size);
	int (*read_default_nosize)(struct cyttsp5_adapter *adap,
		void *buf, int max);
	int (*write_read_specific)(struct cyttsp5_adapter *adap, u8 write_len,
		u8 *write_buf, u8 *read_buf);
};

struct cyttsp5_core_info {
	char const *name;
	char const *id;
	char const *adap_id;
	void *platform_data;
};

struct cyttsp5_core {
	struct list_head node;
	char const *name;
	char const *id;
	char const *adap_id;
	struct device dev;
	struct cyttsp5_adapter *adap;
};
#define to_cyttsp5_core(d) container_of(d, struct cyttsp5_core, dev)

struct cyttsp5_device_info {
	char const *name;
	char const *core_id;
	void *platform_data;
};

struct cyttsp5_device {
	struct list_head node;
	char const *name;
	char const *core_id;
	struct device dev;
	struct cyttsp5_core *core;
};
#define to_cyttsp5_device(d) container_of(d, struct cyttsp5_device, dev)

struct cyttsp5_core_nonhid_cmd {
	int (*start_bl) (struct cyttsp5_device *ttsp, int protect);
	int (*suspend_scanning) (struct cyttsp5_device *ttsp, int protect);
	int (*resume_scanning) (struct cyttsp5_device *ttsp, int protect);
	int (*get_param) (struct cyttsp5_device *ttsp, int protect,
			u8 param_id, u32 *value);
	int (*set_param) (struct cyttsp5_device *ttsp, int protect,
			u8 param_id, u32 value);
	int (*verify_config_block_crc) (struct cyttsp5_device *ttsp,
			int protect, u8 ebid, u8 *status, u16 *calculated_crc,
			u16 *stored_crc);
	int (*get_config_row_size) (struct cyttsp5_device *ttsp,
			int protect, u16 *row_size);
	int (*calibrate_idacs) (struct cyttsp5_device *ttsp, int protect,
			u8 mode);
	int (*initialize_baselines) (struct cyttsp5_device *ttsp, int protect,
			u8 test_id);
	int (*exec_panel_scan) (struct cyttsp5_device *ttsp, int protect);
	int (*retrieve_panel_scan) (struct cyttsp5_device *ttsp, int protect,
			u16 read_offset, u16 read_count, u8 data_id,
			u8 *response, u8 *config, u16 *actual_read_len,
			u8 *read_buf);
	int (*write_conf_block) (struct cyttsp5_device *ttsp, int protect,
		u16 row_number, u16 write_length, u8 ebid, u8 *write_buf,
		u8 *security_key, u16 *actual_write_len);
	int (*user_cmd)(struct cyttsp5_device *ttsp, int protect,
			u16 read_len, u8 *read_buf,
			u16 write_len, u8 *write_buf,
			u16 *actual_read_len);
	int (*get_bl_info) (struct cyttsp5_device *ttsp, int protect,
			u8 *return_data);
	int (*initiate_bl) (struct cyttsp5_device *ttsp, int protect,
		u16 key_size, u8 *key_buf, u16 row_size, u8 *metadata_row_buf);
	int (*launch_app) (struct cyttsp5_device *ttsp, int protect);
	int (*prog_and_verify) (struct cyttsp5_device *ttsp, int protect,
			u16 data_len, u8 *data_buf);
	int (*verify_app_integrity) (struct cyttsp5_device *ttsp, int protect,
			u8 *result);
};

struct cyttsp5_core_driver {
	struct device_driver driver;
	int (*probe)(struct cyttsp5_core *core);
	int (*remove)(struct cyttsp5_core *core);
	int (*subscribe_attention)(struct cyttsp5_device *ttsp,
				enum cyttsp5_atten_type type,
				cyttsp5_atten_func func,
				int flags);
	int (*unsubscribe_attention)(struct cyttsp5_device *ttsp,
				enum cyttsp5_atten_type type,
				cyttsp5_atten_func func,
				int flags);
	int (*request_exclusive)(struct cyttsp5_device *ttsp, int timeout_ms);
	int (*release_exclusive)(struct cyttsp5_device *ttsp);
	int (*request_reset)(struct cyttsp5_device *ttsp);
	int (*request_restart)(struct cyttsp5_device *ttsp);
	struct cyttsp5_sysinfo *(*request_sysinfo)(struct cyttsp5_device *ttsp);
	struct cyttsp5_loader_platform_data
		*(*request_loader_pdata)(struct cyttsp5_device *ttsp);
	int (*request_stop_wd)(struct cyttsp5_device *ttsp);
	int (*request_get_hid_desc)(struct cyttsp5_device *ttsp, int protect);
	int (*request_get_mode)(struct cyttsp5_device *ttsp, int protect,
			u8 *mode);
#ifdef TTHE_TUNER_SUPPORT
	int (*request_tthe_print)(struct cyttsp5_device *ttsp, u8 *buf,
			int buf_len, const u8 *data_name);
#endif
	struct cyttsp5_core_nonhid_cmd *cmd;
};
#define to_cyttsp5_core_driver(d) \
	container_of(d, struct cyttsp5_core_driver, driver)

struct cyttsp5_driver {
	struct device_driver driver;
	int (*probe)(struct cyttsp5_device *dev);
	int (*remove)(struct cyttsp5_device *fev);
};
#define to_cyttsp5_driver(d) container_of(d, struct cyttsp5_driver, driver)

extern int cyttsp5_register_driver(struct cyttsp5_driver *drv);
extern void cyttsp5_unregister_driver(struct cyttsp5_driver *drv);

extern int cyttsp5_register_core_driver(struct cyttsp5_core_driver *drv);
extern void cyttsp5_unregister_core_driver(struct cyttsp5_core_driver *drv);

extern int cyttsp5_register_device(struct cyttsp5_device_info const *dev_info);
extern int cyttsp5_unregister_device(char const *name, char const *core_id);

extern int cyttsp5_register_core_device(
		struct cyttsp5_core_info const *core_info);

extern int cyttsp5_add_adapter(char const *id, struct cyttsp5_ops const *ops,
		struct device *parent);

extern int cyttsp5_del_adapter(char const *id);

static inline int cyttsp5_adap_read_default(struct cyttsp5_adapter *adap,
		void *buf, int size)
{
	return adap->read_default(adap, buf, size);
}

static inline int cyttsp5_adap_read_default_nosize(struct cyttsp5_adapter *adap,
		void *buf, int max)
{
	return adap->read_default_nosize(adap, buf, max);
}

static inline int cyttsp5_adap_write_read_specific(struct cyttsp5_adapter *adap,
		u8 write_len, u8 *write_buf, u8 *read_buf)
{
	return adap->write_read_specific(adap, write_len, write_buf, read_buf);
}

static inline int cyttsp5_subscribe_attention(struct cyttsp5_device *ttsp,
		enum cyttsp5_atten_type type, cyttsp5_atten_func func,
		int flags)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->subscribe_attention(ttsp, type, func, flags);
}

static inline int cyttsp5_unsubscribe_attention(struct cyttsp5_device *ttsp,
		enum cyttsp5_atten_type type, cyttsp5_atten_func func,
		int flags)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->unsubscribe_attention(ttsp, type, func, flags);
}

static inline int cyttsp5_request_exclusive(struct cyttsp5_device *ttsp,
		int timeout_ms)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->request_exclusive(ttsp, timeout_ms);
}

static inline int cyttsp5_release_exclusive(struct cyttsp5_device *ttsp)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->release_exclusive(ttsp);
}

static inline int cyttsp5_request_reset(struct cyttsp5_device *ttsp)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->request_reset(ttsp);
}

static inline int cyttsp5_request_restart(struct cyttsp5_device *ttsp)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->request_restart(ttsp);
}

static inline struct cyttsp5_sysinfo *cyttsp5_request_sysinfo(
		struct cyttsp5_device *ttsp)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->request_sysinfo(ttsp);
}

static inline struct cyttsp5_loader_platform_data *cyttsp5_request_loader_pdata(
		struct cyttsp5_device *ttsp)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->request_loader_pdata(ttsp);
}

static inline int cyttsp5_request_stop_wd(struct cyttsp5_device *ttsp)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->request_stop_wd(ttsp);
}

static inline int cyttsp5_request_nonhid_user_cmd(struct cyttsp5_device *ttsp,
	int protect, u16 read_len, u8 *read_buf, u16 write_len, u8 *write_buf,
	u16 *actual_read_len)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->cmd->user_cmd(ttsp, protect, read_len, read_buf,
			write_len, write_buf, actual_read_len);
}

static inline int cyttsp5_request_nonhid_verify_config_block_crc(
	struct cyttsp5_device *ttsp, int protect, u8 ebid, u8 *status,
	u16 *calculated_crc, u16 *stored_crc)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->cmd->verify_config_block_crc(ttsp, protect, ebid, status,
		calculated_crc, stored_crc);
}

static inline int cyttsp5_request_nonhid_calibrate_idacs(
	struct cyttsp5_device *ttsp, int protect, u8 mode)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->cmd->calibrate_idacs(ttsp, protect, mode);
}

static inline int cyttsp5_request_nonhid_exec_panel_scan(
	struct cyttsp5_device *ttsp, int protect)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->cmd->exec_panel_scan(ttsp, protect);
}

static inline int cyttsp5_request_nonhid_retrieve_panel_scan(
	struct cyttsp5_device *ttsp, int protect, u16 read_offset,
	u16 read_count, u8 data_id, u8 *response, u8 *config,
	u16 *actual_read_len, u8 *read_buf)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->cmd->retrieve_panel_scan(ttsp, protect, read_offset,
			read_count, data_id, response, config, actual_read_len,
			read_buf);
}

static inline int cyttsp5_request_nonhid_write_conf_block(
	struct cyttsp5_device *ttsp, int protect, u16 row_number,
	u16 write_length, u8 ebid, u8 *write_buf, u8 *security_key,
	u16 *actual_write_len)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->cmd->write_conf_block(ttsp, protect, row_number, write_length,
			ebid, write_buf, security_key, actual_write_len);
}

static inline int cyttsp5_request_nonhid_start_bl(struct cyttsp5_device *ttsp,
		int protect)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->cmd->start_bl(ttsp, protect);
}

static inline int cyttsp5_request_nonhid_suspend_scanning(
		struct cyttsp5_device *ttsp, int protect)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->cmd->suspend_scanning(ttsp, protect);
}

static inline int cyttsp5_request_nonhid_resume_scanning(
		struct cyttsp5_device *ttsp, int protect)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->cmd->resume_scanning(ttsp, protect);
}

static inline int cyttsp5_request_nonhid_get_bl_info(
		struct cyttsp5_device *ttsp, int protect, u8 *return_data)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->cmd->get_bl_info(ttsp, protect, return_data);
}

static inline int cyttsp5_request_nonhid_initiate_bl(
		struct cyttsp5_device *ttsp, int protect, u16 key_size,
		u8 *key_buf, u16 row_size, u8 *metadata_row_buf)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->cmd->initiate_bl(ttsp, protect, key_size, key_buf,
			row_size, metadata_row_buf);
}

static inline int cyttsp5_request_nonhid_launch_app(
		struct cyttsp5_device *ttsp, int protect)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->cmd->launch_app(ttsp, protect);
}

static inline int cyttsp5_request_nonhid_prog_and_verify(
		struct cyttsp5_device *ttsp, int protect,
		u16 data_len, u8 *data_buf)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->cmd->prog_and_verify(ttsp, protect, data_len, data_buf);
}

static inline int cyttsp5_request_nonhid_verify_app_integrity(
		struct cyttsp5_device *ttsp, int protect, u8 *result)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->cmd->verify_app_integrity(ttsp, protect, result);
}

static inline int cyttsp5_request_get_hid_desc(struct cyttsp5_device *ttsp,
		int protect)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->request_get_hid_desc(ttsp, protect);
}

static inline int cyttsp5_request_get_mode(struct cyttsp5_device *ttsp,
		int protect, u8 *mode)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->request_get_mode(ttsp, protect, mode);
}

#ifdef TTHE_TUNER_SUPPORT
static inline int cyttsp5_request_tthe_print(struct cyttsp5_device *ttsp,
		u8 *buf, int buf_len, const u8 *data_name)
{
	struct cyttsp5_core *cd = ttsp->core;
	struct cyttsp5_core_driver *d = to_cyttsp5_core_driver(cd->dev.driver);
	return d->request_tthe_print(ttsp, buf, buf_len, data_name);
}
#endif

#endif /* _LINUX_CYTTSP5_BUS_H */
