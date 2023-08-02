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

#if IST30XXC_INTERNAL_CMCS_BIN
#include "ist30xx_cmcs_bin.h"
#endif

#define TSP_CH_UNUSED               (0)
#define TSP_CH_SCREEN               (1)
#define TSP_CH_GTX                  (2)
#define TSP_CH_KEY                  (3)
#define TSP_CH_UNKNOWN              (-1)

#define CMCS_TAGS_PARSE_OK          (0)

int ist30xxc_cmcs_ready = CMCS_READY;
u8 *tsp_cmcs_bin = NULL;
u32 tsp_cmcs_bin_size = 0;
CMCS_BIN_INFO ist30xxc_cmcs_bin;
CMCS_BIN_INFO *tsp_cmcs = (CMCS_BIN_INFO *)&ist30xxc_cmcs_bin;
CMCS_BUF ist30xxc_cmcs_buf;
CMCS_BUF *tsp_cmcs_buf = (CMCS_BUF *)&ist30xxc_cmcs_buf;

int ist30xxc_parse_cmcs_bin(const u8 *buf, const u32 size)
{
	int ret = -EPERM;
    int i;
    int node_spec_cnt;

	memcpy(tsp_cmcs->magic1, buf, sizeof(tsp_cmcs->magic1));
	memcpy(tsp_cmcs->magic2, &buf[size - sizeof(tsp_cmcs->magic2)],
	       sizeof(tsp_cmcs->magic2));

	if (!strncmp(tsp_cmcs->magic1, IST30XXC_CMCS_MAGIC, sizeof(tsp_cmcs->magic1))
	    && !strncmp(tsp_cmcs->magic2, IST30XXC_CMCS_MAGIC,
			sizeof(tsp_cmcs->magic2))) {
		int idx;

		idx = sizeof(tsp_cmcs->magic1);

        memcpy(&tsp_cmcs->items.cnt, &buf[idx], sizeof(tsp_cmcs->items.cnt));
        idx += sizeof(tsp_cmcs->items.cnt);
        tsp_cmcs->items.item = kmalloc(
                sizeof(struct CMCS_ITEM_INFO) * tsp_cmcs->items.cnt, GFP_KERNEL);
        for (i = 0; i < tsp_cmcs->items.cnt; i++) {
            memcpy(&tsp_cmcs->items.item[i], &buf[idx], 
                    sizeof(struct CMCS_ITEM_INFO));
            idx += sizeof(struct CMCS_ITEM_INFO);
        }

        memcpy(&tsp_cmcs->cmds.cnt, &buf[idx], sizeof(tsp_cmcs->cmds.cnt));
        idx += sizeof(tsp_cmcs->cmds.cnt);
        tsp_cmcs->cmds.cmd = kmalloc(
                sizeof(struct CMCS_CMD_INFO) * tsp_cmcs->cmds.cnt, GFP_KERNEL);
        for (i = 0; i < tsp_cmcs->cmds.cnt; i++) {
            memcpy(&tsp_cmcs->cmds.cmd[i], &buf[idx], 
                    sizeof(struct CMCS_CMD_INFO));
            idx += sizeof(struct CMCS_CMD_INFO);
        }

        memcpy(&tsp_cmcs->spec_slope, &buf[idx], sizeof(tsp_cmcs->spec_slope));
        idx += sizeof(tsp_cmcs->spec_slope);

        memcpy(&tsp_cmcs->spec_cr, &buf[idx], sizeof(tsp_cmcs->spec_cr));
        idx += sizeof(tsp_cmcs->spec_cr);

        memcpy(&tsp_cmcs->param, &buf[idx], sizeof(tsp_cmcs->param));
        idx += sizeof(tsp_cmcs->param);

        tsp_cmcs->spec_item = kmalloc(
                sizeof(struct CMCS_SPEC_TOTAL) * tsp_cmcs->items.cnt, 
                GFP_KERNEL);
        for (i = 0; i < tsp_cmcs->items.cnt; i++) {
            if (!strncmp(tsp_cmcs->items.item[i].spec_type, "N", 1)) {
                memcpy(&node_spec_cnt, &buf[idx], sizeof(node_spec_cnt));
                tsp_cmcs->spec_item[i].spec_node.node_cnt = node_spec_cnt;
                idx += sizeof(node_spec_cnt);
                tsp_cmcs->spec_item[i].spec_node.buf_min = (u16 *)&buf[idx];
                idx += node_spec_cnt * sizeof(s16);
                tsp_cmcs->spec_item[i].spec_node.buf_max = (u16 *)&buf[idx];
                idx += node_spec_cnt * sizeof(s16);
            } else if (!strncmp(tsp_cmcs->items.item[i].spec_type, "T", 1)) {
                memcpy(&tsp_cmcs->spec_item[i].spec_total, &buf[idx], 
                    sizeof(struct CMCS_SPEC_TOTAL));
                idx += sizeof(struct CMCS_SPEC_TOTAL);
            }
        }

        tsp_cmcs->buf_cmcs = (u8 *)&buf[idx];
		idx += tsp_cmcs->param.cmcs_size;

        tsp_cmcs->buf_sensor = (u32 *)&buf[idx];

        ret = 0;
	}

    ts_verb("Magic1: %s, Magic2: %s\n", tsp_cmcs->magic1, tsp_cmcs->magic2);
    ts_verb(" item(%d)\n", tsp_cmcs->items.cnt);
    for (i = 0; i < tsp_cmcs->items.cnt; i++) {
        ts_verb(" (%d): %s, 0x%08x, %d, %s, %s\n", 
                i, tsp_cmcs->items.item[i].name, tsp_cmcs->items.item[i].addr, 
                tsp_cmcs->items.item[i].size, tsp_cmcs->items.item[i].data_type, 
                tsp_cmcs->items.item[i].spec_type);
    }
    ts_verb(" cmd(%d)\n", tsp_cmcs->cmds.cnt);
    for (i = 0; i < tsp_cmcs->cmds.cnt; i++) {
        ts_verb(" (%d): 0x%08x, 0x%08x\n", 
                i, tsp_cmcs->cmds.cmd[i].addr, tsp_cmcs->cmds.cmd[i].value);
    }
    ts_verb(" param\n");
    ts_verb("  fw: 0x%08x, %d\n", tsp_cmcs->param.cmcs_size_addr, 
            tsp_cmcs->param.cmcs_size);
    ts_verb("  cm sensor1: 0x%08x, %d\n", tsp_cmcs->param.cm_sensor1_addr, 
            tsp_cmcs->param.cm_sensor1_size);
    ts_verb("  cm sensor2: 0x%08x, %d\n", tsp_cmcs->param.cm_sensor2_addr, 
            tsp_cmcs->param.cm_sensor2_size);
    ts_verb("  cm sensor3: 0x%08x, %d\n", tsp_cmcs->param.cm_sensor3_addr, 
            tsp_cmcs->param.cm_sensor3_size);
    ts_verb("  cs sensor1: 0x%08x, %d\n", tsp_cmcs->param.cs_sensor1_addr, 
            tsp_cmcs->param.cs_sensor1_size);
    ts_verb("  cs sensor2: 0x%08x, %d\n", tsp_cmcs->param.cs_sensor2_addr, 
            tsp_cmcs->param.cs_sensor2_size);
    ts_verb("  cs sensor3: 0x%08x, %d\n", tsp_cmcs->param.cs_sensor3_addr, 
            tsp_cmcs->param.cs_sensor3_size);
    ts_verb("  chksum: 0x%08x\n", tsp_cmcs->param.cmcs_chksum);
    ts_verb(" slope(%s)\n", tsp_cmcs->spec_slope.name);
    ts_verb("  x(%d,%d),y(%d,%d),gtx_x(%d,%d),gtx_y(%d,%d),key(%d,%d)\n",
		 tsp_cmcs->spec_slope.x_min, tsp_cmcs->spec_slope.x_max,
		 tsp_cmcs->spec_slope.y_min, tsp_cmcs->spec_slope.y_max,
         tsp_cmcs->spec_slope.gtx_x_min, tsp_cmcs->spec_slope.gtx_x_max,
         tsp_cmcs->spec_slope.gtx_y_min, tsp_cmcs->spec_slope.gtx_y_max,
		 tsp_cmcs->spec_slope.key_min, tsp_cmcs->spec_slope.key_max);
    ts_verb(" cr: screen(%4d, %4d), gtx(%4d, %4d), key(%4d, %4d)\n",
		 tsp_cmcs->spec_cr.screen_min, tsp_cmcs->spec_cr.screen_max,
         tsp_cmcs->spec_cr.gtx_min, tsp_cmcs->spec_cr.gtx_max,
		 tsp_cmcs->spec_cr.key_min, tsp_cmcs->spec_cr.key_max);
    for (i = 0; i < tsp_cmcs->items.cnt; i++) {
        if (!strncmp(tsp_cmcs->items.item[i].spec_type, "N", 1)) {
            ts_verb(" %s\n", tsp_cmcs->items.item[i].name);
            ts_verb(" min: %x, %x, %x, %x\n", 
                    tsp_cmcs->spec_item[i].spec_node.buf_min[0], 
                    tsp_cmcs->spec_item[i].spec_node.buf_min[1], 
                    tsp_cmcs->spec_item[i].spec_node.buf_min[2], 
                    tsp_cmcs->spec_item[i].spec_node.buf_min[3]);
	        ts_verb(" max: %x, %x, %x, %x\n", 
                    tsp_cmcs->spec_item[i].spec_node.buf_max[0], 
                    tsp_cmcs->spec_item[i].spec_node.buf_max[1], 
                    tsp_cmcs->spec_item[i].spec_node.buf_max[2], 
                    tsp_cmcs->spec_item[i].spec_node.buf_max[3]);
        } else if (!strncmp(tsp_cmcs->items.item[i].spec_type, "T", 1)) {
            ts_verb(" %s: screen(%4d, %4d), gtx(%4d, %4d), key(%4d, %4d)\n",
		    tsp_cmcs->items.item[i].name, 
            tsp_cmcs->spec_item[i].spec_total.screen_min, 
            tsp_cmcs->spec_item[i].spec_total.screen_max, 
            tsp_cmcs->spec_item[i].spec_total.gtx_min, 
            tsp_cmcs->spec_item[i].spec_total.gtx_max, 
            tsp_cmcs->spec_item[i].spec_total.key_min,
            tsp_cmcs->spec_item[i].spec_total.key_max);
        }
    }
    ts_verb(" cmcs: %x, %x, %x, %x\n", tsp_cmcs->buf_cmcs[0],
		 tsp_cmcs->buf_cmcs[1], tsp_cmcs->buf_cmcs[2], tsp_cmcs->buf_cmcs[3]);
	ts_verb(" sensor: %x, %x, %x, %x\n",
		 tsp_cmcs->buf_sensor[0], tsp_cmcs->buf_sensor[1],
		 tsp_cmcs->buf_sensor[2], tsp_cmcs->buf_sensor[3]);	

	return ret;
}

int ist30xxc_get_cmcs_info(const u8 *buf, const u32 size)
{
	int ret;

	ist30xxc_cmcs_ready = CMCS_NOT_READY;

	ret = ist30xxc_parse_cmcs_bin(buf, size);
	if (unlikely(ret != CMCS_TAGS_PARSE_OK))
		ts_warn("Cannot find tags of CMCS, make a bin by 'cmcs2bin.exe'\n");

	return ret;
}

int ist30xxc_set_cmcs_sensor(struct i2c_client *client, CMCS_PARAM param,
			    u32 *buf32)
{
	int ret;
	int len;
	u32 waddr;

	waddr = IST30XXC_DA_ADDR(param.cm_sensor1_addr);
	len = (param.cm_sensor1_size / IST30XXC_DATA_LEN) - 2;
    buf32 += 2;
    ts_verb("%08x %08x %08x %08x\n", buf32[0], buf32[1], buf32[2], buf32[3]);
    ts_verb("%08x(%d)\n", waddr, len);

    if (len > 0) {
        ret = ist30xxc_burst_write(client, waddr, buf32, len);
	    if (ret)
		    return ret;

        ts_info("cm sensor reg1 loaded!\n");
    }

    buf32 += len;
	waddr = IST30XXC_DA_ADDR(param.cm_sensor2_addr);
    len = param.cm_sensor2_size / IST30XXC_DATA_LEN;
    ts_verb("%08x %08x %08x %08x\n", buf32[0], buf32[1], buf32[2], buf32[3]);
    ts_verb("%08x(%d)\n", waddr, len);

    if (len > 0) {
    	ret = ist30xxc_burst_write(client, waddr, buf32, len);
	    if (ret)
		    return ret;

        ts_info("cm sensor reg2 loaded!\n");
    }

    buf32 += len;
	waddr = IST30XXC_DA_ADDR(param.cm_sensor3_addr);
	len = param.cm_sensor3_size / IST30XXC_DATA_LEN;
    ts_verb("%08x %08x %08x %08x\n", buf32[0], buf32[1], buf32[2], buf32[3]);
    ts_verb("%08x(%d)\n", waddr, len);

    if (len > 0) {
        ret = ist30xxc_burst_write(client, waddr, buf32, len);
	    if (ret)
		    return ret;

        ts_info("cm sensor reg3 loaded!\n");
    }

    buf32 += len;
    waddr = IST30XXC_DA_ADDR(param.cs_sensor1_addr);
	len = param.cs_sensor1_size / IST30XXC_DATA_LEN;
    ts_verb("%08x %08x %08x %08x\n", buf32[0], buf32[1], buf32[2], buf32[3]);
    ts_verb("%08x(%d)\n", waddr, len);

    if (len > 0) {
        ret = ist30xxc_burst_write(client, waddr, buf32, len);
	    if (ret)
		    return ret;

        ts_info("cs sensor reg1 loaded!\n");
    }

    buf32 += len;
	waddr = IST30XXC_DA_ADDR(param.cs_sensor2_addr);
    len = param.cs_sensor2_size / IST30XXC_DATA_LEN;
    ts_verb("%08x %08x %08x %08x\n", buf32[0], buf32[1], buf32[2], buf32[3]);
    ts_verb("%08x(%d)\n", waddr, len);

    if (len > 0) {
    	ret = ist30xxc_burst_write(client, waddr, buf32, len);
	    if (ret)
		    return ret;

        ts_info("cs sensor reg2 loaded!\n");
    }

    buf32 += len;
	waddr = IST30XXC_DA_ADDR(param.cs_sensor3_addr);
	len = param.cs_sensor3_size / IST30XXC_DATA_LEN;
    ts_verb("%08x %08x %08x %08x\n", buf32[0], buf32[1], buf32[2], buf32[3]);
    ts_verb("%08x(%d)\n", waddr, len);

    if (len > 0) {
        ret = ist30xxc_burst_write(client, waddr, buf32, len);
	    if (ret)
		    return ret;

        ts_info("cs sensor reg3 loaded!\n");
    }

	return 0;
}

int ist30xxc_set_cmcs_cmd(struct i2c_client *client, CMCS_CMD cmds)
{
	int ret;
    int i;
	u32 val;
    u32 waddr;

    for (i = 0; i < cmds.cnt; i++) {
        waddr = IST30XXC_DA_ADDR(cmds.cmd[i].addr);
    	val = cmds.cmd[i].value;
        ret = ist30xxc_write_cmd(client, waddr, val);    
    	if (ret)
	    	return ret;
	    ts_verb("cmd%d(0x%08x): 0x%08x\n", i, waddr, val);
    }

	return 0;
}

int ist30xxc_parse_cmcs_buf(struct ist30xxc_data *data, s16 *buf)
{
	int i, j;
    TSP_INFO *tsp = &data->tsp_info;

	ts_info(" %d * %d\n", tsp->ch_num.tx, tsp->ch_num.rx);
	for (i = 0; i < tsp->ch_num.tx; i++) {
		ts_info(" ");
		for (j = 0; j < tsp->ch_num.rx; j++)
			printk("%5d ", buf[i * tsp->ch_num.rx + j]);
		printk("\n");
	}

	return 0;
}

int ist30xxc_apply_cmcs_slope(struct ist30xxc_data *data, CMCS_BUF *cmcs_buf)
{
	int i, j, k;
	int idx1, idx2;
    int type;
    int success = false;
    TSP_INFO *tsp = &data->tsp_info;
	int width = tsp->screen.rx;
	int height = tsp->screen.tx;
	s16 *presult;
	s16 *pspec_min = (s16 *)tsp_cmcs_buf->spec_min;
    s16 *pspec_max = (s16 *)tsp_cmcs_buf->spec_max;
	s16 *pslope0 = (s16 *)tsp_cmcs_buf->slope0;
	s16 *pslope1 = (s16 *)tsp_cmcs_buf->slope1;    
    
    memset(tsp_cmcs_buf->slope0, 0, sizeof(tsp_cmcs_buf->slope0));
	memset(tsp_cmcs_buf->slope1, 0, sizeof(tsp_cmcs_buf->slope1));
    
    if (!strncmp(tsp_cmcs->spec_slope.name, IST30XXC_CMCS_CS, 2))
        presult = (s16 *)&cmcs_buf->cs;
    else
        presult = (s16 *)&cmcs_buf->cm;

    for (i = 0; i < tsp_cmcs->items.cnt; i++) {
        if (!strncmp(tsp_cmcs->spec_slope.name, 
                    tsp_cmcs->items.item[i].name, 2)) {
            idx1 = idx2 = 0;
            for (j = 0; j < tsp->ch_num.tx; j++) {
				for (k = 0; k < tsp->ch_num.rx; k++) {
                    type = check_node_type(data, j, k);
                    idx1 = (j * tsp->ch_num.rx) + k;
                    if (type == TSP_CH_UNKNOWN) {
                        continue;
                    } else if (type == TSP_CH_UNUSED) {
                        tsp_cmcs_buf->spec_min[idx1] = 0;
                        tsp_cmcs_buf->spec_max[idx1] = 0;
                    } else {
                        tsp_cmcs_buf->spec_min[idx1] = 
                            tsp_cmcs->spec_item[i].spec_node.buf_min[idx2];
                        tsp_cmcs_buf->spec_max[idx1] = 
                            tsp_cmcs->spec_item[i].spec_node.buf_max[idx2];
                        idx2++;
                    }
                }
            }            
            success = true;
        }
    }

    if (success == false)
        return -1;

#if CMCS_PARSING_DEBUG
    idx1 = 0;
	ts_info("# Node cm min specific\n");
	for (i = 0; i < tsp->ch_num.tx; i++) {
		ts_info(" ");
		for (j = 0; j < tsp->ch_num.rx; j++)
			printk("%5d ", cmcs_buf->spec_min[idx1++]);
		printk("\n");
	}

    idx1 = 0;
    ts_info("# Node cm max specific\n");
    for (i = 0; i < tsp->ch_num.tx; i++) {
		ts_info(" ");
		for (j = 0; j < tsp->ch_num.rx; j++)
			printk("%5d ", cmcs_buf->spec_max[idx1++]);
		printk("\n");
	}
#endif

	ts_verb("# Apply slope0_x\n");
	for (i = 0; i < height; i++) {
		for (j = 0; j < width - 1; j++) {
			idx1 = (i * tsp->ch_num.rx) + j;
			idx2 = idx1 + 1;

			pslope0[idx1] = (presult[idx2] - presult[idx1]);
			pslope0[idx1] += 
                (((pspec_min[idx1] + pspec_max[idx1]) / 2) - 
                ((pspec_min[idx2] + pspec_max[idx2]) / 2));
		}
	}

	ts_verb("# Apply slope1_y\n");
	for (i = 0; i < height - 1; i++) {
		for (j = 0; j < width; j++) {
			idx1 = (i * tsp->ch_num.rx) + j;
			idx2 = idx1 + tsp->ch_num.rx;

			pslope1[idx1] = (presult[idx2] - presult[idx1]);
			pslope1[idx1] += 
                (((pspec_min[idx1] + pspec_max[idx1]) / 2) - 
                ((pspec_min[idx2] + pspec_max[idx2]) / 2));
		}
	}

	ts_verb("# Apply slope0_gtx_x\n");
    for (i = 0; i < tsp->gtx.num; i++) {
        if (tsp->gtx.ch_num[i] > height) {
            for (j = 0; j < width - 1; j++) {
                idx1 = (tsp->gtx.ch_num[i] * tsp->ch_num.rx) + j;
                idx2 = idx1 + 1;

                pslope0[idx1] = (presult[idx2] - presult[idx1]);
    			pslope0[idx1] += 
                    (((pspec_min[idx1] + pspec_max[idx1]) / 2) - 
                    ((pspec_min[idx2] + pspec_max[idx2]) / 2));
            }
        }
    }

    ts_verb("# Apply slope1_gtx_y\n");
    for (i = 0; i < (tsp->gtx.num - 1); i++) {
        if (tsp->gtx.ch_num[i] > height) {
            for (j = 0; j < width; j++) {
                idx1 = (tsp->gtx.ch_num[i] * tsp->ch_num.rx) + j;
                idx2 = idx1 + tsp->ch_num.rx;

                pslope1[idx1] = (presult[idx2] - presult[idx1]);
      			pslope1[idx1] += 
                    (((pspec_min[idx1] + pspec_max[idx1]) / 2) - 
                    ((pspec_min[idx2] + pspec_max[idx2]) / 2));
            }
        }
    }

#if CMCS_PARSING_DEBUG
	ts_info("# slope0_x\n");
	for (i = 0; i < height; i++) {
		ts_info(" ");
		for (j = 0; j < width; j++) {
			idx1 = (i * tsp->ch_num.rx) + j;
			printk("%5d ", pslope0[idx1]);
		}
		printk("\n");
	}

	ts_info("# slope1_y\n");
	for (i = 0; i < height; i++) {
		ts_info(" ");
		for (j = 0; j < width; j++) {
			idx1 = (i * tsp->ch_num.rx) + j;
			printk("%5d ", pslope1[idx1]);
		}
		printk("\n");
	}

    ts_info("# slope0_gtx_x\n");
    for (i = 0; i < tsp->gtx.num; i++) {
		ts_info(" ");
		for (j = 0; j < width; j++) {
			idx1 = (tsp->gtx.ch_num[i] * tsp->ch_num.rx) + j;
			printk("%5d ", pslope0[idx1]);
		}
		printk("\n");
	}

    ts_info("# slope1_gtx_y\n");
	for (i = 0; i < tsp->gtx.num; i++) {
		ts_info(" ");
		for (j = 0; j < width; j++) {
			idx1 = (tsp->gtx.ch_num[i] * tsp->ch_num.rx) + j;
			printk("%5d ", pslope1[idx1]);
		}
		printk("\n");
	}

#endif

	return 0;
}

int ist30xxc_get_cmcs_buf(struct ist30xxc_data *data, const char *mode, 
        CMCS_ITEM items, s16 *buf)
{
	int ret = -EINVAL;
    int i;
    bool success = false;
    u32 waddr;
	u16 len; 
    
    for (i = 0; i < items.cnt; i++) {
        if (!strncmp(items.item[i].name, mode, 2)) {
            waddr = IST30XXC_DA_ADDR(items.item[i].addr);
            len = items.item[i].size / IST30XXC_DATA_LEN;
            ret = ist30xxc_burst_read(data->client, 
                    waddr, (u32 *)buf, len, true);
            if (unlikely(ret))
		        return ret;
            ts_verb("%s, 0x%08x, %d\n", __func__, waddr, len);
            success = true;
        }
    }

    if (success == false)
        return ret;

#if CMCS_PARSING_DEBUG
	ret = ist30xxc_parse_cmcs_buf(data, buf);
#endif

	return ret;
}

int ist30xxc_cmcs_wait(struct ist30xxc_data *data)
{
	int cnt = IST30XXC_CMCS_TIMEOUT / 100;

	data->status.cmcs = 0;
	while (cnt-- > 0) {
		msleep(100);

		if (data->status.cmcs) {
			ts_info("CMCS test complete\n");
            return 0;
		}
	}
	ts_warn("cmcs time out\n");

	return -EPERM;
}

#define cmcs_next_step(ret)   { if (unlikely(ret)) goto end; msleep(20); }
int ist30xxc_cmcs_test(struct ist30xxc_data *data, const u8 *buf, int size)
{
    int ret;
	int len;
    u32 waddr;
    u32 val;
    u32 cs_chksum = 0;
	u32 chksum = 0;
	u32 *buf32;

	ts_info("*** CM/CS test ***\n");
	
	ist30xxc_disable_irq(data);
	ist30xxc_reset(data, false);

    ret = ist30xxc_write_cmd(data->client,
            IST30XXC_HIB_CMD, (eHCOM_RUN_RAMCODE << 16) | 0);
	cmcs_next_step(ret);

	/* Set sensor register */
	buf32 = tsp_cmcs->buf_sensor;
	ret = ist30xxc_set_cmcs_sensor(data->client, tsp_cmcs->param, buf32);
	cmcs_next_step(ret);

    /* Set command */
	ret = ist30xxc_set_cmcs_cmd(data->client, tsp_cmcs->cmds);
	cmcs_next_step(ret);

	/* Load cmcs test code */
	buf32 = (u32 *)tsp_cmcs->buf_cmcs;
	len = tsp_cmcs->param.cmcs_size / IST30XXC_DATA_LEN - 1;
	ts_verb("%08x %08x %08x %08x\n", buf32[0], buf32[1], buf32[2], buf32[3]);
	waddr = IST30XXC_DA_ADDR(data->tags.ram_base);
    ts_verb("%08x(%d)\n", waddr, len);

    ret = ist30xxc_burst_write(data->client, waddr, buf32, len);
	cmcs_next_step(ret);

    waddr = IST30XXC_DA_ADDR(tsp_cmcs->param.cmcs_size_addr);
    val = tsp_cmcs->param.cmcs_size - 4;
    ret = ist30xxc_write_cmd(data->client, waddr, val);    
    cmcs_next_step(ret);
    ts_verb("size(0x%08x): 0x%08x\n", waddr, val);

    ts_info("cmcs code loaded!\n");

    /* Cal checksum & run cmcs */
    ret = ist30xxc_write_cmd(data->client, 
            IST30XXC_HIB_CMD, (eHCOM_RUN_RAMCODE << 16) | 1);
	cmcs_next_step(ret);

    /* Check checksum */
	ret = ist30xxc_read_reg(data->client, IST30XXC_CMCS_CHECKSUM, &chksum);
	cmcs_next_step(ret);
    if (chksum != tsp_cmcs->param.cmcs_chksum)
        goto end;

    /* Check cs sensor checksum */
	ret = ist30xxc_read_reg(data->client, IST30XXC_CMCS_CS_CHECKSUM, &cs_chksum);
	cmcs_next_step(ret);
    if (cs_chksum != tsp_cmcs->param.cs_sensor_chksum)
        goto end;

    ts_info("CM/CS code ready!!\n");

    ist30xxc_enable_irq(data);    
    /* Wait CMCS test result */
	if (ist30xxc_cmcs_wait(data) == 0) {
		ts_info("CM/CS test OK\n");
    } else {
		ts_info("CM/CS test Fail!\n");
        goto end;
    }
	ist30xxc_disable_irq(data);

	/* Read CM data */
	memset(tsp_cmcs_buf->cm, 0, sizeof(tsp_cmcs_buf->cm));

	ret = ist30xxc_get_cmcs_buf(data, 
        IST30XXC_CMCS_CM, tsp_cmcs->items, tsp_cmcs_buf->cm);
	cmcs_next_step(ret);

    /* Read CS data */
	memset(tsp_cmcs_buf->cs, 0, sizeof(tsp_cmcs_buf->cs));

	ret = ist30xxc_get_cmcs_buf(data, 
        IST30XXC_CMCS_CS, tsp_cmcs->items, tsp_cmcs_buf->cs);
	cmcs_next_step(ret);

	ret = ist30xxc_apply_cmcs_slope(data, tsp_cmcs_buf);
    cmcs_next_step(ret);

	ist30xxc_cmcs_ready = CMCS_READY;

end:
	if (unlikely(ret)) {
		ts_warn("CM/CS test Fail!, ret=%d\n", ret);
	} else if (unlikely(chksum != tsp_cmcs->param.cmcs_chksum)) {
		ts_warn("Error CheckSum: %x(%x)\n", 
                chksum, tsp_cmcs->param.cmcs_chksum);
		ret = -ENOEXEC;
	} else if (unlikely(cs_chksum != tsp_cmcs->param.cs_sensor_chksum)) {
        ts_warn("Error CS Sensor CheckSum: %x(%x)\n", 
                cs_chksum, tsp_cmcs->param.cs_sensor_chksum);
		ret = -ENOEXEC;
    }

    ist30xxc_reset(data, false);
	ist30xxc_start(data);
	ist30xxc_enable_irq(data);

    return ret;
}


int check_node_type(struct ist30xxc_data *data, int tx, int rx)
{
    int i;
	TSP_INFO *tsp = &data->tsp_info;
    TKEY_INFO *tkey = &data->tkey_info;

	if ((tx >= tsp->ch_num.tx) || (tx < 0) || 
            (rx >= tsp->ch_num.rx) || (rx < 0)) {
		ts_warn("TSP channel is not correct!! (%d * %d)\n", tx, rx);
		return TSP_CH_UNKNOWN;
	}

	if ((tx >= tsp->screen.tx) || (rx >= tsp->screen.rx)) {
		for (i = 0; i < tsp->gtx.num; i++) {
			if ((tx == tsp->gtx.ch_num[i]) && (rx < tsp->screen.rx))
				return TSP_CH_GTX;
		}

		for (i = 0; i < tkey->key_num; i++) {
			if ((tx == tkey->ch_num[i].tx) &&
			    (rx == tkey->ch_num[i].rx))
                return TSP_CH_KEY;
		}
	} else {
		return TSP_CH_SCREEN;
	}
    
    return TSP_CH_UNUSED;
}

int print_ts_cmcs(struct ist30xxc_data *data, s16 *buf16, char *buf)
{
	int i, j;
	int idx;
    int type;
	int count = 0;
	char msg[128];

	TSP_INFO *tsp = &data->tsp_info;

	int tx_num = tsp->ch_num.tx;
	int rx_num = tsp->ch_num.rx;

	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
            type = check_node_type(data, i, j);
            idx = (i * rx_num) + j;
            if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED))
                count += sprintf(msg, "%5d ", 0);
            else
			    count += sprintf(msg, "%5d ", buf16[idx]);
			strcat(buf, msg);
		}

		count += sprintf(msg, "\n");
		strcat(buf, msg);
	}

	return count;
}

int print_line_ts_cmcs(struct ist30xxc_data *data, int mode, s16 *buf16, char *buf)
{
	int i, j;
	int idx;
	int type;
	int count = 0;
	int key_index[5] = { 0, };
	int key_cnt = 0;
	char msg[128];

    TSP_INFO *tsp = &data->tsp_info;

	int tx_num = tsp->ch_num.tx;
	int rx_num = tsp->ch_num.rx;

	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			type = check_node_type(data, i, j);
			if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED))
				continue;   // Ignore

			if ((mode == CMCS_FLAG_CM_SLOPE0) &&
                    (j == (tsp->screen.rx - 1)))
				continue;
			else if ((mode == CMCS_FLAG_CM_SLOPE1) && 
                    (i == (tsp->screen.tx - 1)))
				continue;

            if ((mode == CMCS_FLAG_CM_SLOPE0) && (type == TSP_CH_GTX) && 
                    (j == (tsp->screen.rx - 1)))
                continue;
            else if ((mode == CMCS_FLAG_CM_SLOPE1) && 
                    (type == TSP_CH_GTX) && 
                    (i == tsp->gtx.ch_num[tsp->gtx.num - 1]))
				continue;

			idx = (i * rx_num) + j;            

			if (type == TSP_CH_KEY) {
				key_index[key_cnt++] = idx;
				continue;
			}

			count += sprintf(msg, "%5d ", buf16[idx]);
			strcat(buf, msg);
		}
	}

	ts_info("key cnt: %d\n", key_cnt);
	if ((mode != CMCS_FLAG_CM_SLOPE0) && (mode != CMCS_FLAG_CM_SLOPE1)) {
		ts_info("key cnt: %d\n", key_cnt);
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
ssize_t ist30xxc_cmcs_info_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
    int i;
	int count;
	char msg[128];
    struct ist30xxc_data *data = dev_get_drvdata(dev);
    TSP_INFO *tsp = &data->tsp_info;
    TKEY_INFO *tkey = &data->tkey_info;

	if (ist30xxc_cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	/* Channel */
	count = sprintf(msg, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d ",
			 tsp->ch_num.tx, tsp->ch_num.rx, tsp->screen.tx, tsp->screen.rx, 
             tsp->gtx.num, tsp->gtx.ch_num[0], tsp->gtx.ch_num[1], 
             tsp->gtx.ch_num[2], tsp->gtx.ch_num[3], tkey->key_num, 
             tkey->ch_num[0].tx, tkey->ch_num[0].rx, tkey->ch_num[1].tx, 
             tkey->ch_num[1].rx, tkey->ch_num[2].tx, tkey->ch_num[2].rx, 
             tkey->ch_num[3].tx, tkey->ch_num[3].rx, tkey->ch_num[4].tx, 
             tkey->ch_num[4].rx);
	strcat(buf, msg);

	/* Slope */
	count += sprintf(msg, "%d %d %d %d %d %d %d %d %d %d ",
			 tsp_cmcs->spec_slope.x_min, tsp_cmcs->spec_slope.x_max,
			 tsp_cmcs->spec_slope.y_min, tsp_cmcs->spec_slope.y_max,
             tsp_cmcs->spec_slope.gtx_x_min, tsp_cmcs->spec_slope.gtx_x_max,
			 tsp_cmcs->spec_slope.gtx_y_min, tsp_cmcs->spec_slope.gtx_y_max,
			 tsp_cmcs->spec_slope.key_min, tsp_cmcs->spec_slope.key_max);
	strcat(buf, msg);

    /* CS */
    for (i = 0; i < tsp_cmcs->items.cnt; i++) {
        if (!strncmp(tsp_cmcs->items.item[i].name, IST30XXC_CMCS_CS, 2)) {
            if (!strncmp(tsp_cmcs->items.item[i].spec_type, "T", 1)) {
	            count += sprintf(msg, "%d %d %d %d %d %d ",
			    tsp_cmcs->spec_item[i].spec_total.screen_min, 
                tsp_cmcs->spec_item[i].spec_total.screen_max,
                tsp_cmcs->spec_item[i].spec_total.gtx_min, 
                tsp_cmcs->spec_item[i].spec_total.gtx_max,
			    tsp_cmcs->spec_item[i].spec_total.key_min, 
                tsp_cmcs->spec_item[i].spec_total.key_max);
	            strcat(buf, msg);                   
            }
        }
    }

	/* CR */
	count += sprintf(msg, "%d %d %d %d %d %d ",
			 tsp_cmcs->spec_cr.screen_min, tsp_cmcs->spec_cr.screen_max,
             tsp_cmcs->spec_cr.gtx_min, tsp_cmcs->spec_cr.gtx_max,
			 tsp_cmcs->spec_cr.key_min, tsp_cmcs->spec_cr.key_max);
	strcat(buf, msg);

	ts_verb("%s\n", buf);

	return count;
}

/* sysfs: /sys/class/touch/cmcs/cmcs_binary */
ssize_t ist30xxc_cmcs_binary_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int ret;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	if ((tsp_cmcs_bin == NULL) || (tsp_cmcs_bin_size == 0))
		return sprintf(buf, "Binary is not correct(%d)\n", tsp_cmcs_bin_size);

	ret = ist30xxc_get_cmcs_info(tsp_cmcs_bin, tsp_cmcs_bin_size);
    if (ret)
        goto binary_end;

	mutex_lock(&ist30xxc_mutex);
	ret = ist30xxc_cmcs_test(data, tsp_cmcs_bin, tsp_cmcs_bin_size);
	mutex_unlock(&ist30xxc_mutex);

binary_end:
	return sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n"));
}

/* sysfs: /sys/class/touch/cmcs/cmcs_custom */
ssize_t ist30xxc_cmcs_custom_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int ret;
	int bin_size = 0;
	u8 *bin = NULL;
	const struct firmware *req_bin = NULL;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	ret = request_firmware(&req_bin, IST30XXC_CMCS_NAME, &data->client->dev);
	if (ret)
		return sprintf(buf, "File not found, %s\n", IST30XXC_CMCS_NAME);

	bin = (u8 *)req_bin->data;
	bin_size = (u32)req_bin->size;

	ret = ist30xxc_get_cmcs_info(bin, bin_size);
    if (ret)
        goto custom_end;

	mutex_lock(&ist30xxc_mutex);
	ret = ist30xxc_cmcs_test(data, bin, bin_size);
	mutex_unlock(&ist30xxc_mutex);

custom_end:
	release_firmware(req_bin);

	return sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n"));
}

#define MAX_FILE_PATH   255
/* sysfs: /sys/class/touch/cmcs/cmcs_sdcard */
ssize_t ist30xxc_cmcs_sdcard_show(struct device *dev,
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
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	old_fs = get_fs();
	set_fs(get_ds());

	snprintf(fw_path, MAX_FILE_PATH, "/sdcard/%s",
		 IST30XXC_CMCS_NAME);
	fp = filp_open(fw_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		ts_info("file %s open error:%d\n", fw_path, (s32)fp);
		ret = -ENOENT;
		goto err_file_open;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;

	buff = kzalloc((size_t)fsize, GFP_KERNEL);
	if (!buff) {
		ts_info("fail to alloc buffer\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	nread = vfs_read(fp, (char __user *)buff, fsize, &fp->f_pos);
	if (nread != fsize) {
		ts_info("mismatch fw size\n");

		goto err_fw_size;
	}

	bin = (u8 *)buff;
	bin_size = (u32)fsize;

	filp_close(fp, current->files);
	set_fs(old_fs);
	ts_info("firmware is loaded!!\n");

	ret = ist30xxc_get_cmcs_info(bin, bin_size);
    if (ret)
        goto sdcard_end;

	mutex_lock(&ist30xxc_mutex);
	ret = ist30xxc_cmcs_test(data, bin, bin_size);
	mutex_unlock(&ist30xxc_mutex);

err_fw_size:
	if (buff)
		kfree(buff);
err_alloc:
	if (fp)
		filp_close(fp, NULL);
err_file_open:
	set_fs(old_fs);

sdcard_end:
    ts_info("size: %d\n", sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n")));

	return sprintf(buf, (ret == 0 ? "OK\n" : "Fail\n"));
}

/* sysfs: /sys/class/touch/cmcs/cm */
ssize_t ist30xxc_cm_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct ist30xxc_data *data = dev_get_drvdata(dev);
    TSP_INFO *tsp = &data->tsp_info;

	if (ist30xxc_cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	ts_verb("CM (%d * %d)\n",tsp->ch_num.tx, tsp->ch_num.rx);

	return print_ts_cmcs(data, tsp_cmcs_buf->cm, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_spec_min */
ssize_t ist30xxc_cm_spec_min_show(struct device *dev, 
            struct device_attribute *attr, char *buf)
{
	struct ist30xxc_data *data = dev_get_drvdata(dev);
    TSP_INFO *tsp = &data->tsp_info;

	if (ist30xxc_cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	ts_verb("CM Spec Min (%d * %d)\n", tsp->ch_num.tx, tsp->ch_num.rx);

	return print_ts_cmcs(data, tsp_cmcs_buf->spec_min, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_spec_max */
ssize_t ist30xxc_cm_spec_max_show(struct device *dev, 
            struct device_attribute *attr, char *buf)
{
	struct ist30xxc_data *data = dev_get_drvdata(dev);
    TSP_INFO *tsp = &data->tsp_info;

	if (ist30xxc_cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	ts_verb("CM Spec Max (%d * %d)\n", tsp->ch_num.tx, tsp->ch_num.rx);

	return print_ts_cmcs(data, tsp_cmcs_buf->spec_max, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_slope0 */
ssize_t ist30xxc_cm_slope0_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct ist30xxc_data *data = dev_get_drvdata(dev);
    TSP_INFO *tsp = &data->tsp_info;

	if (ist30xxc_cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	ts_verb("CM Slope0_X (%d * %d)\n", tsp->ch_num.tx, tsp->ch_num.rx);

	return print_ts_cmcs(data, tsp_cmcs_buf->slope0, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_slope1 */
ssize_t ist30xxc_cm_slope1_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct ist30xxc_data *data = dev_get_drvdata(dev);
    TSP_INFO *tsp = &data->tsp_info;

	if (ist30xxc_cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	ts_verb("CM Slope1_Y (%d * %d)\n", tsp->ch_num.tx, tsp->ch_num.rx);

	return print_ts_cmcs(data, tsp_cmcs_buf->slope1, buf);
}

/* sysfs: /sys/class/touch/cmcs/cs */
ssize_t ist30xxc_cs_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
    struct ist30xxc_data *data = dev_get_drvdata(dev);
    TSP_INFO *tsp = &data->tsp_info;

	if (ist30xxc_cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	ts_verb("CS (%d * %d)\n", tsp->ch_num.tx, tsp->ch_num.rx);

	return print_ts_cmcs(data, tsp_cmcs_buf->cs, buf);
}

int print_ts_cm_slope_result(struct ist30xxc_data *data, u8 flag, s16 *buf16, 
        char *buf)
{
	int i, j;
	int type, idx;
	int count = 0, err_cnt = 0;
	int min_spec, max_spec, gtx_min_spec, gtx_max_spec;

	char msg[128];

    TSP_INFO *tsp = &data->tsp_info;
	struct CMCS_SPEC_SLOPE *spec = 
        (struct CMCS_SPEC_SLOPE *)&tsp_cmcs->spec_slope;

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

	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			idx = (i * tsp->ch_num.rx) + j;

			type = check_node_type(data, i, j);
			if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED) || 
                (type == TSP_CH_KEY))
				continue;   // Ignore

            if (type == TSP_CH_SCREEN) {
    			if ((buf16[idx] >= min_spec) && (buf16[idx] <= max_spec))
	    			continue;   // OK
            } else if (type == TSP_CH_GTX) {
                if ((buf16[idx] >= gtx_min_spec) && 
                        (buf16[idx] <= gtx_max_spec))
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

int print_cm_tkey_slope_result(struct ist30xxc_data *data, s16 *buf16, char *buf, 
        bool print)
{
	int i, j;
	int type, idx;
	int count = 0;
	int min_spec, max_spec;
	int key_num = 0;
	s16 key_cm[5] = { 0, };
	s16 slope_result;

	char msg[128];

    TSP_INFO *tsp = &data->tsp_info;
	struct CMCS_SPEC_SLOPE *spec = 
        (struct CMCS_SPEC_SLOPE *)&tsp_cmcs->spec_slope;

	min_spec = spec->key_min;
	max_spec = spec->key_max;

	if (print) {
		count = sprintf(msg, "Spec: %d ~ %d\n", min_spec, max_spec);
		strcat(buf, msg);
	}

	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			idx = (i * tsp->ch_num.rx) + j;

			type = check_node_type(data, i, j);
			if (type == TSP_CH_KEY)
				key_cm[key_num++] = buf16[idx];
		}
	}

	/* Check key slope */
	slope_result = key_cm[0] - key_cm[1];

	if (print) {
		if ((slope_result >= min_spec) && (slope_result <= max_spec))
			count += sprintf(msg, "OK (%d)\n", slope_result);
		else
			count += sprintf(msg, "Fail (%d)\n", slope_result);
	} else {
		count += sprintf(msg, "%d\n",
				 (slope_result >= 0 ? slope_result : -slope_result));
	}

	strcat(buf, msg);

	return count;
}

int print_ts_cs_result(struct ist30xxc_data *data, s16 *buf16, char *buf)
{
	int i, j;
    bool success = false;
	int type, idx;
	int count = 0, err_cnt = 0;
	int min_spec, max_spec;

	char msg[128];

    TSP_INFO *tsp = &data->tsp_info;
    struct CMCS_SPEC_TOTAL *spec;

     for (i = 0; i < tsp_cmcs->items.cnt; i++) {
        if (!strncmp(tsp_cmcs->items.item[i].name, IST30XXC_CMCS_CS, 2)) {
            if (!strncmp(tsp_cmcs->items.item[i].spec_type, "T", 1)) {
                spec = 
                    (struct CMCS_SPEC_TOTAL *)&tsp_cmcs->spec_item[i].spec_total;
                success = true;
                break;
            }
        }
    }

    if (success == false)
        return 0;

	count = sprintf(msg, "Spec: screen(%d~%d), gtx(%d~%d), key(%d~%d)\n",
			spec->screen_min, spec->screen_max, spec->gtx_min, 
            spec->gtx_max, spec->key_min, spec->key_max);
	strcat(buf, msg);

	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			idx = (i * tsp->ch_num.rx) + j;

			type = check_node_type(data, i, j);
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

			if ((buf16[idx] >= min_spec) && (buf16[idx] <= max_spec))
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
ssize_t ist30xxc_cm_result_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int i, j;
	int type, idx, err_cnt = 0;
	int min_spec, max_spec;
	int count = 0;
	short cm;
	char msg[128];

    struct ist30xxc_data *data = dev_get_drvdata(dev);
    TSP_INFO *tsp = &data->tsp_info;

	if (ist30xxc_cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			idx = (i * tsp->ch_num.rx) + j;

			type = check_node_type(data, i, j);
			if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED))
				continue;

			min_spec = tsp_cmcs_buf->spec_min[idx];
            max_spec = tsp_cmcs_buf->spec_max[idx];
			
			cm = tsp_cmcs_buf->cm[idx];
			if ((cm >= min_spec) && (cm <= max_spec))
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
ssize_t ist30xxc_cm_slope0_result_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	if (ist30xxc_cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_ts_cm_slope_result(data, CMCS_FLAG_CM_SLOPE0,
				     tsp_cmcs_buf->slope0, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_slope1_result */
ssize_t ist30xxc_cm_slope1_result_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	if (ist30xxc_cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_ts_cm_slope_result(data, CMCS_FLAG_CM_SLOPE1,
				     tsp_cmcs_buf->slope1, buf);
}
/* sysfs: /sys/class/touch/cmcs/cm_key_slope_result */
ssize_t ist30xxc_cm_key_slope_result_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	if (ist30xxc_cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_cm_tkey_slope_result(data, tsp_cmcs_buf->cm, buf, true);
}

/* sysfs: /sys/class/touch/cmcs/cs_result */
ssize_t ist30xxc_cs_result_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	if (ist30xxc_cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_ts_cs_result(data, tsp_cmcs_buf->cs, buf);
}

/* sysfs: /sys/class/touch/cmcs/line_cm */
ssize_t ist30xxc_line_cm_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	if (ist30xxc_cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_line_ts_cmcs(data, CMCS_FLAG_CM, tsp_cmcs_buf->cm, buf);
}

/* sysfs: /sys/class/touch/cmcs/line_cm_slope0 */
ssize_t ist30xxc_line_cm_slope0_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	if (ist30xxc_cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_line_ts_cmcs(data, CMCS_FLAG_CM_SLOPE0, tsp_cmcs_buf->slope0, buf);
}

/* sysfs: /sys/class/touch/cmcs/line_cm_slope1 */
ssize_t ist30xxc_line_cm_slope1_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	if (ist30xxc_cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_line_ts_cmcs(data, CMCS_FLAG_CM_SLOPE1, tsp_cmcs_buf->slope1, buf);
}

/* sysfs: /sys/class/touch/cmcs/line_cs */
ssize_t ist30xxc_line_cs_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	if (ist30xxc_cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "CMCS test is not work!!\n");

	return print_line_ts_cmcs(data, CMCS_FLAG_CS, tsp_cmcs_buf->cs, buf);
}

/* sysfs: /sys/class/touch/cmcs/cm_key_slope_value */
ssize_t ist30xxc_cm_key_slope_value_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int ret;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	if ((tsp_cmcs_bin == NULL) || (tsp_cmcs_bin_size == 0))
		return sprintf(buf, "%s", "FFFF");

	ret = ist30xxc_get_cmcs_info(tsp_cmcs_bin, tsp_cmcs_bin_size);
    if (ret)
      	return sprintf(buf, "%s", "FFFF");

	mutex_lock(&ist30xxc_mutex);
	ret = ist30xxc_cmcs_test(data, tsp_cmcs_bin, tsp_cmcs_bin_size);
	mutex_unlock(&ist30xxc_mutex);

	if (ret)
		return sprintf(buf, "%s", "FFFF");

	if (ist30xxc_cmcs_ready == CMCS_NOT_READY)
		return sprintf(buf, "%s", "FFFF");

	return print_cm_tkey_slope_result(data, tsp_cmcs_buf->cm, buf, false);
}

/* sysfs : cmcs */
static DEVICE_ATTR(info, S_IRUGO, ist30xxc_cmcs_info_show, NULL);
static DEVICE_ATTR(cmcs_binary, S_IRUGO, ist30xxc_cmcs_binary_show, NULL);
static DEVICE_ATTR(cmcs_custom, S_IRUGO, ist30xxc_cmcs_custom_show, NULL);
static DEVICE_ATTR(cmcs_sdcard, S_IRUGO, ist30xxc_cmcs_sdcard_show, NULL);
static DEVICE_ATTR(cm, S_IRUGO, ist30xxc_cm_show, NULL);
static DEVICE_ATTR(cm_spec_min, S_IRUGO, ist30xxc_cm_spec_min_show, NULL);
static DEVICE_ATTR(cm_spec_max, S_IRUGO, ist30xxc_cm_spec_max_show, NULL);
static DEVICE_ATTR(cm_slope0, S_IRUGO, ist30xxc_cm_slope0_show, NULL);
static DEVICE_ATTR(cm_slope1, S_IRUGO, ist30xxc_cm_slope1_show, NULL);
static DEVICE_ATTR(cs, S_IRUGO, ist30xxc_cs_show, NULL);
static DEVICE_ATTR(cm_result, S_IRUGO, ist30xxc_cm_result_show, NULL);
static DEVICE_ATTR(cm_slope0_result, S_IRUGO,
		   ist30xxc_cm_slope0_result_show, NULL);
static DEVICE_ATTR(cm_slope1_result, S_IRUGO,
		   ist30xxc_cm_slope1_result_show, NULL);
static DEVICE_ATTR(cm_key_slope_result, S_IRUGO,
		   ist30xxc_cm_key_slope_result_show, NULL);
static DEVICE_ATTR(cs_result, S_IRUGO, ist30xxc_cs_result_show, NULL);
static DEVICE_ATTR(line_cm, S_IRUGO, ist30xxc_line_cm_show, NULL);
static DEVICE_ATTR(line_cm_slope0, S_IRUGO, ist30xxc_line_cm_slope0_show, NULL);
static DEVICE_ATTR(line_cm_slope1, S_IRUGO, ist30xxc_line_cm_slope1_show, NULL);
static DEVICE_ATTR(line_cs, S_IRUGO, ist30xxc_line_cs_show, NULL);
static DEVICE_ATTR(cm_key_slope_value, S_IRUGO,
		   ist30xxc_cm_key_slope_value_show, NULL);

static struct attribute *cmcs_attributes[] = {
	&dev_attr_info.attr,
	&dev_attr_cmcs_binary.attr,
	&dev_attr_cmcs_custom.attr,
	&dev_attr_cmcs_sdcard.attr,
	&dev_attr_cm.attr,
	&dev_attr_cm_spec_min.attr,
    &dev_attr_cm_spec_max.attr,
	&dev_attr_cm_slope0.attr,
	&dev_attr_cm_slope1.attr,
	&dev_attr_cs.attr,
	&dev_attr_cm_result.attr,
	&dev_attr_cm_slope0_result.attr,
	&dev_attr_cm_slope1_result.attr,
	&dev_attr_cm_key_slope_result.attr,
	&dev_attr_cs_result.attr,
	&dev_attr_line_cm.attr,
	&dev_attr_line_cm_slope0.attr,
	&dev_attr_line_cm_slope1.attr,
	&dev_attr_line_cs.attr,
	&dev_attr_cm_key_slope_value.attr,
	NULL,
};

static struct attribute_group cmcs_attr_group = {
	.attrs	= cmcs_attributes,
};

extern struct class *ist30xxc_class;
struct device *ist30xxc_cmcs_dev;

int ist30xxc_init_cmcs_sysfs(struct ist30xxc_data *data)
{
	/* /sys/class/touch/cmcs */
	ist30xxc_cmcs_dev = device_create(ist30xxc_class, NULL, 0, data, "cmcs");

	/* /sys/class/touch/cmcs/... */
	if (unlikely(sysfs_create_group(&ist30xxc_cmcs_dev->kobj,
					&cmcs_attr_group)))
		ts_err("Failed to create sysfs group(%s)!\n", "cmcs");

#if IST30XXC_INTERNAL_CMCS_BIN
	tsp_cmcs_bin = (u8 *)ist30xx_cmcs;
	tsp_cmcs_bin_size = sizeof(ist30xx_cmcs);

	ist30xxc_get_cmcs_info(tsp_cmcs_bin, tsp_cmcs_bin_size);
#endif

	return 0;
}
