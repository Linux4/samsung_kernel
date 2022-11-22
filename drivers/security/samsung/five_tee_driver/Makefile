#
# Integrity TEE Driver
#
TEEC_CLIENT_PATH := multibuild/source/gp-api/client
PROTOCOL_PATH := multibuild/source/gp-api/protocol
FIVE_TEE_DRIVER_PATH := drivers/security/samsung/five_tee_driver

obj-$(CONFIG_FIVE_TEE_DRIVER) += itee_driver.o

obj-$(CONFIG_FIVE_USE_TRUSTONIC) += $(TEEC_CLIENT_PATH)/tee_client_api.o \
                                    $(PROTOCOL_PATH)/teec_param_utils.o \
                                    $(TEEC_CLIENT_PATH)/teec_common_tbase.o \
                                    $(TEEC_CLIENT_PATH)/teec_operation_common.o \
                                    $(TEEC_CLIENT_PATH)/tee_client_api.o

obj-$(CONFIG_FIVE_USE_QSEE) += $(TEEC_CLIENT_PATH)/tee_client_api.o \
                               $(PROTOCOL_PATH)/teec_param_utils.o \
                               $(TEEC_CLIENT_PATH)/teec_common_qsee.o \
                               $(TEEC_CLIENT_PATH)/teec_operation_qsee.o \
                               $(TEEC_CLIENT_PATH)/tee_client_api.o

obj-$(CONFIG_FIVE_USE_TZDEV) += $(TEEC_CLIENT_PATH)/teec_operation_common.o

itee_driver-objs += five_tee_driver.o

ccflags-$(CONFIG_FIVE_TEE_DRIVER) += -Isecurity/samsung/five \
                                -I$(FIVE_TEE_DRIVER_PATH)/$(PROTOCOL_PATH) \
                                -I$(FIVE_TEE_DRIVER_PATH)/$(TEEC_CLIENT_PATH)

ccflags-$(CONFIG_FIVE_USE_TRUSTONIC) += -DCONFIG_TRUSTONIC
ccflags-$(CONFIG_FIVE_USE_TZDEV) += -DCONFIG_TEEGRIS
ccflags-$(CONFIG_FIVE_USE_QSEE) += -DCONFIG_QSEE

ifeq ($(CONFIG_FIVE_USE_TRUSTONIC), y)
  ifeq ($(CONFIG_SOC_EXYNOS9820), y)
    PLATFORM := exynos9820
    ccflags-$(CONFIG_FIVE_TEE_DRIVER) += -Idrivers/gud/gud-$(PLATFORM)/MobiCoreDriver
    ccflags-$(CONFIG_FIVE_TEE_DRIVER) += -Idrivers/gud/gud-$(PLATFORM)/MobiCoreDriver/public
  else
    $(error "Unknown SoC")
  endif
endif

ccflags-$(CONFIG_FIVE_USE_TRUSTONIC) += -I$(FIVE_TEE_DRIVER_PATH)/multibuild/include/gp-api
ccflags-$(CONFIG_FIVE_USE_TZDEV) += -Idrivers/misc/tzdev/include/tzdev \
                                    -I$(FIVE_TEE_DRIVER_PATH)/multibuild/include/gp-api
ccflags-$(CONFIG_FIVE_USE_QSEE) += -I$(FIVE_TEE_DRIVER_PATH)/multibuild/include/gp-api \
                                   -Idrivers/misc
aflags-$(CONFIG_FIVE_USE_QSEE) += -I$(srctree)
