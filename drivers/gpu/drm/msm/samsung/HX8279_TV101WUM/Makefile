#
# panel object
# namd : ss_dsi_panel_"DDI NAME"_"PANEL NAME".o
#

ccflags-y := -Iinclude/drm -Idrivers/gpu/drm/msm -Idrivers/gpu/drm/msm/dsi-staging
ccflags-y += -Idrivers/gpu/drm/msm/sde
ccflags-y += -Idrivers/gpu/drm/msm/samsung

obj-y  += ss_dsi_panel_HX8279_TV101WUM.o
obj-$(CONFIG_PANEL_POWER_ISL98608)	+= isl98608-panel_power.o
