dtbo-y += a52sxq_eur_open_w00_r02.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
a52sxq_eur_open_w00_r02.dtbo-base := ../../../vendor/qcom/yupik.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += a52sxq_eur_open_w00_r01.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
a52sxq_eur_open_w00_r01.dtbo-base := ../../../vendor/qcom/yupik.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += a52sxq_eur_open_w00_r00.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
a52sxq_eur_open_w00_r00.dtbo-base := ../../../vendor/qcom/yupik.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)

define __sec_dtbo_build
dtbo-$(2) += $(1)
$(1)-base := $(3)
endef

always          := $(dtb-y)
subdir-y        := $(dts-dirs)
endif

clean-files := *.dtb *.reverse.dts *.dtbo


