dtbo-y += a23_eur_open_w00_r09.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
a23_eur_open_w00_r09.dtbo-base := ../../../vendor/qcom/khaje.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += a23_eur_open_w00_r08.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
a23_eur_open_w00_r08.dtbo-base := ../../../vendor/qcom/khaje.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += a23_eur_open_w00_r01.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
a23_eur_open_w00_r01.dtbo-base := ../../../vendor/qcom/khaje.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += a23_eur_open_w00_r00.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
a23_eur_open_w00_r00.dtbo-base := ../../../vendor/qcom/khaje.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)

# SPU
SEC_SYSTEM_UPDATE_DTBO := ../../khaje-sec-system-update-overlay.dtbo

define __sec_dtbo_build
dtbo-$(2) += $(1)
$(1)-base := $(3)
endef

define sec_dtbo_build
$(foreach dtbo, $(1), $(eval $(call __sec_dtbo_build, $(dtbo),$(2),$(3))))
endef

# KHAJE BASE DTB
SEC_KHAJE_BASE_DTB := ../../../vendor/qcom/khaje.dtb

$(eval $(call sec_dtbo_build, \
		$(SEC_SYSTEM_UPDATE_DTBO),$(CONFIG_ARCH_KHAJE),$(SEC_KHAJE_BASE_DTB)))

always          := $(dtb-y)
subdir-y        := $(dts-dirs)
endif

clean-files := *.dtb *.reverse.dts *.dtbo

