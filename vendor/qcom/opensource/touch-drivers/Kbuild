
KDIR := $(TOP)/kernel_platform/common

ifeq ($(CONFIG_ARCH_WAIPIO), y)
	include $(TOUCH_ROOT)/config/gki_waipiotouch.conf
	LINUX_INC += -include $(TOUCH_ROOT)/config/gki_waipiotouchconf.h
endif

#ifeq ($(CONFIG_ARCH_KALAMA), y)
	include $(TOUCH_ROOT)/config/gki_kalamatouch.conf
	LINUX_INC += -include $(TOUCH_ROOT)/config/gki_kalamatouchconf.h
#endif

#ifeq ($(CONFIG_ARCH_KHAJE), y)
	include $(TOUCH_ROOT)/config/gki_khajetouch.conf
	LINUX_INC += -include $(TOUCH_ROOT)/config/gki_khajetouchconf.h
#endif

LINUX_INC +=	-Iinclude/linux \
		-Iinclude/linux/drm \
		-Iinclude/linux/gunyah \
		-Iinclude/linux/input

CDEFINES +=	-DANI_LITTLE_BYTE_ENDIAN \
	-DANI_LITTLE_BIT_ENDIAN \
	-DDOT11F_LITTLE_ENDIAN_HOST \
	-DANI_COMPILER_TYPE_GCC \
	-DANI_OS_TYPE_ANDROID=6 \
	-DPTT_SOCK_SVC_ENABLE \
	-Wall\
	-Werror\
	-D__linux__

KBUILD_CPPFLAGS += $(CDEFINES)

ccflags-y += $(LINUX_INC)

ifeq ($(call cc-option-yn, -Wmaybe-uninitialized),y)
EXTRA_CFLAGS += -Wmaybe-uninitialized
endif

ifeq ($(call cc-option-yn, -Wheader-guard),y)
EXTRA_CFLAGS += -Wheader-guard
endif

######### CONFIG_MSM_TOUCH ########

ifeq ($(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX), y)

	LINUX_INC += -include $(TOUCH_ROOT)/synaptics_dsx/synaptics_dsx.h
	LINUX_INC += -include $(TOUCH_ROOT)/synaptics_dsx/synaptics_dsx_core.h

	synaptics_dsx-y := \
		 ./synaptics_dsx/synaptics_dsx_core.o \
		 ./synaptics_dsx/synaptics_dsx_i2c.o

	obj-$(CONFIG_MSM_TOUCH) += synaptics_dsx.o
endif

ifeq ($(CONFIG_TOUCH_FOCALTECH), y)
	LINUX_INC += -include $(TOUCH_ROOT)/focaltech_touch/focaltech_common.h
	LINUX_INC += -include $(TOUCH_ROOT)/focaltech_touch/focaltech_config.h
	LINUX_INC += -include $(TOUCH_ROOT)/focaltech_touch/focaltech_core.h
	LINUX_INC += -include $(TOUCH_ROOT)/focaltech_touch/focaltech_flash.h

	focaltech_fts-y := \
		 ./focaltech_touch/focaltech_core.o \
		 ./focaltech_touch/focaltech_ex_fun.o \
		 ./focaltech_touch/focaltech_ex_mode.o \
		 ./focaltech_touch/focaltech_gesture.o \
		 ./focaltech_touch/focaltech_esdcheck.o \
		 ./focaltech_touch/focaltech_point_report_check.o \
		 ./focaltech_touch/focaltech_i2c.o \
		 ./focaltech_touch/focaltech_flash.o \
		 ./focaltech_touch/focaltech_flash/focaltech_upgrade_ft3518.o

	obj-$(CONFIG_MSM_TOUCH) += focaltech_fts.o
endif

ifeq ($(CONFIG_TOUCHSCREEN_NT36XXX_I2C), y)
	LINUX_INC += -include $(TOUCH_ROOT)/nt36xxx/nt36xxx.h
	LINUX_INC += -include $(TOUCH_ROOT)/nt36xxx/nt36xxx_mem_map.h
	LINUX_INC += -include $(TOUCH_ROOT)/nt36xxx/nt36xxx_mp_ctrlram.h

	nt36xxx-i2c-y := \
		 ./nt36xxx/nt36xxx.o \
		 ./nt36xxx/nt36xxx_fw_update.o \
		 ./nt36xxx/nt36xxx_ext_proc.o \
		 ./nt36xxx/nt36xxx_mp_ctrlram.o

	obj-$(CONFIG_MSM_TOUCH) += nt36xxx-i2c.o
endif

ifeq ($(CONFIG_TOUCHSCREEN_GOODIX_BRL), y)
	LINUX_INC += -include $(TOUCH_ROOT)/goodix_berlin_driver/goodix_ts_core.h
	LINUX_INC += -include $(TOUCH_ROOT)/qts/qts_core.h
	LINUX_INC += -include $(TOUCH_ROOT)/qts/qts_core_common.h

	goodix_ts-y := \
		 ./goodix_berlin_driver/goodix_ts_core.o \
		 ./goodix_berlin_driver/goodix_brl_hw.o \
		 ./goodix_berlin_driver/goodix_cfg_bin.o \
		 ./goodix_berlin_driver/goodix_ts_utils.o \
		 ./goodix_berlin_driver/goodix_brl_fwupdate.o \
		 ./goodix_berlin_driver/goodix_ts_tools.o \
		 ./goodix_berlin_driver/goodix_ts_gesture.o \
		 ./goodix_berlin_driver/goodix_ts_inspect.o \
		 ./goodix_berlin_driver/goodix_brl_spi.o \
		 ./goodix_berlin_driver/goodix_brl_i2c.o \
		 ./qts/qts_core.o

	obj-$(CONFIG_MSM_TOUCH) += goodix_ts.o
endif

ifeq ($(CONFIG_TOUCHSCREEN_ATMEL_MXT), y)

	atmel_mxt_ts-y := \
		 ./atmel_mxt/atmel_mxt_ts.o

	obj-$(CONFIG_MSM_TOUCH) += atmel_mxt_ts.o
endif

ifeq ($(CONFIG_TOUCHSCREEN_DUMMY), y)
	dummy_ts-y := ./dummy_touch/dummy_touch.o

	obj-$(CONFIG_MSM_TOUCH) += dummy_ts.o
endif

ifeq ($(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM), y)
	synaptics_tcm_ts-y := \
		 ./synaptics_tcm/synaptics_tcm_core.o \
		 ./synaptics_tcm/synaptics_tcm_i2c.o \
		 ./synaptics_tcm/synaptics_tcm_touch.o

	obj-$(CONFIG_MSM_TOUCH) += synaptics_tcm_ts.o

endif

CDEFINES += -DBUILD_TIMESTAMP=\"$(shell date -u +'%Y-%m-%dT%H:%M:%SZ')\"
