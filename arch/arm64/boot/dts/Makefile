dtb-y += exynos7420-noblelte_kor_00.dtb
dtb-y += exynos7420-noblelte_kor_01.dtb
dtb-y += exynos7420-noblelte_kor_02.dtb
dtb-y += exynos7420-noblelte_kor_03.dtb
dtb-y += exynos7420-noblelte_kor_04.dtb
dtb-y += exynos7420-noblelte_kor_06.dtb
dtb-y += exynos7420-noblelte_kor_07.dtb
dtb-y += exynos7420-noblelte_kor_08.dtb
dtb-y += exynos7420-noblelte_kor_09.dtb

targets += dtbs
DTB_LIST	:= $(dtb-y) $(dtbo-y)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := *.dtb*