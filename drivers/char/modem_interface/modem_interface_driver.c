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
#include <linux/wakelock.h>
#include <linux/sched.h>
#include<linux/spinlock.h>
#include "modem_buffer.h"
#include "modem_interface_driver.h"
#include <mach/globalregs.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/timer.h>


#define		MAX_MSG_NODE_NUM	32
#define		BUSY_BIT		0x8000
#define         MAX_WAIT_WATCHDOG_TIME  5

extern int      get_alive_status(void);
extern int      get_assert_status(void);
extern int      modem_gpio_uninit(void *para);
extern int      modem_gpio_init(void *para);
extern int      modem_share_gpio_uninit(void *para);
extern int      modem_share_gpio_init(void *para);
extern void  mux_ipc_enable(u8  is_enable);
extern void  modem_gpio_irq_init(void* para);
extern void modem_poweron_async_step1(void);
extern void modem_poweron_async_step2(void);

extern struct modem_device_operation	*modem_sdio_drv_init(void);
extern int    modem_gpio_init(void *para);
extern void   modem_poweron(void);
extern void   modem_poweroff(void);
extern void   Set_modem_status(unsigned char modem_status);
extern void modem_soft_reset(void);
extern void modem_hard_reset(void);
extern void  sdio_ipc_enable(u8  is_enable);
extern void shutdown_protocol(struct modem_message_node *msg);
extern void bootcomp_protocol(struct modem_message_node *msg);
extern void reset_protocol(struct modem_message_node *msg);
extern int modem_intf_get_watchdog_gpio_value();



int modem_intf_send_boot_message(void);
void modem_intf_reboot_routine(void);
void modem_intf_alive_routine(void);
void modem_intf_assert_routine(void);
void modem_intf_wdg_reset_routine(void);


static struct modem_intf_device *modem_intf_device = NULL;

static struct modem_message_node	*msg_nodes=NULL;
static atomic_t				nodes_count;
static int				lost_msg_count;
static LIST_HEAD(msg_list);
static struct semaphore			msg_list_sem;
static struct semaphore			modem_event_sem;
static int    modem_gpio_status[CTRL_GPIO_MAX];
static int  modem_event = MODEM_INTF_EVENT_SHUTDOWN;
static struct wake_lock   s_modem_intf_proc_wake_lock;
static struct wake_lock   s_modem_intf_msg_wake_lock;
static struct wake_lock   s_modem_intf_event_wake_lock;
static struct  timer_list s_modem_watch_timer;



//wait_queue_head_t mdoem_mode_normal;
spinlock_t int_lock;
struct modem_device_operation	*devices_op[8] = {NULL};

/************ modemsts notifier *****************/
static LIST_HEAD(modemsts_chg_handlers);
static DEFINE_MUTEX(modemsts_chg_lock);

/* register a callback function when modem status changed
*  @handler: callback function
*/
int modemsts_notifier_register(struct modemsts_chg *handler)
{

	struct list_head *pos;
	struct modemsts_chg *e;

	mutex_lock(&modemsts_chg_lock);
	list_for_each(pos, &modemsts_chg_handlers) {
		e = list_entry(pos, struct modemsts_chg, link);
		if(e == handler){
			printk("***** %s, %pf already exsited ****\n",
					__func__, e->modemsts_notifier);
			return -1;
		}
	}
	list_for_each(pos, &modemsts_chg_handlers) {
		struct modemsts_chg *e;
		e = list_entry(pos, struct modemsts_chg, link);
		if (e->level > handler->level)
			break;
	}
	list_add_tail(&handler->link, pos);
	mutex_unlock(&modemsts_chg_lock);

	return 0;
}
EXPORT_SYMBOL(modemsts_notifier_register);

/* unregister a callback function for detecting modem status change
*  @handler: callback function
*/
int modemsts_notifier_unregister(struct modemsts_chg *handler)
{
	mutex_lock(&modemsts_chg_lock);
	list_del(&handler->link);
	mutex_unlock(&modemsts_chg_lock);

	return 0;
}
EXPORT_SYMBOL(modemsts_notifier_unregister);

void modemsts_change_notification(unsigned int state)
{
	struct modemsts_chg *pos;

	mutex_lock(&modemsts_chg_lock);
	if(state == MODEM_STATUS_REBOOT) {
		list_for_each_entry(pos, &modemsts_chg_handlers, link) {
			if (pos->modemsts_notifier != NULL) {
				pos->modemsts_notifier(pos, state);
			}
		}
	} else {
		list_for_each_entry_reverse(pos, &modemsts_chg_handlers, link) {
			if (pos->modemsts_notifier != NULL) {
				pos->modemsts_notifier(pos, state);
			}
		}
	}
	mutex_unlock(&modemsts_chg_lock);
}
/************ modem notifier *****************/


char *modem_intf_msg_string(enum MODEM_MSG_type msg)
{
        switch(msg) {
        case MODEM_TRANSFER_REQ:
                return "MODEM_TRANSFER_REQ";
        case MODEM_TRANSFER_END:
                return "MODEM_TRANSFER_END";
        case MODEM_SLAVE_RTS:
                return "MODEM_SLAVE_RTS";
        case MODEM_SLAVE_RDY:
                return "MODEM_SLAVE_RDY";
        case MODEM_SET_MODE:
                return "MODEM_SET_MODE";
        case MODEM_CTRL_GPIO_CHG:
                return "MODEM_CTRL_GPIO_CHG";
        case MODEM_USER_REQ:
                return "MODEM_USER_REQ";
        case MODEM_BOOT_CMP:
                return "MODEM_BOOT_CMP";
        case MODEM_OPEN_DEVICE:
                return "MODEM_OPEN_DEVICE";
        default:
                break;
        }
        return "UNKNOWN MSG";
}
static void modem_intf_register_device_operation(struct modem_device_operation	*op)
{
        int i;
        for(i=0; i<8; i++) {
                if(devices_op[i] == NULL)
                        devices_op[i] = op;
        }
}
static struct modem_device_operation *modem_intf_get_device_operation(enum MODEM_device_type dev) {
        int i;
        for(i=0; i<8; i++) {
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
        for (i=0; i<MAX_MSG_NODE_NUM; i++) {
                msg_nodes[i].buno = i;
        }
        lost_msg_count = 0;
        atomic_set(&nodes_count,i);
        return 0;
}

static struct modem_message_node * find_msg_node(void) {
        unsigned long flags;
        int i;

        if(atomic_read(&nodes_count) == 0)
                return NULL;
        //local_irq_save(flags);
        spin_lock_irqsave(&int_lock, flags);
        for (i=0; i<MAX_MSG_NODE_NUM; i++) {
                if ((msg_nodes[i].buno & BUSY_BIT )==0) {
                        atomic_dec(&nodes_count);
                        msg_nodes[i].buno |= BUSY_BIT;
                        break;
                }
        }
        //local_irq_restore(flags);
        spin_unlock_irqrestore(&int_lock, flags);
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

static struct modem_message_node * modem_intf_get_message( void ) {
        struct list_head *entry;
        struct modem_message_node *msg;
        unsigned long flags;
        do {
                if(!list_empty(&msg_list))
                        break;
                down(&msg_list_sem);
        } while(1);

        if(!list_empty(&msg_list)) {
                entry = msg_list.next;
                //local_irq_save(flags);
                spin_lock_irqsave(&int_lock, flags);
                list_del_init(entry);
                //local_irq_restore(flags);
                spin_unlock_irqrestore(&int_lock, flags);
                msg = (struct modem_message_node *)list_entry(entry,struct modem_message_node,link);
                return (struct modem_message_node *)msg;
        }
        return NULL;
}

static void modem_send_message( struct modem_message_node *msg )
{
        unsigned long flags;

        wake_lock_timeout(&s_modem_intf_msg_wake_lock, HZ / 2);
        //local_irq_save(flags);
        spin_lock_irqsave(&int_lock, flags);
        list_add_tail(&msg->link,&msg_list);
        //local_irq_restore(flags);
        spin_unlock_irqrestore(&int_lock, flags);
        up(&msg_list_sem);
}

static int modem_protocol_thread(void *data)
{
        struct modem_message_node *msg;

        int boot_cpu = smp_processor_id();
        long rc;
        struct sched_param	 param = {.sched_priority = 98};

        current_thread_info()->cpu = boot_cpu;
        rc = sched_setaffinity(current->pid, cpumask_of(boot_cpu));

        sched_setscheduler(current, SCHED_FIFO, &param);

        while(1) {
                msg = modem_intf_get_message();
                if(msg == NULL)
                        continue;
                wake_lock(&s_modem_intf_proc_wake_lock);
                switch(modem_intf_device->mode) {
                case MODEM_MODE_SHUTDOWN:
                        shutdown_protocol(msg);
                        break;
                case MODEM_MODE_BOOT:
                        boot_protocol(msg);
                        break;
                case MODEM_MODE_BOOTCOMP:
                        bootcomp_protocol(msg);
                        break;
                case MODEM_MODE_NORMAL:
                        normal_protocol(msg);
                        break;
                case MODEM_MODE_DUMP:
                        dump_memory_protocol(msg);
                        break;
                case MODEM_MODE_RESET:
                        reset_protocol(msg);
                        break;
                default:
                        break;
                }
                free_msg_node(msg);
                wake_unlock(&s_modem_intf_proc_wake_lock);
        }
        return 0;
}

int modem_intf_open(enum MODEM_Mode_type mode,int index)
{
        int retval=0;

        if(modem_intf_device == NULL)
                return -1;
        printk("boot gpio = %d\n",modem_intf_device->modem_config.modem_boot_gpio);
        if(modem_intf_device->op == NULL) {
                modem_intf_device->op = modem_intf_get_device_operation(modem_intf_device->modem_config.dev_type);
                printk("modem_intf_open: op = %x\n",modem_intf_device->op);
                if(modem_intf_device->op == NULL)
                        return -1;
        }

        if((modem_intf_device->op)&&(mode == MODEM_MODE_BOOT)) {
                if(0 != modem_intf_device->op->open(NULL)) {
                        return -1;
                }
        }

        return retval;
}

int modem_intf_send_bootcomp_message(enum MODEM_Mode_type mode,int index)
{
        struct modem_message_node *msg;

        msg = find_msg_node();
        if (msg == NULL) {
                lost_msg_count++;
                printk("modem_intf_send_bootcomp_message MSG LOST!!!!!!\n");
                return -1;
        }
        msg->src  = SRC_DLOADER;
        msg->parameter1 = (int)modem_intf_device;
        msg->parameter2 = 0;
        msg->type = MODEM_BOOT_CMP;

        modem_send_message(msg);

        return 0;
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

void modem_intf_set_mode(enum MODEM_Mode_type mode,int force)
{

        printk(KERN_INFO "modem_intf_set_mode enter old:%d,new:%d\n",
               modem_intf_device->mode, mode);

        if(modem_intf_device->mode == mode) {
                goto fin;
        }

        if(force) {
                modem_intf_device->mode = mode;
                goto fin;
        }

        switch(modem_intf_device->mode) {
        case MODEM_MODE_BOOT:
                if(MODEM_MODE_NORMAL == mode || MODEM_MODE_BOOTCOMP == mode) {
                        modem_intf_device->mode = mode;
                } else {
                        printk(KERN_ERR "modem_intf: try to enter mode:%d, in MODEM_MODE_BOOT", mode);
                }
                break;
        case MODEM_MODE_BOOTCOMP:
                if(MODEM_MODE_NORMAL == mode) {
                        modem_intf_device->mode = mode;
                } else {
                        printk(KERN_ERR "modem_intf: try to enter mode:%d, in MODEM_MODE_BOOTCOMP", mode);
                }
                break;
        case MODEM_MODE_NORMAL:
                if(MODEM_MODE_DUMP == mode || MODEM_MODE_RESET == mode) {
                        modem_intf_device->mode = mode;
                } else {
                        printk(KERN_ERR "modem_intf: try to enter mode:%d, in MODEM_MODE_NORMAL", mode);
                }
                break;
        case MODEM_MODE_DUMP:
                if(MODEM_MODE_RESET == mode || MODEM_MODE_BOOT == mode) {
                        modem_intf_device->mode = mode;
                } else {
                        printk(KERN_ERR "modem_intf: try to enter mode:%d, in MODEM_MODE_DUMP", mode);
                }
                break;
        default: //MODEM_MODE_RESET
                if(MODEM_MODE_BOOT == mode) {
                        modem_intf_device->mode = mode;
                } else {
                        printk(KERN_ERR "modem_intf: try to enter mode:%d, in MODEM_MODE_RESET", mode);
                }
                break;
        }

fin:
        printk(KERN_INFO "modem_intf_set_mode leave new:%d\n", modem_intf_device->mode);
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

void modem_intf_send_ctrl_gpio_message(enum MSG_CTRL_GPIO_TYPE gpio_type, int gpio_value)
{
        struct modem_message_node *msg;

        printk("ctrl_gpio: gpio_type = %d value = %d\n", gpio_type, gpio_value);

        if(modem_gpio_status[gpio_type] == gpio_value) {
                return;
        }

        modem_gpio_status[gpio_type] = gpio_value;

        //ctrl gpio only effective in these modem mode
        if(modem_intf_device->mode != MODEM_MODE_BOOT &&
            modem_intf_device->mode != MODEM_MODE_BOOTCOMP &&
            modem_intf_device->mode != MODEM_MODE_NORMAL &&
            modem_intf_device->mode != MODEM_MODE_DUMP) {

                return;
        }

        msg = find_msg_node();
        if (msg == NULL) {
                lost_msg_count++;
                printk("modem_intf_send_ctrl_gpio_message MSG LOST!!!!!!\n");
                return ;
        }
        msg->src  = SRC_GPIO;
        msg->parameter1 = (int)modem_intf_device;
        msg->parameter2 = ((gpio_type << 16) | gpio_value);
        msg->type = MODEM_CTRL_GPIO_CHG;

        modem_send_message(msg);
}

void modem_intf_send_user_req_message(int req)
{
        struct modem_message_node *msg;

retry:
        msg = find_msg_node();
        if (msg == NULL) {
                lost_msg_count++;
                printk("modem_intf_send_user_reboot_req_message MSG LOST!!!!!!\n");

                msleep(100);
                printk("modem_intf_send_user_reboot_req_message retry!!!!!!\n");
                goto retry;
        }
        msg->src  = SRC_USER;
        msg->parameter1 = (int)modem_intf_device;
        msg->parameter2 = req;
        msg->type = MODEM_USER_REQ;

        modem_send_message(msg);
}


int modem_intf_send_boot_message()
{
        struct modem_message_node *msg;

retry:
        msg = find_msg_node();
        if (msg == NULL) {
                lost_msg_count++;
                printk("modem_intf_send_boot_message MSG LOST retry!!!!!!\n");

                msleep(100);
                goto retry;
        }
        msg->src  = SRC_DLOADER;
        msg->parameter1 = (int)modem_intf_device;
        msg->parameter2 = MODEM_MODE_BOOT;
        msg->type = MODEM_SET_MODE;

        modem_send_message(msg);

        return 0;
}

int modem_intf_send_shutdown_message()
{
        struct modem_message_node *msg;

        msg = find_msg_node();
        if (msg == NULL) {
                lost_msg_count++;
                printk("modem_intf_send_shutdown_message MSG LOST !!!!!!\n");

                return 0;
        }
        msg->src  = SRC_DLOADER;
        msg->parameter1 = (int)modem_intf_device;
        msg->parameter2 = MODEM_MODE_SHUTDOWN;
        msg->type = MODEM_SET_MODE;

        modem_send_message(msg);

        return 0;
}




void modem_intf_send_GPIO_message(int gpio_no,int status,int index)
{
        struct modem_message_node *msg;

        printk("GPIO_MSG: mode = %d gpio=%d status = %d\n",modem_intf_device->mode,gpio_no,status);
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
        if(modem_intf_device->mode == MODEM_MODE_NORMAL) {
                if (gpio_no == modem_intf_device->modem_config.modem_boot_gpio) {
                        msg->type = MODEM_SLAVE_RDY;
                } else  if (gpio_no == modem_intf_device->modem_config.modem_crash_gpio) {
                        msg->type = MODEM_SLAVE_RTS;
                } else  if (gpio_no == modem_intf_device->modem_config.modem_alive_gpio) {
                        msg->type = MODEM_SLAVE_RTS;
                }
        } else if(modem_intf_device->mode == MODEM_MODE_BOOT) {
                if ((gpio_no == modem_intf_device->modem_config.modem_boot_gpio)) {
                        if ( status != 0)
                                msg->type = MODEM_SLAVE_RTS;
                        else
                                msg->type = MODEM_TRANSFER_END;
                }
        } else if(modem_intf_device->mode == MODEM_MODE_DUMP) {
                if (gpio_no == modem_intf_device->modem_config.modem_crash_gpio) {
                        msg->type = MODEM_SLAVE_RTS;
                }
        } else {
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
        ret = down_interruptible(&modem_event_sem);
        if(ret == -EINTR)
                return ret;
        printk("event available:%d ...\n",modem_event);
        snprintf(buf,4,"%d\n",modem_event);
        printk("event = %s\n",buf);
        return strlen(buf);
}

extern void  mux_ipc_enable(u8  is_enable);


void modem_intf_send_event(int event)
{
        //user space process should wait for modem_event_sem
        wake_lock_timeout(&s_modem_intf_event_wake_lock, HZ / 2);
        modem_event = event;
        up(&modem_event_sem);
}

void modem_intf_cp_reset_req_routine()
{
    modem_intf_send_event(MODEM_INTF_EVENT_CP_REQ_RESET);
}


void modem_intf_reboot_routine()
{
        //disable mux sdio spi
        mux_ipc_enable(0);
        sdio_ipc_enable(0);
        modemsts_change_notification(MODEM_STATUS_REBOOT);
        //modem_share_gpio_init(&(modem_intf_device->modem_config));

        modem_intf_set_mode(MODEM_MODE_RESET, 0);

        modem_intf_send_event(MODEM_INTF_EVENT_FORCE_RESET);
}

void modem_intf_alive_routine()
{
        //msleep(10*1000);
        modem_intf_set_mode(MODEM_MODE_NORMAL, 0);
        modem_share_gpio_uninit(&(modem_intf_device->modem_config));
        sdio_ipc_enable(1);
        modemsts_change_notification(MODEM_STATUS_ALVIE);
        mux_ipc_enable(1);
        modem_intf_send_event(MODEM_INTF_EVENT_ALIVE);
}

void modem_intf_assert_routine()
{
        modemsts_change_notification(MODEM_STATUS_ASSERT);
        modem_intf_send_event(MODEM_INTF_EVENT_ASSERT);
}

void modem_intf_wdg_reset_routine(void)
{
	modem_intf_send_event(MODEM_INTF_EVENT_WDG_RESET);
}

int chk_watchdog_reset()
{
        unsigned long now = jiffies;

        do {
                udelay(10);
                if(1 == modem_intf_get_watchdog_gpio_value())
                {
                        return 1;
                }

        }while((msecs_to_jiffies(jiffies - now) < MAX_WAIT_WATCHDOG_TIME));

        return 0;
}
void modem_intf_ctrl_gpio_handle_normal(int status)
{
        int type, value;

        type = status >> 16;
        value = status & 0xFFFF;

        switch(type) {
        case GPIO_RESET:
                //reset -> 1, ,means need reboot
                if(value) {
                        //modem_intf_assert_routine();
                        modem_intf_set_mode(MODEM_MODE_DUMP, 0);
                        modem_intf_cp_reset_req_routine();
                }
                break;
        case GPIO_ALIVE:
                //alive -> 0,means assert
                if(!value) {
                        //first check if watch dog reset changes cp_alive
                        if(chk_watchdog_reset()) {
                                printk("modem_intf: found watchdog reset in GPIO_ALIVE handle\n");
                                modem_intf_wdg_reset_routine();
                                modem_intf_set_mode(MODEM_MODE_DUMP, 0);
                                break;
                        }
                        modem_intf_assert_routine();
                        modem_intf_set_mode(MODEM_MODE_DUMP, 0);
                }
                break;
        case GPIO_WATCHDOG:
                //watchdog -> 0,means watch dog reset
                if(value) {
                        modem_intf_wdg_reset_routine();
                        modem_intf_set_mode(MODEM_MODE_DUMP, 0);
                }
                break;
        default:
                break;
        }

}

void modem_intf_ctrl_gpio_handle_dump(int status)
{
        int type, value;

        type = status >> 16;
        value = status & 0xFFFF;

        switch(type) {
        case GPIO_RESET:
                //reset -> 1,means need reboot
                if(value) {
                        modem_intf_cp_reset_req_routine();
                }
                break;
        case GPIO_ALIVE:

                break;
        case GPIO_WATCHDOG:

                break;
        default:
                break;
        }

}

void modem_intf_ctrl_gpio_handle_boot(int status)
{
        int type, value;

        type = status >> 16;
        value = status & 0xFFFF;

        switch(type) {
        case GPIO_RESET:
                break;
        case GPIO_ALIVE:
                //alive -> 1, means alive
                if(value) {
                        modem_intf_alive_routine();
                }
                break;
        case GPIO_WATCHDOG:
                break;
        default:
                break;
        }

}

void modem_intf_ctrl_gpio_handle_bootcomp(int status)
{
        int type, value;

        type = status >> 16;
        value = status & 0xFFFF;

        switch(type) {
        case GPIO_RESET:
                break;
        case GPIO_ALIVE:
                //alive -> 1, means alive
                if(value) {
                        modem_intf_alive_routine();
                }
                break;
        case GPIO_WATCHDOG:
                break;
        default:
                break;
        }

}


static ssize_t modem_intf_state_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
        int status;

        printk("modem_intf_state_store :%d : %s\n",modem_gpio_status,buf);
        status = simple_strtoul(buf, NULL, 16);
        //status 0, mean enter boot mode
        if(0 == status) {
                //status 0 means start to boot cp
		modem_share_gpio_init(&(modem_intf_device->modem_config));

                printk("modem_intf_open: modem_intf_send_boot_message mode = %d\n", MODEM_MODE_BOOT);
                modem_intf_send_boot_message();
        } else {
                modem_intf_send_user_req_message(status);
        }

        return 0;
}

DEVICE_ATTR(state, S_IRUGO | S_IWUSR, modem_intf_state_show,modem_intf_state_store);

static int s_modem_poweron = 0;

void on_modem_poweron()
{
    modem_gpio_irq_init(&(modem_intf_device->modem_config));
	modemsts_change_notification(MODEM_STATUS_POWERON);
}

static ssize_t modem_intf_modempower_show(struct device *dev,struct device_attribute *attr, char *buf)
{
        int count=0;

        if(buf == NULL)
                return 0;
        printk("modempower_show: %d\n",s_modem_poweron);

        if(s_modem_poweron == 0) {
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
                if(s_modem_poweron)
                      return size;
			modem_poweron();
			s_modem_poweron = 1;
            on_modem_poweron();
        } else if(power == 0) {
                if(!s_modem_poweron)
                        return size;
                modem_poweroff();
                s_modem_poweron = 0;
        }
        return size;
}

DEVICE_ATTR(modempower, S_IRUGO | S_IWUSR, modem_intf_modempower_show,modem_intf_modempower_store);

static void modem_intf_init_poweron_timeout(unsigned long priv)
{
    modem_poweron_async_step2();
    s_modem_poweron = 1;
    on_modem_poweron();
}

static void modem_intf_init_poweron_modem()
{
    init_timer(&s_modem_watch_timer);
    s_modem_watch_timer.expires = jiffies + (2 * HZ);
    s_modem_watch_timer.data = (unsigned long)NULL;
    s_modem_watch_timer.function = modem_intf_init_poweron_timeout;

    modem_poweron_async_step1();

    add_timer(&s_modem_watch_timer);
}


static ssize_t modem_intf_modemreset_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
        int power;

        if(buf == NULL)
                return 0;
        if(!s_modem_poweron)
                return -1;
        power = simple_strtoul(buf, NULL, 16);
        if (power == 1) {
                modem_soft_reset();
        } else if(power == 2) {
                modem_hard_reset();
        }
        return size;
}

DEVICE_ATTR(modemreset, S_IWUSR, NULL,modem_intf_modemreset_store);

static int modem_intf_driver_probe(struct platform_device *_dev)
{
        int retval = 0;
        int i;
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
        device->mode = MODEM_MODE_SHUTDOWN;
        modem_intf_device = device;
        modem_intf_device->op = NULL;

        for(i = 0; i < CTRL_GPIO_MAX; i++) {
                modem_gpio_status[i] = 0xFF;
        }
        retval = device_create_file(&_dev->dev, &dev_attr_state);
        retval = device_create_file(&_dev->dev, &dev_attr_modempower);
        retval = device_create_file(&_dev->dev, &dev_attr_modemreset);
        modem_gpio_init(&modem_intf_device->modem_config);
        modem_intf_register_device_operation(modem_sdio_drv_init());
        modem_event = MODEM_INTF_EVENT_SHUTDOWN;
        sema_init(&modem_event_sem,1);
        spin_lock_init(&int_lock);
        modem_intf_init_poweron_modem();
        return 0;
fail:
        kfree(device);
        modem_intf_device = NULL;
        return retval;
}

static void modem_intf_driver_shutdown(struct platform_device *_dev)
{
        unsigned long timeout = jiffies + 30 * HZ;

        //wait downloading cp finished
        printk(KERN_ERR "modem_intf: modem_intf_driver_shutdown enter!\n" );
        while(time_after(timeout, jiffies)) {
                if(modem_intf_device->mode == MODEM_MODE_BOOT)
                {
                        printk(KERN_ERR "modem_intf: shutdown found booting cp ...!\n" );
                        msleep(1000);
                } else {
                        break;
                }
        }

        //send message
        printk(KERN_ERR "modem_intf: shutdown modem_intf_send_shutdown_message!\n" );
        modem_intf_send_shutdown_message();

        //power off modem
        printk(KERN_ERR "modem_intf: shutdown modem_poweroff!\n" );
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

        //init_waitqueue_head(&mdoem_mode_normal);
        sema_init(&msg_list_sem,0);
        retval = init_msg_nodes();
        retval = platform_driver_probe(&modem_intf_driver, modem_intf_driver_probe);
        if (retval < 0) {
                printk(KERN_ERR "%s retval=%d\n", __func__, retval);
        }
        if(retval < 0)
                return retval;

        wake_lock_init(&s_modem_intf_proc_wake_lock, WAKE_LOCK_SUSPEND, "modem_if_proc_lock");
        wake_lock_init(&s_modem_intf_msg_wake_lock, WAKE_LOCK_SUSPEND, "modem_if_msg_lock");
        wake_lock_init(&s_modem_intf_event_wake_lock, WAKE_LOCK_SUSPEND, "modem_if_evt_lock");
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
