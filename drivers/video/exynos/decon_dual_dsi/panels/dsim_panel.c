
/* linux/drivers/video/exynos_decon/panel/dsim_panel.c
 *
 * Header file for Samsung MIPI-DSI LCD Panel driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 * Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/lcd.h>
#include "../dsim.h"

#include "dsim_panel.h"
#include "oled_backlight.h"
#include "panel_info.h"

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
#include "mdnie.h"
#if defined(CONFIG_PANEL_S6E3FA3)
#include "mdnie_lite_table_v.h"
#endif
#endif

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
static int mdnie_lite_write_set(struct dsim_device *dsim, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0, i;

	for (i = 0; i < num; i++) {
		if (seq[i].cmd) {
			ret = dsim_write_hl_data(dsim, seq[i].cmd, seq[i].len);
			if (ret != 0) {
				dsim_err("%s failed.\n", __func__);
				return ret;
			}
		}
		if (seq[i].sleep)
			usleep_range(seq[i].sleep * 1000, seq[i].sleep * 1000);
	}
	return ret;
}

int mdnie_lite_send_seq(struct dsim_device *dsim, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	if (panel->op_state == PANEL_STAT_OFF) {
		dsim_info("%s : panel is not active\n", __func__);
		return -EIO;
	}

	mutex_lock(&panel->lock);
	ret = mdnie_lite_write_set(dsim, seq, num);
	mutex_unlock(&panel->lock);

	return ret;
}

int mdnie_lite_read(struct dsim_device *dsim, u8 addr, u8 *buf, u32 size)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	if (panel->op_state == PANEL_STAT_OFF) {
		dsim_info("%s : panel is not active\n", __func__);
		return -EIO;
	}

	mutex_lock(&panel->lock);
	ret = dsim_read_hl_data(dsim, addr, size, buf);
	mutex_unlock(&panel->lock);

	return ret;
}
#endif

struct sync_stat {
	unsigned int main;
	unsigned int sub;
	unsigned int refTE;
	int (*fn)(struct dsim_device *dsim, struct sync_stat *next_stat);
};


int sync_master_slave(struct dsim_device *dsim, struct sync_stat *next_stat)
{
	int ret = 0;
	struct dsim_device *m_dsim, *s_dsim;
	struct panel_private *m_panel, *s_panel;

	dsim_info("%s :  was called\n", __func__);

	if (get_dsim_drvdata(0)) {
		m_dsim = get_dsim_drvdata(0);
		m_panel =  &(get_dsim_drvdata(0)->priv);
	}
	if (get_dsim_drvdata(1)) {
		s_dsim = get_dsim_drvdata(1);
		s_panel = &(get_dsim_drvdata(1)->priv);
	}

	if (dsim == m_dsim)
		dsim_info("request main lcd on\n");
	else
		dsim_info("requet sub lcd on\n");

	ret = m_panel->ops->init(m_dsim, next_stat->main);
	if (ret) {
		dsim_err("ERR:PANEL:%s:failed to master init \n",__func__);
		goto err_sync_master_slave;
	}

	ret = s_panel->ops->init(s_dsim, next_stat->sub);
	if (ret) {
		dsim_err("ERR:PANEL:%s:failed to slave init \n",__func__);
		goto err_sync_master_slave;
	}

	usleep_range(33000, 33000);

	return ret;

err_sync_master_slave:
	return ret;
}

int sync_off_alone(struct dsim_device *dsim, struct sync_stat *next_stat)
{
	int ret = 0;
	struct dsim_device *m_dsim, *s_dsim;
	struct panel_private *m_panel, *s_panel;

	dsim_info("%s :  was called\n", __func__);

	if (get_dsim_drvdata(0)) {
		m_dsim = get_dsim_drvdata(0);
		m_panel =  &(get_dsim_drvdata(0)->priv);
	}
	if (get_dsim_drvdata(1)) {
		s_dsim = get_dsim_drvdata(1);
		s_panel = &(get_dsim_drvdata(1)->priv);
	}

	if (dsim == m_dsim) {
		dsim_info("request main lcd off\n");
		ret = s_panel->ops->init(s_dsim, next_stat->sub);
		if (ret) {
			dsim_err("ERR:PANEL:%s:failed to slave init \n",__func__);
			goto err_sync_off_alone;
		}
		ret = m_panel->ops->exit(m_dsim);
		if (ret) {
			dsim_err("ERR:PANEL:%s:failed to master init \n",__func__);
			goto err_sync_off_alone;
		}
	} else {
		dsim_info("request sub lcd on\n");
		ret = s_panel->ops->init(s_dsim, next_stat->sub);
		if (ret) {
			dsim_err("ERR:PANEL:%s:failed to slave init \n",__func__);
			goto err_sync_off_alone;
		}
	}
	return ret;

err_sync_off_alone:
	return ret;
}


int sync_off_off(struct dsim_device *dsim, struct sync_stat *next_stat)
{
	int ret = 0;
	struct dsim_device *m_dsim, *s_dsim;
	struct panel_private *m_panel, *s_panel;

	dsim_info("%s :  was called\n", __func__);

	if (get_dsim_drvdata(0)) {
		m_dsim = get_dsim_drvdata(0);
		m_panel =  &(get_dsim_drvdata(0)->priv);
	}
	if (get_dsim_drvdata(1)) {
		s_dsim = get_dsim_drvdata(1);
		s_panel = &(get_dsim_drvdata(1)->priv);
	}

	if (dsim == m_dsim) {
		dsim_info("request main lcd off\n");
		ret = m_panel->ops->exit(m_dsim);
		if (ret) {
			dsim_err("ERR:PANEL:%s:failed to slave init \n",__func__);
			goto err_sync_off_off;
		}
	} else {
		dsim_info("request sub lcd off\n");
		ret = s_panel->ops->exit(s_dsim);
		if (ret) {
			dsim_err("ERR:PANEL:%s:failed to slave init \n",__func__);
			goto err_sync_off_off;
		}
	}
	return ret;

err_sync_off_off:
	return ret;
}

int sync_alone_off(struct dsim_device *dsim, struct sync_stat *next_stat)
{
	int ret = 0;

	struct dsim_device *m_dsim, *s_dsim;
	struct panel_private *m_panel, *s_panel;

	dsim_info("%s :  was called\n", __func__);

	if (get_dsim_drvdata(0)) {
		m_dsim = get_dsim_drvdata(0);
		m_panel =  &(get_dsim_drvdata(0)->priv);
	}
	if (get_dsim_drvdata(1)) {
		s_dsim = get_dsim_drvdata(1);
		s_panel = &(get_dsim_drvdata(1)->priv);
	}

	if (dsim == m_dsim) {
		dsim_info("request main lcd on\n");
		ret = m_panel->ops->init(m_dsim, next_stat->main);
		if (ret) {
			dsim_err("ERR:PANEL:%s:failed to master init \n",__func__);
			goto err_sync_alone_off;
		}

	} else {
		dsim_info("request sub lcd off\n");
		ret = m_panel->ops->init(m_dsim, next_stat->main);
		if (ret) {
			dsim_err("ERR:PANEL:%s:failed to master init \n",__func__);
			goto err_sync_alone_off;
		}

		ret = s_panel->ops->exit(s_dsim);
		if (ret) {
			dsim_err("ERR:PANEL:%s:failed to slave exit \n",__func__);
			goto err_sync_alone_off;
		}
	}
	return ret;

err_sync_alone_off:
	return ret;
}


struct sync_stat stat_fsm[REQ_MAX][CUR_MAX] = {
	{
	/* Request to turn on main lcd */
		{.main = PANEL_STAT_MASTER, .sub = PANEL_STAT_SLAVE, .refTE = REF_TE_NONE, .fn = NULL},   /* Main : On, Sub : On */
		{.main = PANEL_STAT_STANDALONE, .sub = PANEL_STAT_OFF, .refTE = REF_TE_MAIN, .fn = NULL}, /* Sub : On, Sub : Off */
		{.main = NEED_TO_UPDATE | FULL_UPDATE | PANEL_STAT_MASTER, .sub = NEED_TO_UPDATE | PANEL_STAT_SLAVE, .refTE = REF_TE_MAIN, .fn = sync_master_slave}, /* Main:Off(Off->Master), Sub : On(Stand->Slave)*/
		{.main = NEED_TO_UPDATE | FULL_UPDATE | PANEL_STAT_STANDALONE, .sub = PANEL_STAT_OFF, .refTE = REF_TE_MAIN, .fn = sync_alone_off} /* Main:Off(Off->Standalone), Sub : Off */
	},
	{
	/* Request to turn on sub lcd */
		{.main = PANEL_STAT_MASTER, .sub = PANEL_STAT_SLAVE, .refTE = REF_TE_NONE, .fn = NULL}, /*Main : On, Sub : On*/
		{.main = NEED_TO_UPDATE | PANEL_STAT_MASTER, .sub = NEED_TO_UPDATE | FULL_UPDATE | PANEL_STAT_SLAVE, .refTE = REF_TE_MAIN, .fn = sync_master_slave}, /* Main : On(Stand -> Master), Sub : Off(Off->Slave)*/
		{.main = PANEL_STAT_OFF, .sub = PANEL_STAT_STANDALONE, .refTE = REF_TE_SUB, .fn = NULL}, /*Main : Off, Sub : On*/
		{.main = PANEL_STAT_OFF, .sub = NEED_TO_UPDATE | FULL_UPDATE | PANEL_STAT_STANDALONE, .refTE = REF_TE_SUB, .fn = sync_off_alone} /* Main : Off, Sub : Off(Off->Stan) */
	},
	{
	/*Requet to turn off main lcd */
		{.main = NEED_TO_UPDATE | FULL_UPDATE | PANEL_STAT_OFF, .sub = NEED_TO_UPDATE | PANEL_STAT_STANDALONE, .refTE = REF_TE_SUB, .fn = sync_off_alone}, /* Main : On(Master->Off), Sub : On(Slave->Stand)*/
		{.main = NEED_TO_UPDATE | FULL_UPDATE | PANEL_STAT_OFF, .sub = PANEL_STAT_OFF, .refTE = REF_TE_NONE, .fn = sync_off_off}, /* Main : On(Stand -> Off), Sub = Off */
		{.main = PANEL_STAT_OFF, .sub = PANEL_STAT_STANDALONE, .refTE = REF_TE_SUB, .fn = NULL}, /*Main : Off, Sub : On(Stand) */
		{.main = PANEL_STAT_OFF, .sub = PANEL_STAT_OFF, .refTE = REF_TE_NONE, .fn = NULL}, /*Main : Off, Sub : Off*/
	},
	{
	/* Request to turn off sub lcd */
		{.main = NEED_TO_UPDATE | PANEL_STAT_STANDALONE, .sub = NEED_TO_UPDATE | FULL_UPDATE | PANEL_STAT_OFF, .refTE = REF_TE_MAIN, .fn = sync_alone_off}, /* Main : On(Master->Stand), Sub : On(Slave->Off)*/
		{.main = PANEL_STAT_STANDALONE, .sub = PANEL_STAT_OFF, .refTE = REF_TE_MAIN, .fn = NULL}, /* Main : On, sub : Off */
		{.main = PANEL_STAT_OFF, .sub = NEED_TO_UPDATE | FULL_UPDATE | PANEL_STAT_OFF, .refTE = REF_TE_NONE, .fn = sync_off_off}, /* Main : Off, Sub : On(Stand->Off)*/
		{.main = PANEL_STAT_OFF, .sub = PANEL_STAT_OFF, .refTE = REF_TE_NONE, .fn = NULL} /*Main : On, Sub : On*/
	}
};

#define REQ_TURN_ON		0
#define REQ_TURN_OFF	0x01


static void print_next_stat(struct sync_stat *next_stat)
{
	unsigned int m_stat = next_stat->main;
	unsigned int s_stat = next_stat->sub;

	dsim_info("sync info : main : %x", m_stat);
	if (m_stat & NEED_TO_UPDATE)
		dsim_info("need to update : \n");
	if (m_stat & FULL_UPDATE)
		dsim_info("full update : \n");
	if ((m_stat& 0x0f) == PANEL_STAT_MASTER)
		dsim_info("master\n");
	else if ((m_stat & 0x0f) == PANEL_STAT_SLAVE)
		dsim_info("slave\n");
	else if ((m_stat & 0x0f) == PANEL_STAT_STANDALONE)
		dsim_info("standalone\n");
	else if ((m_stat & 0x0f) == PANEL_STAT_OFF)
		dsim_info("off\n");

	dsim_info("sync info : sub : %x", s_stat);
		dsim_info("need to update : \n");
	if (s_stat & FULL_UPDATE)
		dsim_info("full update : \n");
	if ((s_stat & 0x0f) == PANEL_STAT_MASTER)
		dsim_info("master\n");
	else if ((s_stat & 0x0f) == PANEL_STAT_SLAVE)
		dsim_info("slave\n");
	else if ((s_stat & 0x0f) == PANEL_STAT_STANDALONE)
		dsim_info("standalone\n");
	else if ((s_stat & 0x0f) == PANEL_STAT_OFF)
		dsim_info("off\n");
}


static struct sync_stat *get_next_panel_stat(struct dsim_device *dsim, int req)
{
	unsigned int req_idx, cur_idx = 0;
	unsigned int m_stat, s_stat;

	struct panel_private *m_panel;
	struct panel_private *s_panel;


	if (get_dsim_drvdata(0))
		m_panel = &(get_dsim_drvdata(0)->priv);
	if (get_dsim_drvdata(1))
		s_panel = &(get_dsim_drvdata(1)->priv);

	m_stat = GET_PANEL_STAT(m_panel->op_state);
	s_stat = GET_PANEL_STAT(s_panel->op_state);

	dsim_info("main stat : %d, sub stat : %d\n", m_stat, s_stat);

	req_idx = dsim->id | req << 1;
	cur_idx = ((m_stat == PANEL_STAT_OFF) ? (0x1 << 1) : 0) |
				((s_stat == PANEL_STAT_OFF) ? 1 : 0);

	dsim_info("sync info : cur : %d, req : %d\n", cur_idx, req_idx);
	if ((req_idx >= REQ_MAX) || (cur_idx >= CUR_MAX)) {
		dsim_err("ERR:DSIM:%s:wrong idx (req : %d, cur :%d)\n", __func__, req_idx, cur_idx);
		return NULL;
	}
	return &stat_fsm[req_idx][cur_idx];
}


static int dsim_panel_set_syncmode(struct dsim_device *dsim, unsigned int req)
{
	int ret = 0;
	struct sync_stat *next_stat;

	next_stat = get_next_panel_stat(dsim, req);
	if (next_stat == NULL) {
		dsim_err("ERR:DSIM:%s:can't get next stat\n", __func__);
		goto init_err;
	}

	print_next_stat(next_stat);

	if (next_stat->fn == NULL) {
		dsim_info("PANEL:%s:id-%d:skip set syncmode\n", __func__, dsim->id);
		goto init_err;
	}

	ret = next_stat->fn(dsim, next_stat);
	if (ret) {
		dsim_err("ERR:PANEL:%s:id-%d:failed to set syncmode\n", __func__, dsim->id);
		goto init_err;
	}
	return ret;

init_err:
	return ret;
}


static int dsim_panel_early_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	panel->ops = dsim_panel_get_priv_ops(dsim);

	if (panel->ops->early_probe) {
		ret = panel->ops->early_probe(dsim);
	}
	return ret;
}

static int dsim_panel_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	const char *lcd_device_name[2] = {
		"panel",
		"panel_1"
	};
	dsim_info("%s:id-%d was called\n", __func__, dsim->id);

	dsim->lcd = lcd_device_register(lcd_device_name[dsim->id], dsim->dev, &dsim->priv, NULL);
	if (IS_ERR(dsim->lcd)) {
		dsim_err("%s : faield to register lcd device\n", __func__);
		ret = PTR_ERR(dsim->lcd);
		goto probe_err;
	}

	ret = probe_backlight_drv(dsim);
	if (ret) {
		dsim_err("%s : failed to prbe backlight driver\n", __func__);
		goto probe_err;
	}

	panel->temperature = NORMAL_TEMPERATURE;
	panel->lcdConnected = PANEL_CONNECTED;
	panel->acl_enable = 0;
	panel->current_acl = 0;
	panel->siop_enable = 0;
	panel->current_hbm = 0;
	panel->current_vint = 0;
	panel->adaptive_control = 1;

	if (dsim->id == 0)
		panel->op_state = PANEL_STAT_STANDALONE | PANEL_STAT_DISPLAY_ON;
	else
		panel->op_state = PANEL_STAT_OFF;

	mutex_init(&panel->lock);

	if (panel->ops->probe) {
		ret = panel->ops->probe(dsim);
		if (ret) {
			dsim_err("%s : failed to probe panel\n", __func__);
			goto probe_err;
		}
	}
	if (panel->op_state == PANEL_STAT_OFF) {
		dsim_panel_set_syncmode(dsim, REQ_TURN_ON);
		if (ret) {
			dsim_err("%s : failed to init panel\n", __func__);
			goto probe_err;
		}
	}

#if defined(CONFIG_EXYNOS_DECON_LCD_SYSFS)
	lcd_init_sysfs(dsim);
#endif

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
	if (panel->mdnie_support) {
		mdnie_register(&dsim->lcd->dev, dsim, (mdnie_w)mdnie_lite_send_seq, (mdnie_r)mdnie_lite_read, panel->coordinate, &tune_info);
		panel->mdnie_class = get_mdnie_class();
	}
#endif

probe_err:
	return ret;
}


static int dsim_panel_displayon(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("%s:id-%d was called\n", __func__, dsim->id);

	if (dsim == NULL || panel->ops->displayon == NULL)
		goto err_display_on;

	if (panel->op_state == PANEL_STAT_OFF)
		goto err_display_on;

	panel_set_brightness(dsim, 1);

	ret = panel->ops->displayon(dsim);
	if (ret) {
		dsim_err("%s : failed to panel display on\n", __func__);
		goto err_display_on;
	}
	return 0;

err_display_on:
	return -1;

}

static int dsim_panel_suspend(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("%s:id-%d was called\n", __func__, dsim->id);

	if (panel->op_state == PANEL_STAT_OFF)
		goto suspend_err;

	ret = dsim_panel_set_syncmode(dsim, REQ_TURN_OFF);
	if (ret) {
		dsim_err("%s : failed to init panel\n", __func__);
		goto suspend_err;
	}

suspend_err:
	return ret;
}

static int dsim_panel_resume(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	if (panel->op_state != PANEL_STAT_OFF)
		goto resume_err;

	ret = dsim_panel_set_syncmode(dsim, REQ_TURN_ON);
	if (ret) {
		dsim_err("%s : failed to init panel\n", __func__);
		goto resume_err;
	}

resume_err:
	return ret;

}


static int dsim_panel_dump(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("%s was called\n", __func__);

	if (panel->ops->dump)
		ret = panel->ops->dump(dsim);

	return ret;
}
static struct mipi_dsim_lcd_driver mipi_lcd_driver = {
	.early_probe = dsim_panel_early_probe,
	.probe		= dsim_panel_probe,
	.displayon	= dsim_panel_displayon,
	.suspend	= dsim_panel_suspend,
	.resume		= dsim_panel_resume,
	.dump		= dsim_panel_dump,
};


int dsim_panel_ops_init(struct dsim_device *dsim)
{
	int ret = 0;

	if (dsim)
		dsim->panel_ops = &mipi_lcd_driver;

	return ret;
}

unsigned int lcdtype = 0;
EXPORT_SYMBOL(lcdtype);

static int __init get_lcd_type(char *arg)
{
	get_option(&arg, &lcdtype);

	dsim_info("--- Parse LCD TYPE ---\n");
	dsim_info("LCDTYPE : %x\n", lcdtype);

	return 0;
}
early_param("lcdtype", get_lcd_type);

