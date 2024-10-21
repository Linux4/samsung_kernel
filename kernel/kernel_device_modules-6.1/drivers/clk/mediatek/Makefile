# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek

obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MEDIATEK) += clk-common.o
clk-common-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MEDIATEK) += clk-mtk.o clk-pll.o clk-gate.o clk-apmixed.o clk-cpumux.o reset.o clk-mux.o clkchk.o clk-fmeter.o mtk-pd-chk.o clkdbg.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MEDIATEK) += clk-disable-unused.o

obj-$(CONFIG_COMMON_CLK_MT6853) += clk-mt6853.o
obj-$(CONFIG_COMMON_CLK_MT6853_APU0) += clk-mt6853-apu0.o
obj-$(CONFIG_COMMON_CLK_MT6853_APU1) += clk-mt6853-apu1.o
obj-$(CONFIG_COMMON_CLK_MT6853_APU_VCORE) += clk-mt6853-apuv.o
obj-$(CONFIG_COMMON_CLK_MT6853_APU_CONN) += clk-mt6853-apuc.o
obj-$(CONFIG_COMMON_CLK_MT6853_AUDIOSYS) += clk-mt6853-audsys.o
obj-$(CONFIG_COMMON_CLK_MT6853_CAMSYS_MAIN) += clk-mt6853-cam_m.o
obj-$(CONFIG_COMMON_CLK_MT6853_CAMSYS_RAWA) += clk-mt6853-cam_ra.o
obj-$(CONFIG_COMMON_CLK_MT6853_CAMSYS_RAWB) += clk-mt6853-cam_rb.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMGSYS1) += clk-mt6853-imgsys1.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMGSYS2) += clk-mt6853-imgsys2.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMP_IIC_WRAP_C) += clk-mt6853-impc.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMP_IIC_WRAP_E) += clk-mt6853-impe.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMP_IIC_WRAP_N) += clk-mt6853-impn.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMP_IIC_WRAP_S) += clk-mt6853-imps.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMP_IIC_WRAP_W) += clk-mt6853-impw.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMP_IIC_WRAP_WS) += clk-mt6853-impws.o
obj-$(CONFIG_COMMON_CLK_MT6853_IPESYS) += clk-mt6853-ipe.o
obj-$(CONFIG_COMMON_CLK_MT6853_MDPSYS_CONFIG) += clk-mt6853-mdp.o
obj-$(CONFIG_COMMON_CLK_MT6853_MFGCFG) += clk-mt6853-mfg.o
obj-$(CONFIG_COMMON_CLK_MT6853_MMSYS_CONFIG) += clk-mt6853-mm.o
obj-$(CONFIG_COMMON_CLK_MT6853_SCP_ADSP) += clk-mt6853-scp_par.o
obj-$(CONFIG_COMMON_CLK_MT6853_VDEC_GCON) += clk-mt6853-vdec.o
obj-$(CONFIG_COMMON_CLK_MT6853_VENC_GCON) += clk-mt6853-venc.o
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6878.c),)
obj-$(CONFIG_COMMON_CLK_MT6878) += clk-mt6878.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6878_ADSP) += clk-mt6878-adsp.o
obj-$(CONFIG_COMMON_CLK_MT6878_CAM) += clk-mt6878-cam.o
obj-$(CONFIG_COMMON_CLK_MT6878_CCU) += clk-mt6878-ccu.o
obj-$(CONFIG_COMMON_CLK_MT6878_IMG) += clk-mt6878-img.o
obj-$(CONFIG_COMMON_CLK_MT6878_MMSYS) += clk-mt6878-mmsys.o
obj-$(CONFIG_COMMON_CLK_MT6878_PERI) += clk-mt6878-peri.o
obj-$(CONFIG_COMMON_CLK_MT6878_BUS) += clk-mt6878-bus.o
obj-$(CONFIG_COMMON_CLK_MT6878_MDPSYS) += clk-mt6878-mdpsys.o
obj-$(CONFIG_COMMON_CLK_MT6878_VLP) += clk-mt6878-vlp.o
obj-$(CONFIG_COMMON_CLK_MT6878_VCODEC) += clk-mt6878-vcodec.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6897.c),)
obj-$(CONFIG_COMMON_CLK_MT6897) += clk-mt6897.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6897_ADSP) += clk-mt6897-adsp.o
obj-$(CONFIG_COMMON_CLK_MT6897_CAM) += clk-mt6897-cam.o
obj-$(CONFIG_COMMON_CLK_MT6897_CCU) += clk-mt6897-ccu.o
obj-$(CONFIG_COMMON_CLK_MT6897_IMG) += clk-mt6897-img.o
obj-$(CONFIG_COMMON_CLK_MT6897_MMSYS) += clk-mt6897-mmsys.o
obj-$(CONFIG_COMMON_CLK_MT6897_PERI) += clk-mt6897-peri.o
obj-$(CONFIG_COMMON_CLK_MT6897_BUS) += clk-mt6897-bus.o
obj-$(CONFIG_COMMON_CLK_MT6897_MDPSYS) += clk-mt6897-mdpsys.o
obj-$(CONFIG_COMMON_CLK_MT6897_VLP) += clk-mt6897-vlp.o
obj-$(CONFIG_COMMON_CLK_MT6897_VCODEC) += clk-mt6897-vcodec.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6886.c),)
obj-$(CONFIG_COMMON_CLK_MT6886) += clk-mt6886.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6886_ADSP) += clk-mt6886-adsp.o
obj-$(CONFIG_COMMON_CLK_MT6886_CAM) += clk-mt6886-cam.o
obj-$(CONFIG_COMMON_CLK_MT6886_CCU) += clk-mt6886-ccu.o
obj-$(CONFIG_COMMON_CLK_MT6886_IMG) += clk-mt6886-img.o
obj-$(CONFIG_COMMON_CLK_MT6886_MMSYS) += clk-mt6886-mmsys.o
obj-$(CONFIG_COMMON_CLK_MT6886_PERI) += clk-mt6886-peri.o
obj-$(CONFIG_COMMON_CLK_MT6886_BUS) += clk-mt6886-bus.o
obj-$(CONFIG_COMMON_CLK_MT6886_MDPSYS) += clk-mt6886-mdpsys.o
obj-$(CONFIG_COMMON_CLK_MT6886_SCP) += clk-mt6886-scp.o
obj-$(CONFIG_COMMON_CLK_MT6886_VCODEC) += clk-mt6886-vcodec.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6983.c),)
obj-$(CONFIG_COMMON_CLK_MT6983) += clk-mt6983.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6983_ADSP) += clk-mt6983-adsp_grp.o
obj-$(CONFIG_COMMON_CLK_MT6983_CCU) += clk-mt6983-ccu_main.o
obj-$(CONFIG_COMMON_CLK_MT6983_IMG) += clk-mt6983-img.o
obj-$(CONFIG_COMMON_CLK_MT6983_IMP_IIC_WRAP) += clk-mt6983-imp_iic_wrap.o
obj-$(CONFIG_COMMON_CLK_MT6983_VCODEC) += clk-mt6983-vcodec.o
obj-$(CONFIG_COMMON_CLK_MT6983_MDPSYS) += clk-mt6983-mdp_grp.o
obj-$(CONFIG_COMMON_CLK_MT6983_MFG) += clk-mt6983-mfg_top_config.o
obj-$(CONFIG_COMMON_CLK_MT6983_MMSYS) += clk-mt6983-mm.o
obj-$(CONFIG_COMMON_CLK_MT6983_CAM) += clk-mt6983-cam.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6985.c),)
obj-$(CONFIG_COMMON_CLK_MT6985) += clk-mt6985.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6985_ADSP) += clk-mt6985-adsp.o
obj-$(CONFIG_COMMON_CLK_MT6985_CAM) += clk-mt6985-cam.o
obj-$(CONFIG_COMMON_CLK_MT6985_CCU) += clk-mt6985-ccu.o
obj-$(CONFIG_COMMON_CLK_MT6985_IMG) += clk-mt6985-img.o
obj-$(CONFIG_COMMON_CLK_MT6985_MMSYS) += clk-mt6985-mmsys.o
obj-$(CONFIG_COMMON_CLK_MT6985_PERI) += clk-mt6985-peri.o
obj-$(CONFIG_COMMON_CLK_MT6985_BUS) += clk-mt6985-bus.o
obj-$(CONFIG_COMMON_CLK_MT6985_MDPSYS) += clk-mt6985-mdpsys.o
obj-$(CONFIG_COMMON_CLK_MT6985_VLP) += clk-mt6985-vlp.o
obj-$(CONFIG_COMMON_CLK_MT6985_VCODEC) += clk-mt6985-vcodec.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6989.c),)
obj-$(CONFIG_COMMON_CLK_MT6989) += clk-mt6989.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6989_ADSP) += clk-mt6989-adsp.o
obj-$(CONFIG_COMMON_CLK_MT6989_CAM) += clk-mt6989-cam.o
obj-$(CONFIG_COMMON_CLK_MT6989_IMG) += clk-mt6989-img.o
obj-$(CONFIG_COMMON_CLK_MT6989_MMSYS) += clk-mt6989-mmsys.o
obj-$(CONFIG_COMMON_CLK_MT6989_PERI) += clk-mt6989-peri.o
obj-$(CONFIG_COMMON_CLK_MT6989_BUS) += clk-mt6989-bus.o
obj-$(CONFIG_COMMON_CLK_MT6989_MDPSYS) += clk-mt6989-mdpsys.o
obj-$(CONFIG_COMMON_CLK_MT6989_VLP) += clk-mt6989-vlp.o
obj-$(CONFIG_COMMON_CLK_MT6989_VCODEC) += clk-mt6989-vcodec.o
endif
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192) += clk-mt8192.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT8192_APU0) += clk-mt8192-apu0.o
obj-$(CONFIG_COMMON_CLK_MT8192_APU1) += clk-mt8192-apu1.o
obj-$(CONFIG_COMMON_CLK_MT8192_APU_VCORE) += clk-mt8192-apu_vcore.o
obj-$(CONFIG_COMMON_CLK_MT8192_APU_CONN) += clk-mt8192-apu_conn.o
obj-$(CONFIG_COMMON_CLK_MT8192_APU_MDLA0) += clk-mt8192-apu_mdla0.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_AUDSYS) += clk-mt8192-aud.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_CAMSYS) += clk-mt8192-cam.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_IMGSYS) += clk-mt8192-img.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_IMP_IIC_WRAP) += clk-mt8192-imp_iic_wrap.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_IPESYS) += clk-mt8192-ipe.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_MDPSYS) += clk-mt8192-mdp.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_MFGCFG) += clk-mt8192-mfg.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_MMSYS) += clk-mt8192-mm.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_MSDC) += clk-mt8192-msdc.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_SCP_ADSP) += clk-mt8192-scp_adsp.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_VDECSYS) += clk-mt8192-vdec.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_VENCSYS) += clk-mt8192-venc.o
ifneq ($(wildcard $(srctree)/$(src)/clk-mt8188.c),)
obj-$(CONFIG_COMMON_CLK_MT8188) += clk-mt8188.o  clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT8188_AUDSYS_SRC) += clk-mt8188-audio_src.o
obj-$(CONFIG_COMMON_CLK_MT8188_CAMSYS) += clk-mt8188-cam.o
obj-$(CONFIG_COMMON_CLK_MT8188_IMGSYS) += clk-mt8188-img.o
obj-$(CONFIG_COMMON_CLK_MT8188_IMP_IIC_WRAP) += clk-mt8188-imp_iic_wrap.o
obj-$(CONFIG_COMMON_CLK_MT8188_IPESYS) += clk-mt8188-ipe.o
obj-$(CONFIG_COMMON_CLK_MT8188_VDECSYS) += clk-mt8188-vdec.o
obj-$(CONFIG_COMMON_CLK_MT8188_VDOSYS0) += clk-mt8188-vdo0.o
obj-$(CONFIG_COMMON_CLK_MT8188_VDOSYS1) += clk-mt8188-vdo1.o
obj-$(CONFIG_COMMON_CLK_MT8188_VENCSYS) += clk-mt8188-venc.o
obj-$(CONFIG_COMMON_CLK_MT8188_VPPSYS0) += clk-mt8188-vpp0.o
obj-$(CONFIG_COMMON_CLK_MT8188_VPPSYS1) += clk-mt8188-vpp1.o
obj-$(CONFIG_COMMON_CLK_MT8188_WPESYS) += clk-mt8188-wpe.o
obj-$(CONFIG_COMMON_CLK_MT8188_CCUSYS) += clk-mt8188-ccu.o
obj-$(CONFIG_COMMON_CLK_MT8188_MFGCFG) += clk-mt8188-mfgcfg.o
endif
obj-$(CONFIG_COMMON_CLK_MT8516) += clk-mt8516.o
obj-$(CONFIG_COMMON_CLK_MT8516_AUDSYS) += clk-mt8516-aud.o
obj-$(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING) += fhctl.o
fhctl-y += clk-fhctl.o
fhctl-y += clk-fhctl-ap.o
fhctl-y += clk-fhctl-pll.o
ifeq ($(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING_MCUPM),y)
fhctl-y += clk-fhctl-mcupm.o
ccflags-y += -DUSE_FHCTL_MCUPM
endif
fhctl-y += clk-fhctl-gpueb.o
ccflags-y += -DUSE_FHCTL_GPUEB
fhctl-$(CONFIG_DEBUG_FS) += clk-fhctl-debug.o
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/include/linux/soc/mediatek/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcupm/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpueb/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/rv/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/include/

obj-$(CONFIG_COMMON_CLK_MT6853) += clk-chk.o
clk-chk-$(CONFIG_COMMON_CLK_MT6853) += clkchk.o clkchk-mt6853.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6853) += clk-dbg.o
clk-dbg-$(CONFIG_COMMON_CLK_MT6853) += clkdbg.o clkdbg-mt6853.o
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6878.c),)
obj-$(CONFIG_COMMON_CLK_MT6878) += clk-chk-mt6878.o
clk-chk-mt6878-$(CONFIG_COMMON_CLK_MT6878) += clkchk-mt6878.o
obj-$(CONFIG_COMMON_CLK_MT6878) += clk-fmeter-mt6878.o
obj-$(CONFIG_COMMON_CLK_MT6878) += pd-chk-mt6878.o
pd-chk-mt6878-$(CONFIG_COMMON_CLK_MT6878) += mtk-pd-chk-mt6878.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6878) += clk-dbg-mt6878.o
clk-dbg-mt6878-$(CONFIG_COMMON_CLK_MT6878) += clkdbg-mt6878.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6886.c),)
obj-$(CONFIG_COMMON_CLK_MT6886) += clk-chk-mt6886.o
clk-chk-mt6886-$(CONFIG_COMMON_CLK_MT6886) += clkchk-mt6886.o
obj-$(CONFIG_COMMON_CLK_MT6886) += clk-fmeter-mt6886.o
obj-$(CONFIG_COMMON_CLK_MT6886) += pd-chk-mt6886.o
pd-chk-mt6886-$(CONFIG_COMMON_CLK_MT6886) += mtk-pd-chk-mt6886.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6886) += clk-dbg-mt6886.o
clk-dbg-mt6886-$(CONFIG_COMMON_CLK_MT6886) += clkdbg-mt6886.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6897.c),)
obj-$(CONFIG_COMMON_CLK_MT6897) += clk-chk-mt6897.o
clk-chk-mt6897-$(CONFIG_COMMON_CLK_MT6897) += clkchk-mt6897.o
obj-$(CONFIG_COMMON_CLK_MT6897) += clk-fmeter-mt6897.o
obj-$(CONFIG_COMMON_CLK_MT6897) += pd-chk-mt6897.o
pd-chk-mt6897-$(CONFIG_COMMON_CLK_MT6897) += mtk-pd-chk-mt6897.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6897) += clk-dbg-mt6897.o
clk-dbg-mt6897-$(CONFIG_COMMON_CLK_MT6897) += clkdbg-mt6897.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6983.c),)
obj-$(CONFIG_COMMON_CLK_MT6983) += clk-chk-mt6983.o
clk-chk-mt6983-$(CONFIG_COMMON_CLK_MT6983) += clkchk-mt6983.o
obj-$(CONFIG_COMMON_CLK_MT6983) += clk-fmeter-mt6983.o
obj-$(CONFIG_COMMON_CLK_MT6983) += pd-chk-mt6983.o
pd-chk-mt6983-$(CONFIG_COMMON_CLK_MT6983) += mtk-pd-chk-mt6983.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6983) += clk-dbg-mt6983.o
clk-dbg-mt6983-$(CONFIG_COMMON_CLK_MT6983) += clkdbg-mt6983.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6985.c),)
obj-$(CONFIG_COMMON_CLK_MT6985) += clk-chk-mt6985.o
clk-chk-mt6985-$(CONFIG_COMMON_CLK_MT6985) += clkchk-mt6985.o
obj-$(CONFIG_COMMON_CLK_MT6985) += clk-fmeter-mt6985.o
obj-$(CONFIG_COMMON_CLK_MT6985) += pd-chk-mt6985.o
pd-chk-mt6985-$(CONFIG_COMMON_CLK_MT6985) += mtk-pd-chk-mt6985.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6985) += clk-dbg-mt6985.o
clk-dbg-mt6985-$(CONFIG_COMMON_CLK_MT6985) += clkdbg-mt6985.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6989.c),)
obj-$(CONFIG_COMMON_CLK_MT6989) += clk-chk-mt6989.o
clk-chk-mt6989-$(CONFIG_COMMON_CLK_MT6989) += clkchk-mt6989.o
obj-$(CONFIG_COMMON_CLK_MT6989) += clk-fmeter-mt6989.o
obj-$(CONFIG_COMMON_CLK_MT6989) += pd-chk-mt6989.o
pd-chk-mt6989-$(CONFIG_COMMON_CLK_MT6989) += mtk-pd-chk-mt6989.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6989) += clk-dbg-mt6989.o
clk-dbg-mt6989-$(CONFIG_COMMON_CLK_MT6989) += clkdbg-mt6989.o
endif
endif