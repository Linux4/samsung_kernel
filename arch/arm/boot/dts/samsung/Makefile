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
dtb-$(CONFIG_SEC_KLEOS_PROJECT) += msm8916-sec-kleos-r00.dtb \
	msm8916-sec-kleos-r01.dtb \
	msm8916-sec-kleos-r02.dtb \
	msm8916-sec-kleos-r03.dtb \
	msm8916-sec-kleos-r04.dtb \
	msm8916-sec-kleos-r05.dtb \
	msm8916-sec-kleos-r07.dtb \
	msm8916-sec-kleos-r08.dtb
dtb-$(CONFIG_SEC_A3_PROJECT) += msm8916-sec-a3-r00.dtb \
	msm8916-sec-a3-r01.dtb \
	msm8916-sec-a3-r03.dtb
dtb-$(CONFIG_SEC_A3_EUR_PROJECT) += msm8916-sec-a3-eur-r00.dtb \
	msm8916-sec-a3-eur-r01.dtb
dtb-$(CONFIG_SEC_A33G_EUR_PROJECT) += msm8916-sec-a33g-eur-r00.dtb \
	msm8916-sec-a33g-eur-r01.dtb
ifeq ($(CONFIG_SEC_ROSSA_PROJECT),y)
	ifeq ($(CONFIG_MACH_ROSSA_SPR),y)
		dtb-y += msm8916-sec-rossaspr-r00.dtb
		dtb-y += msm8916-sec-rossaspr-r01.dtb
	else ifeq ($(CONFIG_MACH_ROSSA_TFN),y)
		dtb-y += msm8916-sec-rossaspr-r00.dtb
		dtb-y += msm8916-sec-rossaspr-r01.dtb
	else ifeq ($(CONFIG_MACH_ROSSA_VZW),y)
		dtb-y += msm8916-sec-rossaspr-r00.dtb
		dtb-y += msm8916-sec-rossaspr-r01.dtb
	else ifeq ($(CONFIG_MACH_ROSSA_AUS),y)
		dtb-y += msm8916-sec-rossa-aus-r02.dtb \
			msm8916-sec-rossa-aus-r03.dtb
	else ifeq ($(CONFIG_MACH_ROSSA_AIO),y)
		dtb-y += msm8916-sec-rossa-aio-r00.dtb
	else
		dtb-y += msm8916-sec-rossa-r00.dtb \
			msm8916-sec-rossa-r01.dtb \
			msm8916-sec-rossa-r02.dtb \
			msm8916-sec-rossa-r03.dtb \
			msm8916-sec-rossa-r04.dtb
	endif
endif
ifeq ($(CONFIG_SEC_FORTUNA_PROJECT),y)
	ifeq ($(CONFIG_MACH_FORTUNA_SEA_OPEN),y)
		dtb-y += msm8916-sec-fortuna-sea-r07.dtb \
			msm8916-sec-fortuna-sea-r08.dtb \
			msm8916-sec-fortuna-sea-r09.dtb
	else ifeq ($(CONFIG_MACH_FORTUNA3G_LTN_DTV),y)
		dtb-y += msm8916-sec-fortunadtv-r00.dtb \
			msm8916-sec-fortunadtv-r01.dtb
	else
		dtb-y += msm8916-sec-fortuna-r00.dtb \
			msm8916-sec-fortuna-r01.dtb \
			msm8916-sec-fortuna-r03.dtb \
			msm8916-sec-fortuna-r04.dtb \
			msm8916-sec-fortuna-r06.dtb \
			msm8916-sec-fortuna-r07.dtb \
			msm8916-sec-fortuna-r09.dtb \
			msm8916-sec-fortuna-r11.dtb
		ifeq ($(CONFIG_MACH_FORTUNA_CMCC),y)
			dtb-y += msm8916-sec-fortuna-r10.dtb
		else ifeq ($(CONFIG_MACH_FORTUNA_CHN_ZT),y)
			dtb-y += msm8916-sec-fortuna-r10.dtb
		endif
	endif
endif
dtb-$(CONFIG_SEC_O1_PROJECT) += msm8916-sec-o1-r00.dtb
dtb-$(CONFIG_SEC_GPEN_PROJECT) += msm8916-sec-gpen-r00.dtb
dtb-$(CONFIG_SEC_VIVALTO_PROJECT) += msm8916-sec-vivalto-r00.dtb
dtb-$(CONFIG_SEC_HEAT_PROJECT) += msm8916-sec-heat-r00.dtb \
	msm8916-sec-heat-r02.dtb
dtb-$(CONFIG_ARCH_MSM8226) += msm8226-sim.dtb \
	msm8226-fluid.dtb \
	msm8226-v1-cdp.dtb \
	msm8226-v1-mtp.dtb \
	msm8226-v1-qrd-evt.dtb \
	msm8226-v1-qrd-dvt.dtb \
	msm8226-v2-720p-cdp.dtb \
	msm8226-v2-1080p-cdp.dtb \
	msm8226-v2-720p-mtp.dtb \
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
	apq8026-v2-xpm.dtb \
	apq8026-v2-720p-cdp.dtb \
	apq8026-v2-1080p-cdp.dtb \
	apq8026-v2-720p-mtp.dtb \
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
	fsm9900-mtp.dtb
dtb-$(CONFIG_ARCH_MDM9630) += mdm9630-sim.dtb \
	mdm9630-cdp.dtb \
	mdm9630-mtp.dtb
dtb-$(CONFIG_ARCH_MSMZIRC) += msmzirc-sim.dtb
dtb-$(CONFIG_ARCH_MSMSAMARIUM) += msmsamarium-sim.dtb \
	msmsamarium-rumi.dtb \
	msmsamarium-cdp-interposer.dtb \
	msmsamarium-cdp.dtb \
	msmsamarium-mtp.dtb \
	apqsamarium-mtp.dtb
dtb-$(CONFIG_ARCH_MPQ8092) += mpq8092-sim.dtb \
	mpq8092-rumi.dtb \
	mpq8092-cdp.dtb \
	mpq8092-dtv.dtb \
	mpq8092-dma.dtb
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

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif

targets += dtbs
endif

$(obj)/../%.dtb: $(src)/%.dts
	$(call if_changed_dep,dtc)

dtbs: $(addprefix $(obj)/../,$(DTB_LIST))

clean-files := *.dtb
