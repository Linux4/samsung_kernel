#
# Makefile for kernel Silead Fingerprint drivers.
#

ifdef CONFIG_ARCH_QCOM
	 ccflags-y += -DBSP_SIL_PLAT_QCOM
endif

obj-$(CONFIG_SILEAD_FP_PLATFORM)	+= silead_fp_platform.o
