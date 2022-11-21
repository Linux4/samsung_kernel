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
#ifndef _VOSLIST_H
#define _VOSLIST_H
enum _listnode_status
{
        LISTNODE_STATUS_ENABLE,
        LISTNODE_STATUS_DISABLE
};

struct listnode
{
        struct listnode *next;
        struct listnode *prev;
        unsigned int key;
        char status;
        void *data;
        int datalen;
        unsigned int datasp;
};

#define SHOW2BUF_DEF        \
        static char buff[1000],*p = NULL; \
        int ret,len,size; \

#define SHOW2BUF_INIT()        \
do \
{        \
        ret=0; \
        if(p&&p!=buff) \
                vfree(p); \
        size=sizeof(buff)-1; \
        len=0; \
        buff[0]=0; \
        p=buff; \
} \
while(0)

#define SHOW2BUF_RET return(p)

#define SHOW2BUF(format,...) \
do \
{ \
        ret=snprintf(p+len,size-len,format,##__VA_ARGS__); \
        if (ret < 0 || ret >= (size-len)) \
        { \
                while (1) \
                { \
                        p=vrealloc(p,2*size+1,size,p!=buff); \
                        if(!p) \
                        { \
                                lprintf(0,"vrealloc error\n"); \
                                break; \
                        } \
                        size*=2; \
                        ret=snprintf(p+len,size-len,format,##__VA_ARGS__); \
                        if (ret > -1 && ret < (size-len)) \
                                break; \
                } \
        } \
        len+=ret; \
        if(p) \
                *(p+len)=0; \
} \
while(0) 

#define lprintf(level,format,...) \
do \
{ \
        if(level==0) \
                printk("%s(%d)%s: "format,__FILE__,__LINE__,(char *)__FUNCTION__,##__VA_ARGS__ ); \
        else \
                printk((format),##__VA_ARGS__ ); \
} \
while(0)
#define vmalloc(size) kmalloc((size),GFP_KERNEL)
#define vfree kfree 

#define PKG_GET_STRING(val, cp) \
do \
{ \
        while(*(cp)) \
                *(val)++=*(cp)++; \
        *(val)++=0; \
} \
while(0)

#define        PKG_GET_BYTE(val, cp)        ((val) = *(cp)++)

#define        PKG_GET_SHORT(val, cp) \
do \
{ \
        unsigned short Xv; \
        Xv = (*(cp)++) << 8; \
        Xv |= *(cp)++; \
        (val) = Xv; \
} \
while(0)

#define        PKG_GET_NETSHORT(val, cp) \
do \
{ \
        unsigned char *Xvp; \
        unsigned short Xv; \
        Xvp = (unsigned char *) &Xv; \
        *Xvp++ = *(cp)++; \
        *Xvp++ = *(cp)++; \
        (val) = Xv; \
} \
while(0)

#define        PKG_GET_LONG(val, cp) \
do \
{ \
        unsigned long Xv; \
        Xv = (*(cp)++) << 24; \
        Xv |= (*(cp)++) << 16; \
        Xv |= (*(cp)++) << 8; \
        Xv |= *(cp)++; \
        (val) = Xv; \
} \
while(0)

#define        PKG_GET_NETLONG(val, cp) \
do \
{ \
        unsigned char *Xvp; \
        unsigned long Xv; \
        Xvp = (unsigned char *) &Xv; \
        *Xvp++ = *(cp)++; \
        *Xvp++ = *(cp)++; \
        *Xvp++ = *(cp)++; \
        *Xvp++ = *(cp)++; \
        (val) = Xv; \
} \
while(0)

#define PKG_PUT_BUF(val, len, cp) \
do \
{ \
        memcpy((cp),(val),(len)); \
        (cp)+=(len); \
} \
while(0)

#define PKG_PUT_STRING(val, cp) \
do \
{ \
        while(*(val)) \
                *(cp)++=*(val)++; \
        *(cp)++=0; \
} \
while(0)

#define        PKG_PUT_BYTE(val, cp)         (*(cp)++ = (unsigned char)(val))

#define        PKG_PUT_SHORT(val, cp) \
do \
{ \
        unsigned short Xv; \
        Xv = (unsigned short)(val); \
        *(cp)++ = (unsigned char)(Xv >> 8); \
        *(cp)++ = (unsigned char)Xv; \
} \
while(0)

#define        PKG_PUT_NETSHORT(val, cp) \
do \
{ \
        unsigned char *Xvp; \
        unsigned short Xv = (unsigned short)(val); \
        Xvp = (unsigned char *)&Xv; \
        *(cp)++ = *Xvp++; \
        *(cp)++ = *Xvp++; \
} \
while(0)

#define        PKG_PUT_LONG(val, cp) \
do \
{ \
        unsigned long Xv; \
        Xv = (unsigned long)(val); \
        *(cp)++ = (unsigned char)(Xv >> 24); \
        *(cp)++ = (unsigned char)(Xv >> 16); \
        *(cp)++ = (unsigned char)(Xv >>  8); \
        *(cp)++ = (unsigned char)Xv; \
} \
while(0)

#define        PKG_PUT_NETLONG(val, cp) \
do \
{ \
        unsigned char *Xvp; \
        unsigned long Xv = (unsigned long)(val); \
        Xvp = (unsigned char *)&Xv; \
        *(cp)++ = *Xvp++; \
        *(cp)++ = *Xvp++; \
        *(cp)++ = *Xvp++; \
        *(cp)++ = *Xvp++; \
} \
while(0)

int inet_pton4(char *src,unsigned char *dst);
int inet_pton6(char *src,unsigned char *dst);
char *inet_ntop4(unsigned char *src, char *dst, size_t size);
char *inet_ntop6(unsigned char *src, char *dst, size_t size);
unsigned int sk_inet_addr(char* ip_str);
char *buff2String(char *buff,int bufflen);
int msg_field2(char *linebuf,int linelen,int num,char **field,int *flen,char gech);
void *vrealloc(void *ptr,int newsize,int oldsize,char freeflag);
int listnode_tj(struct listnode *head);
void *listnode_pop(struct listnode **head);
int listnode_free(struct listnode *node,void (*func)(void *));
int listnode_clear(struct listnode **head,void (*func)(void *));
void listnode_show(struct listnode *head);
struct listnode *listnode_find(struct listnode *head,unsigned int key);
struct listnode *listnode_search(struct listnode *head,void *data,int len,int (*func)(void *,void *,int));
struct listnode *listnode_match(struct listnode *head,void *data,int len,int (*func)(void *,void *,int));
int listnode_insert(struct listnode **head,struct listnode *node);
struct listnode *listnode_uninsert(struct listnode **head,struct listnode *node);
struct listnode *listnode_uninsert2(struct listnode **head,unsigned int key);
struct listnode *listnode_add(struct listnode **head,unsigned int key,void *data,int datalen);
int listnode_del(struct listnode **head,unsigned int key,void (*func)(void *));
struct listnode *listnode_tail(struct listnode **head,void *data,int datalen);
int listnode_join(struct listnode **head,struct listnode **list);

#endif
