pablo-kunit-test-objs := pablo-kunit-test-suites.o \
			pablo-kunit-test-utils.o \
			pablo-subdev-ctrl-kunit-test.o \
			pablo-core-kunit-test.o \
			pablo-device-csi-v4-kunit-test.o \
			pablo-device-sensor-v2-kunit-test.o \
			pablo-video-kunit-test.o \
			pablo-device-ischain-kunit-test.o \
			pablo-groupmgr-kunit-test.o \
			pablo-time-kunit-test.o \
			pablo-interface-wrap-kunit-test.o \
			pablo-ixc-config-kunit-test.o \
			pablo-devicemgr-kunit-test.o \
			pablo-dt-kunit-test.o

pablo-kunit-test-$(CONFIG_PABLO_CRTA_BUFMGR) += pablo-crta-bufmgr-kunit-test.o
ifneq ($(CONFIG_PABLO_V11_30_0),y)
ifneq ($(CONFIG_PABLO_V12_20_0),y)
pablo-kunit-test-$(CONFIG_VIDEO_EXYNOS_PABLO_LIB_VOTF) += pablo-votfmgr-kunit-test.o
endif
endif

obj-$(CONFIG_PABLO_KUNIT_TEST) += pablo-kunit-test.o

# lib
pablo-kunit-test-objs += lib/pablo-lib-kunit-test.o \
			lib/pablo-blob-kunit-test.o \
			lib/pablo-framemgr-kunit-test.o \
			lib/pablo-lib-pmio-kunit-test.o \
			lib/pablo-binary-kunit-test.o \
			lib/pablo-debug-kunit-test.o \
			lib/pablo-internal-subdev-ctrl-kunit-test.o \
			lib/pablo-hw-api-common-kunit-test.o \
			lib/pablo-irq-kunit-test.o \
			lib/pablo-kernel-variant-kunit-test.o \
			lib/pablo-lib-strip-kunit-test.o \
			lib/pablo-mem-kunit-test.o \
			lib/pablo-resourcemgr-kunit-test.o \
			lib/pablo-work-kunit-test.o \
			lib/pablo-lib-json-kunit-test.o \
			lib/pablo-lib-dvfs-kunit-test.o \
			lib/pablo-device-iommu-group-kunit-test.o

# votf
pablo-kunit-test-$(CONFIG_VIDEO_EXYNOS_PABLO_LIB_VOTF) += lib/votf/pablo-votf-kunit-test.o

pablo-kunit-test-$(CONFIG_PABLO_OBTE_SUPPORT) += lib/pablo-lib-pablo-obte-kunit-test.o

# interface
pablo-kunit-test-objs += interface/pablo-interface-ischain-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_SENSOR_ADT) += interface/pablo-sensor-adapter-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_ICPU_ADT_V2) += interface/pablo-icpu-adapter-v2-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_HW_HELPER_V1) += interface/pablo-interface-library-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_ICPU) += interface/pablo-interface-irta-kunit-test.o

# icpu
pablo-kunit-test-$(CONFIG_PABLO_ICPU) += icpu/pablo-icpu-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_ICPU) += icpu/pablo-icpu-mbox-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_ICPU) += icpu/pablo-icpu-itf-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_ICPU) += icpu/pablo-icpu-hw-itf-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_ICPU) += icpu/pablo-icpu-debug-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_ICPU) += icpu/pablo-icpu-msg-queue-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_ICPU) += icpu/pablo-icpu-selftest-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_ICPU) += icpu/pablo-icpu-sysfs-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_ICPU) += icpu/pablo-icpu-mem-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_ICPU) += icpu/firmware/pablo-icpu-firmware-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_ICPU_HW_V1_0) += icpu/pablo-icpu-hw-v1_0-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_ICPU_HW_V2_0) += icpu/pablo-icpu-hw-v2_0-kunit-test.o

# csi
pablo-kunit-test-$(CONFIG_CSIS_V8_0) += csi/pablo-hw-csi-v8_0-kunit-test.o
pablo-kunit-test-$(CONFIG_CSIS_V5_4_8) += csi/pablo-hw-csi-v8_0-kunit-test.o
pablo-kunit-test-$(CONFIG_CSIS_V8_1) += csi/pablo-hw-csi-v8_1-kunit-test.o
pablo-kunit-test-$(CONFIG_CSIS_V6_0) += csi/pablo-hw-csi-v6_0-kunit-test.o
pablo-kunit-test-$(CONFIG_CSIS_V5_20) += csi/pablo-hw-csi-v5_20-kunit-test.o

# hardware

# v10.1
pablo-kunit-test-$(CONFIG_PABLO_V10_1_0) += hardware/pablo-hw-api-mcscaler-v10_0-kunit-test.o

# v11.0
pablo-kunit-test-$(CONFIG_PABLO_V11_0_0) += hardware/pablo-hw-api-lme-v3_0-kunit-test.o \
					hardware/pablo-hw-byrp-v2-kunit-test.o \
					hardware/pablo-hw-api-byrp-v2_0-kunit-test.o \
					hardware/pablo-hw-api-rgbp-v2_0-kunit-test.o \
					hardware/pablo-hw-mcfp-v2-kunit-test.o \
					hardware/pablo-hw-api-mcfp-v11_0-kunit-test.o \
					hardware/pablo-hw-api-yuvp-v2_0-kunit-test.o \
					hardware/pablo-hw-api-mcscaler-v11_0-kunit-test.o \
					hardware/pablo-hw-cstat-v2-kunit-test.o \
					hardware/pablo-hw-djag-v3-kunit-test.o \
					hardware/pablo-hw-mcscaler-v4-kunit-test.o \
					hardware/pablo-hw-rgbp-v2-kunit-test.o \
					hardware/pablo-hw-lme-v3-kunit-test.o \
					hardware/pablo-hw-yuvp-v2-kunit-test.o

# v11.20
pablo-kunit-test-$(CONFIG_PABLO_V11_20_0) += hardware/pablo-hw-api-lme-v2_20-kunit-test.o \
					hardware/pablo-hw-byrp-v2-kunit-test.o \
					hardware/pablo-hw-api-byrp-v1_20-kunit-test.o \
					hardware/pablo-hw-api-rgbp-v1_20-kunit-test.o \
					hardware/pablo-hw-mcfp-v2-kunit-test.o \
					hardware/pablo-hw-api-mcfp-v11_20-kunit-test.o \
					hardware/pablo-hw-api-yuvp-v1_20-kunit-test.o \
					hardware/pablo-hw-api-mcscaler-v11_20-kunit-test.o \
					hardware/pablo-hw-cstat-v2-kunit-test.o \
					hardware/pablo-hw-mcscaler-v4-kunit-test.o \
					hardware/pablo-hw-lme-v3-kunit-test.o

# v11.30
pablo-kunit-test-$(CONFIG_PABLO_V11_30_0) += hardware/pablo-hw-api-lme-v2_20-kunit-test.o \
					hardware/pablo-hw-byrp-v2-kunit-test.o \
					hardware/pablo-hw-api-byrp-v1_30-kunit-test.o \
					hardware/pablo-hw-api-rgbp-v1_40-kunit-test.o \
					hardware/pablo-hw-mcfp-v2-kunit-test.o \
					hardware/pablo-hw-api-mcfp-v11_20-kunit-test.o \
					hardware/pablo-hw-api-yuvp-v1_40-kunit-test.o \
					hardware/pablo-hw-api-mcscaler-v11_30-kunit-test.o \
					hardware/pablo-hw-cstat-v2-kunit-test.o \
					hardware/pablo-hw-mcscaler-v3-kunit-test.o \
					hardware/pablo-hw-rgbp-v2-kunit-test.o \
					hardware/pablo-hw-lme-v3-kunit-test.o

# v12.0
pablo-kunit-test-$(CONFIG_PABLO_V12_0_0) += hardware/pablo-hw-api-lme-v4_0-kunit-test.o \
					hardware/pablo-hw-byrp-v2-kunit-test.o \
					hardware/pablo-hw-api-byrp-v3_0-kunit-test.o \
					hardware/pablo-hw-api-rgbp-v3_0-kunit-test.o \
					hardware/pablo-hw-mcfp-v2-kunit-test.o \
					hardware/pablo-hw-api-mcfp-v12_0-kunit-test.o \
					hardware/pablo-hw-api-yuvp-v3_0-kunit-test.o \
					hardware/pablo-hw-api-shrp-v3_0-kunit-test.o \
					hardware/pablo-hw-api-mcscaler-v12_0-kunit-test.o \
					hardware/pablo-hw-cstat-v2-kunit-test.o \
					hardware/pablo-hw-djag-v3-kunit-test.o \
					hardware/pablo-hw-mcscaler-v4-kunit-test.o \
					hardware/pablo-hw-rgbp-v2-kunit-test.o \
					hardware/pablo-hw-lme-v3-kunit-test.o \
					hardware/pablo-hw-yuvp-v2-kunit-test.o

# v12.1
pablo-kunit-test-$(CONFIG_PABLO_V12_1_0) += hardware/pablo-hw-api-lme-v4_0-kunit-test.o \
					hardware/pablo-hw-byrp-v2-kunit-test.o \
					hardware/pablo-hw-api-byrp-v3_0-kunit-test.o \
					hardware/pablo-hw-api-rgbp-v3_0-kunit-test.o \
					hardware/pablo-hw-mcfp-v2-kunit-test.o \
					hardware/pablo-hw-api-mcfp-v12_0-kunit-test.o \
					hardware/pablo-hw-api-yuvp-v3_0-kunit-test.o \
					hardware/pablo-hw-api-shrp-v3_0-kunit-test.o \
					hardware/pablo-hw-api-mcscaler-v12_0-kunit-test.o \
					hardware/pablo-hw-cstat-v2-kunit-test.o \
					hardware/pablo-hw-djag-v3-kunit-test.o \
					hardware/pablo-hw-mcscaler-v4-kunit-test.o \
					hardware/pablo-hw-rgbp-v2-kunit-test.o \
					hardware/pablo-hw-lme-v3-kunit-test.o \
					hardware/pablo-hw-yuvp-v2-kunit-test.o

pablo-kunit-test-$(CONFIG_PABLO_V12_20_0) += hardware/pablo-hw-byrp-v2-kunit-test.o \
					hardware/pablo-hw-mcfp-v2-kunit-test.o \
					hardware/pablo-hw-mcscaler-v4-kunit-test.o \
					hardware/pablo-hw-rgbp-v2-kunit-test.o \
					hardware/pablo-hw-lme-v3-kunit-test.o \
					hardware/pablo-hw-yuvp-v2-kunit-test.o \
					hardware/pablo-hw-cstat-v2-kunit-test.o \
					hardware/pablo-hw-djag-v3-kunit-test.o \
					hardware/pablo-hw-api-byrp-v2_20-kunit-test.o \
					hardware/pablo-hw-api-rgbp-v2_20-kunit-test.o \
					hardware/pablo-hw-api-mcfp-v12_20-kunit-test.o \
					hardware/pablo-hw-api-yuvp-v2_20-kunit-test.o \
					hardware/pablo-hw-api-mcscaler-v12_20-kunit-test.o \
					hardware/pablo-hw-api-lme-v4_20-kunit-test.o

pablo-kunit-test-$(CONFIG_PABLO_V13_0_0) += hardware/pablo-hw-byrp-v2-kunit-test.o \
					hardware/pablo-hw-api-byrp-v3_0-kunit-test.o \
					hardware/pablo-hw-api-rgbp-v3_0-kunit-test.o \
					hardware/pablo-hw-api-yuvp-v3_0-kunit-test.o \
					hardware/pablo-hw-api-mcscaler-v12_0-kunit-test.o \
					hardware/pablo-hw-djag-v3-kunit-test.o \
					hardware/pablo-hw-mcscaler-v4-kunit-test.o \
					hardware/pablo-hw-rgbp-v2-kunit-test.o \
					hardware/pablo-hw-yuvp-v2-kunit-test.o

pablo-kunit-test-$(CONFIG_PABLO_DLFE) += hardware/pablo-hw-dlfe-kunit-test.o

pablo-kunit-test-$(CONFIG_RGBP_V1_0) += hardware/pablo-hw-api-rgbp-v1_0-kunit-test.o
pablo-kunit-test-$(CONFIG_PDP_V4_1) += hardware/pablo-hw-api-pdp-v4_0-kunit-test.o
pablo-kunit-test-$(CONFIG_PDP_V4_4) += hardware/pablo-hw-api-pdp-v4_0-kunit-test.o
pablo-kunit-test-$(CONFIG_PDP_V5_0) += hardware/pablo-hw-api-pdp-v4_0-kunit-test.o
pablo-kunit-test-$(CONFIG_PDP_V6_0) += hardware/pablo-hw-api-pdp-v6_0-kunit-test.o
pablo-kunit-test-$(CONFIG_BYRP_V1_1) += hardware/pablo-hw-api-byrp-v1_0-kunit-test.o
pablo-kunit-test-$(CONFIG_YUVP_V1_0) += hardware/pablo-hw-api-yuvp-v1_0-kunit-test.o
pablo-kunit-test-$(CONFIG_CSTAT_V1_0) += hardware/pablo-hw-api-cstat-v1_0-kunit-test.o
pablo-kunit-test-$(CONFIG_CSTAT_V2_0) += hardware/pablo-hw-api-cstat-v2_0-kunit-test.o
pablo-kunit-test-$(CONFIG_CSTAT_V1_20) += hardware/pablo-hw-api-cstat-v1_20-kunit-test.o
pablo-kunit-test-$(CONFIG_CSTAT_V1_40) += hardware/pablo-hw-api-cstat-v1_40-kunit-test.o
pablo-kunit-test-$(CONFIG_CSTAT_V3_0) += hardware/pablo-hw-api-cstat-v3_0-kunit-test.o
pablo-kunit-test-$(CONFIG_CSTAT_V2_20) += hardware/pablo-hw-api-cstat-v2_20-kunit-test.o
pablo-kunit-test-$(CONFIG_DRCP_V1_0) += hardware/pablo-hw-api-drcp-v1_0-kunit-test.o
pablo-kunit-test-$(CONFIG_CSIS_PDP_TOP_V5_0) += hardware/pablo-hw-csis-pdp-top-v5_0-kunit-test.o
pablo-kunit-test-$(CONFIG_CSIS_PDP_TOP_V5_20) += hardware/pablo-hw-csis-pdp-top-v5_20-kunit-test.o
pablo-kunit-test-$(CONFIG_CSIS_PDP_TOP_V6_0) += hardware/pablo-hw-csis-pdp-top-v6_0-kunit-test.o
pablo-kunit-test-$(CONFIG_CSIS_PDP_TOP_V6_1) += hardware/pablo-hw-csis-pdp-top-v6_0-kunit-test.o
pablo-kunit-test-$(CONFIG_CSIS_PDP_TOP_V13_0) += hardware/pablo-hw-csis-pdp-top-v13_0-kunit-test.o
pablo-kunit-test-$(CONFIG_COMMON_DMA_V1_0) += hardware/pablo-hw-api-common-dma-kunit-test.o
pablo-kunit-test-$(CONFIG_COMMON_DMA_V2_0) += hardware/pablo-hw-api-common-dma-v2-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_CMN_CTRL_API) += hardware/pablo-hw-api-common-ctrl-kunit-test.o
pablo-kunit-test-$(CONFIG_MCFP_V10_1) += hardware/pablo-hw-api-mcfp-v10_1-kunit-test.o
pablo-kunit-test-$(CONFIG_DLFE_API_V1_0) += hardware/pablo-hw-api-dlfe-v1_0-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_HW_HELPER_V1) += hardware/pablo-hw-helper-v1-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_HW_HELPER_V2) += hardware/pablo-hw-helper-v2-kunit-test.o
pablo-kunit-test-$(CONFIG_PABLO_HW_HELPER_V2) += interface/pablo-interface-ddk-kunit-test.o

# ischain
pablo-kunit-test-objs += ischain/setup-is-module-kunit-test.o

pablo-kunit-test-$(CONFIG_PABLO_V10_1_0) += ischain/is-v10_1_0/pablo-hw-chain-info-kunit-test.o \
						ischain/is-v10_1_0/pablo-subdev-mcs-kunit-test.o

pablo-kunit-test-$(CONFIG_PABLO_V11_0_0) += ischain/is-v11_0_0/pablo-hw-chain-info-kunit-test.o \
						ischain/is-v11_0_0/pablo-subdev-mcs-kunit-test.o \
						ischain/is-v11_0_0/pablo-subdev-byrp-kunit-test.o \
						ischain/is-v11_0_0/pablo-subdev-mcsp-kunit-test.o \
						ischain/is-v11_0_0/pablo-subdev-mcfp-kunit-test.o \
						ischain/is-v11_0_0/pablo-subdev-yuvp-kunit-test.o \
						ischain/is-v11_0_0/pablo-subdev-rgbp-kunit-test.o

pablo-kunit-test-$(CONFIG_PABLO_V11_20_0) += ischain/is-v11_20_0/pablo-hw-chain-info-kunit-test.o \
						ischain/is-v11_20_0/pablo-hw-chain-kunit-test.o \
						ischain/is-v11_20_0/pablo-hw-pwr-kunit-test.o \
						ischain/is-v11_20_0/pablo-subdev-cstat-kunit-test.o \
						ischain/is-v11_20_0/pablo-subdev-mcs-kunit-test.o \
						ischain/is-v11_20_0/pablo-subdev-mcsp-kunit-test.o \
						ischain/is-v11_20_0/pablo-subdev-yuvp-kunit-test.o \
						ischain/is-v11_20_0/pablo-subdev-lme-kunit-test.o \
						ischain/is-v11_20_0/pablo-subdev-sensor-kunit-test.o \
						ischain/is-v11_20_0/setup-is-kunit-test.o \
						ischain/is-v11_20_0/pablo-setup-is-sensor-kunit-test.o \
						ischain/is-v11_20_0/pablo-subdev-paf-kunit-test.o \
						ischain/is-v11_20_0/pablo-subdev-byrp-kunit-test.o \
						ischain/is-v11_20_0/pablo-is-votf-kunit-test.o \
						ischain/is-v11_20_0/pablo-hw-dvfs-kunit-test.o \
						ischain/is-v11_20_0/pablo-subdev-mcfp-kunit-test.o \
						ischain/is-v11_20_0/pablo-subdev-rgbp-kunit-test.o

pablo-kunit-test-$(CONFIG_PABLO_V11_30_0) += ischain/is-v11_30_0/pablo-hw-chain-info-kunit-test.o \
						ischain/is-v11_30_0/pablo-subdev-rgbp-kunit-test.o

pablo-kunit-test-$(CONFIG_PABLO_V12_0_0) += ischain/is-v12_0_0/pablo-hw-chain-info-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-mcs-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-byrp-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-mcsp-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-mcfp-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-yuvp-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-rgbp-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-cstat-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-lme-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-sensor-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-paf-kunit-test.o \
						ischain/is-v12_0_0/pablo-hw-dvfs-kunit-test.o

pablo-kunit-test-$(CONFIG_PABLO_V12_1_0) += ischain/is-v12_0_0/pablo-hw-chain-info-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-mcs-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-byrp-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-mcsp-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-mcfp-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-yuvp-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-rgbp-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-cstat-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-lme-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-sensor-kunit-test.o \
						ischain/is-v12_0_0/pablo-subdev-paf-kunit-test.o \
						ischain/is-v12_0_0/pablo-hw-dvfs-kunit-test.o

pablo-kunit-test-$(CONFIG_PABLO_V12_20_0) += ischain/is-v12_20_0/pablo-hw-chain-info-kunit-test.o \
						ischain/is-v12_20_0/pablo-subdev-mcs-kunit-test.o \
						ischain/is-v12_20_0/pablo-subdev-byrp-kunit-test.o \
						ischain/is-v12_20_0/pablo-subdev-mcsp-kunit-test.o \
						ischain/is-v12_20_0/pablo-subdev-mcfp-kunit-test.o \
						ischain/is-v12_20_0/pablo-subdev-yuvp-kunit-test.o \
						ischain/is-v12_20_0/pablo-subdev-rgbp-kunit-test.o \
						ischain/is-v12_20_0/pablo-subdev-cstat-kunit-test.o \
						ischain/is-v12_20_0/pablo-subdev-lme-kunit-test.o \
						ischain/is-v12_20_0/pablo-subdev-sensor-kunit-test.o \
						ischain/is-v12_20_0/pablo-subdev-paf-kunit-test.o

pablo-kunit-test-$(CONFIG_PABLO_V13_0_0) += ischain/is-v13_0_0/pablo-hw-chain-info-kunit-test.o \
						ischain/is-v13_0_0/pablo-subdev-mcs-kunit-test.o \
						ischain/is-v13_0_0/pablo-subdev-byrp-kunit-test.o \
						ischain/is-v13_0_0/pablo-subdev-mcsp-kunit-test.o \
						ischain/is-v13_0_0/pablo-subdev-yuvp-kunit-test.o \
						ischain/is-v13_0_0/pablo-subdev-rgbp-kunit-test.o

# sensor
pablo-kunit-test-objs += sensor/pablo-interface-sensor-kunit-test.o
pablo-kunit-test-objs += sensor/pablo-helper-i2c-kunit-test.o
pablo-kunit-test-objs += sensor/pablo-helper-i3c-kunit-test.o
pablo-kunit-test-$(CONFIG_CAMERA_CIS_2LD_OBJ) += sensor/pablo-cis-2ld-kunit-test.o

# post
# TODO: remove condition
ifeq ($(CONFIG_PABLO_V11_30_0),y)
pablo-kunit-test-$(CONFIG_CAMERA_PP_GDC_V2_2_0_OBJ) += post/pablo-hw-api-gdc-v220-kunit-test.o
endif
pablo-kunit-test-$(CONFIG_CAMERA_PP_GDC_V10_1_1_OBJ) += post/pablo-hw-api-gdc-v1011-kunit-test.o \
							post/pablo-gdc-core-kunit-test.o
pablo-kunit-test-$(CONFIG_CAMERA_PP_GDC_V11_0_0_OBJ) += post/pablo-hw-api-gdc-v1100-kunit-test.o \
							post/pablo-gdc-core-kunit-test.o
pablo-kunit-test-$(CONFIG_CAMERA_PP_GDC_V11_20_0_OBJ) += post/pablo-hw-api-gdc-v1120-kunit-test.o \
							post/pablo-gdc-core-kunit-test.o
pablo-kunit-test-$(CONFIG_CAMERA_PP_GDC_V12_0_0_OBJ) += post/pablo-hw-api-gdc-v1200-kunit-test.o \
							post/pablo-gdc-core-kunit-test.o
pablo-kunit-test-$(CONFIG_CAMERA_PP_VRA_V6_0_0_OBJ) += post/pablo-hw-api-vra-v6_0_0-kunit-test.o

# subdir-ccflags
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/testing/kunit
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/testing/kunit/include
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/include
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/post
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/sensor/module_framework/cis
