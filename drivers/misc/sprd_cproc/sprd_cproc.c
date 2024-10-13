/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <asm/uaccess.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <asm/pgtable.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif
#include <soc/sprd/hardware.h>
#include <linux/sprd_cproc.h>
#include <linux/sipc.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
//#include <soc/sprd/arch_misc.h>
#define CPROC_WDT_TRUE   1
#define CPROC_WDT_FLASE  0
/*used for ioremap to limit vmalloc size, shi yunlong*/
#define CPROC_VMALLOC_SIZE_LIMIT 4096
#define MAX_CPROC_ENTRY_NUM		0x11

enum {
	NORMAL_STATUS = 0,
	STOP_STATUS,
	WDTIRQ_STATUS,
	MAX_STATUS,
};

enum {
	BE_SEGMFG   = (0x1 << 4),
	BE_RDONLY   = (0x1 << 5),
	BE_WRONLY   = (0x1 << 6),
	BE_CPDUMP   = (0x1 << 7),
	BE_MNDUMP   = (0x1 << 8),
	BE_RDWDT    = (0x1 << 9),
	BE_RDWDTS   = (0x1 << 10),
	BE_RDLDIF   = (0x1 << 11),
	BE_LD	    = (0x1 << 12),
	BE_CTRL_ON  = (0x1 << 13),
	BE_CTRL_OFF	= (0x1 << 14),
	DEACTIVE_S	= (0x1 << 31),
};

#define GET_MD_TYPE(type)	((type) >> 16)
#define TYPE_TO_INDEX(type) (((type) >> ((GET_MD_TYPE(type) - 1) * 0X4)) & 0xf)
#define PM_TO_INDEX(type)	((type) & 0xf)
#define MD_TO_INDEX(type)	(((type) >> 0x4) & 0xf)
#define WB_TO_INDEX(type)	(((type) >> 0x8) & 0xf)
#define DP_TO_INDEX(type)	(((type) >> 0xc) & 0xf)

enum {
	pm_t = 0x1,	/* for description: power management related */
	md_t,	/* for description: modem related */
	wb_t,	/* for description: wifi&bt related */
	dp_t,	/* for description: dsp related */
	pt_num,
};

enum {
	pm_ctrl_t0 = 0x0001,
	pm_ctrl_t1 = 0x0002,
	pm_ctrl_t2 = 0x0003,
	pm_ctrl_t3 = 0x0004,
	ct_num,

	md_ctrl_t0 = 0x0010,
	md_ctrl_t1 = 0x0020,
	md_ctrl_t2 = 0x0030,
	md_ctrl_t3 = 0x0040,

	wb_ctrl_t0 = 0x0100,
	wb_ctrl_t1 = 0x0200,
	wb_ctrl_t2 = 0x0300,
	wb_ctrl_t3 = 0x0400,

	dp_ctrl_t0 = 0x1000,
	dp_ctrl_t1 = 0x2000,
	dp_ctrl_t2 = 0x3000,
	dp_ctrl_t3 = 0x4000,
};

static int cp_boot_mode;

const char *cp_status_info[] = {
	"started\n",
	"stopped\n",
	"wdtirq\n",
};

struct cproc_dev_ctrl {
	int (*start)(void *arg);
	int (*stop)(void *arg);
};

struct cproc_proc_entry {
	char					*name;
	struct proc_dir_entry	*entry;
	struct cproc_device		*cproc;
	unsigned				flag;	/* if set bit4 => bit0~bit3 for segs index */
};

struct cproc_proc_fs {
	struct proc_dir_entry		*procdir;

	struct cproc_proc_entry		start;
	struct cproc_proc_entry		stop;
	struct cproc_proc_entry		status;
	struct cproc_proc_entry		wdtirq;
	struct cproc_proc_entry		mem;
	struct cproc_proc_entry		mini_dump;
	struct cproc_proc_entry		*processor;
	struct cproc_proc_entry		entrys[MAX_CPROC_ENTRY_NUM];
	struct cproc_proc_entry		cp_crash;
};

struct cproc_device {
	struct miscdevice		miscdev;
	struct cproc_init_data	*initdata;
	void				*vbase;
	int 				wdtirq;
	int				wdtcnt;
	wait_queue_head_t		wdtwait;
	char *				name;
	int					status;
	uint32_t					type;
	struct cproc_proc_fs		procfs;
};

static struct cproc_device *pm_dev;

static int __init early_mode(char *str)
{
	if (!memcmp("shutdown", str, 8))
		cp_boot_mode = 1;

	return 0;
}

early_param("modem", early_mode);

static int sprd_cproc_open(struct inode *inode, struct file *filp)
{
	struct cproc_device *cproc = container_of(filp->private_data,
			struct cproc_device, miscdev);

	filp->private_data = cproc;

	pr_info("cproc %s opened!\n", cproc->initdata->devname);

	return 0;
}

static int sprd_cproc_release (struct inode *inode, struct file *filp)
{
	struct cproc_device *cproc = filp->private_data;

	pr_info("cproc %s closed!\n", cproc->initdata->devname);

	return 0;
}

static long sprd_cproc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	/*
	struct cproc_device *cproc = filp->private_data;
	*/
	/* TODO: for general modem download&control */

	return 0;
}

static int sprd_cproc_mmap(struct file *filp, struct vm_area_struct *vma)
{
	/*
	struct cproc_device *cproc = filp->private_data;
	*/
	/* TODO: for general modem download&control */

	return 0;
}

static const struct file_operations sprd_cproc_fops = {
	.owner = THIS_MODULE,
	.open = sprd_cproc_open,
	.release = sprd_cproc_release,
	.unlocked_ioctl = sprd_cproc_ioctl,
	.mmap = sprd_cproc_mmap,
};

struct cproc_dump_info
{
	char parent_name[20];
	char name[20];
	uint32_t start_addr;
	uint32_t size;
};

static int list_each_dump_info(struct cproc_dump_info *base,struct cproc_dump_info **info)
{
	struct cproc_dump_info *next;
	int ret = 1;

	if(info == NULL)
		return 0;

	next = *info;
	if(!next)
		next = base;
	else
		next ++;

	if (next->parent_name[0] != '\0') {
		*info = next;
	} else {
		*info = NULL;
		ret = 0;
	}

	return ret;
}

static ssize_t sprd_cproc_seg_dump(uint32_t base,uint32_t maxsz,
	char __user *buf, size_t count, loff_t offset)
{
	void *vmem;
	uint32_t loop = 0;
	uint32_t start_addr;
	uint32_t total;

	if (offset >= maxsz) {
		return 0;
	}

	if ((offset + count) > maxsz) {
		count = maxsz - offset;
	}
	start_addr = base + offset;
	total = count;

	do{
		uint32_t copy_size = CPROC_VMALLOC_SIZE_LIMIT;

		vmem = ioremap_nocache(start_addr + CPROC_VMALLOC_SIZE_LIMIT * loop, CPROC_VMALLOC_SIZE_LIMIT);
		if (!vmem) {
			printk(KERN_ERR "Unable to map cproc base: 0x%08x\n", start_addr + CPROC_VMALLOC_SIZE_LIMIT * loop);
			if (loop > 0) {
				return CPROC_VMALLOC_SIZE_LIMIT * loop;
			} else {
				return -ENOMEM;
			}
		}

		if (count < CPROC_VMALLOC_SIZE_LIMIT)
			copy_size = count;

		if (unalign_copy_to_user(buf, vmem, copy_size)) {
			printk(KERN_ERR "cproc_proc_read copy data to user error !\n");
			iounmap(vmem);
			return -EFAULT;
		}

		iounmap(vmem);

		count -= copy_size;
		loop ++;
		buf += copy_size;
	}while(count);

	return total;
}

static int cproc_proc_open(struct inode *inode, struct file *filp)
{
	struct cproc_proc_entry *entry = (struct cproc_proc_entry *)PDE_DATA(inode);
	/*move remap to writing or reading, shi yunlong*/
/*
	struct cproc_device *cproc = entry->cproc;

	cproc->vbase = ioremap(cproc->initdata->base, cproc->initdata->maxsz);
	if (!cproc->vbase) {
		printk(KERN_ERR "Unable to map cproc base: 0x%08x\n", cproc->initdata->base);
		return -ENOMEM;
	}
*/
	pr_info("cproc proc open type: %s\n", entry->name);

	filp->private_data = entry;

	return 0;
}

static int cproc_proc_release(struct inode *inode, struct file *filp)
{
	/*move remap to writing or reading, shi yunlong*/
/*
	struct cproc_proc_entry *entry = (struct cproc_proc_entry *)filp->private_data;
	struct cproc_device *cproc = entry->cproc;

	iounmap(cproc->vbase);
*/

	return 0;
}

static ssize_t cproc_proc_read(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos)
{
	struct cproc_proc_entry *entry = (struct cproc_proc_entry *)filp->private_data;
	struct cproc_device *cproc = entry->cproc;
	char *type = entry->name;
	unsigned int len;
	unsigned flag;
	void *vmem;
	int rval;
	size_t r, i;/*count ioremap, shi yunlong*/

	flag = entry->flag;
	pr_debug("cproc proc read type: %s flag 0x%x\n", type, flag);

	if ((flag & BE_RDONLY) == 0) {
		return -EPERM;
	}

	if ((flag & BE_CPDUMP) != 0) {
		if (*ppos >= cproc->initdata->maxsz) {
			return 0;
		}
		if ((*ppos + count) > cproc->initdata->maxsz) {
			count = cproc->initdata->maxsz - *ppos;
		}

		/*remap and unmap in each read operation, shi yunlong, begin*/
		/*
		vmem = cproc->vbase + *ppos;
		if (unalign_copy_to_user(buf, vmem, count)) {
			printk(KERN_ERR "cproc_proc_read copy data to user error !\n");
			return -EFAULT;
		}
		*/
		r = count, i = 0;
		do{
			uint32_t copy_size = CPROC_VMALLOC_SIZE_LIMIT;
			vmem = ioremap_nocache(cproc->initdata->base + *ppos + CPROC_VMALLOC_SIZE_LIMIT*i, CPROC_VMALLOC_SIZE_LIMIT);
			if (!vmem) {
				size_t addr = cproc->initdata->base + *ppos + CPROC_VMALLOC_SIZE_LIMIT*i;
				printk(KERN_ERR "Unable to map cproc base: 0x%lx\n", addr);
				if(i > 0){
					*ppos += CPROC_VMALLOC_SIZE_LIMIT*i;
					return CPROC_VMALLOC_SIZE_LIMIT*i;
				}else{
					return -ENOMEM;
				}
			}
			if(r < CPROC_VMALLOC_SIZE_LIMIT) copy_size = r;
			if (unalign_copy_to_user(buf, vmem, copy_size)) {
				printk(KERN_ERR "cproc_proc_read copy data to user error !\n");
				iounmap(vmem);
				return -EFAULT;
			}
			iounmap(vmem);
			r -= copy_size;
			buf += copy_size;
			i++;
		}while(r > 0);
		/*remap and unmap in each read operation, shi yunlong, end*/

	} else if ((flag & BE_RDWDTS) != 0) {
		if (cproc->status >= MAX_STATUS) {
			return -EINVAL;
		}
		len = strlen(cp_status_info[cproc->status]);
		if (*ppos >= len) {
			return 0;
		}
		count = (len > count) ? count : len;
		if (unalign_copy_to_user(buf, cp_status_info[cproc->status], count)) {
			printk(KERN_ERR "cproc_proc_read copy data to user error !\n");
			return -EFAULT;
		}

	} else if ((flag & BE_RDWDT) != 0) {
		/* wait forever */
		rval = wait_event_interruptible(cproc->wdtwait, cproc->wdtcnt != CPROC_WDT_FLASE);
		if (rval < 0) {
			printk(KERN_ERR "cproc_proc_read wait interrupted error !\n");
		}
		len = strlen(cp_status_info[WDTIRQ_STATUS]);
		if (*ppos >= len) {
			return 0;
		}
		count = (len > count) ? count : len;
		if (unalign_copy_to_user(buf, cp_status_info[WDTIRQ_STATUS], count)) {
			printk(KERN_ERR "cproc_proc_read copy data to user error !\n");
			return -EFAULT;
		}

	} else if ((flag & BE_RDLDIF) != 0) {
		struct cproc_init_data *data = cproc->initdata;
		struct cproc_segments *segm = NULL;
		struct load_node lnode;
		char *p = buf;

		for (i = 0, len = 0, segm = &(data->segs[i]); i < data->segnr; i++, segm++) {
			if (segm->maxsz == 0) {
				break;
			}

			memset(&lnode, 0, sizeof(lnode));
			pr_info("sprd_cproc: segm[%d] name=%s base=0x%x, size=0x%0x\n", i, segm->name, segm->base, segm->maxsz);
			memcpy(lnode.name, segm->name, sizeof(lnode.name));
			lnode.size = segm->maxsz;
			len += sizeof(lnode);

			if (count < len) {
				len -= sizeof(lnode);
				break;
			}

			if (unalign_copy_to_user(p, &lnode, sizeof(lnode))) {
				printk(KERN_ERR "cproc_proc_read copy ldinfo to user error !\n");
				return -EFAULT;
			}

			p += sizeof(lnode);
		}

		count = len;

	} else if ((flag & BE_MNDUMP) != 0) {
		static struct cproc_dump_info *s_cur_info = NULL;
		uint8_t head[sizeof(struct cproc_dump_info) + 32];
		int len, total = 0, offset = 0;
		ssize_t written = 0;

		if (!s_cur_info && *ppos)
			return 0;

		if (!s_cur_info)
			list_each_dump_info(cproc->initdata->shmem, &s_cur_info);

		while (s_cur_info) {
			if (!count)
				break;
			len = sprintf(head, "%s_%s_0x%8x_0x%x.bin", s_cur_info->parent_name, s_cur_info->name,
				s_cur_info->start_addr,s_cur_info->size);

			if (*ppos > len) {
				offset = *ppos - len;
			} else {
				if (*ppos + count > len)
					written = len - *ppos;
				else
					written = count;

				if (unalign_copy_to_user(buf + total, head + *ppos, written)) {
					printk(KERN_ERR "cproc_proc_read copy data to user error !\n");
					return -EFAULT;
				}
				*ppos += written;
			}
			total += written;
			count -= written;
			if (count) {
				written = sprd_cproc_seg_dump(s_cur_info->start_addr, s_cur_info->size, buf + total, count, offset);
				if (written > 0) {
					total += written;
					count -= written;
					*ppos += written;
				} else if (written == 0) {
					if (list_each_dump_info(cproc->initdata->shmem, &s_cur_info))
						*ppos = 0;
				} else {
					return written;
				}

			} else {
				break;
			}

			written = 0;
			offset = 0;
		}

		return total;
	} else {
		return -EINVAL;
	}

	*ppos += count;
	return count;
}

static ssize_t cproc_proc_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct cproc_proc_entry *entry = (struct cproc_proc_entry *)filp->private_data;
	struct cproc_device *cproc = entry->cproc;
	char *type = entry->name;
	uint32_t base = 0;
	uint32_t size = 0;
	uint32_t offset = 0;
	void *vmem = NULL;
	size_t r, i;/*count ioremap, shi yunlong*/
	unsigned flag;

	flag = entry->flag;

	if ((flag & BE_WRONLY) == 0) {
		return -EPERM;
	}

	if (strcmp(type, "cp_crash") == 0) {
		char cp_crash_message[1024] = {0,};
		if (unalign_copy_from_user(cp_crash_message, buf,
			(count >= sizeof(cp_crash_message))? sizeof(cp_crash_message)-1 : count)) {
			printk(KERN_ERR "cproc_proc_write copy data from user error !\n");
		}
		else
			panic("CP Crash : %s", cp_crash_message);
	}
	
	if ((flag & BE_CTRL_ON) != 0) {
		printk(KERN_INFO "cproc_proc_write to map cproc base start\n");
		cproc->initdata->start(cproc);
		cproc->wdtcnt = CPROC_WDT_FLASE;
		cproc->status = NORMAL_STATUS;
		return count;
	}

	if ((flag & BE_CTRL_OFF) != 0) {
		printk(KERN_INFO "cproc_proc_write to map cproc base stop\n");
		cproc->initdata->stop(cproc);
		cproc->status = STOP_STATUS;
		return count;
	}

	if ((flag & BE_LD) != 0) {
		i = ~((~0) << 4) & flag;
		base = cproc->initdata->segs[i].base;
		size = cproc->initdata->segs[i].maxsz;
		offset = *ppos;
	}

	if (size <= offset) {
		pr_debug("cproc_proc_write modem dsp write over pos:%0x\n",offset);
		*ppos += count;
		return count;
	}

	

	
	//pr_info("cproc proc write: 0x%08x, 0x%08x\n!", base + offset, count);
	count = min((size-offset), count);
	r = count, i = 0;
	do{
		uint32_t copy_size = CPROC_VMALLOC_SIZE_LIMIT;
		if(base == 0) {
			printk(KERN_ERR "cproc_proc_write bad segs address\n");
			return -EFAULT;
		}
		vmem = ioremap_nocache(base + offset + CPROC_VMALLOC_SIZE_LIMIT*i, CPROC_VMALLOC_SIZE_LIMIT);
		if (!vmem) {
			size_t addr = base + offset + CPROC_VMALLOC_SIZE_LIMIT*i;
			printk(KERN_ERR "Unable to map cproc base: 0x%lx\n", addr);
			if(i > 0){
				*ppos += CPROC_VMALLOC_SIZE_LIMIT*i;
				return CPROC_VMALLOC_SIZE_LIMIT*i;
			}else{
				return -ENOMEM;
			}
		}
		if(r < CPROC_VMALLOC_SIZE_LIMIT) copy_size = r;
		if (unalign_copy_from_user(vmem, buf+CPROC_VMALLOC_SIZE_LIMIT*i, copy_size)) {
			printk(KERN_ERR "cproc_proc_write copy data from user error !\n");
			iounmap(vmem);
			return -EFAULT;
		}
		iounmap(vmem);
		r -= copy_size;
		i++;
	}while(r > 0);
	/*remap and unmap in each write operation, shi yunlong, end*/

	*ppos += count;
	return count;
}

static loff_t cproc_proc_lseek(struct file* filp, loff_t off, int whence )
{
	struct cproc_proc_entry *entry = (struct cproc_proc_entry *)filp->private_data;
	struct cproc_device *cproc = entry->cproc;
	char *type = entry->name;
	loff_t new;

	switch (whence) {
	case SEEK_SET:
		new = off;
		filp->f_pos = new;
		break;
	case SEEK_CUR:
		new = filp->f_pos + off;
		filp->f_pos = new;
		break;
	case SEEK_END:
		if (strcmp(type, "mem") == 0) {
			new = cproc->initdata->maxsz + off;
			filp->f_pos = new;
		} else {
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return (new);
}

static unsigned int cproc_proc_poll(struct file *filp, poll_table *wait)
{
	struct cproc_proc_entry *entry = (struct cproc_proc_entry *)filp->private_data;
	struct cproc_device *cproc = entry->cproc;
	char *type = entry->name;
	unsigned int mask = 0;

	pr_info("cproc proc poll type: %s \n", type);

	if (strcmp(type, "wdtirq") == 0) {
		poll_wait(filp, &cproc->wdtwait, wait);
		if (cproc->wdtcnt  != CPROC_WDT_FLASE) {
			mask |= POLLIN | POLLRDNORM;
		}
	} else {
		printk(KERN_ERR "cproc_proc_poll file don't support poll !\n");
		return -EINVAL;
	}
	return mask;
}

struct file_operations cpproc_fs_fops = {
	.open		= cproc_proc_open,
	.release	= cproc_proc_release,
	.llseek  = cproc_proc_lseek,
	.read		= cproc_proc_read,
	.write	= cproc_proc_write,
	.poll		= cproc_proc_poll,
};

static inline void sprd_cproc_fs_init(struct cproc_device *cproc)
{
    uint8_t i, m, ucnt;
	unsigned flag;
	uint32_t type;
	umode_t mode = 0;

	type = cproc->type;

	cproc->procfs.procdir = proc_mkdir(cproc->name, NULL);

	memset(cproc->procfs.entrys, 0, sizeof(cproc->procfs.entrys));

	for (flag = 0, ucnt = 0, i = 0; i < MAX_CPROC_ENTRY_NUM; i++, flag = 0, mode = 0) {
		switch (i) {
		case 0:
			cproc->procfs.entrys[i].name = "start";
			flag |= (BE_WRONLY | BE_CTRL_ON);
			if ((GET_MD_TYPE(type) == pm_t)&&(PM_TO_INDEX(type) < pm_ctrl_t2))
				flag |= DEACTIVE_S;
			ucnt++;
			break;

		case 1:
			cproc->procfs.entrys[i].name = "stop";
			flag |= (BE_WRONLY | BE_CTRL_OFF);
			if ((GET_MD_TYPE(type) == pm_t)&&(PM_TO_INDEX(type) < pm_ctrl_t2))
				flag |= DEACTIVE_S;
			ucnt++;
			break;

		case 2:
			cproc->procfs.entrys[i].name = "wdtirq";
			flag |= (BE_RDONLY | BE_RDWDT);
			if (GET_MD_TYPE(type) == pm_t)
				flag |= DEACTIVE_S;
			ucnt++;
			break;

		case 3:
			cproc->procfs.entrys[i].name = "status";
			flag |= (BE_RDONLY | BE_RDWDTS);
			if (GET_MD_TYPE(type) == pm_t)
				flag |= DEACTIVE_S;
			ucnt++;
			break;

		case 4:
			cproc->procfs.entrys[i].name = "mini_dump";
			flag |= (BE_RDONLY | BE_MNDUMP);
			if ((cproc->initdata->shmem == NULL) || (GET_MD_TYPE(type) == pm_t))
				flag |= DEACTIVE_S;
			ucnt++;
			break;

		case 5:
			cproc->procfs.entrys[i].name = "ldinfo";
			flag |= (BE_RDONLY | BE_RDLDIF);
			ucnt++;
			break;

		case 6:
			cproc->procfs.entrys[i].name = "mem";
			flag |= (BE_RDONLY | BE_CPDUMP);
			ucnt++;
			break;

		case 7:
			cproc->procfs.entrys[i].name = "cp_crash";
			flag |= (BE_WRONLY);
			ucnt++;
			break;
			
		default:
			if (cproc->initdata->segnr + ucnt >= MAX_CPROC_ENTRY_NUM) {
				printk(KERN_ERR, "sprd_cproc: entrys num to small\n");
				return;
			}

			if (i - ucnt >= cproc->initdata->segnr) {
				return;
			}

			cproc->procfs.entrys[i].name = cproc->initdata->segs[i-ucnt].name;
			flag |= (BE_WRONLY | BE_LD | BE_SEGMFG | (i-ucnt));
			break;
		}

		cproc->procfs.entrys[i].flag = flag;

		if (flag & DEACTIVE_S) {
			continue;
		}

		if (flag & BE_RDONLY) {
			mode |= S_IRUSR | S_IRGRP;
			if (flag & (BE_CPDUMP | BE_MNDUMP))
				mode |= S_IROTH;
		}

		if (flag & BE_WRONLY) {
			mode |= S_IWUSR | S_IWGRP;
		}

		pr_info("sprd_cproc entry name: %s type 0x%x addr: 0x%p\n", cproc->procfs.entrys[i].name,
			cproc->procfs.entrys[i].flag, &(cproc->procfs.entrys[i]));
		cproc->procfs.entrys[i].entry = proc_create_data(cproc->procfs.entrys[i].name, mode,
			cproc->procfs.procdir, &cpproc_fs_fops, &(cproc->procfs.entrys[i]));
		cproc->procfs.entrys[i].cproc = cproc;
	}

	return;
}

static inline void sprd_cproc_fs_exit(struct cproc_device *cproc)
{
    uint8_t i = 0;

	for (i = 0; i < MAX_CPROC_ENTRY_NUM; i++) {
		if (cproc->procfs.entrys[i].name == NULL) {
			break;
		}

		if ((cproc->procfs.entrys[i].flag & DEACTIVE_S) == 0) {
			remove_proc_entry(cproc->procfs.entrys[i].name, cproc->procfs.procdir);
		}
	}

	remove_proc_entry(cproc->name, NULL);
}

static irqreturn_t sprd_cproc_irq_handler(int irq, void *dev_id)
{
	struct cproc_device *cproc = (struct cproc_device *)dev_id;

	printk("sprd_cproc_irq_handler cp watchdog enable !\n");
	cproc->wdtcnt = CPROC_WDT_TRUE;
	cproc->status = WDTIRQ_STATUS;
	wake_up_interruptible_all(&(cproc->wdtwait));
	return IRQ_HANDLED;
}

#ifdef CONFIG_OF
static int sprd_cproc_native_arm7_start(void *arg)
{
	struct cproc_device *cproc = (struct cproc_device *)arg;
	struct cproc_ctrl *pm_ctrl;

	pr_info("sprd_cproc: start arm7\n");
	pm_ctrl = cproc->initdata->ctrl;

	/* clear cp1 force deep sleep */
	if ((pm_ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP] & 0xff) != 0xff) {
		sci_glb_clr(pm_ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP], pm_ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP]);
	}

	/* clear cp1 force shutdown */
	if ((pm_ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN] & 0xff) != 0xff) {
		sci_glb_clr(pm_ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN], pm_ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN]);
	}

	cproc->status = NORMAL_STATUS;

	return 0;
}

static int sprd_cproc_native_arm7_stop(void *arg)
{
	return 0;
}

static int sprd_cproc_native_cmx_start(void *arg)
{
	struct cproc_device *cproc = (struct cproc_device *)arg;
	struct cproc_ctrl *pm_ctrl;

	pr_info("sprd_cproc: start cmx\n");
	pm_ctrl = pm_dev->initdata->ctrl;

	if ((pm_ctrl->ctrl_reg[CPROC_CTRL_RESET] & 0xff) != 0xff) {
		sci_glb_set(pm_ctrl->ctrl_reg[CPROC_CTRL_RESET], pm_ctrl->ctrl_mask[CPROC_CTRL_RESET]);
	}

	/* clear cp1 force deep sleep */
	if ((pm_ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP] & 0xff) != 0xff) {
		sci_glb_clr(pm_ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP], pm_ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP]);
	}

	/* clear cp1 force shutdown */
	if ((pm_ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN] & 0xff) != 0xff) {
		sci_glb_clr(pm_ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN], pm_ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN]);
	}

	if ((pm_ctrl->ctrl_reg[CPROC_CTRL_RESET] & 0xff) != 0xff) {
		sci_glb_clr(pm_ctrl->ctrl_reg[CPROC_CTRL_RESET], pm_ctrl->ctrl_mask[CPROC_CTRL_RESET]);
	}

	cproc->status = NORMAL_STATUS;

	return 0;
}

static int sprd_cproc_native_cmx_stop(void *arg)
{
	return 0;
}

static int sprd_cproc_native_cp_start(void *arg)
{
	struct cproc_device *cproc = (struct cproc_device *)arg;
	struct cproc_init_data *pdata = cproc->initdata;
	struct cproc_ctrl *ctrl;
	uint32_t value, state;
	void *vmem = NULL;

    if (!pdata) {
		return -ENODEV;
	}

	ctrl = pdata->ctrl;

	pr_info("sprd_cproc: type = 0x%x, status = 0x%x\n", cproc->type, cproc->status);

	/* iram power on */
	if (ctrl->ctrl_reg[CPROC_CTRL_IRAM_PW] != INVALID_REG) {
		sci_glb_clr(ctrl->ctrl_reg[CPROC_CTRL_IRAM_PW], ctrl->ctrl_mask[CPROC_CTRL_IRAM_PW]);
	}

	vmem = ioremap_nocache(ctrl->iram_addr, ctrl->iram_size);
	if(!vmem)
	   return -ENOMEM;

	pr_info("sprd_cproc: vmem = 0x%x, iram_addr = 0x%p, iram_size = 0x%x\n", vmem, ctrl->iram_addr, ctrl->iram_size);

	unalign_memcpy(vmem, (void *)ctrl->iram_data, ctrl->iram_size);

	/* clear cp1 force shutdown */
    if (ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN] != INVALID_REG) {
		sci_glb_clr(ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN], ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN]);
    }
#if !defined(CONFIG_ARCH_SCX30G) && !defined(CONFIG_ARCH_SCX35)
	while (1) {
		state = __raw_readl((void *)ctrl->ctrl_reg[CPROC_CTRL_GET_STATUS]);
		if (!(state & ctrl->ctrl_mask[CPROC_CTRL_GET_STATUS]))  /* (0xf <<16) */
			break;
	}
#endif
    pr_info("deep sllep reg =0x%x, reset reg =0x%x\n", ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP], ctrl->ctrl_reg[CPROC_CTRL_RESET]);
    pr_info("deep sllep mask =0x%x, reset mask =0x%x\n", ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP], ctrl->ctrl_mask[CPROC_CTRL_RESET]);

    if (ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP] != INVALID_REG) {
		/* clear cp1 force deep sleep */
		sci_glb_clr(ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP], ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP]);
    }

    if (ctrl->ctrl_reg[CPROC_CTRL_RESET] != INVALID_REG) {
		/* clear reset cp1 */
		msleep(50);
		sci_glb_clr(ctrl->ctrl_reg[CPROC_CTRL_RESET], ctrl->ctrl_mask[CPROC_CTRL_RESET]);
		while (1) {
			state = sci_glb_read(ctrl->ctrl_reg[CPROC_CTRL_RESET], -1UL);
			if (!(state & ctrl->ctrl_mask[CPROC_CTRL_RESET]))
				break;
		}
	}

	iounmap(vmem);
	pr_info("sprd_cproc:start over\n");
	return 0;
}

static int sprd_cproc_native_cp_stop(void *arg)
{
	struct cproc_device *cproc = (struct cproc_device *)arg;
	struct cproc_init_data *pdata = cproc->initdata;
	struct cproc_ctrl *ctrl;

    if (!pdata) {
		return -ENODEV;
	}
	ctrl = pdata->ctrl;

	pr_info("sprd_cproc: stop %s\n", cproc->name);

	/* reset cp1 */
	if ((ctrl->ctrl_reg[CPROC_CTRL_RESET] & INVALID_REG) != INVALID_REG) {
		sci_glb_set(ctrl->ctrl_reg[CPROC_CTRL_RESET], ctrl->ctrl_mask[CPROC_CTRL_RESET]);
		msleep(50);
	}
	if ((ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP] & INVALID_REG) != INVALID_REG) {
		/* cp1 force deep sleep */
		sci_glb_set(ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP], ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP]);
		msleep(50);
	}
	if ((ctrl->ctrl_reg[CPROC_CTRL_GET_STATUS] & INVALID_REG) != INVALID_REG) {
		sci_glb_clr(ctrl->ctrl_reg[CPROC_CTRL_GET_STATUS], ctrl->ctrl_mask[CPROC_CTRL_GET_STATUS]);
	}
	if ((ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN] & INVALID_REG) != INVALID_REG) {
		/* cp1 force shutdown */
		sci_glb_set(ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN], ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN]);
	}

	return 0;
}

static int sprd_cproc_native_cp2_start(void* arg)
{
	struct cproc_device *cproc = (struct cproc_device *)arg;
	struct cproc_init_data *pdata = cproc->initdata;
	struct cproc_ctrl *ctrl;
	uint32_t state;
	void *cp2_code_addr = NULL;

	if (!pdata) {
		return -ENODEV;
	}
	pr_info("%s\n",__func__);

	ctrl = pdata->ctrl;
	cp2_code_addr = ioremap_nocache(ctrl->iram_addr,0x1000);
	if (!cp2_code_addr)
		return -ENOMEM;

	unalign_memcpy(cp2_code_addr, (void *)ctrl->iram_data, ctrl->iram_size);

#ifdef CONFIG_ARCH_SCX30G
	/* clear cp2 force shutdown */
	sci_glb_clr(ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN], ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN]);
	msleep(5);

	/* set reset cp2 */
	sci_glb_set(ctrl->ctrl_reg[CPROC_CTRL_RESET], ctrl->ctrl_mask[CPROC_CTRL_RESET]);
	msleep(5);

	/* clear cp2 force deep sleep */
	sci_glb_clr(ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP], ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP]);
	msleep(5);

	/* clear reset cp2 */
	sci_glb_clr(ctrl->ctrl_reg[CPROC_CTRL_RESET], ctrl->ctrl_mask[CPROC_CTRL_RESET]);

	while (1) {
		state = sci_glb_read(ctrl->ctrl_reg[CPROC_CTRL_GET_STATUS], -1UL);
		if (!(state & ctrl->ctrl_mask[CPROC_CTRL_GET_STATUS]))	/* (0xf <<16) */
			break;
	}
#else
	/* clear cp2 force shutdown */
	sci_glb_clr(ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN], ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN]);
	while (1) {
		state = sci_glb_read(ctrl->ctrl_reg[CPROC_CTRL_GET_STATUS], -1UL);
		if (!(state & ctrl->ctrl_mask[CPROC_CTRL_GET_STATUS]))	/* 0xf <<16) */
			break;
	}

	/* set reset cp2 */
	sci_glb_set(ctrl->ctrl_reg[CPROC_CTRL_RESET], ctrl->ctrl_mask[CPROC_CTRL_RESET]);

	/* clear cp2 force deep sleep */
	sci_glb_clr(ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP], ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP]);

	/* clear reset cp2 */
	sci_glb_clr(ctrl->ctrl_reg[CPROC_CTRL_RESET], ctrl->ctrl_mask[CPROC_CTRL_RESET]);

#endif
	iounmap(cp2_code_addr);

	return 0;
}

#define WCN_SLEEP_STATUS	(SPRD_PMU_BASE + 0xD4)

static int sprd_cproc_native_cp2_stop(void *arg)
{
	struct cproc_device *cproc = (struct cproc_device *)arg;
	struct cproc_init_data *pdata = cproc->initdata;
	struct cproc_ctrl *ctrl;
	uint32_t state = 0;

	if (!pdata) {
		return -ENODEV;
	}

	pr_info("%s\n",__func__);

	ctrl = pdata->ctrl;

	 while (1) {
		state = sci_glb_read(WCN_SLEEP_STATUS,-1UL);
		if (!(state & (0xf<<12)))
			break;
		msleep(1);
	 }
	pr_info("%s cp2 enter sleep\n",__func__);

	/* reset cp2 */
	sci_glb_set(ctrl->ctrl_reg[CPROC_CTRL_RESET], ctrl->ctrl_mask[CPROC_CTRL_RESET]);

	/* cp2 force deep sleep */
	sci_glb_set(ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP], ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP]);

	/* cp2 force shutdown */
	sci_glb_set(ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN], ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN]);
	return 0;
}

#define WCN_SLEEP_STATUS	(SPRD_PMU_BASE + 0xD4)

static int sprd_cproc_native_dsp_start(void *arg)
{
	struct cproc_device *cproc = (struct cproc_device *)arg;
	struct cproc_init_data *pdata = cproc->initdata;
	struct cproc_ctrl *ctrl;

	if (!pdata) {
		return -ENODEV;
	}

	pr_info("sprd_cproc: %s\n", __func__);

	ctrl = pdata->ctrl;

	/* clear agdsp sys shutdown */
	if ((ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN] & INVALID_REG) != INVALID_REG) {
		pr_info("sprd_cproc: agdsp sys shutdown -- reg = %x, mask = %x\n",
			ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN], ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN]);
		sci_glb_clr(ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN], ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN]);
	}

	/* clear agdsp core shutdown */
	if ((ctrl->ctrl_reg[CPROC_CTRL_EXT0] & INVALID_REG) != INVALID_REG) {
		pr_info("sprd_cproc: agdsp core shutdown -- reg = %x, mask = %x\n",
			ctrl->ctrl_reg[CPROC_CTRL_EXT0], ctrl->ctrl_mask[CPROC_CTRL_EXT0]);
		sci_glb_clr(ctrl->ctrl_reg[CPROC_CTRL_EXT0], ctrl->ctrl_mask[CPROC_CTRL_EXT0]);
	}

	if ((ctrl->ctrl_reg[CPROC_CTRL_RESET] & INVALID_REG) != INVALID_REG) {
		pr_info("sprd_cproc: sys reset -- reg = %x, mask = %x\n",
			ctrl->ctrl_reg[CPROC_CTRL_RESET], ctrl->ctrl_mask[CPROC_CTRL_RESET]);
		sci_glb_clr(ctrl->ctrl_reg[CPROC_CTRL_RESET], ctrl->ctrl_mask[CPROC_CTRL_RESET]);
	}

	if ((ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP] & INVALID_REG) != INVALID_REG) {
		pr_info("sprd_cproc: deep sleep -- reg = %x, mask = %x\n",
			ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP], ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP]);
		sci_glb_clr(ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP], ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP]);
	}

	/* config reg protection */
	if ((ctrl->ctrl_reg[CPROC_CTRL_EXT2] & INVALID_REG) != INVALID_REG) {
		pr_info("sprd_cproc: config protection -- reg = %x, mask = %x\n",
			ctrl->ctrl_reg[CPROC_CTRL_EXT2], ctrl->ctrl_mask[CPROC_CTRL_EXT2]);
		sci_glb_clr(ctrl->ctrl_reg[CPROC_CTRL_EXT2], 0xffff);
		sci_glb_set(ctrl->ctrl_reg[CPROC_CTRL_EXT2], ctrl->ctrl_mask[CPROC_CTRL_EXT2]);
	}

	/* config boot addr */
	if ((ctrl->ctrl_reg[CPROC_CTRL_EXT3] & INVALID_REG) != INVALID_REG) {
		pr_info("sprd_cproc: config boot addr -- reg = %x, mask = %x\n",
			ctrl->ctrl_reg[CPROC_CTRL_EXT3], ctrl->ctrl_mask[CPROC_CTRL_EXT3]);
		sci_glb_clr(ctrl->ctrl_reg[CPROC_CTRL_EXT3], 0xffffffff);
		sci_glb_set(ctrl->ctrl_reg[CPROC_CTRL_EXT3], ctrl->ctrl_mask[CPROC_CTRL_EXT3]);
	}

	/* config agdsp ctrl1 */
	if ((ctrl->ctrl_reg[CPROC_CTRL_EXT4] & INVALID_REG) != INVALID_REG) {
		pr_info("sprd_cproc: config agdsp ctrl1 -- reg = %x, mask = %x\n",
			ctrl->ctrl_reg[CPROC_CTRL_EXT4], ctrl->ctrl_mask[CPROC_CTRL_EXT4]);
		sci_glb_set(ctrl->ctrl_reg[CPROC_CTRL_EXT4], ctrl->ctrl_mask[CPROC_CTRL_EXT4]);
	}

	/* config agdsp ctrl */
	if ((ctrl->ctrl_reg[CPROC_CTRL_EXT5] & INVALID_REG) != INVALID_REG) {
		pr_info("sprd_cproc: config agdsp ctrl -- reg = %x, mask = %x\n",
			ctrl->ctrl_reg[CPROC_CTRL_EXT5], ctrl->ctrl_mask[CPROC_CTRL_EXT5]);
		sci_glb_set(ctrl->ctrl_reg[CPROC_CTRL_EXT5], ctrl->ctrl_mask[CPROC_CTRL_EXT5]);
	}

	/* agdsp reset */
	if ((ctrl->ctrl_reg[CPROC_CTRL_EXT1] & INVALID_REG) != INVALID_REG) {
		pr_info("sprd_cproc: agdsp reset -- reg = %x, mask = %x\n",
			ctrl->ctrl_reg[CPROC_CTRL_EXT1], ctrl->ctrl_mask[CPROC_CTRL_EXT1]);
		sci_glb_clr(ctrl->ctrl_reg[CPROC_CTRL_EXT1], ctrl->ctrl_mask[CPROC_CTRL_EXT1]);
	}

	return 0;
}

static int sprd_cproc_native_dsp_stop(void *arg)
{
	struct cproc_device *cproc = (struct cproc_device *)arg;
	struct cproc_init_data *pdata = cproc->initdata;
	struct cproc_ctrl *ctrl;
	uint32_t state = 0;

	if (!pdata) {
		return -ENODEV;
	}

	pr_info("sprd_cproc: %s\n", __func__);

	ctrl = pdata->ctrl;

	/* agdsp core reset */
	if ((ctrl->ctrl_reg[CPROC_CTRL_EXT1] & INVALID_REG) != INVALID_REG) {
		sci_glb_set(ctrl->ctrl_reg[CPROC_CTRL_EXT1], ctrl->ctrl_mask[CPROC_CTRL_EXT1]);
		msleep(50);
	}

	/* agdsp sys reset */
	if ((ctrl->ctrl_reg[CPROC_CTRL_RESET] & INVALID_REG) != INVALID_REG) {
		sci_glb_set(ctrl->ctrl_reg[CPROC_CTRL_RESET], ctrl->ctrl_mask[CPROC_CTRL_RESET]);
		msleep(50);
	}

	/* agdsp force deep sleep */
	if ((ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP] & INVALID_REG) != INVALID_REG) {
		sci_glb_set(ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP], ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP]);
		msleep(50);
	}

	/* agdsp core force shutdown */
	if ((ctrl->ctrl_reg[CPROC_CTRL_EXT0] & INVALID_REG) != INVALID_REG) {
		sci_glb_set(ctrl->ctrl_reg[CPROC_CTRL_EXT0], ctrl->ctrl_mask[CPROC_CTRL_EXT0]);
	}

	/* agdsp sys force shutdown */
	if ((ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN] & INVALID_REG) != INVALID_REG) {
		sci_glb_set(ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN], ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN]);
	}

	return 0;
}

static const struct cproc_dev_ctrl g_cproc_dev_ctrl[pt_num][ct_num] = {
	[pm_t][PM_TO_INDEX(pm_ctrl_t0)] = {sprd_cproc_native_arm7_start, sprd_cproc_native_arm7_stop},
	[pm_t][PM_TO_INDEX(pm_ctrl_t1)] = {sprd_cproc_native_cmx_start, sprd_cproc_native_cmx_stop},
	[md_t][MD_TO_INDEX(md_ctrl_t0)] = {sprd_cproc_native_cp_start, sprd_cproc_native_cp_stop},
	[wb_t][WB_TO_INDEX(wb_ctrl_t0)] = {sprd_cproc_native_cp2_start, sprd_cproc_native_cp2_stop},
	[dp_t][DP_TO_INDEX(dp_ctrl_t0)] = {sprd_cproc_native_dsp_start, sprd_cproc_native_dsp_stop},
};

static void get_dev_ctrl_row_column(int type, int *row, int *column)
{
	if (NULL != row) {
		*row = GET_MD_TYPE(type);
	}

	if (NULL != column) {
		*column = TYPE_TO_INDEX(type);
	}

	return;
}

static int sprd_cproc_native_start(void *arg)
{
	uint32_t type;
	int row, column;
	int row_pm, column_pm;
	struct cproc_device *cproc = (struct cproc_device *)arg;

	if (!cproc) {
		return -ENODEV;
	}

	get_dev_ctrl_row_column(cproc->type, &row, &column);
	pr_info("sprd_cproc: native start type = 0x%x, line = 0x%x, column = 0x%x\n", cproc->type, row, column);
	if ((row < 0 || row >= pt_num) || (column < 0 || column >= ct_num)) {
		printk(KERN_ERR "sprd_cproc: the row or column out of bounds\n");
		return -1;
	}

	switch (row) {
	case md_t:
		if (pm_dev != NULL && pm_dev->status == STOP_STATUS) {
			get_dev_ctrl_row_column(pm_dev->type, &row_pm, &column_pm);
			if ((row_pm >= 0 && row_pm < pt_num) && (column_pm >= 0 && column_pm < ct_num)) {
				if (g_cproc_dev_ctrl[row_pm][column_pm].start) {
					pr_info("sprd_cproc: start pm\n");
					g_cproc_dev_ctrl[row_pm][column_pm].start(pm_dev);
				}
			}
		}

	default:
		if (g_cproc_dev_ctrl[row][column].start)
			return g_cproc_dev_ctrl[row][column].start(cproc);
		break;
	}

	return -1;
}

static int sprd_cproc_native_stop(void *arg)
{
	int row, column;

	struct cproc_device *cproc = (struct cproc_device *)arg;

	if (!cproc) {
		return -ENODEV;
	}

	get_dev_ctrl_row_column(cproc->type, &row, &column);
	pr_info("sprd_cproc: native stop type = 0x%x, line = 0x%x, column = 0x%x\n", cproc->type, row, column);
	if ((row < 0 || row >= pt_num) || (column < 0 || column >= ct_num)) {
		printk(KERN_ERR "sprd_cproc: the row or column out of bounds\n");
		return -1;
	}

	if (g_cproc_dev_ctrl[row][column].stop)
		return g_cproc_dev_ctrl[row][column].stop(cproc);

	return -1;
}

#endif

static const uint32_t g_common_loader[] = {
	0xe59f0000, 0xe12fff10, 0x8ae00000
};

static const uint32_t g_r5_loader[] = {
	0xee110f10, 0xe3c00005, 0xe3c00a01, 0xf57ff04f,
	0xee010f10, 0xf57ff06f, 0xee110f30, 0xe3800802,
	0xe3800801, 0xe3c00902, 0xee010f30, 0xf57ff04f,
	0xe3a00000, 0xee070f15, 0xee0f0f15, 0xe3a01000,
	0xee061f12, 0xf57ff06f, 0xe3a01000, 0xee061f11,
	0xe3a0103f, 0xee061f51, 0xe3a01f42, 0xee061f91,
	0xee110f30, 0xe3c00802, 0xe3c00801, 0xe3c00902,
	0xee010f30, 0xee110f10, 0xe3800001, 0xf57ff04f,
	0xee010f10, 0xf57ff06f, 0xe51ff004, 0x8b800000
};

struct sprd_cproc_data {
	int (*start)(void *arg);
	int (*stop)(void *arg);
	uint32_t iram_size;
	uint32_t *iramdata;
};

static struct sprd_cproc_data g_cproc_data = {
	.start = sprd_cproc_native_start,
	.stop  = sprd_cproc_native_stop,
	.iram_size = sizeof(g_common_loader),
	.iramdata = &g_common_loader[0],
};

static struct sprd_cproc_data g_cproc_data_pubcp = {
	.start = sprd_cproc_native_start,
	.stop  = sprd_cproc_native_stop,
	.iram_size = sizeof(g_r5_loader),
	.iramdata = &g_r5_loader[0],
};

static const struct of_device_id sprd_cproc_match_table[] = {
	{ .compatible = "sprd,scproc", .data = &g_cproc_data},
	{ .compatible = "sprd,scproc_pubcp", .data = &g_cproc_data_pubcp},
};

static int sprd_cproc_parse_dt(struct cproc_init_data **init, struct device *dev)
{
#ifdef CONFIG_OF
	struct cproc_init_data *pdata;
	struct cproc_ctrl *ctrl;
	struct resource res;
	struct device_node *np = dev->of_node, *chd;
	int ret, i, segnr, d_flag;
	uint32_t base, offset;
	uint32_t ctrl_reg[CPROC_CTRL_NR] = {0};
	uint32_t iram_dsize;
	struct cproc_dump_info *dump_info = NULL;
	uint32_t rindex, cs_index, cr_num;

	struct sprd_cproc_data *pcproc_data;
	const struct of_device_id *of_id = of_match_node(sprd_cproc_match_table, np);

	if (of_id)
		pcproc_data = (struct sprd_cproc_data *)of_id->data;
	else
		panic("%s: Not find matched id!", __func__);

	segnr = of_get_child_count(np);
	pr_info("sprd_cproc mem size: %u\n", sizeof(struct cproc_init_data) + segnr * sizeof(struct cproc_segments));

	pdata = kzalloc(sizeof(struct cproc_init_data) + segnr * sizeof(struct cproc_segments), GFP_KERNEL);
	if (!pdata) {
		return -ENOMEM;
	}

	ctrl = kzalloc(sizeof(struct cproc_ctrl), GFP_KERNEL);
	if (!ctrl) {
		kfree(pdata);
		return -ENOMEM;
	}

	ret = of_property_read_string(np, "sprd,name", (const char **)&pdata->devname);
	if (ret) {
		goto error;
	}

	rindex = 1;
	cs_index = 0;
	d_flag = 0;
	if (of_find_property(np, "sprd,decoup", NULL) != NULL) {
		d_flag = 1;
		rindex -= 1;
	}

	/* get iram_base+offset */
	ret = of_address_to_resource(np, rindex, &res);
	if (ret) {
		goto error;
	}
	ctrl->iram_addr = res.start;
	pr_info("sprd_cproc: iram_addr=0x%x\n", ctrl->iram_addr);

	/* get ctrl_reg */
	rindex++;
	cr_num = CPROC_CTRL_NR;
	do {
		ret = of_property_read_u32_array(np, "sprd,ctrl-reg", (uint32_t *)ctrl_reg, cr_num);
		if (ret) {
			cr_num--;
		}
		if (!cr_num) {
			goto error;
		}
	} while (ret);
	pr_info("sprd_cproc: rindex = 0x%x, cr_num = 0x%x\n", rindex, cr_num);

	for (i = 0; i < cr_num; i++) {
		pr_info("sprd_cproc before p2v: ctrl_reg[%d] = 0x%x\n", i, ctrl_reg[i]);
		if (INVALID_REG != ctrl_reg[i]) {
			ret = of_address_to_resource(np, rindex + i, &res);
			if (ret) {
				goto error;
			}
			ctrl->ctrl_reg[i] = SPRD_DEV_P2V(ctrl_reg[i] + res.start);
		} else {
			ctrl->ctrl_reg[i] = INVALID_REG;
		}
		pr_info("sprd_cproc: ctrl->ctrl_reg[%d] = 0x%x\n", i, ctrl->ctrl_reg[i]);
	}
	for (; i < CPROC_CTRL_NR; i++) {
		ctrl->ctrl_reg[i] = INVALID_REG;
	}

	if (d_flag) {
		cs_index = rindex + cr_num;
		rindex++;
	}

	/* get cp_base+offset */
	pr_info("sprd_cproc: cs_index = %d\n", cs_index);
	ret = of_address_to_resource(np, cs_index, &res);
	if (ret) {
		goto error;
	}
	pdata->base = res.start;
	pdata->maxsz = res.end - res.start + 1;
	pr_info("sprd_cproc: cp base = 0x%lx, size = 0x%x\n", pdata->base, pdata->maxsz);

	/* get mini-dump base+offset */
	rindex += cr_num;
	ret = of_address_to_resource(np, rindex, &res);
	if (ret) {
		dump_info = NULL;
	} else {
		dump_info = ioremap_nocache(res.start, res.end - res.start + 1);
		if (!dump_info) {
			printk(KERN_ERR "Unable to map dump info base: 0x%08x\n", res.start);
			ret = -ENOMEM;
			goto error;
		}
		pdata->shmem = dump_info;
	}

	/* get ctrl_mask */
	ret = of_property_read_u32_array(np, "sprd,ctrl-mask", (uint32_t *)ctrl->ctrl_mask, cr_num);
	if (ret) {
		printk(KERN_ERR "sprd_cproc: get ctrl-mask failed\n", cr_num);
		goto error;
	}
	for (i = 0; i < cr_num; i++) {
		pr_info("sprd_cproc: ctrl_mask[%d] = 0x%08x\n", i, ctrl->ctrl_mask[i]);
	}

	/* get iram data */
	ret = of_property_read_u32(np, "sprd,iram-dsize", &iram_dsize);
	if (ret) {
		iram_dsize = CPROC_IRAM_DATA_NR;
	}
	ctrl->iram_size = iram_dsize * sizeof(uint32_t);
	if (ctrl->iram_size > sizeof(ctrl->iram_data)) {
		printk(KERN_ERR "iram data size too small\n");
		goto error;
	}
	ret = of_property_read_u32_array(np, "sprd,iram-data", ctrl->iram_data, iram_dsize);
	if (ret) {
		ctrl->iram_size = pcproc_data->iram_size;
		memcpy(ctrl->iram_data, pcproc_data->iramdata, pcproc_data->iram_size);
	}

	for (i = 0; i < ctrl->iram_size/sizeof(uint32_t); i++) {
		pr_info("sprd_cproc: iram-data[%d] = 0x%08x\n", i, ctrl->iram_data[i]);
	}

	/* get irq */
	pdata->wdtirq = irq_of_parse_and_map(np, 0);
	pr_info("sprd_cproc: wdt irq %u\n", pdata->wdtirq);

	/* get cproc dev type */
	ret = of_property_read_u32(np, "sprd,type", &(pdata->type));
	if (ret) {
		if (strstr(pdata->devname, "pm")) {
			pdata->type = (pm_t << 16) | (pm_ctrl_t0);
		} else if (strstr(pdata->devname, "cn")) {
			pdata->type = (wb_t << 16) | (wb_ctrl_t0);
		} else {
			pdata->type = (md_t << 16) | (md_ctrl_t0);
		}
	}
	pr_info("sprd_cproc: dev type 0x%x\n", pdata->type);

	i = 0;
	for_each_child_of_node(np, chd) {
		struct cproc_segments *seg;
		seg = &pdata->segs[i];
		ret = of_property_read_string(chd, "cproc,name", (const char **)&seg->name);
		if (ret) {
			goto error;
		}
		pr_info("sprd_cproc: child node [%d] name=%s\n", i, seg->name);
		/* get child base addr */
		ret = of_address_to_resource(chd, 0, &res);
		if (ret) {
			goto error;
		}
		seg->base = res.start;
		seg->maxsz = res.end - res.start + 1;
		pr_info("sprd_cproc: child node [%d] base=0x%x, size=0x%0x\n", i, seg->base, seg->maxsz);

		i++;
	}

	pr_info("sprd_cproc: stop callback 0x%x, start callback 0x%x\n",
		sprd_cproc_native_stop, sprd_cproc_native_start);
	pdata->segnr = segnr;

	pdata->start = pcproc_data->start;
	pdata->stop = pcproc_data->stop;

	pdata->ctrl = ctrl;
	*init = pdata;
	return 0;

error:
	if(dump_info)
		iounmap(dump_info);
	pdata->shmem = NULL;
	kfree(ctrl);
	kfree(pdata);
	return ret;
#else
	return -ENODEV;
#endif
}

static void sprd_cproc_destroy_pdata(struct cproc_init_data **init)
{
#ifdef CONFIG_OF
	struct cproc_init_data *pdata = *init;

	if (pdata) {
		if (pdata->ctrl) {
			kfree(pdata->ctrl);
		}

		if (pdata->shmem) {
			iounmap(pdata->shmem);
			pdata->shmem = NULL;
		}

		kfree(pdata);
	}
	*init = NULL;
#else
	return;
#endif
}

static int sprd_cproc_probe(struct platform_device *pdev)
{
	struct cproc_device *cproc;
	struct cproc_init_data *pdata = pdev->dev.platform_data;
	int rval;
	if (!pdata && pdev->dev.of_node) {
		rval = sprd_cproc_parse_dt(&pdata, &pdev->dev);
		if (rval) {
			printk(KERN_ERR "failed to parse device tree!\n");
			return rval;
		}
	}
	pr_info("sprd_cproc: pdata=0x%x, of_node=0x%x\n",
		(uint32_t)pdata, (uint32_t)pdev->dev.of_node);

	cproc = kzalloc(sizeof(struct cproc_device), GFP_KERNEL);
	if (!cproc) {
		sprd_cproc_destroy_pdata(&pdata);
		printk(KERN_ERR "failed to allocate cproc device!\n");
		return -ENOMEM;
	}

	pr_info("%s %p %x\n", __func__, pdata->base, pdata->maxsz);
#if 0
	if ( pdata->base == WCN_START_ADDR)
		set_section_ro(__va(pdata->base), (WCN_TOTAL_SIZE & ~(SECTION_SIZE - 1)) >> SECTION_SHIFT);
	else {
		if (!(pdata->maxsz % SECTION_SIZE))
			set_section_ro(__va(pdata->base), pdata->maxsz >> SECTION_SHIFT);
		else
			printk("%s WARN can't be marked RO now\n", __func__);
	}
#endif
	cproc->initdata = pdata;

	cproc->miscdev.minor = MISC_DYNAMIC_MINOR;
	cproc->miscdev.name = cproc->initdata->devname;
	cproc->miscdev.fops = &sprd_cproc_fops;
	cproc->miscdev.parent = NULL;
	cproc->name = cproc->initdata->devname;
	cproc->type = cproc->initdata->type;
	rval = misc_register(&cproc->miscdev);
	if (rval) {
		sprd_cproc_destroy_pdata(&cproc->initdata);
		kfree(cproc);
		printk(KERN_ERR "failed to register sprd_cproc miscdev!\n");
		return rval;
	}

	printk(KERN_INFO "sprd_cproc cp boot mode: 0x%x\n", cp_boot_mode);
	if (!cp_boot_mode)
		cproc->status = NORMAL_STATUS;
	else
		cproc->status = STOP_STATUS;
	cproc->wdtcnt = CPROC_WDT_FLASE;
	init_waitqueue_head(&(cproc->wdtwait));

	/* register IPI irq */
	if (cproc->initdata->wdtirq > 32) {
		rval = request_irq(cproc->initdata->wdtirq, sprd_cproc_irq_handler,
				0, cproc->initdata->devname, cproc);
		if (rval != 0) {
			misc_deregister(&cproc->miscdev);
			sprd_cproc_destroy_pdata(&cproc->initdata);
			printk(KERN_ERR "Cproc failed to request irq %s: %d\n",
					cproc->initdata->devname, cproc->initdata->wdtirq);
			kfree(cproc);
			return rval;
		}
	}

	sprd_cproc_fs_init(cproc);

	platform_set_drvdata(pdev, cproc);

	if (GET_MD_TYPE(cproc->type) == pm_t)
		pm_dev = cproc;

	printk(KERN_INFO "cproc %s probed!\n", cproc->initdata->devname);

	return 0;
}

static int sprd_cproc_remove(struct platform_device *pdev)
{
	struct cproc_device *cproc = platform_get_drvdata(pdev);

	sprd_cproc_fs_exit(cproc);
	misc_deregister(&cproc->miscdev);
	sprd_cproc_destroy_pdata(&cproc->initdata);

	printk(KERN_INFO "cproc %s removed!\n", cproc->initdata->devname);
	kfree(cproc);

	return 0;
}

static struct platform_driver sprd_cproc_driver = {
	.probe    = sprd_cproc_probe,
	.remove   = sprd_cproc_remove,
	.driver   = {
		.owner = THIS_MODULE,
		.name = "sprd_cproc",
		.of_match_table = sprd_cproc_match_table,
	},
};

static int __init sprd_cproc_init(void)
{

	if (platform_driver_register(&sprd_cproc_driver) != 0) {
		printk(KERN_ERR "sprd_cproc platform drv register Failed \n");
		return -1;
	}
	return 0;
}

static void __exit sprd_cproc_exit(void)
{
	platform_driver_unregister(&sprd_cproc_driver);
}

module_init(sprd_cproc_init);
module_exit(sprd_cproc_exit);

MODULE_DESCRIPTION("SPRD Communication Processor Driver");
MODULE_LICENSE("GPL");
