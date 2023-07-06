dtb-y += exynos7870-j7xelte_eur_open_00.dtb
dtb-y += exynos7870-j7xelte_eur_open_01.dtb
dtb-y += exynos7870-j7xelte_eur_open_02.dtb
dtb-y += exynos7870-j7xelte_eur_open_03.dtb
dtb-y += exynos7870-j7xelte_eur_open_04.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
