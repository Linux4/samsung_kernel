/*
 *  kernel/driver/char/modem_interface/normal_mode.c
 *
 *  Generic modem interface GPIO handshake and dump memory process.
 *
 *  Author:     Jiayong Yang(Jiayong.Yang@spreadtrum.com)
 *  Created:    Jul 27, 2012
 *  Copyright:  Spreadtrum Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include "modem_buffer.h"
#include "modem_interface_driver.h"

extern size_t Read_modem_data_ext(const unsigned char * buf, unsigned short len);

extern void modem_intf_set_mode(enum MODEM_Mode_type mode,int force);
extern void modem_intf_ctrl_gpio_handle_bootcomp(int status);
extern void modem_intf_ctrl_gpio_handle_dump(int status);
extern void modem_intf_ctrl_gpio_handle_normal(int status);
extern void modem_intf_reboot_routine(void);


void shutdown_protocol(struct modem_message_node *msg)
{
        struct modem_intf_device * device;

        if((msg == NULL)||(msg->parameter1==0))
                return;

        printk(KERN_INFO ">>shutdown entry message: %s \n",modem_intf_msg_string(msg->type));

        device = (struct modem_intf_device *)msg->parameter1;

        switch(msg->type) {
        case MODEM_SET_MODE:
                modem_intf_set_mode(msg->parameter2, 1);
                device->status = 0;
                device->out_transfering = 0;
                break;
        default:
                break;
        }
}

void bootcomp_protocol(struct modem_message_node *msg)
{
        struct modem_intf_device * device;

        if((msg == NULL)||(msg->parameter1==0))
                return;

        printk(KERN_INFO ">>bootcomp entry message: %s \n",modem_intf_msg_string(msg->type));

        device = (struct modem_intf_device *)msg->parameter1;

        switch(msg->type) {
        case MODEM_CTRL_GPIO_CHG:
                modem_intf_ctrl_gpio_handle_bootcomp(msg->parameter2);
                break;
        case MODEM_USER_REQ:
                modem_intf_reboot_routine();
                break;
        case MODEM_SET_MODE:
                modem_intf_set_mode(msg->parameter2, 1);
                device->status = 0;
                device->out_transfering = 0;
                break;
        default:
                break;
        }
}


void dump_memory_protocol(struct modem_message_node *msg)
{
        struct modem_intf_device * device;

        if((msg == NULL)||(msg->parameter1==0))
                return;

        printk(KERN_INFO ">>dump entry message: %s \n",modem_intf_msg_string(msg->type));

        device = (struct modem_intf_device *)msg->parameter1;

        switch(msg->type) {
        case MODEM_CTRL_GPIO_CHG:
                modem_intf_ctrl_gpio_handle_dump(msg->parameter2);
                break;
        case MODEM_USER_REQ:
                modem_intf_reboot_routine();
                break;
        case MODEM_SET_MODE:
                modem_intf_set_mode(msg->parameter2, 1);
                device->status = 0;
                device->out_transfering = 0;
                break;
        default:
                break;
        }
}

void reset_protocol(struct modem_message_node *msg)
{
        struct modem_intf_device * device;

        if((msg == NULL)||(msg->parameter1==0))
                return;

        printk(KERN_INFO ">>reset entry message: %s \n",modem_intf_msg_string(msg->type));

        device = (struct modem_intf_device *)msg->parameter1;

        switch(msg->type) {
        case MODEM_SET_MODE:
                modem_intf_set_mode(msg->parameter2, 1);
                device->status = 0;
                device->out_transfering = 0;
                break;
        default:
                break;
        }
}

void normal_protocol(struct modem_message_node *msg)
{
        struct modem_intf_device * device;
        if((msg == NULL)||(msg->parameter1==0))
                return;

        printk(KERN_INFO ">>normal entry message: %s \n",modem_intf_msg_string(msg->type));

        device = (struct modem_intf_device *)msg->parameter1;

        switch(msg->type) {
        case MODEM_CTRL_GPIO_CHG:
                modem_intf_ctrl_gpio_handle_normal(msg->parameter2);
                break;
        case MODEM_USER_REQ:
                modem_intf_reboot_routine();
                break;
        case MODEM_SET_MODE:
                modem_intf_set_mode(msg->parameter2, 1);
                device->status = 0;
                device->out_transfering = 0;
                break;
        default:
                break;
        }
}
