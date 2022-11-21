# Path of Qcom Device tree
QCOM_DTB := ../../qcom

dtbo-$(CONFIG_SEC_BLOOMQ_PROJECT) = sm8150-sec-bloomq-overlay-r00.dtbo \
					sm8150-sec-bloomq-overlay-r01.dtbo \
					sm8150-sec-bloomq-overlay-r03.dtbo \
					sm8150-sec-bloomq-overlay-r04.dtbo \
					sm8150-sec-bloomq-overlay-r05.dtbo \
					sm8150-sec-bloomq-overlay-r06.dtbo \
					sm8150-sec-bloomq-overlay-r07.dtbo \
					sm8150-sec-bloomq-overlay-r08.dtbo \
					sm8150-sec-bloomq-overlay-r10.dtbo

ifeq ($(filter y,$(CONFIG_MACH_BLOOMQ_JPN_KDI) $(CONFIG_MACH_BLOOMQ_JPN_OPEN)),y)
dtbo-y = sm8150-sec-bloomq-overlay-r00.dtbo \
	sm8150-sec-bloomq-overlay-r01.dtbo \
	sm8150-sec-bloomq-overlay-r03.dtbo \
	sm8150-sec-bloomq-overlay-r04.dtbo \
	sm8150-sec-bloomq-overlay-r05.dtbo \
	sm8150-sec-bloomq-jpn-overlay-r06.dtbo \
	sm8150-sec-bloomq-jpn-overlay-r07.dtbo \
	sm8150-sec-bloomq-jpn-overlay-r08.dtbo
endif

sm8150-sec-bloomq-overlay-r00.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-bloomq-overlay-r01.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-bloomq-overlay-r03.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-bloomq-overlay-r04.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-bloomq-overlay-r05.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-bloomq-overlay-r06.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-bloomq-overlay-r07.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-bloomq-overlay-r08.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-bloomq-overlay-r10.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-bloomq-jpn-overlay-r06.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-bloomq-jpn-overlay-r07.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-bloomq-jpn-overlay-r08.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb

dtbo-y += sm8150-sec-system-update-overlay.dtbo

always      := $(dtbo-y)       
subdir-y    := $(dts-dirs)    
clean-files := *.dtbo 
