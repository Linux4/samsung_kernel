fimc-is-objs	:= is-core.o \
		is-groupmgr.o \
		is-resourcemgr.o \
		is-devicemgr.o \
		is-video.o \
		pablo-video-cmn-subdev.o \
		is-video-sensor.o \
		is-video-sensor-subdev.o \
		is-video-3aa.o \
		is-video-mcs.o \
		is-video-pafrdma.o \
		is-subdev-ctrl.o \
		is-device-ischain.o \
		is-interface-wrap.o \
		is-device-sensor_v2.o \
		is-time.o \
		is-dt.o \
		is-i2c-config.o \
		is-device-camif-dma.o \
		pablo-device-iommu-group.o \
		pablo-device-camif-subblks.o

fimc-is-$(CONFIG_EXYNOS_DEVICE_MIPI_CSIS_VER4) += is-device-csi_v4.o
fimc-is-$(CONFIG_PABLO_V11_0_0) += pablo-crta-bufmgr.o
fimc-is-$(CONFIG_PABLO_V11_20_0) += pablo-crta-bufmgr.o
fimc-is-$(CONFIG_PABLO_V11_30_0) += pablo-crta-bufmgr.o
fimc-is-$(CONFIG_PABLO_V12_0_0) += pablo-crta-bufmgr.o
fimc-is-$(CONFIG_VIDEO_EXYNOS_PABLO_LIB_VOTF) += is-votfmgr.o
# TODO: deprecate CONFIG_VIDEO_EXYNOS_CAMERA_POSTPROCESS_VOTF
fimc-is-$(CONFIG_VIDEO_EXYNOS_CAMERA_POSTPROCESS_VOTF) += is-votfmgr.o

# SPI
pablo-spis-objs := is-spi.o
obj-$(CONFIG_PABLO_SPI) += pablo-spis.o

# video
fimc-is-$(CONFIG_PABLO_V8_1_0) += is-video-isp.o \
		is-video-3aa-subdev.o \
		is-video-isp-subdev.o \
		is-video-lme.o \
		is-video-lme-subdev.o \
		is-video-mcs-subdev.o \
		is-video-vra.o
fimc-is-$(CONFIG_PABLO_V8_10_0) += is-video-isp.o \
		is-video-3aa-subdev.o \
		is-video-isp-subdev.o \
		is-video-lme.o \
		is-video-lme-subdev.o \
		is-video-mcs-subdev.o \
		is-video-vra.o
fimc-is-$(CONFIG_PABLO_V8_20_0) += is-video-isp.o \
		is-video-3aa-subdev.o \
		is-video-isp-subdev.o \
		is-video-lme.o \
		is-video-lme-subdev.o \
		is-video-mcs-subdev.o \
		is-video-vra.o
fimc-is-$(CONFIG_PABLO_V9_1_0) += is-video-isp.o \
		is-video-ypp.o \
		is-video-lme.o
fimc-is-$(CONFIG_PABLO_V9_10_0) += is-video-isp.o \
		is-video-ypp.o \
		is-video-lme.o
fimc-is-$(CONFIG_PABLO_V10_1_0) += is-video-byrp.o \
		is-video-rgbp.o \
		is-video-mcfp.o \
		is-video-yuvp.o \
		is-video-lme.o
fimc-is-$(CONFIG_PABLO_V10_20_0) += is-video-isp.o \
		is-video-orb.o
fimc-is-$(CONFIG_PABLO_V11_0_0) += is-video-byrp.o \
		is-video-rgbp.o \
		is-video-mcfp.o \
		is-video-yuvp.o \
		is-video-lme.o
fimc-is-$(CONFIG_PABLO_V11_20_0) += is-video-byrp.o \
		is-video-rgbp.o \
		is-video-mcfp.o \
		is-video-yuvp.o \
		is-video-lme.o
fimc-is-$(CONFIG_PABLO_V11_30_0) += is-video-byrp.o \
		is-video-rgbp.o \
		is-video-mcfp.o \
		is-video-yuvp.o \
		is-video-lme.o
fimc-is-$(CONFIG_PABLO_V12_0_0) += is-video-byrp.o \
		is-video-rgbp.o \
		is-video-mcfp.o \
		is-video-yuvp.o \
		is-video-lme.o
# testing
fimc-is-objs += testing/pablo-self-test-result.o
fimc-is-objs += testing/pablo-sensor-self-test.o

ifeq ($(CONFIG_CAMERA_DVFS_V2),y)
fimc-is-objs += is-dvfs_v2.o
else
fimc-is-objs += is-dvfs.o
endif

obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += fimc-is.o
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += post/
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += sensor/
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += ischain/
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += hardware/
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += lib/
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += icpu/
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += testing/self/
obj-$(CONFIG_PABLO_KUNIT_TEST) += testing/kunit/

pablo-smc-objs := lib/pablo-smc.o
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += pablo-smc.o

obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += vendor/

# hardware
fimc-is-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += hardware/is-hw-control.o \
					hardware/is-hw-control-m2m.o \
					hardware/is-hw-dm.o

fimc-is-$(CONFIG_PABLO_HW_HELPER_V1) += hardware/pablo-hw-helper-v1.o \
					interface/is-interface-library.o \
					interface/pablo-interface-ddk-mock.o
fimc-is-$(CONFIG_PABLO_ICPU) += interface/pablo-interface-irta.o

fimc-is-$(CONFIG_PABLO_V8_1_0) += hardware/is-hw-3aa.o \
		hardware/is-hw-isp.o \
		hardware/is-hw-vra.o
fimc-is-$(CONFIG_PABLO_V8_10_0) += hardware/is-hw-3aa.o \
		hardware/is-hw-isp.o \
		hardware/is-hw-vra.o
fimc-is-$(CONFIG_PABLO_V8_20_0) += hardware/is-hw-3aa.o \
		hardware/is-hw-isp.o \
		hardware/is-hw-vra-v2.o
fimc-is-$(CONFIG_PABLO_V9_1_0) += hardware/is-hw-3aa.o \
		hardware/is-hw-isp.o \
		hardware/is-hw-lme.o \
		hardware/is-hw-yuvpp.o
fimc-is-$(CONFIG_PABLO_V9_10_0) += hardware/is-hw-3aa.o \
		hardware/is-hw-isp.o \
		hardware/is-hw-lme.o \
		hardware/is-hw-yuvpp.o
fimc-is-$(CONFIG_PABLO_V10_1_0) += hardware/is-hw-cstat.o \
		hardware/is-hw-byrp.o \
		hardware/is-hw-rgbp.o \
		hardware/is-hw-lme-v2.o \
		hardware/is-hw-yuvp-v1_1.o \
		hardware/is-hw-mcfp.o
fimc-is-$(CONFIG_PABLO_V10_20_0) += hardware/is-hw-3aa-v2.o \
		hardware/is-hw-isp-v2.o \
		hardware/is-hw-orbmch.o
fimc-is-$(CONFIG_PABLO_V11_0_0) += hardware/is-hw-cstat-v2.o
fimc-is-$(CONFIG_PABLO_V11_20_0) += hardware/is-hw-cstat-v2.o
fimc-is-$(CONFIG_PABLO_V11_30_0) += hardware/is-hw-cstat-v2.o
fimc-is-$(CONFIG_PABLO_V12_0_0) += hardware/is-hw-cstat-v2.o

fimc-is-$(CONFIG_CAMERA_PDP) += hardware/is-hw-pdp.o

# hardware/api
fimc-is-$(CONFIG_PABLO_V8_1_0) += hardware/api/is-hw-api-vra.o
fimc-is-$(CONFIG_PABLO_V8_10_0) += hardware/api/is-hw-api-vra.o
fimc-is-$(CONFIG_PABLO_V8_20_0) += hardware/api/is-hw-api-vra-v3_2.o

fimc-is-$(CONFIG_CSIS_PDP_TOP_V5_0) += hardware/api/is-hw-api-csis_pdp_top-v5_0.o
fimc-is-$(CONFIG_CSIS_PDP_TOP_V6_0) += hardware/api/is-hw-api-csis_pdp_top-v6_0.o

fimc-is-$(CONFIG_PDP_V1_0_0) += hardware/api/is-hw-api-pdp-v1_0_0.o
fimc-is-$(CONFIG_PDP_V3_1) += hardware/api/is-hw-api-pdp-v3_1.o
fimc-is-$(CONFIG_PDP_V4_1) += hardware/api/is-hw-api-pdp-v4_0.o
fimc-is-$(CONFIG_PDP_V4_4) += hardware/api/is-hw-api-pdp-v4_0.o
fimc-is-$(CONFIG_PDP_V5_0) += hardware/api/is-hw-api-pdp-v4_0.o
fimc-is-$(CONFIG_PDP_V6_0) += hardware/api/is-hw-api-pdp-v6_0.o
fimc-is-$(CONFIG_CSTAT_V1_1) += hardware/api/is-hw-api-cstat-v1_1.o
fimc-is-$(CONFIG_CSTAT_V2_0) += hardware/api/is-hw-api-cstat-v2_0.o
fimc-is-$(CONFIG_CSTAT_V1_20) += hardware/api/is-hw-api-cstat-v1_20.o
fimc-is-$(CONFIG_CSTAT_V1_40) += hardware/api/is-hw-api-cstat-v1_40.o
fimc-is-$(CONFIG_CSTAT_V3_0) += hardware/api/is-hw-api-cstat-v3_0.o
fimc-is-$(CONFIG_BYRP_V1_1) += hardware/api/is-hw-api-byrp-v1_1.o
fimc-is-$(CONFIG_RGBP_V1_1) += hardware/api/is-hw-api-rgbp-v1_1.o
fimc-is-$(CONFIG_YUVPP_V2_1) += hardware/api/is-hw-api-yuvpp-v2_1.o
fimc-is-$(CONFIG_LME_V1_1) += hardware/api/is-hw-api-lme-v1_1.o
fimc-is-$(CONFIG_LME_V2_1) += hardware/api/is-hw-api-lme-v2_1.o
fimc-is-$(CONFIG_MCFP_V10_1) += hardware/api/is-hw-api-mcfp-v10_1.o
fimc-is-$(CONFIG_YUVP_V1_1) += hardware/api/is-hw-api-yuvp-v1_1.o
fimc-is-$(CONFIG_TAA_V9_20) += hardware/api/is-hw-api-3aa-v9_20.o
fimc-is-$(CONFIG_DNS_V1_20) += hardware/api/is-hw-api-dns-v1_20.o
fimc-is-$(CONFIG_ITP_V1_20) += hardware/api/is-hw-api-itp-v1_20.o
fimc-is-$(CONFIG_ORBMCH_V2_1) += hardware/api/is-hw-api-orbmch-v2_1.o
fimc-is-$(CONFIG_MCFP_V10_20) += hardware/api/is-hw-api-mcfp-v10_20.o

# interface
fimc-is-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += interface/is-interface-ischain.o \
		interface/is-interface-ddk.o

fimc-is-$(CONFIG_PABLO_V8_1_0) += interface/is-interface-vra.o
fimc-is-$(CONFIG_PABLO_V8_10_0) += interface/is-interface-vra.o
fimc-is-$(CONFIG_PABLO_V8_20_0) += interface/is-interface-vra-v2.o
fimc-is-$(CONFIG_PABLO_SENSOR_ADT) += interface/pablo-sensor-adapter-v1.o
fimc-is-$(CONFIG_PABLO_ICPU_ADT_V2) += interface/pablo-icpu-adapter-v2.o

# ischain
fimc-is-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += ischain/setup-is-module.o \
					ischain/pablo-hw-chain-info-common.o

fimc-is-$(CONFIG_PABLO_V9_1_0) += ischain/is-v9_1_0/setup-is.o \
		ischain/is-v9_1_0/setup-is-sensor.o \
		ischain/is-v9_1_0/is-hw-pwr.o \
		ischain/is-v9_1_0/is-hw-chain.o \
		ischain/is-v9_1_0/is-hw-dvfs.o \
		ischain/is-v9_1_0/is-subdev-3aa.o \
		ischain/is-v9_1_0/is-subdev-3ac.o \
		ischain/is-v9_1_0/is-subdev-3ap.o \
		ischain/is-v9_1_0/is-subdev-3af.o \
		ischain/is-v9_1_0/is-subdev-3ag.o \
		ischain/is-v9_1_0/is-subdev-3ao.o \
		ischain/is-v9_1_0/is-subdev-3al.o \
		ischain/is-v9_1_0/is-subdev-isp.o \
		ischain/is-v9_1_0/is-subdev-ixc.o \
		ischain/is-v9_1_0/is-subdev-ixp.o \
		ischain/is-v9_1_0/is-subdev-ixg.o \
		ischain/is-v9_1_0/is-subdev-ixt.o \
		ischain/is-v9_1_0/is-subdev-ixv.o \
		ischain/is-v9_1_0/is-subdev-ixw.o \
		ischain/is-v9_1_0/is-subdev-mexc.o \
		ischain/is-v9_1_0/is-subdev-orbxc.o \
		ischain/is-v9_1_0/is-subdev-mcs.o \
		ischain/is-v9_1_0/is-subdev-mcsp.o \
		ischain/is-v9_1_0/is-subdev-paf.o \
		ischain/is-v9_1_0/is-subdev-ypp.o \
		ischain/is-v9_1_0/is-subdev-lme.o \
		ischain/is-v9_1_0/is-subdev-lmes.o \
		ischain/is-v9_1_0/is-subdev-lmec.o \
		ischain/is-v9_1_0/is-subdev-sensor.o \
		ischain/is-v9_1_0/is-subdev-sensor-vc.o \
		ischain/is-v9_1_0/is-votf.o \
		ischain/is-v9_1_0/pablo-hw-chain-info.o \
		ischain/is-v9_1_0/is-subdev-framemgr.o

fimc-is-$(CONFIG_PABLO_V9_10_0) += ischain/is-v9_10_0/setup-is.o \
		ischain/is-v9_10_0/setup-is-sensor.o \
		ischain/is-v9_10_0/is-hw-pwr.o \
		ischain/is-v9_10_0/is-hw-chain.o \
		ischain/is-v9_10_0/is-hw-dvfs.o \
		ischain/is-v9_10_0/is-subdev-3aa.o \
		ischain/is-v9_10_0/is-subdev-3ac.o \
		ischain/is-v9_10_0/is-subdev-3ap.o \
		ischain/is-v9_10_0/is-subdev-3af.o \
		ischain/is-v9_10_0/is-subdev-3ag.o \
		ischain/is-v9_10_0/is-subdev-3ao.o \
		ischain/is-v9_10_0/is-subdev-3al.o \
		ischain/is-v9_10_0/is-subdev-isp.o \
		ischain/is-v9_10_0/is-subdev-ixc.o \
		ischain/is-v9_10_0/is-subdev-ixp.o \
		ischain/is-v9_10_0/is-subdev-ixg.o \
		ischain/is-v9_10_0/is-subdev-ixt.o \
		ischain/is-v9_10_0/is-subdev-ixv.o \
		ischain/is-v9_10_0/is-subdev-ixw.o \
		ischain/is-v9_10_0/is-subdev-mexc.o \
		ischain/is-v9_10_0/is-subdev-orbxc.o \
		ischain/is-v9_10_0/is-subdev-mcs.o \
		ischain/is-v9_10_0/is-subdev-mcsp.o \
		ischain/is-v9_10_0/is-subdev-paf.o \
		ischain/is-v9_10_0/is-subdev-ypp.o \
		ischain/is-v9_10_0/is-subdev-lme.o \
		ischain/is-v9_10_0/is-subdev-lmes.o \
		ischain/is-v9_10_0/is-subdev-lmec.o \
		ischain/is-v9_10_0/is-subdev-sensor.o \
		ischain/is-v9_10_0/is-subdev-sensor-vc.o \
		ischain/is-v9_10_0/is-votf.o \
		ischain/is-v9_10_0/pablo-hw-chain-info.o \
		ischain/is-v9_10_0/is-subdev-framemgr.o

fimc-is-$(CONFIG_PABLO_V10_1_0) += ischain/is-v10_1_0/setup-is.o \
		ischain/is-v10_1_0/setup-is-sensor.o \
		ischain/is-v10_1_0/is-hw-pwr.o \
		ischain/is-v10_1_0/is-hw-chain.o \
		ischain/is-v10_1_0/is-hw-dvfs.o \
		ischain/is-v10_1_0/is-subdev-cstat.o \
		ischain/is-v10_1_0/is-subdev-byrp.o \
		ischain/is-v10_1_0/is-subdev-rgbp.o \
		ischain/is-v10_1_0/is-subdev-mcs.o \
		ischain/is-v10_1_0/is-subdev-mcsp.o \
		ischain/is-v10_1_0/is-subdev-paf.o \
		ischain/is-v10_1_0/is-subdev-yuvp.o \
		ischain/is-v10_1_0/is-subdev-mcfp.o \
		ischain/is-v10_1_0/is-subdev-lme.o \
		ischain/is-v10_1_0/is-subdev-sensor.o \
		ischain/is-v10_1_0/is-subdev-sensor_vc.o \
		ischain/is-v10_1_0/is-votf.o \
		ischain/is-v10_1_0/pablo-hw-chain-info.o \
		ischain/is-v10_1_0/is-subdev-framemgr.o

fimc-is-$(CONFIG_PABLO_V10_20_0) += ischain/is-v10_20_0/setup-is.o \
		ischain/is-v10_20_0/setup-is-sensor.o \
		ischain/is-v10_20_0/is-hw-pwr.o \
		ischain/is-v10_20_0/is-hw-chain.o \
		ischain/is-v10_20_0/is-hw-dvfs.o \
		ischain/is-v10_20_0/is-subdev-3aa.o \
		ischain/is-v10_20_0/is-subdev-3ac.o \
		ischain/is-v10_20_0/is-subdev-3ap.o \
		ischain/is-v10_20_0/is-subdev-3af.o \
		ischain/is-v10_20_0/is-subdev-3ag.o \
		ischain/is-v10_20_0/is-subdev-3ao.o \
		ischain/is-v10_20_0/is-subdev-3al.o \
		ischain/is-v10_20_0/is-subdev-isp.o \
		ischain/is-v10_20_0/is-subdev-ixc.o \
		ischain/is-v10_20_0/is-subdev-ixp.o \
		ischain/is-v10_20_0/is-subdev-ixg.o \
		ischain/is-v10_20_0/is-subdev-ixt.o \
		ischain/is-v10_20_0/is-subdev-ixv.o \
		ischain/is-v10_20_0/is-subdev-ixw.o \
		ischain/is-v10_20_0/is-subdev-mexc.o \
		ischain/is-v10_20_0/is-subdev-orbxc.o \
		ischain/is-v10_20_0/is-subdev-orb.o \
		ischain/is-v10_20_0/is-subdev-mcs.o \
		ischain/is-v10_20_0/is-subdev-mcsp.o \
		ischain/is-v10_20_0/is-subdev-paf.o \
		ischain/is-v10_20_0/is-subdev-sensor.o \
		ischain/is-v10_20_0/is-subdev-sensor-vc.o \
		ischain/is-v10_20_0/is-votf.o \
		ischain/is-v10_20_0/pablo-hw-chain-info.o \
		ischain/is-v10_20_0/is-subdev-framemgr.o

fimc-is-$(CONFIG_PABLO_V11_0_0) += ischain/is-v11_0_0/setup-is.o \
		ischain/is-v11_0_0/setup-is-sensor.o \
		ischain/is-v11_0_0/is-hw-pwr.o \
		ischain/is-v11_0_0/is-hw-chain.o \
		ischain/is-v11_0_0/is-hw-dvfs.o \
		ischain/is-v11_0_0/is-subdev-cstat.o \
		ischain/is-v11_0_0/is-subdev-byrp.o \
		ischain/is-v11_0_0/is-subdev-rgbp.o \
		ischain/is-v11_0_0/is-subdev-mcs.o \
		ischain/is-v11_0_0/is-subdev-mcsp.o \
		ischain/is-v11_0_0/is-subdev-paf.o \
		ischain/is-v11_0_0/is-subdev-yuvp.o \
		ischain/is-v11_0_0/is-subdev-mcfp.o \
		ischain/is-v11_0_0/is-subdev-lme.o \
		ischain/is-v11_0_0/is-subdev-sensor.o \
		ischain/is-v11_0_0/is-subdev-sensor_vc.o \
		ischain/is-v11_0_0/is-votf.o \
		ischain/is-v11_0_0/pablo-hw-chain-info.o \
		ischain/is-v11_0_0/is-subdev-framemgr.o

fimc-is-$(CONFIG_PABLO_V11_20_0) += ischain/is-v11_20_0/setup-is.o \
		ischain/is-v11_20_0/setup-is-sensor.o \
		ischain/is-v11_20_0/is-hw-pwr.o \
		ischain/is-v11_20_0/is-hw-chain.o \
		ischain/is-v11_20_0/is-hw-dvfs.o \
		ischain/is-v11_20_0/is-subdev-cstat.o \
		ischain/is-v11_20_0/is-subdev-byrp.o \
		ischain/is-v11_20_0/is-subdev-rgbp.o \
		ischain/is-v11_20_0/is-subdev-mcs.o \
		ischain/is-v11_20_0/is-subdev-mcsp.o \
		ischain/is-v11_20_0/is-subdev-paf.o \
		ischain/is-v11_20_0/is-subdev-yuvp.o \
		ischain/is-v11_20_0/is-subdev-mcfp.o \
		ischain/is-v11_20_0/is-subdev-lme.o \
		ischain/is-v11_20_0/is-subdev-sensor.o \
		ischain/is-v11_20_0/is-subdev-sensor_vc.o \
		ischain/is-v11_20_0/is-votf.o \
		ischain/is-v11_20_0/pablo-hw-chain-info.o \
		ischain/is-v11_20_0/is-subdev-framemgr.o

fimc-is-$(CONFIG_PABLO_V11_30_0) += ischain/is-v11_30_0/setup-is.o \
		ischain/is-v11_30_0/setup-is-sensor.o \
		ischain/is-v11_30_0/is-hw-pwr.o \
		ischain/is-v11_30_0/is-hw-chain.o \
		ischain/is-v11_30_0/is-hw-dvfs.o \
		ischain/is-v11_30_0/is-subdev-cstat.o \
		ischain/is-v11_30_0/is-subdev-byrp.o \
		ischain/is-v11_30_0/is-subdev-rgbp.o \
		ischain/is-v11_30_0/is-subdev-mcs.o \
		ischain/is-v11_30_0/is-subdev-mcsp.o \
		ischain/is-v11_30_0/is-subdev-paf.o \
		ischain/is-v11_30_0/is-subdev-yuvp.o \
		ischain/is-v11_30_0/is-subdev-mcfp.o \
		ischain/is-v11_30_0/is-subdev-lme.o \
		ischain/is-v11_30_0/is-subdev-sensor.o \
		ischain/is-v11_30_0/is-subdev-sensor_vc.o \
		ischain/is-v11_30_0/is-votf.o \
		ischain/is-v11_30_0/pablo-hw-chain-info.o \
		ischain/is-v11_30_0/is-subdev-framemgr.o

fimc-is-$(CONFIG_PABLO_V12_0_0) += ischain/is-v12_0_0/setup-is.o \
		ischain/is-v12_0_0/setup-is-sensor.o \
		ischain/is-v12_0_0/is-hw-pwr.o \
		ischain/is-v12_0_0/is-hw-chain.o \
		ischain/is-v12_0_0/is-hw-dvfs.o \
		ischain/is-v12_0_0/is-subdev-cstat.o \
		ischain/is-v12_0_0/is-subdev-byrp.o \
		ischain/is-v12_0_0/is-subdev-rgbp.o \
		ischain/is-v12_0_0/is-subdev-mcs.o \
		ischain/is-v12_0_0/is-subdev-mcsp.o \
		ischain/is-v12_0_0/is-subdev-paf.o \
		ischain/is-v12_0_0/is-subdev-yuvp.o \
		ischain/is-v12_0_0/is-subdev-mcfp.o \
		ischain/is-v12_0_0/is-subdev-lme.o \
		ischain/is-v12_0_0/is-subdev-sensor.o \
		ischain/is-v12_0_0/is-subdev-sensor_vc.o \
		ischain/is-v12_0_0/is-votf.o \
		ischain/is-v12_0_0/pablo-hw-chain-info.o \
		ischain/is-v12_0_0/is-subdev-framemgr.o

# sensor/csi
fimc-is-$(CONFIG_CSIS_V1_2) += csi/is-hw-csi-v1_2.o \
				csi/is-hw-phy.o
fimc-is-$(CONFIG_CSIS_V5_3) += csi/is-hw-csi-v5_3.o \
				csi/is-hw-phy.o
fimc-is-$(CONFIG_CSIS_V5_4) += csi/is-hw-csi-v5_4.o \
				csi/is-hw-phy.o
fimc-is-$(CONFIG_CSIS_V5_4_8) += csi/is-hw-csi-v8_0.o \
				csi/is-hw-phy.o
fimc-is-$(CONFIG_CSIS_V8_0) += csi/is-hw-csi-v8_0.o \
				csi/is-hw-phy.o
fimc-is-$(CONFIG_CSIS_V8_1) += csi/is-hw-csi-v8_1.o \
				csi/is-hw-phy.o
fimc-is-$(CONFIG_CSIS_V6_0) += csi/is-hw-csi-v6_0.o \
				csi/is-hw-phy.o

# sensor/module_framework
fimc-is-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP)	+= sensor/is-helper-i2c.o \
						   sensor/is-helper-i3c.o \
						   sensor/module_framework/is-interface-sensor.o \
						   sensor/module_framework/is-interface-actuator.o \
						   sensor/module_framework/is-control-sensor.o \
						   sensor/module_framework/is-control-actuator.o \
						   sensor/module_framework/is-device-sensor-peri.o \
						   sensor/module_framework/modules/is-device-module-base.o \
						   sensor/module_framework/cis/is-cis.o

# for zebu
fimc-is-$(CONFIG_CAMERA_CIS_ZEBU_OBJ)		+= sensor/module_framework/modules/is-device-module-zebu.o

# vendor
fimc-is-$(CONFIG_CAMERA_VENDER_DEFAULT) += vendor/default/is-vender.o \
				    vendor/default/is-sysfs.o

ccflags-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) := -DDYNAMIC_DEBUG_MODULE

subdir-ccflags-y := -I$(srctree)/drivers/media/platform/exynos/camera
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/include
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/lib
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/interface
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/hardware
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/hardware/api
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/hardware/include
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/vendor
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/sensor/module_framework
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/icpu/include
subdir-ccflags-$(CONFIG_PABLO_V8_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v8_1_0
subdir-ccflags-$(CONFIG_PABLO_V8_10_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v8_10_0
subdir-ccflags-$(CONFIG_PABLO_V8_20_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v8_20_0
subdir-ccflags-$(CONFIG_PABLO_V9_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v9_1_0
subdir-ccflags-$(CONFIG_PABLO_V9_10_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v9_10_0
subdir-ccflags-$(CONFIG_PABLO_V10_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v10_1_0
subdir-ccflags-$(CONFIG_PABLO_V10_20_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v10_20_0
subdir-ccflags-$(CONFIG_PABLO_V11_0_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v11_0_0
subdir-ccflags-$(CONFIG_PABLO_V11_20_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v11_20_0
subdir-ccflags-$(CONFIG_PABLO_V11_30_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v11_30_0
subdir-ccflags-$(CONFIG_PABLO_V12_0_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v12_0_0
subdir-ccflags-$(CONFIG_PABLO_V8_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v8_1_0
subdir-ccflags-$(CONFIG_PABLO_V8_10_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v8_10_0
subdir-ccflags-$(CONFIG_PABLO_V8_20_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v8_20_0
subdir-ccflags-$(CONFIG_PABLO_V9_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v9_1_0
subdir-ccflags-$(CONFIG_PABLO_V9_10_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v9_10_0
subdir-ccflags-$(CONFIG_PABLO_V10_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v10_1_0
subdir-ccflags-$(CONFIG_PABLO_V10_20_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v10_20_0
subdir-ccflags-$(CONFIG_PABLO_V11_0_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v11_0_0
subdir-ccflags-$(CONFIG_PABLO_V11_20_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v11_20_0
subdir-ccflags-$(CONFIG_PABLO_V11_30_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v11_30_0
subdir-ccflags-$(CONFIG_PABLO_V12_0_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v12_0_0

# For using vendor specific code, please rename default to xxx.
subdir-ccflags-$(CONFIG_CAMERA_VENDER_DEFAULT) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default

ifeq ($(CONFIG_CAMERA_VENDER_DEFAULT),y)
#subdir-ccflags-$(CONFIG_PABLO_V8_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v8_1_0
#subdir-ccflags-$(CONFIG_PABLO_V8_10_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v8_10_0
#subdir-ccflags-$(CONFIG_PABLO_V8_20_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v8_20_0
#subdir-ccflags-$(CONFIG_PABLO_V9_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v9_1_0
#subdir-ccflags-$(CONFIG_PABLO_V9_10_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v9_10_0
#subdir-ccflags-$(CONFIG_PABLO_V10_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v10_1_0
#subdir-ccflags-$(CONFIG_PABLO_V10_20_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v10_20_0
else ifeq ($(CONFIG_CAMERA_VENDER_MCD_V2),y)
fimc-is-$(CONFIG_CAMERA_VENDER_MCD_V2) += vendor/mcd_v2/is-vender.o \
                                vendor/mcd_v2/is-vender-caminfo.o \
                                vendor/mcd_v2/is-sec-define.o \
                                vendor/mcd_v2/crc32.o \
                                vendor/mcd_v2/is-sysfs.o

fimc-is-$(CONFIG_OIS_USE)                       += vendor/mcd_v2/is-device-ois_common.o
fimc-is-$(CONFIG_AF_HOST_CONTROL)               += vendor/mcd_v2/is-device-af.o
fimc-is-$(CONFIG_CAMERA_USE_INTERNAL_MCU)       += vendor/mcd_v2/is-ois-mcu.o \
                                                vendor/mcd_v2/is-hw-api-ois-mcu-v1_1_1.o
fimc-is-$(CONFIG_CAMERA_USE_MCU)                += vendor/mcd_v2/is-device-ois-mcu.o

subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/mcd_v2
endif
