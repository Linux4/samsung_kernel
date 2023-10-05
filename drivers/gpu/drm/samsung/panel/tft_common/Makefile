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


mcd-panel-ili7807_a14x_02-y += tft_common.o ili7807_a14x_02.o
obj-m += mcd-panel-ili7807_a14x_02.o
mcd-panel-td4160_a14x_00-y += tft_common.o td4160_a14x_00.o
obj-m += mcd-panel-td4160_a14x_00.o
mcd-panel-nt36672c_a14x_01-y += tft_common.o nt36672c_a14x_01.o
obj-m += mcd-panel-nt36672c_a14x_01.o