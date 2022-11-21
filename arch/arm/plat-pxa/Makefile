#
# Makefile for code common across different PXA processor families
#

obj-y	:= dma.o usb.o

obj-$(CONFIG_PXA3xx)		+= mfp.o
obj-$(CONFIG_PXA95x)		+= mfp.o
obj-$(CONFIG_ARCH_MMP)		+= mfp.o
obj-$(CONFIG_CPU_PXA988)	+= clock.o dvfs.o
obj-$(CONFIG_CPU_MMP3)		+= clock.o dvfs.o

obj-$(CONFIG_HAVE_PWM)		+= pwm.o
obj-$(CONFIG_PXA_PANIC_FLUSH)	+= panic.o
obj-$(CONFIG_PXA_SSP)		+= ssp.o
obj-$(CONFIG_DEBUG_FS)		+= debugfs.o
obj-$(CONFIG_DUMP_REGS)		+= dump_regs.o
