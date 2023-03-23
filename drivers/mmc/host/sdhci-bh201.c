/*
 * drivers/mmc/host/sdhci-bh201.c -Bayhub Technologies, Inc. BH201 SDHCI
 * bridge IC for VENDOR SDHCI platform driver source file
 *
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/io.h>
#include <linux/delay.h>
#include "sdhci-bh201.h"
#if IFLYTEK
#include <linux/slab.h>
#include "sprd-sdhcr11.h"
#endif


#define TRUE	1
#define FALSE	0

#define DbgErr pr_err
#define PrintMsg  pr_err
#define DbgInfo(m, f, r, _x_,...)
#define os_memcpy memcpy
#define os_memset memset

//#define FORCE_READ_STATUS_CMD7_TIMEOUT
//#define FORCE_DRIVER_STRENGTH_ERROR
//#define FORCE_FIRST_CMD19_FAIL_ERROR

extern const u32 tuning_block_64[16];
extern const u32 tuning_block_128[32];

typedef struct t_gg_reg_strt {
	u32 ofs;
	u32 mask;
	u32 value;
} t_gg_reg_strt, *p_gg_reg_strt;

typedef struct rl_bit_lct {
	u8 bits;
	u8 rl_bits;
} rl_bit_lct;

static rl_bit_lct cfg_bit_map[6] = {
	{0, 6},
	{1, 5},
	{2, 4},
	{3, 2},
	{4, 1},
	{5, 0}
};

// read
t_gg_reg_strt pha_stas_rx_low32 = { 14, 0xffffffff, 0 };
t_gg_reg_strt pha_stas_rx_high32 = { 46, 0xffffffff, 0 };
t_gg_reg_strt pha_stas_tx_low32 = { 205, 0xffffffff, 0 };
t_gg_reg_strt pha_stas_tx_high32 = { 237, 0xffffffff, 0 };
t_gg_reg_strt dll_sela_after_mask = { 141, 0xf, 0 };
t_gg_reg_strt dll_selb_after_mask = { 145, 0xf, 0 };

t_gg_reg_strt lp3p3v_ocb = { 4, 0x1, 0 };
t_gg_reg_strt analog_ip_read_volatge_mask = { 154, 0xf, 0 };
t_gg_reg_strt dll_delay_100m_backup = { 83, 0xfff, 0 };
t_gg_reg_strt dll_delay_200m_backup = { 95, 0xfff, 0 };
t_gg_reg_strt ssc_bypass = { 171, 1, 0 };
t_gg_reg_strt ssc_enable = { 191, 1, 0 };
t_gg_reg_strt sd_freq_cur = { 151, 0x7, 0 };

// write
t_gg_reg_strt dll_sela_100m_cfg = { 126, 0xf, 0 };
t_gg_reg_strt dll_sela_200m_cfg = { 130, 0xf, 0 };
t_gg_reg_strt dll_selb_100m_cfg = { 140, 0xf, 0 };
t_gg_reg_strt dll_selb_200m_cfg = { 144, 0xf, 0 };

t_gg_reg_strt dll_selb_100m_cfg_en = { 183, 0x1, 0 };
t_gg_reg_strt dll_selb_200m_cfg_en = { 184, 0x1, 0 };
t_gg_reg_strt internl_tuning_en_100m = { 171, 0x1, 0 };
t_gg_reg_strt internl_tuning_en_200m = { 172, 0x1, 0 };
t_gg_reg_strt cmd19_cnt_cfg = { 173, 0x3f , 0 };
t_gg_reg_strt force_dll_lock = { 63, 0x1, 0 };
t_gg_reg_strt analog_ip_write_volatge_mask = { 167, 0xf, 0 };

t_gg_reg_strt dll_phase_adjust_en = { 105, 0x1, 0 };
t_gg_reg_strt dll_delay_config_en = { 64, 0x1, 0 };
t_gg_reg_strt dll_delay_config_wr = { 106, 0x1, 0 };
t_gg_reg_strt dll_delay_100m_cfg = { 81, 0xfff, 0 };
t_gg_reg_strt dll_delay_200m_cfg = { 93, 0xfff, 0 };

//inject
t_gg_reg_strt inject_failure_for_tuning_enable_cfg = { 357, 0x1, 0 };
t_gg_reg_strt inject_failure_for_200m_tuning_cfg = { 93, 0x7ff, 0 };
t_gg_reg_strt inject_failure_for_100m_tuning_cfg = { 81, 0x7ff, 0 };

t_gg_reg_strt cclk_ds_18 = { 3, 0x7, 0 };

#define MAX_CFG_BIT_VAL (383)
#define BIT_VALID_NUM_INBYTE 6
static void cfg_bit_2_bt(int max_bit, int tar, int *byt, int *bit)
{
	*byt = (max_bit - tar) / 6;
	*bit = cfg_bit_map[(max_bit - tar) % 6].rl_bits;
}

static u32 cfg_read_bits_ofs_mask(u8 * cfg, t_gg_reg_strt * bts)
{
	u32 rv = 0;
	u32 msk = bts->mask;
	int byt = 0, bit = 0;
	int i = 0;
	do {
		cfg_bit_2_bt(MAX_CFG_BIT_VAL, bts->ofs + i, &byt, &bit);
		if (cfg[byt] & (1 << bit)) {
			rv |= 1 << i;
		}
		//
		i++;
		msk >>= 1;
		if (0 == msk)
			break;

	}
	while (1);
	return rv;
}

static void cfg_write_bits_ofs_mask(u8 * cfg, t_gg_reg_strt * bts, u32 w_value)
{
	u32 wv = w_value & bts->mask;
	u32 msk = bts->mask;
	int byt = 0, bit = 0;
	int i = 0;
	do {
		cfg_bit_2_bt(MAX_CFG_BIT_VAL, bts->ofs + i, &byt, &bit);
		if (wv & 1) {
			cfg[byt] |= (u8) (1 << bit);
		} else {
			cfg[byt] &= (u8) (~(1 << bit));
		}
		//
		i++;
		wv >>= 1;
		msk >>= 1;
		if (0 == msk)
			break;

	}
	while (1);
}

void ram_bit_2_bt(int tar, int *byt, int *bit)
{
	*byt = tar / 8;
	*bit = tar % 8;
}

static u32 read_ram_bits_ofs_mask(u8 * cfg, t_gg_reg_strt * bts)
{
	u32 rv = 0;
	u32 msk = bts->mask;
	int byt = 0, bit = 0;
	int i = 0;
	do {
		ram_bit_2_bt(bts->ofs + i, &byt, &bit);
		if (cfg[byt] & (1 << bit)) {
			rv |= 1 << i;
		}
		//
		i++;
		msk >>= 1;
		if (0 == msk)
			break;

	}
	while (1);
	return rv;
}

int card_deselect_card(struct sdhci_host *host)
{
	int ret = -1;
	int err;
	struct mmc_host *mmc = host->mmc;
	struct mmc_command cmd = { 0 };
	BUG_ON(!host);
	cmd.opcode = MMC_SELECT_CARD;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_NONE | MMC_CMD_AC;

	err = mmc_wait_for_cmd(mmc, &cmd, 3);
	if (err) {
		pr_err("BHT ERR:%s: ---- CMD7 FAIL: err = %d ----\n", __FUNCTION__, err);
	} else {
		ret = 0;
	}
	return ret;
}

#define GG_ENTER_CMD7_DE_TIMES 2
#define GG_EXIT_CMD7_DE_TIMES 1
bool enter_exit_emulator_mode(struct sdhci_host * host, bool b_enter)
{
	bool ret = FALSE;
	u8 times = b_enter ? GG_ENTER_CMD7_DE_TIMES : GG_EXIT_CMD7_DE_TIMES;
	u8 i = 0;

	for (i = 0; i < times; i++) {

		ret = card_deselect_card(host);
		if (ret) {
			break;
		}
	}
	return ret;
}

static bool _gg_emulator_read_only(struct sdhci_host *host,
				   u8 * in_data, u32 datalen)
{
	struct mmc_host *mmc = host->mmc;
#if IFLYTEK
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
	int rc = 0;
	u8 *data1 = kzalloc(PAGE_SIZE, GFP_KERNEL);
	struct mmc_request mrq = { 0 };
	struct mmc_command cmd = { 0 };
	struct mmc_data data = { 0 };
	struct scatterlist sg;


	if (!data1) {
		PrintMsg("BHT MSG:gg read no memory\n");
		rc = -ENOMEM;
		goto out;
	}

	sg_init_one(&sg, data1, 512);

	cmd.opcode = MMC_READ_SINGLE_BLOCK;
	cmd.sw_cmd_timeout = 150;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
	data.blksz = 512;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.timeout_ns = 1000 * 1000 * 1000;	/* 1 sec */
	data.sg = &sg;
	data.sg_len = 1;
	mrq.cmd = &cmd;
	mrq.data = &data;
	mrq.stop = NULL;

	mmc_wait_for_req(mmc, &mrq);
	memcpy(in_data, data1, datalen);
	if(data1)
	kfree(data1);

	if (cmd.error || data.error) {

		if (cmd.error) {
			if (cmd.err_int_mask & 0xa0000) {
				pr_err("BHT ERR:cmd error 0x%x\n",cmd.err_int_mask);
				vendor_host->sdr50_notuning_crc_error_flag = 1;
			}
		}

		if (data.error) {
			if (data.err_int_mask & 0x200000) {
				pr_err("BHT ERR:data error 0x%x\n",data.err_int_mask);
				vendor_host->sdr50_notuning_crc_error_flag = 1;
			}
		}
		rc = -1;
		goto out;
	}

out:
	return rc;
}

void host_cmddat_line_reset(struct sdhci_host *host)
{
#if IFLYTEK
	struct sprd_sdhc_host *sprd_host ;
	if(host->mmc)
	{
		sprd_host = mmc_priv(host->mmc);
		sprd_sdhc_reset(sprd_host, SPRD_SDHC_BIT_RST_CMD|SPRD_SDHC_BIT_RST_DAT);
	}	
#else
	if (host->ops->reset) {
		host->ops->reset(host, SDHCI_RESET_CMD | SDHCI_RESET_DATA);
	}
#endif

}

static int gg_select_card_spec(struct sdhci_host *host)
{
	int err;
	struct mmc_command cmd = { 0 };
	struct mmc_card *card = host->mmc->card;

	cmd.opcode = MMC_SELECT_CARD;
	cmd.sw_cmd_timeout = 150;

	if (card) {
		cmd.arg = card->rca << 16;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
		//pr_info("BHT DBG: select card CMD argument config\n");
	} else {
		cmd.arg = 0;
		cmd.flags = MMC_RSP_NONE | MMC_CMD_AC;
		pr_err("BHT ERR:%s: Card structure is a null pointer!!!\n", __FUNCTION__);
	}

	err = mmc_wait_for_cmd(host->mmc, &cmd, 0);
	if (err) {

		if (-EILSEQ == err && (0x20000 == (0x30000 & cmd.err_int_mask))) {
			host_cmddat_line_reset(host);
			{
				struct mmc_command cmd = { 0 };
				cmd.opcode = 5;
				cmd.arg = 0;
				cmd.flags =
				    MMC_RSP_SPI_R4 | MMC_RSP_R4 | MMC_CMD_BCR;

				mmc_wait_for_cmd(host->mmc, &cmd, 0);
			}
			pr_err("BHT ERR:%s: CMD7 CRC\n", __FUNCTION__);
			host_cmddat_line_reset(host);
			return 0;
		}
		if (-ETIMEDOUT == err && (0x10000 & cmd.err_int_mask)) {
			pr_err("BHT ERR:%s: CMD7 timeout\n", __FUNCTION__);
			host_cmddat_line_reset(host);
			return err;
		}
		return 0;
	}

	return 0;
}

void dump_u32_buf(u8 * ptb, int len)
{
	int i = 0;
	u32 *tb = (u32 *) ptb;
	for (i = 0; i < len / 4; i++) {
		PrintMsg("BHT MSG: [%d]:%08xh\n", i, tb[i]);
	}
}

bool gg_emulator_read_ext(struct sdhci_host *host,
													bool * card_status,
													bool * read_status,
													u8 * data,
													u32 datalen)
{
	bool ret = FALSE;
	bool card_ret = TRUE;	
	bool rd_ret = FALSE;

	// twice CMD7 deselect
	ret = (0 == enter_exit_emulator_mode(host, TRUE)) ? TRUE: FALSE;
	if (!ret)
		goto exit;

	// CMD17

	rd_ret = (0 == _gg_emulator_read_only(host, data, datalen)) ? TRUE : FALSE;

	// CMD7 deselect
	ret = (0 == enter_exit_emulator_mode(host, FALSE)) ? TRUE : FALSE;
	if (!ret)
		goto exit;

	// CMD7 select

	card_ret = (0 == gg_select_card_spec(host)) ? TRUE : FALSE;

//BH201LN driver--sunsiyuan@wind-mobi.com modify at 20180326 begin
	if (!rd_ret) {
		DbgErr("BHT ERR:GGC read status error\n");
	}

exit:
	if (!card_ret) {
		DbgErr("BHT ERR:GGC Emulator exit Fail!!\n");
		ret = FALSE;
	}
	
	if(card_status)
			*card_status = ret;

	if(read_status)
			*read_status = rd_ret;
	
	if (rd_ret && !ret) {
		DbgErr("BHT ERR:data read ok, but exit NG\n");
	}	else if (!rd_ret && ret) {
		DbgErr("BHT ERR:data read NG, but exit ok\n");
	}

	return ret;
//BH201LN driver--sunsiyuan@wind-mobi.com modify at 20180326 end
}

static void _status_bit_2_bt(int tar, int* byt, int *bit)
{
    *byt = tar / 8;
    *bit = tar % 8;
}

static u32 _read_status_data_read_register(u8 * cfg, t_gg_reg_strt * bts)
{
    u32 rv = 0;
    u32 msk = bts->mask;
    int  byt = 0, bit = 0;
    int i = 0;
    do
    {
        _status_bit_2_bt(bts->ofs + i, &byt, &bit);
        if(cfg[byt] & (1 << bit))
        {
            rv |= 1 << i;
        }
        //
        i++;
        msk >>= 1;
        if(0 == msk)
            break;

    }
    while(1);
    return rv;
}

bool ggc_read_registers_ext(struct sdhci_host *host,     
														bool * card_status,    
														bool * read_status,
                            t_gg_reg_strt * gg_reg_arr, 
														u8 num)
{
    u8 get_idx = 0;
    bool ret = FALSE;
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
		struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
		struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
		ggc_platform_t *ggc = &vendor_host->ggc;
		
    if(read_status)
        * read_status = FALSE;
    if(card_status)
        * card_status = FALSE;
    // read ggc register
    os_memset(ggc->_cur_read_buf, 0, 512);
    ret = gg_emulator_read_ext(host, card_status, read_status, ggc->_cur_read_buf, 512);
#ifdef  FORCE_READ_STATUS_CMD7_TIMEOUT
	{
		static bool cmd7_timeout_issue_test_flag = TRUE;
		if (cmd7_timeout_issue_test_flag)
		{
			DbgErr("BHT ERR:Force first CMD7 timeout error occur\n");
			if (card_status)
				*card_status = FALSE;
			ret = FALSE;
			cmd7_timeout_issue_test_flag = FALSE;
		}
	}
#endif
    if(read_status == FALSE)
        goto exit;

    // read the offset bits value
    for(get_idx = 0; get_idx < num; get_idx++)
    {
        (gg_reg_arr + get_idx)->value = _read_status_data_read_register(ggc->_cur_read_buf, (gg_reg_arr + get_idx));
    }
exit:
    return ret;
}

bool gg_emulator_read(struct sdhci_host *host, u8 * data, u32 datalen)
{
	bool ret = FALSE;
	bool rd_ret = FALSE;

	// twice CMD7 deselect
	ret = enter_exit_emulator_mode(host, TRUE);
	if (ret)
		goto exit;

	// CMD17

	rd_ret = _gg_emulator_read_only(host, data, datalen);

	// CMD7 deselect
	ret = enter_exit_emulator_mode(host, FALSE);
	if (ret)
		goto exit;

	// CMD7 select

	ret = gg_select_card_spec(host);

exit:
	if (rd_ret) {
		DbgErr("BHT ERR:GGC read status error\n");
	}

	if (ret)
		DbgErr("BHT ERR:GGC Emulator exit Fail!!\n");

	if (0 == rd_ret && ret) {
		DbgErr("BHT ERR:data read ok, but exit NG\n");
		ret = 0;
	}

	if (rd_ret && 0 == ret) {
		DbgErr("BHT ERR:data read NG, but exit ok\n");
		ret = -1;
	}

	return ret ? FALSE : TRUE;
}

static bool _ggc_emulator_write_only(struct sdhci_host *host,
				     u8 * in_data, u32 datalen)
{
	struct mmc_host *mmc = host->mmc;
	int rc = 0;
	u8 *data1 = kzalloc(PAGE_SIZE, GFP_KERNEL);
	struct mmc_request mrq = { 0 };
	struct mmc_command cmd = { 0 };
	struct mmc_data data = { 0 };
	struct scatterlist sg;
#if IFLYTEK
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif

	if (!data1) {
		PrintMsg("BHT MSG:gg write no memory\n");
		rc = -ENOMEM;
		goto out;
	}

	memcpy(data1, in_data, datalen);
	sg_init_one(&sg, data1, 512);

	cmd.opcode = MMC_WRITE_BLOCK;
	cmd.sw_cmd_timeout = 150;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
	data.blksz = 512;
	data.blocks = 1;
	data.flags = MMC_DATA_WRITE;
	data.timeout_ns = 1000 * 1000 * 1000;	/* 1 sec */
	data.sg = &sg;
	data.sg_len = 1;
	mrq.cmd = &cmd;
	mrq.data = &data;
	mrq.stop = NULL;

	mmc_wait_for_req(mmc, &mrq);

	if (cmd.error) {
		if (cmd.err_int_mask & 0xa0000) {

			vendor_host->sdr50_notuning_crc_error_flag = 1;
		}
	}
	if(data1)
		kfree(data1);
out:
	return rc;
}

bool gg_emulator_write(struct sdhci_host * host, u8 * data, u32 datalen)
{
	bool ret = FALSE;
	bool wr_ret = FALSE;

	// twice CMD7 deselect
	ret = enter_exit_emulator_mode(host, TRUE);
	if (ret)
		goto exit;

	if (1)
	{
		u32 i = 0;
		u32 reg;
		PrintMsg("BHT MSG:%s: dump config data\n", __FUNCTION__);
		for (i = 0; i < (datalen/sizeof(u32)); i++)
		{
			memcpy(&reg, data+i*sizeof(u32), sizeof(u32));
			PrintMsg("BHT MSG:\tggc_reg32[%03d]=0x%08x\n", i, reg);
		}
	}
	
	// CMD24 ignore CMD timeout
	_ggc_emulator_write_only(host, data, datalen);
	wr_ret = TRUE;		//force ignore
	// CMD7 deselect
	ret = enter_exit_emulator_mode(host, FALSE);
	if (ret)
		goto exit;

	ret = gg_select_card_spec(host);

exit:
	if (wr_ret == FALSE)
		ret = FALSE;

	if (ret == FALSE)
		DbgErr("BHT ERR:%s: GGC Emulator Write Fail!!\n", __FUNCTION__);

	return ret;
}

#define MAX_GG_REG_NUM 16
u32 gg_sw_def[MAX_GG_REG_NUM] = GGC_CFG_DATA;

void get_gg_reg_def(struct sdhci_host *host, u8 * data)
{
	os_memcpy(data, (u8 *) & (gg_sw_def[0]), sizeof(gg_sw_def));
}

static u32 g_gg_reg_cur[16] = { 0 };
static u32 g_gg_reg_tmp[16] = { 0 };

void set_gg_reg_tmp(u8 * data)
{
	os_memcpy(&g_gg_reg_tmp[0], data, sizeof(g_gg_reg_tmp));
}

void set_gg_reg_cur_val(u8 * data)
{
	os_memcpy(&g_gg_reg_cur[0], data, sizeof(g_gg_reg_cur));
}

void get_gg_reg_cur_val(u8 * data)
{
	os_memcpy(data, &g_gg_reg_cur[0], sizeof(g_gg_reg_cur));
}
#if 0
void bht_gg_status(struct mmc_host *mmc_host)
{
	struct sdhci_host *host = mmc_priv(mmc_host);
	u32 data[128] = { 0 };
	int i = 0;
	os_memset(data, 0, 512);
	gg_emulator_read(host, (u8 *) data, 512);
	for (i = 0; i < 40; i++)
		PrintMsg("BHT MSG:[%d]:%x\n", i, data[i]);

}

EXPORT_SYMBOL(bht_gg_status);
#endif
bool get_gg_reg_cur(struct sdhci_host *host, u8 * data,
		    t_gg_reg_strt * gg_reg_arr, u8 num)
{
	u8 get_idx = 0;
	bool ret = FALSE;

	// read ggc register
	os_memset(data, 0, 512);
	ret = gg_emulator_read(host, data, 512);

	if (ret == FALSE)
		goto exit;

	// read the offset bits value
	for (get_idx = 0; get_idx < num; get_idx++) {
		(gg_reg_arr + get_idx)->value =
		    read_ram_bits_ofs_mask(data, (gg_reg_arr + get_idx));
	}
exit:
	return ret;
}

bool chg_gg_reg(struct sdhci_host * host, u8 * data, t_gg_reg_strt * gg_reg_arr,
		u8 num)
{
	u8 chg_idx = 0;
	os_memset(data, 0, 512);
	get_gg_reg_def(host, data);

	for (chg_idx = 0; chg_idx < num; chg_idx++) {
		// modify the ggc register bit value
		cfg_write_bits_ofs_mask(data, (gg_reg_arr + chg_idx),
					(gg_reg_arr + chg_idx)->value);
	}

	// write ggc register
	return gg_emulator_write(host, data, 512);
}

void chg_gg_reg_cur_val(u8 * data, t_gg_reg_strt * gg_reg_arr, u8 num,
			bool b_sav_chg)
{
	u8 chg_idx = 0;

	for (chg_idx = 0; chg_idx < num; chg_idx++) {
		// modify the ggc register bit value
		cfg_write_bits_ofs_mask(data, (gg_reg_arr + chg_idx),
					(gg_reg_arr + chg_idx)->value);
	}

	if (b_sav_chg)
		set_gg_reg_cur_val(data);
}

void generate_gg_reg_val(u8 * data, t_gg_reg_strt * gg_reg_arr, u8 num)
{
	u8 chg_idx = 0;

	for (chg_idx = 0; chg_idx < num; chg_idx++) {
		// modify the ggc register bit value
		cfg_write_bits_ofs_mask(data, (gg_reg_arr + chg_idx),
					(gg_reg_arr + chg_idx)->value);
	}
}

void chg_gg_reg_tmp(u8 * data, t_gg_reg_strt * gg_reg_arr, u8 num)
{
	u8 chg_idx = 0;
	for (chg_idx = 0; chg_idx < num; chg_idx++) {
		cfg_write_bits_ofs_mask(data, (gg_reg_arr + chg_idx),
					(gg_reg_arr + chg_idx)->value);
	}
	set_gg_reg_tmp(data);
}

bool b_output_tuning;

bool is_output_tuning(void)
{
	return b_output_tuning;
}

void log_bin(u32 n)
{
	int i = 0;
	u8 tb[33] = { 0 };
	for (i = 0; i < 32; i++) {
		if (n & (1 << i)) {
			tb[i] = '1';
		} else
			tb[i] = '0';
	}
	PrintMsg("BHT MSG:bin:%s\n", tb);
}

void phase_str(u8 * tb, u32 n)
{
	int i = 0;

	for (i = 0; i < TUNING_PHASE_SIZE; i++) {
		if (n & (1 << i)) {
			tb[i] = '1';
		} else
			tb[i] = '0';
	}
	tb[TUNING_PHASE_SIZE] = 0;

}

int get_bit_number(u32 n)
{
	int i = 0;
	int cnt = 0;
	for (i = 0; i < TUNING_PHASE_SIZE; i++) {
		if (n & (1 << i)) {
			cnt++;
		}
	}
	return cnt;
}

bool gg_emulator_write_ext(struct sdhci_host * host, bool * card_status, u8 * data, u32 datalen)
{
	bool ret = FALSE;
	bool wr_ret = FALSE;

	// twice CMD7 deselect
	ret = enter_exit_emulator_mode(host, TRUE);
	if (ret)
		goto exit;

	if (0)
	{
		u32 i = 0;
		u32 reg;
		PrintMsg("BHT MSG:%s: dump config data\n", __FUNCTION__);
		for (i = 0; i < (datalen/sizeof(u32)); i++)
		{
			memcpy(&reg, data+i*sizeof(u32), sizeof(u32));
			PrintMsg("BHT MSG:    ggc_reg32[%03d]=0x%08x\n", i, reg);
		}
	}
	
	// CMD24 ignore CMD timeout
	_ggc_emulator_write_only(host, data, datalen);
	wr_ret = TRUE;		//force ignore
	// CMD7 deselect
	ret = enter_exit_emulator_mode(host, FALSE);
	if (ret)
		goto exit;

	ret = (0 == gg_select_card_spec(host)) ? TRUE : FALSE;
	if (ret == FALSE) {
		if (card_status) 
			*card_status = FALSE;
	}
	
exit:
	if (wr_ret == FALSE) {
		ret = FALSE;
	}

	if (ret == FALSE)
		DbgErr("BHT ERR:%s: GGC Emulator Write Fail!!\n", __FUNCTION__);

	return ret;
}

bool ggc_write_registers_ext(struct sdhci_host * host, bool * card_status, t_gg_reg_strt * gg_reg_arr, int num)
{
	u8 data[512];
	get_gg_reg_cur_val(data);
	chg_gg_reg_cur_val(data, gg_reg_arr, (u8)num, TRUE);
	return gg_emulator_write_ext(host, card_status, data, 512);
}

bool ggc_set_output_tuning_phase_ext(struct sdhci_host * host, bool * card_status, int sela, int selb)
{
	bool ret = TRUE;
	u8 data[512] = { 0 };
//BH201LN driver--sunsiyuan@wind-mobi.com modify at 20180326 begin
	t_gg_reg_strt gg_reg_arr[8];
//BH201LN driver--sunsiyuan@wind-mobi.com modify at 20180326 end
	DbgInfo(MODULE_SD_HOST, FEATURE_SDREG_TRACEW, NOT_TO_RAM,
		"%s sela:%xh,selb:%xh\n", __FUNCTION__, sela, selb);

	get_gg_reg_cur_val(data);
	os_memcpy(&gg_reg_arr[0], &dll_sela_100m_cfg, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[1], &dll_sela_200m_cfg, sizeof(t_gg_reg_strt));
	gg_reg_arr[0].value = sela;
	gg_reg_arr[1].value = sela;
	os_memcpy(&gg_reg_arr[2], &dll_selb_100m_cfg, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[3], &dll_selb_200m_cfg, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[4], &dll_selb_100m_cfg_en, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[5], &dll_selb_200m_cfg_en, sizeof(t_gg_reg_strt));
	gg_reg_arr[2].value = selb;
	gg_reg_arr[3].value = selb;
	gg_reg_arr[4].value = 1;
	gg_reg_arr[5].value = 1;
	os_memcpy(&gg_reg_arr[6], &internl_tuning_en_100m,
		  sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[7], &internl_tuning_en_200m,
		  sizeof(t_gg_reg_strt));
	gg_reg_arr[6].value = 1;
	gg_reg_arr[7].value = 1;
	if (card_status)
		*card_status = TRUE;
	chg_gg_reg_cur_val(data, gg_reg_arr, 8, TRUE);
	ret = gg_emulator_write_ext(host, card_status, data, 512);
	return ret;
}

bool ggc_set_output_tuning_phase(struct sdhci_host * host, int sela, int selb)
{
	bool ret = TRUE;
	u8 data[512] = { 0 };

	t_gg_reg_strt gg_reg_arr[8];

	DbgInfo(MODULE_SD_HOST, FEATURE_SDREG_TRACEW, NOT_TO_RAM,
		"%s sela:%xh,selb:%xh\n", __FUNCTION__, sela, selb);

	get_gg_reg_cur_val(data);
	os_memcpy(&gg_reg_arr[0], &dll_sela_100m_cfg, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[1], &dll_sela_200m_cfg, sizeof(t_gg_reg_strt));
	gg_reg_arr[0].value = sela;
	gg_reg_arr[1].value = sela;
	os_memcpy(&gg_reg_arr[2], &dll_selb_100m_cfg, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[3], &dll_selb_200m_cfg, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[4], &dll_selb_100m_cfg_en, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[5], &dll_selb_200m_cfg_en, sizeof(t_gg_reg_strt));
	gg_reg_arr[2].value = selb;
	gg_reg_arr[3].value = selb;
	gg_reg_arr[4].value = 1;
	gg_reg_arr[5].value = 1;
	os_memcpy(&gg_reg_arr[6], &internl_tuning_en_100m,
		  sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[7], &internl_tuning_en_200m,
		  sizeof(t_gg_reg_strt));
	gg_reg_arr[6].value = 1;
	gg_reg_arr[7].value = 1;
	chg_gg_reg_cur_val(data, gg_reg_arr, 8, TRUE);
	ret = gg_emulator_write(host, data, 512);
	return ret;
}

bool gg_fix_output_tuning_phase(struct sdhci_host * host, int sela, int selb)
{
	u8 data[512] = { 0 };
	t_gg_reg_strt gg_reg_arr[10];

	PrintMsg("BHT MSG:### %s - sela dll: %x, selb dll: %x \n", __FUNCTION__, sela,
		 selb);

	get_gg_reg_cur_val(data);

	os_memcpy(&gg_reg_arr[0], &dll_sela_100m_cfg, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[1], &dll_sela_200m_cfg, sizeof(t_gg_reg_strt));
	gg_reg_arr[0].value = sela;
	gg_reg_arr[1].value = sela;
	os_memcpy(&gg_reg_arr[2], &dll_selb_100m_cfg, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[3], &dll_selb_200m_cfg, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[4], &dll_selb_100m_cfg_en, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[5], &dll_selb_200m_cfg_en, sizeof(t_gg_reg_strt));
	gg_reg_arr[2].value = selb;
	gg_reg_arr[3].value = selb;
	gg_reg_arr[4].value = 1;
	gg_reg_arr[5].value = 1;
	os_memcpy(&gg_reg_arr[6], &internl_tuning_en_100m,
		  sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[7], &internl_tuning_en_200m,
		  sizeof(t_gg_reg_strt));
	gg_reg_arr[6].value = 0;
	gg_reg_arr[7].value = 0;

	chg_gg_reg_cur_val(data, gg_reg_arr, 8, TRUE);

	return gg_emulator_write(host, data, 512);
}

// generate 11 bit data
void gen_array_data(u32 low32, u32 high32, u32 * ptw)
{

#define MAX_TUNING_RESULT_COMBO_SIZE (6)
	u8 tu_res_per[MAX_TUNING_RESULT_COMBO_SIZE][TUNING_PHASE_SIZE];
	u8 i = 0, j = 0;
	u8 i_mode = 0;
	u32 tw = 0;

	os_memset(tu_res_per, 1, sizeof(tu_res_per));
	for (i = 0; i < 64; i++) {
		u32 tmp_data = (i < 32) ? low32 : high32;
		tu_res_per[i / TUNING_PHASE_SIZE][i % TUNING_PHASE_SIZE] =
		    (tmp_data & (1 << (i % 32))) >> (i % 32);
	}
//
	for (i_mode = 0; i_mode < TUNING_PHASE_SIZE; i_mode++) {
		for (j = 0; j < MAX_TUNING_RESULT_COMBO_SIZE; j++) {
			if (tu_res_per[j][i_mode] == 0) {
				tw &= ~(1 << i_mode);
				break;
			} else
				tw |= (1 << i_mode);
		}
	}
	if (ptw)
		*ptw = tw;
}

bool sw_calc_tuning_result(struct sdhci_host *host, u32 * tx_selb,
			   u32 * all_selb, u64 * raw_tx_selb)
{
	bool ret = FALSE;
	u8 data[512] = { 0 };
	u32 selb_status_tx_low32 = 0, selb_status_tx_high32 = 0;
	u32 selb_status_ggc_low32 = 0, selb_status_ggc_high32 = 0;
	t_gg_reg_strt gg_reg_arr[6];

	os_memcpy(&gg_reg_arr[0], &pha_stas_tx_low32, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[1], &pha_stas_tx_high32, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[2], &pha_stas_rx_low32, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[3], &pha_stas_rx_high32, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[4], &dll_sela_after_mask, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[5], &dll_selb_after_mask, sizeof(t_gg_reg_strt));

	ret = get_gg_reg_cur(host, data, gg_reg_arr, 6);

	if ((TRUE == ret)) {
		//
		selb_status_tx_low32 = gg_reg_arr[0].value;
		PrintMsg("BHT MSG:[205-236]:\n");
		log_bin(selb_status_tx_low32);
		PrintMsg("BHT MSG:[237-268]:\n");
		selb_status_tx_high32 = gg_reg_arr[1].value;
		log_bin(selb_status_tx_high32);

		PrintMsg("BHT MSG:[14-45]:\n");
		selb_status_ggc_low32 = gg_reg_arr[2].value;
		log_bin(selb_status_ggc_low32);
		PrintMsg("BHT MSG:[46-77]:\n");
		selb_status_ggc_high32 = gg_reg_arr[3].value;
		log_bin(selb_status_ggc_high32);
		PrintMsg("BHT MSG:dll sela after mask=%xh\n", gg_reg_arr[4].value);
		PrintMsg("BHT MSG:dll selb after mask=%xh\n", gg_reg_arr[5].value);

		if (raw_tx_selb) {
			*raw_tx_selb = gg_reg_arr[1].value;
			(*raw_tx_selb) <<= 32;
			*raw_tx_selb += gg_reg_arr[0].value;
			PrintMsg("BHT MSG:raw_tx_selb:%llxh\n", *raw_tx_selb);
		}

		if (tx_selb) {
			gen_array_data(gg_reg_arr[0].value, gg_reg_arr[1].value,
				       tx_selb);
			PrintMsg("BHT MSG:tx_selb:%xh\n", *tx_selb);
		}
		if (all_selb) {
			gen_array_data(gg_reg_arr[2].value, gg_reg_arr[3].value,
				       all_selb);
			PrintMsg("BHT MSG:all_selb:%xh\n", *all_selb);
		}
	}

	return ret;
}

bool gg_tuning_result(struct sdhci_host * host, u32 * tx_selb, u32 * all_selb,
		      u64 * raw_tx_selb)
{

	host_cmddat_line_reset(host);
	return sw_calc_tuning_result(host, tx_selb, all_selb, raw_tx_selb);
}


//static u32 g_tx_selb_failed_history_sdr104 = BIT_PASS_MASK;
//static u32 g_tx_selb_failed_history_sdr50 = BIT_PASS_MASK;
//static u32 g_sw_selb_sd104_fail_phase = 0;
//static u32 g_sw_selb_sd50_fail_phase = 0;
//static u32 g_sw_selb_tuning_first_selb = 0;

u64 GENERATE_64_IDX_VALUE(int sft)
{
    u64 val = 1;
    return val << sft;
}

bool is_bus_mode_sdr104(struct sdhci_host * host)
{
	bool ret = FALSE;
	if (host->timing == MMC_TIMING_UHS_SDR104)
		ret = TRUE;

	return ret;
}

bool _check_bus_mode(struct sdhci_host * host) 
{
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
	ggc_platform_t *ggc = &vendor_host->ggc;
	
	if (is_bus_mode_sdr104(host)) {
		ggc->cur_bus_mode = &vendor_host->ggc.sdr104;
	} else {
		ggc->cur_bus_mode = &vendor_host->ggc.sdr50;
	}
	return true;
}

void tx_selb_failed_history_update(struct sdhci_host *host, u32 val)
{
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
	
	_check_bus_mode(host);
	
	vendor_host->ggc.cur_bus_mode->tx_selb_failed_history &= val;
}

void tx_selb_failed_history_reset(struct sdhci_host *host)
{
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
	
	vendor_host->ggc.sdr50.tx_selb_failed_history = BIT_PASS_MASK;
	vendor_host->ggc.sdr104.tx_selb_failed_history = BIT_PASS_MASK;
}

u32 tx_selb_failed_history_get(struct sdhci_host *host)
{
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
	
	_check_bus_mode(host);
	
	return vendor_host->ggc.cur_bus_mode->tx_selb_failed_history;
}

u32 g_tx_selb_tb_sdr104[TUNING_PHASE_SIZE] = {
	BIT_PASS_MASK,
	BIT_PASS_MASK, BIT_PASS_MASK, BIT_PASS_MASK,
	BIT_PASS_MASK, BIT_PASS_MASK, BIT_PASS_MASK,
	BIT_PASS_MASK, BIT_PASS_MASK, BIT_PASS_MASK,
	BIT_PASS_MASK
};

u32 g_tx_selb_tb_sdr50[TUNING_PHASE_SIZE] = {
	BIT_PASS_MASK,
	BIT_PASS_MASK, BIT_PASS_MASK, BIT_PASS_MASK,
	BIT_PASS_MASK, BIT_PASS_MASK, BIT_PASS_MASK,
	BIT_PASS_MASK, BIT_PASS_MASK, BIT_PASS_MASK,
	BIT_PASS_MASK
};

void tx_selb_failed_tb_update(struct sdhci_host *host, int sela, u32 val)
{
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
	
	_check_bus_mode(host);
	vendor_host->ggc.cur_bus_mode->tx_selb_tb[sela] &= val;
	DbgInfo(MODULE_SD_HOST, FEATURE_VENREG_TRACEW, NOT_TO_RAM, "%s %xh %xh\n", 
			__FUNCTION__, val, vendor_host->ggc.cur_bus_mode->tx_selb_tb[sela]);
}

void tx_selb_failed_tb_reset(struct sdhci_host *host)
{
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
	
	os_memset(&vendor_host->ggc.sdr104.tx_selb_tb, 0xff, sizeof(vendor_host->ggc.sdr104.tx_selb_tb));
	os_memset(&vendor_host->ggc.sdr50.tx_selb_tb, 0xff, sizeof(vendor_host->ggc.sdr50.tx_selb_tb));
}

u32 tx_selb_failed_tb_get(struct sdhci_host *host, int sela)
{
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
	_check_bus_mode(host);
	
	if (is_bus_mode_sdr104(host)) {
		DbgInfo(MODULE_SD_HOST, FEATURE_VENREG_TRACEW, NOT_TO_RAM,
			"%s %xh\n", __FUNCTION__, g_tx_selb_tb_sdr104[sela]);
		return vendor_host->ggc.sdr104.tx_selb_tb[sela];
	} else {
		DbgInfo(MODULE_SD_HOST, FEATURE_VENREG_TRACEW, NOT_TO_RAM,
			"%s %xh\n", __FUNCTION__, g_tx_selb_tb_sdr50[sela]);
		return vendor_host->ggc.sdr50.tx_selb_tb[sela];
	}
}

void all_selb_failed_tb_update(struct sdhci_host *host, int sela, u32 val)
{
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
	
	_check_bus_mode(host);
	vendor_host->ggc.cur_bus_mode->all_selb_tb[sela] &= val;
	DbgInfo(MODULE_SD_HOST, FEATURE_VENREG_TRACEW, NOT_TO_RAM,
			"%s %xh %xh\n", __FUNCTION__, val,
			vendor_host->ggc.cur_bus_mode->all_selb_tb[sela]);
}

void all_selb_failed_tb_reset(struct sdhci_host *host)
{
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
	
	os_memset(vendor_host->ggc.sdr104.all_selb_tb, 0xff, sizeof(vendor_host->ggc.sdr104.all_selb_tb));
	os_memset(vendor_host->ggc.sdr50.all_selb_tb, 0xff, sizeof(vendor_host->ggc.sdr50.all_selb_tb));
}

u32 all_selb_failed_tb_get(struct sdhci_host *host, int sela)
{
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
	u32 val;
	_check_bus_mode(host);

	val = vendor_host->ggc.cur_bus_mode->all_selb_tb[sela];
	DbgInfo(MODULE_SD_HOST, FEATURE_VENREG_TRACEW, NOT_TO_RAM,
			"%s %xh\n", __FUNCTION__, val);

	return val;
}

void chk_phase_window(u8 * tuning_win, u8 * mid_val, u8 * max_pass_win)	// type[0] = 0: if 011110, right 1 index is the middle value, type[1] = 1: first_i valid
{
	u8 tuning_pass[TUNING_PHASE_SIZE + 32];
	u8 tuning_pass_start[TUNING_PHASE_SIZE + 32];
	u8 tuning_pass_num_max = 0;
	u8 first_0 = 0;
	u8 i = 0, j = 0;
	u8 i_mode = 0, selb_mode = 0;

	os_memset(tuning_pass, 1, sizeof(tuning_pass));
	os_memset(tuning_pass_start, 1, sizeof(tuning_pass_start));

	{
		for (i = 0; i < TUNING_PHASE_SIZE; i++) {
			if (tuning_win[i] == 0) {
				first_0 = i;
				break;
			}
		}
	}

	for (i = 0; i < TUNING_PHASE_SIZE; i++) {
		i_mode = (first_0 + i) % TUNING_PHASE_SIZE;
		if (tuning_win[i_mode] == 1)
			tuning_pass[j]++;
		else if (tuning_pass[j])
			j++;
		if (tuning_pass[j] == 1)
			tuning_pass_start[j] = i_mode;
	}

	for (i = 0; i < TUNING_PHASE_SIZE; i++) {
		if (tuning_pass_num_max < tuning_pass[i]) {
			tuning_pass_num_max = tuning_pass[i];
			i_mode = i;
		}
	}

	if (tuning_pass_num_max == 0)
		DbgErr
		    ("###### Get max pass window fail, there is no any pass phase!!\n");
	else {
		*max_pass_win = tuning_pass_num_max - 1;
		tuning_pass_num_max /= 2;
		selb_mode = tuning_pass_start[i_mode] + tuning_pass_num_max;
		if ((*max_pass_win % 2 == 0))
			selb_mode += 1;
		selb_mode %= TUNING_PHASE_SIZE;
	}

	*mid_val = selb_mode;

	return;
}

void dump_array(u8 * tb)
{
	int i = 0;
	u8 str[12] = { 0 };

	for (i = 0; i < TUNING_PHASE_SIZE; i++) {
		str[i] = tb[i] + '0';

	}
	DbgInfo(MODULE_SD_HOST, FEATURE_VENREG_TRACEW, NOT_TO_RAM, "%s\n", str);
}

void bits_generate_array(u8 * tb, u32 v)
{
	int i = 0;
	DbgInfo(MODULE_SD_HOST, FEATURE_VENREG_TRACEW, NOT_TO_RAM, "%s %xh\n",
		__FUNCTION__, v);

	for (i = 0; i < TUNING_PHASE_SIZE; i++) {
		if ((v & (1 << i))) {
			tb[i] = 1;
		} else
			tb[i] = 0;
	}
	dump_array(tb);
}

typedef struct {
	u8 right_valid:1;
	u8 first_valid:1;
	u8 record_valid:1;
	u8 reserved:5;
} chk_type_t;

// normal check continue 1
void chk_arr_max_win(u8 * tuning_win, u8 first_i, u8 * mid_val, u8 * first_val, u8 * max_pass_win, chk_type_t type)	// type[0] = 0: if 011110, right 1 index is the middle value, type[1] = 1: first_i valid
{
	u8 tuning_pass[TUNING_PHASE_SIZE];
	u8 tuning_pass_start[TUNING_PHASE_SIZE];
	u8 tuning_pass_num_max = 0;
	u8 first_0 = 0;
	u8 i = 0, j = 0;
	u8 i_mode = 0, selb_mode = 0;

	os_memset(tuning_pass, 1, sizeof(tuning_pass));
	os_memset(tuning_pass_start, 1, sizeof(tuning_pass_start));

	if (type.first_valid)
		first_0 = first_i;
	else {
		for (i = 0; i < TUNING_PHASE_SIZE; i++) {
			if (tuning_win[i] == 0) {
				first_0 = i;
				break;
			}
		}
	}

	for (i = 0; i < TUNING_PHASE_SIZE; i++) {
		i_mode = (first_0 + i) % TUNING_PHASE_SIZE;
		if (tuning_win[i_mode] == 1)
			tuning_pass[j]++;
		else if (tuning_pass[j])
			j++;
		if (tuning_pass[j] == 1)
			tuning_pass_start[j] = i_mode;
	}

	for (i = 0; i < TUNING_PHASE_SIZE; i++) {
		if (tuning_pass_num_max < tuning_pass[i]) {
			tuning_pass_num_max = tuning_pass[i];
			i_mode = i;
		}
	}

	if (tuning_pass_num_max == 0)
		DbgErr
		    ("###### Get max pass window fail, there is no any pass phase!!\n");
	else {
		*max_pass_win = tuning_pass_num_max - 1;
		tuning_pass_num_max /= 2;
		if (first_val)
			*first_val = tuning_pass_start[i_mode];
		selb_mode = tuning_pass_start[i_mode] + tuning_pass_num_max;
		if ((*max_pass_win % 2 == 0) && (type.right_valid)
		    )
			selb_mode += 1;
		selb_mode %= TUNING_PHASE_SIZE;
	}

	*mid_val = selb_mode;
	return;
}

// 0 no fail point
void no_fail_p(u8 * tuning_win, u8 * mid_val, u8 * max_pass_win, u8 * first_val)
{
	chk_type_t type;
	u8 first_0 = 0;

	os_memset((u8 *) & type, 0, sizeof(chk_type_t));

	type.first_valid = 0;
	type.right_valid = 1;
	type.record_valid = 0;

	chk_arr_max_win(tuning_win, first_0, mid_val, first_val, max_pass_win,
			type);

}

static int ggc_get_selx_weight(u32 val)
{
	int i = 0;
	int cnt = 0;
	for (i = 0; i < TUNING_PHASE_SIZE; i++) {
		if (GET_IDX_VALUE(val, i)) {
			cnt++;
		}
	}
	return cnt;
}

void tx_selb_calculate_valid_phase_range(u32 val, int *start,
						int *pass_cnt)
{
//only support one failure range case, because the failure point only one

	int i = 0, flg = 0;
	*pass_cnt = ggc_get_selx_weight(val);
	for (i = 0; i < (TUNING_PHASE_SIZE * 2); i++) {
		if ((0 == GET_TRUNING_RING_IDX_VALUE(val, i)) && (0 == flg))
			flg = 1;
		if ((1 == flg) && GET_TRUNING_RING_IDX_VALUE(val, i)) {
			*start = TRUNING_RING_IDX(i);
			break;
		}
	}
}

bool ggc_update_default_selb_phase_tuning_cnt(struct sdhci_host *host, int selb,
					      int tuning_cnt)
{
	//bool ret = TRUE;
	t_gg_reg_strt gg_reg_arr[3];
	u8 data[512];

	get_gg_reg_cur_val(data);

	PrintMsg("BHT MSG:%s selb:%xh,tuning_cnt:%xh\n", __FUNCTION__, selb,
		 tuning_cnt);
	os_memcpy(&gg_reg_arr[0], &dll_selb_100m_cfg, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[1], &dll_selb_200m_cfg, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[2], &cmd19_cnt_cfg, sizeof(t_gg_reg_strt));

	gg_reg_arr[0].value = selb;
	gg_reg_arr[1].value = selb;
	gg_reg_arr[2].value = tuning_cnt;
	chg_gg_reg_cur_val(data, gg_reg_arr, 3, TRUE);

	return TRUE;
}

static void _ggc_update_cur_setting_for_sw_selb_tuning(struct sdhci_host *host,
						       u32 val)
{
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
	int start = 0, pass_cnt = 0;
//1. calculate tx_selb start phase

	tx_selb_calculate_valid_phase_range(val, &start, &pass_cnt);
	PrintMsg("BHT MSG:%s %x %x %x\n", __FUNCTION__, val, start, pass_cnt);
	ggc_update_default_selb_phase_tuning_cnt(host, start, pass_cnt);	//update
	vendor_host->ggc.ggc_sw_selb_tuning_first_selb = start;
}

#if IFLYTEK
int sdhci_bht_sdr104_execute_tuning(struct sdhci_host *host, u32 opcode)
{
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);

	unsigned long flags;
	int err = 0;
	int i = 0;
	bool value, first_vl, prev_vl = 0;
	int *value_t;
	struct ranges_t *ranges;

	int length = 0;
	unsigned int range_count = 0;
	int longest_range_len = 0;
	int longest_range = 0;
	int mid_step;
	int final_phase = 0;
	u32 dll_cfg = 0;
	u32 mid_dll_cnt = 0;
	u32 dll_cnt = 0;
	u32 dll_dly = 0;

	sprd_sdhc_runtime_pm_get(vendor_host);
	spin_lock_irqsave(&vendor_host->lock, flags);

	sprd_sdhc_reset(vendor_host, SPRD_SDHC_BIT_RST_CMD | SPRD_SDHC_BIT_RST_DAT);

	dll_cfg = sprd_sdhc_readl(vendor_host, SPRD_SDHC_REG_32_DLL_CFG);
	dll_cfg &= ~(0xf << 24);
	sprd_sdhc_writel(vendor_host, dll_cfg, SPRD_SDHC_REG_32_DLL_CFG);
	dll_cnt = sprd_sdhc_readl(vendor_host, SPRD_SDHC_REG_32_DLL_STS0) & 0xff;
	dll_cnt = dll_cnt << 1;
	length = (dll_cnt * 150) / 100;
	pr_info("%s(%s): dll config 0x%08x, dll count %d, tuning length: %d\n",
		__func__, vendor_host->device_name, dll_cfg, dll_cnt, length);

	mmiowb();
	spin_unlock_irqrestore(&vendor_host->lock, flags);

	ranges = kmalloc_array(length + 1, sizeof(*ranges), GFP_KERNEL);
	if (!ranges)
		return -ENOMEM;
	value_t = kmalloc_array(length + 1, sizeof(*value_t), GFP_KERNEL);
	if (!value_t) {
		kfree(ranges);
		return -ENOMEM;
	}

	spin_lock_irqsave(&vendor_host->lock, flags);
	dll_dly = vendor_host->dll_dly;
	do {
		if (vendor_host->flags & SPRD_HS400_TUNING) {
			dll_dly &= ~SPRD_CMD_DLY_MASK;
			dll_dly |= (i << 8);
		} else {
			dll_dly &= ~(SPRD_CMD_DLY_MASK | SPRD_POSRD_DLY_MASK);
			dll_dly |= (((i << 8) & SPRD_CMD_DLY_MASK) |
				((i << 16) & SPRD_POSRD_DLY_MASK));
		}
		sprd_sdhc_writel(vendor_host, dll_dly, SPRD_SDHC_REG_32_DLL_DLY);

		mmiowb();
		spin_unlock_irqrestore(&vendor_host->lock, flags);
		value = !mmc_send_tuning(mmc, opcode, NULL);
		//bug 708366,huangxiaotian,modify for system dump when doing android reboot test,2022/01/28
		usleep_range(1000, 1200);//delay 1ms for CMD19
		spin_lock_irqsave(&vendor_host->lock, flags);

		if ((!prev_vl) && value) {
			range_count++;
			ranges[range_count - 1].start = i;
		}
		if (value) {
			pr_debug("%s tuning ok: %d\n", vendor_host->device_name, i);
			ranges[range_count - 1].end = i;
			value_t[i] = value;
		} else {
			pr_debug("%s tuning fail: %d\n", vendor_host->device_name, i);
			value_t[i] = value;
		}

		prev_vl = value;
	} while (++i <= length);

	mid_dll_cnt = length - dll_cnt;
	vendor_host->dll_cnt = dll_cnt;
	vendor_host->mid_dll_cnt = mid_dll_cnt;
	vendor_host->ranges = ranges;

	first_vl = (value_t[0] && value_t[dll_cnt]);
	range_count = sprd_calc_tuning_range(vendor_host, value_t);

	if (range_count == 0) {
		pr_warn("%s(%s): all tuning phases fail!\n",
			__func__, vendor_host->device_name);
		err = -EIO;
		goto out;
	}

	if ((range_count > 1) && first_vl && value) {
		ranges[0].start = ranges[range_count - 1].start;
		range_count--;

		if (ranges[0].end >= mid_dll_cnt)
			ranges[0].end = mid_dll_cnt;
	}

	for (i = 0; i < range_count; i++) {
		int len = (ranges[i].end - ranges[i].start + 1);

		if (len < 0)
			len += dll_cnt;

		pr_info("%s(%s): good tuning phase range %d ~ %d\n",
			__func__, vendor_host->device_name,
			ranges[i].start, ranges[i].end);

		if (longest_range_len < len) {
			longest_range_len = len;
			longest_range = i;
		}

	}
	pr_info("%s(%s): the best tuning step range %d-%d(the length is %d)\n",
		__func__, vendor_host->device_name, ranges[longest_range].start,
		ranges[longest_range].end, longest_range_len);

	mid_step = ranges[longest_range].start + longest_range_len / 2;
	mid_step %= dll_cnt;

	dll_cfg |= 0xf << 24;
	sprd_sdhc_writel(vendor_host, dll_cfg, SPRD_SDHC_REG_32_DLL_CFG);

	if (mid_step <= dll_cnt)
		final_phase = (mid_step * 256) / dll_cnt;
	else
		final_phase = 0xff;

	if (vendor_host->flags & SPRD_HS400_TUNING) {
		vendor_host->timing_dly->hs400_dly &= ~SPRD_CMD_DLY_MASK;
		vendor_host->timing_dly->hs400_dly |= (final_phase << 8);
		vendor_host->dll_dly = vendor_host->timing_dly->hs400_dly;
	} else {
		vendor_host->dll_dly &= ~(SPRD_CMD_DLY_MASK | SPRD_POSRD_DLY_MASK);
		vendor_host->dll_dly |= (((final_phase << 8) & SPRD_CMD_DLY_MASK) |
				((final_phase << 16) & SPRD_POSRD_DLY_MASK));
	}

	pr_info("%s(%s): the best step %d, phase 0x%02x, delay value 0x%08x\n",
		__func__, vendor_host->device_name, mid_step,
		final_phase, vendor_host->dll_dly);
	sprd_sdhc_writel(vendor_host, vendor_host->dll_dly, SPRD_SDHC_REG_32_DLL_DLY);
	err = 0;

out:
	vendor_host->flags &= ~SPRD_HS400_TUNING;
	mmiowb();
	spin_unlock_irqrestore(&vendor_host->lock, flags);
	kfree(ranges);
	kfree(value_t);
	sprd_sdhc_runtime_pm_put(vendor_host);

	return err;
}
#else
int sdhci_bht_sdr104_execute_tuning(struct sdhci_host *host, u32 opcode)
{
	unsigned long flags;
	int tuning_seq_cnt = 3;
	u8 phase, *data_buf, tuned_phases[NUM_TUNING_PHASES], tuned_phase_cnt;
	const u32 *tuning_block_pattern = tuning_block_64;
	size_t size = sizeof(tuning_block_64);	/* Tuning pattern size in bytes */
	int rc;
	struct mmc_host *mmc = host->mmc;
	struct mmc_ios ios = host->mmc->ios;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
	u8 drv_type = 0;
	bool drv_type_changed = false;
	struct mmc_card *card = host->mmc->card;
	/*
	 * Tuning is required for SDR104, HS200 and HS400 cards and
	 * if clock frequency is greater than 100MHz in these modes.
	 */
	if (host->clock <= CORE_FREQ_100MHZ ||
	    !((ios.timing == MMC_TIMING_MMC_HS400) ||
	      (ios.timing == MMC_TIMING_MMC_HS200) ||
	      (ios.timing == MMC_TIMING_UHS_SDR104))) {

		return 0;
	}

	pr_debug("%s: Enter %s\n", mmc_hostname(mmc), __func__);

	/* CDC/SDC4 DLL HW calibration is only required for HS400 mode */
	if (vendor_host->tuning_done && !vendor_host->calibration_done &&
	    (mmc->ios.timing == MMC_TIMING_MMC_HS400)) {
		rc = sdhci_msm_hs400_dll_calibration(host);
		spin_lock_irqsave(&host->lock, flags);
		if (!rc)
			vendor_host->calibration_done = true;
		spin_unlock_irqrestore(&host->lock, flags);
		goto out;
	}

	spin_lock_irqsave(&host->lock, flags);

	if ((opcode == MMC_SEND_TUNING_BLOCK_HS200) &&
	    (mmc->ios.bus_width == MMC_BUS_WIDTH_8)) {
		tuning_block_pattern = tuning_block_128;
		size = sizeof(tuning_block_128);
	}
	spin_unlock_irqrestore(&host->lock, flags);

	data_buf = kmalloc(size, GFP_KERNEL);
	if (!data_buf) {
		rc = -ENOMEM;
		goto out;
	}

retry:
	tuned_phase_cnt = 0;

	/* first of all reset the tuning block */
	rc = msm_init_cm_dll(host , DLL_INIT_NORMAL);
	if (rc)
		goto kfree;

	phase = 0;
	do {
		struct mmc_command cmd = { 0 };
		struct mmc_data data = { 0 };
		struct mmc_request mrq = {
			.cmd = &cmd,
			.data = &data
		};
		struct scatterlist sg;
		rc = msm_config_cm_dll_phase(host, phase);
		if (rc)
			goto kfree;

		cmd.opcode = opcode;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

		data.blksz = size;
		data.blocks = 1;
		data.flags = MMC_DATA_READ;
		data.timeout_ns = 30 * 1000 * 1000;	/* 30 ms */

		data.sg = &sg;
		data.sg_len = 1;
		sg_init_one(&sg, data_buf, size);
		memset(data_buf, 0, size);
		mmc_wait_for_req(mmc, &mrq);
		usleep_range(1000, 1200);//delay 1ms for CMD19

		if (!cmd.error && !data.error &&
		    !memcmp(data_buf, tuning_block_pattern, size)) {
			/* tuning is successful at this tuning point */
			tuned_phases[tuned_phase_cnt++] = phase;
			pr_debug("%s: %s: found *** good *** phase = %d\n",
				 mmc_hostname(mmc), __func__, phase);
			rc = 0;
		} else {
			pr_debug("%s: %s: found ## bad ## phase = %d, cmd.error=%d, data.error=%d\n",
				 mmc_hostname(mmc), __func__, phase, cmd.error, data.error);
			host_cmddat_line_reset(host);//timeout case do reset
			if (cmd.error == -ETIMEDOUT && phase == 0) {
				DbgErr("BHT ERR:cmd19 timeout\n");
				rc = -ETIMEDOUT;
				goto kfree;
			}
		}
	} while (++phase < 16);

	if ((tuned_phase_cnt == NUM_TUNING_PHASES) &&
	    card && mmc_card_mmc(card)) {
		/*
		 * If all phases pass then its a problem. So change the card's
		 * drive type to a different value, if supported and repeat
		 * tuning until at least one phase fails. Then set the original
		 * drive type back.
		 *
		 * If all the phases still pass after trying all possible
		 * drive types, then one of those 16 phases will be picked.
		 * This is no different from what was going on before the
		 * modification to change drive type and retune.
		 */
		pr_debug("%s: tuned phases count: %d\n", mmc_hostname(mmc),
			 tuned_phase_cnt);

		/* set drive type to other value . default setting is 0x0 */
		while (++drv_type <= MAX_DRV_TYPES_SUPPORTED_HS200) {
			pr_debug("%s: trying different drive strength (%d)\n",
				 mmc_hostname(mmc), drv_type);
			if (card->ext_csd.raw_driver_strength & (1 << drv_type)) {
				sdhci_msm_set_mmc_drv_type(host, opcode,
							   drv_type);
				if (!drv_type_changed)
					drv_type_changed = true;
				goto retry;
			}
		}
	}

	/* reset drive type to default (50 ohm) if changed */
	if (drv_type_changed)
		sdhci_msm_set_mmc_drv_type(host, opcode, 0);

	if (tuned_phase_cnt) {
		rc = msm_find_most_appropriate_phase(host, tuned_phases,
						     tuned_phase_cnt);
		if (rc < 0)
			goto kfree;
		else
			phase = (u8) rc;

		/*
		 * Finally set the selected phase in delay
		 * line hw block.
		 */
		rc = msm_config_cm_dll_phase(host, phase);
		if (rc)
			goto kfree;
		vendor_host->saved_tuning_phase = phase;
		pr_debug("%s: %s: finally setting the tuning phase to %d\n",
			 mmc_hostname(mmc), __func__, phase);
	} else {
		if (--tuning_seq_cnt)
			goto retry;
		/* tuning failed */
		pr_err("BHT ERR:%s: %s: no tuning point found\n",
		       mmc_hostname(mmc), __func__);
		rc = -EIO;
	}

kfree:
	kfree(data_buf);
out:
	spin_lock_irqsave(&host->lock, flags);
	if (!rc)
		vendor_host->tuning_done = true;
	spin_unlock_irqrestore(&host->lock, flags);

	pr_debug("%s: Exit %s, err(%d)\n", mmc_hostname(mmc), __func__, rc);
	return rc;
}
#endif
int sdhci_bht_sdr50_execute_tuning(struct sdhci_host *host, u32 opcode)
{

	u8 phase, *data_buf;
	int size = sizeof(tuning_block_64);	/* Tuning pattern size in bytes */
	int rc = 0;
	struct mmc_host *mmc = host->mmc;
#if IFLYTEK
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif

	pr_debug("%s: Enter %s\n", mmc_hostname(mmc), __func__);

	data_buf = kmalloc(size, GFP_KERNEL);
	if (!data_buf) {
		PrintMsg("BHT MSG:tuning no memory\n");
		rc = -ENOMEM;
		goto out;
	}

	phase = 0;
	do {
		struct mmc_command cmd = { 0 };
		struct mmc_data data = { 0 };
		struct mmc_request mrq = {
			.cmd = &cmd,
			.data = &data
		};
		struct scatterlist sg;

		cmd.opcode = opcode;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

		data.blksz = size;
		data.blocks = 1;
		data.flags = MMC_DATA_READ;
		data.timeout_ns = 30 * 1000 * 1000;	/* 30ms */

		data.sg = &sg;
		data.sg_len = 1;
		sg_init_one(&sg, data_buf, size);
		memset(data_buf, 0, size);
		host_cmddat_line_reset(host);
		mmc_wait_for_req(mmc, &mrq);

		if (cmd.error) {
			if (cmd.err_int_mask & 0xa0000) {
				//pr_err("BHT ERR:sdr50_notuning_crc_error_flag=1\n");
				vendor_host->sdr50_notuning_crc_error_flag = 1;
			}
			if (cmd.error == -ETIMEDOUT && phase == 0) {
				DbgErr("BHT ERR:cmd19 timeout\n");
				rc = -ETIMEDOUT;
				goto kfree;
			}
		}

		if (data.error) {
			if (data.err_int_mask & 0x200000) {
				//pr_err("BHT ERR:sdr50_notuning_crc_error_flag=1\n");
				vendor_host->sdr50_notuning_crc_error_flag = 1;
			}
		}

	} while (++phase < 16);

kfree:
	kfree(data_buf);
out:

	return rc;
}

int sd_tuning_sw(struct sdhci_host *host)
{
	int ret = 0;

	if (is_bus_mode_sdr104(host)) {

		ret = sdhci_bht_sdr104_execute_tuning(host, 0x13);
	} else {
		ret = sdhci_bht_sdr50_execute_tuning(host, 0x13);
	}
	return ret;
}

#define SD_TUNING_TIMEOUT_MS 150
bool sd_gg_tuning_status(struct sdhci_host * host,
			 u32 * tx_selb, u32 * all_selb, u64 * raw_tx_selb,
			 bool * status_ret, bool *first_cmd19_status)
{
	bool ret = TRUE;
	
	
	{

		int err = sd_tuning_sw(host);
		ret = err == 0 ? TRUE: FALSE;
		if (err == -ETIMEDOUT) {
			ret = FALSE;
			if (first_cmd19_status) 
				*first_cmd19_status = false;
			goto exit;
		}
		{
			if (status_ret) {
				*status_ret =
				    gg_tuning_result(host, tx_selb, all_selb,
						     raw_tx_selb);
			} else {
				gg_tuning_result(host, 0, 0, 0);
			}
		}
	}
exit:
	DbgInfo(MODULE_SD_CARD, FEATURE_CARD_INIT, NOT_TO_RAM, "Exit(%d) %s\n",
		ret, __FUNCTION__);
	return ret;
}

bool ggc_sd_tuning(struct sdhci_host * host,
			 bool *first_cmd19_status)
{
	bool ret = TRUE;
	
	
	{

		int err = sd_tuning_sw(host);
		ret = err == 0 ? TRUE: FALSE;
		if (err == -ETIMEDOUT) {
			ret = FALSE;
			if (first_cmd19_status) 
				*first_cmd19_status = false;
			goto exit;
		}
	}
exit:
	DbgInfo(MODULE_SD_CARD, FEATURE_CARD_INIT, NOT_TO_RAM, "Exit(%d) %s\n",
					ret, __FUNCTION__);
	return ret;
}

int g_def_sela_100m = 0, g_def_sela_200m = 0;
int g_def_selb_100m = 0, g_def_selb_200m = 0;
u32 g_def_delaycode_100m = 0, g_def_delaycode_200m = 0;
u32 g_def_cclk_ds_18 = 0;
int get_config_sela_setting(struct sdhci_host *host)
{
	if (is_bus_mode_sdr104(host))
		return g_def_sela_200m;
	else
		return g_def_sela_100m;
}

int get_config_selb_setting(struct sdhci_host *host)
{
	if (is_bus_mode_sdr104(host))
		return g_def_selb_200m;
	else
		return g_def_selb_100m;
}

void get_default_setting(u8 * data)
{
	g_def_sela_100m = cfg_read_bits_ofs_mask(data, &dll_sela_100m_cfg);
	g_def_sela_200m = cfg_read_bits_ofs_mask(data, &dll_sela_200m_cfg);
	g_def_selb_100m = cfg_read_bits_ofs_mask(data, &dll_sela_100m_cfg);
	g_def_selb_200m = cfg_read_bits_ofs_mask(data, &dll_sela_200m_cfg);
	g_def_delaycode_100m =
	    cfg_read_bits_ofs_mask(data, &dll_delay_100m_backup);
	g_def_delaycode_200m =
	    cfg_read_bits_ofs_mask(data, &dll_delay_200m_backup);
	g_def_cclk_ds_18 = cfg_read_bits_ofs_mask(data, &cclk_ds_18);
}

u32 get_all_sela_status(struct sdhci_host *host, u32 target_selb)
{
	u32 all_sela = 0;
	int i = 0;
	for (i = 0; i < TUNING_PHASE_SIZE; i++) {
		u32 all_selb = all_selb_failed_tb_get(host, i);

		if (all_selb & (1 << target_selb)) {
			all_sela |= 1 << i;
		}

	}
	return all_sela;
}

int get_pass_window_weight(u32 val)
{
	int i = 0;
	int cnt = 0;
	for (i = 0; i < TUNING_PHASE_SIZE; i++) {
		if (GET_IDX_VALUE(val, i)) {
			cnt++;
		}
	}
	return cnt;
}

int get_sela_nearby_pass_window(u32 sela, u32 base)
{

	int i = 0;
	int idx = base;
	int cnt = 0;

	if (0 == GET_IDX_VALUE(sela, idx))
		return 0;
//get first 0
	for (i = 0; i < TUNING_PHASE_SIZE; i++) {
		if (GET_IDX_VALUE(sela, idx)) {
			idx++;
			idx %= TUNING_PHASE_SIZE;
		} else {
			break;
		}
	}
//get first 1 idx
	if (0 == idx)
		idx = 0xa;
	else
		idx--;

//
	for (i = 0; i < TUNING_PHASE_SIZE; i++) {
		if (GET_IDX_VALUE(sela, idx)) {
			cnt++;
			if (0 == idx)
				idx = 0xa;
			else
				idx--;
		} else {
			break;
		}

	}
	return cnt;
}

int get_left_one_sel(int base)
{
	if (base == 0)
		return 0xa;
	else
		return base - 1;

}

int get_right_one_sel(int base)
{
	if (base == 0xa)
		return 0x0;
	else
		return base + 1;

}

int get_dif(int x, int y)
{
	int dif = 0;
	if (y > x) {
		dif = y - x;
	} else {
		dif = x - y;
	}
	return dif;
}

#define  NEARBY_THD  2
#define  NEARBY_2_THD  3
bool get_refine_sel(struct sdhci_host * host, u32 tga, u32 tgb, u32 * rfa,
		    u32 * rfb)
{
	int len_tb[TUNING_PHASE_SIZE] = { 0 };
	u32 sela = 0;
	int i = 0;
	u32 selb = 0;
	for (i = 0; i < TUNING_PHASE_SIZE; i++) {
		sela = get_all_sela_status(host, selb);
		len_tb[selb] = get_sela_nearby_pass_window(sela, tga);
		DbgInfo(MODULE_SD_HOST, FEATURE_CARD_INIT, NOT_TO_RAM,
			"%x len:%xh\n", selb, len_tb[selb]);
		selb++;
		selb %= TUNING_PHASE_SIZE;
	}
	PrintMsg("BHT MSG:tgb:%xh, tgb len:%x\n", tgb, len_tb[tgb]);
	if (len_tb[tgb] < 6) {
		int lft1 = get_left_one_sel(tgb);
		int lft2 = get_left_one_sel(lft1);
		int rt1 = get_right_one_sel(tgb);
		int rt2 = get_right_one_sel(rt1);

		if (0 == len_tb[lft1] || 0 == len_tb[lft2]) {
			//left fail
			len_tb[lft1] = 0;
			len_tb[lft2] = 0;
			PrintMsg("BHT MSG:over boundary case\n");
			goto next;
		}
		if (0 == len_tb[rt1] || 0 == len_tb[rt2]) {
			len_tb[rt1] = 0;
			len_tb[rt2] = 0;
			PrintMsg("BHT MSG:over boundary case\n");
			goto next;
		}
		{
			int dif = get_dif(len_tb[lft1], len_tb[tgb]);
			if (dif > NEARBY_THD) {
				len_tb[lft1] = len_tb[tgb];
			}
			dif = get_dif(len_tb[rt1], len_tb[tgb]);
			if (dif > NEARBY_THD) {
				len_tb[rt1] = len_tb[tgb];
			}
			dif = get_dif(len_tb[lft2], len_tb[tgb]);
			if (dif > NEARBY_2_THD) {
				len_tb[lft2] = len_tb[tgb];
			}
			dif = get_dif(len_tb[rt2], len_tb[tgb]);
			if (dif > NEARBY_2_THD) {
				len_tb[rt2] = len_tb[tgb];
			}
		}
next:
		if ((len_tb[lft1] + len_tb[lft2]) >=
		    (len_tb[rt1] + len_tb[rt2])) {
			*rfb = lft2;
			*rfa = get_left_one_sel(tga);
		} else {
			*rfb = rt2;
			*rfa = get_right_one_sel(tga);
		}

		//check it exist
		sela = get_all_sela_status(host, *rfb);
		if (0 == GET_IDX_VALUE(sela, *rfa)) {
			PrintMsg
			    ("refine point is failed point, so no change\n");
			*rfa = tga;
			*rfb = tgb;
		}
	} else {
		*rfa = tga;
		*rfb = tgb;
	}
	PrintMsg("BHT MSG:tg sela:%xh, selb:%x\n", tga, tgb);
	PrintMsg("BHT MSG:rf sela:%xh, selb:%x\n", *rfa, *rfb);
	return TRUE;
}

#if 0
#define OUTPUT_PASS_TYPE  0
#define SET_PHASE_FAIL_TYPE  1
#define TUNING_FAIL_TYPE 2
#define READ_STATUS_FAIL_TYPE 3
#define TUNING_CMD7_TIMEOUT 4
#define RETUNING_CASE 5
#endif

static const char* op_dbg_str[] = {
	"no tuning",
	"pass",
	"set_phase_fail",
	"tuning fail",
	"read status fail",
	"tuning CMD7 timeout",
	"retuning case"
};

int update_selb(struct sdhci_host *host, int target_selb)
{

	return target_selb;
}

static int ggc_get_10case_0_index(u32 val)
{
	int i = 0;
	for (i = 0; i < TUNING_PHASE_SIZE; i++) {
		if (0 == GET_IDX_VALUE(val, i)
		    && GET_IDX_VALUE(val,
				     TRUNING_RING_IDX(i + TUNING_PHASE_SIZE -
						      1))) {
			return i;
		}
	}

	return -1;
}

static u32 ggc_get_01case_0_index(u32 val)
{
	int i = 0;

	for (i = 0; i < TUNING_PHASE_SIZE; i++) {
		if (0 == GET_IDX_VALUE(val, i)
		    && GET_IDX_VALUE(val, TRUNING_RING_IDX(i + 1))) {
			return i;
		}
	}

	return -1;
}

static int ggc_get_next_1_index(u32 val, int pos)
{
	int i = 0;
	pos = pos % TUNING_PHASE_SIZE;
	for (i = 0; i < TUNING_PHASE_SIZE; i++)
	{
		if (GET_IDX_VALUE(val, (pos+i)%TUNING_PHASE_SIZE))
			break;
	}
	if (GET_IDX_VALUE(val, (pos+i)%TUNING_PHASE_SIZE))
		return (pos+i)%TUNING_PHASE_SIZE;
	else
		return -1;
}

static u32 ggc_get_01case_1_index(u32 val)
{
	int i = 0;
	
	for (i = 0; i < TUNING_PHASE_SIZE; i++) {
		if (0 == GET_IDX_VALUE(val, i)
			&& GET_IDX_VALUE(val, TRUNING_RING_IDX(i + 1))) {
			return TRUNING_RING_IDX(i + 1);
		}
	}
	
	return -1;
}

static int ggc_get_first_0_index(u32 val)
{
	int i = 0;
	for (i = 0; i < TUNING_PHASE_SIZE; i++) {
		if (0 == GET_IDX_VALUE(val, i)) {
			return i;
		}
	}
	PrintMsg("BHT MSG:oops-not find 0 index\n");
	return 0;
}

static int _tx_selb_inject_policy(int tx_selb, int org_selb)
{
	if ((org_selb & BIT_PASS_MASK) != BIT_PASS_MASK) {
		//Locate the first postion of continuous zero
		int zero_start, zero_end, sel;
		sel = tx_selb;
		zero_start = ggc_get_10case_0_index(sel);
		sel &=
		    ~GENERATE_TRUNING_RING_IDX_VALUE(get_left_one_sel
						     (zero_start));
		zero_end = ggc_get_01case_0_index(sel);
		sel &=
		    ~GENERATE_TRUNING_RING_IDX_VALUE(get_right_one_sel
						     (zero_end));
		if (sel != (sel & tx_selb)) {
			DbgErr("BHT ERR:!!!=============\n\n\n");
			DbgErr
			    ("tx selb reinject exception case :not adjacent phase\n");
			DbgErr("BHT ERR:selb_failed range:%xh  ,new tx_selb:%x\n",
			       org_selb, tx_selb);
			DbgErr("BHT ERR:\n\n!!!=============\n");
		}
		org_selb &= tx_selb;
	} else {
		int i = 0;
		int cnt = ggc_get_selx_weight(~tx_selb);
		PrintMsg("BHT MSG:%d\n", cnt);
		switch (cnt) {
		case 1:
			i = ggc_get_first_0_index(tx_selb);
			tx_selb &=
			    ~GENERATE_TRUNING_RING_IDX_VALUE(get_right_one_sel
							     (i));
			tx_selb &=
			    ~GENERATE_TRUNING_RING_IDX_VALUE(get_left_one_sel
							     (i));

			break;
		case 2:
			i = ggc_get_10case_0_index(tx_selb);
			tx_selb &=
			    ~GENERATE_TRUNING_RING_IDX_VALUE(get_left_one_sel
							     (i));
			i = ggc_get_01case_0_index(tx_selb);
			tx_selb &=
			    ~GENERATE_TRUNING_RING_IDX_VALUE(get_right_one_sel
							     (i));
			break;
		default:
			PrintMsg("BHT MSG:>= 3 point case\n");
		}
		org_selb &= tx_selb;
	}
	
	PrintMsg("BHT MSG:will check continous 0bits: 0x%x\n", org_selb);
	if (1)
	{
		//check if >1 continous 0 group exist or not
		int group_pos[TUNING_PHASE_SIZE+1][3];
		int group_cnt = 0;
		int i;
		int j;
		
		memset(group_pos, 0, sizeof(group_pos));
		for (i = ggc_get_01case_1_index(org_selb); i < TUNING_PHASE_SIZE && i >= 0 && group_cnt < TUNING_PHASE_SIZE; )
		{
			for (j = 1; j < TUNING_PHASE_SIZE; j++)
			{
				if (GET_TRUNING_RING_IDX_VALUE(org_selb, i+j) == 0)
				{
					break;
				}
				else
					continue;
			}
			group_pos[group_cnt][0] = i;
			group_pos[group_cnt][1] = (i + j -1) % TUNING_PHASE_SIZE;
			group_pos[group_cnt][2] = j;
			group_cnt++;			
			if (group_pos[group_cnt-1][0] > group_pos[group_cnt-1][1])
				break;
			i = ggc_get_next_1_index(org_selb, (i+j)%TUNING_PHASE_SIZE);
			for (j = 0; j < group_cnt; j++)
			{
				if (i == group_pos[j][0])
					break;
			}
			if (j < group_cnt)
				break;
		}
		
		if (group_cnt > 1)
		{
			int max_len_group = 0;
			int max_len = 0;
			DbgErr("BHT ERR:After inject, selb 0x%x has %d continous 0 bits\n", org_selb, group_cnt);
			
			for (i = 0; i < group_cnt; i++)
			{
				if (max_len < group_pos[i][2])
				{
					max_len = group_pos[i][2];
					max_len_group = i;
				}
			}
			for (i = (group_pos[max_len_group][1] + 1) % TUNING_PHASE_SIZE; i != group_pos[max_len_group][0]; i = (i+1)%TUNING_PHASE_SIZE)
			{
				org_selb &= ~(1 << i);
			}
			DbgErr("BHT ERR:After merge incontious 0 group, selb changed to 0x%x\n", org_selb);
		}
		else if (group_cnt > 0){
			DbgErr("BHT ERR:After merge incontious 0 group, selb = 0x%x\n", org_selb);
		}
		else {
			DbgErr("BHT ERR:selb 0x%x has no bit is 0\n", org_selb);			
		}
	}

	return org_selb;
}

void tx_selb_inject_policy(struct sdhci_host *host, int tx_selb)
{
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
	
	PrintMsg("BHT MSG:before inject, failed ragen 0x%x, tx_selb 0x%x\n", vendor_host->ggc.ggc_cmd_tx_selb_failed_range, tx_selb);
	vendor_host->ggc.ggc_cmd_tx_selb_failed_range = _tx_selb_inject_policy(tx_selb, vendor_host->ggc.ggc_cmd_tx_selb_failed_range);
	tx_selb_failed_history_update(host, vendor_host->ggc.ggc_cmd_tx_selb_failed_range);
	PrintMsg("BHT MSG:after inject %xh range:%xh\n", tx_selb,
		 vendor_host->ggc.ggc_cmd_tx_selb_failed_range);
	if (is_bus_mode_sdr104(host))
		vendor_host->ggc.sdr104.fail_phase = vendor_host->ggc.ggc_cmd_tx_selb_failed_range;
	else
		vendor_host->ggc.sdr50.fail_phase = vendor_host->ggc.ggc_cmd_tx_selb_failed_range;
}

int get_selb_failure_point(int start, u64 raw_tx_selb, int tuning_cnt)
{
	int last = start + (tuning_cnt - 1);
	int i = 0;
	int j = 0;
	int phase = start;
	int vct = BIT_PASS_MASK;
	PrintMsg("BHT MSG:%s start:%d tuning_cnt:%d\n", __FUNCTION__, start,
			 tuning_cnt);
	
	for (i = 0; i < tuning_cnt; i++)
	{
		if (0 == (raw_tx_selb & GENERATE_64_IDX_VALUE(last - i))) {
			break;
		}
	}
	if (i == tuning_cnt) {
		phase = last % TUNING_PHASE_SIZE;
		vct &= (~(1 << phase));
		goto exit;
	}
	
	for (i = 0; i < tuning_cnt; i++)
	{
		if (0 != (raw_tx_selb & GENERATE_64_IDX_VALUE(last - i))) {
			break;
		}
	}
	for (j = i - 2; j >= 0; j--)
	{
		raw_tx_selb |= (1 << (last - j));
	}
	for (j = 0; j < tuning_cnt; j++)
	{
		if (0 == (raw_tx_selb & GENERATE_64_IDX_VALUE(last - j)))
			vct &= (~(1 << (last-j)));
	}

exit:
	PrintMsg("BHT MSG:%s: after adjust raw_tx_selb: 0x%llx, vct 0x%x\n",
			 __FUNCTION__, raw_tx_selb, vct);
	
	return vct;
}

bool selx_failure_point_exist(u32 val)
{
	return (val & BIT_PASS_MASK) != BIT_PASS_MASK;
}

static int _bits_vct_get_left_index(int base)
{
#if 0
    if(base == 0)
        return 0xa;
    else
        return base - 1;
#endif
    return TRUNING_RING_IDX(base + TUNING_PHASE_SIZE - 1);
}

int _get_best_window_phase(u32 vct, int * pmax_pass_win, int shif_left_flg)
{
    u8  tuning_win[TUNING_PHASE_SIZE] = { 0 };
    u8 tuning_pass[TUNING_PHASE_SIZE];
    int tuning_pass_start[TUNING_PHASE_SIZE];
    int tuning_pass_num_max = 0;
    int first_0 = 0;
    int i = 0, j = 0;
    int i_mode = 0, selb_mode = 0;


    os_memset(tuning_pass, 0, sizeof(tuning_pass));
    os_memset(tuning_pass_start, 0, sizeof(tuning_pass_start));

    for(i = 0; i < TUNING_PHASE_SIZE; i++)
    {
        if(GET_IDX_VALUE(vct, i))
        {
            tuning_win[i] = 1;
        }
        else
            tuning_win[i] = 0;
    }

    {
        for(i = 0; i < TUNING_PHASE_SIZE; i++)
        {
            if(tuning_win[i] == 0)
            {
                first_0 = i;
                break;
            }
        }
    }

    for(i = 0; i < TUNING_PHASE_SIZE; i++)
    {
        i_mode = TRUNING_RING_IDX(first_0 + i);
        if(tuning_win[i_mode] == 1)
            tuning_pass[j]++;
        else if(tuning_pass[j])
            j++;
        if(tuning_pass[j] == 1)
            tuning_pass_start[j] = i_mode;
    }

    for(i = 0; i < TUNING_PHASE_SIZE; i++)
    {
        if(tuning_pass_num_max < tuning_pass[i])
        {
            tuning_pass_num_max = tuning_pass[i];
            i_mode = i;
        }
    }

    if(tuning_pass_num_max == 0)
    {
        DbgErr("BHT ERR:###### Get max pass window fail, there is no any pass phase!!\n");
        return 0;
    }
    else
    {
        if(tuning_pass_num_max % 2)
        {
            selb_mode = tuning_pass_start[i_mode] + (tuning_pass_num_max - 1) / 2;

        }
        else
        {
            selb_mode = tuning_pass_start[i_mode] + (tuning_pass_num_max) / 2;
            if(shif_left_flg)
            {
                selb_mode = _bits_vct_get_left_index(selb_mode);
                PrintMsg("BHT MSG:shift left index\n");
            }
        }
        selb_mode = TRUNING_RING_IDX(selb_mode);
    }
    if(pmax_pass_win)
        *pmax_pass_win = tuning_pass_num_max;


    return selb_mode;
}


int get_best_window_phase(u32 vct, int * pmax_pass_win)
{
    return _get_best_window_phase(vct, pmax_pass_win, 0);
}

static int _ggc_get_suitable_selb_for_next_tuning(struct sdhci_host *host)
{
	int selb = 0;
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
	
	if (selx_failure_point_exist(vendor_host->ggc.ggc_cmd_tx_selb_failed_range)) {
		selb = vendor_host->ggc.ggc_sw_selb_tuning_first_selb;
	} else {
		u32 inj_tx_selb = BIT_PASS_MASK;
		PrintMsg("BHT MSG:manual inject for all pass case\n");
		
		if (is_bus_mode_sdr104(host)) {
			inj_tx_selb &= SDR104_MANUAL_INJECT;
		} else {
			inj_tx_selb &= SDR50_MANUAL_INJECT;
		}
		PrintMsg("BHT MSG:manual inject for all pass case, inj_tx_selb=0x%x\n", inj_tx_selb);
		selb = get_best_window_phase(inj_tx_selb, NULL);
		PrintMsg("BHT MSG:select selb %d for all pass case\n", selb);
	}
	return selb;
}

void ggc_reset_selx_failed_tb(struct sdhci_host * host)
{
	tx_selb_failed_tb_reset(host);
	all_selb_failed_tb_reset(host);
	tx_selb_failed_history_reset(host);
}
#if IFLYTEK
static void _ggc_reset_sela_tuning_result(struct sprd_sdhc_host * host)
#else
static void _ggc_reset_sela_tuning_result(struct sdhci_msm_host * host)
#endif
{
	int i = 0;
	for(i = 0; i < TUNING_PHASE_SIZE; i++)
	{
		host->ggc.ggc_sela_tuning_result[i] = NO_TUNING;
	}
	
}

void _ggc_reset_tuning_result_for_dll(struct sdhci_host * host)
{
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
	
	ggc_reset_selx_failed_tb(host);
	vendor_host->ggc.ggc_cmd_tx_selb_failed_range = BIT_PASS_MASK;
	vendor_host->ggc.selx_tuning_done_flag = 0;
	_ggc_reset_sela_tuning_result(vendor_host);
}

static u32 g_dll_voltage_unlock_cnt_100m[4] = {0};
static u32 g_dll_voltage_unlock_cnt_200m[4] = {0};

void ggc_dll_voltage_init(struct sdhci_host *  host)
{
	int i = 0;
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
	
	for(i = 0; i < 4; i++)
	{
		vendor_host->ggc.dll_voltage_scan_map[i] = 0;
		
	}
	vendor_host->ggc.sdr50.dll_voltage_unlock_cnt = g_dll_voltage_unlock_cnt_100m;
	vendor_host->ggc.sdr104.dll_voltage_unlock_cnt = g_dll_voltage_unlock_cnt_200m;
}

static int ggc_get_tuning_cnt_from_buffer(struct sdhci_host* host)
{
	
	int cnt = 0;
	u8 data[512];
	
	get_gg_reg_cur_val(data);
	cnt = (int)cfg_read_bits_ofs_mask(data, &cmd19_cnt_cfg);
	
	PrintMsg("BHT MSG:tuning cnt=%d\n", cnt);
	return cnt;
}

bool ggc_hw_inject_ext(struct sdhci_host * host, bool *card_status, u32 sel200, u32 sel100, bool writetobh201)
{
	bool ret = TRUE;
	u8 data[512];
	t_gg_reg_strt gg_reg_arr[10];
	PrintMsg("BHT MSG:%s sel200:%xh,sel100:%xh\n", __FUNCTION__, sel200, sel100);
	
	get_gg_reg_cur_val(data);
	os_memcpy(&gg_reg_arr[0], &inject_failure_for_tuning_enable_cfg, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[1], &inject_failure_for_200m_tuning_cfg, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[2], &inject_failure_for_100m_tuning_cfg, sizeof(t_gg_reg_strt));
	gg_reg_arr[0].value = 1;
	gg_reg_arr[1].value = sel200;
	gg_reg_arr[2].value = sel100;
	
	chg_gg_reg_cur_val(data, gg_reg_arr, 3, TRUE);
	if (writetobh201)
		ret = gg_emulator_write_ext(host, card_status, data, 512);
	else
	{
		if (1)
		{
			u32 i = 0;
			u32 reg;
			PrintMsg("BHT MSG:%s: dump config data instead write to bh201\n", __FUNCTION__);
			for (i = 0; i < 128; i++)
			{
				memcpy(&reg, data+i*sizeof(u32), sizeof(u32));
				PrintMsg("BHT MSG:    ggc_reg32[%03d]=0x%08x\n", i, reg);
			}
		}
	}
	return ret;
}

bool _ggc_hw_inject_may_recursive(struct sdhci_host * host, u32 sel200, u32 sel100, int max_recur, bool writetobh201)
{
	{
		bool ret = TRUE, card_status = TRUE;
	#if IFLYTEK
		struct mmc_host *mmc = host->mmc;
		struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
	#else
		struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
		struct sdhci_msm_host *vendor_host = pltfm_host->priv;
	#endif
		ret = ggc_hw_inject_ext(host, &card_status, vendor_host->ggc.ggc_cmd_tx_selb_failed_range, vendor_host->ggc.ggc_cmd_tx_selb_failed_range, writetobh201);
		PrintMsg("BHT MSG:ret:%x\n", ret);
		if((FALSE == ret) && (FALSE == card_status))
		{
			int selb =	BIT_PASS_MASK;
			PrintMsg("BHT MSG:inject again when hw inject\n");
			selb &= ~ GENERATE_IDX_VALUE(vendor_host->ggc.ggc_sw_selb_tuning_first_selb);
			tx_selb_inject_policy(host, selb);
			_ggc_update_cur_setting_for_sw_selb_tuning(host, vendor_host->ggc.ggc_cmd_tx_selb_failed_range); //update cur
			
			if(((11 - get_bit_number(vendor_host->ggc.ggc_cmd_tx_selb_failed_range)) >= 5))
			{
					DbgErr("BHT ERR:pass windows too small,reinit recursive\n");
					return FALSE;
			}
			
			if(max_recur--)
			{
				return _ggc_hw_inject_may_recursive(host, vendor_host->ggc.ggc_cmd_tx_selb_failed_range, vendor_host->ggc.ggc_cmd_tx_selb_failed_range, max_recur, writetobh201);
			}
			else
				return FALSE;
		}
		else
			return TRUE;
	}
}

bool ggc_hw_inject_may_recursive(struct sdhci_host * host, u32 sel200, u32 sel100, bool writetobh201)
{
	return _ggc_hw_inject_may_recursive(host, sel200, sel100, 4, writetobh201);
}

bool get_next_dll_voltage(int cur, int * next, u32 * dll_voltage_unlock_cnt, int * dll_voltage_scan_map)
{
    int min_idx = 0, cur_cnt = 0, next_cnt = 0;
    int cur_flg = 0;
    int i = 0;
    DbgErr("BHT ERR:dll_voltage_unlock_cnt:%x %x %x %x\n", dll_voltage_unlock_cnt[0], dll_voltage_unlock_cnt[1],
           dll_voltage_unlock_cnt[2], dll_voltage_unlock_cnt[3]);
    DbgErr("BHT ERR:dll_voltage_scan_map:%x %x %x %x\n", dll_voltage_scan_map[0], dll_voltage_scan_map[1],
           dll_voltage_scan_map[2], dll_voltage_scan_map[3]);
    for(i = 1; i < 4; i++)
    {
        if(0 == cur_flg)
        {
            if(0 != dll_voltage_scan_map[(cur + i) % 4])
                continue;
            cur_cnt = dll_voltage_unlock_cnt[(cur + i) % 4];
            cur_flg = 1;
            min_idx = (cur + i) % 4;
            continue;
        }
        else
        {
            if(0 != dll_voltage_scan_map[(cur + i) % 4])
                continue;
            next_cnt = dll_voltage_unlock_cnt[(cur + i) % 4];
            if(cur_cnt > next_cnt)
            {
                cur_cnt = next_cnt;
                min_idx = (cur + i) % 4;
            }

        }
    }
    if(0 == cur_flg)
    {
        DbgErr("BHT ERR:no find available voltage\n");
        return FALSE;
    }
    else
    {
        *next = min_idx;
        DbgErr("BHT ERR:next available voltage %d\n", min_idx);
        return TRUE;
    }
}

bool ggc_sw_calc_tuning_result(struct sdhci_host * host,
			       bool * card_status,
			       bool * read_status,
			       u32 * tx_selb,
			       u32 * all_selb,
			       u64* raw_tx_selb)
{
	bool ret = FALSE;
	bool card_ret = FALSE;
	bool read_ret = FALSE;
	u32 selb_status_tx_low32 = 0, selb_status_tx_high32 = 0;
	u32 selb_status_ggc_low32 = 0, selb_status_ggc_high32 = 0;
	t_gg_reg_strt gg_reg_arr[8] = {{0}};

	os_memcpy(&gg_reg_arr[0], &pha_stas_tx_low32, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[1], &pha_stas_tx_high32, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[2], &pha_stas_rx_low32, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[3], &pha_stas_rx_high32, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[4], &dll_sela_after_mask, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[5], &dll_selb_after_mask, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[6], &dll_delay_100m_backup, sizeof(t_gg_reg_strt));
	os_memcpy(&gg_reg_arr[7], &dll_delay_200m_backup, sizeof(t_gg_reg_strt));
	
	ret = ggc_read_registers_ext(host, &card_ret, &read_ret, gg_reg_arr, 8);
	
	if(TRUE == read_ret)
	{
		//
		selb_status_tx_low32 = gg_reg_arr[0].value;
		PrintMsg("BHT MSG:[205-236]:\n");
		log_bin(selb_status_tx_low32);
		PrintMsg("BHT MSG:[237-268]:\n");
		selb_status_tx_high32 = gg_reg_arr[1].value;
		log_bin(selb_status_tx_high32);

		PrintMsg("BHT MSG:[14-45]:\n");
		selb_status_ggc_low32 = gg_reg_arr[2].value;
		log_bin(selb_status_ggc_low32);
		PrintMsg("BHT MSG:[46-77]:\n");
		selb_status_ggc_high32 = gg_reg_arr[3].value;
		log_bin(selb_status_ggc_high32);
		PrintMsg("BHT MSG:dll  sela after mask=%xh", gg_reg_arr[4].value);
		PrintMsg("BHT MSG:dll  selb after mask=%xh", gg_reg_arr[5].value);

		if (raw_tx_selb) {
			*raw_tx_selb = gg_reg_arr[1].value;
			(*raw_tx_selb) <<= 32;
			*raw_tx_selb += gg_reg_arr[0].value;
			PrintMsg("BHT MSG:raw_tx_selb:%llxh\n", *raw_tx_selb);
		}

		if (tx_selb) {
			gen_array_data(gg_reg_arr[0].value, gg_reg_arr[1].value,
				       tx_selb);
			PrintMsg("BHT MSG:tx_selb:%xh\n", *tx_selb);
		}
		if (all_selb) {
			gen_array_data(gg_reg_arr[2].value, gg_reg_arr[3].value,
				       all_selb);
			PrintMsg("BHT MSG:all_selb:%xh\n", *all_selb);
		}
	}
	
	if(read_status)
		(*read_status) = read_ret;
	if(card_status)
		(*card_status) = card_ret;
	
	if(card_status && read_status)
		PrintMsg("BHT MSG:card_status,read_status:%x %x\n", *card_status, *read_status);
	return ret;
}

bool _ggc_calc_cur_sela_tuning_result(struct sdhci_host * host, int cur_sela, int start_selb)
{
	bool read_status = FALSE;
	bool card_status = FALSE;
	bool ret = TRUE;
	u32 tx_selb, all_selb;
	u64 raw_tx_selb = 0;
	bool retuning_flg = FALSE;
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
	ggc_platform_t * ggc = &vendor_host->ggc;
	tuning_stat_et * psela_tuning_result = ggc->ggc_sela_tuning_result;
	
	ret = ggc_sw_calc_tuning_result(host, &card_status, &read_status, &tx_selb, &all_selb, &raw_tx_selb);

	if(FALSE == card_status) {
		if(TRUE == read_status) {
			int selb = get_selb_failure_point(start_selb, raw_tx_selb, ggc_get_tuning_cnt_from_buffer(host));
#ifdef FORCE_READ_STATUS_CMD7_TIMEOUT
			static bool cmd7_timeout_issue_test_flag = TRUE;
			if (cmd7_timeout_issue_test_flag)
			{
				selb = BIT_PASS_MASK & (~ (1<<start_selb));
				PrintMsg("BHT MSG:inject selb %03x for force CMD7 read timeout error\n", selb);
				cmd7_timeout_issue_test_flag = FALSE;
			}
#else
			PrintMsg("BHT MSG:inject selb %03x for CMD7 read timeout\n", selb);			
#endif
			tx_selb_inject_policy(host, selb);
		} else {
			PrintMsg("BHT MSG:read status failedA!!\n");
			PrintMsg("BHT MSG:============ %s dll:%xh failed ============\n", __FUNCTION__, cur_sela);
		}
		ret = FALSE;
		goto exit;
	} else {
		//update tx history
		if(TRUE == read_status) {
#if 1
			if(selx_failure_point_exist(tx_selb)) {
				if((11-get_bit_number(tx_selb))<=3) {
					tx_selb_inject_policy(host, tx_selb);
					all_selb_failed_tb_update(host, cur_sela, all_selb);
					tx_selb_failed_tb_update(host, cur_sela, tx_selb);
					tx_selb_failed_history_update(host, tx_selb);
				} else if(get_bit_number(tx_selb)==0) {
					int selb = get_selb_failure_point(start_selb, raw_tx_selb, ggc_get_tuning_cnt_from_buffer(host));
					tx_selb_inject_policy(host, selb);
					all_selb_failed_tb_update(host, cur_sela, all_selb);
					tx_selb_failed_tb_update(host, cur_sela, selb);
					tx_selb_failed_history_update(host, selb);
					retuning_flg = TRUE;
				} else {
					tx_selb_inject_policy(host, tx_selb);
					all_selb_failed_tb_update(host, cur_sela, all_selb);
					tx_selb_failed_tb_update(host, cur_sela, tx_selb);
					tx_selb_failed_history_update(host, tx_selb);
					retuning_flg = TRUE;
				}
				//update ggc setting
				_ggc_update_cur_setting_for_sw_selb_tuning(host, ggc->ggc_cmd_tx_selb_failed_range); //update cur
				ggc_hw_inject_may_recursive(host, ggc->ggc_cmd_tx_selb_failed_range, ggc->ggc_cmd_tx_selb_failed_range, TRUE);
			}
			else
			{
				all_selb_failed_tb_update(host, cur_sela, all_selb);
				tx_selb_failed_tb_update(host, cur_sela, tx_selb);
				tx_selb_failed_history_update(host, tx_selb);
			}
#endif
			if(retuning_flg == TRUE)
			{
				PrintMsg("BHT MSG:============ %s dll:%xh need retuning ============\n", __FUNCTION__, cur_sela);
				psela_tuning_result[cur_sela] = RETUNING_CASE;
			}
			else
			{
				PrintMsg("BHT MSG:============ %s dll:%xh pass ============\n", __FUNCTION__, cur_sela);
				psela_tuning_result[cur_sela] = OUTPUT_PASS_TYPE;
			}
		}
		else
		{
			PrintMsg("BHT MSG:read status failed!!\n");
			psela_tuning_result[cur_sela] = READ_STATUS_FAIL_TYPE;
			all_selb_failed_tb_update(host, cur_sela, 0); //mark failed case to failed.
			PrintMsg("BHT MSG:============ %s dll:%xh failed ============\n", __FUNCTION__, cur_sela);
		}
	}
exit:
	return ret;
}

//BH201LN driver--sunsiyuan@wind-mobi.com modify at 20180326 begin
bool _ggc_output_tuning(struct sdhci_host * host, u8 * selb_pass_win)
{
#define DLL_SEL_RESULT_SIZE  16
#define GGC_SW_SELB_TUNING_LOOP 3
	int cur_sela = 0, dll_sela_cnt = 0;
	int dll_sela_basis = 0;
	bool ret = FALSE;
	
	int target_sela = 0;
	int target_selb = 0;
	
	u32 tx_selb, all_selb;
	u64 raw_tx_selb;
	
	bool status_ret = FALSE;
	int cur_selb = 0;
	int tuning_error_type[DLL_SEL_RESULT_SIZE] = { 0 };
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif

	ggc_platform_t * ggc = &vendor_host->ggc;
	tuning_stat_et * psela_tuning_result = ggc->ggc_sela_tuning_result;
	
	//sel_test:
	//init
	vendor_host->ggc.driver_strength_reinit_flg = 0;			
	vendor_host->ggc.dll_unlock_reinit_flg = 0;
	vendor_host->sdr50_notuning_crc_error_flag = 0;
	
	dll_sela_basis = get_config_sela_setting(host);
	cur_selb = get_config_selb_setting(host);
#if 1
	if (ggc->tuning_cmd7_timeout_reinit_flg) {
		ggc->tuning_cmd7_timeout_reinit_flg = 0;
		dll_sela_basis = vendor_host->ggc.ggc_cur_sela;
		cur_selb = vendor_host->ggc.ggc_sw_selb_tuning_first_selb;
		PrintMsg("BHT MSG:Tuning will start from sela: 0x%x, selb: 0x%x where CMD7 timeout occur\n", dll_sela_basis, cur_selb);
	}
#else
	ggc->tuning_cmd7_timeout_reinit_flg = 0;
#endif
	
	for (dll_sela_cnt = 0; dll_sela_cnt < TUNING_PHASE_SIZE; dll_sela_cnt++) {
		int rescan_selb_cnt = 0;
		int returning_selb_cnt = 0;
		cur_sela =
				(dll_sela_cnt + dll_sela_basis) % TUNING_PHASE_SIZE;
		ggc->ggc_cur_sela = cur_sela;
		PrintMsg("BHT MSG:============ %s select sela dll: %x, selb dll: %x ============\n", __FUNCTION__, cur_sela, cur_selb);
		if((NO_TUNING != psela_tuning_result[cur_sela]))
		{
			PrintMsg("BHT MSG:sela tuning=%d already tuning,so skip it\n", cur_sela);
			continue;
		}
rescan_selb:
		host_cmddat_line_reset(host);
		
		if (dll_sela_cnt == 0) {
			bool first_cmd19_sta = TRUE;
			if (!!!selx_failure_point_exist(vendor_host->ggc.ggc_cmd_tx_selb_failed_range)) {
				rescan_selb_cnt = GGC_SW_SELB_TUNING_LOOP;
				PrintMsg("BHT MSG:no need rescan case\n");				
			}
			status_ret = FALSE;
			ret = ggc_sd_tuning(host, &first_cmd19_sta);
#ifdef FORCE_FIRST_CMD19_FAIL_ERROR
			{
				static bool dll_vol_change_test_flag = TRUE;
				if (dll_vol_change_test_flag)
				{
					PrintMsg("BHT MSG:force first cmd19 fail to debug dll voltage change, cur_dll_voltage_idx=%d", ggc->cur_dll_voltage_idx);
					first_cmd19_sta = FALSE;
					dll_vol_change_test_flag = FALSE;
				}
			}
#endif
			if (FALSE == first_cmd19_sta) {
				int next = 0;
				_check_bus_mode(host);
				ggc->cur_bus_mode->dll_voltage_unlock_cnt[ggc->cur_dll_voltage_idx]++;
				ggc->dll_voltage_scan_map[ggc->cur_dll_voltage_idx] = 1;
				if(TRUE == get_next_dll_voltage(ggc->cur_dll_voltage_idx, & next, ggc->cur_bus_mode->dll_voltage_unlock_cnt, ggc->dll_voltage_scan_map)) {
					ggc->cur_dll_voltage_idx = next;
				} else {
					ggc->cur_dll_voltage_idx = (ggc->cur_dll_voltage_idx + 1) % 4;
				}
				DbgErr("BHT ERR:first cmd19 timeout\n");
				vendor_host->ggc.dll_unlock_reinit_flg = 1; //change dll voltage
				_ggc_reset_tuning_result_for_dll(host);
				ret = FALSE;
				goto exit;	
			}
		} else if ((FALSE == is_bus_mode_sdr104(host))
							 && 1 == vendor_host->sdr50_notuning_sela_inject_flag
							 && !GET_IDX_VALUE(vendor_host->sdr50_notuning_sela_rx_inject,
																cur_sela)) {
			PrintMsg("BHT MSG:skip %d\n", cur_sela);
			tuning_error_type[cur_sela] = READ_STATUS_FAIL_TYPE;
			goto cur_sela_failed;
		} else {
			bool card_status = TRUE;
			ret = ggc_set_output_tuning_phase_ext(host, &card_status, cur_sela,
																						update_selb(host, cur_selb));
			if (ret == FALSE || FALSE == card_status) {
				DbgErr("BHT ERR:Error when output_tuning, sd_tuning set sela,selb fail at phase %d, ret = %d, card_status=%d\n",
							 cur_sela, ret, card_status);
				if (FALSE == card_status) {
					int selb =	BIT_PASS_MASK;
					selb &= ~ GENERATE_IDX_VALUE(cur_selb);
					DbgErr("BHT ERR:inject selb %d for update sela/selb failed\n", selb);
					tx_selb_inject_policy(host, selb);
					_ggc_update_cur_setting_for_sw_selb_tuning(host, ggc->ggc_cmd_tx_selb_failed_range); //update cur
					ggc_hw_inject_may_recursive(host, ggc->ggc_cmd_tx_selb_failed_range, ggc->ggc_cmd_tx_selb_failed_range, TRUE);
					
					
					/////2018-11-08
					if (((11 - get_bit_number(ggc->ggc_cmd_tx_selb_failed_range)) >= 5))
					{
						u8 current_ds = (u8)(g_gg_reg_cur[15] >> 28);
						DbgErr("BHT ERR:pass windows too small,reinit when set current sela & selb failed\n");
						if (current_ds < 7)
							vendor_host->ggc.driver_strength_reinit_flg = current_ds + 1;
						else
							vendor_host->ggc.driver_strength_reinit_flg = 7;
						
						g_gg_reg_cur[15] &= 0x0F0FFFFF;
						g_gg_reg_cur[15] |= (vendor_host->ggc.driver_strength_reinit_flg << 28)
															 | (vendor_host->ggc.driver_strength_reinit_flg << 20);
						ret = FALSE;
						DbgErr("BHT ERR:will change driver strength from %d to %d\n", 
									 current_ds,
									 vendor_host->ggc.driver_strength_reinit_flg);
						goto exit;
					}
					cur_selb = _ggc_get_suitable_selb_for_next_tuning(host);
				}
				psela_tuning_result[cur_sela] = RETUNING_CASE;
				goto retuning_case;
			}
			ret = ggc_sd_tuning(host, NULL);
		}
		
		if(ret == FALSE) {
			DbgErr("BHT ERR:Error when output_tuning,  sd_tuning fail at phase %d\n", cur_sela);
			psela_tuning_result[cur_sela] = TUNING_FAIL_TYPE;
			all_selb_failed_tb_update(host, cur_sela, 0); //mark failed case to failed.
			continue;
		}
		
		ret = _ggc_calc_cur_sela_tuning_result(host, cur_sela, cur_selb);
		//force driver strength error for debug
#ifdef FORCE_DRIVER_STRENGTH_ERROR
		{
			static bool driver_strength_reinit_test_flag = TRUE;
			while (driver_strength_reinit_test_flag && get_bit_number(vendor_host->ggc.ggc_cmd_tx_selb_failed_range) > 6) {
				int selb = _ggc_get_suitable_selb_for_next_tuning(host);
				tx_selb_inject_policy(host, selb);
			}
			driver_strength_reinit_test_flag = FALSE;
		}
#endif
		if((11 - get_bit_number(vendor_host->ggc.ggc_cmd_tx_selb_failed_range)) >= 5) {
			u8 current_ds = (u8)(g_gg_reg_cur[15] >> 28);
			DbgErr("BHT ERR:pass windows too small after result calculate, reinit\n");
			if (current_ds < 7)
				vendor_host->ggc.driver_strength_reinit_flg = current_ds + 1;
			else
				vendor_host->ggc.driver_strength_reinit_flg = 7;
			
			g_gg_reg_cur[15] &= 0x0F0FFFFF;
			g_gg_reg_cur[15] |= (vendor_host->ggc.driver_strength_reinit_flg << 28)
												 | (vendor_host->ggc.driver_strength_reinit_flg << 20);
			ret = FALSE;
			DbgErr("BHT ERR:will change driver strength from %d to %d\n", 
						 current_ds,
						 vendor_host->ggc.driver_strength_reinit_flg);
			goto exit;
		}

		if(FALSE == ret) {
			DbgErr("BHT ERR:cmd7 timeout fail,reinit\n");
			vendor_host->ggc.tuning_cmd7_timeout_reinit_flg = 1;

			_ggc_update_cur_setting_for_sw_selb_tuning(host, ggc->ggc_cmd_tx_selb_failed_range); //update cur
			ggc_hw_inject_may_recursive(host, ggc->ggc_cmd_tx_selb_failed_range, ggc->ggc_cmd_tx_selb_failed_range, FALSE);
			if((11 - get_bit_number(vendor_host->ggc.ggc_cmd_tx_selb_failed_range)) >= 5) {
				u8 current_ds = (u8)(g_gg_reg_cur[15] >> 28);
				DbgErr("BHT ERR:pass windows too small after cmd7 timeout inject\n");
				DbgErr("BHT ERR:do driver strength reinit instead cmd7 timeout reinit\n");
				
				vendor_host->ggc.tuning_cmd7_timeout_reinit_flg = 0;
				
				if (current_ds < 7)
					vendor_host->ggc.driver_strength_reinit_flg = current_ds + 1;
				else
					vendor_host->ggc.driver_strength_reinit_flg = 7;
				
				g_gg_reg_cur[15] &= 0x0F0FFFFF;
				g_gg_reg_cur[15] |= (vendor_host->ggc.driver_strength_reinit_flg << 28)
													 | (vendor_host->ggc.driver_strength_reinit_flg << 20);
				ret = FALSE;
				DbgErr("BHT ERR:will change driver strength from %d to %d\n", 
							 current_ds,
							 vendor_host->ggc.driver_strength_reinit_flg);
				goto exit;
			}
			goto exit;
		}
		
		cur_selb = _ggc_get_suitable_selb_for_next_tuning(host);
		
		PrintMsg("BHT MSG:===ot  sela:%xh pass ===\n", cur_sela);
		rescan_selb_cnt++;
		if ((rescan_selb_cnt < GGC_SW_SELB_TUNING_LOOP) &&
				(selx_failure_point_exist(vendor_host->ggc.ggc_cmd_tx_selb_failed_range))) {
			PrintMsg("BHT MSG:rescan cnt %d, ggc_cmd_tx_selb_failed_range=0x%x\n", 
							 rescan_selb_cnt, 
							 vendor_host->ggc.ggc_cmd_tx_selb_failed_range);
			goto rescan_selb;
		}
		
retuning_case:
		if(RETUNING_CASE == psela_tuning_result[cur_sela]) {
			returning_selb_cnt++;
			if(returning_selb_cnt < 3) {
				int rescan_selb_cnt = 0;
				PrintMsg("BHT MSG:retuning %d\n", rescan_selb_cnt);
				goto rescan_selb;
			} else {
				psela_tuning_result[cur_sela] = SET_PHASE_FAIL_TYPE;
				all_selb_failed_tb_update(host, cur_sela, 0); //mark failed case to failed.
				continue;
			}
		}
		
		goto next_dll_sela;

cur_sela_failed:
		PrintMsg("BHT MSG:read status failedB\n");
		all_selb = 0;
		all_selb_failed_tb_update(host, cur_sela, all_selb);	//mark failed case to failed.
		
		PrintMsg("BHT MSG:===ot  sela:%xh failed ===\n", cur_sela);
next_dll_sela:
		if ((FALSE == is_bus_mode_sdr104(host))
				&& vendor_host->sdr50_notuning_crc_error_flag) {
			u32 fp = 0;
			fp = GENERATE_IDX_VALUE(cur_sela);
			fp |=
					GENERATE_IDX_VALUE((cur_sela +
															1) % TUNING_PHASE_SIZE);
			fp |=
					GENERATE_IDX_VALUE((cur_sela +
															10) % TUNING_PHASE_SIZE);
			vendor_host->sdr50_notuning_sela_rx_inject &= ~fp;
			vendor_host->sdr50_notuning_sela_inject_flag = 1;
			PrintMsg("BHT MSG:sdr50_notuning_sela_rx_inject:%x\n",
							 vendor_host->sdr50_notuning_sela_rx_inject);
			ret = FALSE;
			goto exit;
		};
	}

	//
	{
		int i = 0;

		for (i = 0; i < TUNING_PHASE_SIZE; i++) {
			u8 all_str[TUNING_PHASE_SIZE + 1],
					tx_str[TUNING_PHASE_SIZE + 1];
			phase_str(all_str,
								all_selb_failed_tb_get(host, i));
			phase_str(tx_str, tx_selb_failed_tb_get(host, i));
			PrintMsg
					("BHT MSG:DLL sela[%x]  all selb: %s   tx selb: %s [%xh,%xh] %s\n",
					 i, all_str, tx_str,
					 all_selb_failed_tb_get(host, i), 
					 tx_selb_failed_tb_get(host, i),
					 op_dbg_str[tuning_error_type[i]]);
			
		}
		
		// remove margin passed all selb phase by ernest 2019/6/6
		{
			u32 idx_r, idx_c;
			u32 min_pos = 0;
			u32 all_selb[TUNING_PHASE_SIZE] = { 0 };
			u32 pass_cnt[TUNING_PHASE_SIZE] = { 0 };
			for (i = 0; i < TUNING_PHASE_SIZE; i++) {
				all_selb[i] =
						all_selb_failed_tb_get(host, i);
			}
			// calculate cumulation of diagonal bits
			for (idx_c = 0; idx_c < TUNING_PHASE_SIZE; idx_c++) {
				for (idx_r = 0; idx_r < TUNING_PHASE_SIZE;
						 idx_r++) {
					pass_cnt[idx_c] +=
							((all_selb[idx_r] >>
								((idx_r +
									idx_c) %
								 TUNING_PHASE_SIZE)) & 0x01);
				}
				if (idx_c == 0)
					min_pos = 0;
				else if (pass_cnt[idx_c] < pass_cnt[min_pos])
					min_pos = idx_c;
			}
			for (i = 0; i < TUNING_PHASE_SIZE; i++) {
				all_selb[i] &=
						~(1 << (min_pos + i) % TUNING_PHASE_SIZE);
				all_selb_failed_tb_update(host, i,
																	all_selb[i]);
			}
		}
		{
			u32 tx_selb = tx_selb_failed_history_get(host);
			u32 cfg = 0;
			PrintMsg
					("inject sw selb & merge tx_selb failed point to all_selb\n");
			for (i = 0; i < TUNING_PHASE_SIZE; i++) {
				all_selb_failed_tb_update(host, i, tx_selb);	//inject tx_selb failed
			}
#if  1
			PrintMsg("BHT MSG:inject sw sela failed point to all_selb\n");
//bug 728713,huangxiaotian,modify for code scan,2022/01/28
			cfg = 0x7ff;

			for (i = 0; i < TUNING_PHASE_SIZE; i++) {
				if (0 == GET_IDX_VALUE(cfg, i)) {
					all_selb_failed_tb_update(host, i, 0);	//inject sela failed
				}
			}
#endif
			for (i = 0; i < TUNING_PHASE_SIZE; i++) {
				u8 all_str[TUNING_PHASE_SIZE + 1],
						tx_str[TUNING_PHASE_SIZE + 1];
				phase_str(all_str,
									all_selb_failed_tb_get(host, i));
				phase_str(tx_str,
									tx_selb_failed_tb_get(host, i));
				PrintMsg
						("BHT MSG:DLL sela[%x]  all selb: %s   tx selb: %s [%xh,%xh] %s\n",
						 i, all_str, tx_str,
						 all_selb_failed_tb_get(host, i),
						 tx_selb_failed_tb_get(host, i),						 
						 op_dbg_str[tuning_error_type[i]]);
				
			}
		}
		// calculate selb
		{
			u8 win_tb[12] = { 0 };
			u8 win_mid = 0;
			u8 win_max = 0;
			u32 tx_tmp = 0;
			u32 tx_selb = tx_selb_failed_history_get(host);
			tx_selb &= 0x7ff;
			tx_tmp = tx_selb;
			PrintMsg("BHT MSG:---selb merge---\n");			
			if((tx_selb & 0x7ff )== 0x7ff) //all pass
			{
#if 1
				if (is_bus_mode_sdr104(host)) {
					u32 cfg = SDR104_MANUAL_INJECT;
					tx_selb &= cfg;
					PrintMsg
							("tx selb:%xh SDR104 inject:%xh merge tx_selb:%xh\n",
							 tx_tmp, cfg, tx_selb);
				} else {
					u32 cfg = SDR50_MANUAL_INJECT;
					tx_selb &= cfg;
					PrintMsg
							("tx selb:%xh SDR50 inject:%xh merge tx_selb:%xh\n",
							 tx_tmp, cfg, tx_selb);
				}
#endif
			}

			{
				if (0 == tx_selb) {
					DbgErr
							("no valid  selb window, all failed, so force fixed phase sela selb to default\n");
					target_selb =
							get_config_selb_setting(host);
					target_sela =
							get_config_sela_setting(host);
					goto final;
				}
				phase_str(win_tb, tx_selb);
				PrintMsg("BHT MSG:######### tx selb[%xh] 11 bit: %s \n",
								 tx_selb, win_tb);
				bits_generate_array(win_tb, tx_selb);
				chk_phase_window(win_tb, &win_mid, &win_max);
				target_selb = win_mid;

			}
			{
				//sela

				u32 all_sela = 0;

				for (i = 0; i < TUNING_PHASE_SIZE; i++) {
					u32 all_selb =
					    all_selb_failed_tb_get(host,i);
					phase_str(win_tb, all_selb);
					PrintMsg
					    ("######### all_selb[%xh] 11 bit: %s \n",
					     all_selb, win_tb);
					bits_generate_array(win_tb, all_selb);
					chk_phase_window(win_tb, &win_mid,
							 &win_max);
					*selb_pass_win = win_max;
					if (all_selb & (1 << target_selb)) {
						all_sela |= 1 << i;
					}

				}

				phase_str(win_tb, all_sela);
				PrintMsg
				    ("######### all_sela[%xh] 11 bit: %s \n",
				     all_sela, win_tb);
				bits_generate_array(win_tb, all_sela);
				chk_phase_window(win_tb, &win_mid, &win_max);
				target_sela = win_mid;
#if 0
				if (host->info.sw_cur_setting.sd_access_mode ==
				    SD_FNC_AM_SDR50
				    || (host->info.sw_cur_setting.
					sd_access_mode == SD_FNC_AM_SDR104
					&&
					(get_pass_window_weight
					 (host->host->cfg->output_pfm.
					  sw_sela_sdr104.fail_phase) != 11))) {
					// move for host
					PrintMsg("BHT MSG:base sela:%xh, selb:%x\n",
						 target_sela, target_selb);
					get_refine_sel(host, target_sela,
						       target_selb,
						       &target_sela,
						       &target_selb);
				}
#endif
			}

		}

	}

final:
	{
		if (1) {
			gg_fix_output_tuning_phase(host, target_sela,
						   target_selb);
		} else {
			PrintMsg
			    ("######### hw select mode - sela dll: %x, selb dll: %x \n",
			     target_sela, target_selb);
			ggc_set_output_tuning_phase(host, target_sela,
						    target_selb);
		}
		if (1) {
			ret =
					sd_gg_tuning_status(host, &tx_selb, &all_selb, &raw_tx_selb,
															&status_ret, NULL);
			if (ret == FALSE) {
				DbgErr
				    ("Error when output_tuning,  sd_tuning fail\n");
				ret = FALSE;
				goto exit;
			}

			//use final pass windows
			{
				u8 win_tb[12] = { 0 };
				u8 win_mid = 0;
				u8 win_max = 0;
				phase_str(win_tb, all_selb);
				PrintMsg
				    ("######### all_selb[%xh] 11 bit: %s \n",
				     all_selb, win_tb);
				bits_generate_array(win_tb, all_selb);
				chk_phase_window(win_tb, &win_mid, &win_max);
				*selb_pass_win = win_max;
			}
			vendor_host->ggc.selx_tuning_done_flag = 1;
		}

	}

exit:
	PrintMsg("BHT MSG:exit:%s  %d\n", __FUNCTION__, ret);
	return ret;
}

void ggc_tuning_result_reset(struct sdhci_host *host)
{
#if IFLYTEK
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
#else
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
#endif
	PrintMsg("BHT MSG:%s: will clear all tuning results\n", __FUNCTION__);
	_ggc_reset_tuning_result_for_dll(host);
	
	vendor_host->ggc.sdr50.bus_mode = SD_FNC_AM_SDR50;
	vendor_host->ggc.sdr104.bus_mode = SD_FNC_AM_SDR104;
	vendor_host->ggc.driver_strength_reinit_flg = 0;				
	vendor_host->ggc.cur_bus_mode = NULL;
	vendor_host->ggc.dll_unlock_reinit_flg = 0;
	vendor_host->ggc.dll_unlock_reinit_flg = 0;
	vendor_host->ggc.tuning_cmd7_timeout_reinit_flg = 0;
	vendor_host->ggc.tuning_cmd7_timeout_reinit_cnt = 0;
	vendor_host->sdr50_notuning_sela_inject_flag = 1;
	vendor_host->sdr50_notuning_crc_error_flag = 0;
	vendor_host->sdr50_notuning_sela_rx_inject = 0x3F8;
}

//BH201LN driver--sunsiyuan@wind-mobi.com modify at 20180326 end
void ggc_chip_init(struct sdhci_host *host)
{
	static int init_flg = 0;
	if (0 == init_flg) {
		u8 data[512] = { 0 };
		get_gg_reg_def(host, data);
		get_default_setting(data);
		set_gg_reg_cur_val(data);	//save it to cur
		init_flg = 1;
	}
}
#if IFLYTEK
int sdhci_bht_execute_tuning(struct mmc_host *mmc,u32 opcode)
{
	struct sprd_sdhc_host *vendor_host = mmc_priv(mmc);
	struct sdhci_host *host = sprd_priv(vendor_host);
	host->clock = vendor_host->ios.clock;
	host->timing = vendor_host->ios.timing;
	if (bht_target_host(vendor_host)) 
#else
int sdhci_bht_execute_tuning(struct sdhci_host *host, u32 opcode)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *vendor_host = pltfm_host->priv;
	if (bht_target_host(host))
#endif
	{	
		u8 tw = 0;
		int ret = 0;

		PrintMsg("BHT MSG:enter bht tuning\n");
		if (host->clock < CORE_FREQ_100MHZ) {
			PrintMsg("BHT MSG:%d less 100Mhz,no tuning\n", host->clock);
			return 0;
		}

		if (vendor_host->tuning_in_progress) {
			PrintMsg("BHT MSG:tuning_in_progress\n");
			return 0;
		}
		vendor_host->tuning_in_progress = true;

		if (mmc->ios.timing == MMC_TIMING_SD_HS)
			return sprd_sdhc_execute_tuning(mmc,opcode);

		if (vendor_host->ggc.selx_tuning_done_flag) {
			PrintMsg("BHT MSG:GGC tuning is done, just do vendor host tuning");
			if (is_bus_mode_sdr104(host)) {
				ret = sdhci_bht_sdr104_execute_tuning(host, 0x13); 
			} else {
				ret = sdhci_bht_sdr50_execute_tuning(host, 0x13); 
			}
		} else {
			ret = ! ! !_ggc_output_tuning(host, &tw);
		}
		vendor_host->tuning_in_progress = false;
		return ret;
	} else
#if IFLYTEK
		return sprd_sdhc_execute_tuning(mmc,opcode);
#else
		return sdhci_msm_execute_tuning(host, opcode);
#endif
}
