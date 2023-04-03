ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_QN3979) += \
    samsung-sm7225-qrd-default-overlay.dtbo \
    samsung-sm7225-qrd-r01-overlay.dtbo \
    samsung-sm7225-qrd-r02-overlay.dtbo \
    samsung-sm7225-qrd-r03-overlay.dtbo \
    samsung-sm7225-qrd-r04-overlay.dtbo \
    samsung-sm7225-qrd-r05-overlay.dtbo \
    samsung-sm7225-qrd-r06-overlay.dtbo \
    samsung-sm7225-qrd-r07-overlay.dtbo \
    samsung-sm7225-qrd-r08-overlay.dtbo \
    samsung-sm7225-qrd-r09-overlay.dtbo
samsung-sm7225-qrd-default-overlay.dtbo-base := lagoon.dtb
samsung-sm7225-qrd-r01-overlay.dtbo-base := lagoon.dtb
samsung-sm7225-qrd-r02-overlay.dtbo-base := lagoon.dtb
samsung-sm7225-qrd-r03-overlay.dtbo-base := lagoon.dtb
samsung-sm7225-qrd-r04-overlay.dtbo-base := lagoon.dtb
samsung-sm7225-qrd-r05-overlay.dtbo-base := lagoon.dtb
samsung-sm7225-qrd-r06-overlay.dtbo-base := lagoon.dtb
samsung-sm7225-qrd-r07-overlay.dtbo-base := lagoon.dtb
samsung-sm7225-qrd-r08-overlay.dtbo-base := lagoon.dtb
samsung-sm7225-qrd-r09-overlay.dtbo-base := lagoon.dtb
else
dtb-$(CONFIG_ARCH_QN3979) += lagoon-qrd.dtb
endif

always        := $(dtb-y)
subdir-y    := $(dts-dirs)
clean-files    := *.dtb *.dtbo
