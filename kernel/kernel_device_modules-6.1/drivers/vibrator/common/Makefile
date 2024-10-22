subdir-ccflags-y           := -Wformat
obj-$(CONFIG_SEC_VIBRATOR) += sec_vibrator.o
ifeq ($(CONFIG_SEC_KUNIT), y)
obj-$(CONFIG_SEC_KUNIT) += kunit_test/
GCOV_PROFILE_sec_vibrator.o := $(CONFIG_SEC_KUNIT)
endif