/**
 * \copyright
 * MIT License
 *
 * Copyright (c) 2024 Infineon Technologies AG
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE
 *
 * \endcopyright
 */
/**
 * @file   authon_api.h
 * @date   January, 2021
 * @brief  Implementation of Authenticate On API
 */
#ifndef AUTHENTICATE_ON_API_H_
#define AUTHENTICATE_ON_API_H_

#pragma once


#include "authon_config.h"
#include "../authon-sdk/interface/register.h"
#include "interface/swi_crc/swi_crc.h"
#include "../authon-sdk/nvm/nvm.h"
#include "../authon-sdk/platform/bitbang.h"
#include "interface/swi_crc_bus_command.h"
#include "../authon-sdk/interface/interface.h"
#include "authon_config.h"

typedef enum CONFIG_TYPE_ {
	NVM_RESET_CONFIG,
	CHIPLOCK_CONFIG,
	HOSTAUTH_CONFIG,
	AUTOKILL_CONFIG,
	KILL_CONFIG
} CONFIG_TYPE;

typedef enum UID_ORDER_ {MSB_FIRST, LSB_FIRST} UID_ORDER;

/** @brief Type of event handling */
typedef enum EVENT_HANDLE_{
	POLLING,
	INTERRUPT,
	FIXED_WAIT
} EVENT_HANDLE;

/** @brief Device communication interface */
typedef enum INTERFACE_TYPE_{
	UNDEFINED_INTERFACE,
	SWI,
} INTERFACE_TYPE;

typedef enum UID_SEARCH_ {UID_BY_SEARCH, UID_BY_REGISTER_READ} UID_SEARCH_TYPE;

/** @brief SWI property */
typedef struct{
	uint16_t gpio;
	uint8_t device_address;
	uint8_t device_count;
	uint8_t UID_by_search;
}SWI_TYPE;

typedef enum FEATURE {unsupported, supported} feature;

/** @brief Optional configurable features */
struct DEVICE_ATTRIBUTE{
	feature host_authentication_support;
	feature kill_ecc_support;
	feature auto_kill_ecc;
	feature user_nvm_unlock;
};

/** @brief Alternate functions */
void authon_alt_rng(uint8_t* random_byte, uint8_t len);
void authon_alt_sha(uint8_t* sha_in, uint8_t *sha_out, uint16_t len);

/** @brief Device capabilities */
struct AuthOn_CAPABILITY{

	SWI_TYPE swi;

	//UID property
	uint8_t uid[CONFIG_SWI_MAX_COUNT][UID_BYTE_LEN];
	uint8_t uid_length;
	
	//NVM property
	uint16_t nvm_size;
	uint16_t nvm_page_count;

	//Crypto property
	uint16_t odc_bit_length;
	uint16_t ecc_bit_length;
	uint8_t num_lsc_counter;
	uint8_t num_ecc_keypair;

	struct DEVICE_ATTRIBUTE device_attribute;

	//alternate functions
	void (*authon_alt_rng)(uint8_t* random_byte, uint8_t len);
	void (*authon_alt_sha)(uint8_t* sha_in, uint8_t *sha_out, uint32_t len);
};

typedef struct AuthOn_CAPABILITY AuthOn_Capability;
//struct AuthOn_CAPABILITY *AuthOn_Capability_Ptr;

/** @brief support for multiple device on bus */
struct AuthOn_ENUMERATION{
	uint8_t UID[CONFIG_SWI_MAX_COUNT][UID_BYTE_LEN];
	uint16_t device_address[CONFIG_SWI_MAX_COUNT];
	uint16_t active_device;
	uint16_t device_found;
};
typedef struct AuthOn_ENUMERATION AuthOn_Enumeration;

/** SDK Configuration API */
uint16_t authon_get_sdk_version(uint16_t *version, uint32_t*date);
uint16_t authon_init_sdk(AuthOn_Capability *AuthOn_Capability_Handle);
uint16_t authon_get_active_interface(INTERFACE_TYPE* active_interface);
uint16_t authon_deinit_sdk(void);
uint16_t authon_get_sdk_device_found(uint8_t *device_found);
uint16_t authon_get_sdk_active_device(uint8_t *active_device);
uint16_t authon_set_sdk_active_device(uint8_t active_device);
uint16_t authon_set_sdk_active_device_uid(uint8_t * active_device_uid, uint8_t invert_uid);

/** SWI Interface API */
uint16_t authon_exe_search_swi_uid (uint8_t* uid, uint8_t* device_count, UID_ORDER order);
uint16_t authon_get_swi_address(uint16_t* device_address);
uint16_t authon_set_swi_address(uint16_t device_address);
uint16_t authon_read_swi_sfr(uint16_t sfr_address, uint8_t *sfr_value, uint16_t length);
uint16_t authon_write_swi_sfr(uint16_t sfr_address, uint8_t *sfr_value, uint16_t length);

/** Device Characteristics API*/
uint16_t authon_exe_read_uid(uint8_t* uid, uint8_t *vid, uint8_t *pid);
uint16_t authon_get_device_capability (AuthOn_Capability* device_capability);
uint16_t authon_exe_reset(void);
uint16_t authon_exe_power_cycle(void);
uint16_t authon_exe_power_down(void);
uint16_t authon_exe_power_up(void);

/** ECC API */
uint16_t authon_get_ecc_publickey(uint8_t key_number, uint8_t *public_key);
uint16_t authon_get_odc(uint8_t key_number, uint8_t* odc_value);

/** NVM API */
uint16_t authon_read_nvm(uint16_t nvm_start_page, uint8_t *ubp_data, uint8_t page_count);
uint16_t authon_write_nvm(uint16_t nvm_start_page, uint8_t *ubp_Data, uint8_t page_count);
uint16_t authon_set_nvm_page_lock(uint8_t page);
uint16_t authon_get_nvm_lock_status(uint8_t page);
uint16_t authon_get_ifx_config(CONFIG_TYPE config_type);
uint16_t authon_unlock_nvm_locked(uint8_t *mac_byte);

/** LSC API */ 
uint16_t authon_set_lsc_value(uint8_t lsc_select, uint32_t lsc_value);
uint16_t authon_get_lsc_value(uint8_t lsc_select, uint32_t * lsc_value);
uint16_t authon_get_lsc_decvalue(uint16_t *uw_lsc_DecVal );
uint16_t authon_dec_lsc_value(uint8_t lsc_select);
uint16_t authon_set_lsc_protection(uint8_t lsc_number);
uint16_t authon_get_lsc_lock_status(uint8_t lsc_number);

#endif /* AUTHENTICATE_ON_API_H_ */
