dtb-$(CONFIG_ARCH_VEXPRESS) += rtsm_ve-aemv8a.dtb foundation-v8.dtb
dtb-$(CONFIG_MACH_SP9836AEA_FPGA) += sprd_sp9836aea_fpga.dtb
dtb-$(CONFIG_MACH_SP9836AEA_4MOD) += sprd_sp9836aea_4mod.dtb
dtb-$(CONFIG_MACH_SP9836AEA_3MOD) += sprd_sp9836aea_3mod.dtb
dtb-$(CONFIG_MACH_SP9838AEA_5MOD) += sprd_sp9838aea_5mod.dtb
dtb-$(CONFIG_MACH_JIAOTU_TC) += sprd_jiaotu_tc.dtb
dtb-$(CONFIG_MACH_JIAOTU_TC_VOLTE) += sprd_jiaotu_tc_volte.dtb
dtb-$(CONFIG_MACH_SP9838AEA_POWER_DEBUG) += sprd_sp9838aea_power_debug.dtb
dtb-$(CONFIG_MACH_SP9838AEA_8CORE_LIGHT_SLEEP) += sprd_sp9838aea_8core_light_sleep.dtb
dtb-$(CONFIG_MACH_SP9838AEA_FHD) += sprd_sp9838aea_fhd.dtb
dtb-$(CONFIG_MACH_SP9838AEA_AUTO_SLT) += sprd_sp9838aea_auto_slt.dtb
dtb-$(CONFIG_MACH_SP9838AEA_2342A) += sprd_sp9838aea_2342a.dtb
dtb-$(CONFIG_MACH_SP9838AEA_SR3593) += sprd_sp9838aea_sr3593.dtb
dtb-$(CONFIG_MACH_SP9838AEA_3_VOLTE) += sprd_sp9838aea_3_volte.dtb
dtb-$(CONFIG_MACH_SP9838AEA_VOLTE) += sprd_sp9838aea_volte.dtb
dtb-$(CONFIG_MACH_SP9838AEA_2342A_FHD) += sprd_sp9838aea_2342a_fhd.dtb
dtb-$(CONFIG_MACH_SP9838AEA_2723TS) += sprd_sp9838aea_2723ts.dtb
dtb-$(CONFIG_MACH_SP9838AEA_NO_WCN) += sprd_sp9838aea_no_wcn.dtb
dtb-$(CONFIG_MACH_SS_SHARKLT8) += sprd_ss_sharklt8.dtb
dtb-$(CONFIG_MACH_SS_SHARKLT8LIGHTSLEEP) += sprd_ss_sharklt8lightsleep.dtb
dtb-$(CONFIG_MACH_SP9838AEA_FPGA) += sprd_sp9838aea_fpga.dtb
dtb-$(CONFIG_MACH_SPWHALE_5MODEB_VOLTE) += sprd_spwhale_5modeb_volte.dtb
dtb-$(CONFIG_MACH_SPWHALE_5MODEB) += sprd_spwhale_5modeb.dtb

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
