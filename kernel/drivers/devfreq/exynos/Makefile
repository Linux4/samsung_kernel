# Exynos DEVFREQ Drivers
obj-$(CONFIG_ARM_EXYNOS_DEVFREQ)		+= exynos_devfreq.o
exynos_devfreq-y	+= exynos-devfreq.o
exynos_devfreq-y	+= governor_nop.o
exynos_devfreq-$(CONFIG_EXYNOS_ALT_DVFS) += governor_simpleinteractive.o
exynos_devfreq-$(CONFIG_EXYNOS_LEALT_DVFS)	+= governor_lealt.o lealt-mon.o
