TOUCH_DLKM_ENABLE := true
ifeq ($(TARGET_KERNEL_DLKM_DISABLE), true)
	ifeq ($(TARGET_KERNEL_DLKM_TOUCH_OVERRIDE), false)
		TOUCH_DLKM_ENABLE := false
	endif
endif

ifeq ($(TOUCH_DLKM_ENABLE),  true)
	ifneq ($(TARGET_BOARD_AUTO),true)
		ifeq ($(call is-board-platform-in-list,$(TARGET_BOARD_PLATFORM)),true)
			ifeq ($(TARGET_BOARD_PLATFORM), pineapple)
				ifneq ($(SEC_SAMSUNG_EXPERIENCE_PLATFORM_CATEGORY), sep_lite_new)
					BOARD_VENDOR_KERNEL_MODULES +=	$(KERNEL_MODULES_OUT)/sec.ko
				endif
				BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/nt36xxx-i2c.ko \
                    $(KERNEL_MODULES_OUT)/tp_info.ko \
                    $(KERNEL_MODULES_OUT)/lct_tp.ko \
					$(KERNEL_MODULES_OUT)/cmd.ko \
                    $(KERNEL_MODULES_OUT)/ilitek.ko \
					$(KERNEL_MODULES_OUT)/icnl9922c.ko \
					$(KERNEL_MODULES_OUT)/hx83112f.ko \
					$(KERNEL_MODULES_OUT)/goodix_ts.ko \
					$(KERNEL_MODULES_OUT)/atmel_mxt_ts.ko \
					$(KERNEL_MODULES_OUT)/synaptics_tcm_ts.ko
			else # pineapple
				ifneq ($(SEC_SAMSUNG_EXPERIENCE_PLATFORM_CATEGORY), sep_lite_new)
					BOARD_VENDOR_KERNEL_MODULES +=	$(KERNEL_MODULES_OUT)/sec.ko
				endif
				BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/nt36xxx-i2c.ko \
                    $(KERNEL_MODULES_OUT)/tp_info.ko \
                    $(KERNEL_MODULES_OUT)/lct_tp.ko \
					$(KERNEL_MODULES_OUT)/cmd.ko \
                    $(KERNEL_MODULES_OUT)/ilitek.ko \
					$(KERNEL_MODULES_OUT)/icnl9922c.ko \
					$(KERNEL_MODULES_OUT)/hx83112f.ko \
					$(KERNEL_MODULES_OUT)/goodix_ts.ko \
					$(KERNEL_MODULES_OUT)/atmel_mxt_ts.ko \
					$(KERNEL_MODULES_OUT)/synaptics_tcm_ts.ko \
					$(KERNEL_MODULES_OUT)/pt_ts.ko \
					$(KERNEL_MODULES_OUT)/pt_i2c.ko \
					$(KERNEL_MODULES_OUT)/pt_device_access.ko \
					$(KERNEL_MODULES_OUT)/raydium_ts.ko
			endif # pineapple
		endif
	endif
endif
