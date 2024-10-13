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
#ifndef _FANYI_H
#define _FANYI_H
#define FANYI_ETH 0

#define BUFF_SIZE 4096
#define FRAG_BUFF_SIZE 1232 
//#define FRAG_BUFF_SIZE 1400


#define TRANSLATED_PREFIX 0x0000ffff
#define MAPPED_PREFIX 0x0000ffff  

#define IP4_IP6_HDR_DIFF 20 
#define IP6_FRAGMENT_SIZE 8 

#define IP6F_OFF_MASK       0xfff8 
#define IP6F_RESERVED_MASK  0x0006
#define IP6F_MORE_FRAG      0x0001 

struct ip6_opthdr
{
        unsigned char nxt;
        unsigned char len;
};

struct ip6_hdr
{
  union
    {
  struct ip6_hdrctl
    {
      uint32_t ip6_un1_flow;   /* 4 bits version, 8 bits TC,
                  20 bits flow-ID */
      uint16_t ip6_un1_plen;   /* payload length */
      uint8_t  ip6_un1_nxt;    /* next header */
      uint8_t  ip6_un1_hlim;   /* hop limit */
    } ip6_un1;
  uint8_t ip6_un2_vfc;       /* 4 bits version, top 4 bits tclass */
    } ip6_ctlun;
  struct in6_addr ip6_src;      /* source address */
  struct in6_addr ip6_dst;      /* destination address */
};
#define ip6_vfc   ip6_ctlun.ip6_un2_vfc
#define ip6_flow  ip6_ctlun.ip6_un1.ip6_un1_flow
#define ip6_plen  ip6_ctlun.ip6_un1.ip6_un1_plen
#define ip6_nxt   ip6_ctlun.ip6_un1.ip6_un1_nxt
#define ip6_hlim  ip6_ctlun.ip6_un1.ip6_un1_hlim
#define ip6_hops  ip6_ctlun.ip6_un1.ip6_un1_hlim

struct ipv6 {
#if defined(__LITTLE_ENDIAN_BITFIELD)
        unsigned char        priority:4,version:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
        unsigned char        version:4,priority:4;
#else
#error        "Please fix <asm/byteorder.h>"
#endif
        unsigned char flow_lbl[3];
        unsigned short payload_len;
        unsigned char nexthdr;
        unsigned char hop_limit;
        struct        in6_addr        saddr;
        struct        in6_addr        daddr;
};

struct ip
{
#if defined(__LITTLE_ENDIAN_BITFIELD)
        unsigned int ip_hl:4;                /*header length*/
        unsigned int ip_v:4;                /*version*/
#elif defined(__BIG_ENDIAN_BITFIELD)
        unsigned int ip_v:4;                /*version*/
        unsigned int ip_hl:4;                /*header length*/
#endif
        unsigned char ip_tos;                /*type of service*/
        unsigned short ip_len;                /*total length*/
        unsigned short ip_id;                /*identification*/
        unsigned short ip_off;                /*fragment offset field*/
#define IP_RF 0x8000                        /*reserved fragment flag*/
#define IP_DF 0x4000            /*dont fragment flag*/
#define IP_MF 0x2000            /*more fragments flag*/
#define IP_OFFMASK 0x1fff       /*mask for fragmenting bits*/
        unsigned char ip_ttl;                /*time to live*/
        unsigned char ip_p;                        /*protocol*/
        unsigned short ip_sum;                /*checksum*/
        unsigned int ip_src,ip_dst;/*source and dest address*/
}__attribute__((packed));

struct tcp
{
        unsigned short sport;        /*源端口*/
        unsigned short dport;        /*目的端口*/
        unsigned int seq;                /*标识TCP数据包的序列号*/
        unsigned int ack_seq;        /*确认序列号,表示接受方下一次接受的数据序列号*/
#if defined(__LITTLE_ENDIAN_BITFIELD)
        unsigned short res1:4;
        unsigned short doff:4;        /*TCP数据包首部长度,以4字节为单位,
                                   一般的时候为5*/
        unsigned short fin:1;        /*如果设置为1的时候,表示请求关闭连接*/
        unsigned short syn:1;        /*如果设置为1的时候,表示请求建立连接*/
        unsigned short rst:1;        /*如果设置为1的时候,表示请求重新连接*/
        unsigned short psh:1;        /*如果设置为1,那么接收方收到数据后,
                                   立即交给上一层程序*/
        unsigned short ack:1;        /*如果确认号正确则为1*/
        unsigned short urg:1;        /*如果设置紧急数据指针,则该位为1*/
        unsigned short res2:2;
#elif defined(__BIG_ENDIAN_BITFIELD)
        unsigned short doff:4;
        unsigned short res1:4;
        unsigned short res2:2;
        unsigned short urg:1;
        unsigned short ack:1;
        unsigned short psh:1;
        unsigned short rst:1;
        unsigned short syn:1;
        unsigned short fin:1;
#endif
        unsigned short window;        /*告诉接收者可以接收的大小*/
        unsigned short check;        /*TCP数据报校验*/
        unsigned short urg_prt;        /*如果urg=1,那么指出紧急数据
                                   对于历史数据开始的序列号的偏移值*/
}__attribute__((packed));

struct udp
{
        unsigned short sport;
        unsigned short dport;
        unsigned short len;
        unsigned short check;
}__attribute__((packed));

#define lprintf(level,format,...) \
do \
{ \
        if(level==0) \
                printk("%s(%d)%s: "format,__FILE__,__LINE__,(char *)__FUNCTION__,##__VA_ARGS__ ); \
        else \
                printk((format),##__VA_ARGS__ ); \
} \
while(0)

#define        PKG_PUT_SHORT(val, cp) \
do \
{ \
        unsigned short Xv; \
        Xv = (unsigned short)(val); \
        *(cp)++ = (unsigned char)(Xv >> 8); \
        *(cp)++ = (unsigned char)Xv; \
} \
while(0)

#endif
