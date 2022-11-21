#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h> /* printk() */
#include <linux/fs.h>     /* everything... */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>

MODULE_LICENSE("Dual BSD/GPL");
static char *Version = "1.4";

static int memdisk_major = 0;
module_param(memdisk_major, int, 0);
static int hardsect_size = 512;
module_param(hardsect_size, int, 0);

/*
 * We can tweak our hardware sector size, but the kernel talks to us
 * in terms of small sectors, always.
 */
#define KERNEL_SECTOR_SIZE 512
#define MEMDISK_MINORS	16
#define MEMDISKS_COUNT 3
/*
 * The internal representation of our device.
 */
static struct memdisk_dev {
    unsigned long size;             /* Device size in sectors */
    spinlock_t lock;                /* For mutual exclusion */
    u8 *data;                       /* The data array */
    struct request_queue *queue;    /* The device request queue */
    struct gendisk *gd;             /* The gendisk structure */
} memdisk_device;

#if 1 //1G
#define MEMDISK_SYSTEM_PHYS    0xA0000000
#define MEMDISK_SYSTEM_SIZE    0x12c00000 // 300M

#define MEMDISK_USERDATA_PHYS  0xB2E00000
#define MEMDISK_USERDATA_SIZE  0x9600000  // 150M

#define MEMDISK_CACHE_PHYS     0xBC600000
#define MEMDISK_CACHE_SIZE     0x3200000  // 50M
#else //512M
#define MEMDISK_SYSTEM_PHYS    0xA0000000
#define MEMDISK_SYSTEM_SIZE    0xD200000  // 210M

#define MEMDISK_USERDATA_PHYS  0xB2E00000
#define MEMDISK_USERDATA_SIZE  0x2800000  // 40M

#define MEMDISK_CACHE_PHYS     0xBC600000
#define MEMDISK_CACHE_SIZE     0x600000   // 6M
#endif

static struct memdisk_dev sprd_memdisks[] = {
        [0] = {
            .data    = MEMDISK_SYSTEM_PHYS,
            .size    = MEMDISK_SYSTEM_SIZE,
        },
        [1] = {
            .data    = MEMDISK_USERDATA_PHYS,
            .size    = MEMDISK_USERDATA_SIZE,
        },
        [2] = {
            .data    = MEMDISK_CACHE_PHYS,
            .size    = MEMDISK_CACHE_SIZE,
        },
};

/*
 * Handle an I/O request.
 */
static void memdisk_transfer(struct memdisk_dev *dev, sector_t sector
		                         , unsigned long nsect, char *buffer, int write)
{
    unsigned long offset = sector * hardsect_size;
    unsigned long nbytes = nsect * hardsect_size;

    //printk("memdisk_transfer: %s offset:%x, %d bytes. \n", write ? "write" : "read", offset, nbytes);
    if ((offset + nbytes) > (dev->size * hardsect_size)) {
        printk (KERN_NOTICE "memdisk: Beyond-end write (%ld %ld)\n", offset, nbytes);
        return;
    }

    if (write)
        memcpy(dev->data + offset, buffer, nbytes);
    else
        memcpy(buffer, dev->data + offset, nbytes);
}

static void memdisk_request(struct request_queue *q) {
    struct request *req;
    struct memdisk_dev *dev = q->queuedata;

    req = blk_fetch_request(q);
    while (req != NULL) {
        if (req->cmd_type != REQ_TYPE_FS) {
            printk (KERN_NOTICE "Skip non-CMD request/n");
            __blk_end_request_all(req, -EIO);
            continue;
        }
        memdisk_transfer(dev, blk_rq_pos(req), blk_rq_cur_sectors(req),
                req->buffer, rq_data_dir(req));
        if ( ! __blk_end_request_cur(req, 0) ) {
            req = blk_fetch_request(q);
        }
    }
}

/*
 * The HDIO_GETGEO ioctl is handled in blkdev_ioctl(), i
 * calls this. We need to implement getgeo, since we can't
 * use tools such as fdisk to partition the drive otherwise.
 */
int memdisk_getgeo(struct block_device * bd, struct hd_geometry * geo)
{
	long size;

    printk("memdisk_getgeo. \n");

    struct memdisk_dev *dev = bd->bd_disk->private_data;
    size = dev->size *(hardsect_size / KERNEL_SECTOR_SIZE);

    geo->cylinders = (size & ~0x3f) >> 6;
    geo->heads = 4;
    geo->sectors = 16;
    geo->start = 4;

    return 0;
}

/*
 * The device operations structure.
 */
static struct block_device_operations memdisk_ops = {
        .owner  = THIS_MODULE,
        .getgeo = memdisk_getgeo
};

/*
 * Set up our internal device.
 */
static void memdisk_setup_device(struct memdisk_dev *dev, int i)
{
    unsigned char __iomem  *base;
    printk("memdisk_setup_device i:%d. \n", i);

    spin_lock_init(&dev->lock);
    base = ioremap(sprd_memdisks[i].data, sprd_memdisks[i].size);
    if(!base) {
    	printk("memdisk_setup_device ioremap error, dev[%d] data: %x, size: %x\n", i, sprd_memdisks[i].data, sprd_memdisks[i].size);
    	return;
    }

    printk("memdisk_setup_device base: %x, start: %x, size: %x\n",base, sprd_memdisks[i].data, sprd_memdisks[i].size);
    sprd_memdisks[i].data = base;
    sprd_memdisks[i].size = (sprd_memdisks[i].size / hardsect_size);

    dev->queue = blk_init_queue(memdisk_request, &dev->lock);
    if (dev->queue == NULL) {
        printk ("memdisk_setup_device blk_init_queue failure. \n");
        return;
    }

    blk_queue_logical_block_size(dev->queue, hardsect_size);
    dev->queue->queuedata = dev;
    /*
     * And the gendisk structure.
     */
    dev->gd = alloc_disk(MEMDISK_MINORS);
    if (! dev->gd) {
        printk ("memdisk_setup_device alloc_disk failure. \n");
        return;
    }
    dev->gd->major = memdisk_major;
    dev->gd->first_minor = i*MEMDISK_MINORS;
    dev->gd->fops = &memdisk_ops;
    dev->gd->queue = dev->queue;
    dev->gd->private_data = dev;

    snprintf (dev->gd->disk_name, 32, "memdisk.%d", i);
    set_capacity(dev->gd, sprd_memdisks[i].size*(hardsect_size/KERNEL_SECTOR_SIZE));
    add_disk(dev->gd);

    printk("memdisk_setup_device i:%d success.\n", i);
    return;
}

static int __init memdisk_init(void)
{
    int i;

    /*
     * Get registered.
     */
    printk("memdisk_init. \n");
    memdisk_major = register_blkdev(memdisk_major, "memdisk");
    if (memdisk_major <= 0) {
        printk("memdisk: unable to get major number\n");
        return -EBUSY;
    }

    for (i = 0; i < MEMDISKS_COUNT; i++)
         memdisk_setup_device(&sprd_memdisks[i], i);

    printk("memdisk_init finished. \n");
    return 0;
}

static void __exit memdisk_exit(void)
{
	int i;

	printk("memdisk_exit. \n");
	for (i = 0; i < MEMDISKS_COUNT; i++) {
		struct memdisk_dev *dev = &sprd_memdisks[i];

		if (dev->gd) {
			del_gendisk(dev->gd);
			put_disk(dev->gd);
		}
		if (dev->queue) {
		    blk_cleanup_queue(dev->queue);
		}
	}
	unregister_blkdev(memdisk_major, "memdisk");
}

module_init(memdisk_init);
module_exit(memdisk_exit);
