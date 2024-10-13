

ifeq ($(CONFIG_ARCH_SCX35),y)

# CONFIG_VIDEO_GSPN_SPRD = m
ifeq ($(CONFIG_VIDEO_GSPN_SPRD),y)
#sprd_gspn-objs := gspn_coef_calc.o gspn_register_cfg.o gspn_drv.o gspn_work_thread.o
sprd_gspn-objs := sin_cos.o gspn_coef_generate.o gspn_drv.o gspn_work_thread.o gspn_register_cfg.o gspn_sync.o
obj-$(CONFIG_VIDEO_GSPN_SPRD) += sprd_gspn.o
else
#sprd_gspn-objs := sin_cos.o gspn_coef_calc.o gspn_register_cfg.o gspn_drv.o gspn_work_thread.o
sprd_gspn-objs := sin_cos.o gspn_coef_generate.o gspn_drv.o gspn_work_thread.o gspn_register_cfg.o gspn_sync.o
obj-m += sprd_gspn.o
endif

# $(warning  ytc-------------------------- : $(CONFIG_VIDEO_GSPN_SPRD))


# EXTRA_CFLAGS += -march=armv7-a -mfloat-abi=softfp -mfpu=neon
EXTRA_CFLAGS += $(arch_variant_cflags)
# arch_variant_ldflags := -Wl,--fix-cortex-a8

endif
