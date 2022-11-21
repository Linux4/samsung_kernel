
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/platform_data/mv_usb.h>

struct pxa_usb_extern_dev {
	unsigned int id;
	struct pxa_usb_extern_ops ops;
	struct atomic_notifier_head *head;
};

static struct pxa_usb_extern_dev pxa_usb[PXA_USB_DEV_MAX];

struct pxa_usb_extern_ops *pxa_usb_get_extern_ops(unsigned int id)
{
	if (id >= PXA_USB_DEV_MAX)
		return NULL;

	return &pxa_usb[id].ops;
}

int pxa_usb_register_notifier(unsigned int id, struct notifier_block *nb)
{
	struct pxa_usb_extern_dev *dev;
	int ret;

	if (id >= PXA_USB_DEV_MAX)
		return -ENODEV;

	dev = &pxa_usb[id];
	if (dev->head == NULL) {
		dev->head = kzalloc(sizeof(*dev->head), GFP_KERNEL);
		if (dev->head == NULL)
			return -ENOMEM;
		ATOMIC_INIT_NOTIFIER_HEAD(dev->head);
	}

	ret = atomic_notifier_chain_register(dev->head, nb);
	if (ret)
		return ret;

	return 0;
}

int pxa_usb_unregister_notifier(unsigned int id, struct notifier_block *nb)
{
	struct pxa_usb_extern_dev *dev;
	int ret;

	if (id >= PXA_USB_DEV_MAX)
		return -ENODEV;

	dev = &pxa_usb[id];
	if (dev->head == NULL)
		return -EINVAL;

	ret = atomic_notifier_chain_unregister(dev->head, nb);
	if (ret)
		return ret;

	return 0;
}

int pxa_usb_notify(unsigned int id, unsigned long val, void *v)
{
	struct pxa_usb_extern_dev *dev;
	int ret;

	if (id >= PXA_USB_DEV_MAX)
		return -ENODEV;

	dev = &pxa_usb[id];
	if (dev->head == NULL)
		return -EINVAL;

	ret = atomic_notifier_call_chain(dev->head, val, v);
	if (ret)
		return ret;

	return 0;
}
