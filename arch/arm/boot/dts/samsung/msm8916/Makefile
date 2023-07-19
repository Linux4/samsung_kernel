ifeq ($(CONFIG_OF),y)

dtb-$(CONFIG_ARCH_MSM8974) += msm8974-v1-cdp.dtb \
	msm8974-v1-fluid.dtb \
	msm8974-v1-liquid.dtb \
	msm8974-v1-mtp.dtb \
	msm8974-v1-rumi.dtb \
	msm8974-v1-sim.dtb \
	msm8974-v2.0-1-cdp.dtb \
	msm8974-v2.0-1-fluid.dtb \
	msm8974-v2.0-1-liquid.dtb \
	msm8974-v2.0-1-mtp.dtb \
	apq8074-v2.0-1-cdp.dtb \
	apq8074-v2.0-1-liquid.dtb \
	apq8074-v2.0-1-dragonboard.dtb \
	apq8074-v2.2-cdp.dtb \
	apq8074-v2.2-liquid.dtb \
	apq8074-v2.2-dragonboard.dtb \
	msm8974-v2.2-cdp.dtb \
	msm8974-v2.2-fluid.dtb \
	msm8974-v2.2-liquid.dtb \
	msm8974-v2.2-mtp.dtb \
	msm8974pro-ab-pm8941-cdp.dtb \
	msm8974pro-ab-pm8941-fluid.dtb \
	msm8974pro-ab-pm8941-liquid.dtb \
	msm8974pro-ab-pm8941-mtp.dtb \
	msm8974pro-ac-pm8941-cdp.dtb \
	msm8974pro-ac-pm8941-liquid.dtb \
	msm8974pro-ac-pm8941-mtp.dtb \
	msm8974pro-ac-pma8084-pm8941-mtp.dtb
dtb-$(CONFIG_ARCH_MSM8909) += msm8909-sim.dtb \
	msm8909-rumi.dtb \
	msm8909-qrd-skua.dtb \
	msm8909-qrd-skuc.dtb \
	msm8909-qrd-skue.dtb \
	msm8909-cdp.dtb \
	msm8909-mtp.dtb \
	msm8909-mtp-smb1360.dtb \
	msm8909-1gb-qrd-skua.dtb \
	msm8909-1gb-cdp.dtb \
	msm8909-1gb-mtp.dtb

ifeq ($(CONFIG_SEC_ROSSA_PROJECT),y)
	ifeq ($(CONFIG_MACH_ROSSA_EUR_OPEN),y)
		dtb-y += msm8916-sec-rossa-eur-r00.dtb
		dtb-y += msm8916-sec-rossa-eur-r01.dtb
	else ifeq ($(CONFIG_MACH_ROSSA_TMO),y)
		dtb-y += msm8916-sec-rossa-tmo-r00.dtb
	else ifeq ($(CONFIG_MACH_ROSSA_LTN_DTV),y)
		dtb-y += msm8916-sec-rossa-ltn-dtv-r00.dtb
	else ifeq ($(CONFIG_MACH_ROSSA_VZW),y)
		dtb-y += msm8916-sec-rossa-vzw-r00.dtb
		dtb-y += msm8916-sec-rossa-vzw-r01.dtb
		dtb-y += msm8916-sec-rossa-vzw-r02.dtb
	endif
endif

ifeq ($(CONFIG_SEC_FORTUNA_PROJECT),y)
	ifeq ($(CONFIG_MACH_FORTUNA_EUR_OPEN),y)
		dtb-y += msm8916-sec-fortuna-eur-r00.dtb
		dtb-y += msm8916-sec-fortuna-eur-r01.dtb
		dtb-y += msm8916-sec-fortuna-eur-r02.dtb
	else ifeq ($(CONFIG_MACH_FORTUNA3G_EUR),y)
		dtb-y += msm8916-sec-fortuna-r00.dtb
		dtb-y += msm8916-sec-fortuna-r01.dtb
		dtb-y += msm8916-sec-fortuna-r03.dtb
		dtb-y += msm8916-sec-fortuna-r04.dtb
		dtb-y += msm8916-sec-fortuna-r06.dtb
		dtb-y += msm8916-sec-fortuna-r07.dtb
		dtb-y += msm8916-sec-fortuna-r09.dtb
		dtb-y += msm8916-sec-fortuna-r11.dtb
	else ifeq ($(CONFIG_MACH_FORTUNAVE3G_EUR),y)
		dtb-y += msm8916-sec-fortuna-ve3g-r07.dtb
	else ifeq ($(CONFIG_MACH_FORTUNA_AIO), y)
		dtb-y += msm8916-sec-fortuna-aio-r00.dtb
		dtb-y += msm8916-sec-fortuna-aio-r01.dtb
	endif
endif

ifeq ($(CONFIG_SEC_A3_EUR_PROJECT),y)
	dtb-y += msm8916-sec-a3-eur-r00.dtb
	dtb-y += msm8916-sec-a3-eur-r01.dtb
endif

ifeq ($(CONFIG_SEC_A3_PROJECT),y)
	dtb-y += msm8916-sec-a3-r00.dtb
	dtb-y += msm8916-sec-a3-r01.dtb
	dtb-y += msm8916-sec-a3-r03.dtb
endif

ifeq ($(CONFIG_SEC_A33G_EUR_PROJECT),y)
	ifeq ($(CONFIG_MACH_A33G_EUR_OPEN),y)
		dtb-y += msm8916-sec-a33g-eur-r00.dtb
		dtb-y += msm8916-sec-a33g-eur-r01.dtb
		dtb-y += msm8916-sec-a33g-eur-r03.dtb
		dtb-y += msm8916-sec-a33g-eur-r04.dtb
	endif
endif

ifeq ($(CONFIG_SEC_J1_PROJECT),y)
	ifeq ($(CONFIG_MACH_J1_VZW),y)
		dtb-y += msm8916-sec-j1vzw-r00.dtb
		dtb-y += msm8916-sec-j1vzw-r02.dtb
	endif
endif

ifeq ($(CONFIG_SEC_J5_PROJECT),y)
	ifeq ($(CONFIG_MACH_J5LTE_EUR_OPEN),y)
		dtb-y += msm8916-sec-j5lte-eur-r00.dtb
		dtb-y += msm8916-sec-j5lte-eur-r01.dtb
		dtb-y += msm8916-sec-j5lte-eur-r05.dtb
	else ifeq ($(CONFIG_MACH_J53G_EUR_OPEN),y)
		dtb-y += msm8916-sec-j53g-eur-r00.dtb
		dtb-y += msm8916-sec-j53g-eur-r01.dtb
		dtb-y += msm8916-sec-j53g-eur-r02.dtb
		dtb-y += msm8916-sec-j53g-eur-r05.dtb
	else ifeq ($(CONFIG_MACH_J5LTE_CHN_CMCC),y)
		dtb-y += msm8916-sec-j5lte-chn-r00.dtb
		dtb-y += msm8916-sec-j5lte-chn-r01.dtb
	else ifeq ($(CONFIG_MACH_J5LTE_KOR_OPEN),y)
		dtb-y += msm8916-sec-j5lte-kor-r01.dtb
		dtb-y += msm8916-sec-j5lte-kor-r05.dtb
	else ifeq ($(CONFIG_MACH_J5LTE_LTN_DTV),y)
		dtb-y += msm8916-sec-j5lte-ltn-dtv-r04.dtb
		dtb-y += msm8916-sec-j5lte-ltn-dtv-r05.dtb
	else ifeq ($(CONFIG_MACH_J5YLTE_SEA_NZC),y)
		dtb-y += msm8916-sec-j5ylte-sea-r00.dtb
		dtb-y += msm8916-sec-j5ylte-sea-r01.dtb
		dtb-y += msm8916-sec-j5ylte-sea-r05.dtb
	endif
endif

ifeq ($(CONFIG_SEC_J5N_PROJECT),y)
        ifeq ($(CONFIG_MACH_J5NLTE_EUR_OPEN),y)
		dtb-y += msm8916-sec-j5nlte-eur-r04.dtb
		dtb-y += msm8916-sec-j5nlte-eur-r05.dtb
	endif
endif

ifeq ($(CONFIG_SEC_NOVEL_PROJECT),y)
	ifeq ($(CONFIG_MACH_NOVEL_KOR_OPEN),y)
		dtb-y += msm8916-sec-novel-kx-r00.dtb
		dtb-y += msm8916-sec-novel-kx-r01.dtb
	endif
endif
dtb-$(CONFIG_ARCH_MSM8226) += msm8226-sim.dtb \
	msm8226-fluid.dtb \
	msm8226-v1-cdp.dtb \
	msm8226-v1-w-cdp.dtb \
	msm8226-v1-mtp.dtb \
	msm8226-v1-w-mtp.dtb \
	msm8226-v1-qrd-evt.dtb \
	msm8226-v1-qrd-dvt.dtb \
	msm8226-v2-720p-cdp.dtb \
	msm8226-v2-320p-w-cdp.dtb \
	msm8226-v2-1080p-cdp.dtb \
	msm8226-v2-720p-mtp.dtb \
	msm8226-v2-320p-w-mtp.dtb \
	msm8226-v2-1080p-mtp.dtb \
	msm8226-v2-qrd-evt.dtb \
	msm8226-v2-qrd-dvt.dtb \
	msm8926-v1-720p-cdp.dtb \
	msm8926-v1-1080p-cdp.dtb \
	msm8926-v1-720p-mtp.dtb \
	msm8926-v1-1080p-mtp.dtb \
	msm8926-v1-qrd.dtb \
	msm8926-v1-qrd-skug.dtb \
	msm8926-v1-qrd-skug-pvt.dtb \
	msm8926-v2-720p-cdp.dtb \
	msm8926-v2-1080p-cdp.dtb \
	msm8926-v2-1080p-ext-buck-cdp.dtb \
	msm8926-v2-720p-mtp.dtb \
	msm8926-v2-1080p-mtp.dtb \
	msm8926-v2-1080p-ext-buck-mtp.dtb \
	msm8926-v2-qrd.dtb \
	msm8926-v2-qrd-skug.dtb \
	msm8926-v2-qrd-skug-pvt.dtb \
	msm8226-v1-qrd-skuf.dtb \
	msm8226-v2-qrd-skuf.dtb \
	apq8026-v1-xpm.dtb \
	apq8026-v1-cdp.dtb \
	apq8026-v1-mtp.dtb \
	apq8026-v1-w-mtp.dtb \
	apq8026-v2-xpm.dtb \
	apq8026-v2-720p-cdp.dtb \
	apq8026-v2-320p-w-cdp.dtb \
	apq8026-v2-1080p-cdp.dtb \
	apq8026-v2-720p-mtp.dtb \
	apq8026-v2-320p-w-mtp.dtb \
	apq8026-v2-1080p-mtp.dtb
dtb-$(CONFIG_ARCH_MSM8610) += msm8610-rumi.dtb \
	msm8610-v1-cdp.dtb \
	msm8610-v2-cdp.dtb \
	msm8610-v1-mtp.dtb \
	msm8610-v2-mtp.dtb \
	msm8610-sim.dtb \
	msm8610-v1-qrd-skuaa.dtb \
	msm8610-v1-qrd-skuab.dtb \
	msm8610-v2-qrd-skuaa.dtb \
	msm8610-v2-qrd-skuab.dtb
dtb-$(CONFIG_ARCH_FSM9900) += fsm9900-rumi.dtb \
	fsm9900-sim.dtb \
	fsm9900-cdp.dtb \
	fsm9900-mtp-v1.0.dtb \
	fsm9900-mtp-v1.1.dtb
dtb-$(CONFIG_ARCH_FSM9010) += fsm9010-rumi.dtb
dtb-$(CONFIG_ARCH_MDM9630) += mdm9630-sim.dtb \
	mdm9630-v1-cdp.dtb \
	mdm9630-v1-mtp.dtb \
	mdm9630-v2-cdp.dtb \
	mdm9630-v2-mtp.dtb \
	mdm9630-v2-tsmc-cdp.dtb \
	mdm9630-v2-gf-cdp.dtb  \
	mdm9630-v2-tsmc-mtp.dtb \
	mdm9630-v2-gf-mtp.dtb
dtb-$(CONFIG_ARCH_MSMZIRC) += msmzirc-sim.dtb \
	msmzirc-rumi.dtb \
	msmzirc-cdp.dtb \
	msmzirc-mtp.dtb
dtb-$(CONFIG_ARCH_MDMFERRUM) += mdmferrum-sim.dtb \
	mdmferrum-rumi.dtb \
	mdmferrum-cdp.dtb \
	mdmferrum-mtp.dtb
dtb-$(CONFIG_ARCH_APQ8084) += apq8084-v1.0-old-pwr-grid-liquid.dtb \
	apq8084-v1.1-old-pwr-grid-liquid.dtb \
	apq8084-v1.1-liquid.dtb \
	apq8084-v1.0-old-pwr-grid-smb1357-cdp.dtb \
	apq8084-v1.1-old-pwr-grid-smb1357-cdp.dtb \
	apq8084-v1.1-smb1357-cdp.dtb \
	apq8084-v1.0-old-pwr-grid-smb1357-mtp.dtb \
	apq8084-v1.1-old-pwr-grid-smb1357-mtp.dtb \
	apq8084-v1.1-smb1357-mtp.dtb \
	apq8084-v1.0-old-pwr-grid-smb1359-mdm9x35-alt-mtp.dtb \
	apq8084-v1.1-old-pwr-grid-smb1359-mdm9x35-alt-mtp.dtb \
	apq8084-v1.1-smb1359-mdm9x35-alt-mtp.dtb \
	apq8084-old-pwr-grid-smb349-cdp.dtb \
	apq8084-old-pwr-grid-smb349-mtp.dtb \
	apq8084-v1.0-old-pwr-grid-smb1357-mdm9x35-alt-rcm.dtb \
	apq8084-v1.1-old-pwr-grid-smb1357-mdm9x35-alt-rcm.dtb \
	apq8084-v1.1-smb1357-mdm9x35-alt-rcm.dtb \
	apq8084-v1.0-old-pwr-grid-smb1357-mdm9x35-mtp.dtb \
	apq8084-v1.1-old-pwr-grid-smb1357-mdm9x35-mtp.dtb \
	apq8084-v1.1-smb1357-mdm9x35-mtp.dtb \
	apq8084-v1.0-old-pwr-grid-smb1357-mdm9x35-cdp.dtb \
	apq8084-v1.1-old-pwr-grid-smb1357-mdm9x35-cdp.dtb \
	apq8084-v1.1-smb1357-mdm9x35-cdp.dtb \
	apq8084-v1.0-old-pwr-grid-mdm9x35-alt-cdp.dtb \
	apq8084-v1.1-old-pwr-grid-mdm9x35-alt-cdp.dtb \
	apq8084-v1.1-mdm9x35-alt-cdp.dtb \
	apq8084-v1.1-sbc.dtb
dtb-$(CONFIG_ARCH_MSM8994) += msm8994-v1-sim.dtb \
	msm8994-rumi.dtb \
	msm8994-v1-cdp.dtb \
	msm8994-pmi8994-pm8004-v1-cdp.dtb \
	msm8994-v1-mtp.dtb \
	msm8994-pmi8994-pm8004-v1-mtp.dtb \
	msm8994-v1-liquid.dtb \
	msm8994-v1-fluid.dtb \
	apq8094-v1-cdp.dtb \
	apq8094-v1-mtp.dtb \
	apq8094-v1-liquid.dtb \
	apq8094-v1-fluid.dtb \
	apq8094-v1-dragonboard.dtb \
	msm8994-v2-sim.dtb \
	msm8994-v2-cdp.dtb \
	msm8994-pmi8994-pm8004-v2-cdp.dtb \
	msm8994-v2-mtp.dtb \
	msm8994-pmi8994-pm8004-v2-mtp.dtb \
	msm8994-v2-liquid.dtb \
	msm8994-v2-fluid.dtb \
	apq8094-v2-cdp.dtb \
	apq8094-v2-mtp.dtb \
	apq8094-v2-liquid.dtb \
	apq8094-v2-fluid.dtb

dtb-$(CONFIG_ARCH_MSM8992) += msm8992-sim.dtb \
	msm8992-rumi.dtb

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif

targets += dtbs
targets += $(addprefix ../, $(DTB_LIST))
endif

# Samsung modify 2 depth for msm8916,msm8939
$(obj)/../../%.dtb: $(src)/%.dts FORCE
	$(call if_changed_dep,dtc)

# Samsung modify 2 depth for msm8916,msm8939
dtbs: $(addprefix $(obj)/../../,$(DTB_LIST))

clean-files := *.dtb
