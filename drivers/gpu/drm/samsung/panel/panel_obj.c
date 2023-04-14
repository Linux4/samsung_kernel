// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kallsyms.h>

#include "panel_drv.h"
#include "panel_debug.h"
#include "panel_obj.h"

const char *pnobj_type_to_string(u32 pnobj_type)
{
	static const char *pnobj_type_name[MAX_PNOBJ_TYPE] = {
		[PNOBJ_TYPE_NONE] = "NONE",
		[PNOBJ_TYPE_FUNC] = "FUNC",
		[PNOBJ_TYPE_MAP] = "MAP",
		[PNOBJ_TYPE_DELAY] = "DELAY",
		[PNOBJ_TYPE_CONDITION] = "CONDITION",
		[PNOBJ_TYPE_PWRCTRL] = "PWRCTRL",
		[PNOBJ_TYPE_PROPERTY] = "PROPERTY",
		[PNOBJ_TYPE_RX_PACKET] = "RX_PACKET",
		[PNOBJ_TYPE_PACKET] = "PACKET",
		[PNOBJ_TYPE_KEY] = "KEY",
		[PNOBJ_TYPE_RESOURCE] = "RESOURCE",
		[PNOBJ_TYPE_DUMP] = "DUMP",
		[PNOBJ_TYPE_SEQUENCE] = "SEQUENCE",
	};

	return (pnobj_type < MAX_PNOBJ_TYPE) ?
		pnobj_type_name[pnobj_type] :
		pnobj_type_name[PNOBJ_TYPE_NONE];
}

unsigned int cmd_type_to_pnobj_type(unsigned int cmd_type)
{
	static unsigned int cmd_type_pnboj_type[MAX_CMD_TYPE] = {
		[CMD_TYPE_NONE] = PNOBJ_TYPE_NONE,
		[CMD_TYPE_FUNC] = PNOBJ_TYPE_FUNC,
		[CMD_TYPE_MAP] = PNOBJ_TYPE_MAP,
		[CMD_TYPE_DELAY ... CMD_TYPE_TIMER_DELAY_BEGIN] = PNOBJ_TYPE_DELAY,
		[CMD_TYPE_COND_IF ... CMD_TYPE_COND_FI] = PNOBJ_TYPE_CONDITION,
		[CMD_TYPE_PCTRL] = PNOBJ_TYPE_PWRCTRL,
		[CMD_TYPE_PROP] = PNOBJ_TYPE_PROPERTY,
		[CMD_TYPE_RX_PKT_START ... CMD_TYPE_RX_PKT_END] = PNOBJ_TYPE_RX_PACKET,
		[CMD_TYPE_TX_PKT_START ... CMD_TYPE_TX_PKT_END] = PNOBJ_TYPE_PACKET,
		[CMD_TYPE_KEY] = PNOBJ_TYPE_KEY,
		[CMD_TYPE_RES] = PNOBJ_TYPE_RESOURCE,
		[CMD_TYPE_DMP] = PNOBJ_TYPE_DUMP,
		[CMD_TYPE_SEQ] = PNOBJ_TYPE_SEQUENCE,
	};

	return (cmd_type < MAX_CMD_TYPE) ?
		cmd_type_pnboj_type[cmd_type] :
		cmd_type_pnboj_type[PNOBJ_TYPE_NONE];
}

bool is_valid_panel_obj(struct pnobj *pnobj)
{
	unsigned int type;

	if (!pnobj)
		return false;

	type = get_pnobj_cmd_type(pnobj);
	if (type == CMD_TYPE_NONE || type >= MAX_CMD_TYPE)
		return false;

	if (!get_pnobj_name(pnobj))
		return false;

	return true;
}

struct panel_obj_property *panel_obj_find_property(struct panel_obj_properties *properties, char *name)
{
	struct panel_obj_property *property;
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

	if (name_len > MAX_NAME_LEN) {
		panel_err("name is too long(len: %d max: %d)\n", name_len, MAX_NAME_LEN);
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

int panel_obj_set_property_value(struct panel_obj_properties *properties, char *name, unsigned int value)
{
	struct panel_obj_property *property;

	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	property = panel_obj_find_property(properties, name);

	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	if (property->type != PANEL_OBJ_PROP_TYPE_VALUE) {
		panel_err("property type is not match(%s).\n", name);
		return -EINVAL;
	}

	property->value = value;

	return 0;
}

int panel_obj_get_property_value(struct panel_obj_properties *properties, char *name)
{
	struct panel_obj_property *property;

	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	property = panel_obj_find_property(properties, name);

	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	if (property->type != PANEL_OBJ_PROP_TYPE_VALUE) {
		panel_err("property type is not match(%s).\n", name);
		return -EINVAL;
	}

	return property->value;
}

int panel_obj_set_property_str(struct panel_obj_properties *properties, char *name, char *str)
{
	struct panel_obj_property *property;

	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	property = panel_obj_find_property(properties, name);

	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	if (property->type != PANEL_OBJ_PROP_TYPE_STR) {
		panel_err("property type is not match(%s).\n", name);
		return -EINVAL;
	}

	property->str = str;

	return 0;
}

char *panel_obj_get_property_str(struct panel_obj_properties *properties, char *name)
{
	struct panel_obj_property *property;

	if (!properties) {
		panel_err("properties is null.\n");
		return NULL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return NULL;
	}

	property = panel_obj_find_property(properties, name);

	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return NULL;
	}

	if (property->type != PANEL_OBJ_PROP_TYPE_STR) {
		panel_err("property type is not match(%s).\n", name);
		return NULL;
	}

	return property->str;
}

int panel_obj_delete_property(struct panel_obj_properties *properties, char *name)
{
	struct panel_obj_property *property;

	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	property = panel_obj_find_property(properties, name);

	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	list_del(&property->head);
	kfree(property);

	return 0;
}

int panel_obj_add_property_value(struct panel_obj_properties *properties, char *name, unsigned int init_value)
{
	struct panel_obj_property *property;
	int name_len;

	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	if (panel_obj_find_property(properties, name)) {
		panel_err("same name property is exist.\n");
		return -EINVAL;
	}

	name_len = strlen(name);

	if (name_len > MAX_NAME_LEN) {
		panel_err("name is too long(len: %d max: %d)\n", name_len, MAX_NAME_LEN);
		return -EINVAL;
	}

	property = kzalloc(sizeof(struct panel_obj_property), GFP_KERNEL);
	if (!property)
		return -ENOMEM;

	memcpy(property->name, name, name_len);
	property->value = init_value;
	property->type = PANEL_OBJ_PROP_TYPE_VALUE;

	list_add(&property->head, &properties->list);

	panel_info("added: %s init_value: %d.\n", property->name, property->value);

	return 0;
}

int panel_obj_add_property_str(struct panel_obj_properties *properties, char *name, char *str)
{
	struct panel_obj_property *property;
	int name_len;

	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	if (panel_obj_find_property(properties, name)) {
		panel_err("same name property is exist.\n");
		return -EINVAL;
	}

	name_len = strlen(name);

	if (name_len > MAX_NAME_LEN) {
		panel_err("name is too long(len: %d max: %d)\n", name_len, MAX_NAME_LEN);
		return -EINVAL;
	}

	property = kzalloc(sizeof(struct panel_obj_property), GFP_KERNEL);
	if (!property)
		return -ENOMEM;

	memcpy(property->name, name, name_len);
	property->str = str;
	property->type = PANEL_OBJ_PROP_TYPE_STR;

	list_add(&property->head, &properties->list);

	panel_info("added: %s init_str: %s.\n", property->name, property->str);

	return 0;
}

int panel_obj_init(struct panel_obj_properties *properties)
{
	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	INIT_LIST_HEAD(&properties->list);

	/* add properties here */

	if (panel_obj_add_property_value(properties, PANEL_OBJ_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_AUTO) < 0) {
		panel_err("err. (%s)\n", PANEL_OBJ_PROPERTY_WAIT_TX_DONE);
		return -EINVAL;
	}

	return 0;
}

struct pnobj *pnobj_find_by_name(struct list_head *head, char *name)
{
	struct pnobj *pnobj;

	if (!name)
		return NULL;

	list_for_each_entry(pnobj, head, list) {
		if (!get_pnobj_name(pnobj))
			continue;
		if (!strncmp(get_pnobj_name(pnobj), name,
					PNOBJ_NAME_LEN))
			return pnobj;
	}

	return NULL;
}

struct pnobj *pnobj_find_by_substr(struct list_head *head, char *substr)
{
	struct pnobj *pnobj;

	if (!substr)
		return NULL;

	list_for_each_entry(pnobj, head, list) {
		if (!get_pnobj_name(pnobj))
			continue;
		if (strstr(get_pnobj_name(pnobj), substr))
			return pnobj;
	}

	return NULL;
}

struct pnobj *pnobj_find_by_id(struct list_head *head, unsigned int id)
{
	struct pnobj *pnobj;

	list_for_each_entry(pnobj, head, list) {
		if (get_pnobj_id(pnobj) == id)
			return pnobj;
	}

	return NULL;
}

struct pnobj *pnobj_find_by_pnobj(struct list_head *head, struct pnobj *_pnobj)
{
	struct pnobj *pnobj;

	if (!is_valid_panel_obj(_pnobj)) {
		panel_warn("invalid pnobj(%s)\n",
				get_pnobj_name(_pnobj));
		return NULL;
	}

	list_for_each_entry(pnobj, head, list) {
		if (!is_valid_panel_obj(pnobj)) {
			panel_warn("invalid pnobj(%s)\n",
					get_pnobj_name(pnobj));
			continue;
		}

		if (get_pnobj_cmd_type(pnobj) != get_pnobj_cmd_type(_pnobj))
			continue;

		if (strncmp(get_pnobj_name(pnobj),
					get_pnobj_name(_pnobj), PNOBJ_NAME_LEN))
			continue;

		return pnobj;
	}

	return NULL;
}

/* Compare two pnobj items. */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
int pnobj_type_compare(void *priv,
		struct list_head *a, struct list_head *b)
#else
int pnobj_type_compare(void *priv,
		const struct list_head *a, const struct list_head *b)
#endif
{
	struct pnobj *pnobj_a;
	struct pnobj *pnobj_b;
	unsigned int type_a, type_b;

	pnobj_a = container_of(a, struct pnobj, list);
	pnobj_b = container_of(b, struct pnobj, list);

	type_a = get_pnobj_cmd_type(pnobj_a);
	type_b = get_pnobj_cmd_type(pnobj_b);

	if (cmd_type_to_pnobj_type(type_a) !=
			cmd_type_to_pnobj_type(type_b))
		return cmd_type_to_pnobj_type(type_a) -
			cmd_type_to_pnobj_type(type_b);

	return type_a - type_b;
}

#if defined(CONFIG_MCD_PANEL_JSON)
struct pnobj_func *create_pnobj_function(void *f)
{
	char symname[KSYM_NAME_LEN];
	struct pnobj_func *pnobj_func;

	if (!f)
		return NULL;

	pnobj_func = kzalloc(sizeof(struct pnobj_func), GFP_KERNEL);
	if (!pnobj_func)
		return NULL;

	sprint_symbol_no_offset(symname, (unsigned long)f);
	if (!strncmp(symname, "0x", 2)) {
		panel_err("failed to lookup symbol name\n");
		kfree(pnobj_func);
		return NULL;
	}

	pnobj_init(&pnobj_func->base, CMD_TYPE_FUNC, symname);
	pnobj_func->symaddr = (unsigned long)f;

	return pnobj_func;
}

void destroy_pnobj_function(struct pnobj_func *pnobj_func)
{
	if (!pnobj_func)
		return;

	free_pnobj_name(&pnobj_func->base);
	kfree(pnobj_func);
}

int pnobj_function_list_add(void *f, struct list_head *list)
{
	struct pnobj_func *pnobj_func;
	char symname[KSYM_NAME_LEN];

	if (f == NULL)
		return -EINVAL;

	if (!list)
		return -EINVAL;

	sprint_symbol_no_offset(symname, (unsigned long)f);
	if (!strncmp(symname, "0x", 2))
		return -EINVAL;

	if (pnobj_find_by_name(list, symname))
		return 0;

	pnobj_func = create_pnobj_function(f);
	if (!pnobj_func) {
		panel_err("failed to create pnobj function(%s)\n", symname);
		return -EINVAL;
	}

	list_add_tail(get_pnobj_list(&pnobj_func->base), list);

	return 0;
}

unsigned long get_pnobj_function(struct list_head *list, char *name)
{
	struct pnobj *pnobj, t;

	pnobj_init(&t, CMD_TYPE_FUNC, name);

	if (!list || list_empty(list))
		return 0;

	pnobj = pnobj_find_by_pnobj(list, &t);
	if (!pnobj)
		return 0;

	return pnobj_container_of(pnobj, struct pnobj_func)->symaddr;
}
#endif

struct maptbl *get_pnobj_maptbl(struct list_head *list, char *name)
{
	struct pnobj *pnobj, t;

	pnobj_init(&t, CMD_TYPE_MAP, name);

	if (!list || list_empty(list))
		return 0;

	pnobj = pnobj_find_by_pnobj(list, &t);
	if (!pnobj)
		return 0;

	return pnobj_container_of(pnobj, struct maptbl);
}

MODULE_DESCRIPTION("obj driver for panel");
MODULE_LICENSE("GPL");
