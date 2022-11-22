# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos/dtbo/exynos7885.dtb
dtbo-y += exynos/dtbo/exynos7904-gta3xlwifi_eur_open_03.dtbo
dtbo-y += exynos/dtbo/exynos7904-gta3xlwifi_eur_open_04.dtbo

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
