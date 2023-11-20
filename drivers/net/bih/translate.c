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

#include <linux/version.h>
#include <linux/sched.h>
#include <linux/kernel.h>       /* printk() */
#include <linux/slab.h>
#include <linux/errno.h>        /* error codes */
#include <linux/types.h>        /* size_t */
#include <linux/interrupt.h>    /* mark_bh */
#include <linux/random.h>
#include <linux/in.h>
#include <linux/netdevice.h>    /* struct device, and other headers */
#include <linux/etherdevice.h>  /* eth_type_trans */
#include <linux/inetdevice.h>
#include <net/ip.h>             /* struct iphdr */
#include <net/icmp.h>           /* struct icmphdr */
#include <net/ipv6.h>
#include <net/udp.h>
#include <linux/skbuff.h>
#include <linux/in6.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/checksum.h>
#include <net/addrconf.h>
#include "translate.h"
#include "map.h"
#include "pool.h"
#include "voslist.h"
#define NIPQUAD(addr) \
        ((unsigned char *)&addr)[0], \
        ((unsigned char *)&addr)[1], \
        ((unsigned char *)&addr)[2], \
        ((unsigned char *)&addr)[3]
#define __BIHDEBUG__

#define BIH_DEBUG printk
/*
 * If tos_ignore_flag != 0, we don't copy TOS and Traffic Class
 * from origin paket and set it to 0
 */
int tos_ignore_flag = 0;
int BIHMODE,NETWORKTYPE=0;
unsigned int PRIVATEADDR=0;
extern struct net_device *bihdev;

int ip6option(int type)
{
        int i,optarray[8]={IPPROTO_HOPOPTS,IPPROTO_IPV6,IPPROTO_ROUTING,IPPROTO_FRAGMENT,
                        IPPROTO_ESP,IPPROTO_AH,IPPROTO_DSTOPTS,IPPROTO_NONE};
        for(i=0;i<8;i++)
        {
                if(type==optarray[i])
                        return(optarray[i]);
        }
        return(0);
}

unsigned char *getipv6uppkg(unsigned char *ippkg,unsigned char *protocol,int *uppkglen)
{
        unsigned char *ippkgpos=ippkg+40;
        struct ip6_hdr *hdr=(struct ip6_hdr *)ippkg;
        struct ip6_opthdr *opthdr;
        int ip6hdrlen;

        if(ip6option(hdr->ip6_nxt))
        {
                opthdr=(struct ip6_opthdr *)ippkgpos;
                while(ip6option(opthdr->nxt))
                {
                        ippkgpos+=opthdr->len;
                        opthdr=(struct ip6_opthdr *)ippkgpos;
                }
                if(protocol)
                        *protocol=opthdr->nxt;
                ippkgpos+=opthdr->len;
        }
        else
                if(protocol)
                        *protocol=hdr->ip6_nxt;
        ip6hdrlen=ippkgpos-ippkg;
        if(uppkglen)
                *uppkglen=ntohs(hdr->ip6_plen)+40-ip6hdrlen;
        return(ippkgpos);
}

unsigned short chksum(unsigned short *data,int len)
{
        unsigned short answer=0,*p=data;
        int nleft=len,sum=0;
        while(nleft>1)
            {
                sum+=*p++;
                nleft-=2;
        }
        if(nleft==1)
        {
                *(unsigned char *)(&answer)=*(unsigned char *)p;
                sum+=answer;
        }
        sum=(sum>>16)+(sum&0xffff);
        sum+=(sum>>16);
        answer=~sum;
        return(answer);
}

unsigned short tcpchksum6(unsigned char *src,unsigned char *dst,unsigned char *tcppkg,int tcppkglen)
{
        struct tcp *th;
        static unsigned char *tmpbuf=NULL;
        unsigned char *bufpos;
        int tmpbuflen;

        if(tmpbuf==NULL)
        {
                tmpbuf=kmalloc(2048,GFP_KERNEL);
                if(tmpbuf==NULL)
                {
                        lprintf(0,"kmalloc error\n");
                        return(0);
                }
        }
        bufpos=tmpbuf;
        th=(struct tcp *)tcppkg;
        th->check=0;
        memcpy(bufpos,src,16);
        bufpos+=16;
        memcpy(bufpos,dst,16);
        bufpos+=16;
        *bufpos++=0;
        *bufpos++=IPPROTO_TCP;
        PKG_PUT_SHORT(tcppkglen,bufpos);
        memcpy(bufpos,tcppkg,tcppkglen);
        bufpos+=tcppkglen;
        tmpbuflen=bufpos-tmpbuf;
        return(chksum((unsigned short *)tmpbuf,tmpbuflen));
}

unsigned short tcpchksum(unsigned char *src,unsigned char *dst,unsigned char *tcppkg,int tcppkglen)
{
        struct tcp *th;
        static unsigned char *tmpbuf=NULL;
        unsigned char *bufpos;
        int tmpbuflen;

        if(tmpbuf==NULL)
        {
                tmpbuf=kmalloc(2048,GFP_KERNEL);
                if(tmpbuf==NULL)
                {
                        lprintf(0,"kmalloc error\n");
                        return(0);
                }
        }
        bufpos=tmpbuf;
        th=(struct tcp *)tcppkg;
        th->check=0;
        memcpy(bufpos,src,4);
        bufpos+=4;
        memcpy(bufpos,dst,4);
        bufpos+=4;
        *bufpos++=0;
        *bufpos++=IPPROTO_TCP;
        PKG_PUT_SHORT(tcppkglen,bufpos);
        memcpy(bufpos,tcppkg,tcppkglen);
        bufpos+=tcppkglen;
        tmpbuflen=bufpos-tmpbuf;
        return(chksum((unsigned short *)tmpbuf,tmpbuflen));
}

int ipv6_get_addr(struct net_device *dev, struct in6_addr *addr)
{
        struct inet6_dev *idev;
        int err = -EADDRNOTAVAIL;

        BIH_DEBUG("bih: ipv6_get_addr start\n");
        if ((idev = __in6_dev_get(dev)) != NULL) {
                struct inet6_ifaddr *ifp;

                read_lock_bh(&idev->lock);
                list_for_each_entry(ifp, &idev->addr_list, if_list){
                        char dst[64];//test
                        inet_ntop6((unsigned char *)&ifp->addr, dst, sizeof(dst)-1);//test
                        BIH_DEBUG("bih: ipv6_get_addr addr=%s and if->scope=%d ifp->flags=%d\n",dst ,ifp->scope, ifp->flags);
                        if (ifp->scope != IFA_LINK && !(ifp->flags&IFA_F_TENTATIVE))
                        {
                                //ipv6_addr_copy(addr, &ifp->addr);
                                *addr = ifp->addr;
                                err = 0;
                                break;
                        }
                }
                read_unlock_bh(&idev->lock);
        }
        return err;
}

int ipv6_get_laddr(struct net_device *dev, struct in6_addr *addr)
{
        struct inet6_dev *idev;
        int err = -EADDRNOTAVAIL;

        BIH_DEBUG("bih: ipv6_get_laddr start\n");
        if ((idev = __in6_dev_get(dev)) != NULL) {
                struct inet6_ifaddr *ifp;

                read_lock_bh(&idev->lock);
                list_for_each_entry(ifp, &idev->addr_list, if_list) {
                            char dst[64];//test
                        inet_ntop6((unsigned char *)&ifp->addr, dst, sizeof(dst)-1);//test
                        BIH_DEBUG("bih: ipv6_get_addr linklocal addr=%s and if->scope=%d ifp->flags=%d\n",dst ,ifp->scope, ifp->flags);
                        if (ifp->scope == IFA_LINK && !(ifp->flags&IFA_F_TENTATIVE))
                        {
                                //ipv6_addr_copy(addr, &ifp->addr);   //
                                *addr = ifp->addr;
                                err = 0;
                                break;
                        }
                }
                read_unlock_bh(&idev->lock);
        }
        return err;
}

int ipv4_get_addr(struct net_device *dev, unsigned int *addr)
{
        struct in_device *in_dev;
        struct in_ifaddr *ifa = NULL;

        BIH_DEBUG("bih: ipv4_get_addr start\n");
        if ((in_dev = __in_dev_get_rtnl(dev)) != NULL)
        {
                for(ifa=in_dev->ifa_list;ifa!=NULL;ifa=ifa->ifa_next)
                {
                        if(ifa->ifa_local!=0)
                        {
                                *addr=ifa->ifa_local;
                                return(0);
                        }
                }
        }
        return(-1);
}

int ip_show(struct ip *ip)
{
        if(ip==NULL)
                return 0;
        BIH_DEBUG("bih: v(%u) hl(%u) tos(%u) len(%u) id(%u) off(%u) ",
                ip->ip_v,ip->ip_hl<<2,ip->ip_tos,ntohs(ip->ip_len),ntohs(ip->ip_id),ntohs(ip->ip_off));
        lprintf(1,"v(%u) hl(%u) tos(%u) len(%u) id(%u) off(%u) ",
                ip->ip_v,ip->ip_hl<<2,ip->ip_tos,ntohs(ip->ip_len),ntohs(ip->ip_id),ntohs(ip->ip_off));
        switch(ip->ip_p)
        {
                case IPPROTO_TCP:
                        lprintf(1,"TCP ");
                        break;
                case IPPROTO_UDP:
                        lprintf(1,"UDP ");
                        break;
                case IPPROTO_ICMP:
                        lprintf(1,"ICMP ");
                        break;
                default:
                        lprintf(1,"%u ",ip->ip_p);
                        break;
        }
        BIH_DEBUG("bih: sum(%04X) %u.%u.%u.%u --> %u.%u.%u.%u\n",ntohs(ip->ip_sum),NIPQUAD(ip->ip_src),NIPQUAD(ip->ip_dst));
        lprintf(1,"sum(%04X) %u.%u.%u.%u --> %u.%u.%u.%u\n",ntohs(ip->ip_sum),NIPQUAD(ip->ip_src),NIPQUAD(ip->ip_dst));
        return(0);
}

int ipv6_show(struct ipv6hdr *ipv6)
{
        char dst[64],src[64];
        if(ipv6==NULL)
                return 0;
        dst[0]=src[0]=0;
        inet_ntop6((unsigned char *)&ipv6->daddr, dst, sizeof(dst)-1);
        inet_ntop6((unsigned char *)&ipv6->saddr, src, sizeof(src)-1);
        BIH_DEBUG("bih: v(%u) p(%u) plen(%d) proto(%d) hlimit(%d) %s --> %s\n",
                ipv6->version,ipv6->priority,ntohs(ipv6->payload_len),ipv6->nexthdr,ipv6->hop_limit,src,dst);
        lprintf(1,"v(%u) p(%u) plen(%d) proto(%d) hlimit(%d) %s --> %s\n",
                ipv6->version,ipv6->priority,ntohs(ipv6->payload_len),ipv6->nexthdr,ipv6->hop_limit,src,dst);
        return(0);
}

int tcp_show(struct tcp *tcp)
{
        if(tcp==NULL)
                return(0);
        BIH_DEBUG("bih: SPORT(%d) DPORT(%d) SEQ(%u) ACKSEQ(%u) HEADLEN(%d) FIN(%d) SYN(%d) RST(%d) PSH(%d) WINDOW(%d) SUM(%04x) URGPRT(%d)\n",
                ntohs(tcp->sport),ntohs(tcp->dport),ntohl(tcp->seq),ntohl(tcp->ack_seq),tcp->doff<<2,
                tcp->fin,tcp->syn,tcp->rst,tcp->psh,ntohs(tcp->window),ntohs(tcp->check),ntohs(tcp->urg_prt));
        lprintf(1,"SPORT(%d) DPORT(%d) SEQ(%u) ACKSEQ(%u) HEADLEN(%d) FIN(%d) SYN(%d) RST(%d) PSH(%d) WINDOW(%d) SUM(%04x) URGPRT(%d)\n",
                ntohs(tcp->sport),ntohs(tcp->dport),ntohl(tcp->seq),ntohl(tcp->ack_seq),tcp->doff<<2,
                tcp->fin,tcp->syn,tcp->rst,tcp->psh,ntohs(tcp->window),ntohs(tcp->check),ntohs(tcp->urg_prt));
        return(0);
}

int udp_show(struct udp *udp)
{
        if(udp==NULL)
                return(0);
        BIH_DEBUG("bih: SPORT(%d)  DPORT(%d)  LEN(%d)  SUM(%04x)\n",
                ntohs(udp->sport),ntohs(udp->dport),ntohs(udp->len),ntohs(udp->check));
        lprintf(1,"SPORT(%d)  DPORT(%d)  LEN(%d)  SUM(%04x)\n",
                ntohs(udp->sport),ntohs(udp->dport),ntohs(udp->len),ntohs(udp->check));
        return(0);
}

int skb_show(struct sk_buff *skb)
{
        struct ipv6hdr *ipv6;
        struct tcp *tcp;
        struct udp *udp;
        if(skb==NULL)
                return(0);
        ipv6 = (struct ipv6hdr *)ipv6_hdr(skb);
        tcp=(struct tcp *)(ipv6+1);
        udp=(struct udp *)(ipv6+1);
        ipv6_show(ipv6);
        tcp_show(tcp);
        return(0);
}

void array_show(unsigned char *packp,int packlen,char hdflag,char base)
{
        int i;
        if(hdflag)
                lprintf(7,"==================== ARRAY[%d] ====================\n",packlen);
        for(i=0;i<packlen;i++)
        {
                switch(base)
                {
                        case 16:
                                if(i&&!(i%16))
                                        lprintf(7,"\n");
                                lprintf(7,"%02x ",packp[i]);
                                break;
                        case 10:
                                if(i&&!(i%16))
                                        lprintf(7,"\n");
                                lprintf(7,"%03d ",packp[i]);
                                break;
                        case 2:
                        {
                                unsigned char n,mask=0x80;
                                if(i&&!(i%4))
                                        lprintf(7,"\n");
                                for(n=0;n<8;n++)
                                {
                                        if(n&&!(n%4))
                                                lprintf(7,",");
                                        if(packp[i]&mask)
                                                lprintf(7,"1");
                                        else
                                                lprintf(7,"0");
                                        mask>>=1;
                                }
                                lprintf(7," ");
                                break;
                        }
                        default:
                                lprintf(7,"%c",packp[i]);
                                break;
                }
        }
        if(base==16)
                lprintf(7,"\n");
}


static int ip4_ip6(struct net_device *dev,char *src, int len, char *dst, int include_flag)/*v4 address be translated*/
{
        struct in6_addr addr,dstaddr;
        struct iphdr *ih4 = (struct iphdr *) src;
        struct icmphdr *icmp_hdr;
        struct udphdr *udp_hdr;
        struct tcphdr *tcp_hdr;

        struct ipv6hdr *ih6 = (struct ipv6hdr *) dst;
        struct frag_hdr *ih6_frag = (struct frag_hdr *)(dst+sizeof(struct ipv6hdr));
        struct icmp6hdr *icmp6_hdr;

        int hdr_len = (int)(ih4->ihl * 4);
        int icmp_len;
        int plen;

        unsigned int csum;
        int fl_csum = 0;
        int icmperr = 1;
        int fr_flag = 0;
        __u16 new_tot_len;
        __u8 new_nexthdr;
        __u16 icmp_ptr = 0;
        int ret;
        struct map_node *mapnode;

        BIH_DEBUG("bih: ip4_ip6 start\n");
        if(ipv4_is_loopback(ih4->daddr)||ipv4_is_multicast(ih4->daddr)||
                ipv4_is_lbcast(ih4->daddr)||ipv4_is_zeronet(ih4->daddr)||
                ih4->daddr==ih4->saddr)
        {
                BIH_DEBUG("bih: ip4_ip6 ipv4 is not right addr\n");
                return(-1);
        }

        if(ih4->protocol==IPPROTO_UDP)
        {
                udp_hdr = (struct udphdr *)(src+hdr_len);
                if(ntohs(udp_hdr->dest)==53)
                {
                        BIH_DEBUG("bih: ip4_ip6 pkt type is dns\n");
                        return(-1);
                }
        }

        ret=ipv6_get_addr(dev,&addr);
        if(ret!=0)
        {
                BIH_DEBUG("bih: ip4_ip6 get ipv6 addr failed\n");
                return(-1);
        }
        mapnode=map_search(ih4->daddr);
        if(mapnode)
                memcpy(&dstaddr,&mapnode->in6addr,sizeof(struct in6_addr));

        else
        {
                BIH_DEBUG("bih: ip4_ip6 no correct map searched\n");
                return(-1);
        }
#ifdef __BIHDEBUG__
{
        struct ip *ip=(struct ip *)ih4;
        BIH_DEBUG("bih: ---------- %s ----------\n",mapnode?"466":"464");
        lprintf(0,"---------- %s ----------\n",mapnode?"466":"464");
        ip_show(ip);
}
#endif

        if (ntohs(ih4->frag_off) == IP_DF || include_flag ) {
                new_tot_len = ntohs(ih4->tot_len);

                if (ih4->protocol == IPPROTO_ICMP)
                        new_nexthdr = NEXTHDR_ICMP;
                else
                        new_nexthdr = ih4->protocol;
        }
        else {
                /* need to add Fragment Header */
                fr_flag = 1;
                /* total length = total length from IPv4 packet +
                   length of Fragment Header */
                new_tot_len = ntohs(ih4->tot_len) + sizeof(struct frag_hdr);
                /* IPv6 Header NextHeader = NEXTHDR_FRAGMENT */
                new_nexthdr = NEXTHDR_FRAGMENT;
                /* Fragment Header NextHeader copy from IPv4 packet */
                if (ih4->protocol == IPPROTO_ICMP)
                        ih6_frag->nexthdr = NEXTHDR_ICMP;
                else
                        ih6_frag->nexthdr = ih4->protocol;

                /* copy frag offset from IPv4 packet */
                ih6_frag->frag_off = htons((ntohs(ih4->frag_off) & IP_OFFSET) << 3);
                /* copy MF flag from IPv4 packet */
                ih6_frag->frag_off = htons((ntohs(ih6_frag->frag_off) |
                                                                        ((ntohs(ih4->frag_off) & IP_MF) >> 13)));
                /* copy Identification field from IPv4 packet */
                ih6_frag->identification = htonl(ntohs(ih4->id));
                /* reserved field initialized to zero */
                ih6_frag->reserved = 0;
        }

        ih6->version = 6;

        if (tos_ignore_flag) {
                ih6->priority = 0;
                ih6->flow_lbl[0] = 0;
        } else {
                ih6->priority = (ih4->tos & 0xf0) >> 4;
                ih6->flow_lbl[0] = (ih4->tos & 0x0f) << 4;
        }
        ih6->flow_lbl[1] = 0;
        ih6->flow_lbl[2] = 0;

        /* Hop Limit = IPv4 TTL */
        ih6->hop_limit = ih4->ttl;

        memcpy(ih6->saddr.s6_addr,addr.s6_addr,16);
        memcpy(ih6->daddr.s6_addr,dstaddr.s6_addr,16);

        plen = new_tot_len - hdr_len; /* Payload length = IPv4 total len - IPv4 header len */
        ih6->payload_len = htons(plen);
        ih6->nexthdr = new_nexthdr; /* Next Header */

        switch (ih4->protocol) {
        case IPPROTO_ICMP:
                if ( (ntohs(ih4->frag_off) & IP_OFFSET) != 0 || (ntohs(ih4->frag_off) & IP_MF) != 0 ) {
                        BIH_DEBUG("bih: ip4_ip6(): don't translate ICMPv4 fragments - packet dropped.\n");
                        lprintf(0,"ip4_ip6(): don't translate ICMPv4 fragments - packet dropped.\n");
                        return -1;
                }

                icmp_hdr = (struct icmphdr *) (src+hdr_len); /* point to ICMPv4 header */
                csum = 0;
                icmp_len =  ntohs(ih4->tot_len) - hdr_len; /* ICMPv4 packet length */
                icmp6_hdr = (struct icmp6hdr *)(dst+sizeof(struct ipv6hdr)
                                                                            +fr_flag*sizeof(struct frag_hdr)); /* point to ICMPv6 header */

                if (include_flag) {
                        BIH_DEBUG("bih: ip4_ip6(): It's included ICMPv4 in ICMPv4 Error message - packet dropped.\n");
                        lprintf(0,"ip4_ip6(): It's included ICMPv4 in ICMPv4 Error message - packet dropped.\n");
                        return -1;
                }

                /* Check ICMPv4 Type field */
                switch (icmp_hdr->type) {
                /* ICMP Error messages */
                /* Destination Unreachable (Type 3) */
                case ICMP_DEST_UNREACH:
                        icmp6_hdr->icmp6_type = ICMPV6_DEST_UNREACH; /* to Type 1 */
                        icmp6_hdr->icmp6_unused = 0;
                        switch (icmp_hdr->code)
                        {
                        case ICMP_NET_UNREACH: /* Code 0 */
                        case ICMP_HOST_UNREACH: /* Code 1 */
                        case ICMP_SR_FAILED: /* Code 5 */
                        case ICMP_NET_UNKNOWN: /* Code 6 */
                        case ICMP_HOST_UNKNOWN: /* Code 7 */
                        case ICMP_HOST_ISOLATED: /* Code 8 */
                        case ICMP_NET_UNR_TOS: /* Code 11 */
                        case ICMP_HOST_UNR_TOS: /* Code 12 */
                                icmp6_hdr->icmp6_code = ICMPV6_NOROUTE; /* to Code 0 */
                                break;
                        case ICMP_PROT_UNREACH: /* Code 2 */
                                icmp6_hdr->icmp6_type = ICMPV6_PARAMPROB; /* to Type 4 */
                                icmp6_hdr->icmp6_code = ICMPV6_UNK_NEXTHDR; /* to Code 1 */
                                /* Set pointer filed to 6, it's octet offset IPv6 Next Header field */
                                icmp6_hdr->icmp6_pointer = htonl(6);
                                break;
                        case ICMP_PORT_UNREACH: /* Code 3 */
                                icmp6_hdr->icmp6_code = ICMPV6_PORT_UNREACH; /* to Code 4 */
                                break;
                        case ICMP_FRAG_NEEDED: /* Code 4 */
                                icmp6_hdr->icmp6_type = ICMPV6_PKT_TOOBIG; /* to Type 2 */
                                icmp6_hdr->icmp6_code = 0;
                                /* Correct MTU  */
                                if (icmp_hdr->un.frag.mtu == 0)
                                        icmp6_hdr->icmp6_mtu = htonl(576);
                                else
                                        icmp6_hdr->icmp6_mtu = htonl(ntohs(icmp_hdr->un.frag.mtu)+IP4_IP6_HDR_DIFF);

                                break;
                        case ICMP_NET_ANO: /* Code 9 */
                        case ICMP_HOST_ANO: /* Code 10 */
                                icmp6_hdr->icmp6_code = ICMPV6_ADM_PROHIBITED; /* to Code 1 */
                                break;
                        default: /* discard any other Code */
                                BIH_DEBUG("bih: ip4_ip6(): Unknown ICMPv4 Type %d Code %d - packet dropped.\n",
                                           ICMP_DEST_UNREACH, icmp_hdr->code);
                                lprintf(0,"ip4_ip6(): Unknown ICMPv4 Type %d Code %d - packet dropped.\n",
                                           ICMP_DEST_UNREACH, icmp_hdr->code);
                                return -1;
                        }
                        break;
                        /* Time Exceeded (Type 11) */
                case ICMP_TIME_EXCEEDED:
                        icmp6_hdr->icmp6_type = ICMPV6_TIME_EXCEED;
                        icmp6_hdr->icmp6_code = icmp_hdr->code;
                        break;
                        /* Parameter Problem (Type 12) */
                case ICMP_PARAMETERPROB:
                        icmp6_hdr->icmp6_type = ICMPV6_PARAMPROB;
                        icmp6_hdr->icmp6_code = icmp_hdr->code;

                        icmp_ptr = ntohs(icmp_hdr->un.echo.id) >> 8;
                        switch (icmp_ptr) {
                        case 0:
                                icmp6_hdr->icmp6_pointer = 0;
                                break;
                        case 2:
                                icmp6_hdr->icmp6_pointer = __constant_htonl(4);
                                break;
                        case 8:
                                icmp6_hdr->icmp6_pointer = __constant_htonl(7);
                                break;
                        case 9:
                                icmp6_hdr->icmp6_pointer = __constant_htonl(6);
                                break;
                        case 12:
                                icmp6_hdr->icmp6_pointer = __constant_htonl(8);
                                break;
                        case 16:
                                icmp6_hdr->icmp6_pointer = __constant_htonl(24);
                                break;
                        default:
                                icmp6_hdr->icmp6_pointer = 0xffffffff;
                                break;
                        }
                        break;
                case ICMP_ECHO:
                        icmperr = 0;
                        icmp6_hdr->icmp6_type = ICMPV6_ECHO_REQUEST;
                        icmp6_hdr->icmp6_code = 0;
                        memcpy(((char *)icmp6_hdr)+4, ((char *)icmp_hdr)+4, len - hdr_len - 4);
                        break;

                case ICMP_ECHOREPLY:
                        icmperr = 0;
                        icmp6_hdr->icmp6_type = ICMPV6_ECHO_REPLY;
                        icmp6_hdr->icmp6_code = 0;
                        memcpy(((char *)icmp6_hdr)+4, ((char *)icmp_hdr)+4, len - hdr_len - 4);
                        break;

                default:
                        BIH_DEBUG("bih: ip4_ip6(): Unknown ICMPv4 packet Type %x - packet dropped.\n", icmp_hdr->type);
                        lprintf(0,"ip4_ip6(): Unknown ICMPv4 packet Type %x - packet dropped.\n", icmp_hdr->type);
                        return -1;
                }

                if (icmperr) {
                        if (ip4_ip6(dev,src+hdr_len+sizeof(struct icmphdr), len - hdr_len - sizeof(struct icmphdr),
                                                dst+sizeof(struct ipv6hdr)+fr_flag*sizeof(struct frag_hdr)
                                                +sizeof(struct icmp6hdr), 1) == -1) {
                                BIH_DEBUG("bih: ip4_ip6(): Uncorrect translation of ICMPv4 Error message - packet dropped.\n");
                                lprintf(0,"ip4_ip6(): Uncorrect translation of ICMPv4 Error message - packet dropped.\n");
                                return -1;
                        }
                        icmp_len += 20;
                        plen += 20;
                        ih6->payload_len = htons(plen);
                }

                icmp6_hdr->icmp6_cksum = 0;
                csum = 0;

                csum = csum_partial((u_char *)icmp6_hdr, icmp_len, csum);
                icmp6_hdr->icmp6_cksum = csum_ipv6_magic(&ih6->saddr, &ih6->daddr, icmp_len,
                                                                                 IPPROTO_ICMPV6, csum);
                break;

        case IPPROTO_TCP:
                memcpy(dst+sizeof(struct ipv6hdr)+fr_flag*sizeof(struct frag_hdr),
                                   src+hdr_len, len - hdr_len);
                tcp_hdr = (struct tcphdr *)(dst+sizeof(struct ipv6hdr)+ fr_flag*sizeof(struct frag_hdr));
                if(ntohs(ih4->frag_off) == IP_DF||ih4->frag_off==0)
                {
                        tcp_hdr->check = 0;
                        csum = 0;
                        csum = csum_partial((unsigned char *)tcp_hdr, plen - fr_flag*sizeof(struct frag_hdr), csum);
                        tcp_hdr->check = csum_ipv6_magic(&ih6->saddr, &ih6->daddr, plen - fr_flag*sizeof(struct frag_hdr), IPPROTO_TCP, csum);
                }
                else if((ntohs(ih4->frag_off)&IP_OFFSET)==0&&(ntohs(ih4->frag_off)&IP_MF)!= 0)
                        tcp_hdr->check = 0;
#ifdef __BIHDEBUG__
{
        struct tcp *tcp=(struct tcp *)tcp_hdr;
        ipv6_show(ih6);
        tcp_show(tcp);
}
#endif
                break;

        case IPPROTO_UDP:
                udp_hdr = (struct udphdr *)(src+hdr_len);
                if ((ntohs(ih4->frag_off) & IP_OFFSET) == 0) {
                        if ((ntohs(ih4->frag_off) & IP_MF) != 0) {
                                /* It's a first fragment */
                                if (udp_hdr->check == 0) {
                                        BIH_DEBUG("bih: transalte: First fragment of UDP with zero checksum - packet droped\n");
                                        BIH_DEBUG("bih: transalte: addr: %x src port: %d dst port: %d\n",
                                                   htonl(ih4->saddr), htons(udp_hdr->source), htons(udp_hdr->dest));
                                        lprintf(0,"transalte: First fragment of UDP with zero checksum - packet droped\n");
                                        lprintf(0,"transalte: addr: %x src port: %d dst port: %d\n",
                                                   htonl(ih4->saddr), htons(udp_hdr->source), htons(udp_hdr->dest));
                                        return -1;
                                }
                        }
                        else if (udp_hdr->check == 0)
                                fl_csum = 1;
                }

                udp_hdr = (struct udphdr *)(dst+sizeof(struct ipv6hdr)
                                                                        + fr_flag*sizeof(struct frag_hdr));
                memcpy((char *)udp_hdr, src+hdr_len, len - hdr_len);

                fl_csum=1;
                if (fl_csum && (!include_flag)) {
                        if(ntohs(ih4->frag_off) == IP_DF||ih4->frag_off==0)
                        {
                                udp_hdr->check = 0;
                                csum = 0;
                                csum = csum_partial((unsigned char *)udp_hdr, plen - fr_flag*sizeof(struct frag_hdr), csum);
                                udp_hdr->check = csum_ipv6_magic(&ih6->saddr, &ih6->daddr, plen -
                                                                                             fr_flag*sizeof(struct frag_hdr), IPPROTO_UDP, csum);
                        }
                        else if((ntohs(ih4->frag_off)&IP_OFFSET)==0&&(ntohs(ih4->frag_off)&IP_MF)!= 0)
                                udp_hdr->check = 0;
#ifdef __BIHDEBUG__
{
        struct udp *udp=(struct udp *)udp_hdr;
        ipv6_show(ih6);
        udp_show(udp);
        //array_show((unsigned char *)udp_hdr,len-hdr_len,1,16);
}
#endif
                }
                break;

        /* Discard packets with any other protocol */
        default:
                BIH_DEBUG("bih: ip4_ip6(): Unknown upper protocol - packet dropped.\n");
                lprintf(0,"ip4_ip6(): Unknown upper protocol - packet dropped.\n");
                return -1;
        }
        return 0;
}

static int ip6_ip4(struct net_device *dev,char *src, int len, char *dst, int include_flag)/*v6 address be translated*/
{
        unsigned int srcaddr,dstaddr;
        struct in6_addr addr6;
        struct ipv6hdr *ip6_hdr=(struct ipv6hdr *)src;
        struct iphdr *ip_hdr=(struct iphdr *)dst;
        int opts_len = 0;
        int icmperr = 1;
        int ntot_len = 0;
        int real_len;
        int len_delta;
        int ip6_payload_len;
        int inc_opts_len = 0;
        __u8 next_hdr;
        int ret;
        int addr_type;
        struct map_node *mapnode;
        struct udphdr *udp_hdr;
        unsigned char proto=0;

        BIH_DEBUG("bih: ip6_ip4 start\n");
#if 0
        //if it is the RA packet, let it go // test
        //we dont care the linklocal addr translate
        ret=ipv6_get_laddr(dev,&addr6);
        if(ret!=0)
        {
                BIH_DEBUG("bih: ip4_ip6 get ipv6 laddr failed\n");
                return(-1);
        }
        //if pkt dest ip is link local addr
        if(memcmp(&ip6_hdr->daddr,&addr6,sizeof(struct in6_addr))!=0)
        {
                BIH_DEBUG("bih: ip4_ip6 the dest addr is the linklocal addr\n");
                if(NEXTHDR_ICMP == ip6_hdr->nexthdr)
                {
                        BIH_DEBUG("bih: ip4_ip6 the pkt is the icmpv6 pkt\n");
                        return -2;
                }
        }
#endif
        addr_type=ipv6_addr_type(&ip6_hdr->daddr);
        ipv6_show(ip6_hdr);
        if (addr_type&IPV6_ADDR_MULTICAST||
                        addr_type&IPV6_ADDR_LOOPBACK||
                        addr_type&IPV6_ADDR_LINKLOCAL||
                        addr_type&IPV6_ADDR_SITELOCAL)
        {
                BIH_DEBUG("bih: ip6_ip4 addr_type=%di return\n",addr_type);
                return(-1);
        }
        addr_type=ipv6_addr_type(&ip6_hdr->saddr);
        if(addr_type&IPV6_ADDR_LINKLOCAL||
                        addr_type&IPV6_ADDR_SITELOCAL)
        {
                BIH_DEBUG("bih: ip6_ip4 addr_type=%d exit\n",addr_type);
                return(-1);
        }

        udp_hdr=(struct udphdr *)getipv6uppkg(src,&proto,NULL);
        if(proto==NEXTHDR_UDP&&ntohs(udp_hdr->source)==53)
        {
                BIH_DEBUG("bih: ip6_ip4 dns pkt exit\n");
                return(-1);
        }
        ret=ipv6_get_addr(dev,&addr6);
        if(ret!=0)
        {
                BIH_DEBUG("bih: ip6_ip4 dns pkt exit\n");
                return(-1);
        }
        ret=ipv4_get_addr(dev,&dstaddr);
        if(ret!=0)
        {
                BIH_DEBUG("bih: ip6_ip4 get addr failed\n");
                return(-1);
        }
        if(memcmp(&ip6_hdr->daddr,&addr6,sizeof(struct in6_addr))!=0)
        {

#ifdef __BIHDEBUG__
                char dstbuf[64],locbuf[64];
                inet_ntop6((unsigned char *)&ip6_hdr->daddr.s6_addr, dstbuf, sizeof(dstbuf)-1);
                inet_ntop6((unsigned char *)&addr6.s6_addr, locbuf, sizeof(locbuf)-1);
                lprintf(0,"Pkg dst(%s) will be forward\n",dstbuf,locbuf);
                BIH_DEBUG("bih: Pkg dst(%s) will be forward\n",dstbuf,locbuf);
#endif

                BIH_DEBUG("bih: ip6_ip4 pkt will be forward\n");
                return -1;
        }

        mapnode=map_find(ip6_hdr->saddr);
        if(mapnode)
                srcaddr=mapnode->id;

        else
        {
                srcaddr=pool_addr_assign(&ip6_hdr->saddr);
                if(srcaddr==0)
                {
                        BIH_DEBUG(0,"pool_addr_assign error\n");
                        lprintf(0,"pool_addr_assign error\n");
                        return(-1);
                }
                mapnode=(struct map_node *)1;
        }

#ifdef __BIHDEBUG__
        BIH_DEBUG("bih: ---------- %s ----------\n",mapnode?"466":"464");
        lprintf(0,"---------- %s ----------\n",mapnode?"466":"464");
        ipv6_show(ip6_hdr);
#endif
        if ( (len_delta = len - sizeof(struct ipv6hdr)) >= 0)
        {
                real_len = sizeof(struct iphdr);

                ip_hdr->frag_off = 0;
                ip_hdr->id = 0;

                next_hdr = ip6_hdr->nexthdr;

                if (next_hdr == NEXTHDR_HOP) {
                        if ( (len_delta - sizeof(struct ipv6_opt_hdr)) >= 0)
                        {
                                struct ipv6_opt_hdr *ip6h =
                                        (struct ipv6_opt_hdr *)(src+sizeof(struct ipv6hdr) + opts_len);
                                if ( (len_delta -= ip6h->hdrlen*8 + 8) >= 0)
                                {
                                        opts_len += ip6h->hdrlen*8 + 8;
                                        next_hdr = ip6h->nexthdr;
                                }
                                else
                                {
                                        BIH_DEBUG("bih: ip6_ip4(): hop_by_hop header error, packet droped\n");
                                        lprintf(0,"ip6_ip4(): hop_by_hop header error, packet droped\n");
                                        return -1;
                                }
                        }
                }

                if (len_delta > 0)
                {
                        while(next_hdr != NEXTHDR_ICMP && next_hdr != NEXTHDR_TCP
                                  && next_hdr != NEXTHDR_UDP)
                        {
                                /* Destination options header */
                                if (next_hdr == NEXTHDR_DEST)
                                {
                                        if ( (len_delta - sizeof(struct ipv6_opt_hdr)) >= 0)
                                        {
                                                struct ipv6_opt_hdr *ip6d =
                                                        (struct ipv6_opt_hdr *)(src + sizeof(struct ipv6hdr) + opts_len);
                                                if ( (len_delta -= ip6d->hdrlen*8 + 8) >= 0)
                                                {
                                                        opts_len += ip6d->hdrlen*8 + 8;
                                                        next_hdr = ip6d->nexthdr;
                                                }
                                        }
                                        else
                                        {
                                                BIH_DEBUG("bih: ip6_ip4(): destination header error, packet droped");
                                                lprintf(0,"ip6_ip4(): destination header error, packet droped");
                                                return -1;
                                        }
                                }
                                else if (next_hdr == NEXTHDR_ROUTING)
                                {
                                        if ( (len_delta - sizeof(struct ipv6_rt_hdr)) >= 0)
                                        {
                                                struct ipv6_rt_hdr *ip6rt =
                                                        (struct ipv6_rt_hdr *)(src+sizeof(struct ipv6hdr) + opts_len);
                                                if (ip6rt->segments_left != 0) {
                                                        BIH_DEBUG("bih: ip6_ip4(): routing header type != 0\n");
                                                        lprintf(0,"ip6_ip4(): routing header type != 0\n");
                                                        return -1;
                                                }
                                                if ( (len_delta -= ip6rt->hdrlen*8 + 8) >= 0)
                                                {
                                                        opts_len += ip6rt->hdrlen*8 + 8;
                                                        next_hdr = ip6rt->nexthdr;
                                                }
                                                else
                                                {
                                                        BIH_DEBUG("bih: ip6_ip4(): routing header error, packet droped");
                                                        lprintf(0,"ip6_ip4(): routing header error, packet droped");
                                                        return -1;
                                                }
                                        }
                                }
                                else if (next_hdr == NEXTHDR_FRAGMENT)
                                {
                                        if ( (len_delta -= sizeof(struct frag_hdr)) >= 0)
                                        {
                                                struct frag_hdr *ip6f =
                                                        (struct frag_hdr *)(src+sizeof(struct ipv6hdr)+opts_len);

                                                opts_len += sizeof(struct frag_hdr);      /* Frag Header Length = 8 */
                                                ip_hdr->id = htons(ntohl(ip6f->identification)); /* ID field */
                                                ip_hdr->frag_off = htons((ntohs(ip6f->frag_off) & IP6F_OFF_MASK) >> 3);
                                                                                                                   /* fragment offset */
                                                ip_hdr->frag_off = htons(ntohs(ip_hdr->frag_off) |
                                                                                         ((ntohs(ip6f->frag_off) & IP6F_MORE_FRAG) << 13));
                                                                                                                  /* more fragments flag */
                                                next_hdr = ip6f->nexthdr;
                                        }
                                        else
                                        {
                                                BIH_DEBUG("bih: ip6_ip4(): fragment header error, packet droped");
                                                lprintf(0,"ip6_ip4(): fragment header error, packet droped");
                                                return -1;
                                        }
                                }
                                else if (next_hdr == NEXTHDR_NONE)
                                {
                                        break;
                                }
                                else if (next_hdr == NEXTHDR_ESP || next_hdr == NEXTHDR_AUTH)
                                {
                                        BIH_DEBUG("bih: ip6_ip4(): cannot translate AUTH or ESP extention header, packet dropped\n");
                                        lprintf(0,"ip6_ip4(): cannot translate AUTH or ESP extention header, packet dropped\n");
                                        return -1;
                                }
                                else if (next_hdr == NEXTHDR_IPV6)
                                {
                                        BIH_DEBUG("bih: ip6_ip4(): cannot translate IPv6-IPv6 packet, packet dropped\n");
                                        lprintf(0,"ip6_ip4(): cannot translate IPv6-IPv6 packet, packet dropped\n");
                                        return -1;
                                }
                                else if (next_hdr == 0)
                                {
                                        BIH_DEBUG("bih: ip6_ip4(): NEXTHDR in extention header = 0, packet dropped\n");
                                        lprintf(0,"ip6_ip4(): NEXTHDR in extention header = 0, packet dropped\n");
                                        return -1;
                                }
                                else
                                {
                                        BIH_DEBUG("bih: ip6_ip4(): cannot translate extention header = %d, packet dropped\n", next_hdr);
                                        lprintf(0,"ip6_ip4(): cannot translate extention header = %d, packet dropped\n", next_hdr);
                                        return -1;
                                }
                        }
                }
        }
        else
        {
           BIH_DEBUG("bih: ip6_ip4(): error packet len, packet dropped.\n");
           lprintf(0,"ip6_ip4(): error packet len, packet dropped.\n");
           return -1;
        }

        ip_hdr->version = IPVERSION;
        ip_hdr->ihl = 5;

        if (tos_ignore_flag)
                ip_hdr->tos = 0;
        else {
                ip_hdr->tos = ip6_hdr->priority << 4;
                ip_hdr->tos = ip_hdr->tos | (ip6_hdr->flow_lbl[0] >> 4);
        }

        ip6_payload_len = ntohs(ip6_hdr->payload_len);

        if (ip6_payload_len == 0)
                ntot_len = 0;
        else
                ntot_len = ip6_payload_len + IP4_IP6_HDR_DIFF - opts_len;

        ip_hdr->tot_len = htons(ntot_len);
        ip_hdr->ttl = ip6_hdr->hop_limit;
        ip_hdr->protocol = next_hdr;

        ip_hdr->saddr = srcaddr;
        ip_hdr->daddr = dstaddr;

        ip_hdr->check = 0;
        ip_hdr->check = ip_fast_csum((unsigned char *)ip_hdr, ip_hdr->ihl);

        if (len_delta > 0)
        {
                if (next_hdr == NEXTHDR_ICMP)
                {
                        struct icmp6hdr *icmp6_hdr;
                        struct icmphdr *icmp_hdr;

                        if ((len_delta -= sizeof(struct icmp6hdr)) >= 0)
                        {
                                icmp6_hdr = (struct icmp6hdr *)(src + sizeof(struct ipv6hdr) + opts_len);
                                icmp_hdr = (struct icmphdr *)(dst + sizeof(struct iphdr));

                                real_len += len_delta + sizeof(struct icmphdr);

                                ip_hdr->protocol = IPPROTO_ICMP;

                                if (include_flag) {
                                        if (icmp6_hdr->icmp6_type != ICMPV6_ECHO_REQUEST)
                                        {
                                                BIH_DEBUG("bih: ip6_ip4(): included ICMPv6 in ICMPv6 Error message, packet dropped\n");
                                                lprintf(0,"ip6_ip4(): included ICMPv6 in ICMPv6 Error message, packet dropped\n");
                                                return -1;
                                        }
                                }

                                switch (icmp6_hdr->icmp6_type)
                                {
                                case ICMPV6_DEST_UNREACH: /* Type 1 */
                                        icmp_hdr->type = ICMP_DEST_UNREACH; /* to Type 3 */
                                        icmp_hdr->un.echo.id = 0;
                                        icmp_hdr->un.echo.sequence = 0;
                                        switch (icmp6_hdr->icmp6_code)
                                        {
                                        case ICMPV6_NOROUTE: /* Code 0 */
                                        case ICMPV6_NOT_NEIGHBOUR: /* Code 2 */
                                        case ICMPV6_ADDR_UNREACH: /* Code 3  */
                                                icmp_hdr->code = ICMP_HOST_UNREACH; /* To Code 1 */
                                                break;
                                        case ICMPV6_ADM_PROHIBITED: /* Code 1 */
                                                icmp_hdr->code = ICMP_HOST_ANO; /* To Code 10 */
                                                break;
                                        case ICMPV6_PORT_UNREACH: /* Code 4 */
                                                icmp_hdr->code = ICMP_PORT_UNREACH; /* To Code 3 */

                                                break;
                                        default:            /* discard any other codes */
                                                BIH_DEBUG("bih: ip6_ip4(): Unknown ICMPv6 Type %d Code %d - packet dropped.\n",
                                                           ICMPV6_DEST_UNREACH, icmp6_hdr->icmp6_code);
                                                lprintf(0,"ip6_ip4(): Unknown ICMPv6 Type %d Code %d - packet dropped.\n",
                                                           ICMPV6_DEST_UNREACH, icmp6_hdr->icmp6_code);
                                                return -1;
                                        }
                                        break;
                                case ICMPV6_PKT_TOOBIG: /* Type 2 */
                                        icmp_hdr->type = ICMP_DEST_UNREACH; /* to Type 3  */
                                        icmp_hdr->code = ICMP_FRAG_NEEDED; /*  to Code 4 */
                                        icmp_hdr->un.frag.mtu = (__u16) icmp6_hdr->icmp6_mtu;
                                        break;
                                case ICMPV6_TIME_EXCEED:
                                        icmp_hdr->type = ICMP_TIME_EXCEEDED; /* to Type 11 */
                                        icmp_hdr->code = icmp6_hdr->icmp6_code; /* Code unchanged */
                                        break;
                                case ICMPV6_PARAMPROB:
                                        switch (icmp6_hdr->icmp6_code) {
                                        case ICMPV6_UNK_NEXTHDR: /* Code 1 */
                                                icmp_hdr->type = ICMP_DEST_UNREACH; /* to Type 3 */
                                                icmp_hdr->code = ICMP_PROT_UNREACH; /* to Code 2 */
                                                break;
                                        default: /* if Code != 1 */
                                                icmp_hdr->type = ICMP_PARAMETERPROB; /* to Type 12 */
                                                icmp_hdr->code = 0; /* to Code 0 */
                                                switch (ntohl(icmp6_hdr->icmp6_pointer))
                                                {
                                                case 0: /* IPv6 Version -> IPv4 Version */
                                                        icmp_hdr->un.echo.id = 0;
                                                        break;
                                                case 4: /* IPv6 PayloadLength -> IPv4 Total Length */
                                                        icmp_hdr->un.echo.id = 0x0002; /* 2 */
                                                        break;
                                                case 6: /* IPv6 Next Header-> IPv4 Protocol */
                                                        icmp_hdr->un.echo.id = 0x0009; /* 9 */
                                                        break;
                                                case 7: /* IPv6 Hop Limit -> IPv4 TTL */
                                                        icmp_hdr->un.echo.id = 0x0008; /* 8 */
                                                        break;
                                                case 8: /* IPv6 Src addr -> IPv4 Src addr */
                                                        icmp_hdr->un.echo.id = 0x000c; /* 12 */
                                                        break;
                                                case 24: /* IPv6 Dst addr -> IPv4 Dst addr*/
                                                        icmp_hdr->un.echo.id = 0x0010; /* 16 */
                                                        break;
                                                default: /* set all ones in other cases */
                                                        icmp_hdr->un.echo.id = 0xff;
                                                        break;
                                                }
                                                break;
                                        }
                                        break;

                                case ICMPV6_ECHO_REQUEST:
                                        icmperr = 0;        /* not error ICMP message */
                                        icmp_hdr->type = ICMP_ECHO; /* to Type 8 */
                                        icmp_hdr->code = 0; /* to Code 0 */
                                        icmp_hdr->un.echo.id = icmp6_hdr->icmp6_identifier;
                                        icmp_hdr->un.echo.sequence = icmp6_hdr->icmp6_sequence;
                                        if (len_delta > 0)
                                                memcpy(((char *)icmp_hdr) + sizeof(struct icmphdr),
                                                           ((char *)icmp6_hdr) + sizeof(struct icmp6hdr), len_delta);
                                        break;
                                case ICMPV6_ECHO_REPLY:
                                        icmperr = 0;        /* not error ICMP message */
                                        icmp_hdr->type = ICMP_ECHOREPLY; /* to Type 0 */
                                        icmp_hdr->code = 0; /* to Code 0 */
                                        icmp_hdr->un.echo.id = icmp6_hdr->icmp6_identifier;
                                        icmp_hdr->un.echo.sequence = icmp6_hdr->icmp6_sequence;
                                        if (len_delta > 0)
                                                memcpy(((char *)icmp_hdr) + sizeof(struct icmphdr),
                                                           ((char *)icmp6_hdr) + sizeof(struct icmp6hdr), len_delta);
                                        break;
                                default:
                                        BIH_DEBUG("bih: ip6_ip4(): unknown ICMPv6 Type %d, packet dropped.\n", icmp6_hdr->icmp6_type);
                                        lprintf(0,"ip6_ip4(): unknown ICMPv6 Type %d, packet dropped.\n", icmp6_hdr->icmp6_type);
                                        return -1;
                                }

                                if (icmperr)
                                {
                                        if (len_delta >= sizeof(struct ipv6hdr))
                                        {
                                                if((inc_opts_len = ip6_ip4(dev,(char *)icmp6_hdr + sizeof(struct icmp6hdr), len_delta,
                                                                                           (char *)icmp_hdr + sizeof(struct icmphdr), 1)) == -1) {
                                                        BIH_DEBUG("bih: ip6_ip4(): incorrect translation of ICMPv6 Error message, packet dropped\n");
                                                        lprintf(0,"ip6_ip4(): incorrect translation of ICMPv6 Error message, packet dropped\n");
                                                        return -1;
                                                }
                                                if (ntot_len != 0)
                                                        ip_hdr->tot_len = htons(ntot_len - inc_opts_len - IP4_IP6_HDR_DIFF);
                                                real_len = real_len - inc_opts_len - IP4_IP6_HDR_DIFF;
                                        }
                                        else if (len_delta > 0)
                                        {
                                                memcpy(((char *)icmp_hdr) + sizeof(struct icmphdr),
                                                           ((char *)icmp6_hdr) + sizeof(struct icmp6hdr), len_delta);
                                        }
                                }

                                ip_hdr->check = 0;
                                ip_hdr->check = ip_fast_csum((unsigned char *)ip_hdr, ip_hdr->ihl);

                                if (ntot_len != 0)
                                {
                                        icmp_hdr->checksum = 0;
                                        icmp_hdr->checksum = ip_compute_csum((unsigned char *)icmp_hdr, ntohs(ip_hdr->tot_len)
                                                                                             - sizeof(struct iphdr));
                                }
                        }
                        else
                        {
                                BIH_DEBUG("bih: ip6_ip4(): error length ICMP packet, packet dropped.\n");
                                lprintf(0,"ip6_ip4(): error length ICMP packet, packet dropped.\n");
                                return -1;
                        }

                }
                else {
                        unsigned int csum;
                        struct udphdr *udp_hdr;
                        struct tcphdr *tcp_hdr;
                        real_len += len_delta;
                        memcpy(dst+sizeof(struct iphdr), src + sizeof(struct ipv6hdr) + opts_len, len_delta);
                        if (next_hdr == NEXTHDR_TCP)
                        {
                                tcp_hdr = (struct tcphdr *)(dst+sizeof(struct iphdr));
                                if(ntohs(ip_hdr->frag_off) == IP_DF||ip_hdr->frag_off==0)
                                {
                                        tcp_hdr->check = 0;
                                        csum = 0;
                                        csum = csum_partial((unsigned char *)tcp_hdr, len_delta, csum);
                                        tcp_hdr->check = csum_tcpudp_magic(ip_hdr->saddr, ip_hdr->daddr, len_delta, IPPROTO_TCP, csum);
                                }
                                else if((ntohs(ip_hdr->frag_off)&IP_OFFSET)==0&&(ntohs(ip_hdr->frag_off)&IP_MF)!= 0)
                                        tcp_hdr->check = 0;
#ifdef __BIHDEBUG__
{
        struct tcp *tcp=(struct tcp *)tcp_hdr;
        struct ip *ip=(struct ip *)ip_hdr;
        ip_show(ip);
        tcp_show(tcp);
}
#endif
                        }
                        else if (next_hdr == NEXTHDR_UDP)
                        {
                                udp_hdr = (struct udphdr *)(dst+sizeof(struct iphdr));
                                if(ntohs(ip_hdr->frag_off) == IP_DF||ip_hdr->frag_off==0)
                                {
                                        udp_hdr->check = 0;
                                        csum = 0;
                                        csum = csum_partial((unsigned char *)udp_hdr, len_delta, csum);
                                        udp_hdr->check = csum_tcpudp_magic(ip_hdr->saddr, ip_hdr->daddr, len_delta, IPPROTO_UDP, csum);
                                }
                                else if((ntohs(ip_hdr->frag_off)&IP_OFFSET)==0&&(ntohs(ip_hdr->frag_off)&IP_MF)!= 0)
                                        udp_hdr->check = 0;
#ifdef __BIHDEBUG__
{
        struct udp *udp=(struct udp *)udp_hdr;
        struct ip *ip=(struct ip *)ip_hdr;
        ip_show(ip);
        udp_show(udp);
        //array_show(dst+sizeof(struct iphdr), len_delta,1,16);
}
#endif
                        }
                }
        }
        if (include_flag)
                return opts_len;
        else
                return real_len;
}

static int ip4_fragment(struct sk_buff *skb, int len, int hdr_len, struct net_device *dev)
{
        struct sk_buff *skb2 = NULL;       /* pointer to new struct sk_buff for transleded packet */
        char buff[FRAG_BUFF_SIZE+hdr_len]; /* buffer to form new fragment packet */
        char *cur_ptr = skb->data+hdr_len; /* pointter to current packet data with len = frag_len */
        struct iphdr *ih4 = (struct iphdr *) skb->data;
        struct iphdr *new_ih4 = (struct iphdr *) buff; /* point to new IPv4 hdr */
        struct ethhdr *new_eth_h;   /* point to ether hdr, need to set hard header data in fragment */
        int data_len = len - hdr_len; /* origin packet data len */
        int rest_len = data_len;    /* rest data to fragment */
        int frag_len = 0;           /* current fragment len */
        int last_frag = 0;          /* last fragment flag, if = 1, it's last fragment */
        int flag_last_mf = 0;
        int ret;
        __u16 new_id = 0;           /* to generate identification field */
        __u16 frag_offset = 0;      /* fragment offset */
        unsigned int csum;
        unsigned short udp_len;

        BIH_DEBUG("bih: ip4_fragment start\n");
        if ((ntohs(ih4->frag_off) & IP_MF) == 0 )
                /* it's a case we'll clear MF flag in our last packet */
                flag_last_mf = 1;

        if (ih4->protocol == IPPROTO_UDP) {
                if ( (ntohs(ih4->frag_off) & IP_OFFSET) == 0) {
                        struct udphdr *udp_hdr = (struct udphdr *)((char *)ih4 + hdr_len);
                        if (!flag_last_mf) {
                                if (udp_hdr->check == 0) {
                                        /* it's a first fragment with ZERO checksum and we drop packet */
                                        printk("translate: First fragment of UDP with zero checksum - packet droped\n");
                                        printk("translate: addr: %x src port: %d dst port: %d\n",
                                                   htonl(ih4->saddr), htons(udp_hdr->source), htons(udp_hdr->dest));
                                        return -1;
                                }
                        }
                        else if (udp_hdr->check == 0) {
                                /* Calculate UDP checksum only if it's not fragment */
                                udp_len = ntohs(udp_hdr->len);
                                csum = 0;
                                csum = csum_partial((unsigned char *)udp_hdr, udp_len, csum);
                                udp_hdr->check = csum_tcpudp_magic(ih4->saddr, ih4->daddr, udp_len, IPPROTO_UDP, csum);
                        }
                }
        }
        frag_offset = ntohs(ih4->frag_off) & IP_OFFSET;
        new_id = ih4->id;
        while(1) {
                if (rest_len <= FRAG_BUFF_SIZE) {
                        /* it's last fragmen */
                        frag_len = rest_len; /* rest data */
                        last_frag = 1;
                }
                else
                        frag_len = FRAG_BUFF_SIZE;
                /* copy IP header to buffer */
                memcpy(buff, skb->data, hdr_len);
                /* copy data to buffer with len = frag_len */
                memcpy(buff + hdr_len, cur_ptr, frag_len);
                /* set id field in new IPv4 header*/
                new_ih4->id = new_id;
                /* is it last fragmet */
                if(last_frag && flag_last_mf)
                        /* clear MF flag */
                        new_ih4->frag_off = htons(frag_offset & (~IP_MF));
                else
                        /* set MF flag */
                        new_ih4->frag_off = htons(frag_offset | IP_MF);
                /* change packet total length */
                new_ih4->tot_len = htons(frag_len+hdr_len);
                /* rebuild the header checksum (IP needs it) */
                new_ih4->check = 0;
                new_ih4->check = ip_fast_csum((unsigned char *)new_ih4,new_ih4->ihl);
                /* Allocate new sk_buff to compose translated packet */
                skb2 = dev_alloc_skb(frag_len+hdr_len+dev->hard_header_len+IP4_IP6_HDR_DIFF+IP6_FRAGMENT_SIZE);
                if (!skb2) {
                        printk(KERN_DEBUG "%s: alloc_skb failure - packet dropped.\n", dev->name);
                        dev_kfree_skb(skb2);
                        return -1;
                }
                skb_put(skb2, frag_len+hdr_len+dev->hard_header_len+IP4_IP6_HDR_DIFF+IP6_FRAGMENT_SIZE);
                new_eth_h = (struct ethhdr *)skb2->data;
                new_eth_h->h_proto = htons(ETH_P_IPV6);
                skb_reset_mac_header(skb2);
                skb_pull(skb2, dev->hard_header_len);
                skb_reset_network_header(skb2);
                skb2->protocol = htons(ETH_P_IPV6);
                /* call translation function */
                if ( ip4_ip6(dev,buff, frag_len+hdr_len, skb2->data, 0) == -1) {
//                        lprintf(0,"ip4_ip6 error\n");
                        dev_kfree_skb(skb2);
                        return -1;
                }
                /*
                 * Set needed fields in new sk_buff
                 */
                skb2->dev = dev;
                skb2->ip_summed = CHECKSUM_UNNECESSARY;
                skb2->pkt_type = PACKET_HOST;

                /* send packet to upper layer */
//                ret=ipv6_rcv(skb2,dev,NULL,NULL);
                ret=netif_rx(skb2);
                if(ret!=0)
                {
                        BIH_DEBUG("bih: ipv6_rcv error\n");
                        lprintf(0,"ipv6_rcv error\n");
                        dev_kfree_skb(skb2);
                        return -1;
                }
                /* exit if it was last fragment */
                if (last_frag)
                        break;

                /* correct current data pointer */
                cur_ptr += frag_len;
                /* rest data len */
                rest_len -= frag_len;
                /* current fragment offset */
                frag_offset = (frag_offset*8 + frag_len)/8;
        }
        return 0;
}

int bih_xmit(struct sk_buff *skb,struct net_device *dev)
{
        struct sk_buff *skb2 = NULL, *skb3=NULL;
        int ret,new_packet_len,len,skb_delta = 0;
        static char *new_packet_buff;
        struct ethhdr *eth_h;
        struct in6_addr addr;

        BIH_DEBUG("bih: bih_xmit start\n");
        if (skb == NULL || dev==NULL)
        {
                return -1;
        }
/*
        if(skb->dev==bihdev||BIHMODE!=1)
                return(-1);
*/
        BIH_DEBUG("bih: bih_xmit get ipv6 addr start\n");
        ret=ipv6_get_addr(dev,&addr);
        if(ret!=0)
        {
#if 0 //test
                BIH_DEBUG("bih: bih_xmit get ipv6 addr failed\n");
                if(0 == ipv6_get_laddr(dev, &addr))
                {
                        BIH_DEBUG("bih: bih_xmit get ipv6 linklocal addr success\n");
                        ret=netif_rx(skb);
                        if(ret!=0)
                                BIH_DEBUG("bih: bih_xmitip_rcv|ip6_rcv error(ret=%d)\n",ret);
                        else
                                kfree_skb(skb);
                        return(ret);
                }
#endif
                BIH_DEBUG("bih: bih_xmit get ipv6 linklocal addr failed\n");
                return(-1);
        }
        BIH_DEBUG("bih: bih_xmit get ipv6 addr end\n");

        if(new_packet_buff==NULL)
        {
                new_packet_buff=kmalloc(2048,GFP_KERNEL);
                if(new_packet_buff==NULL)
                {
                        BIH_DEBUG("bih: kmalloc error\n");
                        lprintf(0,"kmalloc error\n");
                        return(-1);
                }
        }

        dev->trans_start = jiffies;
        if (ntohs(skb->protocol) == ETH_P_IP)
        {
                int hdr_len;
                int data_len;
                struct iphdr *ih4;
                struct icmphdr *icmp_hdr;

                BIH_DEBUG("bih: bih_xmit ip4\n");
                ih4 = (struct iphdr *)skb->data; /* point to incoming packet's IPv4 header */
                if (skb->len != ntohs(ih4->tot_len)) {
                        lprintf(0,"skb error\n");
                        BIH_DEBUG("bih: bih_xmit skb error");
                        return -1;
                }
                len = skb->len;
                hdr_len = (int)(ih4->ihl * 4);
                data_len = len - hdr_len;

//                if ( (ntohs(ih4->frag_off) & IP_DF) == 0 )
                {
                        if ( data_len > FRAG_BUFF_SIZE )
//                        if ( data_len > 1440 )
                        {
                                if(skb_is_nonlinear(skb))
                                {
                                        skb3=skb_copy(skb, GFP_ATOMIC);
                                        if(skb3)
                                        {
                                                skb3->sk=skb->sk;
                                        }
                                }
                                else
                                        skb3=skb;
                                len = skb3->len;
                                ret=ip4_fragment(skb3, len, hdr_len, dev);
                                if(skb3!=skb)
                                        dev_kfree_skb(skb3);
                                if(ret==0)
                                        dev_kfree_skb(skb);
                                BIH_DEBUG("bih: bih_xmit data_len=%d is bigger than FRAG_BUFF_SIZE(%d)", data_len, FRAG_BUFF_SIZE);
                                return ret;
                        }
                }
                if ( ntohs(ih4->frag_off) == IP_DF )
                        skb_delta = IP4_IP6_HDR_DIFF;
                else
                        skb_delta = IP4_IP6_HDR_DIFF + IP6_FRAGMENT_SIZE;
                if ( ih4->protocol == IPPROTO_ICMP) {
                        icmp_hdr = (struct icmphdr *) (skb->data+hdr_len);
                        if ( icmp_hdr->type != ICMP_ECHO && icmp_hdr->type != ICMP_ECHOREPLY) {
                                skb_delta += IP4_IP6_HDR_DIFF;
                        }
                }
                skb2 = dev_alloc_skb(len+dev->hard_header_len+skb_delta);
                if (!skb2) {
                        lprintf(0,"dev_alloc_skb error\n");
                        BIH_DEBUG("bih: bih_xmit dev_alloc_skb failed\n");
                        return -1;
                }
                skb_put(skb2, len+dev->hard_header_len+skb_delta);
                eth_h = (struct ethhdr *)skb2->data;
                eth_h->h_proto = htons(ETH_P_IPV6);
                skb_reset_mac_header(skb2);
                skb_pull(skb2,dev->hard_header_len);
//                skb2->h.raw = skb2->nh.raw = skb2->data;
                skb_reset_network_header(skb2);
                skb2->protocol = htons(ETH_P_IPV6);

                if(skb_is_nonlinear(skb))
                {
                        skb3=skb_copy(skb, GFP_ATOMIC);
                        if(skb3)
                        {
                                skb3->sk=skb->sk;
                        }
                }
                else
                        skb3=skb;
                if (ip4_ip6(dev,skb3->data, len, skb2->data, 0) == -1 ) {
                        if(skb3!=skb)
                                dev_kfree_skb(skb3);
                        dev_kfree_skb(skb2);
                        BIH_DEBUG("bih: bih_xmit ip4 to ip6 failed\n");
                        return -1;
                }
                if(skb3!=skb)
                        dev_kfree_skb(skb3);
                skb2->dev = bihdev;
        }
        else if (ntohs(skb->protocol) == ETH_P_IPV6)
        {
                BIH_DEBUG("bih: bih_xmit ip6\n");
                len = skb->len;
                if(skb_is_nonlinear(skb))
                {
                        skb3=skb_copy(skb, GFP_ATOMIC);
                        if(skb3)
                        {
                                skb3->sk=skb->sk;
                        }
                }
                else
                        skb3=skb;
                new_packet_len = ip6_ip4(dev,skb3->data, len, new_packet_buff, 0);
                if(new_packet_len==-1)
                {
                        if(skb3!=skb)
                                dev_kfree_skb(skb3);
                        BIH_DEBUG("bih: bih_xmit ip6 to ip4 failed\n");
                        return(-1);
                }
#if 0
                else if(new_packet_len=-2) //add
                {
                        ret = netif_rx(skb3);
                        if(ret!=0)
                               BIH_DEBUG("bih: ip_rcv|ip6_rcv icmpv6 error(ret=%d)\n",ret);
                               else
                                kfree_skb(skb);
                        return(ret);
                }
#endif
                if(skb3!=skb)
                        dev_kfree_skb(skb3);
                skb2 = dev_alloc_skb(new_packet_len + dev->hard_header_len);
                if (!skb2)
                {
                        BIH_DEBUG("bih: dev_alloc_skb error\n");
                        lprintf(0,"dev_alloc_skb error\n");
                        return(-1);
                }
                skb_put(skb2, new_packet_len + dev->hard_header_len);
                eth_h = (struct ethhdr *)skb2->data;
                eth_h->h_proto = htons(ETH_P_IP);
                skb_reset_mac_header(skb2);
                skb_pull(skb2, dev->hard_header_len);
                memcpy(skb2->data, new_packet_buff, new_packet_len);
//                skb2->h.raw = skb2->nh.raw = skb2->data;
                skb_reset_network_header(skb2);
                skb_reset_transport_header(skb2);
                skb2->mac_len = skb2->network_header - skb2->mac_header;
                skb2->protocol = htons(ETH_P_IP);
                skb2->dev=skb->dev;
        }
        else {
                BIH_DEBUG("bih: bih_xmit skb->protocol=%d exception\n",ntohs(skb->protocol));
                return(-1);
        }

        skb2->pkt_type = PACKET_HOST;
        skb2->dev = dev;
        skb2->ip_summed = CHECKSUM_UNNECESSARY;
        if (skb->sk)
                skb_set_owner_w(skb2, skb->sk);

        BIH_DEBUG("bih: send data to netif_rx\n");
        ret=netif_rx(skb2);
/*
        if (ntohs(skb2->protocol) == ETH_P_IP)
                ret=ip_rcv(skb2,dev,NULL,NULL);
        else
                ret=ipv6_rcv(skb2,dev,NULL,NULL);
*/
        if(ret!=0)
                printk("bih: ip_rcv|ip6_rcv error(ret=%d)\n",ret);
        else
                kfree_skb(skb);
        return(ret);
}
