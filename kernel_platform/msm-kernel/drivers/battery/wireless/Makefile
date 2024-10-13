obj-$(CONFIG_WIRELESS_CHARGER_NU1668)	+= nu1668-charger.o

nu1668-charger-$(CONFIG_WIRELESS_CHARGER_NU1668) += nu1668_fod.o nu1668_cmfet.o nu1668_charger.o

ccflags-y := -Wformat

ifdef CONFIG_SEC_KUNIT
ccflags-y := -Wno-unused-variable
GCOV_PROFILE_nu1668_charger.o		:= $(CONFIG_SEC_KUNIT)
endif
