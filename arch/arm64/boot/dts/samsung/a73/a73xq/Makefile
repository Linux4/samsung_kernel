dtbo-y += a73xq_eur_open_w00_r02.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
a73xq_eur_open_w00_r02.dtbo-base := ../../../vendor/qcom/yupik.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += a73xq_eur_open_w00_r00.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
a73xq_eur_open_w00_r00.dtbo-base := ../../../vendor/qcom/yupik.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)

define __sec_dtbo_build
dtbo-$(2) += $(1)
$(1)-base := $(3)
endef

define sec_dtbo_build
$(foreach dtbo, $(1), $(eval $(call __sec_dtbo_build, $(dtbo),$(2),$(3))))
endef

# YUPIK BASE DTB
SEC_YUPIK_BASE_DTB := ../../../vendor/qcom/yupik.dtb

always          := $(dtb-y)
subdir-y        := $(dts-dirs)
endif

clean-files := *.dtb *.reverse.dts *.dtbo


