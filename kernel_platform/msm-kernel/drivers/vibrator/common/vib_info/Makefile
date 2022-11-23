#
# Makefile for vib_info driver
#
subdir-ccflags-y := -Wformat
obj-$(CONFIG_VIBRATOR_VIB_INFO)        += vibrator_vib_info.o
obj-$(CONFIG_SEC_KUNIT) += kunit_test/
GCOV_PROFILE_vibrator_vib_info.o := $(CONFIG_SEC_KUNIT)