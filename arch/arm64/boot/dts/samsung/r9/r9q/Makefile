dtbo-y += r9q_eur_openx2_w00_r14.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
r9q_eur_openx2_w00_r14.dtbo-base := ../../../vendor/qcom/lahaina.dtb ../../../vendor/qcom/lahaina-v2.dtb ../../../vendor/qcom/lahaina-v2.1.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += r9q_eur_openx2_w00_r12.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
r9q_eur_openx2_w00_r12.dtbo-base := ../../../vendor/qcom/lahaina.dtb ../../../vendor/qcom/lahaina-v2.dtb ../../../vendor/qcom/lahaina-v2.1.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += r9q_eur_openx2_w00_r11.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
r9q_eur_openx2_w00_r11.dtbo-base := ../../../vendor/qcom/lahaina.dtb ../../../vendor/qcom/lahaina-v2.dtb ../../../vendor/qcom/lahaina-v2.1.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
# SPDX-License-Identifier: GPL-2.0

define __sec_dtbo_build
dtbo-$(2) += $(1)
$(1)-base := $(3)
endef

define sec_dtbo_build
$(foreach dtbo, $(1), $(eval $(call __sec_dtbo_build, $(dtbo),$(2),$(3))))
endef

# LAHAINA BASE DTB
QCOM_DTB := ../../../vendor/qcom
SEC_LAHAINA_BASE_DTB := $(QCOM_DTB)/lahaina.dtb $(QCOM_DTB)/lahaina-v2.dtb $(QCOM_DTB)/lahaina-v2.1.dtb

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
# TODO: Add system dtbo here
endif

clean-files := *.dtb *.reverse.dts *.dtbo

