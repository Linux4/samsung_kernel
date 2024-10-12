# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos/exynos9630.dtb
dtbo-y += samsung/a71x/a71x/a71x_eur_open_w00_r06.dtbo
dtbo-y += samsung/a71x/a71x/a71x_eur_open_w00_r07.dtbo
dtbo-y += samsung/a71x/a71x/a71x_eur_open_w00_r03.dtbo
dtbo-y += samsung/a71x/a71x/a71x_eur_open_w00_r05.dtbo

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
