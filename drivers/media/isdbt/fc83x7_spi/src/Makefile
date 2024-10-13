ifeq ($(CONFIG_ISDBT_FC83X7_SPI),y)

DRV_NAME = isdbt_fc83x7

#EXTRA_CFLAGS += -DDLOG
#EXTRA_CFLAGS += -DSEC_ENABLE_13SEG_BOOST

ccflags-y := -Idrivers/media/isdbt/fc83x7_spi/inc
obj-$(CONFIG_ISDBT_FC83X7_SPI) += bbm.o \
							fc8300.o \
							fc8300_bb.o \
							fc8300_i2c.o \
							fc8300_isr.o \
							fc8300_ppi.o \
							fc8300_spi.o \
							fc8300_tun.o \
							fc8300_tun_table.o \
							fc8350_bb.o \
							fc8350_tun.o \
							fc8350_tun_table.o \
							fci_hal.o \
							fci_hpi.o \
							fci_oal.o \
							fci_ringbuffer.o \
							fci_tun.o

endif
