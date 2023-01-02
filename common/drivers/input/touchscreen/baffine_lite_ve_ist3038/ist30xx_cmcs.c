/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/err.h>

#include "ist30xx.h"
#include "ist30xx_update.h"
#include "ist30xx_misc.h"
#include "ist30xx_cmcs.h"

#if IST30XX_INTERNAL_CMCS_BIN
#include "ist30xx_cmcs_bin.h"
#endif

extern struct ist30xx_data *ts_data;

int cmcs_ready = CMCS_READY;
u8 *ts_cmcs_bin = NULL;
u32 ts_cmcs_bin_size = 0;
CMCS_BIN_INFO ist30xx_cmcs_bin;
CMCS_BIN_INFO *ts_cmcs = (CMCS_BIN_INFO *)&ist30xx_cmcs_bin;
CMCS_BUF ist30xx_cmcs_buf;
CMCS_BUF *ts_cmcs_buf = (CMCS_BUF *)&ist30xx_cmcs_buf;


int ist30xx_parse_cmcs_bin(const u8 *buf, const u32 size)
{
	int ret = -EPERM;
	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	memcpy(ts_cmcs->magic1, buf, sizeof(ts_cmcs->magic1));
	memcpy(ts_cmcs->magic2, &buf[size - sizeof(ts_cmcs->magic2)],
	       sizeof(ts_cmcs->magic2));
	memcpy(cmcs, &buf[sizeof(ts_cmcs->magic1)], sizeof(ts_cmcs->cmcs));

	if (!strncmp(ts_cmcs->magic1, IST30XX_CMCS_MAGIC, sizeof(ts_cmcs->magic1))
	    && !strncmp(ts_cmcs->magic2, IST30XX_CMCS_MAGIC,
			sizeof(ts_cmcs->magic2))) {
		int idx;

		idx = sizeof(ts_cmcs->magic1) + sizeof(ts_cmcs->cmcs);
		ts_cmcs->buf_cmcs = (u8 *)&buf[idx];

		idx += cmcs->cmd.cmcs_size;
		ts_cmcs->buf_sensor = (u32 *)&buf[idx];

		idx += (cmcs->sensor1_size + cmcs->sensor2_size + cmcs->sensor3_size);
		ts_cmcs->buf_node = (u16 *)&buf[idx];

		ret = 0;
	}

	tsp_verb("Magic1: %s, Magic2: %s\n", ts_cmcs->magic1, ts_cmcs->magic2);
	tsp_verb(" mode: 0x%x, base(screen: %d, key: %d)\n",
		 cmcs->cmd.mode, cmcs->cmd.base_screen, cmcs->cmd.base_key);
	tsp_verb(" start_cp (cm: %d, cs: %d), vcmp (cm: %d, cs: %d)\n",
		 cmcs->cmd.start_cp_cm, cmcs->cmd.start_cp_cs,
		 cmcs->cmd.vcmp_cm, cmcs->cmd.vcmp_cs);
	tsp_verb(" timeout: %d\n", cmcs->timeout);
	tsp_verb(" baseline scrn: 0x%08x, key: 0x%08x\n",
		 cmcs->addr.base_screen, cmcs->addr.base_key);
	tsp_verb(" start_cp: 0x%08x, vcmp: 0x%08x\n",
		 cmcs->addr.start_cp, cmcs->addr.vcmp);
	tsp_verb(" sensor 1: 0x%08x, 2: 0x%08x, 3: 0x%08x\n",
		 cmcs->addr.sensor1, cmcs->addr.sensor2, cmcs->addr.sensor3);
	tsp_verb(" tx: %d, rx:%d, scr tx: %d, scr rx: %d\n",
		 cmcs->ch.tx_num, cmcs->ch.rx_num, 
         cmcs->ch.scr_tx_num, cmcs->ch.scr_rx_num);
    tsp_verb(" gtx num: %d, ch(%d, %d, %d, %d)\n",
		 cmcs->ch.gtx_use_num, cmcs->ch.gtx_ch_num[0], cmcs->ch.gtx_ch_num[1], 
         cmcs->ch.gtx_ch_num[2], cmcs->ch.gtx_ch_num[3]);
    tsp_verb(" key num: %d, ch1(%d,%d), ch2(%d,%d), ch3(%d,%d), \
         ch4(%d,%d), ch5(%d, %d)\n", cmcs->ch.key_use_num, 
         cmcs->ch.key_ch_num[0].tx, cmcs->ch.key_ch_num[0].rx, 
         cmcs->ch.key_ch_num[1].tx, cmcs->ch.key_ch_num[1].rx, 
         cmcs->ch.key_ch_num[2].tx, cmcs->ch.key_ch_num[2].rx, 
         cmcs->ch.key_ch_num[3].tx, cmcs->ch.key_ch_num[3].rx, 
         cmcs->ch.key_ch_num[4].tx, cmcs->ch.key_ch_num[4].rx);
	tsp_verb(" cr: screen(%4d, %4d), gtx(%4d, %4d), key(%4d, %4d)\n",
		 cmcs->spec_cr.screen_min, cmcs->spec_cr.screen_max,
		 cmcs->spec_cr.key_min, cmcs->spec_cr.key_max);
	tsp_verb(" cm: screen(%4d, %4d), gtx(%4d, %4d), key(%4d, %4d)\n",
		 cmcs->spec_cm.screen_min, cmcs->spec_cm.screen_max,
		 cmcs->spec_cm.key_min, cmcs->spec_cm.key_max);
	tsp_verb(" cs0: screen(%4d, %4d), gtx(%4d, %4d), key(%4d, %4d)\n",
		 cmcs->spec_cs0.screen_min, cmcs->spec_cs0.screen_max,
		 cmcs->spec_cs0.key_min, cmcs->spec_cs0.key_max);
	tsp_verb(" cs1: screen(%4d, %4d), gtx(%4d, %4d), key(%4d, %4d)\n",
		 cmcs->spec_cs1.screen_min, cmcs->spec_cs1.screen_max,
		 cmcs->spec_cs1.key_min, cmcs->spec_cs1.key_max);
	tsp_verb(" slope: x(%d, %d), y(%d, %d), gtx_x(%d, %d), gtx_y(%d, %d), key(%d, %d)\n",
		 cmcs->slope.x_min, cmcs->slope.x_max,
		 cmcs->slope.y_min, cmcs->slope.y_max,
         cmcs->slope.gtx_x_min, cmcs->slope.gtx_x_max,
         cmcs->slope.gtx_y_min, cmcs->slope.gtx_y_max,
		 cmcs->slope.key_min, cmcs->slope.key_max);
	tsp_verb(" size: cmcs(%d), sensor(%d, %d, %d)\n",
		 cmcs->cmd.cmcs_size, cmcs->sensor1_size,
		 cmcs->sensor2_size, cmcs->sensor3_size);
	tsp_verb(" checksum: cmcs: 0x%08x, sensor: 0x%08x\n",
		 cmcs->cmcs_chksum, cmcs->sensor_chksum);
	tsp_verb(" cmcs: %x, %x, %x, %x\n", ts_cmcs->buf_cmcs[0],
		 ts_cmcs->buf_cmcs[1], ts_cmcs->buf_cmcs[2], ts_cmcs->buf_cmcs[3]);
	tsp_verb(" sensor: %x, %x, %x, %x\n",
		 ts_cmcs->buf_sensor[0], ts_cmcs->buf_sensor[1],
		 ts_cmcs->buf_sensor[2], ts_cmcs->buf_sensor[3]);
	tsp_verb(" node: %x, %x, %x, %x\n",
		 ts_cmcs->buf_node[0], ts_cmcs->buf_node[1],
		 ts_cmcs->buf_node[2], ts_cmcs->buf_node[3]);

	return ret;
}

int ist30xx_get_cmcs_info(const u8 *buf, const u32 size)
{
	int ret;

	cmcs_ready = CMCS_NOT_READY;

	ret = ist30xx_parse_cmcs_bin(buf, size);
	if (unlikely(ret != TAGS_PARSE_OK))
		tsp_warn("Cannot find tags of CMCS, make a bin by 'cmcs2bin.exe'\n");

	return ret;
}

int ist30xx_set_cmcs_sensor(struct i2c_client *client, CMCS_INFO *cmcs,
			    u32 *buf32)
{
	int i, ret;
	int len;
	u32 waddr;
	u32 *tmp32;

	tsp_verb("%08x %08x %08x %08x\n", buf32[0], buf32[1], buf32[2], buf32[3]);

	waddr = cmcs->addr.sensor1;
	len = cmcs->sensor1_size / IST30XX_DATA_LEN;

	for (i = 0; i < len; i++) {
		ret = ist30xx_write_cmd(client, waddr, *buf32++);
		if (ret)
			return ret;

		waddr += IST30XX_DATA_LEN;
	}

	tmp32 = buf32;
	tsp_verb("%08x %08x %08x %08x\n", tmp32[0], tmp32[1], tmp32[2], tmp32[3]);

	waddr = cmcs->addr.sensor2;
	len = (cmcs->sensor2_size - 0x10) / IST30XX_DATA_LEN;

	for (i = 0; i < len; i++) {
		ret = ist30xx_write_cmd(client, waddr, *buf32++);
		if (ret)
			return ret;

		waddr += IST30XX_DATA_LEN;
	}
	buf32 += (0x10 / IST30XX_DATA_LEN);

	tmp32 = buf32;
	tsp_verb("%08x %08x %08x %08x\n", tmp32[0], tmp32[1], tmp32[2], tmp32[3]);

	waddr = cmcs->addr.sensor3;
	len = cmcs->sensor3_size / IST30XX_DATA_LEN;

	for (i = 0; i < len; i++) {
		ret = ist30xx_write_cmd(client, waddr, *buf32++);
		if (ret)
			return ret;

		waddr += IST30XX_DATA_LEN;
	}

	return 0;
}

int ist30xx_set_cmcs_cmd(struct i2c_client *client, CMCS_INFO *cmcs)
{
	int ret;
	u32 val;

	val = (u32)(cmcs->cmd.base_screen | (cmcs->cmd.mode << 16));
	ret = ist30xx_write_cmd(client, cmcs->addr.base_screen, val);    
	if (ret)
		return ret;
	tsp_verb("Baseline screen(0x%08x): 0x%08x\n", cmcs->addr.base_screen, val);

	val = (u32)cmcs->cmd.base_key;
	ret = ist30xx_write_cmd(client, cmcs->addr.base_key, val);
	if (ret)
		return ret;
	tsp_verb("Baseline key(0x%08x): 0x%08x\n", cmcs->addr.base_key, val);

	val = cmcs->cmd.start_cp_cm | (cmcs->cmd.start_cp_cs << 16);
	ret = ist30xx_write_cmd(client, cmcs->addr.start_cp, val);
	if (ret)
		return ret;
	tsp_verb("StartCP(0x%08x): 0x%08x\n", cmcs->addr.start_cp, val);

	val = cmcs->cmd.vcmp_cm | (cmcs->cmd.vcmp_cs << 16);
	ret = ist30xx_write_cmd(client, cmcs->addr.vcmp, val);
	if (ret)
		return ret;
	tsp_verb("VCMP(0x%08x): 0x%08x\n", cmcs->addr.vcmp, val);

	return 0;
}

int ist30xx_parse_cmcs_buf(CMCS_INFO *cmcs, s16 *buf)
{
	int i, j;

	tsp_info(" %d * %d\n", cmcs->ch.tx_num, cmcs->ch.rx_num);
	for (i = 0; i < cmcs->ch.tx_num; i++) {
		tsp_info(" ");
		for (j = 0; j < cmcs->ch.rx_num; j++)
			printk("%5d ", buf[i * cmcs->ch.rx_num + j]);
		printk("\n");
	}

	return 0;
}

int ist30xx_apply_cmcs_slope(CMCS_INFO *cmcs, CMCS_BUF *cmcs_buf)
{
	int i, j;
	int idx1, idx2;
	int ch_num = cmcs->ch.tx_num * cmcs->ch.rx_num;
	int width = cmcs->ch.scr_rx_num;
	int height = cmcs->ch.scr_tx_num;
	s16 *pcm = (s16 *)cmcs_buf->cm;
	s16 *pspec = (s16 *)cmcs_buf->spec;
	s16 *pslope0 = (s16 *)cmcs_buf->slope0;
	s16 *pslope1 = (s16 *)cmcs_buf->slope1;

	memset(cmcs_buf->slope0, 0, sizeof(cmcs_buf->slope0));
	memset(cmcs_buf->slope1, 0, sizeof(cmcs_buf->slope1));

	memcpy(cmcs_buf->spec, ts_cmcs->buf_node, (ch_num * sizeof(u16)));

	idx1 = 0;
#if CMCS_PARSING_DEBUG
	tsp_info("# Node specific\n");
	for (i = 0; i < cmcs->ch.tx_num; i++) {
		tsp_info(" ");
		for (j = 0; j < cmcs->ch.rx_num; j++)
			printk("%5d ", cmcs_buf->spec[idx1++]);
		printk("\n");
	}
#endif

	tsp_verb("# Apply slope0_x\n");
	for (i = 0; i < height; i++) {
		for (j = 0; j < width - 1; j++) {
			idx1 = (i * cmcs->ch.rx_num) + j;
			idx2 = idx1 + 1;

			pslope0[idx1] = (pcm[idx2] - pcm[idx1]);
			pslope0[idx1] += (pspec[idx1] - pspec[idx2]);
		}
	}

	tsp_verb("# Apply slope1_y\n");
	for (i = 0; i < height - 1; i++) {
		for (j = 0; j < width; j++) {
			idx1 = (i * cmcs->ch.rx_num) + j;
			idx2 = idx1 + cmcs->ch.rx_num;

			pslope1[idx1] = (pcm[idx2] - pcm[idx1]);
			pslope1[idx1] += (pspec[idx1] - pspec[idx2]);
		}
	}

	tsp_verb("# Apply slope0_gtx_x\n");
    for (i = 0; i < cmcs->ch.gtx_use_num; i++) {
        if (cmcs->ch.gtx_ch_num[i] > height) {
            for (j = 0; j < width - 1; j++) {
                idx1 = (cmcs->ch.gtx_ch_num[i] * cmcs->ch.rx_num) + j;
                idx2 = idx1 + 1;

                pslope0[idx1] = (pcm[idx2] - pcm[idx1]);
    			pslope0[idx1] += (pspec[idx1] - pspec[idx2]);
            }
        }
    }

    tsp_verb("# Apply slope1_gtx_y\n");
    for (i = 0; i < (cmcs->ch.gtx_use_num - 1); i++) {
        if (cmcs->ch.gtx_ch_num[i] > height) {
            for (j = 0; j < width; j++) {
                idx1 = (cmcs->ch.gtx_ch_num[i] * cmcs->ch.rx_num) + j;
                idx2 = idx1 + cmcs->ch.rx_num;

                pslope1[idx1] = (pcm[idx2] - pcm[idx1]);
      			pslope1[idx1] += (pspec[idx1] - pspec[idx2]);
            }
        }
    }

#if CMCS_PARSING_DEBUG
	tsp_info("# slope0_x\n");
	for (i = 0; i < height; i++) {
		tsp_info(" ");
		for (j = 0; j < width; j++) {
			idx1 = (i * cmcs->ch.rx_num) + j;
			printk("%5d ", pslope0[idx1]);
		}
		printk("\n");
	}

	tsp_info("# slope1_y\n");
	for (i = 0; i < height; i++) {
		tsp_info(" ");
		for (j = 0; j < width; j++) {
			idx1 = (i * cmcs->ch.rx_num) + j;
			printk("%5d ", pslope1[idx1]);
		}
		printk("\n");
	}

    tsp_info("# slope0_gtx_x\n");
    for (i = 0; i < cmcs->ch.gtx_use_num; i++) {
		tsp_info(" ");
		for (j = 0; j < width; j++) {
			idx1 = (cmcs->ch.gtx_ch_num[i] * cmcs->ch.rx_num) + j;
			printk("%5d ", pslope0[idx1]);
		}
		printk("\n");
	}

    tsp_info("# slope1_gtx_y\n");
	for (i = 0; i < cmcs->ch.gtx_use_num; i++) {
		tsp_info(" ");
		for (j = 0; j < width; j++) {
			idx1 = (cmcs->ch.gtx_ch_num[i] * cmcs->ch.rx_num) + j;
			printk("%5d ", pslope1[idx1]);
		}
		printk("\n");
	}

#endif

	return 0;
}


int ist30xx_get_cmcs_buf(struct i2c_client *client, CMCS_INFO *cmcs, s16 *buf)
{
	int ret;
	u16 len = (IST30XX_CMCS_BUF_SIZE * sizeof(buf[0])) / IST30XX_DATA_LEN;

	ret = ist30xx_read_buf(client, CMD_DEFAULT, (u32 *)buf, len);
	if (unlikely(ret))
		return ret;

#if CMCS_PARSING_DEBUG
	ret = ist30xx_parse_cmcs_buf(cmcs, buf);
#endif

	return ret;
}


extern int ist30xx_calib_wait(void);
#define cmcs_next_step(ret)   { if (unlikely(ret)) goto end; msleep(10); }
int ist30xx_cmcs_test(const u8 *buf, int size)
{
	int ret;
	int len;
	u32 chksum = 0;
	u32 *buf32;
	struct i2c_client *client = (struct i2c_client *)ts_data->client;
	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	tsp_info("*** CM/CS test ***\n");
	tsp_info(" mode: 0x%x, baseline(screen: %d, key: %d)\n",
		 cmcs->cmd.mode, cmcs->cmd.base_screen, cmcs->cmd.base_key);
	tsp_info(" start_cp (cm: %d, cs: %d), vcmp (cm: %d, cs: %d)\n",
		 cmcs->cmd.start_cp_cm, cmcs->cmd.start_cp_cs,
		 cmcs->cmd.vcmp_cm, cmcs->cmd.vcmp_cs);

	ist30xx_disable_irq(ts_data);

	ist30xx_reset(false);

	ret = ist30xx_cmd_reg(client, CMD_ENTER_REG_ACCESS);
	cmcs_next_step(ret);

	/* Set sensor register */
	buf32 = ts_cmcs->buf_sensor;
	ret = ist30xx_set_cmcs_sensor(client, cmcs, buf32);
	cmcs_next_step(ret);

	/* Set command */
	ret = ist30xx_set_cmcs_cmd(client, cmcs);
	cmcs_next_step(ret);

	ret = ist30xx_cmd_reg(client, CMD_EXIT_REG_ACCESS);
	cmcs_next_step(ret);

	/* Load cmcs test code */
	ret = ist30xx_write_cmd(client, CMD_EXEC_MEM_CODE, 0);
	cmcs_next_step(ret);

	buf32 = (u32 *)ts_cmcs->buf_cmcs;
	len = cmcs->cmd.cmcs_size / IST30XX_DATA_LEN;
	tsp_verb("%08x %08x %08x %08x\n", buf32[0], buf32[1], buf32[2], buf32[3]);
	ret = ist30xx_write_buf(client, len, buf32, len);
	cmcs_next_step(ret);

	/* Check checksum */
	ret = ist30xx_read_cmd(client, CMD_DEFAULT, &chksum);
	cmcs_next_step(ret);
	if (chksum != IST30XX_CMCS_LOAD_END)
		goto end;
	tsp_info("CM/CS code ready!!\n");

	/* Check checksum */
	ret = ist30xx_read_cmd(client, CMD_DEFAULT, &chksum);
	cmcs_next_step(ret);
	tsp_info("CM/CS code chksum: %08x, %08x\n", chksum, cmcs->cmcs_chksum);

	ist30xx_enable_irq(ts_data);
	/* Wait CMCS test result */
	if (ist30xx_calib_wait() == 1)
		tsp_info("CM/CS test OK.\n");
	else
		tsp_info("CM/CS test fail.\n");
	ist30xx_disable_irq(ts_data);

	/* Read CM/CS data*/
	if (ENABLE_CM_MODE(cmcs->cmd.mode)) {
		/* Read CM data */
		memset(ts_cmcs_buf->cm, 0, sizeof(ts_cmcs_buf->cm));

		ret = ist30xx_get_cmcs_buf(client, cmcs, ts_cmcs_buf->cm);
		cmcs_next_step(ret);

		ret = ist30xx_apply_cmcs_slope(cmcs, ts_cmcs_buf);
	}

	if (ENABLE_CS_MODE(cmcs->cmd.mode)) {
		/* Read CS0 data */
		memset(ts_cmcs_buf->cs0, 0, sizeof(ts_cmcs_buf->cs0));
		memset(ts_cmcs_buf->cs1, 0, sizeof(ts_cmcs_buf->cs1));

		ret = ist30xx_get_cmcs_buf(client, cmcs, ts_cmcs_buf->cs0);
		cmcs_next_step(ret);

		/* Read CS1 data */
		ret = ist30xx_get_cmcs_buf(client, cmcs, ts_cmcs_buf->cs1);
		cmcs_next_step(ret);
	}

	ist30xx_reset(false);

	ist30xx_start(ts_data);

	cmcs_ready = CMCS_READY;

end:
	if (unlikely(ret)) {
		tsp_warn("CM/CS test Fail!, ret=%d\n", ret);
	} else if (unlikely(chksum != cmcs->cmcs_chksum)) {
		tsp_warn("Error CheckSum: %x(%x)\n", chksum, cmcs->cmcs_chksum);
		ret = -ENOEXEC;
	}

	ist30xx_enable_irq(ts_data);

	return ret;
}


int check_tsp_type(int tx, int rx)
{
    int i;
	struct CMCS_CH_INFO *ch = (struct CMCS_CH_INFO *)&ts_cmcs->cmcs.ch;

	if ((tx >= ch->tx_num) || (tx < 0) || (rx >= ch->rx_num) || (rx < 0)) {
		tsp_warn("TSP channel is not correct!! (%d * %d)\n", tx, rx);
		return TSP_CH_UNKNOWN;
	}

	if ((tx >= ch->scr_tx_num) || (rx >= ch->scr_rx_num)) {
		for (i = 0; i < ch->gtx_use_num; i++) {
			if ((tx == ch->gtx_ch_num[i]) && (rx < ch->scr_rx_num))
				return TSP_CH_GTX;
		}

		for (i = 0; i < ch->key_use_num; i++) {
			if ((tx == ch->key_ch_num[i].tx) &&
			    (rx == ch->key_ch_num[i].rx))
                return TSP_CH_KEY;
		}
	} else {
		return TSP_CH_SCREEN;
	}
    
    return TSP_CH_UNUSED;
}

int print_cmcs(s16 *buf16, char *buf)
{
	int i, j;
	int idx;
	int count = 0;
	char msg[128];

	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	int tx_num = cmcs->ch.tx_num;
	int rx_num = cmcs->ch.rx_num;

	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			idx = (i * cmcs->ch.rx_num) + j;
			count += sprintf(msg, "%5d ", buf16[idx]);
			strcat(buf, msg);
		}

		count += sprintf(msg, "\n");
		strcat(buf, msg);
	}

	return count;
}

int print_line_cmcs(int mode, s16 *buf16, char *buf)
{
	int i, j;
	int idx;
	int type;
	int count = 0;
	int key_index[5] = { 0, };
	int key_cnt = 0;
	char msg[128];

	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	int tx_num = cmcs->ch.tx_num;
	int rx_num = cmcs->ch.rx_num;

	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			type = check_tsp_type(i, j);
			if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED))
				continue;   // Ignore

			if ((mode == CMCS_FLAG_CM_SLOPE0) &&
                    (j == (cmcs->ch.scr_rx_num - 1)))
				continue;
			else if ((mode == CMCS_FLAG_CM_SLOPE1) && 
                    (i == (cmcs->ch.scr_tx_num - 1)))
				continue;

            if ((mode == CMCS_FLAG_CM_SLOPE0) && (type == TSP_CH_GTX) && 
                    (j == (cmcs->ch.scr_rx_num - 1)))
                continue;
            else if ((mode == CMCS_FLAG_CM_SLOPE1) && 
                    (type == TSP_CH_GTX) && 
                    (i == cmcs->ch.gtx_ch_num[cmcs->ch.gtx_use_num - 1]))
				continue;

			idx = (i * cmcs->ch.rx_num) + j;            

			if (type == TSP_CH_KEY) {
				key_index[key_cnt++] = idx;
				continue;
			}

			count += sprintf(msg, "%5d ", buf16[idx]);
			strcat(buf, msg);
		}
	}

	tsp_info("key cnt: %d\n", key_cnt);
	if ((mode != CMCS_FLAG_CM_SLOPE0) && (mode != CMCS_FLAG_CM_SLOPE1)) {
		tsp_info("key cnt: %d\n", key_cnt);
		for (i = 0; i < key_cnt; i++) {
			count += sprintf(msg, "%5d ", buf16[key_index[i]]);
			strcat(buf, msg);
		}
	}

	count += sprintf(msg, "\n");
	strcat(buf, msg);

	return count;
}

/* sysfs: /sys/class/touch/cmcs/info */
ssize_t ist30xx_cmcs_info_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int count;
	char msg[128];

	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if (cmcs == NULL)
		return sprintf(buf, "Unknown cmcs bin\n");

	/* Mode */
	count = sprintf(msg, "%d ", cmcs->cmd.mode);
	strcat(buf, msg);

	/* Channel */
	count += sprintf(msg, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d ",
			 cmcs->ch.tx_num, cmcs->ch.rx_num, cmcs->ch.scr_tx_num, 
             cmcs->ch.scr_rx_num, cmcs->ch.gtx_use_num, 
             cmcs->ch.gtx_ch_num[0], cmcs->ch.gtx_ch_num[1], 
             cmcs->ch.gtx_ch_num[2], cmcs->ch.gtx_ch_num[3], 
             cmcs->ch.key_use_num, cmcs->ch.key_ch_num[0].tx, 
             cmcs->ch.key_ch_num[0].rx, cmcs->ch.key_ch_num[1].tx, 
             cmcs->ch.key_ch_num[1].rx, cmcs->ch.key_ch_num[2].tx, 
             cmcs->ch.key_ch_num[2].rx, cmcs->ch.key_ch_num[3].tx, 
             cmcs->ch.key_ch_num[3].rx, cmcs->ch.key_ch_num[4].tx, 
             cmcs->ch.key_ch_num[4].rx);
	strcat(buf, msg);

	/* Slope */
	count += sprintf(msg, "%d %d %d %d %d %d %d %d %d %d ",
			 cmcs->slope.x_min, cmcs->slope.x_max,
			 cmcs->slope.y_min, cmcs->slope.y_max,
             cmcs->slope.gtx_x_min, cmcs->slope.gtx_x_max,
			 cmcs->slope.gtx_y_min, cmcs->slope.gtx_y_max,
			 cmcs->slope.key_min, cmcs->slope.key_max);
	strcat(buf, msg);

	/* CM */
	count += sprintf(msg, "%d %d %d %d %d %d ",
			 cmcs->spec_cm.screen_min, cmcs->spec_cm.screen_max,
             cmcs->spec_cm.gtx_min, cmcs->spec_cm.gtx_max,
			 cmcs->spec_cm.key_min, cmcs->spec_cm.key_max);
	strcat(buf, msg);

	/* CS0 */
	count += sprintf(msg, "%d %d %d %d %d %d ",
			 cmcs->spec_cs0.screen_min, cmcs->spec_cs0.screen_max,
             cmcs->spec_cs0.gtx_min, cmcs->spec_cs0.gtx_max,
			 cmcs->spec_cs0.key_min, cmcs->spec_cs0.key_max);
	strcat(buf, msg);

	/* CS1 */
	count += sprintf(msg, "%d %d %d %d %d %d ",
			 cmcs->spec_cs1.screen_min, cmcs->spec_cs1.screen_max,
             cmcs->spec_cs1.gtx_min, cmcs->spec_cs1.gtx_max,
			 cmcs->spec_cs1.key_min, cmcs->spec_cs1.key_max);
	strcat(buf, msg);

	/* CR */
	count += sprintf(msg, "%d %d %d %d %d %d ",
			 cmcs->spec_cr.screen_min, cmcs->spec_cr.screen_max,
             cmcs->spec_cr.gtx_min, cmcs->spec_cr.gtx_max,
			 cmcs->spec_cr.key_min, cmcs->spec_cr.key_max);
	strcat(buf, msg);

	tsp_verb("%s\n", buf);

	return count;
}

/* sysfs: /sys/class/touch/cmcs/cmcs_binary */
ssize_t ist30xx_cmcs_binary_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int ret;

	if ((ts_cmcs_bin == NULL) || (ts_cmcs_bin_size == 0))
		return sprintf(buf, "Binary is not correct(%d)\n", ts_cmcs_bin_size);

	ist30xx_get_cmcs_info(ts_cmcs_bin, ts_cmcs_bin_size);

	mutex_lock(&ist30xx_mutex);
	ret = ist30xx_cmcs_test(ts_cmcs_bin, ts_cmcs_bin_size);
	mutex_unlock(&ist30xx_mutex);

	return sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n"));
}

/* sysfs: /sys/class/touch/cmcs/cmcs_custom */
ssize_t ist30xx_cmcs_custom_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int ret;
	int bin_size = 0;
	u8 *bin = NULL;
	const struct firmware *req_bin = NULL;

	ret = request_firmware(&req_bin, IST30XXB_CMCS_NAME, &ts_data->client->dev);
	if (ret)
		return sprintf(buf, "File not found, %s\n", IST30XXB_CMCS_NAME);

	bin = (u8 *)req_bin->data;
	bin_size = (u32)req_bin->size;

	ist30xx_get_cmcs_info(bin, bin_size);

	mutex_lock(&ist30xx_mutex);
	ret = ist30xx_cmcs_test(bin, bin_size);
	mutex_unlock(&ist30xx_mutex);

	release_firmware(req_bin);

	tsp_info("size: %d\n", sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n")));

	return sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n"));
}

#define MAX_FILE_PATH   255
/* sysfs: /sys/class/touch/cmcs/cmcs_sdcard */
ssize_t ist30xx_cmcs_sdcard_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int bin_size = 0;
	u8 *bin = NULL;
	const u8 *buff = 0;
	mm_segment_t old_fs = { 0 };
	struct file *fp = NULL;
	long fsize = 0, nread = 0;
	char fw_path[MAX_FILE_PATH];

	old_fs = get_fs();
	set_fs(get_ds());

	snprintf(fw_path, MAX_FILE_PATH, "/sdcard/touch/firmware/%s",
		 IST30XXB_CMCS_NAME);
	fp = filp_open(fw_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		tsp_info("file %s open error:%d\n", fw_path, (s32)fp);
		ret = -ENOENT;
		goto err_file_open;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;

	buff = kzalloc((size_t)fsize, GFP_KERNEL);
	if (!buff) {
		tsp_info("fail to alloc buffer\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	nread = vfs_read(fp, (char __user *)buff, fsize, &fp->f_pos);
	if (nread != fsize) {
		tsp_info("mismatch fw size\n");

		goto err_fw_size;
	}

	bin = (u8 *)buff;
	bin_size = (u32)fsize;

	filp_close(fp, current->files);
	set_fs(old_fs);
	tsp_info("firmware is loaded!!\n");

	ist30xx_get_cmcs_info(bin, bin_size);

	mutex_lock(&ist30xx_mutex);
	ret = ist30xx_cmcs_test(bin, bin_size);
	mutex_unlock(&ist30xx_mutex);

	kfree(buff);

	tsp_info("size: %d\n", sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n")));

	return sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n"));

err_fw_size:
	if (buff)
		kfree(buff);
err_alloc:
	if (fp)
		filp_close(fp, NULL);
err_file_open:
	set_fs(old_fs);

	return 0;
}

/* sysfs: /sys/class/touch/cmcs/cm */
ssize_t ist30xx_cm_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	tsp_verb("CM (%d * %d)\n", cmcs->ch.tx_num, cmcs->ch.rx_num);

	return print_cmcs(ts_cmcs_buf->cm, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_spec */
ssize_t ist30xx_cm_spec_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	tsp_verb("CM Spec (%d * %d)\n", cmcs->ch.tx_num, cmcs->ch.rx_num);

	return print_cmcs(ts_cmcs_buf->spec, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_slope0 */
ssize_t ist30xx_cm_slope0_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	tsp_verb("CM Slope0_X (%d * %d)\n", cmcs->ch.tx_num, cmcs->ch.rx_num);

	return print_cmcs(ts_cmcs_buf->slope0, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_slope1 */
ssize_t ist30xx_cm_slope1_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	tsp_verb("CM Slope1_Y (%d * %d)\n",
		 ts_cmcs->cmcs.ch.tx_num, ts_cmcs->cmcs.ch.rx_num);

	return print_cmcs(ts_cmcs_buf->slope1, buf);
}

/* sysfs: /sys/class/touch/cmcs/cs0 */
ssize_t ist30xx_cs0_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CS))
		return 0;

	tsp_verb("CS0 (%d * %d)\n", cmcs->ch.tx_num, cmcs->ch.rx_num);

	return print_cmcs(ts_cmcs_buf->cs0, buf);
}

/* sysfs: /sys/class/touch/cmcs/cs1 */
ssize_t ist30xx_cs1_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CS))
		return 0;

	tsp_verb("CS1 (%d * %d)\n", cmcs->ch.tx_num, cmcs->ch.rx_num);

	return print_cmcs(ts_cmcs_buf->cs1, buf);
}


int print_cm_slope_result(u8 flag, s16 *buf16, char *buf)
{
	int i, j;
	int type, idx;
	int count = 0, err_cnt = 0;
	int min_spec, max_spec, gtx_min_spec, gtx_max_spec;

	char msg[128];

	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;
	struct CMCS_SLOPE_INFO *spec = (struct CMCS_SLOPE_INFO *)&cmcs->slope;

	if (flag == CMCS_FLAG_CM_SLOPE0) {
		min_spec = spec->x_min;
		max_spec = spec->x_max;
        gtx_min_spec = spec->gtx_x_min;
		gtx_max_spec = spec->gtx_x_max;
	} else if (flag == CMCS_FLAG_CM_SLOPE1) {
		min_spec = spec->y_min;
		max_spec = spec->y_max;
        gtx_min_spec = spec->gtx_y_min;
		gtx_max_spec = spec->gtx_y_max;
	} else {
		count = sprintf(msg, "Unknown flag: %d\n", flag);
		strcat(buf, msg);
		return count;
	}

	count = sprintf(msg, "Spec: screen(%d~%d), GTx(%d~%d)\n", 
            min_spec, max_spec, gtx_min_spec, gtx_max_spec);
	strcat(buf, msg);

	for (i = 0; i < cmcs->ch.tx_num; i++) {
		for (j = 0; j < cmcs->ch.rx_num; j++) {
			idx = (i * cmcs->ch.rx_num) + j;

			type = check_tsp_type(i, j);
			if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED) || 
                (type == TSP_CH_KEY))
				continue;   // Ignore

            if (type == TSP_CH_SCREEN) {
    			if ((buf16[idx] > min_spec) && (buf16[idx] < max_spec))
	    			continue;   // OK
            } else if (type == TSP_CH_GTX) {
                if ((buf16[idx] > gtx_min_spec) && (buf16[idx] < gtx_max_spec))
	    			continue;   // OK
            }

			count += sprintf(msg, "%2d,%2d:%4d\n", i, j, buf16[idx]);
			strcat(buf, msg);

			err_cnt++;
		}
	}

	/* Check error count */
	if (err_cnt == 0)
		count += sprintf(msg, "OK\n");
	else
		count += sprintf(msg, "Fail, node count: %d\n", err_cnt);
	strcat(buf, msg);

	return count;
}

int print_cm_key_slope_result(s16 *buf16, char *buf, bool print)
{
	int i, j;
	int type, idx;
	int count = 0;
	int min_spec, max_spec;
	int key_num = 0;
	s16 key_cm[5] = { 0, };
	s16 slope_result;

	char msg[128];

	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;
	struct CMCS_SLOPE_INFO *spec = (struct CMCS_SLOPE_INFO *)&cmcs->slope;

	min_spec = spec->key_min;
	max_spec = spec->key_max;

	if (print) {
		count = sprintf(msg, "Spec: %d ~ %d\n", min_spec, max_spec);
		strcat(buf, msg);
	}

	for (i = 0; i < cmcs->ch.tx_num; i++) {
		for (j = 0; j < cmcs->ch.rx_num; j++) {
			idx = (i * cmcs->ch.rx_num) + j;

			type = check_tsp_type(i, j);
			if (type == TSP_CH_KEY)
				key_cm[key_num++] = buf16[idx];
		}
	}

	/* Check key slope */
	slope_result = key_cm[0] - key_cm[1];

	if (print) {
		if ((slope_result > min_spec) && (slope_result < max_spec))
			count += sprintf(msg, "OK (%d)\n", slope_result);
		else
			count += sprintf(msg, "Fail (%d)\n", slope_result);
	} else {
		count += sprintf(msg, "%d\n",
				 (slope_result > 0 ? slope_result : -slope_result));
	}

	strcat(buf, msg);

	return count;
}

int print_cs_result(s16 *buf16, char *buf, int cs_num)
{
	int i, j;
	int type, idx;
	int count = 0, err_cnt = 0;
	int min_spec, max_spec;

	char msg[128];

	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;
	struct CMCS_SPEC_INFO *spec;

	if (cs_num == 0)
		spec = (struct CMCS_SPEC_INFO *)&cmcs->spec_cs0;
	else
		spec = (struct CMCS_SPEC_INFO *)&cmcs->spec_cs1;

	count = sprintf(msg, "Spec: screen(%d~%d), gtx(%d~%d), key(%d~%d)\n",
			spec->screen_min, spec->screen_max, spec->gtx_min, 
            spec->gtx_max, spec->key_min, spec->key_max);
	strcat(buf, msg);

	for (i = 0; i < cmcs->ch.tx_num; i++) {
		for (j = 0; j < cmcs->ch.rx_num; j++) {
			idx = (i * cmcs->ch.rx_num) + j;

			type = check_tsp_type(i, j);
			if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED))
				continue;   // Ignore

			if (type == TSP_CH_SCREEN) {
				min_spec = spec->screen_min;
				max_spec = spec->screen_max;
            } else if (type == TSP_CH_GTX) {
                min_spec = spec->gtx_min;
				max_spec = spec->gtx_max;
			} else {    // TSP_CH_KEY
				min_spec = spec->key_min;
				max_spec = spec->key_max;
			}

			if ((buf16[idx] > min_spec) && (buf16[idx] < max_spec))
				continue;   // OK

			count += sprintf(msg, "%2d,%2d:%4d\n", i, j, buf16[idx]);
			strcat(buf, msg);

			err_cnt++;
		}
	}

	/* Check error count */
	if (err_cnt == 0)
		count += sprintf(msg, "OK\n");
	else
		count += sprintf(msg, "Fail, node count: %d\n", err_cnt);
	strcat(buf, msg);

	return count;
}

/* sysfs: /sys/class/touch/cmcs/cm_result */
ssize_t ist30xx_cm_result_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int i, j;
	int type, idx, err_cnt = 0;
	int min_spec, max_spec;
	int count = 0;
	short cm;
	char msg[128];

	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	for (i = 0; i < cmcs->ch.tx_num; i++) {
		for (j = 0; j < cmcs->ch.rx_num; j++) {
			idx = (i * cmcs->ch.rx_num) + j;

			type = check_tsp_type(i, j);
			if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED))
				continue;

			min_spec = max_spec = ts_cmcs_buf->spec[idx];
			if (type == TSP_CH_SCREEN) {
				min_spec -= (min_spec * cmcs->spec_cm.screen_min / 100);
				max_spec += (min_spec * cmcs->spec_cm.screen_max / 100);
            } else if (type == TSP_CH_GTX) {
				min_spec -= (min_spec * cmcs->spec_cm.gtx_min / 100);
				max_spec += (min_spec * cmcs->spec_cm.gtx_max / 100);
			} else {    // TSP_CH_KEY
				min_spec -= (min_spec * cmcs->spec_cm.key_min / 100);
				max_spec += (min_spec * cmcs->spec_cm.key_max / 100);
			}

			cm = ts_cmcs_buf->cm[idx];
			if ((cm > min_spec) && (cm < max_spec))
				continue; // OK

			count += sprintf(msg, "%2d,%2d:%4d (%4d~%4d)\n",
					 i, j, cm, min_spec, max_spec);
			strcat(buf, msg);

			err_cnt++;
		}
	}

	/* Check error count */
	if (err_cnt == 0)
		count += sprintf(msg, "OK\n");
	else
		count += sprintf(msg, "Fail, node count: %d\n", err_cnt);
	strcat(buf, msg);

	return count;
}

/* sysfs: /sys/class/touch/cmcs/cm_slope0_result */
ssize_t ist30xx_cm_slope0_result_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	return print_cm_slope_result(CMCS_FLAG_CM_SLOPE0,
				     ts_cmcs_buf->slope0, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_slope1_result */
ssize_t ist30xx_cm_slope1_result_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	return print_cm_slope_result(CMCS_FLAG_CM_SLOPE1,
				     ts_cmcs_buf->slope1, buf);
}
/* sysfs: /sys/class/touch/cmcs/cm_key_slope_result */
ssize_t ist30xx_cm_key_slope_result_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	return print_cm_key_slope_result(ts_cmcs_buf->cm, buf, true);
}

/* sysfs: /sys/class/touch/cmcs/cs0_result */
ssize_t ist30xx_cs0_result_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CS))
		return 0;

	return print_cs_result(ts_cmcs_buf->cs0, buf, 0);
}

/* sysfs: /sys/class/touch/cmcs/cs1_result */
ssize_t ist30xx_cs1_result_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CS))
		return 0;

	return print_cs_result(ts_cmcs_buf->cs1, buf, 1);
}

/* sysfs: /sys/class/touch/cmcs/line_cm */
ssize_t ist30xx_line_cm_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	return print_line_cmcs(CMCS_FLAG_CM, ts_cmcs_buf->cm, buf);
}

/* sysfs: /sys/class/touch/cmcs/line_cm_slope0 */
ssize_t ist30xx_line_cm_slope0_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	return print_line_cmcs(CMCS_FLAG_CM_SLOPE0, ts_cmcs_buf->slope0, buf);
}

/* sysfs: /sys/class/touch/cmcs/line_cm_slope1 */
ssize_t ist30xx_line_cm_slope1_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return 0;

	return print_line_cmcs(CMCS_FLAG_CM_SLOPE1, ts_cmcs_buf->slope1, buf);
}

/* sysfs: /sys/class/touch/cmcs/line_cs0 */
ssize_t ist30xx_line_cs0_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CS))
		return 0;

	return print_line_cmcs(CMCS_FLAG_CS0, ts_cmcs_buf->cs0, buf);
}

/* sysfs: /sys/class/touch/cmcs/line_cs1 */
ssize_t ist30xx_line_cs1_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	CMCS_INFO *cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CS))
		return 0;

	return print_line_cmcs(CMCS_FLAG_CS1, ts_cmcs_buf->cs1, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_key_slope_value */
ssize_t ist30xx_cm_key_slope_value_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int ret;
	CMCS_INFO *cmcs;

	if ((ts_cmcs_bin == NULL) || (ts_cmcs_bin_size == 0))
		return sprintf(buf, "%s", "FFFF");

	ist30xx_get_cmcs_info(ts_cmcs_bin, ts_cmcs_bin_size);

	mutex_lock(&ist30xx_mutex);
	ret = ist30xx_cmcs_test(ts_cmcs_bin, ts_cmcs_bin_size);
	mutex_unlock(&ist30xx_mutex);

	if (ret)
		return sprintf(buf, "%s", "FFFF");

	cmcs = (CMCS_INFO *)&ts_cmcs->cmcs;

	if (cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "%s", "FFFF");

	if ((cmcs->cmd.mode) && !(cmcs->cmd.mode & FLAG_ENABLE_CM))
		return sprintf(buf, "%s", "FFFF");

	return print_cm_key_slope_result(ts_cmcs_buf->cm, buf, false);
}

/* sysfs : cmcs */
static DEVICE_ATTR(info, S_IRUGO, ist30xx_cmcs_info_show, NULL);
static DEVICE_ATTR(cmcs_binary, S_IRUGO, ist30xx_cmcs_binary_show, NULL);
static DEVICE_ATTR(cmcs_custom, S_IRUGO, ist30xx_cmcs_custom_show, NULL);
static DEVICE_ATTR(cmcs_sdcard, S_IRUGO, ist30xx_cmcs_sdcard_show, NULL);
static DEVICE_ATTR(cm, S_IRUGO, ist30xx_cm_show, NULL);
static DEVICE_ATTR(cm_spec, S_IRUGO, ist30xx_cm_spec_show, NULL);
static DEVICE_ATTR(cm_slope0, S_IRUGO, ist30xx_cm_slope0_show, NULL);
static DEVICE_ATTR(cm_slope1, S_IRUGO, ist30xx_cm_slope1_show, NULL);
static DEVICE_ATTR(cs0, S_IRUGO, ist30xx_cs0_show, NULL);
static DEVICE_ATTR(cs1, S_IRUGO, ist30xx_cs1_show, NULL);
static DEVICE_ATTR(cm_result, S_IRUGO, ist30xx_cm_result_show, NULL);
static DEVICE_ATTR(cm_slope0_result, S_IRUGO,
		   ist30xx_cm_slope0_result_show, NULL);
static DEVICE_ATTR(cm_slope1_result, S_IRUGO,
		   ist30xx_cm_slope1_result_show, NULL);
static DEVICE_ATTR(cm_key_slope_result, S_IRUGO,
		   ist30xx_cm_key_slope_result_show, NULL);
static DEVICE_ATTR(cs0_result, S_IRUGO, ist30xx_cs0_result_show, NULL);
static DEVICE_ATTR(cs1_result, S_IRUGO, ist30xx_cs1_result_show, NULL);
static DEVICE_ATTR(line_cm, S_IRUGO, ist30xx_line_cm_show, NULL);
static DEVICE_ATTR(line_cm_slope0, S_IRUGO, ist30xx_line_cm_slope0_show, NULL);
static DEVICE_ATTR(line_cm_slope1, S_IRUGO, ist30xx_line_cm_slope1_show, NULL);
static DEVICE_ATTR(line_cs0, S_IRUGO, ist30xx_line_cs0_show, NULL);
static DEVICE_ATTR(line_cs1, S_IRUGO, ist30xx_line_cs1_show, NULL);
static DEVICE_ATTR(cm_key_slope_value, S_IRUGO,
		   ist30xx_cm_key_slope_value_show, NULL);

static struct attribute *cmcs_attributes[] = {
	&dev_attr_info.attr,
	&dev_attr_cmcs_binary.attr,
	&dev_attr_cmcs_custom.attr,
	&dev_attr_cmcs_sdcard.attr,
	&dev_attr_cm.attr,
	&dev_attr_cm_spec.attr,
	&dev_attr_cm_slope0.attr,
	&dev_attr_cm_slope1.attr,
	&dev_attr_cs0.attr,
	&dev_attr_cs1.attr,
	&dev_attr_cm_result.attr,
	&dev_attr_cm_slope0_result.attr,
	&dev_attr_cm_slope1_result.attr,
	&dev_attr_cm_key_slope_result.attr,
	&dev_attr_cs0_result.attr,
	&dev_attr_cs1_result.attr,
	&dev_attr_line_cm.attr,
	&dev_attr_line_cm_slope0.attr,
	&dev_attr_line_cm_slope1.attr,
	&dev_attr_line_cs0.attr,
	&dev_attr_line_cs1.attr,
	&dev_attr_cm_key_slope_value.attr,
	NULL,
};

static struct attribute_group cmcs_attr_group = {
	.attrs	= cmcs_attributes,
};

extern struct class *ist30xx_class;
struct device *ist30xx_cmcs_dev;


int ist30xx_init_cmcs_sysfs(void)
{
	/* /sys/class/touch/cmcs */
	ist30xx_cmcs_dev = device_create(ist30xx_class, NULL, 0, NULL, "cmcs");

	/* /sys/class/touch/cmcs/... */
	if (unlikely(sysfs_create_group(&ist30xx_cmcs_dev->kobj,
					&cmcs_attr_group)))
		tsp_err("Failed to create sysfs group(%s)!\n", "cmcs");

#if IST30XX_INTERNAL_CMCS_BIN
	ts_cmcs_bin = (u8 *)ist30xxb_cmcs;
	ts_cmcs_bin_size = sizeof(ist30xxb_cmcs);

	ist30xx_get_cmcs_info(ts_cmcs_bin, ts_cmcs_bin_size);
#endif

	return 0;
}
