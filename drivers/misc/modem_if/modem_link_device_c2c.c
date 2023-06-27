/*
 * Copyright (C) 2010 Samsung Electronics.
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

#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/if_arp.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/suspend.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio.h>

#include <linux/platform_data/modem.h>
#include "modem_prj.h"
#include "modem_utils.h"
#include "modem_link_device_c2c.h"

static void trigger_forced_cp_crash(struct shmem_link_device *shmd);

/**
 * recv_int2ap
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Returns the value of the CP-to-AP interrupt register.
 */
static inline u16 recv_int2ap(struct shmem_link_device *shmd)
{
	if (shmd->type == C2C_SHMEM)
		return (u16)c2c_read_interrupt();

	if (shmd->mbx2ap)
		return *shmd->mbx2ap;

	return 0;
}

/**
 * send_int2cp
 * @shmd: pointer to an instance of shmem_link_device structure
 * @mask: value to be written to the AP-to-CP interrupt register
 */
static inline void send_int2cp(struct shmem_link_device *shmd, u16 mask)
{
	struct link_device *ld = &shmd->ld;

	mif_debug("%s: <called by %pf> mask = 0x%04X\n",
		ld->name, CALLER, mask);

	if (shmd->type == C2C_SHMEM)
		c2c_send_interrupt(mask);

	if (shmd->mbx2cp)
		*shmd->mbx2cp = mask;
}

/**
 * read_int2cp
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Returns the value of the AP-to-CP interrupt register.
 */
static inline u16 read_int2cp(struct shmem_link_device *shmd)
{
	if (shmd->mbx2cp)
		return ioread16(shmd->mbx2cp);
	else
		return 0;
}

/**
 * get_shmem_status
 * @shmd: pointer to an instance of shmem_link_device structure
 * @dir: direction of communication (TX or RX)
 * @mst: pointer to an instance of mem_status structure
 *
 * Takes a snapshot of the current status of a SHMEM.
 */
static void get_shmem_status(struct shmem_link_device *shmd,
			enum circ_dir_type dir, struct mem_status *mst)
{
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	getnstimeofday(&mst->ts);
#endif

	mst->dir = dir;
	mst->magic = get_magic(shmd);
	mst->access = get_access(shmd);
	mst->head[IPC_FMT][TX] = get_txq_head(shmd, IPC_FMT);
	mst->tail[IPC_FMT][TX] = get_txq_tail(shmd, IPC_FMT);
	mst->head[IPC_FMT][RX] = get_rxq_head(shmd, IPC_FMT);
	mst->tail[IPC_FMT][RX] = get_rxq_tail(shmd, IPC_FMT);
	mst->head[IPC_RAW][TX] = get_txq_head(shmd, IPC_RAW);
	mst->tail[IPC_RAW][TX] = get_txq_tail(shmd, IPC_RAW);
	mst->head[IPC_RAW][RX] = get_rxq_head(shmd, IPC_RAW);
	mst->tail[IPC_RAW][RX] = get_rxq_tail(shmd, IPC_RAW);
	mst->int2ap = recv_int2ap(shmd);
	mst->int2cp = read_int2cp(shmd);
}

#if 1
/* Functions for PM (power-management) */
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void c2c_early_suspend(struct early_suspend *h)
{
	struct shmem_link_device *shmd;
	struct utc_time t;

	get_utc_time(&t);
	mif_info("[%02d:%02d:%02d.%03d]\n", t.hour, t.min, t.sec, t.msec);

	shmd = container_of(h, struct shmem_link_device, early_suspend);
	gpio_set_value(shmd->gpio_pda_active, 0);
}

static void c2c_late_resume(struct early_suspend *h)
{
	struct shmem_link_device *shmd;
	struct utc_time t;

	get_utc_time(&t);
	mif_info("[%02d:%02d:%02d.%03d]\n", t.hour, t.min, t.sec, t.msec);

	shmd = container_of(h, struct shmem_link_device, early_suspend);
	gpio_set_value(shmd->gpio_pda_active, 1);
}
#elif defined(CONFIG_FB)
static int fb_event_cb(struct notifier_block *self, unsigned long event,
			void *data)
{
	struct shmem_link_device *shmd;
	struct fb_event *evdata = (struct fb_event *)data;
	int fb_blank = *(int *)evdata->data;

	/* If we aren't interested in this event, skip it immediately ... */
	if (event != FB_EVENT_BLANK)
		return 0;

	shmd = container_of(self, struct shmem_link_device, fb_nb);
	if (!shmd) {
		mif_err("ERR! shmd == NULL\n");
		return 0;
	}

	mif_debug("LCD state changed: %s\n",
		(fb_blank == FB_BLANK_UNBLANK) ? "OFF -> ON" : "ON -> OFF");

	if (fb_blank == FB_BLANK_UNBLANK)
		gpio_set_value(shmd->gpio_pda_active, 1);
	else
		gpio_set_value(shmd->gpio_pda_active, 0);

	return 0;
}
#endif

static inline int check_link_status(struct shmem_link_device *shmd)
{
	int cnt;
	int cp_status;
	u32 magic;
#if 0
	u32 link_stat;
#endif

	cnt = 0;
	while (1) {
		cp_status = gpio_get_value(shmd->gpio_cp_status);
		if (cp_status)
			break;

		cnt++;
		if (cnt >= 100) {
			mif_err("ERR! gpio_cp_status == 0 (cnt %d)\n", cnt);
			return -EACCES;
		} else {
			if ((cnt%20) == 0) {
				mif_debug("ERR! gpio_cp_status == 0 (cnt %d)\n",
					cnt);
			}
			udelay(100);
		}
	}

	cnt = 0;
	while (1) {
		magic = get_magic(shmd);
		if (magic == SHM_IPC_MAGIC)
			break;

		cnt++;
		if (cnt >= 100) {
			mif_err("ERR! magic 0x%X != SHM_IPC_MAGIC (cnt %d)\n",
				magic, cnt);
			return -EACCES;
		} else {
			if ((cnt % 20) == 0) {
				mif_debug("ERR! magic 0x%X != SHM_IPC_MAGIC "
					"(cnt %d)\n", magic, cnt);
			}
			udelay(100);
		}
	}

#if 0
	link_stat = c2c_read_link();
	if (link_stat) {
		mif_err("ERR! C2C link_stat == 0x%X\n", link_stat);
		return -EACCES;
	}
#endif

	return 0;
}

static void hold_cp_wakeup(struct work_struct *work)
{
	struct shmem_link_device *shmd;
	struct link_device *ld;
	unsigned long delay = msecs_to_jiffies(CP_WAKEUP_HOLD_TIME);

	shmd = container_of(work, struct shmem_link_device,
			cp_wakeup_hold_dwork.work);
	ld = &shmd->ld;

	mif_debug("%s: update cp_wakeup hold time", ld->name);
	cancel_delayed_work(&shmd->cp_wakeup_drop_dwork);
	schedule_delayed_work(&shmd->cp_wakeup_drop_dwork, delay);
}

static void drop_cp_wakeup(struct work_struct *work)
{
	struct shmem_link_device *shmd;
	struct link_device *ld;

	shmd = container_of(work, struct shmem_link_device,
			cp_wakeup_drop_dwork.work);
	ld = &shmd->ld;

	mif_debug("%s: CP sleep enabled!!!\n", ld->name);
	gpio_set_value(shmd->gpio_cp_wakeup, 0);
	wake_unlock(&shmd->cp_wlock);
}

/**
 * forbid_cp_sleep
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Wakes up a CP if it can sleep and increases the "accessing" counter in the
 * shmem_link_device instance.
 *
 * CAUTION!!! permit_cp_sleep() MUST be invoked after forbid_cp_sleep() success
 * to decrease the "accessing" counter.
 */
static int forbid_cp_sleep(struct shmem_link_device *shmd)
{
	struct link_device *ld = &shmd->ld;
	int ref_cnt;

	mif_debug("%s: <called by %pf>\n", ld->name, CALLER);

	if (atomic_read(&shmd->accessing) == 0) {
		wake_lock(&shmd->cp_wlock);

		gpio_set_value(shmd->gpio_cp_wakeup, 1);
		if (check_link_status(shmd) < 0) {
			mif_err("%s: ERR! check_link_status fail\n", ld->name);
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
			trigger_forced_cp_crash(shmd);
#endif
			goto error;
		}

		mif_debug("%s: CP sleep disabled!!!\n", ld->name);
	}

	ref_cnt = atomic_inc_return(&shmd->accessing);

	return 0;

error:
	wake_unlock(&shmd->cp_wlock);
	return -EACCES;
}

/**
 * permit_cp_sleep
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Decreases the "accessing" counter in the shmem_link_device instance if it can
 * sleep and allows a CP to sleep only if the value of "accessing" counter is
 * less than or equal to 0.
 *
 * MUST be invoked after forbid_cp_sleep() success to decrease the "accessing"
 * counter.
 */
static void permit_cp_sleep(struct shmem_link_device *shmd)
{
	struct link_device *ld = &shmd->ld;
	unsigned long delay = msecs_to_jiffies(CP_WAKEUP_HOLD_TIME);
	int ref_cnt;

	mif_debug("%s: <called by %pf>\n", ld->name, CALLER);

	ref_cnt = atomic_dec_return(&shmd->accessing);
	if (ref_cnt <= 0) {
		atomic_set(&shmd->accessing, 0);
		/*
		** Hold gpio_cp_wakeup for CP_WAKEUP_HOLD_TIME to avoid
		** gpio_ap_wakeup pulse
		*/
		mif_debug("%s: update cp_wakeup hold time\n", ld->name);
		cancel_delayed_work(&shmd->cp_wakeup_drop_dwork);
		schedule_delayed_work(&shmd->cp_wakeup_drop_dwork, delay);
	}
}

#if 1
/* Functions for IPC/BOOT/DUMP*/
#endif

/**
 * handle_cp_crash
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Actual handler for the CRASH_EXIT command from a CP.
 */
static void handle_cp_crash(struct shmem_link_device *shmd)
{
	struct link_device *ld = &shmd->ld;
	struct io_device *iod;
	int i;

	if (shmd->forced_cp_crash)
		shmd->forced_cp_crash = false;

	/* Stop network interfaces */
	mif_netif_stop(ld);

	/* Purge the skb_txq in every IPC device (IPC_FMT, IPC_RAW, etc.) */
	for (i = 0; i < MAX_SIPC5_DEV; i++)
		skb_queue_purge(ld->skb_txq[i]);

	/* Change the modem state to STATE_CRASH_EXIT for the FMT IO device */
	iod = link_get_iod_with_format(ld, IPC_FMT);
	if (iod) {
		iod->modem_state_changed(iod, STATE_CRASH_EXIT);

		/* time margin for taking state changes by rild */
		mdelay(100);
	}

	/* Change the modem state to STATE_CRASH_EXIT for the BOOT IO device */
	iod = link_get_iod_with_format(ld, IPC_BOOT);
	if (iod)
		iod->modem_state_changed(iod, STATE_CRASH_EXIT);
}

/**
 * handle_no_cp_crash_ack
 * @arg: pointer to an instance of shmem_link_device structure
 *
 * Invokes handle_cp_crash() to enter the CRASH_EXIT state if there was no
 * CRASH_ACK from a CP in FORCE_CRASH_ACK_TIMEOUT.
 */
static void handle_no_cp_crash_ack(unsigned long arg)
{
	struct shmem_link_device *shmd = (struct shmem_link_device *)arg;
	struct link_device *ld = &shmd->ld;

	mif_err("%s: ERR! No CRASH_EXIT ACK from CP\n", ld->name);

	handle_cp_crash(shmd);
}

/**
 * trigger_forced_cp_crash
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Triggers an enforced CP crash.
 */
static void trigger_forced_cp_crash(struct shmem_link_device *shmd)
{
	struct link_device *ld = &shmd->ld;
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	struct trace_data *trd;
	u8 *dump;
	struct timespec ts;
	getnstimeofday(&ts);
#endif

	if (ld->mode == LINK_MODE_ULOAD) {
		mif_err("%s: <called by %pf> ALREADY in progress\n",
			ld->name, CALLER);
		return;
	}

	ld->mode = LINK_MODE_ULOAD;

	shmd->forced_cp_crash = true;
	forbid_cp_sleep(shmd);

	if (!wake_lock_active(&shmd->wlock))
		wake_lock(&shmd->wlock);

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	dump = capture_mem_dump(ld, shmd->base, shmd->size);
	if (dump) {
		trd = trq_get_free_slot(&shmd->trace_list);
		memcpy(&trd->ts, &ts, sizeof(struct timespec));
		trd->dev = IPC_DEBUG;
		trd->data = dump;
		trd->size = shmd->size;
	}
#endif

	mif_err("%s: <called by %pf>\n", ld->name, CALLER);

	/* Send CRASH_EXIT command to a CP */
	send_int2cp(shmd, INT_CMD(INT_CMD_CRASH_EXIT));
	get_shmem_status(shmd, TX, msq_get_free_slot(&shmd->stat_list));

	/* If there is no CRASH_ACK from a CP in FORCE_CRASH_ACK_TIMEOUT,
	   handle_no_cp_crash_ack() will be executed. */
	mif_add_timer(&shmd->crash_ack_timer, FORCE_CRASH_ACK_TIMEOUT,
			handle_no_cp_crash_ack, (unsigned long)shmd);

	return;
}

/**
 * cmd_req_active_handler
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Handles the REQ_ACTIVE command from a CP.
 */
static void cmd_req_active_handler(struct shmem_link_device *shmd)
{
	send_int2cp(shmd, INT_CMD(INT_CMD_RES_ACTIVE));
}

/**
 * cmd_crash_reset_handler
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Handles the CRASH_RESET command from a CP.
 */
static void cmd_crash_reset_handler(struct shmem_link_device *shmd)
{
	struct link_device *ld = &shmd->ld;
	struct io_device *iod = NULL;
	int i;

	ld->mode = LINK_MODE_ULOAD;

	if (!wake_lock_active(&shmd->wlock))
		wake_lock(&shmd->wlock);

	/* Stop network interfaces */
	mif_netif_stop(ld);

	/* Purge the skb_txq in every IPC device (IPC_FMT, IPC_RAW, etc.) */
	for (i = 0; i < MAX_SIPC5_DEV; i++)
		skb_queue_purge(ld->skb_txq[i]);

	mif_err("%s: Recv 0xC7 (CRASH_RESET)\n", ld->name);

	/* Change the modem state to STATE_CRASH_RESET for the FMT IO device */
	iod = link_get_iod_with_format(ld, IPC_FMT);
	if (iod)
		iod->modem_state_changed(iod, STATE_CRASH_RESET);

	/* time margin for taking state changes by rild */
	mdelay(100);

	/* Change the modem state to STATE_CRASH_RESET for the BOOT IO device */
	iod = link_get_iod_with_format(ld, IPC_BOOT);
	if (iod)
		iod->modem_state_changed(iod, STATE_CRASH_RESET);
}

/**
 * cmd_crash_exit_handler
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Handles the CRASH_EXIT command from a CP.
 */
static void cmd_crash_exit_handler(struct shmem_link_device *shmd)
{
	struct link_device *ld = &shmd->ld;
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	struct trace_data *trd;
	u8 *dump;
	struct timespec ts;
	getnstimeofday(&ts);
#endif

	ld->mode = LINK_MODE_ULOAD;

	del_timer(&shmd->crash_ack_timer);

	if (!shmd->forced_cp_crash) {
		if (!wake_lock_active(&shmd->wlock))
			wake_lock(&shmd->wlock);
	}

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	if (!shmd->forced_cp_crash) {
		dump = capture_mem_dump(ld, shmd->base, shmd->size);
		if (dump) {
			trd = trq_get_free_slot(&shmd->trace_list);
			memcpy(&trd->ts, &ts, sizeof(struct timespec));
			trd->dev = IPC_DEBUG;
			trd->data = dump;
			trd->size = shmd->size;
		}
	}
#endif

	mif_err("%s: Recv 0xC9 (CRASH_EXIT)\n", ld->name);

	handle_cp_crash(shmd);
}

/**
 * cmd_phone_start_handler
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Handles the PHONE_START command from a CP.
 */
static void cmd_phone_start_handler(struct shmem_link_device *shmd)
{
	int err;
	struct link_device *ld = &shmd->ld;
	struct io_device *iod;

	mif_err("%s: Recv 0xC8 (CP_START)\n", ld->name);

	iod = link_get_iod_with_format(ld, IPC_FMT);
	if (!iod) {
		mif_err("%s: ERR! no iod\n", ld->name);
		return;
	}

	err = init_shmem_ipc(shmd);
	if (err)
		return;

	if (iod->mc->phone_state != STATE_ONLINE)
		iod->modem_state_changed(iod, STATE_ONLINE);

	mif_err("%s: Send 0xC2 (INIT_END)\n", ld->name);
	send_int2cp(shmd, INT_CMD(INT_CMD_INIT_END));
}

/**
 * cmd_handler: processes a SHMEM command from a CP
 * @shmd: pointer to an instance of shmem_link_device structure
 * @cmd: SHMEM command from a CP
 */
static void cmd_handler(struct shmem_link_device *shmd, u16 cmd)
{
	struct link_device *ld = &shmd->ld;

	switch (INT_CMD_MASK(cmd)) {
	case INT_CMD_REQ_ACTIVE:
		cmd_req_active_handler(shmd);
		break;

	case INT_CMD_CRASH_RESET:
		cmd_crash_reset_handler(shmd);
		break;

	case INT_CMD_CRASH_EXIT:
		cmd_crash_exit_handler(shmd);
		break;

	case INT_CMD_PHONE_START:
		cmd_phone_start_handler(shmd);
		complete_all(&ld->init_cmpl);
		break;

	case INT_CMD_NV_REBUILDING:
		mif_err("%s: NV_REBUILDING\n", ld->name);
		break;

	case INT_CMD_PIF_INIT_DONE:
		complete_all(&ld->pif_cmpl);
		break;

	case INT_CMD_SILENT_NV_REBUILDING:
		mif_err("%s: SILENT_NV_REBUILDING\n", ld->name);
		break;

	case INT_CMD_NORMAL_POWER_OFF:
		/*ToDo:*/
		/*kernel_sec_set_cp_ack()*/;
		break;

	case INT_CMD_REQ_TIME_SYNC:
	case INT_CMD_CP_DEEP_SLEEP:
	case INT_CMD_EMER_DOWN:
		break;

	default:
		mif_err("%s: unknown command 0x%04X\n", ld->name, cmd);
	}
}

/**
 * rx_ipc_task
 * @data: pointer to an instance of shmem_link_device structure
 *
 * Invokes the recv method in the io_device instance to perform receiving IPC
 * messages from each skb.
 */
static void rx_ipc_task(unsigned long data)
{
	struct shmem_link_device *shmd = (struct shmem_link_device *)data;
	struct link_device *ld = &shmd->ld;
	struct io_device *iod;
	struct sk_buff *skb;
	int i;

	for (i = 0; i < MAX_SIPC5_DEV; i++) {
		iod = shmd->iod[i];
		while (1) {
			skb = skb_dequeue(ld->skb_rxq[i]);
			if (!skb)
				break;
			iod->recv_skb(iod, ld, skb);
		}
	}
}

/**
 * rx_ipc_frames
 * @shmd: pointer to an instance of shmem_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 * @mst: pointer to an instance of mem_status structure
 *
 * Returns
 *   ret < 0  : error
 *   ret == 0 : ILLEGAL status
 *   ret > 0  : valid data
 *
 * Must be invoked only when there is data in the corresponding RXQ.
 *
 * Requires a recv_skb method in the io_device instance, so this function must
 * be used for only SIPC5.
 */
static int rx_ipc_frames(struct shmem_link_device *shmd, int dev,
			struct circ_status *circ)
{
	struct link_device *ld = &shmd->ld;
	struct sk_buff_head *rxq = ld->skb_rxq[dev];
	struct sk_buff *skb;
	int ret;
	/**
	 * variables for the status of the circular queue
	 */
	u8 *src;
	u8 hdr[SIPC5_MIN_HEADER_SIZE];
	/**
	 * variables for RX processing
	 */
	int qsize;	/* size of the queue			*/
	int rcvd;	/* size of data in the RXQ or error	*/
	int rest;	/* size of the rest data		*/
	int out;	/* index to the start of current frame	*/
	u8 *dst;	/* pointer to the destination buffer	*/
	int tot;	/* total length including padding data	*/

	src = circ->buff;
	qsize = circ->qsize;
	out = circ->out;
	rcvd = circ->size;

	rest = rcvd;
	tot = 0;
	while (rest > 0) {
		/* Copy the header in the frame to the header buffer */
		circ_read(hdr, src, qsize, out, SIPC5_MIN_HEADER_SIZE);

		/* Check the config field in the header */
		if (unlikely(!sipc5_start_valid(hdr))) {
			mif_err("%s: ERR! %s INVALID config 0x%02X "
				"(rcvd %d, rest %d)\n", ld->name,
				get_dev_name(dev), hdr[0], rcvd, rest);
			ret = -EBADMSG;
			goto exit;
		}

		/* Verify the total length of the frame (data + padding) */
		tot = sipc5_get_total_len(hdr);
		if (unlikely(tot > rest)) {
			mif_err("%s: ERR! %s tot %d > rest %d (rcvd %d)\n",
				ld->name, get_dev_name(dev), tot, rest, rcvd);
			ret = -EBADMSG;
			goto exit;
		}

		/* Allocate an skb */
		skb = dev_alloc_skb(tot);
		if (!skb) {
			mif_err("%s: ERR! %s dev_alloc_skb fail\n",
				ld->name, get_dev_name(dev));
			ret = -ENOMEM;
			goto exit;
		}

		/* Set the attribute of the skb as "single frame" */
		skbpriv(skb)->single_frame = true;

		/* Read the frame from the RXQ */
		dst = skb_put(skb, tot);
		circ_read(dst, src, qsize, out, tot);

		/* Store the skb to the corresponding skb_rxq */
		skb_queue_tail(rxq, skb);

		/* Calculate new out value */
		rest -= tot;
		out += tot;
		if (unlikely(out >= qsize))
			out -= qsize;
	}

	/* Update tail (out) pointer to empty out the RXQ */
	set_rxq_tail(shmd, dev, circ->in);

	return rcvd;

exit:
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	mif_err("%s: ERR! rcvd:%d tot:%d rest:%d\n", ld->name, rcvd, tot, rest);
	pr_ipc(1, "c2c: ERR! CP2MIF", (src + out), (rest > 20) ? 20 : rest);
#endif

	return ret;
}

/**
 * msg_handler: receives IPC messages from every RXQ
 * @shmd: pointer to an instance of shmem_link_device structure
 * @mst: pointer to an instance of mem_status structure
 *
 * 1) Receives all IPC message frames currently in every IPC RXQ.
 * 2) Sends RES_ACK responses if there are REQ_ACK requests from a CP.
 * 3) Completes all threads waiting for the corresponding RES_ACK from a CP if
 *    there is any RES_ACK response.
 */
static void msg_handler(struct shmem_link_device *shmd, struct mem_status *mst)
{
	struct link_device *ld = &shmd->ld;
	struct circ_status circ;
	int i = 0;
	int ret = 0;
	u16 mask = 0;
	u16 intr = mst->int2ap;

	if (!ipc_active(shmd))
		return;

	/* Read data from every RXQ */
	for (i = 0; i < MAX_SIPC5_DEV; i++) {
		/* Invoke an RX function only when there is data in the RXQ */
		if (likely(mst->head[i][RX] != mst->tail[i][RX])) {
			ret = get_rxq_rcvd(shmd, i, mst, &circ);
			if (unlikely(ret < 0)) {
				mif_err("%s: ERR! get_rxq_rcvd fail (err %d)\n",
					ld->name, ret);
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
				trigger_forced_cp_crash(shmd);
#endif
				return;
			}

			ret = rx_ipc_frames(shmd, i, &circ);
			if (ret < 0) {
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
				trigger_forced_cp_crash(shmd);
#endif
				reset_rxq_circ(shmd, i);
			}
		}
	}

	/* Schedule soft IRQ for RX */
	tasklet_hi_schedule(&shmd->rx_tsk);

	/* Check and process REQ_ACK (at this time, in == out) */
	if (unlikely(intr & INT_MASK_REQ_ACK_SET)) {
		for (i = 0; i < MAX_SIPC5_DEV; i++) {
			if (intr & get_mask_req_ack(shmd, i)) {
				mif_info("%s: set %s_RES_ACK\n",
					ld->name, get_dev_name(i));
				mask |= get_mask_res_ack(shmd, i);
			}
		}

		send_int2cp(shmd, INT_NON_CMD(mask));
	}

	/* Check and process RES_ACK */
	if (unlikely(intr & INT_MASK_RES_ACK_SET)) {
		for (i = 0; i < MAX_SIPC5_DEV; i++) {
			if (intr & get_mask_res_ack(shmd, i)) {
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
				mif_info("%s: recv %s_RES_ACK\n",
					ld->name, get_dev_name(i));
				print_circ_status(ld, i, mst);
#endif
				complete(&shmd->req_ack_cmpl[i]);
			}
		}
	}
}

/**
 * ipc_handler: processes a SHMEM command or receives IPC messages
 * @shmd: pointer to an instance of shmem_link_device structure
 * @mst: pointer to an instance of mem_status structure
 *
 * Invokes cmd_handler for a SHMEM command or msg_handler for IPC messages.
 */
static void ipc_handler(struct shmem_link_device *shmd, struct mem_status *mst)
{
	struct mem_status *slot = msq_get_free_slot(&shmd->stat_list);
	u16 intr = mst->int2ap;

	memcpy(slot, mst, sizeof(struct mem_status));

	if (unlikely(INT_CMD_VALID(intr)))
		cmd_handler(shmd, intr);
	else
		msg_handler(shmd, mst);
}

/**
 * udl_handler: receives BOOT/DUMP IPC messages from every RXQ
 * @shmd: pointer to an instance of shmem_link_device structure
 * @mst: pointer to an instance of mem_status structure
 *
 * 1) Receives all IPC message frames currently in every IPC RXQ.
 * 2) Sends RES_ACK responses if there are REQ_ACK requests from a CP.
 * 3) Completes all threads waiting for the corresponding RES_ACK from a CP if
 *    there is any RES_ACK response.
 */
static void udl_handler(struct shmem_link_device *shmd, struct mem_status *mst)
{
	struct link_device *ld = &shmd->ld;
	struct io_device *iod;
	struct sk_buff_head *rxq = ld->skb_rxq[IPC_RAW];
	struct sk_buff *skb;
	struct circ_status circ;
	int ret = 0;
	u16 intr = mst->int2ap;

	/* Invoke an RX function only when there is data in the RXQ */
	if (mst->head[IPC_RAW][RX] != mst->tail[IPC_RAW][RX]) {
		ret = get_rxq_rcvd(shmd, IPC_RAW, mst, &circ);
		if (unlikely(ret < 0)) {
			mif_err("%s: ERR! get_rxq_rcvd fail (err %d)\n",
				ld->name, ret);
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
			trigger_forced_cp_crash(shmd);
#endif
			return;
		}

		ret = rx_ipc_frames(shmd, IPC_RAW, &circ);
		if (ret < 0) {
			skb_queue_purge(rxq);
			return;
		}
	}

	/* Check and process RES_ACK */
	if (intr & INT_MASK_RES_ACK_SET) {
		if (intr & get_mask_res_ack(shmd, IPC_RAW)) {
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
			mif_info("%s: recv RAW_RES_ACK\n", ld->name);
			print_circ_status(ld, IPC_RAW, mst);
#endif
			complete(&shmd->req_ack_cmpl[IPC_RAW]);
		}
	}

	/* Pass the skb to an iod */
	while (1) {
		skb = skb_dequeue(rxq);
		if (!skb)
			break;

		iod = link_get_iod_with_channel(ld, sipc5_get_ch_id(skb->data));
		if (!iod) {
			mif_err("%s: ERR! no IPC_BOOT iod\n", ld->name);
			break;
		}

		if (!std_udl_with_payload(std_udl_get_cmd(skb->data))) {
			if (ld->mode == LINK_MODE_DLOAD) {
				pr_ipc(1, "[CP->AP] DL CMD", skb->data,
					(skb->len > 20 ? 20 : skb->len));
			} else {
				pr_ipc(1, "[CP->AP] UL CMD", skb->data,
					(skb->len > 20 ? 20 : skb->len));
			}
		}

		iod->recv_skb(iod, ld, skb);
	}
}

/**
 * c2c_irq_handler: interrupt handler for a C2C interrupt
 * @data: pointer to a data
 *
 * 1) Reads the interrupt value
 * 2) Performs interrupt handling
 *
 * Flow for normal interrupt handling:
 *   c2c_irq_handler -> udl_handler
 *   c2c_irq_handler -> ipc_handler -> cmd_handler -> cmd_xxx_handler
 *   c2c_irq_handler -> ipc_handler -> msg_handler -> rx_ipc_frames ->  ...
 */
static void c2c_irq_handler(void *data)
{
	struct shmem_link_device *shmd = (struct shmem_link_device *)data;
	struct link_device *ld = (struct link_device *)&shmd->ld;
	struct mem_status mst;
	u16 intr;

	if (unlikely(ld->mode == LINK_MODE_OFFLINE)) {
		mif_err("%s: ld->mode == LINK_MODE_OFFLINE\n", ld->name);
		return;
	}

	get_shmem_status(shmd, RX, &mst);

	intr = mst.int2ap;
	if (unlikely(!INT_VALID(intr))) {
		mif_err("%s: ERR! invalid intr 0x%04X\n", ld->name, intr);
		return;
	}

	if (ld->mode == LINK_MODE_DLOAD || ld->mode == LINK_MODE_ULOAD)
		udl_handler(shmd, &mst);
	else
		ipc_handler(shmd, &mst);
}

/**
 * ap_wakeup_handler: interrupt handler for a wakeup interrupt
 * @irq: IRQ number
 * @data: pointer to a data
 *
 * 1) Reads the interrupt value
 * 2) Performs interrupt handling
 */
static irqreturn_t ap_wakeup_handler(int irq, void *data)
{
	struct shmem_link_device *shmd = (struct shmem_link_device *)data;
	struct link_device *ld = (struct link_device *)&shmd->ld;
	struct modem_ctl *mc = ld->mc;
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	struct utc_time t;
#endif

	if (unlikely(ld->mode != LINK_MODE_IPC)) {
		mif_err("%s: ld->mode != LINK_MODE_IPC\n", ld->name);
		goto exit;
	}

	wake_lock(&shmd->ap_wlock);
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	get_utc_time(&t);
	mif_debug("%s: [%02d:%02d:%02d.%03d] AP sleep disabled!!!\n",
		mc->name, t.hour, t.min, t.sec, t.msec);
#endif

	if (!c2c_is_suspended()) {
		mif_debug("%s: C2C is NOT suspended\n", ld->name);
		gpio_set_value(shmd->gpio_cp_wakeup, 1);
		schedule_delayed_work(&shmd->cp_wakeup_hold_dwork, 0);
	} else {
		c2c_resume();
		gpio_set_value(mc->gpio_ap_status, 1);
	}


exit:
	return IRQ_HANDLED;
}

static irqreturn_t cp_status_handler(int irq, void *data)
{
	struct shmem_link_device *shmd = (struct shmem_link_device *)data;
	struct link_device *ld = (struct link_device *)&shmd->ld;
	struct modem_ctl *mc = ld->mc;
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	struct utc_time t;
#endif
	int cp_on = gpio_get_value(mc->gpio_cp_on);
	int cp_off = gpio_get_value(mc->gpio_cp_off);
	int cp_reset = gpio_get_value(mc->gpio_cp_reset);
	int cp_active = gpio_get_value(mc->gpio_phone_active);
	int cp_status = gpio_get_value(mc->gpio_cp_status);
	mif_err("cp_on:%d cp_reset:%d ps_hold:%d cp_active:%d cp_status:%d\n",
		cp_on, cp_reset, cp_off, cp_active, cp_status);

	if (unlikely(ld->mode != LINK_MODE_IPC)) {
		mif_err("%s: ld->mode != LINK_MODE_IPC\n", ld->name);
		goto exit;
	}

#if 0 /* hold ap_status when ap_wakeon */
	gpio_set_value(shmd->gpio_ap_status, 0);

	if (c2c_is_suspended()) {
		mif_err("%s: C2C is ALREADY suspended\n", ld->name);
		wake_unlock(&shmd->ap_wlock);
		goto exit;
	}

	c2c_suspend();
#endif

	wake_unlock(&shmd->ap_wlock);
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	get_utc_time(&t);
	mif_err("%s: [%02d:%02d:%02d.%03d] AP sleep enabled!!!\n",
		mc->name, t.hour, t.min, t.sec, t.msec);
#endif

exit:
	return IRQ_HANDLED;
}

/**
 * write_ipc_to_txq
 * @shmd: pointer to an instance of shmem_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 * @circ: pointer to an instance of circ_status structure
 * @skb: pointer to an instance of sk_buff structure
 *
 * Must be invoked only when there is enough space in the TXQ.
 */
static void write_ipc_to_txq(struct shmem_link_device *shmd, int dev,
			struct circ_status *circ, struct sk_buff *skb)
{
	struct link_device *ld = &shmd->ld;
	u32 qsize = circ->qsize;
	u32 in = circ->in;
	u8 *buff = circ->buff;
	u8 *src = skb->data;
	u32 len = skb->len;

	/* Write data to the TXQ */
	circ_write(buff, src, qsize, in, len);

	/* Update new head (in) pointer */
	set_txq_head(shmd, dev, circ_new_pointer(qsize, in, len));

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	/* Take a log for debugging */
	if (dev == IPC_FMT) {
		char tag[MIF_MAX_STR_LEN];
		snprintf(tag, MIF_MAX_STR_LEN, "%s: MIF2CP", ld->name);
		pr_ipc(0, tag, src, (len > 20 ? 20 : len));
	}
#endif
}

/**
 * xmit_ipc_msg
 * @shmd: pointer to an instance of shmem_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 *
 * Tries to transmit IPC messages in the skb_txq of @dev as many as possible.
 *
 * Returns total length of IPC messages transmitted or an error code.
 */
static int xmit_ipc_msg(struct shmem_link_device *shmd, int dev)
{
	struct link_device *ld = &shmd->ld;
	struct sk_buff_head *txq = ld->skb_txq[dev];
	struct sk_buff *skb;
	unsigned long flags;
	struct circ_status circ;
	int space;
	int copied = 0;

	/* Acquire the spin lock for a TXQ */
	spin_lock_irqsave(&shmd->tx_lock[dev], flags);

	while (1) {
		/* Get the size of free space in the TXQ */
		space = get_txq_space(shmd, dev, &circ);
		if (unlikely(space < 0)) {
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
			/* Trigger a enforced CP crash */
			trigger_forced_cp_crash(shmd);
#endif
			/* Empty out the TXQ */
			reset_txq_circ(shmd, dev);
			copied = -EIO;
			break;
		}

		skb = skb_dequeue(txq);
		if (unlikely(!skb))
			break;

		/* Check the free space size comparing with skb->len */
		if (unlikely(space < skb->len)) {
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
			struct mem_status mst;
#endif
			/* Set res_required flag for the "dev" */
			atomic_set(&shmd->res_required[dev], 1);

			/* Take the skb back to the skb_txq */
			skb_queue_head(txq, skb);

			mif_err("%s: <called by %pf> NOSPC in %s_TXQ"
				"{qsize:%u in:%u out:%u}, free:%u < len:%u\n",
				ld->name, CALLER, get_dev_name(dev),
				circ.qsize, circ.in, circ.out, space, skb->len);
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
			get_shmem_status(shmd, TX, &mst);
			print_circ_status(ld, dev, &mst);
#endif
			copied = -ENOSPC;
			break;
		}

		if (!ipc_active(shmd)) {
			if (get_magic(shmd) == SHM_PM_MAGIC) {
				mif_err("%s: Going to SLEEP\n", ld->name);
				copied = -EBUSY;
			} else {
				mif_err("%s: IPC is NOT active\n", ld->name);
				copied = -EIO;
			}
			break;
		}

		/* TX only when there is enough space in the TXQ */
		write_ipc_to_txq(shmd, dev, &circ, skb);
		copied += skb->len;
		dev_kfree_skb_any(skb);
	}

	/* Release the spin lock */
	spin_unlock_irqrestore(&shmd->tx_lock[dev], flags);

	return copied;
}

/**
 * wait_for_res_ack
 * @shmd: pointer to an instance of shmem_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 *
 * 1) Sends an REQ_ACK interrupt for @dev to CP.
 * 2) Waits for the corresponding RES_ACK for @dev from CP.
 *
 * Returns the return value from wait_for_completion_interruptible_timeout().
 */
static int wait_for_res_ack(struct shmem_link_device *shmd, int dev)
{
	struct link_device *ld = &shmd->ld;
	struct completion *cmpl = &shmd->req_ack_cmpl[dev];
	unsigned long timeout = msecs_to_jiffies(RES_ACK_WAIT_TIMEOUT);
	int ret;
	u16 mask;

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	mif_err("%s: send %s_REQ_ACK\n", ld->name, get_dev_name(dev));
#endif

	mask = get_mask_req_ack(shmd, dev);
	send_int2cp(shmd, INT_NON_CMD(mask));

	timeout = msecs_to_jiffies(RES_ACK_WAIT_TIMEOUT);

	/* ret < 0 if interrupted, ret == 0 on timeout */
	ret = wait_for_completion_interruptible_timeout(cmpl, timeout);
	if (ret < 0) {
		mif_err("%s: %s: wait_for_completion interrupted! (ret %d)\n",
			ld->name, get_dev_name(dev), ret);
		goto exit;
	}

	if (ret == 0) {
		struct mem_status mst;
		get_shmem_status(shmd, TX, &mst);

		mif_err("%s: wait_for_completion TIMEOUT! (no %s_RES_ACK)\n",
			ld->name, get_dev_name(dev));

		/*
		** The TXQ must be checked whether or not it is empty, because
		** an interrupt mask can be overwritten by the next interrupt.
		*/
		if (mst.head[dev][TX] == mst.tail[dev][TX]) {
			ret = get_txq_buff_size(shmd, dev);
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
			mif_err("%s: %s_TXQ has been emptied\n",
				ld->name, get_dev_name(dev));
			print_circ_status(ld, dev, &mst);
#endif
		}
	}

exit:
	return ret;
}

/**
 * process_res_ack
 * @shmd: pointer to an instance of shmem_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 *
 * 1) Tries to transmit IPC messages in the skb_txq with xmit_ipc_msg().
 * 2) Sends an interrupt to CP if there is no error from xmit_ipc_msg().
 * 3) Restarts SHMEM flow control if xmit_ipc_msg() returns -ENOSPC.
 *
 * Returns the return value from xmit_ipc_msg().
 */
static int process_res_ack(struct shmem_link_device *shmd, int dev)
{
	int ret;
	u16 mask;

	ret = xmit_ipc_msg(shmd, dev);
	if (ret > 0) {
		mask = get_mask_send(shmd, dev);
		send_int2cp(shmd, INT_NON_CMD(mask));
		get_shmem_status(shmd, TX, msq_get_free_slot(&shmd->stat_list));
	}

	if (ret >= 0)
		atomic_set(&shmd->res_required[dev], 0);

	return ret;
}

/**
 * fmt_tx_work: performs TX for FMT IPC device under SHMEM flow control
 * @work: pointer to an instance of the work_struct structure
 *
 * 1) Starts waiting for RES_ACK of FMT IPC device.
 * 2) Returns immediately if the wait is interrupted.
 * 3) Restarts SHMEM flow control if there is a timeout from the wait.
 * 4) Otherwise, it performs processing RES_ACK for FMT IPC device.
 */
static void fmt_tx_work(struct work_struct *work)
{
	struct link_device *ld;
	struct shmem_link_device *shmd;
	unsigned long delay = 0;
	int ret;

	ld = container_of(work, struct link_device, fmt_tx_dwork.work);
	shmd = to_shmem_link_device(ld);

	ret = wait_for_res_ack(shmd, IPC_FMT);
	/* ret < 0 if interrupted */
	if (ret < 0)
		return;

	/* ret == 0 on timeout */
	if (ret == 0) {
		queue_delayed_work(ld->tx_wq, ld->tx_dwork[IPC_FMT], 0);
		return;
	}

	ret = process_res_ack(shmd, IPC_FMT);
	if (ret >= 0) {
		permit_cp_sleep(shmd);
		return;
	}

	/* At this point, ret < 0 */
	if ((ret == -ENOSPC) || (ret == -EBUSY))
		queue_delayed_work(ld->tx_wq, ld->tx_dwork[IPC_FMT], delay);
}

/**
 * raw_tx_work: performs TX for RAW IPC device under SHMEM flow control.
 * @work: pointer to an instance of the work_struct structure
 *
 * 1) Starts waiting for RES_ACK of RAW IPC device.
 * 2) Returns immediately if the wait is interrupted.
 * 3) Restarts SHMEM flow control if there is a timeout from the wait.
 * 4) Otherwise, it performs processing RES_ACK for RAW IPC device.
 */
static void raw_tx_work(struct work_struct *work)
{
	struct link_device *ld;
	struct shmem_link_device *shmd;
	unsigned long delay = 0;
	int ret;

	ld = container_of(work, struct link_device, raw_tx_dwork.work);
	shmd = to_shmem_link_device(ld);

	ret = wait_for_res_ack(shmd, IPC_RAW);
	/* ret < 0 if interrupted */
	if (ret < 0)
		return;

	/* ret == 0 on timeout */
	if (ret == 0) {
		queue_delayed_work(ld->tx_wq, ld->tx_dwork[IPC_RAW], 0);
		return;
	}

	ret = process_res_ack(shmd, IPC_RAW);
	if (ret >= 0) {
		permit_cp_sleep(shmd);
		mif_netif_wake(ld);
		return;
	}

	/* At this point, ret < 0 */
	if ((ret == -ENOSPC) || (ret == -EBUSY))
		queue_delayed_work(ld->tx_wq, ld->tx_dwork[IPC_RAW], delay);
}

/**
 * c2c_send_ipc
 * @shmd: pointer to an instance of shmem_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 * @skb: pointer to an skb that will be transmitted
 *
 * 1) Tries to transmit IPC messages in the skb_txq with xmit_ipc_msg().
 * 2) Sends an interrupt to CP if there is no error from xmit_ipc_msg().
 * 3) Starts SHMEM flow control if xmit_ipc_msg() returns -ENOSPC.
 */
static int c2c_send_ipc(struct shmem_link_device *shmd, int dev)
{
	struct link_device *ld = &shmd->ld;
	int ret;
	u16 mask;

	if (atomic_read(&shmd->res_required[dev]) > 0) {
		mif_err("%s: %s_TXQ is full\n", ld->name, get_dev_name(dev));
		return 0;
	}

	if (forbid_cp_sleep(shmd) < 0) {
		trigger_forced_cp_crash(shmd);
		return -EIO;
	}

	ret = xmit_ipc_msg(shmd, dev);
	if (likely(ret > 0)) {
		mask = get_mask_send(shmd, dev);
		send_int2cp(shmd, INT_NON_CMD(mask));
		get_shmem_status(shmd, TX, msq_get_free_slot(&shmd->stat_list));
		goto exit;
	}

	/* If there was no TX, just exit */
	if (ret == 0)
		goto exit;

	/* At this point, ret < 0 */
	if ((ret == -ENOSPC) || (ret == -EBUSY)) {
		/* Prohibit CP from sleeping until the TXQ buffer is empty */
		if (forbid_cp_sleep(shmd) < 0) {
			trigger_forced_cp_crash(shmd);
			goto exit;
		}

		/*----------------------------------------------------*/
		/* shmd->res_required[dev] was set in xmit_ipc_msg(). */
		/*----------------------------------------------------*/

		if (dev == IPC_RAW)
			mif_netif_stop(ld);

		queue_delayed_work(ld->tx_wq, ld->tx_dwork[dev], 0);
	}

exit:
	permit_cp_sleep(shmd);
	return ret;
}

/**
 * c2c_try_send_ipc
 * @shmd: pointer to an instance of shmem_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 * @iod: pointer to an instance of the io_device structure
 * @skb: pointer to an skb that will be transmitted
 *
 * 1) Enqueues an skb to the skb_txq for @dev in the link device instance.
 * 2) Tries to transmit IPC messages with c2c_send_ipc().
 */
static void c2c_try_send_ipc(struct shmem_link_device *shmd, int dev,
			struct io_device *iod, struct sk_buff *skb)
{
	struct link_device *ld = &shmd->ld;
	struct sk_buff_head *txq = ld->skb_txq[dev];
	int ret;

	if (unlikely(txq->qlen >= MAX_SKB_TXQ_DEPTH)) {
		mif_err("%s: %s txq->qlen %d >= %d\n", ld->name,
			get_dev_name(dev), txq->qlen, MAX_SKB_TXQ_DEPTH);
		dev_kfree_skb_any(skb);
		return;
	}

	skb_queue_tail(txq, skb);

	ret = c2c_send_ipc(shmd, dev);
	if (ret < 0) {
		mif_err("%s->%s: ERR! shmem_send_ipc fail (err %d)\n",
			iod->name, ld->name, ret);
	}
}

static int c2c_send_udl_cmd(struct shmem_link_device *shmd, int dev,
			struct io_device *iod, struct sk_buff *skb)
{
	struct link_device *ld = &shmd->ld;
	u8 *buff;
	u8 *src;
	u32 qsize;
	u32 in;
	int space;
	int tx_bytes;
	struct circ_status circ;

	if (iod->format == IPC_BOOT) {
		pr_ipc(1, "[AP->CP] DL CMD", skb->data,
			(skb->len > 20 ? 20 : skb->len));
	} else {
		pr_ipc(1, "[AP->CP] UL CMD", skb->data,
			(skb->len > 20 ? 20 : skb->len));
	}

	/* Get the size of free space in the TXQ */
	space = get_txq_space(shmd, dev, &circ);
	if (space < 0) {
		reset_txq_circ(shmd, dev);
		tx_bytes = -EIO;
		goto exit;
	}

	/* Get the size of data to be sent */
	tx_bytes = skb->len;

	/* Check the size of free space */
	if (space < tx_bytes) {
		mif_err("%s: NOSPC in %s_TXQ {qsize:%u in:%u out:%u}, "
			"free:%u < tx_bytes:%u\n", ld->name, get_dev_name(dev),
			circ.qsize, circ.in, circ.out, space, tx_bytes);
		tx_bytes = -ENOSPC;
		goto exit;
	}

	/* Write data to the TXQ */
	buff = circ.buff;
	src = skb->data;
	qsize = circ.qsize;
	in = circ.in;
	circ_write(buff, src, qsize, in, tx_bytes);

	/* Update new head (in) pointer */
	set_txq_head(shmd, dev, circ_new_pointer(qsize, circ.in, tx_bytes));

exit:
	dev_kfree_skb_any(skb);
	return tx_bytes;
}

static int c2c_send_udl_data(struct shmem_link_device *shmd, int dev)
{
	struct link_device *ld = &shmd->ld;
	struct sk_buff_head *txq = ld->skb_txq[dev];
	struct sk_buff *skb;
	u8 *src;
	int tx_bytes;
	int copied;
	u8 *buff;
	u32 qsize;
	u32 in;
	u32 out;
	int space;
	struct circ_status circ;

	/* Get the size of free space in the TXQ */
	space = get_txq_space(shmd, dev, &circ);
	if (space < 0) {
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
		/* Trigger a enforced CP crash */
		trigger_forced_cp_crash(shmd);
#endif
		/* Empty out the TXQ */
		reset_txq_circ(shmd, dev);
		return -EFAULT;
	}

	buff = circ.buff;
	qsize = circ.qsize;
	in = circ.in;
	out = circ.out;
	space = circ.size;

	copied = 0;
	while (1) {
		skb = skb_dequeue(txq);
		if (!skb)
			break;

		/* Get the size of data to be sent */
		src = skb->data;
		tx_bytes = skb->len;

		/* Check the free space size comparing with skb->len */
		if (space < tx_bytes) {
			/* Set res_required flag for the "dev" */
			atomic_set(&shmd->res_required[dev], 1);

			/* Take the skb back to the skb_txq */
			skb_queue_head(txq, skb);

			mif_info("NOSPC in RAW_TXQ {qsize:%u in:%u out:%u}, "
				"space:%u < tx_bytes:%u\n",
				qsize, in, out, space, tx_bytes);
			break;
		}

		/*
		** TX only when there is enough space in the TXQ
		*/
		circ_write(buff, src, qsize, in, tx_bytes);

		copied += tx_bytes;
		in = circ_new_pointer(qsize, in, tx_bytes);
		space -= tx_bytes;

		dev_kfree_skb_any(skb);
	}

	/* Update new head (in) pointer */
	if (copied > 0) {
		in = circ_new_pointer(qsize, circ.in, copied);
		set_txq_head(shmd, dev, in);
	}

	return copied;
}

/**
 * c2c_send_udl
 * @shmd: pointer to an instance of shmem_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 * @iod: pointer to an instance of the io_device structure
 * @skb: pointer to an skb that will be transmitted
 *
 * 1) Enqueues an skb to the skb_txq for @dev in the link device instance.
 * 2) Tries to transmit IPC messages in the skb_txq by invoking xmit_ipc_msg()
 *    function.
 * 3) Sends an interrupt to CP if there is no error from xmit_ipc_msg().
 * 4) Starts SHMEM flow control if xmit_ipc_msg() returns -ENOSPC.
 */
static void c2c_send_udl(struct shmem_link_device *shmd, int dev,
			struct io_device *iod, struct sk_buff *skb)
{
	struct link_device *ld = &shmd->ld;
	struct sk_buff_head *txq = ld->skb_txq[dev];
	struct completion *cmpl = &shmd->req_ack_cmpl[dev];
	struct std_dload_info *dl_info = &shmd->dl_info;
	struct mem_status mst;
	u32 timeout = msecs_to_jiffies(RES_ACK_WAIT_TIMEOUT);
	u32 udl_cmd;
	int ret;
	u16 mask = get_mask_send(shmd, dev);

	udl_cmd = std_udl_get_cmd(skb->data);
	if (iod->format == IPC_RAMDUMP || !std_udl_with_payload(udl_cmd)) {
		ret = c2c_send_udl_cmd(shmd, dev, iod, skb);
		if (ret > 0)
			send_int2cp(shmd, INT_NON_CMD(mask));
		else
			mif_err("UDL CMD xmit_ipc_msg fail (err %d)\n", ret);
		goto exit;
	}

	skb_queue_tail(txq, skb);
	if (txq->qlen < dl_info->num_frames)
		goto exit;

	mask |= get_mask_req_ack(shmd, dev);
	while (1) {
		ret = c2c_send_udl_data(shmd, dev);
		if (ret < 0) {
			mif_err("ERR! c2c_send_udl_data fail\n");
			skb_queue_purge(txq);
			break;
		}

		if (skb_queue_empty(txq)) {
			send_int2cp(shmd, INT_NON_CMD(mask));
			break;
		}

		send_int2cp(shmd, INT_NON_CMD(mask));

		do {
			ret = wait_for_completion_timeout(cmpl, timeout);
			get_shmem_status(shmd, TX, &mst);
		} while (mst.head[dev][TX] != mst.tail[dev][TX]);
	}

exit:
	return;
}

/**
 * c2c_send
 * @ld: pointer to an instance of the link_device structure
 * @iod: pointer to an instance of the io_device structure
 * @skb: pointer to an skb that will be transmitted
 *
 * Returns the length of data transmitted or an error code.
 *
 * Normal call flow for an IPC message:
 *   c2c_try_send_ipc -> c2c_send_ipc -> xmit_ipc_msg -> write_ipc_to_txq
 *
 * Call flow on congestion in a IPC TXQ:
 *   c2c_try_send_ipc -> c2c_send_ipc -> xmit_ipc_msg ,,, queue_delayed_work
 *   => xxx_tx_work -> wait_for_res_ack
 *   => msg_handler
 *   => process_res_ack -> xmit_ipc_msg (,,, queue_delayed_work ...)
 */
static int c2c_send(struct link_device *ld, struct io_device *iod,
			struct sk_buff *skb)
{
	struct shmem_link_device *shmd = to_shmem_link_device(ld);
	int dev = iod->format;
	int len = skb->len;

	switch (dev) {
	case IPC_FMT:
	case IPC_RAW:
		if (likely(ld->mode == LINK_MODE_IPC)) {
			c2c_try_send_ipc(shmd, dev, iod, skb);
		} else {
			mif_err("%s->%s: ERR! ld->mode != LINK_MODE_IPC\n",
				iod->name, ld->name);
			dev_kfree_skb_any(skb);
		}
		return len;

	case IPC_BOOT:
	case IPC_RAMDUMP:
		c2c_send_udl(shmd, IPC_RAW, iod, skb);
		return len;

	default:
		mif_err("%s: ERR! no TXQ for %s\n", ld->name, iod->name);
		dev_kfree_skb_any(skb);
		return -ENODEV;
	}
}

static int c2c_dload_start(struct link_device *ld, struct io_device *iod)
{
	struct shmem_link_device *shmd = to_shmem_link_device(ld);
	u32 magic;

	ld->mode = LINK_MODE_DLOAD;

	clear_shmem_map(shmd);

	set_magic(shmd, SHM_BOOT_MAGIC);
	magic = get_magic(shmd);
	if (magic != SHM_BOOT_MAGIC) {
		mif_err("%s: magic 0x%08X != SHM_BOOT_MAGIC 0x%08X\n",
			ld->name, magic, SHM_BOOT_MAGIC);
		return -EFAULT;
	}

	return 0;
}

static int c2c_force_dump(struct link_device *ld, struct io_device *iod)
{
	struct shmem_link_device *shmd = to_shmem_link_device(ld);
	mif_err("+++\n");
	trigger_forced_cp_crash(shmd);
	mif_err("---\n");
	return 0;
}

static int c2c_dump_start(struct link_device *ld, struct io_device *iod)
{
	struct shmem_link_device *shmd = to_shmem_link_device(ld);

	clear_shmem_map(shmd);

	mif_err("%s: magic = 0x%08X\n", ld->name, SHM_DUMP_MAGIC);
	set_magic(shmd, SHM_DUMP_MAGIC);

	return 0;
}

/**
 * c2c_set_firm_info
 * @ld: pointer to an instance of link_device structure
 * @iod: pointer to an instance of io_device structure
 * @arg: pointer to an instance of std_dload_info structure in "user" memory
 *
 */
static int c2c_set_firm_info(struct link_device *ld, struct io_device *iod,
			unsigned long arg)
{
	struct shmem_link_device *shmd = to_shmem_link_device(ld);
	struct std_dload_info *dl_info = &shmd->dl_info;
	int ret;

	ret = copy_from_user(dl_info, (void __user *)arg,
			sizeof(struct std_dload_info));
	if (ret) {
		mif_err("ERR! copy_from_user fail!\n");
		return -EFAULT;
	}

	return 0;
}

static void c2c_remap_4mb_ipc_region(struct shmem_link_device *shmd)
{
	struct shmem_4mb_phys_map *map;
	struct shmem_ipc_device *dev;
	mif_err("+++\n");

	map = (struct shmem_4mb_phys_map *)shmd->base;

	/* Magic code and access enable fields */
	shmd->ipc_map.magic = (u32 __iomem *)&map->magic;
	shmd->ipc_map.access = (u32 __iomem *)&map->access;

	/* FMT */
	dev = &shmd->ipc_map.dev[IPC_FMT];

	strcpy(dev->name, "FMT");
	dev->id = IPC_FMT;

	dev->txq.head = (u32 __iomem *)&map->fmt_tx_head;
	dev->txq.tail = (u32 __iomem *)&map->fmt_tx_tail;
	dev->txq.buff = (u8 __iomem *)&map->fmt_tx_buff[0];
	dev->txq.size = SHM_4M_FMT_TX_BUFF_SZ;

	dev->rxq.head = (u32 __iomem *)&map->fmt_rx_head;
	dev->rxq.tail = (u32 __iomem *)&map->fmt_rx_tail;
	dev->rxq.buff = (u8 __iomem *)&map->fmt_rx_buff[0];
	dev->rxq.size = SHM_4M_FMT_RX_BUFF_SZ;

	dev->mask_req_ack = INT_MASK_REQ_ACK_F;
	dev->mask_res_ack = INT_MASK_RES_ACK_F;
	dev->mask_send    = INT_MASK_SEND_F;

	/* RAW */
	dev = &shmd->ipc_map.dev[IPC_RAW];

	strcpy(dev->name, "RAW");
	dev->id = IPC_RAW;

	dev->txq.head = (u32 __iomem *)&map->raw_tx_head;
	dev->txq.tail = (u32 __iomem *)&map->raw_tx_tail;
	dev->txq.buff = (u8 __iomem *)&map->raw_tx_buff[0];
	dev->txq.size = SHM_4M_RAW_TX_BUFF_SZ;

	dev->rxq.head = (u32 __iomem *)&map->raw_rx_head;
	dev->rxq.tail = (u32 __iomem *)&map->raw_rx_tail;
	dev->rxq.buff = (u8 __iomem *)&map->raw_rx_buff[0];
	dev->rxq.size = SHM_4M_RAW_RX_BUFF_SZ;

	dev->mask_req_ack = INT_MASK_REQ_ACK_R;
	dev->mask_res_ack = INT_MASK_RES_ACK_R;
	dev->mask_send    = INT_MASK_SEND_R;

	/* interrupt ports */
	shmd->ipc_map.mbx2ap = NULL;
	shmd->ipc_map.mbx2cp = NULL;

	mif_err("---\n");
}

static int c2c_init_ipc_map(struct shmem_link_device *shmd)
{
	mif_err("+++\n");

	if (shmd->size >= SHMEM_SIZE_4MB)
		c2c_remap_4mb_ipc_region(shmd);
	else
		return -EINVAL;

	memset(shmd->base, 0, shmd->size);

	shmd->magic = shmd->ipc_map.magic;
	shmd->access = shmd->ipc_map.access;

	shmd->dev[IPC_FMT] = &shmd->ipc_map.dev[IPC_FMT];
	shmd->dev[IPC_RAW] = &shmd->ipc_map.dev[IPC_RAW];

	shmd->mbx2ap = shmd->ipc_map.mbx2ap;
	shmd->mbx2cp = shmd->ipc_map.mbx2cp;

	mif_err("---\n");
	return 0;
}

#if 0
static void c2c_link_terminate(struct link_device *ld, struct io_device *iod)
{
	if (iod->format == IPC_FMT && ld->mode == LINK_MODE_IPC) {
		if (!atomic_read(&iod->opened)) {
			ld->mode = LINK_MODE_OFFLINE;
			mif_err("%s: %s: link mode changed: IPC -> OFFLINE\n",
				iod->name, ld->name);
		}
	}

	return;
}
#endif

struct link_device *c2c_create_link_device(struct platform_device *pdev)
{
	struct shmem_link_device *shmd = NULL;
	struct link_device *ld = NULL;
	struct modem_data *modem = NULL;
	int err = 0;
	int i = 0;
	int irq;
	u32 phys_base;
	u32 offset;
	u32 size;
	unsigned long irq_flags;
	char name[MIF_MAX_NAME_LEN];
	mif_err("+++\n");

	/*
	** Get the modem (platform) data
	*/
	modem = (struct modem_data *)pdev->dev.platform_data;
	if (!modem) {
		mif_err("ERR! modem == NULL\n");
		goto error;
	}
	mif_err("%s: %s\n", modem->link_name, modem->name);

	if (modem->ipc_version < SIPC_VER_50) {
		mif_err("%s: %s: ERR! IPC version %d < SIPC_VER_50\n",
			modem->link_name, modem->name, modem->ipc_version);
		goto error;
	}

	/*
	** Alloc an instance of shmem_link_device structure
	*/
	shmd = kzalloc(sizeof(struct shmem_link_device), GFP_KERNEL);
	if (!shmd) {
		mif_err("%s: ERR! shmd kzalloc fail\n", modem->link_name);
		goto error;
	}
	ld = &shmd->ld;

	/*
	** Retrieve modem data and SHMEM control data from the modem data
	*/
	ld->mdm_data = modem;
	ld->name = modem->link_name;
	ld->aligned = 1;
	ld->ipc_version = modem->ipc_version;
	ld->max_ipc_dev = MAX_SIPC5_DEV;

	/*
	** Set attributes as a link device
	*/
#if 0
	ld->terminate_comm = c2c_link_terminate;
#endif
	ld->send = c2c_send;
	ld->dload_start = c2c_dload_start;
	ld->firm_update = c2c_set_firm_info;
	ld->force_dump = c2c_force_dump;
	ld->dump_start = c2c_dump_start;

	INIT_LIST_HEAD(&ld->list);

	skb_queue_head_init(&ld->sk_fmt_tx_q);
	skb_queue_head_init(&ld->sk_raw_tx_q);
	ld->skb_txq[IPC_FMT] = &ld->sk_fmt_tx_q;
	ld->skb_txq[IPC_RAW] = &ld->sk_raw_tx_q;

	skb_queue_head_init(&ld->sk_fmt_rx_q);
	skb_queue_head_init(&ld->sk_raw_rx_q);
	ld->skb_rxq[IPC_FMT] = &ld->sk_fmt_rx_q;
	ld->skb_rxq[IPC_RAW] = &ld->sk_raw_rx_q;

	init_completion(&ld->init_cmpl);
	init_completion(&ld->pif_cmpl);

	/*
	** Retrieve SHMEM resource
	*/
	if (modem->link_types & LINKTYPE(LINKDEV_C2C)) {
		shmd->type = C2C_SHMEM;
		mif_err("%s: shmd->type = C2C_SHMEM\n", ld->name);
	} else if (modem->link_types & LINKTYPE(LINKDEV_SHMEM)) {
		shmd->type = REAL_SHMEM;
		mif_err("%s: shmd->type = REAL_SHMEM\n", ld->name);
	} else {
		mif_err("%s: ERR! invalid type\n", ld->name);
		goto error;
	}

	phys_base = c2c_get_phys_base();
	offset = c2c_get_sh_rgn_offset();
	size = c2c_get_sh_rgn_size();
	mif_err("%s: phys_base:0x%08X offset:0x%08X size:%d\n",
		ld->name, phys_base, offset, size);

	shmd->start = phys_base + offset;
	shmd->size = size;
	shmd->base = c2c_request_sh_region(shmd->start, shmd->size);
	if (!shmd->base) {
		mif_err("%s: ERR! c2c_request_sh_region fail\n", ld->name);
		goto error;
	}

	mif_err("%s: phys_addr:0x%08X virt_addr:0x%08X size:%d\n",
		ld->name, shmd->start, (int)shmd->base, shmd->size);

	/*
	** Initialize SHMEM maps (physical map -> logical map)
	*/
	err = c2c_init_ipc_map(shmd);
	if (err < 0) {
		mif_err("%s: ERR! c2c_init_ipc_map fail (err %d)\n",
			ld->name, err);
		goto error;
	}

	/*
	** Initialize locks, completions, and bottom halves
	*/
	memset(name, 0, MIF_MAX_NAME_LEN);
	snprintf(name, MIF_MAX_NAME_LEN, "%s_wlock", ld->name);
	wake_lock_init(&shmd->wlock, WAKE_LOCK_SUSPEND, name);
#if 1
	wake_lock(&shmd->wlock);
#endif

	memset(name, 0, MIF_MAX_NAME_LEN);
	snprintf(name, MIF_MAX_NAME_LEN, "%s_ap_wlock", ld->name);
	wake_lock_init(&shmd->ap_wlock, WAKE_LOCK_SUSPEND, name);

	memset(name, 0, MIF_MAX_NAME_LEN);
	snprintf(name, MIF_MAX_NAME_LEN, "%s_cp_wlock", ld->name);
	wake_lock_init(&shmd->cp_wlock, WAKE_LOCK_SUSPEND, name);

	init_completion(&shmd->udl_cmpl);
	for (i = 0; i < MAX_SIPC5_DEV; i++)
		init_completion(&shmd->req_ack_cmpl[i]);

	tasklet_init(&shmd->rx_tsk, rx_ipc_task, (unsigned long)shmd);

	for (i = 0; i < MAX_SIPC5_DEV; i++) {
		spin_lock_init(&shmd->tx_lock[i]);
		atomic_set(&shmd->res_required[i], 0);
	}

	ld->tx_wq = create_singlethread_workqueue("shmem_tx_wq");
	if (!ld->tx_wq) {
		mif_err("%s: ERR! fail to create tx_wq\n", ld->name);
		goto error;
	}
	INIT_DELAYED_WORK(&ld->fmt_tx_dwork, fmt_tx_work);
	INIT_DELAYED_WORK(&ld->raw_tx_dwork, raw_tx_work);
	ld->tx_dwork[IPC_FMT] = &ld->fmt_tx_dwork;
	ld->tx_dwork[IPC_RAW] = &ld->raw_tx_dwork;

	spin_lock_init(&shmd->stat_list.lock);
	spin_lock_init(&shmd->trace_list.lock);

	INIT_DELAYED_WORK(&shmd->cp_wakeup_drop_dwork, drop_cp_wakeup);
	INIT_DELAYED_WORK(&shmd->cp_wakeup_hold_dwork, hold_cp_wakeup);

	/*
	** Retrieve SHMEM IRQ GPIO#, IRQ#, and IRQ flags
	*/
	shmd->gpio_ap_wakeup = modem->gpio_ap_wakeup;
	shmd->irq_ap_wakeup = modem->irq_ap_wakeup;
	shmd->gpio_ap_status = modem->gpio_ap_status;

	shmd->gpio_pda_active = modem->gpio_pda_active;
	shmd->gpio_cp_wakeup = modem->gpio_cp_wakeup;
	shmd->gpio_cp_status = modem->gpio_cp_status;
	shmd->irq_cp_status = modem->irq_cp_status;
	if (!shmd->irq_cp_status) {
		mif_err("ERR! no irq_cp_status\n");
		goto error;
	}
	mif_err("CP2AP_STATUS IRQ# = %d\n", shmd->irq_cp_status);

	/*
	** Register interrupt handlers
	*/
	err = c2c_register_handler(c2c_irq_handler, shmd);
	if (err) {
		mif_err("%s: ERR! c2c_register_handler fail (err %d)\n",
			ld->name, err);
		goto error;
	}

	memset(name, 0, MIF_MAX_NAME_LEN);
	snprintf(name, MIF_MAX_NAME_LEN, "%s_ap_wakeup", ld->name);
	irq = shmd->irq_ap_wakeup;
	irq_flags = (IRQF_NO_SUSPEND | IRQF_TRIGGER_RISING);
	err = mif_register_isr(irq, ap_wakeup_handler, irq_flags, name, shmd);
	if (err) {
		mif_err("%s: ERR! mif_register_isr(#%d) fail\n", ld->name, irq);
		goto error;
	}

	memset(name, 0, MIF_MAX_NAME_LEN);
	snprintf(name, MIF_MAX_NAME_LEN, "%s_cp_status", ld->name);
	irq = shmd->irq_cp_status;
	irq_flags = (IRQF_NO_SUSPEND | IRQF_TRIGGER_FALLING);
	err = mif_register_isr(irq, cp_status_handler, irq_flags, name, shmd);
	if (err) {
		mif_err("%s: ERR! mif_register_isr(#%d) fail\n", ld->name, irq);
		goto error;
	}

#if defined(CONFIG_HAS_EARLYSUSPEND)
	shmd->early_suspend.suspend = c2c_early_suspend;
	shmd->early_suspend.resume = c2c_late_resume;
	register_early_suspend(&shmd->early_suspend);
#elif defined(CONFIG_FB)
	/* Register fb_event_cb to set "pda_active" according to an FB event */
	memset(&shmd->fb_nb, 0, sizeof(struct notifier_block));
	shmd->fb_nb.notifier_call = fb_event_cb;
	fb_register_client(&shmd->fb_nb);
#endif

	mif_err("---\n");
	return ld;

error:
	kfree(shmd);
	return NULL;
}

