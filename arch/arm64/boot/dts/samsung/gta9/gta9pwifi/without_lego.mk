# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)

define __sec_dtbo_build
dtbo-$(2) += $(1)
$(1)-base := $(3)
endef

define sec_dtbo_build
$(foreach dtbo, $(1), $(eval $(call __sec_dtbo_build, $(dtbo),$(2),$(3))))
endef

# BLAIR BASE DTB
QCOM_DTB := ../../../vendor/qcom
SEC_BLAIR_BASE_DTB := $(QCOM_DTB)/blair.dtb

dtstree		:= $(srctree)/$(src)
# GTA9P
SEC_GTA9P_DTBO := $(patsubst $(dtstree)/%.dts,%.dtbo, $(wildcard $(dtstree)/*.dts))

$(eval $(call sec_dtbo_build, \
	$(SEC_GTA9P_DTBO),$(CONFIG_ARCH_HOLI),$(SEC_BLAIR_BASE_DTB)))
	
endif

clean-files := *.dtb *.reverse.dts *.dtbo
