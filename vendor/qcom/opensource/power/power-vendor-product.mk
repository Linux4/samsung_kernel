#Power product definitions
PRODUCT_PACKAGES += android.hardware.power-service
PRODUCT_PACKAGES += android.hardware.power-impl

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
else ifeq ($(TARGET_BOARD_PLATFORM),parrot)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/parrot/powerhint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifeq ($(TARGET_BOARD_PLATFORM),neo)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/neo/powerhint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifeq ($(TARGET_BOARD_PLATFORM),anorak)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/anorak/powerhint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
endif
