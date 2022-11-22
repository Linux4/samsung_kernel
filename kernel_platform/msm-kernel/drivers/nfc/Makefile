# SPDX-License-Identifier: GPL-2.0
ifeq ($(CONFIG_SAMSUNG_ESE_ONLY),m)
obj-$(CONFIG_SAMSUNG_ESE_ONLY)	+= ese.o
ese-$(CONFIG_ESE_P3_LSI)	+= ese_p3.o
ese-$(CONFIG_SEC_NFC_LOGGER)	+= nfc_logger/nfc_logger.o
else
obj-$(CONFIG_ESE_P3_LSI)	+= ese_p3.o
obj-$(CONFIG_SEC_NFC_LOGGER)	+= nfc_logger/nfc_logger.o
endif

ifeq ($(CONFIG_SEC_STAR), m)
obj-$(CONFIG_SEC_STAR) = sec-star/sec_star.o

sec_star-y += \
                        sec-star/sec_star.o \
                        sec-star/protocol/ese_data.o \
                        sec-star/protocol/ese_iso7816_t1.o \
                        sec-star/hal/ese_i2c.o \
                        sec-star/hal/ese_spi.o \
                        sec-star/hal/ese_hal.o

sec_star-$(CONFIG_STAR_K250A) += sec-star/sec_k250a.o
else
obj-$(CONFIG_SEC_STAR) += sec-star/sec_star.o \
                        sec-star/protocol/ese_data.o \
                        sec-star/protocol/ese_memory.o \
                        sec-star/protocol/ese_iso7816_t1.o \
                        sec-star/hal/ese_i2c.o \
                        sec-star/hal/ese_spi.o \
                        sec-star/hal/ese_hal.o

obj-$(CONFIG_STAR_K250A) += sec-star/sec_k250a.o
endif
