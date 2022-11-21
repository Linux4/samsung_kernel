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
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif
#include "voslist.h"
#include "map.h"
#define NIPQUAD(addr) \
        ((unsigned char *)&addr)[0], \
        ((unsigned char *)&addr)[1], \
        ((unsigned char *)&addr)[2], \
        ((unsigned char *)&addr)[3]
struct pool_node
{
        struct listnode *addr;
        struct listnode *currnode;
        unsigned int curraddr;
}ADDRPOOL;

int pool_addr_add(unsigned int first,unsigned int last);
int pool_addr_add2(char *string);
int pool_addr_del(unsigned int first,unsigned int last);
int pool_addr_del2(char *string);
char *pool_addr_show(void);
unsigned int pool_addr_assign(struct in6_addr *in6addr);

/*
        Insert an address block(from 'first' to 'last') into the address pool.
*/
int pool_addr_add(unsigned int first,unsigned int last)
{
        struct pool_node *node=&ADDRPOOL;
        struct listnode *listnode;

        if(ntohl(first)>ntohl(last))
        {
                lprintf(0,"argument error\n");
                return(-1);
        }
        listnode=listnode_find(node->addr,first);
        if(listnode)
        {
                listnode->data=(void *)last;
                return(0);
        }
        listnode=listnode_add(&(node->addr),first,(void *)last,0);
        if(listnode==NULL)
        {
                lprintf(0,"listnode_add error \n");
                return(-1);
        }
        return(0);
}

/*
        Insert an address block into the address pool, not numbers but characters.
*/
int pool_addr_add2(char *string)
{
        unsigned int first,last;
        char *pstr,*firststr,*laststr;
        int ret,slen,num=0,pstrlen,firststrlen,laststrlen;

        if(string==NULL)
        {
                lprintf(0,"arguments error\n");
                return(-1);
        }
        slen=strlen(string);
        if(string[slen-1]=='\n')
        {
                slen--;
                string[slen]=0;
        }
        if(string[slen-1]=='\r')
        {
                slen--;
                string[slen]=0;
        }
        ret=0;
        while(msg_field2(string,slen,num++,&pstr,&pstrlen,',')==0)
        {
                if(msg_field2(pstr,pstrlen,0,&firststr,&firststrlen,'-')==0&&
                        msg_field2(pstr,pstrlen,1,&laststr,&laststrlen,'-')==0)
                {
                        first=sk_inet_addr(buff2String(firststr,firststrlen));
                        last=sk_inet_addr(buff2String(laststr,laststrlen));
                        ret=pool_addr_add(first,last);
                        if(ret!=0)
                                lprintf(0,"\"%.*s\" error\n",pstrlen,pstr);
                }
                else
                        lprintf(0,"addr(%.*s) error\n",pstrlen,pstr);
        }
        return(0);
}

/*
        Delete the address block from 'first' to 'last'.
*/
int pool_addr_del(unsigned int first,unsigned int last)
{
        struct pool_node *node=&ADDRPOOL;
        struct listnode *listnode;

        if(ntohl(first)>ntohl(last))
        {
                lprintf(0,"argument error\n");
                return(-1);
        }
        listnode=listnode_find(node->addr,first);
        if(listnode&&listnode->data==(void *)last)
        {
                listnode_del(&(node->addr),first,NULL);
                return(0);
        }
        lprintf(0,"\"%u.%u.%u.%u-%u.%u.%u.%u\" is not exist\n",NIPQUAD(first),NIPQUAD(last));
        return(-1);
}

/*
        Delete the address block.
*/
int pool_addr_del2(char *string)
{
        unsigned int first,last;
        char *pstr,*firststr,*laststr;
        int ret,slen,num=0,pstrlen,firststrlen,laststrlen;

        if(string==NULL)
        {
                lprintf(0,"arguments error\n");
                return(-1);
        }
        slen=strlen(string);
        if(string[slen-1]=='\n')
        {
                slen--;
                string[slen]=0;
        }
        if(string[slen-1]=='\r')
        {
                slen--;
                string[slen]=0;
        }
        ret=0;
        while(msg_field2(string,slen,num++,&pstr,&pstrlen,',')==0)
        {
                if(msg_field2(pstr,pstrlen,0,&firststr,&firststrlen,'-')==0&&
                        msg_field2(pstr,pstrlen,1,&laststr,&laststrlen,'-')==0)
                {
                        first=sk_inet_addr(buff2String(firststr,firststrlen));
                        last=sk_inet_addr(buff2String(laststr,laststrlen));
                        ret=pool_addr_del(first,last);
                        if(ret!=0)
                                lprintf(0,"\"%.*s\" error\n",pstrlen,pstr);
                }
                else
                        lprintf(0,"addr(%.*s) error\n",pstrlen,pstr);
        }
        return(0);
}

/*
        Print the information of the address block.
*/
char *pool_addr_show(void)
{
        struct listnode *node,*head=ADDRPOOL.addr;
        unsigned int first,last;
        SHOW2BUF_DEF;
        SHOW2BUF_INIT();
        node=head;
        while(node)
        {
                first=node->key;
                last=(unsigned int)(node->data);
                if(node!=head)
                        SHOW2BUF(",");
                SHOW2BUF("%u.%u.%u.%u-%u.%u.%u.%u %u.%u.%u.%u",NIPQUAD(first),NIPQUAD(last),NIPQUAD(ADDRPOOL.curraddr));
                node=node->next;
        }
        SHOW2BUF_RET;
}

/*
        If there is a new IPv6 address arrival:
        1. If this IPv6 address already exists in the mapping relationship, return the IPv4 address;
        2. Otherwise, check the address block to find a free IPv4 address to make a mapping relationship with this IPv6 address;
        3. If there is no free IPv4 address, then check the next address block, and repeat step 2;
        4. If there is no free IPv4 address in all the address blocks, start from the first block and repeate step 2;
        5. If there is no free IPv4 address in the address pool, print the error message 'There is on free ip address'.
*/
unsigned int pool_addr_assign(struct in6_addr *in6addr)
{
        struct pool_node *node=&ADDRPOOL;
        struct listnode *list_node;
        unsigned int addr,first_addr,last_addr;
        struct map_node *mapnode;

        if((node==NULL)||(node->addr==NULL))
        {
                lprintf(0,"argument error\n");
                return(0);
        }
        if(node->currnode==NULL)
                node->currnode=node->addr;
        list_node=node->currnode;
        if(in6addr)
        {
                mapnode=map_find(*in6addr);
                if(mapnode)
                        return(mapnode->id);
        }
        do
        {
                first_addr=list_node->key;
                last_addr=(unsigned int)(list_node->data);
                if(node->curraddr==0)
                {
                        node->curraddr=first_addr;
                        addr=node->curraddr;
                }
                else
                {
                        addr=htonl(ntohl(node->curraddr)+1);
                }
                do
                {
                        if(ntohl(addr)>ntohl(last_addr))
                                addr=first_addr;
                        if(map_search(addr)==NULL)
                        {
                                node->currnode=list_node;
                                node->curraddr=addr;
                                if(in6addr)
                                        map_add(addr,*in6addr);
                                return(addr);
                        }
                        else
                        {
                                addr=htonl(ntohl(addr)+1);
                        }
                }
                while(addr!=node->curraddr);
                list_node=list_node->next;
                if(list_node==NULL)
                        list_node=node->addr;
        }
        while(list_node!=node->currnode);
        lprintf(0,"There is no free ip address\n");
        return(0);
}
