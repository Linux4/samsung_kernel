ifeq ($(strip $(CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE)), y)
dtb-y += $(addsuffix .dtb, $(notdir $(subst $\",,$(CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE_NAMES))))
endif

ifeq ($(strip $(CONFIG_BUILD_ARM_DTB_OVERLAY_IMAGE)), y)
dtb-y += $(addsuffix .dtb, $(notdir $(subst $\",,$(CONFIG_BUILD_ARM_DTB_OVERLAY_IMAGE_NAMES))))
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb

