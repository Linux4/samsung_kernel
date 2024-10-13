#
#  Makefile for Exynos Hypervisor
#

obj-$(CONFIG_EXYNOS_HYP_MODULE) += exynos-hypervisor.o

$(obj)/hyp/kvm_nvhe.o: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/hyp $(obj)/hyp/kvm_nvhe.o

clean-files := hyp/hyp.lds hyp/hyp-reloc.S

exynos-hypervisor-y := exynos-hyp.o hyp/kvm_nvhe.o

obj-$(CONFIG_EXYNOS_HYP_S2MPU) += exynos-pkvm-s2mpu.o
