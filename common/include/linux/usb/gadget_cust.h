/*
 *	gadget_dbg.h  --  USB Androdid Gadget Debugging
 *
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 */


#ifndef GADGET_DBG
#define GADGET_DBG

#define NPRINTK(x...)		do {									\
								printk("USBD][%s] ", __func__);		\
								printk(x);						\
							} while(0)

#endif // GADGET_DBG
