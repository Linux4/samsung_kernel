/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_PROPERTY_H__
#define __PANEL_PROPERTY_H__

#include "panel_obj.h"

#define PANEL_PROP_NAME_LEN (32)

enum PANEL_PROP_TYPE {
	PANEL_PROP_TYPE_RANGE,
	MAX_PANEL_PROP_TYPE
};


#define __PANEL_PROPERTY_U32_INITIALIZER(_name, _value, _min, _max) { \
	.base = { .name = (_name), .cmd_type = (CMD_TYPE_PROP) }, \
	.type = (PANEL_PROP_TYPE_RANGE), \
	.value = (_value), .min = (_min), .max = (_max) }

struct panel_property {
	struct pnobj base;
	enum PANEL_PROP_TYPE type;
	unsigned int value;
	unsigned int min;
	unsigned int max;
};

const char *prop_type_to_string(u32 type);
int string_to_prop_type(const char *str);
char *get_panel_property_name(struct panel_property *property);
int panel_prop_compare(struct panel_property *prop1, struct panel_property *prop2);
struct panel_property *panel_property_find(struct list_head *prop_list, char *name);
struct panel_property *panel_property_find_by_prop(struct list_head *prop_list,
		struct panel_property *prop);

void panel_property_destroy(struct panel_property *property);
int panel_property_set_value(struct list_head *prop_list, char *name, unsigned int value);
int panel_property_get_value(struct list_head *prop_list, char *name);
int panel_property_add_value(struct list_head *prop_list,
		char *name, unsigned int init_value, unsigned int min, unsigned int max);

int panel_property_add_prop_array(struct list_head *prop_list,
		struct panel_property *prop_array, size_t num_prop_array);
int panel_property_delete(struct list_head *prop_list, char *name);
void panel_property_delete_all(struct list_head *prop_list);
#endif /* __PANEL_PROPERTY_H__ */
