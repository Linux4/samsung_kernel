/******************************************************************************
* Copyright (C) 2013 Broadcom Corporation
*
* @file   /kernel/drivers/misc/bcm-bt-lpm.c
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation version 2.
*
* This program is distributed "as is" WITHOUT ANY WARRANTY of any
* kind, whether express or implied; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
******************************************************************************/
/*
 * Broadcom bluetooth low power mode driver.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#include <linux/gpio.h>
#include <linux/broadcom/bcm-bt-lpm.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <linux/uaccess.h>
#include <linux/serial_core.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/pm_wakeup.h>
#include <linux/irq.h>
#include <linux/wakelock.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>

#define N_BRCM_HCI 25

#ifndef BCM_BT_LPM_BT_WAKE_ASSERT
#define BCM_BT_LPM_BT_WAKE_ASSERT 0
#endif

#ifndef BCM_BT_LPM_BT_WAKE_DEASSERT
#define BCM_BT_LPM_BT_WAKE_DEASSERT (!(BCM_BT_LPM_BT_WAKE_ASSERT))
#endif

#ifndef BCM_BT_LPM_HOST_WAKE_ASSERT
#define BCM_BT_LPM_HOST_WAKE_ASSERT 0
#endif
#ifndef BCM_BT_LPM_HOST_WAKE_DEASSERT
#define BCM_BT_LPM_HOST_WAKE_DEASSERT (!(BCM_BT_LPM_HOST_WAKE_ASSERT))
#endif

#define BCM_SHARED_UART_MAGIC	0x80
#define TIO_ASSERT_BT_WAKE	_IO(BCM_SHARED_UART_MAGIC, 3)
#define TIO_DEASSERT_BT_WAKE	_IO(BCM_SHARED_UART_MAGIC, 4)
#define TIO_GET_BT_WAKE_STATE	_IO(BCM_SHARED_UART_MAGIC, 5)

struct bcm_bt_lpm_struct {
	spinlock_t bcm_bt_lpm_lock;
	struct uart_port *uport;
	int host_irq;
};

struct bcm_bt_lpm_entry_struct {
	struct bcm_bt_lpm_platform_data *pdata;
	struct bcm_bt_lpm_struct *plpm;
	struct platform_device *pdev;
        struct wake_lock bt_wakelock;
        struct wake_lock host_wakelock;
};

struct bcm_bt_lpm_entry_struct *priv_g;

struct bcm_bt_lpm_ldisc_data {
	int	(*open)(struct tty_struct *);
	void	(*close)(struct tty_struct *);
};

static struct tty_ldisc_ops bcm_bt_lpm_ldisc_ops;
static struct bcm_bt_lpm_ldisc_data bcm_bt_lpm_ldisc_saved;

int bcm_bt_lpm_assert_bt_wake(void)
{
	pr_info("%s now BT_WAKE:%s\n", __func__,
		gpio_get_value(priv_g->pdata->bt_wake_gpio) ? "High [POWER ON]" :
		"Low [POWER_OFF]");
	if (priv_g == NULL) {
		pr_err(
		"%s BLUETOOTH:data corrupted: cannot assert bt_wake\n",
				__func__);
		return -EFAULT;
	}
	if (unlikely((priv_g->pdata->bt_wake_gpio == -1)))
		return -EFAULT;
        wake_lock(&priv_g->bt_wakelock);
//	wake_unlock(&priv_g->bt_wakelock);

	gpio_set_value(priv_g->pdata->bt_wake_gpio,
					BCM_BT_LPM_BT_WAKE_ASSERT);
	pr_info("moonys -----bcm_bt_lpm_assert_bt_wake:%s\n",
		gpio_get_value(priv_g->pdata->bt_wake_gpio) ? "High [POWER ON]" :
		"Low [POWER_OFF]");
	return 0;
}

int bcm_bt_lpm_deassert_bt_wake(void)
{
	if (priv_g == NULL) {
		pr_err(
		"%s BLUETOOTH:data corrupted:cannot de-assert bt_wake\n",
						__func__);
		return -EFAULT;
	}
	if (unlikely((priv_g->pdata->bt_wake_gpio == -1)))
		return -EFAULT;
	gpio_set_value(priv_g->pdata->bt_wake_gpio,
					BCM_BT_LPM_BT_WAKE_DEASSERT);
	pr_info("moonys -----bcm_bt_lpm_deassert_bt_wake: value %d %s\n",priv_g->pdata->bt_wake_gpio,
		gpio_get_value(priv_g->pdata->bt_wake_gpio) ? "High [POWER ON]" :
		"Low [POWER_OFF]");
        wake_unlock(&priv_g->bt_wakelock);
	return 0;
}

static int bcm_bt_lpm_init_bt_wake(
				struct bcm_bt_lpm_entry_struct *priv)
{
	int rc = 0;

	printk("%s BLUETOOTH:Enter.\n", __func__);

	if (priv->pdata->bt_wake_gpio < 0) {
		pr_err("%s: Exiting => invalid bt-wake-gpio=%d\n",
		__func__, priv->pdata->bt_wake_gpio);
		return -EFAULT;
	}
	rc = devm_gpio_request_one(&priv->pdev->dev,
			priv->pdata->bt_wake_gpio,
			GPIOF_OUT_INIT_LOW,
			"bt_wake_gpio");

	if (rc) {
		pr_err
	("%s: failed to init bt_wake: bt_wake_gpio request err%d\n",
		     __func__, rc);
		return rc;
	}

	rc = gpio_direction_output(
			priv->pdata->bt_wake_gpio,
			BCM_BT_LPM_BT_WAKE_DEASSERT);

	printk("%s BLUETOOTH:Exit.\n", __func__);
	return 0;
}

static void bcm_bt_lpm_tty_cleanup(void)
{
	int err;

	printk("%s BLUETOOTH: Entering.\n", __func__);
	err = tty_unregister_ldisc(N_BRCM_HCI);
	if (err)
		pr_err(
		"can't unregister N_BRCM_HCI line discipline\n");
	else
		pr_info("N_BRCM_HCI line discipline removed\n");
	printk("%s BLUETOOTH: Exiting.\n", __func__);
}

static void bcm_bt_lpm_clean_bt_wake(
			struct bcm_bt_lpm_entry_struct *priv,
			bool b_tty)
{
	if (priv->pdata->bt_wake_gpio < 0)
		return;

	gpio_free((unsigned)priv->pdata->bt_wake_gpio);

	if (b_tty)
		bcm_bt_lpm_tty_cleanup();

//	printk("%s BLUETOOTH:Exiting.\n", __func__);
}

static irqreturn_t bcm_bt_lpm_host_wake_isr(int irq, void *dev)
{
	unsigned int host_wake;
	unsigned long flags;
	struct bcm_bt_lpm_entry_struct *priv;

	priv = (struct bcm_bt_lpm_entry_struct *)dev;
	if (priv == NULL) {
		pr_err(
		"%s BLUETOOTH: Error data pointer is null\n",
							__func__);
		return IRQ_HANDLED;
	}
	spin_lock_irqsave(&priv->plpm->bcm_bt_lpm_lock, flags);

	host_wake = gpio_get_value(
			priv->pdata->host_wake_gpio);
	if (BCM_BT_LPM_HOST_WAKE_ASSERT == host_wake){
		pr_info("moonys Assered-- %s :[%s]\n", __func__,host_wake?"High":"Low");
                wake_lock(&priv->host_wakelock);
		  irq_set_irq_type(priv->plpm->host_irq, IRQF_TRIGGER_HIGH);
        }
	else{
		pr_info("moonys Deassered-- %s :[%s]\n", __func__,host_wake?"High":"Low");		
                 wake_unlock(&priv->host_wakelock);
//		  wake_lock(&priv->host_wakelock);

		  irq_set_irq_type(priv->plpm->host_irq, IRQF_TRIGGER_LOW);
        }

	spin_unlock_irqrestore(&priv->plpm->bcm_bt_lpm_lock, flags);
//	pr_info("moonys -- %s :[%d][%s]\n", __func__,BCM_BT_LPM_HOST_WAKE_ASSERT,host_wake?"High":"Low");
	return IRQ_HANDLED;
}


static int bcm_bt_lpm_init_hostwake(struct bcm_bt_lpm_entry_struct *priv)
{
	int rc = -1;

	printk("%s BLUETOOTH:Entering.\n", __func__);
	if ((priv == NULL) ||
		(priv->pdata->host_wake_gpio < 0)) {
		pr_err("%s BLUETOOTH: invalid host-wake-gpio.\n",
			__func__);
		return -EFAULT;
	}
	rc = devm_gpio_request_one(&priv->pdev->dev,
				priv->pdata->host_wake_gpio,
				GPIOF_OUT_INIT_LOW,
				"host_wake_gpio");

	if (rc) {
		pr_err
	    ("%s: failed to configure host-wake-gpio err=%d\n",
		     __func__, rc);
		return rc;
	}
	gpio_direction_input(priv->pdata->host_wake_gpio);

	printk("%s BLUETOOTH:Exiting.\n", __func__);
	return rc;
}

static void bcm_bt_lpm_clean_host_wake(
				struct bcm_bt_lpm_entry_struct *priv)
{
	printk("%s BLUETOOTH:Entering.\n", __func__);
	if ((priv == NULL) ||
		 (priv->pdata->host_wake_gpio < 0)) {
		pr_err("%s BLUETOOTH: NULL ptr or invalid hw gpio.\n",
			__func__);
		return;
	}

	gpio_free((unsigned)priv->pdata->host_wake_gpio);

	free_irq(priv->plpm->host_irq, priv_g);
	printk("%s BLUETOOTH:Exiting.\n", __func__);
}


void bcm_bt_lpm_start(void)
{
	int rc = -1;

	printk("%s BLUETOOTH: Entering.\n", __func__);
	if (priv_g != NULL) {
		printk(
		"%s: BLUETOOTH: data pointer is valid\n", __func__);

		rc = gpio_to_irq(
			priv_g->pdata->host_wake_gpio);
		if (rc < 0) {
			pr_err
		("%s: failed to configure BT Host Mgmt err=%d\n",
				__func__, rc);
			return;
		}
		priv_g->plpm->host_irq = rc;
		printk("%s: BLUETOOTH: request host_wake_irq=%d\n",
			__func__, priv_g->plpm->host_irq);

		rc = request_irq(
			priv_g->plpm->host_irq,
			bcm_bt_lpm_host_wake_isr,
			//(IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
			(IRQF_TRIGGER_LOW |
			IRQF_NO_SUSPEND), "bt_host_wake", priv_g);
		if (rc) {
			pr_err("%s: request irq failed: err=%d\n",
						 __func__, rc);
		}
	} else {
		pr_err("%s: BLUETOOTH:data ptr null, Uninitialized\n",
						__func__);
		return;
	}
	printk("%s BLUETOOTH:Exiting.\n", __func__);
	return;
}


static int bcm_bt_get_bt_wake_state(unsigned long arg)
{
	void __user *uarg = (void __user *)arg;
	unsigned long tmp;

	//printk("%s BLUETOOTH: Entering.\n", __func__);
	if ((priv_g == NULL) ||
		 (priv_g->pdata->bt_wake_gpio < 0)) {
		pr_err("%s BLUETOOTH: NULL ptr or invalid bw gpio.\n",
			__func__);
		return -EFAULT;
	}

	tmp = gpio_get_value(priv_g->pdata->bt_wake_gpio);

	pr_info(
		"%s: (bt_wake:%d), gpio_get_value(tmp:%d)\n",
		__func__, priv_g->pdata->bt_wake_gpio,
						(int) tmp);

	if (copy_to_user(uarg, &tmp, sizeof(*uarg)))
		return -EFAULT;
	//printk("%s BLUETOOTH:Exiting.\n", __func__);
	return 0;
}

static int bcm_bt_lpm_tty_ioctl(struct tty_struct *tty,
			struct file *file,
			unsigned int cmd, unsigned long arg)
{
	int rc = -1; 

	printk("%s:BLUETOOTH:Enter bcm_bt_lpm_tty_ioctl,cmd=%X\n",
						__func__, cmd);
	switch (cmd) {
	case TIO_ASSERT_BT_WAKE:
		rc = bcm_bt_lpm_assert_bt_wake();
		printk("%s: BLUETOOTH: TIO_ASSERT_BT_WAKE\n",
							__func__);
		break;

	case TIO_DEASSERT_BT_WAKE:
		rc = bcm_bt_lpm_deassert_bt_wake();
		printk("%s: BLUETOOTH: TIO_DEASSERT_BT_WAKE\n",
							__func__);
		break;

	case TIO_GET_BT_WAKE_STATE:
		rc = bcm_bt_get_bt_wake_state(arg);
		printk("%s: BLUETOOTH: TIO_GET_BT_WAKE_STATE\n",
							__func__);
		break;

	default:
		pr_err("%s: BLUETOOTH: switch default. cmd=%X\n",
						__func__, cmd);
//		printk("moonys : %X,%X,%X,%X,%X,%X,%X,%X,,\n",TCSETSF,TCSETSW, TCSETS,TCGETS,TCGETS2,TCSETSF2,TCSETSW2,TCSETS2);
//		printk("moonys : %X,%X,%X,%X,%X,%X,%X,%X,\n",TCGETA,TCSETAF, TCSETAW,TCSETA,TIOCGLCKTRMIOS,TIOCSLCKTRMIOS,TCGETX,TCSETX );
	//	printk("moonys : %X,%X,%X,\n",TCSETXW,TCSETXF, TIOCGSOFTCAR,TIOCSSOFTCAR);				
		
		return n_tty_ioctl_helper(tty, file, cmd, arg);
		//return 0;

	}
	printk("%s:BLUETOOTH:Exit bcm_bt_lpm_tty_ioctl,cmd=%X\n",
						__func__, cmd);
	return rc;
}

static int bcm_bt_lpm_tty_open(struct tty_struct *tty)
{
	struct uart_state *state;

	printk("%s BLUETOOTH: Entering.\n", __func__);
	state = tty->driver_data;
	if (priv_g == NULL) {
		pr_err("%s BLUETOOTH: NULL ptr or invalid bw gpio.\n",
			__func__);
		return -EFAULT;
	}
	priv_g->plpm->uport = state->uart_port;

	bcm_bt_lpm_init_bt_wake(priv_g);
	bcm_bt_lpm_init_hostwake(priv_g);

    wake_lock_init(&(priv_g->bt_wakelock), WAKE_LOCK_SUSPEND, "bt_wakelock");
    wake_lock_init(&(priv_g->host_wakelock), WAKE_LOCK_SUSPEND, "host_wakelock");

	bcm_bt_lpm_start();
	printk("bcm_bt_lpm_tty_open()::open(): x%p",
				bcm_bt_lpm_ldisc_saved.open);

	if (bcm_bt_lpm_ldisc_saved.open)
		return bcm_bt_lpm_ldisc_saved.open(tty);
	printk("%s BLUETOOTH:Exiting.\n", __func__);
	return 0;
}

static void bcm_bt_lpm_tty_close(struct tty_struct *tty)
{
	struct uart_state *state;

	printk("%s BLUETOOTH: Entering.\n", __func__);
	if (!priv_g || !priv_g->plpm) {
		pr_err("%s:data freed?priv_g:0x%p,p_lpm: 0x%p\n",
				__func__, priv_g, priv_g->plpm);
		return;
	}
	priv_g->plpm->uport = 0;
	state = tty->driver_data;

	bcm_bt_lpm_clean_bt_wake(priv_g, false);
	bcm_bt_lpm_clean_host_wake(priv_g);

    wake_lock_destroy(&priv_g->bt_wakelock);
    wake_lock_destroy(&priv_g->host_wakelock);
	printk("bcm_bt_lpm_tty_close(line: %d)::close(): x%p",
	state->uart_port->line, bcm_bt_lpm_ldisc_saved.close);

	if (bcm_bt_lpm_ldisc_saved.close)
		bcm_bt_lpm_ldisc_saved.close(tty);
	printk("%s BLUETOOTH:Exiting.\n", __func__);
	return;
}

static int bcm_bt_lpm_tty_init(void)
{
	int err;
	printk("%s: BLUETOOTH:\n", __func__);

	/* Inherit the N_TTY's ops */
	n_tty_inherit_ops(&bcm_bt_lpm_ldisc_ops);

	bcm_bt_lpm_ldisc_ops.owner = THIS_MODULE;
	bcm_bt_lpm_ldisc_ops.name = "bcm_bt_lpm_tty";
	bcm_bt_lpm_ldisc_ops.ioctl = bcm_bt_lpm_tty_ioctl;
	bcm_bt_lpm_ldisc_saved.open = bcm_bt_lpm_ldisc_ops.open;
	bcm_bt_lpm_ldisc_saved.close = bcm_bt_lpm_ldisc_ops.close;
	bcm_bt_lpm_ldisc_ops.open = bcm_bt_lpm_tty_open;
	bcm_bt_lpm_ldisc_ops.close = bcm_bt_lpm_tty_close;

	err = tty_register_ldisc(N_BRCM_HCI, &bcm_bt_lpm_ldisc_ops);
	if (err)
		pr_err("can't register N_BRCM_HCI line discipline\n");
	else
		pr_info("N_BRCM_HCI line discipline registered\n");

	printk("%s BLUETOOTH:Exiting.\n", __func__);
	return err;
}

static const struct of_device_id bcm_bt_lpm_of_match[] = {
		{ .compatible = "bcm,bcm-bt-lpm",},
		{ /* Sentinel */ },
	};

MODULE_DEVICE_TABLE(of, bcm_bt_lpm_of_match);
static int bcm_bt_lpm_probe(struct platform_device *pdev)
{
	int rc = -EINVAL;
	struct bcm_bt_lpm_platform_data *pdata = NULL;

	const struct of_device_id *match = NULL;
	       printk("bcm_bt_lpm_probe in\n");

	match = of_match_device(bcm_bt_lpm_of_match, &pdev->dev);
	if (!match) {
		pr_err("%s: **INFO** No matcing device found\n",
							__func__);
	}

	priv_g = devm_kzalloc(&pdev->dev,
			sizeof(*priv_g),
			GFP_KERNEL);

	if (priv_g == NULL)
		return -ENOMEM;

	priv_g->pdata = devm_kzalloc(&pdev->dev,
			sizeof(
			struct bcm_bt_lpm_platform_data
			),
			GFP_KERNEL);

	if (priv_g->pdata == NULL)
		return -ENOMEM;

	priv_g->plpm  = devm_kzalloc(&pdev->dev,
			sizeof(struct bcm_bt_lpm_struct),
			GFP_KERNEL);

	if (priv_g->plpm == NULL)
		return -ENOMEM;


	spin_lock_init(&priv_g->plpm->bcm_bt_lpm_lock);

	if (!match) {
			       printk("bcm_bt_lpm_probe match is null\n");
		if (pdev->dev.platform_data) {
			pdata = pdev->dev.platform_data;
			priv_g->pdata->bt_wake_gpio   = pdata->bt_wake_gpio;
			priv_g->pdata->host_wake_gpio = pdata->host_wake_gpio;
		} else {
			printk("%s: **ERROR** NO platform data available\n", __func__);
			rc = -ENODEV;
			goto out;
		}
	} else if (pdev->dev.of_node) {
				       printk("bcm_bt_lpm_probe match is NOT null\n");
                priv_g->pdata->bt_wake_gpio = of_get_gpio( pdev->dev.of_node, 0);
                if (!gpio_is_valid( priv_g->pdata->bt_wake_gpio)) {
                        pr_err( "%s: ERROR Invalid bt-wake-gpio\n",
							__func__);
			rc = priv_g->pdata->bt_wake_gpio;
			goto out;
                }
                //priv_g->pdata->bt_wake_gpio=232;//moonys
                priv_g->pdata->host_wake_gpio = of_get_gpio(pdev->dev.of_node, 1);
                if (!gpio_is_valid( priv_g->pdata->host_wake_gpio)) {
                        pr_err( "%s:ERROR Invalid host-wake-gpio\n",
							__func__);
			rc = priv_g->pdata->host_wake_gpio;
			goto out;
                }
                //priv_g->pdata->host_wake_gpio=235;//moonys
	} else {
		pr_err("%s: **ERROR** NO platform data available\n",
							__func__);
		rc = -ENODEV;
		goto out;
	}
	pr_err("%s: bt_wake_gpio=%d, host-wake-gpio=%d.\n",
		__func__,
		priv_g->pdata->bt_wake_gpio,
		priv_g->pdata->host_wake_gpio);

	priv_g->pdev = pdev;

	pr_info("%s: bt_wake_gpio=%d, host_wake_gpio=%d\n",
		__func__, priv_g->pdata->bt_wake_gpio,
		priv_g->pdata->host_wake_gpio);

	/* register line discipline driver */
	rc = bcm_bt_lpm_tty_init();

	if (rc) {
		pr_err("%s: bcm_bt_lpm_tty_init failed\n",
							__func__);
		rc = -1;
	}

	printk("%s BLUETOOTH:Exiting.\n", __func__);
	return rc;

out:
	printk("%s BLUETOOTH:Exiting after cleaning.\n",
							__func__);
	return rc;
}

static int bcm_bt_lpm_remove(struct platform_device *pdev)
{
	if (priv_g == NULL)
		return 0;

	if (priv_g->pdata) {
		bcm_bt_lpm_clean_bt_wake(priv_g, true);
		bcm_bt_lpm_clean_host_wake(priv_g);
	}
	if (priv_g != NULL) {
		if ((pdev->dev.of_node != NULL) &&
				(priv_g->pdata != NULL)) {
			kfree(priv_g->pdata);
			kfree(priv_g->plpm);
		}
		kfree(priv_g);
		priv_g = NULL;
	}
	printk("%s BLUETOOTH:Exiting.\n", __func__);
	return 0;
}

static struct platform_driver bcm_bt_lpm_platform_driver = {
	.probe = bcm_bt_lpm_probe,
	.remove = bcm_bt_lpm_remove,
	.driver = {
		   .name = "bcm-bt-lpm",
		   .owner = THIS_MODULE,
		   .of_match_table = bcm_bt_lpm_of_match,
		   },
};

static int __init bcm_bt_lpm_init(void)
{
	int rc = -1;

	printk("%s BLUETOOTH: Entering.\n", __func__);
	rc = platform_driver_register(&bcm_bt_lpm_platform_driver);
	if (rc)
		pr_err(
		"BLUETOOTH: driver register failed err=%d, Exiting %s.\n",
							rc, __func__);
	else
		printk(
		"BLUETOOTH: Init success. Exiting %s.\n", __func__);
	return rc;
}

static void __exit bcm_bt_lpm_exit(void)
{
	platform_driver_unregister(&bcm_bt_lpm_platform_driver);
	printk("%s BLUETOOTH:Exiting.\n", __func__);
}

late_initcall(bcm_bt_lpm_init);
module_exit(bcm_bt_lpm_exit);

MODULE_DESCRIPTION("bcm-bt-lpm");
MODULE_AUTHOR("Broadcom");
MODULE_LICENSE("GPL");
MODULE_ALIAS_LDISC(N_BRCM_HCI);

