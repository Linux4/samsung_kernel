#PATH := $(PREBUILD)/bin:$(PATH)

obj-$(CONFIG_CGX_DRIVER)           := cgxdrv.o

# C Object Files:
cgxdrv-c-objs   += CgxDriverLinux.o
cgxdrv-c-objs   += CgxDriverCore.o
cgxdrv-c-objs   += CgxDriverCoreCommon.o
cgxdrv-c-objs   += CgCpuLinux.o
cgxdrv-c-objs   += CgCpu.o
cgxdrv-c-objs   += platform.o


#cgxdrv-c-objs   += SPRD_FPGA_CPU.o
#cgxdrv-c-objs   += SPRD_FPGA_GPIO.o
#cgxdrv-c-objs   += SPRD_FPGA_DMA.o
#cgxdrv-c-objs   += SPRD_FPGA_CLK.o
#cgxdrv-c-objs   += SPRD_FPGA_INT.o
#cgxdrv-c-objs   += SPRD_FPGA_RF.o
#cgxdrv-c-objs   += gpsspi.o

cgxdrv-c-objs   += gps_gpio.o
cgxdrv-c-objs   += gps_dma.o

#bxd
#cgxdrv-objs     += gpsspi.o


cgxdrv-objs     += $(cgxdrv-c-objs)





#LOCAL_CFLAGS := \
#-DCGCORE_ACCESS_VIA_SPI
# Headers:
#   Unused: CgFeatures.h pkfuncs.h toolhelp.h
cgxdrv-hdrs     += CgBuildNumber.h
cgxdrv-hdrs     += CgReturnCodes.h
cgxdrv-hdrs     += CgTypes.h
cgxdrv-hdrs     += CgVersion.h
cgxdrv-hdrs     += CgCpu.h
cgxdrv-hdrs     += CgCpuOs.h
cgxdrv-hdrs     += SPRD_FPGA_CPU.h
cgxdrv-hdrs     += SPRD_FPGA_CLK.h
cgxdrv-hdrs     += SPRD_FPGA_DMA.h
cgxdrv-hdrs     += SPRD_FPGA_GPIO.h
cgxdrv-hdrs     += SPRD_FPGA_INT.h
cgxdrv-hdrs     += CgxDriverCoreCommon.h
cgxdrv-hdrs     += CgxDriverApi.h
cgxdrv-hdrs     += CgxDriverCore.h
cgxdrv-hdrs     += CgxDriverOs.h
cgxdrv-hdrs     += CgxDriverPlatform.h
cgxdrv-hdrs     += platform.h


#EXTRA_CFLAGS    += 	\
#	-I. \
#	-I. \
#	-I. \
#	-I.	\
#	-I.	\
#	-I. \
#    -D__LINUX__ \
#    -D$(MODE) \
#	$(CG_CPPFLAGS)


ifeq ($(KERNELRELEASE),)
# on first call from remote location we get into this path
# whilst on second call all is managed by the embedding kernel makefile

cgxdrv-cfiles    = ${cgxdrv-c-objs:.o=.c}

#default:: kmod_build

#kmod_build:: $(cgxdrv-cfiles) $(cgxdrv-hdrs)


# distclean:: clean

#grep::
#	grep $(G) $(cgxdrv-cfiles)

#hgrep::
#	grep $(G) $(cgxdrv-hdrs)

#clean::
#	rm -f *.o *.ko .*.cmd cgxdrv.mod.c $(cgxdrv-c-objs) $(obj-m)
#	rm -rf .tmp_versions

#tags::
#	$(MAKE) -C $(CG_KERNEL_DIR) SUBDIRS=$(CURDIR) tags
endif

# ### EOF ###
