/*
 *  pxa-geu driver for generic encrypt unit
 *
 *  Copyright (C) 2009, Marvell International Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uio.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/fb.h>
#include <linux/pagemap.h>
#include <linux/dma-mapping.h>
#include <linux/random.h>
#include <linux/miscdevice.h>
#include <linux/pm_qos.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/page.h>
#include <asm/mman.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <mach/cputype.h>
#include <mach/dma.h>

#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))
#define KERN_USE_UNLOCKEDIOCTL
#endif

#include <mach/regs-apmu.h>
#include <mach/irqs.h>

#include "pxa-geu.h"

//#define GEU_DEBUG_INFO
#ifdef GEU_DEBUG_INFO
#define GEU_DEBUG(x)	do{x;}while(0)
#else
#define GEU_DEBUG(x)
#endif

#define GEU_MODE_DMA

#define GEU_DRIVER_VERSION	"GEU drvier 0.0.5"

#define PXA_GEU_IOMEM		0xD4201000
#define PXA_GEU_IOMEM_SIZE	0x00001000

#if defined(CONFIG_CPU_PXA910)
#define PXA_GEU_IRQ			IRQ_PXA910_AEU
#elif defined(CONFIG_CPU_PXA988)
#define PXA_GEU_IRQ		IRQ_PXA988_AEU
#endif

#define GEU_DMA_PAGES		512	/* max transfer pages in one dma command */
#define GEU_DMA_SIZE		(2*GEU_DMA_PAGES*sizeof(pxa_dma_desc))

/* DMA threshold 2k: must < 8k &&  must < GEU_DMA_SIZE */
#define GEU_DMA_THRESHOLD	0x800
#define GEU_DMA_TIMEOUT		(msecs_to_jiffies(1000))	/*1s in msec */

#define GEU_DMA_ALIGN(x)	((unsigned long)(x) & 0xf)	/* GEU DMA require 16bytes aligned address */
#define DMA_PHY_ADDR(x)		(dma_phy_base + ((unsigned int)(x)-(unsigned int)dma_buf_base))
#define GET_PAGE_NUM(a,l)	(1+(((((unsigned long)(a)+(l)-1) & PAGE_MASK) - ((unsigned long)(a) & PAGE_MASK)) >> PAGE_SHIFT))

#define GEU_IN_DRCMR		68	/* GEU DRCMR 0x1110 */
#define GEU_OUT_DRCMR		69	/* GEU DRCMR 0x1114 */

#define GEU_IOCTL_MAGIC 'g'
#define GEU_AES_INIT		_IOW(GEU_IOCTL_MAGIC, 1, unsigned int)
#define GEU_AES_PROCESS		_IOW(GEU_IOCTL_MAGIC, 2, unsigned int)
#define GEU_AES_FINISH		_IOW(GEU_IOCTL_MAGIC, 3, unsigned int)
#define GEU_AES_INIT_RKEK	_IOW(GEU_IOCTL_MAGIC, 4, unsigned int)
#define GEU_AES_INIT_CBC	_IOW(GEU_IOCTL_MAGIC, 5, unsigned int)
#define GEU_GEN_RAND		_IOW(GEU_IOCTL_MAGIC, 6, unsigned int)
#define GEU_READ_OEMHASHKEY _IOW(GEU_IOCTL_MAGIC, 7, unsigned int)

#define GEU_SHA160_SIZE		20
#define GEU_SHA224_SIZE		28
#define GEU_SHA256_SIZE		32

struct geu_arg {
	int arg0;
	int arg1;
	int arg2;
	int arg3;
};

typedef struct {
	struct page *pages[GEU_DMA_PAGES];
	dma_addr_t addr[GEU_DMA_PAGES];
	unsigned int size[GEU_DMA_PAGES];
	unsigned int num;
} dma_info;

static void *geu_iobase;
static pid_t geu_owner = 0;
static DEFINE_MUTEX(geu_lock);
static DECLARE_COMPLETION(cmd_complete);
static unsigned char *dma_buf_base;
static unsigned int dma_phy_base;
static struct miscdevice pxa_geu_miscdev;
static struct clk *geu_clk;
#ifdef GEU_MODE_DMA
static struct pm_qos_request geu_qos_idle;
static int geu_dma_in;
static int geu_dma_out;
pxa_dma_desc *geu_dma_in_desc;
pxa_dma_desc *geu_dma_out_desc;
dma_info *geu_dma_in_info;
dma_info *geu_dma_out_info;
#endif

static __inline void GEUWriteReg(unsigned int off, unsigned int value)
{
	*(volatile unsigned int *)(geu_iobase + off) = value;
}

static __inline unsigned int GEUReadReg(unsigned int off)
{
	return *(volatile unsigned int *)(geu_iobase + off);
}

#ifdef GEU_MODE_DMA
/* map user address into dma address */
static int pxa_dma_map(dma_info *info, void *buf,
								unsigned int sz, int dir)
{
	int ret, i;
	size_t offset, size;

	down_read(&current->mm->mmap_sem);
	ret = get_user_pages(current,
						current->mm,
						(unsigned long)buf,
						GET_PAGE_NUM(buf, sz),
						dir==DMA_TO_DEVICE?0:1,
						0,
						info->pages,
						NULL);
	up_read(&current->mm->mmap_sem);
	if (ret <= 0) {
		printk("GEU: get user pages fail %d\n", ret);
		return ret;
	}

	info->num = ret;
	for (i=0; i<ret; i++) {
		offset = (size_t)buf & (PAGE_SIZE - 1);
		size = sz>(PAGE_SIZE-offset)?(PAGE_SIZE-offset):sz;
		/* call dma_map_page to get  */
		info->addr[i] = dma_map_page(pxa_geu_miscdev.this_device,
						info->pages[i],
						offset,
						size, dir);
		info->size[i] = size;
		buf += size;
		sz -= size;
	}
	return 0;
}

/* unmap user address */
static void pxa_dma_unmap(dma_info *info, void *buf,
						size_t sz, int dir)
{
	int i;

	BUG_ON(info == NULL || buf == NULL);

	for (i=0; i<info->num; i++) {
		dma_unmap_page(pxa_geu_miscdev.this_device, info->addr[i],
						info->size[i], dir);
		/* put user page, set page dirty on output buffer */
		if (!PageReserved(info->pages[i]) && dir != DMA_TO_DEVICE) {
			SetPageDirty(info->pages[i]);
		}
		page_cache_release(info->pages[i]);
	}
}
#ifdef GEU_DEBUG_INFO
static void dump_dma_desc(pxa_dma_desc *desc, int num)
{
	int i;

	printk("DMA desc %p, num %d\n", desc, num);
	for (i=0; i<num; i++) {
		printk("\tDDADR 0x%x\n", desc[i].ddadr);
		printk("\tDSADR 0x%x\n", desc[i].dsadr);
		printk("\tDTADR 0x%x\n", desc[i].dtadr);
		printk("\tDCMD 0x%x\n", desc[i].dcmd);
	}
}
#endif
#endif

#ifdef GEU_DEBUG_INFO
static void dump_round_key(void)
{
    int i;
    printk("GEU config %x round key:\n", GEUReadReg(GEU_CONFIG));
    for (i=0x68; i< 0x100;) {
        printk("%x %x %x %x\n", GEUReadReg(i), GEUReadReg(i+4), GEUReadReg(i+8), GEUReadReg(i+12));
        i += 16;
    }
    printk("\n");
}
#endif

static int GEU_AES_init(unsigned int *key, int keylen, int encrypt)
{
	int i;
	unsigned int conf;

	if (mutex_trylock(&geu_lock) == 0) {
		printk("GEU busy. Engine has been initialized\n");
		return -EBUSY;
	}
	clk_enable(geu_clk);
	geu_owner = current->pid;

	conf = GEUReadReg(GEU_CONFIG);
	conf &= ~GEU_CONFIG_KEYSZ_MASK;
	conf |= ((keylen>>3)-1)<<GEU_CONFIG_KEYSZ_SHIFT;	/* 128 to 1, 192 to 2, 256 to 3 */
	/* set encrypt/decrypt */
	if (encrypt != 0) {
		conf &= ~GEU_CONFIG_ENCDEC;
	} else {
		conf |= GEU_CONFIG_ENCDEC;
	}
	conf |= GEU_CONFIG_KEYIMR;
	//conf |= GEU_CONFIG_RNDKEYHA|GEU_CONFIG_SBOXHA;
	GEUWriteReg(GEU_CONFIG, conf);

	/* write user key */
	for (i=0; i < 8; i++) {
		GEUWriteReg(GEU_INIT_KEY_VALUE + (i*4), key[i]);
	}
	GEUWriteReg(GEU_STATUS, GEU_STATUS_KEYREADY);

	/* wait for complete, timeout = 1s */
	if (0 == wait_for_completion_timeout(&cmd_complete, GEU_DMA_TIMEOUT)) {
		printk("GEU init AES engine timeout. status(%x)", GEUReadReg(GEU_STATUS));
		clk_disable(geu_clk);
		geu_owner = 0;
		mutex_unlock(&geu_lock);
		return -ETIME;
	}
	GEU_DEBUG(printk("##geu status(%x) config(%x)\n", GEUReadReg(GEU_STATUS), GEUReadReg(GEU_CONFIG)));
	//dump_round_key();
	return 0;
}

#ifdef GEU_MODE_DMA
/*  */
static int GEU_AES_direct(unsigned char *in, unsigned char *out, unsigned int length)
{
	int ret = 0;
	unsigned int conf;

	conf = GEUReadReg(GEU_CONFIG);
	while (length > 0) {
		unsigned int len = length>GEU_DMA_THRESHOLD?GEU_DMA_THRESHOLD:length;
		if (copy_from_user(dma_buf_base, in, len)) {
			ret = -EFAULT;
			break;
		}
		/* setup in/out DMA */
		DCSR(geu_dma_in) = DCSR_NODESC;
		DSADR(geu_dma_in) = dma_phy_base;
		DTADR(geu_dma_in) = PXA_GEU_IOMEM + GEU_INPUT_DATA_ENC_DEC;
		DCMD(geu_dma_in) = DCMD_INCSRCADDR|DCMD_FLOWTRG|DCSR_STOPIRQEN|DCMD_ENDIRQEN|DCMD_BURST16|len;

		DCSR(geu_dma_out) = DCSR_NODESC;
		DSADR(geu_dma_out) = PXA_GEU_IOMEM + GEU_OUT_DATA_AFTER_ENC_DEC;
		DTADR(geu_dma_out) = dma_phy_base;
		DCMD(geu_dma_out) = DCMD_INCTRGADDR|DCMD_FLOWSRC|DCMD_ENDIRQEN|DCMD_BURST16|len;
		/* start DMA */
		DCSR(geu_dma_out) |= DCSR_RUN;
		DCSR(geu_dma_in) |= DCSR_RUN;
		GEUWriteReg(GEU_CONFIG, conf|GEU_CONFIG_EN_DMA_MODE_AES_CIPHER);
		/* wait for complete */
		if (0 == wait_for_completion_timeout(&cmd_complete, GEU_DMA_TIMEOUT)) {
			printk("GEU AES direct timeout statu(%x)\n", GEUReadReg(GEU_STATUS));
			ret = -ETIME;
			break;
		}
		if (copy_to_user(out, dma_buf_base, len)){
			ret = -EFAULT;
			break;
		}
		in += len;
		out += len;
		length -= len;
	}

	GEUWriteReg(GEU_CONFIG, conf);
	return ret;
}

static int GEU_AES_chain(unsigned char *in, unsigned char *out, unsigned int length)
{
	int ret = 0;
	unsigned int i;
	unsigned int conf;
	dma_info *info;

	conf = GEUReadReg(GEU_CONFIG);

	/* for VIVT cache, we should flush user cache. pxa_dma_map only flush kernel & L2 cache. */
	__cpuc_flush_user_range((unsigned long)in, length, 0);
	while (length > 0) {
		/* setup transfer length. reserve 3 pages for unaligned address */
		unsigned int len = length>((GEU_DMA_PAGES-3)*PAGE_SIZE)?((GEU_DMA_PAGES-3)*PAGE_SIZE):length;
		if (in == out) {
			ret = pxa_dma_map(geu_dma_in_info, in, len, DMA_BIDIRECTIONAL);
		} else {
			ret = pxa_dma_map(geu_dma_in_info, in, len, DMA_TO_DEVICE);
			if (ret == 0) {
				ret = pxa_dma_map(geu_dma_out_info, out, len, DMA_FROM_DEVICE);
				if (ret != 0) {
					pxa_dma_unmap(geu_dma_in_info, in, len, DMA_TO_DEVICE);
				}
			}
		}
		if (ret != 0) {
			printk("DMA map addr fail %p -> %p size 0x%x len 0x%x\n", in, out, length, len);
			break;
		}

		/* setup input DMA desc */
		for (i=0; i<geu_dma_in_info->num; i++) {
			geu_dma_in_desc[i].ddadr = DMA_PHY_ADDR(&geu_dma_in_desc[i+1]);
			geu_dma_in_desc[i].dsadr = geu_dma_in_info->addr[i];
			geu_dma_in_desc[i].dtadr = PXA_GEU_IOMEM + GEU_INPUT_DATA_ENC_DEC;
			geu_dma_in_desc[i].dcmd = DCMD_INCSRCADDR|DCMD_FLOWTRG|DCMD_WIDTH4|DCMD_BURST16|geu_dma_in_info->size[i];
		}
		geu_dma_in_desc[i].ddadr = DDADR_STOP; /* mark term */
		geu_dma_in_desc[i].dcmd = DCMD_FLOWTRG | DCMD_BURST16 | DCMD_ENDIRQEN;
		/* for in-place process */
		if (in == out) {
			info = geu_dma_in_info;
		} else {
			info = geu_dma_out_info;
		}
		/* setup output DMA desc */
		for (i=0; i<info->num; i++) {
			geu_dma_out_desc[i].ddadr = DMA_PHY_ADDR(&geu_dma_out_desc[i+1]);
			geu_dma_out_desc[i].dsadr = PXA_GEU_IOMEM + GEU_OUT_DATA_AFTER_ENC_DEC;
			geu_dma_out_desc[i].dtadr = info->addr[i];
			geu_dma_out_desc[i].dcmd = DCMD_INCTRGADDR|DCMD_FLOWSRC|DCMD_WIDTH4|DCMD_BURST16|info->size[i];
		}
		geu_dma_out_desc[i].ddadr = DDADR_STOP; /* mark term */
		geu_dma_out_desc[i].dcmd = DCMD_FLOWSRC | DCMD_BURST16 | DCMD_ENDIRQEN;

#ifdef GEU_DEBUG_INFO
		dump_dma_desc(geu_dma_in_desc, geu_dma_in_info->num);
		dump_dma_desc(geu_dma_out_desc, info->num);
#endif
		/* start input/output dma engine */
		DDADR(geu_dma_in) = DMA_PHY_ADDR(geu_dma_in_desc);
		DCSR(geu_dma_in) = DCSR_RUN;

		DDADR(geu_dma_out) = DMA_PHY_ADDR(geu_dma_out_desc);
		DCSR(geu_dma_out) = DCSR_RUN;
		GEUWriteReg(GEU_CONFIG, conf|GEU_CONFIG_EN_DMA_MODE_AES_CIPHER);

		/* wait for dma complete */
		if (0 == wait_for_completion_timeout(&cmd_complete, GEU_DMA_TIMEOUT)) {
			printk("GEU DMA timeout status(%x)\n", GEUReadReg(GEU_STATUS));
			ret = -ETIME;
		}

		if (in == out) {
			pxa_dma_unmap(geu_dma_in_info, in, len, DMA_BIDIRECTIONAL);
		} else {
			pxa_dma_unmap(geu_dma_in_info, in, len, DMA_TO_DEVICE);
			pxa_dma_unmap(geu_dma_out_info, out, len, DMA_FROM_DEVICE);
		}
		/* break on error. pxa_dma_unmap is need on error */
		if (ret != 0) {
			break;
		}
		in += len;
		out += len;
		length -= len;
	}

	GEUWriteReg(GEU_CONFIG, conf);
	return ret;
}
#else
static int GEU_AES_pio(unsigned char *in, unsigned char *out, unsigned int length)
{
	int ret = 0;
	unsigned int *inData = (unsigned int *)in;
	unsigned int *outData = (unsigned int *)out;
	unsigned int i;
	unsigned int conf;

	conf = GEUReadReg(GEU_CONFIG);
	GEUWriteReg(GEU_CONFIG, conf|GEU_CONFIG_DATAIMR);

	/* setup transfer info */
	if ((unsigned long)inData & 3 || (unsigned long)outData & 3) {
		unsigned int blkbuf[4];
		for (i=0; i<length/16; i++) {
			if (copy_from_user(blkbuf, inData, sizeof(blkbuf))) {
				ret = -EFAULT;
				break;
			}
			GEUWriteReg(GEU_INPUT_DATA_ENC_DEC, blkbuf[0]);
			GEUWriteReg(GEU_INPUT_DATA_ENC_DEC + 4, blkbuf[1]);
			GEUWriteReg(GEU_INPUT_DATA_ENC_DEC + 8, blkbuf[2]);
			GEUWriteReg(GEU_INPUT_DATA_ENC_DEC + 12, blkbuf[3]);
			GEUWriteReg(GEU_STATUS, GEU_STATUS_DINREADY);
			if (0 == wait_for_completion_timeout(&cmd_complete, GEU_DMA_TIMEOUT)) {
				printk("GEU timeout status(%x) config(%x)\n", GEUReadReg(GEU_STATUS), GEUReadReg(GEU_CONFIG));
				ret = -ETIME;
				break;
			}
			/* read output data */
			blkbuf[0] = GEUReadReg(GEU_OUT_DATA_AFTER_ENC_DEC);
			blkbuf[1] = GEUReadReg(GEU_OUT_DATA_AFTER_ENC_DEC + 4);
			blkbuf[2] = GEUReadReg(GEU_OUT_DATA_AFTER_ENC_DEC + 8);
			blkbuf[3] = GEUReadReg(GEU_OUT_DATA_AFTER_ENC_DEC + 12);
			if(copy_to_user(outData, blkbuf, sizeof(blkbuf))) {
				ret = -EFAULT;
				break;
			}
			inData += 4;
			outData += 4;
		}
	} else {
		for (i=0; i<length/16; i++) {
			GEUWriteReg(GEU_INPUT_DATA_ENC_DEC, *inData++);
			GEUWriteReg(GEU_INPUT_DATA_ENC_DEC + 4, *inData++);
			GEUWriteReg(GEU_INPUT_DATA_ENC_DEC + 8, *inData++);
			GEUWriteReg(GEU_INPUT_DATA_ENC_DEC + 12, *inData++);
			GEUWriteReg(GEU_STATUS, GEU_STATUS_DINREADY);
			/* wait for complete, timeout = 1s */
			if (0 == wait_for_completion_timeout(&cmd_complete, GEU_DMA_TIMEOUT)) {
				printk("GEU timeout status(%x) config(%x)\n", GEUReadReg(GEU_STATUS), GEUReadReg(GEU_CONFIG));
				ret = -ETIME;
				break;
			}
			/* read output data */
			*outData++ = GEUReadReg(GEU_OUT_DATA_AFTER_ENC_DEC);
			*outData++ = GEUReadReg(GEU_OUT_DATA_AFTER_ENC_DEC + 4);
			*outData++ = GEUReadReg(GEU_OUT_DATA_AFTER_ENC_DEC + 8);
			*outData++ = GEUReadReg(GEU_OUT_DATA_AFTER_ENC_DEC + 12);
		}
	}
	/* restore configure */
	GEUWriteReg(GEU_CONFIG, conf);
	return ret;
}
#endif

static int GEU_AES_process(unsigned char *in, unsigned char *out, unsigned int length)
{
	int ret;

	if (!mutex_is_locked(&geu_lock)) {
		printk("invalid sequence. GEU engine is not initialized");
		return -EINVAL;
	}

#ifdef GEU_MODE_DMA
	pm_qos_update_request(&geu_qos_idle, PM_QOS_CPUIDLE_BLOCK_AXI_VALUE);
	/* use direct mode in small length or unaligned input/output address */
	if (length < GEU_DMA_THRESHOLD || GEU_DMA_ALIGN(in) || GEU_DMA_ALIGN(out)) {
		ret = GEU_AES_direct(in, out, length);
	} else {
		ret = GEU_AES_chain(in, out, length);
	}
	pm_qos_update_request(&geu_qos_idle, PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
#else
	ret = GEU_AES_pio(in, out, length);
#endif
	return ret;
}

static int GEU_AES_finish(void)
{
	int i;

	if (!mutex_is_locked(&geu_lock)) {
		printk("invalid sequence. GEU engine is not initialized");
		return -EINVAL;
	}
#ifdef GEU_MODE_DMA
	DCSR(geu_dma_in) = 0;
	DCSR(geu_dma_out) = 0;
#endif
	/* clear iv for causion, otherwise next AES will fail  */
	for(i = 0; i < 4; i++) {
		GEUWriteReg(GEU_1ST_OFF_CODE_OCB_MODE + (i*4), 0);
	}
	GEUWriteReg(GEU_STATUS, 0);
	GEUWriteReg(GEU_CONFIG, 0);
	clk_disable(geu_clk);
	geu_owner = 0;
	mutex_unlock(&geu_lock);
	return 0;
}

static int GEU_AES_init_RKEK(int encrypt)
{
	unsigned int conf;

	if (mutex_trylock(&geu_lock) == 0) {
		printk("GEU invalid sequence. Engine has been initialized\n");
		return -EBUSY;
	}
	clk_enable(geu_clk);
	geu_owner = current->pid;

	conf = GEUReadReg(GEU_CONFIG);
	conf &= ~GEU_CONFIG_KEYSZ_MASK;
	conf |= 3;	/* RKEK key size 256 */
	/* set encrypt/decrypt */
	if (encrypt != 0) {
		conf &= ~GEU_CONFIG_ENCDEC;
	} else {
		conf |= GEU_CONFIG_ENCDEC;
	}
	conf |= GEU_CONFIG_KEYIMR;
	conf |= GEU_CONFIG_AES_KEY_SIZE_SEL;
	//conf |= GEU_CONFIG_RNDKEYHA|GEU_CONFIG_SBOXHA;
	GEUWriteReg(GEU_CONFIG, conf);
	GEUWriteReg(GEU_STATUS, GEU_STATUS_KEYREADY);

	/* wait for complete */
	if (0 == wait_for_completion_timeout(&cmd_complete, GEU_DMA_TIMEOUT)) {
		printk("GEU init RKEK timeout. status(%x), config(%x)\n", GEUReadReg(GEU_STATUS), GEUReadReg(GEU_CONFIG));
		clk_disable(geu_clk);
		geu_owner = 0;
		mutex_unlock(&geu_lock);
		return -ETIME;
	}
	//dump_round_key();
	return 0;
}

static int GEU_AES_init_CBC(unsigned int *key, int keylen, unsigned int *iv, int encrypt)
{
	int i;
	unsigned int conf;

	if (mutex_trylock(&geu_lock) == 0) {
		printk("GEU invalid sequence. Engine has been initialized\n");
		return -EBUSY;
	}
	clk_enable(geu_clk);
	geu_owner = current->pid;

	conf = GEUReadReg(GEU_CONFIG);
	conf &= ~GEU_CONFIG_KEYSZ_MASK;
	conf |= ((keylen>>3)-1)<<GEU_CONFIG_KEYSZ_SHIFT;	/* 128 to 1, 192 to 2, 256 to 3 */
	/* set encrypt/decrypt */
	if (encrypt != 0) {
		conf &= ~GEU_CONFIG_ENCDEC;
	} else {
		conf |= GEU_CONFIG_ENCDEC;
	}
	conf |= GEU_CONFIG_KEYIMR;
	GEUWriteReg(GEU_CONFIG, conf);
	/* write user key */
	for (i=0; i < 8; i++) {
		GEUWriteReg(GEU_INIT_KEY_VALUE + (i*4), key[i]);
	}
	/* set CBC IV */
	for(i = 0; i < 4; i++) {
		GEUWriteReg(GEU_1ST_OFF_CODE_OCB_MODE + (i*4), iv[i]);
	}
	/*Need to toggle bit24 according to SteveFeng  */
	GEUWriteReg(GEU_CONFIG, conf|GEU_CONFIG_WRITE_INI_VAL_IN_CBC_MODE);
	GEUWriteReg(GEU_CONFIG, conf);
	conf |= GEU_CONFIG_CBC_ECB_MODE | GEU_CONFIG_OCBBYP;
	GEUWriteReg(GEU_CONFIG, conf);

	GEUWriteReg(GEU_STATUS, GEU_STATUS_KEYREADY);

	/* wait for complete, timeout = 1s */
	if (0 == wait_for_completion_timeout(&cmd_complete, GEU_DMA_TIMEOUT)) {
		printk("GEU AES CBC init timeout. status(%x), config(%x)\n", GEUReadReg(GEU_STATUS), GEUReadReg(GEU_CONFIG));
		clk_disable(geu_clk);
		geu_owner = 0;
		mutex_unlock(&geu_lock);
		return -ETIME;
	}

	GEU_DEBUG(printk("##GEU AES CBC init status(%x) config(%x)\n", GEUReadReg(GEU_STATUS), GEUReadReg(GEU_CONFIG)));
	return 0;
}

static int GEU_gen_rand(unsigned char *rnd, unsigned int len)
{
	int copy;
	unsigned int old, new;
	unsigned int conf;

	if (mutex_trylock(&geu_lock) == 0) {
		printk("GEU engine busy. Finish AES before generate random number\n");
		return -EBUSY;
	}
	clk_enable(geu_clk);
	conf = GEUReadReg(GEU_CONFIG);
	/* set to use analog RNG */
	GEUWriteReg(GEU_CONFIG, conf|GEU_CONFIG_STICKY_CONTROL_BIT|(3<<GEU_CONFIG_FUSE_BLOCK_NUMBER_SHIFT));
	while(len > 0) {
		old = GEUReadReg(GEU_HW_RANDOM_NUM_GEN);
		/* add 0x80 to accerlate RNG */
		GEUWriteReg(GEU_FUSE_PROG_VAL1, (random32() & 0xFF) | 0x80);
		new = GEUReadReg(GEU_HW_RANDOM_NUM_GEN);
		while (old == new) {
			udelay(10);
			new = GEUReadReg(GEU_HW_RANDOM_NUM_GEN);
		}
		copy = len>sizeof(unsigned int)?sizeof(unsigned int):len;
		if (copy_to_user(rnd, &new, copy)) {
			break;
		}
		rnd += copy;
		len -= copy;
	}
	/* clear iv for causion, otherwise next AES will fail */
	for(copy = 0; copy < 4; copy++) {
		GEUWriteReg(GEU_1ST_OFF_CODE_OCB_MODE + (copy*4), 0);
	}
	GEUWriteReg(GEU_CONFIG, conf);
	clk_disable(geu_clk);
	mutex_unlock(&geu_lock);
	return 0;
}

static int GEU_read_key_hash(unsigned int *puiKeyHash, unsigned int len)
{
	unsigned int scratch;
	unsigned int i;

	if (mutex_trylock(&geu_lock) == 0) {
		printk("GEU engine busy.\n");
		return -EBUSY;
	}
	clk_enable(geu_clk);

	if ((len != GEU_SHA160_SIZE) &&
		(len != GEU_SHA224_SIZE) &&
		(len != GEU_SHA256_SIZE)) {
		clk_disable(geu_clk);
		mutex_unlock(&geu_lock);
		return -EFAULT;
	}

	for (i = 0;i < (len / 4); i++) {
		puiKeyHash[i] = GEUReadReg(GEU_FUSE_VAL_OEM_HASH_KEY + (i * 4));
	}

#if 0
	// Check if ECC enabled and any uncorrectable errors
	scratch = GEUReadReg(OEM_KEY_HASH_ECC);
	if (scratch != 0) {
		// ECC enabled - Check the ECC_STATUS value
		scratch = GEUReadReg(GEU_ECC_STATUS);
		if (scratch & K_OEM_UNCORRECTABLE_ECC_ERROR_MASK) {
			clk_disable(geu_clk);
			mutex_unlock(&geu_lock);
			return -EFAULT;
		}
	}
#endif

	clk_disable(geu_clk);
	mutex_unlock(&geu_lock);
	return 0;
}

static int pxa_geu_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int pxa_geu_close(struct inode *inode, struct file *file)
{
	if (geu_owner == current->pid && mutex_is_locked(&geu_lock)) {
		/* user close without unlock geu_lock */
		geu_owner = 0;
		mutex_unlock(&geu_lock);
	}
	return 0;
}

#ifndef KERN_USE_UNLOCKEDIOCTL
static int pxa_geu_ioctl(struct inode *inode, struct file *file, u_int cmd, u_long arg)
#else
static long pxa_geu_ioctl(struct file *file, u_int cmd, u_long arg)
#endif
{
	struct geu_arg geu_arg;
	int ret;

	if (copy_from_user(&geu_arg, (void __user *)arg, sizeof(geu_arg))) {
		return -EFAULT;
	}
	switch (cmd) {
		case GEU_AES_INIT:
		{
			unsigned int userkey[8];
			if (geu_arg.arg1 != 16 && geu_arg.arg1 != 24 && geu_arg.arg1 != 32) {
				printk("GEU invalid key length\n");
				return -EINVAL;
			}
			memset(userkey, 0, sizeof(userkey));
			if (copy_from_user(userkey, (void __user *)geu_arg.arg0, geu_arg.arg1)) {
				return -EFAULT;
			}
			ret = GEU_AES_init(userkey, geu_arg.arg1, geu_arg.arg2);
		}
		break;
		case GEU_AES_PROCESS:
		{
			if (geu_arg.arg2 & 15) {
				return -EINVAL;
			}
			if (!access_ok(VERIFY_READ, geu_arg.arg0, geu_arg.arg2)
				 || !access_ok(VERIFY_WRITE, geu_arg.arg1, geu_arg.arg2)) {
				return -EFAULT;
			}
			ret = GEU_AES_process((unsigned char *)geu_arg.arg0, 
									(unsigned char *)geu_arg.arg1, geu_arg.arg2);
		}
		break;
		case GEU_AES_FINISH:
		{
			ret = GEU_AES_finish();
		}
		break;
		case GEU_AES_INIT_RKEK:
		{
			ret = GEU_AES_init_RKEK(geu_arg.arg0);
		}
		break;
		case GEU_AES_INIT_CBC:
		{
			unsigned int userkey[8];
			unsigned int useriv[4];
			if (geu_arg.arg1 != 16 && geu_arg.arg1 != 24 && geu_arg.arg1 != 32) {
				printk("GEU invalid key length\n");
				return -EINVAL;
			}
			memset(userkey, 0, sizeof(userkey));
			memset(useriv, 0, sizeof(useriv));
			if (copy_from_user(userkey, (void __user *)geu_arg.arg0, geu_arg.arg1)) {
				return -EFAULT;
			}
			if (copy_from_user(useriv, (void __user *)geu_arg.arg2, 16)) {
				return -EFAULT;
			}
			ret = GEU_AES_init_CBC(userkey, geu_arg.arg1, useriv, geu_arg.arg3);
		}
		break;
		case GEU_GEN_RAND:
		{
			if (!access_ok(VERIFY_WRITE, geu_arg.arg0, geu_arg.arg1)) {
				return -EFAULT;
			}
			ret = GEU_gen_rand((void __user *)geu_arg.arg0, geu_arg.arg1);
		}
		break;
		case GEU_READ_OEMHASHKEY:
		{
			if (!access_ok(VERIFY_WRITE, geu_arg.arg0, geu_arg.arg1)) {
				return -EFAULT;
			}
			ret = GEU_read_key_hash((void __user *)geu_arg.arg0, geu_arg.arg1);
		}
		break;
		default:
			printk("GEU IOCTL invald command %x\n", cmd);
			ret = -EINVAL;
	}
	return ret;
}

static struct file_operations pxa_geu_fops = {
	.owner		= THIS_MODULE,
	.open		= pxa_geu_open,
	.release	= pxa_geu_close,
#ifndef KERN_USE_UNLOCKEDIOCTL
	.ioctl		= pxa_geu_ioctl,
#else
	.unlocked_ioctl = pxa_geu_ioctl,
#endif
};

static irqreturn_t pxa_geu_irq (int irq, void *devid)
{
	unsigned int status;
	status = GEUReadReg(GEU_STATUS);
	/* clear interrupt */
	GEUWriteReg(GEU_STATUS, status);
	complete(&cmd_complete);
	return IRQ_HANDLED;
}

#ifdef GEU_MODE_DMA
static void pxa_geu_dma_irq (int ch, void *data)
{
	volatile unsigned long dcsr;

	dcsr = DCSR(ch);
	/* clear interrupt */
	DCSR(ch) = dcsr;

	if (dcsr & DCSR_BUSERR) {
		DCSR(ch) |= DCSR_BUSERR;
		printk("%s(): DMA channel bus error\n", __func__);
	}

	if ((dcsr & DCSR_ENDINTR) || (dcsr & DCSR_STOPSTATE)) {
		if (dcsr & DCSR_STOPSTATE) {
			DCSR(ch) &= ~DCSR_STOPSTATE;
		}

		if (dcsr & DCSR_ENDINTR) {
			DCSR(ch) |= DCSR_ENDINTR;
		}
	}

	if (ch == geu_dma_out) {
		complete(&cmd_complete);
	}
	return;
}
#endif

static int __init pxa_geu_init(void)
{
	int ret;
	struct resource *r;

	geu_clk = clk_get(NULL, "AESCLK");
	if (IS_ERR(geu_clk)) {
		printk("GEU get clock fail\n");
		return -EBUSY;
	}
	dma_buf_base = dma_alloc_coherent(NULL, GEU_DMA_SIZE, 
									(dma_addr_t *)&dma_phy_base, GFP_KERNEL);
	if (dma_buf_base == NULL) {
		printk("GEU: failed to allocate DMA memory\n");
		goto out;
	}

#ifdef GEU_MODE_DMA
	geu_dma_in_desc = (pxa_dma_desc *)dma_buf_base;
	geu_dma_out_desc = (pxa_dma_desc *)(dma_buf_base + GEU_DMA_SIZE/2);
	geu_dma_in_info = kmalloc(2*sizeof(dma_info), GFP_KERNEL);
	if (geu_dma_in_info == NULL) {
		printk("GEU: failed to allocate DMA descript memory\n");
		goto fail_free_dma;
	}
	geu_dma_out_info = geu_dma_in_info + 1;
#endif
	r = request_mem_region(PXA_GEU_IOMEM, PXA_GEU_IOMEM_SIZE, "geu");
	if (r == NULL) {
		printk("GEU: failed to request memory resource\n");
		goto fail_free_info;
	}

	geu_iobase = ioremap(r->start, r->end - r->start + 1);
	if(geu_iobase == NULL) {
		printk("GEU: ioremap fail from 0x%x to 0x%x\n", r->start, r->end);
		goto fail_free_res;
	}
	ret = request_irq(PXA_GEU_IRQ, pxa_geu_irq, 0, "geu", NULL);
	if (ret < 0) {
		printk("GEU: failed to request IRQ\n");
		goto fail_free_iomem;
	}
#ifdef GEU_MODE_DMA
	geu_dma_in = pxa_request_dma("GEU_IN", DMA_PRIO_HIGH, pxa_geu_dma_irq, NULL);
	if (geu_dma_in < 0) {
		printk("GEU: failed to request RX DMA channel\n");
		goto fail_free_irq;
	}
	geu_dma_out = pxa_request_dma("GEU_OUT", DMA_PRIO_HIGH, pxa_geu_dma_irq, NULL);
	if (geu_dma_out < 0) {
		printk("GEU: failed to request TX DMA channel\n");
		goto fail_free_in_dma;
	}
	/* setup DMA channel mapping */
	DRCMR(GEU_OUT_DRCMR) = DRCMR_MAPVLD | geu_dma_out;
	DRCMR(GEU_IN_DRCMR) = DRCMR_MAPVLD | geu_dma_in;
	geu_qos_idle.name = "geu";
	pm_qos_add_request(&geu_qos_idle, PM_QOS_CPUIDLE_BLOCK,
						PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
#endif
	/* register the device */
	pxa_geu_miscdev.minor = MISC_DYNAMIC_MINOR;
	pxa_geu_miscdev.name = "geu";
	pxa_geu_miscdev.fops = &pxa_geu_fops;
	pxa_geu_miscdev.this_device = NULL;
	ret = misc_register(&pxa_geu_miscdev);
	if (ret < 0) {
		printk("GEU: unable to register device node /dev/geu\n");
		goto fail_free_out_dma;
	}
	clk_enable(geu_clk);
	/* clear iv for causion, otherwise next AES will fail  */
	for(ret = 0; ret < 4; ret++) {
		GEUWriteReg(GEU_1ST_OFF_CODE_OCB_MODE + (ret*4), 0);
	}
	GEUWriteReg(GEU_STATUS, 0);
	GEUWriteReg(GEU_CONFIG, 0);
	clk_disable(geu_clk);
	printk("GEU driver info: %s\n", GEU_DRIVER_VERSION);
	return 0;

fail_free_out_dma:
#ifdef GEU_MODE_DMA
	pm_qos_remove_request(&geu_qos_idle);
	pxa_free_dma(geu_dma_out);
fail_free_in_dma:
	pxa_free_dma(geu_dma_in);
fail_free_irq:
#endif
	free_irq(PXA_GEU_IRQ, NULL);
fail_free_iomem:
	iounmap(geu_iobase);
fail_free_res:
	release_mem_region(r->start, r->end - r->start + 1);
fail_free_info:
#ifdef GEU_MODE_DMA
	kfree(geu_dma_in_info);
fail_free_dma:
#endif
	dma_free_coherent(NULL, GEU_DMA_SIZE,
				dma_buf_base, (dma_addr_t)dma_phy_base);
out:
	clk_put(geu_clk);
	return -EBUSY;
}

static void __exit pxa_geu_exit(void)
{
	misc_deregister(&pxa_geu_miscdev);
#ifdef GEU_MODE_DMA
	pm_qos_remove_request(&geu_qos_idle);
	DRCMR(GEU_IN_DRCMR) = 0x00;
	DRCMR(GEU_OUT_DRCMR) = 0x00;
	pxa_free_dma(geu_dma_out);
	pxa_free_dma(geu_dma_in);
#endif
	free_irq(PXA_GEU_IRQ, NULL);
	iounmap(geu_iobase);
	release_mem_region(PXA_GEU_IOMEM, PXA_GEU_IOMEM_SIZE);
#ifdef GEU_MODE_DMA
	kfree(geu_dma_in_info);
#endif
	dma_free_coherent(NULL, GEU_DMA_SIZE,
				dma_buf_base, (dma_addr_t)dma_phy_base);
	clk_put(geu_clk);
}

module_init(pxa_geu_init);
module_exit(pxa_geu_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lindong Wu (ldwu@marvell.com)");
MODULE_DESCRIPTION("Generic Encrypt Uint driver");
