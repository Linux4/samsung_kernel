# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux kernel panel device drivers.
#

mcd-panel-s6e8fc3-y := s6e8fc3.o

mcd-panel-s6e8fc3-$(CONFIG_EXTEND_LIVE_CLOCK) += s6e8fc3_aod.o

obj-$(CONFIG_EXYNOS_DECON_LCD_S6E8FC3) += mcd-panel-s6e8fc3.o

