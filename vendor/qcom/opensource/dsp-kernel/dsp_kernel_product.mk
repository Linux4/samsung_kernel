ifneq ($(TARGET_KERNEL_DLKM_DISABLE), true)
PRODUCT_PACKAGES += frpc-adsprpc.ko
#PRODUCT_PACKAGES += frpc_trusted-adsprpc.ko
PRODUCT_PACKAGES += cdsp-loader.ko
endif