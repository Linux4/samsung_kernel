/*
 *  kernel/driver/char/modem_interface/boot_protocol.c
 *
 *  Generic modem boot protocol
 *
 *  Author:     Jiayong Yang(Jiayong.Yang@spreadtrum.com)
 *  Created:    Jul 27, 2012
 *  Copyright:  Spreadtrum Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
/* #define DEBUG */
/* #define VERBOSE_DEBUG */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/irqflags.h>
#include "modem_buffer.h"
#include "modem_interface_driver.h"

#define SETUP_PACKET_SIZE 8
#define ACK_PACKET_SIZE 6
typedef struct dl_setup_packet{
	unsigned short	data_packet_size;
	unsigned short  data_type;
	unsigned short  data_check_sum;
	unsigned short  setup_check_sum;
}	SETUP_PACKET_T;
typedef enum   MBUS_DL_status{
	MBUS_DL_IDLE,
	MBUS_DL_WAIT_REQ,
	MBUS_DL_WAIT_RTS,
	MBUS_DL_SETUP,
	MBUS_DL_SETUP_COMP,
	MBUS_DL_DATA,
	MBUS_DL_DATA_COMP,
	MBUS_DL_ACK
}DL_STATUS_E;


extern void modem_intf_ctrl_gpio_handle_boot(int status);
extern int dloader_abort();

char *boot_status_string(DL_STATUS_E status)
{
	switch(status){
		case MBUS_DL_IDLE: return "DL_IDLE";
		case MBUS_DL_WAIT_REQ: return "DL_WAIT_REQ";
		case MBUS_DL_WAIT_RTS: return "DL_WAIT_RTS";
		case MBUS_DL_SETUP: return "DL_SETUP";
		case MBUS_DL_SETUP_COMP: return "DL_SETUP_COMP";
		case MBUS_DL_DATA: return "DL_DATA";
		case MBUS_DL_DATA_COMP: return "DL_DATA_COMP";
		case MBUS_DL_ACK: return "DL_ACK";
		default: break;
	}
	return "UNKNOWN status";
}



static void boot_idle(struct modem_message_node *msg,struct modem_intf_device *device)
{
	int send_bffer_index=0xFF;
	switch(msg->type){
		case MODEM_SLAVE_RTS:
			device->status = (int)MBUS_DL_WAIT_REQ;
		break;
		case MODEM_TRANSFER_REQ:
			send_bffer_index = pingpang_buffer_send(&device->send_buffer);
			if (send_bffer_index!= 0xFF) {
				device->out_transfering = send_bffer_index;
				device->status = (int)MBUS_DL_WAIT_RTS;
			}
		break;
		case MODEM_SET_MODE:
            modem_intf_set_mode(msg->parameter2, 1);
			device->status = (int)MBUS_DL_IDLE;
			device->out_transfering = 0;
                        dloader_abort();
		break;
		default:
		break;
	}
}
static void boot_wait_request(struct modem_message_node *msg,struct modem_intf_device *device)
{
	int send_bffer_index=0xFF;
	switch(msg->type){
		case MODEM_TRANSFER_REQ:
			send_bffer_index = pingpang_buffer_send(&device->send_buffer);
			if(send_bffer_index!= 0xFF){
				device->out_transfering = send_bffer_index;
				device->op->write(device->send_buffer.buffer[send_bffer_index].addr,SETUP_PACKET_SIZE);
				device->status = (int)MBUS_DL_SETUP;
			}
		break;
		case MODEM_SET_MODE:
			modem_intf_set_mode(msg->parameter2, 1);
			device->status = (int)MBUS_DL_IDLE;
			device->out_transfering = 0;
                        dloader_abort();
		break;
		case MODEM_TRANSFER_END:
		break;
		default:
		break;
	}
}
static void boot_wait_RTS(struct modem_message_node *msg,struct modem_intf_device *device)
{
	int send_bffer_index=0xFF;
	unsigned short *data;
	switch(msg->type){
		case MODEM_SLAVE_RTS:
			if(device->out_transfering < 2){
				send_bffer_index = device->out_transfering;
				data = (unsigned short *)device->send_buffer.buffer[send_bffer_index].addr;
				device->op->write((char *)data,SETUP_PACKET_SIZE);
				device->status = (int)MBUS_DL_SETUP;
			}
		break;
		case MODEM_TRANSFER_REQ:
			device->out_transfer_pending = 1;
		break;
		case MODEM_SET_MODE:
			modem_intf_set_mode(msg->parameter2, 1);
			device->status = (int)MBUS_DL_IDLE;
			device->out_transfering = 0;
                        dloader_abort();
		break;
		default:
		break;
	}
}
static void boot_setup(struct modem_message_node *msg,struct modem_intf_device *device)
{
	int send_bffer_index=0xFF;
	char *buffer;
	int size;
	switch(msg->type){
		case MODEM_TRANSFER_END:
			device->status = (int)MBUS_DL_SETUP_COMP;
		break;
		case MODEM_TRANSFER_REQ:
			device->out_transfer_pending = 1;
		break;
		case MODEM_SLAVE_RTS:
			if(device->out_transfering < 2){
				send_bffer_index = device->out_transfering;
				buffer = &device->send_buffer.buffer[send_bffer_index].addr[SETUP_PACKET_SIZE];
				size = device->send_buffer.buffer[send_bffer_index].write_point;
				if (size <= SETUP_PACKET_SIZE)
					size =  12;
					device->op->write(buffer,size - SETUP_PACKET_SIZE);
					pingpang_buffer_send_complete(&device->send_buffer,device->out_transfering);
					device->out_transfering = 0xFF;
					device->status = (int)MBUS_DL_DATA;
				}
				if(device->out_transfer_pending){
					send_bffer_index = pingpang_buffer_send(&device->send_buffer);
					if(send_bffer_index!= 0xFF){
						device->out_transfering = send_bffer_index;
						size = device->send_buffer.buffer[send_bffer_index].write_point;
						device->op->write(device->send_buffer.buffer[send_bffer_index].addr,size);
					}
					device->out_transfer_pending = 0;
				}
		break;
		case MODEM_SET_MODE:
			modem_intf_set_mode(msg->parameter2, 1);
			device->status = (int)MBUS_DL_IDLE;
			device->out_transfering = 0;
                        dloader_abort();
		break;
		default:
		break;
	}
}

static void boot_setup_comp(struct modem_message_node *msg,struct modem_intf_device *device)
{
	int send_bffer_index=0xFF;
	char *buffer;
	int size;

	switch(msg->type){
		case MODEM_SLAVE_RTS:
			if(device->out_transfering < 2){
				send_bffer_index = device->out_transfering;
				buffer = &device->send_buffer.buffer[send_bffer_index].addr[SETUP_PACKET_SIZE];
				size = device->send_buffer.buffer[send_bffer_index].write_point;
				if (size <= SETUP_PACKET_SIZE)
					size =  12;
				device->op->write(buffer,size - SETUP_PACKET_SIZE);
				device->status = (int)MBUS_DL_DATA;
				if(device->out_transfer_pending){
					pingpang_buffer_send_complete(&device->send_buffer,device->out_transfering);
					device->out_transfering = 0xFF;
					send_bffer_index = pingpang_buffer_send(&device->send_buffer);
					if(send_bffer_index!= 0xFF){
						device->out_transfering = send_bffer_index;
						size = device->send_buffer.buffer[send_bffer_index].write_point;
						device->op->write(device->send_buffer.buffer[send_bffer_index].addr,size);
					}
					device->out_transfer_pending = 0;
				}
			}
		break;
		case MODEM_TRANSFER_REQ:
			device->out_transfer_pending = 1;
		break;
		case MODEM_SET_MODE:
			modem_intf_set_mode(msg->parameter2, 1);
			device->status = (int)MBUS_DL_IDLE;
			device->out_transfering = 0;
                        dloader_abort();
		break;
		default:
		break;

	}
}
static void boot_data(struct modem_message_node *msg,struct modem_intf_device *device)
{
	unsigned short *data;
	int send_bffer_index=0xFF;
	char *buffer;
	int size;
	switch(msg->type){
		case MODEM_TRANSFER_END:
			if(device->out_transfering < 2){
				pingpang_buffer_send_complete(&device->send_buffer,device->out_transfering);
				device->out_transfering = 0xFF;
				device->status = (int)MBUS_DL_DATA_COMP;
			}
		break;
		case MODEM_TRANSFER_REQ:
			if(device->out_transfering < 2){
				pingpang_buffer_send_complete(&device->send_buffer,device->out_transfering);
				device->out_transfering = 0xFF;
			}
			send_bffer_index = pingpang_buffer_send(&device->send_buffer);
			if(send_bffer_index!= 0xFF){
				device->out_transfering = send_bffer_index;
				size = device->send_buffer.buffer[send_bffer_index].write_point;
				device->op->write(device->send_buffer.buffer[send_bffer_index].addr,size);
			}
		break;
	        case MODEM_SLAVE_RTS:
			pingpang_buffer_send_complete(&device->send_buffer,device->out_transfering);
                        device->op->read(device->recv_buffer.buffer[1].addr,8);
			device->out_transfering = 0xFF;
			data = (unsigned short *)device->recv_buffer.buffer[1].addr;
                        device->status = (int)MBUS_DL_ACK;
			if(device->out_transfer_pending){
				send_bffer_index = pingpang_buffer_send(&device->send_buffer);
				if(send_bffer_index!= 0xFF){
					device->out_transfering = send_bffer_index;
					size = device->send_buffer.buffer[send_bffer_index].write_point;
					device->op->write(device->send_buffer.buffer[send_bffer_index].addr,size);
				}
				device->out_transfer_pending = 0;
			}
		break;
		case MODEM_SET_MODE:
			modem_intf_set_mode(msg->parameter2, 1);
			device->status = (int)MBUS_DL_IDLE;
			device->out_transfering = 0;
                        dloader_abort();
		break;
		default:
		break;
	}
}
static void boot_data_comp(struct modem_message_node *msg,struct modem_intf_device *device)
{
	unsigned short *data;
	switch(msg->type){
		case MODEM_SLAVE_RTS:
			device->op->read(device->recv_buffer.buffer[1].addr,8);
			data = (unsigned short *)device->recv_buffer.buffer[1].addr;
			device->status = (int)MBUS_DL_ACK;
		break;
		case MODEM_TRANSFER_REQ:
			device->out_transfer_pending = 1;
		break;
		case MODEM_SET_MODE:
			modem_intf_set_mode(msg->parameter2, 1);
			device->status = (int)MBUS_DL_IDLE;
			device->out_transfering = 0;
                        dloader_abort();
		break;
		default:
		break;
	}
}
static void boot_ack(struct modem_message_node *msg,struct modem_intf_device *device)
{
	int send_bffer_index=0xFF;
	unsigned short *data;

	switch(msg->type){
		case MODEM_TRANSFER_END:
			save_to_receive_buffer(&device->recv_buffer,device->recv_buffer.buffer[1].addr,ACK_PACKET_SIZE);
			if(device->out_transfer_pending){
				device->out_transfer_pending = 0;
				device->out_transfering = send_bffer_index = pingpang_buffer_send(&device->send_buffer);
				if(device->out_transfering!=0xff){
					device->status = (int)MBUS_DL_WAIT_RTS;
					break;
				}
			}
			device->status = (int)MBUS_DL_IDLE;
		break;
		case MODEM_TRANSFER_REQ:
			device->out_transfer_pending = 1;
		break;
		case MODEM_SLAVE_RTS:
			data = (unsigned short *)device->recv_buffer.buffer[1].addr;
			save_to_receive_buffer(&device->recv_buffer,data,ACK_PACKET_SIZE);
			if(device->out_transfer_pending){
				device->out_transfer_pending = 0;
				device->out_transfering = send_bffer_index = pingpang_buffer_send(&device->send_buffer);

				data = (unsigned short *)device->send_buffer.buffer[send_bffer_index].addr;
				device->op->write((char *)data,SETUP_PACKET_SIZE);

				device->status = (int)MBUS_DL_SETUP;
			} else {
				device->status = (int)MBUS_DL_WAIT_REQ;
			}
		break;
		case MODEM_SET_MODE:
			modem_intf_set_mode(msg->parameter2, 1);
			device->status = (int)MBUS_DL_IDLE;
			device->out_transfering = 0;
                        dloader_abort();
		break;
		default:
		break;
	}
}
extern unsigned long sprd_get_system_tick(void);
extern void dloader_record_timestamp(unsigned long time);

void boot_protocol_process_dl_messages(struct modem_intf_device *device,
    struct modem_message_node *msg)
{
    DL_STATUS_E	status;

    status =(DL_STATUS_E) device->status;

    switch(status){
		case MBUS_DL_IDLE:
			boot_idle(msg,device);
		break;
		case MBUS_DL_SETUP:
			boot_setup(msg,device);
		break;
		case MBUS_DL_WAIT_REQ:
			boot_wait_request(msg,device);
		break;
		case MBUS_DL_WAIT_RTS:
			boot_wait_RTS(msg,device);
		break;
		case MBUS_DL_SETUP_COMP:
			boot_setup_comp(msg,device);
		break;
		case MBUS_DL_DATA:
			boot_data(msg,device);
		break;
		case MBUS_DL_DATA_COMP:
			boot_data_comp(msg,device);
		break;
		case MBUS_DL_ACK:
			boot_ack(msg,device);
		break;
	}
}

void boot_protocol(struct modem_message_node *msg)
{
	struct modem_intf_device *device;

	if((msg == NULL)||(msg->parameter1==0))
		return;

	device = (struct modem_intf_device *)msg->parameter1;

	printk(">>boot entry(%d,%d) status: %s message: %s \n",
        device->out_transfer_pending,device->out_transfering,
        boot_status_string((DL_STATUS_E )device->status),
        modem_intf_msg_string(msg->type));

    //filter messages
    switch(msg->type){
        case MODEM_CTRL_GPIO_CHG:
            modem_intf_ctrl_gpio_handle_boot(msg->parameter2);
            break;
        case MODEM_USER_REQ:
            //now, just ignore user req in boot mode
            break;
        case MODEM_BOOT_CMP:
            modem_intf_set_mode(MODEM_MODE_BOOTCOMP, 0);
            break;
        default:
            //all other messages
            boot_protocol_process_dl_messages(device, msg);
            break;
    }

}
