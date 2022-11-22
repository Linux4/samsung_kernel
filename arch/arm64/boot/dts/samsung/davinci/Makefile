# Path of Qcom Device tree
QCOM_DTB := ../../qcom

dtbo-$(CONFIG_SEC_D2Q_PROJECT) = sm8150-sec-d2q-overlay-r00.dtbo	\
					sm8150-sec-d2q-overlay-r01.dtbo	\
					sm8150-sec-d2q-overlay-r02.dtbo	\
					sm8150-sec-d2q-overlay-r03.dtbo	\
					sm8150-sec-d2q-overlay-r04.dtbo	\
					sm8150-sec-d2q-overlay-r06.dtbo	\
					sm8150-sec-d2q-overlay-r07.dtbo	\
					sm8150-sec-d2q-overlay-r08.dtbo	\
					sm8150-sec-d2q-overlay-r09.dtbo	\
					sm8150-sec-d2q-overlay-r10.dtbo	\
					sm8150-sec-d2q-overlay-r11.dtbo	\
					sm8150-sec-d2q-overlay-r12.dtbo	\
					sm8150-sec-d2q-overlay-r13.dtbo	\
					sm8150-sec-d2q-overlay-r14.dtbo

dtbo-$(CONFIG_SEC_D1Q_PROJECT) = sm8150-sec-d1q-overlay-r03.dtbo	\
					sm8150-sec-d1q-overlay-r04.dtbo	\
					sm8150-sec-d1q-overlay-r05.dtbo	\
					sm8150-sec-d1q-overlay-r06.dtbo	\
					sm8150-sec-d1q-overlay-r07.dtbo	\
					sm8150-sec-d1q-overlay-r08.dtbo

ifeq ($(filter y,$(CONFIG_MACH_D1Q_CHNZC) $(CONFIG_MACH_D1Q_CHNZH) $(CONFIG_MACH_D1Q_CHN_LDU)),y)
dtbo-y = sm8150-sec-d1q-chn-overlay-r03.dtbo \
	sm8150-sec-d1q-chn-overlay-r04.dtbo \
	sm8150-sec-d1q-chn-overlay-r05.dtbo \
	sm8150-sec-d1q-chn-overlay-r06.dtbo \
	sm8150-sec-d1q-chn-overlay-r07.dtbo \
	sm8150-sec-d1q-chn-overlay-r08.dtbo
endif

ifeq ($(filter y,$(CONFIG_MACH_D2Q_CHNZC) $(CONFIG_MACH_D2Q_CHNZH) $(CONFIG_MACH_D2Q_CHNZM) $(CONFIG_MACH_D2Q_CHN_LDU)),y)
dtbo-y = sm8150-sec-d2q-chn-overlay-r00.dtbo \
	sm8150-sec-d2q-chn-overlay-r01.dtbo \
	sm8150-sec-d2q-chn-overlay-r02.dtbo \
	sm8150-sec-d2q-chn-overlay-r03.dtbo \
	sm8150-sec-d2q-chn-overlay-r04.dtbo \
	sm8150-sec-d2q-chn-overlay-r06.dtbo \
	sm8150-sec-d2q-chn-overlay-r07.dtbo \
	sm8150-sec-d2q-chn-overlay-r08.dtbo \
	sm8150-sec-d2q-chn-overlay-r09.dtbo \
	sm8150-sec-d2q-chn-overlay-r10.dtbo \
	sm8150-sec-d2q-chn-overlay-r11.dtbo \
	sm8150-sec-d2q-chn-overlay-r12.dtbo \
	sm8150-sec-d2q-chn-overlay-r13.dtbo \
	sm8150-sec-d2q-chn-overlay-r14.dtbo
endif

ifeq ($(filter y,$(CONFIG_MACH_D2Q_JPN_DCM) $(CONFIG_MACH_D2Q_JPN_KDI) $(CONFIG_MACH_D2Q_JPN_RKT)),y)
dtbo-y = sm8150-sec-d2q-jpn-overlay-r01.dtbo \
	sm8150-sec-d2q-jpn-overlay-r02.dtbo \
	sm8150-sec-d2q-jpn-overlay-r03.dtbo \
	sm8150-sec-d2q-jpn-overlay-r04.dtbo \
	sm8150-sec-d2q-jpn-overlay-r06.dtbo \
	sm8150-sec-d2q-jpn-overlay-r07.dtbo \
	sm8150-sec-d2q-jpn-overlay-r08.dtbo \
	sm8150-sec-d2q-jpn-overlay-r09.dtbo \
	sm8150-sec-d2q-jpn-overlay-r10.dtbo \
	sm8150-sec-d2q-jpn-overlay-r11.dtbo \
	sm8150-sec-d2q-jpn-overlay-r12.dtbo \
	sm8150-sec-d2q-jpn-overlay-r13.dtbo \
	sm8150-sec-d2q-jpn-overlay-r14.dtbo
endif

dtbo-$(CONFIG_SEC_D2XQ_PROJECT) = sm8150-sec-d2xq-overlay-r00.dtbo		\
					sm8150-sec-d2xq-overlay-r01.dtbo	\
					sm8150-sec-d2xq-overlay-r02.dtbo	\
					sm8150-sec-d2xq-overlay-r03.dtbo	\
					sm8150-sec-d2xq-overlay-r04.dtbo	\
					sm8150-sec-d2xq-overlay-r06.dtbo	\
					sm8150-sec-d2xq-overlay-r07.dtbo	\
					sm8150-sec-d2xq-overlay-r08.dtbo	\
					sm8150-sec-d2xq-overlay-r09.dtbo	\
					sm8150-sec-d2xq-overlay-r10.dtbo	\
					sm8150-sec-d2xq-overlay-r11.dtbo	\
					sm8150-sec-d2xq-overlay-r12.dtbo	\
					sm8150-sec-d2xq-overlay-r13.dtbo	\
					sm8150-sec-d2xq-overlay-r14.dtbo

ifeq ($(filter y,$(CONFIG_MACH_D2XQ_CHN_OPEN) $(CONFIG_MACH_D2XQ_CHN_LDU)),y)
dtbo-y = sm8150-sec-d2xq-chn-overlay-r00.dtbo \
	sm8150-sec-d2xq-chn-overlay-r01.dtbo \
	sm8150-sec-d2xq-chn-overlay-r02.dtbo \
	sm8150-sec-d2xq-chn-overlay-r03.dtbo \
	sm8150-sec-d2xq-chn-overlay-r04.dtbo \
	sm8150-sec-d2xq-chn-overlay-r06.dtbo \
	sm8150-sec-d2xq-chn-overlay-r07.dtbo \
	sm8150-sec-d2xq-chn-overlay-r08.dtbo \
	sm8150-sec-d2xq-chn-overlay-r09.dtbo \
	sm8150-sec-d2xq-chn-overlay-r10.dtbo \
	sm8150-sec-d2xq-chn-overlay-r11.dtbo \
	sm8150-sec-d2xq-chn-overlay-r12.dtbo \
	sm8150-sec-d2xq-chn-overlay-r13.dtbo \
	sm8150-sec-d2xq-chn-overlay-r14.dtbo
endif

ifeq ($(filter y,$(CONFIG_MACH_D2XQ_MEA_OPEN)),y)
dtbo-y = sm8150-sec-d2xq-overlay-r00.dtbo \
	sm8150-sec-d2xq-overlay-r01.dtbo \
	sm8150-sec-d2xq-overlay-r02.dtbo \
	sm8150-sec-d2xq-overlay-r03.dtbo \
	sm8150-sec-d2xq-overlay-r04.dtbo \
	sm8150-sec-d2xq-overlay-r06.dtbo \
	sm8150-sec-d2xq-overlay-r07.dtbo \
	sm8150-sec-d2xq-overlay-r08.dtbo \
	sm8150-sec-d2xq-overlay-r09.dtbo \
	sm8150-sec-d2xq-overlay-r10.dtbo \
	sm8150-sec-d2xq-overlay-r11.dtbo \
	sm8150-sec-d2xq-overlay-r12.dtbo \
	sm8150-sec-d2xq-overlay-r13.dtbo \
	sm8150-sec-d2xq-mea-overlay-r14.dtbo
endif

dtbo-$(CONFIG_MACH_D2XQ2_USA_SINGLE) = sm8150p-sec-d2xq2-overlay-r00.dtbo \
	sm8150p-sec-d2xq2-overlay-r09.dtbo \
	sm8150p-sec-d2xq2-overlay-r10.dtbo \
	sm8150p-sec-d2xq2-overlay-r11.dtbo

dtbo-y += sm8150-sec-system-update-overlay.dtbo


sm8150-sec-d1q-overlay-r03.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d1q-overlay-r04.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d1q-overlay-r05.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d1q-overlay-r06.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d1q-overlay-r07.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d1q-overlay-r08.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2xq-overlay-r00.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2xq-overlay-r01.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2xq-overlay-r02.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2xq-overlay-r03.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2xq-overlay-r04.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2xq-overlay-r06.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2xq-overlay-r07.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2xq-overlay-r08.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2xq-overlay-r09.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2xq-overlay-r10.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2xq-overlay-r11.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2xq-overlay-r12.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2xq-overlay-r13.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2xq-overlay-r14.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2xq-mea-overlay-r14.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-overlay-r00.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-overlay-r01.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-overlay-r02.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-overlay-r03.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-overlay-r04.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-overlay-r06.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-overlay-r07.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-overlay-r08.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-overlay-r09.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-overlay-r10.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-overlay-r11.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-overlay-r12.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-overlay-r13.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-overlay-r14.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-chn-overlay-r01.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-chn-overlay-r02.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-chn-overlay-r03.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-chn-overlay-r04.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-chn-overlay-r05.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-chn-overlay-r06.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-chn-overlay-r07.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-chn-overlay-r08.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-chn-overlay-r09.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-chn-overlay-r10.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-chn-overlay-r11.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-chn-overlay-r12.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-chn-overlay-r13.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-chn-overlay-r14.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-jpn-overlay-r01.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-jpn-overlay-r02.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-jpn-overlay-r03.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-jpn-overlay-r04.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-jpn-overlay-r05.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-jpn-overlay-r06.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-jpn-overlay-r07.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-jpn-overlay-r08.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-jpn-overlay-r09.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-jpn-overlay-r10.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-jpn-overlay-r11.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-jpn-overlay-r12.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-jpn-overlay-r13.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-d2q-jpn-overlay-r14.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150p-sec-d2xq2-overlay-r00.dtbo-base := $(QCOM_DTB)/sm8150p.dtb $(QCOM_DTB)/sm8150p-v2.dtb
sm8150p-sec-d2xq2-overlay-r09.dtbo-base := $(QCOM_DTB)/sm8150p.dtb $(QCOM_DTB)/sm8150p-v2.dtb
sm8150p-sec-d2xq2-overlay-r10.dtbo-base := $(QCOM_DTB)/sm8150p.dtb $(QCOM_DTB)/sm8150p-v2.dtb
sm8150p-sec-d2xq2-overlay-r11.dtbo-base := $(QCOM_DTB)/sm8150p.dtb $(QCOM_DTB)/sm8150p-v2.dtb
ifeq ($(CONFIG_MACH_D2XQ2_USA_SINGLE),y)
sm8150-sec-system-update-overlay.dtbo-base := $(QCOM_DTB)/sm8150p.dtb $(QCOM_DTB)/sm8150p-v2.dtb
else
sm8150-sec-system-update-overlay.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
endif

always      := $(dtbo-y)
subdir-y    := $(dts-dirs)
clean-files := *.dtbo
