#
# Makefile for TDMB Device Drivers.
#

ccflags-y += -Idrivers/media/tdmb
ccflags-y += -v

obj-$(CONFIG_TDMB) += tdmb.o tdmb_data.o

# FC8080
ccflags-$(CONFIG_TDMB_FC8080) += -Idrivers/media/tdmb/fc8080
obj-$(CONFIG_TDMB_FC8080) += tdmb_port_fc8080.o
obj-$(CONFIG_TDMB_FC8080) += fc8080/

# i/f
obj-$(CONFIG_TDMB_SPI) += tdmb_spi.o
obj-$(CONFIG_TDMB_EBI) += tdmb_ebi.o

