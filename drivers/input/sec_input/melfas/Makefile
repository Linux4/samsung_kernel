TARGET = melfas_ts

$(TARGET)-objs := melfas_core.o melfas_fn.o melfas_interrupt.o melfas_fw.o melfas_dump.o melfas_cmd.o melfas_vendor.o
obj-$(CONFIG_TOUCHSCREEN_MELFAS) += $(TARGET).o


ccflags-y += -Wformat

