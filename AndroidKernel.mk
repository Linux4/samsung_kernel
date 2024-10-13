KERNEL_OUT := $(TARGET_OUT_INTERMEDIATES)/KERNEL
KERNEL_CONFIG := $(KERNEL_OUT)/.config
KERNEL_MODULES_OUT := $(TARGET_OUT)/lib/modules

ifeq ($(USES_UNCOMPRESSED_KERNEL),true)
TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/arch/arm/boot/Image
else
TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/arch/arm/boot/zImage
endif

$(KERNEL_OUT):
	@echo "==== Start Kernel Compiling ... ===="

$(KERNEL_CONFIG): kernel/arch/arm/configs/$(KERNEL_DEFCONFIG)
	mkdir -p $(KERNEL_OUT)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- $(KERNEL_DEFCONFIG)

ifeq ($(TARGET_BUILD_VARIANT),user)
USER_CONFIG := $(TARGET_OUT)/dummy
TARGET_DEVICE_USER_CONFIG := $(PLATDIR)/user_diff_config
TARGET_DEVICE_CUSTOM_CONFIG := device/sprd/$(TARGET_DEVICE)/ProjectConfig.mk

$(USER_CONFIG) : $(KERNEL_CONFIG)
	$(info $(shell ./kernel/scripts/sprd_custom_config_kernel.sh $(KERNEL_CONFIG) $(TARGET_DEVICE_CUSTOM_CONFIG)))
	$(info $(shell ./kernel/scripts/sprd_create_user_config.sh $(KERNEL_CONFIG) $(TARGET_DEVICE_USER_CONFIG)))
else
USER_CONFIG  := $(TARGET_OUT)/dummy
TARGET_DEVICE_CUSTOM_CONFIG := device/sprd/$(TARGET_DEVICE)/ProjectConfig.mk
$(USER_CONFIG) : $(KERNEL_CONFIG)
	$(info $(shell ./kernel/scripts/sprd_custom_config_kernel.sh $(KERNEL_CONFIG) $(TARGET_DEVICE_CUSTOM_CONFIG)))
endif

$(TARGET_PREBUILT_KERNEL) : $(KERNEL_OUT) $(USER_CONFIG)|$(KERNEL_CONFIG)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- headers_install
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- -j4
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- modules
	@-mkdir -p $(KERNEL_MODULES_OUT)
	@-find $(KERNEL_OUT) -name *.ko | xargs -I{} cp {} $(KERNEL_MODULES_OUT)

kernelheader:
	mkdir -p $(KERNEL_OUT)
	$(MAKE) -j1 -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- headers_install
