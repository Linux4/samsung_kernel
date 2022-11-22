ccflags-y := -Wformat -I$(srctree)/include/drm -I$(srctree)/techpack/display/msm -I$(srctree)/techpack/display/msm/dsi -I$(srctree)/techpack/display/msm/sde -I$(srctree)/techpack/display/msm/samsung

obj-y += ss_dsi_panel_sysfs.o
obj-y += ss_dsi_panel_debug.o
obj-y += ss_dsi_panel_common.o
obj-y += ss_dsi_mdnie_lite_common.o
obj-y += ss_dsi_smart_dimming_common.o
obj-y += ss_interpolation_common.o
obj-y += ss_flash_table_data_common.o
obj-y += ss_dpui_common.o
obj-y += ss_ddi_poc_common.o

# obj-$(CONFIG_SAMSUNG_DEBUG_DISPLAY) += dlog.o

# TO SUPPORT SPI INTERFACE
obj-y  += ss_ddi_spi_common.o

# COPR
obj-y += ss_copr_common.o

# Self Display
obj-y += SELF_DISPLAY/

# mAFPC
obj-y += MAFPC/

#
# panel directory make file
#

# TO SUPPORT PBA BOOINT
obj-y += PBA_BOOTING/ss_dsi_panel_PBA_BOOTING_fhd.o
obj-y += PBA_BOOTING_DSI1/ss_dsi_panel_PBA_BOOTING_fhd_dsi1.o

# R5 panel
obj-$(CONFIG_PANEL_S6E3FA9_AMB667UM01_FHD) += S6E3FA9_AMB667UM01/

# A51_5G panel
obj-$(CONFIG_PANEL_EA8076A_AMS646UJ10_FHD) += EA8076A_AMS646UJ10/
