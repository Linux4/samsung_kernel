#
# Makefile for ESCA.
#

obj-$(CONFIG_EXYNOS_ESCAV2)    += exynos_esca.o 
exynos_esca-$(CONFIG_EXYNOS_ESCAV2)    += esca_drv.o esca_ipc.o esca_dbg.o esca_plg.o
exynos_esca-$(CONFIG_EXYNOS_MFD_SPMI)	+= esca_mfd_spmi.o
obj-$(CONFIG_EXYNOS_ESCA_S2D)	+= esca_s2d.o
obj-$(CONFIG_EXYNOS_ESCA_PLGDBG)	+= plugins/plugin_dbg.o
obj-$(CONFIG_EXYNOS_FLEXPMU_DBG)            += plugins/flexpmu_dbg.o
