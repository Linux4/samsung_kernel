# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos/exynos7904-m30lte_swa_open_00.dtb
dtb-y += exynos/exynos7904-m30lte_swa_open_01.dtb
dtb-y += exynos/exynos7904-m30lte_swa_open_03.dtb
dtb-y += exynos/exynos7904-m30lte_swa_open_04.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
