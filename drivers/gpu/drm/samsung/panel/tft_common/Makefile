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


mcd-panel-nt36672c_a14x_01-y += nt36672c_a14x_01.o
obj-$(CONFIG_USDM_PANEL_NT36672C_A14X_01) += mcd-panel-nt36672c_a14x_01.o

mcd-panel-ft8720_m14x_01-y += ft8720_m14x_01.o
obj-$(CONFIG_USDM_PANEL_FT8720_M14X_01) += mcd-panel-ft8720_m14x_01.o

mcd-panel-nt36672c_m14x_00-y += nt36672c_m14x_00.o
obj-$(CONFIG_USDM_PANEL_NT36672C_M14X_00) += mcd-panel-nt36672c_m14x_00.o
