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
extern void     modem_intf_state_change(int alive_status,int assert_status);
extern int      get_alive_status(void);
extern int      get_assert_status(void);

void dump_memory_protocol(struct modem_message_node *msg)
{


        return;

}
void normal_protocol(struct modem_message_node *msg)
{
	struct modem_intf_device *device;
	if((msg == NULL)||(msg->parameter1==0))
		return;

	device = (struct modem_intf_device *)msg->parameter1;
	printk(KERN_INFO ">>normal entry message: %s \n",modem_intf_msg_string(msg->type));
	{
		int	alive_status = get_alive_status();
		int	assert_status = get_assert_status();
		modem_intf_state_change(alive_status,assert_status);
		return;
	}
}
