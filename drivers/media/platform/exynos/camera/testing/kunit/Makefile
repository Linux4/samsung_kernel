pablo-kunit-test-objs := pablo-kunit-test-suites.o \
			pablo-subdev-ctrl-kunit-test.o \
			pablo-core-kunit-test.o \
			pablo-device-csi-v4-kunit-test.o \
			pablo-device-sensor-v2-kunit-test.o \
			pablo-video-kunit-test.o \
			pablo-time-kunit-test.o \

obj-$(CONFIG_PABLO_KUNIT_TEST) += pablo-kunit-test.o


# lib
pablo-kunit-test-$(CONFIG_PABLO_OBTE_SUPPORT) += pablo-lib-pablo-obte-kunit-test.o


# interface
pablo-kunit-test-objs += interface/pablo-interface-ischain-kunit-test.o
pablo-kunit-test-objs += interface/pablo-interface-library-kunit-test.o


# csi
pablo-kunit-test-$(CONFIG_CSIS_V8_0) += csi/pablo-hw-csi-v8_0-kunit-test.o


# hardware

# v10.1
pablo-kunit-test-$(CONFIG_PABLO_V10_1_0) += hardware/pablo-hw-api-mcscaler-v10_0-kunit-test.o

pablo-kunit-test-$(CONFIG_RGBP_V1_1) += hardware/pablo-hw-api-rgbp-v1_0-kunit-test.o
pablo-kunit-test-$(CONFIG_PDP_V4_1) += hardware/pablo-hw-api-pdp-v4_0-kunit-test.o
pablo-kunit-test-$(CONFIG_PDP_V4_4) += hardware/pablo-hw-api-pdp-v4_0-kunit-test.o
pablo-kunit-test-$(CONFIG_PDP_V5_0) += hardware/pablo-hw-api-pdp-v4_0-kunit-test.o
pablo-kunit-test-$(CONFIG_BYRP_V1_1) += hardware/pablo-hw-api-byrp-v1_0-kunit-test.o
pablo-kunit-test-$(CONFIG_YUVP_V1_1) += hardware/pablo-hw-api-yuvp-v1_0-kunit-test.o
pablo-kunit-test-$(CONFIG_MCFP_V10_1) += hardware/pablo-hw-api-mcfp-v10_1-kunit-test.o
pablo-kunit-test-$(CONFIG_CSTAT_V1_1) += hardware/pablo-hw-api-cstat-v1_0-kunit-test.o
pablo-kunit-test-$(CONFIG_DRCP_V1_0) += hardware/pablo-hw-api-drcp-v1_0-kunit-test.o
pablo-kunit-test-objs += hardware/pablo-hw-api-common-dma-kunit-test.o


#ischain
pablo-kunit-test-$(CONFIG_PABLO_V10_1_0) += ischain/is-v10_1_0/pablo-subdev-byrp-kunit-test.o


# sensor
pablo-kunit-test-objs += sensor/pablo-interface-sensor-kunit-test.o


subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/include
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/testing/kunit
