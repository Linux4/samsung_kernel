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
 * @file   interface.c
 * @date   January, 2021
 * @brief  Implementation of communication interface
 */

#include "../authon_api.h"
#include "../authon_config.h"
#include "../authon_status.h"
extern AuthOn_Capability authon_capability;

/**
* @brief Write data to register. Some registers can only be accessed after host authentication. 
* Certain register write will requires NVM write and will requires NVM programming delay.
* @param uw_Address address of the device
* @param ubp_Data Pointer to data buffer
* @param ub_wr_len Length of data to be write into the register
*/
uint16_t Intf_WriteRegister(uint16_t uw_Address, uint8_t * ubp_Data, uint8_t ub_wr_len )
{
	unsigned long flags;
	if(ubp_Data == NULL || ub_wr_len == 0){
		return SDK_E_INPUT;
	}

	local_irq_save(flags);
	preempt_disable();
	Send_ERA(HiByte(uw_Address));
	Send_WRA(LoByte(uw_Address));
	Send_WD(ub_wr_len, ubp_Data);

	local_irq_restore(flags);
	preempt_enable();
	return SDK_INTERFACE_SUCCESS;
}

/**
* @brief Read_Register. Some registers can only be accessed after host authentication.
* @param uw_Address Address to be read
* @param ubp_Data Pointer to return data buffer
* @param ub_rd_len Length of data to be read from the register
*/
uint16_t Intf_ReadRegister( uint16_t uw_Address, uint8_t *ubp_Data, uint8_t ub_rd_len)
{
	uint16_t rd_len;
	uint16_t ret=SDK_INIT;
	unsigned long flags;
		
	rd_len = ub_rd_len;
	if(ubp_Data == NULL) {
		return SDK_E_INPUT;
	}

	// for NVM READ
	if(uw_Address < 0x4000){
		if(rd_len==0x00){
			ret = Intf_WriteRegister(ON_SFR_SWI_RD_LEN, (uint8_t *)&rd_len, 1 );
			rd_len=256;
		}else{
			//set length to read
			ret = Intf_WriteRegister(ON_SFR_SWI_RD_LEN, (uint8_t *)&rd_len, 1 );
		}
		if(SDK_INTERFACE_SUCCESS!=ret) {
			return ret;
		}
	}else{
		rd_len = 1;
	}

	local_irq_save(flags);
	preempt_disable();

	/* send address to read from */
	Send_ERA(HiByte(uw_Address) );
	ret = Send_RRA(LoByte(uw_Address), rd_len, ubp_Data);
	if(INF_SWI_SUCCESS!=ret){
		local_irq_restore(flags);
		preempt_enable();
		return ret;
	}

	local_irq_restore(flags);
	preempt_enable();

	return SDK_INTERFACE_SUCCESS;
}