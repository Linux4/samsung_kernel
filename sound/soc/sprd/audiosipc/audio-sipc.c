/*
 * driver/sipc/audio-sipc.c
 *
 * SPRD SoC AUDIO -- SpreadTrum SOC SIPC for AUDIO Common function.
 *
 * Copyright (C) 2012 SpreadTrum Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <sound/soc.h>
#include <sound/audio-sipc.h>
#include <sound/sprd_memcpy_ops.h>
#include "sprd-asoc-common.h"
static struct saudio_ipc s_sipc_inst = {0};
static struct sipc_iram_buf s_iram_buf = {0};


static void sprd_memcpy(char * dest_addr, char * src_addr, int n)
{
		while (n--) {
            *(char *)dest_addr = *(char *)src_addr;
            dest_addr = (char *)dest_addr + 1;
            src_addr = (char *)src_addr + 1;
		}
}

static struct mutex	g_commandlock;		/* lock for send-buffer */

static int audio_sipc_create(uint8_t dst, struct audio_sipc_of_data *info)
{
	size_t smsg_txaddr;
	size_t smsg_rxaddr;
	int ret;

	if (dst >= SIPC_ID_NR) {
		pr_err("%s, invalid dst:%d \n", __func__, dst);
		return -EINVAL;
	}
	mutex_init(&g_commandlock);
	smsg_txaddr = (size_t)ioremap_nocache((uint32_t)info->smsg_txbase,
			info->smsg_txsize + info->smsg_rxsize + (uint32_t)AUDIO_SMSG_RINGHDR_EXT_SIZE);
	if(!smsg_txaddr){
		pr_err("%s:ioremap txbuf_addr return NULL\n", __func__);
		return -ENOMEM;
	}
	memset(smsg_txaddr, 0, info->smsg_txsize + info->smsg_rxsize + (uint32_t)AUDIO_SMSG_RINGHDR_EXT_SIZE);
	smsg_rxaddr = smsg_txaddr + info->smsg_txsize;
	sp_asoc_pr_dbg("%s: ioremap txbuf: vbase=0x%lx, pbase=0x%x, size=0x%x\n",
		__func__, smsg_txaddr, info->smsg_txbase, info->smsg_txsize);

	s_sipc_inst.dst = dst;
	s_sipc_inst.name = info->name;
	s_sipc_inst.txbuf_size = ((uint32_t)info->smsg_txsize) / ((uint32_t)sizeof(struct smsg));
	s_sipc_inst.txbuf_addr = smsg_txaddr;
	s_sipc_inst.txbuf_rdptr = smsg_txaddr + info->smsg_txsize + info->smsg_rxsize + (size_t)AUDIO_SMSG_RINGHDR_EXT_TXRDPTR;
	s_sipc_inst.txbuf_wrptr = smsg_txaddr + info->smsg_txsize + info->smsg_rxsize + (size_t)AUDIO_SMSG_RINGHDR_EXT_TXWRPTR;

	s_sipc_inst.rxbuf_size = ((uint32_t)info->smsg_rxsize) / ((uint32_t)sizeof(struct smsg));
	s_sipc_inst.rxbuf_addr = smsg_rxaddr;
	s_sipc_inst.rxbuf_rdptr = smsg_txaddr + info->smsg_txsize + info->smsg_rxsize + (size_t)AUDIO_SMSG_RINGHDR_EXT_RXRDPTR;
	s_sipc_inst.rxbuf_wrptr = smsg_txaddr + info->smsg_txsize + info->smsg_rxsize + (size_t)AUDIO_SMSG_RINGHDR_EXT_RXWRPTR;

	s_sipc_inst.target_id = info->target_id;
	/* create SIPC to target-id */
	ret = saudio_ipc_create(s_sipc_inst.dst, &s_sipc_inst);
	if (ret) {
		return -EIO;
	}
	sp_asoc_pr_info("%s successfully \n", __func__);

	return 0;
}

static int audio_sipc_destroy(uint8_t dst)
{
	int ret = 0;
	if (dst >= SIPC_ID_NR) {
		pr_err("%s, invalid dst:%d \n", __func__, dst);
		return -EINVAL;
	}
	ret = saudio_ipc_destroy(dst);
	if (ret) {
		return -EIO;
	}
	return 0;
}

static int audio_sipc_send_cmd(uint32_t dst,
			uint16_t channel, uint32_t cmd,
			uint32_t value0, uint32_t value1,
			uint32_t value2, int32_t value3,
			int32_t timeout)
{
	int ret = 0;
	struct smsg msend = { 0 };

	if (dst >= SIPC_ID_NR) {
		pr_err("%s, invalid dst:%d \n", __func__, dst);
		return -EINVAL;
	}
	sp_asoc_pr_dbg("%s, dst: %d, cmd: 0x%x, value0: 0x%x, value1: 0x%x, value2: 0x%x, value3: 0x%x, timeout: %d \n",
		__func__, dst, cmd, value0, value1, value2, value3, timeout);

	smsg_set(&msend, channel, cmd, value0, value1, value2, value3);

	ret = saudio_send(dst, &msend, timeout);
	if (ret) {
		pr_err("%s, Failed to send cmd(0x%x), ret(%d) \n", __func__, cmd, ret);
		return -EIO;
	}
	return 0;
}


static int audio_sipc_recv_cmd(uint32_t dst,
			uint16_t channel, uint32_t cmd,
			struct smsg *msg, int32_t timeout)
{
	int ret = 0;
	struct smsg mrecv = { 0 };

	if (dst >= SIPC_ID_NR) {
		pr_err("%s, invalid dst:%d \n", __func__, dst);
		return -EINVAL;
	}
	smsg_set(&mrecv, channel, 0, 0, 0, 0, 0);
    while(-ERESTARTSYS == (ret = saudio_recv(dst, &mrecv, timeout))) {
        pr_err("%s,interrupted so recv again\n", __func__);
    }
	if (ret < 0) {
		pr_err("%s, Failed to recv cmd(0x%x), dst:(%d), ret(%d) \n", __func__, cmd, dst, ret);
		return -EIO;
	}
	sp_asoc_pr_dbg("%s, dst: %d, chan: 0x%x, cmd: 0x%x, value0: 0x%x, value1: 0x%x, value2: 0x%x, value3: 0x%x, timeout: %d \n",
			__func__, dst, mrecv.channel, mrecv.command, mrecv.parameter0, mrecv.parameter1, mrecv.parameter2, mrecv.parameter3, timeout);
	if (cmd == mrecv.command && mrecv.channel == channel) {
		memcpy(msg, &mrecv, sizeof(struct smsg));
		return 0;
	} else {
		pr_err("%s, Haven't got right cmd(0x%x), got cmd(0x%x), got chan(0x%x) \n",
				__func__, cmd, mrecv.command, mrecv.channel);
		return -EIO;
	}
}

static int audio_config_sipc_iram_buf(struct audio_sipc_of_data *node_info)
{
	int i;
	int sum_size = 0;
	/* initialize the address of command para buffer */
	if (!s_iram_buf.cmd_iram_buf.txbuf_addr_v) {
		s_iram_buf.cmd_iram_buf.txbuf_addr_v =
				(size_t)ioremap_nocache(node_info->cmdaddr_base, node_info->cmdaddr_size);
		if(!s_iram_buf.cmd_iram_buf.txbuf_addr_v) {
			pr_err("%s:ioremap cmd txbuf_addr_v return NULL\n", __func__);
			return -ENOMEM;
		}
	}
	memset(s_iram_buf.cmd_iram_buf.txbuf_addr_v, 0, node_info->cmdaddr_size);
	s_iram_buf.cmd_iram_buf.txbuf_addr_p = node_info->cmdaddr_base;
	s_iram_buf.cmd_iram_buf.txbuf_size = node_info->cmdaddr_size;
	/* init the lock */
	spin_lock_init(&(s_iram_buf.cmd_iram_buf.txpinlock));

	/* initialize the address of shm buffer */
	for (i = 0; i < SND_VBC_SHM_MAX; i++) {
		s_iram_buf.shm_iram_buf.audio_shm_size[i] = node_info->shm_size[i];
		sum_size += node_info->shm_size[i];
	}
	if (!s_iram_buf.shm_iram_buf.audio_shm_addr_v[0]) {
		s_iram_buf.shm_iram_buf.audio_shm_addr_v[0] =
				(size_t)ioremap_nocache(node_info->shm_base, sum_size);
		if (!s_iram_buf.shm_iram_buf.audio_shm_addr_v[0]) {
			iounmap(s_iram_buf.cmd_iram_buf.txbuf_addr_v);
			s_iram_buf.cmd_iram_buf.txbuf_addr_v = 0;
			pr_err("%s:ioremap cmd shm buf_addr return NULL\n", __func__);
			return -ENOMEM;
		}
		memset(s_iram_buf.shm_iram_buf.audio_shm_addr_v[0], 0, sum_size);
		s_iram_buf.shm_iram_buf.audio_shm_addr_p[0] = node_info->shm_base;
	}
	for (i = 1; i < SND_VBC_SHM_MAX; i++) {
		s_iram_buf.shm_iram_buf.audio_shm_addr_v[i] = s_iram_buf.shm_iram_buf.audio_shm_addr_v[i-1]
				+ node_info->shm_size[i-1];
		s_iram_buf.shm_iram_buf.audio_shm_addr_p[i] = s_iram_buf.shm_iram_buf.audio_shm_addr_p[i-1]
				+ node_info->shm_size[i-1];
	}
	/* init the lock */
	spin_lock_init(&(s_iram_buf.shm_iram_buf.txpinlock));
	return 0;
}

static int set_sprd_audio_buffer(int type, void *buf, size_t n)
{
	unsigned long flags = 0;
	if (type >= SND_VBC_SHM_MAX) {
		pr_err("%s, ERR: don't support this type(%d) \n", __func__, type);
		return -1;
	}
	if (n > s_iram_buf.shm_iram_buf.audio_shm_size[type]) {
		pr_err("%s, ERR: buffer[%d] size(%u) out of range(%u) \n",
				__func__, type, n, s_iram_buf.shm_iram_buf.audio_shm_size[type]);
		return -1;
	}
	spin_lock_irqsave(&(s_iram_buf.shm_iram_buf.txpinlock), flags);
	if (n > s_iram_buf.shm_iram_buf.audio_shm_size[type]) {
		pr_err("%s, ERR: type(%d) wrong shm iram size(%zu), avali(%d) \n",
				__func__, type, n, s_iram_buf.shm_iram_buf.audio_shm_size[type]);
		spin_unlock_irqrestore(&(s_iram_buf.shm_iram_buf.txpinlock), flags);
		return -1;
	}
	unalign_memcpy((void *)s_iram_buf.shm_iram_buf.audio_shm_addr_v[type], buf, n);
	pr_debug("%s, write buffer: audio_shm_addr_v[%d]=0x%lx, n=%d, s_iram_buf.shm_iram_buf.audio_shm_size[type(%d)]=%#x\n",
			__func__, type, s_iram_buf.shm_iram_buf.audio_shm_addr_v[type], n, type, s_iram_buf.shm_iram_buf.audio_shm_size[type]);
	spin_unlock_irqrestore(&(s_iram_buf.shm_iram_buf.txpinlock), flags);
	return 0;
}

static int set_sprd_audio_cmd_data(uint8_t id, void *para, size_t n)
{
	unsigned long flags = 0;

	if (n > s_iram_buf.cmd_iram_buf.txbuf_size) {
		pr_err("%s, data-size(%u) out of range(%u)!\n",
				__func__, n, s_iram_buf.cmd_iram_buf.txbuf_size);
		return -1;
	}
	/* write cmd para */
	sprd_memcpy((void *)(s_iram_buf.cmd_iram_buf.txbuf_addr_v), para, n);

	sp_asoc_pr_dbg("%s, write cmd para: txbuf_addr_v=0x%lx, tx_len=%d \n",
			__func__, s_iram_buf.cmd_iram_buf.txbuf_addr_v, n);

	return 0;
}

int snd_vbc_sipc_init(struct audio_sipc_of_data *node_info)
{
	int ret =0;
	/* create sipc for audio */
	ret = audio_sipc_create(SIPC_ID_AGDSP, node_info);
	if (ret) {
		pr_err("%s: failed to create sipc, ret=%d\n", __func__, ret);
		return -ENODEV;
	}
	ret = snd_audio_sipc_ch_open(SIPC_ID_AGDSP, SMSG_CH_VBC_CTL, AUDIO_SIPC_WAIT_FOREVER);
	if (ret) {
		pr_err("%s: failed to open sipc channle, ret=%d\n", __func__, ret);
		return -ENODEV;
	}
	/* config iram buffer used */
	ret = audio_config_sipc_iram_buf(node_info);
	if (ret < 0) {
		audio_sipc_destroy(SIPC_ID_AGDSP);
		return -ENOMEM;
	}
	return 0;
}

void snd_vbc_sipc_deinit(void)
{
	snd_audio_sipc_ch_close(SIPC_ID_AGDSP, SMSG_CH_VBC_CTL, AUDIO_SIPC_WAIT_FOREVER);
	audio_sipc_destroy(SIPC_ID_AGDSP);
}

int snd_audio_sipc_ch_open(uint8_t dst, uint16_t channel, int timeout)
{
	int ret = 0;
	if (dst >= SIPC_ID_NR) {
		pr_err("%s, invalid dst:%d \n", __func__, dst);
		return -EINVAL;
	}
	ret = saudio_ch_open(dst, channel, timeout);
	if (ret != 0) {
		pr_err("%s, Failed to open channel \n", __func__);
		return -EIO;
	}
	return 0;
}

int snd_audio_sipc_ch_close(uint8_t dst, uint16_t channel, int timeout)
{
	int ret = 0;

	if (dst >= SIPC_ID_NR) {
		pr_err("%s, invalid dst:%d \n", __func__, dst);
		return -EINVAL;
	}
	ret = saudio_ch_close(dst, channel, timeout);
	if (ret != 0) {
		pr_err("%s, Failed to close channel \n", __func__);
		return -EIO;
	}
	return 0;
}

int snd_audio_recv_cmd(uint16_t channel, uint32_t cmd, uint32_t *buf, int32_t timeout)
{
	int ret = 0;
	struct smsg mrecv = { 0 };
	sp_asoc_pr_dbg("%s, cmd:%d \n",__func__, cmd);
	ret = audio_sipc_recv_cmd(SIPC_ID_AGDSP, channel, cmd,
			&mrecv, timeout);
	if (ret < 0) {
		return -EIO;
	}
	*buf = mrecv.parameter3;
	return 0;
}
/*id :   parameter0(exchange with dsp)
  *cmd: command
*/
int snd_audio_send_cmd(uint16_t channel, int id, int stream,
			uint32_t cmd, void *para, size_t n, int32_t timeout)
{
	int ret = 0;
	int value = 0;
	unsigned long flags;
	mutex_lock(&g_commandlock);
	/* set audio cmd para */
	ret = set_sprd_audio_cmd_data(id, para, n);
	if (ret < 0) {
		pr_err("%s: failed to write command(%d) para, ret=%d\n",
				__func__, cmd, ret);
		mutex_unlock(&g_commandlock);
		return -EIO;
	}
	/* send audio cmd */
	ret = audio_sipc_send_cmd(SIPC_ID_AGDSP, channel, cmd,
		id, stream, s_iram_buf.cmd_iram_buf.txbuf_addr_p, 0, timeout);
	if (ret < 0) {
		pr_err("%s: failed to send command(%d), ret=%d\n",
				__func__, cmd, ret);
		mutex_unlock(&g_commandlock);
		return -EIO;
	}
	/* wait for response */
	if(SND_VBC_DSP_FUNC_HW_TRIGGER == cmd){
		// if cmd is trigger do nothing
		sp_asoc_pr_dbg("%s  SND_VBC_DSP_FUNC_HW_TRIGGER  no recevie\n", __func__);
	}else{
		ret = snd_audio_recv_cmd(channel, cmd, &value, timeout);
		if (ret < 0) {
			pr_err("%s: failed to get command(%d), ret=%d\n",
					__func__, cmd, ret);
			mutex_unlock(&g_commandlock);
			return -EIO;
		}
	}
	mutex_unlock(&g_commandlock);
	sp_asoc_pr_info("%s out,cmd =%d id:%d ret-value:%d\n",__func__,cmd, id, value);
	return ret;
}

int snd_audio_send_cmd_norecev(uint16_t channel, int id, int stream,
			uint32_t cmd, void *para, size_t n, int32_t timeout)
{
	int ret = 0;
	int value = 0;
	unsigned long flags;
	/* send audio cmd */
	ret = audio_sipc_send_cmd(SIPC_ID_AGDSP, channel, cmd,
		id, stream, s_iram_buf.cmd_iram_buf.txbuf_addr_p, 0, timeout);
	if (ret < 0) {
		pr_err("%s: failed to send command(%d), ret=%d\n",
				__func__, cmd, ret);
		return -EIO;
	}
	sp_asoc_pr_info("%s out,id:%d no ret value\n",__func__,cmd, id);
	return ret;
}


int snd_audio_send_buffer(uint16_t channel, int id, int stream,
			int type, void *buf, size_t n, int32_t timeout)
{
	int ret = 0;
	struct sprd_audio_sharemem sharemem_info = {0};
	/* set audio para */
	ret = set_sprd_audio_buffer(type, buf, n);
	if (ret < 0) {
		pr_err("%s: failed to write (type--%d) buf, ret=%d\n",
				__func__, type, ret);
		return -EIO;
	}
	sharemem_info.id = id;
	sharemem_info.type = type;
	sharemem_info.phy_iram_addr = s_iram_buf.shm_iram_buf.audio_shm_addr_p[type];
	sharemem_info.size = s_iram_buf.shm_iram_buf.audio_shm_size[type];
	/* send audio cmd */
	sp_asoc_pr_dbg("sharemem_info.id = %d, sharemem_info.type=%d, sharemem_info.phy_iram_addr=%#x, sharemem_info.size=%#x\n",
	sharemem_info.id, sharemem_info.type, sharemem_info.phy_iram_addr, sharemem_info.size);
	ret = snd_audio_send_cmd(channel, id, stream, SND_VBC_DSP_IO_SHAREMEM_SET,
		&sharemem_info, sizeof(struct sprd_audio_sharemem), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		pr_err("%s: failed to send command(%d), ret=%d\n",
				__func__, SND_VBC_DSP_IO_SHAREMEM_SET, ret);
		return -EIO;
	}
	return 0;
}

int snd_audio_recv_buffer(uint16_t channel, int id, int stream,
			int type, struct sipc_iram_buf **buf_out, int32_t timeout)
{
	int ret = 0;
	uint32_t val = 0;
	struct sprd_audio_sharemem sharemem_info = {0};

	sharemem_info.id = id;
	sharemem_info.type = type;
	/* set iram addr where get info */
	sharemem_info.phy_iram_addr = s_iram_buf.shm_iram_buf.audio_shm_addr_p[type];
	sharemem_info.size = s_iram_buf.shm_iram_buf.audio_shm_size[type];
	/* send audio cmd */
	ret = snd_audio_send_cmd(channel, id, stream, SND_VBC_DSP_IO_SHAREMEM_GET,
		&sharemem_info, sizeof(struct sprd_audio_sharemem), timeout);
	if (ret < 0) {
		pr_err("%s: failed to send command(%d), ret=%d\n",
				__func__, SND_VBC_DSP_IO_SHAREMEM_GET, ret);
		return -EIO;
	} else {
		*buf_out = &s_iram_buf;
		if (ret < 0) {
			pr_err("%s, Failed to get, ret: %d\n", __func__, ret);
		}
	}
	return 0;
}

