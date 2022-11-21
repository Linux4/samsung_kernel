# SPDX-License-Identifier: GPL-2.0
dtb-y +=  exynos7870-j6lte_kor_lgt_01.dtb
dtb-y +=  exynos7870-j6lte_kor_lgt_02.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
