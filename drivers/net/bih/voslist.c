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

int inet_pton4(char *src,unsigned char *dst) /*The "dotted decimal" -> "integer"*/
{
        static const char digits[] = "0123456789";
        int saw_digit, octets, ch;
        unsigned char tmp[4], *tp;

        saw_digit = 0;
        octets = 0;
        *(tp = tmp) = 0;
        while ((ch = *src++) != '\0') {
                const char *pch;

                if ((pch = strchr(digits, ch)) != NULL) {
                        unsigned int new = *tp * 10 + (pch - digits);

                        if (new > 255)
                                return (0);
                        *tp = new;
                        if (! saw_digit) {
                                if (++octets > 4)
                                        return (0);
                                saw_digit = 1;
                        }
                } else if (ch == '.' && saw_digit) {
                        if (octets == 4)
                                return (0);
                        *++tp = 0;
                        saw_digit = 0;
                } else
                        return (0);
        }
        if (octets < 4)
                return (0);
        memcpy(dst, tmp, 4);
        return (1);
}

int inet_pton6(char *src,unsigned char *dst) /*The "dotted hex" -> "integer"*/
{
        static char xdigits_l[] = "0123456789abcdef",xdigits_u[] = "0123456789ABCDEF";
        unsigned char tmp[16], *tp, *endp, *colonp;
        char *xdigits, *curtok;
        int ch, saw_xdigit;
        unsigned int val;

        memset((tp = tmp), '\0', 16);
        endp = tp + 16;
        colonp = NULL;

        if (*src == ':')
                if (*++src != ':')
                        return (0);
        curtok = src;
        saw_xdigit = 0;
        val = 0;
        while ((ch = *src++) != '\0') {
                const char *pch;

                if ((pch = strchr((xdigits = xdigits_l), ch)) == NULL)
                        pch = strchr((xdigits = xdigits_u), ch);
                if (pch != NULL) {
                        val <<= 4;
                        val |= (pch - xdigits);
                        if (val > 0xffff)
                                return (0);
                        saw_xdigit = 1;
                        continue;
                }
                if (ch == ':') {
                        curtok = src;
                        if (!saw_xdigit) {
                                if (colonp)
                                        return (0);
                                colonp = tp;
                                continue;
                        }
                        if (tp + 2 > endp)
                                return (0);
                        *tp++ = (unsigned char) (val >> 8) & 0xff;
                        *tp++ = (unsigned char) val & 0xff;
                        saw_xdigit = 0;
                        val = 0;
                        continue;
                }
                if (ch == '.' && ((tp + 4) <= endp) &&
                    inet_pton4(curtok, tp) > 0) {
                        tp += 4;
                        saw_xdigit = 0;
                        break;  /* '\0' was seen by inet_pton4(). */
                }
                return (0);
        }
        if (saw_xdigit) {
                if (tp + 2 > endp)
                        return (0);
                *tp++ = (unsigned char) (val >> 8) & 0xff;
                *tp++ = (unsigned char) val & 0xff;
        }
        if (colonp != NULL) {
                const int n = tp - colonp;
                int i;

                for (i = 1; i <= n; i++) {
                        endp[- i] = colonp[n - i];
                        colonp[n - i] = 0;
                }
                tp = endp;
        }
        if (tp != endp)
                return (0);
        memcpy(dst, tmp, 16);
        return (1);
}

char *inet_ntop4(unsigned char *src, char *dst, size_t size) /*The "integer" -> "dotted decimal"*/
{
        static const char *fmt = "%u.%u.%u.%u";
        char tmp[sizeof "255.255.255.255"];

        if ((size_t)sprintf(tmp, fmt, src[0], src[1], src[2], src[3]) >= size)
        {
                return (NULL);
        }
        strcpy(dst, tmp);
        return (dst);
}

char *inet_ntop6(unsigned char *src, char *dst, size_t size) /*The "integer" -> "dotted hex"*/
{
        char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"], *tp;
        struct { int base, len; } best, cur;
        unsigned int words[16 / 2];
        int i;

        memset(words, '\0', sizeof words);
        for (i = 0; i < 16; i++)
                words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));
        best.base = -1;
        best.len=0;
        cur.base = -1;
        cur.len=0;
        for (i = 0; i < (16 / 2); i++) {
                if (words[i] == 0) {
                        if (cur.base == -1)
                                cur.base = i, cur.len = 1;
                        else
                                cur.len++;
                } else {
                        if (cur.base != -1) {
                                if (best.base == -1 || cur.len > best.len)
                                        best = cur;
                                cur.base = -1;
                        }
                }
        }
        if (cur.base != -1) {
                if (best.base == -1 || cur.len > best.len)
                        best = cur;
        }
        if (best.base != -1 && best.len < 2)
                best.base = -1;

        tp = tmp;
        for (i = 0; i < (16 / 2); i++) {
                if (best.base != -1 && i >= best.base &&
                    i < (best.base + best.len)) {
                        if (i == best.base)
                                *tp++ = ':';
                        continue;
                }
                if (i != 0)
                        *tp++ = ':';
                if (i == 6 && best.base == 0 &&
                    (best.len == 6 || (best.len == 5 && words[5] == 0xffff))) {
                        if (!inet_ntop4(src+12, tp,
                                        sizeof tmp - (tp - tmp)))
                                return (NULL);
                        tp += strlen(tp);
                        break;
                }
                tp += sprintf(tp, "%x", words[i]);
        }
        if (best.base != -1 && (best.base + best.len) ==
            (16 / 2))
                *tp++ = ':';
        *tp++ = '\0';

        if ((size_t)(tp - tmp) > size) {
                return (NULL);
        }
        strcpy(dst, tmp);
        return (dst);
}

unsigned int sk_inet_addr(char* ip_str) /*Interval of a point into a in_addr address*/
{
        unsigned int ip = 0;
        unsigned char *p=(unsigned char *)&ip;
        unsigned int a1, a2, a3, a4;
    
        if(ip_str==NULL||!strcasecmp(ip_str,"ANY")||!strcmp(ip_str,"0"))
        {
                ip=((unsigned long int) 0x00000000);
                return(ip);
        }
        sscanf(ip_str, "%d.%d.%d.%d", &a1, &a2, &a3, &a4);
        *p=a1;
        *(p+1)=a2;
        *(p+2)=a3;
        *(p+3)=a4;
        return ip;
}

char *buff2String(char *buff,int bufflen)
{
        static char *tmpbuf;
        int len;
        if(buff==NULL)
                return(NULL);
        if(tmpbuf)
                vfree(tmpbuf);
        if(bufflen<=0)
                len=strlen(buff);
        else
                len=bufflen;
        tmpbuf=(char *)vmalloc(len+1);
        if(tmpbuf==NULL)
                return(NULL);
        tmpbuf[len]=0;
        memcpy(tmpbuf,buff,len);
        return(tmpbuf);
}

int msg_field2(char *linebuf,int linelen,int num,char **field,int *flen,char gech)
{
        char *lineend,*fieldstart=linebuf,*fieldend,numflag;
        int fieldlen,i,yhnum,xyhnum,khnum;

        if(linebuf==NULL||linelen<=0||(num>=0&&field==NULL))
                return(-1);
        if(gech=='\t')
                gech=' ';
        lineend=linebuf+linelen;
        for(i=0;i<linelen;i++)
                if(*(linebuf+i)=='\t')
                        *(linebuf+i)=' ';
        while(*fieldstart==gech)
                fieldstart++;
        if(*fieldstart==0)
        {
                if(num<0)
                {
                        return(0);
                }
                else
                {
                        *field=NULL;
                        if(flen)
                                *flen=0;
                        return(-1);
                }
        }
        if(num<0)
        {
                numflag=1;
                num=1000;
        }
        else
                numflag=0;
        while(num>0)
        {
//                fieldend=strchr(fieldstart,gech);
                fieldend=memchr(fieldstart,gech,lineend-fieldstart);
                if(fieldend==NULL)
                {
                        if(numflag)
                        {
                                return(1000-num+1);
                        }
                        else
                        {
                                *field=NULL;
                                if(flen)
                                        *flen=0;
                                return(-1);
                        }
                }
                for(i=1,yhnum=0,xyhnum=0,khnum=0;i<fieldend-fieldstart;i++)
                {
                        if(fieldstart[i]=='"')
                        {
                                if(fieldstart[i-1]!='\\')
                                        yhnum++;
                                else
                                        if(!(yhnum%2))
                                                xyhnum++;
                        }
                        else if(fieldstart[i]=='(')
                                khnum++;
                        else if(fieldstart[i]==')')
                                khnum--;
                }
                if(fieldstart[0]=='"')
                        yhnum++;
                else if(fieldstart[0]=='(')
                        khnum++;
                else if(fieldstart[0]==')')
                        khnum--;
                while((yhnum%2)||(xyhnum%2)||(khnum))
                {
                        while(*++fieldend==gech);
//                        fieldend=strchr(fieldend,gech);
                        fieldend=memchr(fieldend,gech,lineend-fieldend);
                        if(fieldend==NULL)
                        {
                                if(numflag)
                                {
                                        return(1000-num+1);
                                }
                                else
                                {
                                        *field=NULL;
                                        if(flen)
                                                *flen=0;
                                        return(-1);
                                }
                        }
                        for(i=1,yhnum=0,xyhnum=0,khnum=0;i<fieldend-fieldstart;i++)
                        {
                                if(fieldstart[i]=='"')
                                {
                                        if(fieldstart[i-1]!='\\')
                                                yhnum++;
                                        else
                                                if(!(yhnum%2))
                                                        xyhnum++;
                                }
                                else if(fieldstart[i]=='(')
                                        khnum++;
                                else if(fieldstart[i]==')')
                                        khnum--;
                        }
                        if(fieldstart[0]=='"')
                                yhnum++;
                        else if(fieldstart[0]=='(')
                                khnum++;
                        else if(fieldstart[0]==')')
                                khnum--;
                }
                while(*++fieldend==gech&&fieldend<lineend);
                fieldstart=fieldend;
                if(fieldstart>=lineend)
                {
                        if(numflag)
                                return(1000-num+1);
                        else
                        {
                                *field=NULL;
                                if(flen)
                                        *flen=0;
                                return(-1);
                        }
                }
                num--;
        }
//        fieldend=strchr(fieldstart,gech);
        fieldend=memchr(fieldstart,gech,lineend-fieldstart);
        if(fieldend!=NULL)
        {
                for(i=1,yhnum=0,xyhnum=0,khnum=0;i<fieldend-fieldstart;i++)
                {
                        if(fieldstart[i]=='"')
                        {
                                if(fieldstart[i-1]!='\\')
                                        yhnum++;
                                else
                                        if(!(yhnum%2))
                                                xyhnum++;
                        }
                        else if(fieldstart[i]=='(')
                                khnum++;
                        else if(fieldstart[i]==')')
                                khnum--;
                }
                if(fieldstart[0]=='"')
                        yhnum++;
                else if(fieldstart[0]=='(')
                        khnum++;
                else if(fieldstart[0]==')')
                        khnum--;
                while((yhnum%2)||(xyhnum%2)||(khnum))
                {
                        while(*++fieldend==gech);
//                        fieldend=strchr(fieldend,gech);
                        fieldend=memchr(fieldend,gech,lineend-fieldend);
                        if(fieldend==NULL)
                        {
                                fieldend=&fieldstart[strlen(fieldstart)];
                                break;
                        }
                        for(i=1,yhnum=0,xyhnum=0,khnum=0;i<fieldend-fieldstart;i++)
                        {
                                if(fieldstart[i]=='"')
                                {
                                        if(fieldstart[i-1]!='\\')
                                                yhnum++;
                                        else
                                                if(!(yhnum%2))
                                                        xyhnum++;
                                }
                                else if(fieldstart[i]=='(')
                                        khnum++;
                                else if(fieldstart[i]==')')
                                        khnum--;
                        }
                        if(fieldstart[0]=='"')
                                yhnum++;
                        else if(fieldstart[0]=='(')
                                khnum++;
                        else if(fieldstart[0]==')')
                                khnum--;
                }
        }
        else
                fieldend=&fieldstart[strlen(fieldstart)];
        if(fieldend>linebuf+linelen)
                fieldend=linebuf+linelen;
        fieldlen=fieldend-fieldstart;
        if(!fieldlen)
        {
                *field=NULL;
                if(flen)
                        *flen=0;
                return(-1);
        }
        if(flen)
        {
                *field=fieldstart;
                *flen=fieldlen;
                return(0);
        }
        else
        {
                char *retbuf;
                retbuf=vmalloc(fieldlen+1);
                if(retbuf)
                {
                        memcpy(retbuf,fieldstart,fieldlen);
                        retbuf[fieldlen]=0;
                        *field=retbuf;
                        return(0);
                }
                else
                        return(-1);
        }
}

void *vrealloc(void *ptr,int newsize,int oldsize,char freeflag)
{
        void *p=vmalloc(newsize);
        if(ptr&&p)
        {
                memcpy(p, ptr,((oldsize < newsize) ? oldsize : newsize));
        }
        if(ptr&&freeflag)
                vfree(ptr);
        return p;
}

int listnode_tj(struct listnode *head)
{
        int num;
        for(num=0;head;head=head->next,num++);
        return(num);
}

void *listnode_pop(struct listnode **head)
{
        struct listnode *node;
        void *data;
        node=*head;
        if(node==NULL)
                return(NULL);
        *head=node->next;
        if(*head)
                (*head)->prev=NULL;
        data=node->data;
        vfree(node);
        return(data);
}

int listnode_free(struct listnode *node,void (*func)(void *))
{
        if(node==NULL)
                return(0);
        if(node->data&&func)
        {
                func(node->data);
        }
        vfree(node);
        return(0);
}

int listnode_clear(struct listnode **head,void (*func)(void *))
{
        struct listnode *node;
        if(head==NULL)
                return(-1);
        if(*head==(void *)-1)
        {
                *head=NULL;
                return(0);
        }
        for(node=*head;node;node=*head)
        {
                *head=node->next;
                listnode_free(node,func);
        }
        return(0);
}

void listnode_show(struct listnode *head)
{
        lprintf(5,"LIST(%d):",listnode_tj(head));
        while(head)
        {
                lprintf(5," %u",head->key);
                head=head->next;
        }
        lprintf(5,"\n");
        return;
}

struct listnode *listnode_find(struct listnode *head,unsigned int key)
{
        struct listnode *trace=head;
        if(head==NULL||head==(void *)-1)
                return(NULL);
        while(trace)
        {
                if(trace->key==key)
                        return(trace);
                else if(trace->key>key)
                        return(NULL);
                trace=trace->next;
        }
        return(NULL);
}

struct listnode *listnode_search(struct listnode *head,void *data,int len,int (*func)(void *,void *,int))
{
        struct listnode *trace=head;
        if(head==NULL||head==(void *)-1)
                return(NULL);
        while(trace)
        {
                if(func==NULL)
                {
                        if(trace->data==data)
                                return(trace);
                        else
                                trace=trace->next;
                }
                else
                {
                        if(trace->datalen==len&&func(trace->data,data,len)==0)
                                return(trace);
                        else
                                trace=trace->next;
                }
        }
        return(NULL);
}

struct listnode *listnode_match(struct listnode *head,void *data,int len,int (*func)(void *,void *,int))
{
        struct listnode *trace=head;
        if(head==NULL||head==(void *)-1)
                return(NULL);
        while(trace)
        {
                if(func==NULL)
                {
                        if(trace->data==data)
                                return(trace);
                        else
                                trace=trace->next;
                }
                else
                {
                        if(trace->datalen>=len&&func(trace->data,data,len)==0)
                                return(trace);
                        else
                                trace=trace->next;
                }
        }
        return(NULL);
}

int listnode_insert(struct listnode **head,struct listnode *node)
{
        struct listnode *trace=*head,*ptrace=NULL;
        if(node==NULL)
        {
                lprintf(0,"argument error\n");
                return(-1);
        }
        if(trace==(void *)-1)
                trace=NULL;
        while(trace)
        {
                if(trace->key==node->key)
                {
                        return(-1);
                }
                else if(trace->key<node->key)
                {
                        ptrace=trace;
                        trace=trace->next;
                }
                else
                {
                        node->next=trace;
                        trace->prev=node;
                        break;
                }
        }
        if(ptrace)
        {
                ptrace->next=node;
                node->prev=ptrace;
        }
        else
        {
                node->prev=NULL;
                *head=node;
        }
        return(0);
}

struct listnode *listnode_uninsert(struct listnode **head,struct listnode *node)
{
        if(head==NULL||node==NULL)
                return(NULL);
        if(node->prev)
                node->prev->next=node->next;
        else
                *head=node->next;
        if(node->next)
                node->next->prev=node->prev;
        return(node);
}

struct listnode *listnode_uninsert2(struct listnode **head,unsigned int key)
{
        struct listnode *node;
        if(head==NULL)
                return(NULL);
        node=listnode_find(*head,key);
        if(node==NULL)
                return(NULL);
        return(listnode_uninsert(head,node));
}

struct listnode *listnode_add(struct listnode **head,unsigned int key,void *data,int datalen)
{
        struct listnode *node;

        node=listnode_find(*head,key);
        if(node)
        {
                if(node->data)
                        return((struct listnode *)-1);
                else
                {
                        node->data=data;
                        node->datalen=datalen;
                        node->datasp=0;
                        return(node);
                }
        }
        node=(struct listnode *)vmalloc(sizeof(struct listnode));
        if(node==NULL)
        {
                lprintf(0,"vmalloc error\n");
                return(NULL);
        }
        memset(node,0,sizeof(struct listnode));
        node->key=key;
        node->data=data;
        node->datalen=datalen;
        node->datasp=0;
        if(!listnode_insert(head,node))
                return(node);
        else
        {
                vfree(node);
                return(NULL);
        }
}

int listnode_del(struct listnode **head,unsigned int key,void (*func)(void *))
{
        struct listnode *node;
        node=listnode_find(*head,key);
        if(node==NULL)
                return(-1);
        listnode_uninsert(head,node);
        listnode_free(node,func);
        return(0);
}

struct listnode *listnode_tail(struct listnode **head,void *data,int datalen)
{
        struct listnode *node;
        static int num=0;

        node=(struct listnode *)vmalloc(sizeof(struct listnode));
        if(node==NULL)
        {
                lprintf(0,"vmalloc error\n");
                return(NULL);
        }
        memset(node,0,sizeof(struct listnode));
        node->key=num++;
        node->data=data;
        node->datalen=datalen;
        node->datasp=0;
        if(!listnode_insert(head,node))
                return(node);
        else
        {
                vfree(node);
                return(NULL);
        }
}

int listnode_join(struct listnode **head,struct listnode **list)
{
        struct listnode *newlist=NULL,*lhead,*node;
        if(head==NULL||list==NULL)
        {
                return(-1);
        }
        if(*head==NULL)
        {
                *head=*list;
                return(0);
        }
        if(*list==NULL)
                return(0);
        lhead=*list;
        while(lhead)
        {
                node=lhead;
                lhead=lhead->next;
                if(listnode_insert(head,node)!=0)
                        listnode_insert(&newlist,node);
        }
        *list=newlist;
        return(0);
}

#if 0
int main(void)
{
        struct listnode *head=NULL;
        int i;

        for(i=0;i<32;i++)
                listnode_add(&head,randshu(0,100),NULL);
        listnode_show(head);
}
#endif

