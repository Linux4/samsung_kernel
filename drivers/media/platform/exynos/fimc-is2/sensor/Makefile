obj-y += fimc-is-helper-i2c.o

obj-$(CONFIG_VIDEO_EXYNOS_FIMC_IS) += module/
obj-$(CONFIG_VIDEO_EXYNOS_FIMC_IS) += csi/

obj-$(CONFIG_USE_DIRECT_IS_CONTROL) += module_framework/ \
					module_framework/modules/ \
					module_framework/cis/ \
					module_framework/actuator/ \
					module_framework/flash/

obj-$(CONFIG_CAMERA_FPGA_DPHY) += module_framework/dphy/

EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/include
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/vendor
