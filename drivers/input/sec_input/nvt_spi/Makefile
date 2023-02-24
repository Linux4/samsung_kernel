TARGET_SPI = nvt_ts_spi

$(TARGET_SPI)-objs := nvt_spi.o nvt_core.o nvt_fn.o nvt_fw_zero.o nvt_dump.o nvt_cmd.o

obj-$(CONFIG_TOUCHSCREEN_NVT_SPI_ZERO) += $(TARGET_SPI).o

ccflags-y += -Wformat

