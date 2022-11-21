/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2006 Marvell International Ltd.
 * All Rights Reserved
 */


/* this enum define the event type*/
/*ICAT EXPORTED ENUM*/

/* user level ioctl commands for accessing APIs */
#define ACIPC_SET_EVENT         0
#define ACIPC_GET_EVENT         1
#define ACIPC_SEND_DATA         2
#define ACIPC_READ_DATA         3
#define ACIPC_BIND_EVENT        4
#define ACIPC_UNBIND_EVENT      5
#define ACIPC_GET_BIND_EVENT_ARG        6

#define ACIPC_NUMBER_OF_EVENTS (10)
#define ACIPC_NUMBER_OF_INTERRUPTS (3)
#define ACIPC_INT0_EVENTS (0xff)
#define ACIPC_INT1_EVENTS (ACIPC_DDR_260_READY_REQ)
#define ACIPC_INT2_EVENTS (ACIPC_DDR_READY_REQ)


#if 0
typedef enum
{
	ACIPC_DDR_RELQ_REQ      = 0x00000001,
	ACIPC_DDR_RELQ_ACK      = 0x00000001,
	ACIPC_DDR_260_RELQ_REQ  = 0x00000002,
	ACIPC_DDR_260_RELQ_ACK  = 0x00000002,
	ACIPC_MSL_SLEEP_ALLOW   = 0x00000004,
	ACIPC_MSL_WAKEUP_ACK    = 0x00000008,
	ACIPC_MSL_WAKEUP_REQ    = 0x00000010,
	ACIPC_DATA_IND          = 0x00000020,
	ACIPC_SPARE_2           = 0x00000040,
	ACIPC_SPARE_1           = 0x00000080,
	ACIPC_DDR_260_READY_REQ = 0x00000100,
	ACIPC_DDR_260_READY_ACK = 0x00000100,
	ACIPC_DDR_READY_REQ     = 0x00000200,
	ACIPC_DDR_READY_ACK     = 0x00000200,
}acipc_events;
#endif
typedef enum
{
	ACIPC_RINGBUF_TX_STOP   = 0x00000001,
	ACIPC_RINGBUF_TX_RESUME = 0x00000002,
	ACIPC_PORT_FLOWCONTROL  = 0x00000004,
	ACIPC_SPARE_1 = 0x00000008,
	ACIPC_SPARE_2 = 0x00000010,
	ACIPC_SPARE_3 = 0x00000020,
	ACIPC_SPARE_4 = 0x00000040,
	ACIPC_SPARE_5 = 0x00000080,
	ACIPC_SHM_PACKET_NOTIFY = 0x00000100,
	ACIPC_IPM       = 0x00000200,
}acipc_events;
/* used by clients when binding a callback to an event*/
/*ICAT EXPORTED ENUM*/
typedef enum
{
	ACIPC_CB_NORMAL = 0,            /* callback will be called only if the DDR available        */
	ACIPC_CB_ALWAYS_NO_DDR          /* callback will be called always ,even if the DDR is not available*/
}acipc_callback_mode;


struct acipc_ioctl_arg {
	unsigned int arg;
	acipc_events set_event;
	acipc_callback_mode cb_mode;
};

