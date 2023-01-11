#
# Makefile for Marvell's PXA168 processors line
#

obj-y				+= devices.o regdump-mmp.o

# SoC support
obj-$(CONFIG_CPU_PXA168)	+= pxa168.o
obj-$(CONFIG_CPU_PXA910)	+= pxa910.o
obj-$(CONFIG_CPU_MMP2)		+= mmp2.o
obj-$(CONFIG_MMP_SRAM)		+= sram.o

ifeq ($(CONFIG_COMMON_CLK), )
obj-y				+= clock.o
obj-$(CONFIG_CPU_PXA168)	+= clock-pxa168.o
obj-$(CONFIG_CPU_PXA910)	+= clock-pxa910.o
obj-$(CONFIG_CPU_MMP2)		+= clock-mmp2.o
endif
ifeq ($(CONFIG_PM),y)
obj-$(CONFIG_CPU_PXA910)	+= pm-pxa910.o
obj-$(CONFIG_CPU_MMP2)		+= pm-mmp2.o
endif

# coresight support
obj-$(CONFIG_CORESIGHT_SUPPORT) += coresight-v7.o

ARM64=../../../arch/arm64/mach

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
obj-$(CONFIG_MACH_MMPX_DT)	+= mmpx-dt.o regs-addr.o v7_pm_setup.o mmp_cpuidle.o reset.o platsmp.o pxa988_lowpower.o restart.o

obj-$(CONFIG_MACH_HELANX_DT)	+= $(ARM64)/helanx-dt.o $(ARM64)/regs-addr.o $(ARM64)/soc_camera_dkb.o $(ARM64)/cpuidle_qos.o $(ARM64)/pm-pxa1908.o $(ARM64)/pxa1908_lowpower.o $(ARM64)/../mcpm/mcpm_plat.o

obj-$(CONFIG_MACH_J1ACELTE_LTN)	+= $(ARM64)/helanx-dt.o $(ARM64)/regs-addr.o $(ARM64)/soc_camera_j1ace.o $(ARM64)/cpuidle_qos.o $(ARM64)/pm-pxa1908.o $(ARM64)/pxa1908_lowpower.o $(ARM64)/../mcpm/mcpm_plat.o

ifeq ($(CONFIG_SUSPEND),y)
obj-$(CONFIG_ARCH_MMP)		+= pm.o
endif
obj-$(CONFIG_MACH_TETON_BGA)	+= teton_bga.o
obj-$(CONFIG_MACH_GPLUGD)	+= gplugd.o
