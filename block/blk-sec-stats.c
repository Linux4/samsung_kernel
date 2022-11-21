// SPDX-License-Identifier: GPL-2.0

#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/genhd.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/time.h>

struct gendisk *internal_disk;

struct accumulated_stats {
	struct timespec uptime;
	unsigned long sectors[3];       /* READ, WRITE, DISCARD */
	unsigned long ios[3];
	unsigned long iot;
};
struct accumulated_stats old, new;

static inline void get_monotonic_boottime(struct timespec *ts)
{
        *ts = ktime_to_timespec(ktime_get_boottime());
}

struct gendisk *get_internal_gendisk()
{
	dev_t dev;
	struct gendisk *gd;
	struct block_device *bdev;

	if (internal_disk)
		return internal_disk;

	/* Assume that internal storage is not removed */
	dev = blk_lookup_devt("sda", 0);
	if (!dev)
		dev = blk_lookup_devt("mmcblk0", 0);

	bdev = blkdev_get_by_dev(dev, FMODE_WRITE|FMODE_READ, NULL);
	if (IS_ERR(bdev)) {
		pr_err("%s: No device detected.\n", __func__);
		return 0;
	}

	gd = bdev->bd_disk;
	if (IS_ERR(internal_disk)) {
		pr_err("%s: For an unknown reason, gendisk lost.\n", __func__);
		return 0;
	}
	
	return gd;
}

#define UNSIGNED_DIFF(n, o) (((n) >= (o)) ? ((n) - (o)) : ((n) + (0 - (o))))
#define SECTORS2KB(x) ((x) / 2)

static ssize_t diskios_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int ret;
	struct hd_struct *hd;
	long hours;

	if (!internal_disk)
		internal_disk = get_internal_gendisk();

	if (!internal_disk) {
		pr_err("%s: Internal gendisk ptr error.\n", __func__);
		return -1;
	}

	hd = &internal_disk->part0;

	new.ios[STAT_READ] = part_stat_read(hd, ios[STAT_READ]);
	new.ios[STAT_WRITE] = part_stat_read(hd, ios[STAT_WRITE]);
	new.ios[STAT_DISCARD] = part_stat_read(hd, ios[STAT_DISCARD]);
	new.sectors[STAT_READ] = part_stat_read(hd, sectors[STAT_READ]);
	new.sectors[STAT_WRITE] = part_stat_read(hd, sectors[STAT_WRITE]);
	new.sectors[STAT_DISCARD] = part_stat_read(hd, sectors[STAT_DISCARD]);
	new.iot = jiffies_to_msecs(part_stat_read(hd, io_ticks)) / 1000;

	get_monotonic_boottime(&(new.uptime));
	hours = (new.uptime.tv_sec - old.uptime.tv_sec) / 60; 
	hours = (hours + 30) / 60;

	ret = sprintf(buf, "\"ReadC\":\"%lu\",\"ReadKB\":\"%lu\","
			"\"WriteC\":\"%lu\",\"WriteKB\":\"%lu\","
			"\"DiscardC\":\"%lu\",\"DiscardKB\":\"%lu\","
			"\"IOT\":\"%lu\","
			"\"Hours\":\"%ld\"\n",
			UNSIGNED_DIFF(new.ios[STAT_READ], old.ios[STAT_READ]),
			SECTORS2KB(UNSIGNED_DIFF(new.sectors[STAT_READ], old.sectors[STAT_READ])),
			UNSIGNED_DIFF(new.ios[STAT_WRITE], old.ios[STAT_WRITE]),
			SECTORS2KB(UNSIGNED_DIFF(new.sectors[STAT_WRITE], old.sectors[STAT_WRITE])),
			UNSIGNED_DIFF(new.ios[STAT_DISCARD], old.ios[STAT_DISCARD]),
			SECTORS2KB(UNSIGNED_DIFF(new.sectors[STAT_DISCARD], old.sectors[STAT_DISCARD])),
			UNSIGNED_DIFF(new.iot, old.iot),
			hours);

	old.ios[STAT_READ] = new.ios[STAT_READ];
	old.ios[STAT_WRITE] = new.ios[STAT_WRITE];
	old.ios[STAT_DISCARD] = new.ios[STAT_DISCARD];
	old.sectors[STAT_READ] = new.sectors[STAT_READ];
	old.sectors[STAT_WRITE] = new.sectors[STAT_WRITE];
	old.sectors[STAT_DISCARD] = new.sectors[STAT_DISCARD];
	old.uptime = new.uptime;
	old.iot = new.iot;

	return ret;
}

static struct kobj_attribute diskios_attr = __ATTR(diskios, 0444, diskios_show,  NULL);

static struct attribute *blk_sec_stats_attrs[] = {
	&diskios_attr.attr,
	NULL,
};

static struct attribute_group blk_sec_stats_group = {
	.attrs = blk_sec_stats_attrs,
	NULL,
};

static struct kobject *blk_sec_stats_kobj;

static int __init blk_sec_stats_init(void)
{
	int retval;

	blk_sec_stats_kobj = kobject_create_and_add("blk_sec_stats", kernel_kobj);
	if (!blk_sec_stats_kobj)
		return -ENOMEM;

	retval = sysfs_create_group(blk_sec_stats_kobj, &blk_sec_stats_group);
	if (retval)
		kobject_put(blk_sec_stats_kobj);

	return 0;
}

static void __exit blk_sec_stats_exit(void)
{
	kobject_put(blk_sec_stats_kobj);
}

module_init(blk_sec_stats_init);
module_exit(blk_sec_stats_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Manjong Lee <mj0123.lee@samsung.com>");
MODULE_DESCRIPTION("SEC Storage stats module for various purposes");
MODULE_VERSION("0.1");
