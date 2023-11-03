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

#define PANEL_PROP_NAME_LEN (32)

enum PANEL_PROP_TYPE {
	PANEL_PROP_TYPE_VALUE,
	PANEL_PROP_TYPE_STR,
	MAX_PANEL_PROP_TYPE
};

#define __PANEL_PROPERTY_U32_INITIALIZER(_name, _value, _min, _max) { \
	.name = (_name), .type = (PANEL_PROP_TYPE_VALUE), \
	.value = (_value), .min = (_min), .max = (_max) }
#define __PANEL_PROPERTY_STR_INITIALIZER(_name, _str) { \
	.name = (_name), .type = (PANEL_PROP_TYPE_STR), .str = (_str) }

struct panel_property {
	char name[PANEL_PROP_NAME_LEN + 1];
	enum PANEL_PROP_TYPE type;
	union {
		unsigned int value;
		char *str;
	};
	unsigned int min;
	unsigned int max;
	struct list_head head;
};

struct panel_property_list {
	struct list_head list;
};

struct panel_property *panel_property_find(struct panel_property_list *properties, char *name);

int panel_property_set_value(struct panel_property_list *properties, char *name, unsigned int value);
int panel_property_get_value(struct panel_property_list *properties, char *name);
int panel_property_add_value(struct panel_property_list *properties,
		char *name, unsigned int init_value, unsigned int min, unsigned int max);

int panel_property_set_str(struct panel_property_list *properties, char *name, char *str);
char *panel_property_get_str(struct panel_property_list *properties, char *name);
int panel_property_add_str(struct panel_property_list *properties, char *name, char *str);
int panel_property_add_prop_array(struct panel_property_list *properties,
		struct panel_property *prop_array, size_t num_prop_array);
int panel_property_delete(struct panel_property_list *properties, char *name);
void panel_property_delete_all(struct panel_property_list *properties);
#endif /* __PANEL_PROPERTY_H__ */
