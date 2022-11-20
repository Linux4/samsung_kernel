# this make file is used when building modules for desktop linux (ubuntu, redhat, ...)
# export KERNEL_ROOT from outside to contain the root of the kernel headers needed to build the modules


DISPATCHER_OBJ := ntrig-ncp-driver.o ntrig-dispatcher-sysfs.o ntrig-common.o ntrig-dispatcher.o ntrig-direct-event-driver.o ntrig-dispatcher-sys-depend-android.o ntrig-mod-shared.o

ntrig_ts-objs  := $(DISPATCHER_OBJ)
obj-m += ntrig_ts.o 
SPI_OBJ :=  ntrig_spi.o ntrig_low_msg.o
ntrig_spi_mod-objs  := $(SPI_OBJ)
obj-m += ntrig_spi_mod.o

