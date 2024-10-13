/******************************************************************************
 *                                LEGAL NOTICE                                *
 *                                                                            *
 *  USE OF THIS SOFTWARE (including any copy or compiled version thereof) AND *
 *  DOCUMENTATION IS SUBJECT TO THE SOFTWARE LICENSE AND RESTRICTIONS AND THE *
 *  WARRANTY DISLCAIMER SET FORTH IN LEGAL_NOTICE.TXT FILE. IF YOU DO NOT     *
 *  FULLY ACCEPT THE TERMS, YOU MAY NOT INSTALL OR OTHERWISE USE THE SOFTWARE *
 *  OR DOCUMENTATION.                                                         *
 *  NOTWITHSTANDING ANYTHING TO THE CONTRARY IN THIS NOTICE, INSTALLING OR    *
 *  OTHERISE USING THE SOFTWARE OR DOCUMENTATION INDICATES YOUR ACCEPTANCE OF *
 *  THE LICENSE TERMS AS STATED.                                              *
 *                                                                            *
 ******************************************************************************/
/* Version: 1.8.9\3686 */
/* Build  : 13 */
/* Date   : 12/08/2012 */


/**
    \cond OS_LINUX
*/

/**
	\file
	\brief Linux specific driver code
	\attention This file should not be modified.
		If you think something is wrong here, please contact CellGuide
	Specific code required for the CellGuide GPS driver for Linux.
*/
// ===========================================================================

#define __CGXLINUXDRIVER_C__
#include <asm/io.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/completion.h>
#include <linux/dma-mapping.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#endif

#include "CgTypes.h"
#include "CgReturnCodes.h"
#include "CgCpu.h"
#include "CgxDriverCore.h"
#include "CgxDriverOs.h"
#include "CgxDriverPlatform.h"

#include "platform.h"
#include "CgCpuOs.h"


extern U32 CGCORE_REG_OFFSET_CORE_RESETS;
extern U32 CGCORE_REG_OFFSET_VERSION;



struct class       * gps_class;
struct device      * gps_dev;



/*paulluo add test*/
//#define FIX_MEMORY
#ifdef CGCORE_ACCESS_VIA_SPI
extern int  gpsspidev_init(void);
#endif


// ===========================================================================

//
// Driver instance structure.
//
typedef struct {
	struct cdev				 cdev;
	int						 nNumOpens;
	int						 DataReady_irq;
	int						 GpsIntr_irq;
	struct completion		 xferCompleted; // was rcvSem
	TCgxDriverPhysicalMemory chunk;
	TCgxDriverState			 state;
	void					 *xfer_buff_ptr;	  // Internal buffer pointer
	unsigned long			 xfer_buff_len;		  // Internal buffer len (bytes)
	int						 xfer_buff_pg_order;  // Internal buffer page order (log2(n))
	int						 xfer_buff_mmapped;	  // Internal buffer is mapped
	struct fasync_struct	*async_queue;
	wait_queue_head_t		 resumeq;
	char					 pm_resumed;
} DRVCONTEXT;

// Use a static DriverContext, as only one driver instance is needed
static DRVCONTEXT			   DrvContext;
//bxd add for first alloc mem failed from engine
static void *init_buff_ptr;

static void GpsIntr_BH(struct work_struct *work);
DECLARE_WORK( girq_work, GpsIntr_BH );
static struct workqueue_struct *girq_wq	 = NULL;
#define GIRQ_WQ_NAME			"GIRQ WorkQueue"

static void DataReady_BH(struct work_struct *work);
DECLARE_WORK( drdy_work, DataReady_BH );
static struct workqueue_struct *drdy_wq	 = NULL;
#define DRDY_WQ_NAME			"DRDY WorkQueue"

// In certain devices, there will not be an irq for DMA ready, but instead, a wait-able, exported sync-object.
#ifdef CGX_DRIVE_DMA_INT_GLOBAL_SYNC_OBJECT

	static void DataReady_Completion_WorkQueue(struct work_struct *work);
	DECLARE_WORK( drdy_completion_work, DataReady_Completion_WorkQueue );
	static struct workqueue_struct *drdy_completion_wq	 = NULL;
	#define DRDY_COMPLETION_WQ_NAME			"DRDY Completion WorkQueue"

	extern struct completion CGX_DRIVE_DMA_INT_GLOBAL_SYNC_OBJECT;

	/*
	DataReady Interrupt Handlers:
	DataReady_BH: The Bottom-Half (Handler Thread)
	DataReady_TH: The Top-Half (ISR)
	CgxDriverDataReadyInterruptHandlerStart: Register the DataReady ISR
	*/
	static void DataReady_Completion_WorkQueue(struct work_struct *work)
	{
		//	DBG_FUNC_NAME("DataReady_BH")
		//	DBGMSG("START");
		while (1)
		{
			wait_for_completion( &CGX_DRIVE_DMA_INT_GLOBAL_SYNC_OBJECT );
			CgxDriverDataReadyInterruptHandler(&DrvContext, &DrvContext.state);
		}

		//	DBGMSG("END");
	}
#endif

static int					  cgxdrv_major = 0;   /* 0 = dynamic major */
#define MODULE_NAME			 "cgxdrv"


/*
	DataReady Interrupt Handlers:
	DataReady_BH: The Bottom-Half (Handler Thread)
	DataReady_TH: The Top-Half (ISR)
	CgxDriverDataReadyInterruptHandlerStart: Register the DataReady ISR
 */
static void DataReady_BH(struct work_struct *work)
{
	//	DBG_FUNC_NAME("DataReady_BH")
	//	DBGMSG("START");
	CgxDriverDataReadyInterruptHandler(&DrvContext, &DrvContext.state);
	//enable_irq( DrvContext.DataReady_irq );
	//	DBGMSG("END");
}

static irqreturn_t DataReady_TH(int irq, void *dev_id)
{
	//	DBG_FUNC_NAME("DataReady_TH")
	//	DBGMSG("START");
	//disable_irq_nosync( irq );

	queue_work(drdy_wq, &drdy_work);
	//	DBGMSG("END");

	return IRQ_HANDLED;
}


#ifdef CGCORE_ACCESS_VIA_SPI

void* Data_Get_DrvContext()
{

        return DrvContext.xfer_buff_ptr;
}
void DataReady_TH_start(void )
{

    CgxDriverDataReadyInterruptHandler(&DrvContext, &DrvContext.state);

}
#endif


void CCgCpuDmaHandle(int irq,void* dev_id)
{
	CgxDriverDataReadyInterruptHandler(&DrvContext, &DrvContext.state);
}


TCgReturnCode CgxDriverDataReadyInterruptHandlerStart(void *pDriver)
{
	DBG_FUNC_NAME("CgxDriverDataReadyInterruptHandlerStart")
	DRVCONTEXT *pDriverInfo = (DRVCONTEXT *)pDriver;

	int	 rv = 0;

	// First check if there is an irq dedicated to the DataReady event
	pDriverInfo->DataReady_irq = CgxDriverDataReadyInterruptCode();
	if( pDriverInfo->DataReady_irq != 0)
	{
		drdy_wq = create_singlethread_workqueue(DRDY_WQ_NAME);

		if (drdy_wq == NULL) {
			DBGMSG("create_singlethread_workqueue(" DRDY_WQ_NAME ") Failed");
			return ECgInitFailure;
		}

		rv = request_irq( pDriverInfo->DataReady_irq, DataReady_TH, IRQF_DISABLED, "drdy-bh", NULL );
		if (rv != 0) {
			DBGMSG1("request_irq(%d) Failed", pDriverInfo->DataReady_irq );
			return ECgInitFailure;
		}
		DBGMSG1("request_irq(DRDY/%d) OK", pDriverInfo->DataReady_irq );
	}
	else
	{
		// There is no irq. Lets check if there is an exported sync-object
		#ifdef CGX_DRIVE_DMA_INT_GLOBAL_SYNC_OBJECT
			drdy_completion_wq = create_singlethread_workqueue(DRDY_COMPLETION_WQ_NAME);
			if ( OK(drdy_completion_wq) )
				queue_work(drdy_completion_wq, &drdy_completion_work);
			else
				return ECgInitFailure;
		#else
			DBGMSG1("request_irq(DRDY/%d) Skipped", pDriverInfo->DataReady_irq );
		#endif
	}

	return ECgOk;
}



/*
	GpsIntr Interrupt Handlers:
	GpsIntr_BH: The Bottom-Half (Handler Thread)
	GpsIntr_TH: The Top-Half (ISR)
	CgxDriverGeneralInterruptHandlerStart: Register the GpsIntr ISR
 */
static void GpsIntr_BH(struct work_struct *work)
{
	//	DBG_FUNC_NAME("GpsIntr_BH")
	//	DBGMSG("START");
	CgxDriverGpsInterruptHandler(&DrvContext, &DrvContext.state);
	//	DBGMSG("END");
	enable_irq( DrvContext.GpsIntr_irq );
}

static irqreturn_t GpsIntr_TH(int irq, void *dev_id)
{
	//	DBG_FUNC_NAME("GpsIntr_TH")
	//	DBGMSG("START");
	disable_irq_nosync( irq );

	queue_work(girq_wq, &girq_work);

	//	DBGMSG("END");
	return IRQ_HANDLED;
}

TCgReturnCode CgxDriverGpsInterruptHandlerStart(void *pDriver)
{
	DBG_FUNC_NAME("CgxDriverGpsInterruptHandlerStart")
	DRVCONTEXT *pDriverInfo = (DRVCONTEXT *)pDriver;

	int	 rv = 0;
	pDriverInfo->GpsIntr_irq = CgxDriverGpsInterruptCode();
	if (pDriverInfo->GpsIntr_irq != 0)
	{
		girq_wq = create_singlethread_workqueue(GIRQ_WQ_NAME);
		if (girq_wq == NULL) {
			DBGMSG("create_singlethread_workqueue(" GIRQ_WQ_NAME ") Failed");
			return ECgInitFailure;
			}

		rv = request_irq( pDriverInfo->GpsIntr_irq, GpsIntr_TH, IRQF_DISABLED, "girq-bh", NULL );
		if (rv != 0) {
			DBGMSG1("request_irq(%d) Failed", pDriverInfo->GpsIntr_irq );
			return ECgInitFailure;
			}
		DBGMSG1("request_irq(GPS/%d) OK", pDriverInfo->GpsIntr_irq );
	}
	else
		DBGMSG1("request_irq(GPS/%d) Skipped", pDriverInfo->GpsIntr_irq );

	return ECgOk;
}





TCgReturnCode CgxDriverTransferEndCreate(void *pDriver)
{
	DBG_FUNC_NAME("CgxDriverTransferEndCreate")
	DRVCONTEXT *pDriverInfo = (DRVCONTEXT *)pDriver;

	DBGMSG( "Entering... Initializing completion semaphore" );

	init_completion( &pDriverInfo->xferCompleted );

	DBGMSG1( "Initialized Semaphore=%u", pDriverInfo->xferCompleted.done );

	return ECgOk;
}



TCgReturnCode CgxDriverTransferEndSignal(void *pDriver)
{
	//DBG_FUNC_NAME("CgxDriverTransferEndSignal")
	DRVCONTEXT *pDriverInfo = (DRVCONTEXT *)pDriver;

/*	DBGMSG( "Entering" );*/

	//DBGMSG1( "Marking completion... Semaphore=%u", pDriverInfo->xferCompleted.done );
	complete( &pDriverInfo->xferCompleted );
	//DBGMSG1( "Marked completion... After Semaphore=%u", pDriverInfo->xferCompleted.done );

	return ECgOk;
}


TCgReturnCode CgxDriverTransferEndWait(void *pDriver, U32 aTimeoutMS)
{
	DRVCONTEXT	 *pDriverInfo = (DRVCONTEXT *)pDriver;
	TCgReturnCode   rc = ECgOk;
	unsigned long   timeout = aTimeoutMS * HZ / 1000;
	unsigned long   rv = 0 ;
	DBG_FUNC_NAME("CgxDriverTransferEndWait")

	DBGMSG1( "Timeout=%lu", timeout );


	for (;;) {
	    if (pDriverInfo->state.flags.resume == 1) {
			DBGMSG("Resume!");
			return ECgResume;
			}
		//DBGMSG1( "Waiting for completion... Semaphore=%u", pDriverInfo->xferCompleted.done );
			/*bxd modify because of can't entry susnpend when use wait_for_completion_interruptible_timeout func */
            //rv = wait_for_completion_interruptible_timeout( &pDriverInfo->xferCompleted, timeout );
			rv = wait_for_completion_timeout( &pDriverInfo->xferCompleted, timeout );
			if (rv != -ERESTARTSYS)
				break;
		}

	if ((rv == 0) && (timeout > 0))
	{
		rc = ECgTimeout;
		DBGMSG( "Timed-out" );
	}
	else
	{
		DBGMSG1( "wait_for_completion_interruptible_timeout returned=%d", rv );
	}

    if (pDriverInfo->state.flags.resume == 1) {
		DBGMSG("Resume!");
		rc = ECgResume;
	}

	return rc;
}


TCgReturnCode CgxDriverTransferEndDestroy(void *pDriver)
{
    return ECgOk;
}


//#define FIX_MEMORY

#ifdef FIX_MEMORY
unsigned char buf_data[6*1024*1024];
#else
 void* buf_data;
#endif

void clear_buf_data(void)
{
	memset(DrvContext.xfer_buff_ptr,0,DrvContext.xfer_buff_len);
}

TCgReturnCode CgxDriverAllocInternalBuff(void *pDriver, unsigned long aLength, void **apBuffer, U32 aProcessID)
{
	DBG_FUNC_NAME("CgxDriverAllocInternalBuff")
	DRVCONTEXT *pDriverInfo = (DRVCONTEXT *)pDriver;
 	DBGMSG( "Entering..." );

	/* Already alloc'ed, return error and ask to free 1st */
	if (DrvContext.xfer_buff_ptr != NULL) {
 		DBGMSG( "Returning ECgNotSupported" );
		return ECgNotSupported;
	}


      #ifndef FIX_MEMORY
	DrvContext.xfer_buff_pg_order = get_order(aLength);
	//bxd add for first alloc mem failed from engine
	if(init_buff_ptr != NULL)
		DrvContext.xfer_buff_ptr = init_buff_ptr;
	else
		DrvContext.xfer_buff_ptr	  = (void *)__get_free_pages(GFP_KERNEL, DrvContext.xfer_buff_pg_order);
	DrvContext.xfer_buff_len	  = PAGE_SIZE * (1 << DrvContext.xfer_buff_pg_order);
        buf_data  = DrvContext.xfer_buff_ptr;
        #else
	DrvContext.xfer_buff_pg_order = get_order(aLength);
	DrvContext.xfer_buff_ptr	  =(void*)&buf_data[0];
	DrvContext.xfer_buff_len	  = 6*1024*1024;

        #endif


	if (DrvContext.xfer_buff_ptr == NULL) {
		DBGMSG( "Returning ECgErrMemory" );
		return ECgErrMemory;
	}

	pDriverInfo->state.buffer.length = DrvContext.xfer_buff_len;
	pDriverInfo->state.buffer.virtAddr = DrvContext.xfer_buff_ptr;
	pDriverInfo->state.buffer.physAddr = __pa((void *)DrvContext.xfer_buff_ptr) ;
 	DBGMSG2( "Returning ECgOk - Requested: %lu - Granted: %lu", aLength, DrvContext.xfer_buff_len );

	*apBuffer = (void *)pDriverInfo->state.buffer.physAddr;
#if defined(CGTEST) || defined(DEBUG)
	memset(pDriverInfo->state.buffer.virtAddr, 0x43, aLength);
#endif
	DBGMSG3(">>>>>>> memory allocated at DrvContext.xfer_buff_ptr=[0x%08X] buffer.virtAddr=[0x%08X], buffer.physAddr=[0x%08X]",DrvContext.xfer_buff_ptr, pDriverInfo->state.buffer.virtAddr, pDriverInfo->state.buffer.physAddr );
	return ECgOk;
}

#ifdef CGCORE_ACCESS_VIA_SPI
int get_blocksize()
{
    return DrvContext.state.transfer.blockSize;
}
#endif


void *CgxDriverGetInternalBuff(void *pSource, U32 length)
{
// 	if (length != DrvContext.xfer_buff_len)
// 		return NULL;

	return DrvContext.xfer_buff_ptr;
}



TCgReturnCode CgxDriverFreeInternalBuff(void *pDriver, U32 unUsed)
{
	/* Already alloc'ed, return error and ask to free 1st */
	if (DrvContext.xfer_buff_ptr == NULL)
		return ECgNotSupported;

	/* Buffer is mmap'ed. return error and ask to free 1st */
	if (DrvContext.xfer_buff_mmapped)
		return ECgNotAllowed;

#ifndef FIX_MEMORY
//bxd
	//free_pages((unsigned long)DrvContext.xfer_buff_ptr, DrvContext.xfer_buff_pg_order);
#endif

	DrvContext.xfer_buff_ptr	  = NULL;
	DrvContext.xfer_buff_len	  = 0;
	DrvContext.xfer_buff_pg_order = 0;

	//bxd add for first alloc mem failed from engine
	//if(init_buff_ptr != NULL)
	//	init_buff_ptr = NULL;

	return ECgOk;
}



unsigned long CgxCopyFromUser(void *to, const void *fromuser, unsigned long n)
{
	return copy_from_user(to, fromuser, n);
}






//======================================================================
// CGX_IOControl - Called when DeviceIOControl called
//
//static int CGX_IOControl(struct inode *inode,struct file *filp,
static long CGX_IOControl(struct file *filp,

				  unsigned int cmd, unsigned long arg)
{
	DBG_FUNC_NAME("CGX_IOControl")
	TCgReturnCode	  rc  = ECgOk;
	TCgDriverControl   In;
	TCgDriverStatus	Out;

//printk("cmd:0x%x,arg:0x%x\n",cmd,arg);
 /*	DBGMSG1( "Entering...[%d]" , cmd); */


	// Check we can read the TCgDriverControl from User Space
	if ( !access_ok( VERIFY_READ, (void __user *)arg, sizeof(TCgDriverControl) )  ) {
 		DBGMSG( "Returning -EFAULT 1" );
		return -EFAULT;
	}

	// Check we can write the TCgDriverStatus to User Space
	if ( !access_ok( VERIFY_WRITE, (void __user *)arg, sizeof(TCgDriverStatus) )  ) {
 		DBGMSG( "Returning -EFAULT 2" );
		return -EFAULT;
	}

	if (  copy_from_user( &In, (void __user *)arg, sizeof(In) )  ) {
		DBGMSG( "Returning -EFAULT 3" );
		return -EFAULT;
	}

	rc = CgxDriverExecute( &DrvContext, &DrvContext.state, cmd, &In, &Out);

	if (  copy_to_user( (void __user *)arg, &Out, sizeof(Out) )  ) {
 		DBGMSG( "Returning -EFAULT 4" );
		return -EFAULT;
	}

 	DBGMSG1( "Returning %d", (OK(rc)) ? 0 : -ENOTTY );
	return (OK(rc)) ? 0 : -ENOTTY;

}


int CGX_CompleteDrv(void)
{
        complete(&DrvContext.xferCompleted);
		return 0;
}

EXPORT_SYMBOL_GPL(CGX_CompleteDrv);

//======================================================================
// CGX_Fasync - Called when driver closed
//
static int	 CGX_Fasync( int fd, struct file *filp, int mode)
{
	return fasync_helper(fd, filp, mode, &DrvContext.async_queue);
}

unsigned int CgxDriverCheckAllocInternalBuff(void)
{

	if (DrvContext.xfer_buff_ptr != NULL) {
		printk( "\n already alloc page \n" );
		return 0;
	}
	return 1;
}



//======================================================================
// CGX_Close - Called when driver closed
//
static int	 CGX_Close( struct inode *inode, struct file *filp)
{
/* 	DBGMSG( "Entering..." ); */

#if 0
	//gaole add begin
	int rc = 0;

	CgCoreReleaseRes();
	if (CgxDriverCheckAllocInternalBuff() == 0)
	{
	   	printk("\n gaole: close check if old page be released \n");

		rc = CgxDriverFreeInternalBuff(NULL, 0);

		if(ECgOk  !=  rc)
		{
			printk("\n gaole: close run CgxDriverFreeInternalBuff error,error code is %d \n",rc);
		}

	}

	rc = CgCpuDmaStop(0);
	if(ECgOk  !=  rc)
	{
		printk("\n gaole: run CgCpuDmaStop error,error code is %d \n",rc);

	}
	CgGpsReset();
#endif

	CgCpuDmaDestroy(0,0);
	// gaole add end


	if (DrvContext.nNumOpens)
		DrvContext.nNumOpens--;

	if ( filp->f_flags & FASYNC) {
			CGX_Fasync(-1, filp, 0); /* Remove from async queue */
	}

/* 	DBGMSG( "Returning 0" ); */
	return 0;
}


//======================================================================
// CGX_Read - Called when driver read
//
static ssize_t CGX_Read( struct file *filp, char __user *buff, size_t count, loff_t *offp)
{
	while (DrvContext.pm_resumed == 0) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		//DBGMSG( "Waiting for resume to take place... Going to sleep\n" );
		if (wait_event_interruptible(DrvContext.resumeq, (DrvContext.pm_resumed != 0)))
			return -ERESTARTSYS;
	}

	/*
	 * Copy 1 char sizeof(*(char*)) from DrvContext.pm_resumed to buff
	 */
	if (put_user(DrvContext.pm_resumed, (char __user *)buff)) {
		return -EFAULT;
	}

	DrvContext.pm_resumed = 0;
	return 1;
}


//======================================================================
// CGX_Write - Called when driver written
//
static ssize_t CGX_Write( struct file *filp, const char __user *buff, size_t count, loff_t *offp)
{
	return 0;
}


//======================================================================
// CGX_Poll - Called when driver select()/poll()
//
static unsigned int CGX_Poll(struct file *filp, poll_table *wait)
{
	poll_wait(filp, &DrvContext.resumeq,  wait);

	if (DrvContext.pm_resumed == 'R')
		return (POLLIN | POLLRDNORM);

	return 0;
}

//======================================================================
// CGX_Seek - Called when SetFilePtr called
//
static loff_t  CGX_Seek (struct file *filp, loff_t off, int whence)
{
	return 0;
}






//======================================================================
// CGX_Open - Called when driver opened
//
static int CGX_Open( struct inode *inode, struct file *filp )
{

	/*
	 * XXX-PK-XXX: Hold lock on DrvContext
	 */
	CgCpuDmaCreate(0,0);

/* 	DBGMSG( "Entering..." ); */

	// Count the number of opens.
	DrvContext.nNumOpens++;

/* 	DBGMSG( "Returning 0" ); */
	return 0;
}


/*
 * CGX_vma_open and CGX_vma_close keep track of how many times
 * the buffer is mapped, to avoid Freeing it while in use.
 */
static void CGX_vma_open(struct vm_area_struct *vma)
{
	DBG_FUNC_NAME("CGX_vma_open")
	DrvContext.xfer_buff_mmapped++;
	DBGMSG4("vma open [%d]: virt 0x%lx, Offs %lu, Len %lu",
			DrvContext.xfer_buff_mmapped,
			vma->vm_start, vma->vm_pgoff << PAGE_SHIFT, vma->vm_end - vma->vm_start );
}

static void CGX_vma_close(struct vm_area_struct *vma)
{
	DBG_FUNC_NAME("CGX_vma_close")
/* 	DBGMSG( "Entering..." ); */

	DrvContext.xfer_buff_mmapped--;
	DBGMSG1("vma close [%d]", DrvContext.xfer_buff_mmapped );

/* 	DBGMSG( "Exit" ); */
}

#if LINUX_VERSION_CODE >= 132634
static int CGX_vma_nopage(struct vm_area_struct *vma, struct vm_fault *vmf)
{
    return VM_FAULT_SIGBUS; // Remapping is not allowed !
}
#else
static struct page *CGX_vma_nopage(struct vm_area_struct *vma, unsigned long addr, int *type)
{
    return NOPAGE_SIGBUS; // Remapping is not allowed !
}
#endif

static struct vm_operations_struct cgx_remap_ops = {
	.open =   CGX_vma_open,
	.close =  CGX_vma_close,
	#if LINUX_VERSION_CODE >= 132634
		.fault = CGX_vma_nopage,
	#else
	.nopage = CGX_vma_nopage,
	#endif
};

static int CGX_Mmap(struct file *filp, struct vm_area_struct *vma)
{
	DBG_FUNC_NAME("CGX_Mmap")
	int	rv;
	unsigned long len	  = (vma->vm_end - vma->vm_start);
	unsigned long off	  = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long physical = __pa((void *)DrvContext.xfer_buff_ptr) + off;

	DBGMSG3( "Entering... len=%lu off=%lu DrvContext.xfer_buff_len=%lu", len, off, DrvContext.xfer_buff_len );

	if ( len > (DrvContext.xfer_buff_len - off) ) {
 		DBGMSG( "Returning -EINVAL" );
		return -EINVAL;
	}

#if  1


	rv = remap_pfn_range(vma, vma->vm_start,
						 physical >> PAGE_SHIFT, len,
						 pgprot_noncached(vma->vm_page_prot));

#else


        //vma->vm_flags |= VM_RESERVED;  //set the attribute maybe need it ,no need ,the function add it default

       #if 1
	rv = remap_pfn_range(vma, vma->vm_start,
						 physical >> PAGE_SHIFT, len,
						 (vma->vm_page_prot));
        #else

        simple_nopage_mmap(vma, vma->vm_start,
						 physical >> PAGE_SHIFT, len,
						 (vma->vm_page_prot));
        #endif
#endif



	if (rv < 0) {
 		DBGMSG1( "Returning %d", rv );
		return rv;
	}

	vma->vm_ops = &cgx_remap_ops;
	CGX_vma_open(vma);
/* 	DBGMSG( "Returning 0" ); */
	return 0;
}


#if 0
#warning CGX_AddSysDev is needed only for debugging !
static int CGX_AddSysDev(struct sys_device *dev)
{
	DBGMSG( "Got to CGX_AddSysDev\n" );
	return 0;
}
#endif


static int CGX_PMSuspend(struct device *dev, pm_message_t state)
{
 #if 0
	//DBGMSG( "Got to CGX_PMSuspend\n" );
	printk("%s\n",__func__);
	//if (DrvContext.nNumOpens > 0)
	if(flag_power_up == 1)
	{
		CgxDriverPowerDown();
		CgxDriverRFPowerDown();
	}
 #endif 
	return 0;
}


static int CGX_PMResume(struct device *dev)
{
 #if 0
	//DBGMSG( "Got to CGX_PMResume\n" );
	printk("%s\n",__func__);
	//if (DrvContext.nNumOpens > 0)
	if(flag_power_up == 1)
	{

		DrvContext.state.flags.resume = 1;
	#if 0
		DrvContext.pm_resumed = 'R';

		/* awake read()ers and poll()ers (select) */
		wake_up_interruptible(&DrvContext.resumeq);

		/* signal async readers */
		if (DrvContext.async_queue)
			kill_fasync(&DrvContext.async_queue, SIGIO, POLL_IN);
	#endif
		CgxDriverPowerUp();
		CgxDriverRFPowerUp();
	}
#endif 
	return 0;
}



//======================================================================
// CGX_Init - Driver initialization function
//
struct file_operations cgx_fops = {
		.owner =	 THIS_MODULE,
		.llseek =	CGX_Seek,
		.read =	  CGX_Read,
		.write =	 CGX_Write,
		.poll =	  CGX_Poll,
		//bxd
		//.ioctl =	 CGX_IOControl,
		.unlocked_ioctl  =	 CGX_IOControl,
		.open =	  CGX_Open,
		.fasync =   CGX_Fasync,
		.release =   CGX_Close,
		.mmap =	  CGX_Mmap,
};

#ifdef CONFIG_OF
struct gps_2351_addr gps_2351;
u32 gps_get_core_base(void)
{
	return gps_2351.gps_base;
}

u32 gps_get_ahb_base(void)
{
	return gps_2351.ahb_base;
}

u32 gps_get_irq(void)
{
	return gps_2351.irq_num;
}

u32 gps_get_lna_gpio(void)
{
	return gps_2351.lna_gpio;
}

#ifdef CONFIG_ARCH_SCX30G
u32 gps_get_pmu_base(void)
{
	return gps_2351.pmu_base;
}
#endif

static int cgxdrv_gps_source_init(void)
{
	int ret;

	struct device_node *np;
	struct resource res;

	np = of_find_node_by_name(NULL, "gps_2351");
	if (!np) {
		printk("Can't get the gps_2351 node!\n");
		return -ENODEV;
	}
	printk(" find the gps_2351 node!\n");

	ret = of_address_to_resource(np, 0, &res);
	if (ret < 0) {
		printk("Can't get the gps reg base!\n");
		return -EIO;
	}
	gps_2351.gps_base = res.start;
	printk("gps reg base is 0x%x\n", gps_2351.gps_base);

	ret = of_address_to_resource(np, 1, &res);
	if (ret < 0) {
		printk("Can't get the ahbreg base!\n");
		return -EIO;
	}
	gps_2351.ahb_base = res.start;
	printk("ahb reg base is 0x%x\n", gps_2351.ahb_base);

	#ifdef CONFIG_ARCH_SCX30G
	ret = of_address_to_resource(np, 2, &res);
	if (ret < 0) {
		printk("Can't get the pmu base!\n");
		return -EIO;
	}
	gps_2351.pmu_base = res.start;
	printk("pmu_base base is 0x%x\n", gps_2351.pmu_base);
	#endif

	gps_2351.irq_num = irq_of_parse_and_map(np, 0);
	if (gps_2351.irq_num == 0) {
		printk("Can't get the gps irq_num!\n");
		return -EIO;
	}
	printk(" gps_2351.gps_irq_num is %d\n", gps_2351.irq_num);

	gps_2351.lna_gpio = of_get_gpio(np, 0);
	if(gps_2351.lna_gpio < 0){
		printk("fail to get gps lna gpio\n");
	}
	return ret;
}

#endif
static int __init  CGX_Init(void)
{
	DBG_FUNC_NAME("CGX_Init")
	int		 rv;
	dev_t	   devno = MKDEV(cgxdrv_major, 0);

/* 	DBGMSG( "Entering..." ); */

	DBGMSG("Initializing");

	#ifdef CONFIG_OF
	cgxdrv_gps_source_init();
	#endif

	//bxd add for first alloc mem failed from engine
	init_buff_ptr = (void *)__get_free_pages(GFP_KERNEL, 8);
	if(init_buff_ptr == NULL)
		printk("init_buff_ptr alloc failed\n");

	sprd_get_rf2351_ops(&gps_rf_ops);

#ifdef CGCORE_ACCESS_VIA_SPI

        printk(" addr = %x\n",buf_data);
        gpsspidev_init();
#endif

	/* Init structure */
	memset (&(DrvContext), 0, sizeof (DRVCONTEXT));
	init_waitqueue_head(&DrvContext.resumeq);

	/* Register char dev (Major can be provided at load time) */
	if (cgxdrv_major) {
	  rv = register_chrdev_region(devno, 1, "cgxdrv");
      printk(KERN_ERR "[CGX|%s] register_chrdev_region \r\n", __FUNCTION__);
	} else {
	  rv = alloc_chrdev_region(&devno, 0, 1, "cgxdrv");
	  cgxdrv_major = MAJOR(devno);
      printk(KERN_ERR "[CGX|%s] alloc_chrdev_region, cgxdrv_major=%d \r\n", __FUNCTION__, cgxdrv_major);
	}

	if (rv < 0) {
	  printk(KERN_ERR MODULE_NAME " Error can't get major %d\r\n", cgxdrv_major);
	  goto out;
	}

	if ( CgxDriverConstruct(&DrvContext, &DrvContext.state) != ECgOk ) {
	  DBGMSG("CgxDriverConstruct Failed");
	  rv = -ENOMEM;
	  goto out1;
	}

	// Device is ready... Activate it
	cdev_init(&DrvContext.cdev, &cgx_fops);
	DrvContext.cdev.owner = THIS_MODULE;
	DrvContext.cdev.ops   = &cgx_fops;
	rv = cdev_add (&DrvContext.cdev, devno, 1);
	if (rv) {
	  // Cleanup
	  DBGMSG1(" Error %d adding cgxdrv device\r\n", rv);
	  goto out2;
	}

    /* Create the device class. */
    gps_class = class_create(THIS_MODULE, "cgxdrv_class");
    if (IS_ERR(gps_class))
    {
        DBGMSG(" Error: create cgxdrv class failed\r\n");
    }

	//bxd
	gps_class->suspend = CGX_PMSuspend;
	gps_class->resume = CGX_PMResume;

    /* register in /dev */
    gps_dev = device_create(gps_class, NULL, devno, NULL, "cgxdrv");
    if (IS_ERR(gps_dev))
    {
        DBGMSG(" Error: device_create failed\r\n");
        class_destroy(gps_class);
    }

	/*
	 * OK. rv == 0
	 */
/*module insmod */
//	gpsspidev_init();



	DBGMSG2(" Remapped GIB Physmem 0x%08x to VA: 0x%08lx", CG_DRIVER_CGCORE_BASE_PA, CG_DRIVER_CGCORE_BASE_VA );
	DBGMSG1(" Driver Ready... Major DevId: %d", cgxdrv_major );
    DBGMSG1(" Driver Byte order %d", CGX_DRIVER_NATIVE_BYTE_ORDER);

	goto out;

out2:
	/*
	 * XXX-PK-XXX: FIXME: Add CgxDriverConstruct Cleanup
	 * For example: free IRQs, destroy workqueues, etc.
	 */
out1:
	unregister_chrdev_region( devno, 1 );
out:
	return rv;
}


//======================================================================
// CGX_Deinit - Driver de-initialization function
//
static void	 __exit  CGX_Deinit(void)
{
    printk(KERN_ERR "[CGX|%s] in\r\n", __FUNCTION__);

	// Free the driver state buffer.
	if (DrvContext.GpsIntr_irq != 0)
    {
	    free_irq(DrvContext.GpsIntr_irq, NULL);
        printk(KERN_ERR "free_irq GpsIntr_irq");
    }

    if (DrvContext.DataReady_irq != 0)
    {
	    free_irq(DrvContext.DataReady_irq, NULL);
        printk(KERN_ERR "free_irq DataReady_irq");
    }

	if (girq_wq) {
		flush_workqueue(girq_wq);
		destroy_workqueue(girq_wq);
        printk(KERN_ERR "destroy_workqueue girq_wq");
	}
	if (drdy_wq) {
		flush_workqueue(drdy_wq);
		destroy_workqueue(drdy_wq);
        printk(KERN_ERR "destroy_workqueue drdy_wq");
	}

	#ifdef CGX_DRIVE_DMA_INT_GLOBAL_SYNC_OBJECT
		if (drdy_completion_wq) {
            printk(KERN_ERR "CGX_Deinit (1)");
			//flush_workqueue(drdy_completion_wq);  //commented for debug, because this function blocks !
			//destroy_workqueue(drdy_completion_wq);
            printk(KERN_ERR "CGX_Deinit (2)");
        }
	#endif
#if 0
	DrvContext.state.request.terminate = TRUE;
	if(DrvContext.chunk.ptr) {
		BOOL boo = FreePhysMem(DrvContext.chunk.ptr);
	}
#endif
    printk(KERN_ERR "CGX_Deinit (3)");

    //free_irq(TEMPLATE_CPU_DMA_IRQ, NULL);

	sprd_put_rf2351_ops(&gps_rf_ops);
	CgxDriverFreeInternalBuff(&DrvContext, 0);

	cdev_del(&DrvContext.cdev);

    device_destroy(gps_class, MKDEV(cgxdrv_major, 0));
    class_destroy(gps_class);

	unregister_chrdev_region(MKDEV(cgxdrv_major, 0), 1);

    printk(KERN_ERR "CGX_Deinit out");
}


module_init(CGX_Init);
module_exit(CGX_Deinit);


MODULE_LICENSE("GPL and additional rights");
MODULE_AUTHOR("Pablo 'merKur' Kohan <pablo@ximpo.com> - for CellGuide Ltd.");
MODULE_DESCRIPTION("CellGuide GPSense CGX3XXX/CGX5XXX GPS CoProcessor driver");

/** \endcond */
