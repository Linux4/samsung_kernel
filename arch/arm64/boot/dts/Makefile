# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos/exynos9810-haechiy19lte_usa_att_01.dtb
dtb-y += exynos/exynos9810-haechiy19lte_usa_att_00.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
