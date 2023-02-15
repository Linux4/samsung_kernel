# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos/exynos9610.dtb
dtbo-y += samsung/exynos9611-gta4xl_eur_open_03.dtbo
dtbo-y += samsung/exynos9611-gta4xl_eur_open_05.dtbo
dtbo-y += samsung/exynos9611-gta4xl_eur_open_01.dtbo
dtbo-y += samsung/exynos9611-gta4xl_eur_open_08.dtbo
dtbo-y += samsung/exynos9611-gta4xl_eur_open_04.dtbo
dtbo-y += samsung/exynos9611-gta4xl_eur_open_09.dtbo

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
