dtbo-y += m23xq_eur_open_w00_r04.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
m23xq_eur_open_w00_r04.dtbo-base := ../../../vendor/qcom/lagoon.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += m23xq_eur_open_w00_r03.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
m23xq_eur_open_w00_r03.dtbo-base := ../../../vendor/qcom/lagoon.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += m23xq_eur_open_w00_r01.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
m23xq_eur_open_w00_r01.dtbo-base := ../../../vendor/qcom/lagoon.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += m23xq_eur_open_w00_r00.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
m23xq_eur_open_w00_r00.dtbo-base := ../../../vendor/qcom/lagoon.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += m23xq_eur_open_w00_r00.dtbo
m23xq_eur_open_w00_r00.dtbo-base := ../../../vendor/qcom/lagoon.dtb

# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)

# SPU
SEC_SYSTEM_UPDATE_DTBO := ../../lagoon-sec-system-update-overlay.dtbo

define __sec_dtbo_build
dtbo-$(2) += $(1)
$(1)-base := $(3)
endef

define sec_dtbo_build
$(foreach dtbo, $(1), $(eval $(call __sec_dtbo_build, $(dtbo),$(2),$(3))))
endef

# LAGOON BASE DTB
SEC_LAGOON_BASE_DTB := ../../../vendor/qcom/lagoon.dtb

$(eval $(call sec_dtbo_build, \
		$(SEC_SYSTEM_UPDATE_DTBO),$(CONFIG_ARCH_LAGOON),$(SEC_LAGOON_BASE_DTB)))

always          := $(dtb-y)
subdir-y        := $(dts-dirs)
endif

clean-files := *.dtb *.reverse.dts *.dtbo

