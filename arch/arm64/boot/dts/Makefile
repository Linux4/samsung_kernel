# SPDX-License-Identifier: GPL-2.0
dtb-y += exynos7870-j7toplte_kor_skt_00.dtb
dtb-y += exynos7870-j7toplte_kor_skt_01.dtb
dtb-y += exynos7870-j7toplte_kor_skt_02.dtb
dtb-y += exynos7870-j7toplte_kor_skt_03.dtb
dtb-y += exynos7870-j7toplte_kor_skt_04.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)
always    := $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
