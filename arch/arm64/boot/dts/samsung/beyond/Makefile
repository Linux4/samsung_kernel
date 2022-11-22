# Path of Qcom Device tree
QCOM_DTB := ../../qcom

dtbo-$(CONFIG_SEC_BEYOND0QLTE_PROJECT) = sm8150-sec-beyond0qlte-overlay-r07.dtbo \
			sm8150-sec-beyond0qlte-overlay-r08.dtbo \
			sm8150-sec-beyond0qlte-overlay-r09.dtbo	\
			sm8150-sec-beyond0qlte-overlay-r12.dtbo	\
			sm8150-sec-beyond0qlte-overlay-r13.dtbo	\
			sm8150-sec-beyond0qlte-overlay-r14.dtbo	\
			sm8150-sec-beyond0qlte-overlay-r16.dtbo	\
			sm8150-sec-beyond0qlte-overlay-r17.dtbo
dtbo-$(CONFIG_SEC_BEYOND1QLTE_PROJECT) = sm8150-sec-beyond1qlte-overlay-r07.dtbo \
			sm8150-sec-beyond1qlte-overlay-r08.dtbo \
			sm8150-sec-beyond1qlte-overlay-r09.dtbo \
			sm8150-sec-beyond1qlte-overlay-r11.dtbo	\
			sm8150-sec-beyond1qlte-overlay-r12.dtbo	\
			sm8150-sec-beyond1qlte-overlay-r13.dtbo	\
			sm8150-sec-beyond1qlte-overlay-r14.dtbo	\
			sm8150-sec-beyond1qlte-overlay-r16.dtbo	\
			sm8150-sec-beyond1qlte-overlay-r17.dtbo
dtbo-$(CONFIG_SEC_BEYOND2QLTE_PROJECT) = sm8150-sec-beyond2qlte-overlay-r07.dtbo \
			sm8150-sec-beyond2qlte-overlay-r08.dtbo \
			sm8150-sec-beyond2qlte-overlay-r09.dtbo \
			sm8150-sec-beyond2qlte-overlay-r11.dtbo	\
			sm8150-sec-beyond2qlte-overlay-r12.dtbo	\
			sm8150-sec-beyond2qlte-overlay-r13.dtbo	\
			sm8150-sec-beyond2qlte-overlay-r14.dtbo	\
			sm8150-sec-beyond2qlte-overlay-r16.dtbo	\
			sm8150-sec-beyond2qlte-overlay-r17.dtbo
dtbo-$(CONFIG_SEC_BEYONDXQ_PROJECT) = sm8150-sec-beyondxq-overlay-r04.dtbo	\
			sm8150-sec-beyondxq-overlay-r05.dtbo	\
			sm8150-sec-beyondxq-overlay-r09.dtbo	\
			sm8150-sec-beyondxq-overlay-r10.dtbo	\
			sm8150-sec-beyondxq-overlay-r12.dtbo	\
			sm8150-sec-beyondxq-overlay-r14.dtbo

ifeq ($(filter y,$(CONFIG_MACH_BEYOND1QLTE_JPN_DCM) $(CONFIG_MACH_BEYOND1QLTE_JPN_KDI) $(CONFIG_MACH_BEYOND1QLTE_JPN_RKT)),y)
dtbo-y = sm8150-sec-beyond1qlte-jpn-overlay-r08.dtbo \
	sm8150-sec-beyond1qlte-jpn-overlay-r13.dtbo \
	sm8150-sec-beyond1qlte-jpn-overlay-r16.dtbo \
	sm8150-sec-beyond1qlte-jpn-overlay-r17.dtbo
endif
ifeq ($(filter y,$(CONFIG_MACH_BEYOND1QLTE_JPN_DCM) $(CONFIG_MACH_BEYOND1QLTE_JPN_KDI)),y)
dtbo-y = sm8150-sec-beyond1qlte-jpn-overlay-r08.dtbo \
	sm8150-sec-beyond1qlte-jpn-overlay-r13.dtbo \
	sm8150-sec-beyond1qlte-jpn-overlay-r16.dtbo \
	sm8150-sec-beyond1qlte-jpn-overlay-r17.dtbo
endif

ifeq ($(filter y,$(CONFIG_MACH_BEYOND2QLTE_JPN_DCM) $(CONFIG_MACH_BEYOND2QLTE_JPN_KDI) $(CONFIG_MACH_BEYOND2QLTE_JPN_DCMOLY)),y)
dtbo-y = sm8150-sec-beyond2qlte-jpn-overlay-r09.dtbo \
	sm8150-sec-beyond2qlte-jpn-overlay-r13.dtbo \
	sm8150-sec-beyond2qlte-jpn-overlay-r16.dtbo \
	sm8150-sec-beyond2qlte-jpn-overlay-r17.dtbo
endif

ifeq ($(filter y,$(CONFIG_MACH_BEYOND0QLTE_CHNZC) $(CONFIG_MACH_BEYOND0QLTE_CHNZM) $(CONFIG_MACH_BEYOND0QLTE_CHN_LDU)),y)
dtbo-y = sm8150-sec-beyond0qlte-overlay-r07.dtbo \
	sm8150-sec-beyond0qlte-overlay-r08.dtbo \
	sm8150-sec-beyond0qlte-overlay-r09.dtbo	\
	sm8150-sec-beyond0qlte-chn-overlay-r12.dtbo \
	sm8150-sec-beyond0qlte-chn-overlay-r13.dtbo \
	sm8150-sec-beyond0qlte-chn-overlay-r14.dtbo \
	sm8150-sec-beyond0qlte-chn-overlay-r16.dtbo \
	sm8150-sec-beyond0qlte-chn-overlay-r17.dtbo
endif

dtbo-$(CONFIG_MACH_BEYOND0QLTE_CHNZH) = sm8150-sec-beyond0qlte-chnhk-overlay-r07.dtbo \
	sm8150-sec-beyond0qlte-chnhk-overlay-r08.dtbo \
	sm8150-sec-beyond0qlte-chnhk-overlay-r09.dtbo \
	sm8150-sec-beyond0qlte-chnhk-overlay-r12.dtbo \
	sm8150-sec-beyond0qlte-chnhk-overlay-r13.dtbo \
	sm8150-sec-beyond0qlte-chnhk-overlay-r14.dtbo \
	sm8150-sec-beyond0qlte-chnhk-overlay-r16.dtbo \
	sm8150-sec-beyond0qlte-chnhk-overlay-r17.dtbo

dtbo-$(CONFIG_MACH_BEYOND1QLTE_CHNZH) = sm8150-sec-beyond1qlte-chnhk-overlay-r07.dtbo \
	sm8150-sec-beyond1qlte-chnhk-overlay-r08.dtbo \
	sm8150-sec-beyond1qlte-chnhk-overlay-r09.dtbo \
	sm8150-sec-beyond1qlte-chnhk-overlay-r11.dtbo \
	sm8150-sec-beyond1qlte-chnhk-overlay-r12.dtbo \
	sm8150-sec-beyond1qlte-chnhk-overlay-r13.dtbo \
	sm8150-sec-beyond1qlte-chnhk-overlay-r14.dtbo \
	sm8150-sec-beyond1qlte-chnhk-overlay-r16.dtbo \
	sm8150-sec-beyond1qlte-chnhk-overlay-r17.dtbo

dtbo-$(CONFIG_MACH_BEYOND2QLTE_CHNZH) = sm8150-sec-beyond2qlte-chnhk-overlay-r07.dtbo \
	sm8150-sec-beyond2qlte-chnhk-overlay-r08.dtbo \
	sm8150-sec-beyond2qlte-chnhk-overlay-r09.dtbo \
	sm8150-sec-beyond2qlte-chnhk-overlay-r11.dtbo \
	sm8150-sec-beyond2qlte-chnhk-overlay-r12.dtbo \
	sm8150-sec-beyond2qlte-chnhk-overlay-r13.dtbo \
	sm8150-sec-beyond2qlte-chnhk-overlay-r14.dtbo \
	sm8150-sec-beyond2qlte-chnhk-overlay-r16.dtbo \
	sm8150-sec-beyond2qlte-chnhk-overlay-r17.dtbo

dtbo-$(CONFIG_MACH_BEYONDXQ_CHN_OPEN) = sm8150-sec-beyondxq-chn-overlay-r04.dtbo \
	sm8150-sec-beyondxq-chn-overlay-r05.dtbo \
	sm8150-sec-beyondxq-chn-overlay-r09.dtbo \
	sm8150-sec-beyondxq-chn-overlay-r10.dtbo \
	sm8150-sec-beyondxq-chn-overlay-r12.dtbo \
	sm8150-sec-beyondxq-chn-overlay-r14.dtbo

sm8150-sec-beyond0qlte-overlay-r07.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-overlay-r08.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-overlay-r09.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-overlay-r12.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-overlay-r13.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-overlay-r14.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-overlay-r16.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-overlay-r17.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-chn-overlay-r12.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-chn-overlay-r13.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-chn-overlay-r14.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-chn-overlay-r16.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-chn-overlay-r17.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-chnhk-overlay-r07.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-chnhk-overlay-r08.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-chnhk-overlay-r09.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-chnhk-overlay-r12.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-chnhk-overlay-r13.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-chnhk-overlay-r14.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-chnhk-overlay-r16.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond0qlte-chnhk-overlay-r17.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-overlay-r07.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-overlay-r08.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-overlay-r09.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-overlay-r11.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-overlay-r12.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-overlay-r13.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-overlay-r14.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-overlay-r16.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-overlay-r17.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-chnhk-overlay-r07.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-chnhk-overlay-r08.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-chnhk-overlay-r09.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-chnhk-overlay-r11.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-chnhk-overlay-r12.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-chnhk-overlay-r13.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-chnhk-overlay-r14.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-chnhk-overlay-r16.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-chnhk-overlay-r17.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-overlay-r07.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-overlay-r08.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-overlay-r09.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-overlay-r11.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-overlay-r12.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-overlay-r13.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-overlay-r14.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-overlay-r16.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-overlay-r17.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-jpn-overlay-r08.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-jpn-overlay-r13.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-jpn-overlay-r16.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond1qlte-jpn-overlay-r17.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-jpn-overlay-r09.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-jpn-overlay-r13.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-jpn-overlay-r16.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-jpn-overlay-r17.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-chnhk-overlay-r07.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-chnhk-overlay-r08.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-chnhk-overlay-r09.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-chnhk-overlay-r11.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-chnhk-overlay-r12.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-chnhk-overlay-r13.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-chnhk-overlay-r14.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-chnhk-overlay-r16.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyond2qlte-chnhk-overlay-r17.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyondxq-overlay-r04.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyondxq-overlay-r05.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyondxq-overlay-r09.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyondxq-overlay-r10.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyondxq-overlay-r12.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyondxq-overlay-r14.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyondxq-chn-overlay-r04.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyondxq-chn-overlay-r05.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyondxq-chn-overlay-r09.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyondxq-chn-overlay-r10.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyondxq-chn-overlay-r12.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-beyondxq-chn-overlay-r14.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb

always      := $(dtbo-y)       
subdir-y    := $(dts-dirs)    
clean-files := *.dtbo 
