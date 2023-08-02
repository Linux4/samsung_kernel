#include <linux/proc_fs.h>
#include <linux/sipc.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/ieee80211.h>
#include <linux/printk.h>
#include <linux/inetdevice.h>
#include <linux/spinlock.h>
#include <net/cfg80211.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/ieee80211_radiotap.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#include <net/iw_handler.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/ipv6.h>
#include <linux/ip.h>
#include <linux/inetdevice.h>
#include <asm/byteorder.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/completion.h>
#include <asm/atomic.h>
#include <linux/ieee80211.h>
#include <linux/delay.h>

#define WIFI_MEM_BLOCK_NUM   (2)

typedef struct {
	int status;
	unsigned char *mem;
} m_mem_t;

static m_mem_t wifi_mem[WIFI_MEM_BLOCK_NUM] = { 0 };

unsigned char *wifi_256k_alloc(void)
{
	unsigned char *p = NULL;
	int i;
	for (i = 0; i < WIFI_MEM_BLOCK_NUM; i++) {
		if ((0 == wifi_mem[i].status) && (NULL != wifi_mem[i].mem)) {
			p = wifi_mem[i].mem;
			wifi_mem[i].status = 1;
			printk("\001" "0" "[wifi_256k_alloc][%d][0x%x]\n", i,
			       wifi_mem[i].mem);
			break;
		}
	}
	return p;
}

EXPORT_SYMBOL_GPL(wifi_256k_alloc);

int wifi_256k_free(unsigned char *mem)
{
	int i;
	for (i = 0; i < WIFI_MEM_BLOCK_NUM; i++) {
		if (mem == wifi_mem[i].mem) {
			wifi_mem[i].status = 0;
			printk("\001" "0" "[wifi_256k_free][%d][0x%x]\n", i,
			       wifi_mem[i].mem);
			return 0;
		}
	}
	return -1;
}

EXPORT_SYMBOL_GPL(wifi_256k_free);

static int sprdwl_mem_alloc(void)
{
	int i;
	printk("[%s]\n", __func__);
	for (i = 0; i < WIFI_MEM_BLOCK_NUM; i++) {
		wifi_mem[i].mem = kmalloc(256 * 1024, GFP_ATOMIC);
		printk("\001" "0" "[wifi_mem][%d][0x%x]\n", i, wifi_mem[i].mem);
	}
	return 0;
}

arch_initcall(sprdwl_mem_alloc);

MODULE_AUTHOR("jinglong.chen");
MODULE_DESCRIPTION("256k mem alloc");
MODULE_LICENSE("GPL");
