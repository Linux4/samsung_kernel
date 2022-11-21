#
# Makefile for Samsung Secure Element
#

ifeq ($(CONFIG_SEC_STAR), m)
obj-$(CONFIG_SEC_STAR) = sec_star.o

sec_star-y += \
			sec_star.o \
			protocol/ese_data.o \
			protocol/ese_iso7816_t1.o \
			hal/ese_i2c.o \
			hal/ese_spi.o \
			hal/ese_hal.o

sec_star-$(CONFIG_STAR_K250A) += sec_k250a.o
else
obj-$(CONFIG_SEC_STAR) += sec_star.o \
			protocol/ese_data.o \
			protocol/ese_memory.o \
			protocol/ese_iso7816_t1.o \
			hal/ese_i2c.o \
			hal/ese_spi.o \
			hal/ese_hal.o

obj-$(CONFIG_STAR_K250A) += sec_k250a.o
endif
