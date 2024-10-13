/*
 * Copyright (c) 2020,2021 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *	* Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *	* Redistributions in binary form must reproduce the above
 *	  copyright notice, this list of conditions and the following
 *	  disclaimer in the documentation and/or other materials provided
 *	  with the distribution.
 *	* Neither the name of The Linux Foundation nor the names of its
 *	  contributors may be used to endorse or promote products derived
 *	  from this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *	* Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *	* Redistributions in binary form must reproduce the above
 *	  copyright notice, this list of conditions and the following
 *	  disclaimer in the documentation and/or other materials provided
 *	  with the distribution.
 *	* Neither the name of Qualcomm Innovation Center, Inc. nor the
 *	  names of its contributors may be used to endorse or promote products
 *	  derived from this software without specific prior written permission.
 *
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED
 * BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS
 * AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <unordered_map>
#include <android-base/logging.h>
#include <android/hardware/thermal/2.0/IThermal.h>
#include <utility>

#include "thermalData.h"
#include "thermalConfig.h"

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {
	constexpr std::string_view socIDPath("/sys/devices/soc0/soc_id");
	constexpr std::string_view hwPlatformPath("/sys/devices/soc0/hw_platform");

	std::vector<std::string> cpu_sensors_bengal =
	{
		"cpuss-2-usr",
		"cpuss-2-usr",
		"cpuss-2-usr",
		"cpuss-2-usr",
		"cpu-1-0-usr",
		"cpu-1-1-usr",
		"cpu-1-2-usr",
		"cpu-1-3-usr",
	};

	std::vector<struct target_therm_cfg> sensor_cfg_bengal =
	{
		{
			TemperatureType::CPU,
			cpu_sensors_bengal,
			"",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpu-usr" },
			"GPU",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::SKIN,
			{ "xo-therm-usr" },
			"skin",
			40000,
			118000,
			40000,
			true,
		},
		{
			TemperatureType::BCL_VOLTAGE,
			{ "pmi632-vbat-lvl0" },
			"vbat",
			3000,
			2800,
			3000,
			false,
		},
		{
			TemperatureType::BCL_CURRENT,
			{ "pmi632-ibat-lvl0" },
			"ibat",
			4000,
			4200,
			4000,
			true,
		},
		{
			TemperatureType::BCL_PERCENTAGE,
			{ "soc" },
			"soc",
			10,
			2,
			10,
			false,
		},
	};

	std::vector<std::string> cpu_sensors_trinket =
	{
		"cpuss-0-usr",
		"cpuss-0-usr",
		"cpuss-0-usr",
		"cpuss-0-usr",
		"cpu-1-0-usr",
		"cpu-1-1-usr",
		"cpu-1-2-usr",
		"cpu-1-3-usr",
	};

	std::vector<struct target_therm_cfg> sensor_cfg_trinket =
	{
		{
			TemperatureType::CPU,
			cpu_sensors_trinket,
			"",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpu-usr" },
			"GPU",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::SKIN,
			{ "xo-therm-adc" },
			"skin",
			40000,
			118000,
			40000,
			true,
		},
		{
			TemperatureType::BCL_VOLTAGE,
			{ "pmi632-vbat-lvl0" },
			"vbat",
			3000,
			2800,
			3000,
			false,
		},
		{
			TemperatureType::BCL_CURRENT,
			{ "pmi632-ibat-lvl0" },
			"ibat",
			4000,
			4200,
			4000,
			true,
		},
		{
			TemperatureType::BCL_PERCENTAGE,
			{ "soc" },
			"soc",
			10,
			2,
			10,
			false,
		},
	};

	std::vector<std::string> cpu_sensors_lito =
	{
		"cpu-0-0-usr",
		"cpu-0-1-usr",
		"cpu-0-2-usr",
		"cpu-0-3-usr",
		"cpu-0-4-usr",
		"cpu-0-5-usr",
		"cpu-1-0-usr",
		"cpu-1-2-usr",
	};

	std::vector<struct target_therm_cfg> sensor_cfg_lito =
	{
		{
			TemperatureType::CPU,
			cpu_sensors_lito,
			"",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-0-usr" },
			"GPU",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::SKIN,
			{ "xo-therm-usr" },
			"skin",
			40000,
			118000,
			40000,
			true,
		},
		{
			TemperatureType::BCL_CURRENT,
			{ "pm7250b-ibat-lvl0" },
			"ibat",
			4500,
			5000,
			4500,
			true,
		},
		{
			TemperatureType::BCL_VOLTAGE,
			{ "pm7250b-vbat-lvl0" },
			"vbat",
			3200,
			3000,
			3200,
			false,
		},
		{
			TemperatureType::BCL_PERCENTAGE,
			{ "soc" },
			"soc",
			10,
			2,
			10,
			false,
		},
	};

	std::vector<struct target_therm_cfg> sensor_cfg_sdmmagpie =
	{
		{
			TemperatureType::CPU,
			cpu_sensors_lito,
			"",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-0-usr" },
			"GPU",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::SKIN,
			{ "xo-therm-adc" },
			"skin",
			40000,
			118000,
			40000,
			true,
		},
		{
			TemperatureType::BCL_VOLTAGE,
			{ "pm6150-vbat-lvl0" },
			"vbat",
			3000,
			2800,
			3000,
			false,
		},
		{
			TemperatureType::BCL_CURRENT,
			{ "pm6150-ibat-lvl0" },
			"ibat",
			5500,
			6000,
			5500,
			true,
		},
		{
			TemperatureType::BCL_PERCENTAGE,
			{ "soc" },
			"soc",
			10,
			2,
			10,
			false,
		},
	};

	std::vector<struct target_therm_cfg> sensor_cfg_holi =
	{
		{
			TemperatureType::CPU,
			cpu_sensors_lito,
			"",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-0-usr" },
			"gpu0",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-1-usr" },
			"gpu1",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::SKIN,
			{ "quiet-therm-usr" },
			"skin",
			40000,
			118000,
			40000,
			true,
		},
		{
			TemperatureType::BCL_CURRENT,
			{ "pm7250b-ibat-lvl0" },
			"ibat",
			5500,
			6000,
			5500,
			true,
		},
	};

	std::vector<std::string> cpu_sensors_kona =
	{
		"cpu-0-0-usr",
		"cpu-0-1-usr",
		"cpu-0-2-usr",
		"cpu-0-3-usr",
		"cpu-1-0-usr",
		"cpu-1-1-usr",
		"cpu-1-2-usr",
		"cpu-1-3-usr",
	};

	std::vector<struct target_therm_cfg>  sensor_cfg_msmnile = {
		{
			TemperatureType::CPU,
			cpu_sensors_kona,
			"",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-0-usr" },
			"gpu0",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-1-usr" },
			"gpu1",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::SKIN,
			{ "xo-therm" },
			"skin",
			40000,
			118000,
			40000,
			true,
		},
		{
			TemperatureType::BCL_CURRENT,
			{ "pm8150b-ibat-lvl0" },
			"ibat",
			4500,
			5000,
			4500,
			true,
		},
		{
			TemperatureType::BCL_VOLTAGE,
			{ "pm8150b-vbat-lvl0" },
			"vbat",
			3200,
			3000,
			3200,
			false,
		},
		{
			TemperatureType::BCL_PERCENTAGE,
			{ "soc" },
			"soc",
			10,
			2,
			10,
			false,
		},
	};

	std::vector<struct target_therm_cfg>  kona_common = {
		{
			TemperatureType::CPU,
			cpu_sensors_kona,
			"",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-0-usr" },
			"GPU0",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-1-usr" },
			"GPU1",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::SKIN,
			{ "skin-msm-therm-usr" },
			"skin",
			40000,
			118000,
			40000,
			true,
		}
	};

	std::vector<struct target_therm_cfg>  kona_specific = {
		{
			TemperatureType::BCL_CURRENT,
			{ "pm8150b-ibat-lvl0" },
			"ibat",
			4500,
			5000,
			4500,
			true,
		},
		{
			TemperatureType::BCL_VOLTAGE,
			{ "pm8150b-vbat-lvl0" },
			"vbat",
			3200,
			3000,
			3200,
			false,
		},
		{
			TemperatureType::BCL_PERCENTAGE,
			{ "soc" },
			"soc",
			10,
			2,
			10,
			false,
		},
		{
			TemperatureType::NPU,
			{ "npu-usr" },
			"npu",
			115000,
			118000,
			115000,
			true,
		},
	};

	std::vector<std::string> cpu_sensors_lahaina =
	{
		"cpu-0-0-usr",
		"cpu-0-1-usr",
		"cpu-0-2-usr",
		"cpu-0-3-usr",
		"cpu-1-0-usr",
		"cpu-1-2-usr",
		"cpu-1-4-usr",
		"cpu-1-6-usr",
	};

	std::vector<struct target_therm_cfg>  lahaina_common = {
		{
			TemperatureType::CPU,
			cpu_sensors_lahaina,
			"",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-0-usr" },
			"GPU0",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-1-usr" },
			"GPU1",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::BCL_CURRENT,
			{ "pm8350b-ibat-lvl0" },
			"ibat",
			6000,
			7500,
			6000,
			true,
		},
		{
			TemperatureType::NPU,
			{ "nspss-0-usr" },
			"nsp0",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::NPU,
			{ "nspss-1-usr" },
			"nsp1",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::NPU,
			{ "nspss-2-usr" },
			"nsp2",
			115000,
			118000,
			115000,
			true,
		},
	};

	std::vector<struct target_therm_cfg>  lahaina_specific = {
		{
			TemperatureType::SKIN,
			{ "xo-therm-usr" },
			"skin",
			55000,
			118000,
			55000,
			true,
		},
	};

	std::vector<struct target_therm_cfg>  shima_specific = {
		{
			TemperatureType::SKIN,
			{ "quiet-therm-usr" },
			"skin",
			40000,
			118000,
			40000,
			true,
		},
	};

	std::vector<struct target_therm_cfg>  sensor_cfg_yupik = {
		{
			TemperatureType::CPU,
			cpu_sensors_lahaina,
			"",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-0-usr" },
			"GPU0",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-1-usr" },
			"GPU1",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::NPU,
			{ "nspss-0-usr" },
			"nsp0",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::NPU,
			{ "nspss-1-usr" },
			"nsp1",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::SKIN,
			{ "quiet-therm-usr" },
			"skin",
			40000,
			118000,
			40000,
			true,
		},
	};

	std::vector<std::string> cpu_sensors_waipio =
	{
		"cpu-0-0",
		"cpu-0-1",
		"cpu-0-2",
		"cpu-0-3",
		"cpu-1-0",
		"cpu-1-2",
		"cpu-1-4",
		"cpu-1-6",
	};

	std::vector<struct target_therm_cfg>  waipio_common = {
		{
			TemperatureType::CPU,
			cpu_sensors_waipio,
			"",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-0" },
			"GPU0",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-1" },
			"GPU1",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::NPU,
			{ "nspss-0" },
			"nsp0",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::NPU,
			{ "nspss-1" },
			"nsp1",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::NPU,
			{ "nspss-2" },
			"nsp2",
			115000,
			118000,
			115000,
			true,
		},
	};

	std::vector<struct target_therm_cfg>  diwali_common = {
		{
			TemperatureType::CPU,
			cpu_sensors_waipio,
			"",
			115000,
			118000,
			115000,
			true,
			ThrottlingSeverity::LIGHT,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-0" },
			"GPU0",
			115000,
			118000,
			115000,
			true,
			ThrottlingSeverity::LIGHT,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-1" },
			"GPU1",
			115000,
			118000,
			115000,
			true,
			ThrottlingSeverity::LIGHT,
		},
		{
			TemperatureType::NPU,
			{ "nspss-0" },
			"nsp0",
			115000,
			118000,
			115000,
			true,
			ThrottlingSeverity::LIGHT,
		},
		{
			TemperatureType::NPU,
			{ "nspss-1" },
			"nsp1",
			115000,
			118000,
			115000,
			true,
			ThrottlingSeverity::LIGHT,
		},
		{
			TemperatureType::NPU,
			{ "nspss-2" },
			"nsp2",
			115000,
			118000,
			115000,
			true,
			ThrottlingSeverity::LIGHT,
		},
	};

	std::vector<struct target_therm_cfg>  waipio_specific = {
		{
			TemperatureType::BCL_CURRENT,
			{ "pm8350b-ibat-lvl0" },
			"ibat",
			6000,
			7500,
			6000,
			true,
		},
		{
			TemperatureType::SKIN,
			{ "xo-therm" },
			"skin",
			55000,
			118000,
			55000,
			true,
		},
	};

	std::vector<struct target_therm_cfg>  diwali_specific = {
		{
			TemperatureType::BCL_CURRENT,
			{ "pm7250b-ibat-lvl0" },
			"ibat",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::SKIN,
			{ "quiet-therm" },
			"skin",
			115000,
			118000,
			115000,
			true,
			ThrottlingSeverity::LIGHT,
		},
	};

	std::vector<std::string> cpu_sensors_neo =
	{
		"cpu-0-0",
		"cpu-0-1",
		"cpu-0-2",
		"cpu-0-3",
	};

	std::vector<struct target_therm_cfg>  neo_common = {
		{
			TemperatureType::CPU,
			cpu_sensors_neo,
			"",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-0" },
			"GPU0",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-1" },
			"GPU1",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::NPU,
			{ "nspss-0" },
			"nsp0",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::NPU,
			{ "nspss-1" },
			"nsp1",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::NPU,
			{ "nspss-2" },
			"nsp2",
			115000,
			118000,
			115000,
			true,
		},
	};

	std::vector<std::string> cpu_sensors_parrot =
	{
		"cpu-0-0",
		"cpu-0-1",
		"cpu-0-2",
		"cpu-0-3",
		"cpu-1-0",
		"cpu-1-2",
		"cpu-1-4",
		"cpu-1-6",
	};

	std::vector<struct target_therm_cfg>  parrot_common = {
		{
			TemperatureType::CPU,
			cpu_sensors_parrot,
			"",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-0" },
			"GPU0",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-1" },
			"GPU1",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::NPU,
			{ "nspss-0" },
			"nsp0",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::NPU,
			{ "nspss-1" },
			"nsp1",
			115000,
			118000,
			115000,
			true,
		},
	};

	std::vector<struct target_therm_cfg>  parrot_specific = {
		{
			TemperatureType::BCL_CURRENT,
			{ "pm7250b-ibat-lvl0" },
			"ibat",
			6000,
			7500,
			6000,
			true,
		},
		{
			TemperatureType::SKIN,
			{ "xo-therm" },
			"skin",
			55000,
			118000,
			55000,
			true,
		},
	};

	std::vector<std::string> cpu_sensors_anorak =
	{
		"cpu-0-0-0",
		"cpu-0-0-1",
		"cpu-0-1-0",
		"cpu-0-1-1",
		"cpu-1-0-0",
		"cpu-1-0-1",
		"cpu-1-1-0",
		"cpu-1-1-1",
		"cpu-1-2-0",
		"cpu-1-2-1",
		"cpu-1-3-0",
		"cpu-1-3-1",
	};

	std::vector<struct target_therm_cfg>  anorak_common = {
		{
			TemperatureType::CPU,
			cpu_sensors_anorak,
			"",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::NPU,
			{ "nspss-0" },
			"nsp0",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::NPU,
			{ "nspss-1" },
			"nsp1",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::NPU,
			{ "nspss-2" },
			"nsp2",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-0" },
			"GPU0",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-1" },
			"GPU1",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-2" },
			"GPU2",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-3" },
			"GPU3",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-4" },
			"GPU4",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-5" },
			"GPU5",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-6" },
			"GPU6",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss-7" },
			"GPU7",
			115000,
			118000,
			115000,
			true,
		},
	};
	std::vector<struct target_therm_cfg>  anorak_specific = {
		{
			TemperatureType::BCL_CURRENT,
			{ "pm8550b-ibat-lvl0" },
			"ibat",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::SKIN,
			{ "sys-therm-0" },
			"skin",
			55000,
			118000,
			55000,
			true,
		},
	};

	std::vector<std::string> cpu_sensors_ravelin =
	{
		"cpu-0-0",
		"cpu-0-1",
		"cpu-0-2",
		"cpu-0-3",
		"cpu-0-4",
		"cpu-0-5",
		"cpu-1-0",
		"cpu-1-2",
	};

	std::vector<struct target_therm_cfg>  ravelin_common = {
		{
			TemperatureType::CPU,
			cpu_sensors_ravelin,
			"",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::GPU,
			{ "gpuss" },
			"GPU",
			115000,
			118000,
			115000,
			true,
		},
		{
			TemperatureType::SKIN,
			{ "sys-therm-1" },
			"skin",
			55000,
			118000,
			55000,
			true,
		},
	};

	std::vector<struct target_therm_cfg>  ravelin_specific_qrd = {
		{
			TemperatureType::BCL_CURRENT,
			{ "pmi632-ibat-lvl0" },
			"ibat",
			6000,
			7500,
			6000,
			true,
		},
	};

	std::vector<struct target_therm_cfg>  ravelin_specific_idp = {
		{
			TemperatureType::BCL_CURRENT,
			{ "pm7250b-ibat-lvl0" },
			"ibat",
			6000,
			7500,
			6000,
			true,
		},
	};

	struct target_therm_cfg bat_conf = {
		TemperatureType::BATTERY,
		{ "battery" },
		"battery",
		80000,
		90000,
		80000,
		true,
	};

	std::vector<struct target_therm_cfg> bcl_conf = {
		{
			TemperatureType::BCL_VOLTAGE,
			{ "vbat" },
			"vbat",
			3200,
			3000,
			3200,
			false,
		},
		{
			TemperatureType::BCL_PERCENTAGE,
			{ "socd" },
			"socd",
			115,
			118,
			115,
			true,
		},
	};

	const std::unordered_map<int, std::vector<struct target_therm_cfg>>
		msm_soc_map = {
		{417, sensor_cfg_bengal}, // bengal
		{420, sensor_cfg_bengal},
		{444, sensor_cfg_bengal},
		{445, sensor_cfg_bengal},
		{469, sensor_cfg_bengal},
		{470, sensor_cfg_bengal},
		{394, sensor_cfg_trinket},
		{467, sensor_cfg_trinket},
		{468, sensor_cfg_trinket},
		{400, sensor_cfg_lito}, // lito
		{440, sensor_cfg_lito},
		{407, sensor_cfg_lito}, // atoll
		{365, sensor_cfg_sdmmagpie},
		{366, sensor_cfg_sdmmagpie},
		{434, sensor_cfg_lito}, // lagoon
		{435, sensor_cfg_lito},
		{459, sensor_cfg_lito},
		{476, sensor_cfg_lito}, // orchid
		{339, sensor_cfg_msmnile},
		{361, sensor_cfg_msmnile},
		{362, sensor_cfg_msmnile},
		{367, sensor_cfg_msmnile},
		{356, kona_common}, // kona
		{415, lahaina_common}, // lahaina
		{439, lahaina_common}, // lahainap
		{456, lahaina_common}, // lahaina-atp
		{501, lahaina_common},
		{502, lahaina_common},
		{450, lahaina_common}, // shima
		{454, sensor_cfg_holi}, // holi
		{475, sensor_cfg_yupik}, // yupik
		{515, sensor_cfg_yupik}, // YUPIK-LTE
		{457, waipio_common}, //Waipio
		{482, waipio_common}, //Waipio
		{552, waipio_common}, //Waipio-LTE
		{506, diwali_common}, //diwali
		{547, diwali_common}, //diwali
		{564, diwali_common}, //diwali-LTE
		{530, waipio_common}, // cape
		{531, waipio_common}, // cape
		{540, waipio_common}, // cape
		{525, neo_common},
		{554, neo_common},
		{537, parrot_common}, //Netrani mobile
		{583, parrot_common}, //Netrani mobile without modem
		{613, parrot_common}, //Netrani APQ
		{631, parrot_common},
		{549, anorak_common},
		{568, ravelin_common}, //Clarence Mobile
		{581, ravelin_common}, //Clarence IOT
		{582, ravelin_common}, //Clarence IOT without modem
		{591, waipio_common}, //ukee
	};

	const std::unordered_map<int, std::vector<struct target_therm_cfg>>
		msm_soc_specific = {
		{356, kona_specific}, // kona
		{415, lahaina_specific}, // lahaina
		{439, lahaina_specific}, // lahainap
		{456, lahaina_specific}, // lahaina-atp
		{501, lahaina_specific},
		{502, lahaina_specific},
		{450, shima_specific}, // shima
		{457, waipio_specific}, //Waipio
		{482, waipio_specific}, //Waipio
		{552, waipio_specific}, //Waipio-LTE
		{506, diwali_specific}, //diwali
		{547, diwali_specific}, //diwali
		{564, diwali_specific}, //diwali-LTE
		{530, waipio_specific}, // cape
		{531, waipio_specific}, // cape
		{540, waipio_specific}, // cape
		{537, parrot_specific}, //Netrani mobile
		{583, parrot_specific}, //Netrani mobile without modem
		{613, parrot_specific}, //Netrani APQ
		{631, parrot_specific},
		{549, anorak_specific},
		{591, waipio_specific}, //ukee
	};

	const std::unordered_multimap<int, std::pair<std::string,
				std::vector<struct target_therm_cfg>>>
		msm_platform_specific = {
		{568, std::make_pair("QRD", ravelin_specific_qrd)},
		{568, std::make_pair("IDP", ravelin_specific_idp)},
	};

	std::vector<struct target_therm_cfg> add_target_config(
			int socID, std::string hwPlatform,
			std::vector<struct target_therm_cfg> conf)
	{
		std::vector<struct target_therm_cfg> targetConf;

		if (msm_soc_specific.find(socID) != msm_soc_specific.end()) {
			targetConf = (msm_soc_specific.find(socID))->second;
			conf.insert(conf.end(), targetConf.begin(),
					targetConf.end());
		}

		auto range = msm_platform_specific.equal_range(socID);
		auto it = range.first;
		for (; it != range.second; ++it) {
			if (it->second.first != hwPlatform)
				continue;

			targetConf = it->second.second;
			conf.insert(conf.end(), targetConf.begin(),
					targetConf.end());
			break;
		}

		return conf;
	}

	ThermalConfig::ThermalConfig():cmnInst()
	{
		std::unordered_map<int, std::vector<struct target_therm_cfg>>::const_iterator it;
		std::vector<struct target_therm_cfg>::iterator it_vec;
		bool bcl_defined = false;
		std::string soc_val;
		int ct = 0;
		bool read_ok = false;

		soc_id = 0;
		do {
			if (cmnInst.readFromFile(socIDPath, soc_val) <= 0) {
				LOG(ERROR) <<"soc ID fetch error";
				return;
			}

			if (cmnInst.readFromFile(hwPlatformPath, hw_platform) <= 0) {
				LOG(ERROR) <<"hw Platform fetch error";
				continue;
			}

			try {
				soc_id = std::stoi(soc_val, nullptr, 0);
				read_ok = true;
			}
			catch (std::exception &err) {
				LOG(ERROR) <<"soc id stoi err:" << err.what()
					<< " buf:" << soc_val;
			}
		} while (ct++ && !read_ok && ct < RETRY_CT);
		if (soc_id <= 0) {
			LOG(ERROR) << "Invalid soc ID: " << soc_id;
			return;
		}
		it = msm_soc_map.find(soc_id);
		if (it == msm_soc_map.end()) {
			LOG(ERROR) << "No config for soc ID: " << soc_id;
			return;
		}
		thermalConfig = add_target_config(soc_id, hw_platform, it->second);
		for (it_vec = thermalConfig.begin();
				it_vec != thermalConfig.end(); it_vec++) {
			if (it_vec->type == TemperatureType::BCL_PERCENTAGE)
				bcl_defined = true;
		}

		thermalConfig.push_back(bat_conf);
		if (!bcl_defined)
			thermalConfig.insert(thermalConfig.end(),
				bcl_conf.begin(), bcl_conf.end());
		LOG(DEBUG) << "Total sensors:" << thermalConfig.size();
	}
}  // namespace implementation
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android
