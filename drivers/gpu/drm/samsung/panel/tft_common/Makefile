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


mcd-panel-nt36523n_gts9fe_csot-y += nt36523n_gts9fe_csot.o
obj-$(CONFIG_USDM_PANEL_NT36523N_GTS9FE_CSOT) += mcd-panel-nt36523n_gts9fe_csot.o

mcd-panel-hx83102j_gts9fe_boe-y += hx83102j_gts9fe_boe.o
obj-$(CONFIG_USDM_PANEL_HX83102J_GTS9FE_BOE) += mcd-panel-hx83102j_gts9fe_boe.o
