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

struct panel_property *panel_property_find(struct panel_property_list *properties, char *name)
{
	struct panel_property *property;
	int name_len;

	if (!properties) {
		panel_err("properties is null.\n");
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

	list_for_each_entry(property, &properties->list, head) {
		if (strlen(property->name) != name_len)
			continue;

		if (!strncmp(property->name, name, name_len))
			return property;
	}

	return NULL;
}

int panel_property_set_value(struct panel_property_list *properties, char *name, unsigned int value)
{
	struct panel_property *property;

	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	property = panel_property_find(properties, name);
	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	if (property->type != PANEL_PROP_TYPE_VALUE) {
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

int panel_property_get_value(struct panel_property_list *properties, char *name)
{
	struct panel_property *property;

	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	property = panel_property_find(properties, name);

	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	if (property->type != PANEL_PROP_TYPE_VALUE) {
		panel_err("property type is not match(%s).\n", name);
		return -EINVAL;
	}

	return property->value;
}

int panel_property_set_str(struct panel_property_list *properties, char *name, char *str)
{
	struct panel_property *property;

	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	property = panel_property_find(properties, name);

	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	if (property->type != PANEL_PROP_TYPE_STR) {
		panel_err("property type is not match(%s).\n", name);
		return -EINVAL;
	}

	property->str = str;

	return 0;
}

char *panel_property_get_str(struct panel_property_list *properties, char *name)
{
	struct panel_property *property;

	if (!properties) {
		panel_err("properties is null.\n");
		return NULL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return NULL;
	}

	property = panel_property_find(properties, name);

	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return NULL;
	}

	if (property->type != PANEL_PROP_TYPE_STR) {
		panel_err("property type is not match(%s).\n", name);
		return NULL;
	}

	return property->str;
}

int panel_property_delete(struct panel_property_list *properties, char *name)
{
	struct panel_property *property;

	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	property = panel_property_find(properties, name);

	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	list_del(&property->head);
	kfree(property);

	return 0;
}

int panel_property_add_value(struct panel_property_list *properties,
		char *name, unsigned int init_value, unsigned int min, unsigned int max)
{
	struct panel_property *property;
	int name_len;

	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	if (panel_property_find(properties, name)) {
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

	memcpy(property->name, name, name_len);
	property->value = init_value;
	property->type = PANEL_PROP_TYPE_VALUE;
	property->min = min;
	property->max = max;

	list_add(&property->head, &properties->list);

	panel_info("added: %s init_value: %d.\n", property->name, property->value);

	return 0;
}

int panel_property_add_str(struct panel_property_list *properties, char *name, char *str)
{
	struct panel_property *property;
	int name_len;

	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	if (panel_property_find(properties, name)) {
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

	memcpy(property->name, name, name_len);
	property->str = str;
	property->type = PANEL_PROP_TYPE_STR;

	list_add(&property->head, &properties->list);

	panel_info("added: %s init_str: %s.\n", property->name, property->str);

	return 0;
}

int panel_property_add_prop_array(struct panel_property_list *properties,
		struct panel_property *prop_array, size_t num_prop_array)
{
	int i, ret;

	for (i = 0; i < num_prop_array; i++) {
		ret = panel_property_add_value(properties,
				prop_array[i].name, prop_array[i].value,
				prop_array[i].min, prop_array[i].max);
		if (ret < 0) {
			panel_err("failed to add property(%s)\n",
				prop_array[i].name);
			return ret;
		}
	}

	return 0;
}

void panel_property_delete_all(struct panel_property_list *properties)
{
	struct panel_property *pos, *next;

	if (!properties) {
		panel_err("properties is null.\n");
		return;
	}
	
	list_for_each_entry_safe(pos, next, &properties->list, head) {
		list_del(&pos->head);
		kfree(pos);
	}
}

MODULE_DESCRIPTION("property driver for panel");
MODULE_LICENSE("GPL v2");
