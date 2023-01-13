ccflags-y := -Iinclude/drm -Idrivers/gpu/drm/msm -Idrivers/gpu/drm/msm/dsi-staging
ccflags-y += -Idrivers/gpu/drm/msm/sde
ccflags-y += -Idrivers/gpu/drm/msm/samsung

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

#
# panel directory make file
#

# TO SUPPORT PBA BOOINT
obj-y += PBA_BOOTING/ss_dsi_panel_PBA_BOOTING_fhd.o
obj-y += PBA_BOOTING_DSI1/ss_dsi_panel_PBA_BOOTING_fhd_dsi1.o

# WINNER main DSC panel
obj-$(CONFIG_PANEL_S6E3XA0_AMB729WA01_QXGA) += S6E3XA0_AMB729WA01/

# STAR2 DSC panel
obj-$(CONFIG_PANEL_S6E3HA8_AMB622NP01_WQHD) += S6E3HA8_AMB622NP01/

# BEYOND1 L-Cut DSC panel - 3040
obj-$(CONFIG_PANEL_S6E3HA9_AMB611WE01_WQHD) += S6E3HA9_AMB611WE01/

# BLOOM rev00 DSC panel - 2280
obj-$(CONFIG_PANEL_S6E3HA9_AMB611WE01_FHD) += S6E3HA9_AMB611WE01/

# BLOOM rev01 DSC panel - 2640
obj-$(CONFIG_PANEL_S6E3HA9_AMF670UH01_FHD) += S6E3HA9_AMF670UH01/

# BEYOND1 DSC panel - 2960
obj-$(CONFIG_PANEL_S6E3HA9_AMB597RT01_WQHD) += S6E3HA9_AMB597RT01/

# BEYOND2 L-Cut DSC panel - 3040
obj-$(CONFIG_PANEL_S6E3HA9_AMB644WF01_WQHD) += S6E3HA9_AMB644WF01/

# BEYONDX P-Hole DSC panel - 3040
obj-$(CONFIG_PANEL_S6E3HA9_AMB666WS04_WQHD) += S6E3HA9_AMB666WS04/

# CROWN DSC panel
obj-$(CONFIG_PANEL_S6E3HA8_AMB638RD01_WQHD) += S6E3HA8_AMB638RD01/

# WINNER sub panel
obj-$(CONFIG_PANEL_S6E3FA7_AMB458WJ01_HD) += S6E3FA7_AMB458WJ01/

# ASTAR panel (sub panel for WINNER bringup board)
obj-$(CONFIG_PANEL_S6E3FA7_AMS628RF01_FHD) += S6E3FA7_AMS628RF01/

# JACKPOT panel & BEYOND0 temp panel
obj-$(CONFIG_PANEL_S6E3FA7_AMS559NK01_FHD) += S6E3FA7_AMS559NK01/

# BEYOND0 panel
obj-$(CONFIG_PANEL_S6E3FA7_AMB575WD01_FHD) += S6E3FA7_AMB575WD01/

# TAB_S5 panel
obj-$(CONFIG_PANEL_ANA38401_AMSA05RB06_WQXGA) += ANA38401_AMSA05RB06/

# DAVINCI1 P-hole DSC panel - 2280
obj-$(CONFIG_PANEL_S6E3FA7_AMB628TR01_FHD) += S6E3FA7_AMB628TR01/

# DAVINCI2 P-hole DSC panel - 3040
obj-$(CONFIG_PANEL_S6E3HA9_AMB675TG01_WQHD) += S6E3HA9_AMB675TG01/

# bloom sub panel
obj-$(CONFIG_PANEL_S6E36W3_AMB114EU09) += S6E36W3_AMB114EU09/

# R3 panel
obj-$(CONFIG_PANEL_S6E3FC2_AMS670TA01_FHD) += S6E3FC2_AMS670TA01/

# R5 panel
obj-$(CONFIG_PANEL_S6E3FA9_AMB667UM01_FHD) += S6E3FA9_AMB667UM01/

# A82 From Hubble Y2 DSC panel - 3200
obj-$(CONFIG_PANEL_S6E3HAB_AMB677TY01_WQHD) += S6E3HAB_AMB677TY01/
