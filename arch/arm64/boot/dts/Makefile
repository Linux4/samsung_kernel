# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos7870-a6lte_kor_single_00.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
