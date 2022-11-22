/*
 * s2mu107-muic-hv.h - MUIC for the Samsung s2mu106
 *
 * Copyright (C) 2019 Samsung Electrnoics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#ifndef __S2MU107_MUIC_HV_H__
#define __S2MU107_MUIC_HV_H__

#define MUIC_HV_DEV_NAME		"muic-s2mu107-hv"

/* S2MU107 AFC_INT register (0x0) */
#define AFC_INT_MRxRdy_SHIFT			7
#define AFC_INT_MRxPerr_SHIFT			6
#define AFC_INT_MRxTrf_SHIFT			5
#define AFC_INT_MRxBufOw_SHIFT			4
#define AFC_INT_MPNack_SHIFT			3
#define AFC_INT_DNRes_SHIFT			2
#define AFC_INT_VDNMon_SHIFT			1

#define AFC_INT_MRxRdy_MASK			(0x1 << AFC_INT_MRxRdy_SHIFT)
#define AFC_INT_MRxPerr_MASK			(0x1 << AFC_INT_MRxPerr_SHIFT)
#define AFC_INT_MRxTrf_MASK			(0x1 << AFC_INT_MRxTrf_SHIFT)
#define AFC_INT_MRxBufOw_MASK			(0x1 << AFC_INT_MRxBufOw_SHIFT)
#define AFC_INT_MPNack_MASK			(0x1 << AFC_INT_MPNack_SHIFT)
#define AFC_INT_DNRes_MASK			(0x1 << AFC_INT_DNRes_SHIFT)
#define AFC_INT_VDNMon_MASK			(0x1 << AFC_INT_VDNMon_SHIFT)

/* S2MU107 AFC_INT_MASK register (0x08) */
#define AFC_INT_MASK_MRxRdy_Im_SHIFT		7
#define AFC_INT_MASK_MRxPerr_Im_SHIFT		6
#define AFC_INT_MASK_MRxTrf_Im_SHIFT		5
#define AFC_INT_MASK_MRxBufOw_Im_SHIFT		4
#define AFC_INT_MASK_MPNack_Im_SHIFT		3
#define AFC_INT_MASK_DNRes_Im_SHIFT		2
#define AFC_INT_MASK_VDNMon_Im_SHIFT		1

#define AFC_INT_MASK_MRxRdy_Im_MASK		(0x1 << AFC_INT_MASK_MRxRdy_Im_SHIFT)
#define AFC_INT_MASK_MRxPerr_Im_MASK		(0x1 << AFC_INT_MASK_MRxPerr_Im_SHIFT)
#define AFC_INT_MASK_MRxTrf_Im_MASK		(0x1 << AFC_INT_MASK_MRxTrf_Im_SHIFT)
#define AFC_INT_MASK_MRxBufOw_Im_MASK		(0x1 << AFC_INT_MASK_MRxBufOw_Im_SHIFT)
#define AFC_INT_MASK_MPNack_Im_MASK		(0x1 << AFC_INT_MASK_MPNack_Im_SHIFT)
#define AFC_INT_MASK_DNRes_Im_MASK		(0x1 << AFC_INT_MASK_DNRes_Im_SHIFT)
#define AFC_INT_MASK_VDNMon_Im_MASK		(0x1 << AFC_INT_MASK_VDNMon_Im_SHIFT)

/* S2MU107 AFC_STATUS register (0x10) */
#define AFC_STATUS_MPNack_3OR_LATCH_SHIFT	6
#define AFC_STATUS_DNRes_SHIFT			5
#define AFC_STATUS_VDNMon_SHIFT			4

#define AFC_STATUS_MPNack_3OR_LATCH_MASK	(0x1 << AFC_STATUS_MPNack_3OR_LATCH_SHIFT)
#define AFC_STATUS_DNRes_MASK			(0x1 << AFC_STATUS_DNRes_SHIFT)
#define AFC_STATUS_VDNMon_MASK			(0x1 << AFC_STATUS_VDNMon_SHIFT)

/* S2MU107 AFC_CONTROL1 register (0x2D) */
#define AFC_CTRL1_DPDNVDEN_SHIFT	0
#define AFC_CTRL1_DNVD_SHIFT		1
#define AFC_CTRL1_DPVD_SHIFT		3
#define AFC_CTRL1_CTRLIDMON_SHIFT	6
#define AFC_CTRL1_AFCEN_SHIFT		7

#define AFC_CTRL1_DPDNVDEN_MASK		(0x1 << AFC_CTRL1_DPDNVDEN_SHIFT)
#define AFC_CTRL1_DNVD_MASK		(0x3 << AFC_CTRL1_DNVD_SHIFT)
#define AFC_CTRL1_DPVD_MASK		(0x3 << AFC_CTRL1_DPVD_SHIFT)
#define AFC_CTRL1_CTRLIDMON_MASK	(0x1 << AFC_CTRL1_CTRLIDMON_SHIFT)
#define AFC_CTRL1_AFCEN_MASK		(0x1 << AFC_CTRL1_AFCEN_SHIFT)

#define DPDN_HIZ			(0x0)
#define DPDN_GND			(0x1)
#define DPDN_0p6V			(0x2)
#define DPDN_3p3V			(0x3)
#define DP_HIZ_MASK			(DPDN_HIZ << AFC_CTRL1_DPVD_SHIFT)
#define DP_GND_MASK			(DPDN_GND << AFC_CTRL1_DPVD_SHIFT)
#define DP_0p6V_MASK			(DPDN_0p6V << AFC_CTRL1_DPVD_SHIFT)
#define DP_3p3V_MASK			(DPDN_3p3V << AFC_CTRL1_DPVD_SHIFT)
#define DN_HIZ_MASK			(DPDN_HIZ << AFC_CTRL1_DNVD_SHIFT)
#define DN_GND_MASK			(DPDN_GND << AFC_CTRL1_DNVD_SHIFT)
#define DN_0p6V_MASK			(DPDN_0p6V << AFC_CTRL1_DNVD_SHIFT)
#define DN_3p3V_MASK			(DPDN_3p3V << AFC_CTRL1_DNVD_SHIFT)

/* S2MU107 AFC_CONTROL2 register (0x2E) */
#define AFC_CTRL2_MPING_RST_SHIFT	0
#define AFC_CTRL2_DP06EN_SHIFT		1
#define AFC_CTRL2_DNRESEN_SHIFT		2
#define AFC_CTRL2_MTXEN_SHIFT		3
#define AFC_CTRL2_MPING_SHIFT		4
#define AFC_CTRL2_QCCMODE_DM_RST_SHIFT	5
#define AFC_CTRL2_QCCMODE_DP_RST_SHIFT	6
#define AFC_CTRL2_RSTDM100UI_SHIFT	7

#define AFC_CTRL2_MPING_RST_MASK	(0x1 << AFC_CTRL2_MPING_RST_SHIFT)
#define AFC_CTRL2_DP06EN_MASK		(0x1 << AFC_CTRL2_DP06EN_SHIFT)
#define AFC_CTRL2_DNRESEN_MASK		(0x1 << AFC_CTRL2_DNRESEN_SHIFT)
#define AFC_CTRL2_MTXEN_MASK		(0x1 << AFC_CTRL2_MTXEN_SHIFT)
#define AFC_CTRL2_MPING_MASK		(0x1 << AFC_CTRL2_MPING_SHIFT)
#define AFC_CTRL2_QCCMODE_DM_RST_MASK	(0x1 << AFC_CTRL2_QCCMODE_DM_RST_SHIFT)
#define AFC_CTRL2_QCCMODE_DP_RST_MASK	(0x1 << AFC_CTRL2_QCCMODE_DP_RST_SHIFT)
#define AFC_CTRL2_RSTDM100UI_MASK	(0x1 << AFC_CTRL2_RSTDM100UI_SHIFT)

/* S2MU107 TX_BYTE DATA (0x2F) */
#define AFCTXBYTE_VOL_SHIFT 4
#define AFCTXBYTE_VOL_MASK (0xf << AFCTXBYTE_VOL_SHIFT)
#define AFCTXBYTE_CUR_SHIFT 0
#define AFCTXBYTE_CUR_MASK (0xf << AFCTXBYTE_CUR_SHIFT)

#define AFCTXBYTE_5V 0x0
#define AFCTXBYTE_6V 0x1
#define AFCTXBYTE_7V 0x2
#define AFCTXBYTE_8V 0x3
#define AFCTXBYTE_9V 0x4
#define AFCTXBYTE_10V 0x5
#define AFCTXBYTE_11V 0x6
#define AFCTXBYTE_12V 0x7
#define AFCTXBYTE_13V 0x8
#define AFCTXBYTE_14V 0x9
#define AFCTXBYTE_15V 0xA
#define AFCTXBYTE_16V 0xB
#define AFCTXBYTE_17V 0xC
#define AFCTXBYTE_18V 0xD
#define AFCTXBYTE_19V 0xE
#define AFCTXBYTE_20V 0xF

#define AFCTXBYTE_0p75A 0x0
#define AFCTXBYTE_0p90A 0x1
#define AFCTXBYTE_1p05A 0x2
#define AFCTXBYTE_1p20A 0x3
#define AFCTXBYTE_1p35A 0x4
#define AFCTXBYTE_1p50A 0x5
#define AFCTXBYTE_1p65A 0x6
#define AFCTXBYTE_1p80A 0x7
#define AFCTXBYTE_1p95A 0x8
#define AFCTXBYTE_2p10A 0x9
#define AFCTXBYTE_2p25A 0xA
#define AFCTXBYTE_2p40A 0xB
#define AFCTXBYTE_2p55A 0xC
#define AFCTXBYTE_2p70A 0xD
#define AFCTXBYTE_2p85A 0xE
#define AFCTXBYTE_3p00A 0xF

/* S2MU107 AFC_OTP6 register (0x6A) */
#define AFC_OTP6_CTRL_IDM_ON_REG_SEL_SHIFT 	6

#define AFC_OTP6_CTRL_IDM_ON_REG_SEL_MASK	(0x1 << AFC_OTP6_CTRL_IDM_ON_REG_SEL_SHIFT)

#define IS_VCHGIN_9V(x) ((8000 <= x) && (x <= 10300))
#define IS_VCHGIN_5V(x) ((4000 <= x) && (x <= 6000))

#define AFC_MRXRDY_CNT_LIMIT (3)
#define AFC_MPING_RETRY_CNT_LIMIT (20)
#define AFC_QC_RETRY_CNT_LIMIT (3)
#define AFC_QC_RETRY_WAIT_CNT_LIMIT (3)

typedef enum {
	MU107_IRQ_VDNMON = 1,
	MU107_IRQ_DNRES,
	MU107_IRQ_MPNACK,
	MU107_IRQ_MRXBUFOW,
	MU107_IRQ_MRXTRF,
	MU107_IRQ_MRXPERR,
	MU107_IRQ_MRXRDY = 7,
} afc_int_t;

typedef enum {
	MU107_NOT_MASK = 0,
	MU107_MASK = 1,
} int_mask_t;

typedef enum {
	MU107_QC_PROTOCOL,
	MU107_AFC_PROTOCOL,
} protocol_sw_t;

typedef enum {
	QC_UNKHOWN,
	QC_5V,
	QC_9V,
	QC_12V,
} qc_2p0_type_t;

typedef enum {
	VDNMON_LOW		= 0x00,
	VDNMON_HIGH		= (0x1 << AFC_STATUS_VDNMon_SHIFT),

	VDNMON_DONTCARE		= 0xff,
} vdnmon_t;

/* MUIC afc irq type */
typedef enum {
	MUIC_AFC_IRQ_VDNMON = 0,
	MUIC_AFC_IRQ_MRXRDY,
	MUIC_AFC_IRQ_VBADC,
	MUIC_AFC_IRQ_MPNACK,
	MUIC_AFC_IRQ_DONTCARE = 0xff,
} muic_afc_irq_t;

typedef enum tx_data{
    MUIC_HV_5V = 0,
    MUIC_HV_9V,
} muic_afc_txdata_t;

struct s2mu107_muic_data;
extern int s2mu107_hv_muic_init(struct s2mu107_muic_data *muic_data);
extern void s2mu107_hv_muic_remove(struct s2mu107_muic_data *muic_data);
extern muic_attached_dev_t hv_muic_check_id_err(struct s2mu107_muic_data *muic_data,
		muic_attached_dev_t new_dev);
#ifdef CONFIG_HV_MUIC_VOLTAGE_CTRL
extern int s2mu107_muic_afc_set_voltage(int vol);
extern int s2mu107_muic_afc_get_voltage(void);
#endif
#endif /* __S2MU107_MUIC_HV_H__ */
