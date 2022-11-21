TARGET = stm_ts

$(TARGET)-objs := stm_core.o stm_fn.o stm_fw.o stm_dump.o stm_cmd.o
obj-$(CONFIG_TOUCHSCREEN_STM) += $(TARGET).o

ccflags-y += -Wformat

