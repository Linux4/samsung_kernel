ifeq ($(CONFIG_SEC_A23_PROJECT),y)
dts-dirs += a23/a23
endif

ifeq ($(CONFIG_ARM64),y)
always          := $(dtb-y)
subdir-y        := $(dts-dirs)
else
targets += dtbs
targets += $(addprefix ../, $(dtb-y))

$(obj)/../%.dtb: $(src)/%.dts FORCE
        $(call if_changed_dep,dtc)

dtbs: $(addprefix $(obj)/../,$(dtb-y))
endif

clean-files := *.dtb *.reverse.dts *.dtbo


