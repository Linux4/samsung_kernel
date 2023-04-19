# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux kernel panel device drivers.
#

subdir-ccflags-$(CONFIG_SEC_KUNIT) += \
	-I$(srctree)/$(src)/.. \
	-Wframe-larger-than=8192 \
	-Wno-unused-variable \
	-Wunused-function \
	-Wno-missing-braces \
	-Wno-implicit-fallthrough \
	-Wformat

subdir-ccflags-y += -I$(srctree)/$(src)/../oled_common


subdir-ccflags-y += -I$(srctree)/$(src)/../oled_common

mcd-panel-sw83109_m54x-y += sw83109.o sw83109_m54x.o
obj-$(CONFIG_EXYNOS_DECON_LCD_SW83109) += mcd-panel-sw83109_m54x.o
