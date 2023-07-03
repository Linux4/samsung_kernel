dtb-y += exynos7570-j3popelte_usa_att_00.dtb
dtb-y += exynos7570-j3popelte_usa_att_01.dtb
dtb-y += exynos7570-j3popelte_usa_att_02.dtb
dtb-y += exynos7570-j3popelte_usa_att_03.dtb
dtb-y += exynos7570-j3popelte_usa_att_05.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
