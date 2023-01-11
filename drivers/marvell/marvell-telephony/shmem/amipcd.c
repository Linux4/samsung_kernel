/*
    Marvell PXA9XX ACIPC-MSOCKET driver for Linux
    Copyright (C) 2010 Marvell International Ltd.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>

#include "amipcd.h"
#include "shm.h"
#include "portqueue.h"
#include "msocket.h"
#include "pxa_cp_load.h"

struct wakeup_source amipc_wakeup;
/* forward static function prototype, these all are interrupt call-backs */
static u32 amipc_cb_fc(u32 status);
static u32 amipc_cb(u32 status);

/* amipc_init is used to register interrupt call-back function */
int amipc_init(void)
{
	wakeup_source_init(&amipc_wakeup, "amipc_wakeup");

	/* we do not check any return value */
	amipc_eventbind(AMIPC_SHM_PACKET_NOTIFY, amipc_cb);
	amipc_eventbind(AMIPC_RINGBUF_FC, amipc_cb_fc);

	return 0;
}

/* amipc_exit used to unregister interrupt call-back function */
void amipc_exit(void)
{
	amipc_eventunbind(AMIPC_RINGBUF_FC);
	amipc_eventunbind(AMIPC_SHM_PACKET_NOTIFY);

	wakeup_source_trash(&amipc_wakeup);
}

/* new packet arrival interrupt */
static u32 amipc_cb(u32 status)
{
	u32 data = 0;
	u16 event;

	amipc_dataread(AMIPC_SHM_PACKET_NOTIFY, &data, NULL);
	event = data;

	switch (event) {
	case PACKET_SENT:
		shm_packet_send_cb(&shm_rbctl[shm_rb_m3]);
		break;

	case PEER_SYNC:
		shm_peer_sync_cb(&shm_rbctl[shm_rb_m3]);
		break;

	default:
		break;
	}

	return 0;
}

/* flow control interrupt */
static u32 amipc_cb_fc(u32 status)
{
	u32 data = 0;
	u16 event;

	amipc_dataread(AMIPC_RINGBUF_FC, &data, NULL);
	event = data;

	switch (event) {
	case TX_STOP:
		shm_rb_stop_cb(&shm_rbctl[shm_rb_m3]);
		break;

	case TX_RESUME:
		shm_rb_resume_cb(&shm_rbctl[shm_rb_m3]);
		break;

	default:
		break;
	}

	return 0;
}

#ifndef CONFIG_PXA9XX_AMIPC
enum amipc_return_code amipc_setbase(phys_addr_t base_addr, int len)
{
	return AMIPC_RC_OK;
}

enum amipc_return_code amipc_eventbind(u32 user_event,
					     amipc_rec_event_callback cb)
{
	return AMIPC_RC_OK;
}

enum amipc_return_code amipc_eventunbind(u32 user_event)
{
	return AMIPC_RC_OK;
}

enum amipc_return_code amipc_eventset(enum amipc_events user_event,
						int timeout_ms)
{
	return AMIPC_RC_OK;
}

enum amipc_return_code amipc_datasend(enum amipc_events user_event,
				u32 data1, u32 data2, int timeout_ms)
{
	return AMIPC_RC_OK;
}

enum amipc_return_code amipc_dataread(enum amipc_events user_event,
					u32 *data1, u32 *data2)
{
	return AMIPC_RC_OK;
}

enum amipc_return_code amipc_dump_debug_info(void)
{
	return AMIPC_RC_OK;
}
#endif

