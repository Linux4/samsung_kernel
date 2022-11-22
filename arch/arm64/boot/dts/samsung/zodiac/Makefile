# Path of Qcom Device tree
QCOM_DTB := ../../qcom

dtbo-$(CONFIG_SEC_ZODIAC_PROJECT) = sm8150-sec-zodiac-overlay-r04.dtbo	\
					sm8150-sec-zodiac-overlay-r05.dtbo

m8150-sec-zodiac-overlay-r04.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-zodiac-overlay-r05.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb

always      := $(dtbo-y)       
subdir-y    := $(dts-dirs)    
clean-files := *.dtbo 
