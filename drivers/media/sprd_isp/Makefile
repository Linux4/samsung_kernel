include drivers/media/sprd_dcam/common/Makefile.inc
#ccflags-y += -Idrivers/media/sprd_dcam/common

tshark2_isp_like:=0

ifeq ($(CONFIG_MACH_SP9830IEA_5M_H100),y)
tshark2_isp_like=1
endif

ifeq ($(CONFIG_MACH_SP9830IEB_5M_H100),y)
tshark2_isp_like=1
endif

ifeq ($(CONFIG_MACH_SP9830IEC_4M_H100),y)
tshark2_isp_like=1
endif

ifeq ($(CONFIG_MACH_SP9830IEE_4M_H100),y)
tshark2_isp_like=1
endif

ifeq ($(CONFIG_MACH_SP7720EA),y)
pike_isp_like=1
endif

ifeq ($(CONFIG_MACH_SP7720EB),y)
pike_isp_like=1
endif

ifeq ($(CONFIG_MACH_PIKEB_J1MINI_3G),y)
pike_isp_like=1
endif

ifeq ($(CONFIG_MACH_J1MINI3G),y)
pike_isp_like=1
endif

ifeq ($(CONFIG_MACH_J1X3G),y)
pike_isp_like=1
endif

ifeq ($(CONFIG_MACH_SP7720EB_PRIME),y)
pike_isp_like=1
endif

ifeq ($(CONFIG_ARCH_SCX30G2),y)
tshark2_isp_like=1
endif

ifeq ($(CONFIG_ARCH_SCX35LT8),y)
tshark2_isp_like=1
endif

#SharkLS
ifeq ($(CONFIG_MACH_SP9830I),y)
tshark2_isp_like=1
endif

obj-y += sprd_isp.o
ccflags-y += -Idrivers/media/sprd_dcam/common


ifeq ($(CONFIG_ARCH_SCX35),y)

ifeq ($(CONFIG_ARCH_SCX15),y)
sprd_isp-objs := isp1.0/isp_drv_null.o

else ifeq ($(strip $(tshark2_isp_like)),1)
ccflags-y += -Idrivers/media/sprd_isp/isp2.0/tshark2/inc

sprd_isp-objs := isp_drv.o \
		isp2.0/tshark2/src/isp_cfg_param.o \
		isp2.0/tshark2/src/isp_k_capability.o \
		isp2.0/tshark2/src/isp_k_isr.o \
		isp2.0/tshark2/src/isp_k_pgg.o \
		isp2.0/tshark2/src/isp_k_blc.o \
		isp2.0/tshark2/src/isp_k_rgb_gain.o \
		isp2.0/tshark2/src/isp_k_pwd.o \
		isp2.0/tshark2/src/isp_k_nlc.o \
		isp2.0/tshark2/src/isp_k_2d_lsc.o \
		isp2.0/tshark2/src/isp_k_1d_lsc.o \
		isp2.0/tshark2/src/isp_k_binning4awb.o \
		isp2.0/tshark2/src/isp_k_awb.o \
		isp2.0/tshark2/src/isp_k_raw_aem.o \
		isp2.0/tshark2/src/isp_k_bpc.o \
		isp2.0/tshark2/src/isp_k_bdn.o \
		isp2.0/tshark2/src/isp_k_grgb.o \
		isp2.0/tshark2/src/isp_k_rgb_gain2.o \
		isp2.0/tshark2/src/isp_k_nlm.o \
		isp2.0/tshark2/src/isp_k_cfa.o \
		isp2.0/tshark2/src/isp_k_cmc10.o \
		isp2.0/tshark2/src/isp_k_gamma.o \
		isp2.0/tshark2/src/isp_k_cmc8.o \
		isp2.0/tshark2/src/isp_k_ct.o \
		isp2.0/tshark2/src/isp_k_cce.o \
		isp2.0/tshark2/src/isp_k_hsv.o \
		isp2.0/tshark2/src/isp_k_csc.o \
		isp2.0/tshark2/src/isp_k_pre_cdn_rgb.o \
		isp2.0/tshark2/src/isp_k_pstrz.o \
		isp2.0/tshark2/src/isp_k_raw_afm.o \
		isp2.0/tshark2/src/isp_k_yiq_aem.o \
		isp2.0/tshark2/src/isp_k_anti_flicker.o \
		isp2.0/tshark2/src/isp_k_yiq_afm.o \
		isp2.0/tshark2/src/isp_k_pre_cdn.o \
		isp2.0/tshark2/src/isp_k_prefilter.o \
		isp2.0/tshark2/src/isp_k_brightness.o \
		isp2.0/tshark2/src/isp_k_contrast.o \
		isp2.0/tshark2/src/isp_k_hist.o \
		isp2.0/tshark2/src/isp_k_hist2.o \
		isp2.0/tshark2/src/isp_k_aca.o \
		isp2.0/tshark2/src/isp_k_cdn.o \
		isp2.0/tshark2/src/isp_k_edge.o \
		isp2.0/tshark2/src/isp_k_css.o \
		isp2.0/tshark2/src/isp_k_csa.o \
		isp2.0/tshark2/src/isp_k_hua.o \
		isp2.0/tshark2/src/isp_k_post_cdn.o \
		isp2.0/tshark2/src/isp_k_emboss.o \
		isp2.0/tshark2/src/isp_k_y_gamma.o \
		isp2.0/tshark2/src/isp_k_y_delay.o \
		isp2.0/tshark2/src/isp_k_iircnr.o \
		isp2.0/tshark2/src/isp_k_fetch.o \
		isp2.0/tshark2/src/isp_k_store.o \
		isp2.0/tshark2/src/isp_k_dispatch.o \
		isp2.0/tshark2/src/isp_k_arbiter.o \
		isp2.0/tshark2/src/isp_k_raw_sizer.o \
		isp2.0/tshark2/src/isp_k_common.o

ifeq ($(CONFIG_64BIT),y)
sprd_isp-objs += compat_isp_drv.o
endif

else ifeq ($(strip $(pike_isp_like)),1)
ccflags-y += -Idrivers/media/sprd_isp/isp2.0/pike/inc

sprd_isp-objs := isp_drv.o \
		isp2.0/pike/src/isp_cfg_param.o \
		isp2.0/pike/src/isp_k_capability.o \
		isp2.0/pike/src/isp_k_isr.o \
		isp2.0/pike/src/isp_k_pgg.o \
		isp2.0/pike/src/isp_k_blc.o \
		isp2.0/pike/src/isp_k_rgb_gain.o \
		isp2.0/pike/src/isp_k_pwd.o \
		isp2.0/pike/src/isp_k_nlc.o \
		isp2.0/pike/src/isp_k_2d_lsc.o \
		isp2.0/pike/src/isp_k_1d_lsc.o \
		isp2.0/pike/src/isp_k_binning4awb.o \
		isp2.0/pike/src/isp_k_awb.o \
		isp2.0/pike/src/isp_k_raw_aem.o \
		isp2.0/pike/src/isp_k_bpc.o \
		isp2.0/pike/src/isp_k_bdn.o \
		isp2.0/pike/src/isp_k_grgb.o \
		isp2.0/pike/src/isp_k_rgb_gain2.o \
		isp2.0/pike/src/isp_k_nlm.o \
		isp2.0/pike/src/isp_k_cfa.o \
		isp2.0/pike/src/isp_k_cmc10.o \
		isp2.0/pike/src/isp_k_gamma.o \
		isp2.0/pike/src/isp_k_cmc8.o \
		isp2.0/pike/src/isp_k_ct.o \
		isp2.0/pike/src/isp_k_cce.o \
		isp2.0/pike/src/isp_k_hsv.o \
		isp2.0/pike/src/isp_k_csc.o \
		isp2.0/pike/src/isp_k_pre_cdn_rgb.o \
		isp2.0/pike/src/isp_k_pstrz.o \
		isp2.0/pike/src/isp_k_raw_afm.o \
		isp2.0/pike/src/isp_k_yiq_aem.o \
		isp2.0/pike/src/isp_k_anti_flicker.o \
		isp2.0/pike/src/isp_k_yiq_afm.o \
		isp2.0/pike/src/isp_k_pre_cdn.o \
		isp2.0/pike/src/isp_k_prefilter.o \
		isp2.0/pike/src/isp_k_brightness.o \
		isp2.0/pike/src/isp_k_contrast.o \
		isp2.0/pike/src/isp_k_hist.o \
		isp2.0/pike/src/isp_k_hist2.o \
		isp2.0/pike/src/isp_k_aca.o \
		isp2.0/pike/src/isp_k_cdn.o \
		isp2.0/pike/src/isp_k_edge.o \
		isp2.0/pike/src/isp_k_css.o \
		isp2.0/pike/src/isp_k_csa.o \
		isp2.0/pike/src/isp_k_hua.o \
		isp2.0/pike/src/isp_k_post_cdn.o \
		isp2.0/pike/src/isp_k_emboss.o \
		isp2.0/pike/src/isp_k_y_gamma.o \
		isp2.0/pike/src/isp_k_y_delay.o \
		isp2.0/pike/src/isp_k_fetch.o \
		isp2.0/pike/src/isp_k_store.o \
		isp2.0/pike/src/isp_k_dispatch.o \
		isp2.0/pike/src/isp_k_arbiter.o \
		isp2.0/pike/src/isp_k_hdr.o \
		isp2.0/pike/src/isp_k_rgb2y.o \
		isp2.0/pike/src/isp_k_uv_prefilter.o \
		isp2.0/pike/src/isp_k_yuv_nlm.o \
		isp2.0/pike/src/isp_k_common.o

else
ccflags-y += -Idrivers/media/sprd_isp/isp1.0
sprd_isp-objs := isp1.0/isp_drv_kernel.o
ifeq ($(CONFIG_64BIT),y)
sprd_isp-objs += isp1.0/compat_isp_drv_kernel.o
endif
endif

endif

ifeq ($(CONFIG_ARCH_SC8825),y)
ccflags-y += -Idrivers/media/sprd_isp/isp1.0
sprd_isp-objs := isp1.0/isp_drv_kernel.o
endif

obj-y += sprd_isp.o


