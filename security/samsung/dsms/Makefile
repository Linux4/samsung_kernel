#
# Makefile for the DSMS
#

obj-$(CONFIG_SECURITY_DSMS) := dsms_kernel_api.o
obj-$(CONFIG_SECURITY_DSMS) += dsms_access_control.o
obj-$(CONFIG_SECURITY_DSMS) += dsms_rate_limit.o
obj-$(CONFIG_SECURITY_DSMS) += dsms_policy.o
obj-$(CONFIG_SECURITY_DSMS) += dsms_init.o
obj-$(CONFIG_SECURITY_DSMS) += dsms_debug.o

EXTRA_CFLAGS += -DDSMS_WHITELIST_IGNORE_NAME_SUFFIXES_ENABLE

# Debugging options:

ifneq (,$(filter userdebug eng, $(TARGET_BUILD_VARIANT)))
#    CFLAGS_dsms_debug.o := -DDEBUG
endif

GCOV_PROFILE := $(CONFIG_KUNIT)
obj-$(CONFIG_KUNIT) += test/
