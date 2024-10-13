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

#ifndef __ARCH_ARM_MACH_SPRD_DEVICES_H
#define __ARCH_ARM_MACH_SPRD_DEVICES_H

extern struct platform_device sprd_hwspinlock_device0;
extern struct platform_device sprd_hwspinlock_device1;
extern struct platform_device sprd_serial_device0;
extern struct platform_device sprd_serial_device1;
extern struct platform_device sprd_serial_device2;
extern struct platform_device sprd_serial_device3;
extern struct platform_device sprd_device_rtc;
extern struct platform_device sprd_eic_gpio_device;
extern struct platform_device sprd_nand_device;
extern struct platform_device sprd_lcd_device0;
extern struct platform_device sprd_lcd_device1;
#ifdef CONFIG_PSTORE_RAM
extern struct platform_device sprd_ramoops_device;
#endif
extern struct platform_device sprd_otg_device;
extern struct platform_device sprd_backlight_device;
#if(defined(CONFIG_BACKLIGHT_SPRD_PWM)||defined(CONFIG_BACKLIGHT_SPRD_PWM_MODULE))
extern struct platform_device sprd_pwm_bl_device;
#endif
#if(defined(CONFIG_SPRD_KPLED_2723) || defined(CONFIG_SPRD_KPLED_2723_MODULE))
extern struct platform_device sprd_kpled_2723_device;
#endif
extern struct platform_device sprd_i2c_device0;
extern struct platform_device sprd_i2c_device1;
extern struct platform_device sprd_i2c_device2;
extern struct platform_device sprd_i2c_device3;
#if defined(CONFIG_PIN_POWER_DOMAIN_SWITCH)
extern struct platform_device sprd_pin_switch_device;
#endif
extern struct platform_device sprd_spi0_device;
extern struct platform_device sprd_spi1_device;
extern struct platform_device sprd_spi2_device;
extern struct platform_device sprd_keypad_device;
extern struct platform_device sprd_thm_device;
extern struct platform_device sprd_thm_a_device;
extern struct platform_device sprd_battery_device;
extern struct platform_device sprd_headset_device;
#ifdef CONFIG_CPU_FREQ_GOV_SPRDEMAND
extern struct platform_device sprd_cpu_cooling_device;
#endif

extern struct platform_device sprd_battery_device;
extern struct platform_device sprd_vsp_device;
extern struct platform_device sprd_jpg_device;
#ifdef CONFIG_ION
extern struct platform_device sprd_ion_dev;
#endif
extern struct platform_device sprd_iommu_gsp_device;
extern struct platform_device sprd_iommu_mm_device;
#ifdef CONFIG_MUX_SDIO_OPT1_HAL
extern struct platform_device ipc_sdio_device;
#endif
extern struct platform_device sprd_sdio0_device;
extern struct platform_device sprd_sdio1_device;
extern struct platform_device sprd_sdio2_device;
extern struct platform_device sprd_emmc_device;
extern struct platform_device sprd_dcam_device;
extern struct platform_device sprd_scale_device;
extern struct platform_device sprd_gsp_device;
extern struct platform_device sprd_rotation_device;
extern struct platform_device sprd_sensor_device;
extern struct platform_device sprd_isp_device;
extern struct platform_device sprd_dma_copy_device;
extern struct platform_device sprd_ahb_bm0_device;
extern struct platform_device sprd_ahb_bm1_device;
extern struct platform_device sprd_ahb_bm2_device;
extern struct platform_device sprd_ahb_bm3_device;
extern struct platform_device sprd_ahb_bm4_device;
extern struct platform_device sprd_axi_bm0_device;
extern struct platform_device sprd_axi_bm1_device;
extern struct platform_device sprd_axi_bm2_device;
#ifdef CONFIG_SIPC_TD
extern struct platform_device sprd_spipe_td_device;
extern struct platform_device sprd_slog_td_device;
extern struct platform_device sprd_stty_td_device;
extern struct platform_device sprd_spool_td_device;
extern struct platform_device sprd_cproc_td_device;
extern struct platform_device sprd_seth0_td_device;
extern struct platform_device sprd_seth1_td_device;
extern struct platform_device sprd_seth2_td_device;
extern struct platform_device sprd_saudio_td_device;
#endif
#ifdef CONFIG_SIPC_WCDMA
extern struct platform_device sprd_spipe_wcdma_device;
extern struct platform_device sprd_slog_wcdma_device;
extern struct platform_device sprd_stty_wcdma_device;
extern struct platform_device sprd_spool_wcdma_device;
extern struct platform_device sprd_cproc_wcdma_device;
extern struct platform_device sprd_seth0_wcdma_device;
extern struct platform_device sprd_seth1_wcdma_device;
extern struct platform_device sprd_seth2_wcdma_device;
extern struct platform_device sprd_saudio_wcdma_device;
#endif
#ifdef CONFIG_SIPC_WCN
extern struct platform_device sprd_spipe_wcn_device;
extern struct platform_device sprd_slog_wcn_device;
extern struct platform_device sprd_cproc_wcn_device;
extern struct platform_device sprd_sttybt_td_device;
#endif
extern struct platform_device sprd_saudio_voip_device;
extern struct platform_device sprd_a7_pmu_device;
extern struct platform_device sprd_memnand_system_device;
extern struct platform_device sprd_memnand_userdata_device;
extern struct platform_device sprd_memnand_cache_device;

extern struct platform_device sprd_audio_vbc_r2p0_sprd_codec_v3_device;
extern struct platform_device sprd_audio_vbc_r2p0_sprd_codec_device;
extern struct platform_device sprd_audio_i2s_null_codec_device;
extern struct platform_device sprd_audio_platform_pcm_device;
extern struct platform_device sprd_audio_vaudio_device;
extern struct platform_device sprd_audio_vbc_r2p0_device;
extern struct platform_device sprd_audio_i2s0_device;
extern struct platform_device sprd_audio_i2s1_device;
extern struct platform_device sprd_audio_i2s2_device;
extern struct platform_device sprd_audio_i2s3_device;
extern struct platform_device sprd_audio_sprd_codec_v3_device;
extern struct platform_device sprd_audio_sprd_codec_device;
extern struct platform_device sprd_audio_null_codec_device;

#ifdef CONFIG_SPRD_VETH
#ifdef CONFIG_MUX_SPI_HAL
extern struct platform_device sprd_veth_spi0_device;
extern struct platform_device sprd_veth_spi1_device;
extern struct platform_device sprd_veth_spi2_device;
extern struct platform_device sprd_veth_spi3_device;
extern struct platform_device sprd_veth_spi4_device;
#endif
#ifdef CONFIG_MUX_SDIO_OPT1_HAL
extern struct platform_device sprd_veth_sdio0_device;
extern struct platform_device sprd_veth_sdio1_device;
extern struct platform_device sprd_veth_sdio2_device;
extern struct platform_device sprd_veth_sdio3_device;
extern struct platform_device sprd_veth_sdio4_device;
#endif
#ifdef CONFIG_MODEM_INTF
extern struct platform_device modem_interface_device;
#endif
#endif

extern struct platform_device modem_interface_device;
#endif
#ifdef CONFIG_TS0710_MUX_ENABLE
extern struct platform_device sprd_mux_spi_device;
extern struct platform_device sprd_mux_sdio_device;
#endif
