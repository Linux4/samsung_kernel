/*
 * spi-sensor-hub-spi-fpga.c
 *
 * (c) 2014 Shmuel Ungerfeld <sungerfeld@sensorplatforms.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * Copyright (C) 2010 MEMSIC, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <linux/workqueue.h>
#include <linux/hwmon-sysfs.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/firmware.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <linux/spi/fpga_i2c_expander.h>

#define SINGLE_THREAD_ENABLE
#define FPGA_KPI_ENABLE

#ifdef FPGA_KPI_ENABLE
struct timespec ts;
struct timespec td;
int time_diff;
#endif

extern struct fpga_i2c_expander *g_fpga_i2c_expander;
extern unsigned bufsiz;

void fpga_work(struct fpga_i2c_expander *fpga_i2c_expander,  struct spi_list_s *spi_data);

int fpga_i2c_exp_rx_data(
	struct fpga_i2c_expander *fpga_i2c_expander,
	char *txbuf,
	char *rxbuf,
	int len)
{
	int err;
	u8 mode;
	struct spi_device *spi;
	struct spi_message msg;
	struct spi_transfer xfer = {
		.tx_buf = txbuf,
		.rx_buf = rxbuf,
		.len = len,
		.cs_change = 0,
		.speed_hz=SPI_CLK_FREQ,
	};

	mode = 0;	//spi mode3 is set in probe funcion but here we set for spi mode0//if we want spi_mode_3 we can commnet these 3 lines
	spi = spi_dev_get(fpga_i2c_expander->spi_client);
	spi->mode=mode;

//	gpio_set_value (fpga_i2c_expander->pdata->fpgaGpioSpiCs, 0);

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	err = spi_sync(fpga_i2c_expander->spi_client, &msg);

//	gpio_set_value (fpga_i2c_expander->pdata->fpgaGpioSpiCs, 1);

	return err;
}

int fpga_i2c_exp_tx_data(
	struct fpga_i2c_expander *fpga_i2c_expander,
	char *txbuf,
	int len)
{
	int err;
	u8 mode;
	struct spi_device *spi;
	struct spi_message msg;

	struct spi_transfer xfer = {
		.tx_buf = txbuf,
		.rx_buf = NULL,
		.len = len,
		.cs_change = 0,
		.speed_hz=SPI_CLK_FREQ,
	};

	mode = 0;//spi mode3 is set in probe funcion but here we set for spi mode0//if we want spi_mode_3 we can commnet these 3 lines
	spi = spi_dev_get(fpga_i2c_expander->spi_client);
	spi->mode=mode;

//	gpio_set_value (fpga_i2c_expander->pdata->fpgaGpioSpiCs, 0);

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	err = spi_sync(fpga_i2c_expander->spi_client, &msg);

//	gpio_set_value (fpga_i2c_expander->pdata->fpgaGpioSpiCs, 1);

	return err;
}

static int fpga_spi_cmd_queue(struct fpga_i2c_expander *fpga_i2c_expander,  struct spi_list_s *spi_data)
{
	int ret = 0;

#if defined(SINGLE_THREAD_ENABLE)
	fpga_work(fpga_i2c_expander, spi_data);
#else
	mutex_lock(&fpga_i2c_expander->spi_cmd_lock);
	list_add_tail(&spi_data->list, &fpga_i2c_expander->spi_cmd_queue);
	mutex_unlock(&fpga_i2c_expander->spi_cmd_lock);
	schedule_work(&fpga_i2c_expander->spi_cmd_func);
#endif

	return ret;
}

static int fpga_spi_block_write(struct fpga_i2c_expander *fpga_i2c_expander,
		struct spi_cmd_s *spi_cmd)
{
	int ret = 0;

	memset(&fpga_i2c_expander->spi_mem->spi_xfer, 0, sizeof(fpga_i2c_expander->spi_mem->spi_xfer));
	spi_message_init(&fpga_i2c_expander->spi_mem->spi_cmd);

	if(spi_cmd->tx_length0){
		fpga_i2c_expander->spi_mem->spi_xfer[0].len =  spi_cmd->tx_length0;
		fpga_i2c_expander->spi_mem->spi_xfer[0].tx_buf = spi_cmd->data_tx0;
		fpga_i2c_expander->spi_mem->spi_xfer[0].speed_hz = SPI_CLK_FREQ;

		if(spi_cmd->tx_length1){
			fpga_i2c_expander->spi_mem->spi_xfer[0].cs_change = 1;
		}
		spi_message_add_tail(&fpga_i2c_expander->spi_mem->spi_xfer[0], &fpga_i2c_expander->spi_mem->spi_cmd);
	}

	if(spi_cmd->tx_length1){
		fpga_i2c_expander->spi_mem->spi_xfer[1].len =  spi_cmd->tx_length1;
		fpga_i2c_expander->spi_mem->spi_xfer[1].tx_buf = spi_cmd->data_tx1;
		fpga_i2c_expander->spi_mem->spi_xfer[1].cs_change = 0;
		fpga_i2c_expander->spi_mem->spi_xfer[1].speed_hz = SPI_CLK_FREQ;
		spi_message_add_tail(&fpga_i2c_expander->spi_mem->spi_xfer[1], &fpga_i2c_expander->spi_mem->spi_cmd);
	}

	ret = spi_sync(fpga_i2c_expander->spi_client, &fpga_i2c_expander->spi_mem->spi_cmd);
	if (ret != 0) {
		pr_err("[FPGA] SPI WRITE block failed\n");
	}

	return ret;
}

static int fpga_spi_block_one_byte_read(struct fpga_i2c_expander *fpga_i2c_expander,
		struct spi_cmd_s *spi_cmd)
{
	int ret = 0;

	memset(&fpga_i2c_expander->spi_mem->spi_xfer, 0, sizeof(fpga_i2c_expander->spi_mem->spi_xfer));

	spi_message_init(&fpga_i2c_expander->spi_mem->spi_cmd);

	fpga_i2c_expander->spi_mem->spi_xfer[0].tx_buf = spi_cmd->data_tx0;
	fpga_i2c_expander->spi_mem->spi_xfer[0].rx_buf = spi_cmd->data_rx0;
	fpga_i2c_expander->spi_mem->spi_xfer[0].len = spi_cmd->tx_length0;
	fpga_i2c_expander->spi_mem->spi_xfer[0].speed_hz = SPI_CLK_1MHZ;
	spi_message_add_tail(&fpga_i2c_expander->spi_mem->spi_xfer[0],
			&fpga_i2c_expander->spi_mem->spi_cmd);

	ret = spi_sync(fpga_i2c_expander->spi_client, &fpga_i2c_expander->spi_mem->spi_cmd);

	if (ret != 0) {
		pr_err("[FPGA] SPI read block failed\n");
	}

	return ret;
}

static int fpga_spi_block_read(struct fpga_i2c_expander *fpga_i2c_expander,
		struct spi_cmd_s *spi_cmd)
{
	int ret = 0;

	memset(&fpga_i2c_expander->spi_mem->spi_xfer, 0, sizeof(fpga_i2c_expander->spi_mem->spi_xfer));
	spi_message_init(&fpga_i2c_expander->spi_mem->spi_cmd);

	if((spi_cmd->tx_length2 == 1)&&(spi_cmd->tx_length3 == 1)){
		fpga_i2c_expander->spi_mem->spi_xfer[0].tx_buf = spi_cmd->data_tx0;
		fpga_i2c_expander->spi_mem->spi_xfer[0].len = spi_cmd->tx_length0;
		fpga_i2c_expander->spi_mem->spi_xfer[0].cs_change = 1;
		fpga_i2c_expander->spi_mem->spi_xfer[0].delay_usecs = 60;
		fpga_i2c_expander->spi_mem->spi_xfer[0].speed_hz = SPI_CLK_FREQ;
		spi_message_add_tail(&fpga_i2c_expander->spi_mem->spi_xfer[0],
								&fpga_i2c_expander->spi_mem->spi_cmd);

		fpga_i2c_expander->spi_mem->spi_xfer[1].tx_buf = spi_cmd->data_tx1;
		fpga_i2c_expander->spi_mem->spi_xfer[1].len = spi_cmd->tx_length1;
		fpga_i2c_expander->spi_mem->spi_xfer[1].cs_change = 1;
		fpga_i2c_expander->spi_mem->spi_xfer[1].delay_usecs =60;
		fpga_i2c_expander->spi_mem->spi_xfer[1].speed_hz = SPI_CLK_FREQ;
		spi_message_add_tail(&fpga_i2c_expander->spi_mem->spi_xfer[1],
								&fpga_i2c_expander->spi_mem->spi_cmd);

		fpga_i2c_expander->spi_mem->spi_xfer[2].tx_buf = spi_cmd->data_tx2;
		fpga_i2c_expander->spi_mem->spi_xfer[2].rx_buf = spi_cmd->data_rx0;
		fpga_i2c_expander->spi_mem->spi_xfer[2].len = (spi_cmd->tx_length2+spi_cmd->rx_length0);
		fpga_i2c_expander->spi_mem->spi_xfer[2].cs_change = 1;
		fpga_i2c_expander->spi_mem->spi_xfer[2].speed_hz = SPI_CLK_FREQ;
		//fpga_i2c_expander->spi_mem->spi_xfer[2].delay_usecs = 110;
		spi_message_add_tail(&fpga_i2c_expander->spi_mem->spi_xfer[2],
		&fpga_i2c_expander->spi_mem->spi_cmd);

		fpga_i2c_expander->spi_mem->spi_xfer[3].tx_buf = spi_cmd->data_tx3;
		fpga_i2c_expander->spi_mem->spi_xfer[3].rx_buf = spi_cmd->data_rx1;
		fpga_i2c_expander->spi_mem->spi_xfer[3].len =(spi_cmd->tx_length3+spi_cmd->rx_length1);
		fpga_i2c_expander->spi_mem->spi_xfer[3].cs_change = 0;
		fpga_i2c_expander->spi_mem->spi_xfer[3].speed_hz = SPI_CLK_FREQ;
		//fpga_i2c_expander->spi_mem->spi_xfer[3].delay_usecs = 110;
		spi_message_add_tail(&fpga_i2c_expander->spi_mem->spi_xfer[3],
								&fpga_i2c_expander->spi_mem->spi_cmd);

		ret = spi_sync(fpga_i2c_expander->spi_client, &fpga_i2c_expander->spi_mem->spi_cmd);
		if (ret != 0) {
			pr_err("[FPGA] SPI read block failed\n");
		}

	}
	else{
		fpga_i2c_expander->spi_mem->spi_xfer[0].tx_buf = spi_cmd->data_tx0;
		fpga_i2c_expander->spi_mem->spi_xfer[0].rx_buf = spi_cmd->data_rx0;
		fpga_i2c_expander->spi_mem->spi_xfer[0].len = (spi_cmd->tx_length0+spi_cmd->rx_length0);
		fpga_i2c_expander->spi_mem->spi_xfer[0].speed_hz = SPI_CLK_FREQ;
		spi_message_add_tail(&fpga_i2c_expander->spi_mem->spi_xfer[0],
								&fpga_i2c_expander->spi_mem->spi_cmd);

		ret = spi_sync(fpga_i2c_expander->spi_client, &fpga_i2c_expander->spi_mem->spi_cmd);
		if (ret != 0) {
			pr_err("[FPGA] SPI read block failed\n");
		}
	}

	return ret;
}

void fpga_spi_queue_handle(struct work_struct *work)
{
	struct fpga_i2c_expander *fpga_i2c_expander = container_of
		(work, struct fpga_i2c_expander, spi_cmd_func);
	struct spi_list_s *req;
	struct list_head *entry;
	int ret=0;
	int cmd;
#if !defined(SINGLE_THREAD_ENABLE)
	int ch_num;
#endif

#if defined(FPGA_I2C_EXPANDER_DBG)
	char buf[512];
	int i;
	int idx;
#endif

	mutex_lock(&fpga_i2c_expander->spi_cmd_lock);
	if(list_empty(&fpga_i2c_expander->spi_cmd_queue)){
		mutex_unlock(&fpga_i2c_expander->spi_cmd_lock);
		return ;
	}
	mutex_unlock(&fpga_i2c_expander->spi_cmd_lock);

next_cmd:
	mutex_lock(&fpga_i2c_expander->spi_cmd_lock);
	entry = &fpga_i2c_expander->spi_cmd_queue;
	req = list_entry(entry->next,  struct spi_list_s, list);
	list_del(&req->list);
	mutex_unlock(&fpga_i2c_expander->spi_cmd_lock);

#if defined(FPGA_I2C_EXPANDER_DBG)
	pr_err("%s(%d): op_code:%.2X s_addr:%.2X\n", __func__, __LINE__, req->cmds->op_code, req->cmds->slave_addr);
	if (req->cmds->tx_length0) {
		pr_err("tx_length0:%d\n", req->cmds->tx_length0);
		idx = 0;
		for (i = 0; i < req->cmds->tx_length0; i++) {
			idx += sprintf(buf+idx, "%.2X ", req->cmds->data_tx0[i]);
			if (((i+1) % 16) == 0) {
				pr_err("%s\n", buf);
				idx = 0;
			}
		}
		pr_err("%s\n", buf);
	}

	if (req->cmds->tx_length1) {
		pr_err("tx_length1:%d\n", req->cmds->tx_length1);
		idx = 0;
		for (i = 0; i < req->cmds->tx_length1; i++) {
			idx += sprintf(buf+idx, "%.2X ", req->cmds->data_tx1[i]);
			if (((i+1) % 16) == 0) {
				pr_err("%s\n", buf);
				idx = 0;
			}
		}
		pr_err("%s\n", buf);
	}

	if (req->cmds->tx_length2) {
		pr_err("tx_length2:%d\n", req->cmds->tx_length2);
		idx = 0;
		for (i = 0; i < req->cmds->tx_length2; i++) {
			idx += sprintf(buf+idx, "%.2X ", req->cmds->data_tx2[i]);
			if (((i+1) % 16) == 0) {
				pr_err("%s\n", buf);
				idx = 0;
			}
		}
		pr_err("%s\n", buf);
	}

	if (req->cmds->tx_length3) {
		pr_err("tx_length3:%d\n", req->cmds->tx_length3);
		idx = 0;
		for (i = 0; i < req->cmds->tx_length3; i++) {
			idx += sprintf(buf+idx, "%.2X ", req->cmds->data_tx3[i]);
			if (((i+1) % 16) == 0) {
				pr_err("%s\n", buf);
				idx = 0;
			}
		}
		pr_err("%s\n", buf);
	}
#endif

	cmd = req->cmds->op_code & 0xF0;
#if !defined(SINGLE_THREAD_ENABLE)
	ch_num = req->cmds->op_code & 0x0F;
#endif

	if(cmd == DEVICE_I2C_WR){
		ret = fpga_spi_block_write(fpga_i2c_expander,req->cmds);
	}
	else if(cmd == DEVICE_I2C_1M_RD) {
		ret = fpga_spi_block_one_byte_read(fpga_i2c_expander,req->cmds);
	}
	else{
		ret = fpga_spi_block_read(fpga_i2c_expander,req->cmds);
	}

	req->cmds->ret = ret;  //SPI interface return value..
#if !defined(SINGLE_THREAD_ENABLE)
	complete(&fpga_i2c_expander->device_complete[ch_num]);
#endif

#if defined(FPGA_I2C_EXPANDER_DBG)
	if (req->cmds->rx_length0) {
		pr_err("rx_length0:%d\n", req->cmds->rx_length0);
		idx = 0;
		for (i = 0; i < req->cmds->rx_length0; i++) {
			idx += sprintf(buf+idx, "%.2X ", req->cmds->data_rx0[i]);
			if (((i+1) % 16) == 0) {
				pr_err("%s\n", buf);
				idx = 0;
			}
		}
		pr_err("%s\n", buf);
	}

	if (req->cmds->rx_length1) {
		pr_err("rx_length1:%d\n", req->cmds->rx_length1);
		idx = 0;
		for (i = 0; i < req->cmds->rx_length1; i++) {
			idx += sprintf(buf+idx, "%.2X ", req->cmds->data_rx1[i]);
			if (((i+1) % 16) == 0) {
				pr_err("%s\n", buf);
				idx = 0;
			}
		}
		pr_err("%s\n", buf);
	}
#endif
	mutex_lock(&fpga_i2c_expander->spi_cmd_lock);
	if(list_empty(&fpga_i2c_expander->spi_cmd_queue)){
		mutex_unlock(&fpga_i2c_expander->spi_cmd_lock);
		return ;
	}
	else{
		mutex_unlock(&fpga_i2c_expander->spi_cmd_lock);
		goto next_cmd;
	}

	return ;
}

int fpga_i2c_exp_write(	u8 ch_num, u8 s_addr, u16 offset, u8 offset_len, u8 * buf, u16 buf_len)
{
	int ret =0;
	u8 retry_cnt = 0;
	u16 wait_time =0;
	u8 init[5] = {0x00, s_addr, 0xFF, 0xFF,0x01};
	u8 clear[5] = {(FPGA_BASE+FPGA_CH_OFFSET*ch_num), 0x00, 0xFF, 0xFF, 0x80};

	struct spi_list_s *i2c_cmd;

#ifdef FPGA_KPI_ENABLE
	ts = ktime_to_timespec(ktime_get());
#endif

#if defined(FPGA_I2C_EXPANDER_DBG)
	pr_err("%s(%d): ch_num:%.2X s_addr:%.2X offset:%.2X offset_len:%.2X buf:%.2X buf_len:%.2X\n",
			__func__, __LINE__, ch_num, s_addr, offset, offset_len, buf[0], buf_len);
#endif

	i2c_cmd = g_fpga_i2c_expander->spi_cmd[ch_num];

	mutex_lock(&i2c_cmd->device_lock);

	i2c_cmd->cmds->ret = 0;
	memset(&i2c_cmd->cmds->data_tx0, 0x00, 300);
	memset(&i2c_cmd->cmds->data_tx1, 0x00, 20);
	memset(&i2c_cmd->cmds->data_tx2, 0x00, 20);
	memset(&i2c_cmd->cmds->data_tx3, 0x00, 20);

	memcpy(&i2c_cmd->cmds->data_tx0, clear, 5);
	memcpy(&i2c_cmd->cmds->data_tx0[5], init, 5);

	if(offset_len == 1) {
		i2c_cmd->cmds->data_tx0[7] = buf_len + 1; //WRITE BYTE count..
		i2c_cmd->cmds->data_tx0[10] = offset;
		memcpy(&i2c_cmd->cmds->data_tx0[11], buf, buf_len);
		i2c_cmd->cmds->tx_length0 = buf_len + 11;
	}
	else if(offset_len == 2) {
		i2c_cmd->cmds->data_tx0[7] = buf_len + 2; //WRITE BYTE count..
		i2c_cmd->cmds->data_tx0[10] = (offset >> 8) & 0xff;
		i2c_cmd->cmds->data_tx0[11] = offset;
		memcpy(&i2c_cmd->cmds->data_tx0[12], buf, buf_len);
		i2c_cmd->cmds->tx_length0 = buf_len + 12;
	}

	i2c_cmd->cmds->tx_length1 = 0;
	i2c_cmd->cmds->tx_length2 = 0;
	i2c_cmd->cmds->tx_length3 = 0;
	i2c_cmd->cmds->rx_length0 = 0;
	i2c_cmd->cmds->rx_length1 = 0;

	i2c_cmd->cmds->op_code = DEVICE_I2C_WR + ch_num;
#if !defined(SINGLE_THREAD_ENABLE)
	init_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
	fpga_spi_cmd_queue(g_fpga_i2c_expander, i2c_cmd);
#if !defined(SINGLE_THREAD_ENABLE)
	wait_for_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
	if(i2c_cmd->cmds->ret != 0){
		ret = i2c_cmd->cmds->ret;   //SPI return value check..
		pr_err("[FPGA] %s(%d): SPI Error :%d\n", __func__, __LINE__, ret);
		goto fail;
	}

	wait_time = (buf_len/10);
	if(wait_time >1 ){
		wait_time = (wait_time -1)*250 +((buf_len%10)*25);
	}
	else if(wait_time == 1){
		wait_time = ((buf_len%10)*25);
	}
	else {
		wait_time = 0;
	}

status_check:
	memset(&i2c_cmd->cmds->data_tx0, 0x00, 300);
	memset(&i2c_cmd->cmds->data_tx1, 0x00, 20);

	i2c_cmd->cmds->data_tx0[0] = (FPGA_STATUS+FPGA_CH_OFFSET*ch_num);
	i2c_cmd->cmds->tx_length0 = 1;
	i2c_cmd->cmds->rx_length0 = 2;
	i2c_cmd->cmds->tx_length1 = 0;
	i2c_cmd->cmds->tx_length2 = 0;
	i2c_cmd->cmds->tx_length3 = 0;
	i2c_cmd->cmds->rx_length1 = 0;
	i2c_cmd->cmds->op_code = DEVICE_I2C_RD + ch_num;
	usleep_range(wait_time, wait_time);

#if !defined(SINGLE_THREAD_ENABLE)
	init_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
	fpga_spi_cmd_queue(g_fpga_i2c_expander, i2c_cmd);
#if !defined(SINGLE_THREAD_ENABLE)
	wait_for_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
	if(i2c_cmd->cmds->ret != 0){
		ret = i2c_cmd->cmds->ret;   //SPI return value check..
		pr_err("[FPGA] %s(%d): SPI Error :%d\n", __func__, __LINE__, ret);
		goto fail;
	}

	if(i2c_cmd->cmds->data_rx0[1] & FPGA_I2C_TX_ERR){
		pr_err("[FPGA] %s(%d): FPGA_I2C_TX_ERR\n", __func__, __LINE__);

		memset(&i2c_cmd->cmds->data_tx0, 0x00, 300);
		memcpy(&i2c_cmd->cmds->data_tx0, clear, 5);
		i2c_cmd->cmds->tx_length0 = 5;
		i2c_cmd->cmds->tx_length1 = 0;
		i2c_cmd->cmds->tx_length2 = 0;
		i2c_cmd->cmds->tx_length3 = 0;
		i2c_cmd->cmds->rx_length0 = 0;
		i2c_cmd->cmds->rx_length1 = 0;

		i2c_cmd->cmds->op_code = DEVICE_I2C_WR + ch_num;
#if !defined(SINGLE_THREAD_ENABLE)
		init_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
		fpga_spi_cmd_queue(g_fpga_i2c_expander, i2c_cmd);
#if !defined(SINGLE_THREAD_ENABLE)
		wait_for_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
		if(i2c_cmd->cmds->ret != 0){
			ret = i2c_cmd->cmds->ret;   //SPI return value check..
			pr_err("[FPGA] %s(%d): SPI Error :%d\n", __func__, __LINE__, ret);
			goto fail;
		}
		ret = -EIO;
		goto fail;
	}
	else if(i2c_cmd->cmds->data_rx0[1] & FPGA_I2C_BUSY){
		if(retry_cnt<10){
			retry_cnt++;
			wait_time = 0;
			goto status_check; 
		}

		if(retry_cnt == 10){
			memset(&i2c_cmd->cmds->data_tx0, 0x00, 300);
			memcpy(&i2c_cmd->cmds->data_tx0, clear, 5);
			i2c_cmd->cmds->tx_length0 = 5;
			i2c_cmd->cmds->tx_length1 = 0;
			i2c_cmd->cmds->tx_length2 = 0;
			i2c_cmd->cmds->tx_length3 = 0;
			i2c_cmd->cmds->rx_length0 = 0;
			i2c_cmd->cmds->rx_length1 = 0;

			i2c_cmd->cmds->op_code = DEVICE_I2C_WR + ch_num;
#if !defined(SINGLE_THREAD_ENABLE)
			init_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
			fpga_spi_cmd_queue(g_fpga_i2c_expander, i2c_cmd);
#if !defined(SINGLE_THREAD_ENABLE)
			wait_for_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
			if(i2c_cmd->cmds->ret != 0){
				ret = i2c_cmd->cmds->ret;   //SPI return value check..
				pr_err("[FPGA] %s(%d): SPI Error :%d\n", __func__, __LINE__, ret);
				goto fail;
			}
			pr_err("[FPGA] %s(%d): FPGA_I2C_BUSY 10 times trial done\n", __func__, __LINE__);
			ret = -EIO;
			goto fail;
		}
	}

#ifdef FPGA_KPI_ENABLE
	td = ktime_to_timespec(ktime_get());
	time_diff = (td.tv_sec - ts.tv_sec) + (td.tv_nsec - ts.tv_nsec);
	if (time_diff > HANDLE_TIME)
		pr_err("[FPGA] %s Latency was over HANDLE_TIME! Latency : %d, HANDLE_TIME : %d\n",__func__,time_diff,HANDLE_TIME);
#endif

fail:
	mutex_unlock(&i2c_cmd->device_lock);
	return ret;
}

int fpga_i2c_exp_read(u8 ch_num, u8 s_addr, u16 offset, u8 offset_len, u8 * buf, u16 buf_len)
{
	int ret =0;
	u8 retry_cnt = 0;
	u16 wait_time =0;
	u16 first_wait_time=0;
	u8 init[4] = {0x00, s_addr, 0xFF, 0xFF};
	u8 clear[5] = {(FPGA_BASE+FPGA_CH_OFFSET*ch_num), 0x00, 0xFF, 0xFF, 0x80};

	u8 first_packet[7] = {(FPGA_BASE+FPGA_CH_OFFSET*ch_num), 0x01, 0x80, s_addr, 0x01, 0x01, offset};
	u8 second_packet[4] = {0x80, s_addr, buf_len, 0x0F};
	u8 third_packet[3] = {0x00,0x01,0x00};
	u8 multi_third_packet[12] = {0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x00};

	struct spi_list_s *i2c_cmd;

#if defined(FPGA_I2C_EXPANDER_DBG)
	pr_err("%s(%d): ch_num:%.2X s_addr:%.2X offset:%.2X offset_len:%.2X buf:%.2X buf_len:%.2X\n",
			__func__, __LINE__, ch_num, s_addr, offset, offset_len, buf[0], buf_len);
#endif

#ifdef FPGA_KPI_ENABLE
	ts = ktime_to_timespec(ktime_get());
#endif

	i2c_cmd = g_fpga_i2c_expander->spi_cmd[ch_num];

	mutex_lock(&i2c_cmd->device_lock);
	i2c_cmd->cmds->ret = 0;

	memset(&i2c_cmd->cmds->data_tx0, 0x00, 300);
	memset(&i2c_cmd->cmds->data_tx1, 0x00, 20);
	memset(&i2c_cmd->cmds->data_tx2, 0x00, 20);
	memset(&i2c_cmd->cmds->data_tx3, 0x00, 20);
	memset(&i2c_cmd->cmds->data_rx0, 0x00, 2040);
	memset(&i2c_cmd->cmds->data_rx1, 0x00, 10);

	if(buf_len == 1 && offset_len == 1){
		memcpy(&i2c_cmd->cmds->data_tx0, first_packet, 7);
		memcpy(&i2c_cmd->cmds->data_tx0[14], second_packet, 4);
		i2c_cmd->cmds->data_tx0[18] = 0x01;
		memcpy(&i2c_cmd->cmds->data_tx0[25], third_packet, 3);
		i2c_cmd->cmds->tx_length0 = 28;
		i2c_cmd->cmds->rx_length0 = 26 + buf_len;
		i2c_cmd->cmds->op_code = DEVICE_I2C_1M_RD + ch_num;

#if !defined(SINGLE_THREAD_ENABLE)
		init_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
		fpga_spi_cmd_queue(g_fpga_i2c_expander, i2c_cmd);
#if !defined(SINGLE_THREAD_ENABLE)
		wait_for_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
		if(i2c_cmd->cmds->ret != 0) {
			ret = i2c_cmd->cmds->ret;   //SPI return value check..
			pr_err("[FPGA] %s(%d): SPI Error :%d\n", __func__, __LINE__, ret);
			goto fail;
		}

		if(i2c_cmd->cmds->data_rx0[25] &
				( FPGA_I2C_RX_ERR|FPGA_I2C_TX_ERR)){
			pr_err("[FPGA] %s(%d): RTX ERR :%d\n", __func__, __LINE__,
					i2c_cmd->cmds->data_rx0[25]);
			memset(&i2c_cmd->cmds->data_tx0, 0x00, 300);
			memcpy(&i2c_cmd->cmds->data_tx0, clear, 5);
			i2c_cmd->cmds->tx_length0 = 5;
			i2c_cmd->cmds->tx_length1 = 0;
			i2c_cmd->cmds->tx_length2 = 0;
			i2c_cmd->cmds->tx_length3 = 0;
			i2c_cmd->cmds->rx_length0 = 0;
			i2c_cmd->cmds->rx_length1 = 0;

			i2c_cmd->cmds->op_code = DEVICE_I2C_WR + ch_num;
#if !defined(SINGLE_THREAD_ENABLE)
			init_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
			fpga_spi_cmd_queue(g_fpga_i2c_expander, i2c_cmd);
#if !defined(SINGLE_THREAD_ENABLE)
			wait_for_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
			if(i2c_cmd->cmds->ret != 0){
				ret = i2c_cmd->cmds->ret;   //SPI return value check..
				pr_err("[FPGA] %s(%d): SPI Error :%d\n", __func__, __LINE__, ret);
				goto fail;
			}

			ret = -EIO;
			goto fail;
		}
		else if(i2c_cmd->cmds->data_rx0[25] & FPGA_I2C_BUSY){
			retry_cnt++;
			goto RTX_status_check;
		}
		else if(i2c_cmd->cmds->data_rx0[25] &FPGA_I2C_RX_DONE){
			memcpy(buf,&i2c_cmd->cmds->data_rx0[26], buf_len);
			goto success;
		}
		else{
			pr_err("[FPGA] %s(%d): Unknown data :%x\n", __func__, __LINE__,
					i2c_cmd->cmds->data_rx0[25]);
			memset(i2c_cmd->cmds, 0x00, sizeof(struct spi_cmd_s));
			memcpy(&i2c_cmd->cmds->data_tx0, clear, 5);
			i2c_cmd->cmds->tx_length0 = 5;
			i2c_cmd->cmds->op_code = DEVICE_I2C_WR + ch_num;
#if !defined(SINGLE_THREAD_ENABLE)
			init_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
			fpga_spi_cmd_queue(g_fpga_i2c_expander, i2c_cmd);
#if !defined(SINGLE_THREAD_ENABLE)
			wait_for_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
			if(i2c_cmd->cmds->ret != 0){
				ret = i2c_cmd->cmds->ret;   //SPI return value check..
				pr_err("[FPGA] %s(%d): SPI Error :%d\n", __func__, __LINE__, ret);
				goto fail;
			}
			ret = -EIO;
			goto fail;
		}
	}
	else if ((1 < buf_len) && (buf_len < 11) && (offset_len == 1)) {
		memcpy(&i2c_cmd->cmds->data_tx0, first_packet, 7);
		memcpy(&i2c_cmd->cmds->data_tx0[14], second_packet, 4);
		i2c_cmd->cmds->data_tx0[1] = 0x03;
		i2c_cmd->cmds->data_tx0[18] = 0x03;
		memcpy(&i2c_cmd->cmds->data_tx0[58], multi_third_packet, 12);
		i2c_cmd->cmds->tx_length0 = 70;
		i2c_cmd->cmds->rx_length0 = 59 + buf_len;
		i2c_cmd->cmds->op_code = DEVICE_I2C_1M_RD + ch_num;

#if !defined(SINGLE_THREAD_ENABLE)
		init_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
		fpga_spi_cmd_queue(g_fpga_i2c_expander, i2c_cmd);
#if !defined(SINGLE_THREAD_ENABLE)
		wait_for_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
		if(i2c_cmd->cmds->ret !=0){
			ret = i2c_cmd->cmds->ret;   //SPI return value check..
			pr_err("[FPGA] %s(%d): SPI Error :%d  \n", __func__, __LINE__, ret);
			goto fail;
		}

		if(i2c_cmd->cmds->data_rx0[58] &
				( FPGA_I2C_RX_ERR|FPGA_I2C_TX_ERR)){
			pr_err("[FPGA] %s(%d): RTX ERR :%x  \n", __func__, __LINE__,
					i2c_cmd->cmds->data_rx0[58]);
			memset(i2c_cmd->cmds, 0x00, sizeof(struct spi_cmd_s));
			memcpy(&i2c_cmd->cmds->data_tx0, clear, 5);
			i2c_cmd->cmds->tx_length0 = 5;
			i2c_cmd->cmds->op_code = DEVICE_I2C_WR + ch_num;
#if !defined(SINGLE_THREAD_ENABLE)
			init_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
			fpga_spi_cmd_queue(g_fpga_i2c_expander, i2c_cmd);
#if !defined(SINGLE_THREAD_ENABLE)
			wait_for_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
			if(i2c_cmd->cmds->ret !=0){
				ret = i2c_cmd->cmds->ret;   //SPI return value check..
				pr_err("[FPGA] %s(%d): SPI Error :%d  \n", __func__, __LINE__, ret);
				goto fail;
			}
			ret = -EIO;
			goto fail;
		}
		else if(i2c_cmd->cmds->data_rx0[58] &FPGA_I2C_RX_DONE){
			memcpy(buf,&i2c_cmd->cmds->data_rx0[59], buf_len);
			goto success;
		}
		else if(i2c_cmd->cmds->data_rx0[58] & FPGA_I2C_BUSY){
			retry_cnt ++;
			goto RTX_status_check;
		}
		else{
			pr_err("[FPGA] %s(%d): Unknown data :%x  \n", __func__, __LINE__,
					i2c_cmd->cmds->data_rx0[58]);
			memset(i2c_cmd->cmds, 0x00, sizeof(struct spi_cmd_s));
			memcpy(&i2c_cmd->cmds->data_tx0, clear, 5);
			i2c_cmd->cmds->tx_length0 = 5;
			i2c_cmd->cmds->op_code = DEVICE_I2C_WR + ch_num;
#if !defined(SINGLE_THREAD_ENABLE)
			init_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
			fpga_spi_cmd_queue(g_fpga_i2c_expander, i2c_cmd);
#if !defined(SINGLE_THREAD_ENABLE)
			wait_for_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
			if(i2c_cmd->cmds->ret !=0){
				ret = i2c_cmd->cmds->ret;   //SPI return value check..
				pr_err("[FPGA] %s(%d): SPI Error :%d  \n", __func__, __LINE__, ret);
				goto fail;
			}

			ret = -EIO;
			goto fail;
		}

	}
	else{
		if(offset_len == 1) {
			memcpy(&i2c_cmd->cmds->data_tx0, first_packet, 7);
			memcpy(&i2c_cmd->cmds->data_tx0[14], second_packet, 4);
			i2c_cmd->cmds->data_tx0[18] = 0x00;
			i2c_cmd->cmds->tx_length0 = 19;
			i2c_cmd->cmds->op_code= DEVICE_I2C_1M_RD + ch_num;
		}
		else if(offset_len == 2) {
			memcpy(&i2c_cmd->cmds->data_tx0, clear, 5);
			memcpy(&i2c_cmd->cmds->data_tx0[5], init, 4);

			i2c_cmd->cmds->data_tx0[7] = 0x02; //WRITE BYTE count..
			i2c_cmd->cmds->data_tx0[9] = 0x01; // Configuration code..
			i2c_cmd->cmds->data_tx0[10] = (offset >> 8) & 0xff;
			i2c_cmd->cmds->data_tx0[11] = offset & 0xff;
			i2c_cmd->cmds->tx_length0 = 12;

			fpga_spi_cmd_queue(g_fpga_i2c_expander, i2c_cmd);
			usleep_range(200, 200);

			memset(&i2c_cmd->cmds->data_tx0, 0x00, 300);

			memcpy(&i2c_cmd->cmds->data_tx0, clear, 5);
			memcpy(&i2c_cmd->cmds->data_tx0[5], init, 4);
			i2c_cmd->cmds->data_tx0[5] = (buf_len >>8) & 0xff;
			i2c_cmd->cmds->data_tx0[8] = buf_len & 0xff;
			i2c_cmd->cmds->data_tx0[9] = 0x0F; // Configuration code..
			i2c_cmd->cmds->tx_length0 = 10;
			i2c_cmd->cmds->rx_length0 = 0;
			i2c_cmd->cmds->rx_length1 = 0;
			i2c_cmd->cmds->tx_length2 = 0;
			i2c_cmd->cmds->tx_length3 = 0;
			i2c_cmd->cmds->op_code= DEVICE_I2C_WR + ch_num;
		}

#if !defined(SINGLE_THREAD_ENABLE)
		init_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
		fpga_spi_cmd_queue(g_fpga_i2c_expander, i2c_cmd);
#if !defined(SINGLE_THREAD_ENABLE)
		wait_for_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
		if(i2c_cmd->cmds->ret !=0){
			ret = i2c_cmd->cmds->ret;   //SPI return value check..
			pr_err("[FPGA] %s(%d): SPI Error :%d\n", __func__, __LINE__, ret);
			goto fail;
		}

		wait_time = (buf_len/10);
		if(wait_time >1 ){
			wait_time = (wait_time -1)*250 +((buf_len%10)*25);
		}
		else if(wait_time == 1){
			wait_time = ((buf_len%10)*25);
		}
		else
		{
			wait_time = 0;
		}
		first_wait_time = wait_time;
	}

RTX_status_check:
	memset(&i2c_cmd->cmds->data_tx0, 0x00, 300);

	i2c_cmd->cmds->data_tx0[0] = (FPGA_STATUS+FPGA_CH_OFFSET*ch_num);
	i2c_cmd->cmds->tx_length0 = 1;
	i2c_cmd->cmds->rx_length0 = 2;
	i2c_cmd->cmds->tx_length1 = 0;
	i2c_cmd->cmds->tx_length2 = 0;
	i2c_cmd->cmds->tx_length3 = 0;
	i2c_cmd->cmds->rx_length1 = 0;

	i2c_cmd->cmds->op_code = DEVICE_I2C_RD + ch_num;

	usleep_range(wait_time, wait_time);

#if !defined(SINGLE_THREAD_ENABLE)
	init_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
	fpga_spi_cmd_queue(g_fpga_i2c_expander, i2c_cmd);
#if !defined(SINGLE_THREAD_ENABLE)
	wait_for_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
	if(i2c_cmd->cmds->ret !=0){
		ret = i2c_cmd->cmds->ret;   //SPI return value check..
		pr_err("[FPGA] %s(%d): SPI Error :%d\n", __func__, __LINE__, ret);
		goto fail;
	}

	if(i2c_cmd->cmds->data_rx0[1] & (FPGA_I2C_RX_ERR|FPGA_I2C_TX_ERR)){
		pr_err("[FPGA] %s(%d): RTX ERR :%d\n", __func__, __LINE__,
			i2c_cmd->cmds->data_rx0[1]);
		memset(&i2c_cmd->cmds->data_tx0, 0x00, 300);
		memcpy(&i2c_cmd->cmds->data_tx0, clear, 5);
		i2c_cmd->cmds->tx_length0 = 5;
		i2c_cmd->cmds->tx_length1 = 0;
		i2c_cmd->cmds->tx_length2 = 0;
		i2c_cmd->cmds->tx_length3 = 0;
		i2c_cmd->cmds->rx_length0 = 0;
		i2c_cmd->cmds->rx_length1 = 0;

		i2c_cmd->cmds->op_code = DEVICE_I2C_WR + ch_num;
#if !defined(SINGLE_THREAD_ENABLE)
		init_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
		fpga_spi_cmd_queue(g_fpga_i2c_expander, i2c_cmd);
#if !defined(SINGLE_THREAD_ENABLE)
		wait_for_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
		if(i2c_cmd->cmds->ret !=0){
			ret = i2c_cmd->cmds->ret;   //SPI return value check..
			pr_err("[FPGA] %s(%d): SPI Error :%d\n", __func__, __LINE__, ret);
			goto fail;
		}

		ret = -EIO;
		goto fail;
	}
	else if(i2c_cmd->cmds->data_rx0[1] & FPGA_I2C_BUSY){
		if(retry_cnt<30){
			retry_cnt++;
			wait_time = 0;
			if(retry_cnt == 10 )
				wait_time= first_wait_time/2;
			if(retry_cnt == 20 )
				wait_time= first_wait_time/4;
			goto RTX_status_check;
		}
		if(retry_cnt == 30){
			memset(&i2c_cmd->cmds->data_tx0, 0x00, 300);
			memcpy(&i2c_cmd->cmds->data_tx0, clear, 5);
			i2c_cmd->cmds->tx_length0 = 5;
			i2c_cmd->cmds->tx_length1 = 0;
			i2c_cmd->cmds->tx_length2 = 0;
			i2c_cmd->cmds->tx_length3 = 0;
			i2c_cmd->cmds->rx_length0 = 0;
			i2c_cmd->cmds->rx_length1 = 0;

			i2c_cmd->cmds->op_code = DEVICE_I2C_WR + ch_num;
#if !defined(SINGLE_THREAD_ENABLE)
			init_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
			fpga_spi_cmd_queue(g_fpga_i2c_expander, i2c_cmd);
#if !defined(SINGLE_THREAD_ENABLE)
			wait_for_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
			if(i2c_cmd->cmds->ret !=0){
				ret = i2c_cmd->cmds->ret;   //SPI return value check..
				pr_err("[FPGA] %s(%d): SPI Error :%d\n", __func__, __LINE__, ret);
				goto fail;
			}

			pr_err("[FPGA] %s(%d): FPGA_I2C_BUSY 30 times trial done \n", __func__, __LINE__);
			ret = -EIO;
			goto fail;
		}
	}
	else if(i2c_cmd->cmds->data_rx0[1] &FPGA_I2C_RX_DONE){
		memset(&i2c_cmd->cmds->data_tx0, 0x00, 300);
		memset(&i2c_cmd->cmds->data_tx1, 0x00, 20);

		i2c_cmd->cmds->data_tx0[0] = (FPGA_READ+FPGA_CH_OFFSET*ch_num);
		i2c_cmd->cmds->tx_length0 = 1;
		i2c_cmd->cmds->tx_length1 = 0;
		i2c_cmd->cmds->tx_length2 = 0;
		i2c_cmd->cmds->tx_length3 = 0;
		i2c_cmd->cmds->rx_length0 = buf_len;
		i2c_cmd->cmds->rx_length1 = 0;
		i2c_cmd->cmds->op_code = DEVICE_I2C_RD + ch_num;
#if !defined(SINGLE_THREAD_ENABLE)
		init_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
		fpga_spi_cmd_queue(g_fpga_i2c_expander, i2c_cmd);
#if !defined(SINGLE_THREAD_ENABLE)
		wait_for_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
		if(i2c_cmd->cmds->ret !=0){
			ret = i2c_cmd->cmds->ret;   //SPI return value check..
			pr_err("[FPGA] %s(%d): SPI Error :%d\n", __func__, __LINE__, ret);
			goto fail;
		}
		memcpy(buf,&i2c_cmd->cmds->data_rx0[1], buf_len);
	}
	else{
		pr_err("[FPGA] %s(%d): Unknown data :%x\n", __func__, __LINE__,
				i2c_cmd->cmds->data_rx0[1]);
		memset(i2c_cmd->cmds, 0x00, sizeof(struct spi_cmd_s));
		memcpy(&i2c_cmd->cmds->data_tx0, clear, 5);
		i2c_cmd->cmds->tx_length0 = 5;
		i2c_cmd->cmds->op_code = DEVICE_I2C_WR + ch_num;
#if !defined(SINGLE_THREAD_ENABLE)
		init_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
		fpga_spi_cmd_queue(g_fpga_i2c_expander, i2c_cmd);
#if !defined(SINGLE_THREAD_ENABLE)
		wait_for_completion(&g_fpga_i2c_expander->device_complete[ch_num]);
#endif
		if(i2c_cmd->cmds->ret !=0){
			ret = i2c_cmd->cmds->ret;   //SPI return value check..
			pr_err("[FPGA] %s(%d): SPI Error :%d\n", __func__, __LINE__, ret);
			goto fail;
		}

		ret = -EIO;
		goto fail;
	}
success:
#ifdef FPGA_KPI_ENABLE
	td = ktime_to_timespec(ktime_get());
	time_diff = (td.tv_sec - ts.tv_sec) + (td.tv_nsec - ts.tv_nsec);
	if (time_diff > HANDLE_TIME)
		pr_err("[FPGA] %s Latency was over HANDLE_TIME! Latency : %d, HANDLE_TIME : %d\n",__func__,time_diff,HANDLE_TIME);
#endif

fail:
	mutex_unlock(&i2c_cmd->device_lock);
	return ret;
}

void fpga_work(struct fpga_i2c_expander *fpga_i2c_expander,  struct spi_list_s *spi_data)
{
	struct spi_list_s *req;
	int ret=0;
	int cmd;

#if defined(FPGA_I2C_EXPANDER_DBG)
	char buf[512];
	int i;
	int idx;
#endif

	mutex_lock(&fpga_i2c_expander->spi_cmd_lock);
	req = spi_data;
	cmd = req->cmds->op_code & 0xF0;

#if defined(FPGA_I2C_EXPANDER_DBG)
	pr_err("%s(%d): op_code:%.2X s_addr:%.2X\n", __func__, __LINE__, req->cmds->op_code, req->cmds->slave_addr);
	if (req->cmds->tx_length0) {
		pr_err("tx_length0:%d\n", req->cmds->tx_length0);
		idx = 0;
		for (i = 0; i < req->cmds->tx_length0; i++) {
			idx += sprintf(buf+idx, "%.2X ", req->cmds->data_tx0[i]); 
			if (((i+1) % 16) == 0) {
				pr_err("%s\n", buf);
				idx = 0;
			}
		}
		pr_err("%s\n", buf);
	}

	if (req->cmds->tx_length1) {
		pr_err("tx_length1:%d\n", req->cmds->tx_length1);
		idx = 0;
		for (i = 0; i < req->cmds->tx_length1; i++) {
			idx += sprintf(buf+idx, "%.2X ", req->cmds->data_tx1[i]); 
			if (((i+1) % 16) == 0) {
				pr_err("%s\n", buf);
				idx = 0;
			}
		}
		pr_err("%s\n", buf);
	}

	if (req->cmds->tx_length2) {
		pr_err("tx_length2:%d\n", req->cmds->tx_length2);
		idx = 0;
		for (i = 0; i < req->cmds->tx_length2; i++) {
			idx += sprintf(buf+idx, "%.2X ", req->cmds->data_tx2[i]);
			if (((i+1) % 16) == 0) {
				pr_err("%s\n", buf);
				idx = 0;
			}
		}
		pr_err("%s\n", buf);
	}

	if (req->cmds->tx_length3) {
		pr_err("tx_length3:%d\n", req->cmds->tx_length3);
		idx = 0;
		for (i = 0; i < req->cmds->tx_length3; i++) {
			idx += sprintf(buf+idx, "%.2X ", req->cmds->data_tx3[i]);
			if (((i+1) % 16) == 0) {
				pr_err("%s\n", buf);
				idx = 0;
			}
		}
		pr_err("%s\n", buf);
	}
#endif
	if(cmd == DEVICE_I2C_WR) {
		ret = fpga_spi_block_write(fpga_i2c_expander,req->cmds);
	}
	else if(cmd == DEVICE_I2C_1M_RD) {
		ret = fpga_spi_block_one_byte_read(fpga_i2c_expander,req->cmds);
	}
	else{
		ret = fpga_spi_block_read(fpga_i2c_expander,req->cmds);
	}
	req->cmds->ret = ret;  //SPI interface return value..

#if defined(FPGA_I2C_EXPANDER_DBG)
	if (req->cmds->rx_length0) {
		pr_err("rx_length0:%d\n", req->cmds->rx_length0);
		idx = 0;
		for (i = 0; i < req->cmds->rx_length0; i++) {
			idx += sprintf(buf+idx, "%.2X ", req->cmds->data_rx0[i]);
			if (((i+1) % 16) == 0) {
				pr_err("%s\n", buf);
				idx = 0;
			}
		}
		pr_err("%s\n", buf);
	}

	if (req->cmds->rx_length1) {
		pr_err("rx_length1:%d\n", req->cmds->rx_length1);
		idx = 0;
		for (i = 0; i < req->cmds->rx_length1; i++) {
			idx += sprintf(buf+idx, "%.2X ", req->cmds->data_rx1[i]);
			if (((i+1) % 16) == 0) {
				pr_err("%s\n", buf);
				idx = 0;
			}
		}
		pr_err("%s\n", buf);
	}
#endif
	mutex_unlock(&fpga_i2c_expander->spi_cmd_lock);
	return ;
}

