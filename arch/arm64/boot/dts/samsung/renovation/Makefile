# Path of Qcom Device tree
QCOM_DTB := ../../qcom

dtbo-$(CONFIG_SEC_R5Q_PROJECT) = sm8150-sec-r5q-overlay-r00.dtbo	\
					sm8150-sec-r5q-overlay-r01.dtbo \
					sm8150-sec-r5q-overlay-r07.dtbo \
					sm8150-sec-r5q-overlay-r08.dtbo \
					sm8150-sec-r5q-overlay-r09.dtbo

dtbo-$(CONFIG_SEC_R3Q_PROJECT) = sm8150-sec-r3q-overlay-r00.dtbo	\
					sm8150-sec-r3q-overlay-r01.dtbo	\
					sm8150-sec-r3q-overlay-r02.dtbo	\
					sm8150-sec-r3q-overlay-r03.dtbo

ifeq ($(filter y,$(CONFIG_MACH_R3Q_CHN_OPEN) $(CONFIG_MACH_R3Q_MEA_OPEN)),y)
dtbo-y = sm8150-sec-r3q-chn-overlay-r00.dtbo \
	 sm8150-sec-r3q-chn-overlay-r03.dtbo \
	 sm8150-sec-r3q-chn-overlay-r11.dtbo \
	 sm8150-sec-r3q-chn-overlay-r13.dtbo \
	 sm8150-sec-r3q-chn-overlay-r14.dtbo \
	 sm8150-sec-r3q-chn-overlay-r15.dtbo \
	 sm8150-sec-r3q-chn-overlay-r16.dtbo \
	 sm8150-sec-r3q-chn-overlay-r17.dtbo
endif
ifeq ($(filter y,$(CONFIG_MACH_R3Q_KOR_SINGLE)),y)
dtbo-y = sm8150-sec-r3q-kor-overlay-r00.dtbo \
	sm8150-sec-r3q-kor-overlay-r01.dtbo \
	sm8150-sec-r3q-kor-overlay-r02.dtbo \
	sm8150-sec-r3q-kor-overlay-r03.dtbo
endif
ifeq ($(filter y,$(CONFIG_MACH_R5Q_USA_OPEN)),y)
dtbo-y = sm8150-sec-r5q-usa-overlay-r00.dtbo \
	sm8150-sec-r5q-usa-overlay-r01.dtbo
endif

dtbo-y += sm8150-sec-system-update-overlay.dtbo

sm8150-sec-r5q-overlay-r00.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r5q-overlay-r01.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r5q-overlay-r07.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r5q-overlay-r08.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r5q-overlay-r09.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r3q-overlay-r00.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r3q-overlay-r01.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r3q-overlay-r02.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r3q-overlay-r03.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r3q-chn-overlay-r00.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r3q-chn-overlay-r03.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r3q-chn-overlay-r11.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r3q-chn-overlay-r13.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r3q-chn-overlay-r14.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r3q-chn-overlay-r15.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r3q-chn-overlay-r16.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r3q-chn-overlay-r17.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r3q-kor-overlay-r00.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r3q-kor-overlay-r01.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r3q-kor-overlay-r02.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r3q-kor-overlay-r03.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r5q-usa-overlay-r00.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-r5q-usa-overlay-r01.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb

always      := $(dtbo-y)       
subdir-y    := $(dts-dirs)    
clean-files := *.dtbo 
