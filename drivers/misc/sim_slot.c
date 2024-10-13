/* 

 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.


*/
#include "sim_slot.h"

#ifndef SIM_SLOT_PIN
	#error SIM_SLOT_PIN should be have a value. but not defined.
#endif


static int ds_detect_level = 1; //DS is default high

static int check_simslot_count(struct seq_file *m, void *v)
{

	int retval, support_number_of_simslot;

//	printk("\n SIM_SLOT_PIN : %d\n", SIM_SLOT_PIN);  //temp log for checking GPIO Setting correctly applyed or not

	retval = gpio_request(SIM_SLOT_PIN,"SIM_SLOT_PIN");
	if (retval) {
			pr_err("%s:Failed to reqeust GPIO, code = %d.\n",
				__func__, retval);
			support_number_of_simslot = retval;
	}
	else
	{
		retval = gpio_direction_input(SIM_SLOT_PIN);

		if (retval){
			pr_err("%s:Failed to set direction of GPIO, code = %d.\n",
				__func__, retval);
			support_number_of_simslot = retval;
		}
		else
		{
			retval = gpio_get_value(SIM_SLOT_PIN);

			switch(ds_detect_level)
			{
				case DUAL_SIM_DETECT_LOW:
				case DUAL_SIM_DETECT_HIGH:
					if(retval == ds_detect_level)
					{
						support_number_of_simslot = DUAL_SIM;
						printk("\n SIM_SLOT_PIN gpio %d : Dual Sim mode \n", SIM_SLOT_PIN);
					}
					else
					{
						support_number_of_simslot = SINGLE_SIM;
						printk("\n SIM_SLOT_PIN gpio %d : Single Sim mode \n", SIM_SLOT_PIN);
					}
					break; 
				case DUAL_SIM_ALWAYS:
					support_number_of_simslot = DUAL_SIM;
					printk("\n SIM_SLOT_PIN gpio %d : Dual Sim always mode \n", SIM_SLOT_PIN);
					break;
				case SINGLE_SIM_ALWAYS:
					support_number_of_simslot = SINGLE_SIM;
					printk("\n SIM_SLOT_PIN gpio %d : Single Sim always mode \n", SIM_SLOT_PIN);
					break;
					
				default:
					support_number_of_simslot = -1;
					break;
			}
		}
		gpio_free(SIM_SLOT_PIN);
	}

	if(support_number_of_simslot < 0) 
	{
		pr_err("******* Make a forced kernel panic because can't check simslot count******\n");
		panic("kernel panic");
	}
	
	seq_printf(m, "%u\n", support_number_of_simslot);

	return 0;

}

static int check_simslot_count_open(struct inode *inode, struct file *file)
{
	return single_open(file, check_simslot_count, NULL);
}

static const struct file_operations check_simslot_count_fops = {
	.open	= check_simslot_count_open,
	.read	= seq_read,
	.write	= seq_lseek,
	.release= single_release,
};

#ifdef CONFIG_CHECK_SIMSLOT_COUNT_DT
static int sprd_simslot_probe(struct platform_device *pdev)
{
        struct device_node *np = NULL;
	int ret;
        np = pdev->dev.of_node;

        if (np) {
		ret = of_property_read_u32(np, "ds_detect_level", &ds_detect_level);
		if(ret) {
			printk("%s :fail to get ds_detect_level\n", __func__);
			goto fail;
		}
        } else {
		printk("%s : fail to get device_node\n", __func__);
		goto fail;
	}

	printk("%s ds_detect_level:%d\n", __func__, ds_detect_level);

	if(!proc_create("simslot_count",0,NULL,&check_simslot_count_fops))
	{
		pr_err("***** Make a forced kernel panic because can't make a simslot_count file node ******\n"); 
		panic("kernel panic");
		return -ENOMEM;
	}

fail:
	return 0;
}

static const struct of_device_id sprd_simslot_match_table[] = {
	{ .compatible = "sprd,simslot_count", },
	{},
};

static struct platform_driver sprd_simslot_driver = {
        .probe    = sprd_simslot_probe,
        .driver   = {
            .owner = THIS_MODULE,
            .name = "sprd_simslot",
            .of_match_table = sprd_simslot_match_table,
        },
};

static int __init simslot_count_init(void)
{
	printk("%s\n", __func__);
	if(platform_driver_register(&sprd_simslot_driver) != 0) {
		printk("sprd_simslot platform drv register Failed\n"); 
		return -1;

	}

	return 0;
}
#else
static int __init simslot_count_init(void)
{

	if(!proc_create("simslot_count",0,NULL,&check_simslot_count_fops))
	{
		pr_err("***** Make a forced kernel panic because can't make a simslot_count file node ******\n"); 
		panic("kernel panic");
		return -ENOMEM;
	}
	else return 0;
}
#endif

late_initcall(simslot_count_init);

