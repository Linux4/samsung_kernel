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

mcd-panel-s6e3fae_e1s-y += s6e3fae.o s6e3fae_e1s.o
mcd-panel-s6e3fae_e1s-$(CONFIG_USDM_PANEL_SELF_DISPLAY) += s6e3fae_e1s_aod.o

obj-$(CONFIG_USDM_PANEL_S6E3FAE) += mcd-panel-s6e3fae_e1s.o

