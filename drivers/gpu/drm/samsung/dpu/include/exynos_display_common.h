/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Google LLC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 */

#ifndef __EXYNOS_DISPLAY_COMMON_H__
#define __EXYNOS_DISPLAY_COMMON_H__

#include <video/mipi_display.h>
#include <linux/of.h>

/* Structures and definitions shared across exynos display pipeline. */
#ifndef KHZ
#define KHZ (1000)
#endif
#ifndef MHZ
#define MHZ (1000*1000)
#endif
#ifndef MSEC
#define MSEC (1000)
#endif

static inline bool exynos_mipi_dsi_packet_format_is_read(u8 type)
{
	switch (type) {
	case MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM:
	case MIPI_DSI_DCS_READ:
		return true;
	}

	return false;
}

static inline struct property *exynos_prop_dup(struct device *dev,
					   const struct property *prop)
{
	struct property *new;

	new = kzalloc(sizeof(*new), GFP_KERNEL);
	if (!new)
		goto err;

	/*
	 * NOTE: There is no check for zero length value.
	 * In case of a boolean property, this will allocate a value
	 * of zero bytes. We do this to work around the use
	 * of of_get_property() calls on boolean values.
	 */
	new->name = kstrdup(prop->name, GFP_KERNEL);
	new->value = kmemdup(prop->value, prop->length, GFP_KERNEL);
	new->length = prop->length;
	if (!new->name || !new->value)
		goto err;

	return new;
err:
	if (new) {
		if (new->name)
			kfree(new->name);
		if (new->value)
			kfree(new->value);
		kfree(new);
	}

	return NULL;
}

static inline int exynos_duplicate_display_prop(struct device *dev,
						struct device_node *dest,
						struct property *src_pp)
{
	struct property *pp;

	pp = of_find_property(dest, src_pp->name, NULL);
	if (!pp) {
		pp = exynos_prop_dup(dev, src_pp);
		if (!pp) {
			pr_err("[%s] failed to dup prop\n", __func__);

			return -EINVAL;
		}

		of_add_property(dest, pp);
	}

	return 0;
}

static inline bool has_prefix(const char *s, const char *prefix)
{
	size_t len = strlen(s);
	size_t prefix_len = strlen(prefix);

	if (prefix_len >= len)
		return false;

	return !strncmp(s, prefix, prefix_len);
}

static inline int exynos_duplicate_display_props(struct device *dev,
						 struct device_node *dest,
						 struct device_node *src)
{
	struct property *src_pp;

	if (!src || !dest)
		return 0;

	for_each_property_of_node(src, src_pp) {
		pr_debug("[%s] dest(%s) src(%s) src prop(%s)\n",
				__func__, dest->full_name, src->full_name,
				src_pp->name);
		if (has_prefix(src_pp->name, "exynos,"))
			exynos_duplicate_display_prop(dev, dest, src_pp);
	}

	return 0;
}
#endif
