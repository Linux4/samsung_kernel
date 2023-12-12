# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the input core drivers.
#

# Each configuration option enables a list of files.

obj-$(CONFIG_INPUT)		+= input-core.o
input-core-y := input.o input-compat.o input-mt.o input-poller.o ff-core.o

obj-$(CONFIG_INPUT_FF_MEMLESS)	+= ff-memless.o
obj-$(CONFIG_INPUT_POLLDEV)	+= input-polldev.o
obj-$(CONFIG_INPUT_SPARSEKMAP)	+= sparse-keymap.o
obj-$(CONFIG_INPUT_MATRIXKMAP)	+= matrix-keymap.o

obj-$(CONFIG_INPUT_LEDS)	+= input-leds.o
obj-$(CONFIG_INPUT_MOUSEDEV)	+= mousedev.o
obj-$(CONFIG_INPUT_JOYDEV)	+= joydev.o
obj-$(CONFIG_INPUT_EVDEV)	+= evdev.o
obj-$(CONFIG_INPUT_EVBUG)	+= evbug.o

obj-$(CONFIG_INPUT_KEYBOARD)	+= keyboard/
obj-$(CONFIG_INPUT_MOUSE)	+= mouse/
obj-$(CONFIG_INPUT_JOYSTICK)	+= joystick/
obj-$(CONFIG_INPUT_TABLET)	+= tablet/
obj-$(CONFIG_INPUT_TOUCHSCREEN)	+= touchscreen/
# Tab A9 code for SR-AX6739A-01-314 by xiongxiaoliang at 20230502 start
obj-$(CONFIG_INPUT_SAR)	+= sar/
# Tab A9 code for SR-AX6739A-01-314 by xiongxiaoliang at 20230502 end
obj-$(CONFIG_INPUT_MISC)	+= misc/

obj-$(CONFIG_INPUT_APMPOWER)	+= apm-power.o

obj-$(CONFIG_RMI4_CORE)		+= rmi4/

obj-$(CONFIG_SEC_INPUT_BOOSTER)	+= input_booster_lkm.o
ifdef CONFIG_SEC_INPUT_BOOSTER_MODE
	input_booster_lkm-$(CONFIG_SEC_INPUT_BOOSTER)	+= evdev_booster.o input_booster_mode.o
else
	input_booster_lkm-$(CONFIG_SEC_INPUT_BOOSTER)	+= evdev_booster.o input_booster.o
endif
input_booster_lkm-$(CONFIG_SEC_INPUT_BOOSTER_MTK)       += input_booster_mtk.o
ifdef CONFIG_MTK_DVFSRC
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
endif
