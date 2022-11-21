#
# Makefile for STM32 POGO keyboard drivers
#
TARGET = stm32_pogo
TARGETV2 = stm32_pogo_v2

ifeq ($(CONFIG_HALL_LOGICAL), $(filter $(CONFIG_HALL_LOGICAL), y m))
  $(TARGET)-objs := stm32_pogo_i2c.o stm32_pogo_fw.o stm32_pogo_keyboard.o stm32_pogo_touchpad.o ../../misc/hall/hall_ic_logical.o
  $(TARGETV2)-objs := stm32_pogo_i2c.o stm32_pogo_fw.o stm32_pogo_keyboard_v2.o stm32_pogo_touchpad.o ../../misc/hall/hall_ic_logical.o
else
  $(TARGET)-objs := stm32_pogo_i2c.o stm32_pogo_fw.o stm32_pogo_keyboard.o stm32_pogo_touchpad.o
  $(TARGETV2)-objs := stm32_pogo_i2c.o stm32_pogo_fw.o stm32_pogo_keyboard_v2.o stm32_pogo_touchpad.o
endif

obj-$(CONFIG_KEYBOARD_STM32_POGO)   += $(TARGET).o
obj-$(CONFIG_KEYBOARD_STM32_POGO_V2)   += $(TARGETV2).o

ccflags-y += -Wformat
