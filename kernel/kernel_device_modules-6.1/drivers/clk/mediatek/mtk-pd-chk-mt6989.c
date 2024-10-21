// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include <dt-bindings/power/mt6989-power.h>

#include "mtk-pd-chk.h"
#include "clkchk-mt6989.h"
#include "clk-fmeter.h"
#include "clk-mt6989-fmeter.h"
#include "vcp_status.h"

#define TAG				"[pdchk] "
#define BUG_ON_CHK_ENABLE		0
#define EVT_LEN				40
#define PWR_ID_SHIFT			0
#define PWR_STA_SHIFT			8
#define HWV_INT_MTCMOS_TRIGGER		0x0008
#define HWV_IRQ_STATUS			0x0500

#define PWR_EN_ACK			(0xc0000000)

static DEFINE_SPINLOCK(pwr_trace_lock);
static unsigned int pwr_event[EVT_LEN];
static unsigned int evt_cnt, suspend_cnt;

static void trace_power_event(unsigned int id, unsigned int pwr_sta)
{
	unsigned long flags = 0;

	if (id >= MT6989_CHK_PD_NUM)
		return;

	spin_lock_irqsave(&pwr_trace_lock, flags);

	pwr_event[evt_cnt] = (id << PWR_ID_SHIFT) | (pwr_sta << PWR_STA_SHIFT);
	evt_cnt++;
	if (evt_cnt >= EVT_LEN)
		evt_cnt = 0;

	spin_unlock_irqrestore(&pwr_trace_lock, flags);
}

static void dump_power_event(void)
{
	unsigned long flags = 0;
	int i;

	spin_lock_irqsave(&pwr_trace_lock, flags);

	pr_notice("first idx: %d\n", evt_cnt);
	for (i = 0; i < EVT_LEN; i += 5)
		pr_notice("pwr_evt[%d] = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
				i,
				pwr_event[i],
				pwr_event[i + 1],
				pwr_event[i + 2],
				pwr_event[i + 3],
				pwr_event[i + 4]);

	spin_unlock_irqrestore(&pwr_trace_lock, flags);
}

/*
 * The clk names in Mediatek CCF.
 */

/* afe */
struct pd_check_swcg afe_swcgs[] = {
	SWCG("afe_memif_intbus_clk"),
	SWCG("afe_memif_hop_clk"),
	SWCG("afe_pcm1"),
	SWCG("afe_pcm0"),
	SWCG("afe_soundwire"),
	SWCG("afe_cm1"),
	SWCG("afe_cm0"),
	SWCG("afe_stf"),
	SWCG("afe_hw_gain23"),
	SWCG("afe_hw_gain01"),
	SWCG("afe_daibt"),
	SWCG("afe_merge_if"),
	SWCG("afe_fm_i2s"),
	SWCG("afe_ul2_aht"),
	SWCG("afe_ul2_adc_hires"),
	SWCG("afe_ul2_tml"),
	SWCG("afe_ul2_adc"),
	SWCG("afe_ul1_aht"),
	SWCG("afe_ul1_adc_hires"),
	SWCG("afe_ul1_tml"),
	SWCG("afe_ul1_adc"),
	SWCG("afe_ul0_aht"),
	SWCG("afe_ul0_adc_hires"),
	SWCG("afe_ul0_tml"),
	SWCG("afe_ul0_adc"),
	SWCG("afe_dprx_ck"),
	SWCG("afe_dptx_ck"),
	SWCG("afe_etdm_in6"),
	SWCG("afe_etdm_in5"),
	SWCG("afe_etdm_in4"),
	SWCG("afe_etdm_in3"),
	SWCG("afe_etdm_in2"),
	SWCG("afe_etdm_in1"),
	SWCG("afe_etdm_in0"),
	SWCG("afe_etdm_out6"),
	SWCG("afe_etdm_out5"),
	SWCG("afe_etdm_out4"),
	SWCG("afe_etdm_out3"),
	SWCG("afe_etdm_out2"),
	SWCG("afe_etdm_out1"),
	SWCG("afe_etdm_out0"),
	SWCG("afe_general24_asrc"),
	SWCG("afe_general23_asrc"),
	SWCG("afe_general22_asrc"),
	SWCG("afe_general21_asrc"),
	SWCG("afe_general20_asrc"),
	SWCG("afe_general19_asrc"),
	SWCG("afe_general18_asrc"),
	SWCG("afe_general17_asrc"),
	SWCG("afe_general16_asrc"),
	SWCG("afe_general15_asrc"),
	SWCG("afe_general14_asrc"),
	SWCG("afe_general13_asrc"),
	SWCG("afe_general12_asrc"),
	SWCG("afe_general11_asrc"),
	SWCG("afe_general10_asrc"),
	SWCG("afe_general9_asrc"),
	SWCG("afe_general8_asrc"),
	SWCG("afe_general7_asrc"),
	SWCG("afe_general6_asrc"),
	SWCG("afe_general5_asrc"),
	SWCG("afe_general4_asrc"),
	SWCG("afe_general3_asrc"),
	SWCG("afe_general2_asrc"),
	SWCG("afe_general1_asrc"),
	SWCG("afe_general0_asrc"),
	SWCG("afe_connsys_i2s_asrc"),
	SWCG("afe_audio_hopping_ck"),
	SWCG("afe_audio_f26m_ck"),
	SWCG("afe_apll1_ck"),
	SWCG("afe_apll2_ck"),
	SWCG("afe_h208m_ck"),
	SWCG("afe_apll_tuner2"),
	SWCG("afe_apll_tuner1"),
	SWCG(NULL),
};
/* dispsys_config */
struct pd_check_swcg dispsys_config_swcgs[] = {
	SWCG("mm_config"),
	SWCG("mm_disp_mutex0"),
	SWCG("mm_disp_aal0"),
	SWCG("mm_disp_aal1"),
	SWCG("mm_disp_c3d0"),
	SWCG("mm_disp_c3d1"),
	SWCG("mm_disp_ccorr0"),
	SWCG("mm_disp_ccorr1"),
	SWCG("mm_disp_ccorr2"),
	SWCG("mm_disp_ccorr3"),
	SWCG("mm_disp_chist0"),
	SWCG("mm_disp_chist1"),
	SWCG("mm_disp_color0"),
	SWCG("mm_disp_color1"),
	SWCG("mm_disp_dither0"),
	SWCG("mm_disp_dither1"),
	SWCG("mm_disp_dither2"),
	SWCG("mm_dli_async0"),
	SWCG("mm_dli_async1"),
	SWCG("mm_dli_async2"),
	SWCG("mm_dli_async3"),
	SWCG("mm_dli_async4"),
	SWCG("mm_dli_async5"),
	SWCG("mm_dli_async6"),
	SWCG("mm_dli_async7"),
	SWCG("mm_dlo_async0"),
	SWCG("mm_dlo_async1"),
	SWCG("mm_dlo_async2"),
	SWCG("mm_dlo_async3"),
	SWCG("mm_dlo_async4"),
	SWCG("mm_disp_gamma0"),
	SWCG("mm_disp_gamma1"),
	SWCG("mm_mdp_aal0"),
	SWCG("mm_mdp_rdma0"),
	SWCG("mm_disp_oddmr0"),
	SWCG("mm_disp_postalign0"),
	SWCG("mm_disp_postmask0"),
	SWCG("mm_disp_postmask1"),
	SWCG("mm_disp_rsz0"),
	SWCG("mm_disp_rsz1"),
	SWCG("mm_disp_spr0"),
	SWCG("mm_disp_tdshp0"),
	SWCG("mm_disp_tdshp1"),
	SWCG("mm_disp_wdma1"),
	SWCG("mm_disp_y2r0"),
	SWCG("mm_mdp_aal1"),
	SWCG("mm_ssc"),
	SWCG(NULL),
};
/* dispsys1_config */
struct pd_check_swcg dispsys1_config_swcgs[] = {
	SWCG("mm1_dispsys1_config"),
	SWCG("mm1_disp_mutex0"),
	SWCG("mm1_disp_dli_async0"),
	SWCG("mm1_disp_dli_async1"),
	SWCG("mm1_disp_dli_async2"),
	SWCG("mm1_mdp_rdma0"),
	SWCG("mm1_disp_r2y0"),
	SWCG("mm1_disp_splitter0"),
	SWCG("mm1_disp_splitter1"),
	SWCG("mm1_disp_vdcm0"),
	SWCG("mm1_disp_dsc_wrap0"),
	SWCG("mm1_disp_dsc_wrap1"),
	SWCG("mm1_disp_dsc_wrap2"),
	SWCG("mm1_DP_CLK"),
	SWCG("mm1_CLK0"),
	SWCG("mm1_CLK1"),
	SWCG("mm1_CLK2"),
	SWCG("mm1_disp_merge0"),
	SWCG("mm1_disp_wdma0"),
	SWCG("mm1_ssc"),
	SWCG("mm1_disp_wdma1"),
	SWCG("mm1_disp_wdma2"),
	SWCG("mm1_disp_gdma0"),
	SWCG("mm1_disp_dli_async3"),
	SWCG("mm1_disp_dli_async4"),
	SWCG("mm1_mod1"),
	SWCG("mm1_mod2"),
	SWCG("mm1_mod3"),
	SWCG("mm1_mod4"),
	SWCG("mm1_mod5"),
	SWCG("mm1_mod6"),
	SWCG("mm1_mod7"),
	SWCG("mm1_subsys_ck"),
	SWCG("mm1_dsi0_ck"),
	SWCG("mm1_dsi1_ck"),
	SWCG("mm1_dsi2_ck"),
	SWCG("mm1_dp_ck"),
	SWCG("mm1_f26m_ck"),
	SWCG(NULL),
};
/* ovlsys_config */
struct pd_check_swcg ovlsys_config_swcgs[] = {
	SWCG("ovlsys_config"),
	SWCG("ovl_disp_fake_eng0"),
	SWCG("ovl_disp_fake_eng1"),
	SWCG("ovl_disp_mutex0"),
	SWCG("ovl_disp_ovl0_2l"),
	SWCG("ovl_disp_ovl1_2l"),
	SWCG("ovl_disp_ovl2_2l"),
	SWCG("ovl_disp_ovl3_2l"),
	SWCG("ovl_disp_rsz1"),
	SWCG("ovl_mdp_rsz0"),
	SWCG("ovl_disp_wdma0"),
	SWCG("ovl_disp_ufbc_wdma0"),
	SWCG("ovl_disp_wdma2"),
	SWCG("ovl_disp_dli_async0"),
	SWCG("ovl_disp_dli_async1"),
	SWCG("ovl_disp_dli_async2"),
	SWCG("ovl_disp_dl0_async0"),
	SWCG("ovl_disp_dl0_async1"),
	SWCG("ovl_disp_dl0_async2"),
	SWCG("ovl_disp_dl0_async3"),
	SWCG("ovl_disp_dl0_async4"),
	SWCG("ovl_disp_dl0_async5"),
	SWCG("ovl_disp_dl0_async6"),
	SWCG("ovl_inlinerot0"),
	SWCG("ovl_ssc"),
	SWCG("ovl_disp_y2r0"),
	SWCG("ovl_disp_y2r1"),
	SWCG("ovl_disp_ovl4_2l"),
	SWCG(NULL),
};
/* ovlsys1_config */
struct pd_check_swcg ovlsys1_config_swcgs[] = {
	SWCG("ovl1_ovlsys_config"),
	SWCG("ovl1_disp_fake_eng0"),
	SWCG("ovl1_disp_fake_eng1"),
	SWCG("ovl1_disp_mutex0"),
	SWCG("ovl1_disp_ovl0_2l"),
	SWCG("ovl1_disp_ovl1_2l"),
	SWCG("ovl1_disp_ovl2_2l"),
	SWCG("ovl1_disp_ovl3_2l"),
	SWCG("ovl1_disp_rsz1"),
	SWCG("ovl1_mdp_rsz0"),
	SWCG("ovl1_disp_wdma0"),
	SWCG("ovl1_disp_ufbc_wdma0"),
	SWCG("ovl1_disp_wdma2"),
	SWCG("ovl1_disp_dli_async0"),
	SWCG("ovl1_disp_dli_async1"),
	SWCG("ovl1_disp_dli_async2"),
	SWCG("ovl1_disp_dl0_async0"),
	SWCG("ovl1_disp_dl0_async1"),
	SWCG("ovl1_disp_dl0_async2"),
	SWCG("ovl1_disp_dl0_async3"),
	SWCG("ovl1_disp_dl0_async4"),
	SWCG("ovl1_disp_dl0_async5"),
	SWCG("ovl1_disp_dl0_async6"),
	SWCG("ovl1_inlinerot0"),
	SWCG("ovl1_ssc"),
	SWCG("ovl1_disp_y2r0"),
	SWCG("ovl1_disp_y2r1"),
	SWCG("ovl1_disp_ovl4_2l"),
	SWCG(NULL),
};
/* imgsys_main */
struct pd_check_swcg imgsys_main_swcgs[] = {
	SWCG("img_fdvt"),
	SWCG("img_me"),
	SWCG("img_mmg"),
	SWCG("img_larb12"),
	SWCG("img_larb9"),
	SWCG("img_traw0"),
	SWCG("img_traw1"),
	SWCG("img_dip0"),
	SWCG("img_wpe0"),
	SWCG("img_ipe"),
	SWCG("img_wpe1"),
	SWCG("img_wpe2"),
	SWCG("img_adl_larb"),
	SWCG("img_adlrd"),
	SWCG("img_adlwr"),
	SWCG("img_avs"),
	SWCG("img_ips"),
	SWCG("img_sub_common0"),
	SWCG("img_sub_common1"),
	SWCG("img_sub_common2"),
	SWCG("img_sub_common3"),
	SWCG("img_sub_common4"),
	SWCG("img_gals_rx_dip0"),
	SWCG("img_gals_rx_dip1"),
	SWCG("img_gals_rx_traw0"),
	SWCG("img_gals_rx_wpe0"),
	SWCG("img_gals_rx_wpe1"),
	SWCG("img_gals_rx_wpe2"),
	SWCG("img_gals_rx_ipe0"),
	SWCG("img_gals_tx_ipe0"),
	SWCG("img_gals_rx_ipe1"),
	SWCG("img_gals_tx_ipe1"),
	SWCG("img_gals"),
	SWCG(NULL),
};
/* dip_top_dip1 */
struct pd_check_swcg dip_top_dip1_swcgs[] = {
	SWCG("dip_dip1_dip_top"),
	SWCG("dip_dip1_dip_gals0"),
	SWCG("dip_dip1_dip_gals1"),
	SWCG("dip_dip1_dip_gals2"),
	SWCG("dip_dip1_dip_gals3"),
	SWCG("dip_dip1_larb10"),
	SWCG("dip_dip1_larb15"),
	SWCG("dip_dip1_larb38"),
	SWCG("dip_dip1_larb39"),
	SWCG(NULL),
};
/* dip_nr1_dip1 */
struct pd_check_swcg dip_nr1_dip1_swcgs[] = {
	SWCG("dip_nr1_dip1_larb"),
	SWCG("dip_nr1_dip1_dip_nr1"),
	SWCG(NULL),
};
/* dip_nr2_dip1 */
struct pd_check_swcg dip_nr2_dip1_swcgs[] = {
	SWCG("dip_nr2_dip1_larb15"),
	SWCG("dip_nr2_dip1_dip_nr"),
	SWCG("dip_nr2_dip1_larb39"),
	SWCG(NULL),
};
/* wpe1_dip1 */
struct pd_check_swcg wpe1_dip1_swcgs[] = {
	SWCG("wpe1_dip1_larb11"),
	SWCG("wpe1_dip1_wpe"),
	SWCG("wpe1_dip1_gals0"),
	SWCG(NULL),
};
/* wpe2_dip1 */
struct pd_check_swcg wpe2_dip1_swcgs[] = {
	SWCG("wpe2_dip1_larb11"),
	SWCG("wpe2_dip1_wpe"),
	SWCG("wpe2_dip1_gals0"),
	SWCG(NULL),
};
/* wpe3_dip1 */
struct pd_check_swcg wpe3_dip1_swcgs[] = {
	SWCG("wpe3_dip1_larb11"),
	SWCG("wpe3_dip1_wpe"),
	SWCG("wpe3_dip1_gals0"),
	SWCG(NULL),
};
/* traw_dip1 */
struct pd_check_swcg traw_dip1_swcgs[] = {
	SWCG("traw_dip1_larb28"),
	SWCG("traw_dip1_larb40"),
	SWCG("traw_dip1_traw"),
	SWCG("traw_dip1_gals"),
	SWCG(NULL),
};
/* traw_cap_dip1 */
struct pd_check_swcg traw_cap_dip1_swcgs[] = {
	SWCG("traw__dip1_cap"),
	SWCG(NULL),
};
/* img_vcore_d1a */
struct pd_check_swcg img_vcore_d1a_swcgs[] = {
	SWCG("img_vcore_gals_disp"),
	SWCG("img_vcore_main"),
	SWCG("img_vcore_sub0"),
	SWCG("img_vcore_sub1"),
	SWCG(NULL),
};
/* vdec_soc_gcon_base */
struct pd_check_swcg vdec_soc_gcon_base_swcgs[] = {
	SWCG("vde1_larb1_cken"),
	SWCG("vde1_lat_cken"),
	SWCG("vde1_lat_active"),
	SWCG("vde1_lat_cken_eng"),
	SWCG("vde1_vdec_cken"),
	SWCG("vde1_vdec_active"),
	SWCG("vde1_vdec_cken_eng"),
	SWCG("vde1_aptv_en"),
	SWCG("vde1_aptv_topen"),
	SWCG("vde1_vdec_soc_ips_en"),
	SWCG(NULL),
};
/* vdec_gcon_base */
struct pd_check_swcg vdec_gcon_base_swcgs[] = {
	SWCG("vde2_larb1_cken"),
	SWCG("vde2_lat_cken"),
	SWCG("vde2_lat_active"),
	SWCG("vde2_lat_cken_eng"),
	SWCG("vde2_vdec_cken"),
	SWCG("vde2_vdec_active"),
	SWCG("vde2_vdec_cken_eng"),
	SWCG(NULL),
};
/* venc_gcon */
struct pd_check_swcg venc_gcon_swcgs[] = {
	SWCG("ven1_larb"),
	SWCG("ven1_venc"),
	SWCG("ven1_jpgenc"),
	SWCG("ven1_jpgdec"),
	SWCG("ven1_jpgdec_c1"),
	SWCG("ven1_gals"),
	SWCG("ven1_gals_sram"),
	SWCG(NULL),
};
/* venc_gcon_core1 */
struct pd_check_swcg venc_gcon_core1_swcgs[] = {
	SWCG("ven2_larb"),
	SWCG("ven2_venc"),
	SWCG("ven2_jpgenc"),
	SWCG("ven2_jpgdec"),
	SWCG("ven2_gals"),
	SWCG("ven2_gals_sram"),
	SWCG(NULL),
};
/* venc_gcon_core2 */
struct pd_check_swcg venc_gcon_core2_swcgs[] = {
	SWCG("ven_c2_larb"),
	SWCG("ven_c2_venc"),
	SWCG("ven_c2_jpgenc"),
	SWCG("ven_c2_gals"),
	SWCG("ven_c2_gals_sram"),
	SWCG(NULL),
};
/* camsys_main */
struct pd_check_swcg camsys_main_swcgs[] = {
	SWCG("cam_m_larb13"),
	SWCG("cam_m_larb14"),
	SWCG("cam_m_larb27"),
	SWCG("cam_m_larb29"),
	SWCG("cam_m_cam"),
	SWCG("cam_m_cam_suba"),
	SWCG("cam_m_cam_subb"),
	SWCG("cam_m_cam_subc"),
	SWCG("cam_m_cam_mraw"),
	SWCG("cam_m_camtg"),
	SWCG("cam_m_seninf"),
	SWCG("cam_m_camsv"),
	SWCG("cam_m_adlrd"),
	SWCG("cam_m_adlwr"),
	SWCG("cam_m_uisp"),
	SWCG("cam_m_cam2mm0_GCON_0"),
	SWCG("cam_m_cam2mm1_GCON_0"),
	SWCG("cam_m_cam2sys_GCON_0"),
	SWCG("cam_m_cam2mm2_GCON_0"),
	SWCG("cam_m_ccusys"),
	SWCG("cam_m_cam_dpe"),
	SWCG("cam_m_cam_asg"),
	SWCG("cam_m_camsv_a_con_1"),
	SWCG("cam_m_camsv_b_con_1"),
	SWCG("cam_m_camsv_c_con_1"),
	SWCG("cam_m_camsv_d_con_1"),
	SWCG("cam_m_camsv_e_con_1"),
	SWCG("cam_m_camsv_con_1"),
	SWCG(NULL),
};
/* camsys_rmsa */
struct pd_check_swcg camsys_rmsa_swcgs[] = {
	SWCG("camsys_rmsa_larbx"),
	SWCG("camsys_rmsa_cam"),
	SWCG("camsys_rmsa_camtg"),
	SWCG(NULL),
};
/* camsys_rawa */
struct pd_check_swcg camsys_rawa_swcgs[] = {
	SWCG("cam_ra_larbx"),
	SWCG("cam_ra_cam"),
	SWCG("cam_ra_camtg"),
	SWCG(NULL),
};
/* camsys_yuva */
struct pd_check_swcg camsys_yuva_swcgs[] = {
	SWCG("cam_ya_larbx"),
	SWCG("cam_ya_cam"),
	SWCG("cam_ya_camtg"),
	SWCG(NULL),
};
/* camsys_rmsb */
struct pd_check_swcg camsys_rmsb_swcgs[] = {
	SWCG("camsys_rmsb_larbx"),
	SWCG("camsys_rmsb_cam"),
	SWCG("camsys_rmsb_camtg"),
	SWCG(NULL),
};
/* camsys_rawb */
struct pd_check_swcg camsys_rawb_swcgs[] = {
	SWCG("cam_rb_larbx"),
	SWCG("cam_rb_cam"),
	SWCG("cam_rb_camtg"),
	SWCG(NULL),
};
/* camsys_yuvb */
struct pd_check_swcg camsys_yuvb_swcgs[] = {
	SWCG("cam_yb_larbx"),
	SWCG("cam_yb_cam"),
	SWCG("cam_yb_camtg"),
	SWCG(NULL),
};
/* camsys_rmsc */
struct pd_check_swcg camsys_rmsc_swcgs[] = {
	SWCG("camsys_rmsc_larbx"),
	SWCG("camsys_rmsc_cam"),
	SWCG("camsys_rmsc_camtg"),
	SWCG(NULL),
};
/* camsys_rawc */
struct pd_check_swcg camsys_rawc_swcgs[] = {
	SWCG("cam_rc_larbx"),
	SWCG("cam_rc_cam"),
	SWCG("cam_rc_camtg"),
	SWCG(NULL),
};
/* camsys_yuvc */
struct pd_check_swcg camsys_yuvc_swcgs[] = {
	SWCG("cam_yc_larbx"),
	SWCG("cam_yc_cam"),
	SWCG("cam_yc_camtg"),
	SWCG(NULL),
};
/* camsys_mraw */
struct pd_check_swcg camsys_mraw_swcgs[] = {
	SWCG("cam_mr_larbx"),
	SWCG("cam_mr_gals"),
	SWCG("cam_mr_camtg"),
	SWCG("cam_mr_mraw0"),
	SWCG("cam_mr_mraw1"),
	SWCG("cam_mr_mraw2"),
	SWCG("cam_mr_mraw3"),
	SWCG("cam_mr_pda0"),
	SWCG("cam_mr_pda1"),
	SWCG(NULL),
};
/* camsys_ipe */
struct pd_check_swcg camsys_ipe_swcgs[] = {
	SWCG("camsys_ipe_larb19"),
	SWCG("camsys_ipe_dpe"),
	SWCG("camsys_ipe_fus"),
	SWCG("camsys_ipe_dhze"),
	SWCG("camsys_ipe_gals"),
	SWCG(NULL),
};
/* ccu_main */
struct pd_check_swcg ccu_main_swcgs[] = {
	SWCG("ccu_larb30_con"),
	SWCG("ccu_ahb_con"),
	SWCG("ccusys_ccu0_con"),
	SWCG("ccusys_ccu1_con"),
	SWCG("ccu2mm0_GCON"),
	SWCG(NULL),
};
/* cam_vcore */
struct pd_check_swcg cam_vcore_swcgs[] = {
	SWCG("CAM_V_MM0_SUBCOMM"),
	SWCG(NULL),
};
/* mminfra_config */
struct pd_check_swcg mminfra_config_swcgs[] = {
	SWCG("mminfra_smi"),
	SWCG(NULL),
};
/* mminfra_ao_config */
struct pd_check_swcg mminfra_ao_config_swcgs[] = {
	SWCG("mminfra_ao_gce_d"),
	SWCG("mminfra_ao_gce_m"),
	SWCG("mminfra_ao_gce_26m"),
	SWCG(NULL),
};
/* mdpsys_config */
struct pd_check_swcg mdpsys_config_swcgs[] = {
	SWCG("mdp_mutex0"),
	SWCG("mdp_apb_bus"),
	SWCG("mdp_smi0"),
	SWCG("mdp_rdma0"),
	SWCG("mdp_rdma2"),
	SWCG("mdp_hdr0"),
	SWCG("mdp_aal0"),
	SWCG("mdp_rsz0"),
	SWCG("mdp_tdshp0"),
	SWCG("mdp_color0"),
	SWCG("mdp_wrot0"),
	SWCG("mdp_fake_eng0"),
	SWCG("mdp_dli_async0"),
	SWCG("mdp_dli_async1"),
	SWCG("mdp_apb_db"),
	SWCG("mdp_rdma1"),
	SWCG("mdp_rdma3"),
	SWCG("mdp_hdr1"),
	SWCG("mdp_aal1"),
	SWCG("mdp_rsz1"),
	SWCG("mdp_tdshp1"),
	SWCG("mdp_color1"),
	SWCG("mdp_wrot1"),
	SWCG("mdp_rsz2"),
	SWCG("mdp_wrot2"),
	SWCG("mdp_dlo_async0"),
	SWCG("mdp_rsz3"),
	SWCG("mdp_wrot3"),
	SWCG("mdp_dlo_async1"),
	SWCG("mdp_dli_async2"),
	SWCG("mdp_dli_async3"),
	SWCG("mdp_dlo_async2"),
	SWCG("mdp_dlo_async3"),
	SWCG("mdp_birsz0"),
	SWCG("mdp_birsz1"),
	SWCG("mdp_img_dl_async0"),
	SWCG("mdp_img_dl_async1"),
	SWCG("mdp_hre_mdpsys"),
	SWCG("mdp_rrot0"),
	SWCG("mdp_rrot0_2nd"),
	SWCG("mdp_merge0"),
	SWCG("mdp_c3d0"),
	SWCG("mdp_fg0"),
	SWCG("mdp_img_dl_relay0"),
	SWCG("mdp_f26m_slow_ck"),
	SWCG("mdp_f32k_slow_ck"),
	SWCG("mdp_img_dl_relay1"),
	SWCG(NULL),
};
/* mdpsys1_config */
struct pd_check_swcg mdpsys1_config_swcgs[] = {
	SWCG("mdp1_mdp_mutex0"),
	SWCG("mdp1_apb_bus"),
	SWCG("mdp1_smi0"),
	SWCG("mdp1_mdp_rdma0"),
	SWCG("mdp1_mdp_rdma2"),
	SWCG("mdp1_mdp_hdr0"),
	SWCG("mdp1_mdp_aal0"),
	SWCG("mdp1_mdp_rsz0"),
	SWCG("mdp1_mdp_tdshp0"),
	SWCG("mdp1_mdp_color0"),
	SWCG("mdp1_mdp_wrot0"),
	SWCG("mdp1_mdp_fake_eng0"),
	SWCG("mdp1_mdp_dli_async0"),
	SWCG("mdp1_mdp_dli_async1"),
	SWCG("mdp1_apb_db"),
	SWCG("mdp1_mdp_rdma1"),
	SWCG("mdp1_mdp_rdma3"),
	SWCG("mdp1_mdp_hdr1"),
	SWCG("mdp1_mdp_aal1"),
	SWCG("mdp1_mdp_rsz1"),
	SWCG("mdp1_mdp_tdshp1"),
	SWCG("mdp1_mdp_color1"),
	SWCG("mdp1_mdp_wrot1"),
	SWCG("mdp1_mdp_rsz2"),
	SWCG("mdp1_mdp_wrot2"),
	SWCG("mdp1_mdp_dlo_async0"),
	SWCG("mdp1_mdp_rsz3"),
	SWCG("mdp1_mdp_wrot3"),
	SWCG("mdp1_mdp_dlo_async1"),
	SWCG("mdp1_mdp_dli_async2"),
	SWCG("mdp1_mdp_dli_async3"),
	SWCG("mdp1_mdp_dlo_async2"),
	SWCG("mdp1_mdp_dlo_async3"),
	SWCG("mdp1_mdp_birsz0"),
	SWCG("mdp1_mdp_birsz1"),
	SWCG("mdp1_img_dl_async0"),
	SWCG("mdp1_img_dl_async1"),
	SWCG("mdp1_hre_mdpsys"),
	SWCG("mdp1_mdp_rrot0"),
	SWCG("mdp1_mdp_rrot0_2nd"),
	SWCG("mdp1_mdp_merge0"),
	SWCG("mdp1_mdp_c3d0"),
	SWCG("mdp1_mdp_fg0"),
	SWCG("mdp1_img_dl_relay0"),
	SWCG("mdp1_f26m_slow_ck"),
	SWCG("mdp1_f32k_slow_ck"),
	SWCG("mdp1_img_dl_relay1"),
	SWCG(NULL),
};

struct subsys_cgs_check {
	unsigned int pd_id;		/* power domain id */
	int pd_parent;			/* power domain parent id */
	struct pd_check_swcg *swcgs;	/* those CGs that would be checked */
	enum chk_sys_id chk_id;		/*
					 * chk_id is used in
					 * print_subsys_reg() and can be NULL
					 * if not porting ready yet.
					 */
};

struct subsys_cgs_check mtk_subsys_check[] = {
	{MT6989_CHK_PD_AUDIO, PD_NULL, afe_swcgs, afe},
	{MT6989_CHK_PD_DIS0, MT6989_CHK_PD_DIS1, dispsys_config_swcgs, mm},
	{MT6989_CHK_PD_DIS1, MT6989_CHK_PD_DISP_VCORE, dispsys1_config_swcgs, mm1},
	{MT6989_CHK_PD_OVL0, MT6989_CHK_PD_DIS1, ovlsys_config_swcgs, ovl},
	{MT6989_CHK_PD_OVL1, MT6989_CHK_PD_DIS1, ovlsys1_config_swcgs, ovl1},
	{MT6989_CHK_PD_ISP_MAIN, MT6989_CHK_PD_ISP_VCORE, imgsys_main_swcgs, img},
	{MT6989_CHK_PD_ISP_DIP1, MT6989_CHK_PD_ISP_MAIN, dip_top_dip1_swcgs, dip_top_dip1},
	{MT6989_CHK_PD_ISP_DIP1, MT6989_CHK_PD_ISP_MAIN, dip_nr1_dip1_swcgs, dip_nr1_dip1},
	{MT6989_CHK_PD_ISP_DIP1, MT6989_CHK_PD_ISP_MAIN, dip_nr2_dip1_swcgs, dip_nr2_dip1},
	{MT6989_CHK_PD_ISP_DIP1, MT6989_CHK_PD_ISP_MAIN, wpe1_dip1_swcgs, wpe1_dip1},
	{MT6989_CHK_PD_ISP_DIP1, MT6989_CHK_PD_ISP_MAIN, wpe2_dip1_swcgs, wpe2_dip1},
	{MT6989_CHK_PD_ISP_DIP1, MT6989_CHK_PD_ISP_MAIN, wpe3_dip1_swcgs, wpe3_dip1},
	{MT6989_CHK_PD_ISP_DIP1, MT6989_CHK_PD_ISP_MAIN, traw_dip1_swcgs, traw_dip1},
	{MT6989_CHK_PD_ISP_DIP1, MT6989_CHK_PD_ISP_MAIN, traw_cap_dip1_swcgs, traw_cap_dip1},
	{MT6989_CHK_PD_ISP_MAIN, MT6989_CHK_PD_ISP_VCORE, img_vcore_d1a_swcgs, img_v},
	{MT6989_CHK_PD_VDE0, MT6989_CHK_PD_VDE_VCORE0, vdec_soc_gcon_base_swcgs, vde1},
	{MT6989_CHK_PD_VDE1, MT6989_CHK_PD_VDE_VCORE1, vdec_gcon_base_swcgs, vde2},
	{MT6989_CHK_PD_VEN0, MT6989_CHK_PD_MM_INFRA, venc_gcon_swcgs, ven1},
	{MT6989_CHK_PD_VEN1, MT6989_CHK_PD_VEN0, venc_gcon_core1_swcgs, ven2},
	{MT6989_CHK_PD_VEN2, MT6989_CHK_PD_VEN1, venc_gcon_core2_swcgs, ven_c2},
	{MT6989_CHK_PD_CAM_MAIN, MT6989_CHK_PD_CAM_VCORE, camsys_main_swcgs, cam_m},
	{MT6989_CHK_PD_CAM_SUBA, MT6989_CHK_PD_CAM_MAIN, camsys_rmsa_swcgs, camsys_rmsa},
	{MT6989_CHK_PD_CAM_SUBA, MT6989_CHK_PD_CAM_MAIN, camsys_rawa_swcgs, cam_ra},
	{MT6989_CHK_PD_CAM_SUBA, MT6989_CHK_PD_CAM_MAIN, camsys_yuva_swcgs, cam_ya},
	{MT6989_CHK_PD_CAM_SUBB, MT6989_CHK_PD_CAM_MAIN, camsys_rmsb_swcgs, camsys_rmsb},
	{MT6989_CHK_PD_CAM_SUBB, MT6989_CHK_PD_CAM_MAIN, camsys_rawb_swcgs, cam_rb},
	{MT6989_CHK_PD_CAM_SUBB, MT6989_CHK_PD_CAM_MAIN, camsys_yuvb_swcgs, cam_yb},
	{MT6989_CHK_PD_CAM_SUBC, MT6989_CHK_PD_CAM_MAIN, camsys_rmsc_swcgs, camsys_rmsc},
	{MT6989_CHK_PD_CAM_SUBC, MT6989_CHK_PD_CAM_MAIN, camsys_rawc_swcgs, cam_rc},
	{MT6989_CHK_PD_CAM_SUBC, MT6989_CHK_PD_CAM_MAIN, camsys_yuvc_swcgs, cam_yc},
	{MT6989_CHK_PD_CAM_MRAW, MT6989_CHK_PD_CAM_MAIN, camsys_mraw_swcgs, cam_mr},
	{MT6989_CHK_PD_CAM_MRAW, MT6989_CHK_PD_CAM_MAIN, camsys_ipe_swcgs, camsys_ipe},
	{MT6989_CHK_PD_CAM_CCU, MT6989_CHK_PD_CAM_VCORE, ccu_main_swcgs, ccu},
	{MT6989_CHK_PD_CAM_MAIN, MT6989_CHK_PD_CAM_VCORE, cam_vcore_swcgs, cam_vcore},
	{MT6989_CHK_PD_MM_INFRA, PD_NULL, mminfra_config_swcgs, mminfra_config},
	{MT6989_CHK_PD_MM_INFRA, PD_NULL, mminfra_ao_config_swcgs, mminfra_ao_config},
	{MT6989_CHK_PD_MML0, MT6989_CHK_PD_DIS1, mdpsys_config_swcgs, mdp},
	{MT6989_CHK_PD_MML1, MT6989_CHK_PD_DIS1, mdpsys1_config_swcgs, mdp1},
};

static struct pd_check_swcg *get_subsys_cg(unsigned int id)
{
	int i;

	if (id >= MT6989_CHK_PD_NUM)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			return mtk_subsys_check[i].swcgs;
	}

	return NULL;
}

static void dump_subsys_reg(unsigned int id)
{
	int i;

	if (id >= MT6989_CHK_PD_NUM)
		return;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			print_subsys_reg_mt6989(mtk_subsys_check[i].chk_id);
	}
}

unsigned int pd_list[] = {
	MT6989_CHK_PD_MD1,
	MT6989_CHK_PD_CONN,
	MT6989_CHK_PD_PERI_USB0,
	MT6989_CHK_PD_PEXTP_MAC0,
	MT6989_CHK_PD_PEXTP_MAC1,
	MT6989_CHK_PD_PEXTP_PHY0,
	MT6989_CHK_PD_PEXTP_PHY1,
	MT6989_CHK_PD_AUDIO,
	MT6989_CHK_PD_ADSP_TOP,
	MT6989_CHK_PD_ADSP_AO,
	MT6989_CHK_PD_MM_INFRA,
	MT6989_CHK_PD_SSRSYS,
	MT6989_CHK_PD_ISP_TRAW,
	MT6989_CHK_PD_ISP_DIP1,
	MT6989_CHK_PD_ISP_MAIN,
	MT6989_CHK_PD_ISP_VCORE,
	MT6989_CHK_PD_VDE0,
	MT6989_CHK_PD_VDE1,
	MT6989_CHK_PD_VDE_VCORE0,
	MT6989_CHK_PD_VDE_VCORE1,
	MT6989_CHK_PD_VEN0,
	MT6989_CHK_PD_VEN1,
	MT6989_CHK_PD_VEN2,
	MT6989_CHK_PD_CAM_MRAW,
	MT6989_CHK_PD_CAM_SUBA,
	MT6989_CHK_PD_CAM_SUBB,
	MT6989_CHK_PD_CAM_SUBC,
	MT6989_CHK_PD_CAM_MAIN,
	MT6989_CHK_PD_CAM_VCORE,
	MT6989_CHK_PD_CAM_CCU,
	MT6989_CHK_PD_CAM_CCU_AO,
	MT6989_CHK_PD_DISP_VCORE,
	MT6989_CHK_PD_MML0,
	MT6989_CHK_PD_MML1,
	MT6989_CHK_PD_DIS0,
	MT6989_CHK_PD_DIS1,
	MT6989_CHK_PD_OVL0,
	MT6989_CHK_PD_OVL1,
	MT6989_CHK_PD_DP_TX,
	MT6989_CHK_PD_CSI_RX,
};

static bool is_in_pd_list(unsigned int id)
{
	int i;

	if (id >= MT6989_CHK_PD_NUM)
		return false;

	for (i = 0; i < ARRAY_SIZE(pd_list); i++) {
		if (id == pd_list[i])
			return true;
	}

	return false;
}

static enum chk_sys_id debug_dump_id[] = {
	spm,
	top,
	apmixed,
	bcrm_infra1_bus,
	ifr_bus,
	bcrm_ssr_bus,
	vlpcfg,
	vlp_ck,
	hfrp_2_bus,
	hfrp,
	hfrp_1_bus,
	hfrp_hwv,
	hwv,
	hwv_ext,
	mm_hwv,
	mm_hwv_ext,
	vlp_ao,
	chk_sys_num,
};

static void debug_dump(unsigned int id, unsigned int pwr_sta)
{
	const struct fmeter_clk *fclks;
	int i, parent_id = PD_NULL;
	bool need_chk = false;

	if (id >= MT6989_CHK_PD_NUM)
		return;

	fclks = mt_get_fmeter_clks();

	set_subsys_reg_dump_mt6989(debug_dump_id);

	get_subsys_reg_dump_mt6989();

	if ((get_mt6989_reg_value(spm, 0xea8) & PWR_EN_ACK) == PWR_EN_ACK)
		need_chk = false;
	else
		need_chk = true;

	/* vcp no need to vote mminfra */
	if (pwr_sta) {
		if (id == MT6989_CHK_PD_MM_INFRA)
			vcp_cmd_ex(VCP_SET_HALT_MMINFRA, "scpsys_mminfra_hwv_on");
		else
			vcp_cmd_ex(VCP_SET_HALT, "scpsys_hwv_on");
	} else {
		if (id == MT6989_CHK_PD_MM_INFRA)
			vcp_cmd_ex(VCP_SET_HALT_MMINFRA, "scpsys_mminfra_hwv_off");
		else
			vcp_cmd_ex(VCP_SET_HALT, "scpsys_hwv_off");
	}

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id) {
			print_subsys_reg_mt6989(mtk_subsys_check[i].chk_id);
			parent_id = mtk_subsys_check[i].pd_parent;
			break;
		}
	}

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (parent_id == PD_NULL)
			break;

		if (mtk_subsys_check[i].pd_id == parent_id)
			print_subsys_reg_mt6989(mtk_subsys_check[i].chk_id);
	}

	dump_power_event();

	for (; fclks != NULL && fclks->type != FT_NULL; fclks++) {
		if (fclks->type != VLPCK && fclks->type != SUBSYS)
			pr_notice("[%s] %d khz\n", fclks->name,
				mt_get_fmeter_freq(fclks->id, fclks->type));
	}

	mdelay(5000);
	BUG_ON(1);
}

static enum chk_sys_id log_dump_id[] = {
	spm,
	ifr_bus,
	chk_sys_num,
};

static void log_dump(unsigned int id, unsigned int pwr_sta)
{
	if (id >= MT6989_CHK_PD_NUM)
		return;

	if (id == MT6989_CHK_PD_MD1) {
		set_subsys_reg_dump_mt6989(log_dump_id);
		get_subsys_reg_dump_mt6989();
	}
}

static void external_dump(void)
{
	const struct fmeter_clk *fclks;

	fclks = mt_get_fmeter_clks();

	set_subsys_reg_dump_mt6989(debug_dump_id);

	get_subsys_reg_dump_mt6989();

	dump_power_event();

	for (; fclks != NULL && fclks->type != FT_NULL; fclks++) {
		if (fclks->type != VLPCK && fclks->type != SUBSYS)
			pr_notice("[%s] %d khz\n", fclks->name,
				mt_get_fmeter_freq(fclks->id, fclks->type));
	}
}

static struct pd_sta pd_pwr_sta[] = {
	{MT6989_CHK_PD_MD1, spm, 0x0E00, GENMASK(31, 30)},
	{MT6989_CHK_PD_CONN, spm, 0x0E04, GENMASK(31, 30)},
	{MT6989_CHK_PD_PERI_USB0, spm, 0x0E10, GENMASK(31, 30)},
	{MT6989_CHK_PD_PEXTP_MAC0, spm, 0x0E1C, GENMASK(31, 30)},
	{MT6989_CHK_PD_PEXTP_MAC1, spm, 0x0E20, GENMASK(31, 30)},
	{MT6989_CHK_PD_PEXTP_PHY0, spm, 0x0E24, GENMASK(31, 30)},
	{MT6989_CHK_PD_PEXTP_PHY1, spm, 0x0E28, GENMASK(31, 30)},
	{MT6989_CHK_PD_AUDIO, spm, 0x0E2C, GENMASK(31, 30)},
	{MT6989_CHK_PD_ADSP_TOP, spm, 0x0E34, GENMASK(31, 30)},
	{MT6989_CHK_PD_ADSP_AO, spm, 0x0E3C, GENMASK(31, 30)},
	{MT6989_CHK_PD_MM_INFRA, spm, 0x0EA8, GENMASK(31, 30)},
	{MT6989_CHK_PD_SSRSYS, spm, 0x0EF8, GENMASK(31, 30)},
	{MT6989_CHK_PD_ISP_TRAW, spm, 0x0E40, GENMASK(31, 30)},
	{MT6989_CHK_PD_ISP_DIP1, spm, 0x0E44, GENMASK(31, 30)},
	{MT6989_CHK_PD_ISP_MAIN, spm, 0x0E48, GENMASK(31, 30)},
	{MT6989_CHK_PD_ISP_VCORE, spm, 0x0E4C, GENMASK(31, 30)},
	{MT6989_CHK_PD_VDE0, spm, 0x0E50, GENMASK(31, 30)},
	{MT6989_CHK_PD_VDE1, spm, 0x0E54, GENMASK(31, 30)},
	{MT6989_CHK_PD_VDE_VCORE0, spm, 0x0E58, GENMASK(31, 30)},
	{MT6989_CHK_PD_VDE_VCORE1, spm, 0x0E5C, GENMASK(31, 30)},
	{MT6989_CHK_PD_VEN0, spm, 0x0E60, GENMASK(31, 30)},
	{MT6989_CHK_PD_VEN1, spm, 0x0E64, GENMASK(31, 30)},
	{MT6989_CHK_PD_VEN2, spm, 0x0E68, GENMASK(31, 30)},
	{MT6989_CHK_PD_CAM_MRAW, spm, 0x0E6C, GENMASK(31, 30)},
	{MT6989_CHK_PD_CAM_SUBA, spm, 0x0E70, GENMASK(31, 30)},
	{MT6989_CHK_PD_CAM_SUBB, spm, 0x0E74, GENMASK(31, 30)},
	{MT6989_CHK_PD_CAM_SUBC, spm, 0x0E78, GENMASK(31, 30)},
	{MT6989_CHK_PD_CAM_MAIN, spm, 0x0E7C, GENMASK(31, 30)},
	{MT6989_CHK_PD_CAM_VCORE, spm, 0x0E80, GENMASK(31, 30)},
	{MT6989_CHK_PD_CAM_CCU, spm, 0x0E84, GENMASK(31, 30)},
	{MT6989_CHK_PD_CAM_CCU_AO, spm, 0x0E88, GENMASK(31, 30)},
	{MT6989_CHK_PD_DISP_VCORE, spm, 0x0E8C, GENMASK(31, 30)},
	{MT6989_CHK_PD_MML0, spm, 0x0E90, GENMASK(31, 30)},
	{MT6989_CHK_PD_MML1, spm, 0x0E94, GENMASK(31, 30)},
	{MT6989_CHK_PD_DIS0, spm, 0x0E98, GENMASK(31, 30)},
	{MT6989_CHK_PD_DIS1, spm, 0x0E9C, GENMASK(31, 30)},
	{MT6989_CHK_PD_OVL0, spm, 0x0EA0, GENMASK(31, 30)},
	{MT6989_CHK_PD_OVL1, spm, 0x0EA4, GENMASK(31, 30)},
	{MT6989_CHK_PD_DP_TX, spm, 0x0EB0, GENMASK(31, 30)},
	{MT6989_CHK_PD_CSI_RX, spm, 0x0EB4, GENMASK(31, 30)},
};

static u32 get_pd_pwr_status(int pd_id)
{
	u32 val;
	int i;

	if (pd_id == PD_NULL || pd_id > ARRAY_SIZE(pd_pwr_sta))
		return 0;

	for (i = 0; i < ARRAY_SIZE(pd_pwr_sta); i++) {
		if (pd_id == pd_pwr_sta[i].pd_id) {
			val = get_mt6989_reg_value(pd_pwr_sta[i].base, pd_pwr_sta[i].ofs);
			if ((val & pd_pwr_sta[i].msk) == pd_pwr_sta[i].msk)
				return 1;
			else
				return 0;
		}
	}

	return 0;
}

static int off_mtcmos_id[] = {
	MT6989_CHK_PD_ADSP_AO,
	MT6989_CHK_PD_MM_INFRA,
	MT6989_CHK_PD_SSRSYS,
	MT6989_CHK_PD_ISP_TRAW,
	MT6989_CHK_PD_ISP_DIP1,
	MT6989_CHK_PD_ISP_MAIN,
	MT6989_CHK_PD_ISP_VCORE,
	MT6989_CHK_PD_VDE0,
	MT6989_CHK_PD_VDE1,
	MT6989_CHK_PD_VDE_VCORE0,
	MT6989_CHK_PD_VDE_VCORE1,
	MT6989_CHK_PD_VEN0,
	MT6989_CHK_PD_VEN1,
	MT6989_CHK_PD_VEN2,
	MT6989_CHK_PD_CAM_MRAW,
	MT6989_CHK_PD_CAM_SUBA,
	MT6989_CHK_PD_CAM_SUBB,
	MT6989_CHK_PD_CAM_SUBC,
	MT6989_CHK_PD_CAM_MAIN,
	MT6989_CHK_PD_CAM_VCORE,
	MT6989_CHK_PD_CAM_CCU,
	MT6989_CHK_PD_CAM_CCU_AO,
	MT6989_CHK_PD_DISP_VCORE,
	MT6989_CHK_PD_MML0,
	MT6989_CHK_PD_MML1,
	MT6989_CHK_PD_DIS0,
	MT6989_CHK_PD_DIS1,
	MT6989_CHK_PD_OVL0,
	MT6989_CHK_PD_OVL1,
	MT6989_CHK_PD_DP_TX,
	MT6989_CHK_PD_CSI_RX,
	PD_NULL,
};

static int notice_mtcmos_id[] = {
	MT6989_CHK_PD_MD1,
	MT6989_CHK_PD_CONN,
	MT6989_CHK_PD_PEXTP_MAC0,
	MT6989_CHK_PD_PEXTP_MAC1,
	MT6989_CHK_PD_PEXTP_PHY0,
	MT6989_CHK_PD_PEXTP_PHY1,
	MT6989_CHK_PD_PERI_USB0,
	MT6989_CHK_PD_AUDIO,
	MT6989_CHK_PD_ADSP_TOP,
	PD_NULL,
};

static int *get_off_mtcmos_id(void)
{
	return off_mtcmos_id;
}

static int *get_notice_mtcmos_id(void)
{
	return notice_mtcmos_id;
}

static bool is_mtcmos_chk_bug_on(void)
{
#if (BUG_ON_CHK_ENABLE) || (IS_ENABLED(CONFIG_MTK_CLKMGR_DEBUG))
	return true;
#endif
	return false;
}

static int suspend_allow_id[] = {
	PD_NULL,
};

static int *get_suspend_allow_id(void)
{
	return suspend_allow_id;
}

static bool pdchk_is_suspend_retry_stop(bool reset_cnt)
{
	if (reset_cnt == true) {
		suspend_cnt = 0;
		return true;
	}

	suspend_cnt++;
	pr_notice("%s: suspend cnt: %d\n", __func__, suspend_cnt);

	if (suspend_cnt < 2)
		return false;

	return true;
}

static void check_hwv_irq_sta(void)
{
	u32 irq_sta;

	irq_sta = get_mt6989_reg_value(hwv_ext, HWV_IRQ_STATUS);

	if ((irq_sta & HWV_INT_MTCMOS_TRIGGER) == HWV_INT_MTCMOS_TRIGGER)
		debug_dump(MT6989_CHK_PD_NUM, 0);
}

static void check_mm_hwv_irq_sta(void)
{
	u32 irq_sta;

	irq_sta = get_mt6989_reg_value(mm_hwv_ext, HWV_IRQ_STATUS);

	if ((irq_sta & HWV_INT_MTCMOS_TRIGGER) == HWV_INT_MTCMOS_TRIGGER)
		debug_dump(MT6989_CHK_PD_NUM, 0);
}

/*
 * init functions
 */

static struct pdchk_ops pdchk_mt6989_ops = {
	.get_subsys_cg = get_subsys_cg,
	.dump_subsys_reg = dump_subsys_reg,
	.is_in_pd_list = is_in_pd_list,
	.debug_dump = debug_dump,
	.log_dump = log_dump,
	.external_dump = external_dump,
	.get_pd_pwr_status = get_pd_pwr_status,
	.get_off_mtcmos_id = get_off_mtcmos_id,
	.get_notice_mtcmos_id = get_notice_mtcmos_id,
	.is_mtcmos_chk_bug_on = is_mtcmos_chk_bug_on,
	.get_suspend_allow_id = get_suspend_allow_id,
	.trace_power_event = trace_power_event,
	.dump_power_event = dump_power_event,
	.is_suspend_retry_stop = pdchk_is_suspend_retry_stop,
	.check_hwv_irq_sta = check_hwv_irq_sta,
	.check_mm_hwv_irq_sta = check_mm_hwv_irq_sta,
};

static int pd_chk_mt6989_probe(struct platform_device *pdev)
{
	suspend_cnt = 0;

	pdchk_common_init(&pdchk_mt6989_ops);
	pdchk_hwv_irq_init(pdev);

	return 0;
}

static const struct of_device_id of_match_pdchk_mt6989[] = {
	{
		.compatible = "mediatek,mt6989-pdchk",
	}, {
		/* sentinel */
	}
};

static struct platform_driver pd_chk_mt6989_drv = {
	.probe = pd_chk_mt6989_probe,
	.driver = {
		.name = "pd-chk-mt6989",
		.owner = THIS_MODULE,
		.pm = &pdchk_dev_pm_ops,
		.of_match_table = of_match_pdchk_mt6989,
	},
};

/*
 * init functions
 */

static int __init pd_chk_init(void)
{
	return platform_driver_register(&pd_chk_mt6989_drv);
}

static void __exit pd_chk_exit(void)
{
	platform_driver_unregister(&pd_chk_mt6989_drv);
}

subsys_initcall(pd_chk_init);
module_exit(pd_chk_exit);
MODULE_LICENSE("GPL");
