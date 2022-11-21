ifeq ($(filter y,$(CONFIG_SEC_BEYOND0QLTE_PROJECT) $(CONFIG_SEC_BEYOND1QLTE_PROJECT) $(CONFIG_SEC_BEYOND2QLTE_PROJECT) $(CONFIG_SEC_BEYONDXQ_PROJECT)),y)
else ifeq ($(filter y,$(CONFIG_SEC_D1Q_PROJECT) $(CONFIG_SEC_D2Q_PROJECT) $(CONFIG_SEC_D2XQ_PROJECT) $(CONFIG_SEC_D2XQ2_PROJECT)),y)
else ifeq ($(filter y,$(CONFIG_SEC_WINNERLTE_PROJECT) $(CONFIG_SEC_WINNERX_PROJECT)),y)
else ifeq ($(filter y,$(CONFIG_SEC_GTS6L_PROJECT) $(CONFIG_SEC_GTS6LWIFI_PROJECT) $(CONFIG_SEC_GTS6X_PROJECT)),y)
else ifeq ($(filter y,$(CONFIG_SEC_BLOOMQ_PROJECT)),y)
else ifeq ($(filter y,$(CONFIG_SEC_BOLTQ5G_PROJECT)),y)
else ifeq ($(filter y,$(CONFIG_SEC_R5Q_PROJECT) $(CONFIG_SEC_R3Q_PROJECT)),y)
else ifeq ($(filter y,$(CONFIG_SEC_ZODIAC_PROJECT)),y)
else ifeq ($(filter y,$(CONFIG_SEC_A82XQ_PROJECT)),y)
endif

ifeq ($(CONFIG_ARM64),y)
always          := $(dtb-y)
subdir-y        := $(dts-dirs)
else
targets += dtbs
targets += $(addprefix ../, $(dtb-y))

$(obj)/../%.dtb: $(src)/%.dts FORCE
        $(call if_changed_dep,dtc)

dtbs: $(addprefix $(obj)/../,$(dtb-y))
endif
clean-files := *.dtb *.reverse.dts *.dtbo

