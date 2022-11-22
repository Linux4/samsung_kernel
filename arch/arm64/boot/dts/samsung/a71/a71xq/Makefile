dtbo-y += a71xq_usa_singlew_w00_r11.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
a71xq_usa_singlew_w00_r11.dtbo-base := ../../../vendor/qcom/lito.dtb ../../../vendor/qcom/lito-v2.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += a71xq_usa_singlew_w00_r09.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
a71xq_usa_singlew_w00_r09.dtbo-base := ../../../vendor/qcom/lito.dtb ../../../vendor/qcom/lito-v2.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += a71xq_usa_singlew_w00_r06.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
a71xq_usa_singlew_w00_r06.dtbo-base := ../../../vendor/qcom/lito.dtb ../../../vendor/qcom/lito-v2.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += a71xq_usa_singlew_w00_r04.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
a71xq_usa_singlew_w00_r04.dtbo-base := ../../../vendor/qcom/lito.dtb ../../../vendor/qcom/lito-v2.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += a71xq_usa_singlew_w00_r02.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
a71xq_usa_singlew_w00_r02.dtbo-base := ../../../vendor/qcom/lito.dtb ../../../vendor/qcom/lito-v2.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)

# SPU
SEC_SYSTEM_UPDATE_DTBO := ../../lito-sec-system-update-overlay.dtbo

define __sec_dtbo_build
dtbo-$(2) += $(1)
$(1)-base := $(3)
endef

define sec_dtbo_build
$(foreach dtbo, $(1), $(eval $(call __sec_dtbo_build, $(dtbo),$(2),$(3))))
endef

# LITO BASE DTB
SEC_LITO_BASE_DTB := ../../../vendor/qcom/lito.dtb ../../../vendor/qcom/lito-v2.dtb

$(eval $(call sec_dtbo_build, \
		$(SEC_SYSTEM_UPDATE_DTBO),$(CONFIG_ARCH_LITO),$(SEC_LITO_BASE_DTB)))

always          := $(dtb-y)
subdir-y        := $(dts-dirs)
endif

clean-files := *.dtb *.reverse.dts *.dtbo

