/*
 * phy-exynos-usbdp-gen2-v3.h
 *
 *  Created on: 2019. 5. 10.
 *      Author: wooseok.oh
 */

#ifndef DRIVER_USB_USBPHY_CAL_PHY_EXYNOS_USBDP_GEN2_V3_H_
#define DRIVER_USB_USBPHY_CAL_PHY_EXYNOS_USBDP_GEN2_V3_H_

extern void phy_exynos_usbdp_g2_v3_tune_each(struct exynos_usbphy_info *info, char *name, u32 val);
extern void phy_exynos_usbdp_g2_v3_tune(struct exynos_usbphy_info *info);
extern void phy_exynos_usbdp_g2_v3_set_dtb_mux(struct exynos_usbphy_info *info, int mux_val);

extern int phy_exynos_usbdp_g2_v3_internal_loopback(struct exynos_usbphy_info *info, u32 cmn_rate);
extern void phy_exynos_usbdp_g2_v3_eom_init(struct exynos_usbphy_info *info, u32 cmn_rate);
extern void phy_exynos_usbdp_g2_v3_eom_deinit(struct exynos_usbphy_info *info);
extern void phy_exynos_usbdp_g2_v3_eom_start(struct exynos_usbphy_info *info, u32 ph_sel, u32 def_vref);
extern int phy_exynos_usbdp_g2_v3_eom_get_done_status(struct exynos_usbphy_info *info);
extern u64 phy_exynos_usbdp_g2_v3_eom_get_err_cnt(struct exynos_usbphy_info *info);
extern void phy_exynos_usbdp_g2_v3_eom_stop(struct exynos_usbphy_info *info);
extern void phy_exynos_usbdp_g2_v3_eom(struct exynos_usbphy_info *info, u32 cmn_rate, struct usb_eom_result_s *eom_result);

extern int phy_exynos_usbdp_g2_v3_enable(struct exynos_usbphy_info *info);
extern void phy_exynos_usbdp_g2_v3_disable(struct exynos_usbphy_info *info);

#endif /* DRIVER_USB_USBPHY_CAL_PHY_EXYNOS_USBDP_GEN2_V3_H_ */
