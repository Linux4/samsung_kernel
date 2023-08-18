/*******************************************************************************
* Copyright (c) 2021, STMicroelectronics - All Rights Reserved
*
* This file is part of VL53L5 Kernel Driver and is dual licensed,
* either 'STMicroelectronics Proprietary license'
* or 'BSD 3-clause "New" or "Revised" License' , at your option.
*
********************************************************************************
*
* 'STMicroelectronics Proprietary license'
*
********************************************************************************
*
* License terms: STMicroelectronics Proprietary in accordance with licensing
* terms at www.st.com/sla0081
*
* STMicroelectronics confidential
* Reproduction and Communication of this document is strictly prohibited unless
* specifically authorized in writing by STMicroelectronics.
*
*
********************************************************************************
*
* Alternatively, VL53L5 Kernel Driver may be distributed under the terms of
* 'BSD 3-clause "New" or "Revised" License', in which case the following
* provisions apply instead of the ones mentioned above :
*
********************************************************************************
*
* License terms: BSD 3-clause "New" or "Revised" License.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
* may be used to endorse or promote products derived from this software
* without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*
*******************************************************************************/

#include "dci_structs.h"
#include "dci_ui_union_structs.h"
#include "dci_patch_union_structs.h"
#include "vl53l5_d16_defs.h"
#include "vl53l5_d16_luts.h"
#include "vl53l5_d16_size.h"
#include "vl53l5_depth16_packet_main.h"
#include "vl53l5_depth16_buffer_funcs.h"

uint32_t vl53l5_depth16_buffer_decode_init(
	struct  vl53l5_d16_cfg_dev_t          *pcfg_dev,
	struct  dci_d16_buf_dev_t             *pbuf_dev,
	struct  vl53l5_range_results_t                  *prng_dev,
	struct  vl53l5_d16_err_dev_t          *perr_dev)
{

	uint32_t last_packet  = 0;

	last_packet =
		vl53l5_depth16_packet_decode_init(
			&(pbuf_dev->d16_buf_data.data[0]),
			pcfg_dev,
			&(prng_dev->common_data),
			&(pbuf_dev->d16_pkt_meta),
			&(pbuf_dev->d16_blk_meta),
			perr_dev);

	pbuf_dev->d16_buf_meta.max_size =
		(uint32_t)pcfg_dev->d16_general_cfg.d16__max_buffer_size;
	pbuf_dev->d16_buf_meta.size =
		pbuf_dev->d16_pkt_meta.size * pbuf_dev->d16_pkt_meta.count;

	return last_packet;
}

uint32_t vl53l5_depth16_buffer_encode_init(
	struct  vl53l5_d16_cfg_dev_t          *pcfg_dev,
	struct  vl53l5_range_results_t                  *prng_dev,
	struct  dci_d16_buf_dev_t             *pbuf_dev,
	struct  vl53l5_d16_err_dev_t          *perr_dev)
{

	uint32_t last_packet  = 0;

	last_packet =
		vl53l5_depth16_packet_encode_init(
			pcfg_dev,
			&(prng_dev->common_data),
			&(pbuf_dev->d16_pkt_meta),
			&(pbuf_dev->d16_blk_meta),
			perr_dev);

	pbuf_dev->d16_buf_meta.max_size =
		(uint32_t)pcfg_dev->d16_general_cfg.d16__max_buffer_size;
	pbuf_dev->d16_buf_meta.size =
		pbuf_dev->d16_pkt_meta.size * pbuf_dev->d16_pkt_meta.count;

	return last_packet;
}
