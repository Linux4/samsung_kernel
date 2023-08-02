
ifneq ($(KERNELRELEASE),)

	autotst-objs := autotstdrv.o dispc.o dsi.o lcd_dummy.o mipi_dsih_api.o mipi_dsih_dphy.o mipi_dsih_hal.o
	obj-m := autotst.o

else
	PRJ_ROOT=/home/anli/andr4
	export ARCH=arm
	export CROSS_COMPILE=${PRJ_ROOT}/prebuilt/linux-x86/toolchain/arm-eabi-4.4.3/bin/arm-eabi-
	KNRL_DIR=${PRJ_ROOT}/out/target/product/sp8810gb/obj/KERNEL

all:
	make -C ${KNRL_DIR} M=`pwd` $@

clean:
	@rm -rf *.o .*.o.cmd *.mod.c *.ko .*.ko.cmd *.order *.symvers Makefile~ .tmp_versions

#@rmdir .tmp_versions

endif

