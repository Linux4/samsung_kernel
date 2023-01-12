
targets += dtbs
# huaqin added by lanshiming for H000001 at 20180718 start
include $(srctree)/arch/arm64/boot/dts/samsung/Makefile
$(obj)/%.dtb: $(src)/../../../../arm64/boot/dts/samsung/%.dts FORCE
	$(call if_changed_dep,dtc)

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
$(obj)/%.dtbo:$(src)/../../../../arm64/boot/dts/samsung/%.dts FORCE
	$(call if_changed_dep,dtc)
	$(call if_changed,dtbo_verify)
# huaqin added by lanshiming for H000001 at 20180718 end
dtbs: $(addprefix $(obj)/,$(dtb-y)) $(addprefix $(obj)/,$(dtbo-y))
else
dtbs: $(addprefix $(obj)/,$(dtb-y))
endif
clean-files := *.dtb
