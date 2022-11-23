obj-$(CONFIG_SB_MFC)		+= sb-mfc.o
sb-mfc-$(CONFIG_SB_MFC)		+= sb_mfc.o

ifeq ($(CONFIG_SEC_KUNIT), y)
 sb-mfc-$(CONFIG_SB_MFC)	+= kunit_test/sb_mfc_test.o
 GCOV_PROFILE_sb_mfc.o := $(CONFIG_SEC_KUNIT)
endif

ccflags-y := -Wformat

obj-$(CONFIG_WIRELESS_CHARGER_P9320)	+= p9320_charger.o

ccflags-y := -Wformat

obj-$(CONFIG_WIRELESS_CHARGER_S2MIW04)	+= s2miw04_charger.o

ccflags-y := -Wformat
