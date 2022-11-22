#
# Makefile for Marvell's PXA168 processors line
#

obj-y				+= common.o mmp_device.o devices.o pmu.o reset.o onboard/

obj-$(CONFIG_TZ_HYPERVISOR)	+= tzlc/

# SoC support
obj-$(CONFIG_CPU_PXA168)	+= pxa168.o irq.o clock.o time.o
obj-$(CONFIG_CPU_PXA910)	+= pxa910.o irq.o clock.o sram.o time.o
obj-$(CONFIG_CPU_PXA988)	+= pxa988.o clock-pxa988.o wakeupgen.o sram.o time.o \
acpuclock-pxa988.o dvfs-pxa988.o gpio-edge.o helpfunc_v7.o pxa988_lowpower.o reset-pxa988.o
obj-$(CONFIG_CPU_PXA1088)	+= pxa988.o clock-pxa988.o wakeupgen.o sram.o \
acpuclock-pxa988.o dvfs-pxa988.o gpio-edge.o helpfunc_v7.o pxa988_lowpower.o reset-pxa988.o

ifeq ($(CONFIG_APB_LOCALTIMER),y)
obj-$(CONFIG_CPU_PXA1088)       += time-apb.o
else
obj-$(CONFIG_CPU_PXA1088)       += time.o
endif

obj-$(CONFIG_CPU_MMP2)		+= mmp2.o sram.o irq.o clock.o time.o
obj-$(CONFIG_CPU_MMP3)		+= mmp3.o irq-mmp3.o clock-mmp3.o time.o\
sram.o
obj-$(CONFIG_CPU_EDEN)		+= eden.o sram.o clock-eden.o reset-eden.o helpfunc_v7.o time-eden.o
ifeq ($(CONFIG_CPU_EDEN),y)
ifeq ($(CONFIG_GIC_BYPASS),y)
obj-y				+= irq-eden-icu.o
else
obj-y				+= irq-eden.o
endif
endif

ifeq ($(CONFIG_SMP),y)
obj-$(CONFIG_CPU_MMP3)		+= platsmp.o mmp3_headsmp.o reset-mmp3.o
obj-$(CONFIG_CPU_PXA988)	+= platsmp.o pxa988_headsmp.o
obj-$(CONFIG_CPU_PXA1088)	+= platsmp.o pxa988_headsmp.o
obj-$(CONFIG_CPU_EDEN)		+= platsmp.o eden_headsmp.o
endif

ifeq ($(CONFIG_PM),y)
obj-$(CONFIG_CPU_PXA910)	+= pm-pxa910.o
obj-$(CONFIG_CPU_MMP2)		+= pm-mmp2.o
obj-$(CONFIG_CPU_PXA988)	+= sleep-pxa988.o
obj-$(CONFIG_CPU_PXA1088)	+= sleep-pxa988.o
obj-$(CONFIG_CPU_EDEN)		+= pm-eden.o
endif

ifeq ($(CONFIG_CPU_IDLE),y)
obj-$(CONFIG_CPU_PXA988)	+= cpuidle-pxa988.o
obj-$(CONFIG_CPU_PXA1088)	+= cpuidle-pxa988.o
endif

ifeq ($(CONFIG_SUSPEND),y)
obj-$(CONFIG_CPU_PXA988)        += pm-pxa988.o
obj-$(CONFIG_CPU_PXA1088)        += pm-pxa988.o
endif

# fake system off feature base on suspend
obj-$(CONFIG_FAKE_SYSTEMOFF)	+= fake-sysoff.o

# board support
obj-$(CONFIG_MACH_ASPENITE)	+= aspenite.o
obj-$(CONFIG_MACH_ZYLONITE2)	+= aspenite.o
obj-$(CONFIG_MACH_AVENGERS_LITE)+= avengers_lite.o
obj-$(CONFIG_MACH_TAVOREVB)	+= tavorevb.o
obj-$(CONFIG_MACH_TTC_DKB)	+= ttc_dkb.o
obj-$(CONFIG_MACH_BROWNSTONE)	+= brownstone.o
obj-$(CONFIG_MACH_FLINT)	+= flint.o
obj-$(CONFIG_MACH_MARVELL_JASPER) += jasper.o
obj-$(CONFIG_MACH_MMP_DT)	+= mmp-dt.o
obj-$(CONFIG_MACH_MMP2_DT)	+= mmp2-dt.o
obj-$(CONFIG_MACH_TETON_BGA)	+= teton_bga.o
obj-$(CONFIG_MACH_GPLUGD)	+= gplugd.o
obj-$(CONFIG_MACH_YELLOWSTONE)	+= yellowstone.o
obj-$(CONFIG_MACH_EMEIDKB)	+= board-emeidkb.o
obj-$(CONFIG_MACH_ARUBA_TD)	+= board-aruba.o sec-common.o
obj-$(CONFIG_MACH_WARUBA)	+= board-waruba.o sec-common.o
obj-$(CONFIG_MACH_HARRISON)	+= board-harrison.o sec-common.o
obj-$(CONFIG_MACH_HELANDKB)	+= board-helandkb.o
obj-$(CONFIG_MACH_LT02)		+= board-lt02-pins.o board-lt02.o samsung_camera_lt02.o sec-common.o  \
							   board-lt02-regulators.o  board-lt02-battery.o
ifeq ($(CONFIG_MACH_DELOS3GVIA),y)
obj-$(CONFIG_MACH_DELOS3GVIA)	+= board-delos3gvia.o sec-common.o samsung_camera_via.o
else
obj-$(CONFIG_MACH_HELANDELOS)	+= board-delos.o sec-common.o samsung_camera.o
endif

ifeq ($(CONFIG_MACH_CS05),y)
obj-$(CONFIG_MACH_CS05)	+= board-cs05.o board-cs05-battery.o sec-common.o samsung_camera_cs05.o
else
ifeq ($(CONFIG_MACH_DEGAS),y)
obj-$(CONFIG_MACH_DEGAS)	+= board-degas.o board-degas-battery.o board-degas-input.o sec-common.o samsung_camera_degas.o
else
obj-$(CONFIG_MACH_WILCOX)	+= board-wilcox.o board-wilcox-battery.o sec-common.o samsung_camera.o
endif
endif

ifeq ($(CONFIG_MACH_CS02),y)
obj-y		+= board-cs02.o board-cs02-battery.o sec-common.o
obj-$(CONFIG_SOC_CAMERA_S5K4ECGX)	+=samsung_camera.o
obj-$(CONFIG_SOC_CAMERA_SR352)	+= samsung_camera_cs02.o
endif

ifeq ($(CONFIG_MACH_GOLDEN),y)
obj-y		+= board-golden.o board-golden-battery.o sec-common.o board-golden-input.o
obj-$(CONFIG_SOC_CAMERA_S5K4ECGX)	+=samsung_camera_golden.o
endif

ifeq ($(CONFIG_MACH_GOYA),y)
obj-y		+= board-goya.o board-goya-battery.o sec-common.o board-goya-input.o
obj-$(CONFIG_SOC_CAMERA_SR200PC20M)	+=samsung_camera_goya.o
endif

ifeq ($(CONFIG_MACH_BAFFIN),y)
obj-y		+= board-baffin.o board-baffin-battery.o sec-common.o samsung_camera.o
obj-$(CONFIG_SOC_CAMERA_S5K4ECGX)	+=samsung_camera.o
obj-$(CONFIG_SOC_CAMERA_SR352)	+= samsung_camera_baffin.o
endif

ifeq ($(CONFIG_MACH_BAFFINQ),y)
obj-y		+= board-baffinq.o board-baffinq-battery.o sec-common.o
obj-$(CONFIG_SOC_CAMERA_S5K4ECGX)	+=samsung_camera.o
endif

obj-$(CONFIG_MACH_EDEN_FPGA)	+= eden_fpga.o
obj-$(CONFIG_GPU_RESERVE_MEM)   += gpu_mem.o

# feature for tsp
obj-$(CONFIG_TOUCHSCREEN_MXT336S)	+=board_input_mxt.o
obj-$(CONFIG_TOUCHSCREEN_CYPRESS_TMA46X)	+=board-touch-cyttsp4_core.o
obj-$(CONFIG_TOUCHSCREEN_MELFAS_MMS144)	+=board-tsp-mms144.o

ifeq ($(CONFIG_HOTPLUG_CPU),y)
obj-$(CONFIG_CPU_PXA988)        += pxa988_hotplug.o
obj-$(CONFIG_CPU_PXA1088)        += pxa988_hotplug.o
obj-$(CONFIG_CPU_MMP3)		+= mmp3_hotplug.o
obj-$(CONFIG_CPU_EDEN)		+= eden_hotplug.o
obj-$(CONFIG_MMP_ORIGINAL_POLICY)        += pm-hotplug-dual.o
obj-$(CONFIG_STAND_ALONE_POLICY)        += stand-hotplug.o
endif

obj-$(CONFIG_MEMORY_HOTPLUG)	+= mem_hotplug_handler.o

obj-$(CONFIG_MFD_RT5033)		+= board-golden-mfd.o

# coresight support
obj-$(CONFIG_CORESIGHT_SUPPORT)	+= coresight-v7.o
obj-$(CONFIG_SEC_MODEM_DELOS3GVIA) += board-delos3gvia-modems.o

# touchboost support
obj-$(CONFIG_BOOST_SUPPORT)    += boost.o

# sec debug support
obj-$(CONFIG_SEC_DEBUG)		+= sec_debug.o
obj-$(CONFIG_SEC_LOG)		+= sec_log.o

obj-$(CONFIG_SEC_THERMISTOR) += sec_thermistor.o
