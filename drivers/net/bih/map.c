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
#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/timer.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif
#include "voslist.h"
#include "map.h"
#define TIMEOUTSEC 60
#define MAP_ARRAY_NUM 13
#define NIPQUAD(addr) \
        ((unsigned char *)&addr)[0], \
        ((unsigned char *)&addr)[1], \
        ((unsigned char *)&addr)[2], \
        ((unsigned char *)&addr)[3]

#define BIH_DEBUG printk
struct map_node *map_array[MAP_ARRAY_NUM];
struct timer_list map_timer;
unsigned long map_timeout=1800;
DEFINE_SPINLOCK(map_lock);

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

/*
        Initiate the map-node, and set a timer for the node;
        when the node is expired, call map_timeout_func().
*/
int map_init(void)
{
        init_timer(&map_timer);
        map_timer.data = 0;
        map_timer.function = (void (*)(unsigned long))map_timeout_func;
        map_timer.expires = jiffies + TIMEOUTSEC*HZ;
        add_timer(&map_timer);
        return(0);
}

/*
        Delete the timer named map_timer, and destroy the timer's mapping relationship.
*/
int map_uninit(void)
{
        del_timer(&map_timer);
        map_free(NULL);
        return(0);
}

/*
        Check the map_array[] array, and call map_free() to delete the node which is expired(30 minutes).
*/
int map_timeout_func(void)
{
        int i;
        unsigned long now;
        struct map_node *node,*delnode;

        now=jiffies;
        map_timer.expires = jiffies + TIMEOUTSEC*HZ;
        add_timer(&map_timer);
//        spin_lock_bh(&map_lock);
        for(i=0;i<MAP_ARRAY_NUM;i++)
        {
                for(node=map_array[i];node;)
                {
                        delnode=node;
                        node=node->next;
                        if(time_after(now,delnode->last_time+HZ*map_timeout))
                        {
                                map_free(delnode);
                        }
                }
        }
//        spin_unlock_bh(&map_lock);
        return(0);
}

/*
        Check the node in the whole map_array[] array with 'in6_addr',
        if there is an appropriate result, return the node,
        otherwise, return NULL.
*/
struct map_node *map_find(struct in6_addr in6addr)
{
        int i;
        struct map_node *node;

        for(i=0;i<MAP_ARRAY_NUM;i++)
        {
                for(node=map_array[i];node;node=node->next)
                {
                        if(memcmp(&in6addr,&(node->in6addr),sizeof(struct in6_addr))==0)
                        {
                                node->last_time=jiffies;
                                return(node);
                        }
                }
        }
        return(NULL);
}

/*
        Check the linked list with 'id',
        if there is an appropriate result, set the 'last_time' as jiffies, and return the node.
*/
struct map_node *map_search(unsigned int id)
{
        struct map_node *node;
        node=(struct map_node *)listnode_find((struct listnode *)map_array[id%MAP_ARRAY_NUM],id);
        if(node)
                node->last_time=jiffies;
        return(node);
}

/*
        Insert a map_node into the map with a certain id,
        if there is a node with this id already exists in the map, set the node's 'last_time' as jiffies and return the node,
        otherwise, make a new map_node, and insert this new node into the linked list.
*/
struct map_node *map_new(unsigned int id)
{
        struct map_node *node;

        if(id==0)
        {
                BIH_DEBUG("bih: map_new argument error\n");
                lprintf(0,"argument error\n");
                return(NULL);
        }
        node=map_search(id);
        if(node)
        {
                return(node);
        }
        node=(struct map_node *)vmalloc(sizeof(struct map_node));
        if(node==NULL)
        {
                BIH_DEBUG("bih: map_new vmalloc error\n");
                lprintf(0,"vmalloc error\n");
                return(NULL);
        }
        memset(node,0,sizeof(struct map_node));
        node->id=id;
        node->start_time=jiffies;
        node->last_time=node->start_time;
        spin_lock_bh(&map_lock);
        if(listnode_insert((struct listnode **)&(map_array[id%MAP_ARRAY_NUM]),(struct listnode *)node))
        {
                spin_unlock_bh(&map_lock);
                vfree(node);
                BIH_DEBUG("bih: map_new listnode_insert error\n");
                lprintf(0,"listnode_insert error\n");
                return(NULL);
        }
        spin_unlock_bh(&map_lock);
        return(node);
}

/*
        Insert or upgrade the IPv6 address,
        find the node with 'id' or 'key', and then make a new mapping relationship.
*/
struct map_node *map_add(unsigned int in4addr,struct in6_addr in6addr)
{
        struct map_node *node;
        if(in4addr==0)
        {
                BIH_DEBUG("bih: map_add argument error\n");
                lprintf(0,"argument error\n");
                return(NULL);
        }
        node=map_search(in4addr);
        if(node==NULL)
                node=map_new(in4addr);
        memcpy(&(node->in6addr), &in6addr, sizeof(struct in6_addr));
        node->last_time=jiffies;
        return(node);
}

/*
        Call the map_free() to delete the certain node with 'id',
        or, clear the whole map_array[].
*/
int map_del(unsigned int id)
{
        struct map_node *node;
        node=map_search(id);
        if(node)
        {
                map_free(node);
        }
        return(0);
}

/*
        Called by the map_del().
*/
int map_free(struct map_node *node)
{
        unsigned long now;
        char buff[64];
        now=jiffies;
        if(node==NULL)
        {
                int i;
                struct map_node *delnode;
                for(i=0;i<MAP_ARRAY_NUM;i++)
                {
                        for(node=map_array[i];node;)
                        {
                                delnode=node;
                                node=node->next;
                                map_free(delnode);
                        }
                }
        }
        else
        {
                inet_ntop6((unsigned char *)&(node->in6addr), buff, sizeof(buff)-1);
                lprintf(1,"MAP: (%u.%u.%u.%u-%s) timeout: used(%lu) unused(%lu)\n",
                        NIPQUAD(node->id),buff,(now-node->start_time)/HZ,(now-node->last_time)/HZ);
                spin_lock_bh(&map_lock);
                listnode_uninsert((struct listnode **)&(map_array[node->id%MAP_ARRAY_NUM]),(struct listnode *)node);
                vfree(node);
                spin_unlock_bh(&map_lock);
        }
        return(0);
}

/*
        Called by the map_show() and print the information of a certain map_node.
*/
void map_shownode(struct map_node *node)
{
        unsigned long now;
        char buff[64];
        if(node==NULL)
                return;
        now=jiffies;
        inet_ntop6((unsigned char *)&(node->in6addr), buff, sizeof(buff)-1);
        BIH_DEBUG("bih: %u.%u.%u.%u-%s : used(%lu) unused(%lu)\n",
                NIPQUAD(node->id),buff,(now-node->start_time)/HZ,(now-node->last_time)/HZ);
        lprintf(5,"%u.%u.%u.%u-%s : used(%lu) unused(%lu)\n",
                NIPQUAD(node->id),buff,(now-node->start_time)/HZ,(now-node->last_time)/HZ);
        return;
}

/*
        Print the information of a certain node or all the nodes.
*/
int map_show(unsigned int id)
{
        struct map_node *node;
        if(id==0)
        {
                int i;
                lprintf(5,"==================== MAP TABLE ====================\n");
                for(i=0;i<MAP_ARRAY_NUM;i++)
                {
                        for(node=map_array[i];node;node=node->next)
                        {
                                map_shownode(node);
                        }
                }
        }
        else
        {
                node=map_search(id);
                if(node==NULL)
                {
                        lprintf(0,"id(%u.%u.%u.%u) error\n",NIPQUAD(id));
                        return(-1);
                }
                map_shownode(node);
        }
        return(0);
}

/*
        Print the infomation of mapping-address,
        such as the time used and left.
*/
char *map_showrun(void)
{
        int i;
        struct map_node *node;
        unsigned long now=jiffies;
        char strbuff[64];
        SHOW2BUF_DEF;
        SHOW2BUF_INIT();
        spin_lock_bh(&map_lock);
        for(i=0;i<MAP_ARRAY_NUM;i++)
        {
                for(node=map_array[i];node;node=node->next)
                {
                        inet_ntop6((unsigned char *)&(node->in6addr), strbuff, sizeof(strbuff)-1);
                        SHOW2BUF("[%d]: %u.%u.%u.%u-%s : used(%lu) unused(%lu)\n",
                                i,NIPQUAD(node->id),strbuff,(now-node->start_time)/HZ,(now-node->last_time)/HZ);
                }
        }
        spin_unlock_bh(&map_lock);
        SHOW2BUF_RET;
}

int map_timeout_set(unsigned int secs)
{
        map_timeout=secs;
        return(0);
}
