#
# Makefile for the fpga framework and fpga manager drivers.
#

# Core FPGA Manager Framework
obj-$(CONFIG_FPGA)			+= fpga-mgr.o

# FPGA Manager Drivers
obj-$(CONFIG_FPGA_MGR_SOCFPGA)		+= socfpga.o
obj-$(CONFIG_FPGA_MGR_ZYNQ_FPGA)	+= zynq-fpga.o
obj-$(CONFIG_SPI_TO_I2C_FPGA)		+= fpga_i2c_expander_dev.o fpga_i2c_expander_core.o
obj-$(CONFIG_POGO_FPGA)			+= pogo_fpga_dev.o
obj-$(CONFIG_SEC_ICE40XX_IRIS)		+= iris_fpga.o
