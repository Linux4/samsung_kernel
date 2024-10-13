/*
 * Bump in the Host (BIH) 
 * http://code.google.com/p/bump-in-the-host/source/
 * ----------------------------------------------------------
 *
 *  Copyrighted (C) 2010,2011 by the China Mobile Communications 
 *  Corporation <bih.cmcc@gmail.com>;
 *  See the COPYRIGHT file for full details. 
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *
 */
#ifndef _MAP_H
#define _MAP_H
struct map_node
{
        struct map_node *next;
        struct map_node *prev;
        unsigned int id;
        struct in6_addr in6addr;
        unsigned long start_time;
        unsigned long last_time;
};
int map_init(void);
int map_uninit(void);
int map_timeout_func(void);
struct map_node *map_find(struct in6_addr in6addr);
struct map_node *map_search(unsigned int id);
struct map_node *map_new(unsigned int id);
struct map_node *map_add(unsigned int in4addr,struct in6_addr in6addr);
int map_del(unsigned int id);
int map_free(struct map_node *node);
void map_shownode(struct map_node *node);
int map_show(unsigned int id);
char *map_showrun(void);
int map_timeout_set(unsigned int secs);

#endif
