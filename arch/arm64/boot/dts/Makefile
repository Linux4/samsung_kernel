# SPDX-License-Identifier: GPL-2.0
dtb-y +=  exynos7570-j4lte_mea_open_00.dtb
dtb-y +=  exynos7570-j4lte_mea_open_01.dtb
dtb-y +=  exynos7570-j4lte_mea_open_02.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
