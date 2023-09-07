/* mediatek/lcm/mtk_gen_panel/mtk_gen_common.c
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 * Header file for Samsung Display Panel(LCD) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _MTK_GEN_PANEL_GENERIC_H
#define _MTK_GEN_PANEL_GENERIC_H

#include <linux/notifier.h>
#include <linux/export.h>
#include "gen_panel/gen-panel.h"
#include "gen_panel/gen-panel-bl.h"
#include "gen_panel/gen-panel-mdnie.h"
#include "mtk_gen_common.h"

#define FB_LCM_EVENT_RESUME	0x20
#define FB_LCM_EVENT_SUSPEND	0x21

static BLOCKING_NOTIFIER_HEAD(mtk_lcm_notifier_list);
/**
*	mtk_register_client - register a client notifier
*	@nb: notifier block to callback on events
*/
int mtk_lcm_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&mtk_lcm_notifier_list, nb);
}
EXPORT_SYMBOL(mtk_lcm_register_client);
/**
*	mtk_unregister_client - unregister a client notifier
*	@nb: notifier block to callback on events
*/
int mtk_lcm_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&mtk_lcm_notifier_list, nb);
}
EXPORT_SYMBOL(mtk_lcm_unregister_client);
/**
* 	mtk_notifier_call_chain - notify clients of fb_events
*
*/
int mtk_lcm_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&mtk_lcm_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(mtk_lcm_notifier_call_chain);

void mtk_preinit_noti(int state)
{
	if (state)
		mtk_lcm_notifier_call_chain(FB_LCM_EVENT_RESUME, NULL);
	else
		mtk_lcm_notifier_call_chain(FB_LCM_EVENT_SUSPEND, NULL);
}

void mtk_panel_power_on(struct lcd *lcd)
{
	gen_panel_set_external_main_pwr(lcd, GEN_PANEL_PWR_ON_FIR);
	mtk_preinit_noti(true);
	gen_panel_set_external_subsec_pwr(lcd, GEN_PANEL_PWR_ON_SEC);
	gen_panel_set_external_subthi_pwr(lcd, GEN_PANEL_PWR_ON_THI);
	gen_panel_set_external_subfou_pwr(lcd, GEN_PANEL_PWR_ON_FOU);
}

void mtk_panel_power_off(struct lcd *lcd)
{
	mtk_preinit_noti(false);
	gen_panel_set_external_main_pwr(lcd, GEN_PANEL_PWR_OFF);
}
#endif
