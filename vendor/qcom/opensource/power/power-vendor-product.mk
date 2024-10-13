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
else ifeq ($(TARGET_BOARD_PLATFORM),sm6150)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/sm6150/powerhint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifeq ($(TARGET_BOARD_PLATFORM),bengal)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/bengal/powerhint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifeq ($(TARGET_BOARD_PLATFORM),trinket)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/trinket/powerhint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
endif
