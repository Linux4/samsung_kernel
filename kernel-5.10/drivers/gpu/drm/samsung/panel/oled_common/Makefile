# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux kernel panel device drivers.
#

usdm-panel-oled-common-y := \
	oled_common.o \
	oled_common_dump.o \
	oled_common_cond.o

usdm-panel-oled-common-$(CONFIG_EXTEND_LIVE_CLOCK) += oled_common_aod.o
usdm-panel-oled-common-$(CONFIG_EXYNOS_DECON_MDNIE_LITE) += oled_common_mdnie.o
obj-$(CONFIG_USDM_PANEL_OLED_COMMON) += usdm-panel-oled-common.o

subdir-ccflags-$(CONFIG_SEC_KUNIT) += \
	-I$(srctree)/$(src)/.. \
	-Wframe-larger-than=8192 \
	-Wno-unused-variable \
	-Wunused-function \
	-Wno-missing-braces \
	-Wno-implicit-fallthrough \
	-Wformat

