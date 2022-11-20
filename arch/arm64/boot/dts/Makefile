# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos/exynos9610.dtb
dtbo-y += samsung/exynos9610-a50_ltn_open_06.dtbo
dtbo-y += samsung/exynos9610-a50_ltn_open_02.dtbo
dtbo-y += samsung/exynos9610-a50_ltn_open_03.dtbo
dtbo-y += samsung/exynos9610-a50_ltn_open_04.dtbo

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
