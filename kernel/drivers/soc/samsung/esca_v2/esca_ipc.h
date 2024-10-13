#ifndef __ESCA_IPC_H_
#define __ESCA_IPC_H_

#include <soc/samsung/esca.h>

extern u32 ESCA_MBOX__APP, ESCA_MBOX__PHY;

struct buff_info {
	void __iomem *rear;
	void __iomem *front;
	void __iomem *base;
	void __iomem *direction;

	unsigned int size;
	unsigned int len;
	unsigned int d_buff_size;
};

struct callback_info {
	void (*ipc_callback) (unsigned int *cmd, unsigned int size);
	struct device_node *client;
	struct list_head list;
};

#define SEQUENCE_NUM_MAX		(64)
struct esca_ipc_ch {
	struct buff_info rx_ch;
	struct buff_info tx_ch;
	struct list_head list;

	unsigned int id;
	unsigned int type;
	unsigned int seq_num;
	u32 seq_num_flag[SEQUENCE_NUM_MAX];
	unsigned int *cmd;
	spinlock_t rx_lock;
	spinlock_t tx_lock;
	spinlock_t ch_lock;
	struct mutex wait_lock;

	struct completion wait;
	bool polling;
	bool interrupt;
};

struct esca_ipc_info {
	unsigned int num_channels;
	struct device *dev;
	struct esca_ipc_ch *channel;
	unsigned int irq;
	void __iomem *intr;
	void __iomem *sram_base;
	struct esca_framework *initdata;
	unsigned int initdata_base;
	unsigned int intr_status;
	bool is_mailbox_master;
};

#define LOG_ID_SHIFT				(28)
#define LOG_TIME_INDEX				(20)
#define LOG_LEVEL				(19)
#define INTGR0					0x0008
#define INTCR0					0x000C
#define INTMR0					0x0010
#define INTSR0					0x0014
#define INTMSR0					0x0018
#define INTGR1					0x001C
#define INTCR1					0x0020
#define INTMR1					0x0024
#define INTSR1					0x0028
#define INTMSR1					0x002C
#define SR0					0x0080
#define SR1					0x0084
#define SR2					0x0088
#define SR3					0x008C

#define IPC_TIMEOUT				(15000000)
#define APM_PERITIMER_NS_PERIOD			(10416)

#define ESCA_IPC_PROTOCOL_OWN			(31)
#define ESCA_IPC_PROTOCOL_RSP			(30)
#define ESCA_IPC_PROTOCOL_INDIRECTION		(29)
#define ESCA_IPC_PROTOCOL_STOP_WDT		(27)
#define ESCA_IPC_PROTOCOL_ID			(26)
#define ESCA_IPC_PROTOCOL_IDX			(0x7 << ESCA_IPC_PROTOCOL_ID)
#define ESCA_IPC_PROTOCOL_TEST			(23)
#define ESCA_IPC_PROTOCOL_STOP			(22)
#define ESCA_IPC_PROTOCOL_SEQ_NUM		(16)
#define ESCA_IPC_TASK_PROF_START		(24)
#define ESCA_IPC_TASK_PROF_END			(25)
#define ESCA_TASK_PROF_IPI			(1 << 26)

#define ESCA_IPC_NUM_MAX			(16)

#define UNTIL_EQUAL(arg0, arg1, flag)			\
do {							\
	u64 timeout = sched_clock() + IPC_TIMEOUT;	\
	bool t_flag = true;				\
	do {						\
		if ((arg0) == (arg1)) {			\
			t_flag = false;			\
			break;				\
		} else {				\
			cpu_relax();			\
		}					\
	} while (timeout >= sched_clock());		\
	if (t_flag) {					\
		pr_err("%s %d Timeout error!\n",	\
				__func__, __LINE__);	\
	}						\
	(flag) = t_flag;				\
} while(0)

static inline void esca_interrupt_gen(struct esca_ipc_info *ipc, unsigned int id)
{
	/* APM NVIC INTERRUPT GENERATE */
	if (ipc->is_mailbox_master)
		writel((1 << id) << 16, ipc->intr + INTGR0);
	else
		writel((1 << id), ipc->intr + INTGR1);
}

static inline void esca_interrupt_self_gen(struct esca_ipc_info *ipc, unsigned int id)
{
	/* APM NVIC INTERRUPT SELF GENERATE */
	if (ipc->is_mailbox_master)
		writel((1 << id), ipc->intr + INTGR1);
	else
		writel((1 << id) << 16, ipc->intr + INTGR0);
}

static inline void esca_interrupt_clr(struct esca_ipc_info *ipc, unsigned int id)
{
	/* APM NVIC INTERRUPT Clear */
	if (ipc->is_mailbox_master)
		writel((1 << id), ipc->intr + INTCR1);
	else
		writel((1 << id) << 16, ipc->intr + INTCR0);
}

static inline unsigned int get_esca_interrupt_status(struct esca_ipc_info *ipc)
{
	/* APM NVIC INTERRUPT Clear */
	if (ipc->is_mailbox_master)
		return __raw_readl(ipc->intr + INTSR1);
	else
		return __raw_readl(ipc->intr + INTSR0) >> 16;
}

static inline void esca_interrupt_mask(struct esca_ipc_info *ipc, unsigned int mask)
{
	/* APM NVIC INTERRUPT Mask */
	if (ipc->is_mailbox_master)
		writel(mask, ipc->intr + INTMR1);
	else
		writel(mask << 16, ipc->intr + INTMR0);
}

#if IS_ENABLED(CONFIG_EXYNOS_ESCAV2)
extern int esca_ipc_init(struct platform_device *pdev);
#else
static inline int esca_ipc_init(struct platform_device *pdev)
{
	return -1;
}
#endif

#endif		/* __ESCA_IPC_H_ */
