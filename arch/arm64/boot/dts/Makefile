# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos7870-j7toplte_usa_att_00.dtb
dtb-y += exynos7870-j7toplte_usa_att_01.dtb
dtb-y += exynos7870-j7toplte_usa_att_02.dtb
dtb-y += exynos7870-j7toplte_usa_att_04.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
