#ifndef _ET300_LINUX_DIRVER_H_
#define _ET300_LINUX_DIRVER_H_

#include <asm/ioctl.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/vmalloc.h>
#include "et300_register_define.h"

#define ET300_SPI_DEBUG

#ifdef ET300_SPI_DEBUG
//#define DEBUG_PRINT(fmt, args...) printk(KERN_INFO fmt, ## args)
#define DEBUG_PRINT(fmt, args...) printk("[SPI-EGIS]" fmt, ## args)
#else
#define DEBUG_PRINT(fmt, args...)    /* Don't do anything in release builds      */
#endif

#define ET300_MAJOR             101 /* assigned */
#define N_SPI_MINORS            32  /* ... up to 256 */

#define ET300_ADDRESS_0     0x00      //0000 0000
#define ET300_WRITE_ADDRESS 0xAC      //1010 1100
#define ET300_READ_DATA     0xAF      //1010 1111
#define ET300_WRITE_DATA    0xAE      //1010 1110

#define ET300_READ_BUF_SZ	(16384)

#define RECON_PADDING_ON					1
#define PADDING_SIZE						256
#define PADDING_SIZE_HALF					PADDING_SIZE/2
#define NO_PADDING_SIZE						192
#define NO_PADDING_SIZE_HALF				NO_PADDING_SIZE/2
#define JADE_UP_RECON						1
#define JADE_DOWN_RECON						2


//================================ IOCTL =======================================//
//==============================================================================//

struct egis_ioc_transfer {
	__u64		tx_buf;
	__u64		rx_buf;

	__u32		len;
	__u32		speed_hz;

	__u16		delay_usecs;
	__u8		bits_per_word;
	__u8		cs_change;
	__u8		opcode;
	__u8		pad[3];
//  __u32		pad;

};
#define EGIS_IOC_MAGIC			'k'
#define EGIS_MSGSIZE(N) \
	((((N)*(sizeof (struct egis_ioc_transfer))) < (1 << _IOC_SIZEBITS)) \
		? ((N)*(sizeof (struct egis_ioc_transfer))) : 0)
#define EGIS_IOC_MESSAGE(N) _IOW(EGIS_IOC_MAGIC, 0, char[EGIS_MSGSIZE(N)])
//==============================================================================//
//==============================================================================//
struct et300_data {
	dev_t           devt;
	spinlock_t      spi_lock;
	struct spi_device   *spi;
	struct list_head    device_entry;

	/* buffer is NULL unless this device is open (users > 0) */
	struct mutex        buf_lock;
	unsigned        users;
	u8          *buffer;
	u8 		*read_data;
	uint8_t 	*work_buf;
};

//#define TRANSFER_MODE_DMA

#endif
