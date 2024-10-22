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
 * @file   ecc.c
 * @date   January, 2021
 * @brief  ECC implementation of ECC routine
 */
#include "ecc.h"

#include "../host_auth/host_auth.h"
#include "../platform/helper.h"
#include "../authon_api.h"
#include "../authon_status.h"
#include "../authon_config.h"

/**
* @brief Get ECC public key value including random padding
* @param key_number ECC key selection
* @param public_key ECC public key
*/
#define PK_PAGE_COUNT ((PUBLICKEY_BYTE_LEN + PAGE_SIZE_BYTE - 1 ) / PAGE_SIZE_BYTE)
uint16_t authon_get_ecc_publickey(uint8_t key_number, uint8_t *public_key)
{
	uint16_t nvm_addr;
	uint16_t ret = APP_ECC_INIT;
	uint8_t pubk[PK_PAGE_COUNT * PAGE_SIZE_BYTE];

	if((key_number > 1)|| (public_key==NULL)){
		return APP_ECC_E_INPUT;
	}
	// read Public key
	if(key_number == 0) {
		nvm_addr = ON_AUTH_PK1_ADDR;
	}else {
		nvm_addr = ON_AUTH_PK2_ADDR;
	}
		
	ret = authon_read_nvm(nvm_addr, pubk, PK_PAGE_COUNT);
	if(APP_NVM_SUCCESS!=ret){
		return APP_ECC_E_READ_PK;
	}

	//copy to output public key
	memcpy(public_key, (pubk+2), PUBLICKEY_BYTE_LEN);
	return APP_ECC_SUCCESS;
}

/**
* @brief Get ODC value including random padding
* @param key_number ECC key selection
* @param gf2n_ODC ODC to be returned
*/
#define ODC_PAGE_COUNT ((ODC_BYTE_LEN + PAGE_SIZE_BYTE - 1 ) / PAGE_SIZE_BYTE)
uint16_t authon_get_odc(uint8_t key_number, uint8_t *gf2n_ODC)
{
	uint16_t nvm_addr;
	uint16_t ret = APP_ECC_INIT;

	if((key_number > 1) || (gf2n_ODC==NULL)) {
		return APP_ECC_E_INPUT;
	}

	// read ODC
	if(key_number == 0) {
		nvm_addr = ON_ODC1_ADDR;
	}else{ 
		nvm_addr = ON_ODC2_ADDR;
	}

	ret = authon_read_nvm(nvm_addr, gf2n_ODC, ODC_PAGE_COUNT);
	if(APP_NVM_SUCCESS!=ret){
		return APP_ECC_E_READ_ODC;
	}

	return APP_ECC_SUCCESS;
}

/**
* @brief Send Challenge and get response on SWI bus
* @param gf2n_Challenge Challenge value
* @param ecc_Response Calculated ECC Response
* @param ecc_keySet selected ecc key 
* @param evt event handling using polling or interrupt
*/
uint16_t Send_SWI_ChallengeAndGetResponse( uint8_t *gf2n_Challenge, uint8_t *ecc_Response,  uint8_t ecc_key_set, EVENT_HANDLE event_handle)
{
	uint8_t ubData;
	uint8_t timeout_cnt;

	uint8_t bIntDetect = NO_INTERRUPT;
	uint16_t ret;
	unsigned long flags;

	if(gf2n_Challenge == NULL){
		return APP_ECC_E_INPUT;
	}

	if(ecc_key_set > 1){
		return APP_ECC_E_INPUT;
	}

	// if needed for interrupt mode
	// enable interrupt first
	if(event_handle==INTERRUPT){

		// enable Device interrupt enable
		ubData = 1 << BIT_DEV_INT_EN;
		ret = Intf_WriteRegister(ON_SFR_SWI_CONF_1, &ubData, 1);
		if(SDK_INTERFACE_SUCCESS!=ret){ 
			return ret;
		}

		// enable ECC_DONE_INT_EN
		ubData = 1 << BIT_ECC_DONE_INT_EN;
		ret = Intf_WriteRegister(ON_SFR_SWI_INT_EN, &ubData, 1);
		if(SDK_INTERFACE_SUCCESS!=ret){
			return ret;
		}

		// check ecc interrupt done bit is set and clear it
		// incase if interrupt bit status was set because MAC/ECC run before
		ret = Intf_ReadRegister(ON_SFR_SWI_INT_STS, &ubData, 1);
		if(SDK_INTERFACE_SUCCESS!=ret){
			return ret;
		}
		if(ubData != 0){
			// clear it
			ret = Intf_WriteRegister(ON_SFR_SWI_INT_STS, &ubData, 1);
			if(SDK_INTERFACE_SUCCESS!=ret){
				return ret;
			}
		}
	}

	// send ECCC command
	ret = Send_ECCC(gf2n_Challenge);
	if (INF_SWI_SUCCESS!= ret) {
		return ret;
	}

	// start ECC calculation
	if (ecc_key_set == 0) {
		ret = Send_ECCS1();	
		if (INF_SWI_SUCCESS!= ret) {
			return ret;
		}
	} else {
		ret = Send_ECCS2();		
		if (INF_SWI_SUCCESS!= ret) {
			return ret;
		}
	}

	switch(event_handle){
		case FIXED_WAIT:
		case POLLING:
			if(event_handle==FIXED_WAIT){
				delay_ms(ECC_FIXED_WAIT);
			}
		timeout_cnt = ECC_RETRY_COUNT;
		ubData = 0xff;
		do{
			ret = Intf_ReadRegister(ON_SFR_BUSY_STS, &ubData, 1);
			if (SDK_INTERFACE_SUCCESS != ret) {
				return ret;
			}

			if(timeout_cnt == 0){
				return APP_ECC_E_AUTHMAC_BUSY;
			}
			timeout_cnt-- ;

		}while(((ubData >> BIT_AUTH_MAC_BUSY) & 0x01) != 0);
		
		break;
		
		case INTERRUPT:

			delay_ms(INTERRUPT_START_DELAY);
			//Enable interrupt
			Send_EINT(&bIntDetect);

			if( bIntDetect == NO_INTERRUPT ){
				// check ecc interrupt done bit is set and clear it
				ret = Intf_ReadRegister(ON_SFR_SWI_INT_STS, &ubData, 1);
				if(SDK_INTERFACE_SUCCESS != ret){ 
					return ret;
				}
				return APP_ECC_AUTHMAC_EINT_NOINT;
			}

			// check ecc interrupt done bit is set and clear it
			ret = Intf_ReadRegister(ON_SFR_SWI_INT_STS, &ubData, 1);
			if(SDK_INTERFACE_SUCCESS != ret){ 
				return ret;
			}
			if(((ubData >> BIT_ECC_DONE_INT_STS) & 0x01) == 0x01){
				// clear it
				ret = Intf_WriteRegister(ON_SFR_SWI_INT_STS, &ubData, 1);
				if(SDK_INTERFACE_SUCCESS != ret){
					return ret;
				}
			}
			break;
		}

	//Get Response
	local_irq_save(flags);
	preempt_disable();
	
	ret = Send_ECCR(ecc_Response);
	if(INF_SWI_SUCCESS != ret){
		local_irq_restore(flags);
		preempt_enable();
		return ret;
	}

	local_irq_restore(flags);
	preempt_enable();

	return APP_ECC_SUCCESS;
}