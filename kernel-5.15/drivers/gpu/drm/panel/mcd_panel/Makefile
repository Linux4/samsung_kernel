ccflags-y += -I$(srctree)/drivers/gpu/drm/samsung/panel
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensor/2.0/core

mtk-mcd-panel-adapter-y := \
	mcd_panel_adapter.o \
	mcd_panel_adapter_helper.o

obj-$(CONFIG_DRM_PANEL_MCD_COMMON) += \
	mtk-mcd-panel-adapter.o
