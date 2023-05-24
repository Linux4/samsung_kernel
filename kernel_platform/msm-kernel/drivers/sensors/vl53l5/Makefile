################################################################################
# Copyright (c) 2021, STMicroelectronics - All Rights Reserved
#
# This file is part of VL53L5 Kernel Driver and is dual licensed,
# either 'STMicroelectronics Proprietary license'
# or 'BSD 3-clause "New" or "Revised" License' , at your option.
#
################################################################################
#
# 'STMicroelectronics Proprietary license'
#
################################################################################
#
# License terms: STMicroelectronics Proprietary in accordance with licensing
# terms at www.st.com/sla0081
#
# STMicroelectronics confidential
# Reproduction and Communication of this document is strictly prohibited unless
# specifically authorized in writing by STMicroelectronics.
#
#
################################################################################
#
# Alternatively, VL53L5 Kernel Driver may be distributed under the terms of
# 'BSD 3-clause "New" or "Revised" License', in which case the following
# provisions apply instead of the ones mentioned above :
#
################################################################################
#
# License terms: BSD 3-clause "New" or "Revised" License.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors
# may be used to endorse or promote products derived from this software
# without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#
###############################################################################/

# Parameters
MODULE := vl53l5
CONFIG_STMVL53L5 := m
# Configure the build type : RELEASE, DEBUG
BUILD_TYPE = RELEASE

# Sets the servicing mode to use i.e. INTERRUPT, WORKER or DEFAULT
RANGE_SERVICING = INTERRUPT

# Set to TRUE to enable TCDM dump
TCDM_DUMP_ENABLE = FALSE

# Set to TRUE to enable offset calibration
OFFSET_CAL_ENABLE = FALSE

# Set to TRUE to enable code for internal testing
LEGACY_CODE = FALSE

# Set to TRUE to enable logging functions.
LOG_ENABLE = TRUE

# Set to TRUE to enable force error.
FORCE_ERROR = FALSE

# Set to TRUE to enable kernel HAP tests.
HAP_TEST = FALSE

# Set to TRUE to enable kernel Depth16 HAP.
HAP_D16_ENABLE = FALSE

KDIR = /lib/modules/$(shell uname -r)/build

ccflags-y += -I$(srctree)/drivers/sensors/vl53l5/inc
ccflags-y += -I$(srctree)/drivers/sensors/vl53l5/bare_driver/common/inc
ccflags-y += -I$(srctree)/drivers/sensors/vl53l5/bare_driver/api/inc
ccflags-y += -I$(srctree)/drivers/sensors/vl53l5/bare_driver/dci/inc
ccflags-y += -I$(srctree)/drivers/sensors/vl53l5/platform/inc
ccflags-y += -I$(srctree)/drivers/sensors/vl53l5/hap/anti_flicker_filter/inc
ccflags-y += -I$(srctree)/drivers/sensors/vl53l5/hap/dci_patch_datatype_library/inc
ccflags-y += -I$(srctree)/drivers/sensors/vl53l5/hap/depth16/inc
ccflags-y += -I$(srctree)/drivers/sensors/vl53l5/hap/depth16_buffer/inc
ccflags-y += -I$(srctree)/drivers/sensors/vl53l5/hap/glass_detection/inc
ccflags-y += -I$(srctree)/drivers/sensors/vl53l5/hap/long_tail_filter/inc
ccflags-y += -I$(srctree)/drivers/sensors/vl53l5/hap/output_target_filter/inc
ccflags-y += -I$(srctree)/drivers/sensors/vl53l5/hap/output_target_sort/inc
ccflags-y += -I$(srctree)/drivers/sensors/vl53l5/hap/pipe/inc
ccflags-y += -I$(srctree)/drivers/sensors/vl53l5/hap/range_rotation/inc

ccflags-y += -Wall -Werror -Wno-missing-braces

ifeq "$(TEST_ENABLE)" "TRUE"
  $(warning TEST_ENABLE=$(TEST_ENABLE))
  ccflags-y += -DVL53L5_TEST_ENABLE
endif

ifeq "$(HAP_TEST)" "TRUE"
  $(warning HAP_TEST=$(HAP_TEST))
  ccflags-y += -I$(src)/hap/pipe/test/inc
  ccflags-y += -DVL53L5_HAP_TESTS
endif

ifeq "$(OFFSET_CAL_ENABLE)" "TRUE"
  $(warning OFFSET_CAL_ENABLE=$(OFFSET_CAL_ENABLE))
  ccflags-y += -DVL53L5_OFFSET_CAL
endif

ifeq "$(BUILD_TYPE)" "DEBUG"
  $(warning BUILD_TYPE=$(BUILD_TYPE))
  ccflags-y += -DDEBUG -DVL53L5_LOG_ENABLE
endif

ifeq "$(BUILD_TYPE)" "RELEASE"
  $(warning BUILD_TYPE=$(BUILD_TYPE))
endif


ifeq "$(LOG_ENABLE)" "TRUE"
  $(warning LOG_ENABLE=$(LOG_ENABLE))
  ccflags-y += -DVL53L5_KERNEL_LOG
endif

ifeq "$(TCDM_DUMP_ENABLE)" "TRUE"
  $(warning TCDM_DUMP_ENABLE=$(TCDM_DUMP_ENABLE))
  ccflags-y += -DVL53L5_TCDM_DUMP
  BARE_DRIVER_OBJS += bare_driver/common/src/vl53l5_tcdm_dump.o
endif

ifeq "$(LEGACY_CODE)" "TRUE"
  $(warning LEGACY_CODE=$(LEGACY_CODE))
  ccflags-y += -DSTM_VL53L5_SUPPORT_LEGACY_CODE
endif

ifeq "$(FORCE_ERROR)" "TRUE"
  $(warning FORCE_ERROR=$(FORCE_ERROR))
  ccflags-y += -DSTM_VL53L5_FORCE_ERROR_COMMAND
endif

KERNEL_DRIVER_OBJS = \
	src/vl53l5_k_module.o \
	src/vl53l5_k_ioctl_controls.o \
	src/vl53l5_k_gpio_utils.o \
	src/vl53l5_k_error_converter.o \
	src/vl53l5_k_hap_tuning.o \
	src/vl53l5_k_range_wait_handler.o

PLATFORM_OBJS = \
	platform/src/vl53l5_platform.o \
	platform/src/vl53l5_platform_init.o \
	platform/src/vl53l5_platform_maths.o \
	platform/src/vl53l5_platform_log.o

BARE_DRIVER_OBJS = \
	bare_driver/api/src/vl53l5_api_core.o \
	bare_driver/api/src/vl53l5_api_power.o \
	bare_driver/api/src/vl53l5_api_ranging.o \
	bare_driver/api/src/vl53l5_api_range_decode.o \
	bare_driver/api/src/vl53l5_api_calibration_decode.o\
	bare_driver/dci/src/vl53l5_core_decode.o\
	bare_driver/dci/src/vl53l5_dci_core.o \
	bare_driver/dci/src/vl53l5_dci_decode.o \
	bare_driver/dci/src/vl53l5_dci_helpers.o \
	bare_driver/dci/src/vl53l5_dci_ranging.o \
	bare_driver/dci/src/vl53l5_dci_utils.o \
	bare_driver/dci/src/vl53l5_decode_switch.o\
	bare_driver/dci/src/vl53l5_results_decode.o \
	bare_driver/dci/src/vl53l5_calibration_decode.o \
	bare_driver/common/src/vl53l5_checks.o \
	bare_driver/common/src/vl53l5_commands.o \
	bare_driver/common/src/vl53l5_error_handler.o \
	bare_driver/common/src/vl53l5_load_firmware.o \
	bare_driver/common/src/vl53l5_register_utils.o \
	bare_driver/common/src/vl53l5_rom_boot.o

HAP_OBJS = \
	hap/anti_flicker_filter/src/vl53l5_anti_flicker_filter__set_cfg_main.o \
	hap/anti_flicker_filter/src/vl53l5_anti_flicker_filter_funcs.o \
	hap/anti_flicker_filter/src/vl53l5_anti_flicker_filter_main.o \
	hap/glass_detection/src/vl53l5_glass_detection_main.o\
	hap/glass_detection/src/vl53l5_glass_detection__set_cfg_main.o\
	hap/glass_detection/src/vl53l5_glass_detection_funcs.o\
	hap/long_tail_filter/src/vl53l5_long_tail_filter_main.o \
	hap/long_tail_filter/src/vl53l5_long_tail_filter__set_cfg_main.o\
	hap/long_tail_filter/src/vl53l5_long_tail_filter_funcs.o \
	hap/output_target_filter/src/vl53l5_output_target_filter__set_cfg_main.o\
	hap/output_target_filter/src/vl53l5_output_target_filter_funcs.o\
	hap/output_target_filter/src/vl53l5_output_target_filter_main.o\
	hap/output_target_sort/src/vl53l5_output_target_sort__set_cfg_main.o\
	hap/output_target_sort/src/vl53l5_output_target_sort_funcs.o\
	hap/output_target_sort/src/vl53l5_output_target_sort_main.o\
	hap/pipe/src/vl53l5_host_algo_pipe__set_cfg.o\
	hap/pipe/src/vl53l5_host_algo_pipe_funcs.o\
	hap/pipe/src/vl53l5_host_algo_pipe_init.o\
	hap/pipe/src/vl53l5_host_algo_pipe.o\
	hap/range_rotation/src/vl53l5_range_rotation__set_cfg_main.o\
	hap/range_rotation/src/vl53l5_range_rotation_funcs.o\
	hap/range_rotation/src/vl53l5_range_rotation_main.o

HAP_D16_OBJS = \
	hap/depth16/src/vl53l5_depth16__set_cfg_main.o \
	hap/depth16/src/vl53l5_depth16_funcs.o \
	hap/depth16/src/vl53l5_depth16_main.o \
	hap/depth16_buffer/src/vl53l5_depth16_block_funcs.o \
	hap/depth16_buffer/src/vl53l5_depth16_buffer_funcs.o \
	hap/depth16_buffer/src/vl53l5_depth16_buffer_main.o \
	hap/depth16_buffer/src/vl53l5_depth16_packet_funcs.o \
	hap/depth16_buffer/src/vl53l5_depth16_packet_main.o
	
HAP_TEST_OBJS = \
	hap/pipe/test/src/test_hap_dev_sizes.o \
	hap/pipe/test/src/test_hap_get_version.o \
	hap/pipe/test/src/test_host_algo_pipe.o \
	hap/pipe/test/src/test_utils.o \
	hap/pipe/test/src/run_hap_tests.o \
	hap/pipe/test/src/vl53l5_range_results_checker.o \
	hap/pipe/test/src/vl53l5_utils_chk_common.o \
	hap/pipe/test/src/vl53l5_utils_chk_structs_results.o \
	hap/pipe/test/src/test_hap_update_aff_cfg.o

ifeq "$(RANGE_SERVICING)" "INTERRUPT"
  $(warning RANGE_SERVICING=$(RANGE_SERVICING))
  ccflags-y	+= -DVL53L5_INTERRUPT
  KERNEL_DRIVER_OBJS += src/vl53l5_k_interrupt.o
endif

ifeq "$(RANGE_SERVICING)" "WORKER"
  $(warning RANGE_SERVICING=$(RANGE_SERVICING))
  ccflags-y	+= -DVL53L5_WORKER
  KERNEL_DRIVER_OBJS += src/vl53l5_k_work_handler.o
endif


obj-$(CONFIG_SENSORS_VL53L5)	+= $(MODULE).o

# # Kernel level objects
$(MODULE)-objs	+= 	$(KERNEL_DRIVER_OBJS)
$(MODULE)-objs +=	$(HAP_OBJS)
$(MODULE)-objs +=	$(BARE_DRIVER_OBJS)
$(MODULE)-objs +=	$(PLATFORM_OBJS)
ifeq "$(HAP_TEST)" "TRUE"
$(MODULE)-objs +=	$(HAP_TEST_OBJS)
endif
ifeq "$(HAP_D16_ENABLE)" "TRUE"
$(MODULE)-objs +=	$(HAP_D16_OBJS)
endif

