
/*
 * Copyright (C) 2010 Innofidei Corporation
 * Author:      sean <zhaoguangyu@innofidei.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 */

#ifndef __INNO_DEMOD_H_
#define __INNO_DEMOD_H_

#define LG_PREFIX "innolg"
#define UAM_PREFIX "innouam"

//#define IF206
#define IF228


#define LGX_DEV_NUM     5
#define UAM_BIT_SHIFT 	(LGX_DEV_NUM)
#define CA_BIT_SHIFT  	(LGX_DEV_NUM+1)
#define LGX_BIT_CNT   	(LGX_DEV_NUM+2)


struct inno_lgx_info {
        char name[20];
        unsigned char id;       
        void *priv; 
};

struct inno_lgx_platform_info {
        struct inno_lgx_info *info;
        unsigned        num_info;
};

/**
 * lgx_device - 
 * @id :logic channel id 
 * @dev:device
 * @buf:buf use to receive data
 * @valid:len for valid data
 * @priv:
 * @name:device name
 */
struct lgx_device {
        int             id;
        struct device   dev;
        void            *buf;
        unsigned int    buf_len;
        unsigned int    valid;
        void            *priv;
        char            *name;
	unsigned long   irq_jif;
};

/**
 * lgx_driver -
 * @probe:
 * @remove:
 * @shutdown:
 * @suspend:
 * @resume:
 * @data_notify: will be called by _inno_core.c if data is ready for it to fetch
 * @driver:driver
 */
struct lgx_driver {
        int     major;
        int (*probe)(struct lgx_device *);
        int (*remove)(struct lgx_device *);
        void (*shutdown)(struct lgx_device *);
        int (*suspend)(struct lgx_device *, pm_message_t state);
        int (*resume)(struct lgx_device *);
        void (*data_notify)(struct lgx_device *);
        struct device_driver driver;
};

/**
 * inno_req -
 * @queue: used by _inno_core.c, all req will be added to demod reqs queue if need
 * @handler:req handler, must not call inno_req_sync on this context, or it will be in dead lock
 * @complete:
 * @done:
 * @result: used by inno_req_sync to verify if req is successed
 *
 * use inno_req_sync/inno_req_async to add inno_req to demod req queue,
 * when demod req workqueue is scheduled, it will process all reqs on demod req queue
 * for each req ,demod req workqueue will call it's handler to do real job
 * after return from req handler, demod req workqueue will call req complete
 */
struct inno_req {
        struct list_head        queue;
        int                     (*handler)(void *context);
        int                    (*complete)(struct inno_req *req);
        void                    *context;
        void                    *done;
        int                     result;
	unsigned long		irq_jif;
};

extern int inno_req_async(struct inno_req *req);
extern int inno_req_sync(struct inno_req *req);
extern struct lgx_device *inno_lgx_new_device(char *name, int major, int id, void *data);
extern void inno_lgx_device_unregister(struct lgx_device *dev);
extern void inno_lgx_driver_unregister(struct lgx_driver *drv);
extern int inno_lgx_driver_register(struct lgx_driver *drv);

extern int inno_demod_inc_ref(void);
extern void inno_demod_dec_ref(void);
#endif
