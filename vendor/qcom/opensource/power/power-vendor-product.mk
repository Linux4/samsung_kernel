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
ifneq ($(filter r0q%, $(TARGET_PRODUCT)),)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/taro/powerhint_r0q.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifneq ($(filter g0q%, $(TARGET_PRODUCT)),)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/taro/powerhint_g0q.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifneq ($(filter b0q%, $(TARGET_PRODUCT)),)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/taro/powerhint_b0q.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else ifneq ($(filter gts8%, $(TARGET_PRODUCT)),)
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/taro/powerhint_gts8p.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
else
PRODUCT_COPY_FILES += vendor/qcom/opensource/power/config/taro/powerhint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml
endif
endif
