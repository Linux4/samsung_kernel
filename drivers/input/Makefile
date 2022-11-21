#
# Makefile for the input core drivers.
#

# Each configuration option enables a list of files.

obj-$(CONFIG_INPUT)		+= input-core.o
input-core-y := input.o input-compat.o input-mt.o ff-core.o
# HS60 add for SR-ZQL1695-01000000583 by liufurong at 20190724 start
obj-$(CONFIG_INPUT_TOUCHSCREEN)	+= sec_cmd.o
# HS60 add for SR-ZQL1695-01000000583 by liufurong at 20190724 end

obj-$(CONFIG_INPUT_FF_MEMLESS)	+= ff-memless.o
obj-$(CONFIG_INPUT_POLLDEV)	+= input-polldev.o
obj-$(CONFIG_INPUT_SPARSEKMAP)	+= sparse-keymap.o
obj-$(CONFIG_INPUT_MATRIXKMAP)	+= matrix-keymap.o

obj-$(CONFIG_INPUT_LEDS)	+= input-leds.o
obj-$(CONFIG_INPUT_MOUSEDEV)	+= mousedev.o
obj-$(CONFIG_INPUT_JOYDEV)	+= joydev.o
obj-$(CONFIG_INPUT_EVDEV)	+= evdev.o
obj-$(CONFIG_SEC_INPUT_BOOSTER)	+= input_booster.o
obj-$(CONFIG_ARCH_QCOM)	+= input_booster_qc.o
obj-$(CONFIG_INPUT_EVBUG)	+= evbug.o

obj-$(CONFIG_INPUT_KEYBOARD)	+= keyboard/
obj-$(CONFIG_INPUT_MOUSE)	+= mouse/
obj-$(CONFIG_INPUT_JOYSTICK)	+= joystick/
obj-$(CONFIG_INPUT_TABLET)	+= tablet/
obj-$(CONFIG_INPUT_TOUCHSCREEN)	+= touchscreen/
obj-$(CONFIG_SWTP_INT_INPUT_DEV) += rfcable_detect/
# HS50 code for HS50-4050 by xiongxiaoliang at 20201031 start
obj-$(CONFIG_SAR_MODE_DETECT) += sar_mode_detect/
# HS50 code for HS50-4050 by xiongxiaoliang at 20201031 end
obj-$(CONFIG_INPUT_MISC)	+= misc/

obj-$(CONFIG_INPUT_APMPOWER)	+= apm-power.o
obj-$(CONFIG_INPUT_KEYRESET)	+= keyreset.o
obj-$(CONFIG_INPUT_KEYCOMBO)	+= keycombo.o

obj-$(CONFIG_RMI4_CORE)		+= rmi4/

obj-$(CONFIG_INPUT_KEYRESET)	+= keyreset.o
obj-$(CONFIG_INPUT_KEYCOMBO)	+= keycombo.o
obj-$(CONFIG_SMI130)   += sensors/smi130/

obj-$(CONFIG_SILEAD_FP_PLATFORM) += fingerprint/silead/
obj-$(CONFIG_CDFINGER_FP) += fingerprint/cdfinger/
obj-$(CONFIG_FOCALTECH_FINGERPRINT) += fingerprint/focaltech/
obj-$(CONFIG_SYSFS_ADM) += fingerprint/
# HS70 code for HS70-26/HS70-131/HS70-161 by zhuqiang at 2019/10/11 end
# HS70 code for HS70-858 by chenlei at 2019/11/11 start
obj-$(CONFIG_EGIS_FINGERPRINT) += fingerprint/egis/
# HS70 code for HS70-858 by chenlei at 2019/11/11 end

