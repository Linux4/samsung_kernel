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
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/sched.h>

#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/pxa9xx_acipc.h>

struct pxa9xx_acipc {
	struct resource *mem;
	void __iomem *mmio_base;
	int irq[ACIPC_MAX_INTERRUPTS];
	char irq_name[ACIPC_MAX_INTERRUPTS][20];
	u32 irq_number;
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
static u32 acipc_int0_events;
static u32 acipc_int1_events;
static u32 acipc_int2_events;
/* PXA930/PXA910 ACIPC registers */
#define IPC_WDR		0x0004
#define IPC_ISRW	0x0008
#define IPC_ICR		0x000C
#define IPC_IIR		0x0010
#define IPC_RDR		0x0014

#define acipc_readl(off)	__raw_readl(acipc->mmio_base + (off))
#define acipc_writel(off, v)	__raw_writel((v), acipc->mmio_base + (off))


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

#define acipc_writel_withdummy(off, v)	acipc_writel((off), (v))

/* read IIR */
#define ACIPC_IIR_READ(IIR_val)			\
{								\
	/* dummy write to latch the IIR value */	\
	acipc_writel(IPC_IIR, 0x0);		\
	barrier();				\
	(IIR_val) = acipc_readl(IPC_IIR);	\
}

static int acipc_set_global_value(void)
{
	int ret = 0;

	switch (acipc->irq_number) {
	case 1:
		acipc_int0_events = 0x3ff;
		acipc_int1_events = ACIPC_SPARE;
		acipc_int2_events = ACIPC_SPARE;
		break;
	case 3:
		acipc_int0_events = 0xff;
		acipc_int1_events = ACIPC_SHM_PACKET_NOTIFY;
		acipc_int2_events = ACIPC_IPM;
		break;
	default:
		pr_err("invalid acipc->irq_number %u\n", acipc->irq_number);
		ret = -1;
		break;
	}

	return ret;
}

u32 acipc_irq_count(struct device_node *dev)
{
	u32 nr = 0;

	while (of_irq_to_resource(dev, nr, NULL))
		nr++;

	return nr;
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

	if (event & acipc_int0_events) {
		/*
		 * for the first 8 bits we enable the INTC_AC_IPC_0
		 * only if this the first event that has been binded
		 */
		if (!(acipc->acipc_db.int0_events_cnt))
			enable_irq(acipc->irq[0]);

		acipc->acipc_db.int0_events_cnt |= event;
		return;
	}
	if (event & acipc_int1_events) {
		enable_irq(acipc->irq[1]);
		return;
	}
	if (event & acipc_int2_events) {
		enable_irq(acipc->irq[2]);
		return;
	}
}

static void acipc_int_disable(enum acipc_events event)
{
	IPC_ENTER();

	IPCTRACE("acipc_int_disable event 0x%x\n", event);
	if (event & acipc_int0_events) {
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
	if (event & acipc_int1_events) {
		disable_irq(acipc->irq[1]);
		return;
	}
	if (event & acipc_int2_events) {
		disable_irq(acipc->irq[2]);
		return;
	}
}

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

static int pxa9xx_acipc_probe(struct platform_device *pdev)
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

	acipc->irq_number = acipc_irq_count(pdev->dev.of_node);

	ret = acipc_set_global_value();

	if (ret)
		goto failed_deregmisc;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		ret = -ENOMEM;
		goto failed_deregmisc;
	}

	acipc->mmio_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(acipc->mmio_base)) {
		ret = (int)PTR_ERR(acipc->mmio_base);
		goto failed_deregmisc;
	}

	/* init driver database */
	for (i = 0; i < ACIPC_NUMBER_OF_EVENTS; i++) {
		acipc->acipc_db.event_db[i].IIR_bit =
		    acipc_priority_table_dkb[i];
		acipc->acipc_db.event_db[i].mask = acipc_priority_table_dkb[i];

		acipc->acipc_db.event_db[i].cb = acipc_default_callback;
		acipc->acipc_db.event_db[i].mode = ACIPC_CB_NORMAL;
	}
	acipc->acipc_db.driver_mode = ACIPC_CB_NORMAL;
	acipc->acipc_db.int0_events_cnt = 0;

	init_waitqueue_head(&acipc->acipc_wait_q);
	spin_lock_init(&acipc->poll_lock);

	platform_set_drvdata(pdev, acipc);

	for (i = 0; i < ACIPC_MAX_INTERRUPTS; i++) {
		acipc->irq[i] = -1;
		memset(acipc->irq_name[i], 0, 20);
	}

	for (i = 0; i < acipc->irq_number; i++) {
		irq = platform_get_irq(pdev, i);
		irq_name = acipc->irq_name[i];
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
	pr_info("pxa9xx AC-IPC initialized!\n");

	return 0;

failed_deregmisc:
	misc_deregister(&acipc_miscdev);
	return ret;
}

static int pxa9xx_acipc_remove(struct platform_device *pdev)
{
	misc_deregister(&acipc_miscdev);
	return 0;
}

static struct of_device_id pxa9xx_acipc_dt_ids[] = {
	{ .compatible = "marvell,mmp-acipc", },
	{ .compatible = "marvell,pxa-acipc", },
	{}
};

static struct platform_driver pxa9xx_acipc_driver = {
	.driver = {
		   .name = "pxa9xx-acipc",
		   .of_match_table = pxa9xx_acipc_dt_ids,
		   },
	.probe = pxa9xx_acipc_probe,
	.remove = pxa9xx_acipc_remove
};

static int __init pxa9xx_acipc_init(void)
{
	return platform_driver_register(&pxa9xx_acipc_driver);
}

static void __exit pxa9xx_acipc_exit(void)
{
	platform_driver_unregister(&pxa9xx_acipc_driver);
}

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
