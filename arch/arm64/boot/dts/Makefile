dtb-y += exynos7870-on7xelte_swa_open_00.dtb
dtb-y += exynos7870-on7xelte_swa_open_01.dtb

targets += dtbs
DTB_LIST  := $(dtb-y) $(dtbo-y)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*
