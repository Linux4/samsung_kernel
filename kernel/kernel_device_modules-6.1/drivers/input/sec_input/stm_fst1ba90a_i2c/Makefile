TARGET = fst1ba90a_i2c

ifeq ($(CONFIG_SAMSUNG_PRODUCT_SHIP), y)
$(TARGET)-objs	:= stm_ts.o stm_fw.o stm_sec.o
else
$(TARGET)-objs	:= stm_ts.o stm_fw.o stm_sec.o stm_ts_tool_proc.o
endif

obj-$(CONFIG_TOUCHSCREEN_STM_FST1BA90A_I2C)	+= $(TARGET).o

ccflags-y += -Wformat
