#Android makefile to build kernel as a part of Android Build
PERL		= perl

ifeq ($(TARGET_PREBUILT_KERNEL),)

KERNEL_OUT := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
KERNEL_CONFIG := $(KERNEL_OUT)/.config
ifeq ($(TARGET_KERNEL_APPEND_DTB), true)
TARGET_PREBUILT_INT_KERNEL := $(KERNEL_OUT)/arch/arm/boot/zImage-dtb
else
TARGET_PREBUILT_INT_KERNEL := $(KERNEL_OUT)/arch/arm/boot/zImage
endif
KERNEL_HEADERS_INSTALL := $(KERNEL_OUT)/usr
KERNEL_MODULES_INSTALL := system
KERNEL_MODULES_OUT := $(TARGET_OUT)/lib/modules
KERNEL_IMG=$(KERNEL_OUT)/arch/arm/boot/Image

USE_MODULE ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_MODULES=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))

DTS_NAMES ?= $(shell $(PERL) -e 'while (<>) {$$a = $$1 if /CONFIG_ARCH_((?:MSM|QSD|MPQ)[a-zA-Z0-9]+)=y/; $$r = $$1 if /CONFIG_MSM_SOC_REV_(?!NONE)(\w+)=y/; $$arch = $$arch.lc("$$a$$r ") if /CONFIG_ARCH_((?:MSM|QSD|MPQ)[a-zA-Z0-9]+)=y/} print $$arch;' $(KERNEL_CONFIG))
KERNEL_USE_OF ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_USE_OF=y/) { $$of = "y"; break; } } print $$of;' kernel/arch/arm/configs/$(KERNEL_DEFCONFIG))
LOCALE_KOR ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_SEC_LOCALE_KOR=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))
LOCALE_CHN_DUOS ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_MACH_H3GDUOS=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))
K_PROJECT ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_SEC_K_PROJECT=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))
H_PROJECT ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_SEC_H_PROJECT=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))
F_PROJECT ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_SEC_F_PROJECT=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))
MONDRIAN_PROJECT ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_SEC_MONDRIAN_PROJECT=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))
VIENNA_PROJECT ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_SEC_VIENNA_PROJECT=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))
LT03_PROJECT ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_SEC_LT03_PROJECT=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))
MILLET3G_PROJECT ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_MACH_MILLET3G_EUR=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))
MILLETLTE_PROJECT ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_MACH_MILLETLTE_OPEN=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))
MILLETWIFI_PROJECT ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_MACH_MILLETWIFI_OPEN=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))
MATISSE3G_PROJECT ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_MACH_MATISSE3G_OPEN=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))
MATISSELTE_PROJECT ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_MACH_MATISSELTE_OPEN=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))
PICASSO_PROJECT ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_SEC_PICASSO_PROJECT=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))
V2_PROJECT ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_SEC_V2_PROJECT=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))
KS01_PROJECT ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_SEC_KS01_PROJECT=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))
KACTIVE_PROJECT ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_SEC_KACTIVE_PROJECT=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))
HEAT_PROJECT ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_SEC_HEAT_PROJECT=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))
BERLUTI3G_PROJECT ?= $(shell $(PERL) -e '$$of = "n"; while (<>) { if (/CONFIG_MACH_BERLUTI3G_EUR=y/) { $$of = "y"; break; } } print $$of;' $(KERNEL_CONFIG))

ifeq "$(KERNEL_USE_OF)" "y"
ifeq "$(K_PROJECT)" "y"
DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/msm8974pro/$(DTS_NAME)pro-sec*.dts)
endif
ifeq "$(VIENNA_PROJECT)" "y"
DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/msm8974/$(DTS_NAME)-sec-vienna*.dts)
endif
ifeq "$(LT03_PROJECT)" "y"
DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/msm8974/$(DTS_NAME)-sec-lt03*.dts)
endif
ifeq "$(MILLET3G_PROJECT)" "y"
DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/msm8226/$(DTS_NAME)-sec-millet3geur*.dts)
endif
ifeq "$(MILLETLTE_PROJECT)" "y"
DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/msm8226/msm8926-sec-milletlte*.dts)
endif
ifeq "$(MILLETWIFI_PROJECT)" "y"
DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/msm8226/$(DTS_NAME)-sec-milletwifieur*.dts)
endif
ifeq "$(MATISSE3G_PROJECT)" "y"
DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/msm8226/$(DTS_NAME)-sec-matisse3g*.dts)
endif
ifeq "$(MATISSELTE_PROJECT)" "y"
DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/msm8226/msm8926-sec-matisselte*.dts)
endif
ifeq "$(BERLUTI3G_PROJECT)" "y"
DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/msm8226/$(DTS_NAME)-sec-berluti3geur*.dts)
endif

ifeq "$(HEAT_PROJECT)" "y"
DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/msm8610/$(DTS_NAME)-sec-heat-tfnvzw*.dts)
endif
$(info printing $(DTS_NAMES))
$(info printing $(DTS_FILES))

ifeq "$(H_PROJECT)" "y"
	ifeq "$(LOCALE_KOR)" "y"
		DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/msm8974/$(DTS_NAME)-sec-hltekor*.dts)
	else
		ifeq "$(LOCALE_CHN_DUOS)" "y"
			DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/msm8974/$(DTS_NAME)-sec-h3gchnduos*.dts)
		else
			DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/msm8974/$(DTS_NAME)-sec-hlte-*.dts)
		endif
	endif
endif
ifeq "$(F_PROJECT)" "y"
		DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/msm8974/$(DTS_NAME)-sec-fltekor*.dts)
endif
ifeq "$(PICASSO_PROJECT)" "y"
DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/msm8974/$(DTS_NAME)-sec-picasso*.dts)
endif
ifeq "$(V2_PROJECT)" "y"
DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/msm8974/$(DTS_NAME)-sec-v2*.dts)
endif
ifeq "$(KS01_PROJECT)" "y"
	ifeq "$(LOCALE_KOR)" "y"
		DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/msm8974/$(DTS_NAME)-sec-ks01lte*.dts)
	endif
endif
ifeq "$(KACTIVE_PROJECT)" "y"
DTS_FILES = $(wildcard $(TOP)/kernel/arch/arm/boot/dts/msm8974pro/$(DTS_NAME)pro-ac-sec-kactive*.dts)
endif
DTS_FILE = $(lastword $(subst /, ,$(1)))
DTB_FILE = $(addprefix $(KERNEL_OUT)/arch/arm/boot/,$(patsubst %.dts,%.dtb,$(call DTS_FILE,$(1))))
ZIMG_FILE = $(addprefix $(KERNEL_OUT)/arch/arm/boot/,$(patsubst %.dts,%-zImage,$(call DTS_FILE,$(1))))
KERNEL_ZIMG = $(KERNEL_OUT)/arch/arm/boot/zImage
DTC = $(KERNEL_OUT)/scripts/dtc/dtc

define append-dtb
mkdir -p $(KERNEL_OUT)/arch/arm/boot;\
$(foreach DTS_NAME, $(DTS_NAMES), \
   $(foreach d, $(DTS_FILES), \
      $(DTC) -p 1024 -O dtb -o $(call DTB_FILE,$(d)) $(d); \
      cat $(KERNEL_ZIMG) $(call DTB_FILE,$(d)) > $(call ZIMG_FILE,$(d));))
endef
else

define append-dtb
endef
endif

ifeq ($(TARGET_USES_UNCOMPRESSED_KERNEL),true)
$(info Using uncompressed kernel)
TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/piggy
else
TARGET_PREBUILT_KERNEL := $(TARGET_PREBUILT_INT_KERNEL)
endif

define mv-modules
mdpath=`find $(KERNEL_MODULES_OUT) -type f -name modules.dep`;\
if [ "$$mdpath" != "" ];then\
mpath=`dirname $$mdpath`;\
ko=`find $$mpath/kernel -type f -name *.ko`;\
for i in $$ko; do mv $$i $(KERNEL_MODULES_OUT)/; done;\
fi
endef

define clean-module-folder
mdpath=`find $(KERNEL_MODULES_OUT) -type f -name modules.dep`;\
if [ "$$mdpath" != "" ];then\
mpath=`dirname $$mdpath`; rm -rf $$mpath;\
fi
endef

#Tweak defconfig for FACTORY KERNEL without additional fac_defcofig
define modi-facdefconfig
cp kernel/arch/arm/configs/$(KERNEL_DEFCONFIG) kernel/arch/arm/configs/factory_defconfig
echo "CONFIG_FACTORY_KERNEL=y" >> kernel/arch/arm/configs/factory_defconfig
endef

$(KERNEL_OUT):
	mkdir -p $(KERNEL_OUT)

$(KERNEL_CONFIG): $(KERNEL_OUT)
ifeq ($(SEC_FACTORY_BUILD),true)
	$(modi-facdefconfig)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- VARIANT_DEFCONFIG=$(VARIANT_DEFCONFIG) DEBUG_DEFCONFIG=$(DEBUG_DEFCONFIG) SELINUX_DEFCONFIG=$(SELINUX_DEFCONFIG) SELINUX_LOG_DEFCONFIG=$(SELINUX_LOG_DEFCONFIG) factory_defconfig TIMA_DEFCONFIG=$(TIMA_DEFCONFIG)
else
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- VARIANT_DEFCONFIG=$(VARIANT_DEFCONFIG) DEBUG_DEFCONFIG=$(DEBUG_DEFCONFIG) SELINUX_DEFCONFIG=$(SELINUX_DEFCONFIG) SELINUX_LOG_DEFCONFIG=$(SELINUX_LOG_DEFCONFIG) $(KERNEL_DEFCONFIG) TIMA_DEFCONFIG=$(TIMA_DEFCONFIG)
endif
$(KERNEL_OUT)/piggy : $(TARGET_PREBUILT_INT_KERNEL)
	$(hide) gunzip -c $(KERNEL_OUT)/arch/arm/boot/compressed/piggy.gzip > $(KERNEL_OUT)/piggy

$(TARGET_PREBUILT_INT_KERNEL): $(KERNEL_OUT) $(KERNEL_CONFIG) $(KERNEL_HEADERS_INSTALL)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi-
ifeq "$(USE_MODULE)" "y"
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- modules
	$(MAKE) -C kernel O=../$(KERNEL_OUT) INSTALL_MOD_PATH=../../$(KERNEL_MODULES_INSTALL) INSTALL_MOD_STRIP=1 ARCH=arm CROSS_COMPILE=arm-eabi- modules_install
	$(mv-modules)
	$(clean-module-folder)
endif
	$(append-dtb)

$(KERNEL_HEADERS_INSTALL): $(KERNEL_OUT) $(KERNEL_CONFIG)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- headers_install

kerneltags: $(KERNEL_OUT) $(KERNEL_CONFIG)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- tags

kernelconfig: $(KERNEL_OUT) $(KERNEL_CONFIG)
	env KCONFIG_NOTIMESTAMP=true \
	     $(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- menuconfig
	env KCONFIG_NOTIMESTAMP=true \
	     $(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- savedefconfig
	cp $(KERNEL_OUT)/defconfig kernel/arch/arm/configs/$(KERNEL_DEFCONFIG)

endif
