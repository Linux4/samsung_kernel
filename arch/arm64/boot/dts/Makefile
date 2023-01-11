dtb-$(CONFIG_ARCH_VEXPRESS) += rtsm_ve-aemv8a.dtb foundation-v8.dtb
dtb-$(CONFIG_ARCH_XGENE) += apm-mustang.dtb
dtb-$(CONFIG_MACH_PXA1936DKB) += pxa1928concord.dtb pxa1928-psci.dtb pxa1928concord-discrete.dtb \
			  pxa1908-fpga.dtb pxa1908-dkb.dtb pxa1908-cmtb.dtb pxa1908-ff.dtb pxa1936-qt.dtb pxa1936-fpga.dtb pxa1936-dkb-v10.dtb pxa1936-dkb-v20.dtb pxa1936-ff.dtb pxa1936-cmtb.dtb \
			  sec-j7lte-v00.dtb sec-j7lte-v01.dtb
dtb-$(CONFIG_MACH_J7MLTE) += pxa1936-j7mlte-00.dtb \
				pxa1936-j7mlte-01.dtb \
				pxa1936-j7mlte-02.dtb \
				pxa1936-j7mlte-03.dtb
dtb-$(CONFIG_MACH_J7M3G) += pxa1936-j7m3g-00.dtb \
				pxa1936-j7m3g-01.dtb \
				pxa1936-j7m3g-02.dtb \
				pxa1936-j7m3g-03.dtb
dtb-$(CONFIG_MACH_PXA1908DKB) += pxa1908-dkb.dtb
dtb-$(CONFIG_MACH_XCOVER3LTE) += pxa1908-xcover3lte-00.dtb \
				pxa1908-xcover3lte-03.dtb \
				pxa1908-xcover3lte-04.dtb \
				pxa1908-xcover3lte-05.dtb \
				pxa1908-xcover3lte-06.dtb \
				pxa1908-xcover3lte-07.dtb \
				pxa1908-xcover3lte-08.dtb
dtb-$(CONFIG_MACH_J1ACELTE_LTN) += pxa1908-j1acelteltn-00.dtb \
				pxa1908-j1acelteltn-01.dtb \
				pxa1908-j1acelteltn-03.dtb

ifdef CONFIG_MACH_COREPRIMEVELTE_LTN
dtb-$(CONFIG_MACH_COREPRIMEVELTE) += pxa1908-coreprimevelteltn-00.dtb \
				pxa1908-coreprimevelteltn-01.dtb \
				pxa1908-coreprimevelteltn-02.dtb
else
dtb-$(CONFIG_MACH_COREPRIMEVELTE) += pxa1908-coreprimevelte-00.dtb \
				pxa1908-coreprimevelte-01.dtb \
				pxa1908-coreprimevelte-02.dtb
endif

ifdef CONFIG_MACH_GRANDPRIMEVELTE_LTN
dtb-$(CONFIG_MACH_GRANDPRIMEVELTE) += pxa1908-grandprimevelteltn-00.dtb \
				pxa1908-grandprimevelteltn-01.dtb

else
dtb-$(CONFIG_MACH_GRANDPRIMEVELTE) += pxa1908-grandprimevelte-00.dtb \
				pxa1908-grandprimevelte-01.dtb
endif

targets += dtbs

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif
targets += $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := dts/*.dtb *.dtb
