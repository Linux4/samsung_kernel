#
# Makefile for the GalaxyCore touchscreen driver.
#
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/O8/
obj-$(CONFIG_TOUCHSCREEN_GALAXYCORE_IC_GC7272) += galaxycore.o
galaxycore-y    +=  gcore_drv_main.o
galaxycore-y    +=  gcore_drv_mtk.o
galaxycore-y    +=  gcore_drv_fw_update.o
galaxycore-y    +=  gcore_drv_interface.o
galaxycore-y    +=  gcore_drv_mp_test.o