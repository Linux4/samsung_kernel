# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_ARCH_EXYNOS)       += exynos-el3_mon.o
obj-$(CONFIG_ARCH_EXYNOS)       += exynos-smc.o
# CAL
obj-$(CONFIG_CAL_IF)		+= cal-if/
obj-$(CONFIG_EXYNOS_PD)        += exynos-pd.o exynos-pd-dbg.o

obj-$(CONFIG_ECT)		+= ect_parser.o
obj-$(CONFIG_EXYNOS_PMU)	+= exynos-pmu.o

obj-$(CONFIG_EXYNOS_PMU_ARM_DRIVERS)	+= exynos3250-pmu.o exynos4-pmu.o \
					exynos5250-pmu.o exynos5420-pmu.o
# ACPM
obj-$(CONFIG_EXYNOS_ACPM)       += acpm/

obj-$(CONFIG_EXYNOS_RGT)        += exynos-rgt.o
obj-$(CONFIG_EXYNOS_FLEXPMU_DBG)        += exynos-flexpmu-dbg.o
obj-$(CONFIG_EXYNOS_PM_DOMAINS) += pm_domains.o
obj-$(CONFIG_EXYNOS_CHIPID)	+= exynos-chipid.o

# SCI
obj-$(CONFIG_EXYNOS_SCI)	+= exynos-sci.o

# ECT
obj-$(CONFIG_ECT)               += ect_parser.o

# USI_V2
obj-$(CONFIG_USI_V2) += usi_v2.o

# reboot
obj-$(CONFIG_EXYNOS_REBOOT)	+= exynos-reboot.o

# DEBUG
obj-$(CONFIG_EXYNOS_DEBUG)       += debug/

obj-$(CONFIG_EXYNOS_BCM_DBG)    += exynos-bcm_dbg.o exynos-bcm_dbg-dt.o
obj-$(CONFIG_EXYNOS_BCM_DBG_DUMP)       += exynos-bcm_dbg-dump.o

# DVFS
obj-$(CONFIG_EXYNOS_DVFS_MANAGER)	+= exynos-dm.o
obj-$(CONFIG_EXYNOS_MCINFO)	+= exynos-mcinfo.o

#PM
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-powermode.o
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-pm.o

# CPU HOTPLUG
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-cpuhp.o

# CPU Power Management
obj-$(CONFIG_EXYNOS_CPUPM)	+= exynos-cpupm.o

# Exynos Secure Log
obj-$(CONFIG_EXYNOS_SECURE_LOG)	+= exynos-seclog.o

# Exynos Samsung Secure Platform
obj-$(CONFIG_EXYNOS_SSP)	+= exynos-ssp.o

# OCP
obj-$(CONFIG_EXYNOS_OCP)        += exynos-ocp.o

# HIU
obj-$(CONFIG_EXYNOS_PSTATE_HAFM)        += exynos-hiu.o

# Exynos PPMPU fail detector
obj-$(CONFIG_EXYNOS_PPMPU_FAIL_DETECTOR)	+= exynos-ppmpu.o

# Exynos TZASC fail detector
obj-$(CONFIG_EXYNOS_TZASC_FAIL_DETECTOR)	+= exynos-tzasc.o

# HDCP
obj-$(CONFIG_EXYNOS_HDCP2)  += exynos-hdcp/

# secmem
obj-$(CONFIG_ARCH_EXYNOS)       += secmem.o

# SDM
obj-$(CONFIG_EXYNOS_SDM) += exynos-sdm.o

# CPIF
obj-$(CONFIG_EXYNOS_MODEM_IF) += cpif/

# DIT IP packet translator
obj-$(CONFIG_HW_FORWARD)	+= dit/

# GNSS interface driver
obj-$(CONFIG_EXYNOS_GNSS_IF) += gnssif/

# Exynos CPU Performance
obj-$(CONFIG_EXYNOS_CPU_PERF)	+= exynos_cpu_perf/

obj-$(CONFIG_EXYNOS_DEBUG_FREQ)	+= exynos_debug_freq.o
