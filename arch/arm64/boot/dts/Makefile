# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos7570-j2coreplte_usa_aio_00.dtb
dtb-y += exynos7570-j2coreplte_usa_aio_01.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
