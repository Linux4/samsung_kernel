/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
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

#ifndef _SIMPLE_EVENTS_H_
#define _SIMPLE_EVENTS_H_

#include <linux/wait.h>



struct __simple_event {
	spinlock_t lock;
	wait_queue_head_t wq;
        unsigned int flags;
        
};

typedef struct __simple_event simple_event_t;


static inline void simple_evnet_init(simple_event_t* evt)
{
        spin_lock_init(&evt->lock);
        init_waitqueue_head(&evt->wq);
        evt->flags = 0;
}

static inline int simple_evnet_get(simple_event_t* evt, unsigned int req_flags,
        unsigned int *actual_flags)
{
        unsigned long flags;
                
        if(!evt) return -1;

        *actual_flags = 0;

        while(!(*actual_flags)) {
                wait_event(evt->wq, (evt->flags & req_flags));
                  
        	spin_lock_irqsave(&evt->lock, flags);
                *actual_flags = evt->flags & req_flags;
                evt->flags &= ~req_flags;
        	spin_unlock_irqrestore(&evt->lock, flags);
        }
        
        return 0;
}


static inline int simple_evnet_set(simple_event_t* evt, unsigned int flags, 
        int opt_and)
{
        unsigned long irq_flags;
        int need_wake = 0;
        
        if(!evt) return -1;

        spin_lock_irqsave(&evt->lock, irq_flags);
        if(!opt_and) {
                evt->flags |= flags;
        } else {
                evt->flags &= flags;
        }

        if(evt->flags) {
                need_wake = 1;
        }
	spin_unlock_irqrestore(&evt->lock, irq_flags);

        if(need_wake) {
                wake_up(&evt->wq);
        }
        
        return 0;
}


#endif /* !_SIMPLE_EVENTS_H_ */

