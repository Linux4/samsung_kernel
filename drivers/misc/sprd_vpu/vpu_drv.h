/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Unisoc QOGIRN6PRO VPU driver
 *
 * Copyright (C) 2019 Unisoc, Inc.
 */

#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <linux/notifier.h>
#include <linux/compat.h>
#include <uapi/video/sprd_vpu.h>

struct clock_name_map_t {
	unsigned long freq;
	char *name;
	struct clk *clk_parent;
};

enum {
	RESET = 0,
	VPU_DOMAIN_EB
};

struct register_gpr {
	struct regmap *gpr;
	uint32_t reg;
	uint32_t mask;
};

static char *tb_name[] = {
	"reset",
	"vpu_domain_eb"
};

static char *vpu_clk_src[] = {
	"clk_src_256m",
	"clk_src_307m2",
	"clk_src_384m",
	"clk_src_512m",
	"clk_src_680m"
};

struct core_data {
	const char *name;
	irqreturn_t (*isr)(int irq, void *data);
	bool is_enc;
};

struct vpu_clk {
	unsigned int freq_div;

	struct clk *core_clk;
	struct clk *core_parent_clk;
	struct clk *clk_domain_eb;
	struct clk *clk_dev_eb;
	struct clk *clk_ckg_eb;
	struct clk *clk_ahb_vsp;
	struct clk *ahb_parent_clk;
};

struct vpu_qos_cfg {
	u8 awqos;
	u8 arqos_high;
	u8 arqos_low;
	unsigned int reg_offset;
};

struct vpu_platform_data {
	struct platform_device *pdev;
	struct device *dev;
	struct miscdevice mdev;
	const struct core_data *p_data;
	struct vpu_qos_cfg qos;
	struct vpu_clk clk;
	struct register_gpr regs[ARRAY_SIZE(tb_name)];
	struct clock_name_map_t clock_name_map[ARRAY_SIZE(vpu_clk_src)];
	struct semaphore vpu_mutex;
	struct wakeup_source vpu_wakelock;

	void __iomem *vpu_base;
	void __iomem *glb_reg_base;

	unsigned long phys_addr;
	unsigned int version;
	unsigned int max_freq_level;
	unsigned int qos_reg_offset;

	int irq;
	int condition_work;
	int vpu_int_status;

	bool is_vpu_acquired;
	bool iommu_exist_flag;
	bool is_clock_enabled;

	wait_queue_head_t wait_queue_work;
	atomic_t instance_cnt;
};

#define VPU_INT_STS_OFF		0x0
#define VPU_INT_MASK_OFF	0x04
#define VPU_INT_CLR_OFF		0x08
#define VPU_INT_RAW_OFF		0x0c
#define VPU_AXI_STS_OFF		0x1c

#define VPU_MMU_INT_MASK_OFF	0xA0
#define VPU_MMU_INT_CLR_OFF	0xA4
#define VPU_MMU_INT_STS_OFF	0xA8
#define VPU_MMU_INT_RAW_OFF	0xAC

#define VPU_AQUIRE_TIMEOUT_MS	500
#define VPU_INIT_TIMEOUT_MS	200
/*vpu dec*/
#define DEC_BSM_OVF_ERR 	BIT(0)
#define DEC_VLD_ERR		BIT(4)
#define DEC_TIMEOUT_ERR 	BIT(5)
#define DEC_MMU_INT_ERR 	BIT(13)
#define DEC_AFBCD_HERR		BIT(14)
#define DEC_AFBCD_PERR		BIT(15)

/*vpu enc*/
#define ENC_BSM_OVF_ERR 	BIT(2)
#define ENC_TIMEOUT_ERR 	BIT(3)
#define ENC_AFBCD_HERR		BIT(4)
#define ENC_AFBCD_PERR		BIT(5)
#define ENC_MMU_INT_ERR 	BIT(6)

#define MMU_RD_WR_ERR		0xff

irqreturn_t enc_core0_isr(int irq, void *data);
irqreturn_t enc_core1_isr(int irq, void *data);
irqreturn_t dec_core0_isr(int irq, void *data);
int get_clk(struct vpu_platform_data *data, struct device_node *np);
struct clk *get_clk_src_name(struct clock_name_map_t clock_name_map[], unsigned int freq_level, unsigned int max_freq_level);
int find_freq_level(struct clock_name_map_t clock_name_map[], unsigned long freq, unsigned int max_freq_level);
int clock_enable(struct vpu_platform_data *data);
void clock_disable(struct vpu_platform_data *data);
void clr_vpu_interrupt_mask(struct vpu_platform_data *data);
int get_iova(struct vpu_platform_data *data, struct iommu_map_data *mapdata, void __user *arg);
int free_iova(struct vpu_platform_data *data, struct iommu_map_data *ummapdata);
long compat_vpu_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
