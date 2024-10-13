/*copyright (C) 2013 Spreadtrum Communications Inc.
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

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <mach/hardware.h>
#include <mach/bm_sc8830.h>

/*NOTE: depends on ioremap in io.c */
#define AXI_BM_BASE_OFFSET(chn)		(0x2000*chn)
#define AXI_BM_INTC_REG(chn)		(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x0)
#define AXI_BM_CFG_REG(chn)		(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x4)
#define AXI_BM_ADDR_MIN_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x8)
#define AXI_BM_ADDR_MAX_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0xc)
#define AXI_BM_ADDR_MSK_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x10)
#define AXI_BM_DATA_MIN_L_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x14)
#define AXI_BM_DATA_MIN_H_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x18)
#define AXI_BM_DATA_MAX_L_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x1c)
#define AXI_BM_DATA_MAX_H_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x20)
#define AXI_BM_DATA_MSK_L_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x24)
#define AXI_BM_DATA_MSK_H_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x28)
#define AXI_BM_CNT_WIN_LEN_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x2c)
#define AXI_BM_PEAK_WIN_LEN_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x30)
#define AXI_BM_MATCH_ADDR_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x34)
#define AXI_BM_MATCH_CMD_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x38)
#define AXI_BM_MATCH_DATA_L_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x3c)
#define AXI_BM_MATCH_DATA_H_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x40)
#define AXI_BM_RTRANS_IN_WIN_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x44)
#define AXI_BM_RBW_IN_WIN_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x48)
#define AXI_BM_RLATENCY_IN_WIN_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x4c)
#define AXI_BM_WTRANS_IN_WIN_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x50)
#define AXI_BM_WBW_IN_WIN_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x54)
#define AXI_BM_WLATENCY_IN_WIN_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x58)
#define AXI_BM_PEAKBW_IN_WIN_REG(chn)	(SPRD_AXIBM0_BASE + AXI_BM_BASE_OFFSET(chn) + 0x5c)

#define AXI_BM_EN			BIT(0)
#define AXI_BM_CNT_EN			BIT(1)
#define AXI_BM_CNT_START		BIT(3)
#define AXI_BM_CNT_CLR			BIT(4)
#define AXI_BM_INT_EN			BIT(28)
#define AXI_BM_INT_CLR			BIT(29)

//#define DDR_MONITOR_LOG

#ifdef DDR_MONITOR_LOG

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>

#define PER_COUTN_LIST_SIZE 128
/*the buf can store 8 secondes data*/
#define PER_COUNT_RECORD_SIZE 800
#define PER_COUNT_BUF_SIZE (64 * 4 * PER_COUNT_RECORD_SIZE)

#define LOG_FILE_PATH "/mnt/obb/axi_per_log"
/*the log file size about 1.5Mbytes per min*/
#define LOG_FILE_SECONDS (60  * 30)
#define LOG_FILE_MAX_RECORDS (LOG_FILE_SECONDS * 100)

static struct file *log_file;

static DEFINE_SPINLOCK(bm_lock);
static struct semaphore bm_seam;

static void *per_buf;
static int per_count_list[PER_COUTN_LIST_SIZE];
static int list_write_index;
static bool glb_count_flag;

struct bm_per_info {
	u32 t_start;
	u32 t_stop;
	u32 tmp1;
	u32 tmp2;
	u32 per_data[10][6];
};

static int buf_read_index;
static int buf_write_index;
static bool bm_irq_in_process;
long long t_stamp;

#endif

#if CONFIG_BUS_MONITOR_DEBUG
static void __sci_axi_bm_debug(void)
{
	u32 bm_index, reg_index, val;

	pr_debug("REG_PUB_APB_BUSMON_CFG: 0x%08x\n", sci_glb_read(REG_PUB_APB_BUSMON_CFG, -1UL));
	for (bm_index = AXI_BM0; bm_index <= AXI_BM9; bm_index++) {
		for(reg_index=0; reg_index<=0x5c; reg_index+=0x4){
			val = __raw_readl(AXI_BM_INTC_REG(bm_index)+reg_index);
			if(val)
				pr_debug("*** %s, chn:%d reg%x:0x%x ***\n",
						__func__, bm_index, reg_index, val );
		}
	}
}
#endif

static inline void __sci_axi_bm_chn_en(int chn)
{
	u32 val;

	val = __raw_readl(AXI_BM_INTC_REG(chn));
	val |= (AXI_BM_EN);
	__raw_writel(val, AXI_BM_INTC_REG(chn));

	return;
}


static inline void __sci_axi_bm_chn_int_clr(int chn)
{
	u32 val;

	val = __raw_readl(AXI_BM_INTC_REG(chn));
	val |= (AXI_BM_INT_CLR);
	__raw_writel(val, AXI_BM_INTC_REG(chn));

	return;

}

static inline u32 __sci_axi_bm_chn_cnt_bw(int chn)
{
	u32 rbw, wbw;

	rbw = __raw_readl(AXI_BM_RBW_IN_WIN_REG(chn));
	wbw = __raw_readl(AXI_BM_WBW_IN_WIN_REG(chn));
	if(rbw || wbw)
		pr_debug(" chn:%d, rbw:%u, wbw:%u \n", chn, rbw, wbw );

	return (rbw+wbw);
}

static void __sci_axi_bm_cnt_start(void)
{
	int bm_index;
	u32 val;

	for (bm_index = AXI_BM0; bm_index <= AXI_BM9; bm_index++) {
		val = __raw_readl(AXI_BM_INTC_REG(bm_index));
#ifdef DDR_MONITOR_LOG
		val |= (AXI_BM_EN | AXI_BM_CNT_EN | AXI_BM_CNT_START | AXI_BM_INT_EN);
#else
		val |= (AXI_BM_EN | AXI_BM_CNT_EN | AXI_BM_CNT_START | AXI_BM_INT_CLR);
#endif
		__raw_writel(val, AXI_BM_INTC_REG(bm_index));
	}
	return;
}

static void __sci_axi_bm_cnt_stop(void)
{
	int bm_index;
	u32 val;

	for (bm_index = AXI_BM0; bm_index <= AXI_BM9; bm_index++) {
		val = __raw_readl(AXI_BM_INTC_REG(bm_index));
		val &= ~(AXI_BM_CNT_START);
		__raw_writel(val, AXI_BM_INTC_REG(bm_index));
	}
	return;
}

static void __sci_axi_bm_int_clr(void)
{
	int bm_index;

	for (bm_index = AXI_BM0; bm_index <= AXI_BM9; bm_index++) {
		__sci_axi_bm_chn_int_clr(bm_index);
	}
}

/* performance count clear */
static void __sci_axi_bm_cnt_clr(void)
{
	int bm_index;
	u32 val;

	for (bm_index = AXI_BM0; bm_index <= AXI_BM9; bm_index++) {
		val = __raw_readl(AXI_BM_INTC_REG(bm_index));
		val |= (AXI_BM_CNT_EN | AXI_BM_EN);
		__raw_writel(val, AXI_BM_INTC_REG(bm_index));
		val &= ~(AXI_BM_CNT_START);
		__raw_writel(val, AXI_BM_INTC_REG(bm_index));
		val |= (AXI_BM_CNT_CLR);
		__raw_writel(val, AXI_BM_INTC_REG(bm_index));
#if CONFIG_BUS_MONITOR_DEBUG
		pr_debug(" chn:%d, val:0x%x, reg:0x%x \n", bm_index, val, AXI_BM_INTC_REG(bm_index));
		__sci_axi_bm_chn_cnt_bw(bm_index);
#endif
	}
	return;
}

static void __sci_axi_bm_init(void)
{
	int bm_index, reg_index;

	for (bm_index = AXI_BM0; bm_index <= AXI_BM9; bm_index++) {
		for(reg_index=0; reg_index<=0x5c; reg_index+=0x4)
			__raw_writel(0, AXI_BM_INTC_REG(bm_index)+reg_index);
	}

}

static void __sci_bm_glb_count_enable(bool is_enable)
{
	u32 reg_val = 0;

	reg_val = sci_glb_read(REG_PUB_APB_BUSMON_CNT_START, 0x1);
	if (is_enable) {
		if (!reg_val)
			sci_glb_set(REG_PUB_APB_BUSMON_CNT_START, BIT(0));
	} else {
		sci_glb_clr(REG_PUB_APB_BUSMON_CNT_START, BIT(0));
	}
}

static int __sci_bm_glb_reset_and_enable(u32 bm_index, bool is_enable)
{
	u32 reg_en, reg_rst, bit_en, bit_rst;

	switch (bm_index) {
	case AXI_BM0:
	case AXI_BM1:
	case AXI_BM2:
	case AXI_BM3:
	case AXI_BM4:
	case AXI_BM5:
	case AXI_BM6:
	case AXI_BM7:
	case AXI_BM8:
	case AXI_BM9:
		reg_en = REG_PUB_APB_BUSMON_CFG;
		reg_rst = REG_PUB_APB_BUSMON_CFG;
		bit_en = BIT(16 + bm_index);
		bit_rst = BIT(bm_index);
		break;
	case AHB_BM0:
	case AHB_BM1:
	case AHB_BM2:
		reg_en = REG_AP_AHB_AHB_EB;
		reg_rst = REG_AP_AHB_AHB_RST;
		bit_en = BIT(14 + bm_index - AHB_BM0);
		bit_rst = BIT(17 + bm_index - AHB_BM0);
		break;

	default:
		return -1;
	}

	sci_glb_set(reg_rst, bit_rst);
	sci_glb_clr(reg_rst, bit_rst);

	if (is_enable) {
		sci_glb_set(reg_en, bit_en);
	} else {
		sci_glb_clr(reg_en, bit_en);
	}

	return 0;
}

unsigned int dmc_mon_cnt_bw(void)
{
	int chn;
	u32 cnt;
#ifdef DDR_MONITOR_LOG
	int i, read_index;
	cnt = 0x0;
	/*read the last 50 records(500ms)*/
	read_index = list_write_index - 1;
	if (read_index < 0) {
		read_index = PER_COUTN_LIST_SIZE - 1;
	}

	for (i = 0; i < 50; i++) {
		cnt += per_count_list[read_index];
		if (--read_index < 0)
			read_index = PER_COUTN_LIST_SIZE - 1;
	}

	return cnt;
#endif

	cnt = 0;
	for (chn = AXI_BM0; chn <= AXI_BM9; chn++) {
		cnt += __sci_axi_bm_chn_cnt_bw(chn);
	}
#if CONFIG_BUS_MONITOR_DEBUG
	__sci_axi_bm_debug( );
	pr_debug(" %s done \n", __func__ );
#endif
	return cnt;
}
EXPORT_SYMBOL_GPL(dmc_mon_cnt_bw);

void dmc_mon_cnt_clr(void)
{
#ifdef DDR_MONITOR_LOG

#else
	__sci_axi_bm_cnt_clr();
#endif
	return;
}
EXPORT_SYMBOL_GPL(dmc_mon_cnt_clr);

void dmc_mon_cnt_start(void)
{
#ifdef DDR_MONITOR_LOG

#else
	__sci_axi_bm_cnt_start();
	__sci_bm_glb_count_enable(true);
	__sci_axi_bm_cnt_start();
#endif
	return;
}
EXPORT_SYMBOL_GPL(dmc_mon_cnt_start);

void dmc_mon_cnt_stop(void)
{
#ifdef DDR_MONITOR_LOG

#else
	__sci_bm_glb_count_enable(false);
	__sci_axi_bm_cnt_stop();
#endif
	return;
}
EXPORT_SYMBOL_GPL(dmc_mon_cnt_stop);

#ifdef DDR_MONITOR_LOG
static void __sci_axi_bm_set_winlen(void);
#endif
void dmc_mon_resume(void)
{
#ifdef DDR_MONITOR_LOG
	u32 bm_index;

	for (bm_index = AXI_BM0; bm_index <= AXI_BM9; bm_index++) {
		__sci_bm_glb_reset_and_enable(bm_index, true);
	}
	__sci_axi_bm_init();
	__sci_axi_bm_cnt_clr();
	__sci_axi_bm_int_clr();

	__sci_bm_glb_count_enable(glb_count_flag);

	__sci_axi_bm_set_winlen();
	__sci_axi_bm_cnt_start();

#else

#endif
	return;
}
EXPORT_SYMBOL_GPL(dmc_mon_resume);


#ifdef DDR_MONITOR_LOG

extern u32 emc_clk_get(void);

static void __sci_axi_bm_set_winlen(void)
{
	int bm_index;
	u32 axi_clk, win_len;
	/*the win len is 10ms*/
#if CONFIG_ARCH_SCX30G
axi_clk = emc_clk_get();
#else
axi_clk = __raw_readl(REG_AON_APB_DPLL_CFG) & 0x7ff;
axi_clk = axi_clk << 2;
#endif
	/*the win_len = (axi_clk / 1000) * 10 */
	win_len = axi_clk * 10000;
	for (bm_index = AXI_BM0; bm_index <= AXI_BM9; bm_index++) {
		__raw_writel(win_len, AXI_BM_CNT_WIN_LEN_REG(bm_index));
	}
}

static irqreturn_t __bm_isr(int irq_num, void *dev)
{
	int bm_chn;
	u32 rwbw_cnt;
	struct bm_per_info *bm_info;
	bm_info = (struct bm_per_info *)per_buf;

	spin_lock(&bm_lock);

	/*only one precess handle the bm's irq is this case */
	if (!bm_irq_in_process) {
		bm_irq_in_process = true;
	} else {
		spin_unlock(&bm_lock);
		return IRQ_NONE;
	}

	rwbw_cnt = 0x0;

	__sci_axi_bm_cnt_stop();

	/*count stop time stamp */
	bm_info[buf_write_index].t_stop = __raw_readl(SPRD_SYSCNT_BASE + 0xc);

	for (bm_chn = 0; bm_chn < 10; bm_chn++) {
		bm_info[buf_write_index].per_data[bm_chn][0] = __raw_readl(AXI_BM_RTRANS_IN_WIN_REG(bm_chn));
		bm_info[buf_write_index].per_data[bm_chn][1] = __raw_readl(AXI_BM_RBW_IN_WIN_REG(bm_chn));
		bm_info[buf_write_index].per_data[bm_chn][2] = __raw_readl(AXI_BM_RLATENCY_IN_WIN_REG(bm_chn));

		bm_info[buf_write_index].per_data[bm_chn][3] = __raw_readl(AXI_BM_WTRANS_IN_WIN_REG(bm_chn));
		bm_info[buf_write_index].per_data[bm_chn][4] = __raw_readl(AXI_BM_WBW_IN_WIN_REG(bm_chn));
		bm_info[buf_write_index].per_data[bm_chn][5] = __raw_readl(AXI_BM_WLATENCY_IN_WIN_REG(bm_chn));

		rwbw_cnt += bm_info[buf_write_index].per_data[bm_chn][1];
		rwbw_cnt += bm_info[buf_write_index].per_data[bm_chn][4];
	}

	per_count_list[list_write_index] = rwbw_cnt;
	if (++list_write_index == PER_COUTN_LIST_SIZE) {
		list_write_index = 0;
	}

	if (__raw_readl(REG_PUB_APB_BUSMON_CNT_START) == 0x1) {
		glb_count_flag = true;
		if (++buf_write_index == PER_COUNT_RECORD_SIZE) {
			buf_write_index = 0;
		}

		/*wake up the thread to output log per 4 second*/
		if ((buf_write_index == 0) ||
			buf_write_index == (PER_COUNT_RECORD_SIZE >> 1) ) {
#if 0
			int this_cpu;
			this_cpu = smp_processor_id();
			t_stamp = cpu_clock(this_cpu);
#endif
			up(&bm_seam);
		}
	} else {
		glb_count_flag = false;
	}

	__sci_axi_bm_int_clr();
	__sci_axi_bm_cnt_clr();
	__sci_axi_bm_set_winlen();

	/*count start time stamp */
	bm_info[buf_write_index].t_start = __raw_readl(SPRD_SYSCNT_BASE + 0xc);
	bm_info[buf_write_index].tmp1    = emc_clk_get();
	bm_info[buf_write_index].tmp2	 = __raw_readl(REG_AON_APB_DPLL_CFG);

	__sci_axi_bm_cnt_start();

	bm_irq_in_process = false;

	spin_unlock(&bm_lock);

	return IRQ_HANDLED;
}


static int bm_output_log(void *p)
{
	mm_segment_t old_fs;
	int ret;
	struct bm_per_info *bm_info;

	bm_info = (struct bm_per_info *)per_buf;

	while (1) {
		down(&bm_seam);

		if (!log_file) {
			log_file = filp_open(LOG_FILE_PATH, O_RDWR | O_CREAT | O_TRUNC, 0644);
			if (IS_ERR(log_file) || !log_file || !log_file->f_dentry) {
				pr_err("file_open(%s) for create failed\n", LOG_FILE_PATH);
				return -ENODEV;
			}
		}
		switch (buf_write_index) {
		case 0:
			buf_read_index = PER_COUNT_RECORD_SIZE >> 1;
			break;
		case (PER_COUNT_RECORD_SIZE >> 1):
			buf_read_index = 0x0;
			break;
		default:
			pr_err("get buf_read_indiex failed!\n");
		}
#if 0
		unsigned long nanosec_rem;

		nanosec_rem = do_div(t_stamp, 1000000000);

		sprintf(log_file_path, "%s%5lu.%06lu-log",
				LOG_FILE_PRFEX,
				(unsigned long) t_stamp,
				nanosec_rem / 1000);
#endif
		old_fs = get_fs();
		set_fs(get_ds());

		ret = vfs_write(log_file,
			(const char *)(bm_info + buf_read_index),
			sizeof(struct bm_per_info) *(PER_COUNT_RECORD_SIZE >> 1),
			&log_file->f_pos);

		set_fs(old_fs);

		/*raw back file write*/
		if (log_file->f_pos >= (sizeof(struct bm_per_info) * LOG_FILE_MAX_RECORDS)) {
			log_file->f_pos = 0x0;
		}
	}

	filp_close(log_file, NULL);

	return 0;

}

#endif


static int __init sci_bm_init(void)
{
	int bm_index;
	int ret;

#ifdef DDR_MONITOR_LOG
	struct task_struct *t;
	per_buf = kmalloc(PER_COUNT_BUF_SIZE, GFP_KERNEL);
	if (!per_buf) {
		pr_err("kmalloc failed!\n");
		return -ENOMEM;
	}

	ret = request_irq(IRQ_AXI_BM_PUB_INT, __bm_isr, IRQF_TRIGGER_NONE, "axi_bm_per", NULL);
	if (ret) {
		pr_err("request_irq(%d) failed!\n", IRQ_AXI_BM_PUB_INT);
		return ret;
	}

	__sci_bm_glb_count_enable(false);

	sema_init(&bm_seam, 0);

	t = kthread_run(bm_output_log, NULL, "%s", "bm_per_log");
	/*fixme */
#endif

	for (bm_index = AXI_BM0; bm_index <= AXI_BM9; bm_index++) {
		__sci_bm_glb_reset_and_enable(bm_index, true);
	}
	__sci_axi_bm_init();
	__sci_axi_bm_cnt_clr();
	__sci_axi_bm_int_clr();

#ifdef DDR_MONITOR_LOG
	__sci_axi_bm_set_winlen();
	__sci_bm_glb_count_enable(false);
	__sci_axi_bm_cnt_start();
#endif

	return 0;
}

static void __exit sci_bm_exit(void)
{
	int bm_index;
#ifdef DDR_MONITOR_LOG
	kfree(per_buf);
	filp_close(log_file, NULL);
#endif
	for (bm_index = AXI_BM0; bm_index <= AXI_BM9; bm_index++) {
		__sci_bm_glb_reset_and_enable(bm_index, false);
	}

	for (bm_index = AHB_BM0; bm_index <= AHB_BM2; bm_index++) {
		__sci_bm_glb_reset_and_enable(bm_index, false);
	}

	__sci_bm_glb_count_enable(false);
}

module_init(sci_bm_init);
module_exit(sci_bm_exit);
