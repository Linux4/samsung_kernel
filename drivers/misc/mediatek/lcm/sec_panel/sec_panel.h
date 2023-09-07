/* linux/drivers/misc/mediatek/lcm/sec_panel/sec_panel.h
 *
 * Copyright (c) 2017 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __SEC_PANEL_HEADER__
#define __SEC_PANEL_HEADER__

#include <linux/device.h>

struct mipi_dsi_lcd {
	struct platform_device	*pdev;
	struct mutex			lock;
	struct notifier_block	fb_notifier;
	struct mipi_dsi_lcd_driver	*drv;
	bool					cmd_q;
	unsigned int			probe;
	bool					power;

	int (*tx)(unsigned int, unsigned long, unsigned int);
	int (*rx)(unsigned int, u8, unsigned int, u8 *);
};

struct mipi_dsi_lcd_driver {
	struct list_head node;
	struct device_driver driver;
	int	(*probe)(struct platform_device *pdev);
	int	(*init)(struct platform_device *pdev);
	int	(*exit)(struct platform_device *pdev);
	int	(*displayon_late)(struct platform_device *pdev);
	int	(*dump)(struct platform_device *pdev);
#ifdef CONFIG_SEC_PANEL_DOZE_MODE
	int	(*doze_init)(struct platform_device *pdev);
	int	(*doze_exit)(struct platform_device *pdev);
#endif
};

extern struct mipi_dsi_lcd *g_panel;
extern unsigned int lcdtype;

#define call_drv_ops(op)				\
	((g_panel) && ((g_panel)->drv->op) ? ((g_panel)->drv->op((g_panel)->pdev)) : 0)

extern int sec_panel_register_driver(struct mipi_dsi_lcd_driver *drv);
extern int primary_display_dsi_set_withrawcmdq(unsigned int *pdata, unsigned int queue_size, unsigned char force_update);
extern int primary_display_dsi_read_cmdq_cmd(unsigned data_id, unsigned offset, unsigned cmd, unsigned char *buffer, unsigned char size);
#endif
