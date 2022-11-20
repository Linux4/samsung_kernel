#sdiodevhal
obj-$(CONFIG_SDIODEV)           := sdiodevhal.o

sdiodevhal-c-objs   += sdio_dev.o

ifeq ($(CONFIG_SDIODEV_TEST),y)
ccflags-y += -DSDIO_TEST_CASE6
#SDIO_TEST_CASE1
#SDIO_TEST_CASE2
#SDIO_TEST_CASE4
#SDIO_TEST_CASE5
#SDIO_TEST_CASE6
#SDIO_TEST_CASE7
#SDIO_TEST_CASE8
#SDIO_TEST_CASE9
sdiodevhal-c-objs += sdio_dev_test.o
endif

sdiodevhal-objs     += $(sdiodevhal-c-objs)

