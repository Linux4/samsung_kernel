ENABLE_SECUREMSM_DLKM := true
ENABLE_SECUREMSM_QTEE_DLKM := true

ifeq ($(TARGET_KERNEL_DLKM_DISABLE), true)
  ifeq ($(TARGET_KERNEL_DLKM_SECURE_MSM_OVERRIDE), false)
      ENABLE_SECUREMSM_DLKM := false
  endif
  ifeq ($(TARGET_KERNEL_DLKM_SECUREMSM_QTEE_OVERRIDE), false)
      ENABLE_SECUREMSM_QTEE_DLKM := false
  endif
endif

ifeq ($(ENABLE_SECUREMSM_DLKM), true)
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/qcedev-mod_dlkm.ko \
      $(KERNEL_MODULES_OUT)/qcrypto-msm_dlkm.ko \
      $(KERNEL_MODULES_OUT)/qce50_dlkm.ko \
      $(KERNEL_MODULES_OUT)/hdcp_qseecom_dlkm.ko \
      $(KERNEL_MODULES_OUT)/qrng_dlkm.ko \

BOARD_VENDOR_RAMDISK_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/hdcp_qseecom_dlkm.ko

ifeq ($(TARGET_USES_SMMU_PROXY), true)
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/smmu_proxy_dlkm.ko
endif

ifeq ($(ENABLE_SECUREMSM_QTEE_DLKM), true)

BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/smcinvoke_dlkm.ko \
			       $(KERNEL_MODULES_OUT)/tz_log_dlkm.ko \

BOARD_VENDOR_RAMDISK_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/tz_log_dlkm.ko \
                                       $(KERNEL_MODULES_OUT)/smcinvoke_dlkm.ko \

BOARD_VENDOR_RAMDISK_RECOVERY_KERNEL_MODULES_LOAD += $(KERNEL_MODULES_OUT)/smcinvoke_dlkm.ko \
                                                     $(KERNEL_MODULES_OUT)/hdcp_qseecom_dlkm.ko \


BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/qseecom_dlkm.ko
BOARD_VENDOR_RAMDISK_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/qseecom_dlkm.ko
BOARD_VENDOR_RAMDISK_RECOVERY_KERNEL_MODULES_LOAD += $(KERNEL_MODULES_OUT)/qseecom_dlkm.ko

endif #ENABLE_SECUREMSM_QTEE_DLKM
endif #ENABLE_SECUREMSM_DLKM
