
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <soc/sprd/gpio.h>
#include <linux/headset_sprd.h>
#include <soc/sprd/board.h>
#include <linux/input.h>

#include <soc/sprd/adc.h>
#include <asm/io.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/adi.h>
#include <linux/module.h>
#include <linux/wakelock.h>

#include <linux/regulator/consumer.h>
#include <soc/sprd/regulator.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/arch_misc.h>
#include <linux/notifier.h>
#ifdef CONFIG_OF
#include <linux/slab.h>
#include <linux/of_device.h>
#endif

static DEFINE_SPINLOCK(irq_detect_lock);
static int irq_detect;

#define HALL_DEVICE	        			"hall_detect"
//====================  debug  ====================
#define ENTER \
do{ if(debug_level >= 1) printk(KERN_INFO "[SPRD_HALL_DBG] func: %s  line: %04d\n", __func__, __LINE__); }while(0)

#define PRINT_DBG(format,x...)  \
do{ if(debug_level >= 1) printk(KERN_INFO "[SPRD_HALL_DBG] " format, ## x); }while(0)

#define PRINT_INFO(format,x...)  \
do{ printk(KERN_INFO "[SPRD_HALL_INFO] " format, ## x); }while(0)

#define PRINT_WARN(format,x...)  \
do{ printk(KERN_INFO "[SPRD_HALL_WARN] " format, ## x); }while(0)

#define PRINT_ERR(format,x...)  \
do{ printk(KERN_ERR "[SPRD_HALL_ERR] func: %s  line: %04d  info: " format,  __func__, __LINE__, ## x); }while(0)


/******************************************************************************
Error Code No.
******************************************************************************/
#define RSUCCESS        0

/******************************************************************************
Debug Message Settings
******************************************************************************/

/* Debug message event */
#define DBG_EVT_NONE		0x00000000	/* No event */
#define DBG_EVT_INT			0x00000001	/* Interrupt related event */
#define DBG_EVT_TASKLET		0x00000002	/* Tasklet related event */

#define DBG_EVT_ALL			0xffffffff
 
#define DBG_EVT_MASK      	(DBG_EVT_TASKLET)

#if 1
#define MSG(evt, fmt, args...) \
do {	\
	if ((DBG_EVT_##evt) & DBG_EVT_MASK) { \
		printk(fmt, ##args); \
	} \
} while(0)

#define MSG_FUNC_ENTRY(f)	MSG(FUC, "<FUN_ENT>: %s\n", __FUNCTION__)
#else
#define MSG(evt, fmt, args...) do{}while(0)
#define MSG_FUNC_ENTRY(f)	   do{}while(0)
#endif

struct sprd_hall_platform_data {
        int gpio_detect;
};

int gpio_detect_pin;
/******************************************************************************
Global Definations
******************************************************************************/
static struct workqueue_struct *hall_queue;
static struct work_struct hall_work;
//static struct hrtimer hall_timer;
static spinlock_t hall_lock;
static int hall_state= 0;  //0: hall open 1: hall close
static int shutdown_flag;
static struct input_dev *kpd_hall_dev;

static int hall_setup_eint(void);
static void hall_eint_func(void);
static int hall_irq_set_irq_type(unsigned int irq, unsigned int type)
{
        struct irq_desc *irq_desc = NULL;
        unsigned int irq_flags = 0;
        int ret = -1;

        ret = irq_set_irq_type(irq, type);
        irq_desc = irq_to_desc(irq);
        irq_flags = irq_desc->action->flags;

       
        return 0;
}

static void update_hall(struct work_struct *work)
{
       hall_state = gpio_get_value(gpio_detect_pin);
	if(hall_state == 0)  //hall open, screen should turn off
	{
		printk("hall open.\n");
		input_report_key(kpd_hall_dev, KEY_SLEEP, 1);
		input_report_key(kpd_hall_dev, KEY_SLEEP, 0);
		input_sync(kpd_hall_dev);
		hall_irq_set_irq_type(irq_detect,IRQF_TRIGGER_HIGH);
	}
	else //hall close,screen should turn on
	{	
		printk("hall close.\n");
		input_report_key(kpd_hall_dev, KEY_WAKEUP, 1);
		input_report_key(kpd_hall_dev, KEY_WAKEUP, 0);
		input_sync(kpd_hall_dev);
		hall_irq_set_irq_type(irq_detect,IRQF_TRIGGER_LOW);
	}
       enable_irq(irq_detect);

}
#ifdef CONFIG_OF
static struct sprd_hall_platform_data *hall_detect_parse_dt(
                         struct device *dev)
{
	struct sprd_hall_platform_data *pdata;
	struct device_node *np = dev->of_node,*buttons_np = NULL;
	int ret;
	struct headset_buttons *buttons_data;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for platform data\n");
		return NULL;
	}
	ret = of_property_read_u32(np, "gpio_detect", &pdata->gpio_detect);
	if(ret){
		dev_err(dev, "fail to get gpio_detect\n");
		goto fail;
	}
			
	return pdata;
fail:
	kfree(pdata);
	return NULL;
}
#endif
static int hall_probe(struct platform_device *pdev)
{
       struct sprd_hall_platform_data *pdata = pdev->dev.platform_data;
       //int irq_detect;
	int ret;
	unsigned long irqflags = 0;
 
	printk("hall probe enter.\n");
#ifdef CONFIG_OF
        struct device_node *np = pdev->dev.of_node;
        if (pdev->dev.of_node && !pdata){
                pdata = hall_detect_parse_dt(&pdev->dev);
                if(pdata)
                        pdev->dev.platform_data = pdata;
        }
        if (!pdata) {
                printk(KERN_WARNING "headset_detect_probe get platform_data NULL\n");
                return -EINVAL;
                
        }
#endif
	ret = gpio_request(pdata->gpio_detect, "hall_detect");
        if (ret < 0) {
                PRINT_ERR("failed to request GPIO_%d(headset_detect)\n", pdata->gpio_detect);
                return -EINVAL;
        }
      
	gpio_direction_input(pdata->gpio_detect);
	gpio_detect_pin = pdata->gpio_detect;
      //  gpio_direction_input(pdata->gpio_button);
       irq_detect = gpio_to_irq(pdata->gpio_detect);
       hall_state = gpio_get_value(gpio_detect_pin);

       irqflags = hall_state ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH;
	ret = request_irq(irq_detect, hall_eint_func /*headset_detect_irq_handler*/, irqflags  |IRQF_NO_SUSPEND, "hall_detect", NULL);
        if (ret < 0) {
                PRINT_ERR("failed to request IRQ_%d(GPIO_%d)\n", irq_detect, pdata->gpio_detect);
                return -EINVAL;
        }
        printk("request IRQ_%d(GPIO_%d)\n", irq_detect, pdata->gpio_detect);

	hall_queue = create_singlethread_workqueue(HALL_DEVICE);
	if(!hall_queue) {
		printk("[hall]Unable to create workqueue\n");
		return -ENODATA;
	}

	INIT_WORK(&hall_work, update_hall);

	kpd_hall_dev = input_allocate_device();
	if (!kpd_hall_dev) 
	{
		printk("[hall]kpd_hall_dev : fail!\n");
		return -ENOMEM;
	}

	//define multi-key keycode
	__set_bit(EV_KEY, kpd_hall_dev->evbit);
	__set_bit(KEY_SLEEP, kpd_hall_dev->keybit);
	__set_bit(KEY_WAKEUP, kpd_hall_dev->keybit);
    	
	
	kpd_hall_dev->id.bustype = BUS_HOST;
	kpd_hall_dev->name = "HALL";
	if(input_register_device(kpd_hall_dev))
	{
		printk("[hall]kpd_hall_dev register : fail!\n");
	}else
	{
		printk("[hall]kpd_hall_dev register : success!!\n");
	} 
	//spin_lock_init(&hall_lock);
	//shutdown_flag = 0;
	hall_state = 0;
       
	return 0;
}

static int hall_remove(struct platform_device *pdev)
{
	if(hall_queue)
		destroy_workqueue(hall_queue);
	if(kpd_hall_dev)
		input_unregister_device(kpd_hall_dev);
	return 0;
}

/******************************************************************************
Device driver structure
*****************************************************************************/
static const struct of_device_id hall_detect_of_match[] = {
        {.compatible = "sprd,hall-detect",},
        { }
};
static struct platform_driver hall_driver = 
{
    .probe	= hall_probe,
    .remove	= hall_remove,
    //.shutdown   = hall_shutdown,
    .driver     = {
    .name = HALL_DEVICE,
    .owner = THIS_MODULE,
    .of_match_table = hall_detect_of_match,

    },
};

static struct platform_device hall_device =
{
    .name = "hall_detect",
    .id   = -1,
};



static ssize_t store_set_hall_int_por(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	if(buf != NULL && size != 0)
	{
		printk("[hall]buf is %s and size is %d \n",buf,size);
		if(buf[0]== '0')
		{
			hall_state=0;
		}else
		{
			hall_state=1;
		}
	}
	return size;
}

static ssize_t show_get_hall_state(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "HALL state = %u\n", hall_state);
}
/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(get_hall_state,      S_IWUSR | S_IRUGO, show_get_hall_state,         NULL);

static DRIVER_ATTR(set_hall_int_por,      S_IWUSR | S_IRUGO, NULL,         store_set_hall_int_por);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *hall_attr_list[] = {
	&driver_attr_get_hall_state,        
	&driver_attr_set_hall_int_por	
};

static int hall_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(hall_attr_list)/sizeof(hall_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, hall_attr_list[idx])))
		{            
			printk("driver_create_file (%s) = %d\n", hall_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}


static void hall_eint_func(void)
{
	int ret=0;
       unsigned long spin_lock_flags;
       static int current_irq_state = 1;//irq is enabled after request_irq()

      	printk("[hall]hall_eint_func\n");  	
       disable_irq_nosync(irq_detect);

        

	ret = queue_work(hall_queue, &hall_work);	
      if(!ret)
      {
  	    //printk("[hall]hall_eint_func: return:%d!\n", ret);  		
      }

}



/******************************************************************************
 * hall_mod_init
 * 
 * DESCRIPTION:
 *   Register the hall device driver ! 
 * 
 * PARAMETERS: 
 *   None
 * 
 * RETURNS: 
 *   None
 * 
 * NOTES: 
 *   RSUCCESS : Success
 * 
 ******************************************************************************/

static int __init hall_mod_init(void)
{	
   s32 ret;

   printk("Techain hall driver register \n");
	

    ret = platform_driver_register(&hall_driver);

    if(ret) 
    {
	printk("[hall]Unable to register hall driver (%d)\n", ret);
    }	
    
     if((ret = hall_create_attr(&hall_driver.driver))!=0)
     {
	printk("create attribute err = %d\n", ret);
	
     }
     
     printk("[hall]hall_mod_init Done \n");
 
    return RSUCCESS;
}

/******************************************************************************
 * hall_mod_exit
 * 
 * DESCRIPTION: 
 *   Free the device driver ! 
 * 
 * PARAMETERS: 
 *   None
 * 
 * RETURNS: 
 *   None
 * 
 * NOTES: 
 *   None
 * 
 ******************************************************************************/
 
static void __init hall_mod_exit(void)
{
	platform_driver_unregister(&hall_driver);
	printk("Techainhall driver unregister\n");
	
}

module_init(hall_mod_init);
module_exit(hall_mod_exit);
MODULE_AUTHOR("Techain Inc.");
MODULE_DESCRIPTION("Techain hall driver");
MODULE_LICENSE("GPL");
