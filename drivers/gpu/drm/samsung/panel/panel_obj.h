/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_OBJ_H__
#define __PANEL_OBJ_H__

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/version.h>

#define MAX_NAME_LEN (32)
#define PNOBJ_NAME_LEN (128)

#define pnobj_container_of(pnobj, type) \
	container_of(pnobj, type, base)

/* pnobj type */
enum {
	PNOBJ_TYPE_NONE,
	/* independant objects */
	PNOBJ_TYPE_FUNC,
	PNOBJ_TYPE_MAP,
	PNOBJ_TYPE_DELAY,
	PNOBJ_TYPE_CONDITION,
	PNOBJ_TYPE_PWRCTRL,
	PNOBJ_TYPE_PROPERTY,
	PNOBJ_TYPE_RX_PACKET,
	/* dependant objects */
	PNOBJ_TYPE_PACKET,	/* packet -> maptbl */
	PNOBJ_TYPE_KEY,	/* key -> packet */
	PNOBJ_TYPE_RESOURCE, /* resource -> readinfo(packet) */
	PNOBJ_TYPE_DUMP, /* dump -> resource */
	PNOBJ_TYPE_SEQUENCE, /* sequence -> all pnobj */
	MAX_PNOBJ_TYPE,
};

/* panel object */
struct pnobj {
	unsigned int cmd_type;
	char *name;
	unsigned int id;
	struct list_head list;
};

#if defined(CONFIG_MCD_PANEL_JSON)
struct pnobj_func {
	struct pnobj base;
	unsigned long symaddr;
};
#endif

static inline void set_pnobj_cmd_type(struct pnobj *base, unsigned int type)
{
	base->cmd_type = type;
}

static inline unsigned int get_pnobj_cmd_type(struct pnobj *base)
{
	return base->cmd_type;
}

static inline void set_pnobj_name(struct pnobj *base, char *name)
{
	base->name = kstrndup(name, PNOBJ_NAME_LEN, GFP_KERNEL);
}

static inline char *get_pnobj_name(struct pnobj *base)
{
	return base->name;
}

static inline void free_pnobj_name(struct pnobj *base)
{
	kfree(base->name);
}

static inline void set_pnobj_id(struct pnobj *base, unsigned int id)
{
	base->id = id;
}

static inline unsigned int get_pnobj_id(struct pnobj *base)
{
	return base->id;
}

static inline struct list_head *get_pnobj_list(struct pnobj *base)
{
	return &base->list;
}

static inline void pnobj_init(struct pnobj *base, u32 type, char *name)
{
	set_pnobj_cmd_type(base, type);
	set_pnobj_name(base, name);
	INIT_LIST_HEAD(&base->list);
}

#define __PNOBJ_INITIALIZER(_pnobjname, _cmdtype) \
{ .name = (#_pnobjname), .cmd_type = (_cmdtype) }

/* wait_tx_done */
#define PANEL_OBJ_PROPERTY_WAIT_TX_DONE ("wait_tx_done")
enum {
	WAIT_TX_DONE_AUTO,  /* Wait TX DONE every end of cmdq set */
	WAIT_TX_DONE_MANUAL_OFF, /* DO NOT wait for TX DONE */
	WAIT_TX_DONE_MANUAL_ON, /* Wait TX DONE every each cmd */
};

enum PANEL_OBJ_PROP_TYPE {
	PANEL_OBJ_PROP_TYPE_VALUE,
	PANEL_OBJ_PROP_TYPE_STR,
	MAX_PANEL_OBJ_PROP_TYPE
};

struct panel_obj_property {
	char name[MAX_NAME_LEN + 1];
	enum PANEL_OBJ_PROP_TYPE type;
	union {
		unsigned int value;
		char *str;
	};
	struct list_head head;
};

struct panel_obj_properties {
	struct list_head list;
};

bool is_valid_panel_obj(struct pnobj *pnobj);
struct panel_obj_property *panel_obj_find_property(struct panel_obj_properties *properties, char *name);

int panel_obj_set_property_value(struct panel_obj_properties *properties, char *name, unsigned int value);
int panel_obj_get_property_value(struct panel_obj_properties *properties, char *name);
int panel_obj_add_property_value(struct panel_obj_properties *properties, char *name, unsigned int init_value);

int panel_obj_set_property_str(struct panel_obj_properties *properties, char *name, char *str);
char *panel_obj_get_property_str(struct panel_obj_properties *properties, char *name);
int panel_obj_add_property_str(struct panel_obj_properties *properties, char *name, char *str);

int panel_obj_delete_property(struct panel_obj_properties *properties, char *name);
int panel_obj_init(struct panel_obj_properties *properties);

const char *pnobj_type_to_string(u32 type);
unsigned int cmd_type_to_pnobj_type(unsigned int cmd_type);
struct pnobj *pnobj_find_by_name(struct list_head *head, char *name);
struct pnobj *pnobj_find_by_substr(struct list_head *head, char *substr);
struct pnobj *pnobj_find_by_id(struct list_head *head, unsigned int id);
struct pnobj *pnobj_find_by_pnobj(struct list_head *head, struct pnobj *pnobj);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
int pnobj_type_compare(void *priv,
		struct list_head *a, struct list_head *b);
#else
int pnobj_type_compare(void *priv,
		const struct list_head *a, const struct list_head *b);
#endif
#if defined(CONFIG_MCD_PANEL_JSON)
struct pnobj_func *create_pnobj_function(void *f);
void destroy_pnobj_function(struct pnobj_func *pnobj_func);
int pnobj_function_list_add(void *f, struct list_head *list);
unsigned long get_pnobj_function(struct list_head *list, char *name);
struct maptbl *get_pnobj_maptbl(struct list_head *list, char *name);
#endif
#endif /* __PANEL_OBJ_H__ */
