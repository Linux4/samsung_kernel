ifeq ($(CONFIG_OF),y)

ifeq ($(CONFIG_SEC_J7XLTE_PROJECT),y)
        ifeq ($(CONFIG_MACH_J7XLTE_CHN_CTC),y)
                dtb-y += msm8952-sec-j7xlte-chn-r00.dtb
        endif
endif

ifeq ($(CONFIG_SEC_C5LTE_PROJECT),y)
	ifeq ($(CONFIG_MACH_C5LTE_CHN_OPEN),y)
		dtb-y += msm8952-sec-c5lte-chn-r02.dtb
		dtb-y += msm8952-sec-c5lte-chn-r04.dtb
		dtb-y += msm8952-sec-c5lte-chn-r05.dtb
		dtb-y += msm8952-sec-c5lte-chn-r06.dtb
		dtb-y += msm8952-sec-c5lte-chn-r07.dtb
		dtb-y += msm8952-sec-c5lte-chn-r08.dtb
		dtb-y += msm8952-sec-c5lte-chn-r09.dtb
	endif
	ifeq ($(CONFIG_MACH_C5LTE_CHN_HK),y)
		dtb-y += msm8952-sec-c5lte-chn-hk-r09.dtb
	endif
endif

ifeq ($(CONFIG_SEC_ON5XLTE_PROJECT),y)
	ifeq ($(CONFIG_MACH_ON5XLTE_CHN_OPEN),y)
		dtb-y += msm8952-sec-on5xlte-chn-r00.dtb
		dtb-y += msm8952-sec-on5xlte-chn-r01.dtb
		dtb-y += msm8952-sec-on5xlte-chn-r02.dtb
		dtb-y += msm8952-sec-on5xlte-chn-r03.dtb
		dtb-y += msm8952-sec-on5xlte-chn-r04.dtb
	endif
endif

echo '*** Warning: Samsung $@ Device Tree MSM8952.'
DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif

targets += dtbs
targets += $(addprefix ../, $(DTB_LIST))
endif

$(obj)/../../%.dtb: $(src)/%.dts FORCE
	$(call if_changed_dep,dtc)

dtbs: $(addprefix $(obj)/../../,$(DTB_LIST))

clean-files := *.dtb *.reverse.dts
