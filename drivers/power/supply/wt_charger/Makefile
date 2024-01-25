# SPDX-License-Identifier: GPL-2.0-only
KDIR	:= /lib/modules/$(shell uname -r)/build

ifeq ($(filter $(PRODUCT_NAME_WT_KERNEL), P86801EA1), $(PRODUCT_NAME_WT_KERNEL))
subdir-ccflags-y += -DCONFIG_AMERICA_VERSION
endif
obj-$(CONFIG_CHARGE_SGM41542)	+= sgm41542_charger.o
obj-$(CONFIG_CHARGE_CX25890H)   += cx25890h_charger.o
obj-$(CONFIG_CHARGE_SC89890H)   += sc89890h_charger.o
obj-$(CONFIG_FG_CW2217)	+= cw2217_fuel_gauge.o
obj-$(CONFIG_FG_SD77428) += sd77428_battery/
obj-$(CONFIG_WT_CHARGER)	+= wt_chg.o
obj-$(CONFIG_AFC)	+= afc/