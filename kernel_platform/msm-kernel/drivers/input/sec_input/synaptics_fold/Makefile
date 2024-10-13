TARGET_I2C = synaptics_i2c_fold
TARGET_SPI = synaptics_spi_fold

$(TARGET_SPI)-objs := synaptics_spi.o synaptics_core.o synaptics_interrupt.o synaptics_fn.o synaptics_fw.o \
  			synaptics_dump.o synaptics_cmd.o synaptics_ic_setting.o
$(TARGET_I2C)-objs := synaptics_i2c.o synaptics_core.o synaptics_interrupt.o synaptics_fn.o synaptics_fw.o \
  			synaptics_dump.o synaptics_cmd.o synaptics_ic_setting.o

ifneq ($(CONFIG_SAMSUNG_PRODUCT_SHIP), y)
  $(TARGET_SPI)-objs += synaptics_vendor.o
  $(TARGET_I2C)-objs += synaptics_vendor.o
endif

obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_SPI_FOLDABLE) += $(TARGET_SPI).o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_FOLDABLE) += $(TARGET_I2C).o

ccflags-y += -Wformat
