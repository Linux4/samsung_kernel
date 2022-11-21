#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include "startup_reason.h"

/*todo fix debug messages properly*/
#ifdef dev_dbg
#undef dev_dbg
#endif

#define dev_dbg( format, ...)           \
	printk(KERN_WARNING format, ##__VA_ARGS__)

//printk(KERN_WARNING  , format , ## arg)


struct class * startup_reason_class ;
struct device_type startup_reason_dev_type ;

struct startup_reason_dev
{
	struct startup_reason_source *source ;
	struct device *dev;
	int attribute_count ;
	struct device_attribute *attributes ;
	struct list_head reason_source_list ;
} ;

static LIST_HEAD(startup_reasons);
//startup_reason_dev_release

static void startup_reason_dev_release(struct device *dev)
{
	 kfree(dev);
}

static ssize_t show_name(struct device *dev, struct device_attribute *attr, char *buf)
{
       //struct con_driver *con = dev_get_drvdata(dev);
        return snprintf(buf, PAGE_SIZE, "1\n");

}

int register_startup_reasons(struct startup_reason_source * source , int number_of_reasons,struct device * parent )
{
	struct startup_reason_dev * new_dev ;
	struct device * dev; 
	int i;
	int return_value = -EINVAL;
	if (number_of_reasons && source && source->get_reasons_name){
		new_dev=kzalloc(sizeof(struct startup_reason_dev),GFP_KERNEL);
		if (!new_dev){
			return -ENOMEM ;
		}
		dev=kzalloc(sizeof(struct device),GFP_KERNEL);
		if (!dev){
			kfree (new_dev);
			return -ENOMEM ;
		}
		device_initialize(dev);
	        dev->class = startup_reason_class;
	        dev->type = &startup_reason_dev_type;
	        dev->parent = parent;
	        dev->release = startup_reason_dev_release;
	        dev_set_drvdata(dev, new_dev);
	        new_dev->dev = dev;

		INIT_LIST_HEAD(&new_dev->reason_source_list) ;
		new_dev->source=source ;
		new_dev->attribute_count = number_of_reasons ;
		new_dev->attributes=kzalloc(sizeof(struct device_attribute)*number_of_reasons+1
						,GFP_KERNEL) ;
		if (!new_dev->attributes){
			kfree(new_dev);
			return -ENOMEM ;
		}
		source->dev=dev ;
		source->parent=parent ;
		for(i=0;i<number_of_reasons;i++){
			new_dev->attributes[i].attr.name = source->get_reasons_name(source,i);/*to consider should we copy strings*/
			new_dev->attributes[i].attr.mode = 0444 ;	
			new_dev->attributes[i].show = show_name ; 
		}
		list_add(&new_dev->reason_source_list,&startup_reasons);
		return_value =kobject_set_name(&dev->kobj, "%s", source->name);
		if (!return_value) {
				return_value = device_add(dev);
		}
		else
		{
		        dev_dbg("exiting %s kobject_set_name failed \n",__FUNCTION__);
		}
		if (!return_value){
			for (i=0;i<number_of_reasons;i++){
				return_value=device_create_file(dev, &new_dev->attributes[i]);
				}
		}
		else	{
			source->dev=NULL ;
			kfree(dev);
			kfree(new_dev->attributes) ;
                       	kfree(new_dev);
			dev_dbg("exiting %s device_add or kobject_set_name failed\n ",__FUNCTION__);
                        return return_value ;
		}	
	}
	return return_value ;
}

EXPORT_SYMBOL(register_startup_reasons);

void unregister_startup_reasons(struct startup_reason_source * source)
{
	struct startup_reason_dev * device_to_remove=NULL ;
	struct startup_reason_dev * cursor ;
	int i;
	list_for_each_entry(cursor,&startup_reasons,reason_source_list)
	{
		if (cursor->source==source){
			device_to_remove = cursor ;
			break;
		}
	}
	if(device_to_remove)
	{
		list_del(&device_to_remove->reason_source_list);
		for(i=0;i<device_to_remove->attribute_count;i++) {
			device_remove_file(device_to_remove->dev, &device_to_remove->attributes[i]);
		}
		device_unregister(device_to_remove->dev);
		kfree(device_to_remove->attributes);
		kfree(device_to_remove);
	}
}

EXPORT_SYMBOL(unregister_startup_reasons);

static int startup_reason_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	int ret_value = 0;
	int i ;
	struct startup_reason_dev * reasons  = dev_get_drvdata(dev);
	if (!reasons||!reasons->dev) {
		dev_dbg("failed to see powerup_reason\n");
		return ret_value ;
	}
	ret_value = add_uevent_var(env, "STARTUP_DEVICE=%s", reasons->source->name);
	for (i=0;i<reasons->attribute_count;i++){
		ret_value = add_uevent_var(env, "%s=1",reasons->attributes[i].attr.name);
	}
	return ret_value ;
}

static int __init startup_reason_class_init(void)
{
	startup_reason_class = class_create(THIS_MODULE,"startup_reason");
	if (IS_ERR(startup_reason_class))
		return PTR_ERR(startup_reason_class) ;
	//startup_reason_class->dev_uevent = startup_reason_uevent ;
	/*todo set up class attributes */
	startup_reason_class->dev_uevent=startup_reason_uevent ;
	return 0;
}


static void __exit startup_reason_class_exit(void)
{
	class_destroy(startup_reason_class);

}

subsys_initcall(startup_reason_class_init);
module_exit(startup_reason_class_exit);

MODULE_DESCRIPTION("Reports how system was started");
MODULE_AUTHOR("Andrew Roca ");
MODULE_LICENSE("GPL");

