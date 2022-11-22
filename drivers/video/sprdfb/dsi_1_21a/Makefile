obj-$(CONFIG_FB_SC8825) := mipi_dsih_api.o mipi_dsih_dphy.o mipi_dsih_hal.o
obj-$(CONFIG_FB_SCX35) := mipi_dsih_api.o mipi_dsih_dphy.o mipi_dsih_hal.o

# ========== Pike configuration begin =======
obj-$(CONFIG_FB_SCX30G) := mipi_dsih_api.o mipi_dsih_hal.o
ifeq ($(CONFIG_FB_USE_MEGACORES_MIPI_DPHY), y)
obj-$(CONFIG_FB_SCX30G) += mipi_dsih_dphy_megacores.o
else
obj-$(CONFIG_FB_SCX30G) += mipi_dsih_dphy.o
endif
# ========== Pike configuration end =========
obj-$(CONFIG_FB_SCX35L) := mipi_dsih_api.o mipi_dsih_dphy.o mipi_dsih_hal.o
