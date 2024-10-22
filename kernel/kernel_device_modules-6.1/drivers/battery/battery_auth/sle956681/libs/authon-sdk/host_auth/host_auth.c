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
 * @file   host_auth.c
 * @date   January, 2021
 * @brief  Implementation of host authentication routine
 */

#include "../../authon-sdk/platform/helper.h"
#include "../authon_api.h"
#include "../authon_config.h"
#include "../authon_status.h"

extern AuthOn_Capability authon_capability;

/**
* @brief Send HRREQ and HRRES to device to get random number
* @param ub_nonceA Nounce A read from device
*/
uint16_t GetRandomNumber(uint8_t *ub_nonceA){
	uint16_t ret = APP_HA_INIT;
	unsigned long flags;

	if (ub_nonceA == NULL) {		
		return APP_HA_E_INPUT;
	}

	// Request for random number from Authenticate On
	local_irq_save(flags);
	preempt_disable();
	ret = Send_HRREQ();
	if (ret != INF_SWI_SUCCESS) {
		local_irq_restore(flags);
		preempt_enable();
		return APP_HA_E_HRREQ;
	}
	delay_ms(10);

	// Get random number
	ret = Send_HRRES(ub_nonceA);
	if (ret != INF_SWI_SUCCESS) {
		local_irq_restore(flags);
		preempt_enable();
		return APP_HA_E_HRRES;
	}
	local_irq_restore(flags);
	preempt_enable();
	delay_ms(5);
	return ret;

}