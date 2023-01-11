/*
 * linux/drivers/video/mmp/hw/debug.c
 * vsync driver support
 *
 * Copyright (C) 2013 Marvell Technology Group Ltd.
 * Authors:  Yu Xu <yuxu@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/device.h>
#include <linux/io.h>
#include <linux/stat.h>
#include "mmp_dsi.h"
#include "mmp_ctrl.h"
#include "mmp_vdma.h"
#include "mmp_apical.h"
#include <video/mipi_display.h>

static struct timer_list vsync_timer;
static DECLARE_COMPLETION(vsync_check_done);
static void vsync_check_timer(unsigned long data)
{
	struct mmp_path *path = (struct mmp_path *)data;

	path->irq_count.vsync_check = 0;
	del_timer(&vsync_timer);
	pr_info("=== irq_count %d ===\n", path->irq_count.irq_count);
	pr_info("=== vsync_count %d ===\n", path->irq_count.vsync_count);
	pr_info("=== dispd_count %d ===\n", path->irq_count.dispd_count);
	pr_info("=== the vsync rate is %d fps===\n",
		(path->irq_count.vsync_count + 5) / 10);
	complete(&vsync_check_done);
}

void vsync_check_count(struct mmp_path *path)
{
	if (path->irq_count.vsync_check) {
		pr_alert("count vsync ongoing, try again after 10s\n");
		return;
	}

	/* clear counts */
	path->irq_count.vsync_count = 0;
	path->irq_count.irq_count = 0;
	path->irq_count.dispd_count = 0;

	/* trigger vsync check */
	path->irq_count.vsync_check = 1;
	/*
	 * if vsync_check =1, irq handler won't call set_irq(path, 0)
	 * so we can count vsync number with irq enable.
	 * Or there will be corner case that irq happen once enable irq,
	 * it will clear irq before vsync_check=1.
	 */
	path->ops.set_irq(path, 1);
	init_timer(&vsync_timer);
	vsync_timer.data = (unsigned long)path;
	vsync_timer.function = vsync_check_timer;
	mod_timer(&vsync_timer, jiffies + 10*HZ);

	pr_info("starting to check, please wait 10s ......\n");
	init_completion(&vsync_check_done);
	while (!wait_for_completion_timeout(&vsync_check_done,
					msecs_to_jiffies(12000)))
		pr_info("%s:still check vsync count\n", __func__);

	/* disable count interrupts */
	path->ops.set_irq(path, 0);
}

ssize_t dsi_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct mmp_dsi *mmp_dsi = dev_get_drvdata(dev);
	struct mmp_dsi_regs *dsi = mmp_dsi->reg_base;
	int s = 0;

	if (unlikely(mmp_dsi->status == MMP_OFF)) {
		pr_warn("WARN: DSI already off, %s fail\n", __func__);
		return s;
	}

	s += sprintf(buf + s, "dsi regs base 0x%p\n", dsi);
	s += sprintf(buf + s, "\tctrl0      (@%p): 0x%x\n",
		&dsi->ctrl0, readl(&dsi->ctrl0));
	s += sprintf(buf + s, "\tctrl1      (@%p): 0x%x\n",
		&dsi->ctrl1, readl(&dsi->ctrl1));
	s += sprintf(buf + s, "\tirq_status (@%p): 0x%x\n",
		&dsi->irq_status, readl(&dsi->irq_status));
	s += sprintf(buf + s, "\tirq_mask   (@%p): 0x%x\n",
		&dsi->irq_mask, readl(&dsi->irq_mask));
	s += sprintf(buf + s, "\tcmd0       (@%p): 0x%x\n",
		&dsi->cmd0, readl(&dsi->cmd0));
	s += sprintf(buf + s, "\tcmd1       (@%p): 0x%x\n",
		&dsi->cmd1, readl(&dsi->cmd1));
	s += sprintf(buf + s, "\tcmd2       (@%p): 0x%x\n",
		&dsi->cmd2, readl(&dsi->cmd2));
	s += sprintf(buf + s, "\tcmd3       (@%p): 0x%x\n",
		&dsi->cmd3, readl(&dsi->cmd3));
	s += sprintf(buf + s, "\tdat0       (@%p): 0x%x\n",
		&dsi->dat0, readl(&dsi->dat0));
	s += sprintf(buf + s, "\tstatus0    (@%p): 0x%x\n",
		&dsi->status0, readl(&dsi->status0));
	s += sprintf(buf + s, "\tstatus1    (@%p): 0x%x\n",
		&dsi->status1, readl(&dsi->status1));
	s += sprintf(buf + s, "\tstatus2    (@%p): 0x%x\n",
		&dsi->status2, readl(&dsi->status2));
	s += sprintf(buf + s, "\tstatus3    (@%p): 0x%x\n",
		&dsi->status3, readl(&dsi->status3));
	s += sprintf(buf + s, "\tstatus4    (@%p): 0x%x\n",
		&dsi->status4, readl(&dsi->status4));
	s += sprintf(buf + s, "\tsmt_cmd    (@%p): 0x%x\n",
		&dsi->smt_cmd, readl(&dsi->smt_cmd));
	s += sprintf(buf + s, "\tsmt_ctrl0  (@%p): 0x%x\n",
		&dsi->smt_ctrl0, readl(&dsi->smt_ctrl0));
	s += sprintf(buf + s, "\tsmt_ctrl1  (@%p): 0x%x\n",
		&dsi->smt_ctrl1, readl(&dsi->smt_ctrl1));
	s += sprintf(buf + s, "\trx0_status (@%p): 0x%x\n",
		&dsi->rx0_status, readl(&dsi->rx0_status));
	s += sprintf(buf + s, "\trx0_header (@%p): 0x%x\n",
		&dsi->rx0_header, readl(&dsi->rx0_header));
	s += sprintf(buf + s, "\trx1_status (@%p): 0x%x\n",
		&dsi->rx1_status, readl(&dsi->rx1_status));
	s += sprintf(buf + s, "\trx1_header (@%p): 0x%x\n",
		&dsi->rx1_header, readl(&dsi->rx1_header));
	s += sprintf(buf + s, "\trx_ctrl    (@%p): 0x%x\n",
		&dsi->rx_ctrl, readl(&dsi->rx_ctrl));
	s += sprintf(buf + s, "\trx_ctrl1   (@%p): 0x%x\n",
		&dsi->rx_ctrl1, readl(&dsi->rx_ctrl1));
	s += sprintf(buf + s, "\trx2_status (@%p): 0x%x\n",
		&dsi->rx2_status, readl(&dsi->rx2_status));
	s += sprintf(buf + s, "\trx2_header (@%p): 0x%x\n",
		&dsi->rx2_header, readl(&dsi->rx2_header));
	if (DISP_GEN4(mmp_dsi->version)) {
		s += sprintf(buf + s, "\nDPHY regs\n");
		s += sprintf(buf + s, "\tphy_ctrl1  (@%p): 0x%x\n",
			&dsi->phy.phy_ctrl1, readl(&dsi->phy.phy_ctrl1));
		s += sprintf(buf + s, "\tphy_ctrl2  (@%p): 0x%x\n",
			&dsi->phy.phy_ctrl2, readl(&dsi->phy.phy_ctrl2));
		s += sprintf(buf + s, "\tphy_ctrl3  (@%p): 0x%x\n",
			&dsi->phy.phy_ctrl3, readl(&dsi->phy.phy_ctrl3));
		s += sprintf(buf + s, "\tphy_status0(@%p): 0x%x\n",
			&dsi->phy.phy_status0, readl(&dsi->phy.phy_status0));
		s += sprintf(buf + s, "\tphy_status1(@%p): 0x%x\n",
			&dsi->phy.phy_status1, readl(&dsi->phy.phy_status1));
		s += sprintf(buf + s, "\tphy_status2(@%p): 0x%x\n",
			&dsi->phy.phy_status2, readl(&dsi->phy.phy_status2));
		s += sprintf(buf + s, "\tphy_rcomp0 (@%p): 0x%x\n",
			&dsi->phy.phy_rcomp0, readl(&dsi->phy.phy_rcomp0));
		s += sprintf(buf + s, "\tphy_timing0(@%p): 0x%x\n",
			&dsi->phy.phy_timing0, readl(&dsi->phy.phy_timing0));
		s += sprintf(buf + s, "\tphy_timing1(@%p): 0x%x\n",
			&dsi->phy.phy_timing1, readl(&dsi->phy.phy_timing1));
		s += sprintf(buf + s, "\tphy_timing2(@%p): 0x%x\n",
			&dsi->phy.phy_timing2, readl(&dsi->phy.phy_timing2));
		s += sprintf(buf + s, "\tphy_timing3(@%p): 0x%x\n",
			&dsi->phy.phy_timing3, readl(&dsi->phy.phy_timing3));
		s += sprintf(buf + s, "\tphy_code_0 (@%p): 0x%x\n",
			&dsi->phy.phy_code_0, readl(&dsi->phy.phy_code_0));
		s += sprintf(buf + s, "\tphy_code_1 (@%p): 0x%x\n",
			&dsi->phy.phy_code_1, readl(&dsi->phy.phy_code_1));
	} else {
		s += sprintf(buf + s, "\tphy_ctrl1  (@%p): 0x%x\n",
			&dsi->phy_ctrl1, readl(&dsi->phy_ctrl1));
		s += sprintf(buf + s, "\tphy_ctrl2  (@%p): 0x%x\n",
			&dsi->phy_ctrl2, readl(&dsi->phy_ctrl2));
		s += sprintf(buf + s, "\tphy_ctrl3  (@%p): 0x%x\n",
			&dsi->phy_ctrl3, readl(&dsi->phy_ctrl3));
		s += sprintf(buf + s, "\tphy_status0(@%p): 0x%x\n",
			&dsi->phy_status0, readl(&dsi->phy_status0));
		s += sprintf(buf + s, "\tphy_status1(@%p): 0x%x\n",
			&dsi->phy_status1, readl(&dsi->phy_status1));
		s += sprintf(buf + s, "\tphy_status2(@%p): 0x%x\n",
			&dsi->phy_status2, readl(&dsi->phy_status2));
		s += sprintf(buf + s, "\tphy_rcomp0 (@%p): 0x%x\n",
			&dsi->phy_rcomp0, readl(&dsi->phy_rcomp0));
		s += sprintf(buf + s, "\tphy_timing0(@%p): 0x%x\n",
			&dsi->phy_timing0, readl(&dsi->phy_timing0));
		s += sprintf(buf + s, "\tphy_timing1(@%p): 0x%x\n",
			&dsi->phy_timing1, readl(&dsi->phy_timing1));
		s += sprintf(buf + s, "\tphy_timing2(@%p): 0x%x\n",
			&dsi->phy_timing2, readl(&dsi->phy_timing2));
		s += sprintf(buf + s, "\tphy_timing3(@%p): 0x%x\n",
			&dsi->phy_timing3, readl(&dsi->phy_timing3));
		s += sprintf(buf + s, "\tphy_code_0 (@%p): 0x%x\n",
			&dsi->phy_code_0, readl(&dsi->phy_code_0));
		s += sprintf(buf + s, "\tphy_code_1 (@%p): 0x%x\n",
			&dsi->phy_code_1, readl(&dsi->phy_code_1));
	}
	s += sprintf(buf + s, "\tmem_ctrl   (@%p): 0x%x\n",
		&dsi->mem_ctrl, readl(&dsi->mem_ctrl));
	s += sprintf(buf + s, "\ttx_timer   (@%p): 0x%x\n",
		&dsi->tx_timer, readl(&dsi->tx_timer));
	s += sprintf(buf + s, "\trx_timer   (@%p): 0x%x\n",
		&dsi->rx_timer, readl(&dsi->rx_timer));
	s += sprintf(buf + s, "\tturn_timer (@%p): 0x%x\n",
		&dsi->turn_timer, readl(&dsi->turn_timer));

	s += sprintf(buf + s, "\nlcd1 regs\n");
	s += sprintf(buf + s, "\tctrl0     (@%p): 0x%x\n",
		&dsi->lcd1.ctrl0, readl(&dsi->lcd1.ctrl0));
	s += sprintf(buf + s, "\tctrl1     (@%p): 0x%x\n",
		&dsi->lcd1.ctrl1, readl(&dsi->lcd1.ctrl1));
	s += sprintf(buf + s, "\ttiming0   (@%p): 0x%x\n",
		&dsi->lcd1.timing0, readl(&dsi->lcd1.timing0));
	s += sprintf(buf + s, "\ttiming1   (@%p): 0x%x\n",
		&dsi->lcd1.timing1, readl(&dsi->lcd1.timing1));
	s += sprintf(buf + s, "\ttiming2   (@%p): 0x%x\n",
		&dsi->lcd1.timing2, readl(&dsi->lcd1.timing2));
	s += sprintf(buf + s, "\ttiming3   (@%p): 0x%x\n",
		&dsi->lcd1.timing3, readl(&dsi->lcd1.timing3));
	s += sprintf(buf + s, "\twc0       (@%p): 0x%x\n",
		&dsi->lcd1.wc0, readl(&dsi->lcd1.wc0));
	s += sprintf(buf + s, "\twc1       (@%p): 0x%x\n",
		&dsi->lcd1.wc1, readl(&dsi->lcd1.wc1));
	s += sprintf(buf + s, "\twc2       (@%p): 0x%x\n",
		&dsi->lcd1.wc2, readl(&dsi->lcd1.wc2));
	s += sprintf(buf + s, "\tslot_cnt0 (@%p): 0x%x\n",
		&dsi->lcd1.slot_cnt0, readl(&dsi->lcd1.slot_cnt0));
	s += sprintf(buf + s, "\tslot_cnt1 (@%p): 0x%x\n",
		&dsi->lcd1.slot_cnt1, readl(&dsi->lcd1.slot_cnt1));
	s += sprintf(buf + s, "\tstatus_0  (@%p): 0x%x\n",
		&dsi->lcd1.status_0, readl(&dsi->lcd1.status_0));
	s += sprintf(buf + s, "\tstatus_1  (@%p): 0x%x\n",
		&dsi->lcd1.status_1, readl(&dsi->lcd1.status_1));
	s += sprintf(buf + s, "\tstatus_2  (@%p): 0x%x\n",
		&dsi->lcd1.status_2, readl(&dsi->lcd1.status_2));
	s += sprintf(buf + s, "\tstatus_3  (@%p): 0x%x\n",
		&dsi->lcd1.status_3, readl(&dsi->lcd1.status_3));
	s += sprintf(buf + s, "\tstatus_4  (@%p): 0x%x\n",
		&dsi->lcd1.status_4, readl(&dsi->lcd1.status_4));

	if (DISP_GEN4(mmp_dsi->version))
		/* For DC4, the active panel lcd2 was removed */
		return s;

	s += sprintf(buf + s, "\nlcd2 regs\n");
	s += sprintf(buf + s, "\tctrl0     (@%p): 0x%x\n",
		&dsi->lcd2.ctrl0, readl(&dsi->lcd2.ctrl0));
	s += sprintf(buf + s, "\tctrl1     (@%p): 0x%x\n",
		&dsi->lcd2.ctrl1, readl(&dsi->lcd2.ctrl1));
	s += sprintf(buf + s, "\ttiming0   (@%p): 0x%x\n",
		&dsi->lcd2.timing0, readl(&dsi->lcd2.timing0));
	s += sprintf(buf + s, "\ttiming1   (@%p): 0x%x\n",
		&dsi->lcd2.timing1, readl(&dsi->lcd2.timing1));
	s += sprintf(buf + s, "\ttiming2   (@%p): 0x%x\n",
		&dsi->lcd2.timing2, readl(&dsi->lcd2.timing2));
	s += sprintf(buf + s, "\ttiming3   (@%p): 0x%x\n",
		&dsi->lcd2.timing3, readl(&dsi->lcd2.timing3));
	s += sprintf(buf + s, "\twc0       (@%p): 0x%x\n",
		&dsi->lcd2.wc0, readl(&dsi->lcd2.wc0));
	s += sprintf(buf + s, "\twc1       (@%p): 0x%x\n",
		&dsi->lcd2.wc1, readl(&dsi->lcd2.wc1));
	s += sprintf(buf + s, "\twc2       (@%p): 0x%x\n",
		&dsi->lcd2.wc2, readl(&dsi->lcd2.wc2));
	s += sprintf(buf + s, "\tslot_cnt0 (@%p): 0x%x\n",
		&dsi->lcd2.slot_cnt0, readl(&dsi->lcd2.slot_cnt0));
	s += sprintf(buf + s, "\tslot_cnt1 (@%p): 0x%x\n",
		&dsi->lcd2.slot_cnt1, readl(&dsi->lcd2.slot_cnt1));
	s += sprintf(buf + s, "\tstatus_0  (@%p): 0x%x\n",
		&dsi->lcd2.status_0, readl(&dsi->lcd2.status_0));
	s += sprintf(buf + s, "\tstatus_1  (@%p): 0x%x\n",
		&dsi->lcd2.status_1, readl(&dsi->lcd2.status_1));
	s += sprintf(buf + s, "\tstatus_2  (@%p): 0x%x\n",
		&dsi->lcd2.status_2, readl(&dsi->lcd2.status_2));
	s += sprintf(buf + s, "\tstatus_3  (@%p): 0x%x\n",
		&dsi->lcd2.status_3, readl(&dsi->lcd2.status_3));
	s += sprintf(buf + s, "\tstatus_4  (@%p): 0x%x\n",
		&dsi->lcd2.status_4, readl(&dsi->lcd2.status_4));
	return s;
}
DEVICE_ATTR(dsi, S_IRUGO, dsi_show, NULL);

static char display_on[] = {0x29};
static char display_off[] = {0x28};

static struct mmp_dsi_cmd_desc display_on_hs_cmds[] = {
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 1000, sizeof(display_off), display_off},
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 0, sizeof(display_on), display_on},
};
static struct mmp_dsi_cmd_desc display_on_lp_cmds[] = {
	{MIPI_DSI_DCS_SHORT_WRITE, 1, 1000, sizeof(display_off), display_off},
	{MIPI_DSI_DCS_SHORT_WRITE, 1, 0, sizeof(display_on), display_on},
};

static char pkt_size_cmd[] = {0x1};
static char read_status_cmd1[] = {0xda};
static char read_status_cmd2[] = {0xdb};
static char read_status_cmd3[] = {0xdc};

static struct mmp_dsi_cmd_desc read_status_hs_cmds1[] = {
	{MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, 0, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{MIPI_DSI_DCS_READ, 0, 0, sizeof(read_status_cmd1), read_status_cmd1},
};

static struct mmp_dsi_cmd_desc read_status_hs_cmds2[] = {
	{MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, 0, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{MIPI_DSI_DCS_READ, 0, 0, sizeof(read_status_cmd2), read_status_cmd2},
};

static struct mmp_dsi_cmd_desc read_status_hs_cmds3[] = {
	{MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, 0, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{MIPI_DSI_DCS_READ, 0, 0, sizeof(read_status_cmd3), read_status_cmd3},
};

static struct mmp_dsi_cmd_desc read_status_lp_cmds1[] = {
	{MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, 1, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{MIPI_DSI_DCS_READ, 1, 0, sizeof(read_status_cmd1), read_status_cmd1},
};

static struct mmp_dsi_cmd_desc read_status_lp_cmds2[] = {
	{MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, 1, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{MIPI_DSI_DCS_READ, 1, 0, sizeof(read_status_cmd2), read_status_cmd2},
};

static struct mmp_dsi_cmd_desc read_status_lp_cmds3[] = {
	{MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, 1, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{MIPI_DSI_DCS_READ, 1, 0, sizeof(read_status_cmd3), read_status_cmd3},
};

static ssize_t dsi_cmd_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct mmp_dsi *mmp_dsi = dev_get_drvdata(dev);
	struct mmp_dsi_buf dbuf;
	struct mmp_dsi_port *dsi_port = &mmp_dsi->dsi_port;
	struct mmp_dsi_regs *dsi = mmp_dsi->reg_base;
	u32 read_status = 0;
	u32 value;

	if (unlikely(mmp_dsi->status == MMP_OFF)) {
		pr_warn("WARN: DSI already off, %s fail\n", __func__);
		return size;
	}

	if ('r' == buf[0] && '1' == buf[1]) {
		/* read command in hs mode */
		dsi_port->rx_cmds(dsi_port, &dbuf, read_status_hs_cmds1,
			ARRAY_SIZE(read_status_hs_cmds1));
		read_status |= dbuf.data[0] << 16;
		dsi_port->rx_cmds(dsi_port, &dbuf, read_status_hs_cmds2,
			ARRAY_SIZE(read_status_hs_cmds2));
		read_status |= dbuf.data[0] << 8;
		dsi_port->rx_cmds(dsi_port, &dbuf, read_status_hs_cmds3,
			ARRAY_SIZE(read_status_hs_cmds3));
		read_status |= dbuf.data[0];
		pr_info("##################\n");
		pr_info("### panel id is : 0x%x ###\n", read_status);
		pr_info("##################\n");
		return size;
	} else if ('r' == buf[0] && '0' == buf[1]) {
		/* read command in lp mode */
		value = readl_relaxed(&dsi->ctrl0);
		value &= ~(DSI_CTRL_0_CFG_LCD1_TX_EN
			|DSI_CTRL_0_CFG_LCD1_EN);
		writel_relaxed(value, &dsi->ctrl0);
		dsi_port->rx_cmds(dsi_port, &dbuf, read_status_lp_cmds1,
			ARRAY_SIZE(read_status_lp_cmds1));
		read_status |= dbuf.data[0] << 16;
		dsi_port->rx_cmds(dsi_port, &dbuf, read_status_lp_cmds2,
			ARRAY_SIZE(read_status_lp_cmds2));
		read_status |= dbuf.data[0] << 8;
		dsi_port->rx_cmds(dsi_port, &dbuf, read_status_lp_cmds3,
			ARRAY_SIZE(read_status_lp_cmds3));
		read_status |= dbuf.data[0];
		value |= (DSI_CTRL_0_CFG_LCD1_TX_EN
			|DSI_CTRL_0_CFG_LCD1_EN);
		writel_relaxed(value, &dsi->ctrl0);
		value = readl_relaxed(&dsi->phy_ctrl2) & ~(0xf << 4);
		value |= (DSI_LANES(mmp_dsi->setting.lanes) << 4);
		writel_relaxed(value, &dsi->phy_ctrl2);
		pr_info("##################\n");
		pr_info("### panel id is : 0x%x ###\n", read_status);
		pr_info("##################\n");
		return size;
	} else if ('r' == buf[0] && '2' == buf[1]) {
		/* read command in lp mode */
		dsi_port->rx_cmds(dsi_port, &dbuf, read_status_lp_cmds1,
			ARRAY_SIZE(read_status_lp_cmds1));
		read_status |= dbuf.data[0] << 16;
		dsi_port->rx_cmds(dsi_port, &dbuf, read_status_lp_cmds2,
			ARRAY_SIZE(read_status_lp_cmds2));
		read_status |= dbuf.data[0] << 8;
		dsi_port->rx_cmds(dsi_port, &dbuf, read_status_lp_cmds3,
			ARRAY_SIZE(read_status_lp_cmds3));
		read_status |= dbuf.data[0];
		pr_info("##################\n");
		pr_info("### panel id is : 0x%x ###\n", read_status);
		pr_info("##################\n");
		return size;
	} else if ('w' == buf[0] && '1' == buf[1]) {
		/* read command in hs mode */
		dsi_port->tx_cmds(dsi_port, display_on_hs_cmds,
			ARRAY_SIZE(display_on_hs_cmds));
		return size;
	} else if ('w' == buf[0] && '2' == buf[1]) {
		/* read command in hs mode */
		dsi_port->tx_cmds(dsi_port, display_on_lp_cmds,
			ARRAY_SIZE(display_on_lp_cmds));
		return size;
	} else if ('w' == buf[0] && '0' == buf[1]) {
		/* read command in lp mode */
		value = readl_relaxed(&dsi->ctrl0);
		value &= ~(DSI_CTRL_0_CFG_LCD1_TX_EN
			|DSI_CTRL_0_CFG_LCD1_EN);
		writel_relaxed(value, &dsi->ctrl0);
		dsi_port->tx_cmds(dsi_port, display_on_lp_cmds,
			ARRAY_SIZE(display_on_lp_cmds));
		value |= (DSI_CTRL_0_CFG_LCD1_TX_EN
			|DSI_CTRL_0_CFG_LCD1_EN);
		writel_relaxed(value, &dsi->ctrl0);
		value = readl_relaxed(&dsi->phy_ctrl2) & ~(0xf << 4);
		value |= (DSI_LANES(mmp_dsi->setting.lanes) << 4);
		writel_relaxed(value, &dsi->phy_ctrl2);
		return size;
	} else {
		pr_info("help:\n");
		pr_info("'echo r0 > cmd' to test read command in LP mode,if can read panel id successful, then ok\n");
		pr_info("'echo r1 > cmd' to test read command in HS mode,if can read panel id successful, then ok\n");
		pr_info("'echo r2 > cmd' to test read command in LP mode,if can read panel id successful, then ok\n");
		pr_info("'echo w0 > cmd' to test write command in LP mode,if no any errors prink out, then ok\n");
		pr_info("'echo w1 > cmd' to test write command in HS mode,if no any errors print out, and display off 1 minutes, then ok\n");
		pr_info("'echo w2 > cmd' to test write command in LP mode,if no any errors print out, and display off 1 minutes, then ok\n");
	}

	return size;
}
DEVICE_ATTR(cmd, S_IWUSR, NULL, dsi_cmd_store);

int dsi_dbg_init(struct device *dev)
{
	int ret;

	ret = device_create_file(dev, &dev_attr_dsi);
	if (ret < 0)
		goto err_dsireg;
	ret = device_create_file(dev, &dev_attr_cmd);
	if (ret < 0)
		goto err_dsicmd;

	return 0;

err_dsicmd:
	device_remove_file(dev, &dev_attr_dsi);
err_dsireg:
	return ret;

}

void dsi_dbg_uninit(struct device *dev)
{
	device_remove_file(dev, &dev_attr_cmd);
	device_remove_file(dev, &dev_attr_dsi);
}

ssize_t lcd_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct mmphw_ctrl *ctrl = dev_get_drvdata(dev);
	struct lcd_regs *regs;
	struct mmp_path *path;
	int s = 0, i;

	if (unlikely(ctrl->status == MMP_OFF)) {
		pr_warn("WARN: LCDC already off, %s fail\n", __func__);
		return s;
	}

	s += sprintf(buf + s, "lcdc: %s, path_num:%d\n",
				ctrl->name, ctrl->path_num);
	s += sprintf(buf + s, "\nLCD controller version:%d\n", ctrl->version);

	for (i = 0; i < ctrl->path_num; i++) {
		path = ctrl->path_plats[i].path;
		regs = path_regs(path);

		s += sprintf(buf + s, "path: %d\n", path->id);
		s += sprintf(buf + s, "  video layer\n");
		s += sprintf(buf + s, "\tv_y0        (@%p) 0x%x\n",
			&regs->v_y0, readl_relaxed(&regs->v_y0));
		s += sprintf(buf + s, "\tv_u0        (@%p) 0x%x\n",
			&regs->v_u0, readl_relaxed(&regs->v_u0));
		s += sprintf(buf + s, "\tv_v0        (@%p) 0x%x\n",
			&regs->v_v0, readl_relaxed(&regs->v_v0));
		s += sprintf(buf + s, "\tv_c0        (@%p) 0x%x\n",
			&regs->v_c0, readl_relaxed(&regs->v_c0));
		s += sprintf(buf + s, "\tv_y1        (@%p) 0x%x\n",
			&regs->v_y1, readl_relaxed(&regs->v_y1));
		s += sprintf(buf + s, "\tv_u1        (@%p) 0x%x\n",
			&regs->v_u1, readl_relaxed(&regs->v_u1));
		s += sprintf(buf + s, "\tv_v1        (@%p) 0x%x\n",
			&regs->v_v1, readl_relaxed(&regs->v_v1));
		s += sprintf(buf + s, "\tv_c1        (@%p) 0x%x\n",
			&regs->v_c1, readl_relaxed(&regs->v_c1));
		s += sprintf(buf + s, "\tv_pitch_yc  (@%p) 0x%x\n",
			&regs->v_pitch_yc, readl_relaxed(&regs->v_pitch_yc));
		s += sprintf(buf + s, "\tv_pitch_uv  (@%p) 0x%x\n",
			&regs->v_pitch_uv, readl_relaxed(&regs->v_pitch_uv));
		s += sprintf(buf + s, "\tv_start     (@%p) 0x%x\n",
			&regs->v_start,	readl_relaxed(&regs->v_start));
		s += sprintf(buf + s, "\tv_size      (@%p) 0x%x\n",
			&regs->v_size, readl_relaxed(&regs->v_size));
		s += sprintf(buf + s, "\tv_size_z    (@%p) 0x%x\n",
			&regs->v_size_z, readl_relaxed(&regs->v_size_z));

		s += sprintf(buf + s, "  graphic layer\n");
		s += sprintf(buf + s, "\tg_0         (@%p) 0x%x\n",
			&regs->g_0, readl_relaxed(&regs->g_0));
		s += sprintf(buf + s, "\tg_1         (@%p) 0x%x\n",
			&regs->g_1, readl_relaxed(&regs->g_1));
		s += sprintf(buf + s, "\tg_pitch     (@%p) 0x%x\n",
			&regs->g_pitch, readl_relaxed(&regs->g_pitch));
		s += sprintf(buf + s, "\tg_start     (@%p) 0x%x\n",
			&regs->g_start, readl_relaxed(&regs->g_start));
		s += sprintf(buf + s, "\tg_size      (@%p) 0x%x\n",
			&regs->g_size, readl_relaxed(&regs->g_size));
		s += sprintf(buf + s, "\tg_size_z    (@%p) 0x%x\n",
			&regs->g_size_z, readl_relaxed(&regs->g_size_z));

		s += sprintf(buf + s, "  hardware cursor\n");
		s += sprintf(buf + s, "\thc_start    (@%p) 0x%x\n",
			&regs->hc_start, readl_relaxed(&regs->hc_start));
		s += sprintf(buf + s, "\thc_size     (@%p) 0x%x\n",
			&regs->hc_size, readl_relaxed(&regs->hc_size));

		s += sprintf(buf + s, "  screen\n");
		s += sprintf(buf + s, "\tscreen_size     (@%p) 0x%x\n",
			&regs->screen_size, readl_relaxed(&regs->screen_size));
		s += sprintf(buf + s, "\tscreen_active   (@%p) 0x%x\n",
			&regs->screen_active,
			readl_relaxed(&regs->screen_active));
		s += sprintf(buf + s, "\tscreen_h_porch  (@%p) 0x%x\n",
			&regs->screen_h_porch,
			readl_relaxed(&regs->screen_h_porch));
		s += sprintf(buf + s, "\tscreen_v_porch  (@%p) 0x%x\n",
			&regs->screen_v_porch,
			readl_relaxed(&regs->screen_v_porch));

		s += sprintf(buf + s, "  color\n");
		s += sprintf(buf + s, "\tblank_color     (@%p) 0x%x\n",
			 &regs->blank_color, readl_relaxed(&regs->blank_color));
		s += sprintf(buf + s, "\thc_Alpha_color1 (@%p) 0x%x\n",
			 &regs->hc_Alpha_color1,
			 readl_relaxed(&regs->hc_Alpha_color1));
		s += sprintf(buf + s, "\thc_Alpha_color2 (@%p) 0x%x\n",
			 &regs->hc_Alpha_color2,
			 readl_relaxed(&regs->hc_Alpha_color2));
		s += sprintf(buf + s, "\tv_colorkey_y    (@%p) 0x%x\n",
			 &regs->v_colorkey_y,
			 readl_relaxed(&regs->v_colorkey_y));
		s += sprintf(buf + s, "\tv_colorkey_u    (@%p) 0x%x\n",
			 &regs->v_colorkey_u,
			 readl_relaxed(&regs->v_colorkey_u));
		s += sprintf(buf + s, "\tv_colorkey_v    (@%p) 0x%x\n",
			 &regs->v_colorkey_v,
			 readl_relaxed(&regs->v_colorkey_v));

		s += sprintf(buf + s, "  control\n");
		s += sprintf(buf + s, "\tvsync_ctrl      (@%p) 0x%x\n",
			 &regs->vsync_ctrl, readl_relaxed(&regs->vsync_ctrl));
		s += sprintf(buf + s, "\tdma_ctrl0       (@%8x) 0x%x\n",
			 dma_ctrl(0, path->id),
			 readl_relaxed(ctrl->reg_base + dma_ctrl0(path->id)));
		s += sprintf(buf + s, "\tdma_ctrl1       (@%8x) 0x%x\n",
			 dma_ctrl(1, path->id),
			 readl_relaxed(ctrl->reg_base + dma_ctrl1(path->id)));
		s += sprintf(buf + s, "\tintf_ctrl       (@%8x) 0x%x\n",
			 intf_ctrl(path->id),
			 readl_relaxed(ctrl->reg_base + intf_ctrl(path->id)));
		s += sprintf(buf + s, "\tirq_enable      (@%8x) 0x%8x\n",
			 SPU_IRQ_ENA,
			 readl_relaxed(ctrl->reg_base + SPU_IRQ_ENA));
		s += sprintf(buf + s, "\tirq_status      (@%8x) 0x%8x\n",
			 SPU_IRQ_ISR,
			 readl_relaxed(ctrl->reg_base + SPU_IRQ_ISR));
		s += sprintf(buf + s, "\tclk_sclk        (@%8x) 0x%x\n",
			 LCD_SCLK_DIV,
			 readl_relaxed(ctrl->reg_base + LCD_SCLK_DIV));
		s += sprintf(buf + s, "\tclk_tclk        (@%8x) 0x%x\n",
			 LCD_TCLK_DIV,
			 readl_relaxed(ctrl->reg_base + LCD_TCLK_DIV));
	}

	return s;
}

static ssize_t lcd_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct mmphw_ctrl *ctrl = dev_get_drvdata(dev);
	struct mmp_path *path;
	struct lcd_regs *lcd_reg;
	static u32 mmphw_reg;
	int ret, i;
	unsigned long tmp;
	u32 regs;

	if (unlikely(ctrl->status == MMP_OFF)) {
		pr_warn("WARN: LCDC already off, %s fail\n", __func__);
		return size;
	}

	if (size > 30) {
		pr_err("%s size = %zd > max 30 chars\n", __func__, size);
		return size;
	}

	if ('-' == buf[0]) {
		ret = kstrtoul(buf + 1, 0, &tmp);
		if (ret < 0) {
			dev_err(dev, "strtoul err.\n");
			return ret;
		}
		mmphw_reg = (u32)tmp;
		pr_info("reg @ 0x%x: 0x%x\n", mmphw_reg,
			readl_relaxed(ctrl->reg_base + mmphw_reg));
		return size;
	} else if ('0' == buf[0] && 'x' == buf[1]) {
		/* set the register value */
		ret = kstrtoul(buf, 0, &tmp);
		if (ret < 0) {
			dev_err(dev, "strtoul err.\n");
			return ret;
		}
		writel_relaxed((u32)tmp, ctrl->reg_base + mmphw_reg);
		pr_info("set reg @ 0x%x: 0x%x\n", mmphw_reg,
			readl_relaxed(ctrl->reg_base + mmphw_reg));
		return size;
	} else if ('l' == buf[0]) {
		pr_info("\ndisplay controller regs\n");
		for (i = 0; i < 0x300; i += 4) {
			if (!(i % 16))
				pr_info("\n0x%3x: ", i);
			pr_info(" %8x", readl_relaxed(ctrl->reg_base + i));
		}
		pr_info("\n");
		return size;
	} else if ('v' == buf[0]) {
		ret = kstrtoul(buf + 1, 0, &tmp);
		if (ret < 0) {
			dev_err(dev, "strtoul err.\n");
			return ret;
		}
		if (tmp > ctrl->path_num - 1)
			tmp = 0;
		path = ctrl->path_plats[tmp].path;
		vsync_check_count(path);
		return size;
	} else if ('t' == buf[0]) {
		ret = kstrtoul(buf + 1, 0, &tmp);
		if (ret < 0) {
			dev_err(dev, "strtoul err.\n");
			return ret;
		}

		/* test mode for pn path */
		path = ctrl->path_plats[0].path;
		lcd_reg = path_regs(path);
		regs = readl_relaxed(ctrl->reg_base + dma_ctrl(0, 0));
		regs &= ~(CFG_DMA_TSTMODE_MASK | CFG_GRA_TSTMODE_MASK |
				CFG_DMA_ENA_MASK);
		if (tmp == 1) {
			/* graphic layer test mode */
			regs |= CFG_GRA_TSTMODE_MASK | CFG_GRA_ENA_MASK;
			writel_relaxed(readl_relaxed(&lcd_reg->screen_active),
					&lcd_reg->g_size_z);
			pr_info("pn graphic layer test mode!\n");
		} else if (tmp == 2) {
			/* video layer test mode */
			regs |= CFG_DMA_TSTMODE_MASK | CFG_DMA_ENA_MASK;
			writel_relaxed(readl_relaxed(&lcd_reg->screen_active),
					&lcd_reg->v_size_z);
			pr_info("pn video layer test mode!\n");
		}

		writel_relaxed(regs, ctrl->reg_base + dma_ctrl(0, 0));

		return size;
	}

	return size;
}
DEVICE_ATTR(lcd, S_IRUGO | S_IWUSR, lcd_show, lcd_store);

int ctrl_dbg_init(struct device *dev)
{
	return device_create_file(dev, &dev_attr_lcd);
}

ssize_t mmp_vdma_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct mmp_vdma *vdma = dev_get_drvdata(dev);
	struct mmp_vdma_info *vdma_info;
	struct mmp_vdma_reg *vdma_reg;
	struct mmp_vdma_ch_reg *ch_reg;
	int i, path_id, index, s = 0, channel_num = 8;
	u32 tmp;
	void *lcd_reg = vdma->lcd_reg_base;

	if (unlikely(vdma->status == MMP_OFF)) {
		pr_warn("WARN: VDMA already off, %s fail\n", __func__);
		return s;
	}

	for (i = 0; i < vdma->vdma_channel_num; i++) {
		vdma_info = &vdma->vdma_info[i];
		s += sprintf(buf + s, "vdma%d:\n", i);
		s += sprintf(buf + s, "\tvdma->overlay_id:             %d\n",
			vdma_info->overlay_id);
		s += sprintf(buf + s,
			"\tvdma->status(0-free;1-requesed;2-allocated;"
			"3-released): %d\n", vdma_info->status);
		s += sprintf(buf + s, "\tvdma->sram_paddr:           0x%x\n",
			vdma_info->sram_paddr);
		s += sprintf(buf + s, "\tvdma->sram_vaddr:           0x%lx\n",
			vdma_info->sram_vaddr);
		s += sprintf(buf + s, "\tvdma->sram_size:            %zd\n",
			vdma_info->sram_size);
	}

	s += sprintf(buf + s, "\nLCD controller version:%d\n", vdma->version);
	s += sprintf(buf + s, "\nLCD register base: 0x%p\n", lcd_reg);
	s += sprintf(buf + s, "\tLCD_PN2_SQULN2_CTRL: 0x%x\n",
		readl_relaxed(lcd_reg + LCD_PN2_SQULN2_CTRL));
	if (!DISP_GEN4(vdma->version)) {
		for (path_id = 0; path_id < 3; path_id++) {
			for (index = 0; index < 4; index++) {
				/* select SQU */
				squ_switch_index(path_id << 1, index);
				/* read SQU control */
				s += sprintf(buf + s,
					"\tpath:%d, index:%d, squ ctl:0x%x\n",
					path_id, index, readl_relaxed(lcd_reg +
					SQULN_CTRL(path_id << 1)));
			}
		}
	} else {
		s += sprintf(buf + s, "\tLCD_PN_SQULN_CTRL: 0x%x\n",
			readl_relaxed(lcd_reg + LCD_PN_SQULN_CTRL));
		s += sprintf(buf + s, "\tLCD_TV_SQULN_CTRL: 0x%x\n",
			readl_relaxed(lcd_reg + LCD_TV_SQULN_CTRL));
	}

	vdma_reg = (struct mmp_vdma_reg *)(vdma->reg_base);
	s += sprintf(buf + s, "\nVDMA register base: 0x%p\n", vdma->reg_base);
	s += sprintf(buf + s, "\tirqr_0to3    (@%p): 0x%x\n",
		&vdma_reg->irqr_0to3, readl_relaxed(&vdma_reg->irqr_0to3));
	s += sprintf(buf + s, "\tirqr_4to7    (@%p): 0x%x\n",
		&vdma_reg->irqr_4to7, readl_relaxed(&vdma_reg->irqr_4to7));
	s += sprintf(buf + s, "\tirqm_0to3    (@%p): 0x%x\n",
		&vdma_reg->irqm_0to3, readl_relaxed(&vdma_reg->irqm_0to3));
	s += sprintf(buf + s, "\tirqm_4to7    (@%p): 0x%x\n",
		&vdma_reg->irqm_4to7, readl_relaxed(&vdma_reg->irqm_4to7));
	s += sprintf(buf + s, "\tirqs_0to3    (@%p): 0x%x\n",
		&vdma_reg->irqs_0to3, readl_relaxed(&vdma_reg->irqs_0to3));
	s += sprintf(buf + s, "\tirqs_4to7    (@%p): 0x%x\n",
		&vdma_reg->irqs_4to7, readl_relaxed(&vdma_reg->irqs_4to7));

	s += sprintf(buf + s, "\tmain_ctrl    (@%p): 0x%x\n",
		&vdma_reg->main_ctrl, readl_relaxed(&vdma_reg->main_ctrl));

	if (DISP_GEN4_LITE(vdma->version))
		channel_num = 1;
	for (i = 0; i < channel_num; i++) {
		ch_reg = &vdma_reg->ch_reg[i];
		s += sprintf(buf + s, "\tVDMA sub-channel:%d\n", i);
		s += sprintf(buf + s, "\tdc_saddr     (@%p): 0x%x\n",
			&ch_reg->dc_saddr, readl_relaxed(&ch_reg->dc_saddr));
		s += sprintf(buf + s, "\tdc_saddr     (@%p): 0x%x\n",
			&ch_reg->dc_size, readl_relaxed(&ch_reg->dc_size));
		s += sprintf(buf + s, "\tctrl         (@%p): 0x%x\n",
			&ch_reg->ctrl, readl_relaxed(&ch_reg->ctrl));
		s += sprintf(buf + s, "\tsrc_size     (@%p): 0x%x\n",
			&ch_reg->src_size, readl_relaxed(&ch_reg->src_size));
		s += sprintf(buf + s, "\tsrc_addr     (@%p): 0x%x\n",
			&ch_reg->src_addr, readl_relaxed(&ch_reg->src_addr));
		s += sprintf(buf + s, "\tdst_addr     (@%p): 0x%x\n",
			&ch_reg->dst_addr, readl_relaxed(&ch_reg->dst_addr));
		s += sprintf(buf + s, "\tdst_size     (@%p): 0x%x\n",
			&ch_reg->dst_size, readl_relaxed(&ch_reg->dst_size));
		s += sprintf(buf + s, "\tpitch        (@%p): 0x%x\n",
			&ch_reg->pitch, readl_relaxed(&ch_reg->pitch));
		s += sprintf(buf + s, "\tram_ctrl0    (@%p): 0x%x\n",
			&ch_reg->ram_ctrl0, readl_relaxed(&ch_reg->ram_ctrl0));
	}

	s += sprintf(buf + s, "commands:\n");
	s += sprintf(buf + s, " - dump VDMA configuration, registers\n");
	s += sprintf(buf + s, "\tcat mmp_vdma\n");
	s += sprintf(buf + s, " - vdma setting\n"
			"\ts [0/1/2/3/4/5](overlay_id) [1/0](allocat/free) <[sram_size](Byte)>\n");

	return s;
}

static u32 mmp_vdma_reg;
ssize_t mmp_vdma_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct mmp_vdma *vdma = dev_get_drvdata(dev);
	u32 overlay_id, enable, sram_size = 0;
	char vol[10];
	int ret;
	unsigned long tmp;

	if (unlikely(vdma->status == MMP_OFF)) {
		pr_warn("WARN: VDMA already off, %s fail\n", __func__);
		return size;
	}

	if ('s' == buf[0]) {
		memcpy(vol, (void *)(buf + 1), size - 1);
		if (sscanf(vol, "%u %u %u", &overlay_id, &enable, &sram_size)
				!= 3)
			if (sscanf(vol, "%u %u", &overlay_id, &enable) != 2) {
				dev_err(vdma->dev,
					"vdma setting cmd should be like:\n"
					"\ts [0/1/2/3/4/5](overlay_id) "
					"[1/0](allocat/free) "
					"<[sram_size](Byte)>\n");
				return size;
			}
		if (DISP_GEN4(vdma->version)) {
			pr_info("controller version 4 not support "
					"enable/disable dynamically!!\n");
			return size;
		}
		if (enable)
			mmp_vdma_alloc(overlay_id, sram_size);
		else
			mmp_vdma_free(overlay_id);
	} else if ('-' == buf[0]) {
		ret = kstrtoul(buf + 1, 0, &tmp);
		if (ret < 0) {
			dev_err(dev, "strtoul err.\n");
			return ret;
		}
		mmp_vdma_reg = (u32)tmp;

		pr_info("reg @ 0x%x: 0x%x\n", mmp_vdma_reg,
			readl_relaxed(vdma->reg_base + mmp_vdma_reg));
	} else if ('0' == buf[0] && 'x' == buf[1]) {
		/* set the register value */
		ret = kstrtoul(buf, 0, &tmp);
		if (ret < 0) {
			dev_err(dev, "strtoul err.\n");
			return ret;
		}
		writel_relaxed((u32)tmp, vdma->reg_base + mmp_vdma_reg);
		pr_info("set reg @ 0x%x: 0x%x\n", mmp_vdma_reg,
			readl_relaxed(vdma->reg_base + mmp_vdma_reg));
	}

	return size;
}

static DEVICE_ATTR(vdma, S_IRUGO | S_IWUSR, mmp_vdma_show,
	mmp_vdma_store);

int vdma_dbg_init(struct device *dev)
{
	return device_create_file(dev, &dev_attr_vdma);
}

void vdma_dbg_uninit(struct device *dev)
{
	device_remove_file(dev, &dev_attr_vdma);
}

ssize_t mmp_apical_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct mmp_apical *apical = dev_get_drvdata(dev);
	struct mmp_apical_reg *apical_reg;
	int i, s = 0, status = 0;
	void *lcd_reg = apical->lcd_reg_base;

	if (!APICAL_GEN4) {
		s += sprintf(buf + s, "\napical not support\n");
		return s;
	}

	for (i = 0; i < apical->apical_channel_num; i++) {
		if (apical->apical_info[i].status != MMP_OFF) {
			status = 1;
			break;
		}
	}

	if (unlikely(!status)) {
		pr_warn("WARN: APICAL already off, %s fail\n", __func__);
		return s;
	}

	s += sprintf(buf + s, "\nLCD controller version:%d\n", apical->version);
	s += sprintf(buf + s, "\nLCD register base: 0x%p\n", lcd_reg);
	s += sprintf(buf + s, "\tLCD_PN_CTRL2: 0x%x\n",
		readl_relaxed(lcd_reg + dma_ctrl2(0)));
	s += sprintf(buf + s, "\tLCD_TV_CTRL2: 0x%x\n",
		readl_relaxed(lcd_reg + dma_ctrl2(1)));

	s += sprintf(buf + s, "\napical register base: 0x%p\n",
		apical->reg_base);
	for (i = 0; i < 2; i++) {
		apical_reg = (struct mmp_apical_reg *)(apical->reg_base +
				i * 0x200);
		s += sprintf(buf + s, "\t------apical%d------\n", i);
		s += sprintf(buf + s, "\tfmt_ctrl       (@%p):\t0x%x\n",
			&apical_reg->fmt_ctrl,
			readl_relaxed(&apical_reg->fmt_ctrl));
		s += sprintf(buf + s, "\tfrm_size       (@%p):\t0x%x\n",
			&apical_reg->frm_size,
			readl_relaxed(&apical_reg->frm_size));
		s += sprintf(buf + s, "\tiridix_ctrl1   (@%p):\t0x%x\n",
			&apical_reg->iridix_ctrl1,
			readl_relaxed(&apical_reg->iridix_ctrl1));
		s += sprintf(buf + s, "\tiridix_ctrl2   (@%p):\t0x%x\n",
			&apical_reg->iridix_ctrl2,
			readl_relaxed(&apical_reg->iridix_ctrl2));
		s += sprintf(buf + s, "\tlevel_ctrl     (@%p):\t0x%x\n",
			&apical_reg->level_ctrl,
			readl_relaxed(&apical_reg->level_ctrl));
		s += sprintf(buf + s, "\top_ctrl        (@%p):\t0x%x\n",
			&apical_reg->op_ctrl,
			readl_relaxed(&apical_reg->op_ctrl));
		s += sprintf(buf + s, "\tstrength_filt  (@%p):\t0x%x\n",
			&apical_reg->strength_filt,
			readl_relaxed(&apical_reg->strength_filt));
		s += sprintf(buf + s, "\tctrl_in1       (@%p):\t0x%x\n",
			&apical_reg->ctrl_in1,
			readl_relaxed(&apical_reg->ctrl_in1));
		s += sprintf(buf + s, "\tctrl_in2       (@%p):\t0x%x\n",
			&apical_reg->ctrl_in2,
			readl_relaxed(&apical_reg->ctrl_in2));
		s += sprintf(buf + s, "\tcalibrat1      (@%p):\t0x%x\n",
			&apical_reg->calibrat1,
			readl_relaxed(&apical_reg->calibrat1));
		s += sprintf(buf + s, "\tcalibrat2      (@%p):\t0x%x\n",
			&apical_reg->calibrat2,
			readl_relaxed(&apical_reg->calibrat2));
		s += sprintf(buf + s, "\tbl_range       (@%p):\t0x%x\n",
			&apical_reg->bl_range,
			readl_relaxed(&apical_reg->bl_range));
		s += sprintf(buf + s, "\tbl_scale       (@%p):\t0x%x\n",
			&apical_reg->bl_scale,
			readl_relaxed(&apical_reg->bl_scale));
		s += sprintf(buf + s, "\tiridix_config  (@%p):\t0x%x\n",
			&apical_reg->iridix_config,
			readl_relaxed(&apical_reg->iridix_config));
		s += sprintf(buf + s, "\tctrl_out       (@%p):\t0x%x\n",
			&apical_reg->ctrl_out,
			readl_relaxed(&apical_reg->ctrl_out));
		s += sprintf(buf + s, "\tlut_index      (@%p):\t0x%x\n",
			&apical_reg->lut_index,
			readl_relaxed(&apical_reg->lut_index));
		s += sprintf(buf + s, "\tasymmetry_lut  (@%p):\t0x%x\n",
			&apical_reg->asymmetry_lut,
			readl_relaxed(&apical_reg->asymmetry_lut));
		s += sprintf(buf + s, "\tcr_correct_lut (@%p):\t0x%x\n",
			&apical_reg->color_correct_lut,
			readl_relaxed(&apical_reg->color_correct_lut));
		s += sprintf(buf + s, "\tcalibrat_lut   (@%p):\t0x%x\n",
			&apical_reg->calibrat_lut,
			readl_relaxed(&apical_reg->calibrat_lut));
		s += sprintf(buf + s, "\tstrength_out   (@%p):\t0x%x\n",
			&apical_reg->strength_out,
			readl_relaxed(&apical_reg->strength_out));
	}

	return s;
}

static u32 apical_reg_addr;
ssize_t mmp_apical_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct mmp_apical *apical = dev_get_drvdata(dev);
	int i, ret, status = 0;
	unsigned long tmp;

	for (i = 0; i < apical->apical_channel_num; i++) {
		if (apical->apical_info[i].status != MMP_OFF) {
			status = 1;
			break;
		}
	}

	if (unlikely(!status)) {
		pr_warn("WARN: APICAL already off, %s fail\n", __func__);
		return size;
	}

	if ('-' == buf[0]) {
		ret = kstrtoul(buf + 1, 0, &tmp);
		if (ret < 0) {
			dev_err(dev, "strtoul err.\n");
			return ret;
		}
		apical_reg_addr = (u32)tmp;
		pr_info("reg @ 0x%x: 0x%x\n", apical_reg_addr,
			readl_relaxed(apical->reg_base + apical_reg_addr));
	} else if ('0' == buf[0] && 'x' == buf[1]) {
		/* set the register value */
		ret = kstrtoul(buf, 0, &tmp);
		if (ret < 0) {
			dev_err(dev, "strtoul err.\n");
			return ret;
		}
		writel_relaxed((u32)tmp, apical->reg_base + apical_reg_addr);
		pr_info("set reg @ 0x%x: 0x%x\n", apical_reg_addr,
			readl_relaxed(apical->reg_base + apical_reg_addr));
	}

	return size;
}

static DEVICE_ATTR(apical, S_IRUGO | S_IWUSR, mmp_apical_show,
	mmp_apical_store);

int apical_dbg_init(struct device *dev)
{
	return device_create_file(dev, &dev_attr_apical);
}

void apical_dbg_uninit(struct device *dev)
{
	device_remove_file(dev, &dev_attr_apical);
}
