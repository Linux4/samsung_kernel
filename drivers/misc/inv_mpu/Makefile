
# Kernel makefile for motions sensors
#
#

# MPU
ifdef CONFIG_MPU_SENSORS_MPU3050_411
INV_MODULE_NAME := mpu3050
endif

ifdef CONFIG_MPU_SENSORS_MPU6050A2_411
INV_MODULE_NAME := mpu6050
endif

ifdef CONFIG_MPU_SENSORS_MPU6050B1_411
INV_MODULE_NAME := mpu6050
endif

obj-$(CONFIG_INV_SENSORS_411)	+= $(INV_MODULE_NAME).o

$(INV_MODULE_NAME)-objs += mpuirq.o
$(INV_MODULE_NAME)-objs += slaveirq.o
$(INV_MODULE_NAME)-objs += mpu-dev.o
$(INV_MODULE_NAME)-objs += mlsl-kernel.o
$(INV_MODULE_NAME)-objs += mldl_cfg.o
$(INV_MODULE_NAME)-objs += mldl_print_cfg.o
$(INV_MODULE_NAME)-objs += mpu6050_selftest.o

ifdef CONFIG_INPUT_YAS_MAGNETOMETER
$(INV_MODULE_NAME)-objs += compass/yas530_ext.o
endif

ifdef CONFIG_MPU_SENSORS_MPU6050A2_411
$(INV_MODULE_NAME)-objs += accel/mpu6050.o
endif

ifdef CONFIG_MPU_SENSORS_MPU6050B1_411
$(INV_MODULE_NAME)-objs += accel/mpu6050.o
endif

ifdef CONFIG_SENSORS_AK8975
$(INV_MODULE_NAME)-objs += compass/ak8975.o
endif

EXTRA_CFLAGS += -Idrivers/misc/inv_mpu
EXTRA_CFLAGS += -D__C99_DESIGNATED_INITIALIZER
EXTRA_CFLAGS += -DINV_CACHE_DMP=1

obj-$(CONFIG_MPU_SENSORS_TIMERIRQ_411)+= timerirq.o

obj-y			+= accel/
obj-y			+= compass/
