# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos7870-j6lte_cis_ser_00.dtb
dtb-y += exynos7870-j6lte_cis_ser_02.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
