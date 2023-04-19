# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux kernel panel device drivers.
#

subdir-ccflags-$(CONFIG_SEC_KUNIT) += \
	-I$(srctree)/$(src)/.. \
	-Wframe-larger-than=8192 \
	-Wno-unused-variable \
	-Wno-unused-function \
	-Wno-missing-braces \
	-Wno-implicit-fallthrough \
	-Wno-format


mcd-panel-nt36672c_m14x_00-y += tft_common.o nt36672c_m14x_00.o
obj-m += mcd-panel-nt36672c_m14x_00.o
mcd-panel-ft8720_m14x_01-y += tft_common.o ft8720_m14x_01.o
obj-m += mcd-panel-ft8720_m14x_01.o
mcd-panel-nt36672c_a14x_01-y += tft_common.o nt36672c_a14x_01.o
obj-m += mcd-panel-nt36672c_a14x_01.o