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

# obj-$(CONFIG_SAMSUNG_DEBUG_DISPLAY) += dlog.o
obj-$(CONFIG_SUPPORT_POC_FLASH) += ss_ddi_poc_common.o

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

# BEYOND1 DSC panel - 2960
obj-$(CONFIG_PANEL_S6E3HA9_AMB597RT01_WQHD) += S6E3HA9_AMB597RT01/

# BEYOND2 L-Cut DSC panel - 3040
obj-$(CONFIG_PANEL_S6E3HA9_AMB644WF01_WQHD) += S6E3HA9_AMB644WF01/

# CROWN DSC panel
obj-$(CONFIG_PANEL_S6E3HA8_AMB638RD01_WQHD) += S6E3HA8_AMB638RD01/

# WINNER sub panel
obj-$(CONFIG_PANEL_S6E3FA7_AMB458WJ01_HD) += S6E3FA7_AMB458WJ01/

# JACKPOT panel & BEYOND0 temp panel
obj-$(CONFIG_PANEL_S6E3FA7_AMS559NK01_FHD) += S6E3FA7_AMS559NK01/

# BEYOND0 panel
obj-$(CONFIG_PANEL_S6E3FA7_AMB575WD01_FHD) += S6E3FA7_AMB575WD01/

# ASTAR panel / GEN_Z panel
obj-$(CONFIG_PANEL_S6E3FA7_AMS628RF01_FHD) += S6E3FA7_AMS628RF01/

# A50 panel
obj-$(CONFIG_PANEL_S6E3FA7_AMS628RF05_FHD) += S6E3FA7_AMS628RF05/

# On7X IN_CELL panel / A9s 2nd Panel
obj-$(CONFIG_PANEL_TD4300_BS055FHM_FHD) += TD4300_BS055FHM/

# A9s panel / A9s 3nd Panel
obj-$(CONFIG_PANEL_NT36672A_BS063YPM_FHD) += NT36672A_BS063YPM/

# A9s panel / A9s 4th Panel
obj-$(CONFIG_PANEL_NT36672A_B6P064YQ5LP_FHD) += NT36672A_B6P064YQ5LP/

# BLIC
obj-$(CONFIG_BLIC_ISL98611) += BLIC_ISL98611/

# TAB S4 LV Panel
obj-$(CONFIG_PANEL_ANA38401_AMSA05RB01_WQXGA) += ANA38401_AMSA05RB01/

# Tab A10 Active
obj-$(CONFIG_PANEL_HX8279_TV101WUM_WUXGA) += HX8279_TV101WUM/