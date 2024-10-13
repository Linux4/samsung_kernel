/*
 * Copyright (C) 2018-2019 Unisoc Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sipa_dele: " fmt

#include <linux/sipa.h>
#include <linux/sipc.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/mdm_ctrl.h>
#include <linux/soc/sprd/sprd_pcie_ep_device.h>
#include <linux/soc/sprd/sprd_mpm.h>
#include "sipa_dele_priv.h"
#include "../sipa/sipa_hal_priv.h"
#include "../pam_ipa/pam_ipa_core.h"

static struct miniap_delegator *s_miniap_delegator;

int notify_pam_ipa_miniap_ready(void)
{
	int ret;
	struct sipa_to_pam_info info;
	struct sipa_delegate_plat_drv_cfg *cfg;
	phys_addr_t reg_mapped;

	/* map mini ap addresses */
	cfg = s_miniap_delegator->delegator.cfg;
	ret = sprd_pms_request_resource(s_miniap_delegator->pms, -1);
	if (ret) {
		pr_err("sprd sipa pms request resource fail\n");
		return ret;
	}

	sprd_ep_ipa_map(PCIE_IPA_TYPE_MEM,
			cfg->mem_base,
			cfg->mem_end - cfg->mem_base);
	reg_mapped = sprd_ep_ipa_map(PCIE_IPA_TYPE_REG,
				     cfg->reg_base,
				     cfg->reg_end - cfg->reg_base);
	sprd_pms_release_resource(s_miniap_delegator->pms);
	/* notify pam_ipa */
	info.term = SIPA_TERM_PCIE0;
	info.dl_fifo.rx_fifo_base_addr = s_miniap_delegator->dl_free_fifo_phy;
	info.dl_fifo.tx_fifo_base_addr = s_miniap_delegator->dl_filled_fifo_phy;
	info.dl_fifo.fifo_sts_addr = reg_mapped + ((SIPA_FIFO_PCIE_DL + 1) *
						   SIPA_FIFO_REG_SIZE);

	info.ul_fifo.rx_fifo_base_addr = s_miniap_delegator->ul_filled_fifo_phy;
	info.ul_fifo.tx_fifo_base_addr = s_miniap_delegator->ul_free_fifo_phy;

	info.ul_fifo.fifo_sts_addr = reg_mapped + ((SIPA_FIFO_PCIE_UL + 1) *
						   SIPA_FIFO_REG_SIZE);

	pam_ipa_on_miniap_ready(&info);
	return 0;
}

void miniap_dele_on_event(void *priv, u16 flag, u32 data)
{
	struct sipa_delegator *delegator = &s_miniap_delegator->delegator;

	if (!delegator->is_powered) {
		pr_debug("sipa dele start power on\n");
		sipa_prepare_modem_power_on();
		delegator->stat = SIPA_DELE_RELEASED;
		delegator->is_powered = true;
	}

	switch (flag) {
	case SMSG_FLG_DELE_ADDR_DL_TX:
		s_miniap_delegator->dl_filled_fifo_phy =
			PAM_IPA_STI_64BIT(data,
					  PAM_IPA_DDR_MAP_OFFSET_H);
		break;
	case SMSG_FLG_DELE_ADDR_DL_RX:
		s_miniap_delegator->dl_free_fifo_phy =
			PAM_IPA_STI_64BIT(data,
					  PAM_IPA_DDR_MAP_OFFSET_H);
		break;
	case SMSG_FLG_DELE_ADDR_UL_TX:
		s_miniap_delegator->ul_free_fifo_phy =
			PAM_IPA_STI_64BIT(data,
					  PAM_IPA_DDR_MAP_OFFSET_H);
		break;
	case SMSG_FLG_DELE_ADDR_UL_RX:
		s_miniap_delegator->ul_filled_fifo_phy =
			PAM_IPA_STI_64BIT(data,
					  PAM_IPA_DDR_MAP_OFFSET_H);
		/* received last evt, notify pam_ipa mini_ap is ready */
		s_miniap_delegator->ready = true;
		notify_pam_ipa_miniap_ready();
		break;
	default:
		break;
	}
}

void miniap_dele_on_close(void *priv, u16 flag, u32 data)
{
	/* call base class on_close func first */
	sipa_dele_on_close(priv, flag, data);

	s_miniap_delegator->ready = false;
}

static void miniap_dele_power_off(void)
{
	struct sipa_delegator *dele = &s_miniap_delegator->delegator;

	dele->is_powered = false;
	dele->cons_ref_cnt = 0;
	dele->stat = SIPA_DELE_POWER_OFF;

	sipa_prepare_modem_power_off();
	sipa_rm_release_resource(dele->cons_user);
	queue_work(dele->smsg_wq, &dele->notify_work);
}

static int miniap_dele_restart_handle(struct notifier_block *this,
				      unsigned long mode, void *cmd)
{
	if (mode == MDM_CTRL_COLD_RESET || mode == MDM_POWER_OFF ||
	    mode == MDM_CTRL_WARM_RESET)
		miniap_dele_power_off();

	return NOTIFY_DONE;
}

static struct notifier_block miniap_dele_restart_handler = {
	.notifier_call = miniap_dele_restart_handle,
	.priority = 149,
};

int miniap_delegator_init(struct sipa_delegator_create_params *params)
{
	int ret;

	s_miniap_delegator = devm_kzalloc(params->pdev,
					  sizeof(*s_miniap_delegator),
					  GFP_KERNEL);
	if (!s_miniap_delegator)
		return -ENOMEM;

	strncpy(s_miniap_delegator->pms_name, "miniap_dele",
		sizeof(s_miniap_delegator->pms_name));
	s_miniap_delegator->pms =
		sprd_pms_create(params->dst,
				s_miniap_delegator->pms_name,
				false);
	if (!s_miniap_delegator->pms) {
		dev_err(params->pdev, "sipa create miniap dele pms fail\n");
		return -EPROBE_DEFER;
	}

	ret = sipa_delegator_init(&s_miniap_delegator->delegator,
				  params);
	if (ret) {
		sprd_pms_destroy(s_miniap_delegator->pms);
		return ret;
	}

	s_miniap_delegator->delegator.on_close = miniap_dele_on_close;
	s_miniap_delegator->delegator.on_evt = miniap_dele_on_event;

	s_miniap_delegator->ready = false;

	sipa_delegator_start(&s_miniap_delegator->delegator);
	modem_ctrl_register_notifier(&miniap_dele_restart_handler);

	return 0;
}
