/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SMCDSD_PANEL_H__
#define __SMCDSD_PANEL_H__

#include "smcdsd_abd.h"

enum CMDQ_DISP_MODE {
	CMDQ_PRIMARY_DISPLAY = 0,
	CMDQ_MASK_BRIGHTNESS = 1,
};
#if defined(CONFIG_SMCDSD_PROTOS_PLUS)
enum protos_mode {
	PROTOS_NONE,
	PROTOS_READY,
	PROTOS_AWAKE,
	PROTOS_SLEEP,
	PROTOS_MAX,
};

enum doze_mode {
	DOZE = 0,
	DOZE_SUSPEND = 1,
};
#endif
struct mipi_dsi_lcd_parent {
	struct platform_device		*pdev;
	struct mutex			lock;
	struct mipi_dsi_lcd_driver	*drv;
	unsigned int			cmd_q;
	unsigned int			probe;
	bool				power;
	unsigned int			lcdconnected;
	unsigned int data_rate[4];

	int (*tx)(unsigned int, unsigned long, unsigned int);
	int (*rx)(unsigned int, unsigned int, u8, unsigned int, u8 *);

	struct abd_protect		abd;

	bool			mask_wait;
	void			*mask_qhandle;
	int			cmdq_index;

	bool			lcm_path_lock;
	u32 protos_mode;
};

struct mipi_dsi_lcd_driver {
	struct list_head node;
	struct device_driver driver;
	int	(*probe)(struct platform_device *pdev);
	int	(*init)(struct platform_device *pdev);
	int	(*exit)(struct platform_device *pdev);
	int	(*panel_i2c_init)(struct platform_device *pdev);
	int	(*panel_i2c_exit)(struct platform_device *pdev);
	int	(*displayon_late)(struct platform_device *pdev);
	int	(*dump)(struct platform_device *pdev);
#if defined(CONFIG_SMCDSD_DOZE)
	int	(*doze_init)(struct platform_device *pdev);
	int	(*doze_exit)(struct platform_device *pdev);
#endif
	int	(*set_mask)(struct platform_device *pdev, int on);
	int	(*get_mask)(struct platform_device *pdev);
	int	(*lock)(struct platform_device *pdev);
	int	(*framedone_notify)(struct platform_device *pdev);
#if defined(CONFIG_SMCDSD_PROTOS_PLUS)
	int (*get_curr_protos_mode)(struct platform_device *pdev);
#endif
};

extern struct mipi_dsi_lcd_parent *g_smcdsd;
extern unsigned int lcdtype;
extern unsigned char read_offset;
extern unsigned int islcmconnected;

static inline struct mipi_dsi_lcd_parent *get_smcdsd_drvdata(u32 id)
{
	return g_smcdsd;
}

#define call_drv_ops(op, args...)				\
	((g_smcdsd) && ((g_smcdsd)->drv->op) ? ((g_smcdsd)->drv->op((g_smcdsd)->pdev, ##args)) : 0)

#define __XX_ADD_LCD_DRIVER(name)		\
struct mipi_dsi_lcd_driver *p_##name __attribute__((used, section("__lcd_driver"))) = &name

extern struct mipi_dsi_lcd_driver *__start___lcd_driver;
extern struct mipi_dsi_lcd_driver *__stop___lcd_driver;

#if defined(CONFIG_SMCDSD_TD4150_A01CORE)
extern struct mipi_dsi_lcd_driver td4150_mipi_lcd_driver;
#endif

#if defined(CONFIG_SMCDSD_NT36525B_A01CORE)
extern struct mipi_dsi_lcd_driver nt36525b_mipi_lcd_driver;
#endif

#if defined(CONFIG_SMCDSD_PANEL)
extern struct LCM_DRIVER smcdsd_panel_drv;
#endif
extern struct device_node *of_find_lcd_info(struct device *dev);
extern char *mtkfb_find_lcm_driver(void);
extern int smcdsd_panel_dsi_command_tx(unsigned int id, unsigned long data0, unsigned int data1);
extern int smcdsd_panel_dsi_command_rx(unsigned int id, unsigned int offset, u8 cmd, unsigned int len, u8 *buf);
extern int smcdsd_panel_dsi_clk_change(unsigned int target_clk);

extern int primary_display_dsi_set_withrawcmdq(unsigned int *pdata,
	unsigned int queue_size, unsigned char force_update);
extern int primary_display_dsi_read_cmdq_cmd(unsigned int data_id,
	unsigned int offset, unsigned int cmd, unsigned char *buffer,
	unsigned char size);
extern int mipi_clk_change(int msg, int en);
#if defined(CONFIG_SMCDSD_PROTOS_PLUS)
extern int smcdsd_set_doze(enum doze_mode mode);
#endif
#endif
