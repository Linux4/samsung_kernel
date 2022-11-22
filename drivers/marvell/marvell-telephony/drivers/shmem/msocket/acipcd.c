/*
    acipcd.c Created on: Aug 3, 2010, Jinhua Huang <jhhuang@marvell.com>

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
#include <linux/wakelock.h>
#include <plat/devfreq.h>
#include <linux/pm_qos.h>

#include "acipcd.h"
#include "shm.h"
#include "portqueue.h"
#include "msocket.h"

struct wake_lock acipc_wakeup; //used to ensure Workqueue scheduled.
/* forward static function prototype, these all are interrupt call-backs */
static u32 acipc_cb_rb_stop(u32 status);
static u32 acipc_cb_rb_resume(u32 status);
static u32 acipc_cb_port_fc(u32 status);
static u32 acipc_cb_psd_rb_stop(u32 status);
static u32 acipc_cb_psd_rb_resume(u32 status);
static u32 acipc_cb_psd_cb(u32 status);
static u32 acipc_cb(u32 status);
static u32 acipc_cb_modem_ddrfreq_update(u32 status);
static u32 acipc_cb_diag_cb(u32 status);

extern int g_simcard;
static u32 acipc_cb_event_notify(u32 status);
#define RESET_CP_REQUEST 0x544F4F42
#define RESET_CP_REQUEST_DONE 0x454E4F44
DECLARE_COMPLETION(reset_cp_confirm);

static struct pm_qos_request modem_ddr_cons = {
	.name = "cp",
};
struct workqueue_struct *acipc_wq;
struct work_struct acipc_modem_ddr_freq_update;
static int default_ddr_freq = 0;

#define MODEM_DDRFREQ_HI	400
#define MODEM_DDRFREQ_LOW	156
static void acipc_modem_ddr_freq_update_handler(struct work_struct *work)
{
	static int cur_ddrfreq = 0;

	printk ("acipc_cb_modem_ddrfreq_update: %d\n", (unsigned int)shm_rbctl[shm_rb_main].skctl_va->modem_ddrfreq);
	if ((unsigned int)shm_rbctl[shm_rb_main].skctl_va->modem_ddrfreq == MODEM_DDRFREQ_HI) {
		if (cur_ddrfreq == MODEM_DDRFREQ_HI)
			return;
		printk ("DDRFreq set to High\n");
		pm_qos_update_request(&modem_ddr_cons,  DDR_CONSTRAINT_LVL2);
		cur_ddrfreq = MODEM_DDRFREQ_HI;
	}

	if ((unsigned int)shm_rbctl[shm_rb_main].skctl_va->modem_ddrfreq == MODEM_DDRFREQ_LOW)  {
		if (cur_ddrfreq == MODEM_DDRFREQ_LOW)
			return;
		printk ("DDRFreq set to Low\n");
		pm_qos_update_request(&modem_ddr_cons, default_ddr_freq);
		cur_ddrfreq = MODEM_DDRFREQ_LOW;
	}
	return;
}

/* acipc_init is used to register interrupt call-back function */
int acipc_init(void)
{
	wake_lock_init(&acipc_wakeup, WAKE_LOCK_SUSPEND, "acipc_wakeup");

	/* we do not check any return value */
	ACIPCEventBind(ACIPC_MUDP_KEY, acipc_cb, ACIPC_CB_NORMAL, NULL);
	ACIPCEventBind(ACIPC_RINGBUF_TX_STOP, acipc_cb_rb_stop,
		       ACIPC_CB_NORMAL, NULL);
	ACIPCEventBind(ACIPC_RINGBUF_TX_RESUME, acipc_cb_rb_resume,
		       ACIPC_CB_NORMAL, NULL);
	ACIPCEventBind(ACIPC_PORT_FLOWCONTROL, acipc_cb_port_fc,
		       ACIPC_CB_NORMAL, NULL);

	ACIPCEventBind(ACIPC_RINGBUF_PSD_TX_STOP, acipc_cb_psd_rb_stop,
		       ACIPC_CB_NORMAL, NULL);
	ACIPCEventBind(ACIPC_RINGBUF_PSD_TX_RESUME, acipc_cb_psd_rb_resume,
		       ACIPC_CB_NORMAL, NULL);
	ACIPCEventBind(ACIPC_SHM_PSD_PACKET_NOTIFY, acipc_cb_psd_cb,
		       ACIPC_CB_NORMAL, NULL);
	ACIPCEventBind(ACIPC_SHM_DIAG_PACKET_NOTIFY, acipc_cb_diag_cb,
		       ACIPC_CB_NORMAL, NULL);

	/* if we have two SIM cards inserted
	 * DDR freq should not be less than 312MHz
	 */
	if(g_simcard == 3)
		default_ddr_freq = DDR_CONSTRAINT_LVL1;
	else
		default_ddr_freq = PM_QOS_DEFAULT_VALUE;

	ACIPCEventBind(ACIPC_MODEM_DDR_UPDATE_REQ, acipc_cb_event_notify,
		ACIPC_CB_NORMAL, NULL);
	pm_qos_add_request(&modem_ddr_cons, PM_QOS_DDR_DEVFREQ_MIN, PM_QOS_DEFAULT_VALUE);
	INIT_WORK(&acipc_modem_ddr_freq_update, acipc_modem_ddr_freq_update_handler);
	acipc_wq = alloc_workqueue("ACIPC_WQ", WQ_HIGHPRI, 0);

	pm_qos_update_request(&modem_ddr_cons, default_ddr_freq);

	return 0;
}

/* acipc_exit used to unregister interrupt call-back function */
void acipc_exit(void)
{
	ACIPCEventUnBind(ACIPC_SHM_PSD_PACKET_NOTIFY);
	ACIPCEventUnBind(ACIPC_RINGBUF_PSD_TX_RESUME);
	ACIPCEventUnBind(ACIPC_RINGBUF_PSD_TX_STOP);
	ACIPCEventUnBind(ACIPC_PORT_FLOWCONTROL);
	ACIPCEventUnBind(ACIPC_RINGBUF_TX_RESUME);
	ACIPCEventUnBind(ACIPC_RINGBUF_TX_STOP);
	ACIPCEventUnBind(ACIPC_MUDP_KEY);

	wake_lock_destroy(&acipc_wakeup);

	destroy_workqueue(acipc_wq);
}

/* cp xmit stopped notify interrupt */
static u32 acipc_cb_rb_stop(u32 status)
{
	return shm_rb_stop_cb(&shm_rbctl[shm_rb_main]);
}

/* cp wakeup ap xmit interrupt */
static u32 acipc_cb_rb_resume(u32 status)
{
	return shm_rb_resume_cb(&shm_rbctl[shm_rb_main]);
}

/* cp notify ap port flow control */
static u32 acipc_cb_port_fc(u32 status)
{
	return shm_port_fc_cb(&shm_rbctl[shm_rb_main]);
}

/* cp psd xmit stopped notify interrupt */
static u32 acipc_cb_psd_rb_stop(u32 status)
{
	return shm_rb_stop_cb(&shm_rbctl[shm_rb_psd]);
}

/* cp psd wakeup ap xmit interrupt */
static u32 acipc_cb_psd_rb_resume(u32 status)
{
	return shm_rb_resume_cb(&shm_rbctl[shm_rb_psd]);
}

/* psd new packet arrival interrupt */
static u32 acipc_cb_psd_cb(u32 status)
{
	return shm_packet_send_cb(&shm_rbctl[shm_rb_psd]);
}

/* diag new packet arrival interrupt */
static u32 acipc_cb_diag_cb(u32 status)
{
	return shm_packet_send_cb(&shm_rbctl[shm_rb_diag]);
}

/* new packet arrival interrupt */
static u32 acipc_cb(u32 status)
{
	u32 data;
	u16 event;

	ACIPCDataRead(&data);
	event = (data & 0xFF00) >> 8;

	switch (event) {
	case PACKET_SENT:
		shm_packet_send_cb(&shm_rbctl[shm_rb_main]);
		break;

	case PEER_SYNC:
		shm_peer_sync_cb(&shm_rbctl[shm_rb_main]);
		break;

	default:
		break;
	}

	return 0;
}

static u32 acipc_cb_modem_ddrfreq_update(u32 status)
{
	queue_work(acipc_wq, &acipc_modem_ddr_freq_update);

	return 0;
}

static u32 acipc_cb_reset_cp_confirm(u32 status)
{
	if (shm_rbctl[shm_rb_main].skctl_va->reset_request
		== RESET_CP_REQUEST_DONE) {
		complete(&reset_cp_confirm);
	}
	return 0;
}

static u32 acipc_cb_event_notify(u32 status)
{
	acipc_cb_reset_cp_confirm(status);
	acipc_cb_modem_ddrfreq_update(status);
	return 0;
}

void acipc_reset_cp_request(void)
{
	shm_rbctl[shm_rb_main].skctl_va->reset_request = RESET_CP_REQUEST;
	INIT_COMPLETION(reset_cp_confirm);
	acipc_notify_reset_cp_request();
	if (wait_for_completion_timeout(&reset_cp_confirm, 2 * HZ)) {
		printk(KERN_INFO "reset cp request success!\n");
	} else {
		printk(KERN_ERR "reset cp request fail!\n");
		shm_rbctl[shm_rb_main].skctl_va->reset_request
			= RESET_CP_REQUEST_DONE;
	}
	return;
}

