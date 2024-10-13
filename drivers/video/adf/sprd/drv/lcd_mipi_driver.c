
#include <sprd_display.h>
#include <sprd_interface.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/delay.h>


static int32_t lcd_mipi_init(struct panel_if_device *intf)
{
	struct panel_spec *panel = intf->get_panel(intf);
	struct sprd_dsi_context *dsi_ctx = intf->info.mipi->dsi_ctx;
	struct panel_cmds *on_cmds;
	int i = 0;
	mipi_set_cmd_mode_t mipi_set_cmd_mode;
	mipi_dcs_write_t mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set;

	mipi_set_cmd_mode = intf->info.mipi->ops->mipi_set_cmd_mode;
	mipi_dcs_write = intf->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set = intf->info.mipi->ops->mipi_eotp_set;
	mipi_set_cmd_mode(dsi_ctx);
	mipi_eotp_set(dsi_ctx, 0, 0);
	on_cmds = panel->lcd_cmds;
	for (i = 0; i < on_cmds->cmd_total; i++) {
		mipi_dcs_write(dsi_ctx, on_cmds->cmds[i].payload,
		    on_cmds->cmds[i].header.len);
		if (on_cmds->cmds[i].header.wait)
			msleep(on_cmds->cmds[i].header.wait);
	}

	mipi_eotp_set(dsi_ctx, 0, 0);
	return 0;
}
static int32_t lcd_mipi_esd_check(struct panel_if_device *intf)
{
	struct panel_spec *panel = intf->get_panel(intf);
	struct sprd_dsi_context *dsi_ctx = intf->info.mipi->dsi_ctx;
	struct panel_cmds *codes;

	int i = 0;
	uint8_t read_data[3] = {0};
	mipi_set_cmd_mode_t mipi_set_cmd_mode;
	mipi_force_write_t mipi_force_write;
	mipi_force_read_t mipi_force_read;
	mipi_eotp_set_t mipi_eotp_set;

	mipi_set_cmd_mode = intf->info.mipi->ops->mipi_set_cmd_mode;
	mipi_force_write = intf->info.mipi->ops->mipi_force_write;
	mipi_force_read = intf->info.mipi->ops->mipi_force_read;
	mipi_eotp_set = intf->info.mipi->ops->mipi_eotp_set;

	codes = panel->esd_cmds;
	if (!codes) {
		PERROR("esd_return_code is NULL\n");
		return -1;
	}

	mipi_eotp_set(dsi_ctx, 0, 0);
	for (i = 0; i < codes->cmd_total; i++) {
		mipi_force_write(dsi_ctx, codes->cmds[i].header.data_type,
			codes->cmds[i].payload, codes->cmds[i].header.len);
		if (codes->cmds[i].header.wait)
			msleep(codes->cmds[i].header.wait);
	}
	mipi_force_read(dsi_ctx, 0x0A, 1, (uint8_t *)read_data);
	mipi_eotp_set(dsi_ctx, 0, 0);

	if (read_data[0] == (panel->esd_return_code & 0xff))
		return 0;
	else{
		PERROR("error code <0x%x>\n", read_data[0]);
		return -ENODATA;
	}
	return 0;
}
static uint32_t lcd_mipi_readid(struct panel_if_device *intf)
{
	uint32_t lcd_return_id = 0x8394;

	return lcd_return_id;
}

static int32_t lcd_mipi_enter_sleep(struct panel_if_device *intf,
	uint8_t is_sleep)
{
	struct panel_spec *panel = intf->get_panel(intf);
	struct sprd_dsi_context *dsi_ctx = intf->info.mipi->dsi_ctx;

	struct panel_cmds *off_cmds;
	int i = 0;
	mipi_set_cmd_mode_t mipi_set_cmd_mode;
	mipi_dcs_write_t mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set;

	mipi_set_cmd_mode = intf->info.mipi->ops->mipi_set_cmd_mode;
	mipi_dcs_write = intf->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set = intf->info.mipi->ops->mipi_eotp_set;
	mipi_set_cmd_mode(dsi_ctx);
	mipi_eotp_set(dsi_ctx, 0, 0);
	off_cmds = panel->sleep_cmds;
	for (i = 0; i < off_cmds->cmd_total; i++) {
		mipi_dcs_write(dsi_ctx, off_cmds->cmds[i].payload,
		    off_cmds->cmds[i].header.len);
		if (off_cmds->cmds[i].header.wait)
			msleep(off_cmds->cmds[i].header.wait);
	}
	mipi_eotp_set(dsi_ctx, 0, 0);
	return 0;

}


struct panel_operations lcd_mipi_ops = {
	.panel_init = lcd_mipi_init,
	.panel_enter_sleep = lcd_mipi_enter_sleep,
	.panel_readid = lcd_mipi_readid,
	.panel_esd_check = lcd_mipi_esd_check,
};
