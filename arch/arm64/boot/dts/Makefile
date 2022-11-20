# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos/exynos9810-star2lte_kor_18.dtb
dtb-y += exynos/exynos9810-star2lte_kor_16.dtb
dtb-y += exynos/exynos9810-star2lte_kor_23.dtb
dtb-y += exynos/exynos9810-star2lte_kor_17.dtb
dtb-y += exynos/exynos9810-star2lte_kor_20.dtb
dtb-y += exynos/exynos9810-star2lte_kor_26.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
