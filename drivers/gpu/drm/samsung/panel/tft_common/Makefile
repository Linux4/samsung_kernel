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


mcd-panel-hx83122a_gts9fep_csot-y += hx83122a_gts9fep_csot.o
obj-$(CONFIG_USDM_PANEL_HX83122A_GTS9FEP_CSOT) += mcd-panel-hx83122a_gts9fep_csot.o

mcd-panel-hx83122a_gts9fep_boe-y += tft_common.o hx83122a_gts9fep_boe.o
obj-$(CONFIG_USDM_PANEL_HX83122A_GTS9FEP_BOE) += mcd-panel-hx83122a_gts9fep_boe.o