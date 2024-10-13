# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the input core drivers.
#

# Each configuration option enables a list of files.

obj-$(CONFIG_INPUT)		+= input-core.o
input-core-y := input.o input-compat.o input-mt.o ff-core.o

obj-$(CONFIG_INPUT_TOUCHSCREEN) += sec_cmd.o

obj-$(CONFIG_INPUT_FF_MEMLESS)	+= ff-memless.o
obj-$(CONFIG_INPUT_POLLDEV)	+= input-polldev.o
obj-$(CONFIG_INPUT_SPARSEKMAP)	+= sparse-keymap.o
obj-$(CONFIG_INPUT_MATRIXKMAP)	+= matrix-keymap.o

obj-$(CONFIG_INPUT_LEDS)	+= input-leds.o
obj-$(CONFIG_INPUT_MOUSEDEV)	+= mousedev.o
obj-$(CONFIG_INPUT_JOYDEV)	+= joydev.o
obj-$(CONFIG_INPUT_EVDEV)	+= evdev.o
obj-$(CONFIG_SEC_INPUT_BOOSTER)	+= input_booster.o
obj-$(CONFIG_ARCH_MEDIATEK)	+= input_booster_mtk.o
obj-$(CONFIG_INPUT_EVBUG)	+= evbug.o

obj-$(CONFIG_INPUT_KEYBOARD)	+= keyboard/
obj-$(CONFIG_INPUT_MOUSE)	+= mouse/
obj-$(CONFIG_INPUT_JOYSTICK)	+= joystick/
obj-$(CONFIG_INPUT_TABLET)	+= tablet/
obj-$(CONFIG_INPUT_TOUCHSCREEN)	+= touchscreen/
obj-$(CONFIG_INPUT_MISC)	+= misc/

obj-$(CONFIG_INPUT_APMPOWER)	+= apm-power.o

obj-$(CONFIG_RMI4_CORE)		+= rmi4/
obj-$(CONFIG_TYHX_HX9035)   += hx9035/
obj-$(CONFIG_MTK_FINGERPRINT_SUPPORT)  += fingerprint/
obj-$(CONFIG_INPUT_ABOV)    += abov/
obj-$(CONFIG_AW9610X_SAR) += aw9610x_sar/
obj-$(CONFIG_HX9031A_SAR) += hx9031a_sar/
obj-$(CONFIG_SX9375_SAR) += sx9375_sar/
obj-$(CONFIG_HX9023S_SAR) += hx9023_sar/
obj-$(CONFIG_HX9031AS_SAR) += hx9031as_sar/

#+S96818AA1-1936, liuling3.wt,ADD, 2023/06/05, sar sx9338 & sx9328 bringup
obj-$(CONFIG_SX9338_SAR) += sx9338_sar/
obj-$(CONFIG_SX9328_SAR) += sx9328_sar/
#-S96818AA1-1936, liuling3.wt,ADD, 2023/06/05, sar sx9338 & sx9328 bringup
ccflags-y += -I$(srctree)/drivers/misc/mediatek/performance/include/