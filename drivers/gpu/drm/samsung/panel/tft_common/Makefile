# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux kernel panel device drivers.
#

usdm-panel-tft-common-y := \
	tft_function.o \
	tft_common.o

obj-$(CONFIG_USDM_PANEL_TFT_COMMON) += usdm-panel-tft-common.o

subdir-ccflags-$(CONFIG_SEC_KUNIT) += \
	-I$(srctree)/$(src)/.. \
	-Wframe-larger-than=8192 \
	-Wunused-variable \
	-Wunused-function \
	-Wno-missing-braces \
	-Wno-implicit-fallthrough \
	-Wno-format


mcd-panel-td4160_a14x_00-y += td4160_a14x_00.o
obj-$(CONFIG_USDM_PANEL_TD4160_A14X_00) += mcd-panel-td4160_a14x_00.o

mcd-panel-nt36672c_a14x_01-y += nt36672c_a14x_01.o
obj-$(CONFIG_USDM_PANEL_NT36672C_A14X_01) += mcd-panel-nt36672c_a14x_01.o

mcd-panel-ili7807_a14x_02-y += ili7807_a14x_02.o
obj-$(CONFIG_USDM_PANEL_ILI7807_A14X_02) += mcd-panel-ili7807_a14x_02.o
