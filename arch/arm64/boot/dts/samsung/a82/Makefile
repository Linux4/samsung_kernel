# Path of Qcom Device tree
QCOM_DTB := ../../qcom

dtbo-$(CONFIG_SEC_A82XQ_PROJECT) = sm8150-sec-a82xq-overlay-r00.dtbo \
					sm8150-sec-a82xq-overlay-r01.dtbo

sm8150-sec-a82xq-overlay-r00.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb
sm8150-sec-a82xq-overlay-r01.dtbo-base := $(QCOM_DTB)/sm8150.dtb $(QCOM_DTB)/sm8150-v2.dtb

always      := $(dtbo-y)       
subdir-y    := $(dts-dirs)    
clean-files := *.dtbo 
