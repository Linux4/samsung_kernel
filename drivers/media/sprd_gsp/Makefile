# SC88XX Platform Support


ifeq ($(CONFIG_ARCH_SCX35),y)

# CONFIG_VIDEO_GSP_SPRD = m
ifeq ($(CONFIG_VIDEO_GSP_SPRD),y)
sprd_gsp-objs := scaler_coef_cal.o gsp_config_if.o gsp_drv.o 
obj-$(CONFIG_VIDEO_GSP_SPRD) += sprd_gsp.o
else
sprd_gsp-objs := sin_cos.o scaler_coef_cal.o gsp_config_if.o gsp_drv.o
obj-m += sprd_gsp.o
endif

# $(warning  ytc-------------------------- : $(CONFIG_VIDEO_GSP_SPRD))


# EXTRA_CFLAGS += -march=armv7-a -mfloat-abi=softfp -mfpu=neon
EXTRA_CFLAGS += $(arch_variant_cflags) 
# arch_variant_ldflags := -Wl,--fix-cortex-a8

endif
