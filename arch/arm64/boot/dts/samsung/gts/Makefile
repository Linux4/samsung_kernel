# Path of Qcom Device tree
QCOM_DTB := ../../qcom

dtbo-$(CONFIG_SEC_GTS6L_PROJECT) = sm8150-sec-gts6l-overlay-r00.dtbo	\
	sm8150-sec-gts6l-overlay-r01.dtbo	\
	sm8150-sec-gts6l-overlay-r02.dtbo	\
	sm8150-sec-gts6l-overlay-r04.dtbo
dtbo-$(CONFIG_SEC_GTS6LWIFI_PROJECT) = sm8150-sec-gts6lwifi-overlay-r02.dtbo \
	sm8150-sec-gts6lwifi-overlay-r06.dtbo
dtbo-$(CONFIG_SEC_GTS6X_PROJECT) = sm8150-sec-gts6x-overlay-r04.dtbo \
	sm8150-sec-gts6x-overlay-r05.dtbo

sm8150-sec-gts6l-overlay-r00.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-gts6l-overlay-r01.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-gts6l-overlay-r02.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-gts6l-overlay-r04.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-gts6lwifi-overlay-r02.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-gts6lwifi-overlay-r06.dtbo-base := $(QCOM_DTB)/sm8150p.dtb $(QCOM_DTB)/sm8150p-v2.dtb
sm8150-sec-gts6x-overlay-r04.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-gts6x-overlay-r05.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb

always      := $(dtbo-y)       
subdir-y    := $(dts-dirs)    
clean-files := *.dtbo 
