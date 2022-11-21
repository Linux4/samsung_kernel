# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos7570-xcover4lte_can_open_03.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
