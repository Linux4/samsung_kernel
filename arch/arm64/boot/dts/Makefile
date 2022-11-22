# SPDX-License-Identifier: GPL-2.0
dtb-y +=  exynos7870-on7xreflte_ltn_dtv_00.dtb
dtb-y +=  exynos7870-on7xreflte_ltn_dtv_01.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
