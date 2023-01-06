# SPDX-License-Identifier: GPL-2.0
dtbo-y += exynos/exynos9830-universal9830_r05.dtbo
dtbo-y += exynos/exynos9830-universal9830_r04.dtbo
dtb-y += exynos/exynos5433-tm2e.dtb
dtbo-y += exynos/exynos9830-smdk9830_r02.dtbo
dtb-y += exynos/exynos5433-tm2.dtb
dtb-y += exynos/exynos9830.dtb
dtbo-y += exynos/exynos9830-universal9830_r10.dtbo
dtbo-y += exynos/exynos9830-universal9830_r06.dtbo
dtb-y += exynos/exynos7-espresso.dtb
dtbo-y += samsung/exynos990-c2s_eur_openx_16.dtbo
dtbo-y += samsung/exynos990-c2s_eur_openx_17.dtbo
dtbo-y += samsung/exynos990-c2s_eur_openx_18.dtbo
dtbo-y += samsung/exynos990-c2s_eur_openx_24.dtbo
dtbo-y += samsung/exynos990-c2s_eur_openx_22.dtbo
dtbo-y += samsung/exynos990-c2s_eur_openx_23.dtbo
dtbo-y += samsung/exynos990-c2s_eur_openx_21.dtbo
dtbo-y += samsung/exynos990-c2s_eur_openx_00.dtbo
dtbo-y += samsung/exynos990-c2s_eur_openx_19.dtbo

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
