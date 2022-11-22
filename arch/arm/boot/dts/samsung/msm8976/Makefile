ifeq ($(CONFIG_OF),y)

ifeq ($(CONFIG_SEC_MSM8976LTE_PROJECT),y)
	ifeq ($(CONFIG_MACH_MSM8976LTE_EUR_OPEN),y)
		dtb-y += msm8976-sec-msm8976lte-eur-r00.dtb
	endif
endif
ifeq ($(CONFIG_SEC_MSM8976FHDLTE_PROJECT),y)
	ifeq ($(CONFIG_MACH_MSM8976FHDLTE_EUR_OPEN),y)
		dtb-y += msm8976-sec-msm8976fhdlte-eur-r00.dtb \
			 msm8976-sec-msm8976fhdlte-eur-r01.dtb
	endif
endif

ifeq ($(CONFIG_SEC_A9XLTE_PROJECT),y)
        ifeq ($(CONFIG_MACH_A9XLTE_CHN_OPEN),y)
                dtb-y += msm8976-sec-a9xlte-chn-r00.dtb
                dtb-y += msm8976-sec-a9xlte-chn-r01.dtb
                dtb-y += msm8976-sec-a9xlte-chn-r02.dtb
                dtb-y += msm8976-sec-a9xlte-chn-r03.dtb
                dtb-y += msm8976-sec-a9xlte-chn-r04.dtb
        endif
endif

ifeq ($(CONFIG_SEC_A9XPROLTE_PROJECT),y)
        ifeq ($(CONFIG_MACH_A9XPROLTE_CHN_OPEN),y)
                dtb-y += msm8976-sec-a9xprolte-chn-r00.dtb
                dtb-y += msm8976-sec-a9xprolte-chn-r01.dtb
        endif
		ifeq ($(CONFIG_MACH_A9XPROLTE_SEA_OPEN),y)
                dtb-y += msm8976-sec-a9xprolte-sea-r00.dtb
        endif
endif

ifeq ($(CONFIG_SEC_C9LTE_PROJECT),y)
        ifeq ($(CONFIG_MACH_C9LTE_CHN_OPEN),y)
                dtb-y += msm8976-sec-c9lte-chn-r00.dtb
                dtb-y += msm8976-sec-c9lte-chn-r01.dtb
        endif
endif

ifeq ($(CONFIG_SEC_GTS28VELTE_PROJECT),y)
        ifeq ($(CONFIG_MACH_GTS28VELTE_EUR_OPEN),y)
                dtb-y += msm8976-sec-gts28velte-eur-r00.dtb
		dtb-y += msm8976-sec-gts28velte-eur-r01.dtb
		dtb-y += msm8976-sec-gts28velte-eur-r02.dtb
		dtb-y += msm8976-sec-gts28velte-eur-r03.dtb
		dtb-y += msm8976-sec-gts28velte-eur-r04.dtb
	else ifeq ($(CONFIG_MACH_GTS28VELTE_CHN_OPEN),y)
		dtb-y += msm8976-sec-gts28velte-chn-r00.dtb
        endif
endif

ifeq ($(CONFIG_SEC_GTS210VELTE_PROJECT),y)
        ifeq ($(CONFIG_MACH_GTS210VELTE_EUR_OPEN),y)
                dtb-y += msm8976-sec-gts210velte-eur-r00.dtb
                dtb-y += msm8976-sec-gts210velte-eur-r01.dtb
                dtb-y += msm8976-sec-gts210velte-eur-r02.dtb
                dtb-y += msm8976-sec-gts210velte-eur-r04.dtb
        else ifeq ($(CONFIG_MACH_GTS210VELTE_USA_VZW),y)
                dtb-y += msm8976-sec-gts210velte-vzw-r04.dtb
                dtb-y += msm8976-sec-gts210velte-vzw-r07.dtb
                dtb-y += msm8976-sec-gts210velte-vzw-r08.dtb
        else ifeq ($(CONFIG_MACH_GTS210VELTE_SEA_OPEN),y)
                dtb-y += msm8976-sec-gts210velte-sea-r04.dtb
	else ifeq ($(CONFIG_MACH_GTS210VELTE_USA_TMO),y)
		dtb-y += msm8976-sec-gts210velte-tmo-r04.dtb
		dtb-y += msm8976-sec-gts210velte-tmo-r05.dtb
		dtb-y += msm8976-sec-gts210velte-tmo-r06.dtb
		dtb-y += msm8976-sec-gts210velte-tmo-r07.dtb
	else ifeq ($(CONFIG_MACH_GTS210VELTE_CAN_OPEN),y)
		dtb-y += msm8976-sec-gts210velte-can-r04.dtb
		dtb-y += msm8976-sec-gts210velte-can-r05.dtb
		dtb-y += msm8976-sec-gts210velte-can-r06.dtb
		dtb-y += msm8976-sec-gts210velte-can-r07.dtb
		dtb-y += msm8976-sec-gts210velte-can-r08.dtb
	else ifeq ($(CONFIG_MACH_GTS210VELTE_USA_ATT),y)
		dtb-y += msm8976-sec-gts210velte-att-r07.dtb
		dtb-y += msm8976-sec-gts210velte-att-r08.dtb
        endif
endif

ifeq ($(CONFIG_SEC_GTS28VEWIFI_PROJECT),y)
        ifeq ($(CONFIG_MACH_GTS28VEWIFI_EUR_OPEN),y)
                dtb-y += msm8976-sec-gts28vewifi-eur-r00.dtb
				dtb-y += msm8976-sec-gts28vewifi-eur-r01.dtb
				dtb-y += msm8976-sec-gts28vewifi-eur-r04.dtb
				dtb-y += msm8976-sec-gts28vewifi-eur-r05.dtb
        endif
endif

ifeq ($(CONFIG_SEC_GTS210VEWIFI_PROJECT),y)
        ifeq ($(CONFIG_MACH_GTS210VEWIFI_EUR_OPEN),y)
                dtb-y += msm8976-sec-gts210vewifi-eur-r01.dtb
                dtb-y += msm8976-sec-gts210vewifi-eur-r02.dtb
                dtb-y += msm8976-sec-gts210vewifi-eur-r04.dtb
        endif
endif

echo '*** Warning: Samsung $@ Device Tree MSM8976.'
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
