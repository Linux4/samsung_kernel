#
# Makefile for sensor chip drivers.
#

obj-$(CONFIG_THERMAL)		+= thermal_sys.o
obj-$(CONFIG_THERMAL)		+= sprd_thm.o
thermal_sys-y			+= thermal_core.o

# interface to/from other layers providing sensors
thermal_sys-$(CONFIG_THERMAL_HWMON)		+= thermal_hwmon.o
thermal_sys-$(CONFIG_THERMAL_OF)		+= of-thermal.o

# governors
thermal_sys-$(CONFIG_THERMAL_GOV_FAIR_SHARE)	+= fair_share.o
thermal_sys-$(CONFIG_THERMAL_GOV_STEP_WISE)	+= step_wise.o
thermal_sys-$(CONFIG_THERMAL_GOV_USER_SPACE)	+= user_space.o

# cpufreq cooling
thermal_sys-$(CONFIG_CPU_THERMAL)	+= cpu_cooling.o
obj-$(CONFIG_SPRD_CPU_COOLING)	+= sprd_cpu_cooling.o

# platform thermal drivers
obj-$(CONFIG_SPEAR_THERMAL)	+= spear_thermal.o
obj-$(CONFIG_RCAR_THERMAL)	+= rcar_thermal.o
obj-$(CONFIG_KIRKWOOD_THERMAL)  += kirkwood_thermal.o
obj-$(CONFIG_EXYNOS_THERMAL)	+= exynos_thermal.o
obj-$(CONFIG_DOVE_THERMAL)  	+= dove_thermal.o
obj-$(CONFIG_DB8500_THERMAL)	+= db8500_thermal.o
obj-$(CONFIG_ARMADA_THERMAL)	+= armada_thermal.o
obj-$(CONFIG_DB8500_CPUFREQ_COOLING)	+= db8500_cpufreq_cooling.o
obj-$(CONFIG_INTEL_POWERCLAMP)	+= intel_powerclamp.o

# Added for different sprd thm
obj-$(CONFIG_THERMAL_DDIE)		+= sprd_ddie_thm.o
obj-$(CONFIG_THERMAL_ALONE_DDIE)		+= sprd_ddie_alone_thm.o
obj-$(CONFIG_THERMAL_SC2713)		+= sprd_2713_thm.o
obj-$(CONFIG_THERMAL_SC2723S_PMIC)		+= sprd_pmic_2723s_thm.o
obj-$(CONFIG_THERMAL_BOARD_SENSOR)		+= sprd_board_sensor.o
