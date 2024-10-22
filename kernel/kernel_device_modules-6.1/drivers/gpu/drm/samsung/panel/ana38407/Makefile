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

obj-$(CONFIG_SEC_KUNIT) += kunit_test/

subdir-ccflags-y += -I$(srctree)/$(src)/../oled_common

mcd-panel-ana38407_gts10f-y += ana38407.o ana38407_gts10u.o

obj-$(CONFIG_USDM_PANEL_ANA38407) += mcd-panel-ana38407_gts10f.o

obj-$(CONFIG_SEC_KUNIT) += kunit_test/
