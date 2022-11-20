/*
 * Copyright (C) 2014-2019, Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MCU_IPC_PRIV_H__
#define __MCU_IPC_PRIV_H__

/* Registers */
#define EXYNOS_MCU_IPC_MCUCTLR			0x0
#define EXYNOS_MCU_IPC_INTGR0			0x8
#define EXYNOS_MCU_IPC_INTCR0			0xc
#define EXYNOS_MCU_IPC_INTMR0			0x10
#define EXYNOS_MCU_IPC_INTSR0			0x14
#define EXYNOS_MCU_IPC_INTMSR0			0x18
#define EXYNOS_MCU_IPC_INTGR1			0x1c
#define EXYNOS_MCU_IPC_INTCR1			0x20
#define EXYNOS_MCU_IPC_INTMR1			0x24
#define EXYNOS_MCU_IPC_INTSR1			0x28
#define EXYNOS_MCU_IPC_INTMSR1			0x2c
#define EXYNOS_MCU_IPC_ISSR0			0x80
#define EXYNOS_MCU_IPC_ISSR1			0x84
#define EXYNOS_MCU_IPC_ISSR2			0x88
#define EXYNOS_MCU_IPC_ISSR3			0x8c

/* Bits definition */
#define MCU_IPC_MCUCTLR_MSWRST	(0)

#define MCU_IPC_RX_INT0		(1 << 16)
#define MCU_IPC_RX_INT1		(1 << 17)
#define MCU_IPC_RX_INT2		(1 << 18)
#define MCU_IPC_RX_INT3		(1 << 19)
#define MCU_IPC_RX_INT4		(1 << 20)
#define MCU_IPC_RX_INT5		(1 << 21)
#define MCU_IPC_RX_INT6		(1 << 22)
#define MCU_IPC_RX_INT7		(1 << 23)
#define MCU_IPC_RX_INT8		(1 << 24)
#define MCU_IPC_RX_INT9		(1 << 25)
#define MCU_IPC_RX_INT10	(1 << 26)
#define MCU_IPC_RX_INT11	(1 << 27)
#define MCU_IPC_RX_INT12	(1 << 28)
#define MCU_IPC_RX_INT13	(1 << 29)
#define MCU_IPC_RX_INT14	(1 << 30)
#define MCU_IPC_RX_INT15	(1 << 31)

struct mcu_ipc_ipc_handler {
	void *data;
	irq_handler_t handler;
};

struct mcu_ipc_drv_data {
	char *name;
	u32 id;

	void __iomem *ioaddr;
	u32 registered_irq;
	unsigned long unmasked_irq;

	/**
	 * irq affinity cpu mask
	 */
	cpumask_var_t dmask;	/* default cpu mask */
	cpumask_var_t imask;	/* irq affinity cpu mask */

	struct device *mcu_ipc_dev;
	struct mcu_ipc_ipc_handler hd[16];
	spinlock_t lock;
	spinlock_t reg_lock;

	int irq;
};

static struct mcu_ipc_drv_data mcu_dat[MCU_MAX];

static inline void mcu_ipc_writel(enum mcu_ipc_region id, u32 val, long reg)
{
	writel(val, mcu_dat[id].ioaddr + reg);
}

static inline u32 mcu_ipc_readl(enum mcu_ipc_region id, long reg)
{
	return readl(mcu_dat[id].ioaddr + reg);
}

#ifdef CONFIG_ARGOS
/* kernel team needs to provide argos header file. !!!
 * As of now, there's nothing to use.
 */
#ifdef CONFIG_SCHED_HMP
extern struct cpumask hmp_slow_cpu_mask;
extern struct cpumask hmp_fast_cpu_mask;

static inline struct cpumask *get_default_cpu_mask(void)
{
	return &hmp_slow_cpu_mask;
}
#else
static inline struct cpumask *get_default_cpu_mask(void)
{
	return cpu_all_mask;
}
#endif

struct mcu_argos_info {
	int irq;
	u32 affinity;
};

int argos_irq_affinity_setup_label(unsigned int irq, const char *label,
		struct cpumask *affinity_cpu_mask,
		struct cpumask *default_cpu_mask);
int argos_task_affinity_setup_label(struct task_struct *p, const char *label,
		struct cpumask *affinity_cpu_mask,
		struct cpumask *default_cpu_mask);
#endif

#endif /* __MCU_IPC_PRIV_H__ */
