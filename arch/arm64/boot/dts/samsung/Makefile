# SPDX-License-Identifier: GPL-2.0

dts-dirs += gts7/gts7fewifi

# SPU
SEC_SYSTEM_UPDATE_DTBO := ./yupik-sec-system-update-overlay.dtbo

# YUPIK BASE DTB
SEC_YUPIK_BASE_DTB := ../vendor/qcom/yupik.dtb

define __sec_dtbo_build
dtbo-$(2) += $(1)
$(1)-base := $(3)
endef

define sec_dtbo_build
$(foreach dtbo, $(1), $(eval $(call __sec_dtbo_build, $(dtbo),$(2),$(3))))
endef

$(eval $(call sec_dtbo_build, \
                $(SEC_SYSTEM_UPDATE_DTBO),$(CONFIG_ARCH_YUPIK),$(SEC_YUPIK_BASE_DTB)))


always          := $(dtb-y)
subdir-y        := $(dts-dirs)

clean-files := *.dtb *.reverse.dts *.dtbo


