# SPDX-License-Identifier: GPL-2.0
# COPYRIGHT(C) 2023 Samsung Electronics Co., Ltd. All Right Reserved.

dtstree		:= $(srctree)/$(src)
a05s_boards	:= $(patsubst $(dtstree)/%.dts,%.dtbo, $(wildcard $(dtstree)/*.dts))

# add-overlay defines the target with following naming convention:
# <base>-<board>-dtbs = base.dtb board.dtbo
#
# Combined dtb target is also generated using the fdt_overlay tool.
# dtb-y += <base>-<board>.dtb

add-overlays	= $(foreach o,$1,$(foreach b,$2,$(eval $(basename $(notdir $b))-$(basename $o)-dtbs = $b $o) $(basename $(notdir $b))-$(basename $o).dtb))

# KHAJE BASE DTB
qcom_dtb_dir	:= ../../../vendor/qcom
sec_khaje_base_dtb	:= \
		$(qcom_dtb_dir)/khaje.dtb \
		$(qcom_dtb_dir)/khajep.dtb \
		$(qcom_dtb_dir)/khajeq.dtb \
		$(qcom_dtb_dir)/khajeg.dtb

m269-dtb-$(CONFIG_ARCH_KHAJE)	+= \
        $(call add-overlays, $(a05s_boards) ,$(sec_khaje_base_dtb))
m269-overlays-dtb-$(CONFIG_ARCH_KHAJE) += $(a05s_boards) $(sec_khaje_base_dtb)

dtb-$(CONFIG_ARCH_KHAJE)	+= $(m269-dtb-y)

always-y        := $(dtb-y)
subdir-y        := $(dts-dirs)

clean-files	:= *.dtb *.reverse.dts *.dtbo
