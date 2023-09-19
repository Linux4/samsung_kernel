/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_CONFIG_H__
#define __PANEL_CONFIG_H__

#include "panel_property.h"

struct panel_device;

struct pnobj_config {
	struct pnobj base;
	char *prop_name;
	enum PANEL_PROP_TYPE prop_type;
	union {
		unsigned int value;
		char *str;
	};
};

#define __PNOBJ_CONFIG_INITIALIZER(_name_, _prop_name_, _value_) \
	{ .base = __PNOBJ_INITIALIZER(_name_, CMD_TYPE_CFG) \
	, .prop_name = (_prop_name_) \
	, .prop_type = (PANEL_PROP_TYPE_RANGE) \
	, .value = (_value_) }

#define DEFINE_PNOBJ_CONFIG(_name, _prop_name, _value) \
struct pnobj_config PN_CONCAT(pnobj_config_, _name) = \
	__PNOBJ_CONFIG_INITIALIZER(_name, _prop_name, _value)

#define PNOBJ_CONFIG(_name_) PN_CONCAT(pnobj_config_, _name_)

int panel_apply_pnobj_config(struct panel_device *panel,
		struct pnobj_config *pnobj_config);

static inline char *get_pnobj_config_name(struct pnobj_config *prop)
{
	return get_pnobj_name(&prop->base);
}

#endif /* __PANEL_CONFIG_H__ */
