# SPDX-License-Identifier: GPL-2.0

obj-$(CONFIG_EXYNOS_ASV_ARM)	+= exynos5422-asv.o
obj-$(CONFIG_EXYNOS_CHIPID)	+= exynos_chipid.o
exynos_chipid-y			+= exynos-chipid.o exynos-asv.o

obj-$(CONFIG_EXYNOS_USI)	+= exynos-usi.o

obj-$(CONFIG_EXYNOS_PMU)	+= exynos-pmu.o
obj-$(CONFIG_EXYNOS_PMU_IF)	+= exynos-pmu-if.o

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
obj-$(CONFIG_EXYNOS_ESCAV2)	+= esca_v2/
# ECT
obj-$(CONFIG_ECT)               += ect_parser/
obj-$(CONFIG_EXYNOS_PM) 	+= exynos-pm/

obj-$(CONFIG_SOC_SAMSUNG)       += exynos/

# CPU Power Management
obj-$(CONFIG_EXYNOS_CPUPM)      += exynos-cpupm.o
obj-$(CONFIG_EXYNOS_CPUHP)      += exynos-cpuhp.o
obj-$(CONFIG_EXYNOS_IDLE_LAT)	+= exynos-idle-lat.o

# Exynos User Frequency & Cstate Control
obj-$(CONFIG_EXYNOS_UFCC)	+= exynos-ufcc.o

# AFM
obj-$(CONFIG_EXYNOS_AFM)        += exynos-afm.o

# MPAM
obj-$(CONFIG_EXYNOS_MPAM_ARCH) += mpam/mpam_arch.o

# DVFS Manager
obj-y				+= dm/

# Exynos WoW
obj-$(CONFIG_EXYNOS_WOW)       += exynos-wow.o

# EXYNOS_PM_QOS
obj-$(CONFIG_EXYNOS_PM_QOS)	+= pm_qos/

# SCI
obj-$(CONFIG_EXYNOS_SCI)	+= exynos_sci.o
exynos_sci-$(CONFIG_EXYNOS_SCI)		+= exynos-sci.o
exynos_sci-$(CONFIG_EXYNOS_LLCGOV)	+= exynos-llcgov.o

obj-$(CONFIG_EXYNOS_SCI_DBG)	+= exynos-sci_dbg.o

# Exynos Isolated CryptoManager
obj-$(CONFIG_EXYNOS_ICM)	+= exynos-icm.o

# Exynos CPU Performance
obj-$(CONFIG_EXYNOS_PERF)       += xperf/

# EXYNOS_BCM
obj-$(CONFIG_EXYNOS_BCM_DBG)    += exynos-bcm.o
obj-$(CONFIG_EXYNOS_BCM_DBG_DUMP)       += exynos-bcm_dbg-dump.o
exynos-bcm-y			+= exynos-bcm_dbg.o exynos-bcm_dbg-dt.o
exynos-bcm-$(CONFIG_SOC_S5E9935) += exynos-bcm_dbg-9935.o
exynos-bcm-$(CONFIG_SOC_S5E9945) += exynos-bcm_dbg-9945.o
exynos-bcm-$(CONFIG_SOC_S5E8845) += exynos-bcm_dbg-8845.o

# CPIF
obj-$(CONFIG_EXYNOS_MODEM_IF)	+= cpif/

# GNSSIF
obj-$(CONFIG_EXYNOS_GNSS_IF)	+= gnssif/
obj-$(CONFIG_EXYNOS_GNSS_IF_SPI)       += gnssif_spi/

# Profiler Multi IP Governor
obj-$(CONFIG_EXYNOS_MAIN_PROFILER)	+= profiler/

# Exynos Samsung Secure Platform
obj-$(CONFIG_EXYNOS_SSP)        += exynos-ssp.o

# H/W Tailoring System
obj-$(CONFIG_HTS)		+= hts/
# Memory Hierarchy DVFS
obj-$(CONFIG_SAMSUNG_MHDVFS)	+= mhdvfs.o
