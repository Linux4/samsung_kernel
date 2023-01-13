ifeq ($(CONFIG_DISPLAY_SAMSUNG_LEGO),y)
ccflags-y += -I$(srctree)/include/drm
ccflags-y += -I$(srctree)/drivers/gpu/drm/msm
ccflags-y += -I$(srctree)/drivers/gpu/drm/msm/dsi-staging
ccflags-y += -I$(srctree)/drivers/gpu/drm/msm/sde
ccflags-y += -I$(srctree)/drivers/clk/qcom/
ccflags-y += -I$(srctree)/drivers/gpu/drm/msm/samsung_lego
endif

obj-y += ss_dsi_panel_sysfs.o \
	ss_dsi_panel_debug.o \
	ss_dsi_panel_common.o \
	ss_dsi_mdnie_lite_common.o \
	ss_dsi_smart_dimming_common.o \
	ss_interpolation_common.o \
	ss_flash_table_data_common.o \
	ss_dpui_common.o \
	ss_ddi_poc_common.o \
	ss_ddi_spi_common.o \
	ss_copr_common.o \
	SELF_DISPLAY/\
	MAFPC/\
	PBA_BOOTING/ss_dsi_panel_PBA_BOOTING_fhd.o \
	PBA_BOOTING_DSI1/ss_dsi_panel_PBA_BOOTING_fhd_dsi1.o

# A82 & Hubble Y2 DSC panel - 3200
obj-$(CONFIG_PANEL_S6E3HAB_AMB677TY01_WQHD) += S6E3HAB_AMB677TY01/
