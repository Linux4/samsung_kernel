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
 * @file   authon_api.c
 * @date   January, 2021
 * @brief  Implementation of Authenticate On SDK API
 */
#include "authon_api.h"
#include "../authon-sdk/interface/register.h"
#include "../authon-sdk/platform/bitbang.h"
#include "../authon-sdk/platform/helper.h"
#include "../authon-sdk/nvm/nvm.h"
#include "interface/swi_crc_bus_command.h"
#include "authon_config.h"
#include "authon_status.h"

AuthOn_Capability authon_capability;
AuthOn_Capability *authon_capability_ptr;
AuthOn_Enumeration authon_enumeration;

#define	DEFAULT_ECC_KEY_COUNT		(2)  //Every Authenticate On has 2 ECC key pairs
#define UID_SEARCH_RETRY            (3)  //Number of UID search retries

static uint16_t update_active_device_capabilities(void);

static void setup_device_attribute(AuthOn_Capability *sdk_config){

	authon_capability.uid_length = UID_BYTE_LEN;
	//Setup the NVM size	
	authon_capability.nvm_size = PAGE_SIZE_BYTE;

	//Setup the NVM page size
	authon_capability.nvm_page_count = ON_USR_NVM_PAGE_SIZE;
	authon_capability.num_ecc_keypair = DEFAULT_ECC_KEY_COUNT;

}

/**
* @brief Initialize the SDK
* @param sdk_config SDK configuration input
*/
uint16_t authon_init_sdk(AuthOn_Capability* sdk_config) {

	uint16_t ret = SDK_INIT;
	uint8_t uid[12];
	uint8_t tid[4];
	uint8_t pid[4];

	if(sdk_config==NULL){
		return SDK_E_INPUT;
	}

	setup_device_attribute(sdk_config);	
	authon_capability.swi.device_count=0;

	//Power cycle routine
	ret = authon_exe_power_cycle();
	if(ret!=EXE_SUCCESS){
		return ret;
	}


#if (ENABLE_SWI_SEARCH==1)
	uint8_t device_count=0;
	uint8_t retry = UID_SEARCH_RETRY;
	while(retry){

		ret = Swi_SearchUID96((uint8_t *)authon_enumeration.UID[0], &(authon_enumeration.device_found));
		if(ret==APP_SID_SUCCESS){
			break;
		}else if (APP_SID_SUCCESS!= ret || device_count==0){
			authon_exe_reset();
		}

		retry--;
		if(retry==0){
			//store info in the authon_capability
			authon_capability.swi.device_count=0;
			return APP_SID_E_NO_DEV;
		}
	}

	//UID reverse in necessary for ODC verification
	for(uint8_t i=0; i<authon_enumeration.device_found; i++){
		for(uint8_t j=0; j<12; j++){
			authon_capability.uid[i][11-j] = authon_enumeration.UID[i][j];
		}
	}

	//Only make the first device active. Device can only be selected from MSB order. So reverse the order again
	uint8_t first_active_uid[12]={0};
	for(uint8_t j=0; j<12; j++){
		first_active_uid[11-j] = authon_capability.uid[0][j];
	}

	ret = Swi_SelectByPuid((uint8_t *)first_active_uid);
	if(ret != APP_SID_SUCCESS){
		return APP_SID_E_SELECT;
	}

	//Get the address of the active device
	ret = authon_get_swi_address(authon_enumeration.device_address);
	if(ret != SDK_SUCCESS){
		return ret;
	}

	//report the number of device found
	authon_capability.swi.device_count = authon_enumeration.device_found;
	authon_capability.swi.UID_by_search = UID_BY_SEARCH;
#else

	//Select using default address and expects only a single device on the bus
	Send_EDA(0);
	Send_SDA(ON_DEFAULT_ADDRESS);
	authon_capability.swi.UID_by_search=UID_BY_REGISTER_READ;

	ret = authon_exe_read_uid(uid, tid, pid);
	if(ret!=EXE_SUCCESS){
		return ret;
	}

	memcpy(authon_enumeration.UID, uid, 12);

	authon_enumeration.device_found = 1;
	authon_capability.swi.device_count = authon_enumeration.device_found;

#endif

	//Initialize the hardware RNG, SHA and etc
	if(sdk_config->authon_alt_rng != NULL){
		authon_capability.authon_alt_rng = sdk_config->authon_alt_rng;
	}

	if(sdk_config->authon_alt_sha != NULL){
		authon_capability.authon_alt_sha = sdk_config->authon_alt_sha;
	}
	
	return EXE_SUCCESS;
}

/**
* @brief Return the number of device found on the bus
* @param device_found number of device found
*/
uint16_t authon_get_sdk_device_found(uint8_t * device_found)
{
	if(device_found== NULL){
		return SDK_E_INPUT;
	}
	*device_found = authon_enumeration.device_found;
	return SDK_SUCCESS;
}

/**
* @brief Return the current active device
* @param active_device active device
*/
uint16_t authon_get_sdk_active_device(uint8_t * active_device)
{
	if(active_device== NULL){
		return SDK_E_INPUT;
	}
	*active_device = authon_enumeration.active_device;
	return SDK_SUCCESS;
}

/**
* @brief Set and select the current active device.
* @param active_device active device
*/
uint16_t authon_set_sdk_active_device(uint8_t active_device)
{
	uint16_t ret;
	if(active_device > authon_enumeration.device_found){
		return SDK_E_INPUT;
	}

	authon_enumeration.active_device = active_device;

	ret = Swi_SelectByPuid((uint8_t *)authon_enumeration.UID[active_device]);
	if(ret != APP_SID_SUCCESS){
		return APP_SID_E_SELECT;
	}
	//populate the SDK with the active device
	ret = update_active_device_capabilities();
	if(ret != SDK_SUCCESS){
		return ret;
	}

	return SDK_SUCCESS;
}

/**
* @brief Set and select the current active device by UID.
* @param active_device_uid active device uid
* @param invert the UID is stored in reverse and need to be flipped
*/
uint16_t authon_set_sdk_active_device_uid(uint8_t * active_device_uid, uint8_t invert)
{
	uint16_t ret;
	uint8_t compare_uid[12];
	uint8_t device_found = authon_enumeration.device_found;
	uint8_t mismatch=1;
	uint8_t device_found_index=0;
	uint8_t i,j;

	
	if(invert==1){//flip the UID
		for(j=0; j<12; j++){
			compare_uid[11-j] = active_device_uid[j];
		}
	}else{
		for( j=0; j<12; j++){
			compare_uid[j] = active_device_uid[j];
		}
	}

	while(device_found){
		mismatch=0;
		for(i=0; i<12; i++){
			if(compare_uid[i] != authon_enumeration.UID[device_found-1][i]){
				mismatch=1;
				break;
			}
		}

		if(mismatch==0){
			break;
		}
		device_found--;
	}

	if(mismatch==0){
		device_found_index = device_found-1;
		ret = authon_set_sdk_active_device(device_found_index);
		if(ret != SDK_SUCCESS){
			return ret;
		}
		return SDK_SUCCESS;
	}else{
		return SDK_E_UID_NOT_ENUM;
	}	
}

#if (ENABLE_SWI_SEARCH==1)
/**
* @brief Search for all swi on the bus for UID
* @param uid address on the bus
* @param device_count number of devices found on the bus
* @param order either msb order or lsb order
*/
uint16_t authon_exe_search_swi_uid(uint8_t* uid, uint8_t* device_count, UID_ORDER order){
	uint16_t ret;
	//uint8_t device_found = 0;

	if((uid==NULL) || (device_count==NULL)){
		return EXE_E_INPUT;
	}
//Expects multiple device on the SWI bus	
#if(CONFIG_SWI_MAX_COUNT > 1)
	uint16_t devices_found = 0;
	ret = Swi_SearchUID96(uid, &devices_found);
	if(APP_SID_SUCCESS!= ret){
		return ret;
	}
#else	
	ret = Swi_SearchUID96(uid, NULL); //use a simpler search algorith if we only expect single device
	if(APP_SID_SUCCESS!= ret){
		return ret;
	}else{
		device_found=1;
	}
	if(order == LSB_FIRST){
		uint8_t UID96_reverse[UID_BYTE_LEN];
		for (uint8_t i = 0; i < UID_BYTE_LEN; i++) { /*!< reverse uid */
			UID96_reverse[UID_BYTE_LEN - i - 1] = uid[i];
		}
		memcpy(uid,UID96_reverse, UID_BYTE_LEN );
	}
#endif
	*device_count = devices_found;
	return EXE_SUCCESS;
}
#endif

/**
* @brief Get device address of the current active device
* @param device_address device_address
*/
uint16_t authon_get_swi_address(uint16_t *device_address){
	uint8_t ub_data;
	uint16_t ret = APP_NVM_INIT;
	uint16_t nw_rdDevAddr = 0;
	
	ret = Intf_ReadRegister(ON_SFR_SWI_DADR0_H, &ub_data, 1);
	if(ret != SDK_INTERFACE_SUCCESS){
		return ret;
	}
	nw_rdDevAddr = ub_data << 8;
	ret = Intf_ReadRegister(ON_SFR_SWI_DADR0_L, &ub_data, 1);
	if(ret != SDK_INTERFACE_SUCCESS){
		return ret;
	}
	nw_rdDevAddr |= ub_data;

   if (ret == SDK_INTERFACE_SUCCESS) {
	   *device_address = nw_rdDevAddr;
        return SDK_SUCCESS;
    } else {
        return APP_NVM_E_READ_DEVICEADDR;
    }
}
	
/**
* @brief Set device address of the current active device
* @param device_address device_address
*/
uint16_t authon_set_swi_address(uint16_t device_address){

	uint16_t ret;

	if(authon_capability.swi.device_address == device_address){
		return INF_SWI_SUCCESS;
	}
	ret= Send_WDA(device_address);

	if(INF_SWI_SUCCESS!= ret){
		return INF_SWI_E_SET_ADDRESS;
	}
	authon_capability.swi.device_address = device_address;

	return SDK_SUCCESS;
}

/**
* @brief Read SWI SFR directly
* @param sfr_address SFR address to be read
* @param sfr_value SFR value to be read 
* @param length Byte length to be read 
*/
uint16_t authon_read_swi_sfr(uint16_t sfr_address, uint8_t *sfr_value, uint16_t length){

#define SWIHiByte(in_16) ((in_16 >> 8) & 0xff)
#define SWILoByte(in_16) ((in_16) & 0xff)
	uint16_t ret;
	unsigned long flags;

	local_irq_save(flags);
	preempt_disable();

	/* send address to read from */
	Send_ERA(SWIHiByte(sfr_address) );
	ret = Send_RRA(SWILoByte(sfr_address), length, sfr_value);
	if(INF_SWI_SUCCESS!=ret){
		local_irq_restore(flags);
		preempt_enable();
		return ret;
	}
	
	local_irq_restore(flags);
	preempt_enable();

#undef SWIHiByte
#undef SWILoByte

	return ret;
}

/**
* @brief Write SWI SFR directly
* @param sfr_address SFR address to be written
* @param sfr_value SFR value to be written 
* @param length Byte length to be written 
*/
uint16_t authon_write_swi_sfr(uint16_t sfr_address, uint8_t *sfr_value, uint16_t length){
#define SWIHiByte(in_16) ((in_16 >> 8) & 0xff)
#define SWILoByte(in_16) ((in_16) & 0xff)

	/* send address to read from */
	Send_ERA(SWIHiByte(sfr_address) );
	Send_WRA(SWILoByte(sfr_address));
	Send_WD(length, sfr_value);
	
#undef SWIHiByte
#undef SWILoByte
	return INF_SWI_SUCCESS;
}

/**
* @brief Returns the active device UID
* @param uid UID value
* @param tid TID value
* @param pid PID value
*/
uint16_t authon_exe_read_uid(uint8_t* uid, uint8_t *tid, uint8_t *pid){
	uint16_t res;
	uint16_t UID_ADDRESS_OFFSET=ON_UID_ADDR+2;
	unsigned long flags;

	if((uid==NULL)||(tid==NULL)||(pid==NULL)){
		return EXE_E_INPUT;
	}

	memset(authon_capability.uid, 0, authon_capability.uid_length);

	local_irq_save(flags);
	preempt_disable();
	Send_RBL(3); /*Read 2^3 bytes*/

	/* send address to read from */
	Send_ERA(HiByte(ON_UID_ADDR));
	res = Send_RRA(LoByte(ON_UID_ADDR), 8, uid);
	if(res!=INF_SWI_SUCCESS) {
		local_irq_restore(flags);
		preempt_enable();
		return EXE_READUIDFAILED;
	}

	Send_RBL(2); /*Read 2^2 bytes*/

	/* send address to read from */
	Send_ERA(HiByte(UID_ADDRESS_OFFSET));
	res = Send_RRA(LoByte(UID_ADDRESS_OFFSET), 4, uid+8);
	if(res!=INF_SWI_SUCCESS) {
		local_irq_restore(flags);
		preempt_enable();
		return EXE_READUIDFAILED;
	}
	local_irq_restore(flags);
	preempt_enable();

	memcpy(authon_capability.uid, uid, authon_capability.uid_length);
	memcpy(tid, uid+10, 2);
	memcpy(pid, uid+8, 2);
	return EXE_SUCCESS;
}

/**
* @brief Returns the active device capability to external program
*/
uint16_t authon_get_device_capability(AuthOn_Capability* device_capability){
	memcpy(device_capability, &authon_capability, sizeof(authon_capability));
	return ON_SUCCESS;
}

/**
* @brief Soft reset of the active device
*/
uint16_t authon_exe_reset(void) {
	
	Swi_AbortIrq();
	Send_BRES();
	udelay(Get_ResetTime());
	return EXE_SUCCESS;
}

/**
* @brief Power cycle the active device
*/
uint16_t authon_exe_power_cycle(void) {

	authon_exe_power_down();
	authon_exe_power_up();
	return EXE_SUCCESS;
}

/**
* @brief Power down the active device
*/
uint16_t authon_exe_power_down(void) {

	Swi_PowerDown();
	udelay(Get_PowerdownTime());
	return EXE_SUCCESS;
}

/**
* @brief Power up the active device
*/
uint16_t authon_exe_power_up(void) {

	Swi_PowerUp();	
	udelay(Get_PowerupTime());
	return EXE_SUCCESS;
}

/**
* @brief Frees the SDK resource used by the active device
*/
uint16_t authon_deinit_sdk(void) {

	uint16_t ret;

	//Power down device first
	ret = authon_exe_power_down();
	if(ret!= EXE_SUCCESS){
		return ret;
	}

	authon_capability.swi.device_address=0;
	memset(authon_capability.uid, 0, authon_capability.uid_length);

	//Disable all alternate hardware references
	authon_capability.authon_alt_rng = NULL;
	authon_capability.authon_alt_sha = NULL;

	return SDK_SUCCESS;
}

/**
* @brief Update SDK active device capabilities
*/
static uint16_t update_active_device_capabilities(void){

	uint16_t ret;

	ret = authon_get_ifx_config(HOSTAUTH_CONFIG);
	if((ret != APP_HA_ENABLE) && (ret != APP_HA_DISABLE)){
		return ret;
	}else if (ret == APP_HA_ENABLE){
		authon_capability.device_attribute.host_authentication_support=supported;
	} else{
		authon_capability.device_attribute.host_authentication_support=unsupported;
	}
	
	if(authon_get_ifx_config(KILL_CONFIG)== APP_ECC_ENABLE_KILL){
		authon_capability.device_attribute.kill_ecc_support=supported;
	}else{
		authon_capability.device_attribute.kill_ecc_support=unsupported;
	}

	if(authon_get_ifx_config(AUTOKILL_CONFIG)== APP_ECC_ENABLE_AUTOKILL){
		authon_capability.device_attribute.auto_kill_ecc=supported;
	}else{
		authon_capability.device_attribute.auto_kill_ecc=unsupported;
	}

	if(authon_get_ifx_config(NVM_RESET_CONFIG)== APP_NVM_ENABLE_RESET){
		authon_capability.device_attribute.user_nvm_unlock=supported;
	} else{
		authon_capability.device_attribute.user_nvm_unlock=unsupported;
	}

	return SDK_SUCCESS;
}