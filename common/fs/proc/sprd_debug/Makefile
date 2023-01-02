#
# Makefile for the Linux proc filesystem routines.
#

obj-$(CONFIG_SPRD_DEBUG) += sprd_debug.o
sprd_debug-$(CONFIG_SPRD_DEBUG_FRAME)  += proc_sprd_debug_frame.o
sprd_debug-$(CONFIG_PHYMEM_INFO)  += proc_phymem.o
sprd_debug-$(CONFIG_SPRD_DEBUG_SCHED_LOG)  += proc_sprd_debug.o
sprd_debug-$(CONFIG_USRPROCMEM_INFO)  += proc_usermeminfo.o
sprd_debug-$(CONFIG_USRPROCSTACK_INFO) += proc_userstack.o
sprd_debug-$(CONFIG_SPRD_CPU_RATE) += proc_cpu_usage.o
