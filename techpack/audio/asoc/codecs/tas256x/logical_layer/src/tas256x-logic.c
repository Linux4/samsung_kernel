#include "logical_layer/inc/tas256x-logic.h"
#include "physical_layer/inc/tas256x-device.h"
#if IS_ENABLED(CONFIG_TAS25XX_ALGO)
#include "algo/inc/tas_smart_amp_v2.h"
#include "algo/inc/tas25xx-calib.h"
#include "os_layer/inc/tas256x-regmap.h"
#endif /*CONFIG_TAS25XX_ALGO*/

static int tas256x_change_book_page(struct tas256x_priv *p_tas256x,
	enum channel chn,
	int book, int page)
{
	int n_result = 0, rc = 0;
	int i = 0;

	for (i = 0; i < p_tas256x->mn_channels; i++) {
		if (((chn&channel_left) && (i == 0))
			|| ((chn&channel_right) && (i == 1))) {
			if (p_tas256x->devs[i]->mn_current_book != book) {
				n_result = p_tas256x->plat_write(
						p_tas256x->platform_data,
						p_tas256x->devs[i]->mn_addr,
						TAS256X_BOOKCTL_PAGE, 0);
				if (n_result < 0) {
					pr_err(
						"%s, ERROR, L=%d, E=%d\n",
						__func__, __LINE__, n_result);
					rc |= n_result;
					continue;
				}
				p_tas256x->devs[i]->mn_current_page = 0;
				n_result = p_tas256x->plat_write(
						p_tas256x->platform_data,
						p_tas256x->devs[i]->mn_addr,
						TAS256X_BOOKCTL_REG, book);
				if (n_result < 0) {
					pr_err(
						"%s, ERROR, L=%d, E=%d\n",
						__func__, __LINE__, n_result);
					rc |= n_result;
					continue;
				}
				p_tas256x->devs[i]->mn_current_book = book;
			}

			if (p_tas256x->devs[i]->mn_current_page != page) {
				n_result = p_tas256x->plat_write(
						p_tas256x->platform_data,
						p_tas256x->devs[i]->mn_addr,
						TAS256X_BOOKCTL_PAGE, page);
				if (n_result < 0) {
					pr_err(
						"%s, ERROR, L=%d, E=%d\n",
						__func__, __LINE__, n_result);
					rc |= n_result;
					continue;
				}
				p_tas256x->devs[i]->mn_current_page = page;
			}
		}
	}

	if (rc < 0) {
		if (chn&channel_left)
			p_tas256x->mn_err_code |= ERROR_DEVA_I2C_COMM;
		if (chn&channel_right)
			p_tas256x->mn_err_code |= ERROR_DEVB_I2C_COMM;
	} else {
		if (chn&channel_left)
			p_tas256x->mn_err_code &= ~ERROR_DEVA_I2C_COMM;
		if (chn&channel_right)
			p_tas256x->mn_err_code &= ~ERROR_DEVB_I2C_COMM;
	}
	return rc;
}

static int tas256x_dev_read(struct tas256x_priv *p_tas256x,
	enum channel chn,
	unsigned int reg, unsigned int *pValue)
{
	int n_result = 0;
	int i = 0, chnTemp = 0;

	mutex_lock(&p_tas256x->dev_lock);

	if (chn == channel_both) {
		for (i = 0; i < p_tas256x->mn_channels; i++) {
			if (p_tas256x->devs[i]->spk_control == 1)
				chnTemp |= 1<<i;
		}
		chn = (chnTemp == 0)?chn:(enum channel)chnTemp;
	}

	n_result = tas256x_change_book_page(p_tas256x, chn,
		TAS256X_BOOK_ID(reg), TAS256X_PAGE_ID(reg));
	if (n_result < 0)
		goto end;

	/*Force left incase of mono*/
	if ((chn == channel_right) && (p_tas256x->mn_channels == 1))
		chn = channel_left;

	n_result = p_tas256x->plat_read(p_tas256x->platform_data,
			p_tas256x->devs[chn>>1]->mn_addr,
			TAS256X_PAGE_REG(reg), pValue);
	if (n_result < 0) {
		pr_err("%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, n_result);
		if (chn&channel_left)
			p_tas256x->mn_err_code |= ERROR_DEVA_I2C_COMM;
		if (chn&channel_right)
			p_tas256x->mn_err_code |= ERROR_DEVB_I2C_COMM;
	} else {
		pr_debug(
			"%s: chn:%x:BOOK:PAGE:REG 0x%02x:0x%02x:0x%02x,0x%02x\n",
			__func__,
			p_tas256x->devs[chn>>1]->mn_addr, TAS256X_BOOK_ID(reg),
			TAS256X_PAGE_ID(reg),
			TAS256X_PAGE_REG(reg), *pValue);
		if (chn&channel_left)
			p_tas256x->mn_err_code &= ~ERROR_DEVA_I2C_COMM;
		if (chn&channel_right)
			p_tas256x->mn_err_code &= ~ERROR_DEVB_I2C_COMM;
	}
end:
	mutex_unlock(&p_tas256x->dev_lock);
	return n_result;
}

static int tas256x_dev_write(struct tas256x_priv *p_tas256x, enum channel chn,
	unsigned int reg, unsigned int value)
{
	int n_result = 0, rc = 0;
	int i = 0, chnTemp = 0;

	mutex_lock(&p_tas256x->dev_lock);

	if (chn == channel_both) {
		for (i = 0; i < p_tas256x->mn_channels; i++) {
			if (p_tas256x->devs[i]->spk_control == 1)
				chnTemp |= 1<<i;
		}
		chn = (chnTemp == 0)?chn:(enum channel)chnTemp;
	}

	n_result = tas256x_change_book_page(p_tas256x, chn,
		TAS256X_BOOK_ID(reg), TAS256X_PAGE_ID(reg));
	if (n_result < 0)
		goto end;

	for (i = 0; i < p_tas256x->mn_channels; i++) {
		if (((chn&channel_left) && (i == 0))
			|| ((chn&channel_right) && (i == 1))) {
			n_result = p_tas256x->plat_write(
					p_tas256x->platform_data,
					p_tas256x->devs[i]->mn_addr,
					TAS256X_PAGE_REG(reg), value);
			if (n_result < 0) {
				pr_err(
					"%s, ERROR, L=%u, chn=0x%02x, E=%d\n",
					__func__, __LINE__,
					p_tas256x->devs[i]->mn_addr, n_result);
				rc |= n_result;
				if (chn&channel_left)
					p_tas256x->mn_err_code |= ERROR_DEVA_I2C_COMM;
				if (chn&channel_right)
					p_tas256x->mn_err_code |= ERROR_DEVB_I2C_COMM;
			} else {
				pr_debug(
					"%s: %u: chn:0x%02x:BOOK:PAGE:REG 0x%02x:0x%02x:0x%02x, VAL: 0x%02x\n",
					__func__, __LINE__,
					p_tas256x->devs[i]->mn_addr,
					TAS256X_BOOK_ID(reg),
					TAS256X_PAGE_ID(reg),
					TAS256X_PAGE_REG(reg), value);
				if (chn&channel_left)
					p_tas256x->mn_err_code &= ~ERROR_DEVA_I2C_COMM;
				if (chn&channel_right)
					p_tas256x->mn_err_code &= ~ERROR_DEVB_I2C_COMM;
			}
		}
	}
end:
	mutex_unlock(&p_tas256x->dev_lock);
	return rc;
}

static int tas256x_dev_bulk_write(struct tas256x_priv *p_tas256x,
	enum channel chn,
	unsigned int reg, unsigned char *p_data, unsigned int n_length)
{
	int n_result = 0, rc = 0;
	int i = 0, chnTemp = 0;

	mutex_lock(&p_tas256x->dev_lock);

	if (chn == channel_both) {
		for (i = 0; i < p_tas256x->mn_channels; i++) {
			if (p_tas256x->devs[i]->spk_control == 1)
				chnTemp |= 1<<i;
		}
		chn = (chnTemp == 0)?chn:(enum channel)chnTemp;
	}

	n_result = tas256x_change_book_page(p_tas256x, chn,
		TAS256X_BOOK_ID(reg), TAS256X_PAGE_ID(reg));
	if (n_result < 0)
		goto end;

	for (i = 0; i < p_tas256x->mn_channels; i++) {
		if (((chn&channel_left) && (i == 0))
			|| ((chn&channel_right) && (i == 1))) {
			n_result = p_tas256x->plat_bulk_write(
					p_tas256x->platform_data,
					p_tas256x->devs[i]->mn_addr,
					TAS256X_PAGE_REG(reg),
					p_data, n_length);
			if (n_result < 0) {
				pr_err(
					"%s, ERROR, L=%u, chn=0x%02x: E=%d\n",
					__func__, __LINE__,
					p_tas256x->devs[i]->mn_addr, n_result);
				rc |= n_result;
				if (chn&channel_left)
					p_tas256x->mn_err_code |= ERROR_DEVA_I2C_COMM;
				if (chn&channel_right)
					p_tas256x->mn_err_code |= ERROR_DEVB_I2C_COMM;
			} else {
				pr_debug(
					"%s: chn%x:BOOK:PAGE:REG 0x%02x:0x%02x:0x%02x, len: %u\n",
					__func__, p_tas256x->devs[i]->mn_addr,
					TAS256X_BOOK_ID(reg),
					TAS256X_PAGE_ID(reg),
					TAS256X_PAGE_REG(reg), n_length);
				if (chn&channel_left)
					p_tas256x->mn_err_code &= ~ERROR_DEVA_I2C_COMM;
				if (chn&channel_right)
					p_tas256x->mn_err_code &= ~ERROR_DEVB_I2C_COMM;
			}
		}
	}

end:
	mutex_unlock(&p_tas256x->dev_lock);
	return rc;
}

static int tas256x_dev_bulk_read(struct tas256x_priv *p_tas256x,
	enum channel chn,
	unsigned int reg, unsigned char *p_data, unsigned int n_length)
{
	int n_result = 0;
	int i = 0, chnTemp = 0;

	mutex_lock(&p_tas256x->dev_lock);

	if (chn == channel_both) {
		for (i = 0; i < p_tas256x->mn_channels; i++) {
			if (p_tas256x->devs[i]->spk_control == 1)
				chnTemp |= 1<<i;
		}
		chn = (chnTemp == 0)?chn:(enum channel)chnTemp;
	}

	n_result = tas256x_change_book_page(p_tas256x, chn,
		TAS256X_BOOK_ID(reg), TAS256X_PAGE_ID(reg));
	if (n_result < 0)
		goto end;

	n_result = p_tas256x->plat_bulk_read(p_tas256x->platform_data,
		p_tas256x->devs[chn>>1]->mn_addr, TAS256X_PAGE_REG(reg),
		p_data, n_length);
	if (n_result < 0) {
		pr_err("%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, n_result);
		if (chn&channel_left)
			p_tas256x->mn_err_code |= ERROR_DEVA_I2C_COMM;
		if (chn&channel_right)
			p_tas256x->mn_err_code |= ERROR_DEVB_I2C_COMM;
	} else {
		pr_debug(
			"%s: chn%x:BOOK:PAGE:REG %u:%u:%u, len: 0x%02x\n",
			__func__, p_tas256x->devs[chn>>1]->mn_addr,
			TAS256X_BOOK_ID(reg), TAS256X_PAGE_ID(reg),
			TAS256X_PAGE_REG(reg), n_length);
		if (chn&channel_left)
			p_tas256x->mn_err_code &= ~ERROR_DEVA_I2C_COMM;
		if (chn&channel_right)
			p_tas256x->mn_err_code &= ~ERROR_DEVB_I2C_COMM;
	}
end:
	mutex_unlock(&p_tas256x->dev_lock);
	return n_result;
}

static int tas256x_dev_update_bits(struct tas256x_priv *p_tas256x,
	enum channel chn,
	unsigned int reg, unsigned int mask, unsigned int value)
{
	int n_result = 0, rc = 0;
	int i = 0, chnTemp = 0;

	mutex_lock(&p_tas256x->dev_lock);

	if (chn == channel_both) {
		for (i = 0; i < p_tas256x->mn_channels; i++) {
			if (p_tas256x->devs[i]->spk_control == 1)
				chnTemp |= 1<<i;
		}
		chn = (chnTemp == 0)?chn:(enum channel)chnTemp;
	}

	n_result = tas256x_change_book_page(p_tas256x, chn,
		TAS256X_BOOK_ID(reg), TAS256X_PAGE_ID(reg));
	if (n_result < 0) {
		rc = n_result;
		goto end;
	}

	for (i = 0; i < p_tas256x->mn_channels; i++) {
		if (((chn&channel_left) && (i == 0))
			|| ((chn&channel_right) && (i == 1))) {
			n_result = p_tas256x->plat_update_bits(
						p_tas256x->platform_data,
						p_tas256x->devs[i]->mn_addr,
						TAS256X_PAGE_REG(reg),
						mask, value);
			if (n_result < 0) {
				pr_err(
					"%s, ERROR, L=%u, chn=0x%02x: E=%d\n",
					__func__, __LINE__,
					p_tas256x->devs[i]->mn_addr, n_result);
				rc |= n_result;
				p_tas256x->mn_err_code |=
					(chn == channel_left) ? ERROR_DEVA_I2C_COMM : ERROR_DEVB_I2C_COMM;
			} else {
				pr_debug(
					"%s: chn%x:BOOK:PAGE:REG 0x%02x:0x%02x:0x%02x, mask: 0x%02x, val: 0x%02x\n",
					__func__, p_tas256x->devs[i]->mn_addr,
					TAS256X_BOOK_ID(reg),
					TAS256X_PAGE_ID(reg),
					TAS256X_PAGE_REG(reg), mask, value);
				p_tas256x->mn_err_code &=
					(chn == channel_left) ? ~ERROR_DEVA_I2C_COMM : ~ERROR_DEVB_I2C_COMM;
			}
		}
	}

end:
	mutex_unlock(&p_tas256x->dev_lock);
	return rc;
}

static void tas256x_hard_reset(struct tas256x_priv  *p_tas256x)
{
	int i = 0;

	p_tas256x->hw_reset(p_tas256x);

	for (i = 0; i < p_tas256x->mn_channels; i++) {
		p_tas256x->devs[i]->mn_current_book = -1;
		p_tas256x->devs[i]->mn_current_page = -1;
	}

	if (p_tas256x->mn_err_code)
		pr_info("%s: before reset, ErrCode=0x%x\n", __func__,
			p_tas256x->mn_err_code);
	p_tas256x->mn_err_code = 0;
}

void tas256x_failsafe(struct tas256x_priv  *p_tas256x)
{
	int n_result;

	pr_info("tas256x %s\n", __func__);
	p_tas256x->mn_err_code |= ERROR_FAILSAFE;

	if (p_tas256x->mn_restart < RESTART_MAX) {
		p_tas256x->mn_restart++;
		msleep(100);
		pr_err("I2C COMM error, restart SmartAmp.\n");
		tas256x_set_power_state(p_tas256x, p_tas256x->mn_power_state);
		return;
	}

	n_result = tas256x_set_power_shutdown(p_tas256x, channel_both);
	p_tas256x->mb_power_up = false;
	p_tas256x->mn_power_state = TAS256X_POWER_SHUTDOWN;
	msleep(20);
	/*Mask interrupt for TDM*/
	n_result = tas256x_interrupt_enable(p_tas256x, 0/*Disable*/,
		channel_both);
	p_tas256x->enable_irq(p_tas256x, false);
	tas256x_hard_reset(p_tas256x);
	p_tas256x->write(p_tas256x, channel_both, TAS256X_SOFTWARERESET,
		TAS256X_SOFTWARERESET_SOFTWARERESET_RESET);
	udelay(1000);
	/*pTAS256x->write(pTAS256x, channel_both, TAS256X_SPK_CTRL_REG, 0x04);*/
}

int tas256x_load_i2s_tdm_interface_settings(struct tas256x_priv *p_tas256x,
	int ch)
{
	int n_result = 0;

	/*Frame_Start Settings*/
	n_result |= tas256x_rx_set_frame_start(p_tas256x,
		p_tas256x->mn_frame_start, ch);
	/*RX Edge Settings*/
	n_result |= tas256x_rx_set_edge(p_tas256x,
		p_tas256x->mn_rx_edge, ch);
	/*RX Offset Settings*/
	n_result |= tas256x_rx_set_start_slot(p_tas256x,
		p_tas256x->mn_rx_offset, ch);
	/*TX Edge Settings*/
	n_result |= tas256x_tx_set_edge(p_tas256x,
		p_tas256x->mn_tx_edge, ch);
	/*TX Offset Settings*/
	n_result |= tas256x_tx_set_start_slot(p_tas256x,
		p_tas256x->mn_tx_offset, ch);

	return n_result;
}

int tas256x_load_init(struct tas256x_priv *p_tas256x)
{
	int ret = 0, i;

	pr_info("%s:\n", __func__);

	for (i = 0; i < p_tas256x->mn_channels; i++) {
		if (p_tas256x->devs[i]->dev_ops.tas_init)
			ret |= (p_tas256x->devs[i]->dev_ops.tas_init)(p_tas256x,
				i+1);
	}

	ret |= tas256x_set_misc_config(p_tas256x, 0/*Ignored*/, channel_both);
	if (ret < 0)
		goto end;
	ret |= tas256x_set_tx_config(p_tas256x, 0/*Ignored*/, channel_both);
	if (ret < 0)
		goto end;
	ret |= tas256x_load_i2s_tdm_interface_settings(p_tas256x, channel_both);
	if (ret < 0)
		goto end;
	ret |= tas256x_set_clock_config(p_tas256x, 0/*Ignored*/, channel_both);
	if (ret < 0)
		goto end;

	/*ICN Improve Performance*/
	ret |= tas256x_icn_config(p_tas256x, 0/*Ignored*/, channel_both);
	if (ret < 0)
		goto end;

	/*Zero delay between two SAR consecutive converts and one channel */
	ret |= tas256x_zero_sar_delay(p_tas256x, channel_both);
	if (ret < 0)
		goto end;

	/* Boost Always on for 2.95 V */
	ret |= tas256x_update_boost_always_on(p_tas256x, 2, channel_both);
	if (ret < 0)
		goto end;

		/* Boost Always Off for 2.7 V */
	ret |= tas256x_update_boost_always_off(p_tas256x, 2, channel_both);
	if (ret < 0)
		goto end;

	/*Update Dmin to better noise floor in all modes*/
	/*TODO: Uncomment when its verified
	 *ret |= tas256x_update_dmin(p_tas256x, channel_both);
	 *if (ret < 0)
	 *	goto end;
	 */

#if IS_ENABLED(CONFIG_HPF_BYPASS)
	/*Disable the HPF in Forward Path*/
	ret |= tas256x_HPF_FF_Bypass(p_tas256x, 0/*Ignored*/, channel_both);
	if (ret < 0)
		goto end;
	/*Disable the HPF in Reverse Path*/
	ret |= tas256x_HPF_FB_Bypass(p_tas256x, 0/*Ignored*/, channel_both);
	if (ret < 0)
		goto end;
#endif
	ret |= tas256x_set_classH_config(p_tas256x, 0/*Ignored*/, channel_both);
	if (ret < 0)
		goto end;

	/* Enable Receiver mode settings */
	if (p_tas256x->rcv_only) {
		for (i = 0; i < p_tas256x->mn_channels; i++) {
			ret |= tas256x_enable_receiver_mode(p_tas256x,
				p_tas256x->devs[i]->receiver_enable, i+1);
			if (ret < 0)
				goto end;
		}
	}

end:
	if (ret < 0) {
		if (p_tas256x->mn_err_code &
			(ERROR_DEVA_I2C_COMM | ERROR_DEVB_I2C_COMM))
			tas256x_failsafe(p_tas256x);
	}
	return ret;
}

int tas256x_load_ctrl_values(struct tas256x_priv *p_tas256x, int ch)
{
	int n_result = 0;

	n_result |= tas256x_update_playback_volume(p_tas256x,
			p_tas256x->devs[ch-1]->dvc_pcm, ch);

	n_result |= tas256x_update_lim_max_attenuation(p_tas256x,
			p_tas256x->devs[ch-1]->lim_max_attn, ch);

	n_result |= tas256x_update_lim_max_thr(p_tas256x,
			p_tas256x->devs[ch-1]->lim_thr_max, ch);

	n_result |= tas256x_update_lim_min_thr(p_tas256x,
			p_tas256x->devs[ch-1]->lim_thr_min, ch);

	n_result |= tas256x_update_lim_inflection_point(p_tas256x,
			p_tas256x->devs[ch-1]->lim_infl_pt, ch);

	n_result |= tas256x_update_lim_slope(p_tas256x,
			p_tas256x->devs[ch-1]->lim_trk_slp, ch);

	n_result |= tas256x_update_bop_thr(p_tas256x,
			p_tas256x->devs[ch-1]->bop_thd, ch);

	n_result |= tas256x_update_bosd_thr(p_tas256x,
			p_tas256x->devs[ch-1]->bosd_thd, ch);

	n_result |= tas256x_update_boost_voltage(p_tas256x,
			p_tas256x->devs[ch-1]->bst_vltg, ch);

	n_result |= tas256x_update_current_limit(p_tas256x,
			p_tas256x->devs[ch-1]->bst_ilm, ch);

	n_result |= tas256x_update_ampoutput_level(p_tas256x,
			p_tas256x->devs[ch-1]->ampoutput_lvl, ch);

	n_result |= tas256x_update_limiter_enable(p_tas256x,
			p_tas256x->devs[ch-1]->lim_switch, ch);

	n_result |= tas256x_update_limiter_attack_rate(p_tas256x,
			p_tas256x->devs[ch-1]->lim_att_rate, ch);

	n_result |= tas256x_update_limiter_attack_step_size(p_tas256x,
			p_tas256x->devs[ch-1]->lim_att_stp_size, ch);

	n_result |= tas256x_update_limiter_release_rate(p_tas256x,
			p_tas256x->devs[ch-1]->lim_rel_rate, ch);

	n_result |= tas256x_update_limiter_release_step_size(p_tas256x,
			p_tas256x->devs[ch-1]->lim_rel_stp_size, ch);

	n_result |= tas256x_update_bop_enable(p_tas256x,
			p_tas256x->devs[ch-1]->bop_enable, ch);

	n_result |= tas256x_update_bop_mute(p_tas256x,
			p_tas256x->devs[ch-1]->bop_mute, ch);

	n_result |= tas256x_update_bop_shutdown_enable(p_tas256x,
			p_tas256x->devs[ch-1]->bosd_enable, ch);

	n_result |= tas256x_update_bop_attack_rate(p_tas256x,
			p_tas256x->devs[ch-1]->bop_att_rate, ch);

	n_result |= tas256x_update_bop_attack_step_size(p_tas256x,
			p_tas256x->devs[ch-1]->bop_att_stp_size, ch);

	n_result |= tas256x_update_bop_hold_time(p_tas256x,
			p_tas256x->devs[ch-1]->bop_hld_time, ch);

	n_result |= tas256x_update_vbat_lpf(p_tas256x,
			p_tas256x->devs[ch-1]->vbat_lpf, ch);

	n_result |= tas256x_update_rx_cfg(p_tas256x,
			p_tas256x->devs[ch-1]->rx_cfg, ch);

	n_result |= tas256x_update_classh_timer(p_tas256x,
			p_tas256x->devs[ch-1]->classh_timer, ch);

	n_result |= tas256x_enable_receiver_mode(p_tas256x,
			p_tas256x->devs[ch-1]->receiver_enable, ch);

	n_result |= tas256x_icn_disable(p_tas256x,
		p_tas256x->icn_sw, ch);

	n_result |= tas256x_rx_set_slot(p_tas256x,
		p_tas256x->mn_rx_slot_map[ch-1], ch);

	return n_result;
}

void tas256x_irq_reload(struct tas256x_priv *p_tas256x)
{
	int ret = 0;

	pr_info("%s:\n", __func__);
	ret |= tas256x_set_power_state(p_tas256x, p_tas256x->mn_power_state);
	/* power up failed, restart later */
	if (ret < 0) {
		if (p_tas256x->mn_err_code &
			(ERROR_DEVA_I2C_COMM | ERROR_DEVB_I2C_COMM))
			tas256x_failsafe(p_tas256x);
	}

}

void tas256x_load_config(struct tas256x_priv *p_tas256x)
{
	int ret = 0;

	pr_info("%s:\n", __func__);
	tas256x_hard_reset(p_tas256x);
	msleep(20);

	ret |= tas56x_software_reset(p_tas256x, channel_both);
	if (ret < 0)
		goto end;
	ret |= tas256x_load_ctrl_values(p_tas256x, channel_left);
	if (ret < 0)
		goto end;
	if (p_tas256x->mn_channels == 2) {
		ret |= tas256x_load_ctrl_values(p_tas256x, channel_right);
		if (ret < 0)
			goto end;
	}
	ret |= tas256x_load_init(p_tas256x);
	if (ret < 0)
		goto end;
	ret |= tas256x_update_rx_cfg(p_tas256x, p_tas256x->devs[0]->rx_cfg,
			channel_left);
	if (ret < 0)
		goto end;
	if (p_tas256x->mn_channels == 2) {
		ret |= tas256x_update_rx_cfg(
			p_tas256x, p_tas256x->devs[1]->rx_cfg,
			channel_right);
		if (ret < 0)
			goto end;
	}
	if (p_tas256x->rcv_only == 0) {
		ret |= tas256x_iv_sense_enable_set(p_tas256x, 1,
			channel_both);
		if (ret < 0)
			goto end;
	}
	if (p_tas256x->mn_fmt_mode == 2) {
		ret |= tas256x_set_tdm_rx_slot(p_tas256x, p_tas256x->mn_rx_slots,
			p_tas256x->mn_rx_width);
		if (ret < 0)
			goto end;
		ret |= tas256x_set_tdm_tx_slot(p_tas256x, p_tas256x->mn_tx_slots,
			p_tas256x->mn_tx_width);
		if (ret < 0)
			goto end;
	} else { /*I2S Mode*/
		ret |= tas256x_set_bitwidth(p_tas256x,
			p_tas256x->mn_rx_width, TAS256X_STREAM_PLAYBACK);
		if (ret < 0)
			goto end;
		ret |= tas256x_set_bitwidth(p_tas256x,
			p_tas256x->mn_rx_width, TAS256X_STREAM_CAPTURE);
		if (ret < 0)
			goto end;
	}

	ret |= tas256x_set_samplerate(p_tas256x, p_tas256x->mn_sampling_rate,
		channel_both);
	if (ret < 0)
		goto end;
	ret |= tas256x_set_power_state(p_tas256x, p_tas256x->mn_power_state);
	if (ret < 0)
		goto end;
end:
/* power up failed, restart later */
	if (ret < 0) {
		if (p_tas256x->mn_err_code &
			(ERROR_DEVA_I2C_COMM | ERROR_DEVB_I2C_COMM))
			tas256x_failsafe(p_tas256x);
	}
}

void tas256x_reload(struct tas256x_priv *p_tas256x, int chn)
{
	int ret = 0;
	/*To be used later*/
	(void)chn;

	pr_info("%s: chn %d\n", __func__, chn);
	p_tas256x->enable_irq(p_tas256x, false);

	ret |= tas56x_software_reset(p_tas256x, channel_both);
	if (ret < 0)
		goto end;
	ret |= tas256x_load_init(p_tas256x);
	if (ret < 0)
		goto end;
	ret |= tas256x_load_ctrl_values(p_tas256x, channel_left);
	if (ret < 0)
		goto end;
	if (p_tas256x->mn_channels == 2) {
		ret |= tas256x_load_ctrl_values(p_tas256x, channel_right);
		if (ret < 0)
			goto end;
	}
	if (p_tas256x->rcv_only == 0) {
		ret |= tas256x_iv_sense_enable_set(p_tas256x, 1,
			channel_both);
		if (ret < 0)
			goto end;
	}
	/* Since TDM & I2S Mode can have different width and slot settings
	 * It needs to be differentiated here
	 */
	if (p_tas256x->mn_fmt_mode == 2) {
		ret |= tas256x_set_tdm_rx_slot(p_tas256x, p_tas256x->mn_rx_slots,
			p_tas256x->mn_rx_width);
		if (ret < 0)
			goto end;
		ret |= tas256x_set_tdm_tx_slot(p_tas256x, p_tas256x->mn_tx_slots,
			p_tas256x->mn_tx_width);
		if (ret < 0)
			goto end;
	} else { /*I2S Mode*/
		ret |= tas256x_set_bitwidth(p_tas256x,
			p_tas256x->mn_rx_width, TAS256X_STREAM_PLAYBACK);
		if (ret < 0)
			goto end;
		ret |= tas256x_set_bitwidth(p_tas256x,
			p_tas256x->mn_rx_width, TAS256X_STREAM_CAPTURE);
		if (ret < 0)
			goto end;
	}
	ret |= tas256x_set_samplerate(p_tas256x, p_tas256x->mn_sampling_rate,
		channel_both);
	if (ret < 0)
		goto end;
	ret |= tas256x_set_power_state(p_tas256x, p_tas256x->mn_power_state);
	if (ret < 0)
		goto end;
end:
	p_tas256x->enable_irq(p_tas256x, true);
/* power up failed, restart later */
	if (ret < 0) {
		if (p_tas256x->mn_err_code &
			(ERROR_DEVA_I2C_COMM | ERROR_DEVB_I2C_COMM))
			tas256x_failsafe(p_tas256x);
	}
}

static int tas2558_specific(struct tas256x_priv *p_tas256x, int chn)
{
	int ret = 0;

	pr_info("%s: chn %d\n", __func__, chn);
	ret = tas256x_boost_volt_update(p_tas256x, DEVICE_TAS2558, chn);

	return ret;
}

static int tas2564_specific(struct tas256x_priv *p_tas256x, int chn)
{
	int ret = 0;

	pr_info("%s: chn %d\n", __func__, chn);
	ret = tas256x_boost_volt_update(p_tas256x, DEVICE_TAS2564, chn);

	return ret;
}

int tas256x_irq_work_func(struct tas256x_priv *p_tas256x)
{
	unsigned int nDevInt1Status = 0, nDevInt2Status = 0,
		nDevInt3Status = 0, nDevInt4Status = 0;
	int n_counter = 2;
	int n_result = 0;
	int irqreg, irqreg2, i, chnTemp = 0;
	enum channel chn = channel_left;

	pr_info("%s:\n", __func__);

	p_tas256x->enable_irq(p_tas256x, false);

	if (p_tas256x->mn_err_code & ERROR_FAILSAFE)
		goto reload;

	if (p_tas256x->mn_power_state == TAS256X_POWER_SHUTDOWN) {
		pr_err("%s: device not powered\n", __func__);
		goto end;
	}

	n_result = tas256x_interrupt_enable(p_tas256x, 0/*Disable*/,
			channel_both);
	if (n_result < 0)
		goto reload;

	/*Reset error codes*/
	p_tas256x->mn_err_code = 0;

	for (i = 0; i < p_tas256x->mn_channels; i++) {
		if (p_tas256x->devs[i]->spk_control == 1)
			chnTemp |= 1<<i;
	}
	chn = (chnTemp == 0)?chn:(enum channel)chnTemp;

	if (chn & channel_left) {
		n_result = tas256x_interrupt_read(p_tas256x,
			&nDevInt1Status, &nDevInt2Status, channel_left);
		if (n_result < 0)
			goto reload;
		p_tas256x->mn_err_code =
			tas256x_interrupt_determine(p_tas256x, channel_left,
				nDevInt1Status, nDevInt2Status);
	}

	if (chn & channel_right) {
		n_result = tas256x_interrupt_read(p_tas256x,
			&nDevInt3Status, &nDevInt4Status, channel_right);
		if (n_result < 0)
			goto reload;
		p_tas256x->mn_err_code |=
			tas256x_interrupt_determine(p_tas256x, channel_right,
				nDevInt3Status, nDevInt4Status);
	}

	pr_info("%s: IRQ status : 0x%x, 0x%x, 0x%x, 0x%x mn_err_code %d\n",
		__func__,
		nDevInt1Status, nDevInt2Status,
		nDevInt3Status, nDevInt4Status,
		p_tas256x->mn_err_code);

	if (p_tas256x->mn_err_code) {
		tas256x_irq_reload(p_tas256x);
		goto end;
	} else {
		pr_info("%s: Power Up\n", __func__);
		n_counter = 2;
		while (n_counter > 0) {
			if (chn & channel_left)
				n_result = tas256x_power_check(p_tas256x,
						&nDevInt1Status,
						channel_left);
			if (n_result < 0)
				goto reload;
			if (chn & channel_right)
				n_result = tas256x_power_check(p_tas256x,
						&nDevInt3Status,
						channel_right);
			if (n_result < 0)
				goto reload;

			if (nDevInt1Status) {
				/* If only left should be power on */
				if (chn == channel_left)
					break;
				/* If both should be power on */
				if (nDevInt3Status)
					break;
			} else if (chn == channel_right) {
				/*If only right should be power on */
				if (nDevInt3Status)
					break;
			}
			/* Separate left and right read in case of Mono */
			if (chn & channel_left)
				tas256x_interrupt_read(p_tas256x,
					&irqreg, &irqreg2, channel_left);
			if (chn & channel_right)
				tas256x_interrupt_read(p_tas256x,
					&irqreg, &irqreg2, channel_right);
			n_counter--;
			if (n_counter > 0) {
			/* in case check pow status
			 *just after power on TAS256x
			 */
				if (chn & channel_left)
					pr_info("%s: PowSts A: 0x%x, check again after 10ms\n",
						__func__,
						nDevInt1Status);

				if (chn & channel_right)
					pr_info("%s: PowSts B: 0x%x, check again after 10ms\n",
						__func__,
						nDevInt3Status);

				msleep(20);
			}
		}

		if (((!nDevInt1Status) && (chn & channel_left))
			|| ((!nDevInt3Status) && (chn & channel_right))) {
			if (chn & channel_left)
				pr_err("%s, Critical ERROR A REG[POWERCONTROL] = 0x%x\n",
					__func__,
					nDevInt1Status);

			if (chn & channel_right)
				pr_err("%s, Critical ERROR B REG[POWERCONTROL] = 0x%x\n",
					__func__,
					nDevInt3Status);
			goto reload;
		}
	}

	n_result = tas256x_interrupt_enable(p_tas256x, 1/*Enable*/,
		channel_both);
	if (n_result < 0)
		goto reload;

	goto end;

reload:
	/* hardware reset and reload */
	tas256x_load_config(p_tas256x);

end:
	p_tas256x->enable_irq(p_tas256x, true);

	return n_result;
}

int tas256x_init_work_func(struct tas256x_priv *p_tas256x)
{
	int n_result = 0;

	pr_info("%s:\n", __func__);

	/* Clear latched IRQ before power on */
	n_result = tas256x_interrupt_clear(p_tas256x, channel_both);

	/*Un-Mask interrupt for TDM*/
	n_result = tas256x_interrupt_enable(p_tas256x, 1/*Enable*/,
		channel_both);

	p_tas256x->enable_irq(p_tas256x, true);

	return n_result;
}

int tas256x_dc_work_func(struct tas256x_priv *p_tas256x, int ch)
{
	int n_result = 0;

	pr_info("%s: ch %d\n", __func__, ch);
	tas256x_reload(p_tas256x, ch);

	return n_result;
}

int tas256x_register_device(struct tas256x_priv  *p_tas256x)
{
	int n_result;
	int i;

	pr_info("%s:\n", __func__);
	p_tas256x->read = tas256x_dev_read;
	p_tas256x->write = tas256x_dev_write;
	p_tas256x->bulk_read = tas256x_dev_bulk_read;
	p_tas256x->bulk_write = tas256x_dev_bulk_write;
	p_tas256x->update_bits = tas256x_dev_update_bits;

	tas256x_hard_reset(p_tas256x);

	pr_debug("Before SW reset\n");
	/* Reset the chip */
	n_result = tas56x_software_reset(p_tas256x, channel_both);
	if (n_result < 0) {
		pr_err("I2c fail, %d\n", n_result);
		goto err;
	}

	pr_debug("After SW reset\n");

	for (i = 0; i < p_tas256x->mn_channels; i++) {
		n_result = tas56x_get_chipid(p_tas256x,
			&(p_tas256x->devs[i]->mn_chip_id),
			(i == 0) ? channel_left : channel_right);
		if (n_result < 0)
			goto err;
		switch (p_tas256x->devs[i]->mn_chip_id) {
		case 0x10:
		case 0x20:
			pr_info("TAS2562 chip");
			p_tas256x->devs[i]->device_id = DEVICE_TAS2562;
			p_tas256x->devs[i]->dev_ops.tas_init = NULL;
			break;
		case 0x00:
			pr_info("TAS2564 chip");
			p_tas256x->devs[i]->device_id = DEVICE_TAS2564;
			p_tas256x->devs[i]->dev_ops.tas_init =
				tas2564_specific;
			break;
		default:
			pr_info("TAS2558 chip");
			p_tas256x->devs[i]->device_id = DEVICE_TAS2558;
			p_tas256x->devs[i]->dev_ops.tas_init =
				tas2558_specific;
			break;
		}
		n_result |= tas256x_set_misc_config(p_tas256x, 0,
				(i == 0) ? channel_left : channel_right);
	}
err:
	return n_result;
}

int tas256x_probe(struct tas256x_priv *p_tas256x)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_TAS25XX_ALGO)
	struct linux_platform *plat_data =
			(struct linux_platform *) p_tas256x->platform_data;
#endif

	pr_info("%s:\n", __func__);
	ret = tas256x_load_init(p_tas256x);
	if (ret < 0)
		goto end;
	if (p_tas256x->rcv_only == 0)
		ret = tas256x_iv_sense_enable_set(p_tas256x, 1, channel_both);
#if IS_ENABLED(CONFIG_TAS25XX_ALGO)
	if (plat_data) {
		tas_smartamp_add_algo_controls(plat_data->codec, plat_data->dev,
			p_tas256x->mn_channels);
		/*Send IV Vbat format but don't update to algo yet*/
		tas25xx_set_iv_bit_fomat(p_tas256x->mn_iv_width,
			p_tas256x->mn_vbat, 0);
	}
#endif
#if IS_ENABLED(CONFIG_TAS256X_REGBIN_PARSER)
	ret = tas256x_load_container(p_tas256x);
	pr_info("%s Bin file loading requested: %d\n", __func__, ret);
#endif
end:
	return ret;
}

void tas256x_remove(struct tas256x_priv *p_tas256x)
{
#if IS_ENABLED(CONFIG_TAS25XX_ALGO)
	struct linux_platform *plat_data =
			(struct linux_platform *) p_tas256x->platform_data;
	if (plat_data)
		tas_smartamp_remove_algo_controls(plat_data->codec);
#else
	/*Ignore argument*/
	(void)p_tas256x;
#endif
#if IS_ENABLED(CONFIG_TAS256X_REGBIN_PARSER)
	tas256x_config_info_remove(p_tas256x);
#endif
}

int tas256x_set_power_state(struct tas256x_priv *p_tas256x,
			int state)
{
	int n_result = 0, i = 0, chnTemp = 0;
	enum channel chn = channel_left;

	pr_info("%s: state %d\n", __func__, state);

	if ((p_tas256x->mb_mute) && (state == TAS256X_POWER_ACTIVE))
		state = TAS256X_POWER_MUTE;

	for (i = 0; i < p_tas256x->mn_channels; i++) {
		if (p_tas256x->devs[i]->spk_control == 1)
			chnTemp |= 1<<i;
	}
	chn = chnTemp;
	if (chnTemp == 0) {
		pr_err("Both Left and Right Switch Off\n");
		return n_result;
	}

	switch (state) {
	case TAS256X_POWER_ACTIVE:
		if (p_tas256x->rcv_only == 0)
			n_result = tas256x_iv_sense_enable_set(p_tas256x, 1, chn);

		/* Clear latched IRQ before power on */
		tas256x_interrupt_clear(p_tas256x, chn);

		/*Mask interrupt for TDM*/
		n_result = tas256x_interrupt_enable(p_tas256x, 0/*Disable*/,
			channel_both);

		if (p_tas256x->curr_mn_iv_width == 8)
			tas256x_enable_emphasis_filter(p_tas256x,
				channel_both,
				p_tas256x->mn_sampling_rate);
#if IS_ENABLED(CONFIG_TAS256X_REGBIN_PARSER)
		/*set p_tas256x->profile_cfg_id by tinymix*/
		tas256x_select_cfg_blk(p_tas256x,
			p_tas256x->profile_cfg_id,
			TAS256X_BIN_BLK_PRE_POWER_UP);
#endif
		n_result = tas256x_set_power_up(p_tas256x, chn);

		pr_info("%s: set ICN to -80dB\n", __func__);
		n_result = tas256x_icn_data(p_tas256x, chn);
#if IS_ENABLED(CONFIG_TAS256X_REGBIN_PARSER)
		/*set p_tas256x->profile_cfg_id by tinymix*/
		tas256x_select_cfg_blk(p_tas256x,
			p_tas256x->profile_cfg_id,
			TAS256X_BIN_BLK_POST_POWER_UP);
#endif
		p_tas256x->mb_power_up = true;
		p_tas256x->mn_power_state = TAS256X_POWER_ACTIVE;
		p_tas256x->schedule_init_work(p_tas256x);
		break;

	case TAS256X_POWER_MUTE:
		n_result = tas256x_set_power_mute(p_tas256x, chn);
			p_tas256x->mb_power_up = true;
			p_tas256x->mn_power_state = TAS256X_POWER_MUTE;

		/*Mask interrupt for TDM*/
		n_result = tas256x_interrupt_enable(p_tas256x, 0/*Disable*/,
			chn);
		break;

	case TAS256X_POWER_SHUTDOWN:
#if IS_ENABLED(CONFIG_TAS256X_REGBIN_PARSER)
		/*set p_tas256x->profile_cfg_id by tinymix*/
		tas256x_select_cfg_blk(p_tas256x, p_tas256x->profile_cfg_id,
			TAS256X_BIN_BLK_PRE_SHUTDOWN);
#endif
		for (i = 0; i < p_tas256x->mn_channels; i++) {
			if (p_tas256x->devs[i]->device_id == DEVICE_TAS2564) {
				if (chn & (i+1)) {
					/*Mask interrupt for TDM*/
					n_result =
						tas256x_interrupt_enable(
							p_tas256x,
							0/*Disable*/, i+1);
					n_result =
						tas256x_set_power_mute(
							p_tas256x,
							i+1);
					n_result =
						tas256x_iv_sense_enable_set(
							p_tas256x,
							0, i+1);
					n_result =
						tas256x_ivvbat_slot_disable(
							p_tas256x,
							p_tas256x->curr_mn_vbat,
							i+1);

					p_tas256x->mb_power_up = false;
					p_tas256x->mn_power_state =
						TAS256X_POWER_SHUTDOWN;
					msleep(20);
				}
			} else {
				if (chn & (i+1)) {
					n_result =
						tas256x_set_power_shutdown(
							p_tas256x, i+1);
					n_result =
						tas256x_iv_sense_enable_set(
							p_tas256x, 0,
							i+1);
					n_result =
						tas256x_ivvbat_slot_disable(
							p_tas256x,
							p_tas256x->curr_mn_vbat,
							i+1);
					p_tas256x->mb_power_up = false;
					p_tas256x->mn_power_state =
						TAS256X_POWER_SHUTDOWN;
					msleep(20);
					/*Mask interrupt for TDM*/
					n_result = tas256x_interrupt_enable(
						p_tas256x,
						0/*Disable*/,
						i+1);
				}
			}
		}
		p_tas256x->enable_irq(p_tas256x, false);
		break;
	default:
		pr_err("wrong power state setting %d\n",
				state);
	}

	return n_result;
}

/* In Order to fix the issue of "24bit Bitwidth and 8bit IV Width and no VBat"
 * Function is redesigned to support all the (1)Bitwidth (2) IVwidth and (3)VBat
 * Configurations
 */

int tas256x_iv_vbat_slot_config(struct tas256x_priv *p_tas256x,
	int mn_slot_width)
{
	int ret = -1;

	pr_info("%s: mn_slot_width %d\n", __func__, mn_slot_width);

	if (p_tas256x->mn_fmt_mode == 2) { /*TDM Mode*/
		if (p_tas256x->mn_channels == 2) {
			if (mn_slot_width == 16) {
				if (p_tas256x->mn_vbat == 1) {
					ret =
						tas256x_set_iv_slot(p_tas256x,
							channel_left, TX_SLOT1,
							TX_SLOT0);
					ret |=
						tas256x_set_iv_slot(p_tas256x,
							channel_right, TX_SLOT5,
							TX_SLOT4);
					ret |=
						tas256x_set_vbat_slot(p_tas256x,
							channel_left, TX_SLOT3);
					ret |=
						tas256x_set_vbat_slot(p_tas256x,
							channel_right,
							TX_SLOT7);
					p_tas256x->curr_mn_iv_width = 12;
					p_tas256x->curr_mn_vbat = 1;
				} else {
					ret =
						tas256x_set_iv_slot(p_tas256x,
							channel_left, TX_SLOT2,
							TX_SLOT0);
					ret |=
						tas256x_set_iv_slot(p_tas256x,
							channel_right, TX_SLOT6,
							TX_SLOT4);
					p_tas256x->curr_mn_iv_width = 16;
					p_tas256x->curr_mn_vbat = 0;
				}

			} else if (mn_slot_width == 24) {
				ret =
					tas256x_set_iv_slot(p_tas256x,
						channel_left, TX_SLOT4,
						TX_SLOT0);
				ret |= tas256x_set_iv_slot(p_tas256x,
						channel_right, TX_SLOTc,
						TX_SLOT8);
				p_tas256x->curr_mn_vbat = 0;
				if (p_tas256x->mn_vbat == 1) {
					ret |=
						tas256x_set_vbat_slot(p_tas256x,
							channel_left, TX_SLOT6);
					ret |=
						tas256x_set_vbat_slot(p_tas256x,
							channel_right,
							TX_SLOTe);
					p_tas256x->curr_mn_vbat = 1;
				}
				p_tas256x->curr_mn_iv_width = 16;

			} else { /*Assumed 32bit*/
				ret =
					tas256x_set_iv_slot(p_tas256x,
						channel_left, TX_SLOT4,
						TX_SLOT0);
				ret |= tas256x_set_iv_slot(p_tas256x,
						channel_right, TX_SLOTc,
						TX_SLOT8);
				p_tas256x->curr_mn_vbat = 0;
				if (p_tas256x->mn_vbat == 1) {
					ret |=
						tas256x_set_vbat_slot(p_tas256x,
							channel_left, TX_SLOT6);
					ret |=
						tas256x_set_vbat_slot(p_tas256x,
							channel_right,
							TX_SLOTe);
					p_tas256x->curr_mn_vbat = 1;
				}
				p_tas256x->curr_mn_iv_width = 16;
			}
		} else { /*Assumed Mono Channels*/
			if (mn_slot_width == 16) {
				if (p_tas256x->mn_vbat == 1) {
					ret =
						tas256x_set_iv_slot(p_tas256x,
							channel_left, TX_SLOT1,
							TX_SLOT0);
					ret |=
						tas256x_set_vbat_slot(p_tas256x,
							channel_left, TX_SLOT3);
					p_tas256x->curr_mn_iv_width = 12;
					p_tas256x->curr_mn_vbat = 1;
				} else {
					ret =
						tas256x_set_iv_slot(p_tas256x,
							channel_left, TX_SLOT2,
							TX_SLOT0);
					p_tas256x->curr_mn_iv_width = 16;
					p_tas256x->curr_mn_vbat = 1;
				}
			} else if (mn_slot_width == 24) {
				ret =
					tas256x_set_iv_slot(p_tas256x,
						channel_left, TX_SLOT4,
						TX_SLOT0);
				p_tas256x->curr_mn_iv_width = 16;
				p_tas256x->curr_mn_vbat = 0;
				if (p_tas256x->mn_vbat == 1) {
					ret |=
						tas256x_set_vbat_slot(p_tas256x,
							channel_left, TX_SLOT6);
					p_tas256x->curr_mn_vbat = 1;
				}
			} else { /*Assumed 32bit*/
				ret =
					tas256x_set_iv_slot(p_tas256x,
						channel_left, TX_SLOT4,
						TX_SLOT0);
				p_tas256x->curr_mn_iv_width = 16;
				p_tas256x->curr_mn_vbat = 0;
				if (p_tas256x->mn_vbat == 1) {
					ret |=
						tas256x_set_vbat_slot(p_tas256x,
							channel_left, TX_SLOT6);
					p_tas256x->curr_mn_vbat = 1;
				}
			}
		}
	} else { /*I2S Mode*/
		if (mn_slot_width == 16) {
			if (p_tas256x->mn_channels == 2) {
				switch (p_tas256x->mn_iv_width) {
				case 8:
				case 12:
				case 16:
				default:
					ret = tas256x_set_iv_slot(p_tas256x,
						channel_left, TX_SLOT1, TX_SLOT0);
					ret |= tas256x_set_iv_slot(p_tas256x,
						channel_right, TX_SLOT3, TX_SLOT2);
					p_tas256x->curr_mn_iv_width = 8;
					p_tas256x->curr_mn_vbat = 0;
					break;
				}
			} else {
				switch (p_tas256x->mn_iv_width) {
				case 8:
					ret = tas256x_set_iv_slot(p_tas256x,
						channel_left, TX_SLOT1, TX_SLOT0);
					p_tas256x->curr_mn_iv_width = 8;
					if (p_tas256x->mn_vbat == 1) {
						ret |= tas256x_set_vbat_slot(p_tas256x,
							channel_left, TX_SLOT2);
						p_tas256x->curr_mn_vbat = 1;
					} else {
						p_tas256x->curr_mn_vbat = 0;
					}
					break;
				case 12:
					p_tas256x->curr_mn_iv_width = 12;
					if (p_tas256x->mn_vbat == 1) {
						ret = tas256x_set_iv_slot(p_tas256x,
							channel_left, TX_SLOT1, TX_SLOT0);
						ret |= tas256x_set_vbat_slot(p_tas256x,
							channel_left, TX_SLOT3);
						p_tas256x->curr_mn_vbat = 1;
					} else {
						ret = tas256x_set_iv_slot(p_tas256x,
							channel_left, TX_SLOT2, TX_SLOT0);
						p_tas256x->curr_mn_vbat = 0;
					}
					break;
				case 16:
				default:
					if (p_tas256x->mn_vbat == 1) {
						ret = tas256x_set_iv_slot(p_tas256x,
							channel_left, TX_SLOT1, TX_SLOT0);
						ret |= tas256x_set_vbat_slot(p_tas256x,
							channel_left, TX_SLOT3);
						p_tas256x->curr_mn_iv_width = 12;
						p_tas256x->curr_mn_vbat = 1;
					} else {
						ret = tas256x_set_iv_slot(p_tas256x,
							channel_left, TX_SLOT2, TX_SLOT0);
						p_tas256x->curr_mn_iv_width = 16;
						p_tas256x->curr_mn_vbat = 0;
					}
					break;
				}
			}
		} else { /* mn_slot_width == 24 or mn_slot_width == 32 */
			if (p_tas256x->mn_channels == 2) {
				switch (p_tas256x->mn_iv_width) {
				case 8:
					ret = tas256x_set_iv_slot(p_tas256x,
						channel_left, TX_SLOT1, TX_SLOT0);
					ret |= tas256x_set_iv_slot(p_tas256x,
						channel_right, TX_SLOT5, TX_SLOT4);
					p_tas256x->curr_mn_iv_width = 8;
					if (p_tas256x->mn_vbat == 1) {
						ret |= tas256x_set_vbat_slot(p_tas256x,
							channel_left, TX_SLOT2);
						ret |= tas256x_set_vbat_slot(p_tas256x,
							channel_right, TX_SLOT6);
						p_tas256x->curr_mn_vbat = 1;
					} else {
						p_tas256x->curr_mn_vbat = 0;
					}
					break;
				case 12:
					if (p_tas256x->mn_vbat == 1) {
						ret = tas256x_set_iv_slot(p_tas256x,
							channel_left, TX_SLOT1, TX_SLOT0);
						ret |= tas256x_set_iv_slot(p_tas256x,
							channel_right, TX_SLOT5, TX_SLOT4);
						ret |= tas256x_set_vbat_slot(p_tas256x,
							channel_left, TX_SLOT2);
						ret |= tas256x_set_vbat_slot(p_tas256x,
							channel_right, TX_SLOT6);
						p_tas256x->curr_mn_iv_width = 8;
						p_tas256x->curr_mn_vbat = 1;
					} else {
						ret = tas256x_set_iv_slot(p_tas256x,
							channel_left, TX_SLOT1, TX_SLOT0);
						ret |= tas256x_set_iv_slot(p_tas256x,
							channel_right, TX_SLOT5, TX_SLOT4);
						p_tas256x->curr_mn_iv_width = 12;
						p_tas256x->curr_mn_vbat = 0;
					}
					break;
				case 16:
				default:
					if (p_tas256x->mn_vbat == 1) {
						ret = tas256x_set_iv_slot(p_tas256x,
							channel_left, TX_SLOT1, TX_SLOT0);
						ret |= tas256x_set_iv_slot(p_tas256x,
							channel_right, TX_SLOT5, TX_SLOT4);
						ret |= tas256x_set_vbat_slot(p_tas256x,
							channel_left, TX_SLOT2);
						ret |= tas256x_set_vbat_slot(p_tas256x,
							channel_right, TX_SLOT6);
						p_tas256x->curr_mn_iv_width = 8;
						p_tas256x->curr_mn_vbat = 1;
					} else {
						ret = tas256x_set_iv_slot(p_tas256x,
							channel_left, TX_SLOT2, TX_SLOT0);
						ret |= tas256x_set_iv_slot(p_tas256x,
							channel_right, TX_SLOT6, TX_SLOT4);
						p_tas256x->curr_mn_iv_width = 16;
						p_tas256x->curr_mn_vbat = 0;
					}
					break;
				}
			} else { /* Assumed Mono */
				switch (p_tas256x->mn_iv_width) {
				case 8:
					ret = tas256x_set_iv_slot(p_tas256x,
						channel_left, TX_SLOT1, TX_SLOT0);
					p_tas256x->curr_mn_iv_width = 8;
					if (p_tas256x->mn_vbat == 1) {
						ret |= tas256x_set_vbat_slot(p_tas256x,
							channel_left, TX_SLOT2);
						p_tas256x->curr_mn_vbat = 1;
					} else {
						p_tas256x->curr_mn_vbat = 0;
					}
					break;
				case 12:
					ret = tas256x_set_iv_slot(p_tas256x,
						channel_left, TX_SLOT1, TX_SLOT0);
					p_tas256x->curr_mn_iv_width = 12;
					if (p_tas256x->mn_vbat == 1) {
						ret |= tas256x_set_vbat_slot(p_tas256x,
							channel_left, TX_SLOT4);
						p_tas256x->curr_mn_vbat = 1;
					} else {
						p_tas256x->curr_mn_vbat = 0;
					}
					break;
				case 16:
				default:
					ret = tas256x_set_iv_slot(p_tas256x,
						channel_left, TX_SLOT4, TX_SLOT0);
					p_tas256x->curr_mn_iv_width = 16;
					if (p_tas256x->mn_vbat == 1) {
						ret |= tas256x_set_vbat_slot(p_tas256x,
							channel_left, TX_SLOT6);
						p_tas256x->curr_mn_vbat = 1;
					} else {
						p_tas256x->curr_mn_vbat = 0;
					}
					break;
				}
			}
		}
	}

	if (ret == 0)
		p_tas256x->mn_tx_slot_width = mn_slot_width;

	return ret;
}

/* tas256x_set_bitwidth function is redesigned to accommodate change in
 * tas256x_iv_vbat_slot_config()
 */
int tas256x_set_bitwidth(struct tas256x_priv *p_tas256x,
	int bitwidth, int stream)
{
	int n_result = 0;

	pr_info("%s: bitwidth %d stream %d\n", __func__, bitwidth, stream);

	if (stream == TAS256X_STREAM_PLAYBACK) {
		n_result |= tas256x_rx_set_bitwidth(p_tas256x, bitwidth,
			channel_both);
		n_result |= tas256x_rx_set_slot_len(p_tas256x, bitwidth,
			channel_both);
	} else { /*stream == TAS256X_STREAM_CAPTURE*/
		n_result |= tas256x_iv_vbat_slot_config(p_tas256x,
				bitwidth);
		n_result |= tas256x_iv_bitwidth_config(p_tas256x,
			p_tas256x->curr_mn_iv_width, channel_both);
	}

	if (n_result < 0) {
		if (p_tas256x->mn_err_code &
			(ERROR_DEVA_I2C_COMM | ERROR_DEVB_I2C_COMM))
			tas256x_failsafe(p_tas256x);
	}

	return n_result;
}

/* tas256x_set_tdm_rx_slot function is redesigned to accommodate change in
 * tas256x_iv_vbat_slot_config()
 */
int tas256x_set_tdm_rx_slot(struct tas256x_priv *p_tas256x,
	int slots, int slot_width)
{
	int ret = -1;

	if (((p_tas256x->mn_channels == 1) && (slots < 1)) ||
		((p_tas256x->mn_channels == 2) && (slots < 2))) {
		pr_err("Invalid Slots %d\n", slots);
		return ret;
	}
	p_tas256x->mn_rx_slots = slots;

	if ((slot_width != 16) &&
		(slot_width != 24) &&
		(slot_width != 32)) {
		pr_err("Unsupported slot width %d\n", slot_width);
		return ret;
	}

	ret = tas256x_rx_set_slot_len(p_tas256x, slot_width, channel_both);

	ret = tas256x_rx_set_bitwidth(p_tas256x,
			slot_width, channel_both);

	/*Enable Auto Detect of Sample Rate */
	ret = tas256x_set_auto_detect_clock(p_tas256x,
			1, channel_both);

	/*Enable Clock Config*/
	ret = tas256x_set_clock_config(p_tas256x, 0/*Ignored*/,
			channel_both);

	return ret;
}

/* tas256x_set_tdm_tx_slot function is redesigned to accommodate change in
 * tas256x_iv_vbat_slot_config()
 */
int tas256x_set_tdm_tx_slot(struct tas256x_priv *p_tas256x,
	int slots, int slot_width)
{
	int ret = -1;

	if ((slot_width != 16) &&
		(slot_width != 24) &&
		(slot_width != 32)) {
		pr_err("Unsupported slot width %d\n", slot_width);
		return ret;
	}

	if (((p_tas256x->mn_channels == 1) && (slots < 2)) ||
		((p_tas256x->mn_channels == 2) && (slots < 4))) {
		pr_err("Invalid Slots %d\n", slots);
		return ret;
	}
	p_tas256x->mn_tx_slots = slots;
	p_tas256x->mn_tx_width = slot_width;

	ret = tas256x_iv_vbat_slot_config(p_tas256x, slot_width);

	ret |= tas256x_iv_bitwidth_config(p_tas256x,
		p_tas256x->curr_mn_iv_width, channel_both);

	return ret;
}

/* Re-Init the device */
int tas256x_reinit(struct tas256x_priv *p_tas256x)
{
	int i = 0, n_result = 0;

	/* Hardware reset */
	tas256x_hard_reset(p_tas256x);

	/* Software reset */
	n_result = tas56x_software_reset(p_tas256x, channel_both);
	if (n_result < 0) {
		pr_err("I2c fail, %d\n", n_result);
		goto err;
	}

	/* Load all initial values */
	n_result = tas256x_load_init(p_tas256x);

	/* Reload the control values */
	for (i = 0; i < p_tas256x->mn_channels; i++)
		tas256x_load_ctrl_values(p_tas256x, i+1);

err:
	if (n_result < 0) {
		if (p_tas256x->mn_err_code &
			(ERROR_DEVA_I2C_COMM | ERROR_DEVB_I2C_COMM))
			tas256x_failsafe(p_tas256x);
	}
	return n_result;
}

