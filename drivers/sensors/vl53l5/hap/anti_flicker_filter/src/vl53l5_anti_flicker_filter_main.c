/*******************************************************************************
* Copyright (c) 2020, STMicroelectronics - All Rights Reserved
*
* This file is part of VL53L5 Algo Pipe and is dual licensed,
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
* Alternatively, VL53L5 Algo Pipe may be distributed under the terms of
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

#include "vl53l5_anti_flicker_filter_funcs.h"
#include "dci_luts.h"
#include "vl53l5_aff_version_defs.h"
#include "vl53l5_aff_defs.h"
#include "vl53l5_aff_luts.h"
#include "vl53l5_anti_flicker_filter_main.h"

#if defined(AFF_DEBUG)
#include "aff_utils.h"
#endif

void vl53l5_aff_get_version(
	struct  common_grp__version_t  *pversion)
{

	pversion->version__major     = VL53L5_AFF_DEF__VERSION__MAJOR;
	pversion->version__minor     = VL53L5_AFF_DEF__VERSION__MINOR;
	pversion->version__build     = VL53L5_AFF_DEF__VERSION__BUILD;
	pversion->version__revision  = VL53L5_AFF_DEF__VERSION__REVISION;

}

void vl53l5_anti_flicker_filter_init_start_rng(
	struct vl53l5_aff_cfg_dev_t   *pcfg_dev,
	struct vl53l5_aff_int_dev_t   *pint_dev,
	struct vl53l5_aff_err_dev_t   *perr_dev)
{

	vl53l5_aff_clear_data(
		pint_dev,
		perr_dev);

	vl53l5_aff_init_mem(pint_dev, pcfg_dev, perr_dev);

}

void vl53l5_anti_flicker_filter_reset_zone(
	int32_t                       zone_arr_idx,
	struct vl53l5_aff_int_dev_t   *pint_dev,
	struct vl53l5_aff_err_dev_t   *perr_dev)
{

	int32_t *p;
	int32_t max_tgt                           =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_MAX_TGT_IDX];
	int32_t nb_past                           =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_NB_PAST_IDX];
	int32_t tgt_hist_size                     = (4 + nb_past * (int32_t)(sizeof(
				struct vl53l5_aff_tgt_data_t))) / 4;
	int32_t zone_size                         = max_tgt * tgt_hist_size;
	struct common_grp__status_t    *perror    = &(perr_dev->aff_error_status);

	if (zone_arr_idx >
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_NB_ZONE_IDX]) {
		perror->status__stage_id = VL53L5_AFF_STAGE_ID__RESET_ZONE;
		perror->status__type     = VL53L5_AFF_ERROR__BAD_ZONE_ARR_IDX;

	}

	else {
		p  = (int32_t *)(&((
							   pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_HISTORY_IDX]));
		p  = p + zone_arr_idx * zone_size;
		memset(
			p,
			0,
			(size_t)(zone_size) * 4);
	}

}

void vl53l5_anti_flicker_filter_main(
	struct vl53l5_range_results_t           *pdci_in,
	struct vl53l5_range_results_t           *pdci_out,
	struct vl53l5_aff_cfg_dev_t   *pcfg_dev,
	struct vl53l5_aff_int_dev_t   *pint_dev,
	struct vl53l5_aff_err_dev_t   *perr_dev)
{

	int32_t                        z;
	int32_t                        zdci;
	int32_t                        k;
	int32_t                        q;
	int32_t                        j;
	int32_t                        nb_cur_tgts;
	int32_t                        nb_hist_tgts;
	struct vl53l5_aff_tgt_data_t   *pcur;
	struct vl53l5_aff_tgt_out_t    *pout;
	int32_t                        *pcur_hist_asso;
	int32_t                        *phist_cur_asso;
	int32_t                        *pmat;
	int32_t                        nb_zones;
	int32_t                        max_tgt;
	struct vl53l5_aff_tgt_hist_t   *phist_tgts;
	int32_t                        not_match_curr_first_idx;
	uint8_t                        discard[DCI_MAX_TARGET_PER_ZONE];
	int32_t                        iir_enabled;
	struct vl53l5_aff_tgt_hist_t   *phist;
	struct common_grp__status_t    *perror    = &(perr_dev->aff_error_status);
	struct common_grp__status_t    *pwarning  = &(perr_dev->aff_warning_status);

	switch (pcfg_dev->aff_cfg.aff__mode) {
	case VL53L5_AFF_MODE__DISABLED:
		pwarning->status__stage_id  = VL53L5_AFF_STAGE_ID__MAIN;
		pwarning->status__type      = VL53L5_AFF_WARNING__ALGO_DISABLED;
		memcpy(pdci_out, pdci_in, sizeof(struct vl53l5_range_results_t));
		goto exit;
		break;
	case VL53L5_AFF_MODE__ENABLED:
		pwarning->status__stage_id  = VL53L5_AFF_STAGE_ID__MAIN;
		pwarning->status__type      = VL53L5_AFF_WARNING__NONE;
		break;
	default:
		perror->status__stage_id    = VL53L5_AFF_STAGE_ID__MAIN;
		perror->status__type        = VL53L5_AFF_ERROR__INVALID_PARAM;
		goto exit;
		break;
	}

	if (IS_BIT_SET_UINT8((pcfg_dev->aff_cfg).aff__avg_option, AFF__IIR_ENABLED))
		iir_enabled = 1;
	else
		iir_enabled = 0;

	if (iir_enabled == 0) {
		if (
			(IS_BIT_SET_UINT8((pcfg_dev->aff_cfg).aff__avg_option, AFF__MIRRORING_TYPE))
			|| (IS_BIT_SET_UINT8((pcfg_dev->aff_cfg).aff__avg_option,
								 AFF__IIR_AVG_ON_RNG_DATA))
			|| (IS_BIT_SET_UINT8((pcfg_dev->aff_cfg).aff__avg_option,
								 AFF__KEEP_PREV_OUT_ON_EMPTY_TGT_HIST))
			|| (IS_BIT_SET_UINT8((pcfg_dev->aff_cfg).aff__avg_option,
								 AFF__USE_PREV_OUT_IN_STATUS_DERIVATION))
		) {
#if defined(AFF_DEBUG)
			printf("Invalid parameters.\n");
#endif

			perror->status__stage_id = VL53L5_AFF_STAGE_ID__MAIN;
			perror->status__type     = VL53L5_AFF_ERROR__INVALID_PARAM;
			goto exit;
		}
	}

	max_tgt =
		(pint_dev->aff_persistent_container).aff__persistent_array[AFF_PM_MAX_TGT_IDX];
	nb_zones = (pcfg_dev->aff_cfg).aff__nb_zones_to_filter;
	pcur            = vl53l5_aff_cur_tgt_arr(pint_dev);
	phist_tgts      = vl53l5_aff_filled_tgt_hist_arr(pint_dev);
	pcur_hist_asso  = vl53l5_aff_cur_hist_asso(pint_dev);
	phist_cur_asso  = vl53l5_aff_hist_cur_asso(pint_dev);
	pout            = vl53l5_aff_out_cand_arr(pint_dev);
	memcpy(pdci_out, pdci_in, sizeof(struct vl53l5_range_results_t));

#if defined(AFF_DEBUG)
	printf("nb_zones: %d\n", nb_zones);
#endif

	for (z = 0; z < nb_zones; z = z + 1) {

		zdci = (int32_t)((pcfg_dev->aff_arrayed_cfg).aff__zone_sel[z]);

#if defined(AFF_DEBUG)
		printf("***************************************************************************\n");
		printf("            Processing PM zone index: %d, dci zone arr idx: %d\n", z,
			   zdci);
		printf("***************************************************************************\n");
#endif

		nb_cur_tgts   = vl53l5_aff_dci_to_cur_tgt(
							pdci_in,
							zdci,
							pint_dev,
							perr_dev
						);
#if defined(AFF_DEBUG)
		disp_cur(pcur, nb_cur_tgts, NULL);
#endif

		nb_hist_tgts = 0;
		for (k = 0; k < max_tgt; k++) {
			vl53l5_aff_get_tgt_hist(
				pint_dev,
				k,
				z,
				&(phist_tgts[nb_hist_tgts]),
				perr_dev
			);
			if (((*(phist_tgts[nb_hist_tgts].pfilled)) & 0x1) == 1)
				nb_hist_tgts = nb_hist_tgts + 1;
		}

		if (nb_hist_tgts < max_tgt) {
			q = nb_hist_tgts;
			for (k = 0; k < max_tgt; k++) {
				vl53l5_aff_get_tgt_hist(
					pint_dev,
					k,
					z,
					&(phist_tgts[q]),
					perr_dev
				);
				if (((*(phist_tgts[q].pfilled)) & 0x1) == 0)
					q = q + 1;
			}
		}
#if defined(AFF_DEBUG)
		disp_hist(phist_tgts, nb_hist_tgts, pint_dev, iir_enabled,
				  "Just after extraction.");
#endif

		vl53l5_aff_comp_aff_matrix(
			nb_cur_tgts,
			nb_hist_tgts,
			iir_enabled,
			pcfg_dev,
			pint_dev,
			perr_dev);
#if defined(AFF_DEBUG)
		disp_aff_mat(pint_dev, nb_cur_tgts, nb_hist_tgts, NULL);
#endif

		vl53l5_aff_match_targets(
			nb_cur_tgts,
			nb_hist_tgts,
			pcur_hist_asso,
			phist_cur_asso,
			pint_dev,
			perr_dev);
#if defined(AFF_DEBUG)
		disp_asso(pcur_hist_asso, phist_cur_asso, nb_cur_tgts, nb_hist_tgts, NULL);
		printf("-----------------------\n");
		printf("      Main loops:\n");
		printf("-----------------------\n");
#endif

		q              = 0;
		for (k = 0; k < nb_hist_tgts; k++) {
			if (pcur_hist_asso[k] == -1) {
				vl53l5_aff_avg_tgt_hist(
					NULL,
					&(phist_tgts[k]),
					pcfg_dev,
					pint_dev,
					&(pout[q]),
					perr_dev);
				pout[q].filled = 0;
#if defined(AFF_DEBUG)
				printf("    Case history target %d not matching any current target.\n", k);
				printf("        pout[%d]:\n", q);
				aff_utils_display_tgt_out(&(pout[q]));
#endif

				if ((iir_enabled == 1)
					&& (IS_BIT_SET_UINT8((pcfg_dev->aff_cfg).aff__avg_option,
										 AFF__TWEAK_MIRRORING))) {
					pout[q].target_zscore = vl53l5_aff_tweak_mirroring_calc(
												pout[q].target_zscore,
												&(phist_tgts[k]),
												pcfg_dev,
												pint_dev,
												perr_dev);
				}

				vl53l5_aff_temporal_shift(
					NULL,
					&(pout[q]),
					iir_enabled,
					&(phist_tgts[k]),
					pcfg_dev,
					pint_dev,
					perr_dev);

				q = q + 1;
			}
		}

		for (k = 0; k < nb_hist_tgts; k++) {
			if (pcur_hist_asso[k] != -1) {
				vl53l5_aff_avg_tgt_hist(
					&(pcur[pcur_hist_asso[k]]),
					&(phist_tgts[k]),
					pcfg_dev,
					pint_dev,
					&(pout[q]),
					perr_dev);
				pout[q].filled = 0;
#if defined(AFF_DEBUG)
				printf("    Case current target %d matching history target %d.\n",
					   pcur_hist_asso[k], k);
				printf("        pout[%d]:\n", q);
				aff_utils_display_tgt_out(&(pout[q]));
#endif
				vl53l5_aff_temporal_shift(
					&(pcur[pcur_hist_asso[k]]),
					&(pout[q]),
					iir_enabled,
					&(phist_tgts[k]),
					pcfg_dev,
					pint_dev,
					perr_dev);
				q = q + 1;
			}
		}

		not_match_curr_first_idx = q;
		memset(discard, 0, DCI_MAX_TARGET_PER_ZONE * sizeof(uint8_t));
		for (k = 0; k < nb_cur_tgts; k++) {

			if (phist_cur_asso[k] == -1) {

				if ((pcfg_dev->aff_cfg).aff__discard_close_tgt == 1) {
					pmat = vl53l5_aff_affinity_matrix(pint_dev);
					for (j = 0; j < nb_hist_tgts; j++) {
						if (pmat[k * nb_hist_tgts + j] < (pcfg_dev->aff_cfg).aff__discard_threshold) {
							discard[k] = 1;
#if defined(AFF_DEBUG)
							printf("    Case current target %d not matching any history target and to be discarded.\n",
								   k);
							printf("        pmat(%d, %d): %d\n", k, j, pmat[k * nb_hist_tgts + j]);
#endif
							break;
						}
					}
				}
				if (discard[k] == 0) {
					vl53l5_aff_avg_tgt_hist(
						&(pcur[k]),
						NULL,
						pcfg_dev,
						pint_dev,
						&(pout[q]),
						perr_dev);
					pout[q].filled = 0;

#if defined(AFF_DEBUG)
					printf("    Case current target %d not matching any history target.\n", k);
					printf("        pout[%d]:\n", q);
					aff_utils_display_tgt_out(&(pout[q]));
#endif
					q = q + 1;
				}
			}
		}

		q = not_match_curr_first_idx;
		for (k = 0; k < nb_cur_tgts; k++) {
			if ((phist_cur_asso[k] == -1)
				&& (discard[k] != 1)) {

				phist = vl53l5_aff_tgt_to_mem(
							&(pcur[k]),
							&(pout[q]),
							(pout[q]).target_zscore,
							iir_enabled,
							pint_dev,
							perr_dev);

				for (j = 0; j < q; j++) {
					if ((pout[j]).ptgt_hist == phist)
						pout[j].ptgt_hist = NULL;
				}
				q = q + 1;

			}
		}

#if defined(AFF_DEBUG)
		printf("-----------------------\n");
		printf("      Sorting out:\n");
		printf("-----------------------\n");
#endif

		if (perror->status__type == VL53L5_AFF_ERROR__NONE) {
			vl53l5_aff_cand_to_dci(
				zdci,
				q,
				pcfg_dev,
				pint_dev,
				pdci_out,
				perr_dev);
		}

	}

	vl53l5_aff_update_temp_idx(pint_dev, iir_enabled);

exit:

	return;

}
