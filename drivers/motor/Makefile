#
#  makefile for tspdrv kernel module
#

obj-$(CONFIG_VIBETONZ) += vibrator.o
obj-$(CONFIG_MOTOR_DRV_MAX77693)	+= max77693_haptic.o

ifdef CONFIG_SEC_PRODUCT_8960
vibrator-objs +=tspdrv_msm8960.o
else
vibrator-objs +=tspdrv.o
endif
