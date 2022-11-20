# SPDX-License-Identifier: GPL-2.0
dtbo-$(CONFIG_BOARD_UNIVERSAL2100) += \
	exynos2100-universal2100_rev02.dtbo \
	exynos2100-universal2100_rev10.dtbo \
	exynos2100-universal2100_rev11.dtbo

dtbo-$(CONFIG_ARCH_EXYNOS) += \
	s5e9925-universal9925_rev00.dtbo \
	s5e9925-universal9925_rev17.dtbo \
	s5e9925-universal9925_rev1b.dtbo \
	s5e9925_evt0-universal9925_rev00.dtbo

dtbo-$(CONFIG_ARCH_EXYNOS) += \
	s5e8825-universal8825_r00.dtbo

always-y	:= $(dtb-y) $(dtbo-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
