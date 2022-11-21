#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/ioctl.h>

#include "diag.h"

extern int diagRegisterRxCallBack(DiagRxCallbackFunc callback);
extern int diagUnregisterRxCallBack(void);

typedef int (*marvell_diag_rx_callback)(char* packet, int len);
extern marvell_diag_rx_callback gs_marvell_diag_rx_callback;

typedef int (*marvell_diag_ioctl)(unsigned int cmd, unsigned long arg);
extern marvell_diag_ioctl gs_marvell_diag_ioctl;

extern int gs_marvell_diag_send(const unsigned char *buf, int count);

extern int gRcvdCpPkt;
extern int gRcvdCpLongPkt;
extern int gUsbSentCpPkt;
extern int gUsbSentCpPktError;

#define DIAG_CORE_SELECTOR_RX_SHIFT         7
#define ICAT_READY_NOTIFY                   4
#define ATOMIC_READY_NOTIFY_AND_DB_VER      10

typedef struct
{
	u8 sap;
	u8 serviceid;
	u16 moduleid;
	u32 commid;
	u32 sourceid;
	u8 params[1]; //contains the requested command input parameters
}RxPDU;
#if 0
static u8 get_message_coreid(u8* pData)
{
	if ( !pData )
		return -1;

#if !defined (DIAGNEWHEADER)

	return (((RxPDU*)pData)->sap>> DIAG_CORE_SELECTOR_RX_SHIFT);

#else   /* DIAG_NEW_HEADER is defined */

	return ((TxPDUNewHeader *)pData)->source;

#endif  /* DIAG_NEW_HEADER */
}

static bool is_icat_ready_notify(u8* pdata)
{
#if !defined (DIAGNEWHEADER)
	RxPDU *pRxPdu;
#else   /* DIAGNEWHEADER is defined */
	RxMsgHolder *pRxMsgHolder;
#endif /* DIAGNEWHEADER */
	if (!pdata) {
		return false;
	}
#if !defined (DIAGNEWHEADER)
	pRxPdu = (RxPDU *)pdata;

	return ((pRxPdu->serviceid == ICAT_READY_NOTIFY) || (pRxPdu->serviceid == ATOMIC_READY_NOTIFY_AND_DB_VER));  /* pData[1] is start of serviceID - UINT8 field  */

#else /* DIAGNEWHEADER is defined */
	pRxMsgHolder = (RxMsgHolder *)pdata;

	return ((pRxMsgHolder->messageSAP == SAP_INTERNAL_SERVICE_REQUEST) &&
		(pRxMsgHolder->isSapRelatedData & SAP_RELATED_DATA_BIT) &&
		((pRxMsgHolder->sapRelatedData == ICAT_READY_NOTIFY) || (pRxMsgHolder->sapRelatedData == ATOMIC_READY_NOTIFY_AND_DB_VER)));

#endif  /* DIAGNEWHEADER */
}
#endif

/* function: gs_marvell_diag_rx
    reture value is used for TTY push filter in diag low level driver.
   

    return value:
       1: do not need to push to TTY. (default)
       0: need push to TTY.

    NOTE:Now, the function will always return 0, all the data should be pushed to user space.
*/
int gs_marvell_diag_rx(char *packet, int size)
{
	return 0;
}

int gs_marvell_diag_ioctl_callback(unsigned int cmd, unsigned long arg)
{
	int val;
	switch (cmd)
	{
	case DIAGSTUB_USBADD:
#ifndef DIAG_USB_TEST
		diagUsbConnectNotify();
#endif
		val = 0;
		break;

	case DIAGSTUB_USBREMOVE:
#ifndef DIAG_USB_TEST
		diagUsbDisConnectNotify();
#endif
		val = 0;
		break;
#ifdef DIAG_USB_TEST
	case DIAGSTUB_TESTSTART:
		printk("daig usb test case %d invoked.\n", arg);
		diagUsbTest(arg);
		val = 0;
		break;
#endif
	case DIAGSTUB_DUMPCOUNT:
		printk("received diag packet from CP %d\n", gRcvdCpPkt);
		printk("received long diag packet from CP %d\n", gRcvdCpLongPkt);
		printk("send diag packet to USB successfully %d\n", gUsbSentCpPkt);
		printk("send diag packet to USB failed %d\n", gUsbSentCpPktError);
		val = 0;
		break;
	default:
		/* could not handle ioctl */
		val = -ENOIOCTLCMD;
	}
	return val;
}

static int __init gs_module_init(void)
{
	diagRegisterRxCallBack((DiagRxCallbackFunc)gs_marvell_diag_send);
	gs_marvell_diag_rx_callback = (marvell_diag_rx_callback)gs_marvell_diag_rx;
	gs_marvell_diag_ioctl = (marvell_diag_ioctl)gs_marvell_diag_ioctl_callback;
	return 0;
}

static void __exit gs_module_exit(void)
{
	diagUnregisterRxCallBack();
	gs_marvell_diag_rx_callback = (marvell_diag_rx_callback)NULL;
	gs_marvell_diag_ioctl = (marvell_diag_ioctl)NULL;
}

/* Module */
MODULE_DESCRIPTION("Marvell USB Diag");
MODULE_AUTHOR("Al Borchers");
MODULE_LICENSE("GPL");


module_init(gs_module_init);
module_exit(gs_module_exit);


