/*/mediatek/lcm/mkt_gen_panel/mtk_gen_common.h
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
#include "gen_panel/gen-panel.h"
#include "gen_panel/gen-panel-bl.h"
#include "gen_panel/gen-panel-mdnie.h"

#define FB_LCM_EVENT_RESUME	0x20
#define FB_LCM_EVENT_SUSPEND	0x21

/**
*	mtk_register_client - register a client notifier
*	@nb: notifier block to callback on events
*/
extern int mtk_lcm_register_client(struct notifier_block *nb);
/**
*	mtk_unregister_client - unregister a client notifier
*	@nb: notifier block to callback on events
*/
extern int mtk_lcm_unregister_client(struct notifier_block *nb);
/**
* 	mtk_notifier_call_chain - notify clients of fb_events
*
*/
extern int mtk_lcm_notifier_call_chain(unsigned long val, void *v);
/**
*	related  panel power  function
*/
extern void mtk_panel_power_on(struct lcd *lcd);
extern void mtk_panel_power_off(struct lcd *lcd);

extern int primary_display_dsi_set_withrawcmdq(unsigned int *pdata,
			unsigned int queue_size, unsigned char force_update);
extern int primary_display_dsi_read_cmdq_cmd(unsigned data_id, unsigned offset,
		unsigned cmd, unsigned char *buffer, unsigned char size);
static DEFINE_MUTEX(mtk_panel_lock);
#endif
