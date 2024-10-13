ifneq ($(CONFIG_HQ_PROJECT_O22),y)
ifneq ($(CONFIG_HQ_PROJECT_O8),y)
obj-y   +=  tpd_control.o
obj-y   +=  tpd_button.o
obj-y   +=  tpd_debug.o
obj-y   +=  tpd_default.o
obj-y   +=  tpd_misc.o
obj-y   +=  tpd_setting.o
# Tab A7 lite_U code for SR-AX3565U-01-4  by zhengkunbang at 20230807 start
subdir-ccflags-y += -I$(srctree)/drivers/input/keyboard/mediatek/mt6765/
# Tab A7 lite_U code for SR-AX3565U-01-4  by zhengkunbang at 20230807 end
obj-$(CONFIG_TOUCHSCREEN_ILI7806S_TOUCH) += ilitek_touch/
obj-$(CONFIG_TOUCHSCREEN_CHIPONE) += chipone/
obj-$(CONFIG_TOUCHSCREEN_JADARD_CHIPSET) += jdchipset/
obj-$(CONFIG_TOUCHSCREEN_NT36xxx_HOSTDL_SPI) += novatek_touch/
obj-$(CONFIG_TOUCHSCREEN_FTS) += focaltech_touch_ax3565/
obj-$(CONFIG_TOUCHSCREEN_HIMAX_OT8) += hxchipset_ot8/
obj-$(CONFIG_TOUCHSCREEN_HIMAX_CHIPSET) += hxchipset/
#hs03s_NM code for SR-AL5625-01-644 by fengzhigang at 20220414 start
obj-$(CONFIG_TOUCHSCREEN_GALAXYCORE_CHIPSET) += galaxycore/
#hs03s_NM code for SR-AL5625-01-644 by fengzhigang at 20220414 end
# hs04 code for DEAL6398A-1876 by tangsumian at 20221012 start
obj-$(CONFIG_TOUCHSCREEN_JADARD_CHIPSET_M04) += M04/jdchipset/
obj-$(CONFIG_TOUCHSCREEN_GALAXYCORE_CHIPSET_M04) += M04/galaxycore/
obj-$(CONFIG_TOUCHSCREEN_CHIPONE_CHIPSET_M04) += M04/chipone/
# hs04 code for DEAL6398A-1876 by tangsumian at 20221012 end
endif
endif

ifeq ($(CONFIG_HQ_PROJECT_O22),y)
# hs14 code for AL6528A-213 by hehaoran5 at 20221002 start
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/include/
# hs14 code for AL6528A-213 by hehaoran5 at 20221002 end
# hs14 code for AL6528A-338 by liudi at 20221018 start
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/accdet/mt6358/
# hs14 code for AL6528A-338 by liudi at 20221018 end
# hs14 code for AL6528ADEU-723 by hehaoran5 at 20221021 start
subdir-ccflags-y += -I$(srctree)/drivers/input/keyboard/mediatek/mt6768/
# hs14 code for AL6528ADEU-723 by hehaoran5 at 20221021 end
# hs14 code for AL6528A-5 by hehaoran5 at 20220829 start
obj-$(CONFIG_TOUCHSCREEN_OMNIVISION_TCM_O22) += O22/omnivision_tcm/
# hs14 code for AL6528A-5 by hehaoran5 at 20220829 end
# hs14 code for SR-AL6528A-01-479 by liudi at 20220906 start
obj-$(CONFIG_TOUCHSCREEN_JADARD_CHIPSET_O22) += O22/jdchipset/
# hs14 code for SR-AL6528A-01-479 by liudi at 20220906 end
# hs14 code for SR-AL6528A-01-454 by zhangxiongyi at 20220909 start
obj-$(CONFIG_TOUCHSCREEN_HIMAX_CHIPSET_O22) += O22/hxchipset/
# hs14 code for SR-AL6528A-01-454 by zhangxiongyi at 20220909 end
# hs14 code for SR-AL6528A-01-491 by hehaoran5 at 20220914 start
obj-$(CONFIG_TOUCHSCREEN_CHIPONE_TDDI_O22) += O22/chipone-tddi/
# hs14 code for SR-AL6528A-01-491 by hehaoran5 at 20220914 end
endif

ifeq ($(CONFIG_HQ_PROJECT_O8),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/include/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/accdet/mt6358/
subdir-ccflags-y += -I$(srctree)/drivers/input/keyboard/mediatek/mt6768/
# A06 code for AL7160A-5 by wenghailong at 20240313 start
obj-$(CONFIG_TOUCHSCREEN_HX83108B_CHIPSET_O8) += O8/hxchipset_hx83108b/
# A06 code for AL7160A-5 by wenghailong at 20240313 end
# A06 code for SR-AL7160A-01-71 by wenghailong at 20240407 start
obj-$(CONFIG_TOUCHSCREEN_GALAXYCORE_IC_GC7272) += O8/galaxycore/
# A06 code for SR-AL7160A-01-71 by wenghailong at 20240407 end
# A06 code for AL7160A-5 by wenghailong at 20240313 start
obj-$(CONFIG_ODM_SEC_COMMON) += O8/touch_common/
# A06 code for AL7160A-5 by wenghailong at 20240313 end
# A06 code for SR-AL7160A-01-215 by wenghailong at 20240401 start
obj-$(CONFIG_TOUCHSCREEN_FT8057P_O8) += O8/focaltech_touch_ft8057p/
# A06 code for SR-AL7160A-01-215 by wenghailong at 20240401 end
endif
