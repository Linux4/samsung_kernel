#
# Exynos SoC Specific drivers
#

obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-pm.o
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-pmu.o
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-powermode.o
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-el3_mon.o
obj-$(CONFIG_HOTPLUG_CPU)	+= exynos-cpu_hotplug.o
obj-$(CONFIG_EXYNOS_MCINFO)	+= exynos-mcinfo.o
obj-$(CONFIG_EXYNOS_PD)		+= exynos-pd.o exynos-pd-dbg.o
obj-$(CONFIG_EXYNOS_RGT)	+= exynos-rgt.o

obj-$(CONFIG_ARCH_EXYNOS)	+= secmem.o
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-smc.o
obj-$(CONFIG_EXYNOS_SEC_LOGGER) += secprint.o

# CAL
obj-$(CONFIG_PWRCAL)            += pwrcal/
obj-$(CONFIG_EXYNOS_CHIPID)	+= exynos-chipid.o
obj-$(CONFIG_EXYNOS_REBOOT)	+= exynos-reboot.o

# CP
obj-$(CONFIG_SEC_SIPC_MODEM_IF)	+= pmu-cp.o
obj-$(CONFIG_SEC_MODEM_IF)	+= pmu-cp.o

obj-$(CONFIG_ECT)		+= ect_parser.o

obj-$(CONFIG_EXYNOS_HOTPLUG_GOVERNOR)	+= exynos-hotplug_governor.o

obj-$(CONFIG_EXYNOS_BCM)		+= exynos-bcm.o

# ACPM
obj-$(CONFIG_EXYNOS_ACPM)    	+= acpm/

# USI
obj-$(CONFIG_USI) += usi.o

obj-$(CONFIG_SEC_INCELL) += sec_incell.o