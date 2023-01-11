/*
* This software program is licensed subject to the GNU General Public License
* (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

* (C) Copyright 2013 Marvell International Ltd.
* All Rights Reserved
*/
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/dmaengine.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/io.h>
#include <linux/semaphore.h>
#include <linux/platform_device.h>
#include <asm/page.h>
#include <linux/ratelimit.h>
#include <linux/dma-mapping.h>

#include "msa_ring_buffer.h"
#include "shm.h"
#include "shm_map.h"

#if 0
#define debug_print(x, ...)   pr_dbg(x, ##__VA_ARGS__)
#else
#define debug_print(x, ...)
#endif

#if 0
#define irq_debug_print(x, ...)   trace_printk(x, ##__VA_ARGS__)
#else
#define irq_debug_print(x, ...)
#endif

#define RETRY_NUM 6
#define PXA_CONTROLLER_NAME_LEN 15

#define RIPC_STATUS_REG 0x0000
#define RIPC_AP_INT_REQ 0x0004

#define RIPC_DMA_MAJOR 0
#define IML_BLOCK_SIZE      8192 /* size of each iml buffer */
#define DMA_NAME "mmp-pdma"
#define DCMD_LENGTH 0x01fff

#define IML_FLAG_RIPC (1 << 0x4)
#define IML_FLAG_DDR (1 << 0x8)

#ifndef NO_IRQ
#define NO_IRQ ((unsigned int)(-1))
#endif

static int mem_size = -1;
static int buffer_size = -1;
static DEFINE_SEMAPHORE(dma_sem);
module_param(mem_size, int, 0);
MODULE_PARM_DESC(mem_size, " ");
module_param(buffer_size, int, 0);
MODULE_PARM_DESC(buffer_size, " ");

struct ring_ctl_head {
	int total_block;
	int read_ptr; /* pointer to last user fetched block */
	int write_pending_ptr; /* pointer to last DMA-ing block */
	int write_ptr; /* pointer to last DMA-ed block*/
	int signature;
};

struct msa_ap_shmem_head {
	u16 start_offset; /* not used */
	u16 length; /* number of bytes for current DMA transfer */
	u32 start_ringbuf; /* start addr of current DMA transfer */
	u32 start_addr; /* start addr of DMA buffer */
	u16 ripc_flag;
	u16 ripc_irq_num; /* number of irq triggered by DSP (for debug)*/
};

struct dma_req_arg {
	int slot_index; /* CP side index */
	int block_index; /* AP side index */
	dma_cookie_t tx_cookie;
};

static u32 iml_block_num = 512;
static void __iomem *p_sharemem;
static struct msa_ap_shmem_head *p_share_mem;
static void __iomem *ripc_base;
static void __iomem *ripc_clock_reg;
static void __iomem *cp_remap_reg;

static struct iml_module_device iml_dev;
static struct class *iml_dev_class;

/*if the value of major is not zero, dynamic alloc will not be called */
static int iml_module_major = RIPC_DMA_MAJOR;
static int iml_module_minor;
static dma_addr_t iml_dma_mask_set, iml_dma_mask_clear = (dma_addr_t)~0;


static void first_ripc_handshake(void);
static void request_ripc_interrupt(void);

static void dump_mas_ap_head(struct msa_ap_shmem_head  *head)
{
	pr_info("head addr = %p\n", head);
	pr_info("start_offset = %d\n", head->start_offset);
	pr_info("length = %d\n", head->length);
	pr_info("start_ringbuf = 0x%08x\n", head->start_ringbuf);
	pr_info("start_addr = 0x%08x\n", head->start_addr);
	pr_info("ripc_flag = 0x%04x\n", head->ripc_flag);
	pr_info("ripc_irq_num = %d\n", head->ripc_irq_num);
}


/*
 * DMA related variables
 */
struct iml_pxa_dma {
	struct dma_chan *dma_chan;
	struct dma_async_tx_descriptor *desc;
	struct tasklet_struct tsklet;
	atomic_t refcnt;
};

struct iml_module_device {
	struct iml_pxa_dma *iml_dma;
	int irq;
	wait_queue_head_t rx_wq;
	u8 *virbuf_addr;		/* used for DMA alloc */
	dma_addr_t phybuf_addr;	/* used for DMA alloc */
	struct ring_ctl_head *ctl_head;
	struct cdev cdev;
	struct platform_device *pdev;
};

static inline unsigned long slot_to_phy(int slot_index)
{
	return (slot_index << SLOT_SHIFT) + p_share_mem->start_addr;
}

static inline int phy_to_slot(unsigned long addr)
{
	unsigned long start_addr = p_share_mem->start_addr;
	int slot;

	if (unlikely(addr < start_addr))
		return -1;
	slot = (addr - start_addr) >> SLOT_SHIFT;
	return slot;
}


static bool iml_log_pause(void)
{
	debug_print("%s\n", __func__);

	/* MSA Log through AP Switch for initialization,
	   1 -> switch on; 0 -> switch off */
	p_share_mem->ripc_flag &= ~(IML_FLAG_RIPC | IML_FLAG_DDR);
	/* clear RIPC interrupt request. */
	writel(0x00, ripc_base + RIPC_AP_INT_REQ);
	return true;
}

static void first_ripc_handshake(void)
{
	debug_print("%s\n", __func__);

	init_ring_state();
	request_ripc_interrupt();
}

static void request_ripc_interrupt(void)
{
	irq_debug_print("%s\n", __func__);

	readl(ripc_base + RIPC_STATUS_REG);
	/* when MSA frees the resource, AP will get a RIPC INT */
	writel(0x1, ripc_base + RIPC_AP_INT_REQ);
}


static inline int get_next_write_pending_block(struct ring_ctl_head *ctl_head)
{
	return ctl_head->write_pending_ptr + 1 == ctl_head->total_block ?
					0 : ctl_head->write_pending_ptr + 1;
}

static inline int get_next_write_block(struct ring_ctl_head *ctl_head)
{
	return ctl_head->write_ptr + 1 == ctl_head->total_block ?
					0 : ctl_head->write_ptr + 1;
}

static inline int get_next_read_block(struct ring_ctl_head *ctl_head)
{
	return ctl_head->read_ptr + 1 == ctl_head->total_block ?
					0 : ctl_head->read_ptr + 1;
}

static inline int get_free_write_pending_block(struct ring_ctl_head *ctl_head)
{
	int free = ctl_head->read_ptr - get_next_write_pending_block(ctl_head);
	if (free < 0)
		free += ctl_head->total_block;
	return free;
}

static inline int get_free_read_block(struct ring_ctl_head *ctl_head)
{
	int free = ctl_head->write_ptr - ctl_head->read_ptr;
	if (free < 0)
		free += ctl_head->total_block;
	return free;
}

static inline void block_write_mark_complete(struct ring_ctl_head *ctl_head,
						int block)
{
	int next = get_next_write_block(ctl_head);
	if (block != next)
		pr_err("DMA Disorder:%d-%d\n", next, block);
	do {
		ctl_head->write_ptr = get_next_write_block(ctl_head);

	} while (ctl_head->write_ptr != block);

}


static inline unsigned long block_to_phy(int block)
{

	return (block << SLOT_SHIFT) + iml_dev.phybuf_addr;
}

static void cal_dma_mask(void)
{
	/* reg - content of cp remap register
	 * offset[0] - remap bit starting pos in CP addr remap register
	 * offset[1] - remap bit starting pos in CP address
	 * offset[2] - number of bits to remap
	 */
	u32 reg, offset[3];

	if (of_property_read_u32_array(iml_dev.pdev->dev.of_node,
			"remap", offset, 3)) {
		dev_err(&iml_dev.pdev->dev, "cannot find remap offset\n");
		return;
	}

	reg = readl(cp_remap_reg);
	iml_dma_mask_set = ((reg >> offset[0]) & ((1 << offset[2]) - 1));
	iml_dma_mask_set <<= offset[1];
	iml_dma_mask_clear = (1 << offset[2]) - 1;
	iml_dma_mask_clear = ~(iml_dma_mask_clear << offset[1]);
	pr_info("dma_set_mask %p, dma_clear_mask %p\n",
		(void *)iml_dma_mask_set, (void *)iml_dma_mask_clear);
}

/* replace higher bits of MSA addr */
static inline dma_addr_t remap_dma_addr(dma_addr_t addr)
{
	return (addr & iml_dma_mask_clear) | iml_dma_mask_set;
}

static void schedule_rx(struct iml_module_device *dev)
{
	tasklet_schedule(&dev->iml_dma->tsklet);
}

static inline void dma_cleanup(struct iml_module_device *dev)
{
	dma_free_coherent(&dev->pdev->dev,
		IML_BLOCK_SIZE * iml_block_num,
		dev->virbuf_addr - IML_BLOCK_SIZE,
		dev->phybuf_addr - IML_BLOCK_SIZE);
	dev->virbuf_addr = NULL;
	dev->phybuf_addr = 0;
	dev_info(&dev->pdev->dev, "deallocate DMA region\n");
}

static void dma_func_callback(void *arg)
{
	struct dma_tx_state dma_state;
	enum dma_status status;
	struct dma_req_arg *req_arg = (struct dma_req_arg *)arg;
	int block = req_arg->block_index;
	int slot = req_arg->slot_index;
	irq_debug_print("%s\n", __func__);

	status =
	    dmaengine_tx_status(iml_dev.iml_dma->dma_chan,
				req_arg->tx_cookie, &dma_state);

	irq_debug_print("complete DMA from slot %d => block %d status :%d\n",
		slot, block, status);

	if (status == DMA_ERROR)
		pr_err("dma bus error, a 8K data corrupted\n");

	slot_data_out_ring(slot);
	block_write_mark_complete(iml_dev.ctl_head, block);
	wake_up_interruptible(&iml_dev.rx_wq);
	kfree(arg);
	if (atomic_dec_and_test(&iml_dev.iml_dma->refcnt)) {
		dma_cleanup(&iml_dev);
		up(&dma_sem);
	}
}

static void iml_dma_start_func(struct iml_module_device *dev)
{
	u16 datasize;
	dma_addr_t src_phy_add, dst_phy_add;
	enum dma_ctrl_flags flags = 0;
	int read_slot_index;
	int free_block;
	int next_write_block;
	struct dma_req_arg *arg;

	read_slot_index = get_next_read_slot();
	irq_debug_print("%s, next_read_slot :%d\n", __func__, read_slot_index);
	while (read_slot_index >= 0) {
		free_block = get_free_write_pending_block(dev->ctl_head);
		if (unlikely(free_block <= 0)) {
			pr_err_ratelimited("error: no AP buffer is left\n");
			wake_up_interruptible(&iml_dev.rx_wq);
			schedule_rx(&iml_dev);
			break;
		}
		next_write_block = get_next_write_pending_block(dev->ctl_head);
		irq_debug_print("%s, next_write_block :%d\n", __func__,
						next_write_block);
		datasize = p_share_mem->length;
		dst_phy_add = block_to_phy(next_write_block);
		src_phy_add = slot_to_phy(read_slot_index);
		src_phy_add = remap_dma_addr(src_phy_add);

		irq_debug_print("prepare DMA from slot %d => block %d\n",
			read_slot_index, next_write_block);

		arg = kmalloc(sizeof(struct dma_req_arg), GFP_ATOMIC);
		if (!arg) {
			pr_err_ratelimited("failed to alloc dma req\n");
			break;
		}
		arg->block_index = next_write_block;
		arg->slot_index = read_slot_index;
		iml_dev.iml_dma->desc = iml_dev.iml_dma->dma_chan->
		device->device_prep_dma_memcpy(iml_dev.iml_dma->dma_chan,
					dst_phy_add, src_phy_add,
					(DCMD_LENGTH & datasize), flags);
		iml_dev.iml_dma->desc->callback = dma_func_callback;
		iml_dev.iml_dma->desc->callback_param = arg;
		arg->tx_cookie = dmaengine_submit(iml_dev.iml_dma->desc);
		if (arg->tx_cookie  <= 0) {
			pr_err("iml_dev.iml_dma->cookie = %d\n",
					arg->tx_cookie);
		} else
			atomic_inc(&iml_dev.iml_dma->refcnt);

		read_slot_index = get_next_read_slot();
		irq_debug_print("%s, next_read_slot :%d\n", __func__,
					read_slot_index);
		dev->ctl_head->write_pending_ptr =
				get_next_write_pending_block(dev->ctl_head);
	}
	/*On completion of each DMA operation, the next in queue is started and
	   a tasklet is triggered. The tasklet will then call the client driver
	   completion callback routine for notification, if set. */
	/*The transactions in the pending queue can be activated
	   by calling the issue_pending API, */
	dma_async_issue_pending(iml_dev.iml_dma->dma_chan);
	request_ripc_interrupt();
}


static void rx_func(unsigned long arg)
{
	struct iml_module_device *dev = (struct iml_module_device *)arg;
	iml_dma_start_func(dev);
}

static int iml_module_open(struct inode *inode, struct file *file)
{
	u8 *virt_addr;
	dma_addr_t phy_addr;
	int ddr_on = (file->f_flags & O_ACCMODE) == O_WRONLY;

	if (down_trylock(&dma_sem)) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		pr_warn("wait for DMA transaction to finish\n");
		if (down_interruptible(&dma_sem))
			return -ERESTARTSYS;
	}

	if (!ddr_on) {
		virt_addr = dma_alloc_coherent(&iml_dev.pdev->dev,
			IML_BLOCK_SIZE * iml_block_num, &phy_addr, GFP_KERNEL);
		if (!virt_addr) {
			up(&dma_sem);
			dev_err(&iml_dev.pdev->dev, "IML_module can't malloc dma buffer\n");
			return -ENOMEM;
		}

		if (unlikely(cp_remap_reg != NULL)) {
			cal_dma_mask();
			cp_remap_reg = NULL; /* no need to read it anymore */
		}
		atomic_set(&iml_dev.iml_dma->refcnt, 1);
		iml_dev.ctl_head = (struct ring_ctl_head *)virt_addr;
		iml_dev.ctl_head->signature = 0x12345678;
		iml_dev.ctl_head->read_ptr = 0;
		iml_dev.ctl_head->write_pending_ptr = 0;
		iml_dev.ctl_head->write_ptr = 0;
		iml_dev.ctl_head->total_block = iml_block_num - 1;
		iml_dev.virbuf_addr = virt_addr + IML_BLOCK_SIZE;
		iml_dev.phybuf_addr = phy_addr + IML_BLOCK_SIZE;
		dev_info(&iml_dev.pdev->dev,
			"DMA alloc coherent virt 0x%p, phy 0x%p, mem size: 0x%x\n",
			virt_addr, (void *)phy_addr,
			IML_BLOCK_SIZE * iml_block_num);

		p_share_mem->ripc_flag |= IML_FLAG_RIPC;
		first_ripc_handshake();
		pr_info("iml RIPC on\n");
	} else {
		p_share_mem->ripc_flag |= IML_FLAG_DDR;
		pr_info("iml DDR on\n");
	}
	file->private_data = container_of(inode->i_cdev, struct iml_module_device, cdev);
	return 0;
}

static ssize_t iml_module_read(struct file *file, char __user *buf,
			       size_t count, loff_t *offset)
{
	return 0;
}

static ssize_t iml_module_write(struct file *file, const char __user *buf,
				size_t count, loff_t *offset)
{
	return 0;
}

static unsigned int iml_module_poll(struct file *filp, poll_table *wait)
{
	int mask = 0;
	int read_avaliable;
	struct iml_module_device *dev =
	    (struct iml_module_device *)filp->private_data;
	poll_wait(filp, &dev->rx_wq, wait);
	read_avaliable = get_free_read_block(dev->ctl_head);
	if (read_avaliable > 0)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static void cp_vma_open(struct vm_area_struct *vma)
{
	pr_info("cp vma open 0x%lx -> 0x%lx (0x%lx)\n",
	       vma->vm_start, vma->vm_pgoff << PAGE_SHIFT,
	       (long unsigned int)vma->vm_page_prot);
}

static void cp_vma_close(struct vm_area_struct *vma)
{
	pr_info("cp vma close 0x%lx -> 0x%lx\n",
	       vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

/* These are mostly for debug: do nothing useful otherwise */
static struct vm_operations_struct vm_ops = {
	.open = cp_vma_open,
	.close = cp_vma_close
};

static int iml_mmap(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_flags |= VM_IO;
	vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	pr_info("cp vma open 0x%lx -> 0x%lx (0x%lx)\n",
	       vma->vm_start, vma->vm_pgoff << PAGE_SHIFT,
	       (long unsigned int)vma->vm_page_prot);
	if ((vma->vm_end - vma->vm_start) != IML_BLOCK_SIZE * iml_block_num)
		return -EINVAL;
	if (remap_pfn_range(vma, vma->vm_start,
			(iml_dev.phybuf_addr - IML_BLOCK_SIZE) >> PAGE_SHIFT,
			IML_BLOCK_SIZE * iml_block_num, vma->vm_page_prot)) {
		return -EAGAIN;
	}
	vma->vm_ops = &vm_ops;
	return 0;
}

static int iml_module_release(struct inode *inode, struct file *file)
{
	pr_info("iml off\n");
	iml_log_pause();
	if ((file->f_flags & O_ACCMODE) != O_WRONLY) {
		if (atomic_dec_and_test(&iml_dev.iml_dma->refcnt)) {
			dma_cleanup(&iml_dev);
			up(&dma_sem);
		}
	} else
		up(&dma_sem);
	return 0;
}

static const struct file_operations iml_dev_fops = {
	.owner = THIS_MODULE,
	.llseek = NULL,
	.read = iml_module_read,
	.write = iml_module_write,
	.open = iml_module_open,
	.release = iml_module_release,
	.poll = iml_module_poll,
	.mmap = iml_mmap,
};

static irqreturn_t ripc_interrupt_handler(int irq, void *dev_id)
{
	int slot;

	irq_debug_print("%s\n", __func__);
	writel(0x0, ripc_base + RIPC_AP_INT_REQ);

	slot = phy_to_slot(p_share_mem->start_ringbuf);
	irq_debug_print("receive slot %d (0x%08x) from msa\n",
			slot, p_share_mem->start_ringbuf);
	if (unlikely(slot < 0 || slot >= SLOT_NUMBER)) {
		irq_debug_print("invalid slot %d (0x%08x) from msa\n",
				slot, p_share_mem->start_ringbuf);
		return IRQ_HANDLED;
	}
	slot_data_in_ring(slot);
	schedule_rx(&iml_dev);
	return IRQ_HANDLED;
}

static int iml_dma_init(struct platform_device *pdev)
{
	iml_dev.iml_dma->dma_chan
			= dma_request_slave_channel(&pdev->dev,
						"iml-dma");

	if (NULL == iml_dev.iml_dma->dma_chan) {
		dev_err(&pdev->dev, "IML dma request fail\n");
		return -ENODEV;
	}

	return 0;
}

static void *of_shm_map_nocache(struct platform_device *pdev, int index)
{
	struct resource res;
	struct device_node *np = pdev->dev.of_node;
	if (of_address_to_resource(np, index, &res))
		return ERR_PTR(-EINVAL);

	dev_info(&pdev->dev, "iml res:0x%lx -- 0x%lx\n",
		(unsigned long)res.start, (unsigned long)res.end);
	return devm_shm_map(&pdev->dev, res.start, resource_size(&res));
}

static int iml_cdev_probe(struct platform_device *pdev)
{
	int result;
	dev_t dev_no;

	if (iml_module_major) {
		dev_no = MKDEV(iml_module_major, iml_module_minor);
		result = register_chrdev_region(dev_no, 1, "iml");
	} else {
		result =
		    alloc_chrdev_region(&dev_no, iml_module_minor, 1, "iml");
		iml_module_major = MAJOR(dev_no);
	}
	if (result < 0) {

		dev_err(&pdev->dev, "IML-module can't get major\n");
		result = -ENOMEM;
		goto error;
	}

	cdev_init(&iml_dev.cdev, &iml_dev_fops);
	iml_dev.cdev.owner = THIS_MODULE;
	result = cdev_add(&iml_dev.cdev, dev_no, 1);
	if (result) {
		dev_err(&pdev->dev, "register iml_device fail\n");
		goto error_region;
	}

	iml_dev_class = class_create(THIS_MODULE, "imldev");

	if (IS_ERR(iml_dev_class)) {
		dev_err(&pdev->dev, "create device error");
		cdev_del(&iml_dev.cdev);
		result = PTR_ERR(iml_dev_class);
		goto error_region;
	}
	dev_dbg(&pdev->dev, "create device imldev");
	device_create(iml_dev_class, NULL, dev_no,
		      NULL, "imldev%d", iml_module_minor);


	init_waitqueue_head(&iml_dev.rx_wq);
	return 0;

error_region:
	unregister_chrdev_region(dev_no, 1);
error:
	return result;

}

static void iml_cdev_remove(struct platform_device *pdev)
{
	dev_t dev_no;
	dev_no = MKDEV(iml_module_major, iml_module_minor);
	cdev_del(&iml_dev.cdev);
	unregister_chrdev_region(dev_no, 1);
	device_destroy(iml_dev_class, dev_no);
	class_destroy(iml_dev_class);
}

static int iml_iomem_probe(struct  platform_device *pdev)
{
	struct resource res;
	struct device_node *np = pdev->dev.of_node;
	int ret;

	if (!devres_open_group(&pdev->dev, iml_iomem_probe, GFP_KERNEL))
		return -ENOMEM;

	/* iomap share mem header */
	p_sharemem = of_shm_map_nocache(pdev, 0);
	if (IS_ERR(p_sharemem)) {
		dev_err(&pdev->dev, "ioremap p_sharemem failure\n");
		ret = PTR_ERR(p_sharemem);
		goto iomem_error;
	}

	/* iomap ripc reg */
	if (of_address_to_resource(np, 1, &res)) {
		ret = -EINVAL;
		goto iomem_error;
	}
	ripc_base = devm_ioremap_resource(&pdev->dev, &res);
	if (IS_ERR(ripc_base)) {
		dev_err(&pdev->dev, "ioremap ripc_base failure\n");
		ret = PTR_ERR(ripc_base);
		goto iomem_error;
	}

	/* iomap ripc clock */
	if (of_address_to_resource(np, 2, &res)) {
		ret = -EINVAL;
		goto iomem_error;
	}
	ripc_clock_reg = devm_ioremap_resource(&pdev->dev, &res);
	if (IS_ERR(ripc_clock_reg)) {
		dev_err(&pdev->dev, "ioremap ripc_clock_reg failure\n");
		ret = PTR_ERR(ripc_clock_reg);
		goto iomem_error;
	}

	writel(0x02, ripc_clock_reg);

	/* iomap dma addr remap */
	if (of_address_to_resource(np, 3, &res) == 0) {
		/* the DMA addr sent by MSA is not physical address
		 * so we need to manually remap it to phy addr
		 * note that DMA engine hardware on some platforms
		 * can support non phy addr sent by MSA
		 */
		cp_remap_reg = devm_ioremap_resource(&pdev->dev, &res);
		if (IS_ERR(cp_remap_reg)) {
			dev_err(&pdev->dev,
				"ioremap cp remap reg failure\n");
			ret = PTR_ERR(cp_remap_reg);
			goto iomem_error;
		}
	}

	p_share_mem = (struct msa_ap_shmem_head *)p_sharemem;
	p_share_mem->ripc_flag = 0;
	dump_mas_ap_head(p_share_mem);
	devres_remove_group(&pdev->dev, iml_iomem_probe);
	return 0;

iomem_error:
	devres_release_group(&pdev->dev, iml_iomem_probe);
	return ret;
}

static int iml_irq_probe(struct platform_device *pdev)
{
	int ret;
	iml_dev.irq = irq_of_parse_and_map(pdev->dev.of_node, 0);

	if (!devres_open_group(&pdev->dev, iml_irq_probe, GFP_KERNEL))
		return -ENOMEM;
	if (iml_dev.irq == NO_IRQ) {
		dev_err(&pdev->dev,
			"Can't find assigned irq 0x%x RIPC\n", iml_dev.irq);
		ret = -EINVAL;
		goto irq_error;
	}
	ret = devm_request_irq(&pdev->dev, iml_dev.irq,
			ripc_interrupt_handler, IRQF_SHARED, "iml", &iml_dev);
	if (ret) {
		dev_err(&pdev->dev,
			"RIPC can't get assigned irq 0x%x\n", ret);
		goto irq_error;
	}
	devres_remove_group(&pdev->dev, iml_irq_probe);
	return 0;
irq_error:
	devres_release_group(&pdev->dev, iml_irq_probe);
	return ret;
}

static int iml_dma_probe(struct  platform_device *pdev)
{
	int ret;

	if (!devres_open_group(&pdev->dev, iml_dma_probe, GFP_KERNEL))
		return -ENOMEM;

	dev_dbg(&pdev->dev,
			"try to allocate %d\n", iml_block_num);

	iml_dev.iml_dma = devm_kzalloc(&pdev->dev,
				sizeof(struct iml_pxa_dma), GFP_KERNEL);
	if (!iml_dev.iml_dma) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "IML_module can't get iml_dma\n");
		goto dma_error;
	}

	ret = iml_dma_init(pdev);
	if (ret) {
		dev_err(&pdev->dev, "IML_module can't get dma channel\n");
		goto dma_error;
	}
	tasklet_init(&iml_dev.iml_dma->tsklet,
			rx_func, (unsigned long)&iml_dev);
	devres_remove_group(&pdev->dev, iml_dma_probe);

	return 0;
dma_error:
	devres_release_group(&pdev->dev, iml_dma_probe);
	return ret;
}


static int iml_module_probe(struct platform_device *pdev)
{
	int result = 0;

	dev_info(&pdev->dev, "IML module probe\n");

	memset(&iml_dev, 0, sizeof(struct iml_module_device));
	iml_dev.pdev = pdev;

	result = iml_iomem_probe(pdev);
	if (result)
		return result;

	result = iml_irq_probe(pdev);
	if (result)
		return result;

	result = iml_dma_probe(pdev);
	if (result)
		return result;

	result = iml_cdev_probe(pdev);
	if (result)
		goto exit;

	return 0;

exit:
	iml_cdev_remove(pdev);
	if (iml_dev.iml_dma->dma_chan)
		dma_release_channel(iml_dev.iml_dma->dma_chan);
	tasklet_kill(&iml_dev.iml_dma->tsklet);
	return result;

}

static int iml_module_remove(struct platform_device *pdev)
{
	iml_cdev_remove(pdev);
	if (iml_dev.iml_dma->dma_chan)
		dma_release_channel(iml_dev.iml_dma->dma_chan);
	tasklet_kill(&iml_dev.iml_dma->tsklet);
	return 0;
}


static const struct of_device_id iml_of_match[] = {
		{.compatible = "marvell,mmp-iml", },
		{},
};

static struct platform_driver pxa9xx_iml_driver = {
	.driver = {
		   .name = "iml",
		   .of_match_table = iml_of_match,
		   },
	.probe = iml_module_probe,
	.remove = iml_module_remove
};

module_platform_driver(pxa9xx_iml_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhongmin Wu <zmwu@marvell.com>");
MODULE_DESCRIPTION("IML driver to dump DSP logs");

