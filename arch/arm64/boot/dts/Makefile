dtb-$(CONFIG_ARCH_VEXPRESS) += rtsm_ve-aemv8a.dtb foundation-v8.dtb
dtb-$(CONFIG_MACH_SP9836AEA_FPGA) += sprd_sp9836aea_fpga.dtb
dtb-$(CONFIG_MACH_SP9836AEA_4MOD) += sprd_sp9836aea_4mod.dtb
dtb-$(CONFIG_MACH_SP9836AEA_3MOD) += sprd_sp9836aea_3mod.dtb
dtb-$(CONFIG_MACH_SP9838AEA_5MOD) += sprd_sp9838aea_5mod.dtb
dtb-$(CONFIG_MACH_SS_SHARKLT8) += sprd_ss_sharklt8.dtb
dtb-$(CONFIG_MACH_SP9838AEA_FPGA) += sprd_sp9838aea_fpga.dtb

dtb-$(CONFIG_ARCH_VEXPRESS) += rtsm_ve-aemv8a.dtb foundation-v8.dtb \
				fvp-base-gicv2-psci.dtb
dtb-$(CONFIG_ARCH_VEXPRESS) += juno.dtb
dtb-$(CONFIG_ARCH_XGENE) += apm-mustang.dtb

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
