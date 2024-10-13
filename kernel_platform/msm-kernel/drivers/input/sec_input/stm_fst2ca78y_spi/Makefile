TARGET = fst2ca78y_spi

ifeq ($(CONFIG_SAMSUNG_PRODUCT_SHIP), y)
  $(TARGET)-objs := stm_spi.o stm_core.o stm_fn.o stm_fw.o stm_dump.o stm_cmd.o
else
  $(TARGET)-objs := stm_spi.o stm_core.o stm_fn.o stm_fw.o stm_dump.o stm_cmd.o stm_mmap.o stm_vendor.o
endif

obj-$(CONFIG_TOUCHSCREEN_STM_FST2CA78Y_SPI) += $(TARGET).o

ccflags-y += -Wformat

