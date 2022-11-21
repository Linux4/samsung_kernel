/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2012 Marvell International Ltd.
 * All Rights Reserved
 */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/aio.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/sched.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <asm/system.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <plat/pxa9xx_acipc.h>
#if defined(CONFIG_PXA95x_DVFM)
#include <mach/dvfm.h>
#endif
#ifdef CONFIG_PXA_MIPSRAM
#include <mach/pxa_mips_ram.h>
#else
#define MIPS_RAM_ADD_PM_TRACE(a) {}
#endif
#include <asm/mach-types.h>

#if defined(CONFIG_PXA95x) || defined(CONFIG_PXA93x)
#include <mach/regs-ost.h>
#include <mach/hardware.h>
#endif

struct pxa9xx_acipc {
	struct resource *mem;
	void __iomem *mmio_base;
	int irq[ACIPC_NUMBER_OF_INTERRUPTS];
	char irq_name[ACIPC_NUMBER_OF_INTERRUPTS][20];
	u32 IIR_val;
	struct acipc_database acipc_db;
	u32 bind_event_arg;
	wait_queue_head_t acipc_wait_q;
	int poll_status;
	spinlock_t poll_lock;
	u32 historical_events;
	struct DDR_status g_ddr_status;
};

static struct pxa9xx_acipc *acipc;

#if defined(CONFIG_PXA95x) || defined(CONFIG_PXA93x)
#define ACIPC_DDR_NOT_AVAIL		0
#define ACIPC_DDR_AVAIL			1

static u32 ddr_avail_flag;

/* this parameter will hold information whether CP is availible */
static int internal_acipc_pm_cp;

u32 set_DDR_avail_flag(void)
{
	unsigned long flags;
	/*
	 * If there's no CP acipc request should return without
	 * HW handling. SW returns expected value in order to avoid
	 * warnings
	 */
	if (!internal_acipc_pm_cp)
		return 1;

	local_irq_save(flags);

	if (ddr_avail_flag)
		pr_warning("******EDDR ***WARNING*** shared flag=1 "\
			   "althoght it already on\n");
	/*
	 * TODO - need to create a shadow for WDR
	 * register and implement clear and set Marcos
	 */
	acipc_writel(IPC_WDR, ACIPC_DDR_AVAIL);
	ddr_avail_flag = 1;

	local_irq_restore(flags);

	return 1;
}
EXPORT_SYMBOL(set_DDR_avail_flag);
#endif

/* PXA930/PXA910 ACIPC registers */
#define IPC_WDR		0x0004
#define IPC_ISRW	0x0008
#define IPC_ICR		0x000C
#define IPC_IIR		0x0010
#define IPC_RDR		0x0014

#define acipc_readl(off)	__raw_readl(acipc->mmio_base + (off))
#define acipc_writel(off, v)	__raw_writel((v), acipc->mmio_base + (off))

#if defined(CONFIG_CPU_PXA978)
static const enum acipc_events acipc_priority_table_dkb[ACIPC_NUMBER_OF_EVENTS]
	= {
	ACIPC_DDR_READY_REQ,
	ACIPC_DDR_RELQ_REQ,
	ACIPC_RINGBUF_TX_STOP,
	ACIPC_RINGBUF_TX_RESUME,
	ACIPC_PORT_FLOWCONTROL,
	ACIPC_SPARE,
	ACIPC_SPARE,
	ACIPC_SPARE,
	ACIPC_SHM_PACKET_NOTIFY,
	ACIPC_SHM_PEER_SYNC
};
#endif

#if defined(CONFIG_CPU_PXA910)
static const enum acipc_events
	acipc_priority_table_dkb[ACIPC_NUMBER_OF_EVENTS] = {
	ACIPC_RINGBUF_TX_STOP,
	ACIPC_RINGBUF_TX_RESUME,
	ACIPC_PORT_FLOWCONTROL,
	ACIPC_SPARE,
	ACIPC_SPARE,
	ACIPC_SPARE,
	ACIPC_SPARE,
	ACIPC_SPARE,
	ACIPC_SHM_PACKET_NOTIFY,
	ACIPC_IPM
};
#elif defined(CONFIG_CPU_PXA988)
static const enum acipc_events
	acipc_priority_table_dkb[ACIPC_NUMBER_OF_EVENTS] = {
	ACIPC_RINGBUF_TX_STOP,
	ACIPC_RINGBUF_TX_RESUME,
	ACIPC_PORT_FLOWCONTROL,
	ACIPC_MODEM_DDR_UPDATE_REQ,
	ACIPC_RINGBUF_PSD_TX_STOP,
	ACIPC_RINGBUF_PSD_TX_RESUME,
	ACIPC_SHM_PSD_PACKET_NOTIFY,
	ACIPC_SHM_DIAG_PACKET_NOTIFY,
	ACIPC_SHM_PACKET_NOTIFY,
	ACIPC_IPM
};
#endif

/* PXA910 specific define */
#if defined(CONFIG_CPU_PXA910) || defined(CONFIG_CPU_PXA988)
#define acipc_writel_withdummy(off, v)	acipc_writel((off), (v))
#endif

/* PXA95x/93x specific define */
#if defined(CONFIG_PXA95x) || defined(CONFIG_PXA93x)
static const enum acipc_events acipc_priority_table[ACIPC_NUMBER_OF_EVENTS] = {
	ACIPC_DDR_READY_REQ,
	ACIPC_DDR_260_READY_REQ,
	ACIPC_DDR_RELQ_REQ,
	ACIPC_DDR_260_RELQ_REQ,
	ACIPC_DATA_Q_ADRS,
	ACIPC_DATA_IND,
	ACIPC_MSL_WAKEUP_REQ,
	ACIPC_MSL_WAKEUP_ACK,
	ACIPC_MSL_SLEEP_ALLOW,
	ACIPC_SPARE_1
};

/* write ICR/ISRW (may require dummy access) */
#define acipc_writel_withdummy(off, v)	\
do {	\
		acipc_writel((off), (v));\
		if (cpu_is_pxa95x() || cpu_is_pxa935())	{\
			/* A dummy read is required after write */\
			volatile unsigned long *dummy =	\
			(volatile unsigned long *)(acipc_readl(off));\
			(void)dummy; /* prevent unused variable warning */ \
	}	\
} while (0)

#endif

/* read IIR */
#define ACIPC_IIR_READ(IIR_val)			\
{								\
	/* dummy write to latch the IIR value */	\
	acipc_writel(IPC_IIR, 0x0);		\
	barrier();				\
	(IIR_val) = acipc_readl(IPC_IIR);	\
}

static u32 acipc_default_callback(u32 status)
{
	IPC_ENTER();
	/*
	 * getting here means that the client didn't yet bind his callback.
	 * we will save the event until the bind occur
	 */
	acipc->acipc_db.historical_event_status |= status;

	IPC_LEAVE();
	return 0;
}

static enum acipc_return_code acipc_event_set(enum acipc_events user_event)
{
	IPC_ENTER();
	acipc_writel_withdummy(IPC_ISRW, user_event);
	IPCTRACE("acipc_event_set userEvent 0x%x\n", user_event);

	IPC_LEAVE();
	return ACIPC_RC_OK;
}

static void acipc_int_enable(enum acipc_events event)
{
	IPCTRACE("acipc_int_enable event 0x%x\n", event);

	if (event & ACIPC_INT0_EVENTS) {
		/*
		 * for the first 8 bits we enable the INTC_AC_IPC_0
		 * only if this the first event that has been binded
		 */
		if (!(acipc->acipc_db.int0_events_cnt))
			enable_irq(acipc->irq[0]);

		acipc->acipc_db.int0_events_cnt |= event;
		return;
	}
	if (event & ACIPC_INT1_EVENTS) {
		enable_irq(acipc->irq[1]);
		return;
	}
	if (event & ACIPC_INT2_EVENTS) {
		enable_irq(acipc->irq[2]);
		return;
	}
}

static void acipc_int_disable(enum acipc_events event)
{
	IPC_ENTER();

	IPCTRACE("acipc_int_disable event 0x%x\n", event);
	if (event & ACIPC_INT0_EVENTS) {
		if (!acipc->acipc_db.int0_events_cnt)
			return;

		acipc->acipc_db.int0_events_cnt &= ~event;
		/*
		 * for the first 8 bits we disable INTC_AC_IPC_0
		 * only if this is the last unbind event
		 */
		if (!(acipc->acipc_db.int0_events_cnt))
			disable_irq(acipc->irq[0]);
		return;
	}
	if (event & ACIPC_INT1_EVENTS) {
		disable_irq(acipc->irq[1]);
		return;
	}
	if (event & ACIPC_INT2_EVENTS) {
		disable_irq(acipc->irq[2]);
		return;
	}
}

#if defined(CONFIG_PXA95x) || defined(CONFIG_PXA93x)
static void acipc_change_driver_state(int is_DDR_ready)
{
	IPC_ENTER();

	IPCTRACE("acipc_change_driver_state isDDRReady %d\n", is_DDR_ready);
	if (is_DDR_ready)
		acipc->acipc_db.driver_mode = ACIPC_CB_NORMAL;
	else
		acipc->acipc_db.driver_mode = ACIPC_CB_ALWAYS_NO_DDR;

	IPC_LEAVE();
}
#endif

static enum acipc_return_code acipc_data_send(enum acipc_events user_event,
					      acipc_data data)
{
	IPC_ENTER();
	IPCTRACE("acipc_data_send userEvent 0x%x, data 0x%x\n",
		 user_event, data);
	/* writing the data to WDR */
	acipc_writel(IPC_WDR, data);

	/*
	 * fire the event to the other subsystem
	 * to indicate the data is ready for read
	 */
	acipc_writel_withdummy(IPC_ISRW, user_event);

	IPC_LEAVE();
	return ACIPC_RC_OK;
}

static enum acipc_return_code acipc_data_read(acipc_data *data)
{
	IPC_ENTER();
	/* reading the data from RDR */
	*data = acipc_readl(IPC_RDR);

	IPC_LEAVE();

	return ACIPC_RC_OK;
}

static enum acipc_return_code acipc_event_status_get(u32 user_event,
						     u32 *status)
{
	u32 IIR_local_val;

	IPC_ENTER();
	/* reading the status from IIR */
	ACIPC_IIR_READ(IIR_local_val);

	/* clear the events hw status */
	acipc_writel_withdummy(IPC_ICR, user_event);

	/*
	 * verify that this event will be cleared from the global IIR
	 * variable. for cases this API is called from user callback
	 */
	acipc->IIR_val &= ~(user_event);

	*status = IIR_local_val & user_event;

	IPC_LEAVE();
	return ACIPC_RC_OK;
}

static enum acipc_return_code acipc_event_bind(u32 user_event,
					       acipc_rec_event_callback cb,
					       enum acipc_callback_mode cb_mode,
					       u32 *historical_event_status)
{
	u32 i;

	IPC_ENTER();

	for (i = 0; i < ACIPC_NUMBER_OF_EVENTS; i++) {
		if (acipc->acipc_db.event_db[i].IIR_bit & user_event) {
			if (acipc->acipc_db.event_db[i].cb !=
			    acipc_default_callback)
				return ACIPC_EVENT_ALREADY_BIND;
			else {
				acipc->acipc_db.event_db[i].cb = cb;
				acipc->acipc_db.event_db[i].mode = cb_mode;
				acipc->acipc_db.event_db[i].mask = user_event &
				    acipc->acipc_db.event_db[i].IIR_bit;
				acipc_int_enable(acipc->acipc_db.event_db[i].
						 IIR_bit);
			}
		}
	}
	/* if there were historical events */
	if (acipc->acipc_db.historical_event_status & user_event) {
		if (historical_event_status)
			*historical_event_status = user_event &
			    acipc->acipc_db.historical_event_status;
		/* clear the historical events from the database */
		acipc->acipc_db.historical_event_status &= ~user_event;
		return ACIPC_HISTORICAL_EVENT_OCCUR;
	}

	IPC_LEAVE();
	return ACIPC_RC_OK;
}

static enum acipc_return_code acipc_event_unbind(u32 user_event)
{
	u32 i;

	IPC_ENTER();

	for (i = 0; i < ACIPC_NUMBER_OF_EVENTS; i++) {
		if (acipc->acipc_db.event_db[i].mask & user_event) {
			if (acipc->acipc_db.event_db[i].IIR_bit & user_event) {
				acipc_int_disable(acipc->acipc_db.event_db[i].
						  IIR_bit);
				acipc->acipc_db.event_db[i].cb =
				    acipc_default_callback;
				acipc->acipc_db.event_db[i].mode =
				    ACIPC_CB_NORMAL;
				acipc->acipc_db.event_db[i].mask = 0;
			}
			/* clean this event from other event's mask */
			acipc->acipc_db.event_db[i].mask &= ~user_event;
		}
	}

	IPC_LEAVE();
	return ACIPC_RC_OK;
}

static irqreturn_t acipc_interrupt_handler(int irq, void *dev_id)
{
	u32 i, on_events;

	IPC_ENTER();
	ACIPC_IIR_READ(acipc->IIR_val);	/* read the IIR */
	/* using ACIPCEventStatusGet might cause getting here with IIR=0 */
	if (acipc->IIR_val) {
		for (i = 0; i < ACIPC_NUMBER_OF_EVENTS; i++) {
			if ((acipc->acipc_db.event_db[i].IIR_bit &
			     acipc->IIR_val)
			    && (acipc->acipc_db.event_db[i].mode ==
				ACIPC_CB_NORMAL)) {
				on_events =
				    (acipc->IIR_val) & (acipc->acipc_db.
							event_db[i].mask);

				/* clean the event(s) */
				acipc_writel_withdummy(IPC_ICR, on_events);

				/*
				 * call the user callback with the status
				 * of other events as define when the user
				 * called ACIPCEventBind
				 */
				acipc->acipc_db.event_db[i].cb(on_events);

				/*
				 * if more then one event exist we clear
				 * the rest of the bits from the global
				 * IIR_val so user callback will be called
				 * only once.
				 */
				acipc->IIR_val &= (~on_events);
			}
		}
	}

	IPC_LEAVE();

	return IRQ_HANDLED;
}

static u32 user_callback(u32 events_status)
{
	acipc->bind_event_arg = events_status;
	acipc->poll_status = 1;
	wake_up_interruptible(&acipc->acipc_wait_q);

	return 0;
}

#if defined(CONFIG_PXA95x_DVFM)
struct acipc_lock {
	spinlock_t lock;
	int dev_idx;
	int ddr208_cnt;
	int ddr260_cnt;
	int init;
	unsigned long flags;
};

static struct acipc_lock acipc_lock = {
	.lock = __SPIN_LOCK_UNLOCKED(),
	.dev_idx = -1,
	.ddr208_cnt = 0,
	.ddr260_cnt = 0,
	.init = 0,
};

/*
 * The limitation is that constraint is added in initialization.
 * It will be removed at the first DDR request constraint.
 * Notice: At here, ddr208_cnt won't be updated.
 */
static void set_constraint(void)
{
	spin_lock_irqsave(&acipc_lock.lock, acipc_lock.flags);
	if (cpu_is_pxa95x())
		dvfm_disable_op_name("156M_HF", acipc_lock.dev_idx);

	dvfm_disable_op_name_no_change("D1", acipc_lock.dev_idx);
	dvfm_disable_op_name_no_change("D2", acipc_lock.dev_idx);
	acipc_lock.init = 0;
	spin_unlock_irqrestore(&acipc_lock.lock, acipc_lock.flags);
}

static unsigned int oscr_ts_1, oscr_ts_2;
struct workqueue_struct *acipc_wq;
struct work_struct acipc_ddr_hi_freq_acquire, acipc_ddr_hi_freq_release;

static void acipc_ddr_hi_freq_acquire_handler(struct work_struct *work)
{
	unsigned long flags;
	if (cpu_is_pxa95x())
		dvfm_enable_op_name("156M_HF", acipc_lock.dev_idx);

	dvfm_disable_op_name("156M", acipc_lock.dev_idx);
	/* MIPSRAM and ACIPC event should be atomic */
	local_irq_save(flags);
	acipc_event_set(ACIPC_DDR_260_READY_ACK);
	MIPS_RAM_ADD_PM_TRACE(ACIPC_HF_DDR_REQ_HANDHELD_MIPS_RAM);
	oscr_ts_2 = OSCR;
	local_irq_restore(flags);
	if ((oscr_ts_2 - oscr_ts_1) > MAX_ACIPC_REACT_TIME) {
		pr_warning("%s: constraint aquiring is slow %d cycles.\n",
			   __func__, oscr_ts_2 - oscr_ts_1);
		BUG_ON(1);
	}
}

static void acipc_ddr_hi_freq_release_handler(struct work_struct *work)
{
	dvfm_enable_op_name("156M", acipc_lock.dev_idx);
	if (cpu_is_pxa95x())
		/* 208MHz and 208+HF should be reconsidered by System eng */
		dvfm_disable_op_name("156M_HF", acipc_lock.dev_idx);

	MIPS_RAM_ADD_PM_TRACE(ACIPC_HF_DDR_REL_HANDHELD_MIPS_RAM);
}

static u32 acipc_kernel_callback(u32 events_status)
{
	unsigned int ReceivedData;
	IPC_ENTER();

	spin_lock_irqsave(&acipc_lock.lock, acipc_lock.flags);

	if (events_status & ACIPC_DDR_READY_REQ) {
		MIPS_RAM_ADD_PM_TRACE(ACIPC_DDR_REQ_RECEIVED_MIPS_RAM);
		if (acipc_lock.ddr208_cnt++ == 0) {
			dvfm_disable_op_name_no_change("D1",
						       acipc_lock.dev_idx);
			dvfm_disable_op_name_no_change("D2",
						       acipc_lock.dev_idx);
			MIPS_RAM_ADD_PM_TRACE(ACIPC_DVFM_CONSTRAITNS_SET);
		}
		if (acipc_lock.init == 0)
			acipc_lock.init = 1;
		acipc_event_set(ACIPC_DDR_READY_ACK);
		if (!ddr_avail_flag)
			set_DDR_avail_flag();
	}
	if (events_status & ACIPC_DDR_RELQ_REQ) {
		/*
		 * geeting the relative time form the comm
		 * side via ACIPC and notyfing dvfm when the
		 * next comm wakup will be.
		 */
		acipc_data_read(&ReceivedData);
		dvfm_notify_next_comm_wakeup_time(ReceivedData);
		MIPS_RAM_ADD_PM_TRACE(ACIPC_DDR_REL_RECEIVED_MIPS_RAM);
		if (acipc_lock.ddr208_cnt == 0)
			pr_warning("%s: constraint was removed.\n", __func__);
		else if (--acipc_lock.ddr208_cnt == 0) {
			dvfm_enable_op_name_no_change("D1", acipc_lock.dev_idx);
			dvfm_enable_op_name_no_change("D2", acipc_lock.dev_idx);
			MIPS_RAM_ADD_PM_TRACE(ACIPC_DVFM_CONSTRAITNS_RELEASED);
		}
	}
	if (events_status & ACIPC_DDR_260_READY_REQ) {
		MIPS_RAM_ADD_PM_TRACE(ACIPC_HF_DDR_REQ_RECEIVED_MIPS_RAM);
		if (acipc_lock.ddr260_cnt++ == 0) {
#if defined(CONFIG_PXA95x) || defined(CONFIG_PXA93x)
			oscr_ts_1 = OSCR;
#endif
			queue_work(acipc_wq, &acipc_ddr_hi_freq_acquire);
		} else {
			acipc_event_set(ACIPC_DDR_260_READY_ACK);
			MIPS_RAM_ADD_PM_TRACE
			    (ACIPC_HF_DDR_REQ_HANDHELD_MIPS_RAM);
		}
	}
	if (events_status & ACIPC_DDR_260_RELQ_REQ) {
		MIPS_RAM_ADD_PM_TRACE(ACIPC_HF_DDR_REL_RECEIVED_MIPS_RAM);
		acipc_event_set(ACIPC_DDR_260_RELQ_ACK);
		if (acipc_lock.ddr260_cnt == 0)
			pr_warning("%s: constraint was removed.\n", __func__);
		else if (--acipc_lock.ddr260_cnt == 0)
			queue_work(acipc_wq, &acipc_ddr_hi_freq_release);
		else
			MIPS_RAM_ADD_PM_TRACE
			    (ACIPC_HF_DDR_REL_HANDHELD_MIPS_RAM);
	}
	if (acipc_lock.ddr208_cnt || acipc_lock.ddr260_cnt)
		acipc_change_driver_state(1);
	else
		acipc_change_driver_state(0);
	spin_unlock_irqrestore(&acipc_lock.lock, acipc_lock.flags);

	IPC_LEAVE();
	return 0;
}

#else
static void set_constraint(void)
{
}

#if defined(CONFIG_PXA95x) || defined(CONFIG_PXA93x)
static u32 acipc_kernel_callback(u32 events_status)
{
	IPC_ENTER();

	if (events_status & ACIPC_DDR_READY_REQ) {
		acipc_event_set(ACIPC_DDR_READY_ACK);
		if (!ddr_avail_flag)
			set_DDR_avail_flag();
		/* Remove incorrect bit */
		if (events_status & ACIPC_DDR_RELQ_REQ)
			events_status &= ~ACIPC_DDR_RELQ_REQ;
		acipc_change_driver_state(1);
	}
	if (events_status & ACIPC_DDR_RELQ_REQ) {
		acipc_event_set(ACIPC_DDR_RELQ_ACK);
		acipc_change_driver_state(0);
	}
	if (events_status & ACIPC_DDR_260_READY_REQ) {
		acipc_event_set(ACIPC_DDR_260_READY_ACK);
		acipc_change_driver_state(1);
	}

	if (events_status & ACIPC_DDR_260_RELQ_REQ) {
		acipc_event_set(ACIPC_DDR_260_RELQ_ACK);
		acipc_change_driver_state(0);
	}
	IPC_LEAVE();
	return 0;
}
#endif /* CONFIG_CPU_PXA910 */
#endif /* CONFIG_PXA95X_DVFM */

#if defined(CONFIG_CPU_PXA910) || defined(CONFIG_CPU_PXA988)
static void register_pm_events(void)
{
	return;
}

#else
static void register_pm_events(void)
{
	acipc_event_bind(ACIPC_DDR_RELQ_REQ | ACIPC_DDR_260_RELQ_REQ |
			 ACIPC_DDR_260_READY_REQ | ACIPC_DDR_READY_REQ,
			 acipc_kernel_callback, ACIPC_CB_NORMAL,
			 &acipc->historical_events);
}
#endif /* CONFIG_CPU_PXA910 */

static long acipc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct acipc_ioctl_arg acipc_arg;
	u32 status;
	int ret = 0;

	IPC_ENTER();

	if (copy_from_user(&acipc_arg,
			   (void __user *)arg, sizeof(struct acipc_ioctl_arg)))
		return -EFAULT;

	switch (cmd) {
	case ACIPC_SET_EVENT:
		acipc_event_set(acipc_arg.set_event);
		break;
	case ACIPC_GET_EVENT:
		acipc_event_status_get(acipc_arg.arg, &status);
		acipc_arg.arg = status;
		break;
	case ACIPC_SEND_DATA:
		acipc_data_send(acipc_arg.set_event, acipc_arg.arg);
		break;
	case ACIPC_READ_DATA:
		acipc_data_read(&acipc_arg.arg);
		break;
	case ACIPC_BIND_EVENT:
		acipc_event_bind(acipc_arg.arg, user_callback,
				 acipc_arg.cb_mode, &status);
		acipc_arg.arg = status;
		break;
	case ACIPC_UNBIND_EVENT:
		acipc_event_unbind(acipc_arg.arg);
		break;
	case ACIPC_GET_BIND_EVENT_ARG:
		acipc_arg.arg = acipc->bind_event_arg;
		break;
	default:
		ret = -1;
		break;
	}

	if (copy_to_user((void __user *)arg, &acipc_arg,
			 sizeof(struct acipc_ioctl_arg)))
		return -EFAULT;

	IPC_LEAVE();

	return ret;
}

static unsigned int acipc_poll(struct file *file, poll_table *wait)
{
	IPC_ENTER();

	poll_wait(file, &acipc->acipc_wait_q, wait);

	IPC_LEAVE();

	if (acipc->poll_status == 0) {
		return 0;
	} else {
		acipc->poll_status = 0;
		return POLLIN | POLLRDNORM;
	}
}

static const struct file_operations acipc_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = acipc_ioctl,
	.poll = acipc_poll,
};

static struct miscdevice acipc_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "acipc",
	.fops = &acipc_fops,
};

static int __devinit pxa9xx_acipc_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret = -EINVAL, irq;
	char *irq_name;
	int i;

	ret = misc_register(&acipc_miscdev);
	if (ret < 0)
		return ret;

	acipc =
	    devm_kzalloc(&pdev->dev, sizeof(struct pxa9xx_acipc), GFP_KERNEL);
	if (!acipc) {
		ret = -ENOMEM;
		goto failed_deregmisc;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		ret = -ENOMEM;
		goto failed_deregmisc;
	}

	acipc->mmio_base = devm_request_and_ioremap(&pdev->dev, res);
	if (acipc->mmio_base == NULL) {
		ret = -ENODEV;
		goto failed_deregmisc;
	}
#if defined(CONFIG_CPU_PXA910) || defined(CONFIG_CPU_PXA988)
	set_constraint();
#endif
	/* init driver database */
	for (i = 0; i < ACIPC_NUMBER_OF_EVENTS; i++) {
#if defined(CONFIG_CPU_PXA910) || defined(CONFIG_CPU_PXA988)
		acipc->acipc_db.event_db[i].IIR_bit =
		    acipc_priority_table_dkb[i];
		acipc->acipc_db.event_db[i].mask = acipc_priority_table_dkb[i];
#endif

#if defined(CONFIG_PXA95x) || defined(CONFIG_PXA93x)
			acipc->acipc_db.event_db[i].IIR_bit =
			    acipc_priority_table[i];
			acipc->acipc_db.event_db[i].mask =
			    acipc_priority_table[i];
#endif
		acipc->acipc_db.event_db[i].cb = acipc_default_callback;
		acipc->acipc_db.event_db[i].mode = ACIPC_CB_NORMAL;
	}
	acipc->acipc_db.driver_mode = ACIPC_CB_NORMAL;
	acipc->acipc_db.int0_events_cnt = 0;

	init_waitqueue_head(&acipc->acipc_wait_q);
	spin_lock_init(&acipc->poll_lock);

	platform_set_drvdata(pdev, acipc);

	for (i = 0; i < ACIPC_NUMBER_OF_INTERRUPTS; i++) {
		irq = platform_get_irq(pdev, i);
		acipc->irq[i] = -1;
		irq_name = acipc->irq_name[i];
		memset(irq_name, 0, 20);
		sprintf(irq_name, "pxa9xx-ACIPC%d", i);
		if (irq >= 0) {
			ret = devm_request_irq(&pdev->dev, irq,
					       acipc_interrupt_handler,
					       IRQF_NO_SUSPEND | IRQF_DISABLED |
					       IRQF_TRIGGER_HIGH, irq_name,
					       acipc);
			disable_irq(irq);
		}
		if (irq < 0 || ret < 0) {
			ret = -ENXIO;
			goto failed_deregmisc;
		}
		acipc->irq[i] = irq;
	}
	register_pm_events();
	pr_info("pxa9xx AC-IPC initialized!\n");

#if defined(CONFIG_PXA95x) || defined(CONFIG_PXA93x)
	INIT_WORK(&acipc_ddr_hi_freq_acquire,
		  acipc_ddr_hi_freq_acquire_handler);
	INIT_WORK(&acipc_ddr_hi_freq_release,
		  acipc_ddr_hi_freq_release_handler);
	acipc_wq = alloc_workqueue("ACIPC_WQ", WQ_HIGHPRI, 0);
#endif

	return 0;

failed_deregmisc:
	misc_deregister(&acipc_miscdev);
	return ret;
}

static int __devexit pxa9xx_acipc_remove(struct platform_device *pdev)
{
	misc_deregister(&acipc_miscdev);
	return 0;
}

static struct platform_driver pxa9xx_acipc_driver = {
	.driver = {
		   .name = "pxa9xx-acipc",
		   },
	.probe = pxa9xx_acipc_probe,
	.remove = __devexit_p(pxa9xx_acipc_remove)
};

static int __init pxa9xx_acipc_init(void)
{

#ifdef CONFIG_PXA95x_DVFM
	dvfm_register("ACIPC", &acipc_lock.dev_idx);
#endif
	return platform_driver_register(&pxa9xx_acipc_driver);
}

static void __exit pxa9xx_acipc_exit(void)
{
	platform_driver_unregister(&pxa9xx_acipc_driver);
#ifdef CONFIG_PXA95x_DVFM
	dvfm_unregister("ACIPC", &acipc_lock.dev_idx);
#endif
}

#if defined(CONFIG_PXA95x) || defined(CONFIG_PXA93x)
int set_acipc_cp_enable(unsigned int pm_cp)
{
	internal_acipc_pm_cp = pm_cp;
	return 0;
}
EXPORT_SYMBOL(set_acipc_cp_enable);

u32 clear_DDR_avail_flag(void)
{
	unsigned long flags, IIR_val;
	/*
	 * if there's no CP acipc request should return without HW handling.
	 * SW returns expected value in order to avoid warnings
	 */
	if (!internal_acipc_pm_cp)
		return 0;

	local_irq_save(flags);

	/* this should never be true */
	if (ddr_avail_flag == 0) {
		pr_warning("****WARNING - attempting to clear ddr_avail_flag "\
			   "although it is allreday cleared - this should not"\
			   "occur!!!\n");
		local_irq_restore(flags);
		return ddr_avail_flag;	/* returning 0 */
	}

	/*
	 * In suspend, D2/CG will be entered without constraint checking.
	 * This is to make sure D2 can't be entered if DDR request is acquired.
	 */
	if (acipc_lock.ddr208_cnt > 0) {
		pr_debug("ACIPC has required DDR. Enter CG instead.\n");
		local_irq_restore(flags);
		return 1;
	}

	/* critical section ... */
	ACIPC_IIR_READ(IIR_val);
	if (!(IIR_val & ACIPC_DDR_READY_REQ)) {
		/*
		 * TODO - need to create a shadow for WDR
		 * register and implement clear and set Marcos
		 */
		acipc_writel(IPC_WDR, ACIPC_DDR_NOT_AVAIL);
		ddr_avail_flag = 0;

		udelay(5);	/* ACIPC propagation delay */
		/*
		 * verify that DDR request did not arrive
		 * after we clear the shared flag
		 */
		ACIPC_IIR_READ(IIR_val);
		if (IIR_val & ACIPC_DDR_READY_REQ) {
			pr_warning("******EDDR DDR_req arrive while trying to "\
				   "clear shared flag. should be rare\n");
			acipc_writel(IPC_WDR, ACIPC_DDR_AVAIL);
			/*
			 * restore shared DDR since comm
			 * DDR request was pending
			 */
			ddr_avail_flag = 1;
		}
	} else {
		if (ddr_avail_flag == 0)
			/*
			 * this can happen if ddr_req was pending while
			 * entering D0CS (not D2 because apps-comm sync) in
			 * that case comm is waiting for ddr_ack. since the
			 * PM state machine is not design to abort D0CS entry,
			 * in that case i rather ddr_req to take us out of
			 * D0CS. this behavior will not hurt the efficiency
			 * of this feature since we are setting the flag in
			 * D0CS exit this should happen only if ddr_request
			 * arrive between disable interrupt in the beginning
			 * of pxa3xx_set_op, and the call to
			 * clear_DDR_avail_flag so it is fairy rare case.
			 */
			pr_info("******EDDR DDR_req=1, shared flag=0 rare,"\
				"not risky\n");
		else
			/* this can happen if ddr_req was pending while
			 * entering D0CS (not D2 because apps-comm sync) in
			 * that case comm is *not* waiting for ddr_ack.
			 * returning the state of the flag, which in that case
			 * true, will prevent D0CS. this should happen only if
			 * ddr_request arrive between disable interrupt in the
			 * beginning of pxa3xx_set_op, and the call to
			 * clear_DDR_avail_flag so it is fairy rare case.
			 */
			pr_info("******EDDR DDR_req=1, shared flag=1 rare "\
				"and risky\n");
	}

	local_irq_restore(flags);

	return ddr_avail_flag;

}

#if defined(CONFIG_PXA95x_DVFM)
void acipc_set_constraint_no_op_change(void)
{
	dvfm_disable_op_name_no_change("D1", acipc_lock.dev_idx);
	dvfm_disable_op_name_no_change("D2", acipc_lock.dev_idx);
}

void acipc_unset_constraint_no_op_change(void)
{
	dvfm_enable_op_name_no_change("D1", acipc_lock.dev_idx);
	dvfm_enable_op_name_no_change("D2", acipc_lock.dev_idx);
}
#endif

void acipc_start_cp_constraints(void)
{
	set_constraint();
	set_DDR_avail_flag();
}
EXPORT_SYMBOL(acipc_start_cp_constraints);

u32 get_ddr_avail_state(void)
{
	return ddr_avail_flag;
}

u32 get_acipc_pending_events(void)
{
	u32 iir_val;
	ACIPC_IIR_READ(iir_val);
	return iir_val;
}

int acipc_handle_DDR_req_relq(void)
{
	int ret = 0;

	IPC_ENTER();

	ACIPC_IIR_READ(acipc->IIR_val);	/* read the IIR */

	/*
	 * If both events bit set, the only case happen here is
	 * ACIPC_DDR_RELQ_REQ and then ACIPC_DDR_READY_REQ, because
	 * ACIPC_DDR_RELQ_REQ will be sent by Comm side only the ACK for
	 * ACIPC_DDR_READY_REQ has been sent.
	 */
	if ((acipc->IIR_val & ACIPC_DDR_READY_REQ) &&
	    (acipc->IIR_val & ACIPC_DDR_RELQ_REQ)) {
		if (acipc_lock.ddr208_cnt == 0) {
			pr_err("Both DDR request and relinquish pending."\
			       "And previous event is relinquish."\
			       "This should not happen.\n");
			BUG_ON(1);
		}
	}

	if (ACIPC_DDR_RELQ_REQ & acipc->IIR_val) {
		pr_debug("ACIPC_DDR_RELQ_REQ happen.\n");
		/* Clean the event(s) and call callback */
		acipc_writel_withdummy(IPC_ICR, ACIPC_DDR_RELQ_REQ);
		acipc_kernel_callback(ACIPC_DDR_RELQ_REQ);
	}

	if (ACIPC_DDR_READY_REQ & acipc->IIR_val) {
		pr_debug("ACIPC_DDR_READY_REQ happen.\n");
		/* Clean the event(s) and call callback */
		acipc_writel_withdummy(IPC_ICR, ACIPC_DDR_READY_REQ);
		acipc_kernel_callback(ACIPC_DDR_READY_REQ);
	}

	/* If other events happen than DDR req and relq, start resume. */
	if (acipc->IIR_val & ~(ACIPC_DDR_READY_REQ | ACIPC_DDR_RELQ_REQ)) {
		pr_info("Other ACIPC events happen: 0x%08x, ", acipc->IIR_val);
		pr_info("Exit suspend.\n");
		ret = 1;
	}

	IPC_LEAVE();

	return ret;
}
EXPORT_SYMBOL(acipc_handle_DDR_req_relq);
#endif

enum acipc_return_code ACIPCEventBind(u32 user_event,
				      acipc_rec_event_callback cb,
				      enum acipc_callback_mode cb_mode,
				      u32 *historical_event_status)
{
	return acipc_event_bind(user_event, cb, cb_mode,
				historical_event_status);
}
EXPORT_SYMBOL(ACIPCEventBind);

enum acipc_return_code ACIPCEventUnBind(u32 user_event)
{
	return acipc_event_unbind(user_event);
}
EXPORT_SYMBOL(ACIPCEventUnBind);

enum acipc_return_code ACIPCEventSet(enum acipc_events user_event)
{
	return acipc_event_set(user_event);
}
EXPORT_SYMBOL(ACIPCEventSet);

enum acipc_return_code ACIPCDataSend(enum acipc_events user_event,
				     acipc_data data)
{
	return acipc_data_send(user_event, data);
}
EXPORT_SYMBOL(ACIPCDataSend);

enum acipc_return_code ACIPCDataRead(acipc_data *data)
{
	return acipc_data_read(data);
}
EXPORT_SYMBOL(ACIPCDataRead);

enum acipc_return_code ACIPCEventStatusGet(u32 userEvent, u32 *status)
{
	return acipc_event_status_get(userEvent, status);
}
EXPORT_SYMBOL(ACIPCEventStatusGet);

module_init(pxa9xx_acipc_init);
module_exit(pxa9xx_acipc_exit);
MODULE_AUTHOR("MARVELL");
MODULE_DESCRIPTION("AC-IPC driver");
MODULE_LICENSE("GPL");
