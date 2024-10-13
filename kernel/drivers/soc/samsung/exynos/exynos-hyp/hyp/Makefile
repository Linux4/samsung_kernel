#
#  Makefile for Exynos Hypervisor EL2 module
#

EXTRA_CFLAGS=-I$(srctree)/arch/arm64/kvm/hyp/include
hyp-obj-y := exynos-hyp-module.o exynos-hyp-s2mpu.o
include $(srctree)/arch/arm64/kvm/hyp/nvhe/Makefile.module
