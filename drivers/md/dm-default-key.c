/*
 * Copyright (C) 2017 Google, Inc.
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

#include <linux/device-mapper.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/crypto.h>
#include <crypto/diskcipher.h>

#define DM_MSG_PREFIX "default-key"

struct default_key_c {
	struct dm_dev *dev;
	sector_t start;
	struct crypto_diskcipher *diskc;
};

static void default_key_dtr(struct dm_target *ti)
{
	struct default_key_c *dkc = ti->private;

	pr_info("%s: %p\n", __func__, dkc);

	crypto_free_diskcipher(dkc->diskc);
	if (dkc->dev)
		dm_put_device(ti, dkc->dev);
	kzfree(dkc);
}

/*
 * Construct a default-key mapping: <mode> <key> <dev_path> <start>
 */
static int default_key_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	struct default_key_c *dkc;
	size_t key_size;
	unsigned long long tmp;
	char dummy;
	int err;

	pr_info("%s: algo:%s, keysize:%s, path:%s\n", __func__, argv[0], argv[1], argv[2]);
	if (argc != 4) {
		ti->error = "Invalid argument count";
		return -EINVAL;
	}

	dkc = kzalloc(sizeof(*dkc), GFP_KERNEL);
	if (!dkc) {
		ti->error = "Out of memory";
		return -ENOMEM;
	}
	ti->private = dkc;

	if (strcmp(argv[0], "AES-256-XTS") != 0) {
		ti->error = "Unsupported encryption mode";
		err = -EINVAL;
		goto bad;
	}

	dkc->diskc = crypto_alloc_diskcipher("xts(aes)-disk", 0, 0, 0);
	if (IS_ERR(dkc->diskc)) {
		 pr_err("%s: no diskcipher with %s\n", __func__, argv[0]);
		return -EINVAL;
	}
	key_size = strlen(argv[1]) / 2;

	err = crypto_diskcipher_setkey(dkc->diskc, argv[1], key_size, 0, NULL);
	if (err) {
		 pr_err("%s: fails to set diskcipher key:%s, size:%zu\n", __func__, argv[1], key_size);
		return err;
	}

	err = dm_get_device(ti, argv[2], dm_table_get_mode(ti->table),
			    &dkc->dev);
	if (err) {
		ti->error = "Device lookup failed";
		goto bad;
	}

	if (sscanf(argv[3], "%llu%c", &tmp, &dummy) != 1) {
		ti->error = "Invalid start sector";
		err = -EINVAL;
		goto bad;
	}
	dkc->start = tmp;

	/* Pass flush requests through to the underlying device. */
	ti->num_flush_bios = 1;

	/*
	 * We pass discard requests through to the underlying device, although
	 * the discarded blocks will be zeroed, which leaks information about
	 * unused blocks.  It's also impossible for dm-default-key to know not
	 * to decrypt discarded blocks, so they will not be read back as zeroes
	 * and we must set discard_zeroes_data_unsupported.
	 */
	ti->num_discard_bios = 1;

	pr_info("%s: success with cipher_algo:%s, cipher_drv:%s\n", __func__,
		crypto_diskcipher_alg(dkc->diskc)->base.cra_name,
		crypto_diskcipher_alg(dkc->diskc)->base.cra_driver_name);

	return 0;

bad:
	default_key_dtr(ti);
	return -EINVAL;
}

static int default_key_map(struct dm_target *ti, struct bio *bio)
{
	const struct default_key_c *dkc = ti->private;

	bio_set_dev(bio, dkc->dev->bdev);

	if (bio_sectors(bio))
		bio->bi_iter.bi_sector = dkc->start +
			dm_target_offset(ti, bio->bi_iter.bi_sector);

	if (dkc->diskc && (bio_has_crypt(bio) == NULL) && !bio->bi_crypt_skip)
		crypto_diskcipher_set(bio, dkc->diskc, 0);

	return DM_MAPIO_REMAPPED;
}

static void default_key_status(struct dm_target *ti, status_type_t type,
			       unsigned int status_flags, char *result,
			       unsigned int maxlen)
{
	const struct default_key_c *dkc = ti->private;
	unsigned int sz = 0;

	switch (type) {
	case STATUSTYPE_INFO:
		result[0] = '\0';
		break;

	case STATUSTYPE_TABLE:

		/* encryption mode */
		DMEMIT("AES-256-XTS");

		/* reserved for key; dm-crypt shows it, but we don't for now */
		DMEMIT(" -");

		/* name of underlying device, and the start sector in it */
		DMEMIT(" %s %llu", dkc->dev->name,
		       (unsigned long long)dkc->start);
		break;
	}
}

static int default_key_prepare_ioctl(struct dm_target *ti,
				     struct block_device **bdev)
{
	struct default_key_c *dkc = ti->private;
	struct dm_dev *dev = dkc->dev;

	*bdev = dev->bdev;

	/*
	 * Only pass ioctls through if the device sizes match exactly.
	 */
	if (dkc->start ||
	    ti->len != i_size_read(dev->bdev->bd_inode) >> SECTOR_SHIFT)
		return 1;
	return 0;
}

static int default_key_iterate_devices(struct dm_target *ti,
				       iterate_devices_callout_fn fn,
				       void *data)
{
	struct default_key_c *dkc = ti->private;

	return fn(ti, dkc->dev, dkc->start, ti->len, data);
}

static struct target_type default_key_target = {
	.name   = "default-key",
	.version = {1, 0, 0},
	.module = THIS_MODULE,
	.ctr    = default_key_ctr,
	.dtr    = default_key_dtr,
	.map    = default_key_map,
	.status = default_key_status,
	.prepare_ioctl = default_key_prepare_ioctl,
	.iterate_devices = default_key_iterate_devices,
};

static int __init dm_default_key_init(void)
{
	return dm_register_target(&default_key_target);
}

static void __exit dm_default_key_exit(void)
{
	dm_unregister_target(&default_key_target);
}

module_init(dm_default_key_init);
module_exit(dm_default_key_exit);

MODULE_AUTHOR("Paul Lawrence <paullawrence@google.com>");
MODULE_AUTHOR("Paul Crowley <paulcrowley@google.com>");
MODULE_AUTHOR("Eric Biggers <ebiggers@google.com>");
MODULE_DESCRIPTION(DM_NAME " target for encrypting filesystem metadata");
MODULE_LICENSE("GPL");
