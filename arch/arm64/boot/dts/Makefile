# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos7870-j7toplte_usa_tmo_00.dtb
dtb-y += exynos7870-j7toplte_usa_tmo_01.dtb
dtb-y += exynos7870-j7toplte_usa_tmo_02.dtb
dtb-y += exynos7870-j7toplte_usa_tmo_04.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
