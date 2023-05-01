#
#  makefile for tspdrv kernel module
#

obj-$(CONFIG_VIBETONZ) += vibrator.o
obj-$(CONFIG_MOTOR_DRV_MAX77693)	+= max77693_haptic.o

vibrator-objs +=tspdrv.o
# Makefile for Vibrator controlled by PMIC Regulator.
obj-$(CONFIG_MSM_VIBRATOR) += msm_vibrator.o
