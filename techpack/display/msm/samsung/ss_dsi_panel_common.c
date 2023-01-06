/*
 * =================================================================
 *
 *
 *	Description:  samsung display common file
 *
 *	Author: jb09.kim
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2020, Samsung Electronics. All rights reserved.

 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "ss_dsi_panel_common.h"
#include <linux/preempt.h>
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
#include "../../../drivers/input/sec_input/sec_input.h"
#endif

__visible_for_testing int ss_event_esd_recovery_init(
		struct samsung_display_driver_data *vdd, int event, void *arg);

struct samsung_display_driver_data vdd_data[MAX_DISPLAY_NDX];

/* disable pll ssc */
int vdd_pll_ssc_disabled;

void __iomem *virt_mmss_gp_base;

LIST_HEAD(vdds_list);

char *brr_mode_name[MAX_ALL_BRR_MODE] = {
	"BRR_48_60",
	"BRR_60_48",
	"BRR_96_120",
	"BRR_120_96",
	"BRR_60HS_120",
	"BRR_120_60HS",
	"BRR_60HS_96",
	"BRR_96_60HS",
	"MAX_BRR_ON_MODE",
	"BRR_STOP_MODE",
	"BRR_OFF_MODE",
};

char ss_cmd_set_prop_map[SS_CMD_PROP_SIZE][SS_CMD_PROP_STR_LEN] = {
	/* samsung feature */
	"SS_DSI_CMD_SET_START not parsed from DTSI",

	"SS_DSI_CMD_SET_FOR_EACH_MODE_START not parsed from DTSI",
	"samsung,panel_ltps_tx_cmds_revA",
	"samsung,mdss_each_mode_dsi_on_tx_cmds_revA",
	"SS_DSI_CMD_SET_FOR_EACH_MODE_END not parsed from DTSI",

	/* TX */
	"TX_CMD_START not parsed from DTSI",
	"samsung,mdss_dsi_on_tx_cmds_revA",
	"samsung,mtp_write_sysfs_tx_cmds_revA",
	"samsung,temp_dsc_tx_cmds_revA",
	"samsung,display_on_tx_cmds_revA",
	"samsung,display_on_post_tx_cmds_revA",
	"samsung,first_display_on_tx_cmds_revA",
	"samsung,display_off_tx_cmds_revA",
	"samsung,brightness_tx_cmds_revA",
	"samsung,manufacture_read_pre_tx_cmds_revA",
	"samsung,manufacture_read_post_tx_cmds_revA",
	"samsung,level0_key_enable_tx_cmds_revA",
	"samsung,level0_key_disable_tx_cmds_revA",
	"samsung,level1_key_enable_tx_cmds_revA",
	"samsung,level1_key_disable_tx_cmds_revA",
	"samsung,level2_key_enable_tx_cmds_revA",
	"samsung,level2_key_disable_tx_cmds_revA",
	"samsung,flash_key_enable_tx_cmds_revA",
	"samsung,flash_key_disable_tx_cmds_revA",
	"TX_MDNIE_ADB_TEST not parsed from DTSI",
	"samsung,self_grid_on_revA",
	"samsung,self_grid_off_revA",
	"samsung,lpm_on_pre_tx_cmds_revA",
	"samsung,lpm_on_tx_cmds_revA",
	"samsung,lpm_off_tx_cmds_revA",
	"samsung,lpm_on_aor_tx_cmds_revA",
	"samsung,lpm_off_aor_tx_cmds_revA",
	"samsung,lpm_ctrl_alpm_aod_on_tx_cmds_revA",
	"samsung,lpm_ctrl_alpm_aod_off_tx_cmds_revA",
	"samsung,lpm_2nit_tx_cmds_revA",
	"samsung,lpm_10nit_tx_cmds_revA",
	"samsung,lpm_30nit_tx_cmds_revA",
	"samsung,lpm_40nit_tx_cmds_revA",
	"samsung,lpm_60nit_tx_cmds_revA",
	"samsung,lpm_ctrl_alpm_2nit_tx_cmds_revA",
	"samsung,lpm_ctrl_alpm_10nit_tx_cmds_revA",
	"samsung,lpm_ctrl_alpm_30nit_tx_cmds_revA",
	"samsung,lpm_ctrl_alpm_40nit_tx_cmds_revA",
	"samsung,lpm_ctrl_alpm_60nit_tx_cmds_revA",
	"samsung,lpm_ctrl_alpm_off_tx_cmds_revA",
	"samsung,lpm_ctrl_hlpm_2nit_tx_cmds_revA",
	"samsung,lpm_ctrl_hlpm_10nit_tx_cmds_revA",
	"samsung,lpm_ctrl_hlpm_30nit_tx_cmds_revA",
	"samsung,lpm_ctrl_hlpm_40nit_tx_cmds_revA",
	"samsung,lpm_ctrl_hlpm_60nit_tx_cmds_revA",
	"samsung,lpm_ctrl_hlpm_off_tx_cmds_revA",
	"samsung,lpm_brightnes_tx_cmds_revA",
	"samsung,packet_size_tx_cmds_revA",
	"samsung,reg_read_pos_tx_cmds_revA",
	"samsung,mdnie_tx_cmds_revA",
	"samsung,osc_te_fitting_tx_cmds_revA",
	"samsung,avc_on_revA",
	"samsung,ldi_fps_change_tx_cmds_revA",
	"samsung,hmt_enable_tx_cmds_revA",
	"samsung,hmt_disable_tx_cmds_revA",
	"TX_HMT_LOW_PERSISTENCE_OFF_BRIGHT not parsed from DTSI",
	"samsung,hmt_reverse_tx_cmds_revA",
	"samsung,hmt_forward_tx_cmds_revA",
	"samsung,ffc_tx_cmds_revA",
	"samsung,ffc_off_tx_cmds_revA",
	"samsung,dyn_mipi_clk_ffc_cmds_pre_revA",
	"samsung,dyn_mipi_clk_ffc_cmds_revA",
	"samsung,osc_tx_cmds_revA",
	"samsung,dyn_mipi_clk_osc_cmds_revA",
	"samsung,cabc_on_tx_cmds_revA",
	"samsung,cabc_off_tx_cmds_revA",
	"samsung,tft_pwm_tx_cmds_revA",
	"samsung,gamma_mode2_normal_tx_cmds_revA",
	"samsung,gamma_mode2_hbm_tx_cmds_revA",
	"samsung,gamma_mode2_hbm_60hz_tx_cmds_revA",
	"samsung,gamma_mode2_hmt_tx_cmds_revA",
	"samsung,panel_ldi_vdd_offset_write_cmds_revA",
	"samsung,panel_ldi_vddm_offset_write_cmds_revA",
	"samsung,hsync_on_tx_cmds_revA",
	"samsung,cabc_on_duty_tx_cmds_revA",
	"samsung,cabc_off_duty_tx_cmds_revA",
	"samsung,copr_enable_tx_cmds_revA",
	"samsung,copr_disable_tx_cmds_revA",
	"samsung,ccb_on_tx_cmds_revA",
	"samsung,ccb_off_tx_cmds_revA",
	"samsung,esd_recovery_1_tx_cmds_revA",
	"samsung,esd_recovery_2_tx_cmds_revA",
	"samsung,esd_sleep_in_cmds_revA", /* SiliconWorks DDI need sleep in cmd in ESD Recovery */
	"samsung,mcd_read_resistantant_pre_tx_cmds_revA", /* For read real MCD R/L resistance */
	"samsung,mcd_read_resistantant_tx_cmds_revA", /* For read real MCD R/L resistance */
	"samsung,mcd_read_resistantant_post_tx_cmds_revA", /* For read real MCD R/L resistance */
	"samsung,gradual_acl_tx_cmds_revA",
	"samsung,hw_cursor_tx_cmds_revA",
	"samsung,dynamic_hlpm_enable_tx_cmds_revA",
	"samsung,dynamic_hlpm_disable_tx_cmds_revA",
	"samsung,panel_multires_fhd_to_wqhd_revA",
	"samsung,panel_multires_hd_to_wqhd_revA",
	"samsung,panel_multires_fhd_revA",
	"samsung,panel_multires_hd_revA",
	"samsung,panel_cover_control_enable_cmds_revA",
	"samsung,panel_cover_control_disable_cmds_revA",
	"samsung,hbm_gamma_tx_cmds_revA",
	"samsung,hbm_etc_tx_cmds_revA",
	"samsung,hbm_irc_tx_cmds_revA",
	"samsung,hbm_off_tx_cmds_revA",
	"samsung,aid_tx_cmds_revA",
	"samsung,aid_subdivision_tx_cmds_revA",
	"samsung,pac_aid_subdivision_tx_cmds_revA",
	"samsung,test_aid_tx_cmds_revA",
	"samsung,acl_on_tx_cmds_revA",
	"samsung,acl_off_tx_cmds_revA",
	"samsung,elvss_tx_cmds_revA",
	"samsung,elvss_high_tx_cmds_revA",
	"samsung,elvss_mid_tx_cmds_revA",
	"samsung,elvss_low_tx_cmds_revA",
	"samsung,elvss_pre_tx_cmds_revA",
	"samsung,gamma_tx_cmds_revA",
	"samsung,smart_dimming_mtp_tx_cmds_revA",
	"samsung,hmt_elvss_tx_cmds_revA",
	"samsung,hmt_vint_tx_cmds_revA",
	"samsung,hmt_irc_tx_cmds_revA",
	"samsung,hmt_gamma_tx_cmds_revA",
	"samsung,hmt_aid_tx_cmds_revA",
	"samsung,elvss_lowtemp_tx_cmds_revA",
	"samsung,elvss_lowtemp2_tx_cmds_revA",
	"samsung,smart_acl_elvss_tx_cmds_revA",
	"samsung,smart_acl_elvss_lowtemp_tx_cmds_revA",
	"samsung,vint_tx_cmds_revA",
	"samsung,mcd_on_tx_cmds_revA",
	"samsung,mcd_off_tx_cmds_revA",
	"samsung,irc_tx_cmds_revA",
	"samsung,irc_subdivision_tx_cmds_revA",
	"samsung,pac_irc_subdivision_tx_cmds_revA",
	"samsung,irc_off_tx_cmds_revA",
	"samsung,smooth_dimming_on_tx_cmds_revA",
	"samsung,smooth_dimming_off_tx_cmds_revA",
	"samsung,normal_brightness_etc_tx_cmds_revA",
	"TX_POC_CMD_START not parsed from DTSI",
	"samsung,poc_enable_tx_cmds_revA",
	"samsung,poc_disable_tx_cmds_revA",
	"samsung,poc_erase_tx_cmds_revA",
	"samsung,poc_erase1_tx_cmds_revA",
	"samsung,poc_pre_erase_sector_tx_cmds_revA",
	"samsung,poc_erase_sector_tx_cmds_revA",
	"samsung,poc_post_erase_sector_tx_cmds_revA",
	"samsung,poc_pre_write_tx_cmds_revA",
	"samsung,poc_write_loop_start_tx_cmds_revA",
	"samsung,poc_write_loop_data_add_tx_cmds_revA",
	"samsung,poc_write_loop_1byte_tx_cmds_revA",
	"samsung,poc_write_loop_256byte_tx_cmds_revA",
	"samsung,poc_write_loop_end_tx_cmds_revA",
	"samsung,poc_post_write_tx_cmds_revA",
	"samsung,poc_pre_read_tx_cmds_revA",
	"samsung,poc_pre_read2_tx_cmds_revA",
	"samsung,poc_read_tx_cmds_revA",
	"samsung,poc_post_read_tx_cmds_revA",
	"samsung,reg_poc_read_pos_tx_cmds_revA",
	"TX_POC_CMD_END not parsed from DTSI",
	"TX_DDI_RAM_IMG_DATA not parsed from DTSI",
	"samsung,isc_defect_test_on_tx_cmds_revA",
	"samsung,isc_defect_test_off_tx_cmds_revA",
	"samsung,partial_display_on_tx_cmds_revA",
	"samsung,partial_display_off_tx_cmds_revA",
	"samsung,dia_on_tx_cmds_revA",
	"samsung,dia_off_tx_cmds_revA",
	"samsung,manual_dbv_tx_cmds_revA",
	"samsung,self_idle_aod_enter",
	"samsung,self_idle_aod_exit",
	"samsung,self_idle_timer_on",
	"samsung,self_idle_timer_off",
	"samsung,self_idle_move_on_pattern1",
	"samsung,self_idle_move_on_pattern2",
	"samsung,self_idle_move_on_pattern3",
	"samsung,self_idle_move_on_pattern4",
	"samsung,self_idle_move_off",
	"samsung,spsram_data_write_cmds_revA",
	"samsung,spsram_data_read_cmds_revA",
	"samsung,spsram_done_cmds_revA",

	/* self display */
	"TX_SELF_DISP_CMD_START not parsed from DTSI",
	"samsung,self_dispaly_on_revA",
	"samsung,self_dispaly_off_revA",
	"samsung,self_time_set_revA",
	"samsung,self_move_on_revA",
	"samsung,self_move_on_100_revA",
	"samsung,self_move_on_200_revA",
	"samsung,self_move_on_500_revA",
	"samsung,self_move_on_1000_revA",
	"samsung,self_move_on_debug_revA",
	"samsung,self_move_reset_revA",
	"samsung,self_move_off_revA",
	"samsung,self_move_2c_sync_off_revA",
	"samsung,self_mask_setting_pre_revA",
	"samsung,self_mask_setting_post_revA",
	"samsung,self_mask_mem_setting_revA",
	"samsung,self_mask_on_revA",
	"samsung,self_mask_on_factory_revA",
	"samsung,self_mask_off_revA",
	"samsung,self_mask_udc_on_revA",
	"samsung,self_mask_udc_off_revA",
	"samsung,self_mask_green_circle_on_revA",
	"samsung,self_mask_green_circle_off_revA",
	"samsung,self_mask_green_circle_on_factory_revA",
	"samsung,self_mask_green_circle_off_factory_revA",
	"TX_SELF_MASK_IMAGE not parsed from DTSI",
	"TX_SELF_MASK_IMAGE_CRC not parsed from DTSI",
	"samsung,self_icon_setting_pre_revA",
	"samsung,self_icon_setting_post_revA",
	"samsung,self_icon_mem_setting_revA",
	"samsung,self_icon_grid_revA",
	"samsung,self_icon_on_revA",
	"samsung,self_icon_on_grid_on_revA",
	"samsung,self_icon_on_grid_off_revA",
	"samsung,self_icon_off_grid_on_revA",
	"samsung,self_icon_off_grid_off_revA",
	"samsung,self_icon_grid_2c_sync_off_revA",
	"samsung,self_icon_off_revA",
	"TX_SELF_ICON_IMAGE not parsed from DTSI",
	"samsung,self_brightness_icon_on_revA",
	"samsung,self_brightness_icon_off_revA",
	"samsung,self_aclock_setting_pre_revA",
	"samsung,self_aclock_setting_post_revA",
	"samsung,self_aclock_sidemem_setting_revA",
	"samsung,self_aclock_on_revA",
	"samsung,self_aclock_time_update_revA",
	"samsung,self_aclock_rotation_revA",
	"samsung,self_aclock_off_revA",
	"samsung,self_aclock_hide_revA",
	"TX_SELF_ACLOCK_IMAGE not parsed from DTSI",
	"samsung,self_dclock_setting_pre_revA",
	"samsung,self_dclock_setting_post_revA",
	"samsung,self_dclock_sidemem_setting_revA",
	"samsung,self_dclock_on_revA",
	"samsung,self_dclock_blinking_on_revA",
	"samsung,self_dclock_blinking_off_revA",
	"samsung,self_dclock_time_update_revA",
	"samsung,self_dclock_off_revA",
	"samsung,self_dclock_hide_revA",
	"TX_SELF_DCLOCK_IMAGE not parsed from DTSI",
	"samsung,self_clock_2c_sync_off_revA",
	"TX_SELF_VIDEO_IMAGE not parsed from DTSI",
	"samsung,self_video_mem_setting_revA",
	"samsung,self_video_on_revA",
	"samsung,self_video_of_revA",
	"samsung,self_partial_hlpm_scan_set_revA",
	"samsung,self_disp_debug_rx_cmds_revA",
	"samsung,self_mask_check_tx_pre1_revA",
	"samsung,self_mask_check_tx_pre2_revA",
	"samsung,self_mask_check_tx_post_revA",
	"samsung,self_mask_check_rx_cmds_revA",
	"TX_SELF_DISP_CMD_END not parsed from DTSI",

	/* MAFPC */
	"TX_MAFPC_CMD_START not parsed from DTSI",
	"samsung,mafpc_flash_sel_revA",
	"samsung,mafpc_brightness_scale_revA",
	"samsung,mafpc_read_1_revA",
	"samsung,mafpc_read_2_revA",
	"samsung,mafpc_read_3_revA",
	"samsung,mafpc_setting_pre_for_instant_revA",
	"samsung,mafpc_setting_pre_revA",
	"samsung,mafpc_setting_pre2_revA",
	"samsung,mafpc_setting_post_revA",
	"samsung,mafpc_setting_post_for_instant_revA",
	"samsung,mafpc_on_revA",
	"samsung,mafpc_on_factory_revA",
	"samsung,mafpc_off_revA",
	"samsung,mafpc_te_on_revA",
	"samsung,mafpc_te_off_revA",
	"TX_SELF_MAFPC_IMAGE not parsed from DTSI",
	"TX_MAFPC_CRC_CHECK_IMAGE not parsed from DTSI",
	"samsung,mafpc_check_tx_pre1_revA",
	"samsung,mafpc_check_tx_pre2_revA",
	"samsung,mafpc_check_tx_post_revA",
	"samsung,mafpc_check_rx_cmds_revA",
	"TX_MAFPC_CMD_END not parsed from DTSI",

	/* TEST MODE */
	"TX_TEST_MODE_CMD_START not parsed from DTSI",
	"samsung,gct_checksum_rx_cmds_revA",
	"samsung,gct_ecc_rx_cmds_revA",
	"samsung,ssr_on_rx_cmds_revA",
	"samsung,ssr_check_rx_cmds_revA",
	"samsung,gct_enter_tx_cmds_revA",
	"samsung,gct_mid_tx_cmds_revA",
	"samsung,gct_exit_tx_cmds_revA",
	"samsung,gct_lv_tx_cmds_revA", /* support_ss_cmd  */
	"samsung,gct_hv_tx_cmds_revA", /* support_ss_cmd  */
	"samsung,gray_spot_test_on_tx_cmds_revA",
	"samsung,gray_spot_test_off_tx_cmds_revA",
	"samsung,gray_spot_rx_cmds_revA",
	"samsung,micro_short_test_on_tx_cmds_revA",
	"samsung,micro_short_test_off_tx_cmds_revA",
	"samsung,ccd_test_on_tx_cmds_revA",
	"samsung,ccd_test_off_tx_cmds_revA",
	"samsung,ccd_state_rx_cmds_revA",
	"samsung,brightdot_on_tx_cmds_revA",
	"samsung,brightdot_off_tx_cmds_revA",
	"samsung,brightdot_lf_on_tx_cmds_revA",
	"samsung,brightdot_lf_off_tx_cmds_revA",
	"TX_TEST_MODE_CMD_END not parsed from DTSI",

	/*FLASH GAMMA */
	"samsung,flash_gamma_pre_tx_cmds1_revA",
	"samsung,flash_gamma_pre_tx_cmds2_revA",
	"samsung,flash_gamma_tx_cmds_revA",
	"samsung,flash_gamma_post_tx_cmds_revA",

	/* on_pre cmds */
	"samsung,on_pre_cmds_revA",

	/* ISC data threshold test */
	"samsung,isc_data_threshold_tx_cmds_revA",

	/* STM */
	"samsung,stm_enable_tx_cmds_revA",
	"samsung,stm_disable_tx_cmds_revA",

	/* Gamma Mode 1 interpolation */
	"samsung,gamma_mode1_interpolation_test_tx_cmds_revA",

	/* SPI i/f sel on/off  */
	"samsung,spi_if_sel_on_tx_cmds_revA",
	"samsung,spi_if_sel_off_tx_cmds_revA",

	/* POC Compensation */
	"samsung,poc_compensation_cmds_revA",

	/* FD settings */
	"samsung,fd_on_tx_cmds_revA",
	"samsung,fd_off_tx_cmds_revA",

	"samsung,vrr_tx_cmds_revA",

	"samsung,gm2_gamma_comp_revA",

	"samsung,vrr_gm2_gamma_comp_tx_cmds_revA",
	"samsung,vrr_gm2_gamma_comp2_tx_cmds_revA",

	"samsung,vrr_glut_offset_cmds_revA",

	/* for vidoe panel dfps */
	"samsung,dfps_tx_cmds_revA",

	/* for certain panel that need TE adjust cmd*/
	"samsung,adjust_te_cmds_revA",

	"samsung,fg_err_cmds_revA",

	"samsung,timing_switch_pre_revA",
	"samsung,timing_switch_post_revA",

	"TX_CMD_END not parsed from DTSI",

	/* RX */
	"RX_CMD_START not parsed from DTSI",
	"samsung,smart_dimming_mtp_rx_cmds_revA",
	"samsung,center_gamma_60hs_rx_cmds_revA",
	"samsung,center_gamma_120hs_rx_cmds_revA",
	"samsung,manufacture_id_rx_cmds_revA",
	"samsung,manufacture_id0_rx_cmds_revA",
	"samsung,manufacture_id1_rx_cmds_revA",
	"samsung,manufacture_id2_rx_cmds_revA",
	"samsung,module_info_rx_cmds_revA",
	"samsung,manufacture_date_rx_cmds_revA",
	"samsung,ddi_id_rx_cmds_revA",
	"samsung,cell_id_rx_cmds_revA",
	"samsung,octa_id_rx_cmds_revA",
	"samsung,octa_id1_rx_cmds_revA",
	"samsung,octa_id2_rx_cmds_revA",
	"samsung,octa_id3_rx_cmds_revA",
	"samsung,octa_id4_rx_cmds_revA",
	"samsung,octa_id5_rx_cmds_revA",
	"samsung,rddpm_rx_cmds_revA",
	"samsung,mtp_read_sysfs_rx_cmds_revA",
	"samsung,elvss_rx_cmds_revA",
	"samsung,irc_rx_cmds_revA",
	"samsung,hbm_rx_cmds_revA",
	"samsung,hbm2_rx_cmds_revA",
	"samsung,hbm3_rx_cmds_revA",
	"samsung,mdnie_read_rx_cmds_revA",
	"samsung,ldi_debug0_rx_cmds_revA",
	"samsung,ldi_debug1_rx_cmds_revA",
	"samsung,ldi_debug2_rx_cmds_revA",
	"samsung,ldi_debug3_rx_cmds_revA",
	"samsung,ldi_debug4_rx_cmds_revA",
	"samsung,ldi_debug5_rx_cmds_revA",
	"samsung,ldi_debug6_rx_cmds_revA",
	"samsung,ldi_debug7_rx_cmds_revA",
	"samsung,ldi_debug8_rx_cmds_revA",
	"samsung,ldi_debug_logbuf_rx_cmds_revA",
	"samsung,ldi_debug_pps1_rx_cmds_revA",
	"samsung,ldi_debug_pps2_rx_cmds_revA",
	"samsung,ldi_loading_det_rx_cmds_revA",
	"samsung,ldi_fps_rx_cmds_revA",
	"samsung,poc_read_rx_cmds_revA",
	"samsung,poc_status_rx_cmds_revA",
	"samsung,poc_checksum_rx_cmds_revA",
	"samsung,poc_mca_check_rx_cmds_revA",
	"samsung,mcd_read_resistantant_rx_cmds_revA", /* For read real MCD R/L resistance */
	"samsung,flash_gamma_rx_cmds_revA",
	"samsung,flash_loading_check_revA",

	"samsung,vbias_mtp_rx_cmds_revA",
	"samsung,vaint_mtp_rx_cmds_revA",
	"samsung,ddi_fw_id_rx_cmds_revA",
	"samsung,alpm_rx_cmds_revA",
	"RX_CMD_END not parsed from DTSI",
};

char ss_panel_id0_get(struct samsung_display_driver_data *vdd)
{
	return (vdd->manufacture_id_dsi & 0xFF0000) >> 16;
}

char ss_panel_id1_get(struct samsung_display_driver_data *vdd)
{
	return (vdd->manufacture_id_dsi & 0xFF00) >> 8;
}

char ss_panel_id2_get(struct samsung_display_driver_data *vdd)
{
	return vdd->manufacture_id_dsi & 0xFF;
}

char ss_panel_rev_get(struct samsung_display_driver_data *vdd)
{
	return vdd->manufacture_id_dsi & 0x0F;
}

int ss_panel_attach_get(struct samsung_display_driver_data *vdd)
{
	return vdd->panel_attach_status;
}

int ss_panel_attach_set(struct samsung_display_driver_data *vdd, bool attach)
{
	/* 0bit->DSI0 1bit->DSI1 */
	/* check the lcd id for DISPLAY_1 and DISPLAY_2 */
	if (likely(ss_panel_attached(vdd->ndx) && attach))
		vdd->panel_attach_status = true;
	else
		vdd->panel_attach_status = false;

	LCD_INFO(vdd, "panel_attach_status : %d\n", vdd->panel_attach_status);

	return vdd->panel_attach_status;
}

/*
 * Check the lcd id for DISPLAY_1 and DISPLAY_2 using the ndx
 */
int ss_panel_attached(int ndx)
{
	int lcd_id = 0;

	/*
	 * ndx 0 means DISPLAY_1 and ndx 1 means DISPLAY_2
	 */
	if (ndx == PRIMARY_DISPLAY_NDX)
		lcd_id = get_lcd_attached("GET");
	else if (ndx == SECONDARY_DISPLAY_NDX)
		lcd_id = get_lcd_attached_secondary("GET");

	/*
	 * The 0xFFFFFF is the id for PBA booting
	 * if the id is same with 0xFFFFFF, this function
	 * will return 0
	 */
	return !(lcd_id == PBA_ID);
}

static char *lcd_id;
module_param(lcd_id, charp, S_IRUGO);

char *lcd_id1;
EXPORT_SYMBOL(lcd_id1);
module_param(lcd_id1, charp, S_IRUGO);

static int ss_parse_panel_id(char *panel_id)
{
	char *pt;
	int lcd_id = 0;

	if (!IS_ERR_OR_NULL(panel_id))
		pt = panel_id;
	else
		return lcd_id;

	for (pt = panel_id; *pt != 0; pt++)  {
		lcd_id <<= 4;
		switch (*pt) {
		case '0' ... '9':
			lcd_id += *pt - '0';
			break;
		case 'a' ... 'f':
			lcd_id += 10 + *pt - 'a';
			break;
		case 'A' ... 'F':
			lcd_id += 10 + *pt - 'A';
			break;
		}
	}

	return lcd_id;
}

/* Return lcd_id that got by cmdline */
int get_lcd_attached(char *mode)
{
	static int _lcd_id = -EINVAL;

	pr_err("[SDE] %s : %s", __func__, mode);

	if (mode == NULL) {
		pr_err("[SDE] err: arg is NULL\n");
		return -EINVAL;
	}

	if (!strncmp(mode, "GET", 3)) {
		if (lcd_id)
			_lcd_id = ss_parse_panel_id(lcd_id);
		goto end;
	} else {
		_lcd_id = ss_parse_panel_id(mode);
	}

	pr_info("[SDE] LCD_ID = 0x%X\n", _lcd_id);
end:

	/* case 03830582: Sometimes, bootloader fails to save lcd_id value in cmdline
	 * which is located in TZ.
	 * W/A: if panel name is PBA panel while lcd_id is not PBA ID, force to set lcd_id to PBA ID.
	 */
	if (unlikely(_lcd_id == -EINVAL)) {
		char panel_name[MAX_CMDLINE_PARAM_LEN];
		char panel_string[] = "ss_dsi_panel_PBA_BOOTING_FHD";

		ss_get_primary_panel_name_cmdline(panel_name);
		if (!strncmp(panel_string, panel_name, strlen(panel_string)) && _lcd_id != PBA_ID) {
			pr_info("[SDE] pba panel name, force lcd id: 0x%X -> 0xFFFFFF\n", _lcd_id);
			_lcd_id =  PBA_ID;
		}
	}

	return _lcd_id;
}
EXPORT_SYMBOL(get_lcd_attached);
__setup("lcd_id=0x", get_lcd_attached);

int get_lcd_attached_secondary(char *mode)
{
	static int _lcd_id = -EINVAL;

	pr_err("[SDE1] %s : %s", __func__, mode);

	if (mode == NULL) {
		pr_err("[SDE1] err: arg is NULL\n");
		return true;
	}

	if (!strncmp(mode, "GET", 3)) {
		if (lcd_id1)
			_lcd_id = ss_parse_panel_id(lcd_id1);
		goto end;
	} else {
		_lcd_id = ss_parse_panel_id(mode);
	}

	pr_info("[SDE1] LCD_ID = 0x%X\n", _lcd_id);
end:

	/* case 03830582: Sometimes, bootloader fails to save lcd_id value in cmdline
	 * which is located in TZ.
	 * W/A: if panel name is PBA panel while lcd_id is not PBA ID, force to set lcd_id to PBA ID.
	 */
	if (unlikely(_lcd_id == -EINVAL)) {
		char panel_name[MAX_CMDLINE_PARAM_LEN];
		char panel_string[] = "ss_dsi_panel_PBA_BOOTING_FHD_DSI1";

		ss_get_secondary_panel_name_cmdline(panel_name);
		if (!strncmp(panel_string, panel_name, strlen(panel_string)) && _lcd_id != PBA_ID) {
			pr_info("[SDE1] pba panel name, force lcd id: 0x%X -> 0xFFFFFF\n", lcd_id);
			_lcd_id =  PBA_ID;
		}
	}

	return _lcd_id;
}
EXPORT_SYMBOL(get_lcd_attached_secondary);
__setup("lcd_id1=0x", get_lcd_attached_secondary);

int get_window_color(char *color)
{
	struct samsung_display_driver_data *vdd;
	int i;

	for (i = PRIMARY_DISPLAY_NDX; i < MAX_DISPLAY_NDX; i++) {
		vdd = ss_get_vdd(i);

		LCD_INFO(vdd, "window_color from cmdline : %s\n", color);
		memcpy(vdd->window_color, color, sizeof(vdd->window_color));
		LCD_INFO(vdd, "window_color : %s\n", vdd->window_color);
	}

	return true;
}
EXPORT_SYMBOL(get_window_color);
__setup("window_color=", get_window_color);

int ss_is_recovery_check(char *str)
{
	struct samsung_display_driver_data *vdd;
	int i;

	for (i = PRIMARY_DISPLAY_NDX; i < MAX_DISPLAY_NDX; i++) {
		vdd = ss_get_vdd(i);
		if (strncmp(str, "1", 1) == 0)
			vdd->is_recovery_mode = 1;
		else
			vdd->is_recovery_mode = 0;
	}
	LCD_INFO(vdd, "vdd->is_recovery_mode = %d\n", vdd->is_recovery_mode);
	return true;
}
EXPORT_SYMBOL(ss_is_recovery_check);
__setup("androidboot.boot_recovery=", ss_is_recovery_check);
void ss_event_frame_update_pre(struct samsung_display_driver_data *vdd)
{
	/* mAFPC */
	if (vdd->mafpc.is_support) {
		if (vdd->mafpc.need_to_write) {
			LCD_INFO(vdd, "update mafpc\n");

			if (!IS_ERR_OR_NULL(vdd->mafpc.img_write))
				vdd->mafpc.img_write(vdd, false);
			else
				LCD_ERR(vdd, "mafpc img_write function is NULL\n");

			if (!IS_ERR_OR_NULL(vdd->mafpc.enable)) {
				if (vdd->mafpc.en)
					vdd->mafpc.enable(vdd, true);
				else
					vdd->mafpc.enable(vdd, false);
			}
			else {
				LCD_ERR(vdd, "mafpc enable function is NULL\n");
			}

			vdd->mafpc.need_to_write = false;
		}
	}

	/* Block frame update during exclusive mode on. (deprecated) */
	if (unlikely(vdd->exclusive_tx.enable)) {
		if (unlikely(!vdd->exclusive_tx.permit_frame_update)) {
			LCD_INFO(vdd, "exclusive mode on, stop frame update\n");
			wait_event(vdd->exclusive_tx.ex_tx_waitq,
				!vdd->exclusive_tx.enable);
			LCD_INFO(vdd, "continue frame update\n");
		}
	}

	/* exclusive_tx.permit_frame_update is deprecated..
	 * Instead, use support_ss_cmd and block_commit_cnt.
	 */
	if (atomic_read(&vdd->block_commit_cnt) > 0) {
		int ret;

		LCD_INFO(vdd, "wait block commit cnt +++\n");
		ret = wait_event_timeout(vdd->block_commit_wq,
				atomic_read(&vdd->block_commit_cnt) == 0,
				msecs_to_jiffies(10000)); /* 10 sec. */
		if (ret) {
			atomic_set(&vdd->block_commit_cnt, 0);
			LCD_ERR(vdd, "block commit timeout(10 sec)\n");
		}
		LCD_INFO(vdd, "wait block commit cnt ---\n");
	}
}

/*
 * ss_delay(delay, from).
 * calcurate needed delay from 'from' kernel time and wait.
 */
void ss_delay(s64 delay, ktime_t from)
{
	s64 wait_ms, past_ms;

	if (!delay || !from)
		return;

	// past time from 'from' time
	past_ms = ktime_ms_delta(ktime_get(), from);

	// determine needed delay
	wait_ms = delay - past_ms;

	if (wait_ms > 0) {
		pr_info("[SDE] delay:%lld, past:%lld, need wait_ms:%lld [ms] start\n", delay, past_ms, wait_ms);
		usleep_range(wait_ms*1000, wait_ms*1000);
	} else
		pr_info("[SDE] delay:%lld, past:%lld, [ms] skip\n", delay, past_ms);

	return;
}

void ss_event_frame_update_post(struct samsung_display_driver_data *vdd)
{
	struct panel_func *panel_func = NULL;
	static u8 frame_count = 1;

	if (!vdd)
		return;

	panel_func = &vdd->panel_func;

	if (vdd->display_status_dsi.wait_disp_on) {
		/* TODO: Add condition to check the susupend state
		 * insted of !msm_is_suspend_state(GET_DRM_DEV(vdd))
		 */

		/* Skip a number of frames to avoid garbage image output from wakeup */
		if (frame_count <= vdd->dtsi_data.samsung_delayed_display_on) {
			LCD_DEBUG(vdd, "Skip %d frame\n", frame_count);
			frame_count++;
			goto skip_display_on;
		}
		frame_count = 1;

		/* set self_mask_udc before display on */
		if (vdd->self_disp.self_mask_udc_on)
			vdd->self_disp.self_mask_udc_on(vdd, vdd->self_disp.udc_mask_enable);
		else
			LCD_DEBUG(vdd, "Self Mask UDC Function is NULL\n");

		/* delay between sleep_out and display_on cmd */
		ss_delay(vdd->dtsi_data.sleep_out_to_on_delay, vdd->sleep_out_time);

		/* insert black frame using self_grid on cmd in case of LPM mode */
		if (vdd->panel_lpm.need_self_grid && ss_is_panel_lpm(vdd)) {
			ss_send_cmd(vdd, TX_SELF_GRID_ON);
			LCD_INFO(vdd, "self_grid [ON]\n");
		}

		if (!SS_IS_CMDS_NULL(ss_get_cmds(vdd, TX_DISPLAY_ON)))
			ss_send_cmd(vdd, TX_DISPLAY_ON);

		vdd->display_status_dsi.wait_disp_on = false;
		if (vdd->display_status_dsi.first_commit_disp_on) {
			if (!SS_IS_CMDS_NULL(ss_get_cmds(vdd, TX_FIRST_DISPLAY_ON))) {
				usleep_range(18000, 18100);
				ss_send_cmd(vdd, TX_FIRST_DISPLAY_ON);
			}
			vdd->display_status_dsi.first_commit_disp_on = false;
		}

		if (!SS_IS_CMDS_NULL(ss_get_cmds(vdd, TX_DISPLAY_ON_POST))) {
			LCD_INFO(vdd, "Sending tx disp on post cmd.\n");
			ss_send_cmd(vdd, TX_DISPLAY_ON_POST);
		}

		if (vdd->panel_func.samsung_display_on_post_debug)
			vdd->panel_func.samsung_display_on_post_debug(vdd);

		if (vdd->panel_lpm.need_self_grid && ss_is_panel_lpm(vdd)) {
			/* should distinguish display_on and self_grid_off with vsnyc */
			/* delay 34ms in self_grid_off_revA cmds */
			ss_send_cmd(vdd, TX_SELF_GRID_OFF);
			LCD_INFO(vdd, "self_grid [OFF]\n");
		}

		vdd->panel_lpm.need_self_grid = false;

		if (ss_is_esd_check_enabled(vdd))
			vdd->esd_recovery.is_enabled_esd_recovery = true;

		/* For read flash gamma data before first brightness set */
		LCD_INFO(vdd, "flash_gamma_init_done: %d, flash_gamma_support: %d\n",
			vdd->br_info.flash_gamma_init_done, vdd->br_info.flash_gamma_support);

		if (vdd->br_info.flash_gamma_support && !vdd->br_info.flash_gamma_init_done) {
			if (!work_busy(&vdd->br_info.flash_br_work.work)) {
				queue_delayed_work(vdd->br_info.flash_br_workqueue, &vdd->br_info.flash_br_work, msecs_to_jiffies(0));
			}
		}

		vdd->display_on = true;
		LCD_INFO(vdd, "DISPLAY_ON\n");
	}

	/* copr - check actual display on (frame - copr > 0) debug */
	/* work thread will be stopped if copr is over 0 */
	if (vdd->display_status_dsi.wait_actual_disp_on && ss_is_panel_on(vdd)) {
		if (vdd->copr.read_copr_wq && vdd->copr.copr_on)
			queue_work(vdd->copr.read_copr_wq, &vdd->copr.read_copr_work);
	}
skip_display_on:
	return;
}

void ss_check_te(struct samsung_display_driver_data *vdd)
{
	unsigned int disp_te_gpio;
	int rc, te_count = 0;
	int te_max = 20000; /*sampling 200ms */

	disp_te_gpio = ss_get_te_gpio(vdd);

	LCD_INFO(vdd, "============ start waiting for TE CTRL%d ============\n", vdd->ndx);
	if (ss_gpio_is_valid(disp_te_gpio)) {
		for (te_count = 0 ; te_count < te_max ; te_count++) {
			rc = ss_gpio_get_value(vdd, disp_te_gpio);
			if (rc == 1) {
				LCD_INFO(vdd, "LDI generate TE within = %d.%dms\n", te_count/100, te_count%100);
				break;
			}
			/* usleep suspends the calling thread whereas udelay is a
			 * busy wait. Here the value of te_gpio is checked in a loop of
			 * max count = 250. If this loop has to iterate multiple
			 * times before the te_gpio is 1, the calling thread will end
			 * up in suspend/wakeup sequence multiple times if usleep is
			 * used, which is an overhead. So use udelay instead of usleep.
			 */
			udelay(10);
		}
		if (te_count == te_max) {
			LCD_ERR(vdd, "LDI doesn't generate TE");
			SS_XLOG(0xbad);
			inc_dpui_u32_field(DPUI_KEY_QCT_NO_TE, 1);
		}
	} else
		LCD_ERR(vdd, "disp_te_gpio is not valid\n");
	LCD_INFO(vdd, "============ end waiting for TE CTRL%d ============\n", vdd->ndx);
}

/* SAMSUNG_FINGERPRINT */
/* TODO: replace with sde_encoder_wait_for_event(drm_enc, MSM_ENC_VBLANK) */
void ss_wait_for_te_gpio(struct samsung_display_driver_data *vdd, int num_of_te, int delay_after_te, bool preemption)
{
	unsigned int disp_te_gpio;
	int rc, te_count = 0;
	int te_max = 20000; /*sampling 100ms */
	int iter;
	s64 start_time_1_64, start_time_3_64;

	/*
	 * If you need correct TE signal such as Finger Brightness case, you should use preemption & delay.
	 * But, you should know that preept_disable & delay can cause Audio problem (P201222-01434, P211129-05047)
	 */
	if (preemption)
		preempt_disable();

	disp_te_gpio = ss_get_te_gpio(vdd);

	for(iter = 0 ; iter < num_of_te ; iter++) {
		/* 1. check high te gpio value */
		if (ss_gpio_is_valid(disp_te_gpio)) {
			start_time_1_64 = ktime_to_us(ktime_get());

			for (te_count = 0 ; te_count < te_max ; te_count++) {
				rc = ss_gpio_get_value(vdd, disp_te_gpio);
				if (rc == 1) {
					start_time_3_64 = ktime_to_us(ktime_get());
					LCD_INFO(vdd, "ss_wait_for_te_gpio	= %llu\n", start_time_3_64- start_time_1_64);
					break;
				}

				if (preemption)
					ndelay(5000);
				else
					usleep_range(1, 2);
			}
		}

		/* 2. wait for low gpio value */
		if (te_count == te_max)
			LCD_ERR(vdd, "LDI doesn't generate TE");
		else {
			if (ss_gpio_is_valid(disp_te_gpio) && (iter < (num_of_te - 1))) {
				for (te_count = 0 ; te_count < te_max ; te_count++) {
					rc = gpio_get_value(disp_te_gpio);
					if (rc == 0)
						break;
					else {
						if (preemption)
							udelay(200);
						else
							usleep_range(200, 220);
					}
				}
			}
		}
	}

	if (delay_after_te)
		udelay(delay_after_te);

	if (preemption)
		preempt_enable();
}

extern struct sde_encoder_phys *ss_get_encoder_phys(struct drm_encoder *drm_enc, int dsi_index);
void ss_wait_for_vsync(struct samsung_display_driver_data *vdd,
		int num_of_vsnc, int delay_after_vsync)
{
	struct drm_encoder *drm_enc = GET_DRM_ENCODER(vdd);
	struct sde_encoder_phys *phys_encs = ss_get_encoder_phys(drm_enc, vdd->ndx);
	s64 start_time, end_time;

	/* Skip before display on */
	if (vdd->display_status_dsi.wait_disp_on) {
		LCD_INFO(vdd, "skip waiting, no vsync yet\n");
		usleep_range(10000, 10005);
		return;
	}

	phys_encs->ops.control_vblank_irq(phys_encs, true);
	start_time = ktime_to_us(ktime_get()); /* us */
	atomic_set(&vdd->ss_vsync_cnt, num_of_vsnc);

	LCD_INFO(vdd, "+++ vsnyc_cnt:%d\n",	atomic_read(&vdd->ss_vsync_cnt));
	wait_event_timeout(vdd->ss_vync_wq, atomic_read(&vdd->ss_vsync_cnt) == 0,
			msecs_to_jiffies(50));
	phys_encs->ops.control_vblank_irq(phys_encs, false);

	if (atomic_read(&vdd->ss_vsync_cnt) == num_of_vsnc)
		atomic_set(&vdd->ss_vsync_cnt, 0);

	if (in_atomic())
		udelay(delay_after_vsync);
	else
		usleep_range(delay_after_vsync, delay_after_vsync+5);

	end_time = ktime_to_us(ktime_get());
	LCD_INFO(vdd, "--- waited %llu us (include %d delay)\n",
		end_time-start_time, delay_after_vsync);
}

/* SAMSUNG_FINGERPRINT */
void ss_send_hbm_fingermask_image_tx(struct samsung_display_driver_data *vdd, bool on)
{
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);

	LCD_INFO(vdd, "++ %s\n", on ? "on" : "off");
	if (on) {
		ss_brightness_dcs(vdd, 0, BACKLIGHT_FINGERMASK_ON);
	} else {
		if ((panel->panel_mode == DSI_OP_CMD_MODE) || !(vdd->support_optical_fingerprint))
			ss_brightness_dcs(vdd, 0, BACKLIGHT_FINGERMASK_OFF);
		else /* If video mode, opt, fingermask off */
			LCD_INFO(vdd, "skip to call brightness ft in video mode && mask off\n");
			/* Calling br moved to _sde_encoder_trigger_flush */
	}
	LCD_INFO(vdd, "--\n");
}

void ss_pba_config(struct samsung_display_driver_data *vdd, void *arg)
{
	if (unlikely(vdd->panel_func.samsung_pba_config))
		vdd->panel_func.samsung_pba_config(vdd, arg);

	return;
}

/* Identify VRR HS by drm_mode's name.
 * drm_mode's name is defined by dsi_mode->timing.sot_hs_mode parsed
 * from samsung,mdss-dsi-sot-hs-mode in panel dtsi file.
 * ex) drm_mode->name is "1080x2316x60x193345cmdHS" for HS mode.
 *     drm_mode->name is "1080x2316x60x193345cmdNS" for NS mode.
 * To use this feature, declare different porch between HS and NS modes,
 * in panel dtsi file.
 */
bool ss_is_sot_hs_from_drm_mode(const struct drm_display_mode *drm_mode)
{
	int len;
	bool sot_hs = false;

	if (!drm_mode) {
		pr_err("invalid drm_mode\n");
		goto end;
	}

	len = strlen(drm_mode->name);
	if (len >= DRM_DISPLAY_MODE_LEN || len <= 0) {
		pr_err("invalid drm_mode name len (%d)\n", len);
		goto end;
	}

	if (!strncmp(drm_mode->name + len - 2, "HS", 2))
		sot_hs = true;
	else
		sot_hs = false;

end:
	return sot_hs;
}

bool ss_is_phs_from_drm_mode(const struct drm_display_mode *drm_mode)
{
	int len;
	bool phs = false;

	if (!drm_mode) {
		pr_err("invalid drm_mode\n");
		goto end;
	}

	len = strlen(drm_mode->name);
	if (len >= DRM_DISPLAY_MODE_LEN || len <= 0) {
		pr_err("invalid drm_mode name len (%d)\n", len);
		goto end;
	}

	if (!strncmp(drm_mode->name + len - 3, "PHS", 3))
		phs = true;
	else
		phs = false;

end:
	return phs;
}

/*
 * Determine LFD min and max freq by client's request, and priority of each clients.
 *
 * FACTORY	: factory test scenario.
 *		  determine min and max freq., priority = critical
 * TSP LPM	: tsp lpm mode scenario.
 * 		  determine min freq., priority = high
 * INPUT	: touch press scenario.
 * 		  determine min freq., priority = high
 * DISPLAY MANAGER : HBM (1hz) and low brighntess (LFD off) scenario.
 * 		     determine min freq., priority = high
 * VIDEO DETECT	: video detect scenario.
 * 		  determine max freq., priority = high
 * AOD		: AOD scenario.
 * 		  determine min freq. in lpm: handle thi in lpm brightenss, not here.
 *
 * ss_get_lfd_div should be called followed by ss_update_base_lfd_val in panel function....
 * or keep it in ss_vrr...
 */
int ss_get_lfd_div(struct samsung_display_driver_data *vdd,
		enum LFD_SCOPE_ID scope, u32 *out_min_div, u32 *out_max_div)
{
	struct vrr_info *vrr = &vdd->vrr;
	struct lfd_mngr *mngr;
	u32 max_div, min_div, max_div_def, min_div_def, min_div_lowest;
	u32 min_div_clear;
	u32 min_div_scal;
	int i;
	u32 max_freq, min_freq;
	u32 base_rr;
	struct lfd_base_str lfd_base;

	/* DEFAULT */
	if (IS_ERR_OR_NULL(vdd->panel_func.samsung_lfd_get_base_val)) {
		LCD_ERR(vdd, "fail to get lfd bsae val..\n");
		return -ENODEV;
	}

	vdd->panel_func.samsung_lfd_get_base_val(vrr, scope, &lfd_base);

	base_rr = lfd_base.base_rr;
	max_div_def = lfd_base.max_div_def;
	min_div_def = lfd_base.min_div_def;
	min_div_lowest = lfd_base.min_div_lowest;
	min_div_clear = min_div_lowest + 1;

	/* FIX */
	for (i = 0, mngr = &vrr->lfd.lfd_mngr[i]; i < LFD_CLIENT_MAX; i++, mngr++) {
		if (mngr->fix[scope] == LFD_FUNC_FIX_LOW &&
				i == LFD_CLIENT_FAC) {
			max_div = min_div = min_div_def;
			LCD_INFO(vdd, "FIX: (FAC): fix low\n");
			goto done;
		} else if (mngr->fix[scope] == LFD_FUNC_FIX_HIGH) {
			max_div = min_div = max_div_def;
			LCD_INFO(vdd, "FIX: LFD off, client: %s\n", lfd_client_name[i]);
			goto done;
		}
	}

	/* MAX and MIN */
	max_freq = 0;
	min_freq = 0;
	for (i = 0, mngr = &vrr->lfd.lfd_mngr[i]; i < LFD_CLIENT_MAX; i++, mngr++) {
		if (mngr->max[scope] != LFD_FUNC_MAX_CLEAR) {
			max_freq = max(max_freq, mngr->max[scope]);
			LCD_INFO(vdd, "client: %s, max freq=%dhz -> %dhz\n",
				lfd_client_name[i], mngr->max[scope], max_freq);
		}

		if (mngr->min[scope] != LFD_FUNC_MIN_CLEAR) {
			min_freq = max(min_freq, mngr->min[scope]);
			LCD_INFO(vdd, "client: %s, min freq=%dhz -> %dhz\n",
				lfd_client_name[i], mngr->min[scope], min_freq);
		}
	}
	if (max_freq == 0)
		max_div = max_div_def;
	else
		max_div = base_rr / max_freq; // round down in case of freq -> div

	if (min_freq == 0) {
		min_div = min_div_clear; // choose min_div_clear instead of min_div_def to allow min_div_lowest below sequence..
	} else {
		min_div = base_rr / min_freq; // round down in case of freq -> div
		min_div = min(min_div, min_div_def);
	}

	max_div = min(min_div, max_div);

	/* SCALABILITY: handle min_freq.
	 * This function is depends on panel. So use panel callback function
	 * if below fomula doesn't fit for your panel.
	 */
	for (i = 0, mngr = &vrr->lfd.lfd_mngr[i]; i < LFD_CLIENT_MAX; i++, mngr++) {
		/* TODO: get this panel specific data from lfd->... parsed by panel dtsi */
		if (mngr->scalability[scope] == LFD_FUNC_SCALABILITY1) /* under 40lux, LFD off */
			min_div_scal = 1;
		else if (mngr->scalability[scope] == LFD_FUNC_SCALABILITY2) /* touch press/release, div=2 */
		      min_div_scal = 2;
		else if (mngr->scalability[scope] == LFD_FUNC_SCALABILITY3) /* TBD, div=4 */
		      min_div_scal = 4;
		else if (mngr->scalability[scope] == LFD_FUNC_SCALABILITY4) /* TBD, div=5 */
		      min_div_scal = 5;
		else if (mngr->scalability[scope] == LFD_FUNC_SCALABILITY6) /* over 7400lux (HBM), LFD min 1hz */
			min_div_scal = min_div_lowest;
		else /* clear */
			min_div_scal = min_div_clear;

		if (mngr->scalability[scope] != LFD_FUNC_SCALABILITY0 &&
				mngr->scalability[scope] != LFD_FUNC_SCALABILITY5)
			LCD_INFO(vdd, "client: %s, scalability: %d\n",
					lfd_client_name[i], mngr->scalability[scope]);

		min_div = min(min_div, min_div_scal);
	}

	if (min_div == min_div_clear)
		min_div = min_div_def;

done:
	/* check limitation
	 * High LFD frequency means stable display quality.
	 * Low LFD frequency means low power consumption.
	 * Those are trade-off, and if there are multiple requests of LFD min/max frequency setting,
	 * choose most highest LFD freqency, means most lowest  for display quality.
	 */
	if (max_div > min_div_def) {
		LCD_INFO(vdd, "raise too low max freq %dhz (max_div=%d->%d)\n",
				DIV_ROUND_UP(base_rr, max_div),
				max_div, min_div_def);
		max_div = min_div_def;
	}

	if (max_div < max_div_def) {
		LCD_INFO(vdd, "lower too high max freq %dhz (max_div=%d->%d)\n",
				DIV_ROUND_UP(base_rr, max_div),
				max_div, max_div_def);
		max_div = max_div_def;
	}

	if (min_div > min_div_def && min_div != min_div_lowest) {
		LCD_INFO(vdd, "raisetoo low min freq %dhz (min_div=%d->%d)\n",
				DIV_ROUND_UP(base_rr, min_div),
				min_div, min_div_def);
		min_div = min_div_def;
	}

	if (min_div < max_div_def) {
		LCD_INFO(vdd, "lower too high min freq %dhz (min_div=%d->%d)\n",
				DIV_ROUND_UP(base_rr, min_div),
				min_div, max_div_def);
		min_div = max_div_def;
	}

	if (min_div < max_div) {
		LCD_ERR(vdd, "raise max freq(%dhz), lower than min freq(%dhz) (max_div=%d->%d)\n",
				DIV_ROUND_UP(base_rr, max_div),
				DIV_ROUND_UP(base_rr, min_div),
				max_div, min_div);
		max_div = min_div;
	}

	*out_min_div = min_div;
	*out_max_div = max_div;

	LCD_INFO(vdd, "LFD: cur: %dHZ@%s, base_rr: %uhz, def: %uhz(%u)~%uhz(%u), " \
			"lowest: %uhz(%u), result: %uhz(%u)~%uhz(%u)\n",
			vrr->cur_refresh_rate,
			vrr->cur_sot_hs_mode ? "HS" : "NS",
			base_rr,
			DIV_ROUND_UP(base_rr, min_div_def), min_div_def,
			DIV_ROUND_UP(base_rr, max_div_def), max_div_def,
			DIV_ROUND_UP(base_rr, min_div_lowest), min_div_lowest,
			DIV_ROUND_UP(base_rr, min_div), min_div,
			DIV_ROUND_UP(base_rr, max_div), max_div);

	return 0;
}

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
static void ss_lfd_touch_work(struct work_struct *work)
{
	struct lfd_info *lfd = container_of(work, struct lfd_info, lfd_touch_work);
	struct vrr_info *vrr = container_of(lfd, struct vrr_info, lfd);
	struct samsung_display_driver_data *vdd = container_of(vrr,
				struct samsung_display_driver_data, vrr);

	ss_brightness_dcs(vdd, USE_CURRENT_BL_LEVEL, BACKLIGHT_NORMAL);
}
static int ss_lfd_touch_notify_cb(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct lfd_info *lfd;
	struct samsung_display_driver_data *vdd;
	struct lfd_mngr *mngr;
	enum sec_input_notify_t event = val;
	struct sec_input_notify_data *tsp_ndx = v;


	if (event != NOTIFIER_LCD_VRR_LFD_LOCK_REQUEST &&
		event != NOTIFIER_LCD_VRR_LFD_LOCK_RELEASE)
		goto done;

	vdd = ss_get_vdd(tsp_ndx->dual_policy);
	if (!vdd->vrr.lfd.support_lfd) {
		LCD_INFO(vdd, "not support lfd\n");
		goto done;
	}

	lfd = &vdd->vrr.lfd;
	mngr = &lfd->lfd_mngr[LFD_CLIENT_INPUT];

	if (event == NOTIFIER_LCD_VRR_LFD_LOCK_REQUEST) {
		mngr->scalability[LFD_SCOPE_NORMAL] = LFD_FUNC_SCALABILITY2; /* div=2*/
		LCD_INFO(vdd, "touch: control LFD\n");
		if (ss_is_panel_on(vdd))
			queue_work(lfd->lfd_touch_wq, &lfd->lfd_touch_work);
		else
			LCD_INFO(vdd, "panel is not normal on(%d), delay applying\n",
					vdd->panel_state);
	} else if (event == NOTIFIER_LCD_VRR_LFD_LOCK_RELEASE) {
		mngr->scalability[LFD_SCOPE_NORMAL] = LFD_FUNC_SCALABILITY0;
		LCD_INFO(vdd, "touch LFD release LFD\n");
		if (ss_is_panel_on(vdd))
			queue_work(lfd->lfd_touch_wq, &lfd->lfd_touch_work);
		else
			LCD_INFO(vdd, "panel is not normal on(%d), delay applying\n",
					vdd->panel_state);
	}
done:
	return NOTIFY_DONE;
}

static int ss_esd_touch_notifier_cb(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct sde_connector *conn;
	struct samsung_display_driver_data *vdd;
	struct dsi_panel *panel = NULL;
	enum sec_input_notify_t event = val;
	struct sec_input_notify_data *tsp_ndx = v;

	if (event != NOTIFIER_TSP_ESD_INTERRUPT)
		goto done;

	if (!tsp_ndx) {
		vdd = ss_get_vdd(PRIMARY_DISPLAY_NDX);
		LCD_INFO(vdd, "tsp_ndx is null... Primary NDX\n");
	} else
		vdd = ss_get_vdd(tsp_ndx->dual_policy);

	conn = GET_SDE_CONNECTOR(vdd);

	if (!vdd->esd_recovery.is_enabled_esd_recovery) {
		LCD_ERR(vdd, "esd recovery is not enabled yet");
		goto done;
	}

	if (unlikely(vdd->is_factory_mode)) {
		if (ss_gpio_is_valid(vdd->ub_con_det.gpio))
			LCD_INFO(vdd, "ub_con_det.gpio = %d\n", gpio_get_value(vdd->ub_con_det.gpio));
	}

	LCD_INFO(vdd, "++\n");

	vdd->panel_lpm.esd_recovery = true; //??

	panel = (struct dsi_panel *)vdd->msm_private;
	panel->esd_config.status_mode = ESD_MODE_PANEL_IRQ;

	schedule_work(&conn->status_work.work);

	LCD_INFO(vdd, "Panel Recovery ESD, nofity val:%x, Recovery Count = %d\n",
			val, vdd->panel_recovery_cnt++);
	SS_XLOG(vdd->panel_recovery_cnt);
	inc_dpui_u32_field(DPUI_KEY_QCT_RCV_CNT, 1);

	LCD_INFO(vdd, "--\n");

done:
	return NOTIFY_DONE;

}
#endif

#if IS_ENABLED(CONFIG_DEV_RIL_BRIDGE) || IS_ENABLED(CONFIG_UML)
static int ss_find_dyn_mipi_clk_timing_idx(struct samsung_display_driver_data *vdd)
{
	int idx = -EINVAL;
	int loop;
	int rat, band, arfcn;
	struct clk_sel_table sel_table = vdd->dyn_mipi_clk.clk_sel_table;

	if (!sel_table.tab_size) {
		LCD_ERR(vdd, "Table is NULL");
		return -ENOENT;
	}

	rat = vdd->dyn_mipi_clk.rf_info.rat;
	band = vdd->dyn_mipi_clk.rf_info.band;
	arfcn = vdd->dyn_mipi_clk.rf_info.arfcn;

	for (loop = 0 ; loop < sel_table.tab_size ; loop++) {
		if ((rat == sel_table.rat[loop]) && (band == sel_table.band[loop])) {
			if ((arfcn >= sel_table.from[loop]) && (arfcn <= sel_table.end[loop])) {
				idx = sel_table.target_clk_idx[loop];
				break;
			}
		}
	}

	if (vdd->dyn_mipi_clk.force_idx) {
		idx = vdd->dyn_mipi_clk.force_idx;
		LCD_INFO(vdd, "use force index = %d\n", idx);
	} else {
		LCD_INFO(vdd, "RAT(%d), BAND(%d), ARFCN(%d), Clock Index(%d)\n",
				rat, band, arfcn, idx);
	}

	return idx;
}

static int ss_find_dyn_mipi_clk_timing_osc_idx(struct samsung_display_driver_data *vdd)
{
	int idx = -EINVAL;
	int loop;
	int rat, band, arfcn;
	struct clk_sel_table sel_table = vdd->dyn_mipi_clk.clk_sel_table;

	if (!sel_table.tab_size) {
		LCD_ERR(vdd, "Table is NULL");
		return -ENOENT;
	}

	rat = vdd->dyn_mipi_clk.rf_info.rat;
	band = vdd->dyn_mipi_clk.rf_info.band;
	arfcn = vdd->dyn_mipi_clk.rf_info.arfcn;

	for (loop = 0 ; loop < sel_table.tab_size ; loop++) {
		if ((rat == sel_table.rat[loop]) && (band == sel_table.band[loop])) {
			if ((arfcn >= sel_table.from[loop]) && (arfcn <= sel_table.end[loop])) {
				idx = sel_table.target_osc_idx[loop];
				break;
			}
		}
	}

	LCD_INFO(vdd, "RAT(%d), BAND(%d), ARFCN(%d), OSC Index(%d)\n", rat, band, arfcn, idx);

	return idx;
}

/* CP notity format (HEX raw format)
 * 10 00 AA BB 27 01 03 XX YY YY YY YY ZZ ZZ ZZ ZZ
 *
 * 00 10 (0x0010) - len
 * AA BB - not used
 * 27 - MAIN CMD (SYSTEM CMD : 0x27)
 * 01 - SUB CMD (CP Channel Info : 0x01)
 * 03 - NOTI CMD (0x03)
 * XX - RAT MODE
 * YY YY YY YY - BAND MODE
 * ZZ ZZ ZZ ZZ - FREQ INFO
 */

int ss_rf_info_notify_callback(struct notifier_block *nb,
				unsigned long size, void *data)
{
	struct dyn_mipi_clk *dyn_mipi_clk = container_of(nb, struct dyn_mipi_clk, notifier);
	struct samsung_display_driver_data *vdd =
		container_of(dyn_mipi_clk, struct samsung_display_driver_data, dyn_mipi_clk);
	int ret = NOTIFY_DONE;
#if IS_ENABLED(CONFIG_UML)
	int dev_id = DISPLAY_TEST_CP_CHANNEL_INFO;
	struct display_uml_bridge_msg *msg;
	struct rf_info rf_info_backup;
	msg = (struct display_uml_bridge_msg *)data;
#else
	int dev_id = IPC_SYSTEM_CP_CHANNEL_INFO;
	struct dev_ril_bridge_msg *msg;
	struct rf_info rf_info_backup;
	msg = (struct dev_ril_bridge_msg *)data;
#endif

	LCD_INFO(vdd, "RIL noti: ndx: %d, size: %lu, dev_id: %d, len: %d\n",
			vdd->ndx, size, msg->dev_id, msg->data_len);

	if (msg->dev_id == dev_id // #define IPC_SYSTEM_CP_CHANNEL_INFO	0x01
			&& msg->data_len == sizeof(struct rf_info)) {

		mutex_lock(&dyn_mipi_clk->dyn_mipi_lock);

		/* backup currrent rf_info */
		memcpy(&rf_info_backup, &dyn_mipi_clk->rf_info, sizeof(struct rf_info));
		memcpy(&dyn_mipi_clk->rf_info, msg->data, sizeof(struct rf_info));

		/* check & update*/
		if (ss_change_dyn_mipi_clk_timing(vdd) < 0) {
			LCD_ERR(vdd, "Failed to update MIPI clock timing, restore previous rf_info..\n");
			/* restore origin data */
			memcpy(&dyn_mipi_clk->rf_info, &rf_info_backup, sizeof(struct rf_info));
			ret = NOTIFY_BAD;
		}

		if (vdd->dyn_mipi_clk.osc_support && vdd->panel_func.update_osc)
			vdd->panel_func.update_osc(vdd, vdd->dyn_mipi_clk.requested_osc_idx);

		mutex_unlock(&dyn_mipi_clk->dyn_mipi_lock);

		LCD_INFO(vdd, "RIL noti: RAT(%d), BAND(%d), ARFCN(%d)\n",
				dyn_mipi_clk->rf_info.rat,
				dyn_mipi_clk->rf_info.band,
				dyn_mipi_clk->rf_info.arfcn);
	}

	return ret;
}

int ss_change_dyn_mipi_clk_timing(struct samsung_display_driver_data *vdd)
{
	struct clk_timing_table timing_table = vdd->dyn_mipi_clk.clk_timing_table;
	int idx;
	int osc_idx;
	int clk_rate;

	if (!vdd->dyn_mipi_clk.is_support) {
		LCD_ERR(vdd, "Dynamic MIPI Clock does not support\n");
		return -ENODEV;
	}

	if (!timing_table.tab_size) {
		LCD_ERR(vdd, "Table is NULL");
		return -ENODEV;
	}

	LCD_INFO(vdd, "+++");

	idx = ss_find_dyn_mipi_clk_timing_idx(vdd);
	if (idx < 0) {
		LCD_ERR(vdd, "Failed to find MIPI clock timing (%d)\n", idx);
	} else {
		clk_rate = timing_table.clk_rate[idx];
		LCD_INFO(vdd, "clk idx: %d, clk_rate: %d\n", idx, clk_rate);

		/* Set requested clk rate.
		 * This requested clk will be applied in dsi_display_pre_kickoff.
		 */
		vdd->dyn_mipi_clk.requested_clk_rate = clk_rate;
		vdd->dyn_mipi_clk.requested_clk_idx = idx;
	}

	if (vdd->dyn_mipi_clk.osc_support) {
		osc_idx = ss_find_dyn_mipi_clk_timing_osc_idx(vdd);
		if (osc_idx < 0) {
			LCD_ERR(vdd, "Failed to find OSC Index (%d)\n", osc_idx);
		} else {
			vdd->dyn_mipi_clk.requested_osc_idx = osc_idx;
			LCD_INFO(vdd, "osc idx: %d\n", osc_idx);
		}
	}

	LCD_INFO(vdd, "---");

	return idx;
}

int ss_dyn_mipi_clk_tx_ffc(struct samsung_display_driver_data *vdd)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(vdd->panel_func.set_ffc))
		LCD_ERR(vdd, "no set_ffc fucntion\n");
	else
		ret = vdd->panel_func.set_ffc(vdd, vdd->dyn_mipi_clk.requested_clk_idx);

	LCD_INFO(vdd, "--- clk idx: %d, tx FFC\n", vdd->dyn_mipi_clk.requested_clk_idx);

	return ret;
}

int ss_dyn_mipi_clk_tx_osc(struct samsung_display_driver_data *vdd)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(vdd->panel_func.set_osc))
		LCD_ERR(vdd, "no set_osc fucntion\n");
	else
		ret = vdd->panel_func.set_osc(vdd, vdd->dyn_mipi_clk.requested_osc_idx);

	LCD_INFO(vdd, "OSC idx: %d, tx OSC\n", vdd->dyn_mipi_clk.requested_osc_idx);

	return ret;
}

static int ss_parse_dyn_mipi_clk_timing_table(struct device_node *np,
		struct samsung_display_driver_data *vdd, char *keystring)
{
	struct clk_timing_table *table = &vdd->dyn_mipi_clk.clk_timing_table;
	const __be32 *data;
	int len = 0, i = 0, data_offset = 0;

	data = of_get_property(np, keystring, &len);
	if (!data) {
		LCD_ERR(vdd, "%d, Unable to read table %s ", __LINE__, keystring);
		return -EINVAL;
	}

	table->tab_size = len / sizeof(int);
	table->clk_rate = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->clk_rate)
		return -ENOMEM;

	for (i = 0 ; i < table->tab_size; i++)
		table->clk_rate[i] = be32_to_cpup(&data[data_offset++]);

	LCD_INFO(vdd, "[SDE] %s tab_size (%d)\n", keystring, table->tab_size);

	return 0;
}

static int ss_parse_dyn_mipi_clk_sel_table(struct device_node *np,
		struct samsung_display_driver_data *vdd, char *keystring)
{
	struct clk_sel_table *table = &vdd->dyn_mipi_clk.clk_sel_table;
	const __be32 *data;
	int len = 0, i = 0, data_offset = 0;
	int col_size = 0;

	data = of_get_property(np, keystring, &len);

	if (data)
		LCD_INFO(vdd, "Success to read table %s\n", keystring);
	else {
		LCD_ERR(vdd, "%d, Unable to read table %s ", __LINE__, keystring);
		return -EINVAL;
	}

	col_size = 5;

	if ((len % col_size) != 0) {
		LCD_ERR(vdd, "%d, Incorrect table entries for %s , len : %d",
					__LINE__, keystring, len);
		return -EINVAL;
	}

	table->tab_size = len / (sizeof(int) * col_size);

	table->rat = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->rat)
		return -ENOMEM;
	table->band = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->band)
		goto error;
	table->from = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->from)
		goto error;
	table->end = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->end)
		goto error;
	table->target_clk_idx = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->target_clk_idx)
		goto error;

	for (i = 0 ; i < table->tab_size; i++) {
		table->rat[i] = be32_to_cpup(&data[data_offset++]);
		table->band[i] = be32_to_cpup(&data[data_offset++]);
		table->from[i] = be32_to_cpup(&data[data_offset++]);
		table->end[i] = be32_to_cpup(&data[data_offset++]);
		table->target_clk_idx[i] = be32_to_cpup(&data[data_offset++]);
		LCD_DEBUG(vdd, "%dst : %d %d %d %d %d\n",
			i, table->rat[i], table->band[i], table->from[i],
			table->end[i], table->target_clk_idx[i]);
	}

	LCD_INFO(vdd, "%s tab_size (%d)\n", keystring, table->tab_size);

	return 0;

error:
	LCD_ERR(vdd, "Allocation Fail\n");
	kfree(table->rat);
	kfree(table->band);
	kfree(table->from);
	kfree(table->end);
	kfree(table->target_clk_idx);

	return -ENOMEM;
}
#endif

int ss_parse_candella_mapping_table(struct samsung_display_driver_data *vdd,
		struct device_node *np,	void *tbl, char *keystring)
{
	struct candela_map_table *table = (struct candela_map_table *) tbl;
	const __be32 *data;
	int len = 0, i = 0, data_offset = 0;
	int col_size = 0;
	bool is_gamma_mode2_support = 0;

	is_gamma_mode2_support = of_property_read_bool(np, "samsung,support_gamma_mode2");

	LCD_INFO_ONCE(vdd, "is_gamma_mode2_support = %d\n", is_gamma_mode2_support);

	data = of_get_property(np, keystring, &len);
	if (!data) {
		LCD_DEBUG(vdd, "%d, Unable to read table %s ", __LINE__, keystring);
		return -EINVAL;
	} else
		LCD_INFO(vdd, "Success to read table %s\n", keystring);

	if (is_gamma_mode2_support)
		col_size = 5;
	else
		col_size = 4;

	if ((len % col_size) != 0) {
		LCD_ERR(vdd, "%d, Incorrect table entries for %s , len : %d",
					__LINE__, keystring, len);
		return -EINVAL;
	}

	table->tab_size = len / (sizeof(int) * col_size);

	table->cd = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->cd)
		goto error;
	table->idx = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->idx)
		goto error;
	table->from = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->from)
		goto error;
	table->end = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->end)
		goto error;
	if (is_gamma_mode2_support) {
		table->gm2_wrdisbv = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
		if (!table->gm2_wrdisbv)
			goto error;
	}
	for (i = 0 ; i < table->tab_size; i++) {
		table->idx[i] = be32_to_cpup(&data[data_offset++]);		/* field => <idx> */
		table->from[i] = be32_to_cpup(&data[data_offset++]);	/* field => <from> */
		table->end[i] = be32_to_cpup(&data[data_offset++]);		/* field => <end> */
		if (is_gamma_mode2_support)
			table->gm2_wrdisbv[i] = be32_to_cpup(&data[data_offset++]);	/* field => <gm2_wrdisbv (panel_real_cd)> */
		table->cd[i] = be32_to_cpup(&data[data_offset++]);		/* field => <cd> */
	}

	table->min_lv = table->from[0];
	table->max_lv = table->end[table->tab_size-1];
	if (table->max_lv > vdd->br_info.common_br.max_bl_level)
		vdd->br_info.common_br.max_bl_level = table->max_lv;

	LCD_INFO(vdd, "tab_size (%d), min/max lv (%d/%d), max_bl_level(%d)\n",
		table->tab_size, table->min_lv, table->max_lv, vdd->br_info.common_br.max_bl_level);

	return 0;

error:
	kfree(table->cd);
	kfree(table->idx);
	kfree(table->from);
	kfree(table->end);
	if (is_gamma_mode2_support)
		kfree(table->gm2_wrdisbv);

	return -ENOMEM;
}

int ss_parse_pac_candella_mapping_table(struct samsung_display_driver_data *vdd,
		struct device_node *np,	void *tbl, char *keystring)
{
	struct candela_map_table *table = (struct candela_map_table *) tbl;
	const __be32 *data;
	int len = 0, i = 0, data_offset = 0;
	int col_size = 0;

	data = of_get_property(np, keystring, &len);
	if (!data) {
		LCD_DEBUG(vdd, "%d, Unable to read table %s ", __LINE__, keystring);
		return -EINVAL;
	} else
		LCD_INFO(vdd, "Success to read table %s\n", keystring);

	col_size = 6;

	if ((len % col_size) != 0) {
		LCD_ERR(vdd, "%d, Incorrect table entries for %s , len : %d",
					__LINE__, keystring, len);
		return -EINVAL;
	}

	table->tab_size = len / (sizeof(int) * col_size);

	table->interpolation_cd = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->interpolation_cd)
		return -ENOMEM;
	table->cd = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->cd)
		goto error;
	table->scaled_idx = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->scaled_idx)
		goto error;
	table->idx = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->idx)
		goto error;
	table->from = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->from)
		goto error;
	table->end = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->end)
		goto error;

	for (i = 0 ; i < table->tab_size; i++) {
		table->scaled_idx[i] = be32_to_cpup(&data[data_offset++]);	/* field => <scaeled idx> */
		table->idx[i] = be32_to_cpup(&data[data_offset++]);			/* field => <idx> */
		table->from[i] = be32_to_cpup(&data[data_offset++]);		/* field => <from> */
		table->end[i] = be32_to_cpup(&data[data_offset++]);			/* field => <end> */
		table->cd[i] = be32_to_cpup(&data[data_offset++]);			/* field => <cd> */
		table->interpolation_cd[i] = be32_to_cpup(&data[data_offset++]);	/* field => <interpolation cd> */
	}

	table->min_lv = table->from[0];
	table->max_lv = table->end[table->tab_size-1];
	if (table->max_lv > vdd->br_info.common_br.max_bl_level)
		vdd->br_info.common_br.max_bl_level = table->max_lv;

	LCD_INFO(vdd, "tab_size (%d), hbm min/max lv (%d/%d)\n", table->tab_size, table->min_lv, table->max_lv);

	return 0;

error:
	kfree(table->interpolation_cd);
	kfree(table->cd);
	kfree(table->scaled_idx);
	kfree(table->idx);
	kfree(table->from);
	kfree(table->end);

	return -ENOMEM;
}

int ss_parse_multi_to_one_candella_mapping_table(struct samsung_display_driver_data *vdd,
		struct device_node *np,	void *tbl, char *keystring)
{
	struct candela_map_table *table = (struct candela_map_table *) tbl;
	const __be32 *data;
	int len = 0, i = 0, data_offset = 0;
	int col_size = 0;

	data = of_get_property(np, keystring, &len);
	if (!data) {
		LCD_DEBUG(vdd, "%d, Unable to read table %s ", __LINE__, keystring);
		return -EINVAL;
	} else
		LCD_INFO(vdd, "Success to read table %s\n", keystring);

	col_size = 4;

	if ((len % col_size) != 0) {
		LCD_ERR(vdd, "%d, Incorrect table entries for %s , len : %d",
					__LINE__, keystring, len);
		return -EINVAL;
	}

	table->tab_size = len / (sizeof(int) * col_size);

	table->cd = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->cd)
		goto error;
	table->idx = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->idx)
		goto error;
	table->from = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->from)
		goto error;
	table->end = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->end)
		goto error;

	for (i = 0 ; i < table->tab_size; i++) {
		table->idx[i] = be32_to_cpup(&data[data_offset++]); 	/* field => <idx> */
		table->from[i] = be32_to_cpup(&data[data_offset++]);	/* field => <from> */
		table->end[i] = be32_to_cpup(&data[data_offset++]); 	/* field => <end> */
		table->cd[i] = be32_to_cpup(&data[data_offset++]);		/* field => <cd> */
	}

	table->min_lv = table->from[0];
	table->max_lv = table->end[table->tab_size-1];

	LCD_INFO(vdd, "tab_size (%d), hbm min/max lv (%d/%d)\n", table->tab_size, table->min_lv, table->max_lv);

	return 0;

error:
	kfree(table->cd);
	kfree(table->idx);
	kfree(table->from);
	kfree(table->end);

	return -ENOMEM;
}


int ss_parse_hbm_candella_mapping_table(struct samsung_display_driver_data *vdd,
		struct device_node *np,	void *tbl, char *keystring)
{
	struct candela_map_table *table = (struct candela_map_table *) tbl;
	const __be32 *data;
	int data_offset = 0, len = 0, i = 0;
	int col_size = 0;
	bool is_gamma_mode2_support = 0;

	is_gamma_mode2_support = of_property_read_bool(np, "samsung,support_gamma_mode2");

	LCD_INFO_ONCE(vdd, "is_gamma_mode2_support = %d\n", is_gamma_mode2_support);

	data = of_get_property(np, keystring, &len);
	if (!data) {
		LCD_DEBUG(vdd, "%d, Unable to read table %s ", __LINE__, keystring);
		return -EINVAL;
	} else
		LCD_INFO(vdd, "Success to read table %s\n", keystring);

	if (is_gamma_mode2_support)
		col_size = 5;
	else
		col_size = 4;

	if ((len % col_size) != 0) {
		LCD_ERR(vdd, "%d, Incorrect table entries for %s",
					__LINE__, keystring);
		return -EINVAL;
	}

	table->tab_size = len / (sizeof(int)*col_size);

	table->interpolation_cd = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->interpolation_cd)
		return -ENOMEM;
	table->cd = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->cd)
		goto error;
	table->scaled_idx = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->scaled_idx)
		goto error;
	table->idx = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->idx)
		goto error;
	table->from = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->from)
		goto error;
	table->end = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
	if (!table->end)
		goto error;

	if (is_gamma_mode2_support) {
		table->gm2_wrdisbv = kzalloc((sizeof(int) * table->tab_size), GFP_KERNEL);
		if (!table->gm2_wrdisbv)
			goto error;
	}
	for (i = 0 ; i < table->tab_size; i++) {
		table->idx[i] = be32_to_cpup(&data[data_offset++]);		/* 1st field => <idx> */
		table->from[i] = be32_to_cpup(&data[data_offset++]);		/* 2nd field => <from> */
		table->end[i] = be32_to_cpup(&data[data_offset++]);			/* 3rd field => <end> */
		if (is_gamma_mode2_support)
			table->gm2_wrdisbv[i] = be32_to_cpup(&data[data_offset++]);	/* field => <gm2_wrdisbv (panel_real_cd)> */

		table->cd[i] = be32_to_cpup(&data[data_offset++]);		/* 4th field => <candella> */
	}

	table->min_lv = table->from[0];
	table->max_lv = table->end[table->tab_size-1];
	if (table->max_lv > vdd->br_info.common_br.max_bl_level)
		vdd->br_info.common_br.max_bl_level = table->max_lv;

	LCD_INFO(vdd, "tab_size (%d), hbm min/max lv (%d/%d), max_bl_level(%d)\n",
			table->tab_size, table->min_lv, table->max_lv, vdd->br_info.common_br.max_bl_level);

	return 0;
error:
	kfree(table->cd);
	kfree(table->idx);
	kfree(table->from);
	kfree(table->end);
	if (is_gamma_mode2_support)
		kfree(table->gm2_wrdisbv);

	return -ENOMEM;
}

int ss_parse_panel_table(struct samsung_display_driver_data *vdd,
		struct device_node *np,	void *tbl, char *keystring)
{
	struct cmd_map *table = (struct cmd_map *) tbl;
	const __be32 *data;
	int  data_offset, len = 0, i = 0;

	data = of_get_property(np, keystring, &len);
	if (!data) {
		LCD_DEBUG(vdd, "%d, Unable to read table %s\n", __LINE__, keystring);
		return -EINVAL;
	} else
		LCD_INFO(vdd, "Success to read table %s\n", keystring);

	if ((len % 2) != 0) {
		LCD_ERR(vdd, "%d, Incorrect table entries for %s",
					__LINE__, keystring);
		return -EINVAL;
	}

	table->size = len / (sizeof(int)*2);
	table->bl_level = kzalloc((sizeof(int) * table->size), GFP_KERNEL);
	if (!table->bl_level)
		return -ENOMEM;

	table->cmd_idx = kzalloc((sizeof(int) * table->size), GFP_KERNEL);
	if (!table->cmd_idx)
		goto error;

	data_offset = 0;
	for (i = 0 ; i < table->size; i++) {
		table->bl_level[i] = be32_to_cpup(&data[data_offset++]);
		table->cmd_idx[i] = be32_to_cpup(&data[data_offset++]);
	}

	return 0;
error:
	kfree(table->cmd_idx);

	return -ENOMEM;
}

/* check if specific command set (type) is for samsung style cmd */
bool is_ss_style_cmd(struct samsung_display_driver_data *vdd, int type)
{
	struct ss_cmd_set *ss_set = ss_get_ss_cmds(vdd, type);

	if (!ss_set) {
		LCD_ERR(vdd, "fail to get ss_cmd(%d)\n", type);
		return false;
	}

	if (vdd->support_ss_cmd && ss_set->is_ss_style_cmd)
		return true;
	else
		return false;
}

/* Return QCT cmd format */
struct dsi_panel_cmd_set *ss_get_cmds(struct samsung_display_driver_data *vdd, int type)
{
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);
	struct dsi_display_mode_priv_info *priv_info;
	struct dsi_panel_cmd_set *qc_set;
	int rev = vdd->panel_revision;
	int ss_cmd_type;

	/* save the mipi cmd type */
	vdd->cmd_type = type;

	/* QCT cmds */
	if (type < SS_DSI_CMD_SET_START) {
		if (!panel || !panel->cur_mode || !panel->cur_mode->priv_info) {
			LCD_ERR(vdd, "err: (%d) panel has no valid priv\n", type);
			return NULL;
		}

		priv_info = panel->cur_mode->priv_info;
		qc_set = &priv_info->cmd_sets[type];

		return qc_set;
	}

	/* SS cmds */
	if (is_ss_style_cmd(vdd, type)) {
		struct ss_cmd_set *ss_set = ss_get_ss_cmds(vdd, type);

		qc_set = &ss_set->base;
	} else {
		if (type == TX_AID_SUBDIVISION && vdd->br_info.common_br.pac)
			type = TX_PAC_AID_SUBDIVISION;

		if (type == TX_IRC_SUBDIVISION && vdd->br_info.common_br.pac)
			type = TX_PAC_IRC_SUBDIVISION;

		ss_cmd_type = type - SS_DSI_CMD_SET_START;
		qc_set = &vdd->dtsi_data.ss_cmd_sets[ss_cmd_type].base;

		if (qc_set->cmd_set_rev[rev])
			qc_set = (struct dsi_panel_cmd_set *) qc_set->cmd_set_rev[rev];
	}

	return qc_set;
}

int ss_send_cmd(struct samsung_display_driver_data *vdd,
		int type)
{
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);
	struct dsi_panel_cmd_set *set;
	bool is_vdd_locked = false;
	int rc = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null\n");
		return -ENODEV;
	}

	/* Make not to turn on the panel power when ub_con_det gpio is high (ub is not connected) */
	if (unlikely(vdd->is_factory_mode)) {
		if (ss_gpio_is_valid(vdd->ub_con_det.gpio) && ss_gpio_get_value(vdd, vdd->ub_con_det.gpio)) {
			LCD_INFO(vdd, "ub_con_det.gpio = %d\n", ss_gpio_get_value(vdd, vdd->ub_con_det.gpio));
			return -EAGAIN;
		}

		if (ss_gpio_is_valid(vdd->fg_err_gpio) && ss_gpio_get_value(vdd, vdd->fg_err_gpio)) {
			LCD_INFO(vdd, "fg_err_gpio val: %d\n", ss_gpio_get_value(vdd, vdd->fg_err_gpio));
			return -EAGAIN;
		}
	}

	if (!ss_panel_attach_get(vdd)) {
		LCD_INFO(vdd, "ss_panel_attach_get(%d) : %d\n",
				vdd->ndx, ss_panel_attach_get(vdd));
		return -EAGAIN;
	}

	/* Skip to lock vdd_lock for commands that has exclusive_pass token
	 * to prevent deadlock between vdd_lock and ex_tx_waitq.
	 * cmd_lock guarantees exclusive tx cmds without vdd_lock.
	 */
	set = ss_get_cmds(vdd, type);

	if (likely(!vdd->exclusive_tx.enable || !set->exclusive_pass)) {
		mutex_lock(&vdd->vdd_lock);
		is_vdd_locked = true;
	}

	if (ss_is_panel_off(vdd) || (vdd->panel_func.samsung_lvds_write_reg)) {
		LCD_INFO(vdd, "skip to tx cmd (%d), panel off (%d)\n",
				type, ss_is_panel_off(vdd));
		goto error;
	}

	if (vdd->debug_data->print_cmds)
		LCD_INFO(vdd, "Send cmd(%d): %s ++\n", type, ss_get_cmd_name(type));
	else
		LCD_DEBUG(vdd, "Send cmd(%d): %s ++\n", type, ss_get_cmd_name(type));

	/* In pm suspend status, it fails to control display clock.
	 * In result,
	 * - fails to transmit command to DDI.
	 * - sometimes causes unbalanced reference count of qc display clock.
	 * To prevent above issues, wait for pm resume before control display clock.
	 * You don't have to force to wake up PM system by calling pm_wakeup_ws_event().
	 * If cpu core reach to this code, it means interrupt or the other event is waking up
	 * PM system. So, it is enough just waiting until PM resume.
	 */
	if (ss_wait_for_pm_resume(vdd))
		goto error;

	/* To guarantee below 2 things in Command Mode panel
	 * 1. To prevent image(video) data tx, we need 2 TE wait
	 * 2. To make sure the timing of command tx, any command should not be interrupted
	 */
	if (vdd->support_optical_fingerprint && vdd->finger_mask_updated &&
			type == TX_BRIGHT_CTRL && panel->panel_mode == DSI_OP_CMD_MODE)
		ss_wait_for_te_gpio(vdd, 2, vdd->panel_hbm_entry_after_te, true);

	ss_wrapper_dsi_panel_tx_cmd_set(panel, type);

	if (vdd->debug_data->print_cmds)
		LCD_INFO(vdd, "Send cmd(%d): %s --\n", type, ss_get_cmd_name(type));
	else
		LCD_DEBUG(vdd, "Send cmd(%d): %s --\n", type, ss_get_cmd_name(type));

error:
	if (likely(is_vdd_locked))
		mutex_unlock(&vdd->vdd_lock);

	ss_print_rx_buf(vdd, type);

	return rc;
}

#include <linux/cpufreq.h>
#define CPUFREQ_MAX	100
#define CPUFREQ_ALT_HIGH	70
struct cluster_cpu_info {
	int cpu_man; /* cpu number that manages the policy */
	int max_freq;
};

static void __ss_set_cpufreq_cpu(struct samsung_display_driver_data *vdd,
				int enable, int cpu, int max_percent)
{
	struct cpufreq_policy *policy;
	static unsigned int org_min[NR_CPUS + 1];
	static unsigned int org_max[NR_CPUS + 1];

	policy = cpufreq_cpu_get(cpu);
	if (policy == NULL) {
		LCD_ERR(vdd, "policy is null..\n");
		return;
	}

	if (enable) {
		org_min[cpu] = policy->min;
		org_max[cpu] = policy->max;

		/* max_freq's unit is kHz, and it should not cause overflow.
		 * After applying user_policy, cpufreq driver will find closest
		 * frequency in freq_table, and set the frequency.
		 */
		policy->min = policy->max =
			policy->cpuinfo.max_freq * max_percent / 100;
	} else {
		policy->min = org_min[cpu];
		policy->max = org_max[cpu];
	}

	cpufreq_update_policy(cpu);
	cpufreq_cpu_put(policy);

	LCD_DEBUG(vdd, "en=%d, cpu=%d, per=%d, min=%d, org=(%d-%d)\n",
			enable, cpu, max_percent, policy->min,
			org_min[cpu], org_max[cpu]);
}

void ss_set_max_cpufreq(struct samsung_display_driver_data *vdd,
				int enable, enum mdss_cpufreq_cluster cluster)
{
	struct cpufreq_policy *policy;
	int cpu;
	struct cluster_cpu_info cluster_info[NR_CPUS];
	int cnt_cluster;
//	int bigp_cpu_man;
	int big_cpu_man = 0;
	int little_cpu_man = 0;
	int i = 0;

	//return;

	LCD_DEBUG(vdd, "en=%d, cluster=%d\n", enable, cluster);
	get_online_cpus();

	/* find clusters */
	cnt_cluster = 0;
	for_each_online_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		/* In big-little octa core system,
		 * cpu0 ~ cpu3 has same policy (policy0) and
		 * policy->cpu would be 0. (cpu0 manages policy0)
		 * cpu4 ~ cpu7 has same policy (policy4) and
		 * policy->cpu would be 4. (cpu4 manages policy0)
		 * In this case, cnt_cluster would be two, and
		 * cluster_info[0].cpu_man = 0, cluster_info[1].cpu_man = 4.
		 */
		if (policy && policy->cpu == cpu) {
			cluster_info[cnt_cluster].cpu_man = cpu;
			cluster_info[cnt_cluster].max_freq =
				policy->cpuinfo.max_freq;
			cnt_cluster++;
			//LCD_INFO(vdd, "cpu(%d)\n", cpu);
		}
		cpufreq_cpu_put(policy);
	}

	if (!cnt_cluster || cnt_cluster > 3) {
		LCD_ERR(vdd, "cluster count err (%d)\n", cnt_cluster);
		goto end;
	}

	if (cnt_cluster == 1) {
		/* single policy (none big-little case) */
		LCD_INFO(vdd, "cluster count is one...\n");

		if (cluster == CPUFREQ_CLUSTER_ALL)
			/* set all cores' freq to max*/
			__ss_set_cpufreq_cpu(vdd, enable,
					cluster_info[0].cpu_man, CPUFREQ_MAX);
		else
			/* set all cores' freq to max * 70 percent */
			__ss_set_cpufreq_cpu(vdd, enable,
				cluster_info[0].cpu_man, CPUFREQ_ALT_HIGH);
		goto end;
	}

#if 0
	/* big-little */
	if (cluster_info[0].max_freq > cluster_info[1].max_freq) {
		if (cluster_info[1].max_freq > cluster_info[2].max_freq) {
			/* 0 > 1 > 2 */
			big_cpu_man = cluster_info[0].cpu_man;
			little_cpu_man = cluster_info[2].cpu_man;
		}
		else if (cluster_info[2].max_freq > cluster_info[0].max_freq) {
			/* 2 > 0 > 1 */
			big_cpu_man = cluster_info[2].cpu_man;
			little_cpu_man = cluster_info[1].cpu_man;
		}
		else {
			/* 0 > 2 > 1 */
			big_cpu_man = cluster_info[0].cpu_man;
			little_cpu_man = cluster_info[1].cpu_man;
		}
	} else {
		if (cluster_info[0].max_freq > cluster_info[2].max_freq) {
			/* 1 > 0 > 2 */
			big_cpu_man = cluster_info[1].cpu_man;
			little_cpu_man = cluster_info[2].cpu_man;
		}
		else if (cluster_info[2].max_freq > cluster_info[1].max_freq) {
			/* 2 > 1 > 0 */
			big_cpu_man = cluster_info[2].cpu_man;
			little_cpu_man = cluster_info[0].cpu_man;
		}
		else {
			/* 1 > 2 > 0 */
			big_cpu_man = cluster_info[1].cpu_man;
			little_cpu_man = cluster_info[0].cpu_man;
		}
	}
#endif

	if (cluster == CPUFREQ_CLUSTER_BIG) {
		__ss_set_cpufreq_cpu(vdd, enable, big_cpu_man,
				CPUFREQ_MAX);
	} else if (cluster == CPUFREQ_CLUSTER_LITTLE) {
		__ss_set_cpufreq_cpu(vdd, enable, little_cpu_man,
				CPUFREQ_MAX);
	} else if  (cluster == CPUFREQ_CLUSTER_ALL) {
		for (i = 0; i < cnt_cluster; i++) {
			__ss_set_cpufreq_cpu(vdd, enable, cluster_info[i].cpu_man,
				CPUFREQ_MAX);
		}
		/*
		__ss_set_cpufreq_cpu(vdd, enable, big_cpu_man,
				CPUFREQ_MAX);
		__ss_set_cpufreq_cpu(vdd, enable, little_cpu_man,
				CPUFREQ_MAX);
		*/
	}

end:
	put_online_cpus();
}

void ss_set_max_mem_bw(struct samsung_display_driver_data *vdd, int enable)
{
	struct dsi_display *display = NULL;
	struct msm_drm_private *priv = NULL;
	static u64 cur_mnoc_ab, cur_mnoc_ib;

	display = GET_DSI_DISPLAY(vdd);
	if (IS_ERR_OR_NULL(display)) {
		LCD_ERR(vdd, "no display");
		return;
	}

	priv = display->drm_dev->dev_private;

	if (enable) {
		/* Save Current MNOC AB/IB Value */
		cur_mnoc_ab = priv->phandle.data_bus_handle[SDE_POWER_HANDLE_DBUS_ID_MNOC].in_ab_quota;
		cur_mnoc_ib = priv->phandle.data_bus_handle[SDE_POWER_HANDLE_DBUS_ID_MNOC].in_ib_quota;
		sde_power_data_bus_set_quota(&priv->phandle,
				SDE_POWER_HANDLE_DBUS_ID_MNOC,
				SDE_POWER_HANDLE_CONT_SPLASH_BUS_AB_QUOTA,
				SDE_POWER_HANDLE_CONT_SPLASH_BUS_IB_QUOTA);
	} else {
		/* Restore Last MNOC AB/IB Value */
		sde_power_data_bus_set_quota(&priv->phandle,
			SDE_POWER_HANDLE_DBUS_ID_MNOC,
			cur_mnoc_ab,
			cur_mnoc_ib);
	}
}

/* ss_set_exclusive_packet():
 * This shuold be called in ex_tx_lock lock.
 */
void ss_set_exclusive_tx_packet(
		struct samsung_display_driver_data *vdd,
		int cmd, int pass)
{
	struct dsi_panel_cmd_set *pcmds = NULL;

	pcmds = ss_get_cmds(vdd, cmd);
	pcmds->exclusive_pass = pass;
}

/*
 * ex_tx_lock should be locked before panel_lock if there is a dsi_panel_tx_cmd_set after panel_lock is locked.
 * because of there is a "ex_tx_lock -> panel lock" routine at the sequence of ss_gct_write.
 * So we need to add ex_tx_lock to protect all "dsi_panel_tx_cmd_set after panel_lock".
 */

void ss_set_exclusive_tx_lock_from_qct(struct samsung_display_driver_data *vdd, bool lock)
{
	if (lock)
		mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	else
		mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);
}

/**
 * controller have 4 registers can hold 16 bytes of rxed data
 * dcs packet: 4 bytes header + payload + 2 bytes crc
 * 1st read: 4 bytes header + 10 bytes payload + 2 crc
 * 2nd read: 14 bytes payload + 2 crc
 * 3rd read: 14 bytes payload + 2 crc
 */
#define MAX_LEN_RX_BUF	500
int ss_panel_data_read_no_gpara(struct samsung_display_driver_data *vdd,
		int type, u8 *buffer, int level_key)
{
	struct dsi_panel_cmd_set *set;
	char show_buffer[MAX_LEN_RX_BUF] = {0,};
	char temp_buffer[MAX_LEN_RX_BUF] = {0,};
	int orig_rx_len = 0;
	int orig_offset = 0;
	int pos = 0;
	int i;

	/* samsung mipi rx cmd feature supports only one command */
	set = ss_get_cmds(vdd, type);
	if (SS_IS_CMDS_NULL(set) || !ss_is_read_cmd(set)) {
		LCD_ERR(vdd, "invalid set(%d): %s\n", type, ss_get_cmd_name(type));
		return -EINVAL;
	}

	/* enable level key */
	if (level_key & LEVEL0_KEY)
		ss_send_cmd(vdd, TX_LEVEL0_KEY_ENABLE);
	if (level_key & LEVEL1_KEY)
		ss_send_cmd(vdd, TX_LEVEL1_KEY_ENABLE);
	if (level_key & LEVEL2_KEY)
		ss_send_cmd(vdd, TX_LEVEL2_KEY_ENABLE);
	if (level_key & POC_KEY)
		ss_send_cmd(vdd, TX_POC_KEY_ENABLE);

	orig_rx_len = set->cmds[0].msg.rx_len;
	orig_offset = set->read_startoffset;

	/* reset rx len/offset */
	set->cmds[0].msg.rx_len += orig_offset;
	set->read_startoffset = 0;

	set->cmds[0].msg.rx_buf = temp_buffer;

	LCD_DEBUG(vdd, "orig_rx_len (%d) , orig_offset (%d) \n", orig_rx_len, orig_offset);
	LCD_DEBUG(vdd, "new_rx_len (%ld) , new_offset (%d) \n", set->cmds[0].msg.rx_len, set->read_startoffset);

	/* RX */
	ss_send_cmd(vdd, type);

	/* copy to buffer from original offset */
	if (buffer)
		memcpy(buffer, temp_buffer + orig_offset, orig_rx_len);

	/* show buffer */
	if (buffer)
		for (i = 0; i < orig_rx_len; i++)
			pos += snprintf(show_buffer + pos, sizeof(show_buffer) - pos, "%02x ",
				buffer[i]);

	/* restore rx len/offset */
	set->cmds[0].msg.rx_len = orig_rx_len;
	set->read_startoffset = orig_offset;

	/* disable level key */
	if (level_key & POC_KEY)
		ss_send_cmd(vdd, TX_POC_KEY_DISABLE);
	if (level_key & LEVEL2_KEY)
		ss_send_cmd(vdd, TX_LEVEL2_KEY_DISABLE);
	if (level_key & LEVEL1_KEY)
		ss_send_cmd(vdd, TX_LEVEL1_KEY_DISABLE);
	if (level_key & LEVEL0_KEY)
		ss_send_cmd(vdd, TX_LEVEL0_KEY_DISABLE);

	if (type != RX_FLASH_GAMMA && type != RX_POC_READ)
		LCD_INFO(vdd, "[%d]%s, addr: 0x%02X, off: %d, len: %d, buf: %s\n",
				type, ss_get_cmd_name(type),
				set->cmds[0].ss_txbuf[0], orig_offset, orig_rx_len,
				show_buffer);

	return 0;
}

int ss_panel_data_read_gpara(struct samsung_display_driver_data *vdd,
		int type, u8 *buffer, int level_key)
{
	struct dsi_panel_cmd_set *set;
	struct dsi_panel_cmd_set *read_pos_cmd;
	static u8 rx_buffer[MAX_LEN_RX_BUF] = {0,};
	char show_buffer[MAX_LEN_RX_BUF] = {0,};
	char temp_buffer[MAX_LEN_RX_BUF] = {0,};
	char rx_addr = 0;
	int orig_rx_len = 0;
	int new_rx_len = 0;
	int orig_offset = 0;
	int new_offset = 0;
	int gpara_byte0, gpara_byte1 = 0;
	int loop_limit = 0;
	int copy_pos = 0;
	int pos = 0;
	int i, j;
	int read_pos_cmd_offset = 0;

	/* samsung mipi rx cmd feature supports only one command */
	set = ss_get_cmds(vdd, type);
	if (SS_IS_CMDS_NULL(set) || !ss_is_read_cmd(set)) {
		LCD_ERR(vdd, "invalid set(%d): %s\n", type, ss_get_cmd_name(type));
		return -EINVAL;
	}

	/* enable level key */
	if (level_key & LEVEL0_KEY)
		ss_send_cmd(vdd, TX_LEVEL0_KEY_ENABLE);
	if (level_key & LEVEL1_KEY)
		ss_send_cmd(vdd, TX_LEVEL1_KEY_ENABLE);
	if (level_key & LEVEL2_KEY)
		ss_send_cmd(vdd, TX_LEVEL2_KEY_ENABLE);
	if (level_key & POC_KEY)
		ss_send_cmd(vdd, TX_POC_KEY_ENABLE);

	set->cmds[0].msg.rx_buf = rx_buffer;

	/* B0h cmd */
	read_pos_cmd = ss_get_cmds(vdd, TX_REG_READ_POS);
	if (SS_IS_CMDS_NULL(read_pos_cmd)) {
		LCD_ERR(vdd, "No cmds for TX_REG_READ_POS.. \n");
		return -EINVAL;
	}

	rx_addr = set->cmds[0].ss_txbuf[0];
	orig_rx_len = set->cmds[0].msg.rx_len;
	orig_offset = new_offset = set->read_startoffset;
	if (vdd->two_byte_gpara) {
		gpara_byte0 = (orig_offset & 0xff00) >> 8;
		gpara_byte1 = orig_offset & 0xff;
	} else {
		gpara_byte0 = orig_offset;
	}

	loop_limit = (orig_rx_len + RX_SIZE_LIMIT - 1) / RX_SIZE_LIMIT;


	LCD_DEBUG(vdd, "orig_rx_len (%d) , orig_offset (%d) loop_limit (%d)\n", orig_rx_len, orig_offset, loop_limit);

	for (i = 0; i < loop_limit; i++) {
		read_pos_cmd_offset = 1;
		/* 1. Send gpara first with new offset */
		/* if samsung,two_byte_gpara is true,
		 * that means DDI uses two_byte_gpara
		 * to figure it, check cmd length
		 * 06 01 00 00 00 00 01 DA 01 00 : use 1byte gpara, length is 10
		 * 06 01 00 00 00 00 01 DA 01 00 00 : use 2byte gpara, length is 11
		*/
		if (vdd->two_byte_gpara) {
			gpara_byte0 = (new_offset & 0xff00) >> 8;
			gpara_byte1 = new_offset & 0xff;
			read_pos_cmd->cmds->ss_txbuf[read_pos_cmd_offset++] = gpara_byte0;
			read_pos_cmd->cmds->ss_txbuf[read_pos_cmd_offset++] = gpara_byte1;
		} else {
			gpara_byte0 = new_offset;
			read_pos_cmd->cmds->ss_txbuf[read_pos_cmd_offset++] = gpara_byte0;
		}
		/* set pointing gpara */
		if (vdd->pointing_gpara)
			read_pos_cmd->cmds->ss_txbuf[read_pos_cmd_offset] = rx_addr;

		ss_send_cmd(vdd, TX_REG_READ_POS);

		/* 2. Set new read length */
		new_rx_len = ((orig_rx_len - new_offset + orig_offset) < RX_SIZE_LIMIT) ?
						(orig_rx_len - new_offset + orig_offset) : RX_SIZE_LIMIT;
		set->cmds[0].msg.rx_len = new_rx_len;

		LCD_DEBUG(vdd, "new_offset (%d) new_rx_len (%d) \n", new_offset, new_rx_len);

		if (vdd->dtsi_data.samsung_anapass_power_seq) {
			/*
				some panel need to update read size by address.
				It means "address + read_size" is essential.
			*/
			if (set->cmds[0].msg.tx_len == 2) {
				set->cmds[0].ss_txbuf[1] = new_rx_len;
				LCD_DEBUG(vdd, "update address + read_size(%d) \n", new_rx_len);
			}
		}

		/* 3. RX */
		ss_send_cmd(vdd, type);

		/* copy to buffer */
		if (buffer) {
			memcpy(&buffer[copy_pos], rx_buffer, new_rx_len);
			copy_pos += new_rx_len;
		}

		/* snprint */
		memcpy(temp_buffer, set->cmds[0].msg.rx_buf, new_rx_len);
		for (j = 0; j < new_rx_len && pos < MAX_LEN_RX_BUF; j++) {
			pos += snprintf(show_buffer + pos, MAX_LEN_RX_BUF - pos, "%02x ",
				temp_buffer[j]);
		}

		/* 4. Set new read offset for gpara */
		new_offset += new_rx_len;

		if (new_offset - orig_offset >= orig_rx_len)
			break;
	}

	/* restore rx len */
	set->cmds[0].msg.rx_len = orig_rx_len;

	/* disable level key */
	if (level_key & POC_KEY)
		ss_send_cmd(vdd, TX_POC_KEY_DISABLE);
	if (level_key & LEVEL2_KEY)
		ss_send_cmd(vdd, TX_LEVEL2_KEY_DISABLE);
	if (level_key & LEVEL1_KEY)
		ss_send_cmd(vdd, TX_LEVEL1_KEY_DISABLE);
	if (level_key & LEVEL0_KEY)
		ss_send_cmd(vdd, TX_LEVEL0_KEY_DISABLE);

	if (type != RX_FLASH_GAMMA && type != RX_POC_READ)
		LCD_INFO(vdd, "[%d] %s, addr: 0x%02X, off: %d(0x%02X), len: %d, buf: %s\n",
				type, ss_get_cmd_name(type),
				set->cmds[0].ss_txbuf[0], orig_offset, orig_offset, orig_rx_len,
				show_buffer);

	return 0;
}

int ss_panel_data_read(struct samsung_display_driver_data *vdd,
		int type, u8 *buffer, int level_key)
{
	int ret;

	if (!ss_panel_attach_get(vdd)) {
		LCD_ERR(vdd, "ss_panel_attach_get(%d) : %d\n",
				vdd->ndx, ss_panel_attach_get(vdd));
		return -EPERM;
	}

	if (ss_is_panel_off(vdd)) {
		LCD_ERR(vdd, "skip to rx cmd (%d), panel off (%d)\n",
				type, ss_is_panel_off(vdd));
		return -EPERM;
	}

	/* To block read operation at esd-recovery */
	if (vdd->panel_dead) {
		LCD_ERR(vdd, "esd recovery, skip %s\n", ss_get_cmd_name(type));
		return -EPERM;
	}

	if (is_ss_style_cmd(vdd, type)) {
		struct ss_cmd_set *ss_set = ss_get_ss_cmds(vdd, type);

		if (ss_set->count == 1) {
			if (level_key & LEVEL0_KEY)
				ss_send_cmd(vdd, TX_LEVEL0_KEY_ENABLE);
			if (level_key & LEVEL1_KEY)
				ss_send_cmd(vdd, TX_LEVEL1_KEY_ENABLE);
			if (level_key & LEVEL2_KEY)
				ss_send_cmd(vdd, TX_LEVEL2_KEY_ENABLE);
			if (level_key & POC_KEY)
				ss_send_cmd(vdd, TX_POC_KEY_ENABLE);

			ss_send_cmd(vdd, type);

			if (level_key & POC_KEY)
				ss_send_cmd(vdd, TX_POC_KEY_DISABLE);
			if (level_key & LEVEL2_KEY)
				ss_send_cmd(vdd, TX_LEVEL2_KEY_DISABLE);
			if (level_key & LEVEL1_KEY)
				ss_send_cmd(vdd, TX_LEVEL1_KEY_DISABLE);
			if (level_key & LEVEL0_KEY)
				ss_send_cmd(vdd, TX_LEVEL0_KEY_DISABLE);

			memcpy(buffer, ss_set->cmds[0].rxbuf, ss_set->cmds[0].rx_len);

			return 0;
		} else if (ss_set->count > 0) {
			/* multiple rx cmds should be called in ss_send_cmd */
			LCD_ERR(vdd, "multiple rx cmds(cnt%d)\n", ss_set->count);
		}
	}

	if (vdd->gpara)
		ret = ss_panel_data_read_gpara(vdd, type, buffer, level_key);
	else
		ret = ss_panel_data_read_no_gpara(vdd, type, buffer, level_key);

	return ret;
}

struct STM_REG_OSSET stm_offset_list[] = {
	{.name = "stm_ctrl_en=", 		.offset = offsetof(struct STM_CMD, STM_CTRL_EN)},
	{.name = "stm_max_opt=", 		.offset = offsetof(struct STM_CMD, STM_MAX_OPT)},
	{.name = "stm_default_opt=", 	.offset = offsetof(struct STM_CMD, STM_DEFAULT_OPT)},
	{.name = "stm_dim_step=", 		.offset = offsetof(struct STM_CMD, STM_DIM_STEP)},
	{.name = "stm_frame_period=",	.offset = offsetof(struct STM_CMD, STM_FRAME_PERIOD)},
	{.name = "stm_min_sect=", 		.offset = offsetof(struct STM_CMD, STM_MIN_SECT)},
	{.name = "stm_pixel_period=",	.offset = offsetof(struct STM_CMD, STM_PIXEL_PERIOD)},
	{.name = "stm_line_period=", 	.offset = offsetof(struct STM_CMD, STM_LINE_PERIOD)},
	{.name = "stm_min_move=", 		.offset = offsetof(struct STM_CMD, STM_MIN_MOVE)},
	{.name = "stm_m_thres=", 		.offset = offsetof(struct STM_CMD, STM_M_THRES)},
	{.name = "stm_v_thres=", 		.offset = offsetof(struct STM_CMD, STM_V_THRES)},
};

int ss_stm_set_cmd_offset(struct STM_CMD *cmd, char* p)
{
	int list_size = ARRAY_SIZE(stm_offset_list);
	const char *name;
	int i, val;
	int *offset;

	for (i = 0; i < list_size; i++) {
		name = stm_offset_list[i].name;
		if (!strncmp(name, p, strlen(name))) {
			sscanf(p + strlen(name), "%d", &val);
			offset = (int *)((void*)cmd + stm_offset_list[i].offset);
			*offset = val;
			return 0;
		}
	}

	return -1;
}

void print_stm_cmd(struct samsung_display_driver_data *vdd)
{
	struct STM_CMD cmd = vdd->stm.cur_cmd;

	LCD_INFO(vdd, "CTRL_EN(%d) MAX_OPT(%d) DEFAULT_OPT(%d) DIM_STEP(%d)\n",
		cmd.STM_CTRL_EN, cmd.STM_MAX_OPT, cmd.STM_DEFAULT_OPT, cmd.STM_DIM_STEP);
	LCD_INFO(vdd, "FRAME_PERIOD(%d) MIN_SECT(%d) PIXEL_PERIOD(%d) LINE_PEROID(%d) \n",
		cmd.STM_FRAME_PERIOD, cmd.STM_MIN_SECT, cmd.STM_PIXEL_PERIOD, cmd.STM_LINE_PERIOD);
	LCD_INFO(vdd, "MIN_MOVE(%d) M_THRES(%d) V_THRES(%d)\n",
		cmd.STM_MIN_MOVE, cmd.STM_M_THRES, cmd.STM_V_THRES);
}

/**
 * ss_get_stm_orig_cmd - get original stm cmd from panel dtsi.
 */
int ss_get_stm_orig_cmd(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds = NULL;
	struct STM_CMD *cmd = &vdd->stm.orig_cmd;
	u8 *cmd_pload;
	int cmd_len;
	char buf[256];
	int i, len = 0;

	pcmds = ss_get_cmds(vdd, TX_STM_ENABLE);
	if (SS_IS_CMDS_NULL(pcmds) || pcmds->count < 2) {
		LCD_ERR(vdd, "No cmds for TX_STM_ENABLE..\n");
		return -EINVAL;
	}

	cmd_pload = pcmds->cmds[1].ss_txbuf;
	cmd_len = pcmds->cmds[1].msg.tx_len;

	if (cmd_len < 10) {
		LCD_ERR(vdd, "invalid tx_len(%d)\n", cmd_len);
		return -EINVAL;
	}

	cmd->STM_CTRL_EN = (cmd_pload[1] & 0x01) ? 1 : 0;
	cmd->STM_MAX_OPT = (cmd_pload[3] & 0xF0) >> 4;

	cmd->STM_DEFAULT_OPT = (cmd_pload[3] & 0x0F);

	cmd->STM_DIM_STEP = (cmd_pload[4] & 0x0F);

	cmd->STM_FRAME_PERIOD = (cmd_pload[5] & 0xF0) >> 4;
	cmd->STM_MIN_SECT = (cmd_pload[5] & 0x0F);

	cmd->STM_PIXEL_PERIOD = (cmd_pload[6] & 0xF0) >> 4;
	cmd->STM_LINE_PERIOD = (cmd_pload[6] & 0x0F);

	cmd->STM_MIN_MOVE = cmd_pload[7];
	cmd->STM_M_THRES = cmd_pload[8];
	cmd->STM_V_THRES = cmd_pload[9];

	/* init current cmd with origianl cmd */
	memcpy(&vdd->stm.cur_cmd, cmd, sizeof(struct STM_CMD));

	print_stm_cmd(vdd);

	for (i = 0; i < cmd_len; i++)
		len += snprintf(buf + len, sizeof(buf) - len,
						"%02x ", cmd_pload[i]);
	LCD_INFO(vdd, "cmd[%d] : %s\n", cmd_len, buf);

	return 1;
}

/**
 * ss_stm_set_cmd - set stm mipi cmd using STM_CMD para.
 * stm_cmd : new stm cmd to update.
 */
void ss_stm_set_cmd(struct samsung_display_driver_data *vdd, struct STM_CMD *cmd)
{
	struct dsi_panel_cmd_set *pcmds = NULL;
	int cmd_len;
	u8 *cmd_pload;
	char buf[256];
	int i, len = 0;

	pcmds = ss_get_cmds(vdd, TX_STM_ENABLE);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_STM_ENABLE..\n");
		return;
	}

	cmd_pload = pcmds->cmds[1].ss_txbuf;
	cmd_len = pcmds->cmds[1].msg.tx_len;

	cmd_pload[1] = cmd->STM_CTRL_EN ? 0x01 : 0x00;

	cmd_pload[3] = ((cmd->STM_MAX_OPT & 0x0F) << 4);
	cmd_pload[3] |= (cmd->STM_DEFAULT_OPT & 0x0F);

	cmd_pload[4] |= (cmd->STM_DIM_STEP & 0x0F);

	cmd_pload[5] = ((cmd->STM_FRAME_PERIOD & 0x0F) << 4);
	cmd_pload[5] |= (cmd->STM_MIN_SECT & 0x0F);

	cmd_pload[6] = ((cmd->STM_PIXEL_PERIOD & 0x0F) << 4);
	cmd_pload[6] |= (cmd->STM_LINE_PERIOD & 0x0F);

	cmd_pload[7] = cmd->STM_MIN_MOVE & 0xFF;
	cmd_pload[8] = cmd->STM_M_THRES & 0xFF;
	cmd_pload[9] = cmd->STM_V_THRES & 0xFF;

	for (i = 0; i < cmd_len; i++)
		len += snprintf(buf + len, sizeof(buf) - len,
						"%02x ", cmd_pload[i]);
	LCD_INFO(vdd, "cmd[%d] : %s\n", cmd_len, buf);

	/* reset current stm cmds */
	memcpy(&vdd->stm.cur_cmd, cmd, sizeof(struct STM_CMD));

	return;
}

/* Return: zero on error, 1 on success */
int ss_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	int lcd_id_cmdline;

	if (!ss_panel_attach_get(vdd)) {
		LCD_ERR(vdd, "ss_panel_attach_get NG\n");
		return false;
	}

	LCD_INFO(vdd, "+\n");

	ss_set_vrr_switch(vdd, false);

	/* At this time, it already enabled SDE clock/power and  display power.
	 * It is possible to send mipi comamnd to display.
	 * To send mipi command, like mdnie setting or brightness setting,
	 * change panel_state: PANEL_PWR_OFF -> PANEL_PWR_ON_READY, here.
	 */
	ss_set_panel_state(vdd, PANEL_PWR_ON_READY);

	vdd->display_status_dsi.disp_on_pre = 1;

	if (unlikely(!vdd->esd_recovery.esd_recovery_init))
		ss_event_esd_recovery_init(vdd, 0, NULL);

	if (unlikely(vdd->is_factory_mode &&
			vdd->dtsi_data.samsung_support_factory_panel_swap)) {
		/* LCD ID read every wake_up time incase of factory binary */
		vdd->manufacture_id_dsi = PBA_ID;

		/* Factory Panel Swap*/
		vdd->manufacture_date_loaded_dsi = 0;
		vdd->ddi_id_loaded_dsi = 0;
		vdd->module_info_loaded_dsi = 0;
		vdd->cell_id_loaded_dsi = 0;
		vdd->octa_id_loaded_dsi = 0;
		vdd->br_info.hbm_loaded_dsi = 0;
		vdd->mdnie_loaded_dsi = 0;
		vdd->br_info.smart_dimming_loaded_dsi = 0;
		vdd->br_info.smart_dimming_hmt_loaded_dsi = 0;
		vdd->br_info.table_interpolation_loaded = 0;
		vdd->udc.read_done = 0;

		LCD_INFO(vdd, "set ddi id (0x%x)\n", vdd->manufacture_id_dsi);
	}

	if (vdd->manufacture_id_dsi == PBA_ID || vdd->manufacture_id_dsi == -EINVAL) {
		u8 recv_buf[3];

		LCD_INFO(vdd, "invalid ddi id from cmdline(0x%x), read ddi id\n", vdd->manufacture_id_dsi);

		/*
		*	At this time, panel revision it not selected.
		*	So last index(SUPPORT_PANEL_REVISION-1) used.
		*/
		vdd->panel_revision = SUPPORT_PANEL_REVISION-1;

		/*
		*	Some panel needs to update register at init time to read ID & MTP
		*	Such as, dual-dsi-control or sleep-out so on.
		*/
		if ((ss_get_cmds(vdd, RX_MANUFACTURE_ID)->count)) {
			ss_panel_data_read(vdd, RX_MANUFACTURE_ID,
					recv_buf, LEVEL1_KEY);
		} else {
			LCD_INFO(vdd, "manufacture_read_pre_tx_cmds\n");
			ss_send_cmd(vdd, TX_MANUFACTURE_ID_READ_PRE);

			ss_panel_data_read(vdd, RX_MANUFACTURE_ID0,
					recv_buf, LEVEL1_KEY);
			ss_panel_data_read(vdd, RX_MANUFACTURE_ID1,
					recv_buf + 1, LEVEL1_KEY);
			ss_panel_data_read(vdd, RX_MANUFACTURE_ID2,
					recv_buf + 2, LEVEL1_KEY);

			ss_send_cmd(vdd, TX_MANUFACTURE_ID_READ_POST);
		}

		vdd->manufacture_id_dsi =
			(recv_buf[0] << 16) | (recv_buf[1] << 8) | recv_buf[2];
		LCD_INFO(vdd, "manufacture id: 0x%x\n", vdd->manufacture_id_dsi);

		/* compare lcd_id from bootloader and kernel side for debugging */
		if (likely(!(vdd->is_factory_mode &&
				vdd->dtsi_data.samsung_support_factory_panel_swap))) {
			if (vdd->ndx == PRIMARY_DISPLAY_NDX)
				lcd_id_cmdline = get_lcd_attached("GET");
			else
				lcd_id_cmdline = get_lcd_attached_secondary("GET");

			if (vdd->manufacture_id_dsi != lcd_id_cmdline)
				LCD_ERR(vdd, "LCD ID is changed (0x%X -> 0x%X)\n",
						lcd_id_cmdline, vdd->manufacture_id_dsi);
		}

		/* Panel revision selection */
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_revision))
			LCD_ERR(vdd, "no panel_revision_selection_error function\n");
		else
			vdd->panel_func.samsung_panel_revision(vdd);

		LCD_INFO(vdd, "Panel_Revision = %c %d\n", vdd->panel_revision + 'A', vdd->panel_revision);
	}

	/* Read panel status to check panel is ok from bootloader */
	if (!vdd->read_panel_status_from_lk) {
		ss_read_ddi_debug_reg(vdd);
		vdd->read_panel_status_from_lk = 1;
	}

	if (vdd->skip_read_on_pre) {
		LCD_INFO(vdd, "Skip read operation in on_pre\n");
		goto skip_read;
	}

	/* Module info */
	if (!vdd->module_info_loaded_dsi) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_module_info_read))
			LCD_INFO(vdd, "no samsung_module_info_read function\n");
		else
			vdd->module_info_loaded_dsi = vdd->panel_func.samsung_module_info_read(vdd);
	}

	/* Manufacture date */
	if (!vdd->manufacture_date_loaded_dsi) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_manufacture_date_read))
			LCD_INFO(vdd, "no samsung_manufacture_date_read function\n");
		else
			vdd->manufacture_date_loaded_dsi = vdd->panel_func.samsung_manufacture_date_read(vdd);
	}

	/* DDI ID */
	if (!vdd->ddi_id_loaded_dsi) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_ddi_id_read))
			LCD_INFO(vdd, "no samsung_ddi_id_read function\n");
		else
			vdd->ddi_id_loaded_dsi = vdd->panel_func.samsung_ddi_id_read(vdd);
	}

	/* ELVSS read */
	if (!vdd->br_info.elvss_loaded_dsi) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_elvss_read))
			LCD_INFO(vdd, "no samsung_elvss_read function\n");
		else
			vdd->br_info.elvss_loaded_dsi = vdd->panel_func.samsung_elvss_read(vdd);
	}

	/* IRC read */
	if (!vdd->br_info.irc_loaded_dsi) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_irc_read))
			LCD_INFO(vdd, "no samsung_irc_read function\n");
		else
			vdd->br_info.irc_loaded_dsi = vdd->panel_func.samsung_irc_read(vdd);
	}

	/* HBM */
	if (!vdd->br_info.hbm_loaded_dsi) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_hbm_read))
			LCD_INFO(vdd, "no samsung_hbm_read function\n");
		else
			vdd->br_info.hbm_loaded_dsi = vdd->panel_func.samsung_hbm_read(vdd);
	}

	/* MDNIE X,Y (1.Manufacture Date -> 2.MDNIE X,Y -> 3.Cell ID -> 4.OCTA ID) */
	if (!vdd->mdnie_loaded_dsi && vdd->mdnie.support_mdnie) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_mdnie_read))
			LCD_INFO(vdd, "no samsung_mdnie_read function\n");
		else
			vdd->mdnie_loaded_dsi = vdd->panel_func.samsung_mdnie_read(vdd);
	}

	/* Panel Unique Cell ID (1.Manufacture Date -> 2.MDNIE X,Y -> 3.Cell ID -> 4.OCTA ID) */
	if (!vdd->cell_id_loaded_dsi) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_cell_id_read))
			LCD_INFO(vdd, "no samsung_cell_id_read function\n");
		else
			vdd->cell_id_loaded_dsi = vdd->panel_func.samsung_cell_id_read(vdd);
	}

	/* Panel Unique OCTA ID (1.Manufacture Date -> 2.MDNIE X,Y -> 3.Cell ID -> 4.OCTA ID) */
	if (!vdd->octa_id_loaded_dsi) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_octa_id_read))
			LCD_INFO(vdd, "no samsung_octa_id_read function\n");
		else
			vdd->octa_id_loaded_dsi = vdd->panel_func.samsung_octa_id_read(vdd);
	}

	/* Smart dimming */
	if (vdd->br_info.flash_gamma_support && !vdd->br_info.smart_dimming_loaded_dsi) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_smart_dimming_init)) {
			LCD_ERR(vdd, "no samsung_smart_dimming_init function\n");
		} else {
			int count;
			LCD_ERR(vdd, "br_info.br_tbl_count %d \n", vdd->br_info.br_tbl_count);

			for (count = 0; count < vdd->br_info.br_tbl_count; count++) {
				struct brightness_table *br_tbl = &vdd->br_info.br_tbl[count];

				vdd->br_info.smart_dimming_loaded_dsi =
					vdd->panel_func.samsung_smart_dimming_init(vdd, br_tbl);
			}
		}
	}

	/* Smart dimming for hmt */
	if (vdd->br_info.flash_gamma_support && vdd->hmt.is_support) {
		if (!vdd->br_info.smart_dimming_hmt_loaded_dsi) {
			if (IS_ERR_OR_NULL(vdd->panel_func.samsung_smart_dimming_hmt_init))
				LCD_ERR(vdd, "no samsung_smart_dimming_hmt_init function\n");
			else
				vdd->br_info.smart_dimming_hmt_loaded_dsi = vdd->panel_func.samsung_smart_dimming_hmt_init(vdd);
		}
	}

	/* copr */
	if (!vdd->copr_load_init_cmd && vdd->copr.copr_on)
		vdd->copr_load_init_cmd = ss_get_copr_orig_cmd(vdd);

	/* stm */
	if (!vdd->stm_load_init_cmd)
		vdd->stm_load_init_cmd = ss_get_stm_orig_cmd(vdd);

	/* gamma flash */
	if (vdd->br_info.flash_gamma_support &&
			!vdd->br_info.support_early_gamma_flash &&
			!vdd->br_info.flash_gamma_init_done) {

		if (!strcmp(vdd->br_info.flash_gamma_type, "flash"))
			__flash_br(vdd);
		else if (!strcmp(vdd->br_info.flash_gamma_type, "table"))
			__table_br(vdd);
	}

	/* gamma mode2 and HOP display */
	if (vdd->br_info.common_br.gamma_mode2_support) {
		struct flash_gm2 *gm2_table = &vdd->br_info.gm2_table;

		if (gm2_table->mtp_one_vbias_mode &&
				(vdd->is_factory_mode || !gm2_table->flash_gm2_init_done)) {
			/* Read one vbias mode from MTP.
			 * Factory binary has panel swap test. For that case, read vbias MTP data
			 * in every display on sequence.
			 */
			LCD_INFO(vdd, "read vbias mtp data\n");
			ss_panel_data_read(vdd, RX_VBIAS_MTP, gm2_table->mtp_one_vbias_mode, LEVEL1_KEY);
		}

		if (vdd->panel_func.samsung_gm2_ddi_flash_prepare &&
				!gm2_table->flash_gm2_init_done) {
			LCD_INFO(vdd, "init gamma mode2 ddi flash\n");
			vdd->panel_func.samsung_gm2_ddi_flash_prepare(vdd);
			gm2_table->flash_gm2_init_done = true;
		}

		if (vdd->panel_func.samsung_gm2_gamma_comp_init &&
				!vdd->br_info.gm2_mtp.mtp_gm2_init_done) {
			LCD_INFO(vdd, "init gamma mode2 gamma compensation\n");
			vdd->panel_func.samsung_gm2_gamma_comp_init(vdd);
			vdd->br_info.gm2_mtp.mtp_gm2_init_done = true;
		}

	}

	/* table itp if it failed gamma flash */
	if (vdd->br_info.flash_gamma_support && !vdd->br_info.table_interpolation_loaded) {
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_interpolation_init)
			&& !vdd->br_info.flash_gamma_init_done) {
			int count;

			LCD_INFO(vdd, "flash interpolation is not done, use table interpolation !\n");
			for (count = 0; count < vdd->br_info.br_tbl_count; count++) {
				struct brightness_table *br_tbl = &vdd->br_info.br_tbl[count];

				table_br_func(vdd, br_tbl);
				vdd->br_info.table_interpolation_loaded =
					vdd->panel_func.samsung_interpolation_init(vdd,	br_tbl, TABLE_INTERPOLATION);
			}
			vdd->br_info.flash_gamma_init_done = true;
		}

		/* Even though flash interpolation was done, use table gamma for hmd.
		 * max mtp gamma for hmd is wrong from SDC.
		 */
		if (!IS_ERR_OR_NULL(vdd->panel_func.force_use_table_for_hmd)
			&& !vdd->br_info.force_use_table_for_hmd_done
			&& vdd->br_info.flash_gamma_init_done) {
			int count;

			for (count = 0; count < vdd->br_info.br_tbl_count; count++) {
				struct brightness_table *br_tbl = &vdd->br_info.br_tbl[count];
				vdd->panel_func.force_use_table_for_hmd(vdd, br_tbl);
			}

			vdd->br_info.force_use_table_for_hmd_done = true;
		}
	}

	/* UDC */
	if (vdd->panel_func.read_udc_data && !vdd->udc.read_done)
		vdd->panel_func.read_udc_data(vdd);

skip_read:

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_on_pre))
		vdd->panel_func.samsung_panel_on_pre(vdd);

	if (!vdd->samsung_splash_enabled)
		ss_send_cmd(vdd, TX_ON_PRE_SEQ);
	else
		LCD_INFO(vdd, "splash booting.. do not send ON_PRE_SEQ..\n");

	LCD_INFO(vdd, "-\n");

	return true;
}

int ss_panel_on_post(struct samsung_display_driver_data *vdd)
{
	if (!ss_panel_attach_get(vdd)) {
		LCD_ERR(vdd, "ss_panel_attach_get NG\n");
		return false;
	}

	/* Ignore dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ON).
	 * Instead, send TX_DSI_CMD_SET_ON in ss_panel_on_post(), here.
	 */
	if (is_ss_style_cmd(vdd, TX_DSI_CMD_SET_ON)) {
		LCD_INFO(vdd, "tx ss on_cmd\n");
		ss_send_cmd(vdd, TX_DSI_CMD_SET_ON);
	}

	LCD_INFO(vdd, "+\n");

	if (vdd->support_cabc && !vdd->br_info.is_hbm)
		ss_cabc_update(vdd);
	else if (vdd->br_info.ss_panel_tft_outdoormode_update && vdd->br_info.is_hbm)
		vdd->br_info.ss_panel_tft_outdoormode_update(vdd);
	else if (vdd->support_cabc && vdd->br_info.is_hbm)
		ss_tft_autobrightness_cabc_update(vdd);

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_on_post))
		vdd->panel_func.samsung_panel_on_post(vdd);

	if (ss_is_bl_dcs(vdd)) {
		struct backlight_device *bd = GET_SDE_BACKLIGHT_DEVICE(vdd);

		/* In case of backlight update in panel off,
		 * dsi_display_set_backlight() returns error
		 * without updating vdd->br_info.common_br.bl_level.
		 * Update bl_level from bd->props.brightness.
		 */
		if (bd && vdd->br_info.common_br.bl_level != bd->props.brightness) {
			LCD_INFO(vdd, "update bl_level: %d -> %d\n",
				vdd->br_info.common_br.bl_level, bd->props.brightness);
			vdd->br_info.common_br.bl_level = bd->props.brightness;
		}

		ss_brightness_dcs(vdd, USE_CURRENT_BL_LEVEL, BACKLIGHT_NORMAL);
	}

	if (vdd->mdnie.support_mdnie) {
		vdd->mdnie.lcd_on_notifiy = true;
		update_dsi_tcon_mdnie_register(vdd);
		if (vdd->mdnie.support_trans_dimming)
			vdd->mdnie.disable_trans_dimming = false;
	}

	vdd->display_status_dsi.wait_disp_on = true;
	vdd->display_status_dsi.wait_actual_disp_on = true;
	vdd->panel_lpm.need_self_grid = true;

	/* in case the code that display_enabled is set to true in bridge_enable function is not ported */
	vdd->display_enabled = true;

	if (vdd->dyn_mipi_clk.is_support) {
		LCD_INFO(vdd, "FFC Setting for Dynamic MIPI Clock\n");
		ss_send_cmd(vdd, TX_FFC);
	}

	ss_send_cmd(vdd, TX_COPR_ENABLE);

	if (unlikely(vdd->is_factory_mode)) {
		ss_send_cmd(vdd, TX_FD_ON);
		/*
		* 1Frame Delay(33.4ms - 30FPS) Should be added for the project that needs FD cmd
		* Swire toggled at TX_FD_OFF, TX_LPM_ON, TX_FD_ON, TX_LPM_OFF.
		*/
		if(!SS_IS_CMDS_NULL(ss_get_cmds(vdd, TX_FD_ON)))
			usleep_range(34*1000, 34*1000);
	}

	/*
	 * ESD Enable should be called after all the cmd's tx is done.
	 * Some command may occur unexpected esd detect because of EL_ON signal control
	 */
	if (ss_is_esd_check_enabled(vdd)) {
		if (vdd->esd_recovery.esd_irq_enable)
			vdd->esd_recovery.esd_irq_enable(true, true, (void *)vdd);
	}

	ss_set_panel_state(vdd, PANEL_PWR_ON);

	if (vdd->poc_driver.check_read_case)
		vdd->poc_driver.read_case = vdd->poc_driver.check_read_case(vdd);

	if (ss_gpio_is_valid(vdd->ub_con_det.gpio) && unlikely(vdd->is_factory_mode))
		ss_is_ub_connected(vdd);

	LCD_INFO(vdd, "-\n");

	return true;
}

int ss_panel_off_pre(struct samsung_display_driver_data *vdd)
{
	int ret = 0;

	LCD_INFO(vdd, "+\n");

	ss_read_ddi_debug_reg(vdd);

	if (vdd->panel_func.samsung_display_on_post_debug)
		vdd->panel_func.samsung_display_on_post_debug(vdd);

	/* self mask checksum read */
	if (vdd->self_disp.self_display_debug)
		vdd->self_disp.self_display_debug(vdd);

	if (ss_is_esd_check_enabled(vdd)) {
		vdd->esd_recovery.is_wakeup_source = false;
		if (vdd->esd_recovery.esd_irq_enable)
			vdd->esd_recovery.esd_irq_enable(false, true, (void *)vdd);
		vdd->esd_recovery.is_enabled_esd_recovery = false;
	}

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_off_pre))
		vdd->panel_func.samsung_panel_off_pre(vdd);

	/* Use lpm_lock to prevent
	 * ss_panel_off_pre makes vdd->panel_state PANEL_PWR_OFF
	 * while ss_panel_lpm_ctrl is being performed.
	 *
	 * Otherwise, ss_panel_lpm_ctrl will make vdd->panel_state PANEL_PWR_LPM,
	 * even though it is a physically display off state.
	 */
	mutex_lock(&vdd->panel_lpm.lpm_lock);
	mutex_lock(&vdd->vdd_lock);
	ss_set_panel_state(vdd, PANEL_PWR_OFF);
	mutex_unlock(&vdd->vdd_lock);
	mutex_unlock(&vdd->panel_lpm.lpm_lock);

	LCD_INFO(vdd, "-\n");

	return ret;
}

/*
 * Do not call ss_send_cmd() or ss_panel_data_read() here.
 * Any MIPI Tx/Rx can not be alowed in here.
 */
int ss_panel_off_post(struct samsung_display_driver_data *vdd)
{
	int ret = 0;

	LCD_INFO(vdd, "+\n");

	vdd->display_on = false;

	if (vdd->mdnie.support_trans_dimming)
		vdd->mdnie.disable_trans_dimming = true;

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_off_post))
		vdd->panel_func.samsung_panel_off_post(vdd);

	/* Reset Self Display Status */
	if (vdd->self_disp.reset_status)
		vdd->self_disp.reset_status(vdd);

	if (vdd->finger_mask)
		vdd->finger_mask = false;

	/* To prevent panel off without finger off */
	if (vdd->br_info.common_br.finger_mask_hbm_on)
		vdd->br_info.common_br.finger_mask_hbm_on = false;

	LCD_INFO(vdd, "- : mdp underrun: %d\n", vdd->cnt_mdp_clk_underflow);
	SS_XLOG(vdd->cnt_mdp_clk_underflow);

	return ret;
}

int ss_panel_power_on_pre(struct samsung_display_driver_data *vdd)
{
	int ret = 0;

	LCD_INFO(vdd, "+\n");

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_power_on_pre))
		vdd->panel_func.samsung_panel_power_on_pre(vdd);

	LCD_INFO(vdd, "-\n");

	return ret;
}

int ss_panel_power_off_post(struct samsung_display_driver_data *vdd)
{
	int ret = 0;

	LCD_INFO(vdd, "+\n");

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_power_off_post))
		vdd->panel_func.samsung_panel_power_off_post(vdd);

	LCD_INFO(vdd, "-\n");

	return ret;
}

int ss_frame_delay(int fps, int frame)
{
	int delay_us = (1000000 / fps) + 1000; /* 1ms dummy */
	int totla_delay_us = delay_us * frame;

	return totla_delay_us;
}

/*************************************************************
*
*		TFT BACKLIGHT GPIO FUNCTION BELOW.
*
**************************************************************/
int ss_backlight_tft_request_gpios(struct samsung_display_driver_data *vdd)
{
	int rc = 0, i;
	/*
	 * gpio_name[] named as gpio_name + num(recomend as 0)
	 * because of the num will increase depend on number of gpio
	 */
	char gpio_name[17] = "disp_bcklt_gpio0";
	static u8 gpio_request_status = -EINVAL;

	if (!gpio_request_status)
		goto end;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data pinfo : 0x%zx\n", (size_t)vdd);
		goto end;
	}

	for (i = 0; i < MAX_BACKLIGHT_TFT_GPIO; i++) {
		if (ss_gpio_is_valid(vdd->dtsi_data.backlight_tft_gpio[i])) {
			rc = gpio_request(vdd->dtsi_data.backlight_tft_gpio[i],
							gpio_name);
			if (rc) {
				LCD_ERR(vdd, "request %s failed, rc=%d\n", gpio_name, rc);
				goto tft_backlight_gpio_err;
			}
		}
	}

	gpio_request_status = rc;
end:
	return rc;
tft_backlight_gpio_err:
	if (i) {
		do {
			if (ss_gpio_is_valid(vdd->dtsi_data.backlight_tft_gpio[i]))
				gpio_free(vdd->dtsi_data.backlight_tft_gpio[i--]);
			LCD_INFO(vdd, "i = %d\n", i);
		} while (i > 0);
	}

	return rc;
}

#if 0 // not_used
int ss_backlight_tft_gpio_config(struct samsung_display_driver_data *vdd, int enable)
{
	int ret = 0, i = 0, add_value = 1;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data pinfo : 0x%zx\n",  (size_t)vdd);
		goto end;
	}

	LCD_INFO(vdd, "+ enable(%d) ndx(%d)\n", enable, ss_get_display_ndx(vdd));

	if (ss_backlight_tft_request_gpios(vdd)) {
		LCD_ERR(vdd, "fail to request tft backlight gpios");
		goto end;
	}

	LCD_DEBUG(vdd, "%s tft backlight gpios\n", enable ? "enable" : "disable");

	/*
	 * The order of backlight_tft_gpio enable/disable
	 * 1. Enable : backlight_tft_gpio[0], [1], ... [MAX_BACKLIGHT_TFT_GPIO - 1]
	 * 2. Disable : backlight_tft_gpio[MAX_BACKLIGHT_TFT_GPIO - 1], ... [1], [0]
	 */
	if (!enable) {
		add_value = -1;
		i = MAX_BACKLIGHT_TFT_GPIO - 1;
	}

	do {
		if (ss_gpio_is_valid(vdd->dtsi_data.backlight_tft_gpio[i])) {
			gpio_set_value(vdd->dtsi_data.backlight_tft_gpio[i], enable);
			LCD_DEBUG(vdd, "set backlight tft gpio[%d] to %s\n",
						 vdd->dtsi_data.backlight_tft_gpio[i],
						enable ? "high" : "low");
			usleep_range(500, 500);
		}
	} while (((i += add_value) < MAX_BACKLIGHT_TFT_GPIO) && (i >= 0));

end:
	LCD_INFO(vdd, "-\n");
	return ret;
}

#endif

/*************************************************************
*
*		ESD RECOVERY RELATED FUNCTION BELOW.
*
**************************************************************/

/*
 * esd_irq_enable() - Enable or disable esd irq.
 *
 * @enable	: flag for enable or disabled
 * @nosync	: flag for disable irq with nosync
 * @data	: point ot struct ss_panel_info
 */
#define IRQS_PENDING	0x00000200
#define istate core_internal_state__do_not_mess_with_it
static int esd_irq_enable(bool enable, bool nosync, void *data)
{
	/* The irq will enabled when do the request_threaded_irq() */
	static bool is_enabled[MAX_DISPLAY_NDX] = {true, true};
	static bool is_wakeup_source[MAX_DISPLAY_NDX];
	int gpio;
	int irq[MAX_ESD_GPIO] = {0,};
	unsigned long flags;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)data;
	struct irq_desc *desc = NULL;
	struct esd_recovery *esd;
	u8 i = 0;
	int ret = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx\n", (size_t)vdd);
		return -ENODEV;
	}

	if (vdd->gct.is_running && enable) {
		LCD_ERR(vdd, "do not enable ESD irq during gct test..\n");
		return -ENODEV;
	}

	esd = &vdd->esd_recovery;

	if (esd->num_of_gpio == 0)
		return 0;

	for (i = 0; i < esd->num_of_gpio; i++) {
		gpio = esd->esd_gpio[i];

		if (!ss_gpio_is_valid(gpio)) {
			/* TODO: stop and return error here??? */
			LCD_ERR(vdd, "Invalid ESD_GPIO : %d\n", gpio);
			return -EINVAL;
		}
		irq[i] = ss_gpio_to_irq(gpio);
		if (irq[i] < 0) {
			LCD_ERR(vdd, "invalid irq[%d](%d)\n", i, irq[i]);
			return -EINVAL;
		}
	}

	spin_lock_irqsave(&esd->irq_lock, flags);

	if (enable == is_enabled[vdd->ndx]) {
		LCD_INFO(vdd, "ESD irq already %s\n",
				enable ? "enabled" : "disabled");
		goto config_wakeup_source;
	}


	for (i = 0; i < esd->num_of_gpio; i++) {
		gpio = esd->esd_gpio[i];
		desc = irq_to_desc(irq[i]);
		if (enable) {
			/* clear esd irq triggered while it was disabled. */
			if (desc->istate & IRQS_PENDING) {
				LCD_INFO(vdd, "clear esd irq pending status\n");
				desc->istate &= ~IRQS_PENDING;
			}
		}

		if (enable) {
			is_enabled[vdd->ndx] = true;
			enable_irq(irq[i]);
		} else {
			if (nosync)
				disable_irq_nosync(irq[i]);
			else
				disable_irq(irq[i]);
			is_enabled[vdd->ndx] = false;
		}
	}

	/* TODO: Disable log if the esd function stable */
	LCD_INFO(vdd, "ndx=%d, ESD irq %s with %s\n",
				vdd->ndx, enable ? "enabled" : "disabled",
				nosync ? "nosync" : "sync");

config_wakeup_source:
	if (esd->is_wakeup_source == is_wakeup_source[vdd->ndx]) {
		LCD_DEBUG(vdd, "[ESD] IRQs are already irq_wake %s\n",
				is_wakeup_source[vdd->ndx] ? "enabled" : "disabled");
		goto end;
	}

	for (i = 0; i < esd->num_of_gpio; i++) {
		gpio = esd->esd_gpio[i];

		if (!ss_gpio_is_valid(gpio)) {
			LCD_ERR(vdd, "Invalid ESD_GPIO : %d\n", gpio);
			continue;
		}

		is_wakeup_source[vdd->ndx] =
			esd->is_wakeup_source;

		if (is_wakeup_source[vdd->ndx])
			enable_irq_wake(irq[i]);
		else
			disable_irq_wake(irq[i]);
	}

	LCD_INFO(vdd, "[ESD] ndx=%d, IRQs are set to irq_wake %s\n",
				vdd->ndx, is_wakeup_source[vdd->ndx] ? "enabled" : "disabled");

end:
	spin_unlock_irqrestore(&esd->irq_lock, flags);

	return ret;
}

static irqreturn_t ss_ub_con_det_handler(int irq, void *handle);

__visible_for_testing irqreturn_t esd_irq_handler(int irq, void *handle)
{
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *) handle;
	struct sde_connector *conn;
	struct esd_recovery *esd = NULL;
	int i;
	int rc = 0;

	if (!vdd->esd_recovery.is_enabled_esd_recovery) {
		LCD_ERR(vdd, "esd recovery is not enabled yet");
		goto end;
	}

	conn = GET_SDE_CONNECTOR(vdd);
	if (!conn) {
		LCD_ERR(vdd, "fail to get valid conn\n");
		goto end;
	}

	if (unlikely(vdd->is_factory_mode)) {
		if (ss_gpio_is_valid(vdd->ub_con_det.gpio)) {
			LCD_INFO(vdd, "ub_con_det.gpio = %d\n", ss_gpio_get_value(vdd, vdd->ub_con_det.gpio));
		}
	}

	LCD_INFO(vdd, "++\n");

	if (vdd->dtsi_data.esd_sleep_in) {
		if (gpio_is_valid(vdd->esd_recovery.esd_gpio[0])) {
			rc = gpio_get_value(vdd->esd_recovery.esd_gpio[0]);
			if (rc == 1) {
				LCD_DEBUG(vdd, "gpio_octa_esd_det (%d) is HIGH\n", vdd->esd_recovery.esd_gpio[0]);
			}
			else {
				LCD_INFO(vdd, "gpio_octa_esd_det (%d) is LOW\n", vdd->esd_recovery.esd_gpio[0]);
				usleep_range(1*1000, 1*1000);

				if (gpio_is_valid(vdd->esd_recovery.esd_gpio[1])) {
					rc = gpio_get_value(vdd->esd_recovery.esd_gpio[1]);
					if (rc == 1) {
						LCD_INFO(vdd, "gpio_fg_err (%d) is HIGH\n", vdd->esd_recovery.esd_gpio[1]);
					}
					else {
						LCD_INFO(vdd, "gpio_fg_err (%d) is LOW => SKIP Recovery..\n", vdd->esd_recovery.esd_gpio[1]);
						goto end;
					}
				}
			}
		}
	}

	esd_irq_enable(false, true, (void *)vdd);

	vdd->panel_lpm.esd_recovery = true;

	schedule_work(&conn->status_work.work);

	LCD_INFO(vdd, "Panel Recovery(ESD, irq%d), Trial Count = %d\n", irq, vdd->panel_recovery_cnt++);
	SS_XLOG(vdd->ndx, vdd->panel_recovery_cnt);
	inc_dpui_u32_field(DPUI_KEY_QCT_RCV_CNT, 1);

	if (vdd->is_factory_mode) {
		esd = &vdd->esd_recovery;

		for (i = 0; i < esd->num_of_gpio; i++) {
			if ((esd->esd_gpio[i] == vdd->ub_con_det.gpio) &&
				(ss_gpio_to_irq(esd->esd_gpio[i]) == irq)) {
				ss_ub_con_det_handler(irq, handle);
			}
		}
	}

	ss_notify_queue_work(vdd, PANEL_EVENT_ESD);

	LCD_INFO(vdd, "--\n");

end:
	return IRQ_HANDLED;
}

__visible_for_testing int ss_panel_parse_dt_esd(struct device_node *np,
		struct samsung_display_driver_data *vdd)
{
	int rc = 0;
	const char *data = NULL;
	char esd_irq_gpio[] = "samsung,esd-irq-gpio1";
	char esd_irqflag[] = "samsung,esd-irq-trigger1";
	struct esd_recovery *esd = NULL;
	struct dsi_panel *panel = NULL;
	struct drm_panel_esd_config *esd_config = NULL;
	int ret = 0;
	u8 i = 0;

	panel = (struct dsi_panel *)vdd->msm_private;
	esd_config = &panel->esd_config;

	esd = &vdd->esd_recovery;
	esd->num_of_gpio = 0;

	esd_config->esd_enabled = of_property_read_bool(np,
		"qcom,esd-check-enabled");

	if (!esd_config->esd_enabled) {
		ret = -EPERM;
		goto end;
	}

	for (i = 0; i < MAX_ESD_GPIO; i++) {
		esd_irq_gpio[strlen(esd_irq_gpio) - 1] = '1' + i;
		esd->esd_gpio[esd->num_of_gpio] = ss_wrapper_of_get_named_gpio(np, esd_irq_gpio, 0);

		if (ss_gpio_is_valid(esd->esd_gpio[esd->num_of_gpio])) {
			LCD_INFO(vdd, "[ESD] gpio : %d, irq : %d\n",
					esd->esd_gpio[esd->num_of_gpio],
					ss_gpio_to_irq(esd->esd_gpio[esd->num_of_gpio]));
			esd->num_of_gpio++;
		}
	}

	if (vdd->is_factory_mode && ss_gpio_is_valid(vdd->fg_err_gpio)) {
		esd->esd_gpio[esd->num_of_gpio] = vdd->fg_err_gpio;
		esd->num_of_gpio++;
		LCD_INFO(vdd, "add fg_err esd gpio(%d), num_of_gpio: %d\n",
				vdd->fg_err_gpio, esd->num_of_gpio);
	}

	if (esd->num_of_gpio == 0) {
		LCD_ERR(vdd, "fail to find valid esd gpio\n");
		ret = -EINVAL;
		goto end;
	}

	rc = of_property_read_string(np, "qcom,mdss-dsi-panel-status-check-mode", &data);
	if (rc == 0 && !strcmp(data, "irq_check"))
		esd_config->status_mode = ESD_MODE_PANEL_IRQ;
	else
		LCD_ERR(vdd, "invalid panel-status-check-mode string(rc: %d)\n", rc);

	for (i = 0; i < esd->num_of_gpio; i++) {
		/* Without ONESHOT setting, it fails in request_threaded_irq()
		 * with 'handler=NULL' error log
		 */
		esd->irqflags[i] = IRQF_ONESHOT | IRQF_NO_SUSPEND; /* default setting */

		esd_irqflag[strlen(esd_irqflag) - 1] = '1' + i;
		rc = of_property_read_string(np, esd_irqflag, &data);
		if (!rc) {
			if (!strcmp(data, "rising"))
				esd->irqflags[i] |= IRQF_TRIGGER_RISING;
			else if (!strcmp(data, "falling"))
				esd->irqflags[i] |= IRQF_TRIGGER_FALLING;
			else if (!strcmp(data, "high"))
				esd->irqflags[i] |= IRQF_TRIGGER_HIGH;
			else if (!strcmp(data, "low"))
				esd->irqflags[i] |= IRQF_TRIGGER_LOW;
		} else {
			LCD_ERR(vdd, "[%d] fail to find ESD irq flag (gpio%d). set default as FALLING..\n",
					i, esd->esd_gpio[i]);
			esd->irqflags[i] |= IRQF_TRIGGER_FALLING;
		}
	}

end:
	LCD_INFO(vdd, "samsung esd %s mode (%d)\n",
		esd_config->esd_enabled ? "enabled" : "disabled",
		esd_config->status_mode);
	return ret;
}

__visible_for_testing int ss_event_esd_recovery_init(
		struct samsung_display_driver_data *vdd, int event, void *arg)
{
	int ret;
	u8 i;
	int gpio, irqflags;
	struct esd_recovery *esd;
	struct dsi_panel *panel;
	struct drm_panel_esd_config *esd_config;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx\n", (size_t)vdd);
		return -ENODEV;
	}

	LCD_INFO(vdd, "++\n");
	panel = GET_DSI_PANEL(vdd);
	esd_config = &panel->esd_config;
	esd = &vdd->esd_recovery;

	if (unlikely(!esd->esd_recovery_init)) {
		esd->esd_recovery_init = true;
		esd->esd_irq_enable = esd_irq_enable;
		if (esd_config->status_mode == ESD_MODE_PANEL_IRQ) {
			for (i = 0; i < esd->num_of_gpio; i++) {
				/* Set gpio num and irqflags */
				gpio = esd->esd_gpio[i];
				irqflags = esd->irqflags[i];
				if (!ss_gpio_is_valid(gpio)) {
					LCD_ERR(vdd, "[ESD] Invalid GPIO : %d\n", gpio);
					continue;
				}

				gpio_request(gpio, "esd_recovery");
				ret = request_threaded_irq(
						ss_gpio_to_irq(gpio),
						NULL,
						esd_irq_handler,
						irqflags,
						"esd_recovery",
						(void *)vdd);
				if (ret)
					LCD_ERR(vdd, "Failed to request_irq, ret=%d\n",
							ret);
				else
					LCD_INFO(vdd, "request esd irq !!\n");
			}
			esd_irq_enable(false, true, (void *)vdd);
		}
	}

	LCD_INFO(vdd, "--\n");

	return 0;
}

void ss_panel_recovery(struct samsung_display_driver_data *vdd)
{
	struct sde_connector *conn = GET_SDE_CONNECTOR(vdd);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx\n", (size_t)vdd);
		return;
	}

	if (!vdd->esd_recovery.is_enabled_esd_recovery) {
		LCD_ERR(vdd, "esd recovery is not enabled yet");
		return;
	}
	LCD_INFO(vdd, "Panel Recovery, Trial Count = %d\n", vdd->panel_recovery_cnt++);
	SS_XLOG(vdd->panel_recovery_cnt);
	inc_dpui_u32_field(DPUI_KEY_QCT_RCV_CNT, 1);

	esd_irq_enable(false, true, (void *)vdd);
	vdd->panel_lpm.esd_recovery = true;
	schedule_work(&conn->status_work.work);

	LCD_INFO(vdd, "Panel Recovery --\n");

}

/*************************************************************
*
*		UB_CON_DET event for factory binary.
*
**************************************************************/

void ss_send_ub_uevent(struct samsung_display_driver_data *vdd)
{
	char *envp[3] = {"CONNECTOR_NAME=UB_CONNECT", "CONNECTOR_TYPE=HIGH_LEVEL", NULL};
	char *envp_sub[3] = {"CONNECTOR_NAME=UB_CONNECT_SUB", "CONNECTOR_TYPE=HIGH_LEVEL", NULL};

	LCD_INFO(vdd, "[%s] send uvent \n", vdd->ndx == PRIMARY_DISPLAY_NDX ? "UB_CONNECT" : "UB_CONNECT_SUB");

	if (vdd->ndx == PRIMARY_DISPLAY_NDX) {
		kobject_uevent_env(&vdd->lcd_dev->dev.kobj, KOBJ_CHANGE, envp);
#if IS_ENABLED(CONFIG_SEC_ABC)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		sec_abc_send_event("MODULE=ub_main@INFO=ub_disconnected");
#else
		sec_abc_send_event("MODULE=ub_main@WARN=ub_disconnected");
#endif
#endif
	} else {
		kobject_uevent_env(&vdd->lcd_dev->dev.kobj, KOBJ_CHANGE, envp_sub);
#if IS_ENABLED(CONFIG_SEC_ABC)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		sec_abc_send_event("MODULE=ub_sub@INFO=ub_disconnected");
#else
		sec_abc_send_event("MODULE=ub_sub@WARN=ub_disconnected");
#endif
#endif
	}

	return;
}

int ub_con_det_status(int index)
{
	struct samsung_display_driver_data *vdd = ss_get_vdd(index);

	/* high gpio value means panel detached */
	if (vdd && ss_gpio_is_valid(vdd->ub_con_det.gpio)) {
		if (vdd->ub_con_det.current_wakeup_context_gpio_status ||
			ss_gpio_get_value(vdd, vdd->ub_con_det.gpio)) {
			return 1; /* detach */
		} else
			return 0; /* attach */
	} else {
		return 0; /* attach */
	}
}

bool ss_is_ub_connected(struct samsung_display_driver_data *vdd)
{
	struct panel_ub_con_event_data data;
	bool connected;

	connected = !ss_gpio_get_value(vdd, vdd->ub_con_det.gpio); /* gpio low: connected */
	data.state = connected ? PANEL_EVENT_UB_CON_CONNECTED :
				PANEL_EVENT_UB_CON_DISCONNECTED;
	data.display_idx = vdd->ndx;

	LCD_INFO(vdd, "ub_con_det is [%s] [%s]\n",
			vdd->ub_con_det.enabled ? "enabled" : "disabled",
			connected ? "connected" : "disconnected");

	ss_panel_notifier_call_chain(PANEL_EVENT_UB_CON_CHANGED, &data);

	return connected;
}

static irqreturn_t ss_ub_con_det_handler(int irq, void *handle)
{
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *) handle;
	bool connected;
	struct dsi_panel *panel =  GET_DSI_PANEL(vdd);

	connected = ss_is_ub_connected(vdd);

	if (connected)
		return IRQ_HANDLED;

	if (vdd->ub_con_det.enabled)
		ss_send_ub_uevent(vdd);

	vdd->ub_con_det.ub_con_cnt++;

	/* Regardless of ESD interrupt, it should guarantee display power off */
	LCD_INFO(vdd, "turn off dipslay power\n");
	dsi_panel_power_off(panel);

	LCD_INFO(vdd, "-- cnt : %d\n", vdd->ub_con_det.ub_con_cnt);

	return IRQ_HANDLED;
}

static void ss_panel_parse_dt_ub_con(struct device_node *np,
		struct samsung_display_driver_data *vdd)
{
	struct ub_con_detect *ub_con_det;
	struct esd_recovery *esd = NULL;
	int ret, i;

	ub_con_det = &vdd->ub_con_det;

	ub_con_det->gpio = ss_wrapper_of_get_named_gpio(np,
			"samsung,ub-con-det", 0);

	esd = &vdd->esd_recovery;
	if (vdd->is_factory_mode) {
		for (i = 0; i < esd->num_of_gpio; i++) {
			if (esd->esd_gpio[i] == ub_con_det->gpio) {
				LCD_INFO(vdd, "[ub_con_det] gpio : %d share esd gpio : %d\n",
					ub_con_det->gpio, esd->esd_gpio[i]);
				return;
			}
		}
	}

	if (ss_gpio_is_valid(ub_con_det->gpio)) {
		gpio_request(ub_con_det->gpio, "UB_CON_DET");
		LCD_INFO(vdd, "[ub_con_det] gpio : %d, irq : %d status : %d\n",
				ub_con_det->gpio, ss_gpio_to_irq(ub_con_det->gpio),
				ss_gpio_get_value(vdd, ub_con_det->gpio));
	} else {
		LCD_ERR(vdd, "ub-con-det gpio is not valid (%d) \n", ub_con_det->gpio);
		return;
	}

	ret = request_threaded_irq(
				ss_gpio_to_irq(ub_con_det->gpio),
				NULL,
				ss_ub_con_det_handler,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"UB_CON_DET",
				(void *) vdd);
	if (ret)
		LCD_ERR(vdd, "Failed to request_irq, ret=%d\n", ret);

	return;
}

int ss_set_backlight(struct samsung_display_driver_data *vdd, u32 bl_lvl)
{
	struct dsi_panel *panel = NULL;
	int ret = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		ret = -EINVAL;
		goto end;
	}

	panel = GET_DSI_PANEL(vdd);
	if (IS_ERR_OR_NULL(panel)) {
		LCD_ERR(vdd, "panel is NULL\n");
		ret = -EINVAL;
		goto end;
	}

	ret = dsi_panel_set_backlight(panel, bl_lvl);

end:
	return ret;
}

/*************************************************************
*
*		HMT RELATED FUNCTION BELOW.
*
**************************************************************/
int hmt_bright_update(struct samsung_display_driver_data *vdd)
{
	if (vdd->hmt.hmt_on) {
		ss_brightness_dcs_hmt(vdd, vdd->hmt.bl_level);
	} else {
		ss_brightness_dcs(vdd, USE_CURRENT_BL_LEVEL, BACKLIGHT_NORMAL);
		LCD_INFO(vdd, "hmt off state!\n");
	}

	return 0;
}

int hmt_enable(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "[HMT] HMT %s\n", vdd->hmt.hmt_on ? "ON" : "OFF");

	if (vdd->hmt.hmt_on) {
		ss_send_cmd(vdd, TX_HMT_ENABLE);
	} else {
		ss_send_cmd(vdd, TX_HMT_DISABLE);
	}

	return 0;
}

int hmt_reverse_update(struct samsung_display_driver_data *vdd, bool enable)
{
	LCD_INFO(vdd, "[HMT] HMT %s\n", enable ? "REVERSE" : "FORWARD");

	if (enable)
		ss_send_cmd(vdd, TX_HMT_REVERSE);
	else
		ss_send_cmd(vdd, TX_HMT_FORWARD);

	return 0;
}

static void parse_dt_data(struct samsung_display_driver_data *vdd, struct device_node *np,
		void *data, size_t size, char *cmd_string, char panel_rev,
		int (*fnc)(struct samsung_display_driver_data *, struct device_node *, void *, char *))
{
	char string[PARSE_STRING];
	int ret = 0;

	/* Generate string to parsing from DT */
	snprintf(string, PARSE_STRING, "%s%c", cmd_string, 'A' + panel_rev);

	ret = fnc(vdd, np, data, string);

	/* If there is no dtsi data for panel rev B ~ T,
	 * use valid previous panel rev dtsi data.
	 * TODO: Instead of copying all data from previous panel rev,
	 * copy only the pointer...
	 */
	if (ret && (panel_rev > 0))
		memcpy(data, (u8 *) data - size, size);
}

static void ss_panel_parse_dt_mapping_tables(struct device_node *np,
		struct samsung_display_driver_data *vdd)
{
	struct ss_brightness_info *info = &vdd->br_info;
	int panel_rev;

	for (panel_rev = 0; panel_rev < SUPPORT_PANEL_REVISION; panel_rev++) {
		parse_dt_data(vdd, np, &info->vint_map_table[panel_rev],
				sizeof(struct cmd_map),
				"samsung,vint_map_table_rev", panel_rev,
				ss_parse_panel_table); /* VINT TABLE */

		parse_dt_data(vdd, np, &info->candela_map_table[NORMAL][panel_rev],
				sizeof(struct candela_map_table),
				"samsung,candela_map_table_rev", panel_rev,
				ss_parse_candella_mapping_table);

		parse_dt_data(vdd, np, &info->candela_map_table[AOD][panel_rev],
				sizeof(struct candela_map_table),
				"samsung,aod_candela_map_table_rev", panel_rev,
				ss_parse_candella_mapping_table);

		parse_dt_data(vdd, np, &info->candela_map_table[HBM][panel_rev],
				sizeof(struct candela_map_table),
				"samsung,hbm_candela_map_table_rev", panel_rev,
				ss_parse_hbm_candella_mapping_table);

		parse_dt_data(vdd, np, &info->candela_map_table[PAC_NORMAL][panel_rev],
				sizeof(struct candela_map_table),
				"samsung,pac_candela_map_table_rev", panel_rev,
				ss_parse_pac_candella_mapping_table);

		parse_dt_data(vdd, np, &info->candela_map_table[PAC_HBM][panel_rev],
				sizeof(struct candela_map_table),
				"samsung,pac_hbm_candela_map_table_rev", panel_rev,
				ss_parse_hbm_candella_mapping_table);

		parse_dt_data(vdd, np, &info->candela_map_table[MULTI_TO_ONE_NORMAL][panel_rev],
				sizeof(struct candela_map_table),
				"samsung,multi_to_one_candela_map_table_rev", panel_rev,
				ss_parse_multi_to_one_candella_mapping_table);

		/* ACL */
		parse_dt_data(vdd, np, &info->acl_map_table[panel_rev],
				sizeof(struct cmd_map),
				"samsung,acl_map_table_rev", panel_rev,
				ss_parse_panel_table); /* ACL TABLE */

		parse_dt_data(vdd, np, &info->elvss_map_table[panel_rev],
				sizeof(struct cmd_map),
				"samsung,elvss_map_table_rev", panel_rev,
				ss_parse_panel_table); /* ELVSS TABLE */

		parse_dt_data(vdd, np, &info->aid_map_table[panel_rev],
				sizeof(struct cmd_map),
				"samsung,aid_map_table_rev", panel_rev,
				ss_parse_panel_table); /* AID TABLE */

		parse_dt_data(vdd, np, &info->smart_acl_elvss_map_table[panel_rev],
				sizeof(struct cmd_map),
				"samsung,smart_acl_elvss_map_table_rev", panel_rev,
				ss_parse_panel_table); /* TABLE */

		parse_dt_data(vdd, np, &info->hmt_reverse_aid_map_table[panel_rev],
				sizeof(struct cmd_map),
				"samsung,hmt_reverse_aid_map_table_rev", panel_rev,
				ss_parse_panel_table); /* TABLE */

		parse_dt_data(vdd, np, &info->candela_map_table[HMT][panel_rev],
				sizeof(struct candela_map_table),
				"samsung,hmt_candela_map_table_rev", panel_rev,
				ss_parse_candella_mapping_table);
	}
}

static void ss_panel_set_gamma_flash_data(struct samsung_display_driver_data *vdd,
	struct brightness_table *br_tbl, int idx, struct device_node *gamma_node)
{
	struct brightness_table *c_br_tbl;
	struct flash_raw_table *gamma_tbl;
	u32 tmp[3], data;
	int rc;

	gamma_tbl = kzalloc((sizeof(struct flash_raw_table)), GFP_KERNEL);
	if (!gamma_tbl) {
		LCD_ERR(vdd, "fail to alloc gamma_tbl\n");
		return;
	}

	/* get current bightness_table */
	c_br_tbl = &br_tbl[idx];

	/* copy gamma table from parent's first */
	if (c_br_tbl->parent_idx != -1)
		memcpy(gamma_tbl, br_tbl[c_br_tbl->parent_idx].gamma_tbl, sizeof(struct flash_raw_table));

	c_br_tbl->gamma_tbl = gamma_tbl;

	rc = of_property_read_u32(gamma_node, "samsung,refresh_rate", &data);
	c_br_tbl->refresh_rate = !rc ? data : 0;

	c_br_tbl->is_sot_hs_mode = of_property_read_bool(gamma_node, "samsung,br_tbl_sot_hs_mode");

	/* ddi vertical porches are used for AOR interpolation.
	 * In case of 96/48hz mode, its base AOR came from below base RR mode.
	 * - 96hz: 120hz HS -> AOR_96hz = AOR_120hz * (vtotal_96hz) / (vtotal_120hz)
	 * - 48hz: 60hz normal -> AOR_48hz = AOR_60hz * (vtotal_48hz) / (vtotal_60hz)
	 * If there is no base vertical porches, (ex: ddi_vbp_base) in panel dtsi,
	 * parser function set base value as target value (ex: ddi_vbp_base = ddi_vbp).
	 */
	rc = of_property_read_u32(gamma_node, "samsung,ddi_vfp", &data);
	if (!rc)
		c_br_tbl->ddi_vfp = data;
	rc = of_property_read_u32(gamma_node, "samsung,ddi_vbp", &data);
	if (!rc)
		c_br_tbl->ddi_vbp = data;
	rc = of_property_read_u32(gamma_node, "samsung,ddi_vactive", &data);
	if (!rc)
		c_br_tbl->ddi_vactive = data;

	rc = of_property_read_u32(gamma_node, "samsung,ddi_vfp_base", &data);
	c_br_tbl->ddi_vfp_base = !rc ? data : c_br_tbl->ddi_vfp;
	rc = of_property_read_u32(gamma_node, "samsung,ddi_vbp_base", &data);
	c_br_tbl->ddi_vbp_base = !rc ? data : c_br_tbl->ddi_vbp;
	rc = of_property_read_u32(gamma_node, "samsung,ddi_vactive_base", &data);
	c_br_tbl->ddi_vactive_base = !rc ? data : c_br_tbl->ddi_vactive;

	/* 3byte address cover 15Mbyte */
	rc = of_property_read_u32_array(gamma_node, "samsung,flash_gamma_data_read_addr",
			tmp, SS_FLASH_GAMMA_READ_ADDR_SIZE);
	if (!rc) {
		gamma_tbl->flash_gamma_data_read_addr[0] = tmp[0];
		gamma_tbl->flash_gamma_data_read_addr[1] = tmp[1];
		gamma_tbl->flash_gamma_data_read_addr[2] = tmp[2];
	}

	/* size */
	rc = of_property_read_u32_array(gamma_node, "samsung,flash_gamma_data_read_size",
			tmp, SS_FLASH_GAMMA_READ_ADDR_SIZE);
	if (!rc) {
		gamma_tbl->flash_gamma_data_read_size[0] = tmp[0];
		gamma_tbl->flash_gamma_data_read_size[1] = tmp[1];
		gamma_tbl->flash_gamma_data_read_size[2] = tmp[2];
	}

	rc = of_property_read_u32(gamma_node, "samsung,flash_gamma_write_check_addr", &data);
	if (!rc)
		gamma_tbl->flash_gamma_write_check_address = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_gamma_start_bank", &data);
	if (!rc)
		gamma_tbl->flash_gamma_bank_start = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_gamma_end_bank", &data);
	if (!rc)
		gamma_tbl->flash_gamma_bank_end = data;

	LCD_INFO(vdd, "write_check_address: 0x%x start_bank: 0x%x, end_bank: 0x%x\n",
		gamma_tbl->flash_gamma_write_check_address,
		gamma_tbl->flash_gamma_bank_start, gamma_tbl->flash_gamma_bank_end);

	rc = of_property_read_u32(gamma_node, "samsung,flash_gamma_check_sum_start_offset", &data);
	if (!rc)
		gamma_tbl->flash_gamma_check_sum_start_offset = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_gamma_check_sum_end_offset", &data);
	if (!rc)
		gamma_tbl->flash_gamma_check_sum_end_offset = data;

	LCD_INFO(vdd, "check_sum_start_offset : 0x%x check_sum_end_offset: 0x%x\n",
		gamma_tbl->flash_gamma_check_sum_start_offset,
		gamma_tbl->flash_gamma_check_sum_end_offset);

	rc = of_property_read_u32(gamma_node, "samsung,flash_gamma_0xc8_start_offset", &data);
	if (!rc)
		gamma_tbl->flash_gamma_0xc8_start_offset = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_gamma_0xc8_end_offset", &data);
	if (!rc)
		gamma_tbl->flash_gamma_0xc8_end_offset = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_gamma_0xc8_size", &data);
	if (!rc)
		gamma_tbl->flash_gamma_0xc8_size = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_gamma_0xc8_check_sum_start_offset", &data);
	if (!rc)
		gamma_tbl->flash_gamma_0xc8_check_sum_start_offset = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_gamma_0xc8_check_sum_end_offset", &data);
	if (!rc)
		gamma_tbl->flash_gamma_0xc8_check_sum_end_offset = data;

	LCD_INFO(vdd, "flash_gamma 0xC8 start_addr:0x%x end_addr: 0x%x size: 0x%x check_sum_start : 0x%x check_sum_end: 0x%x\n",
		gamma_tbl->flash_gamma_0xc8_start_offset,
		gamma_tbl->flash_gamma_0xc8_end_offset,
		gamma_tbl->flash_gamma_0xc8_size,
		gamma_tbl->flash_gamma_0xc8_check_sum_start_offset,
		gamma_tbl->flash_gamma_0xc8_check_sum_end_offset);

	rc = of_property_read_u32(gamma_node, "samsung,flash_table_hbm_gamma_offset", &data);
	if (!rc)
		gamma_tbl->flash_table_hbm_gamma_offset = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_table_hbm_aor_offset", &data);
	if (!rc)
		gamma_tbl->flash_table_hbm_aor_offset = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_table_hbm_vint_offset", &data);
	if (!rc)
		gamma_tbl->flash_table_hbm_vint_offset = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_table_hbm_elvss_offset", &data);
	if (!rc)
		gamma_tbl->flash_table_hbm_elvss_offset = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_table_hbm_irc_offset", &data);
	if (!rc)
		gamma_tbl->flash_table_hbm_irc_offset = data;

	LCD_INFO(vdd, "flash_table_hbm_addr gamma:0x%x aor: 0x%x vint:0x%x elvss 0x%x hbm:0x%x\n",
		gamma_tbl->flash_table_hbm_gamma_offset,
		gamma_tbl->flash_table_hbm_aor_offset,
		gamma_tbl->flash_table_hbm_vint_offset,
		gamma_tbl->flash_table_hbm_elvss_offset,
		gamma_tbl->flash_table_hbm_irc_offset);

	rc = of_property_read_u32(gamma_node, "samsung,flash_table_normal_gamma_offset", &data);
	if (!rc)
		gamma_tbl->flash_table_normal_gamma_offset = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_table_normal_aor_offset", &data);
	if (!rc)
		gamma_tbl->flash_table_normal_aor_offset = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_table_normal_vint_offset", &data);
	if (!rc)
		gamma_tbl->flash_table_normal_vint_offset = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_table_normal_elvss_offset", &data);
	if (!rc)
		gamma_tbl->flash_table_normal_elvss_offset = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_table_normal_irc_offset", &data);
	if (!rc)
		gamma_tbl->flash_table_normal_irc_offset = data;

	LCD_INFO(vdd, "flash_table__normal_addr gamma:0x%x aor: 0x%x vint:0x%x elvss 0x%x hbm:0x%x\n",
		gamma_tbl->flash_table_normal_gamma_offset,
		gamma_tbl->flash_table_normal_aor_offset,
		gamma_tbl->flash_table_normal_vint_offset,
		gamma_tbl->flash_table_normal_elvss_offset,
		gamma_tbl->flash_table_normal_irc_offset);

	rc = of_property_read_u32(gamma_node, "samsung,flash_table_hmd_gamma_offset", &data);
	if (!rc)
		gamma_tbl->flash_table_hmd_gamma_offset = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_table_hmd_aor_offset", &data);
	if (!rc)
		gamma_tbl->flash_table_hmd_aor_offset = data;

	LCD_INFO(vdd, "flash_table_hmt_addr gamma:0x%x aor: 0x%x\n",
		gamma_tbl->flash_table_hmd_gamma_offset,
		gamma_tbl->flash_table_hmd_aor_offset);

	rc = of_property_read_u32(gamma_node, "samsung,flash_MCD1_R_addr", &data);
	if (!rc)
		gamma_tbl->flash_MCD1_R_address = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_MCD2_R_addr", &data);
	if (!rc)
		gamma_tbl->flash_MCD2_R_address = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_MCD1_L_addr", &data);
	if (!rc)
		gamma_tbl->flash_MCD1_L_address = data;
	rc = of_property_read_u32(gamma_node, "samsung,flash_MCD2_L_addr", &data);
	if (!rc)
		gamma_tbl->flash_MCD2_L_address = data;

	LCD_INFO(vdd, "flash_gamma MCD1_R:0x%x MCD2_R:0x%x MCD1_L:0x%x MCD2_L:0x%x\n",
		gamma_tbl->flash_MCD1_R_address,
		gamma_tbl->flash_MCD2_R_address,
		gamma_tbl->flash_MCD1_L_address,
		gamma_tbl->flash_MCD2_L_address);

	LCD_INFO(vdd, "[%d] refresh_rate: %d, HS mode: %d, " \
		"DDI target res: %d+%d+%d / base res: %d+%d+%d, parent_node: %d\n" \
		, c_br_tbl->idx,
		c_br_tbl->refresh_rate,
		c_br_tbl->is_sot_hs_mode,
		c_br_tbl->ddi_vfp, c_br_tbl->ddi_vbp, c_br_tbl->ddi_vactive,
		c_br_tbl->ddi_vfp_base, c_br_tbl->ddi_vbp_base, c_br_tbl->ddi_vactive_base,
		c_br_tbl->parent_idx);

	return;
}

static int ss_panel_parse_dt_br_table(struct device_node *np,
		struct samsung_display_driver_data *vdd)
{
	struct device_node *gamma_root_node = NULL;
	struct device_node *gamma_node = NULL;
	struct device_node *gamma_sub_node = NULL;
	int count = 0;
	int p_idx;
	struct brightness_table *br_tbl;
	int rc;
	const char *data;

	/* check support gamma flash first */
	rc = of_property_read_string(np, "samsung,flash_gamma_type", &data);
	if (rc) {
		vdd->br_info.flash_gamma_support = false;
		LCD_INFO(vdd, "no samsung,flash_gamma_type, rc=%d\n", rc);
	} else {
		snprintf(vdd->br_info.flash_gamma_type,
				ARRAY_SIZE((vdd->br_info.flash_gamma_type)), "%s", data);
		LCD_INFO(vdd, "Flash gamma type : [%s] \n", vdd->br_info.flash_gamma_type);

		if (!strcmp(vdd->br_info.flash_gamma_type, "flash")) {
			vdd->br_info.flash_gamma_support = true;
			LCD_ERR(vdd, "flash gamma support\n");

			vdd->br_info.support_early_gamma_flash = of_property_read_bool(np, "samsung,support_early_gamma_flash");
			LCD_INFO(vdd, "support_early_gamma_flash: %d\n", vdd->br_info.support_early_gamma_flash);

			vdd->br_info.flash_br_workqueue = create_singlethread_workqueue("flash_br_workqueue");
			INIT_DELAYED_WORK(&vdd->br_info.flash_br_work, flash_br_work_func);

			rc = of_property_read_string(np, "samsung,flash_read_intf", &data);
			if (rc) {
				LCD_ERR(vdd, "error reading samsung,flash_read_intf rc=%d\n", rc);
			} else {
				snprintf(vdd->br_info.flash_read_intf,
						ARRAY_SIZE((vdd->br_info.flash_read_intf)), "%s", data);
				LCD_INFO(vdd, "Flash read intf : [%s] \n", vdd->br_info.flash_read_intf);
			}
		} else if (!strcmp(vdd->br_info.flash_gamma_type, "table")) {
			vdd->br_info.flash_gamma_support = true;
			LCD_ERR(vdd, "table gamma support\n");
		}
	}

	gamma_root_node = of_get_child_by_name(np, "samsung,br_tables");
	if (!gamma_root_node) {
		LCD_INFO(vdd, "no br_tables root node to parse\n");
		return -EINVAL;
	}

	for_each_child_of_node(gamma_root_node, gamma_node) {
		count++;
		for_each_child_of_node(gamma_node, gamma_sub_node)
				count++;
	}

	vdd->br_info.br_tbl_count = count;
	LCD_INFO(vdd, "total gamma nodes count: %d\n", count);

	br_tbl = kzalloc((sizeof(struct brightness_table) * count), GFP_KERNEL);
	if (!br_tbl)
		return -ENOMEM;

	vdd->br_info.br_tbl = br_tbl;

	count = 0;
	for_each_child_of_node(gamma_root_node, gamma_node) {

		/* set current index and parent index
		 * -1 means this node is parent gamma flash node
		 */
		br_tbl[count].idx = count;
		br_tbl[count].parent_idx = -1;

		ss_panel_set_gamma_flash_data(vdd, br_tbl, count, gamma_node);

		/* set parent node index */
		p_idx = count;

		vdd->br_info.br_tbl_flash_cnt++;
		count++;

		/* set sub node of gamma flash table */
		for_each_child_of_node(gamma_node, gamma_sub_node) {

			/* set current index and parent index */
			br_tbl[count].idx = count;
			br_tbl[count].parent_idx = p_idx;

			/* update child node specific data */
			ss_panel_set_gamma_flash_data(vdd, br_tbl, count, gamma_sub_node);

			count++;
		}
	}

	return 0;
}

/*
 * parse flash table for the panel using GM2.
 * devide node ? : address could be not consecutive.
 * read/classify data ? : read/classify raw data in each panel functions.
 */
static int ss_panel_parse_dt_gm2_flash_table(struct device_node *np,
		struct samsung_display_driver_data *vdd)
{
	struct device_node *root_node = NULL;
	struct device_node *node = NULL;
	struct gm2_flash_table *gm2_flash_tbl;
	int count = 0;
	int rc, data;

	root_node = of_get_child_by_name(np, "samsung,gm2_flash_table");
	if (!root_node) {
		/* Depends on DDI, some DDI (ex: HAD) doesn't support
		 * samsung,gm2_flash_table.
		 */
		LCD_INFO(vdd, "no gm2_flash_table root node\n");
		return 0;
	}

	/* count sub table */
	for_each_child_of_node(root_node, node)
		count++;

	vdd->br_info.gm2_flash_tbl_cnt = count;
	LCD_INFO(vdd, "total gm2 flash node count : %d\n", count);

	gm2_flash_tbl = kzalloc((sizeof(struct gm2_flash_table) * count), GFP_KERNEL);
	if (!gm2_flash_tbl)
		return -ENOMEM;

	vdd->br_info.gm2_flash_tbl = gm2_flash_tbl;

	count = 0;
	for_each_child_of_node(root_node, node) {

		gm2_flash_tbl[count].idx = count;

		rc = of_property_read_u32(node, "samsung,gm2_flash_read_start", &data);
		if (!rc)
			gm2_flash_tbl[count].start_addr = data;
		rc = of_property_read_u32(node, "samsung,gm2_flash_read_end", &data);
		if (!rc)
			gm2_flash_tbl[count].end_addr = data;

		gm2_flash_tbl[count].buf_len =
			gm2_flash_tbl[count].end_addr - gm2_flash_tbl[count].start_addr + 1;

		if (gm2_flash_tbl[count].buf_len <= 0) {
			LCD_ERR(vdd, "wrong address is implemented.. please check!!\n");
		} else {
			gm2_flash_tbl[count].raw_buf = kzalloc(gm2_flash_tbl[count].buf_len, GFP_KERNEL | GFP_DMA);
			if (!gm2_flash_tbl[count].raw_buf) {
				LCD_ERR(vdd, "fail to alloc raw_buf\n");
				return -ENOMEM;
			}
		}

		LCD_INFO(vdd, "GM2 FLASH [%d] - addr : 0x%x ~ 0x%x (len %d)\n", count,
			gm2_flash_tbl[count].start_addr, gm2_flash_tbl[count].end_addr, gm2_flash_tbl[count].buf_len);

		count++;
	}

	return 0;
}

int ss_panel_power_ctrl(struct samsung_display_driver_data *vdd, bool enable)
{
	if (enable) {
		/* off -> normal on */
		ss_panel_regulator_short_detection(vdd, PANEL_OFF);
	}

	return 0;
}

int ss_panel_regulator_short_detection(struct samsung_display_driver_data *vdd, enum panel_state state)
{
	int i, voltage;
	int rc = 0;
	bool enable;

	for (i = 0; i < vdd->panel_regulator.vreg_count; i++) {
		if (vdd->panel_regulator.vregs[i].ssd) {

			/* set voltage */
			voltage = (state == PANEL_LPM) ?
				vdd->panel_regulator.vregs[i].from_lpm_v : vdd->panel_regulator.vregs[i].from_off_v;

			/* If voltage is 0, diable SSD */
			enable = voltage ? true : false;
			rc = ss_wrapper_regulator_set_short_detection(vdd,
					vdd->panel_regulator.vregs[i].vreg,
					enable, voltage);
			LCD_INFO(vdd, "[%s] state : %d, voltage : %d, ssd %s - %s\n",
				vdd->panel_regulator.vregs[i].name, state, voltage, (enable ? "enable" : "disable"),
				(rc < 0 ? "fail" : "success"));
		}
	}

	return 0;
}

static void ss_panel_parse_regulator(struct samsung_display_driver_data *vdd, struct device_node *panel_node)
{
	struct device_node *regulators_np, *np;
	int rc;
	const char* data;
	const __be32 *data_32;
	int count = 0;

	regulators_np = of_get_child_by_name(panel_node, "regulators");
	if (!regulators_np) {
		LCD_INFO(vdd, "no regulators node to parse\n");
		return;
	}

	for_each_child_of_node(regulators_np, np)
		count++;

	if (count == 0) {
		LCD_ERR(vdd, "No panel regulators..\n");
		return;
	} else
		vdd->panel_regulator.vreg_count = count;

	LCD_INFO(vdd, "panel regulator count is %d\n", count);

	vdd->panel_regulator.vregs = kcalloc(vdd->panel_regulator.vreg_count,
		sizeof(*vdd->panel_regulator.vregs), GFP_KERNEL);
	if (!vdd->panel_regulator.vregs) {
		vdd->panel_regulator.vreg_count = 0;
		LCD_ERR(vdd, "fail to kcalloc for panel regulators..\n");
		return;
	}

	count = 0;
	for_each_child_of_node(regulators_np, np) {
		rc = of_property_read_string(np, "reg,name", &data);
		if (rc)
			LCD_ERR(vdd, "error reading regulator name. rc=%d\n", rc);
		else {
			snprintf(vdd->panel_regulator.vregs[count].name,
				ARRAY_SIZE((vdd->panel_regulator.vregs[count].name)), "%s", data);

			vdd->panel_regulator.vregs[count].vreg = regulator_get(NULL, vdd->panel_regulator.vregs[count].name);
			if (!vdd->panel_regulator.vregs[count].vreg)
				LCD_ERR(vdd, "Regulator[%s] get fail\n", vdd->panel_regulator.vregs[count].name);
		}

		rc = of_property_read_string(np, "reg,type", &data);
		if (rc)
			LCD_ERR(vdd, "error reading regulator type. rc=%d\n", rc);
		else {
			if (!strcmp(data, "ssd"))
				vdd->panel_regulator.vregs[count].ssd = true;
		}

		data_32 = of_get_property(np, "reg,from-off", NULL);
		if (data_32)
			vdd->panel_regulator.vregs[count].from_off_v = (int)(be32_to_cpup(data_32));
		else
			LCD_ERR(vdd, "error reading from-off voltage\n");

		data_32 = of_get_property(np, "reg,from-lpm", NULL);
		if (data_32)
			vdd->panel_regulator.vregs[count].from_lpm_v = (int)(be32_to_cpup(data_32));
		else
			LCD_ERR(vdd, "error reading from-lpm voltage\n");

		LCD_INFO(vdd, "PANEL REGULATOR[%d] Name [%s], ssd = %d, from_off_v = %d, from_lpm_v = %d\n", count,
			vdd->panel_regulator.vregs[count].name, vdd->panel_regulator.vregs[count].ssd,
			vdd->panel_regulator.vregs[count].from_off_v,
			vdd->panel_regulator.vregs[count].from_lpm_v);

		count++;
	}

	return;
}

static void ss_mafpc_parse_dt(struct samsung_display_driver_data *vdd)
{
	struct device_node *np;
	const char *data;
	int i, len;

	np = ss_get_mafpc_of(vdd);
	if (!np) {
		LCD_INFO(vdd, "No mAFPC np..\n");
		return;
	}

	/* has mafpc np? -> Suppot mafpc */
	vdd->mafpc.is_support = true;

	/* mAFPC Check CRC data */
	data = of_get_property(np, "samsung,mafpc_crc_pass_data", &len);
	if (!data)
		LCD_ERR(vdd, "fail to get samsung,mafpc_crc_pass_data .. \n");
	else {
		vdd->mafpc.crc_pass_data = kzalloc(len, GFP_KERNEL);
		vdd->mafpc.crc_read_data = kzalloc(len, GFP_KERNEL);
		vdd->mafpc.crc_size = len;
		if (!vdd->mafpc.crc_pass_data || !vdd->mafpc.crc_read_data)
			LCD_ERR(vdd, "fail to alloc for mafpc crc data \n");
		else {
			for (i = 0; i < len; i++) {
				vdd->mafpc.crc_pass_data[i] = data[i];
				LCD_INFO(vdd, "mafpc crc_data[%d] = %02x\n", i, vdd->mafpc.crc_pass_data[i]);
			}
		}
	}

	return;
}
static void ss_self_disp_parse_dt(struct samsung_display_driver_data *vdd)
{
	struct device_node *np;
	const char *data;
	int i, len;

	np = ss_get_self_disp_of(vdd);
	if (!np) {
		LCD_ERR(vdd, "No self display np..\n");
		return;
	}

	/* has self disp np? -> Suppot Self Display */
	vdd->self_disp.is_support = true;

	/* Self Mask Check CRC data */
	data = of_get_property(np, "samsung,mask_crc_pass_data", &len);
	if (!data)
		LCD_ERR(vdd, "fail to get samsung,mask_crc_pass_data .. \n");
	else {
		vdd->self_disp.mask_crc_pass_data = kzalloc(len, GFP_KERNEL);
		vdd->self_disp.mask_crc_read_data = kzalloc(len, GFP_KERNEL);
		vdd->self_disp.mask_crc_size = len;
		if (!vdd->self_disp.mask_crc_pass_data || !vdd->self_disp.mask_crc_read_data) {
			LCD_ERR(vdd, "fail to alloc for mask_crc_data \n");
		} else {
			for (i = 0; i < len; i++) {
				vdd->self_disp.mask_crc_pass_data[i] = data[i];
				LCD_INFO(vdd, "self mask crc_data[%d] = %02x\n", i, vdd->self_disp.mask_crc_pass_data[i]);
			}
		}
	}

	return;
}

static void ss_test_mode_parse_dt(struct samsung_display_driver_data *vdd)
{
	struct device_node *np;
	u32 tmp[2];
	int rc;

	np = ss_get_test_mode_of(vdd);
	if (!np) {
		LCD_INFO(vdd, "No test_mode np..\n");
		return;
	}

	/* Gram Checksum Test */
	vdd->gct.is_support = of_property_read_bool(np, "samsung,support_gct");
	if (vdd->support_ss_cmd) {
		int len;
		const __be32 *data;

		data = of_get_property(np, "samsung,gct_pass_val", &len);
		if (data && len == 2) {
			vdd->gct.valid_checksum[0] = be32_to_cpup(&data[0]);
			vdd->gct.valid_checksum[1] = be32_to_cpup(&data[1]);
			vdd->gct.valid_checksum[2] = vdd->gct.valid_checksum[0];
			vdd->gct.valid_checksum[3] = vdd->gct.valid_checksum[1];

			LCD_INFO(vdd, "gct valid_chksum: %02X %02X %02X %02X\n",
					vdd->gct.valid_checksum[0],
					vdd->gct.valid_checksum[1],
					vdd->gct.valid_checksum[2],
					vdd->gct.valid_checksum[3]);
		}
	}
	LCD_INFO(vdd, "vdd->gct.is_support = %d\n", vdd->gct.is_support);

	/* ccd success,fail value */
	rc = of_property_read_u32(np, "samsung,ccd_pass_val", tmp);
	vdd->ccd_pass_val = (!rc ? tmp[0] : 0);
	rc = of_property_read_u32(np, "samsung,ccd_fail_val", tmp);
	vdd->ccd_fail_val = (!rc ? tmp[0] : 0);

	LCD_INFO(vdd, "CCD fail value [%02x] \n", vdd->ccd_fail_val);

	return;
}

static int __ss_parse_cmd_sets_revision(struct dsi_panel_cmd_set *cmd,
					int type, struct dsi_parser_utils *utils,
					char (*ss_cmd_set_prop)[SS_CMD_PROP_STR_LEN])
{
	int rc = 0;
	int rev;
	char *map;
	struct dsi_panel_cmd_set *set_rev[SUPPORT_PANEL_REVISION];
	int len;
	char org_val;
	int ss_cmd_type = type - SS_DSI_CMD_SET_START;

	/* For revA */
	cmd->cmd_set_rev[0] = cmd;

	/* For revB ~ revT */
	map = ss_cmd_set_prop[ss_cmd_type];
	len = strlen(ss_cmd_set_prop[ss_cmd_type]);

	org_val = map[len - 1];

	for (rev = 1; rev < SUPPORT_PANEL_REVISION; rev++) {
		set_rev[rev] = kzalloc(sizeof(struct dsi_panel_cmd_set),
				GFP_KERNEL);

		if (map[len - 1] >= 'A' && map[len - 1] <= 'Z')
			map[len - 1] = 'A' + rev;

		rc = __ss_dsi_panel_parse_cmd_sets(set_rev[rev], type, utils, ss_cmd_set_prop);
		if (rc) {
			/* If there is no data for the panel rev,
			 * copy previous panel rev data pointer.
			 */
			kfree(set_rev[rev]);
			cmd->cmd_set_rev[rev] = cmd->cmd_set_rev[rev - 1];
		} else {
			cmd->cmd_set_rev[rev] = set_rev[rev];
		}
	}

	/* cmd_set_prop_map will be used only for debugging log.
	 * cmd_set_prop_map variable is located in ro. area.
	 * PMK feature checks ro data between vmlinux and runtime kernel ram,
	 * and reports error if thoes have different value.
	 * Reset revision value to original value to make same value in vmlinux.
	 */
	if (map[len - 1] >= 'A' && map[len - 1] <= 'Z')
		map[len - 1] = org_val;

	return rc;
}

static int ss_dsi_panel_parse_cmd_sets(struct samsung_display_driver_data *vdd)
{
	struct ss_cmd_set *cmd_sets = vdd->dtsi_data.ss_cmd_sets;
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);

	int rc = 0;
	u32 i;
	struct dsi_parser_utils *utils;
	int ss_cmd_type;
	struct ss_cmd_set *ss_set;
	struct dsi_panel_cmd_set *qc_set;
	char *cmd_name;

	for (i = SS_DSI_CMD_SET_START; i <= SS_DSI_CMD_SET_MAX ; i++) {
		cmd_name = ss_cmd_set_prop_map[i - SS_DSI_CMD_SET_START];
		ss_cmd_type = i - SS_DSI_CMD_SET_START;

		ss_set = &cmd_sets[ss_cmd_type];
		ss_set->ss_type = i;
		ss_set->count = 0;

		qc_set = &ss_set->base;
		qc_set->ss_cmd_type = i;
		qc_set->count = 0;

		/* Self display has different device node */
		if (i >= TX_SELF_DISP_CMD_START && i <= TX_SELF_DISP_CMD_END)
			utils = &panel->self_display_utils;
		/* mafpc has different device node */
		else if (i >= TX_MAFPC_CMD_START && i <= TX_MAFPC_CMD_END)
			utils = &panel->mafpc_utils;
		/* test mode has different device node */
		else if (i >= TX_TEST_MODE_CMD_START && i <= TX_TEST_MODE_CMD_END)
			utils = &panel->test_mode_utils;
		else
			utils = &panel->utils;

		/* To use samsung style command format,
		 * 1) Define samsung,support_ss_cmd in panel dtsi.
		 * 2) Define command with string style like below.
		 *    samsung,mcd_on_tx_cmds_revA = "W F0 5A 5A ....";
		 */
		if (vdd->support_ss_cmd && ss_is_prop_string_style(utils->data, cmd_name)) {
			/* each mode value will be parsed in ss_wrapper_parse_cmd_sets_each_mode() */
			if (i >= SS_DSI_CMD_SET_FOR_EACH_MODE_START && i <= SS_DSI_CMD_SET_FOR_EACH_MODE_END)
				continue;

			ss_set->is_ss_style_cmd = true;

			rc = ss_wrapper_parse_cmd_sets(vdd, ss_set, i, utils->data, cmd_name);
			if (rc) {
				pr_err("failed to parse set %d, rc: %d\n", i, rc);
				return rc;
			}

			rc = __ss_parse_ss_cmd_sets_revision(vdd, ss_set, i, utils->data, cmd_name);
			if (rc) {
				pr_err("failed to parse subrev set %d, rc: %d\n", i, rc);
				return rc;
			}
		} else {
			rc = __ss_dsi_panel_parse_cmd_sets(qc_set, i, utils, ss_cmd_set_prop_map);
			if (rc && rc != -ENOTSUPP)
				pr_err("failed to parse set %d, rc=%d\n", i, rc);

			__ss_parse_cmd_sets_revision(qc_set, i, utils, ss_cmd_set_prop_map);
		}
	}

	return rc;
}

static void ss_panel_parse_dt(struct samsung_display_driver_data *vdd)
{
	int rc, i;
	u32 tmp[2];
	int len;
	char backlight_tft_gpio[] = "samsung,panel-backlight-tft-gpio1";
	const char *data;
	const __be32 *data_32;
	struct device_node *np;
	struct dsi_panel *panel;

	np = ss_get_panel_of(vdd);
	if (!np) {
		LCD_ERR(vdd, "No panel np..\n");
		return;
	}

	panel = GET_DSI_PANEL(vdd);

	/* Set LP11 init flag */
	vdd->dtsi_data.samsung_lp11_init = of_property_read_bool(np, "samsung,dsi-lp11-init");
	LCD_INFO(vdd, "LP11 init %s\n",
		vdd->dtsi_data.samsung_lp11_init ? "enabled" : "disabled");

	rc = of_property_read_u32(np, "samsung,mdss-power-on-reset-delay-us", tmp);
	vdd->dtsi_data.samsung_power_on_reset_delay = (!rc ? tmp[0] : 0);

	rc = of_property_read_u32(np, "samsung,mdss-dsi-off-reset-delay-us", tmp);
	vdd->dtsi_data.samsung_dsi_off_reset_delay = (!rc ? tmp[0] : 0);

	/* Set esc clk 128M */
	vdd->dtsi_data.samsung_esc_clk_128M = of_property_read_bool(np, "samsung,esc-clk-128M");
	LCD_INFO(vdd, "ESC CLK 128M %s\n",
		vdd->dtsi_data.samsung_esc_clk_128M ? "enabled" : "disabled");

	vdd->panel_lpm.is_support = of_property_read_bool(np, "samsung,support_lpm");
	LCD_INFO(vdd, "alpm enable %s\n", vdd->panel_lpm.is_support? "enabled" : "disabled");

	vdd->skip_read_on_pre = of_property_read_bool(np, "samsung,skip_read_on_pre");
	LCD_INFO(vdd, "Skip read on pre %s\n", vdd->skip_read_on_pre ? "enabled" : "disabled");

	/* Set HMT flag */
	vdd->hmt.is_support = of_property_read_bool(np, "samsung,support_hmt");
	LCD_INFO(vdd, "support_hmt %s\n", vdd->hmt.is_support ? "enabled" : "disabled");

	/* TCON Clk On Support */
	vdd->dtsi_data.samsung_tcon_clk_on_support =
		of_property_read_bool(np, "samsung,tcon-clk-on-support");
	LCD_INFO(vdd, "tcon clk on support: %s\n",
			vdd->dtsi_data.samsung_tcon_clk_on_support ?
			"enabled" : "disabled");

	/* DDI Uses Flash memory */
	vdd->dtsi_data.ddi_no_flash =
		of_property_read_bool(np, "samsung,ddi_no_flash");
	LCD_INFO(vdd, "ddi_no_flash: %s\n", vdd->dtsi_data.ddi_no_flash ?
			"enabled" : "disabled");

	vdd->dtsi_data.samsung_tcon_rdy_gpio =
		ss_wrapper_of_get_named_gpio(np, "samsung,tcon-rdy-gpio", 0);

	if (ss_gpio_is_valid(vdd->dtsi_data.samsung_tcon_rdy_gpio))
		gpio_request(vdd->dtsi_data.samsung_tcon_rdy_gpio, "tcon_rdy");
	else
		LCD_INFO(vdd, "no valid TCON RDY GPIO(%d)\n", vdd->dtsi_data.samsung_tcon_rdy_gpio);

	LCD_INFO(vdd, "tcon rdy gpio: %d\n", vdd->dtsi_data.samsung_tcon_rdy_gpio);

	data_32 = of_get_property(np, "samsung,delay-after-tcon-ready", NULL);
	vdd->dtsi_data.samsung_delay_after_tcon_rdy = (data_32 ? (int)(be32_to_cpup(data_32)) : 0);

	LCD_INFO(vdd, "Delay after Tcon rdy : %d\n", vdd->dtsi_data.samsung_delay_after_tcon_rdy);

	/* Set unicast flags for whole mipi cmds for dual DSI panel
	 * If display->ctrl_count is 2, it broadcasts.
	 * To prevent to send mipi cmd thru mipi dsi1, set unicast flag
	 */
	vdd->dtsi_data.samsung_cmds_unicast =
		of_property_read_bool(np, "samsung,cmds-unicast");
	LCD_INFO(vdd, "mipi cmds unicast flag: %d\n",
			vdd->dtsi_data.samsung_cmds_unicast);

	/* anapass DDI power sequence: VDD on -> LP11 -> reset -> wait TCON RDY high -> HS clock.
	 * To keep above unique sequence, use anapass_power_seq flag..
	 */
	vdd->dtsi_data.samsung_anapass_power_seq =
		of_property_read_bool(np, "samsung,anapass-power-seq");
	LCD_INFO(vdd, "anapass ddi power seq. flag: %d\n",
			vdd->dtsi_data.samsung_anapass_power_seq );

	/* Set TFT flag */
	vdd->mdnie.tuning_enable_tft = of_property_read_bool(np,
				"samsung,mdnie-tuning-enable-tft");
	vdd->dtsi_data.tft_common_support  = of_property_read_bool(np,
		"samsung,tft-common-support");

	LCD_INFO(vdd, "tft_common_support %s\n",
	vdd->dtsi_data.tft_common_support ? "enabled" : "disabled");

	vdd->dtsi_data.tft_module_name = of_get_property(np,
		"samsung,tft-module-name", NULL);  /* for tft tablet */

	vdd->dtsi_data.panel_vendor = of_get_property(np,
		"samsung,panel-vendor", NULL);

	vdd->dtsi_data.disp_model = of_get_property(np,
		"samsung,disp-model", NULL);

	vdd->dtsi_data.backlight_gpio_config = of_property_read_bool(np,
		"samsung,backlight-gpio-config");

	LCD_INFO(vdd, "backlight_gpio_config %s\n",
	vdd->dtsi_data.backlight_gpio_config ? "enabled" : "disabled");

	/* Factory Panel Swap*/
	vdd->dtsi_data.samsung_support_factory_panel_swap = of_property_read_bool(np,
		"samsung,support_factory_panel_swap");

	vdd->support_window_color = of_property_read_bool(np, "samsung,support_window_color");
	LCD_INFO(vdd, "ndx(%d) support_window_color %s\n", vdd->ndx,
		vdd->support_window_color ? "support" : "not support");

	/* Set tft backlight gpio */
	for (i = 0; i < MAX_BACKLIGHT_TFT_GPIO; i++) {
		backlight_tft_gpio[strlen(backlight_tft_gpio) - 1] = '1' + i;
		vdd->dtsi_data.backlight_tft_gpio[i] =
				 ss_wrapper_of_get_named_gpio(np,
						backlight_tft_gpio, 0);
		if (ss_gpio_is_valid(vdd->dtsi_data.backlight_tft_gpio[i]))
			LCD_INFO(vdd, "[%d] backlight_tft_gpio num : %d\n", i,
					vdd->dtsi_data.backlight_tft_gpio[i]);
	}

	/* Set Mdnie lite HBM_CE_TEXT_MDNIE mode used */
	vdd->dtsi_data.hbm_ce_text_mode_support = of_property_read_bool(np, "samsung,hbm_ce_text_mode_support");

	/* Set Backlight IC discharge time */
	rc = of_property_read_u32(np, "samsung,blic-discharging-delay-us", tmp);
	vdd->dtsi_data.blic_discharging_delay_tft = (!rc ? tmp[0] : 6);

	/* Set cabc delay time */
	rc = of_property_read_u32(np, "samsung,cabc-delay-us", tmp);
	vdd->dtsi_data.cabc_delay = (!rc ? tmp[0] : 6);

	/* IRC */
	vdd->br_info.common_br.support_irc = of_property_read_bool(np, "samsung,support_irc");

	/* POC Driver */
	vdd->poc_driver.is_support = of_property_read_bool(np, "samsung,support_poc_driver");
	LCD_INFO(vdd, "[POC] is_support = %d\n", vdd->poc_driver.is_support);

	if (vdd->poc_driver.is_support) {
		rc = of_property_read_u32(np, "samsung,poc_image_size", tmp);
		vdd->poc_driver.image_size = (!rc ? tmp[0] : 0);

		rc = of_property_read_u32(np, "samsung,poc_start_addr", tmp);
		vdd->poc_driver.start_addr = (!rc ? tmp[0] : 0);

		LCD_INFO(vdd, "[POC] start_addr (%d) image_size (%d)\n",
			vdd->poc_driver.start_addr, vdd->poc_driver.image_size);

		/* ERASE */
		rc = of_property_read_u32(np, "samsung,poc_erase_delay_us", tmp);
		vdd->poc_driver.erase_delay_us = (!rc ? tmp[0] : 0);

		rc = of_property_read_u32_array(np, "samsung,poc_erase_sector_addr_idx",
				vdd->poc_driver.erase_sector_addr_idx, 3);
		if (rc) {
			vdd->poc_driver.erase_sector_addr_idx[0] = -1;
			LCD_INFO(vdd, "fail to get poc_erase_sector_addr_idx\n");
		}

		LCD_INFO(vdd, "[POC][ERASE] delay_us(%d) addr idx (%d %d %d)\n",
			vdd->poc_driver.erase_delay_us,
			vdd->poc_driver.erase_sector_addr_idx[0],
			vdd->poc_driver.erase_sector_addr_idx[1],
			vdd->poc_driver.erase_sector_addr_idx[2]);

		/* WRITE */
		rc = of_property_read_u32(np, "samsung,poc_write_delay_us", tmp);
		vdd->poc_driver.write_delay_us = (!rc ? tmp[0] : 0);
		rc = of_property_read_u32(np, "samsung,poc_write_data_size", tmp);
		vdd->poc_driver.write_data_size = (!rc ? tmp[0] : 0);
		rc = of_property_read_u32(np, "samsung,poc_write_loop_cnt", tmp);
		vdd->poc_driver.write_loop_cnt = (!rc ? tmp[0] : 0);

		rc = of_property_read_u32_array(np, "samsung,poc_write_addr_idx",
				vdd->poc_driver.write_addr_idx, 3);
		if (rc) {
			vdd->poc_driver.write_addr_idx[0] = -1;
			LCD_INFO(vdd, "fail to get poc_write_addr_idx\n");
		}

		LCD_INFO(vdd, "[POC][WRITE] delay_us(%d) data_size(%d) loo_cnt(%d) addr idx (%d %d %d)\n",
			vdd->poc_driver.write_delay_us,
			vdd->poc_driver.write_data_size,
			vdd->poc_driver.write_loop_cnt,
			vdd->poc_driver.write_addr_idx[0],
			vdd->poc_driver.write_addr_idx[1],
			vdd->poc_driver.write_addr_idx[2]);

		/* READ */
		rc = of_property_read_u32(np, "samsung,poc_read_delay_us", tmp);
		vdd->poc_driver.read_delay_us = (!rc ? tmp[0] : 0);

		rc = of_property_read_u32_array(np, "samsung,poc_read_addr_idx",
				vdd->poc_driver.read_addr_idx, 3);
		if (rc) {
			vdd->poc_driver.read_addr_idx[0] = -1;
			LCD_INFO(vdd, "fail to get poc_read_addr_idx\n");
		} else
			LCD_INFO(vdd, "read addr idx (%d %d %d)\n",
				vdd->poc_driver.read_addr_idx[0],
				vdd->poc_driver.read_addr_idx[1],
				vdd->poc_driver.read_addr_idx[2]);

		LCD_INFO(vdd, "[POC][READ] delay_us(%d) addr idx (%d %d %d)\n",
			vdd->poc_driver.read_delay_us,
			vdd->poc_driver.read_addr_idx[0],
			vdd->poc_driver.read_addr_idx[1],
			vdd->poc_driver.read_addr_idx[2]);

		/* parse spi cmds */
		ss_panel_parse_spi_cmd(np, vdd);
	}

	/* PAC */
	vdd->br_info.common_br.pac = of_property_read_bool(np, "samsung,support_pac");
	LCD_INFO(vdd, "vdd->br_info.common_br.pac = %d\n", vdd->br_info.common_br.pac);

	/* Gamma Mode 2 */
	vdd->br_info.common_br.gamma_mode2_support = of_property_read_bool(np, "samsung,support_gamma_mode2");
	LCD_INFO(vdd, "vdd->br.gamma_mode2_support = %d\n", vdd->br_info.common_br.gamma_mode2_support);

	if (vdd->br_info.common_br.gamma_mode2_support) {
		rc = of_property_read_u32(np, "samsung,flash_vbias_start_addr", tmp);
		vdd->br_info.gm2_table.ddi_flash_start_addr = (!rc ? tmp[0] : 0);

		rc = of_property_read_u32(np, "samsung,flash_vbias_tot_len", tmp);
		vdd->br_info.gm2_table.ddi_flash_raw_len = (!rc ? tmp[0] : 0);

		rc = of_property_read_u32(np, "samsung,flash_vbias_one_mode_len", tmp);
		vdd->br_info.gm2_table.len_one_vbias_mode = (!rc ? tmp[0] : 0);

		rc = of_property_read_u32(np, "samsung,flash_vbias_mtp_one_mode_len", tmp);
		vdd->br_info.gm2_table.len_mtp_one_vbias_mode = (!rc ? tmp[0] : 0);

		rc = of_property_read_u32(np, "samsung,flash_vbias_ns_temp_warm_offset", tmp);
		vdd->br_info.gm2_table.off_ns_temp_warm = (!rc ? tmp[0] : 0);

		rc = of_property_read_u32(np, "samsung,flash_vbias_ns_temp_cool_offset", tmp);
		vdd->br_info.gm2_table.off_ns_temp_cool = (!rc ? tmp[0] : 0);

		rc = of_property_read_u32(np, "samsung,flash_vbias_ns_temp_cold_offset", tmp);
		vdd->br_info.gm2_table.off_ns_temp_cold = (!rc ? tmp[0] : 0);

		rc = of_property_read_u32(np, "samsung,flash_vbias_hs_temp_warm_offset", tmp);
		vdd->br_info.gm2_table.off_hs_temp_warm = (!rc ? tmp[0] : 0);

		rc = of_property_read_u32(np, "samsung,flash_vbias_hs_temp_cool_offset", tmp);
		vdd->br_info.gm2_table.off_hs_temp_cool = (!rc ? tmp[0] : 0);

		rc = of_property_read_u32(np, "samsung,flash_vbias_hs_temp_cold_offset", tmp);
		vdd->br_info.gm2_table.off_hs_temp_cold = (!rc ? tmp[0] : 0);

		rc = of_property_read_u32(np, "samsung,flash_vbias_checksum_offset", tmp);
		vdd->br_info.gm2_table.off_gm2_flash_checksum = (!rc ? tmp[0] : 0);

		LCD_INFO(vdd, "flash: start: %x, tot len: %d, one mode len: %d, mode off={%x %x %x %x %x %x, checksum: %x}\n",
				vdd->br_info.gm2_table.ddi_flash_start_addr,
				vdd->br_info.gm2_table.ddi_flash_raw_len,
				vdd->br_info.gm2_table.len_one_vbias_mode,
				vdd->br_info.gm2_table.off_ns_temp_warm,
				vdd->br_info.gm2_table.off_ns_temp_cool,
				vdd->br_info.gm2_table.off_ns_temp_cold,
				vdd->br_info.gm2_table.off_hs_temp_warm,
				vdd->br_info.gm2_table.off_hs_temp_cool,
				vdd->br_info.gm2_table.off_hs_temp_cold,
				vdd->br_info.gm2_table.off_gm2_flash_checksum);

		ss_panel_parse_dt_gm2_flash_table(np, vdd);
	}

	vdd->br_info.common_br.multi_to_one_support = of_property_read_bool(np, "samsung,multi_to_one_support");
	LCD_INFO(vdd, "vdd->br.multi_to_one_support = %d\n", vdd->br_info.common_br.multi_to_one_support);

	/* Global Para */
	vdd->gpara = of_property_read_bool(np, "samsung,support_gpara");
	LCD_INFO(vdd, "vdd->support_gpara = %d\n", vdd->gpara);
	vdd->pointing_gpara = of_property_read_bool(np, "samsung,pointing_gpara");
	LCD_INFO(vdd, "vdd->pointing_gpara = %d\n", vdd->pointing_gpara);
	vdd->two_byte_gpara = of_property_read_bool(np, "samsung,two_byte_gpara");
	LCD_INFO(vdd, "vdd->two_byte_gpara = %d\n", vdd->two_byte_gpara);

	/* DDI SPI */
	vdd->samsung_support_ddi_spi = of_property_read_bool(np, "samsung,support_ddi_spi");
	if (vdd->samsung_support_ddi_spi) {
		rc = of_property_read_u32(np, "samsung,ddi_spi_cs_high_gpio_for_gpara", tmp);
		vdd->ddi_spi_cs_high_gpio_for_gpara = (!rc ? tmp[0] : -1);

		LCD_INFO(vdd, "vdd->ddi_spi_cs_high_gpio_for_gpara = %d\n", vdd->ddi_spi_cs_high_gpio_for_gpara);
	}

	/* Set elvss_interpolation_temperature */
	data_32 = of_get_property(np, "samsung,elvss_interpolation_temperature", NULL);

	if (data_32)
		vdd->br_info.common_br.elvss_interpolation_temperature = (int)(be32_to_cpup(data_32));
	else
		vdd->br_info.common_br.elvss_interpolation_temperature = ELVSS_INTERPOLATION_TEMPERATURE;

	/* Set lux value for mdnie HBM */
	data_32 = of_get_property(np, "samsung,enter_hbm_ce_lux", NULL);
	if (data_32)
		vdd->mdnie.enter_hbm_ce_lux = (int)(be32_to_cpup(data_32));
	else
		vdd->mdnie.enter_hbm_ce_lux = MDNIE_HBM_CE_LUX;

	/* SAMSUNG_FINGERPRINT */
	vdd->support_optical_fingerprint = of_property_read_bool(np, "samsung,support-optical-fingerprint");
	LCD_INFO(vdd, "support_optical_fingerprint %s\n",
		vdd->support_optical_fingerprint ? "enabled" : "disabled");

	/* Power Control for LPM */
	vdd->panel_lpm.lpm_pwr.support_lpm_pwr_ctrl = of_property_read_bool(np, "samsung,lpm-power-control");
	LCD_INFO(vdd, "lpm_power_control %s\n", vdd->panel_lpm.lpm_pwr.support_lpm_pwr_ctrl ? "enabled" : "disabled");

	if (vdd->panel_lpm.lpm_pwr.support_lpm_pwr_ctrl) {
		rc = of_property_read_string(np, "samsung,lpm-power-control-supply-name", &data);
		if (rc)
			LCD_ERR(vdd, "error reading lpm-power name. rc=%d\n", rc);
		else
			snprintf(vdd->panel_lpm.lpm_pwr.lpm_pwr_ctrl_supply_name,
				ARRAY_SIZE((vdd->panel_lpm.lpm_pwr.lpm_pwr_ctrl_supply_name)), "%s", data);

		data_32 = of_get_property(np, "samsung,lpm-power-control-supply-min-voltage", NULL);
		if (data_32)
			vdd->panel_lpm.lpm_pwr.lpm_pwr_ctrl_supply_min_v = (int)(be32_to_cpup(data_32));
		else
			LCD_ERR(vdd, "error reading lpm-power min_voltage\n");

		data_32 = of_get_property(np, "samsung,lpm-power-control-supply-max-voltage", NULL);
		if (data_32)
			vdd->panel_lpm.lpm_pwr.lpm_pwr_ctrl_supply_max_v = (int)(be32_to_cpup(data_32));
		else
			LCD_ERR(vdd, "error reading lpm-power max_voltage\n");

		LCD_INFO(vdd, "lpm_power_control Supply Name=%s, Min=%d, Max=%d\n",
			vdd->panel_lpm.lpm_pwr.lpm_pwr_ctrl_supply_name, vdd->panel_lpm.lpm_pwr.lpm_pwr_ctrl_supply_min_v,
			vdd->panel_lpm.lpm_pwr.lpm_pwr_ctrl_supply_max_v);

		rc = of_property_read_string(np, "samsung,lpm-power-control-elvss-name", &data);
		if (!rc) {
			snprintf(vdd->panel_lpm.lpm_pwr.lpm_pwr_ctrl_elvss_name,
				ARRAY_SIZE((vdd->panel_lpm.lpm_pwr.lpm_pwr_ctrl_elvss_name)), "%s", data);
			LCD_INFO(vdd, "lpm-power-elvss name: %s\n",
					vdd->panel_lpm.lpm_pwr.lpm_pwr_ctrl_elvss_name);
		}

		data_32 = of_get_property(np, "samsung,lpm-power-control-elvss-lpm-voltage", NULL);
		if (data_32) {
			vdd->panel_lpm.lpm_pwr.lpm_pwr_ctrl_elvss_lpm_v = (int)(be32_to_cpup(data_32));
			LCD_INFO(vdd, "lpm-power-elvss lpm voltage: %d\n",
					vdd->panel_lpm.lpm_pwr.lpm_pwr_ctrl_elvss_lpm_v);
		}

		data_32 = of_get_property(np, "samsung,lpm-power-control-elvss-normal-voltage", NULL);
		if (data_32) {
			vdd->panel_lpm.lpm_pwr.lpm_pwr_ctrl_elvss_normal_v = (int)(be32_to_cpup(data_32));
			LCD_INFO(vdd, "lpm-power-elvss normal voltage: %d\n",
					vdd->panel_lpm.lpm_pwr.lpm_pwr_ctrl_elvss_normal_v);
		}

		LCD_INFO(vdd, "lpm_power_control ELVSS Name=%s, lpm=%d, normal=%d\n",
			vdd->panel_lpm.lpm_pwr.lpm_pwr_ctrl_elvss_name, vdd->panel_lpm.lpm_pwr.lpm_pwr_ctrl_elvss_lpm_v,
			vdd->panel_lpm.lpm_pwr.lpm_pwr_ctrl_elvss_normal_v);
	}

	ss_panel_parse_regulator(vdd, np);

	/* To reduce DISPLAY ON time */
	vdd->dtsi_data.samsung_reduce_display_on_time = of_property_read_bool(np,"samsung,reduce_display_on_time");
	vdd->dtsi_data.samsung_dsi_force_clock_lane_hs = of_property_read_bool(np,"samsung,dsi_force_clock_lane_hs");
	rc = of_property_read_u32(np, "samsung,wait_after_reset_delay", tmp);
	vdd->dtsi_data.after_reset_delay = (!rc ? (s64)tmp[0] : 0);
	rc = of_property_read_u32(np, "samsung,sleep_out_to_on_delay", tmp);
	vdd->dtsi_data.sleep_out_to_on_delay = (!rc ? (s64)tmp[0] : 0);

	if (vdd->dtsi_data.samsung_reduce_display_on_time) {
		/*
			Please Check interrupt gpio is general purpose gpio
			1. pmic gpio or gpio-expender is not permitted.
			2. gpio should have unique interrupt number.
		*/

		if (ss_gpio_is_valid(ss_wrapper_of_get_named_gpio(np, "samsung,home_key_irq_gpio", 0))) {
				vdd->dtsi_data.samsung_home_key_irq_num =
					ss_gpio_to_irq(ss_wrapper_of_get_named_gpio(np,
								"samsung,home_key_irq_gpio", 0));
				LCD_INFO(vdd, "%s gpio : %d (irq : %d)\n",
					np->name, ss_wrapper_of_get_named_gpio(np, "samsung,home_key_irq_gpio", 0),
					vdd->dtsi_data.samsung_home_key_irq_num);
		}

		if (ss_gpio_is_valid(ss_wrapper_of_get_named_gpio(np, "samsung,fingerprint_irq_gpio", 0))) {
			vdd->dtsi_data.samsung_finger_print_irq_num =
				ss_gpio_to_irq(ss_wrapper_of_get_named_gpio(np, "samsung,fingerprint_irq_gpio", 0));
			LCD_INFO(vdd, "%s gpio : %d (irq : %d)\n",
				np->name, ss_wrapper_of_get_named_gpio(np, "samsung,fingerprint_irq_gpio", 0),
				vdd->dtsi_data.samsung_finger_print_irq_num);
		}
	}

#if IS_ENABLED(CONFIG_DEV_RIL_BRIDGE)
	/* Dynamic MIPI Clock */
	vdd->dyn_mipi_clk.is_support = of_property_read_bool(np, "samsung,support_dynamic_mipi_clk");
	LCD_INFO(vdd, "vdd->dyn_mipi_clk.is_support = %d\n", vdd->dyn_mipi_clk.is_support);

	if (vdd->dyn_mipi_clk.is_support) {
		ss_parse_dyn_mipi_clk_sel_table(np, vdd, "samsung,dynamic_mipi_clk_sel_table");
		ss_parse_dyn_mipi_clk_timing_table(np, vdd,	"samsung,dynamic_mipi_clk_timing_table");

		vdd->dyn_mipi_clk.notifier.priority = 0;
		vdd->dyn_mipi_clk.notifier.notifier_call = ss_rf_info_notify_callback;
		register_dev_ril_bridge_event_notifier(&vdd->dyn_mipi_clk.notifier);
	}
#endif

	ss_panel_parse_dt_mapping_tables(np, vdd);

	if (panel) {
		panel->bl_config.bl_max_level = panel->bl_config.brightness_max_level = vdd->br_info.common_br.max_bl_level;
		LCD_INFO(vdd, "set panel bl max level = %d\n",panel->bl_config.bl_max_level);
	}

	vdd->support_ss_cmd = of_property_read_bool(np, "samsung,support_ss_cmd");

	ss_dsi_panel_parse_cmd_sets(vdd);
	ss_wrapper_parse_cmd_sets_each_mode(vdd);

	/* pll ssc */
	vdd_pll_ssc_disabled = of_property_read_bool(np, "samsung,pll_ssc_disabled");
	LCD_INFO(vdd, "vdd->pll_ssc_disabled = %d\n", vdd_pll_ssc_disabled);

	// LCD SELECT
	vdd->select_panel_gpio = ss_wrapper_of_get_named_gpio(np,
			"samsung,mdss_dsi_lcd_sel_gpio", 0);
	if (ss_gpio_is_valid(vdd->select_panel_gpio))
		gpio_request(vdd->select_panel_gpio, "lcd_sel_gpio");

	// MIPI SELECT
	vdd->mipisel_gpio = ss_wrapper_of_get_named_gpio(np,
			"samsung,mipisel-gpio", 0);
	if (ss_gpio_is_valid(vdd->mipisel_gpio)) {
		gpio_request(vdd->mipisel_gpio, "mipisel_gpio");

		rc = of_property_read_u32(np, "samsung,mipisel-on_val", tmp);
		vdd->mipisel_on_val = (!rc ? tmp[0] : 0);
		LCD_INFO(vdd, "mipisel_on_val is %d\n", vdd->mipisel_on_val);
	}

	vdd->select_panel_use_expander_gpio = of_property_read_bool(np, "samsung,mdss_dsi_lcd_sel_use_expander_gpio");

	/* Panel LPM */
	rc = of_property_read_u32(np, "samsung,lpm_swire_delay", tmp);
	vdd->dtsi_data.lpm_swire_delay = (!rc ? tmp[0] : 0);

	rc = of_property_read_u32(np, "samsung,lpm-init-delay-ms", tmp);
	vdd->dtsi_data.samsung_lpm_init_delay = (!rc ? tmp[0] : 0);

	rc = of_property_read_u32(np, "samsung,delayed-display-on", tmp);
	vdd->dtsi_data.samsung_delayed_display_on = (!rc ? tmp[0] : 0);

	/* FG_ERR */
	if (vdd->is_factory_mode) {
		vdd->fg_err_gpio = ss_wrapper_of_get_named_gpio(np, "samsung,fg-err_gpio", 0);
		if (ss_gpio_is_valid(vdd->fg_err_gpio))
			LCD_INFO(vdd, "FG_ERR gpio: %d\n", vdd->fg_err_gpio);
	} else {
		vdd->fg_err_gpio = -1; /* default 0 is valid gpio... set invalid gpio.. */
	}

	vdd->support_lp_rx_err_recovery = of_property_read_bool(np, "samsung,support_lp_rx_err_recovery");
	LCD_INFO(vdd, "support_lp_rx_err_recovery : %d\n", vdd->support_lp_rx_err_recovery);

	ss_panel_parse_dt_esd(np, vdd);
	ss_panel_parse_dt_ub_con(np, vdd);

	/* AOR & IRC INTERPOLATION (FLASH or NO FLASH SUPPORT)*/
	data_32 = of_get_property(np, "samsung,hbm_brightness", &len);
			vdd->br_info.hbm_brightness_step = (data_32 ? len/sizeof(len) : 0);
	data_32 = of_get_property(np, "samsung,normal_brightness", &len);
			vdd->br_info.normal_brightness_step = (data_32 ? len/sizeof(len) : 0);
	data_32 = of_get_property(np, "samsung,hmd_brightness", &len);
			vdd->br_info.hmd_brightness_step = (data_32 ? len/sizeof(len) : 0);
	LCD_INFO(vdd, "gamma_step hbm: %d normal: %d hmt: %d\n",
		vdd->br_info.hbm_brightness_step,
		vdd->br_info.normal_brightness_step,
		vdd->br_info.hmd_brightness_step);

	rc = of_property_read_u32(np, "samsung,gamma_size", tmp);
			vdd->br_info.gamma_size = (!rc ? tmp[0] : 34);
	rc = of_property_read_u32(np, "samsung,aor_size", tmp);
			vdd->br_info.aor_size = (!rc ? tmp[0] : 2);
	rc = of_property_read_u32(np, "samsung,vint_size", tmp);
			vdd->br_info.vint_size = (!rc ? tmp[0] : 1);
	rc = of_property_read_u32(np, "samsung,elvss_size", tmp);
			vdd->br_info.elvss_size = (!rc ? tmp[0] : 3);
	rc = of_property_read_u32(np, "samsung,irc_size", tmp);
			vdd->br_info.irc_size = (!rc ? tmp[0] : 17);
	rc = of_property_read_u32(np, "samsung,dbv_size", tmp);
			vdd->br_info.dbv_size = (!rc ? tmp[0] : 17);

	LCD_INFO(vdd, "gamma_size gamma:%d aor:%d vint:%d elvss:%d irc:%d dbv:%d\n",
		vdd->br_info.gamma_size,
		vdd->br_info.aor_size,
		vdd->br_info.vint_size,
		vdd->br_info.elvss_size,
		vdd->br_info.irc_size,
		vdd->br_info.dbv_size);

	/* parse br table */
	ss_panel_parse_dt_br_table(np, vdd);

	vdd->rsc_4_frame_idle = of_property_read_bool(np, "samsung,rsc_4_frame_idle");
	LCD_INFO(vdd, "rsc_4_frame_idle : %d\n", vdd->rsc_4_frame_idle);

	/* VRR: Variable Refresh Rate */
	vdd->vrr.support_vrr_based_bl = of_property_read_bool(np, "samsung,support_vrr_based_bl");

	/* BRR: Bridge Refresh Rate */
	vdd->vrr.is_support_brr = of_property_read_bool(np, "samsung,support_brr");
	vdd->vrr.is_support_black_frame = of_property_read_bool(np, "samsung,support_black_frame");
	vdd->vrr.is_support_delayed_perf = of_property_read_bool(np, "samsung,support_delayed_perf");

	/* Low Frequency Driving mode for HOP display */
	vdd->vrr.lfd.support_lfd = of_property_read_bool(np, "samsung,support_lfd");

	/* VRR: TE modulation  */
	vdd->vrr.support_te_mod = of_property_read_bool(np, "samsung,support_te_mod");

	LCD_INFO(vdd, "support: vrr: %d, brr: %d, black_frame: %d, delayed_perf: %d, lfd: %d, te_mod: %d\n",
			vdd->vrr.support_vrr_based_bl, vdd->vrr.is_support_brr,
			vdd->vrr.is_support_black_frame,
			vdd->vrr.is_support_delayed_perf,
			vdd->vrr.lfd.support_lfd,
			vdd->vrr.support_te_mod);

	/* Need exclusive tx for Brightness */
	vdd->need_brightness_lock= of_property_read_bool(np, "samsung,need_brightness_lock");
	LCD_INFO(vdd, "need_brightness_lock : %s\n",
		vdd->need_brightness_lock ? "support brightness_lock" : "Not support brightness_lock");
	/* Not to send QCOM made PPS (A data type) */
	vdd->no_qcom_pps= of_property_read_bool(np, "samsung,no_qcom_pps");
	LCD_INFO(vdd, "no_qcom_pps : %s\n",
		vdd->no_qcom_pps ? "support no_qcom_pps" : "Not support no_qcom_pps");


	/*
		AOT support : tddi video panel
		To keep high status panel power & reset
	*/
	vdd->aot_enable = of_property_read_bool(np, "samsung,aot_enable");
	LCD_INFO(vdd, "aot_enable : %s\n",
		vdd->aot_enable ? "aot_enable support" : "aot_enable doesn't support");

	vdd->aot_reset_regulator = of_property_read_bool(np, "samsung,aot_reset_regulator");
	LCD_INFO(vdd, "aot_reset_regulator : %s\n",
		vdd->aot_reset_regulator ? "aot_reset_regulator enabled" : "Not aot_reset_regulator");
	vdd->aot_reset_regulator_late = of_property_read_bool(np, "samsung,aot_reset_regulator_late");
	LCD_INFO(vdd, "aot_reset_regulator_late : %s\n",
		vdd->aot_reset_regulator_late ? "aot_reset_regulator LATE enabled" : "Not aot_reset_regulator LATE");

	/* TDDI, touch notify esd when no esd gpio from ddi */
	vdd->esd_touch_notify = of_property_read_bool(np, "samsung,esd_touch_notify");
	LCD_INFO(vdd, "esd_touch_notify is %s\n",
			vdd->esd_touch_notify ? "enabled" : "Not supported");

	/* Read module ID at probe timing */
	vdd->support_early_id_read = of_property_read_bool(np, "samsung,support_early_id_read");
	LCD_INFO(vdd, "support_early_id_read : %s\n",
		vdd->support_early_id_read ? "support" : "doesn't support");

	/*
		panel dsi/csi mipi strength value
	*/
	for (i = 0; i < SS_PHY_CMN_MAX; i++) {
		clear_bit(i, vdd->ss_phy_ctrl_bit);
		vdd->ss_phy_ctrl_data[i] = 0;
	}

	if (!of_property_read_u32(np, "samsung,phy_vreg_ctrl_0", tmp)) {
		set_bit(SS_PHY_CMN_VREG_CTRL_0, vdd->ss_phy_ctrl_bit);
		vdd->ss_phy_ctrl_data[SS_PHY_CMN_VREG_CTRL_0] = tmp[0];
		LCD_INFO(vdd, "phy_vreg_ctrl_0 : 0x%x\n", vdd->ss_phy_ctrl_data[SS_PHY_CMN_VREG_CTRL_0]);
	}

	if (!of_property_read_u32(np, "samsung,phy_ctrl2", tmp)) {
		set_bit(SS_PHY_CMN_CTRL_2, vdd->ss_phy_ctrl_bit);
		vdd->ss_phy_ctrl_data[SS_PHY_CMN_CTRL_2] = tmp[0];
		LCD_INFO(vdd, "phy_ctrl2 : 0x%x\n", vdd->ss_phy_ctrl_data[SS_PHY_CMN_CTRL_2]);
	}

	if (!of_property_read_u32(np, "samsung,phy_offset_top_ctrl", tmp)) {
		set_bit(SS_PHY_CMN_GLBL_RESCODE_OFFSET_TOP_CTRL, vdd->ss_phy_ctrl_bit);
		vdd->ss_phy_ctrl_data[SS_PHY_CMN_GLBL_RESCODE_OFFSET_TOP_CTRL] = tmp[0];
		LCD_INFO(vdd, "phy_offset_top_ctrl : 0x%x\n", vdd->ss_phy_ctrl_data[SS_PHY_CMN_GLBL_RESCODE_OFFSET_TOP_CTRL]);
	}

	if (!of_property_read_u32(np, "samsung,phy_offset_bot_ctrl", tmp)) {
		set_bit(SS_PHY_CMN_GLBL_RESCODE_OFFSET_BOT_CTRL, vdd->ss_phy_ctrl_bit);
		vdd->ss_phy_ctrl_data[SS_PHY_CMN_GLBL_RESCODE_OFFSET_BOT_CTRL] = tmp[0];
		LCD_INFO(vdd, "phy_offset_bot_ctrl : 0x%x\n", vdd->ss_phy_ctrl_data[SS_PHY_CMN_GLBL_RESCODE_OFFSET_BOT_CTRL]);
	}

	if (!of_property_read_u32(np, "samsung,phy_offset_mid_ctrl", tmp)) {
		set_bit(SS_PHY_CMN_GLBL_RESCODE_OFFSET_MID_CTRL, vdd->ss_phy_ctrl_bit);
		vdd->ss_phy_ctrl_data[SS_PHY_CMN_GLBL_RESCODE_OFFSET_MID_CTRL] = tmp[0];
		LCD_INFO(vdd, "phy_offset_mid_ctrl : 0x%x\n", vdd->ss_phy_ctrl_data[SS_PHY_CMN_GLBL_RESCODE_OFFSET_MID_CTRL]);
	}

	if (!of_property_read_u32(np, "samsung,phy_str_swi_cal_sel_ctrl", tmp)) {
		set_bit(SS_PHY_CMN_GLBL_STR_SWI_CAL_SEL_CTRL, vdd->ss_phy_ctrl_bit);
		vdd->ss_phy_ctrl_data[SS_PHY_CMN_GLBL_STR_SWI_CAL_SEL_CTRL] = tmp[0];
		LCD_INFO(vdd, "phy_str_swi_cal_sel_ctrl : 0x%x\n", vdd->ss_phy_ctrl_data[SS_PHY_CMN_GLBL_STR_SWI_CAL_SEL_CTRL]);
	}

	/* sustain LP11 signal before LP00 on entering sleep status */
	rc = of_property_read_u32(np, "samsung,lp11_sleep_ms_time", tmp);
	vdd->lp11_sleep_ms_time = (!rc ? tmp[0] : 0);
	LCD_INFO(vdd, "lp11_sleep_ms_time : %d\n",
		vdd->lp11_sleep_ms_time);

	/* skip DSI_CMD_SET_ON durning splash booting */
	vdd->skip_cmd_set_on_splash_enabled = of_property_read_bool(np, "samsung,skip_cmd_set_on_splash_enabled");
	LCD_INFO(vdd, "skip_cmd_set_on_splash_enabled [%s]\n",
		vdd->skip_cmd_set_on_splash_enabled ? "enabled" : "disabled");

	/* Support ddi has 6 bytes DDI_ID */
	rc = of_property_read_u32(np, "samsung,ddi_id_length", tmp);
	vdd->dtsi_data.ddi_id_length = (!rc ? tmp[0] : 0);
	LCD_INFO(vdd, "ddi_id_length = [%d]\n", vdd->dtsi_data.ddi_id_length);

	/* Siliconworks_ddi has different CELL_ID Type */
	rc = of_property_read_u32(np, "samsung,cell_id_type", tmp);
	vdd->dtsi_data.cell_id_type = (!rc ? tmp[0] : 0);
	LCD_INFO(vdd, "cell_id_type = [%d]\n", vdd->dtsi_data.cell_id_type);

	/* SiliconWorks DDI need sleep in cmd in ESD Recovery */
	vdd->dtsi_data.esd_sleep_in = of_property_read_bool(np, "samsung,esd_sleep_in");
	LCD_INFO(vdd, "esd_sleep_in [%s]\n",
		vdd->dtsi_data.esd_sleep_in ? "enabled" : "disabled");

	/* (mafpc) (self_display) (test_mode) have dependent dtsi file */
	ss_mafpc_parse_dt(vdd);
	ss_self_disp_parse_dt(vdd);
	ss_test_mode_parse_dt(vdd);

	/* UDC */
	rc = of_property_read_u32(np, "samsung,udc_start_addr", tmp);
	vdd->udc.start_addr = (!rc ? tmp[0] : 0);
	rc = of_property_read_u32(np, "samsung,udc_data_size", tmp);
	vdd->udc.size = (!rc ? tmp[0] : 0);
	if (vdd->udc.size) {
		vdd->udc.data = kzalloc(vdd->udc.size, GFP_KERNEL);
		if (!vdd->udc.data)
			LCD_INFO(vdd, "[UDC] fail to alloc udc data buf..\n");
		else
			LCD_INFO(vdd, "[UDC] start_addr : %X, data_size : %d\n", vdd->udc.start_addr, vdd->udc.size);
	}

}

/************************************************/
/*												*/
/* 				 LPM control 				    */
/*											  	*/
/************************************************/
void ss_panel_low_power_config(struct samsung_display_driver_data *vdd, int enable)
{
	if (!vdd->panel_lpm.is_support) {
		LCD_INFO(vdd, "[Panel LPM] LPM(ALPM/HLPM) is not supported\n");
		return;
	}

	ss_panel_lpm_power_ctrl(vdd, enable);

	if (enable) {
		vdd->esd_recovery.is_wakeup_source = true;
	} else {
		vdd->esd_recovery.is_wakeup_source = false;
	}

	ss_panel_lpm_ctrl(vdd, enable);
}

/*
 * ss_find_reg_offset()
 * This function find offset for reg value
 * reg_list[X][0] is reg value
 * reg_list[X][1] is offset for reg value
 * cmd_list is the target cmds for searching reg value
 */
int ss_find_reg_offset(int (*reg_list)[2],
		struct dsi_panel_cmd_set *cmd_list[], int list_size)
{
	struct dsi_panel_cmd_set *target_cmds = NULL;
	int i = 0, j = 0, max_cmd_cnt;

	if (IS_ERR_OR_NULL(reg_list) || IS_ERR_OR_NULL(cmd_list))
		goto end;

	for (i = 0; i < list_size; i++) {
		target_cmds = cmd_list[i];
		max_cmd_cnt = target_cmds->count;

		for (j = 0; j < max_cmd_cnt; j++) {
			if (target_cmds->cmds[j].ss_txbuf &&
					target_cmds->cmds[j].ss_txbuf[0] == reg_list[i][0]) {
				reg_list[i][1] = j;
				break;
			}
		}
	}

end:
	for (i = 0; i < list_size; i++)
		pr_debug("[SDE] offset[%d] : %d\n", i, reg_list[i][1]);
	return 0;
}

static bool is_new_lpm_version(struct samsung_display_driver_data *vdd)
{
	if (vdd->panel_lpm.ver == LPM_VER1)
		return true;
	else
		return false;
}

static void set_lpm_br_values(struct samsung_display_driver_data *vdd)
{
	int from, end;
	int left, right, p = 0;
	int loop = 0;
	struct candela_map_table *table;
	int bl_level = vdd->br_info.common_br.bl_level;

	table = &vdd->br_info.candela_map_table[AOD][vdd->panel_revision];

	if (IS_ERR_OR_NULL(table->cd)) {
		LCD_ERR(vdd, "No aod candela_map_table..\n");
		return;
	}

	LCD_DEBUG(vdd, "table size (%d)\n", table->tab_size);

	if (bl_level > table->max_lv)
		bl_level = table->max_lv;

	left = 0;
	right = table->tab_size - 1;

	while (left <= right) {
		loop++;
		p = (left + right) / 2;
		from = table->from[p];
		end = table->end[p];
		LCD_DEBUG(vdd, "[%d] from(%d) end(%d) / %d\n", p, from, end, bl_level);

		if (bl_level >= from && bl_level <= end) {
			/* AOD service set brightness based on aod candela map in config_display.xml.
			 * If config_display.xml have unmatched value with samsung,aod_candela_map_table_revA,
			 * prints error log.
			 */
			if (ss_is_panel_lpm(vdd) && bl_level != from)
				LCD_ERR(vdd, "wrong aod candela map in config xml(from: %d, bl_level: %d)\n",
						from, bl_level);

			break;
		}

		if (bl_level < from)
			right = p - 1;
		else
			left = p + 1;

		if (loop > table->tab_size) {
			LCD_ERR(vdd, "can not find (%d) level in table!\n", bl_level);
			p = table->tab_size - 1;
			break;
		}
	};
	vdd->panel_lpm.lpm_bl_level = table->cd[p];

	LCD_DEBUG(vdd, "%s: (%d)->(%d)\n",
		__func__, vdd->br_info.common_br.bl_level, vdd->panel_lpm.lpm_bl_level);
}

int ss_panel_lpm_power_ctrl(struct samsung_display_driver_data *vdd, int enable)
{
	int i;
	int rc = 0;
	int get_voltage;
	struct dsi_panel *panel = NULL;
	struct dsi_vreg *target_vreg = NULL;
	struct dsi_regulator_info regs;
	struct lpm_pwr_ctrl *lpm_pwr;

	if (!vdd->panel_lpm.is_support) {
		LCD_INFO(vdd, "[Panel LPM] LPM(ALPM/HLPM) is not supported\n");
		return -ENODEV;
	}

	panel = GET_DSI_PANEL(vdd);
	if (IS_ERR_OR_NULL(panel)) {
		LCD_ERR(vdd, "No Panel Data\n");
		return -ENODEV;
	}

	LCD_DEBUG(vdd, "%s ++\n", enable == true ? "Enable" : "Disable");

	regs = panel->power_info;
	lpm_pwr = &vdd->panel_lpm.lpm_pwr;

	if (!lpm_pwr->support_lpm_pwr_ctrl) {
		LCD_INFO(vdd, "%s: No panel power control for LPM\n", __func__);
		return -EPERM;
	}

	mutex_lock(&vdd->panel_lpm.lpm_lock);

	/* Find vreg for LPM setting */
	for (i = 0; i < regs.count; i++) {
		target_vreg = &regs.vregs[i];
		if (!strcmp(target_vreg->vreg_name, lpm_pwr->lpm_pwr_ctrl_supply_name)) {
			LCD_INFO(vdd, "Found Voltage(%d)\n", i);
			break;
		}
	}

	if (i == regs.count) {
		LCD_ERR(vdd, "err: no target_vreg(%s)\n", lpm_pwr->lpm_pwr_ctrl_supply_name);
		rc = -ENODEV;
		goto end;
	}

	/* To check previous voltage */
	get_voltage = ss_wrapper_regulator_get_voltage(vdd, target_vreg->vreg);
	if (get_voltage < 0) {
		LCD_ERR(vdd, "fail to get volt(%d)\n", get_voltage);
		rc = -ENODEV;
		goto end;
	}

	if (enable) { /* AOD ON(Enter) */
		if (get_voltage != lpm_pwr->lpm_pwr_ctrl_supply_min_v) {
			rc = ss_wrapper_regulator_set_voltage(vdd,
					target_vreg->vreg,
					lpm_pwr->lpm_pwr_ctrl_supply_min_v,
					lpm_pwr->lpm_pwr_ctrl_supply_max_v);
			if (rc < 0) {
				LCD_ERR(vdd, "Voltage Set Fail enable=%d voltage : %d rc : %d\n",
							enable, lpm_pwr->lpm_pwr_ctrl_supply_min_v, rc);
			} else {
				get_voltage = ss_wrapper_regulator_get_voltage(vdd, target_vreg->vreg);
				LCD_INFO(vdd, "enable=%d, current get_voltage=%d rc : %d\n", enable, get_voltage, rc);
			}

			/* normal on -> lpm */
			ss_panel_regulator_short_detection(vdd, PANEL_ON);
		} else {
			LCD_DEBUG(vdd, "enable=%d, previous voltage : %d\n", enable, get_voltage);
		}

	} else { /* AOD OFF(Exit) */
		if (get_voltage != target_vreg->min_voltage) {
			rc = ss_wrapper_regulator_set_voltage(vdd,
						target_vreg->vreg,
						target_vreg->min_voltage,
						target_vreg->max_voltage);
			if (rc < 0) {
				LCD_ERR(vdd, "Voltage Set Fail enable=%d voltage : %d rc : %d\n",
							enable, target_vreg->min_voltage, rc);
				panic("Voltage Set Fail to NORMAL");
			} else {
				get_voltage = ss_wrapper_regulator_get_voltage(vdd, target_vreg->vreg);
				LCD_INFO(vdd, "enable=%d, current get_voltage=%d\n", enable, get_voltage);

				if (get_voltage != target_vreg->min_voltage)
					panic("Voltage Set Fail to NORMAL");
			}

			/* lpm -> normal on */
			ss_panel_regulator_short_detection(vdd, PANEL_LPM);
		} else {
			LCD_DEBUG(vdd, "enable=%d, previous voltage : %d\n", enable, get_voltage);
		}
	}

end:
	mutex_unlock(&vdd->panel_lpm.lpm_lock);
	LCD_DEBUG(vdd, "[Panel LPM] --\n");

	return rc;
}

void ss_panel_lpm_ctrl(struct samsung_display_driver_data *vdd, int enable)
{
	s64 lpm_init_delay_us = 0;

	if (!vdd->panel_lpm.is_support) {
		LCD_INFO(vdd, "[Panel LPM][DIPSLAY_%d] LPM(ALPM/HLPM) is not supported\n", vdd->ndx);
		return;
	}

	if (ss_is_panel_off(vdd)) {
		LCD_INFO(vdd, "[Panel LPM][DIPSLAY_%d] Do not change mode\n", vdd->ndx);
		return;
	}

	LCD_INFO(vdd, "[Panel LPM][DIPSLAY_%d] ++\n", vdd->ndx);

	lpm_init_delay_us = (s64)vdd->dtsi_data.samsung_lpm_init_delay * 1000;

	mutex_lock(&vdd->panel_lpm.lpm_lock);

	vdd->panel_lpm.during_ctrl = true;

	/* To avoid unexpected ESD detction, Disable ESD irq before cmd tx related with lpm */
	if (vdd->esd_recovery.esd_irq_enable)
		vdd->esd_recovery.esd_irq_enable(false, true, (void *)vdd);

	if (enable) { /* AOD ON(Enter) */
		if (vdd->panel_func.samsung_update_lpm_ctrl_cmd) {
			vdd->panel_func.samsung_update_lpm_ctrl_cmd(vdd, enable);
			LCD_INFO(vdd, "[Panel LPM][DIPSLAY_%d] update lpm cmd done(on)\n", vdd->ndx);
		}

		/* lpm init delay */
		if (lpm_init_delay_us) {
			s64 delay_us = lpm_init_delay_us -
						ktime_us_delta(ktime_get(), vdd->tx_set_on_time);

			LCD_INFO(vdd, "[Panel LPM][DIPSLAY_%d] %lld us delay before turn on lpm mode\n",
					vdd->ndx, (delay_us < 0) ? 0 : delay_us);
			if (delay_us > 0)
				usleep_range(delay_us, delay_us);
		}

		/* Self Display Setting */
		if (vdd->self_disp.aod_enter)
			vdd->self_disp.aod_enter(vdd);

		if (unlikely(vdd->is_factory_mode))
			ss_send_cmd(vdd, TX_FD_OFF);

		if (!ss_is_panel_lpm(vdd)) {
			LCD_INFO(vdd, "tx lpm_on_pre\n");
			ss_send_cmd(vdd, TX_LPM_ON_PRE);
		}

 		LCD_INFO(vdd, "[Panel LPM][DIPSLAY_%d] Send panel LPM cmds +++\n", vdd->ndx);
		mutex_lock(&vdd->panel_lpm.lpm_bl_lock);
		ss_send_cmd(vdd, TX_LPM_ON_AOR);
		ss_send_cmd(vdd, TX_LPM_ON);
 		LCD_INFO(vdd, "[Panel LPM][DIPSLAY_%d] Send panel LPM cmds ---\n", vdd->ndx);

		if (vdd->panel_lpm.entry_frame)
			ss_wait_for_te_gpio(vdd, vdd->panel_lpm.entry_frame, 0, false);

		if (unlikely(vdd->is_factory_mode)) {
			/* Do not apply self grid in case of factort lpm (conrolled by adb cmd) */
			ss_send_cmd(vdd, TX_DISPLAY_ON);
			/*
			* 1Frame Delay(33.4ms - 30FPS) Should be added for the project that needs FD cmd
			* Swire toggled at TX_FD_OFF, TX_LPM_ON, TX_FD_ON, TX_LPM_OFF.
			* And the on-off sequences in factory binary are like below
			* AOD on : ESD disable -> TX_FD_OFF -> TX_LPM_ON -> need 34ms delay -> ESD enable
			* AOD off : ESD disable -> TX_LPM_OFF -> TX_FD_ON -> need 34ms delay -> ESD enable
			* so we need 34ms delay for TX_LPM_ON & TX_FD_ON respectively.
			* But to reduce flicker issue, Add delay 34ms after 29(display on) cmd.
			*/
			if(!SS_IS_CMDS_NULL(ss_get_cmds(vdd, TX_FD_OFF)) && !vdd->dtsi_data.lpm_swire_delay)
				usleep_range(34*1000, 34*1000);
		} else {
			/* The display_on cmd will be sent on next commit */
			vdd->display_status_dsi.wait_disp_on = true;
			vdd->display_status_dsi.wait_actual_disp_on = true;
			LCD_INFO(vdd, "[Panel LPM][DIPSLAY_%d] Set wait_disp_on to true\n", vdd->ndx);
		}

 		LCD_INFO(vdd, "[Panel LPM][DIPSLAY_%d] panel_state: %d -> LPM\n",
				vdd->ndx, vdd->panel_state);
		ss_set_panel_state(vdd, PANEL_PWR_LPM);
		mutex_unlock(&vdd->panel_lpm.lpm_bl_lock);

		/*
			Update mdnie to disable mdnie operation by scenario at AOD display status.
		*/
		if (vdd->mdnie.support_mdnie) {
			update_dsi_tcon_mdnie_register(vdd);
		}
	} else { /* AOD OFF(Exit) */
		/* Self Display Setting */
		if (vdd->self_disp.aod_exit)
			vdd->self_disp.aod_exit(vdd);

		if (vdd->panel_func.samsung_update_lpm_ctrl_cmd) {
			vdd->panel_func.samsung_update_lpm_ctrl_cmd(vdd, enable);
			LCD_INFO(vdd, "[Panel LPM][DIPSLAY_%d] update lpm cmd done(off)\n", vdd->ndx);
		}

		/* Turn Off ALPM Mode */
		ss_send_cmd(vdd, TX_LPM_OFF);

		/* 1Frame Delay(33.4ms - 30FPS) Should be added */
		usleep_range(34*1000, 34*1000);

		ss_send_cmd(vdd, TX_LPM_OFF_AOR);

		LCD_INFO(vdd, "[Panel LPM][DIPSLAY_%d] Send panel LPM off cmds\n", vdd->ndx);

		ss_set_panel_state(vdd, PANEL_PWR_ON);

		if ((vdd->support_optical_fingerprint) && (vdd->br_info.common_br.finger_mask_hbm_on)) {
			/* SAMSUNG_FINGERPRINT */
			ss_brightness_dcs(vdd, 0, BACKLIGHT_FINGERMASK_ON);
		}
		else
			ss_brightness_dcs(vdd, USE_CURRENT_BL_LEVEL, BACKLIGHT_NORMAL);

		if (vdd->mdnie.support_mdnie) {
			vdd->mdnie.lcd_on_notifiy = true;
			update_dsi_tcon_mdnie_register(vdd);
			if (vdd->mdnie.support_trans_dimming)
				vdd->mdnie.disable_trans_dimming = false;
		}

		if (vdd->panel_lpm.exit_frame)
			ss_wait_for_te_gpio(vdd, vdd->panel_lpm.exit_frame, 0, false);

		if (unlikely(vdd->is_factory_mode)) {
			ss_send_cmd(vdd, TX_FD_ON);
			ss_send_cmd(vdd, TX_DISPLAY_ON);
			ss_set_panel_state(vdd, PANEL_PWR_ON);
			/*
			* 1Frame Delay(33.4ms - 30FPS) Should be added for the project that needs FD cmd
			* Swire toggled at TX_FD_OFF, TX_LPM_ON, TX_FD_ON, TX_LPM_OFF.
			* And the on-off sequences in factory binary are like below
			* AOD on : ESD disable -> TX_FD_OFF -> TX_LPM_ON -> need 34ms delay -> ESD enable
			* AOD off : ESD disable -> TX_LPM_OFF -> TX_FD_ON -> need 34ms delay -> ESD enable
			* so we need 34ms delay for TX_LPM_ON & TX_FD_ON respectively.
			* But to reduce flicker issue, Add delay 34ms after 29(display on) cmd.
			*/
			if(!SS_IS_CMDS_NULL(ss_get_cmds(vdd, TX_FD_ON)) && !vdd->dtsi_data.lpm_swire_delay)
				usleep_range(34*1000, 34*1000);
		} else {
			/* The display_on cmd will be sent on next commit */
			vdd->display_status_dsi.wait_disp_on = true;
			vdd->display_status_dsi.wait_actual_disp_on = true;
			LCD_INFO(vdd, "[Panel LPM][DIPSLAY_%d] Set wait_disp_on to true\n", vdd->ndx);
		}
	}

	/*
	 * Swire toggle cmd could be included in lpm on/off cmd.
	 * (esd enable + swire toggle (en_on toggle) -> esd detection).
	 * In that case, need to add over 1frame delay.
	 */
	if (vdd->dtsi_data.lpm_swire_delay)
		usleep_range(vdd->dtsi_data.lpm_swire_delay*1000,
			vdd->dtsi_data.lpm_swire_delay*1000);

	/* To avoid unexpected ESD detction, Enable ESD irq after cmd tx related with lpm */
	if (vdd->esd_recovery.esd_irq_enable)
		vdd->esd_recovery.esd_irq_enable(true, true, (void *)vdd);

	LCD_INFO(vdd, "[Panel LPM][DIPSLAY_%d] En/Dis : %s, LPM_MODE : %s, Hz : 30Hz, bl_level : %s\n",
				vdd->ndx,
				/* Enable / Disable */
				enable ? "Enable" : "Disable",
				/* Check LPM mode */
				vdd->panel_lpm.mode == ALPM_MODE_ON ? "ALPM" :
				vdd->panel_lpm.mode == HLPM_MODE_ON ? "HLPM" :
				vdd->panel_lpm.mode == LPM_MODE_OFF ? "MODE_OFF" : "UNKNOWN",
				/* Check current brightness level */
				vdd->panel_lpm.lpm_bl_level == LPM_2NIT ? "2NIT" :
				vdd->panel_lpm.lpm_bl_level == LPM_10NIT ? "10NIT" :
				vdd->panel_lpm.lpm_bl_level == LPM_30NIT ? "30NIT" :
				vdd->panel_lpm.lpm_bl_level == LPM_60NIT ? "60NIT" : "UNKNOWN");

	vdd->panel_lpm.during_ctrl = false;

	mutex_unlock(&vdd->panel_lpm.lpm_lock);
	LCD_INFO(vdd, "[Panel LPM][DIPSLAY_%d] --\n", vdd->ndx);

	return;
}

/* This is depricated.. Get vdd pointer not from global variable. */
struct samsung_display_driver_data *ss_get_vdd(enum ss_display_ndx ndx)
{
	if (ndx >= MAX_DISPLAY_NDX || ndx < 0) {
		pr_debug("invalid ndx(%d)\n", ndx);
		return NULL;
	}

	return &vdd_data[ndx];
}

/***************************************************************************************************
*		BRIGHTNESS RELATED FUNCTION.
****************************************************************************************************/
static void set_normal_br_values(struct samsung_display_driver_data *vdd)
{
	int from, end;
	int left, right, p = 0;
	int loop = 0;
	struct candela_map_table *table;

	if (vdd->br_info.common_br.pac)
		table = &vdd->br_info.candela_map_table[PAC_NORMAL][vdd->panel_revision];
	else {
		if(vdd->br_info.common_br.multi_to_one_support)
			table = &vdd->br_info.candela_map_table[MULTI_TO_ONE_NORMAL][vdd->panel_revision];
		else
			table = &vdd->br_info.candela_map_table[NORMAL][vdd->panel_revision];
	}

	if (IS_ERR_OR_NULL(table->cd)) {
		LCD_ERR(vdd, "No candela_map_table.. \n");
		return;
	}

	LCD_DEBUG(vdd, "table size (%d)\n", table->tab_size);

	if (vdd->br_info.common_br.bl_level > table->max_lv)
		vdd->br_info.common_br.bl_level = table->max_lv;

	left = 0;
	right = table->tab_size - 1;

	while (left <= right) {
		loop++;
		p = (left + right) / 2;
		from = table->from[p];
		end = table->end[p];
		LCD_DEBUG(vdd, "[%d] from(%d) end(%d) / %d\n", p, from, end, vdd->br_info.common_br.bl_level);

		if (vdd->br_info.common_br.bl_level >= from && vdd->br_info.common_br.bl_level <= end)
			break;
		if (vdd->br_info.common_br.bl_level < from)
			right = p - 1;
		else
			left = p + 1;
		LCD_DEBUG(vdd, "left(%d) right(%d)\n", left, right);

		if (loop > table->tab_size) {
			LCD_ERR(vdd, "can not find (%d) level in table!\n", vdd->br_info.common_br.bl_level);
			p = table->tab_size - 1;
			break;
		}
	};

	// set values..
	vdd->br_info.common_br.cd_idx = table->idx[p];
	vdd->br_info.common_br.cd_level = table->cd[p];

	if (vdd->br_info.common_br.pac) {
		vdd->br_info.common_br.pac_cd_idx = table->scaled_idx[p];
		vdd->br_info.common_br.interpolation_cd = table->interpolation_cd[p];

		LCD_INFO(vdd, "[%d] pac_cd_idx (%d) cd_idx (%d) cd (%d) interpolation_cd (%d)\n",
				p, vdd->br_info.common_br.pac_cd_idx,
				vdd->br_info.common_br.cd_idx,
				vdd->br_info.common_br.cd_level,
				vdd->br_info.common_br.interpolation_cd);
	} else {
		if (vdd->br_info.common_br.gamma_mode2_support) {
			vdd->br_info.common_br.gm2_wrdisbv = table->gm2_wrdisbv[p];
			LCD_INFO(vdd, "[%d] cd_idx (%d) cd (%d) gm2_wrdisbv (%d)\n",
					p, vdd->br_info.common_br.cd_idx,
					vdd->br_info.common_br.cd_level,
					vdd->br_info.common_br.gm2_wrdisbv);
		} else {
			LCD_INFO(vdd, "[%d] cd_idx (%d) cd (%d) \n",
					p, vdd->br_info.common_br.cd_idx,
					vdd->br_info.common_br.cd_level);
		}
	}
	return;
}

static void set_hbm_br_values(struct samsung_display_driver_data *vdd)
{
	int from, end;
	int left, right, p;
	int loop = 0;
	struct candela_map_table *table;

	if (vdd->br_info.common_br.pac)
		table = &vdd->br_info.candela_map_table[PAC_HBM][vdd->panel_revision];
	else
		table = &vdd->br_info.candela_map_table[HBM][vdd->panel_revision];

	if (IS_ERR_OR_NULL(table->cd)) {
		LCD_ERR(vdd, "No hbm candela_map_table..\n");
		return;
	}

	if (vdd->br_info.common_br.bl_level > table->max_lv)
		vdd->br_info.common_br.bl_level = table->max_lv;

	left = 0;
	right = table->tab_size - 1;

	while (left <= right) {
		loop++;
		p = (left + right) / 2;
		from = table->from[p];
		end = table->end[p];
		LCD_DEBUG(vdd, "[%d] from(%d) end(%d) / %d\n", p, from, end, vdd->br_info.common_br.bl_level);

		if (vdd->br_info.common_br.bl_level >= from && vdd->br_info.common_br.bl_level <= end)
			break;
		if (vdd->br_info.common_br.bl_level < from)
			right = p - 1;
		else
			left = p + 1;
		LCD_DEBUG(vdd, "left(%d) right(%d)\n", left, right);

		if (loop > table->tab_size) {
			LCD_ERR(vdd, "can not find (%d) level in table!\n", vdd->br_info.common_br.bl_level);
			p = table->tab_size - 1;
			break;
		}
	};

	// set values..
	vdd->br_info.common_br.cd_idx = table->idx[p];
	vdd->br_info.common_br.cd_level = table->cd[p];

	if (vdd->br_info.common_br.pac) {
		vdd->br_info.common_br.pac_cd_idx = table->scaled_idx[p];
		vdd->br_info.common_br.interpolation_cd = table->cd[p];

		LCD_INFO(vdd, "[%d] pac_cd_idx (%d) cd_idx (%d) cd (%d) interpolation_cd (%d) \n",
				p, vdd->br_info.common_br.pac_cd_idx,
				vdd->br_info.common_br.cd_idx,
				vdd->br_info.common_br.cd_level,
				vdd->br_info.common_br.interpolation_cd);
	} else {
		if (vdd->br_info.common_br.gamma_mode2_support) {
			vdd->br_info.common_br.gm2_wrdisbv = table->gm2_wrdisbv[p];
			LCD_INFO(vdd, "[%d] cd_idx (%d) cd (%d) gm2_wrdisbv (%d)\n",
					p, vdd->br_info.common_br.cd_idx,
					vdd->br_info.common_br.cd_level,
					vdd->br_info.common_br.gm2_wrdisbv);
		} else {
			LCD_INFO(vdd, "[%d] cd_idx (%d) cd (%d) interpolation_cd (%d) \n",
					p, vdd->br_info.common_br.cd_idx,
					vdd->br_info.common_br.cd_level,
					vdd->br_info.common_br.interpolation_cd);
		}
	}

	return;
}

// HMT brightness
static void set_hmt_br_values(struct samsung_display_driver_data *vdd)
{
	int from, end;
	int left, right, p = 0;
	struct candela_map_table *table;

	table = &vdd->br_info.candela_map_table[HMT][vdd->panel_revision];

	if (IS_ERR_OR_NULL(table->cd)) {
		LCD_ERR(vdd, "No candela_map_table..\n");
		return;
	}

	LCD_DEBUG(vdd, "table size (%d)\n", table->tab_size);

	if (vdd->hmt.bl_level > table->max_lv)
		vdd->hmt.bl_level = table->max_lv;

	left = 0;
	right = table->tab_size - 1;

	while (left <= right) {
		p = (left + right) / 2;
		from = table->from[p];
		end = table->end[p];
		LCD_DEBUG(vdd, "[%d] from(%d) end(%d) / %d\n", p, from, end, vdd->hmt.bl_level);

		if (vdd->hmt.bl_level >= from && vdd->hmt.bl_level <= end)
			break;
		if (vdd->hmt.bl_level < from)
			right = p - 1;
		else
			left = p + 1;
	};

	// for elvess, vint etc.. which are using 74 steps.
	vdd->br_info.common_br.interpolation_cd = vdd->hmt.candela_level = table->cd[p];
	vdd->hmt.cmd_idx = table->idx[p];

	if (vdd->br_info.common_br.gamma_mode2_support) {
		vdd->br_info.common_br.gm2_wrdisbv = table->gm2_wrdisbv[p];
		LCD_INFO(vdd, "[%d] cd_idx (%d) cd (%d) gm2_wrdisbv (%d)\n",
				p, vdd->hmt.cmd_idx,
				vdd->hmt.candela_level,
				vdd->br_info.common_br.gm2_wrdisbv);
	} else {
		LCD_INFO(vdd, "cd_idx (%d) cd_level (%d) \n",
				vdd->hmt.cmd_idx,
				vdd->hmt.candela_level);
	}

	return;
}

static void ss_update_brightness_packet(struct samsung_display_driver_data *vdd,
		struct dsi_cmd_desc *packet, int *count, struct dsi_panel_cmd_set *tx_cmd)
{
	int loop = 0;

	if (IS_ERR_OR_NULL(packet)) {
		LCD_ERR(vdd, "%ps no packet\n", __builtin_return_address(0));
		return;
	}

	if (SS_IS_CMDS_NULL(tx_cmd)) {
		LCD_DEBUG(vdd, "%ps no tx_cmd\n", __builtin_return_address(0));
		return;
	}

	if (*count > (BRIGHTNESS_MAX_PACKET - 1))
		panic("over max brightness_packet size(%d).. !!",
				BRIGHTNESS_MAX_PACKET);

	for (loop = 0; loop < tx_cmd->count; loop++)
		packet[(*count)++] = tx_cmd->cmds[loop];
}

void ss_add_brightness_packet(struct samsung_display_driver_data *vdd,
	enum BR_FUNC_LIST func,	struct dsi_cmd_desc *packet, int *cmd_cnt)
{
	int level_key = 0;
	struct dsi_panel_cmd_set *tx_cmd = NULL;

	if (!vdd->panel_func.br_func[func]) {
		LCD_DEBUG(vdd, "func[%d] is null!!\n", func);
		return;
	}

	level_key = false;
	tx_cmd = vdd->panel_func.br_func[func](vdd, &level_key);

	update_packet_level_key_enable(vdd, packet, cmd_cnt, level_key);
	ss_update_brightness_packet(vdd, packet, cmd_cnt, tx_cmd);
	update_packet_level_key_disable(vdd, packet, cmd_cnt, level_key);

	return;
}

void update_packet_level_key_enable(struct samsung_display_driver_data *vdd,
		struct dsi_cmd_desc *packet, int *cmd_cnt, int level_key)
{
	if (!level_key)
		return;
	else {
		if (level_key & LEVEL0_KEY)
			ss_update_brightness_packet(vdd, packet, cmd_cnt, ss_get_cmds(vdd, TX_LEVEL0_KEY_ENABLE));

		if (level_key & LEVEL1_KEY)
			ss_update_brightness_packet(vdd, packet, cmd_cnt, ss_get_cmds(vdd, TX_LEVEL1_KEY_ENABLE));

		if (level_key & LEVEL2_KEY)
			ss_update_brightness_packet(vdd, packet, cmd_cnt, ss_get_cmds(vdd, TX_LEVEL2_KEY_ENABLE));
	}
}

void update_packet_level_key_disable(struct samsung_display_driver_data *vdd,
		struct dsi_cmd_desc *packet, int *cmd_cnt, int level_key)
{
	if (!level_key)
		return;
	else {
		if (level_key & LEVEL0_KEY)
			ss_update_brightness_packet(vdd, packet, cmd_cnt, ss_get_cmds(vdd, TX_LEVEL0_KEY_DISABLE));

		if (level_key & LEVEL1_KEY)
			ss_update_brightness_packet(vdd, packet, cmd_cnt, ss_get_cmds(vdd, TX_LEVEL1_KEY_DISABLE));

		if (level_key & LEVEL2_KEY)
			ss_update_brightness_packet(vdd, packet, cmd_cnt, ss_get_cmds(vdd, TX_LEVEL2_KEY_DISABLE));
	}
}

static int ss_make_brightness_packet(struct samsung_display_driver_data *vdd, enum BR_TYPE br_type)
{
	int cmd_cnt = 0;
	int level_key = 0;
	struct dsi_panel_cmd_set *set;
	struct dsi_cmd_desc *packet = NULL;

	/* init packet */
	set = ss_get_cmds(vdd, TX_BRIGHT_CTRL);
	if (SS_IS_CMDS_NULL(set)) {
		LCD_ERR(vdd, "No cmds for TX_BRIGHT_CTRL.. \n");
		return -EINVAL;
	}

	packet = set->cmds;

	if (br_type == BR_TYPE_NORMAL)
		set_normal_br_values(vdd);
	else if (br_type == BR_TYPE_HBM)
		set_hbm_br_values(vdd);
	else if (br_type == BR_TYPE_HMT)
		set_hmt_br_values(vdd);
	else
		LCD_ERR(vdd, "undefined br_type (%d)\n", br_type);

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_tot_level)) {
		level_key = vdd->panel_func.samsung_brightness_tot_level(vdd);
		update_packet_level_key_enable(vdd, packet, &cmd_cnt, level_key);
	}

	/* define cmd order in each panel file. */
	if (!IS_ERR_OR_NULL(vdd->panel_func.make_brightness_packet))
		vdd->panel_func.make_brightness_packet(vdd, packet, &cmd_cnt, br_type);
	else
		LCD_ERR(vdd, "undefined make_brightness_packet\n");

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_tot_level)) {
		level_key = vdd->panel_func.samsung_brightness_tot_level(vdd);
		update_packet_level_key_disable(vdd, packet, &cmd_cnt, level_key);
	}

	return cmd_cnt;
}

int ss_single_transmission_packet(struct dsi_panel_cmd_set *cmds)
{
	int loop;
	struct dsi_cmd_desc *packet = cmds->cmds;
	int packet_cnt = cmds->count;

	for (loop = 0; (loop < packet_cnt) && (loop < BRIGHTNESS_MAX_PACKET); loop++) {
		if (packet[loop].msg.type == MIPI_DSI_DCS_LONG_WRITE ||
			packet[loop].msg.type == MIPI_DSI_GENERIC_LONG_WRITE) {
			packet[loop].last_command = false;
		} else {
			if (loop > 0)
				packet[loop - 1].last_command = true; /*To ensure previous single tx packet */

			packet[loop].last_command = true;
		}

		/* use null packet for last_command in brightness setting */
		if (packet[loop].ss_txbuf[0] == 0x0)
			packet[loop].last_command = true;
	}

	if (loop == BRIGHTNESS_MAX_PACKET)
		return false;
	else {
		packet[loop - 1].last_command = true; /* To make last packet flag */
		return true;
	}
}

bool is_hbm_level(struct samsung_display_driver_data *vdd)
{
	struct candela_map_table *table;

	if (vdd->br_info.common_br.pac)
		table = &vdd->br_info.candela_map_table[PAC_HBM][vdd->panel_revision];
	else
		table = &vdd->br_info.candela_map_table[HBM][vdd->panel_revision];

	/* Normal brightness */
	if (vdd->br_info.common_br.bl_level < table->min_lv)
		return false;

	/* max should not be zero */
	if (!table->max_lv)
		return false;

	if (vdd->br_info.common_br.bl_level > table->max_lv) {
		LCD_ERR(vdd, "bl_level(%d) is over max_level (%d), force set to max\n", vdd->br_info.common_br.bl_level, table->max_lv);
		vdd->br_info.common_br.bl_level = table->max_lv;
	}
	return true;
}

/* ss_brightness_dcs() is called not in locking status.
 * Instead, calls ss_set_backlight() when you need to controll backlight
 * in locking status.
 */
int ss_brightness_dcs(struct samsung_display_driver_data *vdd, int level, int backlight_origin)
{
	int cmd_cnt = 0;
	int ret = 0;
	int need_lpm_lock = 0;
	struct dsi_panel_cmd_set *brightness_cmds = NULL;
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);
	static int backup_bl_level, backup_acl;

	/* Bloom project TEMP CODE
	 * This code will remove.
	 */
	{
		char panel_name[MAX_CMDLINE_PARAM_LEN];
		char panel_string[] = "ss_dsi_panel_S6E3HA9_AMF670UH01_FHD";

		ss_get_primary_panel_name_cmdline(panel_name);

		// bloom bringup panel
		if (!strncmp(panel_string, panel_name, strlen(panel_string))) {
			if (level != USE_CURRENT_BL_LEVEL)
				level *= 100;
		}
	}

#if IS_ENABLED(CONFIG_ARCH_LAHAINA)
	/* On GSI build, module init could be called after on init. so set low level to avoid current onsumption */
#if !IS_ENABLED(CONFIG_QGKI) && !IS_ENABLED(CONFIG_UML)
	level = 83;
#endif
#endif

	/* FC2 change: set panle mode in SurfaceFlinger initialization, instead of kenrel booting... */
	if (!panel->cur_mode) {
		LCD_ERR(vdd, "err: no panel mode yet...\n");
		return -EINVAL;
	}

	/*
	 * panel_lpm.lpm_lock should be locked
	 * to avoid brightness mis-handling (LPM brightness <-> normal brightness) from other thread
	 * but running ss_panel_lpm_ctrl thread
	*/
	if (ss_is_panel_lpm(vdd)) {
		need_lpm_lock = 1;
		mutex_lock(&vdd->panel_lpm.lpm_lock);
	}

	/* check BRR_STOP_MODE before bl_lock to avoid deadlock and BRR_OFF
	 *  wait-timeout, with ss_panel_vrr_switch() funciton.
	 * TODO: compare candela, instead of bl_level.
	 */
	if (vdd->vrr.is_support_brr &&
			level != USE_CURRENT_BL_LEVEL &&
			vdd->br_info.common_br.bl_level != level) {
		/* check brightness setting, and stop bridge RR, and jump to target RR. */
		mutex_lock(&vdd->vrr.brr_lock);
		if (ss_is_brr_on(vdd->vrr.brr_mode)) {
			int max_cnt = 100; /* 100ms */

			vdd->vrr.brr_mode = BRR_STOP_MODE;
			vdd->vrr.brr_bl_level = level;
			mutex_unlock(&vdd->vrr.brr_lock);

			LCD_INFO(vdd, "VRR: new brightness(%d->%d), set to BRR_STOP_MODE\n",
					vdd->br_info.common_br.bl_level, level);

			while (vdd->vrr.brr_mode != BRR_OFF_MODE && max_cnt--)
				usleep_range(1000, 1000);

			if (max_cnt < 0)
				LCD_ERR(vdd, "BRR OFF timeout: (%s)\n", ss_get_brr_mode_name(vdd->vrr.brr_mode));
		} else {
			mutex_unlock(&vdd->vrr.brr_lock);
		}
	}

	mutex_lock(&vdd->panel_lpm.lpm_bl_lock);

	// need bl_lock..
	mutex_lock(&vdd->bl_lock);

	if (vdd->support_optical_fingerprint) {
		/* SAMSUNG_FINGERPRINT */
		/* From nomal at finger mask on state
		* 1. backup acl
		* 2. backup level if it is not the same value as vdd->br_info.common_br.bl_level
		*   note. we don't need to backup this case because vdd->br_info.common_br.bl_level is a finger_mask_bl_level now
		*/

		/* finger mask hbm on & bl update from normal */
		if ((vdd->br_info.common_br.finger_mask_hbm_on) && (backlight_origin == BACKLIGHT_NORMAL)) {
			backup_acl = vdd->br_info.acl_status;
			if (level != USE_CURRENT_BL_LEVEL)
				backup_bl_level = level;
			LCD_INFO(vdd, "[FINGER_MASK]BACKLIGHT_NORMAL save backup_acl = %d, backup_level = %d, vdd->br_info.common_br.bl_level=%d\n",
				backup_acl, backup_bl_level, vdd->br_info.common_br.bl_level);
			goto skip_bl_update;
		}

		/* From BACKLIGHT_FINGERMASK_ON
		*   1. use finger_mask_bl_level(HBM) & acl 0
		*   2. backup previous bl_level & acl value
		*  From BACKLIGHT_FINGERMASK_OFF
		*   1. restore backup bl_level & acl value
		*  From BACKLIGHT_FINGERMASK_ON_SUSTAIN
		*   1. brightness update with finger_bl_level.
		*/

		if (backlight_origin == BACKLIGHT_FINGERMASK_ON) {
			vdd->br_info.common_br.finger_mask_hbm_on = true;
			backup_acl = vdd->br_info.acl_status;

			if (vdd->finger_mask_updated) /* do not backup br.bl_level at on to on */
				backup_bl_level = vdd->br_info.common_br.bl_level;
			level = vdd->br_info.common_br.finger_mask_bl_level;
			vdd->br_info.acl_status = 0;

			LCD_INFO(vdd, "[FINGER_MASK]BACKLIGHT_FINGERMASK_ON turn on finger hbm & back up acl = %d, level = %d\n",
				backup_acl, backup_bl_level);
		}
		else if(backlight_origin == BACKLIGHT_FINGERMASK_OFF) {
			vdd->br_info.common_br.finger_mask_hbm_on = false;
			vdd->br_info.acl_status = backup_acl;
			level = backup_bl_level;

			LCD_INFO(vdd, "[FINGER_MASK]BACKLIGHT_FINGERMASK_OFF turn off finger hbm & restore acl = %d, level = %d\n",
				vdd->br_info.acl_status, level);
		}
		else if(backlight_origin == BACKLIGHT_FINGERMASK_ON_SUSTAIN) {
			LCD_INFO(vdd, "[FINGER_MASK]BACKLIGHT_FINGERMASK_ON_SUSTAIN \
				send finger hbm & back up acl = %d, level = %d->%d\n",
				vdd->br_info.acl_status, level,vdd->br_info.common_br.finger_mask_bl_level);
			level = vdd->br_info.common_br.finger_mask_bl_level;
		}
	}

	/* store bl level from PMS */
	if (level != USE_CURRENT_BL_LEVEL)
		vdd->br_info.common_br.bl_level = level;

	if (!ss_panel_attached(vdd->ndx)) {
		ret = -EINVAL;
		goto skip_bl_update;
	}

	if (vdd->grayspot) {
		LCD_INFO(vdd, "grayspot is [%d], skip bl update\n", vdd->grayspot);
		ret = -EINVAL;
		goto skip_bl_update;
	}

	if (vdd->br_info.flash_gamma_support && !vdd->br_info.flash_gamma_init_done) {
		LCD_ERR(vdd, "flash_gamme is not ready, skip bl update\n");
		ret = -EINVAL;
		goto skip_bl_update;
	}

	if (vdd->brightdot_state) {
		/* During brightdot test, prevent whole brightntess setting,
		 * which changes brightdot setting.
		 * BIT0: brightdot test, BIT1: brightdot test in LFD 0.5hz
		 * allow brightness update in both brightdot test off case
		 */
		LCD_INFO(vdd, "brightdot test on(%d), skip bl update\n", vdd->brightdot_state);
		ret = -EINVAL;
		goto skip_bl_update;
	}

	if (vdd->panel_lpm.is_support && is_new_lpm_version(vdd)) {
		set_lpm_br_values(vdd);

		if (ss_is_panel_lpm(vdd)) {
			LCD_INFO(vdd, "[Panel LPM]: set brightness.(%d level)->(%d cd)\n", vdd->br_info.common_br.bl_level, vdd->panel_lpm.lpm_bl_level);

			if (vdd->panel_func.samsung_set_lpm_brightness)
				vdd->panel_func.samsung_set_lpm_brightness(vdd);
			goto skip_bl_update;
		}
	}

	if (!is_new_lpm_version(vdd) && ss_is_panel_lpm(vdd)) {
		LCD_ERR(vdd, "error: AOD service didn't set proper LPM mode " \
			"before enter LPM. bl: %d, lpm_bl: %d, lpm_mode: %d, ver: %d\n",
				vdd->br_info.common_br.bl_level,
				vdd->panel_lpm.lpm_bl_level,
				vdd->panel_lpm.mode,
				vdd->panel_lpm.ver);

		LCD_ERR(vdd, "ver: %d, skip brightness setting to prevent false ESD detection\n", vdd->panel_lpm.ver);
		goto skip_bl_update;
	}

	if (vdd->hmt.is_support && vdd->hmt.hmt_on) {
		LCD_ERR(vdd, "HMT is on. do not set normal brightness..(%d)\n", level);
		ret = -EINVAL;
		goto skip_bl_update;
	}

	if (ss_is_seamless_mode(vdd)) {
		LCD_ERR(vdd, "splash is not done..\n");
		ret = -EINVAL;
		goto skip_bl_update;
	}

	/* make NORMAL/HBM brightness cmd packet */
	if (is_hbm_level(vdd) && !vdd->dtsi_data.tft_common_support) {
		cmd_cnt = ss_make_brightness_packet(vdd, BR_TYPE_HBM);
		cmd_cnt > 0 ? vdd->br_info.is_hbm = true : false;
	} else {
		cmd_cnt = ss_make_brightness_packet(vdd, BR_TYPE_NORMAL);
		cmd_cnt > 0 ? vdd->br_info.is_hbm = false : false;
	}

	if (cmd_cnt > 0) {
		/* setting tx cmds cmt */
		brightness_cmds = ss_get_cmds(vdd, TX_BRIGHT_CTRL);
		brightness_cmds->count = cmd_cnt;

		/* sending tx cmds */
		if (ss_single_transmission_packet(brightness_cmds)) {	/* generate single tx packet */
			if (vdd->need_brightness_lock) {
				int locked = 0;
				int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */

				if(!mutex_trylock(&vdd->exclusive_tx.ex_tx_lock)) {
					locked = 1;
				}
				vdd->exclusive_tx.enable = 1;
				while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
					usleep_range(500, 500);

				vdd->exclusive_tx.permit_frame_update = 0;
				ss_set_exclusive_tx_packet(vdd, TX_BRIGHT_CTRL, 1);

				ss_send_cmd(vdd, TX_BRIGHT_CTRL);

				ss_set_exclusive_tx_packet(vdd, TX_BRIGHT_CTRL, 0);
				vdd->exclusive_tx.enable = 0;
				wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);
				if(!locked)
					mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);
			} else {
				LCD_INFO(vdd, "tx bl cmd +\n");
				ss_send_cmd(vdd, TX_BRIGHT_CTRL);
				LCD_INFO(vdd, "tx bl cmd -\n");
			}

			/* copr sum after changing brightness to calculate brightness avg. */
			if (vdd->copr.copr_on) {
				ss_set_copr_sum(vdd, COPR_CD_INDEX_0);
				ss_set_copr_sum(vdd, COPR_CD_INDEX_1);
			}

			/* send poc compensation */
			if (vdd->poc_driver.is_support)
				ss_poc_comp(vdd);

			LCD_INFO(vdd, "level : %d  candela : %dCD hbm : %d itp : %s\n",
				vdd->br_info.common_br.bl_level, vdd->br_info.common_br.cd_level, vdd->br_info.is_hbm,
				vdd->br_info.panel_br_info.itp_mode == 0 ? "TABLE" : "FLASH");

			ss_notify_queue_work(vdd, PANEL_EVENT_BL_CHANGED);
		} else
			LCD_INFO(vdd, "single_transmission_fail error\n");

		/* Boost_en early off when pwm 0bl_leve
		 * Condition : vdd->boost_early_off is true
		 * To prevent reverse-voltage loading at output of boost IC
		 */
		if (vdd->dtsi_data.tft_common_support && vdd->boost_early_off
			&& !vdd->br_info.common_br.bl_level) {
			if (vdd->panel_func.samsung_boost_en_control)
				vdd->panel_func.samsung_boost_en_control(vdd, false);
		}

	} else
		LCD_INFO(vdd, "level : %d skip, cmd_cnt is 0\n", vdd->br_info.common_br.bl_level);

skip_bl_update:
	if (vdd->support_optical_fingerprint) {
	/* SAMSUNG_FINGERPRINT */
	/* hbm needs vdd->panel_hbm_entry_delay TE to be updated, where as normal needs no */
		if (backlight_origin == BACKLIGHT_FINGERMASK_ON) {
			if (panel->panel_mode == DSI_OP_CMD_MODE)
				ss_wait_for_te_gpio(vdd, vdd->panel_hbm_entry_delay, 200, true);
			else /* Video mode*/
				ss_wait_for_vsync(vdd, vdd->panel_hbm_entry_delay, 0);
		}
		if ((backlight_origin >= BACKLIGHT_FINGERMASK_ON) && (vdd->finger_mask_updated)) {
			SDE_ERROR("finger_mask_updated/ sysfs_notify finger_mask_state = %d\n", vdd->finger_mask);
			sysfs_notify(&vdd->lcd_dev->dev.kobj, NULL, "actual_mask_brightness");
			vdd->finger_mask_updated = 0;
		}
		if (backlight_origin == BACKLIGHT_FINGERMASK_OFF) {
			if (panel->panel_mode == DSI_OP_CMD_MODE)
				ss_wait_for_te_gpio(vdd, vdd->panel_hbm_exit_delay, 200, true);
			else
				ss_wait_for_vsync(vdd, vdd->panel_hbm_exit_delay, 0);
		}
	}
	mutex_unlock(&vdd->bl_lock);

	mutex_unlock(&vdd->panel_lpm.lpm_bl_lock);

	if (need_lpm_lock)
		mutex_unlock(&vdd->panel_lpm.lpm_lock);

	return ret;
}

int ss_brightness_dcs_hmt(struct samsung_display_driver_data *vdd,
		int level)
{
	struct dsi_panel_cmd_set *set = NULL;
	int cmd_cnt;
	int ret = 0;

	set = ss_get_cmds(vdd, TX_BRIGHT_CTRL);
	if (SS_IS_CMDS_NULL(set)) {
		LCD_ERR(vdd, "No cmds for TX_BRIGHT_CTRL.. \n");
		return -EINVAL;
	}

	vdd->hmt.bl_level = level;
	LCD_INFO(vdd, "[HMT] hmt_bl_level(%d)\n", vdd->hmt.bl_level);

	cmd_cnt = ss_make_brightness_packet(vdd, BR_TYPE_HMT);
	if (cmd_cnt) {
		/* setting tx cmds cmt */
		set->count = cmd_cnt;

		/* generate single tx packet */
		ret = ss_single_transmission_packet(set);
		if (ret) {
			ss_send_cmd(vdd, TX_BRIGHT_CTRL);

			LCD_INFO(vdd, "idx(%d), cd_level(%d), hmt_bl_level(%d) itp : %s",
				vdd->hmt.cmd_idx, vdd->hmt.candela_level, vdd->hmt.bl_level,
				vdd->br_info.panel_br_info.itp_mode == 0 ? "TABLE" : "FLASH");
		} else
			LCD_DEBUG(vdd, "single_transmission_fail error\n");
	} else
		LCD_INFO(vdd, "level : %d skip\n", vdd->br_info.common_br.bl_level);

	return cmd_cnt;
}

// TFT brightness
void ss_brightness_tft_pwm(struct samsung_display_driver_data *vdd, int level)
{
	if (vdd == NULL) {
		LCD_ERR(vdd, "no PWM\n");
		return;
	}

	if (ss_is_panel_off(vdd))
		return;

	vdd->br_info.common_br.bl_level = level;

	if (vdd->panel_func.samsung_brightness_tft_pwm)
		vdd->panel_func.samsung_brightness_tft_pwm(vdd, level);
}

void ss_tft_autobrightness_cabc_update(struct samsung_display_driver_data *vdd)
{
#if 0
	LCD_INFO(vdd, "\n");

	switch (vdd->br_info.common_br.auto_level) {
	case 0:
		ss_cabc_update(vdd);
		break;
	case 1:
	case 2:
	case 3:
	case 4:
		ss_send_cmd(vdd, TX_CABC_ON);
		break;
	case 5:
	case 6:
		ss_send_cmd(vdd, TX_CABC_OFF);
		break;
	}
#endif
}

void ss_read_mtp(struct samsung_display_driver_data *vdd, int addr, int len, int pos, u8 *buf)
{
	struct dsi_panel_cmd_set *rx_cmds;
	int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "no vdd");
		return;
	}

	if (!ss_is_ready_to_send_cmd(vdd)) {
		LCD_ERR(vdd, "Panel is not ready. Panel State(%d)\n", vdd->panel_state);
		return;
	}

	if (vdd->two_byte_gpara) {
		if (addr > 0xFF || pos > 0xFFFF || len > 0xFF) {
			LCD_ERR(vdd, "unvalind para addr(%x) pos(%d) len(%d)\n", addr, pos, len);
			return;
		}
	} else {
		if (addr > 0xFF || pos > 0xFF || len > 0xFF) {
			LCD_ERR(vdd, "unvalind para addr(%x) pos(%d) len(%d)\n", addr, pos, len);
			return;
		}
	}

	rx_cmds = ss_get_cmds(vdd, RX_MTP_READ_SYSFS);
	if (SS_IS_CMDS_NULL(rx_cmds)) {
		LCD_ERR(vdd, "No cmds for RX_MTP_READ_SYSFS.. \n");
		return;
	}

	rx_cmds->cmds[0].ss_txbuf[0] =  addr;
	rx_cmds->cmds[0].msg.rx_len =  len;
	rx_cmds->read_startoffset = pos;

	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.permit_frame_update = 1;
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);

	ss_set_exclusive_tx_packet(vdd, RX_MTP_READ_SYSFS, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_ENABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_DISABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL1_KEY_ENABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL1_KEY_DISABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL2_KEY_ENABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL2_KEY_DISABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 1);

	ss_panel_data_read(vdd, RX_MTP_READ_SYSFS, buf, LEVEL0_KEY | LEVEL1_KEY | LEVEL2_KEY);

	ss_set_exclusive_tx_packet(vdd, RX_MTP_READ_SYSFS, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_ENABLE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_DISABLE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL1_KEY_ENABLE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL1_KEY_DISABLE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL2_KEY_ENABLE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL2_KEY_DISABLE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 0);

	vdd->exclusive_tx.permit_frame_update = 0;
	vdd->exclusive_tx.enable = 0;
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);

	return;
}

void ss_write_mtp(struct samsung_display_driver_data *vdd, int len, u8 *buf)
{
	struct dsi_panel_cmd_set *tx_cmds;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "no vdd");
		return;
	}

	if (!ss_is_ready_to_send_cmd(vdd)) {
		LCD_ERR(vdd, "Panel is not ready. Panel State(%d)\n", vdd->panel_state);
		return;
	}

	if (len > 0xFF) {
		LCD_ERR(vdd, "unvalind para len(%d)\n", len);
		return;
	}

	tx_cmds = ss_get_cmds(vdd, TX_MTP_WRITE_SYSFS);
	if (SS_IS_CMDS_NULL(tx_cmds)) {
		LCD_ERR(vdd, "No cmds for TX_MTP_WRITE_SYSFS.. \n");
		return;
	}

	tx_cmds->cmds[0].msg.tx_len = len;
	tx_cmds->cmds[0].ss_txbuf = buf;
	tx_cmds->cmds[0].msg.tx_buf = buf;

	ss_send_cmd(vdd, TX_LEVEL0_KEY_ENABLE);
	ss_send_cmd(vdd, TX_LEVEL1_KEY_ENABLE);
	ss_send_cmd(vdd, TX_LEVEL2_KEY_ENABLE);

	ss_send_cmd(vdd, TX_MTP_WRITE_SYSFS);

	ss_send_cmd(vdd, TX_LEVEL2_KEY_DISABLE);
	ss_send_cmd(vdd, TX_LEVEL1_KEY_DISABLE);
	ss_send_cmd(vdd, TX_LEVEL0_KEY_DISABLE);

	return;
}

static BLOCKING_NOTIFIER_HEAD(panel_notifier_list);

/**
 *	ss_panel_notifier_register - register a client notifier
 *	@nb: notifier block to callback on events
 */
int ss_panel_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&panel_notifier_list, nb);
}
EXPORT_SYMBOL(ss_panel_notifier_register);

/**
 *	ss_panel_notifier_unregister - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int ss_panel_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&panel_notifier_list, nb);
}
EXPORT_SYMBOL(ss_panel_notifier_unregister);

/**
 * panel_notifier_call_chain - notify clients
 *
 */
int ss_panel_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&panel_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(ss_panel_notifier_call_chain);

__visible_for_testing void ss_bl_event_work(struct work_struct *work)
{
	struct samsung_display_driver_data *vdd = NULL;
	struct panel_bl_event_data bl_evt_data;

	vdd = container_of(work, struct samsung_display_driver_data, bl_event_work);

	bl_evt_data.bl_level = vdd->br_info.common_br.bl_level;
	bl_evt_data.aor_data = vdd->br_info.common_br.aor_data;
	bl_evt_data.finger_mask_hbm_on = vdd->br_info.common_br.finger_mask_hbm_on;
	bl_evt_data.display_idx = vdd->ndx;
	bl_evt_data.acl_status = vdd->br_info.acl_status;

	LCD_DEBUG(vdd, "%d %d %d ++\n", bl_evt_data.bl_level, bl_evt_data.aor_data, bl_evt_data.acl_status);
	ss_panel_notifier_call_chain(PANEL_EVENT_BL_CHANGED, &bl_evt_data);
	LCD_DEBUG(vdd, "--\n");

	return;
}

__visible_for_testing void ss_vrr_event_work(struct work_struct *work)
{
        struct samsung_display_driver_data *vdd =
		container_of(work, struct samsung_display_driver_data, vrr_event_work);
	struct vrr_info *vrr = NULL;
	struct panel_dms_data dms_data;
	char vrr_mode[16];

        if (IS_ERR_OR_NULL(vdd)) {
                LCD_ERR(vdd, "no vdd");
                return;
        }
	vrr = &vdd->vrr;

	if (vrr->param_update) {
		snprintf(vrr_mode, sizeof(vrr_mode), "%dX%d:%s\n",
			vrr->adjusted_h_active, vrr->adjusted_v_active, vrr->adjusted_sot_hs_mode ?
			(vrr->adjusted_phs_mode ? "PHS" : "HS") : "NOR");
#if IS_ENABLED(CONFIG_SEC_PARAM)
		if(!sec_set_param(param_index_VrrStatus, &vrr_mode)) {
			LCD_ERR(vdd, "Fail to set vrr_mode param\n");
			return;
		}
#endif
		LCD_DEBUG(vdd, " fps(%d) / vrr_mode = %s\n", vrr->adjusted_refresh_rate, vrr_mode);
	}

	dms_data.fps = vrr->adjusted_refresh_rate;
	dms_data.display_idx = vdd->ndx;

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_lfd_get_base_val)) {
		dms_data.lfd_min_freq = DIV_ROUND_UP(vrr->lfd.base_rr, vrr->lfd.min_div);
		dms_data.lfd_max_freq = DIV_ROUND_UP(vrr->lfd.base_rr, vrr->lfd.max_div);
	} else {
		dms_data.lfd_min_freq = dms_data.fps;
		dms_data.lfd_max_freq = dms_data.fps;
	}

	LCD_INFO(vdd, "fps=%d, lfd_min=%dhz(%d), lfd_max=%dhz(%d), base_rr=%dhz ++\n",
			dms_data.fps,
			dms_data.lfd_min_freq, vrr->lfd.min_div,
			dms_data.lfd_max_freq, vrr->lfd.max_div,
			vrr->lfd.base_rr);

	ss_panel_notifier_call_chain(PANEL_EVENT_VRR_CHANGED, &dms_data);
	LCD_INFO(vdd, "--\n");

	return;
}

__visible_for_testing void ss_lfd_event_work(struct work_struct *work)
{
        struct samsung_display_driver_data *vdd =
		container_of(work, struct samsung_display_driver_data, lfd_event_work);
	struct vrr_info *vrr;
	struct panel_dms_data dms_data;

        if (IS_ERR_OR_NULL(vdd)) {
                LCD_ERR(vdd, "no vdd");
                return;
        }
	vrr = &vdd->vrr;

	dms_data.fps = vrr->adjusted_refresh_rate;
	dms_data.lfd_min_freq = DIV_ROUND_UP(vrr->lfd.base_rr, vrr->lfd.min_div);
	dms_data.lfd_max_freq = DIV_ROUND_UP(vrr->lfd.base_rr, vrr->lfd.max_div);
	dms_data.display_idx = vdd->ndx;

	LCD_INFO(vdd, "fps=%d, lfd_min=%d, lfd_max=%d ++\n", dms_data.fps,
			dms_data.lfd_min_freq, dms_data.lfd_max_freq);
	ss_panel_notifier_call_chain(PANEL_EVENT_LFD_CHANGED, &dms_data);
	LCD_INFO(vdd, "--\n");

	return;
}

__visible_for_testing void ss_panel_state_event_work(struct work_struct *work)
{
	struct samsung_display_driver_data *vdd = NULL;
	struct panel_state_data state_data;

	vdd = container_of(work, struct samsung_display_driver_data, panel_state_event_work);

	switch(vdd->panel_state) {
		case PANEL_PWR_ON:
			state_data.state = PANEL_ON;
			break;
		case PANEL_PWR_OFF:
			state_data.state = PANEL_OFF;
			break;
		case PANEL_PWR_LPM:
			state_data.state = PANEL_LPM;
			break;
		default:
			state_data.state = MAX_PANEL_STATE;
			break;
	}

	state_data.display_idx = vdd->ndx;
	LCD_DEBUG(vdd, "%d ++\n", state_data.state);
	ss_panel_notifier_call_chain(PANEL_EVENT_STATE_CHANGED, &state_data);
	LCD_DEBUG(vdd, "--\n");

	return;
}

__visible_for_testing void ss_test_mode_event_work(struct work_struct *work)
{
	struct samsung_display_driver_data *vdd =
	container_of(work, struct samsung_display_driver_data, test_mode_event_work);
	struct panel_test_mode_data test_mode_data;

	if (IS_ERR_OR_NULL(vdd)) {
	    LCD_ERR(vdd, "no vdd");
	    return;
	}

	switch(vdd->test_mode_state) {
		case PANEL_TEST_GCT:
			test_mode_data.state = TEST_GCT;
			break;
		default:
			test_mode_data.state = TEST_NONE;
			break;
	}

	test_mode_data.display_idx = vdd->ndx;
	LCD_INFO(vdd, "test_mode_state = %d \n", test_mode_data.state);

	ss_panel_notifier_call_chain(PANEL_EVENT_TEST_MODE_CHANGED, &test_mode_data);

	return;
}

__visible_for_testing void ss_screen_mode_event_work(struct work_struct *work)
{
	struct samsung_display_driver_data *vdd =
	container_of(work, struct samsung_display_driver_data, screen_mode_event_work);
	struct mdnie_lite_tun_type *tune = NULL;
	struct panel_screen_mode_data screen_mode_data;

	if (IS_ERR_OR_NULL(vdd)) {
	    LCD_ERR(vdd, "no vdd");
	    return;
	}

	tune = vdd->mdnie.mdnie_tune_state_dsi;

	screen_mode_data.display_idx = vdd->ndx;
	screen_mode_data.mode = tune->mdnie_mode;

	LCD_INFO(vdd, "screen_mode = %d \n", screen_mode_data.mode);

	ss_panel_notifier_call_chain(PANEL_EVENT_SCREEN_MODE_CHANGED, &screen_mode_data);

	return;
}

__visible_for_testing void ss_esd_event_work(struct work_struct *work)
{
	struct samsung_display_driver_data *vdd =
		container_of(work, struct samsung_display_driver_data, esd_event_work);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "no vdd");
		return;
	}

	LCD_INFO(vdd, "noity ESD\n");

	ss_panel_notifier_call_chain(PANEL_EVENT_ESD, NULL);
}

int ss_notify_queue_work(struct samsung_display_driver_data *vdd,
	enum panel_notifier_event_t event)
{
	int ret = 0;

	mutex_lock(&vdd->notify_lock);

	LCD_DEBUG(vdd, "++ %d\n", event);

	switch (event) {
		case PANEL_EVENT_BL_CHANGED:
			/* notify clients of brightness change */
			queue_work(vdd->notify_wq, &vdd->bl_event_work);
			break;
		case PANEL_EVENT_VRR_CHANGED:
			queue_work(vdd->notify_wq, &vdd->vrr_event_work);
			break;
		case PANEL_EVENT_LFD_CHANGED:
			queue_work(vdd->notify_wq, &vdd->lfd_event_work);
			break;
		case PANEL_EVENT_STATE_CHANGED:
			queue_work(vdd->notify_wq, &vdd->panel_state_event_work);
			break;
		case PANEL_EVENT_TEST_MODE_CHANGED:
			queue_work(vdd->notify_wq, &vdd->test_mode_event_work);
			break;
		case PANEL_EVENT_SCREEN_MODE_CHANGED:
			queue_work(vdd->notify_wq, &vdd->screen_mode_event_work);
			break;
		case PANEL_EVENT_ESD:
			queue_work(vdd->notify_wq, &vdd->esd_event_work);
			break;
		default:
			LCD_ERR(vdd, "unkown panel notify event %d\n", event);
			ret = -EINVAL;
			break;
	}

	LCD_DEBUG(vdd, "-- %d\n", event);

	mutex_unlock(&vdd->notify_lock);

	return ret;
}

bool ss_is_brr_on(enum SS_BRR_MODE brr_mode)
{
	bool is_brr_on = false;

	if (brr_mode < MAX_BRR_ON_MODE)
		is_brr_on = true;

	return is_brr_on;
}

static enum SS_BRR_MODE ss_get_brr_mode(struct samsung_display_driver_data *vdd)
{
	struct vrr_info *vrr = &vdd->vrr;
	int cur_rr = vrr->cur_refresh_rate;
	bool cur_hs = vrr->cur_sot_hs_mode;
	int new_rr = vrr->adjusted_refresh_rate;
	bool new_hs = vrr->adjusted_sot_hs_mode;
	int candela = vdd->br_info.common_br.cd_level;
	enum SS_BRR_MODE brr_mode = BRR_OFF_MODE;

	if (!vdd->vrr.is_support_brr)
		return BRR_OFF_MODE;

	if (cur_hs != new_hs)
		return BRR_OFF_MODE;

	/* set BRR mode */
	if (cur_rr == 48 && new_rr == 60)
		brr_mode = BRR_48_60;
	else if (cur_rr == 60 &&  new_rr == 48)
		brr_mode = BRR_60_48;
	else if (cur_rr == 96 && new_rr == 120)
		brr_mode = BRR_96_120;
	else if (cur_rr == 120 && new_rr == 96)
		brr_mode = BRR_120_96;
	else if (cur_rr == 60 && new_rr == 120)
		brr_mode = BRR_60HS_120;
	else if (cur_rr == 120 && new_rr == 60)
		brr_mode = BRR_120_60HS;
	else if (cur_rr == 60 && new_rr == 96)
		brr_mode = BRR_60HS_96;
	else if (cur_rr == 96 && new_rr == 60)
		brr_mode = BRR_96_60HS;
	else
		brr_mode = BRR_OFF_MODE;

	/* Out of allowed brightness level case, turn off BRR mode */
	if (brr_mode < MAX_BRR_ON_MODE) {
		struct vrr_bridge_rr_tbl *brr = &vrr->brr_tbl[brr_mode];
		if (candela < brr->min_cd  || candela > brr->max_cd) {
			LCD_INFO(vdd, "out of BRR brightness range(%d~%d): cur: %d\n",
					brr->min_cd, brr->max_cd, candela);
			brr_mode = BRR_OFF_MODE;
		}
	}

	return brr_mode;
}

/* delayed gamma update after sleep out */
/* sleep out -> 20ms -> FCh and etc... -> 130ms -> gamma update (F7h) */
#define DELAY_TIME_SLEEPOUT_GAMMA_UPDATE	150

/* delayed work doesn't have precise timer. trigger work early, and wait 150ms after sleep out */
#define DELAYED_WORK_GAMMA_UPDATE_TIME_MS	100

#define SDE_PERF_MODE_NORMAL	(0)	/* refer to enum sde_perf_mode */

__visible_for_testing int ss_panel_vrr_switch(struct vrr_info *vrr)
{
	struct samsung_display_driver_data *vdd =
		container_of(vrr, struct samsung_display_driver_data, vrr);
	struct drm_device *ddev = GET_DRM_DEV(vdd);
	struct vrr_bridge_rr_tbl *brr;
	int step;
	int direction;
	int interval_ms;
	s64 wait_ms_wqhd_hs;

	int cur_rr = vrr->cur_refresh_rate;
	bool cur_hs = vrr->cur_sot_hs_mode;
	bool cur_phs = vrr->cur_phs_mode;
	int adjusted_rr = vrr->adjusted_refresh_rate;
	bool adjusted_hs = vrr->adjusted_sot_hs_mode;
	bool adjusted_phs = vrr->adjusted_phs_mode;

	int fps_min = (cur_rr < adjusted_rr) ? cur_rr : adjusted_rr;
	s64 DELAY_TIME_WQHD_HS_VRR  = (1000 / fps_min) + 2;
	bool is_hs_change = !!(cur_hs != adjusted_hs);
	char trace_name[128];
	int ret = 0;

	mutex_lock(&vrr->vrr_lock);
	vrr->running_vrr = true;
	vrr->running_vrr_mdp = false;

	snprintf(trace_name, 128, "VRR: %d->%d", cur_rr, adjusted_rr);
	SDE_ATRACE_BEGIN(trace_name);
	LCD_INFO(vdd, "+++ VRR: %d%s -> %d%s\n",
			cur_rr, cur_hs ? (cur_phs ? "PHS" : "HS") : "NM",
			adjusted_rr, adjusted_hs ?  (adjusted_phs ? "PHS" : "HS") : "NM");

	if (vrr->hs_nm_seq == HS_NM_MULTI_RES_FIRST) {
		// TODO: check: if new dms ioctl change this nm_seq?? add lock?
		// This rarely occurs, so doesn't require lock..
		vrr->hs_nm_seq = HS_NM_OFF;

		wait_ms_wqhd_hs = DELAY_TIME_WQHD_HS_VRR - ktime_ms_delta(ktime_get(), vrr->time_vrr_start);
		LCD_INFO(vdd, "VRR: wait_ms_wqhd_hs = %lld ms\n", wait_ms_wqhd_hs);
		while (wait_ms_wqhd_hs > 0) {
			LCD_INFO(vdd, "VRR: WQHD_HS wait for %lld ms\n", wait_ms_wqhd_hs);
			usleep_range(wait_ms_wqhd_hs * 1000, wait_ms_wqhd_hs * 1000);
			wait_ms_wqhd_hs = DELAY_TIME_WQHD_HS_VRR -
						ktime_ms_delta(ktime_get(), vrr->time_vrr_start);
		}
	}

	vrr->skip_vrr_in_brightness = false;

	/* compenstate gamma MTP value for 48/96hz modes in gamma mode2 panel */
	if (vdd->panel_func.br_func[BR_FUNC_GAMMA_COMP]) {
		if ((cur_rr != 48 && cur_rr != 96) &&
				(adjusted_rr == 48 || adjusted_rr == 96)) {
			/* 60/120hz mode -> 48/96hz mode: apply compensated gamma */
			vrr->gm2_gamma = VRR_GM2_GAMMA_COMPENSATE;
			LCD_INFO(vdd, "compensate gamma\n");
		} else if ((cur_rr == 48 || cur_rr == 96) &&
				(adjusted_rr != 48 && adjusted_rr != 96)) {
			/* 48/96hz mode -> 60/120hz mode: restore original gamma MTP */
			vrr->gm2_gamma = VRR_GM2_GAMMA_RESTORE_ORG;
			LCD_INFO(vdd, "restore org gamma\n");
		}
	}

	/* In OFF/LPM mode, skip VRR change, and just save VRR values.
	 * After normal display on, it will be applied in brightness setting.
	 */
	if (ss_is_panel_off(vdd) || ss_is_panel_lpm(vdd)) {
		LCD_ERR(vdd, "error: panel_state(%d), skip VRR (save VRR state)\n",
				vdd->panel_state);
		ret = -EPERM;
		SS_XLOG(0xbad, vdd->panel_state);

		vrr->cur_refresh_rate = adjusted_rr;
		vrr->cur_sot_hs_mode = adjusted_hs;
		vrr->cur_phs_mode = adjusted_phs;

		goto brr_done;
	}

	mutex_lock(&vrr->brr_lock);
	vrr->brr_mode = ss_get_brr_mode(vdd);
	mutex_unlock(&vrr->brr_lock);

	if (!ss_is_brr_on(vrr->brr_mode)) {
		LCD_INFO(vdd, "VRR: BRR off (%s)\n", ss_get_brr_mode_name(vrr->brr_mode));

		/* HS <-> Normal mode: insert one black frame to hide screen noise
		 * ss_gamma() set gamma MTP (C8h) to zero.
		 * None zero gamma (CAh) setting causes greenish screen issue during MTP(C8h) is zero.
		 * So, gamma value should be set to zero while black_frame_mode is BLACK_FRAME_INSERT mode.
		 */
		/* TODO:
		 * - pending frame update using exclusive lock..? -> No
		 * - this code is called in display_lock. so frame update and brightness setting
		 *   would be blocked..
		*/
		if (vrr->is_support_black_frame && is_hs_change) {
			LCD_INFO(vdd, "VRR: set BLACK_FRAME_INSERT\n");
			vrr->black_frame_mode = BLACK_FRAME_INSERT;
		}

		/* In VRR, TE period is configured by cmd included in DSI_CMD_SET_TIMING_SWITCH.
		 * To avoid brightness flicker, TE period and proper brightness setting
		 * should be applied at once.
		 * TE period setting cmd will be applied by gamma update cmd (F7h).
		 * So, update brightness here, to send gamma update (F7h) and apply
		 * new brightness and new TE period which is configured in above DSI_CMD_SET_TIMING_SWITCH.
		 */

		vrr->prev_refresh_rate = vrr->cur_refresh_rate;
		vrr->prev_sot_hs_mode = vrr->cur_sot_hs_mode;
		vrr->prev_phs_mode = vrr->cur_phs_mode;

		vrr->cur_refresh_rate = adjusted_rr;
		vrr->cur_sot_hs_mode = adjusted_hs;
		vrr->cur_phs_mode = adjusted_phs;

		if (vdd->br_info.common_br.finger_mask_hbm_on)
			ss_brightness_dcs(vdd, 0, BACKLIGHT_FINGERMASK_ON_SUSTAIN);
		else
			ss_brightness_dcs(vdd, USE_CURRENT_BL_LEVEL, BACKLIGHT_NORMAL);

		/* HS <-> Normal mode: remove one black frame and restore to original brightness */
		if (vrr->black_frame_mode == BLACK_FRAME_INSERT) {
			/* Above brightness setting will be applied to DDI
			 * in next vsync (previous refresh rate).
			 * So, wait for interval based on previous fps.
			 */
			int interval_us = (1000000 / cur_rr) + 1000; /* 1ms dummy */
			usleep_range(interval_us, interval_us);

			/* ss_gamma() will restore gamma (CAh) and MTP (C8h) */
			LCD_INFO(vdd, "VRR: waited %d us, set BLACK_FRAME_REMOVE\n", interval_us);
			vrr->black_frame_mode = BLACK_FRAME_REMOVE;
			ss_brightness_dcs(vdd, USE_CURRENT_BL_LEVEL, BACKLIGHT_NORMAL);
		}

		goto brr_done;
	}

	LCD_INFO(vdd, "VRR: BRR on (%s)\n", ss_get_brr_mode_name(vrr->brr_mode));

	brr = &vrr->brr_tbl[vrr->brr_mode];

	step = 0;
	direction = 1;
	while (step >= 0 && step < brr->tot_steps) {
		int fps_brr = brr->fps[step];

		LCD_INFO(vdd, "VRR: [%d] fps: %d, delay %d frames\n", step, fps_brr, brr->delay_frame_cnt);

		/* in ss_gamma, and ss_aor, calcaulate gamma or aor interpolation...
		 * In BRR mode, SOT_HS mode is not changed.
		 */
		vrr->cur_refresh_rate = fps_brr;
		if (vdd->br_info.common_br.finger_mask_hbm_on)
			ss_brightness_dcs(vdd, 0, BACKLIGHT_FINGERMASK_ON_SUSTAIN);
		else
			ss_brightness_dcs(vdd, USE_CURRENT_BL_LEVEL, BACKLIGHT_NORMAL);

		if (brr->delay_frame_cnt <= 0) {
			LCD_ERR(vdd, "VRR: delay_frame_cnt: %d, set default value 4\n", brr->delay_frame_cnt);
			brr->delay_frame_cnt = 4;
		}

		interval_ms = (1000 / fps_brr) * brr->delay_frame_cnt + 1; /* add 1ms dummy delay */
		while (interval_ms--) {
			/* check vrr change, and stop bridge RR, return.
			 * New VRR setting will change RR.
			 * - BRR_STOP_MODE :  case of new brightness level change. Jump to final adjusted RR.
			 *   Here, just stop BRR and update current RR to final adjusted RR.
			 *   New brightness setting will chagne to final adjusted RR.
			 */
			mutex_lock(&vrr->brr_lock);
			if (vrr->brr_mode == BRR_STOP_MODE) {
				vrr->brr_mode = BRR_OFF_MODE; // BRR is done, set to off mode
				mutex_unlock(&vrr->brr_lock);

				vrr->cur_refresh_rate = adjusted_rr;
				vrr->cur_sot_hs_mode = adjusted_hs;
				vrr->cur_phs_mode = adjusted_phs;

				LCD_INFO(vdd, "VRR: stop BRR (bl: %d)\n", vrr->brr_bl_level);

				/* update current VRR mode before restore
				 * SDE core clock to prevent screen noise
				 */
				if (vdd->br_info.common_br.finger_mask_hbm_on)
					ss_brightness_dcs(vdd, 0, BACKLIGHT_FINGERMASK_ON_SUSTAIN);
				else
					ss_brightness_dcs(vdd, vrr->brr_bl_level, BACKLIGHT_NORMAL);

				goto brr_done;
			}
			mutex_unlock(&vrr->brr_lock);

			usleep_range(1000, 1000);
		}

		/* BRR_REWIND_MODE */
		mutex_lock(&vrr->brr_lock);
		if (vrr->brr_rewind_on) {
			vrr->brr_rewind_on = false;
			direction *= -1;

			LCD_INFO(vdd, "VRR: rewind BRR, direction: %d, adjusted_rr: %d -> %d\n",
					direction, adjusted_rr, vrr->adjusted_refresh_rate);

			/* In rewind mode, its final adjusted RR is changed by new VRR */
			adjusted_rr = vrr->adjusted_refresh_rate;
		}
		mutex_unlock(&vrr->brr_lock);

		step += direction;
	}

	/* brr_mode is used in ss_gamma or ss_aid. So turn off brr_mode */
	mutex_lock(&vrr->brr_lock);
	vrr->brr_mode = BRR_OFF_MODE; // BRR is done, set to off mode
	mutex_unlock(&vrr->brr_lock);

	/* In VRR, TE period is configured by cmd included in brightness setting */
	vrr->cur_refresh_rate = adjusted_rr;
	vrr->cur_sot_hs_mode = adjusted_hs;
	vrr->cur_phs_mode = adjusted_phs;

	if (vdd->br_info.common_br.finger_mask_hbm_on)
		ss_brightness_dcs(vdd, 0, BACKLIGHT_FINGERMASK_ON_SUSTAIN);
	else
		ss_brightness_dcs(vdd, USE_CURRENT_BL_LEVEL, BACKLIGHT_NORMAL);

brr_done:

	/* TE modulation */
	if (vrr->support_te_mod && vdd->panel_func.samsung_vrr_set_te_mod) {
		vdd->panel_func.samsung_vrr_set_te_mod(vdd,
						vrr->cur_refresh_rate,
						vrr->cur_sot_hs_mode);
		SS_XLOG(vrr->te_mod_on, vrr->te_mod_divider, vrr->te_mod_cnt);
	}

	/* Restore to normal sde core clock boosted up to max mdp clock during VRR.
	 * In below case, current VRR mode and adjusting target VRR mode are different,
	 * and should keep maximum SDE core clock to prevent screen noise.
	 *
	 * 1) DISP thread#1: get 60HS -> 120HS VRR change request.
	 *                   Request SDE core clock to 200Mhz.
	 * 2) VRR thread#2:  start 60HS -> 120HS BRR which takes hundreds miliseconds.
	 *                   Set maximum SDE core clock (460Mhz)
	 * 3) DISP thread#1: get 120HS -> 60HS VRR change request.
	 *                   Request SDE core clock to 150Mhz.
	 * 4) VRR thread#2:  finish 60HS -> 120HS BRR.
	 *                   Restore SDE core clock to 150Mhz for 60HS while panel works at 120hz..
	 * 		     It causes screen noise...
	 *                   In this case, current VRR mode and adjusting target VRR mode are different!
	 * 5) VRR thread#2:  start 120HS -> 60HS BRR. Set maximum SDE core clock, and fix screen noise.
	 *                   After hundreds miliseconds, it finishes BRR,
	 *                   and restore SDE core clock to 150Mhz while panel works at 60hz.
	 *                   No more screen noise.
	 */
	if (vrr->cur_refresh_rate == vrr->adjusted_refresh_rate &&
			vrr->cur_sot_hs_mode == vrr->adjusted_sot_hs_mode) {
		/* SDE core clock will be applied to system by calling ss_set_normal_sde_core_clk().
		 * But, panel's new refresh rate will be applied in next TE after sending VRR commands.
		 * So, delay one frame before restore SDE core clock.
		 */
		int min_rr = min(cur_rr, adjusted_rr);
		int interval_us = (1000000 / min_rr) + 1000; /* 1ms dummy */

		LCD_INFO(vdd, "delay 1frame(min_rr: %d, %dus)\n", min_rr, interval_us);
		usleep_range(interval_us, interval_us);

		if (!vrr->running_vrr_mdp) {
			if (ss_set_normal_sde_core_clk(ddev)) {
				LCD_ERR(vdd, "fail to set normal sde core clock..\n");
				SS_XLOG(0xbad2);
				ret = -EINVAL;
			}
		} else {
			LCD_ERR(vdd, "Do not set normal clk, vrr is ongoing!! (%d)\n", vrr->running_vrr_mdp);
		}
	} else {
		LCD_INFO(vdd, "consecutive VRR req, keep max sde clk (cur: %d%s, adj: %d%s)\n",
				vrr->cur_refresh_rate,
				vrr->cur_sot_hs_mode ? "HS" : "NS",
				vrr->adjusted_refresh_rate,
				vrr->adjusted_sot_hs_mode ? "HS" : "NS");
	}

	/* TODO: need atomic read?? for delayed_perf_normal??? -> No.
	 * There could be race condition with sysfs_sde_core_perf_mode_write(),
	 * But even though, it will set delayed_perf_normal to true,
	 * and reset the perf tune params finally.
	 */
	if (vrr->delayed_perf_normal) {
		struct sde_kms *sde_kms = GET_SDE_KMS(vdd);
		struct sde_core_perf *perf = &sde_kms->perf;;

		vrr->delayed_perf_normal = false;

		if (!perf) {
			LCD_ERR(vdd, "error: fail to get perf.. skip to normal perf mode..\n");
			goto vrr_end;
		}

		/* reset the perf tune params to 0 */
		perf->perf_tune.min_core_clk = 0;
		perf->perf_tune.min_bus_vote = 0;

		LCD_INFO(vdd, "VRR: delayed normal performance mode in BRR end (%d -> NORMAL)\n",
				perf->perf_tune.mode);
		perf->perf_tune.mode = SDE_PERF_MODE_NORMAL;
	}

vrr_end:

	vrr->running_vrr = false;

	LCD_INFO(vdd, "--- VRR\n");
	SDE_ATRACE_END(trace_name);

	mutex_unlock(&vrr->vrr_lock);

	ss_notify_queue_work(vdd, PANEL_EVENT_LFD_CHANGED);

	return ret;
}

void ss_panel_vrr_switch_work(struct work_struct *work)
{
	struct vrr_info *vrr = container_of(work, struct vrr_info, vrr_work);

	ss_panel_vrr_switch(vrr);
}

int ss_panel_dms_switch(struct samsung_display_driver_data *vdd)
{
	struct vrr_info *vrr = &vdd->vrr;

	int cur_hact = vrr->cur_h_active;
	int cur_vact = vrr->cur_v_active;
	bool cur_hs = vrr->cur_sot_hs_mode;
	bool cur_phs = vrr->cur_phs_mode;
	int adjusted_hact = vrr->adjusted_h_active;
	int adjusted_vact = vrr->adjusted_v_active;
	bool adjusted_hs = vrr->adjusted_sot_hs_mode;
	bool adjusted_phs = vrr->adjusted_phs_mode;

	bool is_hs_change = !!(cur_hs != adjusted_hs) || !!(cur_phs != adjusted_phs);

	LCD_INFO(vdd, "%dx%d@%d%s -> %dx%d@%d%s , is_hs_change %d\n",
			cur_hact, cur_vact, vrr->cur_refresh_rate,
			cur_hs ? (cur_phs ? "PHS" : "HS") : "NM",
			adjusted_hact, adjusted_vact, vrr->adjusted_refresh_rate,
			adjusted_hs ? (adjusted_phs ? "PHS" : "HS") : "NM",
			is_hs_change);

	/* Do we have to change param only when the HS<->NORMAL be changed? */
	ss_set_vrr_switch(vdd, true);

	vrr->time_vrr_start = ktime_get();
	if (vrr->max_h_active_support_120hs &&
			vrr->is_multi_resolution_changing &&
			vrr->is_vrr_changing &&
			is_hs_change) {
		/* WQHD@Normal -> FHD/HD@HS: multi resolution -> one image frame -> VRR */
		/* TODO: replace this with check code if it supports HS mode or not.... */
		if ((vrr->cur_h_active > vrr->max_h_active_support_120hs) &&
				(adjusted_hs)) {
			LCD_INFO(vdd, "WQHD(%d>%d)@NM -> FHD/HD@HS: multi resolution -> one frame -> VRR\n",
					vrr->cur_h_active, vrr->max_h_active_support_120hs);
			vrr->hs_nm_seq = HS_NM_MULTI_RES_FIRST;
		}

		/* FHD/HD@HS -> WQHD@Normal: VRR -> multi resolution */
		if ((vrr->adjusted_h_active > vrr->max_h_active_support_120hs) &&
				(cur_hs)) {
			LCD_INFO(vdd, "FHD/HD@HS -> WQHD(%d>%d)@NM: VRR -> multi resolution\n",
					vrr->cur_h_active, vrr->max_h_active_support_120hs);
			vrr->hs_nm_seq = HS_NM_VRR_FIRST;
		}
	}

	/* case of HS_NM_VRR_FIRST: change multi resolution after VRR setting */
	if (vrr->is_multi_resolution_changing) {
		if (vrr->hs_nm_seq != HS_NM_VRR_FIRST) {
			LCD_INFO(vdd, "multi resolution: tx DMS switch cmd\n");
			if (vdd->panel_func.samsung_timing_switch_pre)
				vdd->panel_func.samsung_timing_switch_pre(vdd);
			ss_send_cmd(vdd, DSI_CMD_SET_TIMING_SWITCH);
			if (vdd->panel_func.samsung_timing_switch_post)
				vdd->panel_func.samsung_timing_switch_post(vdd);
		}
		vrr->is_multi_resolution_changing = false;
	}

	if (vrr->is_vrr_changing) {
		mutex_lock(&vrr->brr_lock);
		if (ss_is_brr_on(vrr->brr_mode) &&
				vrr->adjusted_refresh_rate == vrr->brr_tbl[vrr->brr_mode].fps_start &&
				vrr->adjusted_sot_hs_mode == vrr->brr_tbl[vrr->brr_mode].sot_hs_base) {
			vrr->brr_rewind_on = true;

			LCD_INFO(vdd, "VRR: new vrr (%dhz%s), set brr_rewind_on\n",
					vrr->adjusted_refresh_rate,
					vrr->adjusted_sot_hs_mode ? "HS" : "NM" );
		}
		mutex_unlock(&vrr->brr_lock);

		if (vrr->hs_nm_seq == HS_NM_VRR_FIRST) {
			LCD_INFO(vdd, "VRR: start VRR switch to pend frame update until change resolution\n");
			ss_panel_vrr_switch(vrr);
			LCD_INFO(vdd, "VRR: multi resolution: tx DMS switch cmd\n");
			if (vdd->panel_func.samsung_timing_switch_pre)
				vdd->panel_func.samsung_timing_switch_pre(vdd);
			ss_send_cmd(vdd, DSI_CMD_SET_TIMING_SWITCH);
			if (vdd->panel_func.samsung_timing_switch_post)
				vdd->panel_func.samsung_timing_switch_post(vdd);
			vrr->hs_nm_seq = HS_NM_OFF;
		} else {
			enum SS_BRR_MODE brr_mode = ss_get_brr_mode(vdd);

			/* skip VRR setting in brightness command (ss_vrr() funciton)
			 * 1) start waiting HS_NM change in vrr_work thread
			 * 2) triger brightness setting in other thread, and change fps
			 *    before finishing HS_NM waiting...
			 *    In result, it causes side effect, like screen noise or stuck..
			 * 3) finish waiting HS_NM change...
			 */
			if (vrr->hs_nm_seq == HS_NM_MULTI_RES_FIRST) {
				LCD_INFO(vdd, "VRR: HS_NM transition, set skip_vrr_in_brightness true\n");
				vrr->skip_vrr_in_brightness = true;
			}

			LCD_INFO(vdd, "VRR: %s, always enQ VRR work\n", ss_get_brr_mode_name(brr_mode));
			if (vrr->vrr_workqueue)
				queue_work(vrr->vrr_workqueue, &vrr->vrr_work);
		}
		vrr->is_vrr_changing = false;
	}

	vrr->cur_h_active = vrr->adjusted_h_active;
	vrr->cur_v_active = vrr->adjusted_v_active;

	return 0;
}

void ss_set_panel_state(struct samsung_display_driver_data *vdd, enum ss_panel_pwr_state panel_state)
{
	vdd->panel_state = panel_state;

	LCD_INFO(vdd, "set panel state %d\n", vdd->panel_state);

	ss_notify_queue_work(vdd, PANEL_EVENT_STATE_CHANGED);
}

void ss_set_test_mode_state(struct samsung_display_driver_data *vdd, enum ss_test_mode_state state)
{
	vdd->test_mode_state = state;

	LCD_INFO(vdd, "set test_mde state %d\n", vdd->test_mode_state);

	ss_notify_queue_work(vdd, PANEL_EVENT_TEST_MODE_CHANGED);
}

/* bring start time of gamma flash forward: first frame update -> display driver probe.
 * As of now, gamma flash starts when bootanimation kickoff frame update.
 *
 * In that case, it causes below problem.
 * 1) bootanimation turns on main and sub display.
 * 2) bootanimation update one black frame to both displays.
 * 3) bootanimation turns off sub dipslay, in case of folder open.
 *    But, sub display gamma flash takes several seconds (about 3 sec in user binary),
 *    so, it bootanimation waits for it, and both displays shows black screen for a while.
 * 4) After sub display gamma flash is done and finish to turn off sub display,
 *    bootanimation starts to draw animation on main display, and it is too late then expected..
 *
 * To avoid above issue, starts gamma flash earlyer, at display driver probe timing.
 * For this, to do belows at driver probe timing.
 * 1) start gamma flash only in case of splash mode on. Splash on mode means that bootlaoder already turned on mipi phy.
 * 2) initialize mipi host, including enable mipi error interrupts, to avoid blocking mipi transmission.
 * 3) disable autorefresh, which was disabled in first frame update, to prevent dsi fifo underrun error.
 */
int ss_early_display_init(struct samsung_display_driver_data *vdd)
{
	if (!vdd) {
		LCD_ERR(vdd, "error: vdd NULL\n");
		return -ENODEV;
	}

	if (vdd->br_info.support_early_gamma_flash &&
			vdd->br_info.flash_gamma_support &&
			!vdd->br_info.flash_gamma_init_done &&
			!work_busy(&vdd->br_info.flash_br_work.work)) {
		LCD_DEBUG(vdd, "early gamma flash\n");
	} else if (vdd->support_early_id_read) {
		LCD_DEBUG(vdd, "early module ID read\n");
	} else {
		LCD_INFO(vdd, "no action\n");
		return 0;
	}

	LCD_INFO(vdd, "++ ndx=%d: cur panel_state: %d\n", vdd->ndx, vdd->panel_state);

	/* set lcd id for getting proper mapping table */
	if (vdd->ndx == PRIMARY_DISPLAY_NDX)
		vdd->manufacture_id_dsi = get_lcd_attached("GET");
	else if (vdd->ndx == SECONDARY_DISPLAY_NDX)
		vdd->manufacture_id_dsi = get_lcd_attached_secondary("GET");

	LCD_INFO(vdd, "manufacture_id_dsi = 0x%x\n", vdd->manufacture_id_dsi);

	/* Panel revision selection */
	if (IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_revision))
		LCD_ERR(vdd, "no panel_revision_selection_error function\n");
	else
		vdd->panel_func.samsung_panel_revision(vdd);

	/* in gamma flash work thread, it will initialize mipi host */
	/* In case of autorefresh_fail, do gamma flash job in ss_event_frame_update()
	 * which is called after RESET MDP to recover error status.
	 * TODO: get proper phys_enc and check if phys_enc->enable_state = SDE_ENC_ERR_NEEDS_HW_RESET...
	 */
	if (vdd->is_autorefresh_fail) {
		LCD_ERR(vdd, "autorefresh failure error\n");
		return -EINVAL;
	}

	/* early gamma flash */
	if (vdd->br_info.support_early_gamma_flash) {
		LCD_INFO(vdd, "early enQ gamma flash work\n");
		queue_delayed_work(vdd->br_info.flash_br_workqueue, &vdd->br_info.flash_br_work, msecs_to_jiffies(0));
	}

	/* early module id read */
	if (vdd->support_early_id_read) {
		bool skip_restore_panel_state_off = true;

		LCD_INFO(vdd, "early module ID read\n");

		/* set panel_state pwr on ready, to allow mipi transmission */
		if (vdd->panel_state == PANEL_PWR_OFF) {
			skip_restore_panel_state_off = false;
			vdd->panel_state = PANEL_PWR_ON_READY;
		}

		/* Module info */
		if (!vdd->module_info_loaded_dsi) {
			if (IS_ERR_OR_NULL(vdd->panel_func.samsung_module_info_read))
				LCD_ERR(vdd, "no samsung_module_info_read function\n");
			else
				vdd->module_info_loaded_dsi = vdd->panel_func.samsung_module_info_read(vdd);
		}

		/* restore panel_state to poweroff
		 * to prevent other mipi transmission before gfx HAL enable display.
		 *
		 * disp_on_pre == true means panel_state is already overwritten by gfx HAL,
		 * so no more need to restore panel_state
		 */
		if (vdd->display_status_dsi.disp_on_pre)
			skip_restore_panel_state_off = true;
		if (!skip_restore_panel_state_off) {
			LCD_INFO(vdd, "restore panel state to off\n");
			vdd->panel_state = PANEL_PWR_OFF;
		}
	}

	LCD_INFO(vdd, "--\n");

	return 0;
}

static int ss_gm2_ddi_flash_init(struct samsung_display_driver_data *vdd)
{
	struct flash_gm2 *gm2_table = &vdd->br_info.gm2_table;
	int ret;

	if (vdd->br_info.gm2_table.ddi_flash_raw_len == 0 ||
			vdd->br_info.gm2_table.len_one_vbias_mode == 0) {
		LCD_DEBUG(vdd, "do not support gamma mode2 flash (%d, %d)\n",
				vdd->br_info.gm2_table.ddi_flash_raw_len,
				vdd->br_info.gm2_table.len_one_vbias_mode);
		return 0;
	}

	gm2_table->ddi_flash_raw_buf = NULL;
	gm2_table->vbias_data = NULL;
	gm2_table->mtp_one_vbias_mode = NULL;

	/* init gamma mode2 flash data */
	gm2_table->ddi_flash_raw_buf = kzalloc(gm2_table->ddi_flash_raw_len, GFP_KERNEL | GFP_DMA);
	if (!gm2_table->ddi_flash_raw_buf) {
		LCD_ERR(vdd, "fail to alloc ddi_flash_raw_buf\n");
		return -ENOMEM;
	}

	gm2_table->vbias_data = kzalloc((gm2_table->len_one_vbias_mode * VBIAS_MODE_MAX),
						GFP_KERNEL | GFP_DMA);
	if (!gm2_table->vbias_data) {
		LCD_ERR(vdd, "fail to alloc vbias_data\n");
		ret = -ENOMEM;
		goto err;
	}
	gm2_table->mtp_one_vbias_mode = kzalloc(gm2_table->len_mtp_one_vbias_mode,
							GFP_KERNEL | GFP_DMA);
	if (!gm2_table->mtp_one_vbias_mode) {
		LCD_ERR(vdd, "fail to alloc mtp_one_vbias_mode\n");
		ret = -ENOMEM;
		goto err;
	}

	LCD_INFO(vdd, "init flash: OK\n");

	return 0;

err:
	LCD_ERR(vdd, "fail to init vbias flash.. free memory..(ret: %d)\n", ret);
	kfree(gm2_table->mtp_one_vbias_mode);
	kfree(gm2_table->vbias_data );
	kfree(gm2_table->ddi_flash_raw_buf);
	return ret;
}


void ss_panel_init(struct dsi_panel *panel)
{
	struct samsung_display_driver_data *vdd;
	enum ss_display_ndx ndx;
	char panel_name[MAX_CMDLINE_PARAM_LEN];
	char panel_secondary_name[MAX_CMDLINE_PARAM_LEN];
	int i = 0;

	/* compare panel name in command line and dsi_panel.
	 * primary panel: ndx = 0
	 * secondary panel: ndx = 1
	 */

	ss_get_primary_panel_name_cmdline(panel_name);
	ss_get_secondary_panel_name_cmdline(panel_secondary_name);

	if (!strcmp(panel->name, panel_name)) {
		ndx = PRIMARY_DISPLAY_NDX;
	} else if (!strcmp(panel->name, panel_secondary_name)) {
		ndx = SECONDARY_DISPLAY_NDX;
	} else {
		/* If it fails to find panel name, it cannot turn on display,
		 * and this is critical error case...
		 */
		WARN(1, "fail to find panel name, panel=%s, cmdline=%s\n",
				panel->name, panel_name);
		return;
	}

	/* TODO: after using component_bind in samsung_panel_init,
	 * it doesn't have to use vdd_data..
	 * Remove vdd_data, and allocate vdd memory here.
	 * vdds will be managed by vdds_list...
	 */
	vdd = ss_get_vdd(ndx);
	vdd->ndx = ndx;

	LCD_INFO(vdd, "++ \n");

	panel->panel_private = vdd;
	vdd->msm_private = panel;
	list_add(&vdd->vdd_list, &vdds_list);

	if (ss_panel_debug_init(vdd))
		LCD_ERR(vdd, "Fail to create debugfs\n");

	if (ss_smmu_debug_init(vdd))
		LCD_ERR(vdd, "Fail to create smmu debug\n");

	mutex_init(&vdd->vdd_lock);
	mutex_init(&vdd->cmd_lock);
	mutex_init(&vdd->bl_lock);
	mutex_init(&vdd->ss_spi_lock);

	/* To guarantee ALPM ON or OFF mode change operation*/
	mutex_init(&vdd->panel_lpm.lpm_lock);

	/* To prevent normal brightness patcket after tx LPM on */
	mutex_init(&vdd->panel_lpm.lpm_bl_lock);

	/* To guarantee dynamic MIPI clock change*/
	mutex_init(&vdd->dyn_mipi_clk.dyn_mipi_lock);
	vdd->dyn_mipi_clk.requested_clk_rate = 0;

	vdd->panel_dead = false;
	vdd->is_autorefresh_fail = false;

	/* init function poiner for brighntess packet */
	for (i = 0; i < BR_FUNC_MAX; i++)
		vdd->panel_func.br_func[i] = NULL;

	if (IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_init))
		LCD_ERR(vdd, "no samsung_panel_init func\n");
	else
		vdd->panel_func.samsung_panel_init(vdd);

	/* Panel revision selection */
	if (IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_revision)) {
		LCD_ERR(vdd, "no panel_revision_selection_error function\n");
	} else {
		/* set manufacture_id_dsi from cmdline to set proper panel revision */
		if (vdd->ndx == PRIMARY_DISPLAY_NDX)
			vdd->manufacture_id_dsi = get_lcd_attached("GET");
		else if (vdd->ndx == SECONDARY_DISPLAY_NDX)
			vdd->manufacture_id_dsi = get_lcd_attached_secondary("GET");

		LCD_INFO(vdd, "manufacture_id_dsi = 0x%x\n", vdd->manufacture_id_dsi);

		vdd->panel_func.samsung_panel_revision(vdd);
	}

	/* set manufacture_id_dsi to PBA_ID to read ID later  */
	vdd->manufacture_id_dsi = PBA_ID;

	if (vdd->mdnie.support_mdnie ||
			vdd->support_cabc ||
			ss_panel_attached(ndx))
		vdd->mdnie.mdnie_tune_state_dsi =
			init_dsi_tcon_mdnie_class(vdd);

	spin_lock_init(&vdd->esd_recovery.irq_lock);

	ss_panel_attach_set(vdd, true);

	mutex_init(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.enable = 0;
	init_waitqueue_head(&vdd->exclusive_tx.ex_tx_waitq);
	atomic_set(&vdd->ss_vsync_cnt, 0);
	init_waitqueue_head(&vdd->ss_vync_wq);

	atomic_set(&vdd->block_commit_cnt, 0);
	init_waitqueue_head(&vdd->block_commit_wq);

	ss_create_sysfs(vdd);

#if IS_ENABLED(CONFIG_SEC_FACTORY)
	vdd->is_factory_mode = true;
#endif

	/* parse display dtsi node */
	ss_panel_parse_dt(vdd);

	ss_spi_init(vdd);

	ss_dsi_poc_init(vdd);

	/* self display */
	if (vdd->self_disp.is_support) {
		if (vdd->self_disp.init)
			vdd->self_disp.init(vdd);

		if (vdd->self_disp.data_init)
			vdd->self_disp.data_init(vdd);
		else
			LCD_ERR(vdd, "no self display data init function\n");
	}

	/* mafpc */
	if (vdd->mafpc.is_support) {
		if (vdd->mafpc.init)
			vdd->mafpc.init(vdd);

		if (vdd->mafpc.data_init)
			vdd->mafpc.data_init(vdd);
		else
			LCD_ERR(vdd, "no mafpc data init function\n");
	}

	if (vdd->copr.panel_init)
		vdd->copr.panel_init(vdd);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	/* register notifier and init workQ.
	 * TODO: move hall_ic_notifier_display and dyn_mipi_clk notifier here.
	 */
	if (vdd->vrr.lfd.support_lfd) {
		vdd->vrr.lfd.nb_lfd_touch.priority = 3;
		vdd->vrr.lfd.nb_lfd_touch.notifier_call = ss_lfd_touch_notify_cb;
		sec_input_register_notify(&vdd->vrr.lfd.nb_lfd_touch, ss_lfd_touch_notify_cb, 3);

		vdd->vrr.lfd.lfd_touch_wq = create_singlethread_workqueue("lfd_touch_wq");
		if (!vdd->vrr.lfd.lfd_touch_wq) {
			LCD_ERR(vdd, "failed to create touch_lfd workqueue..\n");
			return;
		}
		INIT_WORK(&vdd->vrr.lfd.lfd_touch_work, ss_lfd_touch_work);
	}

	if (vdd->esd_touch_notify) {
		vdd->nb_esd_touch.priority = 3;
		vdd->nb_esd_touch.notifier_call = ss_esd_touch_notifier_cb;
		sec_input_register_notify(&vdd->nb_esd_touch, ss_esd_touch_notifier_cb, 3);
	}
#endif

	/* work thread for panel notifier */
	vdd->notify_wq = create_singlethread_workqueue("panel_notify_wq");
	if (vdd->notify_wq == NULL)
		LCD_ERR(vdd, "failed to create read notify workqueue..\n");
	else {
		INIT_WORK(&vdd->bl_event_work, (work_func_t)ss_bl_event_work);
		INIT_WORK(&vdd->vrr_event_work, (work_func_t)ss_vrr_event_work);
		INIT_WORK(&vdd->lfd_event_work, (work_func_t)ss_lfd_event_work);
		INIT_WORK(&vdd->panel_state_event_work, (work_func_t)ss_panel_state_event_work);
		INIT_WORK(&vdd->test_mode_event_work, (work_func_t)ss_test_mode_event_work);
		INIT_WORK(&vdd->screen_mode_event_work, (work_func_t)ss_screen_mode_event_work);
		INIT_WORK(&vdd->esd_event_work, (work_func_t)ss_esd_event_work);
	}

	mutex_init(&vdd->notify_lock);

	/* init gamma mode2 flash data */
	vdd->br_info.gm2_table.flash_gm2_init_done = false;
	ss_gm2_ddi_flash_init(vdd);

	LCD_INFO(vdd, "window_color : [%s]\n", vdd->window_color);

	LCD_INFO(vdd, "-- \n");
}

void ss_set_vrr_switch(struct samsung_display_driver_data *vdd, bool enable)
{
	/* notify clients that vrr has changed */
	vdd->vrr.param_update = enable;
	ss_notify_queue_work(vdd, PANEL_EVENT_VRR_CHANGED);
	return;
}

int samsung_panel_initialize(char *panel_string, unsigned int ndx)
{
	struct samsung_display_driver_data *vdd;

	vdd = ss_get_vdd(ndx);
	if(!vdd) {
		LCD_ERR(vdd, " vdd is null\n");
		return -1;
	}

	LCD_INFO(vdd, "[module] lcd_id = main(0x%d) sub(0x%d)\n", lcd_id, lcd_id1);

	LCD_INFO(vdd, "index(%d), %s ++\n", ndx, panel_string);

	if (!strncmp(panel_string, "ss_dsi_panel_PBA_BOOTING_FHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = PBA_BOOTING_FHD_init;
#if IS_ENABLED(CONFIG_PANEL_PBA_FHD_DSI1)
	else if (!strncmp(panel_string, "ss_dsi_panel_PBA_BOOTING_FHD_DSI1", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = PBA_BOOTING_FHD_DSI1_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E8FC1_AMS660XR01_HD)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E8FC1_AMS660XR01_HD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E8FC1_AMS660XR01_HD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E3FAB_AMB624XT01_FHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E3FAB_AMB624XT01_FHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E3FAB_AMB624XT01_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E3FAB_AMB667XU01_FHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E3FAB_AMB667XU01_FHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E3FAB_AMB667XU01_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E3FC3_AMS646YD01_FHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E3FC3_AMS646YD01_FHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E3FC3_AMS646YD01_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E3FC3_AMS646YD01_LO_FHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E3FC3_AMS646YD01_LO_FHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E3FC3_AMS646YD01_LO_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E3HAB_AMB623TS01_WQHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E3HAB_AMB623TS01_WQHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E3HAB_AMB623TS01_WQHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E3HAB_AMB677TY01_WQHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E3HAB_AMB677TY01_WQHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E3HAB_AMB677TY01_WQHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E3HAD_AMB681XV01_WQHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E3HAD_AMB681XV01_WQHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E3HAD_AMB681XV01_WQHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_HX83102_TV104WUM_WUXGA)
	else if (!strncmp(panel_string, "ss_dsi_panel_HX83102_TV104WUM_WUXGA", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = HX83102_TV104WUM_WUXGA_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_HX83121_PPC357DB11_WQXGA)
	else if (!strncmp(panel_string, "ss_dsi_panel_HX83121_PPC357DB11_WQXGA", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = HX83121_PPC357DB11_WQXGA_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_FT8203_TS124QDM_WQXGA)
	else if (!strncmp(panel_string, "ss_dsi_panel_FT8203_TS124QDM_WQXGA", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = FT8203_TS124QDM_WQXGA_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E3XA1_AMF759VG01_QXGA)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E3XA1_AMF759VG01_QXGA", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E3XA1_AMF759VG01_QXGA_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E3FA7_AMB623VH01_HD)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E3FA7_AMB623VH01_HD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E3FA7_AMB623VH01_HD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E3HA9_AMF670UH01_FHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E3HA9_AMF670UH01_FHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E3HA9_AMF670UH01_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E36W3_AMB114EU09_116X300)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E36W3_AMB114EU09_116X300", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E36W3_AMB114EU09_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E3HAD_AMF670ZA01_FHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E3HAD_AMF670ZA01_FHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E3HAD_AMF670ZA01_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_B4_S6E3FAC_AMF670BS01_FHD)
	else if (!strncmp(panel_string, "B4_S6E3FAC_AMF670BS01", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = B4_S6E3FAC_AMF670BS01_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E36W3_AMB190ZB01_260X488)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E36W3_AMB190ZB01_260X488", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E36W3_AMB190ZB01_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E3XA1_AMF755ZE01_QXGA)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E3XA1_AMF755ZE01_QXGA", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E3XA1_AMF755ZE01_QXGA_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E3XA2_AMF755ZE01_QXGA)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E3XA2_AMF755ZE01_QXGA", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E3XA2_AMF755ZE01_QXGA_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E3FAB_AMB623ZF01_HD)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E3FAB_AMB623ZF01_HD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E3FAB_AMB623ZF01_HD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_EA8082_AMB641ZR01_FHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_EA8082_AMB641ZR01_FHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = EA8082_AMB641ZR01_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E3FC3_AMB641ZR02_FHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E3FC3_AMB641ZR02_FHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E3FC3_AMB641ZR02_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E3FA9_AMB667UM36_FHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E3FA9_AMB667UM36_FHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E3FA9_AMB667UM36_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_SW83109_BF067XMM_FHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_SW83109_BF067XMM_FHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = SW83109_BF067XMM_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_SW83109_BF067XMM_A73_FHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_SW83109_BF067XMM_A73_FHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = SW83109_BF067XMM_A73_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E3HAB_AMB667AN01_FHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E3HAB_AMB667AN01_FHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E3HAB_AMB667AN01_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_S6E3HAB_AMB667AN01_A73_FHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_S6E3HAB_AMB667AN01_A73_FHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = S6E3HAB_AMB667AN01_A73_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_SW83109_WM6676Z21_FHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_SW83109_WM6676Z21_FHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = SW83109_WM6676Z21_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_A73_S6E3FC3_AMB667BV01_FHD)
	else if (!strncmp(panel_string, "A73_S6E3FC3_AMB667BV01", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = A73_S6E3FC3_AMB667BV01_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_NT36672C_TL066FVMC02_FHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_NT36672C_TL066FVMC02_FHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = NT36672C_TL066FVMC02_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_NT36672C_PM6585JB3_FHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_NT36672C_PM6585JB3_FHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = NT36672C_PM6585JB3_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_NT36672C_PM6585JB3_M23_FHD)
	else if (!strncmp(panel_string, "ss_dsi_panel_NT36672C_PM6585JB3_M23_FHD", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = NT36672C_PM6585JB3_M23_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_XCP2_NT36672C_PM6585JB2_FHD)
	else if (!strncmp(panel_string, "XCP2_NT36672C_PM6585JB2", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = XCP2_NT36672C_PM6585JB2_FHD_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_TAP2_HX8279_TV101WUM_WUXGA)
	else if (!strncmp(panel_string, "TAP2_HX8279_TV101WUM", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = TAP2_HX8279_TV101WUM_WUXGA_init;
#endif
#if IS_ENABLED(CONFIG_PANEL_GTACT4PRO_HX8279_TV101WUM_WUXGA)
	else if (!strncmp(panel_string, "GTACT4PRO_HX8279_TV101WUM", strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = GTACT4PRO_HX8279_TV101WUM_WUXGA_init;
#endif

	else {
		LCD_ERR(vdd, "%s not found\n", panel_string);
		return -1;
	}

	LCD_INFO(vdd, "%s found --\n", panel_string);
	return 0;
}
