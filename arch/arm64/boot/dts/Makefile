dtb-y += exynos7870-j7popelte_kor_skt_00.dtb
dtb-y += exynos7870-j7popelte_kor_skt_01.dtb
dtb-y += exynos7870-j7popelte_kor_skt_03.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
