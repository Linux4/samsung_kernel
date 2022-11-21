# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos/dtbo/exynos7885.dtb
dtbo-y += exynos/dtbo/exynos7884B-a20_aus_xsa_00.dtbo
dtbo-y += exynos/dtbo/exynos7884B-a20_aus_xsa_01.dtbo
dtbo-y += exynos/dtbo/exynos7884B-a20_aus_xsa_03.dtbo

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
