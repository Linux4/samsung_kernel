# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos7870-j7velte_ltn_dtv_01.dtb
dtb-y += exynos7870-j7velte_ltn_dtv_03.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
