#Power product definitions
#PRODUCT_PACKAGES += android.hardware.power-service
#PRODUCT_PACKAGES += android.hardware.power-impl

#Powerhint File
ifeq ($(TARGET_BOARD_PLATFORM),msmnile)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/msmnile/powerhint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifeq ($(TARGET_BOARD_PLATFORM),kona)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/kona/powerhint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifeq ($(TARGET_BOARD_PLATFORM),lito)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/lito/powerhint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifeq ($(TARGET_BOARD_PLATFORM),atoll)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/atoll/powerhint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifeq ($(TARGET_BOARD_PLATFORM),lahaina)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/lahaina/powerhint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifeq ($(TARGET_BOARD_PLATFORM),holi)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/holi/powerhint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifeq ($(TARGET_BOARD_PLATFORM),taro)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/taro/powerhint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifeq ($(TARGET_BOARD_PLATFORM),kalama)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/kalama/powerhint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifeq ($(TARGET_BOARD_PLATFORM),pineapple)
ifneq ($(filter mu1q% e1q%, $(TARGET_PRODUCT)),)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/pineapple/powerhint_e1q.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifneq ($(filter mu2q% e2q%, $(TARGET_PRODUCT)),)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/pineapple/powerhint_e2q.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifneq ($(filter mu3q% e3q%, $(TARGET_PRODUCT)),)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/pineapple/powerhint_e3q.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifneq ($(filter b6q%, $(TARGET_PRODUCT)),)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/pineapple/powerhint_b6q.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifneq ($(filter q6q%, $(TARGET_PRODUCT)),)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/pineapple/powerhint_q6q.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/pineapple/powerhint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
endif
else ifeq ($(TARGET_BOARD_PLATFORM),blair)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/holi/powerhint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
endif
