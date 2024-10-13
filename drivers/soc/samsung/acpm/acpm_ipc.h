#ifndef __ACPM_IPC_H_
#define __ACPM_IPC_H_

#include <soc/samsung/acpm_ipc_ctrl.h>

struct acpm_ipc_ch {
	struct device_node *ch_node;
	unsigned int id;
	void __iomem *rx_rear;
	void __iomem *rx_front;
	void __iomem *rx_base;

	void __iomem *tx_rear;
	void __iomem *tx_front;
	void __iomem *tx_base;
	void __iomem *tx_direction;

	unsigned int buff_size;
	unsigned int buff_len;
	unsigned int d_buff_size;
	unsigned int *cmd;
	struct tasklet_struct dequeue_task;
	spinlock_t rx_lock;
	spinlock_t tx_lock;
	bool polling;

	void (*ipc_callback) (unsigned int *cmd, unsigned int size);
};

struct acpm_ipc_info {
	unsigned int channel_num;
	struct device *dev;
	struct acpm_ipc_ch *channel;
	unsigned int irq;
	void __iomem *intr;
};

#define LOG_ID_SHIFT				(29)
#define LOG_TIME_INDEX				(23)

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

#define IPC_TIMEOUT				(10000000)
#define APM_SYSTICK_NS_PERIOD			(1000 / 26)

#define UNTIL_EQUAL(arg0, arg1, flag)			\
do {							\
	u64 timeout = sched_clock() + IPC_TIMEOUT;	\
	bool t_flag = true;				\
	do {						\
		if ((arg0) == (arg1)) {			\
			t_flag = false;			\
			break;				\
		}					\
	} while (timeout >= sched_clock());		\
	if (t_flag) {					\
		pr_err("%s %d Timeout error!\n",	\
				__func__, __LINE__);	\
	}						\
	(flag) = t_flag;				\
} while(0)

#endif
