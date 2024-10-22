#
# Makefile for STM32 POGO keyboard drivers
#
TARGET = stm32_pogo_v3

$(TARGET)-objs := stm32_pogo_i2c_v3.o stm32_pogo_fw.o stm32_pogo_keyboard_v3.o stm32_pogo_keyboard_v3_common.o        \
                  stm32_pogo_core_v3.o stm32_pogo_interrupt_v3.o stm32_pogo_notifier_v3.o stm32_pogo_voting_v3.o      \
                  stm32_pogo_fn_v3.o stm32_pogo_cmd_v3.o stm32_pogo_keyboard_v3_row.o stm32_pogo_keyboard_v3_bypass.o \
                  stm32_pogo_touchpad_v3.o kbd_max77816_i2c.o

ifneq ($(filter y m, $(CONFIG_HALL_LOGICAL)),)
  $(TARGET)-objs += ../../misc/hall/hall_ic_logical.o
endif

obj-$(CONFIG_KEYBOARD_STM32_POGO_V3)   += $(TARGET).o

ccflags-y += -Wformat
