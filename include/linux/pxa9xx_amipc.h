/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2012 Marvell International Ltd.
 * All Rights Reserved
 */

#ifndef _PXA9XX_AMIPC_H_
#define _PXA9XX_AMIPC_H_

#define DEFAULT_TIMEOUT         (100)

/* user level ioctl commands for accessing APIs */
#define AMIPC_IOC_MAGIC		0xD2
#define AMIPC_SET_EVENT		_IO(AMIPC_IOC_MAGIC, 0)
#define AMIPC_GET_EVENT		_IO(AMIPC_IOC_MAGIC, 1)
#define AMIPC_SEND_DATA		_IO(AMIPC_IOC_MAGIC, 2)
#define AMIPC_READ_DATA		_IO(AMIPC_IOC_MAGIC, 3)
#define AMIPC_BIND_EVENT	_IO(AMIPC_IOC_MAGIC, 4)
#define AMIPC_UNBIND_EVENT	_IO(AMIPC_IOC_MAGIC, 5)
#define AMIPC_IPC_CRL		_IO(AMIPC_IOC_MAGIC, 6)
#define AMIPC_IPC_MSG_GET	_IO(AMIPC_IOC_MAGIC, 7)
#define AMIPC_TEST_CRL		_IO(AMIPC_IOC_MAGIC, 8)

/* TEST CRL */
#define TEST_CRL_CLR		(1)
#define TEST_CRL_SET_TX		(2)
#define TEST_CRL_SET_RX		(3)
#define TEST_CRL_GEN_IRQ	(4)
/* ACQ related */
#define AMIPC_ACQ_GET_MEM	(0xF0000000)
#define AMIPC_ACQ_REL_MEM	(0xF0000001)
#define AMIPC_ACQ_CACHE_FLUSH	(0xF0000002)
#define AMIPC_ACQ_SET_TO_DMACHAIN      (0xF0000003)
#define AMIPC_ACQ_CLR_DMACHAIN      (0xF0000004)

typedef u32(*amipc_rec_event_callback) (u32 events_status);

enum amipc_events {
	AMIPC_SHM_PACKET_NOTIFY = 0x00000001,
	AMIPC_RINGBUF_FC = 0x00000002,
	AMIPC_ACQ_COMMAND = 0x00000003,
	AMIPC_LL_PING = 0x00000004,
	AMIPC_EVENT_LAST = AMIPC_LL_PING,
};

enum amipc_return_code {
	AMIPC_RC_OK = 0,
	AMIPC_EVENT_ALREADY_BIND,
	AMIPC_RC_FAILURE,
	AMIPC_RC_TIMEOUT,
	AMIPC_RC_AGAIN
};

#ifdef AMIPC_DEBUG
#define IPCTRACE(format, args...) \
	pr_info(format, ## args)
#define	IPC_ENTER()	\
	pr_info("IPC: ENTER %s\n", __func__)
#define	IPC_LEAVE()	\
	pr_info("IPC: LEAVE %s\n", __func__)
#else
#define IPCTRACE(s...)	do {} while (0)
#define	IPC_ENTER()	do {} while (0)
#define	IPC_LEAVE()	do {} while (0)
#endif

struct amipc_ioctl_arg {
	enum amipc_events event;
	u32 data1, data2;
	u32 more_msgs;
};

/* declared the export APIs for TD telephony */
extern enum amipc_return_code amipc_setbase(phys_addr_t base_addr, int len);
extern enum amipc_return_code amipc_eventbind(u32 user_event,
					     amipc_rec_event_callback cb);
extern enum amipc_return_code amipc_eventunbind(u32 user_event);
extern enum amipc_return_code amipc_eventset(enum amipc_events user_event,
						int timeout_ms);
extern enum amipc_return_code amipc_datasend(enum amipc_events user_event,
				u32 data1, u32 data2, int timeout_ms);
extern enum amipc_return_code amipc_dataread(enum amipc_events user_event,
					u32 *data1, u32 *data2);
extern enum amipc_return_code amipc_dump_debug_info(void);

#endif	/* _PXA9XX_AMIPC_H_ */
