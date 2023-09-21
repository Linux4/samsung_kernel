ccflags-y += -Idrivers/media/platform/msm/camera_v2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/common
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/io
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/cci
ifdef CONFIG_USE_CAMERA_HW_BIG_DATA
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor
endif
ccflags-y += -DEXPORT_SYMTAB

obj-$(CONFIG_SAMSUNG_OIS_RUMBA_S4) += msm_ois_rumba_s4.o
obj-$(CONFIG_SAMSUNG_OIS_RUMBA_S6) += msm_ois_rumba_s6.o
obj-$(CONFIG_COMPANION3) += msm_companion3.o

