#
# Makefile for the Ilitek ili9881x touchscreen driver.
#

TARGET = ilitek_ts_il9881x

$(TARGET)-objs	:= ili9881x.o ili9881x_spi.o ili9881x_init.o ili9881x_ic.o ili9881x_touch.o ili9881x_mp.o ili9881x_hostdl.o ili9881x_node.o ili9881x_sec_fn.o ili9881x_sec_fn_test.o
ccflags-$(CONFIG_SAMSUNG_TUI)	+= -Idrivers/misc/tui

obj-$(CONFIG_TOUCHSCREEN_ILITEK_ILI9881X_SPI) += $(TARGET).o

ccflags-y += -Wformat
