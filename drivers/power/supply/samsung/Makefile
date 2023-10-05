# SPDX-License-Identifier: GPL-2.0
#
# Makefile for multifunction miscellaneous devices for samsung pmic
#

obj-$(CONFIG_FUELGAUGE_S2MU106) += s2mu106_fuelgauge.o
obj-$(CONFIG_CHARGER_S2MU106)   += s2mu106_charger.o
obj-$(CONFIG_PM_S2MU106)  		+= s2mu106_pmeter.o
obj-$(CONFIG_SYSFS_S2MF301)   += ifpmic_class.o
obj-$(CONFIG_CHARGER_S2MF301)   += s2mf301_charger.o
obj-$(CONFIG_FUELGAUGE_S2MF301) += s2mf301_fuelgauge.o
obj-$(CONFIG_PM_S2MF301)  		+= s2mf301_pmeter.o
obj-$(CONFIG_TOP_S2MF301)  		+= s2mf301_top.o
obj-$(CONFIG_MANAGER_S2M_CHARGER)   += s2m_chg_manager.o
obj-$(CONFIG_SWITCHING_S2ASL01)   += s2asl01_switching.o
s2mc501_dc_module-$(CONFIG_CHARGER_S2MC501) += s2mc501_direct_charger.o s2mc501_pmeter.o
obj-$(CONFIG_CHARGER_S2MC501)	+= s2mc501_dc_module.o
