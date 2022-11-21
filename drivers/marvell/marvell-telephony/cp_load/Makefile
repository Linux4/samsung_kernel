ccflags-y += -Idrivers/marvell/marvell-telephony/shmem
ccflags-y += -Idrivers/marvell/marvell-telephony/errorhandler
cploaddev-objs := pxa_cp_load.o watchdog.o
ifeq ($(CONFIG_CPU_PXA988),y)
cploaddev-objs += pxa988_cp.o
endif
ifeq ($(CONFIG_CPU_PXA1088),y)
cploaddev-objs += pxa988_cp.o
endif
ifeq ($(CONFIG_CPU_PXA1L88),y)
cploaddev-objs += pxa988_cp.o
endif

obj-m	+= cploaddev.o
