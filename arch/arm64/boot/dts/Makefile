dtb-y += exynos7870-j7popelte_usa_att_00.dtb
dtb-y += exynos7870-j7popelte_usa_att_01.dtb
dtb-y += exynos7870-j7popelte_usa_att_04.dtb
dtb-y += exynos7870-j7popelte_usa_att_05.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
