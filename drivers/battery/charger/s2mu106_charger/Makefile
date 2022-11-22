obj-$(CONFIG_PM_S2MU106)  		+= s2mu106_pmeter.o
obj-$(CONFIG_CHARGER_S2MU106)   += s2mu106_charger.o

GCOV_PROFILE := $(CONFIG_KUNIT)
obj-$(CONFIG_KUNIT) += test/

ccflags-y := -Wformat