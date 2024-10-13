// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 Samsung Electronics.
 *
 */

#include <linux/string.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <soc/samsung/exynos-modem-ctrl.h>
#if IS_ENABLED(CONFIG_EXYNOS_ESCAV2)
#include <soc/samsung/esca.h>
#else
#include <soc/samsung/acpm_ipc_ctrl.h>
#endif
#include <soc/samsung/exynos-bcm_dbg.h>

#include "modem_prj.h"
#include "modem_utils.h"
#include "cpif_version.h"

enum bit_debug_flags {
	DEBUG_FLAG_FMT,
	DEBUG_FLAG_MISC,
	DEBUG_FLAG_RFS,
	DEBUG_FLAG_PS,
	DEBUG_FLAG_BOOT,
	DEBUG_FLAG_DUMP,
	DEBUG_FLAG_CSVT,
	DEBUG_FLAG_LOG,
	DEBUG_FLAG_BT_DUN, /* for rx/tx of umts_router */
	DEBUG_FLAG_ALL
};

#define DEBUG_FLAG_DEFAULT    (1 << DEBUG_FLAG_FMT | 1 << DEBUG_FLAG_MISC)

static unsigned long dflags = (DEBUG_FLAG_DEFAULT);

module_param(dflags, ulong, 0664);
MODULE_PARM_DESC(dflags, "modem_v1 debug flags");

static unsigned long wakeup_dflags =
		(DEBUG_FLAG_DEFAULT | 1 << DEBUG_FLAG_RFS | 1 << DEBUG_FLAG_PS);
module_param(wakeup_dflags, ulong, 0664);
MODULE_PARM_DESC(wakeup_dflags, "modem_v1 wakeup debug flags");

static bool wakeup_log_enable;
inline void set_wakeup_packet_log(bool enable)
{
	wakeup_log_enable = enable;
}

inline unsigned long get_log_flags(void)
{
	return wakeup_log_enable ? wakeup_dflags : dflags;
}

static inline bool log_enabled(struct sk_buff *skb)
{
	struct io_device *iod = skbpriv(skb)->iod;
	struct link_device *ld = skbpriv(skb)->ld;
	unsigned long flags = get_log_flags();
	u8 ch = iod->ch;

	if (is_fmt_iod(iod))
		return test_bit(DEBUG_FLAG_FMT, &flags);
	else if (is_boot_iod(iod))
		return test_bit(DEBUG_FLAG_BOOT, &flags);
	else if (is_dump_iod(iod))
		return test_bit(DEBUG_FLAG_DUMP, &flags);
	else if (is_rfs_iod(iod))
		return test_bit(DEBUG_FLAG_RFS, &flags);
	else if (ld->is_csd_ch && ld->is_csd_ch(ch))
		return test_bit(DEBUG_FLAG_CSVT, &flags);
	else if (ld->is_log_ch && ld->is_log_ch(ch))
		return test_bit(DEBUG_FLAG_LOG, &flags);
	else if (is_ps_iod(iod))
		return test_bit(DEBUG_FLAG_PS, &flags);
	else if (ld->is_router_ch && ld->is_router_ch(ch))
		return test_bit(DEBUG_FLAG_BT_DUN, &flags);
	else if (ld->is_misc_ch && ld->is_misc_ch(ch))
		return test_bit(DEBUG_FLAG_MISC, &flags);
	else
		return test_bit(DEBUG_FLAG_ALL, &flags);
}

/* print ipc packet */
void mif_pkt(const char *tag, struct sk_buff *skb)
{
	if (!skbpriv(skb)->ld)
		return;

	if (!log_enabled(skb))
		return;

	if (unlikely(!skb)) {
		mif_err("ERR! NO skb!!!\n");
		return;
	}

	pr_skb(tag, skb, skbpriv(skb)->ld);
}

/* print buffer as hex string */
int pr_buffer(const char *tag, const char *data, size_t data_len,
							size_t max_len)
{
	size_t len = min(data_len, max_len);
	unsigned char str[PR_BUFFER_SIZE * 3]; /* 1 <= sizeof <= max_len*3 */

	if (len > PR_BUFFER_SIZE)
		len = PR_BUFFER_SIZE;

	dump2hex(str, (len ? len * 3 : 1), data, len);

	/* don't change this printk to mif_debug for print this as level7 */
	return pr_info("%s: %s(%ld): %s%s\n", MIF_TAG, tag, (long)data_len,
			str, (len == data_len) ? "" : " ...");
}

void insert_iod_with_channel(struct modem_shared *msd, unsigned int channel,
			     struct io_device *iod)
{
	unsigned int idx = msd->num_channels;

	msd->ch2iod[channel] = iod;
	msd->ch[idx] = channel;
	msd->num_channels++;
}

/* netif wake/stop queue of iod having activated ndev */
static void netif_tx_flowctl(struct modem_shared *msd, bool tx_stop)
{
	struct io_device *iod;
	unsigned long flags;

	if (!msd) {
		mif_err_limited("modem shared data does not exist\n");
		return;
	}

	spin_lock_irqsave(&msd->active_list_lock, flags);
	list_for_each_entry(iod, &msd->activated_ndev_list, node_ndev) {
		if (tx_stop)
			netif_stop_subqueue(iod->ndev, 0);
		else
			netif_wake_subqueue(iod->ndev, 0);
	}
	spin_unlock_irqrestore(&msd->active_list_lock, flags);
}

bool stop_net_ifaces(struct link_device *ld, unsigned long set_mask)
{
	bool ret = false;

	if (set_mask > 0)
		cpif_set_bit(ld->tx_flowctrl_mask, set_mask);

	if (!atomic_read(&ld->netif_stopped)) {
		mif_info("Normal netif txq stopped: flowctrl=0x%04lx(set_bit:%lu) dup_res:%u\n",
			 ld->tx_flowctrl_mask, set_mask, ld->dup_count_resume);

		netif_tx_flowctl(ld->msd, true);
		atomic_set(&ld->netif_stopped, 1);
		ret = true;
	}

	return ret;
}

void resume_net_ifaces(struct link_device *ld, unsigned long clear_mask)
{
	cpif_clear_bit(ld->tx_flowctrl_mask, clear_mask);

	if (!ld->tx_flowctrl_mask && atomic_read(&ld->netif_stopped)) {
		mif_info("Normal netif txq resumed: flowctrl=0x%04lx(clear_bit:%lu) dup_sus:%u\n",
			 ld->tx_flowctrl_mask, clear_mask, ld->dup_count_suspend);

		netif_tx_flowctl(ld->msd, false);
		atomic_set(&ld->netif_stopped, 0);
	}
}

void mif_add_timer(struct timer_list *timer, unsigned long expire,
			void (*function)(struct timer_list *))
{
	if (timer_pending(timer))
		return;

	timer_setup(timer, function, 0);
	timer->expires = get_jiffies_64() + expire;

	add_timer(timer);
}

void mif_init_irq(struct modem_irq *irq, unsigned int num, const char *name,
		  unsigned long flags)
{
	spin_lock_init(&irq->lock);
	irq->num = num;
	strncpy(irq->name, name, (MAX_NAME_LEN - 1));
	irq->flags = flags;
	mif_info("name:%s num:%d flags:0x%08lX\n", name, num, flags);
}

int mif_request_irq(struct modem_irq *irq, irq_handler_t isr, void *data)
{
	int ret;

	ret = request_irq(irq->num, isr, irq->flags, irq->name, data);
	if (ret) {
		mif_err("%s: ERR! request_irq fail (%d)\n", irq->name, ret);
		return ret;
	}

	enable_irq_wake(irq->num);
	irq->active = true;
	irq->registered = true;

	mif_info("%s(#%d) handler registered (flags:0x%08lX)\n",
		irq->name, irq->num, irq->flags);

	return 0;
}

void mif_enable_irq(struct modem_irq *irq)
{
	unsigned long flags;

	if (irq->registered == false)
		return;

	spin_lock_irqsave(&irq->lock, flags);

	if (irq->active) {
		mif_err("%s(#%d) is already active <%ps>\n", irq->name, irq->num, CALLER);
		goto exit;
	}

	enable_irq(irq->num);
	enable_irq_wake(irq->num);

	irq->active = true;

	mif_info("%s(#%d) is enabled <%ps>\n", irq->name, irq->num, CALLER);

exit:
	spin_unlock_irqrestore(&irq->lock, flags);
}

void mif_disable_irq(struct modem_irq *irq)
{
	unsigned long flags;

	if (irq->registered == false)
		return;

	spin_lock_irqsave(&irq->lock, flags);

	if (!irq->active) {
		mif_info("%s(#%d) is not active <%ps>\n", irq->name, irq->num, CALLER);
		goto exit;
	}

	disable_irq_nosync(irq->num);
	disable_irq_wake(irq->num);

	irq->active = false;

	mif_info("%s(#%d) is disabled <%ps>\n", irq->name, irq->num, CALLER);

exit:
	spin_unlock_irqrestore(&irq->lock, flags);
}

bool mif_gpio_set_value(struct cpif_gpio *gpio, int value, unsigned int delay_ms)
{
	int dup = 0;

	if (!gpio->valid) {
		mif_err("SET GPIO %d is not valid\n", gpio->num);
		return false;
	}

	if (gpio_get_value(gpio->num) == value)
		dup = 1;

	/* set gpio even if it is set already */
	gpio_set_value(gpio->num, value);

	mif_info("SET GPIO %s = %d (wait %dms, dup %d)\n", gpio->label, value, delay_ms, dup);

	if (delay_ms > 0 && !dup) {
		if (in_interrupt() || irqs_disabled())
			mdelay(delay_ms);
		else if (delay_ms < 20)
			usleep_range(delay_ms * 1000, (delay_ms * 1000) + 5000);
		else
			msleep(delay_ms);
	}

	return (!dup);
}
EXPORT_SYMBOL(mif_gpio_set_value);

int mif_gpio_get_value(struct cpif_gpio *gpio, bool log_print)
{
	int value;

	if (!gpio->valid) {
		mif_err("GET GPIO %d is not valid\n", gpio->num);
		return -EINVAL;
	}

	value = gpio_get_value(gpio->num);

	if (log_print)
		mif_info("GET GPIO %s = %d\n", gpio->label, value);

	return value;
}
EXPORT_SYMBOL(mif_gpio_get_value);

int mif_gpio_toggle_value(struct cpif_gpio *gpio, int delay_ms)
{
	int value;

	value = mif_gpio_get_value(gpio, false);
	mif_gpio_set_value(gpio, !value, delay_ms);
	mif_gpio_set_value(gpio, value, 0);

	return value;
}
EXPORT_SYMBOL(mif_gpio_toggle_value);

void mif_stop_logging(void)
{
#if IS_ENABLED(CONFIG_EXYNOS_ESCAV2)
	esca_stop_log();
#else
	acpm_stop_log();
#endif
}

void mif_stop_ap_ppmu_logging(void)
{
	mif_info("stop ap ppmu logging");
	exynos_bcm_dbg_stop(0);
}

const char *get_cpif_driver_version(void)
{
	return &(cpif_driver_version[0]);
}

int copy_from_user_memcpy_toio(void __iomem *dst, const void __user *src, size_t count)
{
	u8 buf[256];

	while (count) {
		size_t c = count;

		if (c > sizeof(buf))
			c = sizeof(buf);
		if (copy_from_user(buf, src, c))
			return -EFAULT;

		memcpy_toio(dst, buf, c);
		count -= c;
		dst += c;
		src += c;
	}

	return 0;
}

