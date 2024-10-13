obj-$(CONFIG_TZDEV) += tzdev.o

ifeq ($(CONFIG_TZDEV), m)
tzdev-y += core/subsystem.o
endif

ccflags-y += -I$(srctree)/drivers/misc/tzdev/4.2.1/include
ccflags-y += -I$(srctree)/drivers/misc/tzdev/4.2.1
ccflags-$(CONFIG_SAMSUNG_GKI_KERNEL) += -DANDROID_GKI_VFS_EXPORT_ONLY=VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver
ccflags-$(CONFIG_TZDEV_DEPLOY_TZAR) += -D"_STR(s)=\#s" -D"KBUILD_SRC=_STR($(KBUILD_SRC))"
ccflags-$(CONFIG_SAMSUNG_GKI_KERNEL) += -DCONFIG_SAMSUNG_GKI_KERNEL

# Core
tzdev-y						+= core/cdev.o
tzdev-y						+= core/cred.o
tzdev-y						+= core/event.o
tzdev-y						+= core/iwio.o
tzdev-y						+= core/iwservice.o
tzdev-y						+= core/iwsock.o
tzdev-y						+= core/kthread_pool.o
tzdev-y						+= core/log.o
tzdev-y						+= core/mem.o
tzdev-y						+= core/notifier.o
tzdev-y						+= core/ree_time.o
tzdev-y						+= core/sysdep.o
tzdev-y						+= core/tzdev.o
tzdev-y						+= core/uiwsock.o
tzdev-y						+= core/umem.o
tzdev-$(CONFIG_TZLOG)				+= core/iwlog.o
tzdev-$(CONFIG_TZDEV_DEPLOY_TZAR)		+= core/deploy_tzar.o

ifeq ($(CONFIG_MSM_SCM), y)
tzdev-y						+= core/qc_platform.o
tzdev-y						+= core/qc_clocks.o
else
tzdev-y						+= core/platform.o
endif

# Lib
tzdev-y						+= lib/circ_buf.o
tzdev-y						+= lib/circ_buf_packet.o

# Extensions
tzdev-$(CONFIG_TZDEV_BOOST)			+= extensions/boost.o
tzdev-$(CONFIG_TZDEV_CPUFREQ_TRANSITION)	+= extensions/cpufreq_trans.o
tzdev-$(CONFIG_TZDEV_HOTPLUG)			+= extensions/hotplug.o
tzdev-$(CONFIG_TZIRS)				+= extensions/irs.o
tzdev-$(CONFIG_TZ_NWFS)				+= extensions/fsdev.o
tzdev-$(CONFIG_TZ_SCMA)				+= extensions/scma.o
tzdev-$(CONFIG_ION_FD2PHYS)			+= extensions/ion_fd2phys_v$(CONFIG_ION_FD2PHYS_VARIANT).o

# TEE client API
tzdev-y						+= teec/context.o
tzdev-y						+= teec/misc.o
tzdev-y						+= teec/session.o
tzdev-y						+= teec/shared_memory.o

# Debug
tzdev-$(CONFIG_TZPROFILER)			+= debug/profiler.o
tzdev-$(CONFIG_TZ_BOOT_LOG)			+= debug/iw_boot_log.o
tzdev-$(CONFIG_TZ_PANIC_DUMP)			+= debug/panic_dump.o
tzdev-$(CONFIG_TZ_PMF)				+= debug/pmf.o
tzdev-$(CONFIG_TZDEV_TRACING)			+= debug/trace.o
