dtb-y += exynos7570-j3toplte_usa_tmo_00.dtb
dtb-y += exynos7570-j3toplte_usa_tmo_02.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
