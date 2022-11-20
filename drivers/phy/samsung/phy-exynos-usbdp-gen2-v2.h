/*
 * phy-exynos-usbdp-gen2-v2.h
 *
 *  Created on: 2018. 9. 5.
 *      Author: daeman.ko
 */

#ifndef DRIVER_USB_USBPHY_CAL_PHY_EXYNOS_USBDP_GEN2_V2_H_
#define DRIVER_USB_USBPHY_CAL_PHY_EXYNOS_USBDP_GEN2_V2_H_

extern void phy_exynos_usbdp_g2_v2_tune_each(struct exynos_usbphy_info *info, char *name, u32 val);
extern void phy_exynos_usbdp_g2_v2_tune(struct exynos_usbphy_info *info);
extern void phy_exynos_usbdp_g2_v2_tune_each_late(struct exynos_usbphy_info *info, char *name, u32 val);
extern void phy_exynos_usbdp_g2_v2_tune_late(struct exynos_usbphy_info *info);
extern void phy_exynos_usbdp_g2_v2_set_dtb_mux(struct exynos_usbphy_info *info, int mux_val);
extern int phy_exynos_usbdp_g2_v2_internal_loopback(struct exynos_usbphy_info *info, u32 cmn_rate);
extern void phy_exynos_usbdp_g2_v2_eom_init(struct exynos_usbphy_info *info, u32 cmn_rate);
extern void phy_exynos_usbdp_g2_v2_eom_deinit(struct exynos_usbphy_info *info);
extern void phy_exynos_usbdp_g2_v2_eom_start(struct exynos_usbphy_info *info, u32 ph_sel, u32 def_vref);
extern int phy_exynos_usbdp_g2_v2_eom_get_done_status(struct exynos_usbphy_info *info);
extern u64 phy_exynos_usbdp_g2_v2_eom_get_err_cnt(struct exynos_usbphy_info *info);
extern void phy_exynos_usbdp_g2_v2_eom_stop(struct exynos_usbphy_info *info);
extern void phy_exynos_usbdp_g2_v2_eom(struct exynos_usbphy_info *info, u32 cmn_rate, struct usb_eom_result_s *eom_result);

extern int phy_exynos_usbdp_g2_v2_enable(struct exynos_usbphy_info *info);
extern void phy_exynos_usbdp_g2_v2_disable(struct exynos_usbphy_info *info);

#endif /* DRIVER_USB_USBPHY_CAL_PHY_EXYNOS_USBDP_GEN2_V2_H_ */
