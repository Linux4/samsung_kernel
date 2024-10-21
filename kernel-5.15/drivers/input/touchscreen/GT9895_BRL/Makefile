ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

obj-m += gt9895_brl.o
gt9895_brl-y := \
			sec_common_fn.o sec_cmd.o sec_tclm_v2.o \
			goodix_brl_i2c.o goodix_ts_core.o goodix_brl_hw.o \
			goodix_cfg_bin.o goodix_ts_utils.o goodix_brl_fwupdate.o goodix_ts_gesture.o \
			goodix_ts_inspect.o goodix_ts_tools.o goodix_ts_cmd.o
ifeq ($(CONFIG_GCOV_KERNEL),y)
	GCOV_PROFILE := y
endif