# SPDX-License-Identifier: GPL-2.0

# CAL
obj-$(CONFIG_CAL_IF)		+= cal-if/
obj-$(CONFIG_EXYNOS_PD)         += exynos-pd.o
obj-$(CONFIG_EXYNOS_PD_EL3)       += exynos-el3_mon.o
obj-$(CONFIG_EXYNOS_PD_DBG)	+= exynos-pd-dbg.o

obj-$(CONFIG_EXYNOS_PMU)	+= exynos-pmu.o

obj-$(CONFIG_EXYNOS_PMU_ARM_DRIVERS)	+= exynos3250-pmu.o exynos4-pmu.o \
					exynos5250-pmu.o exynos5420-pmu.o
# ACPM
obj-$(CONFIG_EXYNOS_ACPM)       += acpm/

obj-$(CONFIG_EXYNOS_FLEXPMU_DBG)        += exynos-flexpmu-dbg.o
obj-$(CONFIG_EXYNOS_PM_DOMAINS) += pm_domains.o
obj-$(CONFIG_EXYNOS_PM)		+= exynos-pm.o
obj-$(CONFIG_EXYNOS_CHIPID)	+= exynos-chipid.o

# Exynos Memlogger
obj-$(CONFIG_EXYNOS_MEMORY_LOGGER)      += memlogger.o

# Exynos Subsystem
obj-$(CONFIG_EXYNOS_SYSTEM_EVENT)       += sysevent.o sysevent_notif.o

# Exynos Image Loader
obj-$(CONFIG_EXYNOS_IMGLOADER)       += imgloader.o

# DEBUG
obj-$(CONFIG_EXYNOS_DEBUG)      += debug/

obj-$(CONFIG_EXYNOS_BCM_DBG)    += exynos-bcm_dbg.o exynos-bcm_dbg-dt.o
obj-$(CONFIG_EXYNOS_BCM_DBG_DUMP)       += exynos-bcm_dbg-dump.o

# ECT
obj-$(CONFIG_ECT)               += ect_parser/

# reboot
obj-$(CONFIG_EXYNOS_REBOOT)	+= exynos-reboot.o

# USI_V2
obj-$(CONFIG_USI_V2)		+= usi_v2.o

obj-$(CONFIG_EXYNOS_CHIPID_V2)  += exynos-chipid_v2.o

obj-$(CONFIG_EXYNOS_SEH)        += exynos-seh.o

obj-$(CONFIG_EXYNOS_SECURE_LOG) += exynos-seclog.o

obj-$(CONFIG_EXYNOS_TZASC_FAIL_DETECTOR) += exynos-tzasc.o

# SDM
obj-$(CONFIG_EXYNOS_SDM)	+= exynos-sdm.o

# DVFS MANAGER
obj-$(CONFIG_EXYNOS_DVFS_MANAGER)       += dm/

# EXYNOS_PM_QOS
obj-$(CONFIG_EXYNOS_PM_QOS)	+= pm_qos/

# CPU Power Management
obj-$(CONFIG_EXYNOS_CPUPM)     += exynos-cpupm.o

# CPU Hotplug
obj-$(CONFIG_EXYNOS_CPUHP)      += exynos-cpuhp.o

# Exynos User Frequency & Cstate Control
obj-$(CONFIG_EXYNOS_UFCC)       += exynos-ufcc.o

# Exynos CPU Performance
obj-$(CONFIG_EXYNOS_PERF)	+= xperf/

#GNSS Interface Driver
obj-$(CONFIG_EXYNOS_GNSS_IF) += gnssif/

# CPIF
obj-$(CONFIG_EXYNOS_MODEM_IF) += cpif/

# secmem
obj-$(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION) += secmem.o
