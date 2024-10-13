# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos/dtbo/exynos7885.dtb
dtbo-y += exynos/dtbo/exynos7884B-a10e_usa_single_00.dtbo
dtbo-y += exynos/dtbo/exynos7884B-a10e_usa_single_01.dtbo
dtbo-y += exynos/dtbo/exynos7884B-a10e_usa_single_02.dtbo

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
