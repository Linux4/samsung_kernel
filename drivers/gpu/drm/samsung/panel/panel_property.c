// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include "panel_drv.h"
#include "panel_debug.h"
#include "panel_obj.h"

const char *panel_prop_type_name[MAX_PANEL_PROP_TYPE] = {
	[PANEL_PROP_TYPE_RANGE] = "RANGE",
};

const char *prop_type_to_string(u32 type)
{
	if (type >= MAX_PANEL_PROP_TYPE)
		return NULL;

	return panel_prop_type_name[type];
}

int string_to_prop_type(const char *str)
{
	unsigned int type;

	if (!str)
		return -EINVAL;

	for (type = 0; type < MAX_PANEL_PROP_TYPE; type++) {
		if (!panel_prop_type_name[type])
			continue;

		if (!strcmp(panel_prop_type_name[type], str))
			return type;
	}

	return -EINVAL;
}

char *get_panel_property_name(struct panel_property *property)
{
	return get_pnobj_name(&property->base);
}

int panel_prop_compare(struct panel_property *prop1, struct panel_property *prop2)
{
	int ret;

	if (prop1 == prop2)
		return 0;

	ret = strncmp(get_panel_property_name(prop1),
			get_panel_property_name(prop2),
			PANEL_PROP_NAME_LEN);
	if (ret != 0)
		return ret;

	if (prop1->type != prop2->type)
		return prop1->type - prop2->type;

	if (prop1->min != prop2->min)
		return prop1->min - prop2->min;

	if (prop1->max != prop2->max)
		return prop1->max - prop2->max;

	return 0;
}

struct panel_property *panel_property_find(struct list_head *prop_list, char *name)
{
	struct pnobj *pos;
	int name_len;

	if (!prop_list) {
		panel_err("prop_list is null.\n");
		return NULL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return NULL;
	}

	name_len = strlen(name);

	if (name_len > PANEL_PROP_NAME_LEN) {
		panel_err("name is too long(len: %d max: %d)\n", name_len, PANEL_PROP_NAME_LEN);
		return NULL;
	}

	list_for_each_entry(pos, prop_list, list) {
		struct panel_property *property =
			pnobj_container_of(pos, struct panel_property);

		if (!strncmp(get_panel_property_name(property),
					name, PANEL_PROP_NAME_LEN))
			return property;
	}

	return NULL;
}

struct panel_property *panel_property_find_by_prop(struct list_head *prop_list,
		struct panel_property *prop)
{
	struct panel_property *prop1 =
		panel_property_find(prop_list, get_panel_property_name(prop));

	if (!prop1)
		return NULL;

	if (prop->type != prop1->type)
		return NULL;

	if (prop->min != prop1->min)
		return NULL;

	if (prop->max != prop1->max)
		return NULL;

	return prop1;
}

int panel_property_set_value(struct list_head *prop_list, char *name, unsigned int value)
{
	struct panel_property *property;

	if (!prop_list) {
		panel_err("prop_list is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	property = panel_property_find(prop_list, name);
	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	if (property->type != PANEL_PROP_TYPE_RANGE) {
		panel_err("property type is not match(%s).\n", name);
		return -EINVAL;
	}

	if ((value < property->min) || (value > property->max)) {
		panel_err("property(%s) value(%u) out of range(%u ~ %u)\n",
				name, value, property->min, property->max);
		return -EINVAL;
	}

	property->value = value;

	return 0;
}

int panel_property_get_value(struct list_head *prop_list, char *name)
{
	struct panel_property *property;

	if (!prop_list) {
		panel_err("prop_list is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	property = panel_property_find(prop_list, name);

	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	if (property->type != PANEL_PROP_TYPE_RANGE) {
		panel_err("property type is not match(%s).\n", name);
		return -EINVAL;
	}

	return property->value;
}

void panel_property_destroy(struct panel_property *property)
{
	if (!property)
		return;

	list_del(get_pnobj_list(&property->base));
	free_pnobj_name(&property->base);
	kfree(property);
}

int panel_property_delete(struct list_head *prop_list, char *name)
{
	struct panel_property *property;

	if (!prop_list) {
		panel_err("prop_list is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	property = panel_property_find(prop_list, name);
	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	panel_property_destroy(property);

	return 0;
}

int panel_property_add_value(struct list_head *prop_list,
		char *name, unsigned int init_value, unsigned int min, unsigned int max)
{
	struct panel_property *property;
	int name_len;

	if (!prop_list) {
		panel_err("prop_list is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	if (panel_property_find(prop_list, name)) {
		panel_err("same name property is exist.\n");
		return -EINVAL;
	}

	name_len = strlen(name);

	if (name_len > PANEL_PROP_NAME_LEN) {
		panel_err("name is too long(len: %d max: %d)\n", name_len, PANEL_PROP_NAME_LEN);
		return -EINVAL;
	}

	property = kzalloc(sizeof(struct panel_property), GFP_KERNEL);
	if (!property)
		return -ENOMEM;

	pnobj_init(&property->base, CMD_TYPE_PROP, name);
	property->value = init_value;
	property->type = PANEL_PROP_TYPE_RANGE;
	property->min = min;
	property->max = max;

	list_add_tail(get_pnobj_list(&property->base), prop_list);

	panel_info("added: %s init_value: %d.\n", get_panel_property_name(property), property->value);

	return 0;
}

int panel_property_add_prop_array(struct list_head *prop_list,
		struct panel_property *prop_array, size_t num_prop_array)
{
	int i, ret;

	for (i = 0; i < num_prop_array; i++) {
		ret = panel_property_add_value(prop_list,
				get_panel_property_name(&prop_array[i]),
				prop_array[i].value, prop_array[i].min, prop_array[i].max);
		if (ret < 0) {
			panel_err("failed to add property(%s)\n",
					get_panel_property_name(&prop_array[i]));
			return ret;
		}
	}

	return 0;
}

void panel_property_delete_all(struct list_head *prop_list)
{
	struct pnobj *pos, *next;

	if (!prop_list) {
		panel_err("prop_list is null.\n");
		return;
	}
	
	list_for_each_entry_safe(pos, next, prop_list, list)
		panel_property_destroy(pnobj_container_of(pos, struct panel_property));
}

MODULE_DESCRIPTION("property driver for panel");
MODULE_LICENSE("GPL v2");
