/*
 * =================================================================
 *
 *
 *	Description:  samsung display common file
 *
 *	Author: jb09.kim
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2012, Samsung Electronics. All rights reserved.

 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef SS_WRAPPER_H
#define SS_WRAPPER_H

#include "ss_dsi_panel_common.h"

int ss_wait_for_pm_resume(struct samsung_display_driver_data *vdd);

int ss_gpio_set_value(struct samsung_display_driver_data *vdd, unsigned int gpio, int value);
int ss_gpio_get_value(struct samsung_display_driver_data *vdd, unsigned int gpio);

int ss_sde_vm_owns_hw(struct samsung_display_driver_data *vdd);
void ss_sde_vm_lock(struct samsung_display_driver_data *vdd, bool lock);
bool ss_gpio_is_valid(int number);
int ss_gpio_to_irq(unsigned gpio);
int ss_wrapper_dsi_panel_tx_cmd_set(struct dsi_panel *panel, int type);
int ss_wrapper_regulator_get_voltage(struct samsung_display_driver_data *vdd, struct regulator *regulator);
int ss_wrapper_regulator_set_voltage(struct samsung_display_driver_data *vdd, struct regulator *regulator,
		int min_uV, int max_uV);
int ss_wrapper_regulator_set_short_detection(struct samsung_display_driver_data *vdd,
		struct regulator *regulator, bool enable, int lv_uA);
int ss_wrapper_misc_register(struct samsung_display_driver_data *vdd, struct miscdevice *misc);
int ss_wrapper_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
int ss_wrapper_spi_sync(struct spi_device *spi, struct spi_message *message);

struct ss_cmd_set;
int ss_wrapper_parse_cmd_sets_each_mode(struct samsung_display_driver_data *vdd);
int ss_wrapper_restore_backup_cmd(struct samsung_display_driver_data *vdd,
					int type, int offset, u8 addr);

int ss_wrapper_parse_cmd_sets(struct samsung_display_driver_data *vdd,
				struct ss_cmd_set *set, int type,
				struct device_node *np, char *cmd_name);
int __ss_parse_ss_cmd_sets_revision(struct samsung_display_driver_data *vdd,
					struct ss_cmd_set *set,
					int type, struct device_node *np,
					char *cmd_name);
u8 *ss_wrapper_of_get_property(const struct device_node *np,
					const char *name, int *lenp);
int ss_wrapper_of_get_named_gpio(struct device_node *np,
					const char *name, int index);
struct ss_cmd_set *ss_get_ss_cmds(struct samsung_display_driver_data *vdd, int type);
bool is_ss_style_cmd(struct samsung_display_driver_data *vdd, int type);
int ss_is_prop_string_style(struct device_node *np, char *cmd_name);
int ss_print_rx_buf(struct samsung_display_driver_data *vdd, int type);
int ss_get_rx_buf_addr(struct samsung_display_driver_data *vdd, int type,
			u8 addr, int cmd_order, u8 *out_buf, int out_len);
int ss_get_rx_buf(struct samsung_display_driver_data *vdd, int type,
			int cmd_order, u8 *out_buf);
int ss_get_all_rx_buf(struct samsung_display_driver_data *vdd, int type,
			u8 *out_buf);
int ss_send_cmd_get_rx(struct samsung_display_driver_data *vdd, int type,
				u8 *out_buf);
#endif /* SS_WRAPPER_H */
