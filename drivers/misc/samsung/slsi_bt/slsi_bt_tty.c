/******************************************************************************
 *                                                                            *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * S.LSI BT TTY Driver                                                        *
 *                                                                            *
 ******************************************************************************/
#include <linux/module.h>

#include <linux/skbuff.h>
#include <linux/mutex.h>
#include <linux/tty.h>

#include "hci_trans.h"
#include "hci_pkt.h"
#include "hci_h4.h"
#include "slsi_bt_tty.h"
#include "slsi_bt_log.h"

#define N_SLSI_BT       (N_NULL + 1)

struct {
	struct mutex                   lock;
	struct tty_struct              *tty;
	struct hci_trans               trans;
	unsigned int                   baud;
} bt_tty;

struct {
	struct mutex                   lock;
	struct sk_buff                 *recv_skb;
} bt_ldisc;

/*
 * slsi BT ldisc
 */
static int slsi_bt_ldisc_open(struct tty_struct *tty)
{
	tty->disc_data = NULL;
	tty->receive_room = 65536;

	mutex_lock(&bt_ldisc.lock);
	bt_ldisc.recv_skb = NULL;
	mutex_unlock(&bt_ldisc.lock);
	return 0;
}

static void slsi_bt_ldisc_close(struct tty_struct *tty)
{
	mutex_lock(&bt_ldisc.lock);
	if (bt_ldisc.recv_skb)
		kfree_skb(bt_ldisc.recv_skb);

	bt_ldisc.recv_skb = NULL;
	mutex_unlock(&bt_ldisc.lock);

	tty->disc_data = NULL;
}

static void slsi_bt_ldisc_receive(struct tty_struct *tty, const unsigned char *data,
				 const char *fp, int count)
{
	int offset = 0, ret;
	BT_INFO("count: %d\n", count);

	mutex_lock(&bt_ldisc.lock);
	if (bt_tty.trans.host != NULL && bt_tty.trans.host->recv != NULL) {
		while (count > 0) {
			ret = bt_tty.trans.host->recv(bt_tty.trans.host,
				data + offset, count, 0);

			/* In the case of 1byte (c0), ret can be 0. In this
			 * case, it has already been copied */
			if (ret <= 0)
				break;

			offset += ret;
			count -= ret;
		}
	} else {
		BT_WARNING("%s does not have host or host->recv\n",
			bt_tty.trans.name);
	}
	mutex_unlock(&bt_ldisc.lock);
}

/* Linux version compatibility */
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(5, 10, 0))
#define tty_kopen_exclusive            tty_kopen
#define comp_tty_register_ldisc(ldisc)         tty_register_ldisc(N_SLSI_BT, (ldisc))
#define comp_tty_unregister_ldisc(arg)         tty_unregister_ldisc(N_SLSI_BT)
#else
#define comp_tty_register_ldisc(ldisc)         tty_register_ldisc((ldisc))
#define comp_tty_unregister_ldisc(arg)         tty_unregister_ldisc((arg))
#endif

static struct tty_ldisc_ops slsi_bt_ldisc = {
	.owner          = THIS_MODULE,
	.num            = N_SLSI_BT,
	.name           = "n_slsi_bt_hci",
	.open           = slsi_bt_ldisc_open,
	.close          = slsi_bt_ldisc_close,
	.receive_buf    = slsi_bt_ldisc_receive,
};

/*
 * slsi BT tty
 */
static int slsi_bt_tty_send_skb(struct hci_trans *htr, struct sk_buff *skb)
{
	size_t ret, count = 0;

	if (htr == NULL || skb == NULL)
		return -EINVAL;

	if (!bt_tty.tty || !bt_tty.tty->ops->write) {
		BT_ERR("TTY is not initialized.\n");
		return -EINVAL;
	}

	while (skb->len > 0) {
		ret = bt_tty.tty->ops->write(bt_tty.tty, skb->data, skb->len);
		if (ret < 0) {
			BT_ERR("tty write fail\n");
			kfree_skb(skb);
			return ret;
		}

		count += ret;
		if (skb_pull(skb, ret) == NULL)
			break;
	}
	kfree_skb(skb);

	BT_DBG("Send %zu bytes to tty\n", count);
	return 0;
}

static void slsi_bt_tty_set_baudrate(unsigned int baud)
{
	struct ktermios ti;

	if (bt_tty.tty == NULL)
		return;

	BT_INFO("baud rate: %d\n", baud);
	bt_tty.baud = baud;

	ti = bt_tty.tty->termios;
	ti.c_cflag &= ~CBAUD;
	ti.c_cflag |= BOTHER | CRTSCTS;
	ti.c_ispeed = baud;
	ti.c_ospeed = baud;

	tty_set_termios(bt_tty.tty, &ti);
}

unsigned int dev_host_uart_baudrate = SLSI_BT_TTY_BAUD;
module_param(dev_host_uart_baudrate, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dev_host_uart_baudrate, "UART baud rate");

int slsi_bt_tty_open(struct hci_trans **htr)
{
	dev_t dev;
	struct tty_struct *tty;
	int ret = 0;

	BT_INFO("dev_name: %s\n", SLSI_BT_TTY_DEV_NAME);

	ret = tty_dev_name_to_number(SLSI_BT_TTY_DEV_NAME, &dev);
	if (ret) {
		BT_ERR("ttydev_name_to_number is failed (%d\n)", ret);
		return ret;
	}

	tty = tty_kopen_exclusive(dev);
	if (IS_ERR(tty)) {
		BT_ERR("tty_kopen is failed\n");
		return PTR_ERR(tty);
	}
	if (tty->ops->open)
		ret = tty->ops->open(tty, NULL);
	else
		ret = -ENODEV;

	tty_unlock(tty);
	if (ret) {
		BT_ERR("tty open is failed (%d)\n", ret);
		return ret;
	}

	ret = tty_set_ldisc(tty, N_SLSI_BT);
	if (ret) {
		BT_ERR("tty_set_ldisc (%d) is failed (%d)\n", N_SLSI_BT, ret);
		tty_lock(tty);
		if (tty->ops->close)
			tty->ops->close(tty, NULL);
		tty_unlock(tty);
		tty_kclose(tty);
		return ret;
	}
	tty->ops->flush_chars(tty);

	mutex_lock(&bt_tty.lock);
	bt_tty.tty = tty;
	slsi_bt_tty_set_baudrate(dev_host_uart_baudrate);
	mutex_unlock(&bt_tty.lock);

	hci_trans_init(&bt_tty.trans, "tty Transport");
	bt_tty.trans.send_skb = slsi_bt_tty_send_skb;
	*htr = &bt_tty.trans;

	BT_INFO("tty=%p\n", tty);
	return 0;
}

void slsi_bt_tty_close(void)
{
	if (!bt_tty.tty)
		return;

	hci_trans_deinit(&bt_tty.trans);
	if (bt_tty.tty->ops->close)
		bt_tty.tty->ops->close(bt_tty.tty, NULL);
	tty_kclose(bt_tty.tty);

	mutex_lock(&bt_tty.lock);
	bt_tty.tty = NULL;
	mutex_unlock(&bt_tty.lock);
}

long slsi_bt_tty_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return tty_mode_ioctl(bt_tty.tty, file, cmd, arg);
}

int slsi_bt_tty_init(void)
{
	int err = 0;

	memset(&bt_tty, 0, sizeof(bt_tty));
	memset(&bt_ldisc, 0, sizeof(bt_ldisc));

	mutex_init(&bt_tty.lock);
	mutex_init(&bt_ldisc.lock);

	/* Register the tty discipline */
	err = comp_tty_register_ldisc(&slsi_bt_ldisc);
	if (err) {
		BT_ERR("SLSI BT ldisc registration failed. (%d)\n", err);
		return err;
	}
	BT_INFO("done.\n", err);

	return err;
}

void slsi_bt_tty_exit(void)
{
	slsi_bt_tty_close();
	comp_tty_unregister_ldisc(&slsi_bt_ldisc);

	mutex_destroy(&bt_tty.lock);
	mutex_destroy(&bt_ldisc.lock);
}
