#
# Makefile for Samsung Secure Element
#
# M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 start
ifneq ($(CONFIG_CUSTOM_FACTORY_BUILD),y)
ifeq ($(CONFIG_SEC_STAR), m)
obj-$(CONFIG_SEC_STAR) = snvmstar.o

snvmstar-y += sec_star.o \
			protocol/ese_data.o \
			protocol/ese_memory.o \
			protocol/ese_iso7816_t1.o \
			hal/ese_i2c.o \
			hal/ese_spi.o \
			hal/ese_hal.o \
			sec_k250a.o

snvmstar-$(CONFIG_STAR_V9RRP) += sec_s3fv9rrp.o
else
obj-$(CONFIG_SEC_STAR) += sec_star.o \
			protocol/ese_data.o \
			protocol/ese_memory.o \
			protocol/ese_iso7816_t1.o \
			hal/ese_i2c.o \
			hal/ese_spi.o \
			hal/ese_hal.o \
			sec_k250a.o

obj-$(CONFIG_STAR_V9RRP) += sec_v9rrp.o
endif
endif
# M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 end