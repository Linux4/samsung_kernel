/*
 *
 *  N-Trig direct event character driver header
 *
 *  Copyright (c) 2011 N-TRIG
 *
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#ifndef _NTRIG_DIRECT_EVENT_DRIVER_H
#define _NTRIG_DIRECT_EVENT_DRIVER_H

/**
 * External Interface API
 */
int bus_init_direct_events(void);
void bus_exit_direct_events(void);

#endif /* _NTRIG_DIRECT_EVENT_DRIVER_H */
