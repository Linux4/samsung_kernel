obj-$(CONFIG_PM_S2MU107)  		+= s2mu107_pmeter.o
obj-$(CONFIG_CHARGER_S2MU107)   += s2mu107_switching_charger.o
obj-$(CONFIG_CHARGER_S2MU107_DIRECT)   += s2mu107_direct_charger.o

GCOV_PROFILE := $(CONFIG_KUNIT)
obj-$(CONFIG_KUNIT) += test/

ccflags-y := -Wformat