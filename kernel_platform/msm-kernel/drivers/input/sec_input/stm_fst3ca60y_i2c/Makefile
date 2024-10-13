TARGET = fst3ca60y_i2c


ifeq ($(CONFIG_SAMSUNG_PRODUCT_SHIP), y)
  $(TARGET)-objs := stm_i2c.o stm_core.o stm_fn.o stm_fw.o stm_dump.o stm_cmd.o
else
  $(TARGET)-objs := stm_i2c.o stm_core.o stm_fn.o stm_fw.o stm_dump.o stm_cmd.o stm_vendor.o
endif

obj-$(CONFIG_TOUCHSCREEN_STM_FST3CA60Y_I2C) += $(TARGET).o

ccflags-y += -Wformat

