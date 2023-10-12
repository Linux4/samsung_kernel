# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux kernel panel device drivers.
#

mcd-panel-s6e3hae-y := s6e3hae.o
mcd-panel-s6e3hae-$(CONFIG_EXTEND_LIVE_CLOCK) += s6e3hae_aod.o
mcd-panel-s6e3hae-$(CONFIG_SUPPORT_DISPLAY_PROFILER) += s6e3hae_profiler.o

obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3HAE) += mcd-panel-s6e3hae.o

