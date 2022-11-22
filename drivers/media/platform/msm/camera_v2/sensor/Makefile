ccflags-y += -Idrivers/media/platform/msm/camera_v2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/common
ccflags-y += -Idrivers/media/platform/msm/camera_v2/msm_vb2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/camera
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/io
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/cci
obj-$(CONFIG_MSMB_CAMERA) += cci/ io/ csiphy/ csid/ actuator/ eeprom/ ois/ flash/ ir_led/ ir_cut/
obj-$(CONFIG_MSMB_CAMERA) += laser_led/
obj-$(CONFIG_MSM_CAMERA_SENSOR) += msm_sensor_init.o msm_sensor_driver.o msm_sensor.o

# +bug515614 yinjie1.wt, add,2019/12/02,86118 project camera kernel code for bring up(add eeprom so)
obj-$(CONFIG_MSM_CAMERA_SENSOR) += msm_eeprom_hi846.o
obj-$(CONFIG_MSM_CAMERA_SENSOR) += msm_eeprom_gc5035.o
# -bug515614 yinjie1.wt, add,2019/12/02,86118 project camera kernel code for bring up(add eeprom so)