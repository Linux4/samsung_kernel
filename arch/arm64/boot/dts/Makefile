# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos/exynos5433-tm2e.dtb
dtb-y += exynos/exynos9830.dtb
dtbo-y += exynos/exynos9830-universal9830_r06.dtbo
dtbo-y += exynos/exynos9830-universal9830_r04.dtbo
dtb-y += exynos/exynos5433-tm2.dtb
dtbo-y += exynos/exynos9830-smdk9830_r02.dtbo
dtbo-y += exynos/exynos9830-universal9830_r10.dtbo
dtb-y += exynos/exynos7-espresso.dtb
dtbo-y += exynos/exynos9830-universal9830_r05.dtbo
dtbo-y += samsung/exynos990-r8s_eur_open_02.dtbo
dtbo-y += samsung/exynos990-r8s_eur_open_00.dtbo
dtbo-y += samsung/exynos990-r8s_eur_open_05.dtbo

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
