# Gpu libs
PRODUCT_PACKAGES += \
	vulkan.samsung \
	libEGL_samsung \
	libGLESv1_CM_samsung \
	libGLESv2_samsung \
	libOpenCL \
	libOpenCL_icd \
	libSGPUOpenCL \
	libgpudataproducer \
	libdrm_sgpu \
	libsbwchelper \
	whitelist

# Gpu properties
PRODUCT_PROPERTY_OVERRIDES += \
	ro.hardware.egl = samsung \
	ro.hardware.vulkan = samsung \
	graphics.gpu.profiler.support = true

# Gralloc libs
PRODUCT_PACKAGES += \
    android.hardware.graphics.mapper@4.0-impl-sgr \
    android.hardware.graphics.allocator-aidl-service-sgr \
    libexynosgraphicbuffer_public

# SGPU firmware signing
ifeq ($(SEC_BUILD_OPTION_KNOX_CSB),true)
type=${VBMETA_TYPE}
project=${SEC_BUILD_CONF_MODEL_SIGNING_NAME}
SECURE_SCRIPT=../buildscript/tools/signclient.jar

UNSIGN_GPU_EVT0 := device/samsung/erd9945/firmware/sgpu/unsigned_vangogh_lite_unified_evt0.bin
SIGNED_GPU_EVT0 := device/samsung/erd9945/firmware/sgpu/vangogh_lite_unified_evt0.bin
GPU_RENAMING_EVT0 := $(shell mv $(SIGNED_GPU_EVT0) $(UNSIGN_GPU_EVT0))
GPU_SIGNNING_EVT0 := $(shell java -jar $(SECURE_SCRIPT) -runtype ${type} -model ${project} -input $(UNSIGN_GPU_EVT0) -output $(SIGNED_GPU_EVT0))

UNSIGN_GPU_EVT1 := device/samsung/erd9945/firmware/sgpu/unsigned_vangogh_lite_unified_evt1.bin
SIGNED_GPU_EVT1 := device/samsung/erd9945/firmware/sgpu/vangogh_lite_unified_evt1.bin
GPU_RENAMING_EVT1 := $(shell mv $(SIGNED_GPU_EVT1) $(UNSIGN_GPU_EVT1))
GPU_SIGNNING_EVT1 := $(shell java -jar $(SECURE_SCRIPT) -runtype ${type} -model ${project} -input $(UNSIGN_GPU_EVT1) -output $(SIGNED_GPU_EVT1))
endif

# SGPU firmware overwrite
PRODUCT_COPY_FILES += \
	device/samsung/erd9945/firmware/sgpu/vangogh_lite_unified_evt0.bin:$(NORMAL_TARGET_COPY_OUT_VENDOR_RAMDISK)/lib/firmware/sgpu/vangogh_lite_unified_evt0.bin \
	device/samsung/erd9945/firmware/sgpu/vangogh_lite_unified_evt0.bin:$(TARGET_COPY_OUT_RECOVERY)/root/lib/firmware/sgpu/vangogh_lite_unified_evt0.bin \
	device/samsung/erd9945/firmware/sgpu/vangogh_lite_unified_evt1.bin:$(NORMAL_TARGET_COPY_OUT_VENDOR_RAMDISK)/lib/firmware/sgpu/vangogh_lite_unified_evt1.bin \
	device/samsung/erd9945/firmware/sgpu/vangogh_lite_unified_evt1.bin:$(TARGET_COPY_OUT_RECOVERY)/root/lib/firmware/sgpu/vangogh_lite_unified_evt1.bin \

# FEATURE_OPENGLES_EXTENSION_PACK support string config file
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.opengles.aep.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.opengles.aep.xml

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.software.opengles.deqp.level-2023-03-01.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.opengles.deqp.level.xml

# vulkan version information
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.vulkan.compute-0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.compute.xml \
    frameworks/native/data/etc/android.hardware.vulkan.level-1.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.level.xml \
    frameworks/native/data/etc/android.hardware.vulkan.version-1_3.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.version.xml \
    frameworks/native/data/etc/android.software.vulkan.deqp.level-2023-03-01.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.vulkan.deqp.level.xml

# RenderScript HAL
PRODUCT_PACKAGES += \
    android.hardware.renderscript@1.0-impl

PRODUCT_PROPERTY_OVERRIDES += \
    ro.opengles.version=196610

# Support ANGLE features
PRODUCT_PROPERTY_OVERRIDES += \
	ro.egl.blobcache.multifile = true \
    ro.egl.blobcache.multifile_limit = 268435456
