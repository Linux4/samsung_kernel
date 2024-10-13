/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __EXYNOS_BTS_H_
#define __EXYNOS_BTS_H_

#if defined(CONFIG_EXYNOS5430_BTS) || defined(CONFIG_EXYNOS5422_BTS)	\
	|| defined(CONFIG_EXYNOS5433_BTS) || defined(CONFIG_EXYNOS3475_BTS)
void bts_initialize(const char *pd_name, bool on);
#else
#define bts_initialize(a, b) do {} while (0)
#endif

#if defined(CONFIG_EXYNOS5430_BTS) || defined(CONFIG_EXYNOS5433_BTS)
void exynos5_bts_show_mo_status(void);
#else
#define exynos5_bts_show_mo_status() do { } while (0)
#endif

#if defined (CONFIG_EXYNOS3475_BTS)
enum bts_media_type {
	TYPE_DECON_INT,
	TYPE_DECON_EXT,
	TYPE_VPP0,
	TYPE_VPP1,
	TYPE_VPP2,
	TYPE_VPP3,
	TYPE_CAM,
	TYPE_YUV,
	TYPE_UD_ENC,
	TYPE_UD_DEC,
	TYPE_SPDMA,
};

void exynos_update_media_scenario(enum bts_media_type media_type,
		unsigned int bw, int bw_type);
int exynos_update_bts_param(int target_idx, int work);
int exynos_bts_register_notifier(struct notifier_block *nb);
int exynos_bts_unregister_notifier(struct notifier_block *nb);
#else
#define exynos_update_media_scenario(a, b, c) do {} while (0)
#define exynos_update_bts_param(a, b) do {} while (0)
#define exynos_bts_register_notifier(a) do {} while (0)
#define exynos_bts_unregister_notifier(a) do {} while (0)
#endif

#if defined(CONFIG_EXYNOS5430_BTS) || defined(CONFIG_EXYNOS5433_BTS)
void bts_otf_initialize(unsigned int id, bool on);
#else
#define bts_otf_initialize(a, b) do {} while (0)
#endif

#if defined(CONFIG_EXYNOS5422_BTS) || defined(CONFIG_EXYNOS5433_BTS)
enum bts_scen_type {
	TYPE_MFC_UD_DECODING = 0,
	TYPE_MFC_UD_ENCODING,
	TYPE_LAYERS,
	TYPE_G3D_FREQ,
	TYPE_G3D_SCENARIO,
};

void bts_scen_update(enum bts_scen_type type, unsigned int val);
#else
#define bts_scen_update(a, b) do {} while(0)
#endif

#endif
