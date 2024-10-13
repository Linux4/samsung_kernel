/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/earlysuspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>

#include "sprd_display.h"
#include "sprd_interface.h"
#include "sprd_dispc_reg.h"


static struct panel_if_device *g_sprd_interfaces[SPRDFB_MAX_LCD_ID];
static uint32_t lcd_id_from_uboot;
unsigned long lcd_base_from_uboot;
struct panel_if_ctrl *get_if_ctrl(int if_type)
{
	struct panel_if_ctrl *ctrl;
	switch (if_type) {
	case SPRDFB_PANEL_TYPE_MIPI:
		ctrl = &sprd_mipi_ctrl;
		break;
	case SPRDFB_PANEL_TYPE_MCU:
		PERROR("Cannot support mcu interface now\n");
		ctrl = NULL;
		break;
	case SPRDFB_PANEL_TYPE_RGB:
		PERROR("Cannot support rgb interface now\n");
		ctrl = NULL;
		break;
	default:
		ctrl = NULL;
	}
	return ctrl;
}

static int __init lcd_id_get(char *str)
{
	if ((str != NULL) && (str[0] == 'I') && (str[1] == 'D'))
		sscanf(&str[2], "%x", &lcd_id_from_uboot);
	PERROR("LCD Panel ID from uboot: 0x%x\n"
		   , lcd_id_from_uboot);
	return 1;
}

__setup("lcd_id=", lcd_id_get);
static int __init lcd_base_get(char *str)
{
	if (str != NULL)
		sscanf(&str[0], "%lx", &lcd_base_from_uboot);
	PERROR("LCD Panel Base from uboot: 0x%lx\n",
	       lcd_base_from_uboot);
	return 1;
}

__setup("lcd_base=", lcd_base_get);


static int32_t panel_reset_dispc(struct panel_if_device *intf)
{
	uint16_t timing1, timing2, timing3;
	struct panel_spec *panel = intf->get_panel(intf);

	if ((NULL != panel) && (0 != panel->reset_timing.time1) &&
	    (0 != panel->reset_timing.time2)
	    && (0 != panel->reset_timing.time3)) {
		timing1 = panel->reset_timing.time1;
		timing2 = panel->reset_timing.time2;
		timing3 = panel->reset_timing.time3;
	} else {
		timing1 = 20;
		timing2 = 20;
		timing3 = 120;
	}

	dispc_write(intf->dev_id, 1, DISPC_RSTN);
	usleep_range(timing1 * 1000, timing1 * 1000 + 500);
	dispc_write(intf->dev_id, 0, DISPC_RSTN);
	usleep_range(timing2 * 1000, timing2 * 1000 + 500);
	dispc_write(intf->dev_id, 1, DISPC_RSTN);

	/* wait 10ms util the lcd is stable */
	usleep_range(timing3 * 1000, timing3 * 1000 + 500);

	return 0;
}

static int32_t panel_set_resetpin_dispc(int dev_id, uint32_t status)
{
	if (0 == status)
		dispc_write(dev_id, 0, DISPC_RSTN);
	else
		dispc_write(dev_id, 1, DISPC_RSTN);
	return 0;
}

static int panel_reset(struct panel_if_device *intf)
{
	struct panel_spec *panel = intf->get_panel(intf);

	if (NULL == panel) {
		PERROR(" Invalid param\n");
		return -1;
	}

	/* clk/data lane enter LP */
	if (NULL != intf->ctrl->panel_if_before_panel_reset)
		intf->ctrl->panel_if_before_panel_reset(intf);
	usleep_range(5000, 5500);

	/* reset panel rst_pin high ,low,high */
	panel_reset_dispc(intf);

	return 0;
}

static void panel_set_resetpin(uint16_t dev_id, uint32_t status)
{

	/*panel set reset pin status */
	if (SPRDFB_MAINLCD_ID == dev_id)
		panel_set_resetpin_dispc(dev_id, status);
}

static int32_t panel_before_resume(struct panel_if_device *intf)
{
	/*restore  the reset pin to high */
	panel_set_resetpin(intf->dev_id, 1);
	return 0;
}

static int32_t panel_after_suspend(struct panel_if_device *intf)
{
	/*set the reset pin to low */
	panel_set_resetpin(intf->dev_id, 0);
	return 0;
}

static int panel_ready(struct panel_if_device *intf)
{
	struct panel_spec *panel = intf->get_panel(intf);
	if (NULL == panel) {
		PERROR("Invalid param\n");
		return -1;
	}

	pr_debug("sprd: [%s], dev_id= %d, type = %d\n", __func__,
		 panel->dev_id, panel->type);

	if (NULL != intf->ctrl->panel_if_ready)
		intf->ctrl->panel_if_ready(intf);

	return 0;
}

static void sprd_panel_suspend(struct panel_if_device *intf)
{
	struct panel_spec *panel = intf->get_panel(intf);
	if (NULL == panel)
		return;

#if 0
	/* step1-1 clk/data lane enter LP */
	if (NULL != dev->panel->if_ctrl->panel_if_before_panel_reset)
		dev->panel->if_ctrl->panel_if_before_panel_reset(dev);
	/* step1-2 enter sleep  (another way : reset panel) */
	/*Jessica TODO: Need do some I2c, SPI, mipi sleep here */
	/* let lcdc sleep in */

	if (dev->panel->ops->panel_enter_sleep != NULL)
		dev->panel->ops->panel_enter_sleep(intf, 1);
	msleep(100);

	/* step1 send lcd sleep cmd or reset panel directly */
	if (panel->suspend_mode == SEND_SLEEP_CMD)
		panel_sleep(intf);
	 else
		panel_reset(intf);
#endif

	/* step2 clk/data lane enter ulps */
	if (NULL != intf->ctrl->panel_if_enter_ulps) {
		PRINT("TODO: some panel will send ulps command by self\n");
		intf->ctrl->panel_if_enter_ulps(intf);
	}
	/* step3 turn off mipi */
	if (NULL != intf->ctrl->panel_if_suspend)
		intf->ctrl->panel_if_suspend(intf);
	/* step4 reset pin to low */
	if (panel->ops->panel_after_suspend != NULL) {
		/* himax mipi lcd may define empty function */
		panel->ops->panel_after_suspend(intf);
	} else
		panel_after_suspend(intf);
}

static void sprd_panel_resume(struct panel_if_device *intf,
	bool from_deep_sleep)
{
	struct panel_spec *panel = intf->get_panel(intf);

	if (NULL == panel)
		return;

	PRINT("from_deep_sleep = %d\n", from_deep_sleep);
	/* step1 reset pin to high */
	if (panel->ops->panel_before_resume != NULL) {
		/* himax mipi lcd may define empty function */
		panel->ops->panel_before_resume(intf);
	} else
		panel_before_resume(intf);

	if (from_deep_sleep) {
		/* step2 turn on mipi */
		intf->ctrl->panel_if_init(intf);
		/* step3 reset panel */
		panel_reset(intf);
		/* step4 panel init */
		panel->ops->panel_init(intf);
		/* step5 clk/data lane enter HS */
		panel_ready(intf);
	} else {
		/* step2 turn on mipi */
		if (NULL != intf->ctrl->panel_if_resume)
			intf->ctrl->panel_if_resume(intf);
		/* step3 sleep out */
		if (NULL != panel->ops->panel_enter_sleep)
			panel->ops->panel_enter_sleep(intf, 0);
		/* step4 clk/data lane enter HS */
		panel_ready(intf);
	}

}

static void sprd_panel_shutdown(struct panel_if_device *intf)
{
#if 0
	dispc_write(intf->dev_id, 0, DISPC_RSTN);
	mdelay(120);
#endif
	if (intf->need_do_esd)
		cancel_delayed_work_sync(&intf->esd_work);
	sprd_panel_suspend(intf);
}
static void esd_work_func(struct work_struct *work)
{
	struct panel_if_device *intf = container_of(work,
		struct panel_if_device, esd_work.work);
	int ret;
	static uint32_t count;
	if (intf->ctrl->panel_if_esd)
		ret = intf->ctrl->panel_if_esd(intf);
	if (ret < 0) {
		PERROR("ESD check error , Need restart panel_if\n");
		sprd_panel_suspend(intf);
		sprd_panel_resume(intf, true);
	}
	if (++count % 30 == 0)
		PERROR("ESD function: %d\n", count);

	schedule_delayed_work(&intf->esd_work,
		msecs_to_jiffies(intf->esd_timeout));
}
static void panel_if_suspend(struct early_suspend *es)
{
	struct panel_if_device **interfaces = g_sprd_interfaces;
	int index;
	ENTRY();
	for (index = 0; index < SPRDFB_MAX_LCD_ID; index++) {
		struct panel_if_device *intf = interfaces[index];
		if (intf) {
			PRINT("intf [%d] suspend\n", index);
			if (intf->need_do_esd)
				cancel_delayed_work_sync(&intf->esd_work);

			sprd_panel_suspend(intf);
		}
	}
	LEAVE();
}

static void panel_if_resume(struct early_suspend *es)
{
	struct panel_if_device **interfaces = g_sprd_interfaces;
	int index;
	ENTRY();
	for (index = 0; index < SPRDFB_MAX_LCD_ID; index++) {
		struct panel_if_device *intf = interfaces[index];
		if (intf) {
			int from_deep_sleep = 1;
			PRINT("intf [%d] suspend,from_deep_sleep = %d\n",
				index, from_deep_sleep);
			sprd_panel_resume(intf, from_deep_sleep);
			if (intf->need_do_esd)
				schedule_delayed_work(&intf->esd_work,
					msecs_to_jiffies(intf->esd_timeout));

		}
	}
	LEAVE();
}

static struct panel_spec *interface_get_panel(struct panel_if_device *intf)
{
	if (intf->type == SPRDFB_PANEL_TYPE_MCU)
		return intf->info.mcu->panel;
	if (intf->type == SPRDFB_PANEL_TYPE_RGB)
		return intf->info.rgb->panel;
	if (intf->type == SPRDFB_PANEL_TYPE_MIPI)
		return intf->info.mipi->panel;
	return NULL;
}

static struct panel_if_device *interface_alloc(int type)
{
#define BYTES_PER_LONG (BITS_PER_LONG/8)
#define PADDING(size) (BYTES_PER_LONG - (size % BYTES_PER_LONG))
	struct panel_if_device *intf;
	int info_size, intf_size;
	int dsi_context_size = 0;
	char *p;

	intf_size = sizeof(struct panel_if_device);
	intf_size += PADDING(intf_size);
	switch (type) {
	case SPRDFB_PANEL_TYPE_MIPI:
		PRINT("type =  SPRDFB_PANEL_TYPE_MIPI\n");
		info_size = sizeof(struct info_mipi);
		info_size += PADDING(info_size);
		dsi_context_size = sizeof(struct sprd_dsi_context);
		break;
	case SPRDFB_PANEL_TYPE_RGB:
		PRINT("type =  SPRDFB_PANEL_TYPE_RGB\n");
		info_size = sizeof(struct info_rgb);
		info_size += PADDING(info_size);
		break;
	case SPRDFB_PANEL_TYPE_MCU:
		PRINT("type =  SPRDFB_PANEL_TYPE_MCU\n");
		info_size = sizeof(struct info_mcu);
		info_size += PADDING(info_size);
		break;
	default:
		PERROR("error type\n");
		return NULL;
	}

	p = kzalloc(intf_size + info_size +
					      dsi_context_size, GFP_KERNEL);
	if (!p) {
		PERROR("alloc interface fail\n");
		return NULL;
	}
	intf = (struct panel_if_device *)p;
	intf->info.mipi = (struct info_mipi *)(p + intf_size);
	intf->info.mipi->dsi_ctx =
	    (struct sprd_dsi_context *)(p + intf_size + info_size);
	intf->info.rgb = (struct info_rgb *)(p + intf_size);
	intf->info.mcu = (struct info_mcu *)(p + intf_size);
	return intf;
}

static int get_panel_if_nums_dts(struct platform_device *pdev)
{

	unsigned int temp;
	int rc;
	rc = of_property_read_u32(pdev->dev.of_node, "intf-nums", &temp);
	if (rc) {
		PERROR("%s:%d, intf is not specified\n", __func__, __LINE__);
		return -EINVAL;
	}
	return temp;

}

static int sprd_dsi_parse_dcs_cmds(struct device_node *np,
				   struct panel_cmds **in_pcmds, char *name)
{
	const char *base;
	int blen = 0, len;
	char *buf, *bp;
	struct dsi_cmd_header *header;
	struct panel_cmds *pcmds;
	int i, total = 0;

	*in_pcmds = kzalloc(sizeof(struct panel_cmds), GFP_KERNEL);
	pcmds = *in_pcmds;
	if (IS_ERR_OR_NULL(pcmds)) {
		PERROR("pcmds alloc memory fail\n");
		return -ENOMEM;
	}
	base = of_get_property(np, name, &blen);
	if (!base) {
		PERROR("failed, name=%s\n", name);
		return -ENOMEM;
	}

	buf = kzalloc(sizeof(char) * blen, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memcpy(buf, base, blen);

	for (i = 0; i < blen; i += 4)
		pr_debug("sprd panel cmds  %2x  %2x %2x %2x\n",
			buf[i], buf[i + 1], buf[i + 2], buf[i + 3]);
	/* scan dcs commands */
	bp = buf;
	len = blen;
	pr_debug("sprd is panel cmds len %2x\n", len);
	total = 0;
	while (len >= sizeof(struct dsi_cmd_header)) {
		header = (struct dsi_cmd_header *)bp;
		header->len = ntohs(header->len);
		if (header->len > len) {
			PERROR("%s: dsi total =%2x wait=%x error, len=%x\n",
			      __func__, total, header->wait, header->len);
			goto error;
		}
		bp += sizeof(*header);
		len -= sizeof(*header);
		bp += header->len;
		len -= header->len;
		total++;
	}

	if (len != 0) {
		PERROR("%s: dcs_cmd=%x len=%d error!", __func__, buf[0], blen);
		goto error;
	}

	pcmds->cmds = kzalloc(total * sizeof(struct dsi_cmd_desc), GFP_KERNEL);
	if (!pcmds->cmds)
		goto error;

	pcmds->cmd_total = total;
	pcmds->cmd_base = buf;
	pcmds->byte_len = blen;

	bp = buf;
	len = blen;
	for (i = 0; i < total; i++) {
		header = (struct dsi_cmd_header *)bp;
		len -= sizeof(*header);
		bp += sizeof(*header);
		pcmds->cmds[i].header = *header;
		pcmds->cmds[i].payload = bp;
		bp += header->len;
		len -= header->len;
	}

	return 0;

error:
	kfree(buf);
	return -ENOMEM;
}

int get_panel_from_dts(struct device_node *panel_node,
		       struct panel_spec *panel, unsigned int lcd_id)
{
	unsigned int temp;
	int rc;
	char *name;
	const char *lcd_name;
	struct panel_cmds *pcmds = NULL;

	/* unsigned char MAX_NAME = 30; */
	name = kzalloc(sizeof("lcd8888"), GFP_KERNEL);
	sprintf(name, "lcd%x", lcd_id);
	if (strcmp(panel_node->name, name)) {
		PERROR("cannot connect  panel %s %s,next...\n", panel_node->name,name);
		return -1;
	}else
		PRINT("connect %s panel\n", panel_node->name);

	rc = sprd_dsi_parse_dcs_cmds(panel_node, &pcmds, "init_data");
	if (!rc) {
		panel->lcd_cmds = pcmds;
	} else {
		PERROR("sprd_dsi_parse_dcs_cmds error\n");
		return -1;
	}
	rc = of_property_read_u32(panel_node, "esd_return_code", &temp);
	if (!rc)
		panel->esd_return_code = temp;
	rc = sprd_dsi_parse_dcs_cmds(panel_node, &pcmds, "esd_check");
	if (!rc) {
		panel->esd_cmds = pcmds;
	} else {
		PERROR("esd_check error\n");
	}
	rc = sprd_dsi_parse_dcs_cmds(panel_node, &pcmds, "read_id_cmds");
	if (!rc) {
		panel->read_id_cmds = pcmds;
	} else {
		PERROR("read_id_cmds error\n");
	}
	rc = sprd_dsi_parse_dcs_cmds(panel_node, &pcmds, "sleep_out");
	if (!rc) {
		PRINT(" pcmds panel = %p\n", panel);
		panel->sleep_cmds = pcmds;
	} else {
		PERROR("sleep_out error\n");
	}
	rc = sprd_dsi_parse_dcs_cmds(panel_node, &pcmds, "sleep_in");
	if (!rc) {
		PRINT(" pcmds panel = %p\n", panel);
		panel->sleep_cmds = pcmds;
	} else {
		PERROR("sleep_in error\n");
	}
	rc = of_property_read_u32(panel_node, "hfp", &temp);
	if (!rc)
		panel->timing.rgb_timing.hfp = temp;
	rc = of_property_read_u32(panel_node, "hbp", &temp);
	if (!rc)
		panel->timing.rgb_timing.hbp = temp;
	rc = of_property_read_u32(panel_node, "hsync", &temp);
	if (!rc)
		panel->timing.rgb_timing.hsync = temp;
	rc = of_property_read_u32(panel_node, "vfp", &temp);
	if (!rc)
		panel->timing.rgb_timing.vfp = temp;
	rc = of_property_read_u32(panel_node, "vbp", &temp);
	if (!rc)
		panel->timing.rgb_timing.vbp = temp;
	rc = of_property_read_u32(panel_node, "vsync", &temp);
	if (!rc)
		panel->timing.rgb_timing.vsync = temp;
	rc = of_property_read_u32(panel_node, "work_mode", &temp);
	if (!rc) {
		PRINT("panel->work_mode %d\n",panel->work_mode);
		panel->work_mode = temp;
	}
	rc = of_property_read_u32(panel_node, "bpp", &temp);
	if (!rc)
		panel->video_bus_width = temp;
	rc = of_property_read_u32(panel_node, "lan_number", &temp);
	if (!rc) {
		panel->lan_number = temp;
		PRINT("lan_number = %d\n", panel->lan_number);
	}
	rc = of_property_read_u32(panel_node, "phy_feq", &temp);
	if (!rc)
		panel->phy_feq = temp;
	rc = of_property_read_u32(panel_node, "h_sync_pol", &temp);
	if (!rc)
		panel->h_sync_pol = temp;
	rc = of_property_read_u32(panel_node, "v_sync_pol", &temp);
	if (!rc)
		panel->v_sync_pol = temp;
	rc = of_property_read_u32(panel_node, "de_pol", &temp);
	if (!rc)
		panel->de_pol = temp;
	rc = of_property_read_u32(panel_node, "te_pol", &temp);
	if (!rc)
		panel->te_pol = temp;
	rc = of_property_read_u32(panel_node, "color_mode_pol", &temp);
	if (!rc)
		panel->color_mode_pol = temp;
	rc = of_property_read_u32(panel_node, "shut_down_pol", &temp);
	if (!rc)
		panel->shut_down_pol = temp;
	rc = of_property_read_u32(panel_node, "width", &temp);
	if (!rc)
		panel->width = temp;
	rc = of_property_read_u32(panel_node, "height", &temp);
	if (!rc)
		panel->height = temp;
	rc = of_property_read_u32(panel_node, "fps", &temp);
	if (!rc)
		panel->fps = temp;
	rc = of_property_read_u32(panel_node, "direction", &temp);
	if (!rc)
		panel->direction = temp;
	rc = of_property_read_u32(panel_node, "lcd_id", &temp);
	if (!rc)
		panel->lcd_id = temp;
	rc = of_property_read_string(panel_node, "panel_name", &lcd_name);
	if (!rc){
		PDBG(lcd_name);
		panel->lcd_name = lcd_name;
		PDBG(panel->lcd_name);
	}
	
	return 0;
}

int dc_connect_interface(struct sprd_device *dev, int index)
{
	if (!dev) {
		PERROR("sprd_device NULL\n");
		BUG();
	}
	dev->intf = g_sprd_interfaces[index];
	if (dev->intf) {
		PRINT("[%d] dev->intf = %p\n", index, dev->intf);
		return 0;
	}
	return -ENODEV;
}
static struct panel_spec *get_panel_node(struct device_node *panel_if_node,
					 int panel_id_from_uboot)
{
	int i;
	int rc;
	struct device_node *panel_node = NULL;
	struct panel_spec *panel;

	if (panel_id_from_uboot == 0) {
		PERROR(" no panel connected, Use dummy instead\n");
		panel_id_from_uboot = 0xffff;
	}

	for (i = 0; i < 10; i++) {
		panel_node = of_parse_phandle(panel_if_node, "panel_driver", i);
		panel = (struct panel_spec *)
			kzalloc(sizeof(struct panel_spec), GFP_KERNEL);
		rc = get_panel_from_dts(panel_node, panel, panel_id_from_uboot);
		if (rc == 0) {
			PRINT("get panel sucess %s\n", panel->lcd_name);
			break;
		}
		kfree(panel);
	}
	return panel;
}

static int mipi_dsi_context_init(struct panel_if_device *intf,
		struct device_node *panel_if_node)
{
	int temp,rc;
	struct resource r;
	char temp_str[100] = {0};
	struct device_node *dsi_node = NULL;

	if (intf->type == SPRDFB_PANEL_TYPE_MIPI) {
		struct sprd_dsi_context *dsi_ctx = intf->info.mipi->dsi_ctx;
		sprintf(temp_str, "sprd,dsi%d", intf->dev_id);
		PRINT("of_parse_phandle : %s\n",temp_str);
		dsi_node = of_parse_phandle(panel_if_node, temp_str, 0);
		if (dsi_node == NULL) {
			PERROR("error: dsi_node cannot found\n");
			goto err0;
		}
		if (0 != of_address_to_resource(dsi_node, 0, &r)) {
			PERROR("can't get register base address)\n");
			goto err0;
		}
		XDBG(r.start);
		dsi_ctx->dsi_inst.address =
			(unsigned long)ioremap_nocache(r.start, resource_size(&r));
		if (0 == dsi_ctx->dsi_inst.address) {
			PERROR("dsi_inst.address = 0!\n");
			return -ENOMEM;
		}
		dsi_ctx->dsi_node = dsi_node;
		dsi_ctx->dsi_inst.dev_id = intf->dev_id;
	}
	return 0;
err0:
	PERROR("failed to probe sprd\n");
	return -ENODEV;
}
static void register_earlysuspend(void)
{
	struct early_suspend *early_suspend;
	early_suspend = kzalloc(sizeof(struct early_suspend), GFP_KERNEL);
	early_suspend->suspend = panel_if_suspend;
	early_suspend->resume  = panel_if_resume;
	early_suspend->level   = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(early_suspend);
}
static int panel_if_probe(struct platform_device *pdev)
{
#define NO_PANEL (0)
#define PANEL_NT35597 (0x5597)
	struct device_node *panel_if_node = NULL;
	int num_intf;
	int index;
	int ret;
	char temp_str[100];
	int if_type;
	struct panel_spec *panel;
	struct panel_if_device *intf;
	int panel_id_from_uboots[2]={lcd_id_from_uboot,NO_PANEL};

	num_intf = get_panel_if_nums_dts(pdev);
	if (num_intf < 0) {
		PERROR("error: num_intf < 0\n");
		return -ENODEV;
	}
	for (index = 0; index < num_intf; index++) {
		sprintf(temp_str, "panel-if%d", index);
		panel_if_node =
			of_get_child_by_name(pdev->dev.of_node, temp_str);
		of_property_read_u32(panel_if_node, "type", &if_type);
		intf = interface_alloc(if_type);
		intf->ctrl = get_if_ctrl(if_type);
		intf->dev_id = index;
		intf->type = if_type;
		intf->of_dev = &pdev->dev;
		intf->panel_ready = true;
		intf->get_panel = interface_get_panel;
		intf->need_do_esd =
			of_property_read_bool(panel_if_node, "need_do_esd");
		panel = get_panel_node(panel_if_node,
							   panel_id_from_uboots[index]);
		mipi_dsi_context_init(intf, panel_if_node);

		if (!intf->ctrl->panel_if_attach(panel, intf)) {
			PERROR("panel %d attach interface %d fail\n",
				   panel->dev_id, intf->dev_id);
			goto err0;
		}
		g_sprd_interfaces[index] = intf;
		if (intf->need_do_esd) {
			INIT_DELAYED_WORK(&intf->esd_work, esd_work_func);
			intf->esd_timeout = 1000;
			schedule_delayed_work(&intf->esd_work,
					msecs_to_jiffies(intf->esd_timeout));
		}
		continue;
	err0:
		dev_err(&pdev->dev, "failed to probe sprd\n");
		kfree(intf);
	}
	platform_set_drvdata(pdev, g_sprd_interfaces);
	register_earlysuspend();
	return 0;
}

static void panel_if_shutdown(struct platform_device *pdev)
{
	struct panel_if_device **interfaces = platform_get_drvdata(pdev);
	int index;

	for (index = 0; index < SPRDFB_MAX_LCD_ID; index++) {
		struct panel_if_device *intf = interfaces[index];
		if (intf) {
			PRINT("intf [%d] suspend\n", index);
			sprd_panel_shutdown(intf);
		}
	}
}

static int panel_if_remove(struct platform_device *pdev)
{
	struct panel_if_device **interfaces = platform_get_drvdata(pdev);
	int index;

	for (index = 0; index < SPRDFB_MAX_LCD_ID; index++) {
		struct panel_if_device *intf = interfaces[index];
		if (intf) {
			PRINT("intf [%d] suspend\n", index);
			intf->ctrl->panel_if_uninit(intf);
			kfree(intf);
		}
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id interface_dt_ids[] = {
	{.compatible = "sprd,panel-if",},
	{}
};
#endif

static struct platform_driver panel_if_driver = {
	.probe = panel_if_probe,

#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = panel_if_suspend,
	.resume = panel_if_resume,
#endif
	.remove = panel_if_remove,
	.shutdown = panel_if_shutdown,
	.driver = {
		.name = "sprd_panel_if",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(interface_dt_ids),
#endif
	},
};

static int __init panel_if_init(void)
{
	return platform_driver_register(&panel_if_driver);
}

static void __exit panel_if_exit(void)
{
	return platform_driver_unregister(&panel_if_driver);
}

module_init(panel_if_init);
module_exit(panel_if_exit);
