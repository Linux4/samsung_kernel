dtb-$(CONFIG_ARCH_VEXPRESS) += rtsm_ve-aemv8a.dtb foundation-v8.dtb \
				fvp-base-gicv2-psci.dtb
dtb-$(CONFIG_ARCH_VEXPRESS) += juno.dtb
dtb-$(CONFIG_ARCH_XGENE) += apm-mustang.dtb
dtb-$(CONFIG_MACH_ESPRESSO7420) += exynos7420-espresso7420.dtb
dtb-$(CONFIG_MACH_UNIVERSAL7420) += exynos7420-universal7420.dtb exynos7420-universal7420-ufs.dtb exynos7420-universal7420-ufs_rev00.dtb
dtb-$(CONFIG_MACH_SMDK7580) += exynos7580-smdk7580.dtb
dtb-$(CONFIG_MACH_UNIVERSAL7580) += exynos7580-universal7580.dtb exynos7580-universal7580_rev01.dtb exynos7580-universal7580_q.dtb

dtb-$(CONFIG_MACH_UNIVERSAL5433) += exynos5433-universal5433_rev02.dtb exynos5433-universal5433.dtb
targets += dtbs

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif
targets += $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := dts/*.dtb *.dtb
