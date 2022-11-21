#
# Makefile for Marvell's PXA168 processors line
#

obj-y				+= common.o mmp_device.o devices.o time.o pmu.o reset.o onboard/

# SoC support
obj-$(CONFIG_CPU_PXA168)	+= pxa168.o irq.o clock.o
obj-$(CONFIG_CPU_PXA910)	+= pxa910.o irq.o clock.o sram.o
obj-$(CONFIG_CPU_PXA988)	+= pxa988.o clock-pxa988.o wakeupgen.o sram.o \
acpuclock-pxa988.o dvfs-pxa988.o gpio-edge.o helpfunc_v7.o pxa988_lowpower.o reset-pxa988.o
obj-$(CONFIG_CPU_MMP2)		+= mmp2.o sram.o irq.o clock.o
obj-$(CONFIG_CPU_MMP3)		+= mmp3.o irq-mmp3.o clock-mmp3.o \
sram.o

ifeq ($(CONFIG_SMP),y)
obj-$(CONFIG_CPU_MMP3)		+= platsmp.o mmp3_headsmp.o reset-mmp3.o
obj-$(CONFIG_CPU_PXA988)	+= platsmp.o pxa988_headsmp.o
endif

ifeq ($(CONFIG_HOTPLUG_CPU),y)
obj-$(CONFIG_CPU_MMP3)		+= mmp3_hotplug.o
endif

ifeq ($(CONFIG_PM),y)
obj-$(CONFIG_CPU_PXA910)	+= pm-pxa910.o
obj-$(CONFIG_CPU_MMP2)		+= pm-mmp2.o
obj-$(CONFIG_CPU_PXA988)	+= sleep-pxa988.o
endif

ifeq ($(CONFIG_CPU_IDLE),y)
obj-$(CONFIG_CPU_PXA988)	+= cpuidle-pxa988.o
endif

ifeq ($(CONFIG_SUSPEND),y)
obj-$(CONFIG_CPU_PXA988)        += pm-pxa988.o
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
obj-$(CONFIG_MACH_ARUBA_TD)	+= board-aruba.o samsung_camera.o sec-common.o
obj-$(CONFIG_MACH_HENDRIX)	+= board-hendrix.o sec-common.o camera-sr030pc50.o
obj-$(CONFIG_MACH_LT02)		+= board-lt02-pins.o board-lt02-battery.o board-lt02.o samsung_camera_lt02.o sec-common.o \
							   board-lt02-regulators.o board-lt02-irled.o
obj-$(CONFIG_MACH_COCOA7)	+= board-cocoa7-pins.o board-cocoa7-battery.o board-cocoa7.o samsung_camera_lt02.o sec-common.o \
							   board-cocoa7-regulators.o
obj-$(CONFIG_GPU_RESERVE_MEM)   += gpu_mem.o

ifeq ($(CONFIG_HOTPLUG_CPU),y)
obj-$(CONFIG_CPU_PXA988)        += pxa988_hotplug.o
obj-$(CONFIG_MMP_ORIGINAL_POLICY)        += pm-hotplug-dual.o
obj-$(CONFIG_STAND_ALONE_POLICY)        += stand-hotplug.o
endif

# coresight support
obj-$(CONFIG_CORESIGHT_SUPPORT)	+= coresight-v7.o

obj-$(CONFIG_SEC_THERMISTOR) += sec_thermistor.o
