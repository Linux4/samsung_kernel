# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos7880-a7y17lte_kor_all_00.dtb
dtb-y += exynos7880-a7y17lte_kor_all_01.dtb
dtb-y += exynos7880-a7y17lte_kor_all_02.dtb
dtb-y += exynos7880-a7y17lte_kor_all_03.dtb
dtb-y += exynos7880-a7y17lte_kor_all_04.dtb
dtb-y += exynos7880-a7y17lte_kor_all_06.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
