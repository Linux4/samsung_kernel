# SPDX-License-Identifier: GPL-2.0

obj-$(CONFIG_EXYNOS_ASV_ARM)	+= exynos5422-asv.o

obj-$(CONFIG_EXYNOS_CHIPID)	+= exynos-chipid.o exynos-asv.o
obj-$(CONFIG_EXYNOS_PMU)	+= exynos-pmu.o
obj-$(CONFIG_EXYNOS_PMU_IF)	+= exynos-pmu-if.o

ifdef CONFIG_SOC_S5E9925
obj-$(CONFIG_EXYNOS_FLEXPMU_DBG)            += exynos-flexpmu-dbg.o
endif

obj-$(CONFIG_EXYNOS_PMU_ARM_DRIVERS)	+= exynos3250-pmu.o exynos4-pmu.o \
					exynos5250-pmu.o exynos5420-pmu.o
obj-$(CONFIG_EXYNOS_PM_DOMAINS) += pm_domains.o
obj-$(CONFIG_EXYNOS_REGULATOR_COUPLER) += exynos-regulator-coupler.o

obj-$(CONFIG_SAMSUNG_PM_CHECK)	+= s3c-pm-check.o
obj-$(CONFIG_SAMSUNG_PM_DEBUG)	+= s3c-pm-debug.o
obj-$(CONFIG_EXYNOS_PD)		+= exynos-pd/
# CAL
obj-$(CONFIG_CAL_IF)            += cal-if/
# ACPM
obj-$(CONFIG_EXYNOS_ACPM)	+= acpm/
# ECT
obj-$(CONFIG_ECT)               += ect_parser/
obj-$(CONFIG_EXYNOS_PM) 	+= exynos-pm/

# SOC Exynos Kernel DD
obj-$(CONFIG_SOC_SAMSUNG)	+= exynos/

# CPIF
obj-$(CONFIG_EXYNOS_MODEM_IF)	+= cpif/

# GNSSIF
obj-$(CONFIG_EXYNOS_GNSS_IF)	+= gnssif/

# EXYNOS_PM_QOS
obj-$(CONFIG_EXYNOS_PM_QOS)	+= pm_qos/

# SCI
obj-$(CONFIG_EXYNOS_SCI)	+= exynos_sci.o
exynos_sci-$(CONFIG_EXYNOS_SCI)		+= exynos-sci.o
exynos_sci-$(CONFIG_EXYNOS_LLCGOV)	+= exynos-llcgov.o

obj-$(CONFIG_EXYNOS_SCI_DBG)	+= exynos-sci_dbg.o

# Exynos SEH
obj-$(CONFIG_EXYNOS_SEH)        += exynos-seh.o

# Exynos Secure Log
obj-$(CONFIG_EXYNOS_SECURE_LOG) += exynos-seclog.o

# Exynos Isolated CryptoManager
obj-$(CONFIG_EXYNOS_ICM)	+= exynos-icm.o

# EXYNOS_BCM
obj-$(CONFIG_EXYNOS_BCM_DBG)    += exynos-bcm.o
obj-$(CONFIG_EXYNOS_BCM_DBG_DUMP)       += exynos-bcm_dbg-dump.o
exynos-bcm-y			+= exynos-bcm_dbg.o exynos-bcm_dbg-dt.o
exynos-bcm-$(CONFIG_SOC_S5E9935) += exynos-bcm_dbg-9925.o
exynos-bcm-$(CONFIG_SOC_S5E8835) += exynos-bcm_dbg-8835.o
exynos-bcm-$(CONFIG_SOC_S5E8535) += exynos-bcm_dbg-8535.o

# DVFS MANAGER
obj-y			       += dm/

# Exynos WoW
obj-$(CONFIG_EXYNOS_WOW)       += exynos-wow.o

# CPU Hotplug
obj-$(CONFIG_EXYNOS_CPUHP)      += exynos-cpuhp.o

# CPU Power Management
obj-$(CONFIG_EXYNOS_CPUPM)      += exynos-cpupm.o

# Exynos User Frequency & Cstate Control
obj-$(CONFIG_EXYNOS_UFCC)	+= exynos-ufcc.o

# AFM
obj-$(CONFIG_EXYNOS_AFM)        += exynos-afm.o

# Exynos Samsung Secure Platform
obj-$(CONFIG_EXYNOS_SSP)	+= strong/

# Exynos Samsung Secure Platform OS
obj-$(CONFIG_CAMELLIA)		+= camellia/

# Exynos CPU Performance
obj-$(CONFIG_EXYNOS_PERF)       += xperf/

# Profiler Multi IP Governor
obj-$(CONFIG_EXYNOS_PROFILER)         += profiler/
