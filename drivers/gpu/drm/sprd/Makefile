ccflags-y += -Iinclude/drm
subdir-ccflags-y += -Idrivers/staging/android/ion/
subdir-ccflags-y += -Idrivers/devfreq/apsys/
subdir-ccflags-y += -I$(src)

obj-y := sprd_drm.o \
	sprd_gem.o \
	sprd_dpu.o \
	sprd_dpu1.o \
	sprd_dsi.o \
	sprd_dphy.o \
	sprd_panel.o \
	sprd_dp.o

obj-y += dummy/dummy_crtc.o
obj-y += dummy/dummy_connector.o

obj-y += disp_lib.o sprd_corner.o disp_trusty.o

#obj-y += sprd_hdcp.o

obj-y += sprd_drm_notifier.o

obj-y += sprd_bl.o

obj-y += dpu/
obj-y += dsi/
obj-y += dphy/
obj-y += dp/
obj-y += sysfs/

obj-y += sharkle/
obj-y += pike2/
obj-y += sharkl3/
obj-y += sharkl5/
obj-y += sharkl5Pro/
obj-y += roc1/
obj-y += qogirl6/
obj-y += qogirn6pro/

obj-$(CONFIG_DRM_SPRD_GSP) += gsp/

obj-$(CONFIG_COMPAT) += sprd_ioc32.o

obj-$(CONFIG_DRM_SPRD_HDMI) += lt9611_i2c.o

obj-$(CONFIG_LCD_I2C_BACKLIGHT_SUPPORT) += aw99703_bl.o
