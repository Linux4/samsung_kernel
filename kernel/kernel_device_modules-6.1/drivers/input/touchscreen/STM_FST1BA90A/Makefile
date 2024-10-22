# Goodix Touchscreen Makefile
# This Makefile is only for tempory compiling use
# Use xxxproject/Makefile-release when release driver
#
#For display ESD recovery

ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/

obj-m += stm_fst1ba90a.o
stm_fst1ba90a-y += fts.o fts_proc.o ftsCompensation.o ftsCore.o ftsError.o ftsFrame.o ftsIO.o ftsTest.o ftsTime.o ftsTool.o ftsFlash.o ftsGesture.o