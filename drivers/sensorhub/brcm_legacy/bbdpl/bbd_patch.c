/*
 * Copyright 2014 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation (the "GPL").
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *  
 * A copy of the GPL is available at 
 * http://www.broadcom.com/licenses/GPLv2.php, or by writing to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * The BBD (Broadcom Bridge Driver)
 *
 * tabstop = 8
 */

#include "bbd_internal.h"

static unsigned char bbd_patch[] =
{
#if defined(CONFIG_SENSORS_SSP_LUCKY)
#include "bbd_patch_file_lucky.h"
#elif defined(CONFIG_SENSORS_SSP_NOBLELTE) || defined(CONFIG_SENSORS_SSP_ZENLTE)
#include "bbd_patch_file_noble.h"
#elif defined (CONFIG_SENSORS_SSP_VLTE_W02)
#include "bbd_patch_file_vlte_w02.h"
#elif defined (CONFIG_SENSORS_SSP_VLTE)
#include "bbd_new_patch_file_V.h"
#elif defined (CONFIG_SENSORS_SSP_MARINELTE)
#include "bbd_patch_file.h"
#else  // ZERO
#if ANDROID_VERSION < 70000			/* M OS*/
#include "bbd_new_patch_file_m.h"
#else								/* N OS*/
#include "bbd_new_patch_file.h"
#endif	
#endif
};

void bbd_patch_init_vars(struct bbd_device *dev, int* result)
{
    printk(KERN_INFO "BBD:%s() %u\n", __func__, (u32)sizeof(bbd_patch));

    dev->bbd_ptr[BBD_MINOR_PATCH] = 0;
}

int bbd_patch_uninit(void)
{
    return 0;
}

int bbd_patch_open(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "BBD:%s() %u\n", __func__, (u32)sizeof(bbd_patch));

    if (!gpbbd_dev)
        return -EFAULT;

    filp->private_data = gpbbd_dev;

    return 0;
}

int bbd_patch_release(struct inode *inode, struct file *filp)
{
    return 0;
}

ssize_t bbd_patch_read( struct file *filp,
			char __user *buf,
			size_t size, loff_t *ppos)
{
    ssize_t rd_size = size;
    size_t  offset = filp->f_pos;

    if (offset >= sizeof(bbd_patch)) {       /* signal EOF */
        *ppos = 0;
        return 0;
    }
    if (offset+size > sizeof(bbd_patch))
        rd_size = sizeof(bbd_patch) - offset;
    if (copy_to_user(buf, bbd_patch + offset, rd_size))
        rd_size = -EFAULT;
    else
        *ppos = filp->f_pos + rd_size;

    return rd_size;
}
