/* driver/char/modem_interface/modem_interface_drv.c
 *
 * modem interface API for dloader driver and API for application layer
 *
 * Copyright (C) 2012 Spreadtrum
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <mach/modem_interface.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include "modem_buffer.h"
#include "modem_interface_driver.h"
#include <mach/globalregs.h>
#include <linux/kthread.h>

#define		MAX_MSG_NODE_NUM	32
#define		BUSY_BIT		0x8000

extern int      get_alive_status(void);
extern int      get_assert_status(void);
extern int      modem_gpio_uninit(void *para);
extern struct modem_device_operation	*modem_sdio_drv_init(void);
extern int    modem_gpio_init(void *para);
extern void   modem_poweron(void);
extern void   modem_poweroff(void);
extern void   Set_modem_status(unsigned char modem_status);
static struct modem_intf_device *modem_intf_device = NULL;

static struct modem_message_node	*msg_nodes=NULL;
static atomic_t				nodes_count;
static int				lost_msg_count;
static LIST_HEAD(msg_list);
static struct semaphore			msg_list_sem;
static struct semaphore			modem_status_sem;
static int    modem_gpio_status=0;
wait_queue_head_t mdoem_mode_normal;

void     modem_intf_state_change(int alive_status,int assert_status);
struct modem_device_operation	*devices_op[8] ={NULL};
int     get_modem_alive_status(void)
{
        if(modem_intf_device == NULL)
                return 0;
        else if(modem_intf_device->mode != MODEM_MODE_NORMAL)
                return 0;
        return get_alive_status();
}

char *modem_intf_msg_string(enum MODEM_MSG_type msg)
{
	switch(msg){
		case MODEM_TRANSFER_REQ: return "MODEM_TRANSFER_REQ";
		case MODEM_TRANSFER_END: return "MODEM_TRANSFER_END";
		case MODEM_SLAVE_RTS: return "MODEM_SLAVE_RTS";
		case MODEM_SLAVE_RDY: return "MODEM_SLAVE_RDY";
		case MODEM_SET_MODE: return "MODEM_SET_MODE";
		default:break;
	}
	 return "UNKNOWN MSG";
}
static void modem_intf_register_device_operation(struct modem_device_operation	*op)
{
	int i;
	for(i=0;i<8;i++){
		if(devices_op[i] == NULL)
			devices_op[i] = op;
	}
}
static struct modem_device_operation *modem_intf_get_device_operation(enum MODEM_device_type dev)
{
        int i;
        for(i=0;i<8;i++){
                if((devices_op[i] != NULL)&&(devices_op[i]->dev == dev))
                        return devices_op[i];
        }
	return NULL;
}
static int init_msg_nodes(void)
{
	int i;

	if(msg_nodes)
		return 0;

	msg_nodes = kzalloc(sizeof(struct modem_message_node)*MAX_MSG_NODE_NUM, GFP_KERNEL);
	if (msg_nodes == NULL)
		return -ENOMEM;

	atomic_set(&nodes_count,0);
	for (i=0;i<MAX_MSG_NODE_NUM;i++) {
		msg_nodes[i].buno = i;
	}
	lost_msg_count = 0;
	atomic_set(&nodes_count,i);
	return 0;
}

static struct modem_message_node * find_msg_node(void)
{
	unsigned long flags;
	int i;

	if(atomic_read(&nodes_count) == 0)
		return NULL;
	local_irq_save(flags);
	for (i=0;i<MAX_MSG_NODE_NUM;i++) {
		if ((msg_nodes[i].buno & BUSY_BIT )==0) {
			atomic_dec(&nodes_count);
			msg_nodes[i].buno |= BUSY_BIT;
			break;
		}
	}
	local_irq_restore(flags);
	if(i==MAX_MSG_NODE_NUM)
		return NULL;
	return &msg_nodes[i];
}

static void free_msg_node(struct modem_message_node *msg)
{
	if (msg) {
		msg->buno &=  ~BUSY_BIT;
		atomic_inc(&nodes_count);
	}
}

static struct modem_message_node * modem_intf_get_message( void )
{
	struct list_head *entry;
	struct modem_message_node *msg;
	unsigned long flags;

	do{
		if(!list_empty(&msg_list))
			break;
		down(&msg_list_sem);
	}while(1);

	if(!list_empty(&msg_list)){
		entry = msg_list.next;
		local_irq_save(flags);
		list_del_init(entry);
		local_irq_restore(flags);
		msg = (struct modem_message_node *)list_entry(entry,struct modem_message_node,link);
		return (struct modem_message_node *)msg;
	}
	return NULL;
}

static void modem_send_message( struct modem_message_node *msg )
{
	unsigned long flags;
	local_irq_save(flags);
	list_add_tail(&msg->link,&msg_list);
	local_irq_restore(flags);
	up(&msg_list_sem);
}

static int modem_protocol_thread(void *data)
{
	struct modem_message_node *msg;

	while(1){
		msg = modem_intf_get_message();
		if(msg == NULL)
			continue;
		switch(modem_intf_device->mode){
			case MODEM_MODE_BOOT:
				boot_protocol(msg);
			break;
			case MODEM_MODE_NORMAL:
				normal_protocol(msg);
			break;
			case MODEM_MODE_DUMP:
				dump_memory_protocol(msg);
			break;
			default:
			break;
		}
		free_msg_node(msg);
	}
	return 0;
}

int modem_intf_open(enum MODEM_Mode_type mode,int index)
{
	int retval=0;

	if(modem_intf_device == NULL)
		return -1;
	printk("boot gpio = %d\n",modem_intf_device->modem_config.modem_boot_gpio);
	if(modem_intf_device->op == NULL){
		modem_intf_device->op = modem_intf_get_device_operation(modem_intf_device->modem_config.dev_type);
		printk("modem_intf_open: op = %x\n",modem_intf_device->op);
		if(modem_intf_device->op == NULL)
			return -1;
	}
        modem_intf_device->mode = mode;
        printk("modem_intf_open: mode = %d\n",modem_intf_device->mode);
        if((modem_intf_device->op)&&(modem_intf_device->mode == MODEM_MODE_BOOT))
		modem_intf_device->op->open(NULL);
	return retval;
}

int modem_intf_read(char *buffer, int size,int index)
{
	int read_len;

	if(modem_intf_device->mode == MODEM_MODE_UNKNOWN)
		return 0;

	read_len = pingpang_buffer_read(&modem_intf_device->recv_buffer,buffer,size);
	return read_len;
}

int modem_intf_write(char *buffer, int size,int index)
{
	struct modem_message_node *msg;

	if(modem_intf_device->mode == MODEM_MODE_UNKNOWN)
		return 0;
	msg = find_msg_node();
	if (msg == NULL) {
		lost_msg_count++;
		return -1;
	}
        if(buffer  && size )
               pingpang_buffer_write(&modem_intf_device->send_buffer,buffer,size);
	msg->type = MODEM_TRANSFER_REQ;
	msg->parameter1 = (int)modem_intf_device;
	msg->parameter2 = 0;
	modem_send_message(msg);
	return size;
}

void modem_intf_set_mode(enum MODEM_Mode_type mode,int index)
{
	struct modem_message_node *msg;
	if(modem_intf_device->mode == MODEM_MODE_UNKNOWN)
		return ;

	printk(KERN_INFO "modem_intf_set_mode: %d\n", modem_intf_device->mode);
	if(modem_intf_device->mode == MODEM_MODE_BOOT){
		modem_intf_device->mode = MODEM_MODE_NORMAL;
		modem_gpio_uninit(&(modem_intf_device->modem_config));
	} else if(modem_intf_device->mode == MODEM_MODE_DUMP)
		modem_intf_device->mode = MODEM_MODE_BOOT;
}

void modem_intf_send_channel_message(int dir,int para,int index)
{
	struct modem_message_node *msg;

	if(modem_intf_device->mode == MODEM_MODE_UNKNOWN)
		return ;
	if(modem_intf_device->mode != MODEM_MODE_NORMAL)
		return;
	msg = find_msg_node();
	if (msg == NULL) {
		lost_msg_count++;
		return ;
	}

	msg->src  = SRC_SPI;
	msg->parameter1 = (int)modem_intf_device;
	msg->parameter2 = dir;
	msg->type = MODEM_TRANSFER_END;
	modem_send_message(msg);
}

void modem_intf_send_GPIO_message(int gpio_no,int status,int index)
{
	struct modem_message_node *msg;

	pr_info("GPIO_MSG: mode = %d gpio=%d status = %d\n",modem_intf_device->mode,gpio_no,status);
	if(modem_intf_device->mode == MODEM_MODE_UNKNOWN)
		return ;
	msg = find_msg_node();
	if (msg == NULL) {
		lost_msg_count++;
		printk("modem_intf_send_GPIO_message MSG LOST!!!!!!\n");
		return ;
	}
	msg->src  = SRC_GPIO;
	msg->parameter1 = (int)modem_intf_device;
	msg->parameter2 = gpio_no|(status << 16);
	if(modem_intf_device->mode == MODEM_MODE_NORMAL){
		if (gpio_no == modem_intf_device->modem_config.modem_boot_gpio){
			msg->type = MODEM_SLAVE_RDY;
		} else  if (gpio_no == modem_intf_device->modem_config.modem_crash_gpio){
			msg->type = MODEM_SLAVE_RTS;
		}
	} else if(modem_intf_device->mode == MODEM_MODE_BOOT) {
		if ((gpio_no == modem_intf_device->modem_config.modem_boot_gpio)){
			if ( status != 0)
				msg->type = MODEM_SLAVE_RTS;
			 else
				msg->type = MODEM_TRANSFER_END;
		}
	} else if(modem_intf_device->mode == MODEM_MODE_DUMP){
		if (gpio_no == modem_intf_device->modem_config.modem_crash_gpio){
			int	alive_status = get_alive_status();
			int	assert_status = get_assert_status();
			modem_intf_state_change(alive_status,assert_status);
		}
	}else{
		free_msg_node(msg);
	 	return;
	}
	modem_send_message(msg);
}
static int modem_intf_driver_remove( struct platform_device *_dev)
{
	return 0;
}

/**
 * This function shows the modem state
 */
static ssize_t modem_intf_state_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	int ret;
	printk("call modem_intf_state_show\n");
	ret = down_interruptible(&modem_status_sem);
	if(ret == -EINTR)
		return ret;
	printk("event available:%d ...\n",modem_gpio_status);
	snprintf(buf,3,"%d\n",modem_gpio_status);
	printk("state = %s\n",buf);
	return strlen(buf);
}

extern void  mux_ipc_enable(u8  is_enable);
/**
 * This function update momdem state
 */
void     modem_intf_state_change(int alive_status,int assert_status)
{
	int status = (alive_status|(assert_status<<1));

	if(status == modem_gpio_status)
		return;
	printk("modem_intf_state_change :(%d,%d)\n",alive_status,assert_status);
        if((assert_status == 0) && (alive_status == 1)){
		//mux_ipc_enable(1);
		printk("wake_up_interruptible: modem_intf_device->mode = 0x%x !\n",modem_intf_device->mode);
		wake_up_interruptible(&mdoem_mode_normal);
        }
        else if((assert_status == 1)||(alive_status == 0)){
		if(assert_status == 1)
			//mux_ipc_enable(0);
		if(modem_intf_device->mode == MODEM_MODE_NORMAL){
			modem_intf_device->mode = MODEM_MODE_DUMP;
			modem_intf_write(NULL,0,0);
			printk("start dump process!!!\n");
		}
	}

	modem_gpio_status = (alive_status|(assert_status<<1));
	up(&modem_status_sem);
	return;
}

void  wait_modem_normal(void)
{
    wait_event_interruptible(mdoem_mode_normal, ((MODEM_MODE_NORMAL == modem_intf_device->mode) || (MODEM_MODE_DUMP == modem_intf_device->mode)));
}


static ssize_t modem_intf_state_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int status;
	printk("modem_intf_state_store :%d : %s\n",modem_gpio_status,buf);
	status = simple_strtoul(buf, NULL, 16);
	modem_intf_state_change(0,0);
	return 0;
}

DEVICE_ATTR(state, S_IRUGO | S_IWUSR, modem_intf_state_show,modem_intf_state_store);

static int s_modem_poweron = 1;
static ssize_t modem_intf_modempower_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	int count=0;

        if(buf == NULL)
		return 0;
	printk("modempower_show: %d\n",s_modem_poweron);

	if(s_modem_poweron == 0){
		snprintf(buf,4,"off");
		count = 4;
	} else {
		snprintf(buf,3,"on");
		count = 3;
	}
        return count;
}

static ssize_t modem_intf_modempower_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
        int power;

        if(buf == NULL)
		return 0;
        power = simple_strtoul(buf, NULL, 16);
        if (power == 1) {
                modem_poweron();
		s_modem_poweron = 1;
        } else if(power == 0){
                modem_poweroff();
		s_modem_poweron = 0;
        }
        return size;
}

DEVICE_ATTR(modempower, S_IRUGO | S_IWUSR, modem_intf_modempower_show,modem_intf_modempower_store);


static int modem_intf_driver_probe(struct platform_device *_dev)
{
	int retval = 0;
	struct modem_intf_platform_data *modem_config = _dev->dev.platform_data;
	struct modem_intf_device *device;

	dev_dbg(&_dev->dev, "MODEM_INTF_driver_probe(%p)\n", _dev);

	device =  kzalloc(sizeof(*device), GFP_KERNEL);

	if (!device) {
		printk("MODEM device alloc memory failed!!!\n");
		retval = -ENOMEM;
		goto fail;
	}

	memcpy(&device->modem_config,modem_config,sizeof(*modem_config));
	device->send_buffer.type = BUF_SEND;
	device->recv_buffer.type = BUF_RECV;
	if (pingpang_buffer_init(&device->send_buffer)) {
		dev_dbg(&_dev->dev, "send_buffer init failed\n");
		retval = -ENOMEM;
		goto fail;
	}

	if (pingpang_buffer_init(&device->recv_buffer)) {
		dev_dbg(&_dev->dev, "receive_buffer init failed\n");
		pingpang_buffer_free(&device->send_buffer);
		retval = -ENOMEM;
		goto fail;
	}
       // sprd_greg_set_bits(REG_TYPE_GLOBAL, 0x00000040, GR_PIN_CTL);
	device->mode = MODEM_MODE_UNKNOWN;
	modem_intf_device = device;
	modem_intf_device->op = NULL;

	retval = device_create_file(&_dev->dev, &dev_attr_state);
	retval = device_create_file(&_dev->dev, &dev_attr_modempower);
      modem_gpio_init(&modem_intf_device->modem_config);
	modem_intf_register_device_operation(modem_sdio_drv_init());
	modem_gpio_status=0;
	sema_init(&modem_status_sem,1);
	return 0;
fail:
	kfree(device);
	modem_intf_device = NULL;
	return retval;
}
static void modem_intf_driver_shutdown(struct platform_device *_dev)
{
	modem_poweroff();
}

static struct platform_driver modem_intf_driver = {
	.driver		= {
		.name =         "modem_interface",
		.owner = 	THIS_MODULE,
	},

	.probe 	=       modem_intf_driver_probe,
	.remove =       modem_intf_driver_remove,
	.shutdown =     modem_intf_driver_shutdown,

};
static int __init init(void)
{
	int retval = 0;
	struct task_struct * task;
	printk(KERN_INFO "MODEM_INF_DRIVER: version 0.1\n" );

	init_waitqueue_head(&mdoem_mode_normal);
	sema_init(&msg_list_sem,0);
	retval = init_msg_nodes();
	retval = platform_driver_probe(&modem_intf_driver, modem_intf_driver_probe);
	if (retval < 0) {
		printk(KERN_ERR "%s retval=%d\n", __func__, retval);
	}
	if(retval < 0)
		return retval;
	task = kthread_create(modem_protocol_thread, NULL, "ModemIntf");
	if(0 != task)
		wake_up_process(task);
	return retval;
}
module_init(init);

static void __exit cleanup(void)
{
	printk(KERN_DEBUG "MODEM_INTF_driver_cleanup()\n");

	if (modem_intf_device) {
		pingpang_buffer_free(&modem_intf_device->send_buffer);
		pingpang_buffer_free(&modem_intf_device->recv_buffer);
		kfree(modem_intf_device);
		modem_intf_device = NULL;
	}
	platform_driver_unregister(&modem_intf_driver);

	printk(KERN_INFO "MODEM_INTF module removed\n" );
}
module_exit(cleanup);
