/*
 * Essential Camera Sensor Driver
 *
 * Copyright (c) 2012 Marvell Ltd.
 * Jiaquan Su <jqsu@marvell.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* basic sensor */
/* echo sensor */
/* plat sensor */
/* HW audit sensor */
/* SW audit sensor */

#include <linux/slab.h>
#include <linux/module.h>
#include "ecs-core.h"

/* trace_level: */
/* 0 - only actual errors */
/* 1 - ecs internal error included */
/* 2 - all ecs log */
/* 3 - all ecs log and sensor behavior, performance impactable */
int ecs_trace = 3;
module_param(ecs_trace, int, 0644);


#define ECS_DEFAULT_STN_NAME	"setting_name_unknown"
#define ECS_DEFAULT_PROP_NAME	"property name unknown"
#define ECS_DEFAULT_STATE_NAME	"state name unknown"
#define ECS_DEFAULT_SENSOR_NAME	"X-style sensor"

static __attribute__((unused)) void ecs_dump_property(struct ecs_property *prop)
{
	struct ecs_setting *stn;
	int i;
	char buffer[256], temp[32];
	sprintf(buffer, "%15s: ", prop->name);
	for (i = 0; i < prop->stn_num; i++) {
		stn = prop->stn_tab + i;
		sprintf(temp, "%5s ", stn->name);
		strcat(buffer, temp);
	}
	pr_info("%s\n", buffer);
};

static __attribute__((unused)) void ecs_sensor_dump(struct ecs_sensor *snsr)
{
	struct ecs_property *prop;
	int i;

	pr_info("--------------------------------\n"
		".name = %s\n.prop:\n", snsr->name);
	for (i = 0; i < snsr->prop_num; i++) {
		prop = snsr->prop_tab + i;
		ecs_dump_property(prop);
	}
	pr_info("--------------------------------\n");
}

static __attribute__((unused)) int ecs_reg_array_dump(void *useless,
							void *table, int len)
{
	int i;
	char *ch = table;

	if ((table == NULL) || (len == 0))
		return 0;

	for (i = 0; i < len; i += 4) {
		pr_info("%02X %02X %02X %02X\n",
				ch[i], ch[i+1], ch[i+2], ch[i+3]);
	}
	return i;
}
/* ecs core functions */
/* ecs_find_xxx */
static inline struct ecs_property *ecs_find_property(struct ecs_sensor *snsr,
								int prop_id)
{
	struct ecs_property *prop = NULL;

	if (unlikely((prop_id < 0) || (prop_id >= snsr->prop_num))) {
		x_log(2, "property %d not found", prop_id);
		return NULL;
	}
	prop = snsr->prop_tab + prop_id;
	if (unlikely(prop->id == prop_id))
		return prop;
	x_log(3, "property '%s' is the %dth property, but it's id is %d, "
		"suggest move it to the %dth in property list to avoid bug",
		prop->name, prop_id, prop->id, prop->id);
	return NULL;
}

static inline struct ecs_state *ecs_find_state(struct ecs_sensor *snsr,
								int state_id)
{
	struct ecs_state *state = NULL;

	if (unlikely((state_id < 0) || (state_id >= snsr->state_num))) {
		x_log(2, "state %d not found", state_id);
		return NULL;
	}
	state = snsr->state_tab + state_id;
	if (unlikely(state_id == state->id))
		return state;
	xlog(3, "setting '%s' is the %dth state, but it's id is %d, "
		"suggest move it to the %dth in state list to avoid bug",
		state->name, state_id, state->id, state->id);
	return NULL;
}

static inline struct ecs_setting *ecs_find_setting(struct ecs_property *prop,
								int value)
{
	struct ecs_setting *stn = NULL;

	if (unlikely(prop == NULL))
		return NULL;
	if (unlikely((value < 0) || (value >= prop->stn_num))) {
		x_log(2, "setting %d not found", value);
		return NULL;
	}
	if (prop->stn_tab == NULL) {
		x_log(2, "no setting table defined for %s", prop->name);
		return NULL;
	}

	stn = prop->stn_tab + value;
	if (value == stn->id)
		return stn;
	x_log(3, "setting '%s' is the %dth setting, but it's id is %d, "
		"suggest move it to the %dth in setting list to avoid bug",
		stn->name, value, stn->id, stn->id);
	return NULL;
}

/* get all */
inline int ecs_get_state(struct ecs_sensor *snsr)
{
	return snsr->state_now;
}

inline int ecs_get_value(struct ecs_property *prop)
{
	return prop->value_now;
}

/* set all */
inline int ecs_set_value(struct ecs_sensor *snsr,
			const struct ecs_state_cfg *cfg)
{
	struct ecs_property *prop = NULL;
	struct ecs_setting *stn = NULL;
	int ret;

	prop = ecs_find_property(snsr, cfg->prop_id);
	if (prop == NULL)
		return -EINVAL;
	/* interpret property value according to its type */
	if (prop->value_type == 0) {
		stn = ecs_find_setting(prop, cfg->prop_val);
		if (stn == NULL)
			return -EINVAL;
		if (prop->speculate && (prop->value_now == cfg->prop_val)) {
			x_inf(2, "%10s == %s", prop->name, stn->name);
			return 0;
		}

		x_inf(2, "%10s := %s", prop->name, stn->name);
		ret = (*prop->cfg_handler)(snsr->hw_ctx,
						stn->cfg_tab, stn->cfg_sz);
		if (ret != stn->cfg_sz) {
			x_inf(0, "%s: error applying setting %s = %s ",
				snsr->name, prop->name, stn->name);
			return -EIO;
		}
	} else {
		/* the value is not integral, sensor driver knows what to do*/
		if ((prop->speculate) && (prop->value_equal) &&
			(*prop->value_equal)((void *)(uintptr_t)prop->value_now,
						(void *)(uintptr_t)cfg->prop_val)) {
			x_inf(0, "%10s needs no change\n", prop->name);
			return 0;
		}
		/* Here ECS assumes as soon as sensor driver mark a property
		 * value as none-integral, there is no way to convert property
		 * value to integral, even if there is someway, ECS don't want
		 * to envolve into this complexity, but cfg_handler shall */
		x_inf(2, "%10s will be changed\n", prop->name);
		ret = (*prop->cfg_handler)(snsr->hw_ctx,
				prop->stn_tab[0].cfg_tab, cfg->prop_val);
		if (ret < 0) {
			x_inf(0, "error changing property '%s'", prop->name);
			return -EIO;
		}
	}

	prop->value_now = cfg->prop_val;
	return ret;
}

static inline int _ecs_set_state(struct ecs_sensor *snsr, int state_id)
{
	struct ecs_state_cfg *cfg;
	struct ecs_state *state;
	int i, ret = 0;

	state = snsr->state_tab + state_id;
	for (i = 0; i < state->cfg_num; i++) {
		int cnt;
		cfg = &(state->cfg_tab[i]);
		cnt = ecs_set_value(snsr, cfg);
		if (cnt < 0) {
			x_inf(0, "change to state '%s' failed, when changing "
				"property %d", state->name, cfg->prop_id);
			return -EIO;
		}
		ret += cnt;
	}

	snsr->state_now = state->id;
	return ret;
}

int ecs_get_info(struct ecs_sensor *snsr, int prop_id, void **feedback)
{
	struct ecs_property *prop = NULL;
	struct ecs_setting *stn = NULL;

	if (unlikely(feedback == NULL))
		return -EINVAL;
	prop = ecs_find_property(snsr, prop_id);
	if (unlikely(prop == NULL))
		return -EINVAL;
	stn = ecs_find_setting(prop, prop->value_now);
	if (unlikely(stn == NULL))
		return -EINVAL;

	*feedback = stn->info;
	return 0;
}
EXPORT_SYMBOL(ecs_get_info);

int ecs_setting_override(struct ecs_sensor *snsr, int prop_id, int stn_id,
				void *cfg_tab, int cfg_sz)
{
	struct ecs_property *_prop = ecs_find_property(snsr, prop_id);
	struct ecs_setting *_stn = ecs_find_setting(_prop, stn_id);

	if (unlikely(_prop == NULL || _stn == NULL))
		return -EINVAL;

	_stn->cfg_tab = cfg_tab;
	_stn->cfg_sz = cfg_sz;
	return 0;
}
EXPORT_SYMBOL(ecs_setting_override);

int ecs_property_override(struct ecs_sensor *snsr, int prop_id,
			struct ecs_setting *stn_tab,
			int (*cfg_handler)(void *, const void *, int))
{
	struct ecs_property *prop = ecs_find_property(snsr, prop_id);

	if (unlikely(prop == NULL))
		return -EINVAL;

	if (stn_tab != NULL)
		prop->stn_tab = stn_tab;
	if (cfg_handler != NULL)
		prop->cfg_handler = cfg_handler;
	return 0;
}
EXPORT_SYMBOL(ecs_property_override);

int ecs_sensor_reset(struct ecs_sensor *snsr)
{
	struct ecs_property *prop;
	int i;

	if (snsr == NULL)
		return -EINVAL;
	for (i = 0; i < snsr->prop_num; i++) {
		prop = ecs_find_property(snsr, i);
		if (prop)
			prop->value_now = UNSET;
	}
	snsr->state_now = UNSET;
	return 0;
}
EXPORT_SYMBOL(ecs_sensor_reset);

int ecs_sensor_merge(struct ecs_sensor *orig, const struct ecs_sensor *plat)
{
	char *name;
	int i, j;

	/* if no speciman defined */
	if (plat == NULL)
		return 0;

	x_log(0, "merge '%s' into '%s'", plat->name, orig->name);
	name = (plat->name == NULL) ? ECS_DEFAULT_SENSOR_NAME : plat->name;
	if (orig->name == NULL)
		orig->name = name;

	/* allow speculate only if both sensor physical nature and
	 * sensor implementation allows speculate */
	orig->speculate &= plat->speculate;

	if ((orig->hw_ctx == NULL) && (plat->hw_ctx != NULL))
		orig->hw_ctx = plat->hw_ctx;

	if ((orig->cfg_handler == NULL) && (plat->cfg_handler != NULL))
		orig->cfg_handler = plat->cfg_handler;

	/* deal with the platform specific setting and property */
	for (i = 0; i < plat->prop_num; i++) {
		struct ecs_property *prop = ecs_find_property(orig,
			plat->prop_tab[i].id);

		if (prop == NULL) {
			x_inf(0, "can't find property id(%d) in %s",
				plat->prop_tab[i].id, orig->name);
			return -EINVAL;
		}

		x_log(0, "set %s as %s", prop->name, plat->prop_tab[i].name);
		if (plat->prop_tab[i].name != NULL)
			prop->name = plat->prop_tab[i].name;

		if (prop->stn_tab == NULL) {
			/* If original template has no setting table at all,
			 * just simply use the platform specific table */
			prop->stn_tab = plat->prop_tab[i].stn_tab;
			prop->stn_num = plat->prop_tab[i].stn_num;
			goto next;
		}
		/* deal with settings */
		for (j = 0; j < plat->prop_tab[i].stn_num; j++) {
			struct ecs_setting *stn = ecs_find_setting(prop,
				plat->prop_tab[i].stn_tab[j].id);
			if (stn == NULL) {
				x_inf(0, "can't find setting id(%d) in %s",
					plat->prop_tab[i].stn_tab[j].id,
					prop->name);
				return -EINVAL;
			}
			stn->cfg_tab = plat->prop_tab[i].stn_tab[j].cfg_tab;
			stn->cfg_sz = plat->prop_tab[i].stn_tab[j].cfg_sz;
		}
next:
		if (plat->prop_tab[i].cfg_handler != NULL)
			prop->cfg_handler = plat->prop_tab[i].cfg_handler;
	}

	/* finally...states */
	/* deal with the platform specific setting and property */
	for (i = 0; i < plat->state_num; i++) {
		struct ecs_state *state = ecs_find_state(orig,
			plat->state_tab[i].id);
		if (state == NULL) {
			x_inf(0, "can't find state id(%d) in template",
				plat->state_tab[i].id);
			return -EINVAL;
		}

		state->cfg_tab = plat->state_tab[i].cfg_tab;
		state->cfg_num = plat->state_tab[i].cfg_num;
	}

	return 0;
}
EXPORT_SYMBOL(ecs_sensor_merge);

int ecs_sensor_init(struct ecs_sensor *snsr)
{
	int i, j, ret = 0;

	if (unlikely(snsr == NULL))
		return -EINVAL;

	if (snsr->hw_ctx == NULL) {
		x_log(0, "hardware context not defined");
		return -EINVAL;
	}
	if (snsr->cfg_handler == NULL) {
		x_inf(0, "config handler not defined");
		return -EINVAL;
	}

	/* by this time, sensor driver should have line up all the settings and
	 * propertys. ECS driver will check if all of them are well organized */
	for (i = 0; i < snsr->prop_num; i++) {
		struct ecs_property *prop = ecs_find_property(snsr, i);
		if (unlikely(prop == NULL)) {
			x_inf(0, "error checking property %d", i);
			return -EINVAL;
		}

		/* copy handlers by the way. If no specific handler defined for
		 * this property, use global default handler */
		if (prop->cfg_handler == NULL)
			prop->cfg_handler = snsr->cfg_handler;

		for (j = 0; j < prop->stn_num; j++) {
			struct ecs_setting *stn = ecs_find_setting(prop, j);
			if (unlikely(stn == NULL)) {
				x_inf(0, "error checking '%s' setting %d",
								prop->name, j);
				return -EINVAL;
			}
		}
		prop->speculate = prop->speculate && snsr->speculate;
	}

	for (i = 1; i < snsr->state_num; i++) {
		struct ecs_state *state = ecs_find_state(snsr, i);
		if (unlikely(state == NULL)) {
			x_inf(0, "error checking state %d", i);
			return -EINVAL;
		}
	}
ecs_sensor_dump(snsr);
	/* from this point on, ecs can directly access property/setting/state
	 * rather than calling ecs_find_property/setting/state */
	return ret;
}
EXPORT_SYMBOL(ecs_sensor_init);

/* Interfacing funtions */

int ecs_set_state(struct ecs_sensor *snsr, int state)
{
	if (unlikely((snsr == NULL) || (ecs_find_state(snsr, state) == NULL)))
		return -EINVAL;

	return _ecs_set_state(snsr, state);
}
EXPORT_SYMBOL(ecs_set_state);

int ecs_set_list(struct ecs_sensor *snsr,
					struct ecs_state_cfg *list, int len)
{
	int i, ret = 0;

	if ((snsr == NULL) || (list == NULL) || (len == 0))
		return -EINVAL;

	for (i = 0; i < len; i++) {
		int cnt;
		cnt = ecs_set_value(snsr, list);
		if (cnt < 0) {
			x_log(2, "set state failed");
			return -EIO;
		}
		ret += cnt;
		list++;
	}

	return ret;
}
EXPORT_SYMBOL(ecs_set_list);
