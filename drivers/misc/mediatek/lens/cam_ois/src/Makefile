ifneq ($(CONFIG_MTK_LENS_USE_MODULE), y)

ifdef CONFIG_CAMERA_SAMSUNG_MODEL_NAME
CAMERA_OIS_PROJECT_FOLDER := camera_project/$(subst ",,$(CONFIG_CAMERA_SAMSUNG_MODEL_NAME))
else
CAMERA_OIS_PROJECT_FOLDER := camera_project/default
endif

$(info cam_ois project dir $(CAMERA_OIS_PROJECT_FOLDER))

subdir-ccflags-$(CONFIG_CAMERA_OIS)    += -I$(CAMERA_OIS_PROJECT_FOLDER)
obj-$(CONFIG_CAMERA_OIS)               += $(CAMERA_OIS_PROJECT_FOLDER)/cam_ois_mcu_fw.o
obj-$(CONFIG_CAMERA_OIS)               += $(CAMERA_OIS_PROJECT_FOLDER)/cam_ois_power.o
obj-$(CONFIG_CAMERA_OIS)               += cam_ois_aois_if.o
obj-$(CONFIG_CAMERA_OIS)               += cam_ois.o
obj-$(CONFIG_CAMERA_OIS)               += cam_ois_drv.o
obj-$(CONFIG_CAMERA_OIS)               += cam_ois_sysfs.o
obj-$(CONFIG_CAMERA_OIS)               += cam_ois_i2c.o

endif
