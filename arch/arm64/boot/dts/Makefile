# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos/exynos9630.dtb
dtbo-y += samsung/a51x/a51x/a51x_kor_single_w00_r00.dtbo
dtbo-y += samsung/a51x/a51x/a51x_kor_single_w00_r01.dtbo

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
