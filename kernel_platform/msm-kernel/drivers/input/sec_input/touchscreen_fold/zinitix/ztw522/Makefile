#
# Makefile for the Zinitix Touchscreen driver
#
TARGET = zinitix_ts

$(TARGET)-objs := zinitix_ts.o
obj-$(CONFIG_TOUCHSCREEN_ZINITIX_ZTW522)	+= $(TARGET).o

ccflags-y += -Wformat