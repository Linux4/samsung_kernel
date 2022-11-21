#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/module.h>

#define GNSS_TAG "GNSS"
#define GNSS_ERR(fmt,args...)	\
	pr_err(GNSS_TAG "%s:" fmt "\n", __func__, ## args)
#define GNSS_DEBUG(fmt,args...)	\
	pr_debug(GNSS_TAG "%s:" fmt "\n", __func__, ## args)

#define PUBLIC extern 
#define LOCAL static 

#define GNSS_RING_R 		0
#define GNSS_RING_W 		1

#define GNSS_RX_RING_SIZE 			(1024*1024)

#define GNSS_SUCCESS 			(0)
#define GNSS_ERR_RING_FULL 		(-1)
#define GNSS_ERR_MALLOC_FAIL 	(-2)
#define GNSS_ERR_BAD_PARAM		(-3)
#define GNSS_ERR_SDIO_ERR		(-4)
#define GNSS_ERR_TIMEOUT		(-5)
#define GNSS_ERR_NO_FILE		(-6)

#define FALSE false
#define TRUE true

//#define GNSS_RING_REMAIN(rp,wp,size) \
//	((u_long)wp >= (u_long)rp?(size - (u_long)wp + (u_long)rp - 1): ((u_long)rp - (u_long)wp -1))
#define GNSS_RING_REMAIN(rp, wp, size) \
	((u_long)(wp) >= (u_long)(rp) ? ((size)-(u_long)(wp)+(u_long)(rp)) : ((u_long)(rp)-(u_long)(wp)))

typedef long unsigned int GNSS_SIZE_T;
//return 0 is success, other is error
typedef int (*memcpy_cb)(u_long, u_long, size_t);
typedef struct gnss_ring_t {
	GNSS_SIZE_T size;
	char * pbuff;
	char* rp;
	char* wp;
	char* end;
	struct mutex* plock;
	memcpy_cb memcpy_rd;
	memcpy_cb memcpy_wr;	
}GNSS_RING_T;
struct gnss_device{
	wait_queue_head_t rxwait;
};

GNSS_RING_T* gnss_rx_ring=NULL;
struct gnss_device *gnss_dev=NULL;

LOCAL GNSS_SIZE_T gnss_ring_remain(GNSS_RING_T* pRing)
{
	return ((GNSS_SIZE_T)GNSS_RING_REMAIN(pRing->rp, pRing->wp, pRing->size));
}

LOCAL GNSS_SIZE_T gnss_ring_content_len(GNSS_RING_T* pRing)
{
	return (pRing->size - gnss_ring_remain(pRing));
}

LOCAL bool gnss_ring_will_full(GNSS_RING_T* pRing, GNSS_SIZE_T len)
{
	return (len > gnss_ring_remain(pRing));
}

LOCAL char* gnss_ring_start(GNSS_RING_T* pRing)
{
	return(pRing->pbuff);
}

LOCAL char* gnss_ring_end(GNSS_RING_T* pRing)
{
	return(pRing->end);
}

LOCAL bool gnss_ring_over_loop(GNSS_RING_T* pRing, u_long len, int rw)
{
	if(GNSS_RING_R == rw){
		return ((u_long)pRing->rp + len > (u_long)gnss_ring_end(pRing));
	}else{
		return ((u_long)pRing->wp + len > (u_long)gnss_ring_end(pRing));
	}
}

PUBLIC void gnss_ring_destroy(GNSS_RING_T *pRing)
{
	GNSS_DEBUG("pRing = %p",pRing);
	GNSS_DEBUG("pRing->pbuff = %p",pRing->pbuff);
	if(pRing != NULL){
		if(pRing->pbuff != NULL){
			GNSS_DEBUG("to free pRing->pbuff.");
			kfree(pRing->pbuff);
			pRing->pbuff = NULL;
		}

		if(pRing->plock != NULL){
			GNSS_DEBUG("to free pRing->plock.");
			mutex_destroy(pRing->plock);
			kfree(pRing->plock);
			pRing->plock = NULL;
		}
		GNSS_DEBUG("to free pRing.");
		kfree(pRing);
		pRing = NULL;
	}
	return;
}

PUBLIC GNSS_RING_T* gnss_ring_init(GNSS_SIZE_T size, memcpy_cb rd, memcpy_cb wr)
{
	GNSS_RING_T* pRing = NULL;
	do{
		if (rd == NULL || wr == NULL){
			GNSS_ERR("Ring must assign callback.");
			return NULL;
		}
		pRing = kmalloc(sizeof(GNSS_RING_T),GFP_KERNEL);
		if(NULL == pRing){
			GNSS_ERR("Ring malloc Failed.");
			break;
		}
		pRing->pbuff = kmalloc(size,GFP_KERNEL);
		if(NULL == pRing->pbuff){
			GNSS_ERR("Ring buff malloc Failed.");
			break;
		}
		pRing->plock = kmalloc(sizeof(struct mutex), GFP_KERNEL);
		if(NULL == pRing->plock){
			GNSS_ERR("Ring lock malloc Failed.");
			break;
		}
		mutex_init(pRing->plock);
		memset(pRing->pbuff,0,size);
		pRing->size = size;
		pRing->rp = pRing->pbuff;
		pRing->wp = pRing->pbuff;
		pRing->end = (char*)(((u_long)pRing->pbuff) + (pRing->size - 1));
		pRing->memcpy_rd = rd;
		pRing->memcpy_wr = wr;
		return (pRing);
	}while(0);
	gnss_ring_destroy(pRing);
	return (NULL);//failed
}

/*******************************************************/
/****************Ring RW Fucntions*****************/
/*******************************************************/
PUBLIC int gnss_ring_read(GNSS_RING_T* pRing, char* buf, int len)
{
	int len1,len2 = 0;
	int cont_len = 0;
	int read_len = 0;
	char* pstart = NULL;
	char* pend = NULL;

	if((NULL == buf) ||(NULL == pRing) ||(0 == len)){
		GNSS_ERR("Ring Read Failed, Param Error!,buf=%p,pRing=%p,len=%d",buf,pRing,len);
		return (GNSS_ERR_BAD_PARAM);
	}	
	mutex_lock(pRing->plock);
	cont_len = gnss_ring_content_len(pRing);
	read_len = cont_len >= len?len:cont_len;
	pstart = gnss_ring_start(pRing);
	pend = gnss_ring_end(pRing);
	GNSS_DEBUG("read_len=%d",read_len);
	GNSS_DEBUG("buf=%p",buf);
	GNSS_DEBUG("pstart=%p",pstart);
	GNSS_DEBUG("pend=%p",pend);
	GNSS_DEBUG("pRing->rp=%p",pRing->rp);
	
	if((0 == read_len)||(0 == cont_len)){
		GNSS_DEBUG("read_len=0 OR Ring Empty.");
		mutex_unlock(pRing->plock);
		return (0);//ring empty
	}
	
	if(gnss_ring_over_loop(pRing,read_len,GNSS_RING_R)){
		GNSS_DEBUG("Ring loopover.");
		len1 = pend - pRing->rp + 1;
		len2 = read_len -len1;
		pRing->memcpy_rd(buf, pRing->rp, len1);
		pRing->memcpy_rd((buf+ len1), pstart, len2);
		pRing->rp = (char *)((u_long)pstart + len2);
	}else{
		pRing->memcpy_rd(buf, pRing->rp, read_len);
		pRing->rp += read_len;
	}
	GNSS_DEBUG("Ring did read len =%d.",read_len);
	mutex_unlock(pRing->plock);
	return (read_len);
}

PUBLIC int gnss_ring_write(GNSS_RING_T* pRing, char* buf, int len)
{
	int len1,len2 = 0;
	char* pstart = NULL;
	char* pend = NULL;
	bool check_rp = FALSE;
	
	if((NULL == pRing) || (NULL == buf)||(0 == len)){
		GNSS_ERR("Ring Write Failed, Param Error!,buf=%p,pRing=%p,len=%d",buf,pRing,len);
		return (GNSS_ERR_BAD_PARAM);
	}
	pstart = gnss_ring_start(pRing);
	pend = gnss_ring_end(pRing);
	GNSS_DEBUG("pstart = %p",pstart);
	GNSS_DEBUG("pend = %p",pend);
	GNSS_DEBUG("buf = %p",buf);
	GNSS_DEBUG("len = %d",len);
	GNSS_DEBUG("pRing->wp = %p",pRing->wp);
	
	if(gnss_ring_over_loop(pRing, len, GNSS_RING_W)){
		GNSS_DEBUG("Ring overloop.");
		len1 = pend - pRing->wp + 1;
		len2 = len - len1;
		pRing->memcpy_wr(pRing->wp, buf, len1);
		pRing->memcpy_wr(pstart, (buf + len1), len2);
		pRing->wp = (char *)((u_long)pstart + len2);
		check_rp = TRUE;
	}else{
		pRing->memcpy_wr(pRing->wp, buf, len);
		if (pRing->wp < pRing->rp)
			check_rp = TRUE;
		pRing->wp += len;	
	}
	if (check_rp && pRing->wp > pRing->rp)
		pRing->rp = pRing->wp;
	GNSS_DEBUG("Ring Wrote len = %d",len);
	return (len);
}

/*****************************************************************
 *
 *gnss 
 *****************************************************************/
int gnss_memcpy_rd(u_long dest, u_long src, size_t count)
{
	return copy_to_user((void __user*)dest, (char*)src, count);
}

int gnss_memcpy_wr(u_long dest, u_long src, size_t count)
{
	return copy_from_user((char*)dest, (void __user*)src, count);
}

LOCAL int gnss_device_init(void)
{
	gnss_dev = kzalloc(sizeof(*gnss_dev), GFP_KERNEL);
	if (gnss_dev == NULL){
		GNSS_ERR("alloc gnss device error");
		return -ENOMEM;
	}
	init_waitqueue_head(&gnss_dev->rxwait);
	return 0;
}

LOCAL int gnss_device_destroy(void)
{
	if (gnss_dev != NULL){
		kfree(gnss_dev);
		gnss_dev = NULL;
	}
}

/****************************************************************
 *
 *
 ***************************************************************/
static int gnss_dbg_open(struct inode *inode,struct file *filp)
{
	GNSS_ERR();

	return 0;
}

static int gnss_dbg_release(struct inode *inode,struct file *filp)
{
	GNSS_ERR();

	return 0;
}

static ssize_t gnss_dbg_read(struct file *filp,char __user *buf,size_t count,loff_t *pos)
{
	return 0;
}

LOCAL ssize_t gnss_dbg_write(struct file *filp, const char __user *buf,size_t count,loff_t *pos)
{
	ssize_t len;

	len = gnss_ring_write(gnss_rx_ring, (char*)buf, count);
	if (len > 0)
		wake_up_interruptible(&gnss_dev->rxwait);
	return len;
}


static struct file_operations gnss_dbg_fops = {
	.owner = THIS_MODULE,
	.read  = gnss_dbg_read,
	.write = gnss_dbg_write,
	.open  = gnss_dbg_open,
};
static struct miscdevice gnss_dbg_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gnss_dbg",
	.fops = &gnss_dbg_fops,
};

static ssize_t gnss_slog_read(struct file *filp,char __user *buf,size_t count,loff_t *pos)
{
	ssize_t len;
	len = gnss_ring_read(gnss_rx_ring, (char*)buf, count);
	return len;
}

LOCAL ssize_t gnss_slog_write(struct file *filp, const char __user *buf,size_t count,loff_t *pos)
{
	ssize_t len = 0;
	return len;
}

static int gnss_slog_open(struct inode *inode,struct file *filp)
{
	GNSS_ERR();
	return 0;
}

static int gnss_slog_release(struct inode *inode,struct file *filp)
{
	GNSS_ERR();
	return 0;
}

static unsigned int gnss_slog_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(filp, &gnss_dev->rxwait, wait);
	if(gnss_ring_content_len(gnss_rx_ring) > 0)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static struct file_operations gnss_slog_fops = {
	.owner = THIS_MODULE,
	.read  = gnss_slog_read,
	.write = gnss_slog_write,
	.open  = gnss_slog_open,
	.release = gnss_slog_release,
	.poll	= gnss_slog_poll,
};
static struct miscdevice gnss_slog_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "slog_gnss",
	.fops = &gnss_slog_fops,
};

LOCAL int __init gnss_module_init(void)
{
	int ret;

	gnss_rx_ring = gnss_ring_init(GNSS_RX_RING_SIZE, gnss_memcpy_rd, gnss_memcpy_wr);
	if(!gnss_rx_ring){
		GNSS_ERR("Ring malloc error.");
		return GNSS_ERR_MALLOC_FAIL;
	}

	ret = gnss_device_init();
	if (0 != ret)
		return ret; 
	//wake_lock_init(&mdbg_wake_lock, WAKE_LOCK_SUSPEND, "mdbg_wake_lock");
	//MDBG_LOG("mdbg_sdio_init!");
	
	do{
		if ((ret = misc_register(&gnss_dbg_device)) != 0)
			break;
		if ((ret = misc_register(&gnss_slog_device)) != 0){
			misc_deregister(&gnss_dbg_device);
			break;
		}
	}while(0);
	if (ret != 0)
		GNSS_ERR("misc register error");
	return ret;
}

LOCAL void __exit gnss_module_exit(void)
{
	gnss_ring_destroy(gnss_rx_ring);
	gnss_rx_ring = NULL;
	gnss_device_destroy();
	misc_deregister(&gnss_dbg_device);
	misc_deregister(&gnss_slog_device);
}

module_init(gnss_module_init);
module_exit(gnss_module_exit);

MODULE_LICENSE("GPL");

