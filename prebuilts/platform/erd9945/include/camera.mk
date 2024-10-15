#
# Copyright (C) 2013 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

include vendor/samsung_slsi/exynos/camera/R24/mcd/Interface/VendorPrebuilt/R24/camera.mk

# UniPlugin Libraries
PRODUCT_PACKAGES += \
	libuniplugin \
	libsmartfocus_interface \
	libsmartfocusengine \
	libblurdetection_interface \
	libblurdetection \
	libvdis_interface \
	libvdis_core \
	libeis_interface \
	libeis_core \
	libhypermotion_interface \
	libhypermotion_core \
	libfocuspeaking \
	libfocuspeaking_interface \
	libhifills_interface \
	libIDDQD_interface \
	libIDDQD_core \
	libLocalTM_preview_core \
	libvideobeauty_interface \
	libmpbase.hal \
	lib_supernight_interface \
	libarcsoft_super_night_raw \
	libRMEngine_interface

ifneq ($(filter mu1s% mu2s% mu3s% e1s% e2s% e3s% b6s% r12s%, $(TARGET_PRODUCT)),)
PRODUCT_PACKAGES += \
	libtriplepreview_interface \
	libfusionoverlay_interface \
	libtriplecam_video_optical_zoom \
	libtriplecapture_interface \
	libdualcam_refocus_image_wt \
	libdualcam_refocus_video_wt \
	libarcsoft_stereodistancemeasure \
	libdualcam_refocus_image \
	libdualcam_refocus_video \
	liblivefocus_preview_interface \
	liblivefocus_preview_engine \
	liblivefocus_capture_interface \
	liblivefocus_capture_engine \
	libarcsoft_dualcam_portraitlighting \
	libarcsoft_dualcam_portraitlighting_preview \
	libai_denoiser_interface \
	libhdraid.gpu.arcsoft \
	libhdraid.npu.arcsoft \
	libvideosupernight_interface \
	libDepthBokehVideo_interface \
	libarcsoft_aieffectpk_video \
	libPetDetector_interface \
	libPersonal_interface \
	libPlaneDetector_interface \
	libPersonal_core.camera.samsung \
	libautotracking_interface \
	libbodyid.arcsoft \
	libzoomroi.samsung \
	libSSMAutoTrigger_interface \
	libswb_interface \
	libautoframing_object_tracker.camera.samsung \
	aic_autoframing_detector_tracker_cnn.tflite \
	aic_autoframing_detector_tracker_cnn.info_vendor
endif

# UniAPI
#PRODUCT_PACKAGES += \
#	libuniapi

# Camera HAL
PRODUCT_PACKAGES += \
    vendor.samsung.hardware.camera.provider-service_64 \
    camera.$(TARGET_SOC)

PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/android.hardware.camera.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.xml \
	frameworks/native/data/etc/android.hardware.camera.front.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.front.xml \
	frameworks/native/data/etc/android.hardware.camera.full.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.full.xml \
	frameworks/native/data/etc/android.hardware.camera.autofocus.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.autofocus.xml \
	frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.flash-autofocus.xml \
	frameworks/native/data/etc/android.hardware.camera.manual_postprocessing.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.manual_postprocessing.xml \
	frameworks/native/data/etc/android.hardware.camera.manual_sensor.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.manual_sensor.xml \
	frameworks/native/data/etc/android.hardware.camera.raw.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.raw.xml \
	frameworks/native/data/etc/android.hardware.camera.concurrent.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.concurrent.xml

# Camera
PRODUCT_COPY_FILES += \
    device/samsung/erd9945/media_profiles.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_profiles_V1_0.xml
