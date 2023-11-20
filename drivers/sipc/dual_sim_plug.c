/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/err.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif

#include <linux/sipc.h>

#include <linux/dual_sim_plug.h>

#define DELAY_COUNT  1000

#define VALUE_SIM1_PLUGIN  0x11
#define VALUE_SIM1_PLUGOUT  0x10
#define VALUE_SIM2_PLUGIN  0x21
#define VALUE_SIM2_PLUGOUT  0x20

#define SMSG_SIM_PLUG 0xA5A5

#define DUAL_SIM_PLUG_1_GPIO	0xffff /*same to dts*/

int flag_low_count_sim1 = 0;
int flag_high_count_sim1 = 0;

int flag_low_count_sim2 = 0;
int flag_high_count_sim2 = 0;

struct dual_sim_plug_device{
  struct dual_sim_plug_init_data *init;
	struct task_struct *th1;
	int irq_cd1;
	int irq_cd2;
	unsigned int sim1_detect_gpio;
	unsigned int sim2_detect_gpio;
};


enum {
	DUAL_SIM_GPIO_INIT_OK 		= 0x00,
	SIM1_GPIO_OK_SIM2_GPIO_NOK 	= 0x01,
	SIM1_GPIO_NOK_SIM2_GPIO_OK	= 0x10,
	DUAL_SIM_GPIO_INIT_NOK		= 0x11,
	SIM1_1_GPIO_FOR_DUAL_SIM	= 0x0f,
	SIM2_1_GPIO_FOR_DUAL_SIM	= 0xf0,
	DUAL_SIM_GPIO_UNDEF 		= 0xff
};


static irqreturn_t sim1_irq_cd(int irq, void *dev_id)
{
	struct dual_sim_plug_device *dual_sim_plug = (struct dual_sim_plug_device *)dev_id;

	struct smsg msend;
	int  flag = 0;

	flag = gpio_get_value(dual_sim_plug->sim1_detect_gpio);

	if (flag == 0) {
		flag_low_count_sim1++;
		flag_high_count_sim1 = 0;
	} else {
		flag_high_count_sim1++;
		flag_low_count_sim1= 0;
	}

	if (flag_low_count_sim1 >= DELAY_COUNT) {
		flag_low_count_sim1 = 0;
		irq_set_irq_type(irq, IRQF_TRIGGER_HIGH);
		printk(KERN_INFO "Enter sim1_irq_cd,flag = low\n");

	#ifdef CONFIG_SIM_PLUG_IN_GPIO_L2H
		smsg_set(&msend, dual_sim_plug->init->channel, SMSG_TYPE_CMD, SMSG_SIM_PLUG, VALUE_SIM1_PLUGOUT);/*plug out*/
	#else
		smsg_set(&msend, dual_sim_plug->init->channel, SMSG_TYPE_CMD, SMSG_SIM_PLUG, VALUE_SIM1_PLUGIN);/*plug in*/
	#endif

		smsg_send(dual_sim_plug->init->dst, &msend, -1);
		printk(KERN_INFO "dual_sim_plug->init->dst = 0x%x,msend.channel = 0x%x,	msend.type = 0x%x,msend.flag = 0x%x,msend.value = 0x%x\n",\
 dual_sim_plug->init->dst, msend.channel, msend.type, msend.flag, msend.value);

	} else if (flag_high_count_sim1 >= DELAY_COUNT) {
		flag_high_count_sim1 = 0;
		irq_set_irq_type(irq, IRQF_TRIGGER_LOW);
		printk(KERN_INFO "Enter sim1_irq_cd,flag = high\n");

	#ifdef CONFIG_SIM_PLUG_IN_GPIO_L2H
		smsg_set(&msend, dual_sim_plug->init->channel, SMSG_TYPE_CMD, SMSG_SIM_PLUG, VALUE_SIM1_PLUGIN);/*plug in*/
	#else
		smsg_set(&msend, dual_sim_plug->init->channel, SMSG_TYPE_CMD, SMSG_SIM_PLUG, VALUE_SIM1_PLUGOUT);/*plug out*/
	#endif

		smsg_send(dual_sim_plug->init->dst, &msend, -1);
		printk(KERN_INFO "dual_sim_plug->init->dst = 0x%x,msend.channel = 0x%x,	msend.type = %d,msend.flag = 0x%x,msend.value = 0x%x\n",\
  dual_sim_plug->init->dst, msend.channel, msend.type, msend.flag, msend.value);

	} else {

	}

   return IRQ_HANDLED;

}

static irqreturn_t sim2_irq_cd (int irq, void *dev_id)
{
	struct dual_sim_plug_device *dual_sim_plug = (struct dual_sim_plug_device *)dev_id;

	struct smsg msend;
	int  flag = 0;

	flag = gpio_get_value(dual_sim_plug->sim2_detect_gpio);

	if (flag == 0) {
		flag_low_count_sim2++;
		flag_high_count_sim2 = 0;
	} else {
		flag_high_count_sim2++;
		flag_low_count_sim2 = 0;
	}

	if (flag_low_count_sim2 >= DELAY_COUNT) {
		flag_low_count_sim2 = 0;
		irq_set_irq_type(irq, IRQF_TRIGGER_HIGH);
		printk(KERN_INFO "Enter sim2_irq_cd,flag = low\n");

	#ifdef CONFIG_SIM_PLUG_IN_GPIO_L2H
			smsg_set(&msend, dual_sim_plug->init->channel, SMSG_TYPE_CMD, SMSG_SIM_PLUG, VALUE_SIM2_PLUGOUT);/*plug out*/
	#else
			smsg_set(&msend, dual_sim_plug->init->channel, SMSG_TYPE_CMD, SMSG_SIM_PLUG, VALUE_SIM2_PLUGIN);/*plug in*/
	#endif

		smsg_send(dual_sim_plug->init->dst, &msend, -1);
		printk(KERN_INFO "dual_sim_plug->init->dst = 0x%x,msend.channel = 0x%x,	msend.type = %d,msend.flag = 0x%x,msend.value = 0x%x\n",\
 dual_sim_plug->init->dst, msend.channel, msend.type, msend.flag, msend.value);

	} else if (flag_high_count_sim2 >= DELAY_COUNT) {
		flag_high_count_sim2 = 0;
		irq_set_irq_type(irq, IRQF_TRIGGER_LOW);
		printk(KERN_INFO "Enter sim2_irq_cd,flag = high\n");

	#ifdef CONFIG_SIM_PLUG_IN_GPIO_L2H
		smsg_set(&msend, dual_sim_plug->init->channel, SMSG_TYPE_CMD, SMSG_SIM_PLUG, VALUE_SIM2_PLUGIN);/*plug in*/
	#else
		smsg_set(&msend, dual_sim_plug->init->channel, SMSG_TYPE_CMD, SMSG_SIM_PLUG, VALUE_SIM2_PLUGOUT);/*plug out*/
	#endif

		smsg_send(dual_sim_plug->init->dst, &msend, -1);
		printk(KERN_INFO "dual_sim_plug->init->dst = 0x%x,msend.channel = 0x%x,msend.type = %x, msend.flag = 0x%x,msend.value = 0x%x\n",\
 dual_sim_plug->init->dst, msend.channel, msend.type, msend.flag, msend.value);

	} else {

	}

   return IRQ_HANDLED;
}

static int dual_sim_plug_thread(void *data)
{
	struct dual_sim_plug_device *dual_sim_plug = (struct dual_sim_plug_device *)data;
	struct smsg mrecv;
	int rval;
	struct sched_param param = {.sched_priority = 91};

	rval = smsg_ch_open(dual_sim_plug->init->dst, dual_sim_plug->init->channel, -1);
	if (rval != 0) {
		printk(KERN_ERR "Failed to open channel: %d\n", dual_sim_plug->init->channel);
		return 0;
	}

	sched_setscheduler(current, SCHED_RR, &param);
	printk(KERN_INFO "Enter dual_sim_plug_thread\n");

	while (!kthread_should_stop()) {
		smsg_set(&mrecv, dual_sim_plug->init->channel, 0, 0, 0);
		rval = smsg_recv(dual_sim_plug->init->dst, &mrecv, -1);
		printk(KERN_INFO "Enter dual_sim_plug_thread rval = %d,mrecv.channel = %d,mrecv.type =%d,mrecv.flag =%d,mrecv.value =%d\n", rval, mrecv.channel, mrecv.type, mrecv.flag, mrecv.value);
		if (rval == -EIO) {
			msleep(5);
			continue;
		}
		switch (mrecv.type) {
			case SMSG_TYPE_DONE:
				if (mrecv.flag == SMSG_SIM_PLUG) {
					if (mrecv.value == VALUE_SIM1_PLUGOUT)
						printk(KERN_INFO "Sim1 plug out done\n");
					else if (mrecv.value == VALUE_SIM1_PLUGIN)
						printk(KERN_INFO "Sim1 plug in done\n");
					else if (mrecv.value == VALUE_SIM2_PLUGOUT)
						printk(KERN_INFO "Sim2 plug out done\n");
					else if (mrecv.value == VALUE_SIM2_PLUGIN)
						printk(KERN_INFO "Sim2 plug in done\n");
					else {
					}
				}
				break;
			case SMSG_TYPE_OPEN:
				printk(KERN_INFO "SMSG_TYPE_OPEN!!!\n");
				smsg_open_ack(dual_sim_plug->init->dst, dual_sim_plug->init->channel);
				break;
			default:
				break;
	 	}
	}

	return 0;
}

static inline void dual_sim_plug_destroy_pdata(struct dual_sim_plug_init_data **init)
{
#ifdef CONFIG_OF
	struct dual_sim_plug_init_data *pdata = *init;

	if (pdata) {
		kfree(pdata);
	}

	*init = NULL;
#else
	return;
#endif
}

static int dual_sim_plug_parse_dt(struct dual_sim_plug_init_data **init, struct device *dev)
{
#ifdef CONFIG_OF
	struct device_node *np = dev->of_node;
	struct dual_sim_plug_init_data *pdata = NULL;
	int ret;
	uint32_t data;

	pdata = kzalloc(sizeof(struct dual_sim_plug_init_data),GFP_KERNEL);
	if (!pdata) {
		printk(KERN_ERR "Failed to allocate pdata memory\n");
		return -ENOMEM;
	}

	ret = of_property_read_string(np,"sprd,name",(const char**)&pdata->name);
	if (ret) {
		goto error;
	}

	ret = of_property_read_u32(np,"sprd,dst",(uint32_t *)&data);
	if (ret) {
		goto error;
	}
	pdata->dst = (uint8_t)data;

	ret = of_property_read_u32(np,"sprd,channel",(uint32_t *)&data);
	if (ret) {
		goto error;
	}
	pdata->channel = (uint8_t)data;

	ret = of_property_read_u32(np,"sprd,sim1_gpio",(uint32_t *)&data);
	if (ret) {
		goto error;
	}
	pdata->sim1_gpio = (uint32_t)data;

	ret = of_property_read_u32(np,"sprd,sim2_gpio",(uint32_t *)&data);
	if (ret) {
		goto error;
	}
	pdata->sim2_gpio = (uint32_t)data;

	*init = pdata;
	return ret;
error:
	kfree(pdata);
	*init = NULL;
	return ret;
#else
	return -ENODEV;
#endif
}

static void get_dual_sim_plug_device(struct dual_sim_plug_device *simplg_dev, struct dual_sim_plug_init_data *init_data)
{
	simplg_dev->sim1_detect_gpio = init_data->sim1_gpio;
	simplg_dev->sim2_detect_gpio = init_data->sim2_gpio;
	simplg_dev->init = init_data;
}


static void dual_sim_plug_memory_reclaim(struct dual_sim_plug_device *simplg_dev, struct dual_sim_plug_init_data *init_data)
{
	kfree(simplg_dev);
	dual_sim_plug_destroy_pdata(&init_data);
}

static void dual_sim_plug_close_ch(struct dual_sim_plug_device *simplg_dev, struct dual_sim_plug_init_data *init_data)
{
	smsg_ch_close(init_data->dst,init_data->channel,-1);

	free_irq(simplg_dev->irq_cd2,simplg_dev);
	gpio_free(simplg_dev->sim2_detect_gpio);

	free_irq(simplg_dev->irq_cd1,simplg_dev);
	gpio_free(simplg_dev->sim1_detect_gpio);

	dual_sim_plug_memory_reclaim(simplg_dev, init_data);
}


static int dual_sim_plug_gpio_init(struct dual_sim_plug_device *simplg_dev)
{
	int rtn_sim1;
	int rtn_sim2;

	/* sim 1 : */
	if (DUAL_SIM_PLUG_1_GPIO != simplg_dev->sim1_detect_gpio) {
		rtn_sim1 = gpio_request(simplg_dev->sim1_detect_gpio, "sim1_detect");
		if (rtn_sim1) {
			printk(KERN_ERR "failed to get sim1 detect gpio\n");
			rtn_sim1 = -ENXIO;
		}

		if (rtn_sim1 == 0) {
			gpio_direction_input(simplg_dev->sim1_detect_gpio);
			printk(KERN_INFO "Succed to get sim1 detect gpio\n");
		}
	} else {
		printk(KERN_INFO "ignore the sim1 gpio init!\n");/*set in dts and ignore init*/
		rtn_sim1 = -EINVAL;
	}

	/* sim2 : */
	if (DUAL_SIM_PLUG_1_GPIO != simplg_dev->sim2_detect_gpio) {
		rtn_sim2 = gpio_request(simplg_dev->sim2_detect_gpio, "sim2_detect");
		if (rtn_sim2) {
			printk(KERN_ERR "failed to get sim2 detect gpio\n");
			rtn_sim2 = -ENXIO;
		}

		if (rtn_sim2 == 0) {
			gpio_direction_input(simplg_dev->sim2_detect_gpio);
			printk(KERN_INFO "Succed to get sim2 detect gpio\n");
		}
	} else {
		printk(KERN_INFO "ignore the sim2 gpio init!\n");
		rtn_sim2 = -EINVAL;
	}

	if ((rtn_sim1 == 0) && (rtn_sim2 == 0))
		return DUAL_SIM_GPIO_INIT_OK;
	else if ((rtn_sim1 == -ENXIO) && (rtn_sim2 == 0))
		return SIM1_GPIO_NOK_SIM2_GPIO_OK;
	else if ((rtn_sim1 == 0) && (rtn_sim2 == -ENXIO))
		return SIM1_GPIO_OK_SIM2_GPIO_NOK;
	else if ((rtn_sim1 == -ENXIO) && (rtn_sim2 == -ENXIO))
		return DUAL_SIM_GPIO_INIT_NOK;
	else if ((rtn_sim1 == 0) && (rtn_sim2 == -EINVAL))
		return SIM1_1_GPIO_FOR_DUAL_SIM;
	else if ((rtn_sim1 == -EINVAL) && (rtn_sim2 == 0))
		return SIM2_1_GPIO_FOR_DUAL_SIM;
	else
		return DUAL_SIM_GPIO_UNDEF;
}



static int dual_sim_plug_irq_probe(struct dual_sim_plug_device * simplg_dev, int sim_num)
{
	int val_gpio;
	int rtn_sim_stat = 0;

	/*sim 1*/
	if (sim_num == 1) {
		val_gpio = gpio_get_value(simplg_dev->sim1_detect_gpio);
		simplg_dev->irq_cd1 = gpio_to_irq(simplg_dev->sim1_detect_gpio);
		printk(KERN_INFO "Succed to get sim1 irq dual_sim_plug ->irq_cd1 =%d\n", simplg_dev->irq_cd1);

		if (simplg_dev->irq_cd1 >= 0) {
			if (val_gpio == 0) {
				printk(KERN_INFO "Sim1 detect gpio = low.\n");
				if (request_irq(simplg_dev->irq_cd1, sim1_irq_cd, IRQF_TRIGGER_HIGH|IRQF_NO_SUSPEND, "sim1_detect_hander", simplg_dev)) {
					printk(KERN_ERR "can't get sim1 detect irq.\n");
					rtn_sim_stat = -EBUSY;
				}
				printk(KERN_INFO "Get sim1 detect irq.\n");
			} else {
				printk(KERN_INFO "Sim1 detect gpio = high.\n");
				if (request_irq(simplg_dev->irq_cd1, sim1_irq_cd, IRQF_TRIGGER_LOW|IRQF_NO_SUSPEND, "sim1_detect_hander", simplg_dev)) {
					printk(KERN_ERR "can't get sim1 detect irq.\n");
					rtn_sim_stat = -EBUSY;
				}
				printk(KERN_INFO "Get sim1 detect irq.\n");
			}
		} else {
			printk(KERN_INFO "host detect has no irq available\n");
			rtn_sim_stat = -ENODEV;
		}

	} else if (sim_num == 2) {
		val_gpio = gpio_get_value(simplg_dev->sim2_detect_gpio);
		simplg_dev->irq_cd2 = gpio_to_irq(simplg_dev->sim2_detect_gpio);
		printk(KERN_INFO "Succed to get sim2 irq dual_sim_plug ->irq_cd2 =%d\n", simplg_dev->irq_cd2);

		if (simplg_dev->irq_cd2 >= 0) {
			if (val_gpio == 0) {
				printk(KERN_INFO "Sim2 detect gpio = low.\n");
				if (request_irq(simplg_dev->irq_cd2, sim2_irq_cd, IRQF_TRIGGER_HIGH|IRQF_NO_SUSPEND, "sim2_detect_hander", simplg_dev)) {
					printk(KERN_ERR "can't get sim2 detect irq.\n");
					rtn_sim_stat = -EBUSY;
				}
				printk(KERN_INFO "Get sim2 detect irq.\n");
			} else {
				printk(KERN_INFO "Sim2 detect gpio = high.\n");
				if (request_irq(simplg_dev->irq_cd2, sim2_irq_cd, IRQF_TRIGGER_LOW|IRQF_NO_SUSPEND, "sim2_detect_hander", simplg_dev)) {
					printk(KERN_ERR "can't get sim2 detect irq.\n");
					rtn_sim_stat = -EBUSY;
				}
				printk(KERN_INFO "Get sim2 detect irq.\n");
			}
		} else {
			printk(KERN_INFO "host detect has no irq available\n");
			rtn_sim_stat = -ENODEV;
		}

	} else {
		rtn_sim_stat = -EINVAL;
	}

	return rtn_sim_stat;
}


static int dual_sim_plug_probe(struct platform_device *pdev) /*modified by yuqi*/
{
	struct dual_sim_plug_init_data *init; /*struct dual_sim_plug_init_data *init = pdev->dev.platform_data;*/
	struct dual_sim_plug_device *dual_sim_plug;
	int rval = 0;
	int rtn_gpio_init;
	int rtn_sim1_irq_prb, rtn_sim2_irq_prb;


	printk(KERN_INFO "dual_sim_plug_probe start\n");

	if (pdev->dev.of_node && !init) {
		rval = dual_sim_plug_parse_dt(&init, &pdev->dev);
		if (rval) {
			printk(KERN_ERR "Failed to parse spipe device tree, ret=%d\n", rval);
			return rval;
		}
	}

	printk(KERN_INFO "Succed to parse spipe device tree,\
 ret=%d,init->name=%s,init->dst=%d,init->channel=%d,\
 init->sim1_gpio=%d,init->sim2_gpio=%d\n", \
 rval, init->name, init->dst, init->channel, init->sim1_gpio, init->sim2_gpio);

	dual_sim_plug = kzalloc(sizeof(struct dual_sim_plug_device), GFP_KERNEL);
	if (!dual_sim_plug) {
		printk(KERN_ERR "Failed to allocate dual_sim_plug_device\n");
		dual_sim_plug_destroy_pdata(&init);
		return -ENOMEM;
	}

	printk(KERN_INFO "Succed to allocate dual_sim_plug_device\n");

	get_dual_sim_plug_device(dual_sim_plug, init);

	rtn_gpio_init = dual_sim_plug_gpio_init(dual_sim_plug);
	switch (rtn_gpio_init) {
		case DUAL_SIM_GPIO_INIT_OK:
			printk(KERN_INFO "dual_sim_plug: DUAL_SIM_GPIO_INIT_OK!\n");
			rtn_sim1_irq_prb = dual_sim_plug_irq_probe(dual_sim_plug, 1);
			rtn_sim2_irq_prb = dual_sim_plug_irq_probe(dual_sim_plug, 2);

			if (rtn_sim1_irq_prb)
				gpio_free(dual_sim_plug->sim1_detect_gpio);
			if (rtn_sim2_irq_prb)
				gpio_free(dual_sim_plug->sim2_detect_gpio);
			if (rtn_sim1_irq_prb && rtn_sim1_irq_prb) {
				dual_sim_plug_memory_reclaim(dual_sim_plug, init);
				return -ENXIO;
			}
			break;

		case SIM1_GPIO_OK_SIM2_GPIO_NOK:
			printk(KERN_INFO "dual_sim_plug: SIM1_GPIO_OK_SIM2_GPIO_NOK!\n");
			rtn_sim1_irq_prb = dual_sim_plug_irq_probe(dual_sim_plug, 1);
			if (rtn_sim1_irq_prb) {
				gpio_free(dual_sim_plug->sim1_detect_gpio);
				dual_sim_plug_memory_reclaim(dual_sim_plug, init);
				return -ENXIO;
			}
			break;

		case SIM1_GPIO_NOK_SIM2_GPIO_OK:
			printk(KERN_INFO "dual_sim_plug: SIM1_GPIO_NOK_SIM2_GPIO_OK!\n");
			rtn_sim2_irq_prb = dual_sim_plug_irq_probe(dual_sim_plug, 2);
			if (rtn_sim2_irq_prb) {
				gpio_free(dual_sim_plug->sim2_detect_gpio);
				dual_sim_plug_memory_reclaim(dual_sim_plug, init);
				return -ENXIO;
			}
			break;

		case SIM1_1_GPIO_FOR_DUAL_SIM:
			printk(KERN_INFO "dual_sim_plug: SIM1_1_GPIO_FOR_DUAL_SIM!\n");
			rtn_sim1_irq_prb = dual_sim_plug_irq_probe(dual_sim_plug, 1);
			if (rtn_sim1_irq_prb) {
				gpio_free(dual_sim_plug->sim1_detect_gpio);
				dual_sim_plug_memory_reclaim(dual_sim_plug, init);
				return -ENXIO;
			}
			break;

		case SIM2_1_GPIO_FOR_DUAL_SIM:
			printk(KERN_INFO "dual_sim_plug: SIM2_1_GPIO_FOR_DUAL_SIM!\n");
			rtn_sim2_irq_prb = dual_sim_plug_irq_probe(dual_sim_plug, 2);
			if (rtn_sim2_irq_prb) {
				gpio_free(dual_sim_plug->sim2_detect_gpio);
				dual_sim_plug_memory_reclaim(dual_sim_plug, init);
				return -ENXIO;
			}
			break;

		default:
			printk(KERN_INFO "dual_sim_plug: default!\n");
			dual_sim_plug_memory_reclaim(dual_sim_plug, init);
			return -ENODEV;
	}


	dual_sim_plug->th1 = kthread_create(dual_sim_plug_thread, dual_sim_plug, "dual_sim_plug-%d-%d", init->dst, init->channel);
	if (IS_ERR(dual_sim_plug->th1)) {
		printk(KERN_ERR "Failed to create kthread: dual_sim_plug-%d-%d\n", init->dst, init->channel);
		rval = PTR_ERR(dual_sim_plug->th1);
		dual_sim_plug_close_ch(dual_sim_plug, init);

		return rval;
	}

	printk(KERN_INFO "Succed to create kthread: dual_sim_plug-%d-%d\n", init->dst, init->channel);

	wake_up_process(dual_sim_plug->th1);

	platform_set_drvdata(pdev, dual_sim_plug);

	return 0;

}


static int  dual_sim_plug_remove(struct platform_device *pdev)
{
	struct dual_sim_plug_device *dual_sim_plug = platform_get_drvdata(pdev);

	printk(KERN_INFO "dual_sim_plug->init->dst = %d,dual_sim_plug->init->channel = %d,\
 dual_sim_plug->init->sim1_gpio=%d,dual_sim_plug->init->sim2_gpio=%d\n",\
 dual_sim_plug->init->dst, dual_sim_plug->init->channel, dual_sim_plug->init->sim1_gpio, dual_sim_plug->init->sim2_gpio);

	if (!IS_ERR_OR_NULL(dual_sim_plug->th1))
		kthread_stop(dual_sim_plug->th1);

	smsg_ch_close(dual_sim_plug->init->dst, dual_sim_plug->init->channel, -1);

	free_irq(dual_sim_plug->irq_cd2, dual_sim_plug);

	gpio_free(dual_sim_plug->sim2_detect_gpio);

	free_irq(dual_sim_plug->irq_cd1, dual_sim_plug);

	gpio_free(dual_sim_plug->sim1_detect_gpio);

	dual_sim_plug_destroy_pdata(&dual_sim_plug->init);

	kfree(dual_sim_plug);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id dual_sim_plug_match_table[] = {
	{.compatible = "sprd,dual_sim_plug", },
	{ },
};

static struct platform_driver dual_sim_plug_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "dual_sim_plug",
		.of_match_table = dual_sim_plug_match_table,
	},
	.probe = dual_sim_plug_probe,
	.remove = dual_sim_plug_remove,
};

static int __init dual_sim_plug_init(void)
{
	printk(KERN_INFO "Dual_sim_plug_init start\n");
	return platform_driver_register(&dual_sim_plug_driver);
}

static void __exit dual_sim_plug_exit(void)
{
	printk(KERN_INFO "Dual_sim_plug_exit start\n");
	platform_driver_unregister(&dual_sim_plug_driver);
}

module_init(dual_sim_plug_init);
module_exit(dual_sim_plug_exit);

MODULE_AUTHOR("Bi Xiangru");
MODULE_DESCRIPTION("SIPC/DUAL_SIM_PLUG driver");
MODULE_LICENSE("GPL");
