/*
 * f_simmian.h -- interface to USB gadget "simmian link" function
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __F_SIMMIAN_H
#define __F_SIMMIAN_H

#define SIMMIAN_CTRL_MAXPACKET 0x8000

struct ep0_rbuf{
    int count;
    u8 rbuf[SIMMIAN_CTRL_MAXPACKET];
};

#define SIMMIAN_EP0_MAXPACKET _IOR('S', 0x01, int)
#define SIMMIAN_EP0_DATA_STAGE _IOR('S', 0x02, int)

#endif /* __F_SIMMIAN_H */
