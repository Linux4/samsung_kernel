# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos/exynos7884-j7duolte_swa_00.dtb
dtb-y += exynos/exynos7884-j7duolte_swa_01.dtb
dtb-y += exynos/exynos7884-j7duolte_swa_02.dtb
dtb-y += exynos/exynos7884-j7duolte_swa_03.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
