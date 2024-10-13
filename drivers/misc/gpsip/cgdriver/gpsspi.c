#include <linux/init.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <asm/uaccess.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/delay.h>

#include <mach/dma.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/ldo.h>
#include <mach/adi_hal_internal.h>
#include <mach/regs_ana.h>
//add by zhangbing for intr.
//#define __USE_EXTERNAL_INTR__
#ifdef __USE_EXTERNAL_INTR__
//add for irq mode
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/sched.h>
#endif

#include <mach/regs_gpio.h>

#include <max2769_reg.h>

#include <linux/proc_fs.h>

#include <linux/kthread.h>
#include <linux/sched.h>

/*
 * This supports acccess to SPI devices using normal userspace I/O calls.
 * Note that while traditional UNIX/POSIX I/O semantics are half duplex,
 * and often mask message boundaries, full SPI support requires full duplex
 * transfers.  There are several kinds of of internal message boundaries to
 * handle chipselect management and other protocol options.
 *

 */
//#define SPIDEV_MAJOR          153 /* assigned */
int SPIDEV_MAJOR = 153;			/* assigned */

#define N_SPI_MINORS			32	/* ... up to 256 */

static unsigned long minors[N_SPI_MINORS / BITS_PER_LONG];
extern struct spi_device *sprd_spi_cmmb_device_register (int
														 master_bus_num,
														 struct
														 spi_board_info
														 *chip);
extern void sprd_spi_tmod (struct spi_device *spi, u32 transfer_mod);


/* Bit masks for spi_device.mode management.  Note that incorrect
 * settings for CS_HIGH and 3WIRE can cause *lots* of trouble for other
 * devices on a shared bus:  CS_HIGH, because this device will be
 * active when it shouldn't be;  3WIRE, because when active it won't
 * behave as it should.
 *
 * REVISIT should changing those two modes be privileged?
 */
#define SPI_MODE_MASK		(SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
				| SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP)


#define SPI_READ_CMD(len,addr)    ((1 << 30) | (addr & 0x1FFFF)|(((len-1) & 0x1FF) << 20))
#define SPI_WRITE_CMD(len,addr)   ((1 << 30) |(1 << 31) | (addr & 0x1FFFF)|(((len-1) & 0x1FF) << 20))
#define SPI_BLOCK_READ_CMD(len) (                   (len-1) & 0xF )
#define SPI_BLOCK_WRITE_CMD(len) (                (1 << 9) |  (len-1) & 0xF )

#define SPI_BLOCK_DATA_ENC(len,addr)   ((  ((len-1)&0x7FFF) << 17 ) | ( addr & 0x1FFFF))
//#define SPI_BLOCK_DATA_ENC(len,addr)   ((  ((len-1)&0x7FFF) << 17 ) | ( addr & 0x1FFFF))

struct gpsspidev_data
{
	dev_t devt;
	spinlock_t spi_lock;
	struct spi_device *spi;
	struct list_head device_entry;

	//add by zhangbing.
#ifdef __USE_EXTERNAL_INTR__
	/*add for irq mode */
	unsigned long irq_flag;		/* irq flag */
	unsigned char irq_enable;
	wait_queue_head_t iowait;	/* use for user space polling */
#endif

	/* buffer is NULL unless this device is open (users > 0) */
	struct mutex buf_lock;
	unsigned users;
	u8 *buffer;
};




/*************************************************************************************************/
#define GPS_DEBUG_TRACE(x...)              printk(x)


struct spi_device *gps_spi_dev = NULL;


static struct proc_dir_entry *sprd_gps_dir;
static struct proc_dir_entry *sprd_gps_data;

static int g_irq, gp_start_irq, irq_number, addr_flag_side;
static u32 count_block = 0;
static struct workqueue_struct *spi_recv_wq;
struct completion spi_completed;

static struct timer_list gps_timer = { 0 };

static void gps_spi_recv_handle (void *data);

#define GPS_SPI_HANLE_Q    gps_work_queue

DECLARE_WORK (GPS_SPI_HANLE_Q, gps_spi_recv_handle);



//#define TASK_LET_EN
#ifdef TASK_LET_EN
struct tasklet_struct spi_task_let;
#endif




EXPORT_SYMBOL_GPL (gps_spi_dev);
static LIST_HEAD (device_list);
static DEFINE_MUTEX (device_list_lock);

static void spi_transfer (unsigned char *tx1_buf, int tx1_len,
						  unsigned char *tx2_buf, int tx2_len,
						  unsigned char *rx_buf, int rx_len,
						  bool cs_change);

static int gps_spi_test (u8 * buf, u32 cnt);

//static unsigned bufsiz = (64*1024);
static unsigned bufsiz = (4);

//static    u32 read_buf_local[16*4096]= {0};


u32 cg_block_num, cg_block_size;

//#define FIX_MEMORY
#ifdef FIX_MEMORY
extern unsigned char buf_data[6 * 1024 * 1024];
static u8 *read_buf = (u8 *) buf_data;

#else
extern void *buf_data;
static u8 *read_buf;
#endif


int gps_spi_read_bytes (u32 len, u32 addr, u32 * data);
int gps_spi_write_bytes (u32 len, u32 addr, u32 data);


module_param (bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC (bufsiz, "data bytes in biggest supported SPI message");


static int spi_cs_gpio;


/*-------------------------------------------------------------------------*/

/*
 * We can't use the standard synchronous wrappers for file I/O; we
 * need to protect against async removal of the underlying spi_device.
 */
static void
mxdspidev_complete (void *arg)
{
	complete (arg);
}

static ssize_t
mxdspidev_sync (struct gpsspidev_data *mxdspidev,
				struct spi_message *message)
{
	DECLARE_COMPLETION_ONSTACK (done);
	int status;

	message->complete = mxdspidev_complete;
	message->context = &done;

	spin_lock_irq (&mxdspidev->spi_lock);
	if (mxdspidev->spi == NULL)
		status = -ESHUTDOWN;
	else
		status = spi_async (mxdspidev->spi, message);
	spin_unlock_irq (&mxdspidev->spi_lock);

	if (status == 0)
	{
		wait_for_completion (&done);
		status = message->status;
		if (status == 0)
			status = message->actual_length;
	}
	//printk("\mxdspidev_sync out!status = %d\n\n",status);
	return status;
}

static inline ssize_t
mxdspidev_sync_write (struct gpsspidev_data *mxdspidev, size_t len)
{
}

static inline ssize_t
mxdspidev_sync_read (struct gpsspidev_data *mxdspidev, size_t len)
{
}

/*-------------------------------------------------------------------------*/
extern void* Data_Get_DrvContext();
#include <linux/pagemap.h>
//int data_buf[2*1024] ;
int* data_buf = NULL;
static int mxdspidev_mmap(struct file *file, struct vm_area_struct * vma)
{
    	int i;
	unsigned long physical ;

	unsigned long len	  = (vma->vm_end - vma->vm_start);
	unsigned long off	  = vma->vm_pgoff << PAGE_SHIFT;

        printk("len =0x%x ,off=0x%x \n",len,off);
#if 0
        physical = __pa((void *)Data_Get_DrvContext()) + off;
#else
        if(data_buf == NULL)
        {
            data_buf = kmalloc(1024*4,GFP_KERNEL);

            if(NULL==data_buf)
             return;
        }


        physical = __pa((void*)(data_buf))+ off;

        for(i = 0; i < 1024;i++)
        {
            data_buf[i] = ( i | 0xFF550000);
        }
#endif


/*
       vma->vm_flags |= VM_IO;
        vma->vm_flags |= VM_RESERVED;  //set the attribute


	rv = remap_pfn_range(vma, vma->vm_start,
						 physical >> PAGE_SHIFT, len,
						 (vma->vm_page_prot));

	rv = remap_pfn_range(vma, vma->vm_start,
						 physical >> PAGE_SHIFT, len,
						 (vma->vm_page_prot));
*/


        printk ("mxdspidev_mmap physical = 0x%x  vma->vm_start=0x%x  end \n",physical,vma->vm_start);

        if (remap_pfn_range(vma, vma->vm_start, physical >>  PAGE_SHIFT,
            len, (vma->vm_page_prot) ) )
        {
            return -EAGAIN;
        }

    return 0;


}


/* Read-only message with current device setup */
static ssize_t
mxdspidev_read (struct file *filp, char __user * buf, size_t count,
				loff_t * f_pos)
{
	ssize_t status = 0;
	u8 test_buf[128] = " mxdspidev_read copy_to_user test_buf";
	u8 *temp_buf;
	u32 data_length;

	// copy_to_user(to, from, n)
	temp_buf = buf_data;
	data_length = cg_block_num * cg_block_size;
	printk ("spi_dev read count=%x\t\n", count);

	//copy_to_user(buf,temp_buf,data_length*4);

	printk ("spi_dev read  test_buf count=%x\t\n", strlen (test_buf));

	copy_to_user (buf, test_buf, strlen (test_buf));
}

/* Write-only message with current device setup */
typedef struct msg_gps
{
	int addr;
	int value;
} MSG_GPS;

static ssize_t
mxdspidev_write (struct file *filp, const char __user * buf,
				 size_t count, loff_t * f_pos)
{

	ssize_t status = 0;
	MSG_GPS *p_buf;

	p_buf = (MSG_GPS *) kmalloc (sizeof (MSG_GPS), GFP_KERNEL);

	if (p_buf == NULL)
	{
		return -ENOMEM;
	}
/*
if(data_buf == NULL)
{
    data_buf =(void*)__get_free_pages(GFP_ATOMIC,get_order(2048));

	if (data_buf == NULL)
	{
	        printk("__get_free_pages  errror");
		return -ENOMEM;
	}

       printk("get free page buf =0x%x \n", data_buf);
}
*/

	copy_from_user (p_buf, buf, count);

	printk ("kernel buf = 0x%p,count = 0x%x\n", p_buf, count);

	printk ("spi_dev write count=%x ,addr=%x,value= %x\t\n", count,
			p_buf->addr, p_buf->value);

	gps_spi_test (0, 0);

	if (p_buf->addr != 0)
	{
		//     gps_spi_write_bytes(1,p_buf->addr,p_buf->value);
	}

	if (p_buf != NULL)
		kfree (p_buf);
	return status;
}

static int
mxdspidev_message (struct gpsspidev_data *mxdspidev,
				   struct spi_ioc_transfer *u_xfers, unsigned n_xfers)
{
}

static long
mxdspidev_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
}


#ifdef __USE_EXTERNAL_INTR__
static int store_irq;
static irqreturn_t
mxdspidev_irq (int irq, void *data)
{
	struct gpsspidev_data *cmmb_dev = data;
	int frame = 0;

	//printk("enter cmmb intr!\n");
	spin_lock (&cmmb_dev->spi_lock);
	/*
	 * Basic frame housekeeping.
	 */
	if (test_bit (frame, &cmmb_dev->irq_flag) && printk_ratelimit ())
	{
		/* LOG_INFO("Frame overrun on %d, frames lost\n", frame); */
		;
	}
	set_bit (frame, &cmmb_dev->irq_flag);

	spin_unlock (&cmmb_dev->spi_lock);

	wake_up_interruptible (&cmmb_dev->iowait);

	return IRQ_HANDLED;
}

static unsigned int
mxdspidev_poll (struct file *filp, struct poll_table_struct *pt)
{
	int frame = 0;
	int data_ready = 0;
	struct gpsspidev_data *cmmb_dev;

	cmmb_dev = filp->private_data;
	//printk("enter gps poll!\n");
	poll_wait (filp, &cmmb_dev->iowait, pt);

	spin_lock (&cmmb_dev->spi_lock);	/* TODO: not need? */
	data_ready = test_and_clear_bit (frame, &cmmb_dev->irq_flag);
	spin_unlock (&cmmb_dev->spi_lock);

	if (data_ready)
		return POLLIN | POLLRDNORM;
	else
		return 0;
}
#endif



/*************************************************************************************************************/
extern unsigned int get_block_num (void);
extern int get_blocksize ();
static int
sprd_gps_data_read (char *page, char **start, off_t offset, int count,
					int *eof, void *data)
{
	u32 *temp_data;
	u32 len = 0;
	u32 i, n, j;
	u32 max = 4096;

//  max = (get_blocksize()/4)*get_block_num();
	max = (cg_block_size / 4) * cg_block_num;
#if 0
	n = max - 0xc00 + 100;
	temp_data = read_buf;


	/* This dump the current data buf */
	len += snprintf (page + len, count - len,
					 "\n####################start data##################\n ");

	len += snprintf (page + len, count - len,
					 "\n page =0x%p start =0x%x ,offset =0x%x, count=0x%x  \n ",
					 page, *start, offset, count);

	for (i = max; i > n; i -= 4)
		len += snprintf (page + len, count - len,
						 "%d= 0x%x,%d= 0x%x, %d=0x%x,%d= 0x%x \n ", i - 1,
						 temp_data[i - 1], i - 2, temp_data[i - 2], i - 3,
						 temp_data[i - 3], i - 4, temp_data[i - 4]);
#endif
	//n= max-200;

	{
		temp_data = (u32 *) buf_data;	//+max*(7+j);

		printk ("address start=0x%p ,ox%x,0x%x\n", temp_data, cg_block_num,
				cg_block_size);

		// for(i = max ; i > 0;i-=4)
		for (i = 4; i < max; i += 4)
		{

/*
                  printk( "%x= 0x%x,%x= 0x%x, %x=0x%x,%x= 0x%x \n ",i-1,temp_data[i -1],
                                                                                                                i-2,temp_data[i -2],
                                                                                                                i-3,temp_data[i -3],
                                                                                                                i-4,temp_data[i -4]);
*/
			printk ("%x= 0x%x,%x= 0x%x, %x=0x%x,%x= 0x%x \n ", i - 4,
					temp_data[i - 4], i - 3, temp_data[i - 3], i - 2,
					temp_data[i - 2], i - 1, temp_data[i - 1]);

		}
		msleep (200);
	}

	return len;
}

void gps_debug_print(u32* pAddress, u32 dataLen)
{
	u32 *temp_data;
	u32 len = 0;
	u32 i, n, j;
	u32 max = 4096;

	temp_data =  pAddress;

	GPS_DEBUG_TRACE ("address start=0x%p ,datalen =ox%x,0x%x\n", temp_data, dataLen);

	for (i = 4; i < dataLen; i += 4)
	{
			GPS_DEBUG_TRACE("%x= 0x%x,%x= 0x%x, %x=0x%x,%x= 0x%x \n ", i - 4,
					temp_data[i - 4], i - 3, temp_data[i - 3], i - 2,
					temp_data[i - 2], i - 1, temp_data[i - 1]);
	}

}



void
clear_gps_start_int (void)
{
	int addr;
	int value;

	value = 0x0;
	addr = 0x84;
	gps_spi_read_bytes (1, addr, &value);
	value |= ~BIT_1;
	addr = 0x8c;
	gps_spi_write_bytes (1, addr, value);

}


/* spi work queue callback*/
extern void DataReady_TH_start ();
extern int get_blocksize ();

int timeout_period = 200;
u32 enable_timer = 0;
void
set_timeout_period (int time)
{
	gps_timer.expires = jiffies + timeout_period;;

}

//#define TRIGGER_TIME
void
trigger_gps_timeout (int timeout)
{
#ifdef TRIGGER_TIME
	timeout_period = timeout;

	if (enable_timer && (count_block > 0) && (gps_timer.function != NULL))
	{
		set_timeout_period (timeout);
		add_timer (&gps_timer);
	}
#endif
}

void
gps_timeout_handle (unsigned long para)
{
#ifdef TRIGGER_TIME
	//#define TIMEOUT_TEST
#ifndef TIMEOUT_TEST
	GPS_DEBUG_TRACE ("gps_timeout_handle\n");
	DataReady_TH_start ();
	count_block--;
	if (!count_block)
		enable_timer = 0;
#else
	GPS_DEBUG_TRACE ("\ngps_timeout_handle\n");
#endif

#endif

}

void
gps_timeout_init (void)
{
	init_timer (&gps_timer);
	gps_timer.expires = jiffies + timeout_period;
	gps_timer.data = 0;
	gps_timer.function = gps_timeout_handle;	/* timer handler */
}




void
gps_spi_recv_handle (void *data)
{
	struct work_struct *work;
	int status, i;
	struct timespec cur_time;

//        u32 read_buf[200];
	int wait_time = 0;
	u32 read_len = 1024 * 4;

	//      GPS_DEBUG_TRACE(" gps_spi_recv_handle jeff tjime = %x,%x\n ",jiffies,cur_time.tv_nsec);

#if 1

	read_buf =
		(u8 *) ((u8 *) buf_data + (get_blocksize () * count_block) +
				(irq_number - 1) * read_len * 4);
	gps_spi_read_bytes (read_len, 0x5000 + addr_flag_side * read_len,
						(u32 *) read_buf);

	if (cg_block_num == 0)
	{
		cg_block_num = get_block_num ();
		cg_block_size = get_blocksize ();
	}

	if (addr_flag_side)
		addr_flag_side = 0;
	else
		addr_flag_side = 1;

	if (get_blocksize () <= irq_number * read_len * 4)
	{
		count_block++;
#ifndef TRIGGER_TIME
		DataReady_TH_start ();
#endif
		irq_number = 0;
	}

	if (count_block == get_block_num ())
	{
#ifdef TRIGGER_TIME
		enable_timer = 1;
		// DataReady_TH_start();
		trigger_gps_timeout (1);
#else
		count_block = 0;
		addr_flag_side = 0;
#endif
	}

//        GPS_DEBUG_TRACE("0=0x%x,1=0x%x,199=0x%x,200=0x%x \n",read_buf[0],read_buf[1],
//                                                                                                            read_buf[read_len-2],read_buf[read_len-1]                                                                                                           );

#endif
#ifdef TASK_LET_EN
	tasklet_disable (&spi_task_let);
	tasklet_hi_schedule (&spi_task_let);
#endif

	GPS_DEBUG_TRACE ("buf=0x%p 0x%p count_block=0x%x, 0x%x\n", buf_data,
					 read_buf, count_block, get_block_num ());
}



/*gpio inturrupt handle*/
int
data_req_handle (int para, void *param)
{
	int status, wait_time;

#if 1
	gpio_set_value (24, 1);
	for (wait_time = 0; wait_time < 0x0FF; wait_time++);
	gpio_set_value (24, 0);
#endif


	/*clear the irq flag */
	//disable_irq(g_irq);
#if 0
	gpio_set_value (24, 1);
	for (wait_time = 0; wait_time < 0xFFF; wait_time++);
	gpio_set_value (24, 0);
#endif

	irq_number++;

/*

#ifdef TASK_LET_EN
        tasklet_enable(&spi_task_let);

//#define WORK_QUEUE
#elif  defined WORK_QUEUE


        status = queue_work(spi_recv_wq,&GPS_SPI_HANLE_Q);
#else
        complete(&spi_completed);

#endif
*/
	gps_spi_recv_handle (0);
	return 0;
}


void
data_start_handle (int para, void *param)
{
	int status;

	/*clear the irq flag */
	//disable_irq(gp_start_irq);

	//  GPS_DEBUG_TRACE(" data_start_handle  ");
	status = queue_work (spi_recv_wq, &GPS_SPI_HANLE_Q);
	//  return 0;;

}


extern struct spi_device *sprd_spi_wifi_device_register (int
														 master_bus_num,
														 struct
														 spi_board_info
														 *chip);


void
gps_spi_thread (void *data)
{

	while (1)
	{
		wait_for_completion (&spi_completed);
		// printk("gps_spi_thread \n");
		gps_spi_recv_handle (0);
		init_completion (&spi_completed);
	}
}

extern int sched_setscheduler_nocheck (struct task_struct *p, int policy,
									   struct sched_param *param);
int
gps_int_init (void)
{
	int gpio_irq, irq, ret, gps_int_start;

	/* Initialize the interrupt pin for chip */
	gpio_irq = 23;				/*dma_req */
	gps_int_start = 84;
	gpio_free (gpio_irq);
	gpio_free (gps_int_start);

/***************************************************/

	ret = gpio_request (gpio_irq, "gps-req- int");
	if (ret)
	{
		printk ("gps-req- int err: %d\n", gpio_irq);
		goto err_gpio;
	}

	gpio_direction_input (gpio_irq);


	g_irq = sprd_alloc_gpio_irq (gpio_irq);

	set_irq_type (g_irq, IRQ_TYPE_LEVEL_HIGH);

	ret = 0;
	ret =
		request_irq (g_irq, data_req_handle, IRQF_DISABLED, "dma_req",
					 gps_spi_dev);

	printk ("spi irq req :%d ,ret = %d  handle=%p\n", g_irq, ret,
			data_req_handle);

/***************************************************/

#if 0
	ret = gpio_request (gps_int_start, "gps-start- int");
	if (ret)
	{
		printk ("gps-start- int err: %d\n", gps_int_start);
		goto err_gpio;
	}

	gp_start_irq = sprd_alloc_gpio_irq (gps_int_start);
	printk ("startirq req :%d\n", gp_start_irq);

	gpio_direction_input (gps_int_start);
	set_irq_type (gp_start_irq, IRQ_TYPE_LEVEL_HIGH);
	request_irq (gp_start_irq, data_start_handle, IRQF_SHARED, "start_req",
				 NULL);
	disable_irq (gp_start_irq);
#endif


	disable_irq (g_irq);

#ifdef  TASK_LET_EN
	tasklet_init (&spi_task_let, gps_spi_recv_handle, 0);
	tasklet_disable (&spi_task_let);

	tasklet_hi_schedule (&spi_task_let);

#elif  defined WORK_QUEUE

	//spi_recv_wq  = create_singlethread_workqueue("spi_recv")/*it is used for gpio for spi reiceve data*/;
	spi_recv_wq = create_rt_workqueue ("spi_recv");
	if (!spi_recv_wq)
	{
		printk ("ENOMEM \n;");
	}
#elif defined WORK_THREAD
	struct sched_param param = {.sched_priority = 0 };
	//struct workqueue_struct *wq = cwq->wq;
//  const char *fmt = is_wq_single_threaded(wq) ? "%s" : "%s/%d";
	struct task_struct *p;

/*
	struct workqueue_struct *wq;
	struct cpu_workqueue_struct *cwq;
	int err = 0, cpu;

	wq = kzalloc(sizeof(*wq), GFP_KERNEL);
	if (!wq)
		return NULL;

	wq->cpu_wq = alloc_percpu(struct cpu_workqueue_struct);
	if (!wq->cpu_wq) {
		kfree(wq);
		return NULL;
	}


	p = kthread_create(gps_spi_thread, cwq, fmt, wq->name, cpu);

	if (IS_ERR(p))
		return PTR_ERR(p);

	if (cwq->wq->rt)
		sched_setscheduler_nocheck(p, SCHED_FIFO, &param);
	cwq->thread = p;

*/

//////work well
	p = kthread_create (gps_spi_thread, NULL, "gps_spi_thread");

	if (IS_ERR (p))
		return PTR_ERR (p);

	sched_setscheduler (p, SCHED_FIFO, &param);

	init_completion (&spi_completed);
	wake_up_process (p);


#endif
//        gps_timeout_init();

	return 0;
  err_gpio:

	return 0;


}

static int __init
spi_module_init (void)
{
	printk ("spi_module_init begin !\n");

	if (gps_spi_dev == NULL)
		gps_spi_dev = sprd_spi_wifi_device_register (1, NULL);

	sprd_spi_tmod (gps_spi_dev, 2);	/*what is the meaning of mod 2 ,need confirm,paul */
	printk ("spi_dev master=%p \n", gps_spi_dev->master);


	gps_int_init ();


	// Create /proc/sprd-gps
	sprd_gps_dir = proc_mkdir ("sprd-gps", NULL);
	if (!sprd_gps_dir)
	{
		printk (KERN_ERR "Can't create sprd-gps proc dir\n");
		return -1;
	}

	// Create /proc/sprd-gps/buf_data
	sprd_gps_data = create_proc_read_entry ("buf_data",
											0,
											sprd_gps_dir,
											sprd_gps_data_read, NULL);
	if (!sprd_gps_data)
	{
		printk (KERN_ERR "Can't create sprd-gps  data proc\n");
		return -1;
	}
	return 0;

}


int
work_queue_test (void)
{

	struct timespec cur_time;
	int status;

	cur_time = current_kernel_time ();

	GPS_DEBUG_TRACE ("work_queue_test jeff tjime = %x ,%x\n ", jiffies,
					 cur_time.tv_nsec);

#ifdef TASK_LET_EN
	tasklet_enable (&spi_task_let);
	//tasklet_schedule(&spi_task_let);
//        tasklet_hi_schedule(&spi_task_let);
#else
	//status = queue_work(spi_recv_wq,&GPS_SPI_HANLE_Q);
#endif

	// count_block =1;
	// trigger_gps_timeout(10);

	complete (&spi_completed);

	GPS_DEBUG_TRACE ("status\n");


}


static int
mxdspidev_open (struct inode *inode, struct file *filp)
{
	struct gpsspidev_data *mxdspidev;
	int status = -ENXIO;

	lock_kernel ();
	mutex_lock (&device_list_lock);
	printk ("liwk mxdspidev_open\n");

	list_for_each_entry (mxdspidev, &device_list, device_entry)
	{
		if (mxdspidev->devt == inode->i_rdev)
		{
			status = 0;
			break;
		}
	}
	if (status == 0)
	{
		if (!mxdspidev->buffer)
		{
			mxdspidev->buffer = kmalloc (bufsiz, (GFP_KERNEL | GFP_DMA));
			if (!mxdspidev->buffer)
			{
				dev_dbg (&mxdspidev->spi->dev, "open/ENOMEM\n");
				status = -ENOMEM;
			}
		}
		if (status == 0)
		{
			mxdspidev->users++;
			filp->private_data = mxdspidev;
			nonseekable_open (inode, filp);
		}
	}
	else
		pr_debug ("mxdspidev: nothing for minor %d\n", iminor (inode));


#ifdef __USE_EXTERNAL_INTR__
	printk ("enable irq!\n");
	enable_irq (store_irq);
#endif

	mutex_unlock (&device_list_lock);
	unlock_kernel ();

	printk ("status =%d\n", status);
	return status;
}

static int
mxdspidev_release (struct inode *inode, struct file *filp)
{
	struct gpsspidev_data *mxdspidev;
	int status = 0;

	mutex_lock (&device_list_lock);
	mxdspidev = filp->private_data;
	filp->private_data = NULL;
	printk ("liwk mxdspidev_release\n");

//add by zhangbing to disable intr.
#ifdef __USE_EXTERNAL_INTR__
	printk ("disable irq!\n");
	disable_irq (store_irq);
#endif


	/* last close? */
	mxdspidev->users--;
	if (!mxdspidev->users)
	{
		int dofree;

		kfree (mxdspidev->buffer);
		mxdspidev->buffer = NULL;

		/* ... after we unbound from the underlying device? */
		spin_lock_irq (&mxdspidev->spi_lock);
		dofree = (mxdspidev->spi == NULL);
		spin_unlock_irq (&mxdspidev->spi_lock);

		if (dofree)
			kfree (mxdspidev);
	}
	mutex_unlock (&device_list_lock);

	return status;
}

static struct file_operations mxdspidev_fops = {
	.owner = THIS_MODULE,
	/* REVISIT switch to aio primitives, so that userspace
	 * gets more complete API coverage.  It'll simplify things
	 * too, except for the locking.
	 */
	.write = mxdspidev_write,
	.read = mxdspidev_read,
	.unlocked_ioctl = mxdspidev_ioctl,

#ifdef __USE_EXTERNAL_INTR__
	.poll = mxdspidev_poll,
#endif
	.open = mxdspidev_open,
	.release = mxdspidev_release,
	.mmap = mxdspidev_mmap,
};

#define MXD_SPI_BITS_PER_WORD					(32)
#define MXD_SPI_WORK_SPEED_HZ					(24000000)
#define MXD_SPI_INIT_SPEED_HZ					(24000000)


/*-------------------------------------------------------------------------*/

/* The main reason to have this class is to make mdev/udev create the
 * /dev/mxdspidevB.C character device nodes exposing our userspace API.
 * It also simplifies memory management.
 */

static struct class *mxdspidev_class;
struct mxd0251_platform_data
{
	//int gpio_irq;
	int gpio_cs;
	int (*power) (int onoff);
};

/*-------------------------------------------------------------------------*/

static int
gpsspidev_probe (struct spi_device *spi)
{
	struct gpsspidev_data *mxdspidev;
	int status;
	unsigned long minor;

	//struct mxd_spi_phy *phy = (struct mxd_spi_phy *)gphy;
	struct mxd0251_platform_data *pdata = spi->controller_data;
	int ret;
	int gpio_irq;
	int irq;

	if (!pdata)
		return -EINVAL;

	/* Allocate driver data */
	mxdspidev = kzalloc (sizeof (*mxdspidev), GFP_KERNEL);
	if (!mxdspidev)
	{
		printk ("kzalloc fail !!\n");
		return -ENOMEM;
	}

	/* Initialize the driver data */
	mxdspidev->spi = spi;
	spin_lock_init (&mxdspidev->spi_lock);
	mutex_init (&mxdspidev->buf_lock);
	INIT_LIST_HEAD (&mxdspidev->device_entry);

	printk ("gpsspidev_probe spi cs gpio: %d\n", pdata->gpio_cs);
	spi_cs_gpio = pdata->gpio_cs;
	if (spi_cs_gpio == 0 || spi_cs_gpio == -1)
	{
		printk ("mxd spi cs pin error!\n");
		goto err_gpio;
	}
	//add by zhangbing for intr.
#ifdef __USE_EXTERNAL_INTR__
	//for cmmb wait queue.
	init_waitqueue_head (&mxdspidev->iowait);

	/* Initialize the interrupt pin for chip */
	gpio_irq = sprd_3rdparty_gpio_wifi_irq;
	gpio_free (gpio_irq);
	ret = gpio_request (gpio_irq, "demod int");
	if (ret)
	{
		printk ("mxd spi req gpio err: %d\n", gpio_irq);
		goto err_gpio;
	}


	irq = sprd_alloc_gpio_irq (gpio_irq);
	store_irq = irq;
	printk ("mxd spi irq req 1:%d,%d\n", sprd_3rdparty_gpio_wifi_irq, irq);


	gpio_direction_input (gpio_irq);
	set_irq_type (irq, IRQ_TYPE_EDGE_FALLING);
	ret =
		request_irq (irq, mxdspidev_irq, IRQF_TRIGGER_FALLING, "MXDSPIIRQ",
					 mxdspidev);
	disable_irq (irq);
	if (ret)
	{
		printk ("CMMB request irq failed.\n");
		goto err_irq;
	}
	printk ("request_irq OK\n");
#endif
	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */

	mutex_lock (&device_list_lock);
	minor = find_first_zero_bit (minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS)
	{
		struct device *dev;

		mxdspidev->devt = MKDEV (SPIDEV_MAJOR, minor);
		dev =
			device_create (mxdspidev_class, &spi->dev, mxdspidev->devt,
						   mxdspidev, "gpsspidev");
		//mxdspidev, "mxdspidev%d.%d",
		//spi->master->bus_num, spi->chip_select);
		status = IS_ERR (dev) ? PTR_ERR (dev) : 0;
	}
	else
	{
		dev_dbg (&spi->dev, "no minor number available!\n");
		status = -ENODEV;
	}
	if (status == 0)
	{
		set_bit (minor, minors);
		list_add (&mxdspidev->device_entry, &device_list);
	}
	mutex_unlock (&device_list_lock);
	if (status == 0)
		spi_set_drvdata (spi, mxdspidev);
	else
		kfree (mxdspidev);

	// comip_mxd0251_power(0);
	return status;

  err_irq:
	gpio_free (gpio_irq);
  err_gpio:
	printk (" mxdspidev_probe gpio error!\n");

	return ret;
}

static int
mxdspidev_remove (struct spi_device *spi)
{
	struct gpsspidev_data *mxdspidev = spi_get_drvdata (spi);

	/* make sure ops on existing fds can abort cleanly */
	spin_lock_irq (&mxdspidev->spi_lock);
	mxdspidev->spi = NULL;
	spi_set_drvdata (spi, NULL);
	spin_unlock_irq (&mxdspidev->spi_lock);

	/* prevent new opens */
	mutex_lock (&device_list_lock);
	list_del (&mxdspidev->device_entry);
	device_destroy (mxdspidev_class, mxdspidev->devt);
	clear_bit (MINOR (mxdspidev->devt), minors);
	if (mxdspidev->users == 0)
		kfree (mxdspidev);
	mutex_unlock (&device_list_lock);

	//comip_mxd0251_power(0);

	return 0;
}


/****************************
 * SPI R/W functions
 ****************************/
static void
packcmd (unsigned addr, unsigned char cmd, u32 * buf, u32 len)
{
	u32 data, block_cnt;

#if 0
	buf[0] = cmd;
	buf[1] = (addr >> 24) & 0xff;
	buf[2] = (addr >> 16) & 0xff;
	buf[3] = (addr >> 8) & 0xff;
	buf[4] = (addr) & 0xff;
#else
	if (cmd & 0x8000)			/*write cmd */
	{
		data = ((1 << 31) |		/*cmd */
				(addr & 0x1FFFF) | ((len - 1) & 0x1FF) << 20);
		buf[0] = data;
	}
	else if (cmd & 0x4000)		/*read cmd */
	{
		data = ((1 << 30) | (addr & 0x1FFFF) | ((len - 1) & 0x1FF) << 20);
		buf[0] = data;
	}
	else if (cmd & 0x1)			/*need modify */
	{
		block_cnt = (len - 1) & 0xF;

		buf[0] = block_cnt;
	}


#endif
}


int
timeout_check_spi (void)
{

	u32 read_data;
	u32 addr;
	u32 count;

	addr = 0x85 * 4;			/*fifo status */
	count = 0;
	do
	{

		count++;
		gps_spi_read_bytes (1, addr, &read_data);

	}
	while ((read_data & BIT_1) && (count < 0x10));

}


#define  CHECK_STATUS_FIFO     timeout_check_spi

int
gps_rf_control_init (void)
{
	u32 write_cmd[10], data;
	u32 len;
	u32 addr;

	addr = 0x0081 * 4;
	len = 1;					/*data len,it does not include the cmd */
//    write_cmd[0] = SPI_WRITE_CMD(len,addr+0x3000);
	data = 0x0000101F;			/*bit_12,0-4len */

	/*configure the spi between the rf */
	gps_spi_write_bytes (len, addr, data);

	addr = 0x0082 * 4;
	write_cmd[0] = SPI_WRITE_CMD (len, addr + 0x3000);
	CHECK_STATUS_FIFO ();

	write_cmd[1] = 0xA2951A30;
//write_cmd[1] =reg_CONF1.dwValue;
	gps_spi_write_bytes (len, addr, write_cmd[1]);
	CHECK_STATUS_FIFO ();
//write_cmd[2] =reg_CONF2.dwValue;
//write_cmd[2] = 0x89604881; //--ADC BITS =2
	write_cmd[2] = 0x89604081;	//ADC BITS =1
	gps_spi_write_bytes (len, addr, write_cmd[2]);
	CHECK_STATUS_FIFO ();

	write_cmd[3] = 0xFEFF1DC2;
	gps_spi_write_bytes (len, addr, write_cmd[3]);
	CHECK_STATUS_FIFO ();

	write_cmd[4] = 0x9EC00083;	//
	gps_spi_write_bytes (len, addr, write_cmd[4]);
	CHECK_STATUS_FIFO ();

	write_cmd[5] = 0x0C000804;	//
	gps_spi_write_bytes (len, addr, write_cmd[5]);
	CHECK_STATUS_FIFO ();
	write_cmd[6] = 0x80000705;	//
	gps_spi_write_bytes (len, addr, write_cmd[6]);
	CHECK_STATUS_FIFO ();
	write_cmd[7] = 0x80000006;	//
	gps_spi_write_bytes (len, addr, write_cmd[7]);
	CHECK_STATUS_FIFO ();
	write_cmd[8] = 0x10061B27;	//
	gps_spi_write_bytes (len, addr, write_cmd[8]);
	CHECK_STATUS_FIFO ();

}



int
gps_spi_write_bytes (u32 len, u32 addr, u32 data)
{
	u32 write_cmd[2], one_read;

	printk ("enter write addr %x len %x,data#=0x%x\n", addr, len, data);


	if (addr > 0x6000)
	{
		BUG_ON (1);
		return 0;
	}
	if (len == 1)
	{
		write_cmd[0] = SPI_WRITE_CMD (len * 1, addr / 4 + 0x3000);
		write_cmd[1] = data;
		spi_transfer (&write_cmd[0], 8, NULL, 0, NULL, 0, true);
	}
	return 0;
}

EXPORT_SYMBOL_GPL (gps_spi_write_bytes);

static int
spi_read_block (u8 * data, u32 block)
{
	u32 read_cmd, write_cmd;

}

/*make sure len is word units*/
int
gps_spi_read_bytes (u32 len, u32 addr, u32 * data)
{
	u32 read_cmd[2];

	//       printk("enter read addr  %x len %x  %p \n",addr,len,data);


	if (len == 1)				/*one block all way */
	{
		read_cmd[0] = SPI_READ_CMD (len, addr / 4 + 0x3000);
		spi_transfer (NULL, 0, (u8 *) & read_cmd[0], 4,
					  (u8 *) & read_cmd[1], 4, true);

		*data = read_cmd[1];
	}

	else						/*more than one word,block */
	{
		u32 read_cmd_block[2];

#if 1
		/*len word unit */
		read_cmd_block[0] = SPI_BLOCK_READ_CMD (1);
		read_cmd_block[1] = SPI_BLOCK_DATA_ENC (len, addr);

		spi_transfer (NULL, 0, &read_cmd_block[0], 8, (u8 *) data, len * 4,
					  true);

		read_cmd[1] = data[0];
#else
		read_cmd[0] = SPI_READ_CMD (1, addr);
		spi_transfer (NULL, 0, (u8 *) & read_cmd[0], 4,
					  (u8 *) & read_cmd[1], 4, true);

		*data = read_cmd[1];

#endif

		//       printk(" end read addr value 0x%x \n",data[1]);

	}


	return 0;
}

EXPORT_SYMBOL_GPL (gps_spi_read_bytes);


#define GPIO_CFG_REG_BASE(io_num, offset)	(GPIO_BASE+ (((io_num)>>4)-1)*0x80 + offset)
#define GPIO_CFG_REG_SET(io_num, offset)  	(*((volatile uint32 *)(GPIO_CFG_REG_BASE(io_num, offset))) |= (0x01<<((io_num)&0xf)))
#define GPIO_CFG_REG_CLR(io_num, offset)  	(*((volatile uint32 *)(GPIO_CFG_REG_BASE(io_num, offset))) &= ~(0x01<<((io_num)&0xf)))

#define TROUT_GPIO_INT_NUM				142
#define TROUT_SPI_WRITE_CTL_GPIO_NUM 	24

#define SET_SPI1CS0_GPIO22

#ifdef  SET_SPI1CS0_GPIO22
#define TROUT_SPI_WR_CMD_EN() \
do{\
	uint32 count;\
	GPIO_CFG_REG_CLR(22, GPIO_DATA);\
	GPIO_CFG_REG_CLR(24, GPIO_DATA);\
	for(count=0;count<0x4;count++);\
}while(0)

#define TROUT_SPI_WR_CMD_DIS() \
do{\
	uint32 count;\
	for(count=0;count<0x4;count++);\
	GPIO_CFG_REG_SET(22, GPIO_DATA);\
	GPIO_CFG_REG_SET(24, GPIO_DATA);\
	for(count=0;count<0x4;count++);\
}while(0)
#endif


static void
spi_assert_function (char on_off)
{
	if (on_off)
	{
		gpio_set_value (22, 1);
		// gpio_set_value(24,1);
	}
	else
	{
		gpio_set_value (22, 0);
		// gpio_set_value(24,0);
	}
}


static void
spi_transfer (unsigned char *tx1_buf, int write_len,
			  unsigned char *tx2_buf, int tx2_len,
			  unsigned char *rx_buf, int rx_len, bool cs_change)
{
	int status;
	struct spi_message m;

	struct spi_transfer _tx_1 = {
		.tx_buf = tx1_buf,
		.len = write_len,
		.cs_change = cs_change,
		.bits_per_word = 32,
		// .delay_usecs = 200,
	};
	struct spi_transfer _tx_2 = {
		.tx_buf = tx2_buf,
		.len = tx2_len,
		.cs_change = 1,
		.bits_per_word = 32,
		// .delay_usecs = 200,
	};

	struct spi_transfer _rx = {
		.rx_buf = rx_buf,
		.len = rx_len,

		.bits_per_word = 32,
		.cs_change = 1,
	};

	spi_assert_function (0);

	spi_message_init (&m);
	if (write_len)
		spi_message_add_tail (&_tx_1, &m);
	if (tx2_len)
		spi_message_add_tail (&_tx_2, &m);
	if (rx_len)
		spi_message_add_tail (&_rx, &m);

	if (!list_empty (&m.transfers))
	{

		status = spi_async (gps_spi_dev, &m);
		//status    = spi_async(gps_spi_dev, &m);
		printk ("spi_async status %x \n", status);


	}
	spi_assert_function (1);


}


void
write_temp_reg (void)
{
	u32 txb[2] = { 0 };
	u32 rxb[2] = { 0 };


	txb[0] = SPI_WRITE_CMD (1, 0x00003086);
	txb[1] = 0x76543210;
	rxb[0] = SPI_READ_CMD (1, 0x00003086);
	rxb[1];
	spi_transfer (&txb[0], 8, &rxb[0], 4, &rxb[1], 4, true);

	printk ("write_temp_reg rxb=0x%x \n", rxb[1]);

}



/*should two times for problem*/
void
reset_cg_version_reg (void)
{
	u32 txb[2] = { 0 };
	u32 rxb[2] = { 0 };

	txb[0] = SPI_WRITE_CMD (1, 0x0000303F);
	txb[1] = 0xF;
	rxb[0] = SPI_READ_CMD (1, 0x00003011);
	rxb[1];
	printk ("###Reset command send now###\n");
	spi_transfer (&txb[0], 8, 0, 0, 0, 0, true);
	printk ("###Read Vesion reg command send now###\n");
	spi_transfer (0, 0, &rxb[0], 4, &rxb[1], 4, true);
	printk ("###Read Vesion reg command finish now value=0x%x###\n",
			rxb[1]);
	//   spi_transfer(&txb[0],8, 0, 0, 0, 0, true);
	//   spi_transfer(0,0,&rxb[0], 4, &rxb[1], 4 ,true);

}

static int
gps_spi_test (u8 * buf, u32 cnt)
{
	int count;
	unsigned long flags;
	u32 txb[2] = { 0 };
	u32 rxb[2] = { 0 };
	u32 write_buf[10] =
		{ 0xaaaa5555, 0xaaaa5555, 0, 0xaaaa5555, 0xaaaa5555,
		0xaaaa5555, 0xaaaa5555, 0xaaaa5555, 0xaaaa5555, 0xaaaa5555
	};

	printk ("gps_spi_test\n");

	irq_number = 0;
	addr_flag_side = 0;

	cg_block_num = 0;
	cg_block_size = 0;
	//packcmd(0x2001,);/*0x2000 version ,0x2001  read/write*/
#if 0

	txb[0] = SPI_WRITE_CMD (1, 0x00002001);
	txb[1] = 0xaaaa5555;
	rxb[0] = SPI_READ_CMD (1, 0x00002001);
	rxb[1];
#else
//    txb[0] = SPI_WRITE_CMD(1,0x00003081);txb[1] = 0xaaaa5555;
	// rxb[0] = SPI_READ_CMD(1,0x00003081); rxb[1];


//read rw register
	//   txb[0] = SPI_WRITE_CMD(1,0x00003086);txb[1] = 0xaaaa5555;
	//   rxb[0] = SPI_READ_CMD(1,0x00003086); rxb[1];

//reset
//   txb[0] = SPI_WRITE_CMD(1,0x0000303F);txb[1] = 0xF;
	//  rxb[0] = SPI_READ_CMD(1,0x00003000); rxb[1];

//reset and version

/*
       txb[0] = SPI_WRITE_CMD(1,0x0000303F);txb[1] = 0xF;
       rxb[0] = SPI_READ_CMD(1,0x00003011); rxb[1];

       spi_transfer(&txb[0],8, 0, 0, 0, 0, true);
       spi_transfer(0,0,&rxb[0], 4, &rxb[1], 4 ,true);
*/


	{
		reset_cg_version_reg ();

	}

/*generate the high signal for dma_req*/


/*
       txb[0] = SPI_WRITE_CMD(1,0x00003086);txb[1] = 0x76543210;
       rxb[0] = SPI_READ_CMD(1,0x00003086); rxb[1];
       spi_transfer(&txb[0],8, &rxb[0], 4, &rxb[1], 4, true);
*/

#endif

	//spi_transfer(&txb[0], 8, &rxb[0], 4, &rxb[1], 4, true);


	//count = 10;
	// while(count--)
	// spi_transfer(&txb[0],4,0,0,0,0,true);    /*write*/

	//spi_transfer(&txb[0],8, &rxb[0], 4, &rxb[1], 4, true);

	count = 1;
	while (count--)
	{
		if (1)
		{
			//spi_transfer(0,0, 0,0, &rxb[1], 4, true);
			//spi_transfer(&txb[0],8, &rxb[0], 4, &rxb[1], 4, true);

			/*reset and read version */
			//spi_transfer(&txb[0],8, 0, 0, 0, 0, true);
			//  spi_transfer(0,0,&rxb[0], 4, &rxb[1], 4 ,true);


#if  0
			/*rf test send the control data */
			txb[0] = SPI_WRITE_CMD (1, 0x00003081);
			txb[1] = 0x0101F;
			spi_transfer (&txb[0], 8, 0, 0, 0, 0, true);

			CHECK_STATUS_FIFO ();

			txb[0] = SPI_WRITE_CMD (1, 0x00003082);
			txb[1] = 0xA2959A3;
			spi_transfer (&txb[0], 8, 0, 0, 0, 0, true);
#endif

		}
		if (0)
		{
			/*write */
			write_buf[0] = txb[0];
			spi_transfer (&write_buf[0], 20, 0, 0, 0, 0, true);	/*write */
		}

		enable_irq (g_irq);


		if (1)
		{
                    gps_rf_control_init ();
                 //   GPS_FPGA_Configure(); //add by wlh for gps fpga default configure
                  //  msleep(6);
		}

		if (0)
		{
			work_queue_test ();
		}

		if (0)
		{
			local_irq_save (flags);
			snap_reg_init ();
			local_irq_restore (flags);
		}



		//enable_irq(gp_start_irq);

	}
	//printk("test spi test->txb 0x%x    rxb=0x%x \n", txb[0], rxb[1]);
	return 0;
}

EXPORT_SYMBOL_GPL (gps_spi_test);

struct spi_device *
gps_get_spi_device (void)
{
	return gps_spi_dev;
}

EXPORT_SYMBOL_GPL (gps_get_spi_device);



static struct spi_driver gpsspidev_spi = {
	.driver = {
			   .name = "spi_slot0",
			   .owner = THIS_MODULE,
			   },
	.probe = gpsspidev_probe,
	.remove = __devexit_p (mxdspidev_remove),

	/* NOTE:  suspend/resume methods are not necessary here.
	 * We don't do anything except pass the requests to/from
	 * the underlying controller.  The refrigerator handles
	 * most issues; the controller driver handles the rest.
	 */
};


/*-------------------------------------------------------------------------*/

/*
void*  global_buf = NULL;
int size_num = 4000000;
*/

//static int __init gpsspidev_init(void)
int
gpsspidev_init (void)
{
#if 1
	int major;
	int order_index;

	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	BUILD_BUG_ON (N_SPI_MINORS > 256);

/*
        order_index  = get_order(size_num);
        global_buf = (void*)__get_free_pages(GFP_KERNEL,order_index);

        if(!global_buf)
         {
                printk("error error,not get pages");
        }

        printk("global_buf = %x, order=%x\n",global_buf,order_index);
*/

	spi_module_init ();

	major = register_chrdev (0, "spi", &mxdspidev_fops);
	if (major < 0)
	{
		printk ("register_chrdev fail !!major = %d\n", major);
		return major;
	}

	SPIDEV_MAJOR = major;

	mxdspidev_class = class_create (THIS_MODULE, "gpsspidev");
	if (IS_ERR (mxdspidev_class))
	{
		unregister_chrdev (major, gpsspidev_spi.driver.name);
		printk ("class_create fail !!major = %d\n", major);
		return PTR_ERR (mxdspidev_class);
	}

	major = spi_register_driver (&gpsspidev_spi);
	if (major < 0)
	{
		class_destroy (mxdspidev_class);
		unregister_chrdev (major, gpsspidev_spi.driver.name);
		printk ("spi_register_driver fail !!major = %d\n", major);
	}
	printk ("mxdspidev_init end major=%d!\n", major);

	return major;
#endif
}




static void __exit
gpsspidev_exit (void)
//void  gpsspidev_exit(void)
{
	spi_unregister_driver (&gpsspidev_spi);
	class_destroy (mxdspidev_class);
	unregister_chrdev (SPIDEV_MAJOR, gpsspidev_spi.driver.name);
}

#if 0
module_init (gpsspidev_init);

module_exit (gpsspidev_exit);


MODULE_AUTHOR ("paul.luo");
MODULE_DESCRIPTION ("gps Use mode SPI device interface");
MODULE_LICENSE ("GPL");
#endif
