obj-$(CONFIG_TZDEV) += tzdev.o

ifeq ($(CONFIG_TZDEV), m)
tzdev-y += core/subsystem.o
endif

ccflags-y += -I$(srctree)/drivers/misc/tzdev/include
ccflags-y += -I$(srctree)/drivers/misc/tzdev
ccflags-$(CONFIG_TZDEV_DEPLOY_TZAR) += -D"_STR(s)=\#s" -D"KBUILD_SRC=_STR($(KBUILD_SRC))"

# Core
tzdev-y						+= core/cdev.o
tzdev-y						+= core/cred.o
tzdev-y						+= core/event.o
tzdev-y						+= core/iwio.o
tzdev-y						+= core/iwservice.o
tzdev-y						+= core/iwsock.o
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

tzdev-$(CONFIG_TZDEV_KTHREADS_NOTIFIER)	+= core/kthread_pool_notifier.o
tzdev-$(CONFIG_TZDEV_KTHREADS_SMPBOOT)	+= core/kthread_pool_smpboot.o

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
tzdev-$(CONFIG_TZDEV_HOTPLUG)			+= extensions/hotplug.o
tzdev-$(CONFIG_TZIRS)				+= extensions/irs.o
tzdev-$(CONFIG_TZ_NWFS)				+= extensions/fsdev.o
tzdev-$(CONFIG_TZ_SCMA)				+= extensions/scma.o

ifeq ($(CONFIG_SOC_EXYNOS9830), y)
tzdev-$(CONFIG_ION_FD2PHYS)		+= extensions/ion_fd2phys_v1.o
ccflags-y += -DTZDEV_BOOST_CLUSTER_2
else ifeq ($(CONFIG_SOC_EXYNOS9820), y)
tzdev-$(CONFIG_ION_FD2PHYS)		+= extensions/ion_fd2phys_v1.o
ccflags-y += -DTZDEV_BOOST_CLUSTER_2
else ifeq ($(CONFIG_SOC_EXYNOS9630), y)
tzdev-$(CONFIG_ION_FD2PHYS)		+= extensions/ion_fd2phys_v1.o
ccflags-y += -DTZDEV_BOOST_CLUSTER_1
else ifeq ($(CONFIG_SOC_EXYNOS9610), y)
tzdev-$(CONFIG_ION_FD2PHYS)		+= extensions/ion_fd2phys_v1.o
ccflags-y += -DTZDEV_BOOST_CLUSTER_1
else ifeq ($(CONFIG_SOC_EXYNOS7885), y)
tzdev-$(CONFIG_ION_FD2PHYS)		+= extensions/ion_fd2phys_v1.o
ccflags-y += -DTZDEV_BOOST_CLUSTER_1
else ifeq ($(CONFIG_SOC_EXYNOS3830), y)
tzdev-$(CONFIG_ION_FD2PHYS)		+= extensions/ion_fd2phys_v1.o
ccflags-y += -DTZDEV_BOOST_CLUSTER_1
else ifeq ($(CONFIG_ARCH_RANCHU), y)
tzdev-$(CONFIG_ION_FD2PHYS)		+= extensions/ion_fd2phys_v1.o
else
tzdev-$(CONFIG_ION_FD2PHYS)		+= extensions/ion_fd2phys.o
ccflags-y += -DTZDEV_BOOST_CLUSTER_1
endif

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

