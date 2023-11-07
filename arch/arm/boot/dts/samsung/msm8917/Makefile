ifeq ($(CONFIG_OF),y)

ifeq ($(CONFIG_SEC_ON5QLTE_PROJECT),y)
        ifeq ($(CONFIG_MACH_ON5QLTE_CHN_OPEN),y)
                dtb-y += msm8917-sec-on5qlte-chn-r00.dtb
		dtb-y += msm8917-sec-on5qlte-chn-r01.dtb
        endif
endif

ifeq ($(CONFIG_SEC_ELITELTE_PROJECT),y)
        ifeq ($(CONFIG_MACH_ELITELTE_CHN_OPEN),y)
                dtb-y += msm8917-sec-elitelte-chn-r00.dtb
		dtb-y += msm8917-sec-elitelte-chn-r01.dtb
		dtb-y += msm8917-sec-elitelte-chn-r02.dtb
		dtb-y += msm8917-sec-elitelte-chn-r03.dtb
        endif

        ifeq ($(CONFIG_MACH_ELITELTE_KOR_OPEN),y)
		dtb-y += msm8917-sec-elitelte-kor-r03.dtb
		dtb-y += msm8917-sec-elitelte-kor-r04.dtb
		dtb-y += msm8917-sec-elitelte-kor-r05.dtb
        endif
endif

ifeq ($(CONFIG_SEC_ELITEXLTE_PROJECT),y)
        ifeq ($(CONFIG_MACH_ELITEXLTE_CHN_OPEN),y)
                dtb-y += msm8917-sec-elitexlte-chn-r00.dtb
				dtb-y += msm8917-sec-elitexlte-chn-r01.dtb
				dtb-y += msm8917-sec-elitexlte-chn-r02.dtb
        endif
endif

ifeq ($(CONFIG_SEC_ELITE3G_PROJECT),y)
        ifeq ($(CONFIG_MACH_ELITE3G_KOR_OPEN),y)
		dtb-y += msm8917-sec-elite3g-kor-r03.dtb
		dtb-y += msm8917-sec-elite3g-kor-r04.dtb
		dtb-y += msm8917-sec-elite3g-kor-r05.dtb
        endif
endif

ifeq ($(CONFIG_SEC_J3LLTE_PROJECT),y)
        ifeq ($(CONFIG_MACH_J3LLTE_USA_SPR),y)
                dtb-y += msm8917-sec-j3llte-usa-spr-r00.dtb
        endif
endif

ifeq ($(CONFIG_SEC_J3POPLTE_PROJECT),y)
        ifeq ($(CONFIG_MACH_J3POPLTE_USA_SPR),y)
                dtb-y += msm8917-sec-j3poplte-usa-spr-r00.dtb
                dtb-y += msm8917-sec-j3poplte-usa-spr-r01.dtb
                dtb-y += msm8917-sec-j3poplte-usa-spr-r03.dtb
                dtb-y += msm8917-sec-j3poplte-usa-spr-r04.dtb
                dtb-y += msm8917-sec-j3poplte-usa-spr-r05.dtb
                dtb-y += msm8917-sec-j3poplte-usa-spr-r06.dtb
        endif

        ifeq ($(CONFIG_MACH_J3POPLTE_USA_TFNVZW),y)
                dtb-y += msm8917-sec-j3poplte-usa-tfnvzw-r00.dtb
                dtb-y += msm8917-sec-j3poplte-usa-tfnvzw-r04.dtb
                dtb-y += msm8917-sec-j3poplte-usa-tfnvzw-r06.dtb
        endif
endif

ifeq ($(CONFIG_SEC_J7POPQLTE_PROJECT),y)
        ifeq ($(CONFIG_MACH_J7POPQLTE_USA_TFNVZW),y)
                dtb-y += msm8917-sec-j7popqlte-usa-tfnvzw-r00.dtb
                dtb-y += msm8917-sec-j7popqlte-usa-tfnvzw-r01.dtb
        endif
endif

ifeq ($(CONFIG_SEC_ON5XLLTE_PROJECT),y)
        ifeq ($(CONFIG_MACH_ON5XLLTE_CHN_OPEN),y)
                dtb-y += msm8917-sec-on5xllte-chn-r00.dtb
                dtb-y += msm8917-sec-on5xllte-chn-r01.dtb
                dtb-y += msm8917-sec-on5xllte-chn-r02.dtb
                dtb-y += msm8917-sec-on5xllte-chn-r03.dtb
        endif

        ifeq ($(CONFIG_MACH_ON5XFLTE_CHN_OPEN),y)
                dtb-y += msm8917-sec-on5xflte-chn-r00.dtb
                dtb-y += msm8917-sec-on5xflte-chn-r01.dtb
                dtb-y += msm8917-sec-on5xflte-chn-r02.dtb
		dtb-y += msm8917-sec-on5xflte-chn-r03.dtb
        endif
endif

echo '*** Warning: Samsung $@ Device Tree MSM8917.'
DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif

targets += dtbs
targets += $(addprefix ../, $(DTB_LIST))
endif

$(obj)/../../%.dtb: $(src)/%.dts FORCE
	$(call if_changed_dep,dtc)

dtbs: $(addprefix $(obj)/../../,$(DTB_LIST))

clean-files := *.dtb *.reverse.dts


