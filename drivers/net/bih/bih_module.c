/*
 * Bump in the Host (BIH)
 * http://code.google.com/p/bump-in-the-host/source/
 * ----------------------------------------------------------
 *
 *  Copyrighted (C) 2010,2011 by the China Mobile Communications
 *  Corporation; see the COPYRIGHT file for full details.
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
#include <linux/module.h>                /* Needed by all modules */
#include <linux/kernel.h>                /* Needed for KERN_ALERT */
#include <linux/init.h>                        /* Needed for the macros */
#include <linux/list.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>
#include <linux/proc_fs.h>        /* Necessary because we use proc fs */
#include <linux/skbuff.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/nsproxy.h>
#include <net/route.h>
#include <net/ip6_route.h>
#include <net/ip_fib.h>
#include <net/ip6_fib.h>
#include <net/flow.h>
#include <net/addrconf.h>
#include <asm/uaccess.h>                /* for get_user and put_user */
#include "translate.h"
#include "voslist.h"
#include "pool.h"
#include "map.h"
#define NIPQUAD(addr) \
        ((unsigned char *)&addr)[0], \
        ((unsigned char *)&addr)[1], \
        ((unsigned char *)&addr)[2], \
        ((unsigned char *)&addr)[3]
#define BIH_OPS_BASIC  128
#define BIH_SET        BIH_OPS_BASIC
#define BIH_GET        BIH_OPS_BASIC
#define BIH_MAX        BIH_OPS_BASIC+1
#define MODULE_NAME "bih"
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ZHAODAYI");
MODULE_DESCRIPTION("BIH");
MODULE_INFO(vermagic,VERMAGIC_STRING);

extern int BIHMODE,NETWORKTYPE;
extern unsigned int PRIVATEADDR;
extern char *inet_ntop6(unsigned char *src, char *dst, size_t size);
extern int inet_pton6(char *src,unsigned char *dst);
extern int bih_xmit(struct sk_buff *skb,struct net_device *dev);
static struct proc_dir_entry *proc_dir=NULL,*proc_file_bis,*proc_file_mode,*proc_file_pool,*proc_file_map,*proc_file_network,*proc_file_private;
struct net_device *bihdev=NULL;

#define RT_OPS_BASIC  130
#define RT_QUERY                        RT_OPS_BASIC
#define SP_QUERY      RT_OPS_BASIC+1
#define RT_MAX        RT_OPS_BASIC+2

#ifndef CONFIG_IP_MULTIPLE_TABLES
struct fib_table *dev_fib_get_table(struct net *net, u32 id)
{
        struct hlist_head *ptr;

        ptr = id == RT_TABLE_LOCAL ?
                &net->ipv4.fib_table_hash[0] :
                &net->ipv4.fib_table_hash[1];
        return hlist_entry(ptr->first, struct fib_table, tb_hlist);
}

int dev_fib_lookup(struct flowi *flp,struct fib_result *res)
{
        struct fib_table *table;
        struct net *net=&init_net;
        table = dev_fib_get_table(net, RT_TABLE_LOCAL);
        if (!table->tb_lookup(table, flp, res))
                return 0;

        table = dev_fib_get_table(net, RT_TABLE_MAIN);
        if (!table->tb_lookup(table, flp, res))
                return 0;
        return -ENETUNREACH;
}
#else
struct fib_table *dev_fib_get_table(struct net *net, u32 id)
{
        struct fib_table *tb;
        struct hlist_head *head;
        unsigned int h;

        if (id == 0)
                id = RT_TABLE_MAIN;
        h = id & (FIB_TABLE_HASHSZ - 1);

        rcu_read_lock();
        head = &net->ipv4.fib_table_hash[h];
        hlist_for_each_entry_rcu(tb, head, tb_hlist) {
                if (tb->tb_id == id) {
                        rcu_read_unlock();
                        return tb;
                }
        }
        rcu_read_unlock();
        return NULL;
}

int dev_fib_lookup(struct flowi *flp, struct fib_result *res)
{
        struct fib_lookup_arg arg = {
                .result = res,
        };
        struct net *net=&init_net;
        int err;

        err = fib_rules_lookup(net->ipv4.rules_ops, flp, 0, &arg);
#ifdef CONFIG_IP_ROUTE_CLASSID
        if (arg.rule)
                res->tclassid = ((struct fib4_rule *)arg.rule)->tclassid;
        else
                res->tclassid = 0;
#endif
        return err;
}
#endif

int dev_get_addr6(struct net_device *dev, struct in6_addr *addr)
{
        struct inet6_dev *idev;
        int err = -EADDRNOTAVAIL;

        if ((idev = __in6_dev_get(dev)) != NULL) {
                struct inet6_ifaddr *ifp;

                read_lock_bh(&idev->lock);
                 list_for_each_entry(ifp, &idev->addr_list, if_list){
                        if (ifp->scope != IFA_LINK/* && !(ifp->flags&IFA_F_TENTATIVE)*/)
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

int dev_get_addr(struct net_device *dev, unsigned int *addr)
{
        struct in_device *in_dev;
        struct in_ifaddr *ifa = NULL;

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

char *ipaddr_show(unsigned char *addr,int len)
{
        static char strbuf[128];
        if(len==4)
                inet_ntop4(addr,strbuf,sizeof(strbuf)-1);
        else if(len==16)
                inet_ntop6(addr,strbuf,sizeof(strbuf)-1);
        else
                return(NULL);
        return(strbuf);
}

static int rt_query(struct sock *sk, int cmd, void *user, int *len)
{
        switch(cmd)
        {
                case RT_QUERY:
                {
                        struct
                        {
                                unsigned char dst[16];
                                unsigned char src[16];
                                unsigned char gw[16];
                                char dev[32];
                        }argu;
                        struct flowi fl;
                        struct fib_result res;
                        struct net_device *dev = NULL;
                        struct rtable *rp;
                        int ret;
                        memset(&argu,0,sizeof(argu));
                        memset(&fl,0,sizeof(fl));
                        memset(&res,0,sizeof(res));
                        memset(&rp,0,sizeof(rp));
                        copy_from_user(&argu, user, sizeof(argu));
                        if(memcmp(argu.dst+4,"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",12)==0)
                        {
                                unsigned int addr;
                                memcpy(&addr,argu.dst,4);
                                fl.u.ip4.daddr=addr;
/*
                                rp = __ip_route_output_key(current->nsproxy->net_ns, &fl.u.ip4);
                                if (IS_ERR(rp))
                                        return PTR_ERR(rp);

                                if(ret==0)
                                {
                                        ip_rt_put(rp);
                                        addr=rp->rt_src;
                                        memset(argu.src,0,sizeof(argu.src));
                                        memcpy(argu.src,&addr,4);
                                        copy_to_user(user,&argu, sizeof(argu));
                                }
*/
                                ret=dev_fib_lookup(&fl, &res);
                                if(ret==0)
                                {
                                        dev = FIB_RES_DEV(res);
                                        strncpy(argu.dev,dev->name,sizeof(argu.dev)-1);
                                        dev_get_addr(dev,&addr);
                                        memset(argu.src,0,sizeof(argu.src));
                                        memcpy(argu.src,&addr,4);
                                        addr=FIB_RES_GW(res);
                                        memcpy(argu.gw,&addr,4);
                                        copy_to_user(user,&argu, sizeof(argu));
                                        return(0);
                                }
                        }
                        else
                        {
                                //struct fib6_table *tb=fib6_get_table(&init_net,RT6_TABLE_MAIN);
                                struct rt6_info *rt;
                                rt = rt6_lookup(&init_net, (const struct in6_addr *)argu.dst, NULL, 0, 1);
                                if(rt)
                                {
                                        //dev = rt->rt6i_dev;
                                        dev = rt->dst.dev;
                                        dst_release(&rt->dst);
                                        strncpy(argu.dev,dev->name,sizeof(argu.dev)-1);
                                        dev_get_addr6(dev,(struct in6_addr *)argu.src);
                                        memcpy(argu.gw,rt->rt6i_gateway.s6_addr,16);
                                        copy_to_user(user,&argu, sizeof(argu));
                                        return(0);
                                }
                        }
                        break;
                }
                case SP_QUERY:
                {
                        struct
                        {
                                unsigned char dst[16];
                                unsigned char src[16];
                                unsigned char gw[16];
                                char dev[32];
                        }argu;
                        struct flowi fl;
                        struct fib_result res;
                        struct net_device *dev = NULL;
                        struct rtable *rp=NULL;
                        int ret;
                        memset(&argu,0,sizeof(argu));
                        memset(&fl,0,sizeof(fl));
                        memset(&res,0,sizeof(res));
                        memset(&rp,0,sizeof(rp));
                        copy_from_user(&argu, user, sizeof(argu));
                        if(memcmp(argu.dst+4,"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",12)==0)
                        {
                                unsigned int addr;
                                memcpy(&addr,argu.dst,4);
                                fl.u.ip4.daddr=addr;
                                rp = __ip_route_output_key(current->nsproxy->net_ns, &fl.u.ip4);
                                if (IS_ERR(rp))
                                        return PTR_ERR(rp);
                                /*
                                if (!fl.u.ip4.saddr)
                                        fl.u.ip4.saddr = rp->rt_src;
                                if (!fl.u.ip4.daddr)
                                        fl.u.ip4.daddr = rp->rt_dst;
                                */
                                ret = xfrm_lookup(&init_net,(struct dst_entry **)&rp, &fl, NULL,0);
                                if(ret==-EREMOTE)
                                {
                                        dev=dev_get_by_index(&init_net, rp->rt_iif);
                                        if(dev)
                                                strncpy(argu.dev,dev->name,sizeof(argu.dev)-1);
                                        /*
                                        addr=rp->rt_src;
                                        */
                                        addr=fl.u.ip4.saddr;
                                        memset(argu.src,0,sizeof(argu.src));
                                        memcpy(argu.src,&addr,4);
                                        addr=rp->rt_gateway;
                                        memcpy(argu.gw,&addr,4);
                                        ip_rt_put(rp);
                                        copy_to_user(user,&argu, sizeof(argu));
                                        return(0);
                                }
                        }
                        else
                        {
                                struct rt6_info *rt;
                                rt = rt6_lookup(&init_net, (const struct in6_addr *)argu.dst, NULL, 0, 1);
                                if(rt)
                                {
                                        dev = rt->dst.dev;
                                        //dev = rt->rt6i_dev;
                                        dev_get_addr6(dev,(struct in6_addr *)argu.src);
                                        memcpy(&fl.u.ip6.daddr,argu.dst,16);
                                        memcpy(&fl.u.ip6.saddr,argu.src,16);
                                        ret = xfrm_lookup(&init_net,(struct dst_entry **)&rt, &fl, NULL,0);
                                        if(ret==-EREMOTE)
                                        {
                                                dev = rt->dst.dev;
                                                //dev = rt->rt6i_dev;
                                                dst_release(&rt->dst);
                                                strncpy(argu.dev,dev->name,sizeof(argu.dev)-1);
                                                dev_get_addr6(dev,(struct in6_addr *)argu.src);
                                                memcpy(argu.gw,rt->rt6i_gateway.s6_addr,16);
                                                copy_to_user(user,&argu, sizeof(argu));
                                                return(0);
                                        }
                                }
                        }
                        break;
                }
        }
  return -1;
}

static struct nf_sockopt_ops rt_sockops =
{
  .pf = PF_INET,
  .get_optmin = RT_QUERY,
  .get_optmax = RT_MAX,
  .get = rt_query,
};

static int data_to_kernel(struct sock *sk, int cmd, void *user,unsigned int len)
{
        switch(cmd)
        {
                case BIH_SET:
                {
                        char umsg[64];
                        memset(umsg, 0, sizeof(char)*64);
                        copy_from_user(umsg, user, sizeof(char)*64);
                        printk("umsg: %s", umsg);
                        break;
                }
        }
  return 0;
}

static int data_from_kernel(struct sock *sk, int cmd, void *user, int *len)
{
        switch(cmd)
        {
                case BIH_GET:
                {
                        struct
                        {
                                unsigned int in4addr;
                                struct in6_addr in6addr;
                        }argu;
                        char strbuff[128];
                        memset(&argu,0,sizeof(argu));
                        copy_from_user(&argu, user, sizeof(argu));
                        argu.in4addr=pool_addr_assign(&argu.in6addr);
                        inet_ntop6((unsigned char *)&argu.in6addr, strbuff, sizeof(strbuff)-1);
                        printk("%u.%u.%u.%u %s\n",NIPQUAD(argu.in4addr),strbuff);
                        copy_to_user(user,&argu, sizeof(argu));
                }
                break;
        }
  return 0;
}

static struct nf_sockopt_ops bih_sockops =
{
  .pf = PF_INET,
  .set_optmin = BIH_SET,
  .set_optmax = BIH_MAX,
  .set = data_to_kernel,
  .get_optmin = BIH_GET,
  .get_optmax = BIH_MAX,
  .get = data_from_kernel,
};

static unsigned int hook_func6(unsigned int hookunm,struct sk_buff *skb,const struct net_device *in,const struct net_device *out,int (*okfn)(struct sk_buff *))
{
        int ret;
        printk("bih: hook_func6 bih_xmit start\n");
        ret=bih_xmit(skb,(struct net_device *)in);
        if(ret==0)
                return(NF_STOLEN);
        else
                return(NF_ACCEPT);
}

static unsigned int hook_func4(unsigned int hookunm,struct sk_buff *skb,const struct net_device *in,const struct net_device *out,int (*okfn)(struct sk_buff *))
{
        int ret;
        printk("bih: hook_func4 bih_xmit start\n");
        ret=bih_xmit(skb,(struct net_device *)out);
        if(ret==0)
                return(NF_STOLEN);
        else
                return(NF_ACCEPT);
}

struct nf_hook_ops nfho_ipv4 = { 
        .hook = hook_func4,
        .owner = THIS_MODULE,
        .pf = PF_INET,
        .hooknum = 4,
        .priority        = NF_IP_PRI_FIRST,
}; 

struct nf_hook_ops nfho_ipv6 = { 
        .hook = hook_func6,
        .owner = THIS_MODULE,
        .pf = PF_INET6,
        .hooknum = 0,
        .priority        = NF_IP6_PRI_FIRST,
}; 

int hook_init(void)
{
        int ret;
        ret=nf_register_hook(&nfho_ipv4);
        if(ret!=0)
        {
                printk("nf_register_hook(ipv4) error\n");
                return(-1);
        }
        ret=nf_register_hook(&nfho_ipv6);
        if(ret!=0)
        {
                printk("nf_register_hook(ipv6) error\n");
                nf_unregister_hook(&nfho_ipv4);
                return(-1);
        }
        return 0;
}

int hook_exit(void)
{
        nf_unregister_hook(&nfho_ipv4);
        nf_unregister_hook(&nfho_ipv6);
        return 0;
}

static int proc_show_bis(struct seq_file *m, void *v)
{
        if(BIHMODE==1)
                seq_printf(m, "ENABLE\n");
        else
                seq_printf(m, "DISABLE\n");
        return(0);
}

static int proc_open_bis(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show_bis, PDE_DATA(inode));
}

static const struct file_operations proc_file_bis_fops = {
	.owner	    = THIS_MODULE,
	.open		= proc_open_bis,
	.read		= seq_read,
	.write		= seq_write,
	.llseek	    = seq_lseek,
	.release	= single_release,

};

static int proc_init_bis(char *name,mode_t mode)
{
        if(proc_dir == NULL)
        {
                printk("proc_dir error\n");
                return(-1);
        }

        proc_file_bis = proc_create(name, mode, proc_dir, &proc_file_bis_fops);
		if(!proc_file_bis)
		{
			return(-1);
		}
		return (0);
}

static int proc_show_mode(struct seq_file *m, void *v)
{
        switch(BIHMODE)
        {
                case 0:
                        seq_printf(m, "NULL\n");
                        break;
                case 1:
                        seq_printf(m, "BIS\n");
                        break;
                case 2:
                        seq_printf(m, "BIA\n");
                        break;
                default:
                        seq_printf(m, "ERROR\n");
                        break;
        }
        return (0);
}
static int proc_write_mode(struct file *file, const char __user *buffer,size_t count, loff_t *ofs)
{
        int len;
        char buff[32];

        if(count >= sizeof(buff))
                len = sizeof(buff)-1;
        else
                len = count;
        if(copy_from_user(buff, buffer, len))
        {
                printk("copy_from_user error\n");
                return -1;
        }
        buff[len] = 0;
        if(strncmp(buff,"NULL",4)==0)
                BIHMODE=0;
        else if(strncmp(buff,"BIS",3)==0)
                BIHMODE=1;
        else if(strncmp(buff,"BIA",3)==0)
        {
                if(BIHMODE!=2)
                {
                        BIHMODE=2;
                }
        }
        else
        {
                printk("buff(%s) error\n",buff);
                return(-1);
        }
        return len;
}

static int proc_open_mode(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show_mode, PDE_DATA(inode));
}

static const struct file_operations proc_file_mode_fops = {
	.owner	    = THIS_MODULE,
	.open		= proc_open_mode,
	.read		= seq_read,
	.write		= proc_write_mode,
	.llseek	    = seq_lseek,
	.release	= single_release,
};

static int proc_init_mode(char *name,mode_t mode)
{
        if(proc_dir == NULL)
        {
                printk("proc_dir error\n");
                return(-1);
        }

        proc_file_mode = proc_create(name, mode,proc_dir, &proc_file_mode_fops);
		if(!proc_file_mode)
		{
			return(-1);
		}
		return (0);
}

static int proc_show_pool(struct seq_file *m, void *v)
{
        seq_printf(m, "%s\n",pool_addr_show());
        return 0;
}
static int proc_write_pool(struct file *file, const char __user *buffer,size_t count, loff_t *ofs)
{
        int len,cmdlen=0,slen;
        char buff[512],*cmd=NULL,*argu=NULL;
        unsigned int addr;
        if(count >= sizeof(buff))
                len = sizeof(buff)-1;
        else
                len = count;
        if(copy_from_user(buff, buffer, len))
        {
                printk("copy_from_user error\n");
                return -1;
        }
        buff[len] = 0;
        slen=strlen(buff);
        if(buff[slen-1]=='\n')
        {
                slen--;
                buff[slen]=0;
        }
        if(buff[slen-1]=='\r')
        {
                slen--;
                buff[slen]=0;
        }
        msg_field2(buff,slen,0,&cmd,&cmdlen,' ');
        argu=buff+cmdlen+1;
        if(strncmp(cmd,"ADD",cmdlen)==0)
                pool_addr_add2(argu);
        else if(strncmp(cmd,"DEL",cmdlen)==0)
                pool_addr_del2(argu);
        else if(strncmp(cmd,"SHOW",cmdlen)==0)
                printk("%s\n",pool_addr_show());
        else if(strncmp(cmd,"ASSIGN",cmdlen)==0)
        {
                struct in6_addr in6addr;
                char *pstr,strbuff[64];
                int pstrlen;
                if(msg_field2(buff,slen,1,&pstr,&pstrlen,' ')==0)
                {
                        snprintf(buff,sizeof(buff)-1,"%.*s",pstrlen,pstr);
                        memset(&in6addr,0,sizeof(struct in6_addr));
                        inet_pton6(buff,in6addr.s6_addr);
                        addr=pool_addr_assign(&in6addr);
                        inet_ntop6((unsigned char *)&in6addr, strbuff, sizeof(strbuff)-1);
                        printk("%u.%u.%u.%u %s\n",NIPQUAD(addr),strbuff);
                }
                else
                {
                        addr=pool_addr_assign(NULL);
                        printk("%u.%u.%u.%u\n",NIPQUAD(addr));
                }
        }
        else
                pool_addr_add2(buff);
        return len;
}

static int proc_open_pool(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show_pool, PDE_DATA(inode));
}

static const struct file_operations proc_file_pool_fops = {
	.owner	    = THIS_MODULE,
	.open		= proc_open_pool,
    .read       = seq_read,
    .write      = proc_write_pool,
	.llseek	    = seq_lseek,
	.release	= single_release,
};

static int proc_init_pool(char *name,mode_t mode)
{
        if(proc_dir == NULL)
        {
                printk("proc_dir error\n");
                return(-1);
        }

        proc_file_pool = proc_create(name, mode,proc_dir, &proc_file_pool_fops);
		if(!proc_file_pool)
		{
			return(-1);
		}
		return (0);
}

static int proc_show_map(struct seq_file *m, void *v)
{
		seq_printf(m,"%s",map_showrun());
        return (0);
}

static int proc_write_map(struct file *file, const char __user *buffer,size_t count, loff_t *ofs)
{
        int len,slen,in4len,in6len,cmdlen;
        char buff[512],*str,*cmdstr,*in4str,*in6str;
        unsigned int in4addr;
        struct in6_addr in6addr;

        if(count >= sizeof(buff))
                len = sizeof(buff)-1;
        else
                len = count;
        if(copy_from_user(buff, buffer, len))
        {
                printk("copy_from_user error\n");
                return -1;
        }
        buff[len] = 0;
        slen=strlen(buff);
        if(buff[slen-1]=='\n')
        {
                slen--;
                buff[slen]=0;
        }
        if(buff[slen-1]=='\r')
        {
                slen--;
                buff[slen]=0;
        }

        if(msg_field2(buff,slen,0,&cmdstr,&cmdlen,' ')!=0)
        {
                printk("msg_field2 error\n");
                return -1;
        }
        if(strncmp(cmdstr,"ADD",cmdlen)==0)
        {
                if(msg_field2(buff,slen,1,&in4str,&in4len,' ')==0&&
                        msg_field2(buff,slen,2,&in6str,&in6len,' ')==0)
                {
                        str=buff2String(in4str,in4len);
                        inet_pton4(str,(unsigned char *)&in4addr);
                        str=buff2String(in6str,in6len);
                        inet_pton6(str,in6addr.s6_addr);
                        if(map_add(in4addr,in6addr)==NULL)
                        {
                                printk("map_add error\n");
                                return -1;
                        }
                }
                else
                {
                        printk("msg_field2 error\n");
                        return -1;
                }
        }
        else if(strncmp(cmdstr,"DEL",cmdlen)==0)
        {
                if(msg_field2(buff,slen,1,&in4str,&in4len,' ')==0)
                {
                        str=buff2String(in4str,in4len);
                        inet_pton4(str,(unsigned char *)&in4addr);
                        map_del(in4addr);
                }
                else
                {
                        printk("msg_field2 error\n");
                        return -1;
                }
        }
        else if(strncmp(cmdstr,"CLEAR",cmdlen)==0)
        {
                map_free(NULL);
        }
        else
        {
                printk("cmd(%.*s error\n",cmdlen,cmdstr);
                return -1;
        }
        return len;
}

static int proc_open_map(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show_map, PDE_DATA(inode));
}

static const struct file_operations proc_file_map_fops = {
	.owner	    = THIS_MODULE,
	.open		= proc_open_map,
	.read       = seq_read,
    .write      = proc_write_map,
	.llseek	    = seq_lseek,
	.release	= single_release, 
};

static int proc_init_map(char *name,mode_t mode)
{
        if(proc_dir == NULL)
        {
                printk("proc_dir error\n");
                return(-1);
        }

        proc_file_map = proc_create(name, mode,proc_dir, &proc_file_map_fops);
		if(!proc_file_map)
		{
			return(-1);
		}
		return (0);
}

static int proc_show_network(struct seq_file *m, void *v)
{
        switch(NETWORKTYPE)
        {
                case 1:
                        seq_printf(m, "V4\n");
                        break;
                case 2:
                        seq_printf(m, "V6\n");
                        break;
                case 3:
                        seq_printf(m, "DS\n");
                        break;
                default:
                        seq_printf(m, "ERR\n");
                        break;
        }
        return 0;
}
static int proc_write_network(struct file *file, const char __user *buffer,size_t count, loff_t *ofs)
{
        int len;
        char buff[32];

        if(count >= sizeof(buff))
                len = sizeof(buff)-1;
        else
                len = count;
        if(copy_from_user(buff, buffer, len))
        {
                printk("copy_from_user error\n");
                return -1;
        }
        buff[len] = 0;
        if(strncmp(buff,"V4",2)==0)
                NETWORKTYPE=1;
        else if(strncmp(buff,"V6",2)==0)
                NETWORKTYPE=2;
        else if(strncmp(buff,"DS",2)==0)
        {
                NETWORKTYPE=3;
        }
        else
        {
                printk("buff(%s) error\n",buff);
                return(-1);
        }
        return len;
}

static int proc_open_network(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show_network, PDE_DATA(inode));
}

static const struct file_operations proc_file_network_fops = {
	.owner	    = THIS_MODULE,
	.open		= proc_open_network,
	.read       = seq_read,
    .write      = proc_write_network,
	.llseek	    = seq_lseek,
	.release	= single_release, 
};

static int proc_init_network(char *name,mode_t mode)
{
        if(proc_dir == NULL)
        {
                printk("proc_dir error\n");
                return(-1);
        }

        proc_file_network = proc_create(name, mode,proc_dir, &proc_file_network_fops);
		if(!proc_file_network)
		{
			return(-1);
		}
		return (0);
}

static int proc_show_private(struct seq_file *m, void *v)
{
        seq_printf(m, "%u.%u.%u.%u\n",NIPQUAD(PRIVATEADDR));
        return 0;
}
static int proc_write_private(struct file *file, const char __user *buffer,size_t count, loff_t *ofs)
{
        int len;
        char buff[32];

        if(count >= sizeof(buff))
                len = sizeof(buff)-1;
        else
                len = count;
        if(copy_from_user(buff, buffer, len))
        {
                printk("copy_from_user error\n");
                return -1;
        }
        buff[len] = 0;
        PRIVATEADDR=sk_inet_addr(buff);
        return len;
}

static int proc_open_private(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show_private, PDE_DATA(inode));
}

static const struct file_operations proc_file_private_fops = {
	.owner	    = THIS_MODULE,
	.open		= proc_open_private,
	.read       = seq_read,
    .write      = proc_write_private,
	.llseek	    = seq_lseek,
	.release	= single_release, 
};

static int proc_init_private(char *name,mode_t mode)
{
        if(proc_dir == NULL)
        {
                printk("proc_dir error\n");
                return(-1);
        }
		
        proc_file_private = proc_create(name, mode,proc_dir, &proc_file_private_fops);
		if(!proc_file_private)
		{
			return(-1);
		}
		return (0);
}

extern int ipv6_get_addr(struct net_device *dev, struct in6_addr *addr);
int get_lladdr(char *devname)
{
        struct in6_addr addr;
        struct net_device *dev;
        char buff[64];
        int ret;

        dev=dev_get_by_name(&init_net,devname);
        if(dev==NULL)
        {
                printk("dev_get_by_name error\n");
                return(-1);
        }
        ret=ipv6_get_addr(dev,&addr);
        if(ret!=0)
        {
                printk("ipv6_get_addr(%s) error\n",dev->name);
                return(-1);
        }
        if(inet_ntop6(addr.s6_addr, buff, sizeof(buff)-1)==NULL)
        {
                printk("inet_ntop6 error\n");
                return(-1);
        }
//        printk("addr=%s\n",buff);
        return(0);
}

static int __init init_procfs(void)   //initiate file
{
        int ret = 0;
        bihdev=dev_get_by_name(&init_net,"lo");
        if(bihdev==NULL)
        {
                printk("dev_get_by_name error\n");
                return(-1);
        }
        proc_dir= proc_mkdir(MODULE_NAME, NULL);
        if(proc_dir == NULL)
        {
                printk("proc_mkdir error\n");
                return(-1);
        }
//        proc_dir->owner = THIS_MODULE;
        ret=proc_init_bis("bis",06444);
        if(ret!=0)
        {
                remove_proc_entry(MODULE_NAME, NULL);
                return(-1);
        }
        ret=proc_init_mode("mode",06444);
        if(ret!=0)
        {
                remove_proc_entry("bis", proc_dir);
                remove_proc_entry(MODULE_NAME, NULL);
                return(-1);
        }
        ret=proc_init_pool("pool",06444);
        if(ret!=0)
        {
                remove_proc_entry("bis", proc_dir);
                remove_proc_entry("mode", proc_dir);
                remove_proc_entry(MODULE_NAME, NULL);
                return(-1);
        }
        ret=proc_init_map("map",06444);
        if(ret!=0)
        {
                remove_proc_entry("pool", proc_dir);
                remove_proc_entry("bis", proc_dir);
                remove_proc_entry("mode", proc_dir);
                remove_proc_entry(MODULE_NAME, NULL);
                return(-1);
        }
        ret=proc_init_network("network",06444);
        if(ret!=0)
        {
                remove_proc_entry("map", proc_dir);
                remove_proc_entry("pool", proc_dir);
                remove_proc_entry("bis", proc_dir);
                remove_proc_entry("mode", proc_dir);
                remove_proc_entry(MODULE_NAME, NULL);
                return(-1);
        }
        ret=proc_init_private("private",06444);
        if(ret!=0)
        {
                remove_proc_entry("map", proc_dir);
                remove_proc_entry("pool", proc_dir);
                remove_proc_entry("bis", proc_dir);
                remove_proc_entry("mode", proc_dir);
                remove_proc_entry("network", proc_dir);
                remove_proc_entry(MODULE_NAME, NULL);
                return(-1);
        }
//        get_lladdr("eth0");
        hook_init();
        map_init();
        nf_register_sockopt(&bih_sockops);
        nf_register_sockopt(&rt_sockops);
        return(0);
}

static void __exit cleanup_procfs(void)
{
        remove_proc_entry("map", proc_dir);
        remove_proc_entry("pool", proc_dir);
        remove_proc_entry("bis", proc_dir);
        remove_proc_entry("mode", proc_dir);
        remove_proc_entry("network", proc_dir);
        remove_proc_entry("private", proc_dir);
        remove_proc_entry(MODULE_NAME, NULL);
        map_uninit();
        hook_exit();
        nf_unregister_sockopt(&bih_sockops);
        nf_unregister_sockopt(&rt_sockops);
}

module_init(init_procfs);      //programe entry
module_exit(cleanup_procfs);   //programe exit
