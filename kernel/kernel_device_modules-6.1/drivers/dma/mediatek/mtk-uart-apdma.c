// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek UART APDMA driver.
 *
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Long Cheng <long.cheng@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/serial_8250.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sched/clock.h>

#include "../virt-dma.h"
#include "../../tty/serial/8250/8250.h"

/* The default number of virtual channel */
#define MTK_UART_APDMA_NR_VCHANS	8

#define VFF_EN_B		BIT(0)
#define VFF_STOP_B		BIT(0)
#define VFF_FLUSH_B		BIT(0)
/* rx valid size >=  vff thre */
#define VFF_RX_INT_EN_B		(BIT(0) | BIT(1))
/* tx left size >= vff thre */
#define VFF_TX_INT_EN_B		BIT(0)
#define VFF_WARM_RST_B		BIT(0)
#define VFF_RX_INT_CLR_B	3
#define VFF_TX_INT_CLR_B	0
#define VFF_STOP_CLR_B		0
#define VFF_EN_CLR_B		0
#define VFF_INT_EN_CLR_B	0
#define VFF_4G_SUPPORT_CLR_B	0
#define VFF_ORI_ADDR_BITS_NUM    32
#define VFF_RX_FLOWCTL_THRE_SIZE 0xc00
#define VFF_RX_TRANS_FINISH_MASK 0x7F  /*bit 0~7*/
/*
 * interrupt trigger level for tx
 * if threshold is n, no polling is required to start tx.
 * otherwise need polling VFF_FLUSH.
 */
#define VFF_TX_THRE(n)		(n)
/* interrupt trigger level for rx */
#define VFF_RX_THRE(n)		((n) / 16)

#define VFF_RING_SIZE	0xffff
/* invert this bit when wrap ring head again */
#define VFF_RING_WRAP	0x10000

#define VFF_INT_FLAG		0x00
#define VFF_INT_EN		0x04
#define VFF_EN			0x08
#define VFF_RST			0x0c
#define VFF_STOP		0x10
#define VFF_FLUSH		0x14
#define VFF_ADDR		0x1c
#define VFF_LEN			0x24
#define VFF_THRE		0x28
#define VFF_WPT			0x2c
#define VFF_RPT			0x30
#define VFF_RX_FLOWCTL_THRE	 0x34
#define VFF_INT_BUF_SIZE	0x38
/* TX: the buffer size HW can read. RX: the buffer size SW can read. */
#define VFF_VALID_SIZE		0x3c
/* TX: the buffer size SW can write. RX: the buffer size HW can write. */
#define VFF_LEFT_SIZE		0x40
#define VFF_DEBUG_STATUS	0x50
#define VFF_4G_SUPPORT		0x54

#define UART_RECORD_COUNT	10
#define MAX_POLLING_CNT		5000
#define UART_RECORD_MAXLEN	4096
#define UART_RX_DUMP_MAXLEN	4000
#define UART_TX_DUMP_MAXLEN	1000
#define UART_TX_DUMP_MINLEN	1
#define CONFIG_UART_DMA_DATA_RECORD
#define DBG_STAT_WD_ACT		BIT(5)
#define MAX_POLL_CNT_RX		200
#define MAX_POLL_CNT_TX		200
#define MAX_GLOBAL_VD_COUNT	5
#define VFF_POLL_INTERVAL	100
#define VFF_POLL_TIMEOUT	4000
#define IBUF_POLL_INTERVAL	10
#define IBUF_POLL_TIMEOUT	100

struct uart_info {
	unsigned int wpt_reg;
	unsigned int rpt_reg;
	unsigned int trans_len;
	unsigned long long trans_time;
	unsigned long long trans_duration_time;
	unsigned long long complete_time;
	unsigned char rec_buf[UART_RECORD_MAXLEN];
	unsigned int copy_wpt_reg;
	unsigned int vff_dbg_reg;
	unsigned int irq_cur_cpu;
	pid_t irq_cur_pid;
	char irq_cur_comm[16]; /* task command name from sched.h */
	int poll_cnt_rx;
};

struct mtk_uart_apdmacomp {
	unsigned int addr_bits;
};
struct mtk_uart_apdmadev {
	struct dma_device ddev;
	struct clk *clk;
	unsigned int support_bits;
	unsigned int dma_requests;
	unsigned int support_hub;
	unsigned int support_wakeup;
};

struct mtk_uart_apdma_desc {
	struct virt_dma_desc vd;

	dma_addr_t addr;
	unsigned int avail_len;
	bool is_global_vd;
	bool is_using;
};

struct mtk_chan {
	struct virt_dma_chan vc;
	struct dma_slave_config	cfg;
	struct mtk_uart_apdma_desc *desc;
	struct mtk_uart_apdma_desc c_desc[MAX_GLOBAL_VD_COUNT];
	enum dma_transfer_direction dir;

	void __iomem *base;
	unsigned int irq;
	unsigned int is_hub_port;
	int chan_desc_count;
	spinlock_t dma_lock;
	atomic_t rxdma_state;

	unsigned int irq_wg;
	unsigned int rx_status;
	unsigned int rec_idx;
	unsigned int cur_rpt;
	unsigned long long rec_total;
	unsigned int start_record_wpt;
	unsigned int start_record_rpt;
	unsigned int start_int_flag;
	unsigned int start_int_en;
	unsigned int start_en;
	unsigned int start_int_buf_size;
	unsigned long long start_record_time;
	unsigned int peri_dbg;
	unsigned int start_debug_states;
	unsigned int start_vff_thre;
	unsigned int start_flush;
	unsigned int start_addr;
	unsigned int start_stop;
	unsigned int start_valid_size;
	unsigned int cur_rec_idx;
	struct uart_info rec_info[UART_RECORD_COUNT];
};

static unsigned long long num;
static unsigned int flag_state;
static unsigned int g_vff_sz;
#if IS_ENABLED(CONFIG_MTK_UARTHUB)
atomic_t tx_res_status;
atomic_t rx_res_status;
static unsigned int peri_0_axi_dbg;
static struct clk *g_dma_clk;
struct mutex g_dma_clk_mutex;
atomic_t dma_clk_count;
static unsigned int clk_count;
struct mtk_chan *hub_dma_tx_chan;
struct mtk_chan *hub_dma_rx_chan;
#endif

static inline struct mtk_uart_apdmadev *
to_mtk_uart_apdma_dev(struct dma_device *d)
{
	return container_of(d, struct mtk_uart_apdmadev, ddev);
}

static inline struct mtk_chan *to_mtk_uart_apdma_chan(struct dma_chan *c)
{
	return container_of(c, struct mtk_chan, vc.chan);
}

static inline struct mtk_uart_apdma_desc *to_mtk_uart_apdma_desc
	(struct dma_async_tx_descriptor *t)
{
	return container_of(t, struct mtk_uart_apdma_desc, vd.tx);
}

static void mtk_uart_apdma_write(struct mtk_chan *c,
			       unsigned int reg, unsigned int val)
{
	writel(val, c->base + reg);
	/* Flush register write */
	mb();
}

static unsigned int mtk_uart_apdma_read(struct mtk_chan *c, unsigned int reg)
{
	return readl(c->base + reg);
}

static void mtk_uart_apdma_desc_free(struct virt_dma_desc *vd)
{
	unsigned long flags;
	struct mtk_uart_apdma_desc *d = NULL;
	struct mtk_chan *c = NULL;
	struct virt_dma_chan *vc = to_virt_chan(vd->tx.chan);

	c = container_of(vc, struct mtk_chan, vc);
	d = container_of(vd, struct mtk_uart_apdma_desc, vd);
	spin_lock_irqsave(&c->dma_lock, flags);
	c->chan_desc_count--;
	spin_unlock_irqrestore(&c->dma_lock, flags);
	if (d->is_global_vd)
		d->is_using = false;
	else
		kfree(d);
}

#if IS_ENABLED(CONFIG_MTK_UARTHUB)
void mtk_save_uart_apdma_reg(struct dma_chan *chan, unsigned int *reg_buf)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);

	reg_buf[0] = mtk_uart_apdma_read(c, VFF_INT_FLAG);
	reg_buf[1] = mtk_uart_apdma_read(c, VFF_INT_EN);
	reg_buf[2] = mtk_uart_apdma_read(c, VFF_EN);
	reg_buf[3] = mtk_uart_apdma_read(c, VFF_FLUSH);
	reg_buf[4] = mtk_uart_apdma_read(c, VFF_ADDR);
	reg_buf[5] = mtk_uart_apdma_read(c, VFF_LEN);
	reg_buf[6] = mtk_uart_apdma_read(c, VFF_THRE);
	reg_buf[7] = mtk_uart_apdma_read(c, VFF_WPT);
	reg_buf[8] = mtk_uart_apdma_read(c, VFF_RPT);
	reg_buf[9] = mtk_uart_apdma_read(c, VFF_INT_BUF_SIZE);
	reg_buf[10] = mtk_uart_apdma_read(c, VFF_VALID_SIZE);
	reg_buf[11] = mtk_uart_apdma_read(c, VFF_LEFT_SIZE);
	reg_buf[12] = mtk_uart_apdma_read(c, VFF_DEBUG_STATUS);
}
EXPORT_SYMBOL(mtk_save_uart_apdma_reg);

static unsigned int mtk_uart_apdma_get_peri_axi_status(void)
{
	void __iomem *peri_remap_0_axi_dbg = NULL;
	unsigned int ret = 0;

	if (peri_0_axi_dbg == 0)
		return 0;
	peri_remap_0_axi_dbg = ioremap(peri_0_axi_dbg, 0x10);
	if (!peri_remap_0_axi_dbg) {
		pr_info("[%s] peri_remap_0_axi_dbg(%x) ioremap fail\n",
			__func__, peri_0_axi_dbg);
		return 0;
	}
	ret = readl(peri_remap_0_axi_dbg);

	if (peri_remap_0_axi_dbg)
		iounmap(peri_remap_0_axi_dbg);

	return ret;
}

void mtk_uart_apdma_start_record(struct dma_chan *chan)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);

	c->start_record_wpt =  mtk_uart_apdma_read(c, VFF_WPT);
	c->start_record_rpt = mtk_uart_apdma_read(c, VFF_RPT);
	c->start_int_flag =  mtk_uart_apdma_read(c, VFF_INT_FLAG);
	c->start_int_en =  mtk_uart_apdma_read(c, VFF_INT_EN);
	c->start_en =  mtk_uart_apdma_read(c, VFF_EN);
	c->start_int_buf_size =  mtk_uart_apdma_read(c, VFF_INT_BUF_SIZE);
	c->start_record_time = sched_clock();
	c->peri_dbg = mtk_uart_apdma_get_peri_axi_status();
	c->start_debug_states = mtk_uart_apdma_read(c, VFF_DEBUG_STATUS);
	c->start_vff_thre = mtk_uart_apdma_read(c, VFF_THRE);
	c->start_flush = mtk_uart_apdma_read(c, VFF_FLUSH);
	c->start_addr = mtk_uart_apdma_read(c, VFF_ADDR);
	c->start_stop = mtk_uart_apdma_read(c, VFF_STOP);
	c->start_valid_size = mtk_uart_apdma_read(c, VFF_VALID_SIZE);
}
EXPORT_SYMBOL(mtk_uart_apdma_start_record);

void mtk_uart_apdma_end_record(struct dma_chan *chan)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	unsigned int _wpt =  mtk_uart_apdma_read(c, VFF_WPT);
	unsigned int _rpt = mtk_uart_apdma_read(c, VFF_RPT);
	unsigned int _int_flag = mtk_uart_apdma_read(c, VFF_INT_FLAG);
	unsigned int _int_en = mtk_uart_apdma_read(c, VFF_INT_EN);
	unsigned int _en = mtk_uart_apdma_read(c, VFF_EN);
	unsigned int _int_buf_size = mtk_uart_apdma_read(c, VFF_INT_BUF_SIZE);
	unsigned int _debug_states = mtk_uart_apdma_read(c, VFF_DEBUG_STATUS);
	unsigned int _vff_thre = mtk_uart_apdma_read(c, VFF_THRE);
	unsigned int _flush = mtk_uart_apdma_read(c, VFF_FLUSH);
	unsigned int _addr = mtk_uart_apdma_read(c, VFF_ADDR);
	unsigned int _stop = mtk_uart_apdma_read(c, VFF_STOP);
	unsigned int _valid_size= mtk_uart_apdma_read(c, VFF_VALID_SIZE);
	unsigned long long starttime = c->start_record_time;
	unsigned long long endtime = sched_clock();
	unsigned long ns1 = do_div(starttime, 1000000000);
	unsigned long ns2 = do_div(endtime, 1000000000);
	unsigned int peri_dbg = mtk_uart_apdma_get_peri_axi_status();

	pr_info("[%s] [%s] [start %5lu.%06lu] wpt=0x%x, rpt=0x%x, int_flag=0x%x, int_en=0x%x, "
		"en=0x%x, int_buf_size=0x%x, 0x%x = 0x%x\n"
		"[%s] [%s] debug_states=0x%x, vff_thre=0x%x, flush=0x%x, addr=0x%x, stop=0x%x, "
		"valid_size=0x%x\n",
		__func__, c->dir == DMA_DEV_TO_MEM ? "dma_rx" : "dma_tx",
		(unsigned long)starttime, ns1 / 1000, c->start_record_wpt, c->start_record_rpt,
		c->start_int_flag, c->start_int_en, c->start_en, c->start_int_buf_size,
		peri_0_axi_dbg, c->peri_dbg,
		__func__, c->dir == DMA_DEV_TO_MEM ? "dma_rx" : "dma_tx",
		c->start_debug_states,c->start_vff_thre,c->start_flush,
		c->start_addr,c->start_stop,c->start_valid_size);

	pr_info("[%s] [%s] [end   %5lu.%06lu] wpt=0x%x, rpt=0x%x, int_flag=0x%x, int_en=0x%x, "
		"en=0x%x, int_buf_size=0x%x, 0x%x = 0x%x\n"
		"[%s] [%s] debug_states=0x%x, vff_thre=0x%x, flush=0x%x, addr=0x%x, stop=0x%x, "
		"valid_size=0x%x\n",
		__func__, c->dir == DMA_DEV_TO_MEM ? "dma_rx" : "dma_tx",
		(unsigned long)endtime, ns2 / 1000, _wpt, _rpt, _int_flag,
		_int_en, _en, _int_buf_size, peri_0_axi_dbg, peri_dbg,
		__func__, c->dir == DMA_DEV_TO_MEM ? "dma_rx" : "dma_tx",
		_debug_states, _vff_thre, _flush, _addr, _stop, _valid_size);
}
EXPORT_SYMBOL(mtk_uart_apdma_end_record);

void mtk_uart_apdma_data_dump(struct dma_chan *chan)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	unsigned int count = 0;
	unsigned int idx = 0;

	idx = c->rec_total < UART_RECORD_COUNT ? 0 : (c->rec_idx + 1) % UART_RECORD_COUNT;

	while (count < min_t(unsigned int, UART_RECORD_COUNT, c->rec_total)) {
		unsigned long long endtime = c->rec_info[idx].trans_time;
		unsigned long long durationtime = c->rec_info[idx].trans_duration_time;
		unsigned long ns = 0;
		unsigned long long elapseNs = do_div(durationtime, 1000000000);
		unsigned long long complete_time = c->rec_info[idx].complete_time;
		unsigned long complete_ns = do_div(complete_time, 1000000000);
#ifdef CONFIG_UART_DMA_DATA_RECORD
		unsigned int cnt = 0;
		unsigned int cyc = 0;
		unsigned int len = c->rec_info[idx].trans_len;
		unsigned char raw_buf[256*3 + 4];
		const unsigned char *ptr = c->rec_info[idx].rec_buf;
#endif

		ns = do_div(endtime, 1000000000);

		pr_info("[%s] [%s] [begin_t: %5lu.%06lu] [%s: %5lu.%06llu] [exit_handler_t: %5lu.%06lu] "
			"irq_handler cpu:%d, pid:%d, comm:%s\n",
			__func__, c->dir == DMA_DEV_TO_MEM ? "dma_rx" : "dma_tx",
			(unsigned long)endtime, ns / 1000,
			c->dir == DMA_DEV_TO_MEM ? "enter_handler_t" : "elapsed_t",
			(unsigned long)durationtime, elapseNs/1000,
			(unsigned long)complete_time, complete_ns / 1000,
			c->rec_info[idx].irq_cur_cpu, c->rec_info[idx].irq_cur_pid,
			c->rec_info[idx].irq_cur_comm);

		pr_info("[%s] [%s] idx=%d, total=%llu, wpt=0x%x, rpt=0x%x, len=%d, poll_cnt_rx=%d, "
			"vff_dbg=0x%x, copy_wpt=0x%x\n",
			__func__, c->dir == DMA_DEV_TO_MEM ? "dma_rx" : "dma_tx",
			idx, c->rec_total,
			c->rec_info[idx].wpt_reg, c->rec_info[idx].rpt_reg,
			c->rec_info[idx].trans_len, c->rec_info[idx].poll_cnt_rx,
			c->rec_info[idx].vff_dbg_reg, c->rec_info[idx].copy_wpt_reg);
#ifdef CONFIG_UART_DMA_DATA_RECORD
		if (len <= UART_RECORD_MAXLEN) {
			if (len > 256)
				len = 256;
			for (cyc = 0; cyc < len; cyc += 256) {
				unsigned int cnt_min = len - cyc < 256 ? len - cyc : 256;

				for (cnt = 0; cnt < cnt_min; cnt++)
					(void)snprintf(raw_buf + 3 * cnt, 4, "%02X ",
						ptr[cnt + cyc]);
				raw_buf[3*cnt] = '\0';
				if (c->dir == DMA_MEM_TO_DEV)/*log too much, remove RX data dump*/
					pr_info("[%s] [%s] Tx[%d] data = %s\n", __func__,
						c->dir == DMA_DEV_TO_MEM ? "dma_rx" : "dma_tx",
							cyc, raw_buf);
			}
		}
#endif
		idx++;
		idx = idx % UART_RECORD_COUNT;
		count++;
	}
}
EXPORT_SYMBOL(mtk_uart_apdma_data_dump);

int mtk_uart_get_apdma_rx_state(void)
{
	return atomic_read(&hub_dma_rx_chan->rxdma_state);
}
EXPORT_SYMBOL(mtk_uart_get_apdma_rx_state);

bool mtk_uart_get_apdma_handler_state(void)
{
	bool state1 = true;
	bool state2 = true;

	irq_get_irqchip_state(hub_dma_rx_chan->irq, IRQCHIP_STATE_PENDING, &state1);
	irq_get_irqchip_state(hub_dma_rx_chan->irq, IRQCHIP_STATE_ACTIVE, &state2);
	return (state1 || state2);
}
EXPORT_SYMBOL(mtk_uart_get_apdma_handler_state);

void mtk_uart_set_apdma_rx_state(bool enable)
{
	if (enable )
		atomic_inc(&hub_dma_rx_chan->rxdma_state);
	else
		atomic_dec(&hub_dma_rx_chan->rxdma_state);
}
EXPORT_SYMBOL(mtk_uart_set_apdma_rx_state);

void mtk_uart_set_apdma_rx_irq(bool enable)
{
	if (enable)
		mtk_uart_apdma_write(hub_dma_rx_chan, VFF_INT_EN, VFF_RX_INT_EN_B);
	else
		mtk_uart_apdma_write(hub_dma_rx_chan, VFF_INT_EN, VFF_INT_EN_CLR_B);
}
EXPORT_SYMBOL(mtk_uart_set_apdma_rx_irq);

void mtk_uart_set_apdma_clk(bool enable)
{
	int ret = 0;

	mutex_lock(&g_dma_clk_mutex);
	if (enable) {
		if (atomic_read(&dma_clk_count) < 0)
			pr_info("[%s] dma_clk_count < 0\n", __func__);
		atomic_inc(&dma_clk_count);
		if (atomic_read(&dma_clk_count) == 1) {
			ret = clk_prepare_enable(g_dma_clk);
			if (ret)
				pr_info("[%s] clk_prepare_enable fail\n", __func__);
		}
	} else {
		atomic_dec(&dma_clk_count);
		if (atomic_read(&dma_clk_count) < 0)
			pr_info("[%s] dma_clk_count < 0\n", __func__);
		if (atomic_read(&dma_clk_count) == 0) {
			mtk_uart_apdma_write(hub_dma_rx_chan, VFF_INT_EN, VFF_INT_EN_CLR_B);
			mtk_uart_apdma_write(hub_dma_rx_chan, VFF_INT_FLAG, VFF_RX_INT_CLR_B);
			clk_disable_unprepare(g_dma_clk);
		}
	}
	mutex_unlock(&g_dma_clk_mutex);
}
EXPORT_SYMBOL(mtk_uart_set_apdma_clk);

void mtk_uart_apdma_enable_vff(bool enable)
{
	int ret = 0;
	unsigned int vff_state = 0;
	unsigned int status;

	if (enable == false) {
		/* TX & RX vff stop */
		mtk_uart_apdma_write(hub_dma_tx_chan, VFF_STOP, VFF_STOP_B);
		ret = readx_poll_timeout(readl, hub_dma_tx_chan->base + VFF_EN,
			 status, !status, 10, 100);
		if (ret)
			pr_info("[%s]stop: fail, status=0x%x\n", __func__,
				mtk_uart_apdma_read(hub_dma_tx_chan, VFF_DEBUG_STATUS));
		mtk_uart_apdma_write(hub_dma_tx_chan, VFF_STOP, VFF_STOP_CLR_B);

		mtk_uart_apdma_write(hub_dma_rx_chan, VFF_STOP, VFF_STOP_B);
		ret = readx_poll_timeout(readl, hub_dma_rx_chan->base + VFF_EN,
			status, !status, 10, 100);
		if (ret)
			pr_info("[%s]stop: fail, status=0x%x\n", __func__,
				mtk_uart_apdma_read(hub_dma_rx_chan, VFF_DEBUG_STATUS));
		mtk_uart_apdma_write(hub_dma_rx_chan, VFF_STOP, VFF_STOP_CLR_B);
	} else {
		/* enable RX VFF */
		vff_state = mtk_uart_apdma_read(hub_dma_rx_chan, VFF_EN);
		if (vff_state) {
			pr_info("[%s] :tx_chan addr1[0x%x], tx_chan addr2[0x%x], "
				"rx_chan addr1[0x%x],rx_chan addr2[0x%x]\n",
				__func__, mtk_uart_apdma_read(hub_dma_tx_chan, VFF_ADDR),
				mtk_uart_apdma_read(hub_dma_tx_chan, VFF_4G_SUPPORT),
				mtk_uart_apdma_read(hub_dma_rx_chan, VFF_ADDR),
				mtk_uart_apdma_read(hub_dma_rx_chan, VFF_4G_SUPPORT));
		}
		mtk_uart_apdma_write(hub_dma_rx_chan, VFF_RPT, 0x0);
		mtk_uart_apdma_write(hub_dma_rx_chan, VFF_THRE, VFF_RX_THRE(g_vff_sz));
		mtk_uart_apdma_write(hub_dma_rx_chan, VFF_EN, VFF_EN_B);
	}
}
EXPORT_SYMBOL(mtk_uart_apdma_enable_vff);

void mtk_uart_set_tx_res_status(int status)
{
	atomic_set(&tx_res_status, status);
}
EXPORT_SYMBOL(mtk_uart_set_tx_res_status);

int mtk_uart_get_tx_res_status(void)
{
	return atomic_read(&tx_res_status);
}
EXPORT_SYMBOL(mtk_uart_get_tx_res_status);

void mtk_uart_set_rx_res_status(int status)
{
	atomic_set(&rx_res_status, status);
}
EXPORT_SYMBOL(mtk_uart_set_rx_res_status);

int mtk_uart_get_rx_res_status(void)
{
	return atomic_read(&rx_res_status);
}
EXPORT_SYMBOL(mtk_uart_get_rx_res_status);

void mtk_uart_apdma_polling_rx_finish(void)
{
	unsigned int rx_status = 0, poll_cnt = MAX_POLL_CNT_RX;

	rx_status = mtk_uart_apdma_read(hub_dma_rx_chan, VFF_DEBUG_STATUS);
	while ((rx_status & VFF_RX_TRANS_FINISH_MASK) && poll_cnt) {
		udelay(1);
		rx_status = mtk_uart_apdma_read(hub_dma_rx_chan, VFF_DEBUG_STATUS);
		poll_cnt--;
	}

	rx_status = mtk_uart_apdma_read(hub_dma_rx_chan, VFF_DEBUG_STATUS);
	if (!poll_cnt && (rx_status & VFF_RX_TRANS_FINISH_MASK))
		pr_info("[WARN]poll cnt is exhausted, DEBUG_STATUS:0x%x\n", rx_status);
}
EXPORT_SYMBOL(mtk_uart_apdma_polling_rx_finish);

int mtk_uart_apdma_polling_tx_finish(void)
{
	int ret = 0, result = 0;
	unsigned int tx_data_cnt = 0, vff_valid_size = 0;
	unsigned int poll_cnt = MAX_POLL_CNT_TX;
	bool is_irq_pending = true;
	bool is_irq_active = true;

	// Check apdma VFF valid size
	vff_valid_size = mtk_uart_apdma_read(hub_dma_tx_chan, VFF_VALID_SIZE);
	if (vff_valid_size != 0) {
		ret = readx_poll_timeout(readl, hub_dma_tx_chan->base + VFF_VALID_SIZE,
			vff_valid_size, vff_valid_size == 0, VFF_POLL_INTERVAL, VFF_POLL_TIMEOUT);
		pr_info("%s: polling vff done: valid_size: %d, ret: %d\n", __func__, vff_valid_size, ret);
	}

	// Check apdma internal buf size
	tx_data_cnt = mtk_uart_apdma_read(hub_dma_tx_chan, VFF_INT_BUF_SIZE);
	if (tx_data_cnt != 0) {
		ret = readx_poll_timeout(readl, hub_dma_tx_chan->base + VFF_INT_BUF_SIZE,
			tx_data_cnt, tx_data_cnt == 0, IBUF_POLL_INTERVAL, IBUF_POLL_TIMEOUT);
		pr_info("%s: polling int buf done: data_cnt: %d, ret: %d\n", __func__, tx_data_cnt, ret);
	}

	irq_get_irqchip_state(hub_dma_tx_chan->irq, IRQCHIP_STATE_PENDING, &is_irq_pending);
	while (is_irq_pending && (poll_cnt > 0)) {
		udelay(2);
		irq_get_irqchip_state(hub_dma_tx_chan->irq, IRQCHIP_STATE_PENDING, &is_irq_pending);
		poll_cnt--;
	}

	poll_cnt = MAX_POLL_CNT_TX;
	irq_get_irqchip_state(hub_dma_tx_chan->irq, IRQCHIP_STATE_ACTIVE, &is_irq_active);
	while (is_irq_active && (poll_cnt > 0)) {
		udelay(2);
		irq_get_irqchip_state(hub_dma_tx_chan->irq, IRQCHIP_STATE_ACTIVE, &is_irq_active);
		poll_cnt--;
	}

	if (is_irq_pending || is_irq_active) {
		pr_info("%s: irq_pending: %d, irq_active: %d\n", __func__, is_irq_pending, is_irq_active);
		result = -1;
	}

	return result;
}
EXPORT_SYMBOL(mtk_uart_apdma_polling_tx_finish);
#endif

void mtk_uart_rx_setting(struct dma_chan *chan, int copied, int total)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	unsigned int rpt_old, rpt_new, vff_sz;

	if (total > copied) {
		vff_sz = c->cfg.src_port_window_size;
		rpt_old = mtk_uart_apdma_read(c, VFF_RPT);
		rpt_new = rpt_old + (unsigned int)copied;
		if ((rpt_new & vff_sz) == vff_sz)
			rpt_new = (rpt_new - vff_sz) ^ VFF_RING_WRAP;
		c->irq_wg = rpt_new;
	}
	/* Flush before update vff_rpt */
	mb();
	/* Let DMA start moving data */
	mtk_uart_apdma_write(c, VFF_RPT, c->irq_wg);

}
EXPORT_SYMBOL(mtk_uart_rx_setting);

void mtk_uart_get_apdma_rpt(struct dma_chan *chan, unsigned int *rpt)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	unsigned int idx = 0;

	*rpt = c->cur_rpt & VFF_RING_SIZE;
	idx = (unsigned int)((c->rec_idx - 1 + UART_RECORD_COUNT) % UART_RECORD_COUNT);
	c->rec_info[idx].copy_wpt_reg = mtk_uart_apdma_read(c, VFF_WPT);
}
EXPORT_SYMBOL(mtk_uart_get_apdma_rpt);

static dma_addr_t is_tx_addr_valid(struct mtk_uart_apdma_desc *d)
{
	struct uart_8250_port *p = NULL;
	struct uart_8250_dma *dma = NULL;
	dma_addr_t tx_addr;

	if(d == NULL)
		return 0;
	p = (struct uart_8250_port *)d->vd.tx.callback_param;
	if(p == NULL)
		return 0;
	dma = p->dma;
	if(dma == NULL)
		return 0;
	tx_addr = dma->tx_addr;

	return tx_addr;
}

static void mtk_uart_apdma_start_tx(struct mtk_chan *c)
{
	struct mtk_uart_apdmadev *mtkd =
				to_mtk_uart_apdma_dev(c->vc.chan.device);
	struct mtk_uart_apdma_desc *d = c->desc;
	unsigned int wpt, vff_sz, left_data, rst_status;
	unsigned int idx = 0;
	int poll_cnt = MAX_POLLING_CNT;

	if (c->is_hub_port) {
		if (!mtk_uart_get_tx_res_status()) {
			pr_info("[WARN]%s, tx_request is not set\n", __func__);
			return;
		}
	}

	vff_sz = c->cfg.dst_port_window_size;

	left_data = mtk_uart_apdma_read(c, VFF_INT_BUF_SIZE);
	while ((left_data != 0) && (poll_cnt > 0)) {
		udelay(2);
		left_data = mtk_uart_apdma_read(c, VFF_INT_BUF_SIZE);
		poll_cnt--;
	}

	if ((poll_cnt == 0) || (c->chan_desc_count <= 0) || (is_tx_addr_valid(d) == 0)) {
		pr_info("[WARN] %s, c->chan_desc_count[%d], poll_cnt[%d],tx_addr[%llu]\n",
			__func__, c->chan_desc_count, poll_cnt,is_tx_addr_valid(d));
		return;
	}
	wpt = mtk_uart_apdma_read(c, VFF_ADDR);
	if (wpt == ((unsigned int)d->addr)) {
		mtk_uart_apdma_write(c, VFF_ADDR, 0);
		mtk_uart_apdma_write(c, VFF_THRE, 0);
		mtk_uart_apdma_write(c, VFF_LEN, 0);
		mtk_uart_apdma_write(c, VFF_RST, VFF_WARM_RST_B);
		/* Make sure cmd sequence */
		mb();
		udelay(1);
		rst_status = mtk_uart_apdma_read(c, VFF_RST);
		if (rst_status != 0) {
			udelay(5);
			pr_info("%s: apdma: rst_status: 0x%x, new rst_status: 0x%x!\n",
				__func__, rst_status, mtk_uart_apdma_read(c, VFF_RST));
		}
	}

	if (!mtk_uart_apdma_read(c, VFF_LEN)) {
		mtk_uart_apdma_write(c, VFF_ADDR, d->addr);
		mtk_uart_apdma_write(c, VFF_LEN, vff_sz);
		mtk_uart_apdma_write(c, VFF_THRE, VFF_TX_THRE(vff_sz));
		mtk_uart_apdma_write(c, VFF_WPT, 0);
		mtk_uart_apdma_write(c, VFF_INT_FLAG, VFF_TX_INT_CLR_B);

		if (mtkd->support_bits > VFF_ORI_ADDR_BITS_NUM)
			mtk_uart_apdma_write(c, VFF_4G_SUPPORT,
					upper_32_bits(d->addr));
	}

	mtk_uart_apdma_write(c, VFF_EN, VFF_EN_B);
	if (mtk_uart_apdma_read(c, VFF_EN) != VFF_EN_B)
		dev_err(c->vc.chan.device->dev, "Enable TX fail\n");

	if (!mtk_uart_apdma_read(c, VFF_LEFT_SIZE)) {
		mtk_uart_apdma_write(c, VFF_INT_EN, VFF_TX_INT_EN_B);
		return;
	}

	wpt = mtk_uart_apdma_read(c, VFF_WPT);

	idx = (unsigned int)(c->rec_idx % UART_RECORD_COUNT);
	c->rec_idx++;
	c->rec_idx = (unsigned int)(c->rec_idx % UART_RECORD_COUNT);
	c->rec_total++;

	c->rec_info[idx].wpt_reg = wpt;
	c->rec_info[idx].rpt_reg = mtk_uart_apdma_read(c, VFF_RPT);
	c->rec_info[idx].trans_len = c->desc->avail_len;
	c->rec_info[idx].trans_time = sched_clock();

	c->rec_info[idx].irq_cur_pid = current->pid;
	memcpy(c->rec_info[idx].irq_cur_comm, current->comm,
		sizeof(c->rec_info[idx].irq_cur_comm));
	c->rec_info[idx].irq_cur_comm[15] = 0;
	c->rec_info[idx].irq_cur_cpu = raw_smp_processor_id();

#ifdef CONFIG_UART_DMA_DATA_RECORD
	if (d->vd.tx.callback_param != NULL) {
		struct uart_8250_port *p = (struct uart_8250_port *)d->vd.tx.callback_param;
		struct uart_state *u_state = p->port.state;
		struct circ_buf *xmit = &u_state->xmit;
		const char *ptr = xmit->buf + xmit->tail;
		int tx_size = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);
		int dump_len = min(tx_size, UART_RECORD_MAXLEN);

		if (u_state != NULL) {
			if (c->rec_info[idx].trans_len <= UART_RECORD_MAXLEN)
				memcpy(c->rec_info[idx].rec_buf, ptr,
					min((unsigned int)dump_len, c->rec_info[idx].trans_len));
		} else {
			dev_info(c->vc.chan.device->dev, "[%s] u_state==NULL\n", __func__);
		}
	}
#endif

	wpt += c->desc->avail_len;
	if ((wpt & VFF_RING_SIZE) == vff_sz)
		wpt = (wpt & VFF_RING_WRAP) ^ VFF_RING_WRAP;

	/* Flush before update vff_wpt */
	mb();
	/* Let DMA start moving data */
	mtk_uart_apdma_write(c, VFF_WPT, wpt);

	/* HW auto set to 0 when left size >= threshold */
	mtk_uart_apdma_write(c, VFF_INT_EN, VFF_TX_INT_EN_B);
	if (!mtk_uart_apdma_read(c, VFF_FLUSH))
		mtk_uart_apdma_write(c, VFF_FLUSH, VFF_FLUSH_B);
}

static void mtk_uart_apdma_start_rx(struct mtk_chan *c)
{
	struct mtk_uart_apdmadev *mtkd =
				to_mtk_uart_apdma_dev(c->vc.chan.device);
	struct mtk_uart_apdma_desc *d = c->desc;
	unsigned int vff_sz;
	int idx;

	if (d == NULL) {
		dev_info(c->vc.chan.device->dev, "%s:[%d] FIX ME1!", __func__, c->irq);
		return;
	}
	idx = (unsigned int)(c->rec_idx % UART_RECORD_COUNT);
	c->rec_info[idx].trans_time = sched_clock();

	vff_sz = c->cfg.src_port_window_size;
	g_vff_sz = vff_sz;
	if (!mtk_uart_apdma_read(c, VFF_LEN)) {
		mtk_uart_apdma_write(c, VFF_ADDR, d->addr);
		mtk_uart_apdma_write(c, VFF_LEN, vff_sz);
		mtk_uart_apdma_write(c, VFF_THRE, VFF_RX_THRE(vff_sz));
		mtk_uart_apdma_write(c, VFF_RPT, 0);
		mtk_uart_apdma_write(c, VFF_INT_FLAG, VFF_RX_INT_CLR_B);

		if (mtkd->support_bits > VFF_ORI_ADDR_BITS_NUM)
			mtk_uart_apdma_write(c, VFF_4G_SUPPORT,
					upper_32_bits(d->addr));
	}

	mtk_uart_apdma_write(c, VFF_RX_FLOWCTL_THRE, VFF_RX_FLOWCTL_THRE_SIZE);

#if IS_ENABLED(CONFIG_MTK_UARTHUB)
	if ((mtkd->support_hub) && (mtkd->support_wakeup) && (c->is_hub_port)) {
		if (mtk_uart_get_rx_res_status())
			mtk_uart_apdma_write(c, VFF_INT_EN, VFF_RX_INT_EN_B);
	} else {
		mtk_uart_apdma_write(c, VFF_INT_EN, VFF_RX_INT_EN_B);
	}
#else
	mtk_uart_apdma_write(c, VFF_INT_EN, VFF_RX_INT_EN_B);
#endif
	mtk_uart_apdma_write(c, VFF_EN, VFF_EN_B);
	if (mtk_uart_apdma_read(c, VFF_EN) != VFF_EN_B)
		dev_err(c->vc.chan.device->dev, "Enable RX fail\n");
}

/* vchan_cookie_complete use irq_thread */
static inline void vchan_cookie_complete_thread_irq(struct virt_dma_desc *vd)
{
	struct virt_dma_chan *vc = to_virt_chan(vd->tx.chan);
	dma_cookie_t cookie;

	cookie = vd->tx.cookie;
	dma_cookie_complete(&vd->tx);
	dev_vdbg(vc->chan.device->dev, "txd %p[%x]: marked complete\n",
		 vd, cookie);
	list_add_tail(&vd->node, &vc->desc_completed);
}

/* vchan_complete use irq_thread */
static irqreturn_t vchan_complete_thread_irq(int irq, void *dev_id)
{
	struct dma_chan *chan = (struct dma_chan *)dev_id;
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	struct mtk_uart_apdma_desc *d = c->desc;
	struct virt_dma_chan *vc = to_virt_chan(d->vd.tx.chan);
	struct virt_dma_desc *vd, *_vd;
	struct dmaengine_desc_callback cb;
	unsigned int idx = 0;
	unsigned int wpt = 0;
	unsigned int rpt = 0;
	unsigned long long recv_sec = 0;
	unsigned long long recv_ns = 0;
	unsigned long long start_sec = 0;
	unsigned long long end_sec = 0;
	unsigned long long start_ns = 0;
	unsigned long long end_ns = 0;
	unsigned long long cost_time = 0;

	LIST_HEAD(head);

	if (c->dir == DMA_MEM_TO_DEV) {
		idx = c->cur_rec_idx;
		rpt = c->cur_rpt;
	} else {
		idx = (unsigned int)((c->rec_idx - 1 + UART_RECORD_COUNT) % UART_RECORD_COUNT);
		start_sec = sched_clock();
		wpt =  mtk_uart_apdma_read(c, VFF_WPT);
	}

	spin_lock_irq(&vc->lock);
	list_splice_tail_init(&vc->desc_completed, &head);
	vd = vc->cyclic;
	if (vd) {
		vc->cyclic = NULL;
		dmaengine_desc_get_callback(&vd->tx, &cb);
	} else {
		memset(&cb, 0, sizeof(cb));
	}
	spin_unlock_irq(&vc->lock);

	dmaengine_desc_callback_invoke(&cb, &vd->tx_result);

	list_for_each_entry_safe(vd, _vd, &head, node) {
		dmaengine_desc_get_callback(&vd->tx, &cb);

		list_del(&vd->node);
		dmaengine_desc_callback_invoke(&cb, &vd->tx_result);
		vchan_vdesc_fini(vd);
	}
	c->rec_info[idx].complete_time = sched_clock();

	if ((c->dir == DMA_DEV_TO_MEM) && (c->rec_info[idx].trans_len >= UART_RX_DUMP_MAXLEN)) {
		recv_sec = c->rec_info[idx].trans_duration_time;
		recv_ns = do_div(recv_sec, 1000000000);
		start_ns = do_div(start_sec, 1000000000);
		end_sec = c->rec_info[idx].complete_time;
		end_ns = do_div(end_sec, 1000000000);
		pr_info("rx h_t:[%5lu.%06llu], cb_s:[%5lu.%06llu], cb_e:[%5lu.%06llu], wpt:0x%x, len:%d\n",
			(unsigned long)recv_sec, recv_ns / 1000, (unsigned long)start_sec, start_ns / 1000,
			(unsigned long)end_sec, end_ns / 1000, wpt, c->rec_info[idx].trans_len);
	} else if ((c->dir == DMA_MEM_TO_DEV)
		&& ((c->rec_info[idx].trans_len == UART_TX_DUMP_MINLEN)
			|| (c->rec_info[idx].trans_len >= UART_TX_DUMP_MAXLEN))) {
		start_sec = c->rec_info[idx].trans_time;
		end_sec = c->rec_info[idx].complete_time;
		cost_time = end_sec - start_sec;
		start_ns = do_div(start_sec, 1000000000);
		recv_sec = c->rec_info[idx].trans_duration_time;
		recv_ns = do_div(recv_sec, 1000000000);
		end_ns = do_div(end_sec, 1000000000);
		if (cost_time > 10000000)
			pr_info("tx s_t:[%5lu.%06llu], h_t:[%5lu.%06llu], cb_e:[%5lu.%06llu],"
				"rpt:0x%x, len:%d, cost_time: %llu\n",
				(unsigned long)start_sec, start_ns / 1000,
				(unsigned long)recv_sec, recv_ns / 1000,
				(unsigned long)end_sec, end_ns / 1000,
				rpt, c->rec_info[idx].trans_len, cost_time);
	} else
		return IRQ_HANDLED;

	return IRQ_HANDLED;
}

static int mtk_uart_apdma_tx_handler(struct mtk_chan *c)
{
	struct mtk_uart_apdma_desc *d = c->desc;
	unsigned int idx = (c->rec_idx - 1 + UART_RECORD_COUNT) % UART_RECORD_COUNT;

	c->cur_rec_idx = idx;

	mtk_uart_apdma_write(c, VFF_INT_FLAG, VFF_TX_INT_CLR_B);
	if (unlikely(d == NULL))
		return -EINVAL;
	mtk_uart_apdma_write(c, VFF_INT_EN, VFF_INT_EN_CLR_B);
	mtk_uart_apdma_write(c, VFF_EN, VFF_EN_CLR_B);
	c->cur_rpt =  mtk_uart_apdma_read(c, VFF_RPT);

	list_del(&d->vd.node);
	vchan_cookie_complete_thread_irq(&d->vd);
	c->rec_info[idx].trans_duration_time = sched_clock() - c->rec_info[idx].trans_time;
	return 0;
}

static int mtk_uart_apdma_rx_handler(struct mtk_chan *c)
{
	struct mtk_uart_apdma_desc *d = c->desc;
	struct mtk_uart_apdmadev *mtkd =
		to_mtk_uart_apdma_dev(c->vc.chan.device);
	unsigned int len, wg, rg, left_data;
	int cnt;
	unsigned int idx = 0;
	int poll_cnt = MAX_POLL_CNT_RX;

	if (mtkd->support_wakeup && c->is_hub_port) {
		if (atomic_read(&dma_clk_count) == 0) {
			pr_info("[%s]: dma_clk_count == 0\n", __func__);
			return -EINVAL;
		}
		mtk_uart_set_apdma_rx_state(true);
	}
	left_data = mtk_uart_apdma_read(c, VFF_DEBUG_STATUS);
	while (((left_data & DBG_STAT_WD_ACT) == DBG_STAT_WD_ACT) && (poll_cnt > 0)) {
		udelay(1);
		left_data = mtk_uart_apdma_read(c, VFF_DEBUG_STATUS);
		poll_cnt--;
	}
	flag_state = mtk_uart_apdma_read(c, VFF_INT_FLAG);
	mtk_uart_apdma_write(c, VFF_INT_FLAG, VFF_RX_INT_CLR_B);
	//Read VFF_VALID_FLAG value
	mb();

	if (!mtk_uart_apdma_read(c, VFF_VALID_SIZE)) {
		num++;
		if (mtkd->support_wakeup  && c->is_hub_port)
			mtk_uart_set_apdma_rx_state(false);
		return -EINVAL;
	}
	num = 0;
	mtk_uart_apdma_write(c, VFF_EN, VFF_EN_CLR_B);
	mtk_uart_apdma_write(c, VFF_INT_EN, VFF_INT_EN_CLR_B);

	len = c->cfg.src_port_window_size;
	rg = mtk_uart_apdma_read(c, VFF_RPT);
	wg = mtk_uart_apdma_read(c, VFF_WPT);
	cnt = (wg & VFF_RING_SIZE) - (rg & VFF_RING_SIZE);

	/*
	 * The buffer is ring buffer. If wrap bit different,
	 * represents the start of the next cycle for WPT
	 */
	if ((rg ^ wg) & VFF_RING_WRAP)
		cnt += len;

	c->rx_status = d->avail_len - cnt;
	c->irq_wg = wg;
	c->cur_rpt = rg;

	idx = (unsigned int)(c->rec_idx % UART_RECORD_COUNT);
	c->rec_idx++;
	c->rec_idx = (unsigned int)(c->rec_idx % UART_RECORD_COUNT);
	c->rec_total++;

	c->rec_info[idx].poll_cnt_rx = poll_cnt;
	c->rec_info[idx].vff_dbg_reg = left_data;
	c->rec_info[idx].wpt_reg = wg;
	c->rec_info[idx].rpt_reg = rg;
	c->rec_info[idx].trans_len = cnt;
	c->rec_info[idx].trans_duration_time = sched_clock();
	c->rec_info[idx].irq_cur_pid = current->pid;
	memcpy(c->rec_info[idx].irq_cur_comm, current->comm,
		sizeof(c->rec_info[idx].irq_cur_comm));
	c->rec_info[idx].irq_cur_comm[15] = 0;
	c->rec_info[idx].irq_cur_cpu = raw_smp_processor_id();

	list_del(&d->vd.node);
	vchan_cookie_complete_thread_irq(&d->vd);
	return 0;
}

static irqreturn_t mtk_uart_apdma_irq_handler(int irq, void *dev_id)
{
	struct dma_chan *chan = (struct dma_chan *)dev_id;
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	enum dma_transfer_direction current_dir;
	unsigned int current_irq;
	bool dump_tx_err = false;
	int ret = -EINVAL;
	//unsigned long flags;

	//spin_lock_irqsave(&c->vc.lock, flags);
	spin_lock(&c->vc.lock);
	current_dir = c->dir;
	current_irq = c->irq;
	if (c->dir == DMA_DEV_TO_MEM)
		ret = mtk_uart_apdma_rx_handler(c);
	else if (c->dir == DMA_MEM_TO_DEV) {
		if (unlikely(c->desc == NULL))
			dump_tx_err = true;
		ret = mtk_uart_apdma_tx_handler(c);
	}
	//spin_unlock_irqrestore(&c->vc.lock, flags);
	spin_unlock(&c->vc.lock);
	if (current_dir == DMA_DEV_TO_MEM) {
		if (num % 5000 == 10)
			pr_debug("debug: %s: VFF_VALID_SIZE=0, num[%llu], flag_state[0x%x]\n",
				__func__, num, flag_state);
	} else if (current_dir == DMA_MEM_TO_DEV) {
		if (dump_tx_err)
			pr_info("debug: %s: TX[%d] FIX ME!", __func__, current_irq);
	}
	if (ret != 0)
		return IRQ_HANDLED;
	return IRQ_WAKE_THREAD;
}

static int mtk_uart_apdma_alloc_chan_resources(struct dma_chan *chan)
{
	struct mtk_uart_apdmadev *mtkd = to_mtk_uart_apdma_dev(chan->device);
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	unsigned int status;
	int ret;

	if (mtkd->support_hub && c->is_hub_port) {
		if (mtkd->support_wakeup)
			mtk_uart_set_apdma_clk(true);
		if (c->dir == DMA_DEV_TO_MEM) {
			pr_info("[%s] before:VFF_EN[%d], INT_EN[0x%x] INT_FLAG[0x%x],"
				"WPT[0x%x] RPT[0x%x] THRE[0x%x] LEN[0x%x], ADDR1[0x%x],"
				"ADDR2[0x%x]\n",
				__func__, mtk_uart_apdma_read(c, VFF_EN),
				mtk_uart_apdma_read(c, VFF_INT_EN),
				mtk_uart_apdma_read(c, VFF_INT_FLAG),
				mtk_uart_apdma_read(c, VFF_WPT),
				mtk_uart_apdma_read(c, VFF_RPT),
				mtk_uart_apdma_read(c, VFF_THRE),
				mtk_uart_apdma_read(c, VFF_LEN),
				mtk_uart_apdma_read(c, VFF_ADDR),
				mtk_uart_apdma_read(c, VFF_4G_SUPPORT));
		}
	} else {
		ret = clk_prepare_enable(mtkd->clk);
		if (ret) {
			pr_info("[%s]:clk_prepare_enable fail\n", __func__);
			goto err_out;
		}
		pr_info("%s, enable clk\n", __func__);
	}

	if (c->dir == DMA_MEM_TO_DEV) {
		mtk_uart_apdma_write(c, VFF_INT_EN, 0);
		mtk_uart_apdma_write(c, VFF_ADDR, 0);
		mtk_uart_apdma_write(c, VFF_THRE, 0xFFFF);
		mtk_uart_apdma_write(c, VFF_WPT, 0);
		mtk_uart_apdma_write(c, VFF_LEN, 0);
		mtk_uart_apdma_write(c, VFF_RST, VFF_WARM_RST_B);
	} else {
		mtk_uart_apdma_write(c, VFF_ADDR, 0);
		mtk_uart_apdma_write(c, VFF_THRE, 0);
		mtk_uart_apdma_write(c, VFF_LEN, 0);
		mtk_uart_apdma_write(c, VFF_RST, VFF_WARM_RST_B);
	}

	ret = readx_poll_timeout(readl, c->base + VFF_EN,
			  status, !status, 10, 100);
	//
	if (c->dir == DMA_MEM_TO_DEV)
		mtk_uart_apdma_write(c, VFF_THRE, 0);
	//
	if (ret)
		goto err_out;

	ret = request_threaded_irq(c->irq, mtk_uart_apdma_irq_handler,
			vchan_complete_thread_irq,
			IRQF_TRIGGER_NONE, KBUILD_MODNAME, chan);
	if (ret < 0) {
		dev_err(chan->device->dev, "Can't request dma IRQ\n");
		ret = -EINVAL;
		goto err_out;
	}

	ret = enable_irq_wake(c->irq);
	if (ret) {
		dev_info(chan->device->dev, "Can't enable dma IRQ wake\n");
		ret = -EINVAL;
		goto err_out;
	}

	if (c->dir == DMA_DEV_TO_MEM) {
		pr_info("[%s] after: VFF_EN[%d], INT_EN[0x%x] INT_FLAG[0x%x],"
			"WPT[0x%x] RPT[0x%x] THRE[0x%x] LEN[0x%x], ADDR1[0x%x],"
			"ADDR2[0x%x],\n",
			__func__, mtk_uart_apdma_read(c, VFF_EN),
			mtk_uart_apdma_read(c, VFF_INT_EN),
			mtk_uart_apdma_read(c, VFF_INT_FLAG),
			mtk_uart_apdma_read(c, VFF_WPT),
			mtk_uart_apdma_read(c, VFF_RPT),
			mtk_uart_apdma_read(c, VFF_THRE),
			mtk_uart_apdma_read(c, VFF_LEN),
			mtk_uart_apdma_read(c, VFF_ADDR),
			mtk_uart_apdma_read(c, VFF_4G_SUPPORT));
	}

	if (mtkd->support_bits > VFF_ORI_ADDR_BITS_NUM)
		mtk_uart_apdma_write(c, VFF_4G_SUPPORT, VFF_4G_SUPPORT_CLR_B);

err_out:
	if (mtkd->support_hub && (mtkd->support_wakeup) && c->is_hub_port)
		mtk_uart_set_apdma_clk(false);
	return ret;
}

static void mtk_uart_apdma_free_chan_resources(struct dma_chan *chan)
{
	struct mtk_uart_apdmadev *mtkd = to_mtk_uart_apdma_dev(chan->device);
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);

	free_irq(c->irq, chan);

	tasklet_kill(&c->vc.task);

	if (c->chan_desc_count > 0)
		pr_info("[WARN] %s, c->chan_desc_count[%d]\n", __func__, c->chan_desc_count);
	vchan_free_chan_resources(&c->vc);

	if (!c->is_hub_port) {
		clk_disable_unprepare(mtkd->clk);
		pr_info("%s, disable clk\n", __func__);
	}
}

static enum dma_status mtk_uart_apdma_tx_status(struct dma_chan *chan,
					 dma_cookie_t cookie,
					 struct dma_tx_state *txstate)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	enum dma_status ret;

	ret = dma_cookie_status(chan, cookie, txstate);
	if (!txstate)
		return ret;

	dma_set_residue(txstate, c->rx_status);

	return ret;
}

/*
 * dmaengine_prep_slave_single will call the function. and sglen is 1.
 * 8250 uart using one ring buffer, and deal with one sg.
 */
static struct dma_async_tx_descriptor *mtk_uart_apdma_prep_slave_sg
	(struct dma_chan *chan, struct scatterlist *sgl,
	unsigned int sglen, enum dma_transfer_direction dir,
	unsigned long tx_flags, void *context)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	struct mtk_uart_apdma_desc *d;
	unsigned int poll_cnt = 0;
	unsigned int idx_vd = 0;
	unsigned long flags;

	if (!is_slave_direction(dir) || sglen != 1) {
		pr_info("%s is_slave_direction: %d, sglen: %d\n",
			__func__, is_slave_direction(dir), sglen);
		return NULL;
	}

	/* Now allocate and setup the descriptor */
	d = kzalloc(sizeof(*d), GFP_NOWAIT);
	while ((!d) && (poll_cnt < MAX_POLLING_CNT)) {
		udelay(4);
		d = kzalloc(sizeof(*d), GFP_NOWAIT);
		poll_cnt++;
	}
	if (!d) {
		for (idx_vd = 0; idx_vd < MAX_GLOBAL_VD_COUNT; idx_vd++) {
			if (c->c_desc[idx_vd].is_using == false) {
				d = &(c->c_desc[idx_vd]);
				d->is_using = true;
				d->is_global_vd = true;
				break;
			}
		}
		if (idx_vd >= MAX_GLOBAL_VD_COUNT) {
			pr_info("%s kzalloc fail cnt:%d, no_global_vd\n", __func__, poll_cnt);
			return NULL;
		}
	} else {
		d->is_global_vd = false;
	}

	d->avail_len = sg_dma_len(sgl);
	d->addr = sg_dma_address(sgl);
	c->dir = dir;
	spin_lock_irqsave(&c->dma_lock, flags);
	c->chan_desc_count++;
	spin_unlock_irqrestore(&c->dma_lock, flags);
	return vchan_tx_prep(&c->vc, &d->vd, tx_flags);
}

static void mtk_uart_apdma_issue_pending(struct dma_chan *chan)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	struct virt_dma_desc *vd;
	unsigned long flags;

	spin_lock_irqsave(&c->vc.lock, flags);
	if (vchan_issue_pending(&c->vc)) {
		vd = vchan_next_desc(&c->vc);
		c->desc = to_mtk_uart_apdma_desc(&vd->tx);

		if (c->dir == DMA_DEV_TO_MEM)
			mtk_uart_apdma_start_rx(c);
		else if (c->dir == DMA_MEM_TO_DEV)
			mtk_uart_apdma_start_tx(c);
	}

	spin_unlock_irqrestore(&c->vc.lock, flags);
}

static int mtk_uart_apdma_slave_config(struct dma_chan *chan,
				   struct dma_slave_config *config)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);

	memcpy(&c->cfg, config, sizeof(*config));

	return 0;
}

static int mtk_uart_apdma_terminate_all(struct dma_chan *chan)
{
#if IS_ENABLED(CONFIG_MTK_UARTHUB)
	struct mtk_uart_apdmadev *mtkd = to_mtk_uart_apdma_dev(chan->device);
#endif
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	unsigned long flags;
	unsigned int status;
	LIST_HEAD(head);
	int ret;
	bool state;

#if IS_ENABLED(CONFIG_MTK_UARTHUB)
	if (mtkd->support_hub && (mtkd->support_wakeup) && c->is_hub_port)
		mtk_uart_set_apdma_clk(true);
#endif
	if (mtk_uart_apdma_read(c, VFF_INT_BUF_SIZE)) {
		mtk_uart_apdma_write(c, VFF_FLUSH, VFF_FLUSH_B);
		ret = readx_poll_timeout(readl, c->base + VFF_FLUSH,
				  status, status != VFF_FLUSH_B, 10, 100);
		dev_info(c->vc.chan.device->dev, "flush begin %s[%d]: %d\n",
			c->dir == DMA_DEV_TO_MEM ? "RX":"TX", c->irq, ret);
		/*
		 * DMA hardware will generate a interrupt immediately
		 * once flush done, so we need to wait the interrupt to be
		 * handled before free resources.
		 */
		state = true;
		while (state)
			irq_get_irqchip_state(c->irq,
				IRQCHIP_STATE_PENDING, &state);
		state = true;
		while (state)
			irq_get_irqchip_state(c->irq,
				IRQCHIP_STATE_ACTIVE, &state);
		dev_info(c->vc.chan.device->dev, "flush end %s\n",
			c->dir == DMA_DEV_TO_MEM ? "RX":"TX");
	}

	/*
	 * Stop need 3 steps.
	 * 1. set stop to 1
	 * 2. wait en to 0
	 * 3. set stop as 0
	 */
	mtk_uart_apdma_write(c, VFF_STOP, VFF_STOP_B);
	ret = readx_poll_timeout(readl, c->base + VFF_EN,
			  status, !status, 10, 100);
	if (ret)
		dev_err(c->vc.chan.device->dev, "stop: fail, status=0x%x\n",
			mtk_uart_apdma_read(c, VFF_DEBUG_STATUS));

	mtk_uart_apdma_write(c, VFF_STOP, VFF_STOP_CLR_B);
	mtk_uart_apdma_write(c, VFF_INT_EN, VFF_INT_EN_CLR_B);

	if (c->dir == DMA_DEV_TO_MEM)
		mtk_uart_apdma_write(c, VFF_INT_FLAG, VFF_RX_INT_CLR_B);
	else if (c->dir == DMA_MEM_TO_DEV)
		mtk_uart_apdma_write(c, VFF_INT_FLAG, VFF_TX_INT_CLR_B);

	synchronize_irq(c->irq);

	spin_lock_irqsave(&c->vc.lock, flags);
	vchan_get_all_descriptors(&c->vc, &head);
	spin_unlock_irqrestore(&c->vc.lock, flags);

	vchan_dma_desc_free_list(&c->vc, &head);
#if IS_ENABLED(CONFIG_MTK_UARTHUB)
	if (mtkd->support_hub && (mtkd->support_wakeup) && c->is_hub_port)
		mtk_uart_set_apdma_clk(false);
#endif
	return 0;
}

static int mtk_uart_apdma_device_pause(struct dma_chan *chan)
{
	struct mtk_chan *c = to_mtk_uart_apdma_chan(chan);
	unsigned long flags;

	spin_lock_irqsave(&c->vc.lock, flags);

	mtk_uart_apdma_write(c, VFF_EN, VFF_EN_CLR_B);
	mtk_uart_apdma_write(c, VFF_INT_EN, VFF_INT_EN_CLR_B);

	synchronize_irq(c->irq);

	spin_unlock_irqrestore(&c->vc.lock, flags);

	return 0;
}

static void mtk_uart_apdma_free(struct mtk_uart_apdmadev *mtkd)
{
	while (!list_empty(&mtkd->ddev.channels)) {
		struct mtk_chan *c = list_first_entry(&mtkd->ddev.channels,
			struct mtk_chan, vc.chan.device_node);

		list_del(&c->vc.chan.device_node);
		tasklet_kill(&c->vc.task);
	}
}

static const struct mtk_uart_apdmacomp mt6779_comp = {
	.addr_bits = 34
};

static const struct mtk_uart_apdmacomp mt6985_comp = {
	.addr_bits = 35
};

static const struct of_device_id mtk_uart_apdma_match[] = {
	{ .compatible = "mediatek,mt6577-uart-dma", .data = NULL},
	{ .compatible = "mediatek,mt2712-uart-dma", .data = NULL},
	{ .compatible = "mediatek,mt6779-uart-dma", .data = &mt6779_comp},
	{ .compatible = "mediatek,mt6985-uart-dma", .data = &mt6985_comp},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mtk_uart_apdma_match);

#if IS_ENABLED(CONFIG_MTK_UARTHUB)
static void mtk_uart_apdma_parse_peri(struct platform_device *pdev)
{
		void __iomem *peri_remap_apdma = NULL;
		unsigned int peri_apdma_base = 0, peri_apdma_mask = 0, peri_apdma_val = 0;
		unsigned int index = 0;

		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-regs", 0, &peri_apdma_base);
		if (index) {
			pr_notice("[%s] get peri_addr fail\n", __func__);
			return;
		}

		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-regs", 1, &peri_apdma_mask);
		if (index) {
			pr_notice("[%s] get peri_addr fail\n", __func__);
			return;
		}

		index = of_property_read_u32_index(pdev->dev.of_node,
			"peri-regs", 2, &peri_apdma_val);
		if (index) {
			pr_notice("[%s] get peri_addr fail\n", __func__);
			return;
		}

		peri_remap_apdma = ioremap(peri_apdma_base, 0x10);
		if (!peri_remap_apdma) {
			pr_notice("[%s] peri_remap_addr(%x) ioremap fail\n",
					__func__, peri_apdma_base);

				return;
		}

		writel(((readl(peri_remap_apdma) & (~peri_apdma_mask)) | peri_apdma_val),
			(void *)peri_remap_apdma);

		dev_info(&pdev->dev, "apdma clock protection:0x%x=0x%x",
			peri_apdma_base, readl(peri_remap_apdma));

}

#endif

static int mtk_uart_apdma_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct mtk_uart_apdmadev *mtkd;
	int rc;
	struct mtk_chan *c;
	unsigned int i;
	const struct mtk_uart_apdmacomp *comp;

	mtkd = devm_kzalloc(&pdev->dev, sizeof(*mtkd), GFP_KERNEL);
	if (!mtkd)
		return -ENOMEM;

#ifndef CONFIG_FPGA_EARLY_PORTING
	mtkd->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(mtkd->clk)) {
		dev_err(&pdev->dev, "No clock specified\n");
		rc = PTR_ERR(mtkd->clk);
		return rc;
	}
#endif

	comp = of_device_get_match_data(&pdev->dev);
	if (comp == NULL) {
		/*In order to compatiable with legacy device tree file*/
		dev_info(&pdev->dev,
			"No compatiable, using DTS configration\n");

		if (of_property_read_bool(pdev->dev.of_node,
				"mediatek,dma-33bits"))
			mtkd->support_bits = 33;
		else
			mtkd->support_bits = 32;
	} else
		mtkd->support_bits = comp->addr_bits;

	dev_info(&pdev->dev,
			"DMA address bits: %d\n",  mtkd->support_bits);
	rc = dma_set_mask_and_coherent(&pdev->dev,
			DMA_BIT_MASK(mtkd->support_bits));
	if (rc)
		return rc;

	dma_cap_set(DMA_SLAVE, mtkd->ddev.cap_mask);
	mtkd->ddev.device_alloc_chan_resources =
				mtk_uart_apdma_alloc_chan_resources;
	mtkd->ddev.device_free_chan_resources =
				mtk_uart_apdma_free_chan_resources;
	mtkd->ddev.device_tx_status = mtk_uart_apdma_tx_status;
	mtkd->ddev.device_issue_pending = mtk_uart_apdma_issue_pending;
	mtkd->ddev.device_prep_slave_sg = mtk_uart_apdma_prep_slave_sg;
	mtkd->ddev.device_config = mtk_uart_apdma_slave_config;
	mtkd->ddev.device_pause = mtk_uart_apdma_device_pause;
	mtkd->ddev.device_terminate_all = mtk_uart_apdma_terminate_all;
	mtkd->ddev.src_addr_widths = BIT(DMA_SLAVE_BUSWIDTH_1_BYTE);
	mtkd->ddev.dst_addr_widths = BIT(DMA_SLAVE_BUSWIDTH_1_BYTE);
	mtkd->ddev.directions = BIT(DMA_DEV_TO_MEM) | BIT(DMA_MEM_TO_DEV);
	mtkd->ddev.residue_granularity = DMA_RESIDUE_GRANULARITY_SEGMENT;
	mtkd->ddev.dev = &pdev->dev;
	INIT_LIST_HEAD(&mtkd->ddev.channels);

	mtkd->dma_requests = MTK_UART_APDMA_NR_VCHANS;
	if (of_property_read_u32(np, "dma-requests", &mtkd->dma_requests)) {
		dev_info(&pdev->dev,
			 "Using %u as missing dma-requests property\n",
			 MTK_UART_APDMA_NR_VCHANS);
	}

#if IS_ENABLED(CONFIG_MTK_UARTHUB)
	mtkd->support_hub = 0;
	mtkd->support_wakeup = 0;
	hub_dma_tx_chan = NULL;
	hub_dma_rx_chan = NULL;

	if (of_property_read_u32(np, "support-hub", &mtkd->support_hub)) {
		mtkd->support_hub = 0;
		dev_info(&pdev->dev,
			 "Using %u as missing support-hub property\n",
			 mtkd->support_hub);
	}

	if (mtkd->support_hub) {
		if (of_property_read_u32(np, "wakeup-irq-support", &mtkd->support_wakeup)) {
			dev_info(&pdev->dev,
				"Using %u as missing swakeup_irq_support property\n", mtkd->support_wakeup);
		}

		if (of_property_read_u32_index(pdev->dev.of_node,
			"peri-axi-dbg", 0, &peri_0_axi_dbg))
			pr_notice("[%s] get peri-axi-dbg fail\n", __func__);

		if (!mtkd->support_wakeup) {
			clk_count = 0;
			if (!clk_prepare_enable(mtkd->clk))
				clk_count++;
			pr_info("[%s]: support_hub[0x%x], clk_count[%d]\n", __func__,
			mtkd->support_hub , clk_count);
		} else {
			g_dma_clk = mtkd->clk;
			atomic_set(&dma_clk_count, 0);
			mutex_init(&g_dma_clk_mutex);
		}
		atomic_set(&tx_res_status, 0);
		atomic_set(&rx_res_status, 1);
		mtk_uart_apdma_parse_peri(pdev);
	}
#else
	dev_info(&pdev->dev, "CONFIG_MTK_UARTHUB is disabled.\n");
#endif

	for (i = 0; i < mtkd->dma_requests; i++) {
		c = devm_kzalloc(mtkd->ddev.dev, sizeof(*c), GFP_KERNEL);
		if (!c) {
			rc = -ENODEV;
			goto err_no_dma;
		}

		c->base = devm_platform_ioremap_resource(pdev, i);
		if (IS_ERR(c->base)) {
			rc = PTR_ERR(c->base);
			goto err_no_dma;
		}
		c->vc.desc_free = mtk_uart_apdma_desc_free;
		vchan_init(&c->vc, &mtkd->ddev);
		spin_lock_init(&c->dma_lock);

		rc = platform_get_irq(pdev, i);
		if (rc < 0) {
			dev_err(&pdev->dev, "failed to get IRQ[%d]\n", i);
			goto err_no_dma;
		}
		c->irq = rc;
		c->rec_idx = 0;
		c->cur_rec_idx = 0;
		atomic_set(&c->rxdma_state, 0);
#if IS_ENABLED(CONFIG_MTK_UARTHUB)
		c->is_hub_port = mtkd->support_hub & (1 << i);
		if (c->is_hub_port) {
			pr_info("c->is_hub_port is %d\n", c->is_hub_port);
			if (!hub_dma_tx_chan) {
				hub_dma_tx_chan = c;
				continue;
			}
			if (!hub_dma_rx_chan)
				hub_dma_rx_chan = c;
		}
	}
#endif

	rc = dma_async_device_register(&mtkd->ddev);
	if (rc)
		goto err_no_dma;

	platform_set_drvdata(pdev, mtkd);

	/* Device-tree DMA controller registration */
	rc = of_dma_controller_register(np, of_dma_xlate_by_chan_id, mtkd);
	if (rc)
		goto dma_remove;

	return rc;

dma_remove:
	dma_async_device_unregister(&mtkd->ddev);
err_no_dma:
	mtk_uart_apdma_free(mtkd);
	return rc;
}

static int mtk_uart_apdma_remove(struct platform_device *pdev)
{
	struct mtk_uart_apdmadev *mtkd = platform_get_drvdata(pdev);

	of_dma_controller_free(pdev->dev.of_node);

	mtk_uart_apdma_free(mtkd);

	dma_async_device_unregister(&mtkd->ddev);

	pm_runtime_disable(&pdev->dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mtk_uart_apdma_suspend(struct device *dev)
{
#if IS_ENABLED(CONFIG_MTK_UARTHUB)
	struct mtk_uart_apdmadev *mtkd = dev_get_drvdata(dev);
	if (mtkd->support_hub) {
		pr_info("[%s]: support_hub:%d, skip suspend\n", __func__, mtkd->support_hub);
		return 0;
	}
#endif

	return 0;
}

static int mtk_uart_apdma_resume(struct device *dev)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_MTK_UARTHUB)
	struct mtk_uart_apdmadev *mtkd = dev_get_drvdata(dev);
	if (mtkd->support_hub && (mtkd->support_wakeup)) {
		pr_info("[%s]: support_hub:%d,\n", __func__, mtkd->support_hub);
		return 0;
	} else if(mtkd->support_hub) {
		if (clk_count >= 1)
			return 0;
		else
			clk_count++;
	}
#endif

	return ret;
}
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops mtk_uart_apdma_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_uart_apdma_suspend, mtk_uart_apdma_resume)
};

static struct platform_driver mtk_uart_apdma_driver = {
	.probe	= mtk_uart_apdma_probe,
	.remove	= mtk_uart_apdma_remove,
	.driver = {
		.name		= KBUILD_MODNAME,
		.pm		= &mtk_uart_apdma_pm_ops,
		.of_match_table = of_match_ptr(mtk_uart_apdma_match),
	},
};

module_platform_driver(mtk_uart_apdma_driver);

MODULE_DESCRIPTION("MediaTek UART APDMA Controller Driver");
MODULE_AUTHOR("Long Cheng <long.cheng@mediatek.com>");
MODULE_LICENSE("GPL v2");
