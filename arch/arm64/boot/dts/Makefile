dtb-$(CONFIG_ARCH_THUNDER) += thunder-88xx.dtb
dtb-$(CONFIG_ARCH_VEXPRESS) += rtsm_ve-aemv8a.dtb foundation-v8.dtb
dtb-$(CONFIG_ARCH_XGENE) += apm-mustang.dtb

targets += dtbs

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES))
#
numbers := 0 1 2 3 4 5 6 7 8 9
DTB_NAMES_EXT := $(foreach x,$(numbers),$(foreach y,$(numbers),$(if $(wildcard $(srctree)/arch/arm64/boot/dts/$(DTB_NAMES)_$(x)$(y).dts),$(DTB_NAMES)_$(x)$(y))))
DTB_NAMES := $(strip $(DTB_NAMES) $(DTB_NAMES_EXT))

ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif
targets += $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := dts/*.dtb *.dtb
