obj-$(CONFIG_MEMORY_ZEROISATION) += mz_ioctl.o mz.o mz_crypto.o mz_mem.o mz_log.o

ifneq (,$(filter $(CONFIG_SOC_S5E8825), y))
	obj-$(CONFIG_MEMORY_ZEROISATION) += mz_page_v5_10.o
else
	obj-$(CONFIG_MEMORY_ZEROISATION) += mz_page.o
endif

ifneq (,$(filter y m, $(CONFIG_MZ_TEE_DRIVER)))
	ccflags-y += -DMZ_TA
endif

ifneq (,$(filter userdebug eng, $(TARGET_BUILD_VARIANT)))
	ccflags-$(CONFIG_MEMORY_ZEROISATION) += -DMZ_DEBUG -DDEBUG
endif

ifeq ($(CONFIG_SEC_KUNIT), y)
	GCOV_PROFILE := y
	obj-$(CONFIG_MEMORY_ZEROISATION) += test/
endif
