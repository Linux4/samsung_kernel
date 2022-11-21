ifeq ($(CONFIG_OF),y)

dtb-$(CONFIG_MACH_COREPRIMELITE) += sprd-scx35l_coreprimelite.dtb

ifeq ($(CONFIG_MACH_GRANDPRIMEVE3G),y)
ifeq ($(CONFIG_MACH_GRANDPRIMEVE3G_LTN_DTV),y)
dtb-$(CONFIG_MACH_GRANDPRIMEVE3G_LTN_DTV) += sprd-scx35_grandprimeve3gltndtv_rev00.dtb \
				sprd-scx35_grandprimeve3gltndtv_rev01.dtb \
				sprd-scx35_grandprimeve3gltndtv_rev02.dtb \
				sprd-scx35_grandprimeve3gltndtv_rev03.dtb \
				sprd-scx35_grandprimeve3gltndtv_rev04.dtb
else
dtb-$(CONFIG_MACH_GRANDPRIMEVE3G) += sprd-scx35_grandprimeve3g.dtb \
				sprd-scx35_grandprimeve3g_rev00.dtb \
				sprd-scx35_grandprimeve3g_rev01.dtb \
				sprd-scx35_grandprimeve3g_rev02.dtb \
				sprd-scx35_grandprimeve3g_rev03.dtb \
				sprd-scx35_grandprimeve3g_rev04.dtb
endif
endif
dtb-$(CONFIG_MACH_GTEL3G) += sprd-scx35_gtel3g.dtb
dtb-$(CONFIG_MACH_COREPRIMEVE3G) += sprd-scx35_coreprimeve3g.dtb \
				sprd-scx35_coreprimeve3g_rev01.dtb \
				sprd-scx35_coreprimeve3g_rev02.dtb \
				sprd-scx35_coreprimeve3g_rev03.dtb


dtb-$(CONFIG_MACH_J23G) += sprd-scx35_j23g_rev00.dtb

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif

targets += dtbs
targets += $(dtb-y)
endif

# *.dtb used to be generated in the directory above. Clean out the
# old build results so people don't accidentally use them.
dtbs: $(addprefix $(obj)/, $(dtb-y))
	$(Q)rm -f $(obj)/../*.dtb

clean-files := *.dtb
