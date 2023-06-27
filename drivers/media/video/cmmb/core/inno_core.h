
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

#ifndef __INNO_CORE_H_
#define __INNO_CORE_H_
#include <linux/types.h>
//#define MAX_CH  4

#define POWER_ON        0x1
#define POWER_OFF       0x2
#define POWER_IDLE      0x3


struct inno_demod {
        struct kref             kref;
        int                     power_status;
        struct class            cls;
        struct platform_device  *pdev;
        struct device           *dev;
        int                     irq;
        struct workqueue_struct *irq_wq;
        struct work_struct      irq_work;
        
        struct workqueue_struct *req_wq;
        struct work_struct      req_work;
        struct list_head        req_queue;
        spinlock_t              req_lock;
};


int inno_demod_power_status(void);

#endif
