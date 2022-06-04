# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux kernel panel device drivers.
#

mcd-panel-s6e3fac-y := s6e3fac.o
mcd-panel-s6e3fac-$(CONFIG_EXTEND_LIVE_CLOCK) += s6e3fac_aod.o
mcd-panel-s6e3fac-$(CONFIG_SUPPORT_DISPLAY_PROFILER) += s6e3fac_profiler.o

obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3FAC) += mcd-panel-s6e3fac.o

