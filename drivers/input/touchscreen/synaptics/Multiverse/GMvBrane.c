///////////////////////////////////////////////////////////////////////////////////
// GMvBrane.c
//---------------------------------------------------------------------------------
// Created by Byeongjae Gim
// Email: gaiama78@gmail.com, byeongjae.kim@samsung.com
///////////////////////////////////////////////////////////////////////////////////

// ! include
#include "GMvSystem.h"

// ! struct
#if DG_MV_BRANE_BUFFER_MMAP_ENABLE
struct bin_buffer // from fs/sysfs/bin.c
{
	struct mutex			mutex;
	void				*buffer;
	int				mmapped;
	const struct vm_operations_struct *vm_ops;
	struct file			*file;
	struct hlist_node		list;
};
#endif

struct CGMvBrane
{
	struct mutex m_sPublicMutex;

	sint32 m_s32Width, m_s32Height;

	// Brane Thread
	struct mutex m_sBtMutex;
	struct task_struct *m_psBtTask;
	sint32 m_s32BtRun;
	struct semaphore m_sBtSema;
	sint32 m_s32BtSemaLock;

	// Brane Buffer
#if DG_MV_BRANE_BUFFER_MMAP_ENABLE
	struct mutex m_sBbMutex;
#if DG_MV_BRANE_SIGACTION_ENABLE
	struct siginfo m_sBbSi;
#endif
	sint32 m_s32BbMvPid;
	struct pid *m_psBbPid;
	struct task_struct *m_psBbTask;
	struct bin_buffer *m_psBbBb;
	struct SGMvBraneBufInfo *m_psBbi;
	uint8 *m_pu8Bb;
#if DG_MV_BRANE_BUFFER_MMAP_INTERCEPT
	sint32 m_s32BbInterceptedSize;
	uint8 *m_pu8BbIntercepted;
#endif
#else
	uint8 m_pu8Bb[DG_KERNEL_PAGE_SIZE];
#endif

	// Multiverse Packet
	EG_MV_PKT_ARG1_ACK m_ePktAck;
	uint8 m_pu8PktRxBuf[DG_MV_PKT_MAX_SIZE];
};

// ! extern
extern struct class *sec_class;

// ! Prototype
#if DG_MV_BRANE_BUFFER_MMAP_ENABLE
static ssize_t SGMvBraneWrite( struct file*, struct kobject*, struct bin_attribute*, char*, loff_t, size_t );
static ssize_t SGMvBraneMmap( struct file*, struct kobject*, struct bin_attribute*, struct vm_area_struct* );
static void SGMvBraneMmapOpen( struct vm_area_struct* );
#else
static ssize_t SGMvBraneRead( struct device*, struct device_attribute*, char* );
static ssize_t SGMvBraneWrite( struct device*, struct device_attribute*, const char*, size_t );
#endif
static sint32 SGMvBraneSendPacket( uint8*, sint32, sint32 );
static void SGMvBraneSendResetPacket( void );

// ! Global Variable
static struct device *sg_psMvDev = NULL;
#if DG_MV_BRANE_BUFFER_MMAP_ENABLE
static struct bin_attribute dev_attr_Brane =
{
	.attr =
	{
		.name = "Brane",
		.mode = 00600,
	},
	.size = PAGE_SIZE,
	.write = SGMvBraneWrite,
	.mmap = SGMvBraneMmap,
};
static struct vm_operations_struct sg_sVmOps =
{
	.open = SGMvBraneMmapOpen,
};
#else
static DEVICE_ATTR( Brane, 00600, SGMvBraneRead, SGMvBraneWrite );
#endif

static struct CGMvBrane sg_cMb;

// ! System Call
#if !DG_MV_BRANE_BUFFER_MMAP_ENABLE
static ssize_t SGMvBraneRead( struct device *psDev, struct device_attribute *psDevAttr, char *ps8Buf )
{
	sint32 s32Read;
mutex_lock( &sg_cMb.m_sPublicMutex ); // goto LG_UNLOCK_AND_RETURN;

	if( sg_cMb.m_s32BbPosR )
	{
		memcpy( ps8Buf, sg_cMb.m_pu8Bb, sg_cMb.m_s32BbPosR );
		s32Read = sg_cMb.m_s32BbPosR;
		sg_cMb.m_s32BbPosR = 0;
	}
	else
		s32Read = 0;

LG_UNLOCK_AND_RETURN:
mutex_unlock( &sg_cMb.m_sPublicMutex );
	return s32Read;
}
#endif

#if DG_MV_BRANE_BUFFER_MMAP_ENABLE
static ssize_t SGMvBraneWrite( struct file *psFile, struct kobject *psKObj, struct bin_attribute *psBinAttr, char *ps8Buf, loff_t tOffset, size_t tCount )
#else
static ssize_t SGMvBraneWrite( struct device *psDev, struct device_attribute *psDevAttr, const char *ps8Buf, size_t tCount )
#endif
{
#if DG_MV_BRANE_BUFFER_MMAP_ENABLE
	struct SGMvPktHdr *psPktHdr;
#endif
mutex_lock( &sg_cMb.m_sPublicMutex ); // goto LG_UNLOCK_AND_RETURN;

#if DG_MV_BRANE_BUFFER_MMAP_ENABLE
	psPktHdr = (struct SGMvPktHdr*)ps8Buf;
	switch( psPktHdr->u8PktId )
	{
	case EG_MV_PKT_ID_BRANE:
		{
			switch( psPktHdr->u8Arg1 )
			{
			case EG_MV_PKT_ARG1_BRANE_MMAP:
				SGMvBraneSendResetPacket();

				mutex_lock( &sg_cMb.m_sBbMutex );
				sg_cMb.m_s32BbMvPid = *(sint32*)&ps8Buf[sizeof( struct SGMvPktHdr )];
				rcu_read_lock();
#if 1
				sg_cMb.m_psBbPid = find_get_pid( sg_cMb.m_s32BbMvPid );
				sg_cMb.m_psBbTask = pid_task( sg_cMb.m_psBbPid, PIDTYPE_PID );
#else
				sg_cMb.m_psBbTask = find_task_by_vpid( sg_cMb.m_s32BbMvPid );
#endif
				rcu_read_unlock();
#if DG_MV_BRANE_BUFFER_MMAP_INTERCEPT
				if( sg_cMb.m_pu8BbIntercepted )
					kfree( sg_cMb.m_pu8BbIntercepted );
				sg_cMb.m_s32BbInterceptedSize = *(sint32*)&ps8Buf[sizeof( struct SGMvPktHdr ) + sizeof( sint32 )];
				sg_cMb.m_pu8BbIntercepted = kmalloc( sg_cMb.m_s32BbInterceptedSize, GFP_KERNEL );
				DG_DBG_PRINT_INFOX( _T("INTERCEPT - %ld, 0x%08X"), sg_cMb.m_s32BbInterceptedSize, (unsigned int)sg_cMb.m_pu8BbIntercepted );
#endif
				mutex_unlock( &sg_cMb.m_sBbMutex );
				break;

			case EG_MV_PKT_ARG1_BRANE_UNMMAP:
				SGMvBraneSendResetPacket();

				mutex_lock( &sg_cMb.m_sBbMutex );
				sg_cMb.m_s32BbMvPid = DG_NONE;
				sg_cMb.m_psBbPid = NULL;
				sg_cMb.m_psBbTask = NULL;
				sg_cMb.m_psBbBb = NULL;
				sg_cMb.m_psBbi = NULL;
				sg_cMb.m_pu8Bb = NULL;
				mutex_unlock( &sg_cMb.m_sBbMutex );
				break;

			default:
				break;
			}
		}
		break;
	default:
		tCount = SGMvBraneSendPacket( (uint8*)ps8Buf, tCount, 0 );
		break;
	}
#else
	tCount = SGMvBraneSendPacket( (uint8*)ps8Buf, tCount, 0 );
#endif

//LG_UNLOCK_AND_RETURN:
mutex_unlock( &sg_cMb.m_sPublicMutex );
	return tCount;
}

#if DG_MV_BRANE_BUFFER_MMAP_ENABLE
static int SGMvBraneMmap( struct file *psFile, struct kobject *psKObj, struct bin_attribute *psBinAttr, struct vm_area_struct *psVma )
{
	int s32Rst;
	uint32 u32PageOffset;
	struct bin_buffer *psBb;
DG_DBG_PRINT_INFO( "0" );
mutex_lock( &sg_cMb.m_sPublicMutex ); // goto LG_UNLOCK_AND_RETURN;
mutex_lock( &sg_cMb.m_sBbMutex ); // goto LG_UNLOCK_AND_RETURN;
//down_read( &sg_cMb.m_psBbTask->mm->mmap_sem ); // Make Kernel Panic!
//down_wrtie( &sg_cMb.m_psBbTask->mm->mmap_sem ); // Make Kernel Panic!

	s32Rst = 0;

#if DG_MV_BRANE_BUFFER_MMAP_INTERCEPT
	DG_SAFE_IS_ZERO( sg_cMb.m_pu8BbIntercepted, DG_DBG_PRINT_ERROR( _T("m_pu8BbIntercepted == NULL") ); s32Rst = -EAGAIN; goto LG_UNLOCK_AND_RETURN );

	psBb = (struct bin_buffer*)psFile->private_data;
	u32PageOffset = page_to_pfn( virt_to_page( sg_cMb.m_pu8BbIntercepted ) );
	psVma->vm_ops = &sg_sVmOps;
	psVma->vm_flags |= VM_RESERVED;
	psVma->vm_pgoff = u32PageOffset;
	psVma->vm_private_data = psFile->private_data;
	if( remap_pfn_range( psVma, psVma->vm_start,  psVma->vm_pgoff, psVma->vm_end - psVma->vm_start, psVma->vm_page_prot ) )
	{
		DG_DBG_PRINT_ERROR( _T("Failed remap_pfn_range()") );
		s32Rst = -EAGAIN;
		goto LG_UNLOCK_AND_RETURN;
	}

	sg_cMb.m_psBbBb = psBb;
	sg_cMb.m_pu8Bb = sg_cMb.m_pu8BbIntercepted;
	sg_cMb.m_psBbi = (struct SGMvBraneBufInfo*)&sg_cMb.m_pu8Bb[sg_cMb.m_s32BbInterceptedSize - sizeof( struct SGMvBraneBufInfo )];
	memset( sg_cMb.m_psBbi, 0, sizeof( struct SGMvBraneBufInfo ) );
	sg_cMb.m_psBbi->u16TotalSize = sg_cMb.m_s32BbInterceptedSize;
#else
	psBb = (struct bin_buffer*)psFile->private_data;
	u32PageOffset = page_to_pfn( virt_to_page( psBb->buffer ) );
	psVma->vm_ops = &sg_sVmOps;
	psVma->vm_flags |= VM_RESERVED;
	psVma->vm_pgoff = u32PageOffset;
	psVma->vm_private_data = psFile->private_data;
	if( remap_pfn_range( psVma, psVma->vm_start,  psVma->vm_pgoff, psVma->vm_end - psVma->vm_start, psVma->vm_page_prot ) )
	{
		DG_DBG_PRINT_ERROR( _T("Failed remap_pfn_range()") );
		s32Rst = -EAGAIN;
		goto LG_UNLOCK_AND_RETURN;
	}

	sg_cMb.m_psBbBb = psBb;
	sg_cMb.m_pu8Bb = (uint8*)psBb->buffer;
	sg_cMb.m_psBbi = (struct SGMvBraneBufInfo*)&sg_cMb.m_pu8Bb[DG_KERNEL_PAGE_SIZE - sizeof( struct SGMvBraneBufInfo )];
	memset( sg_cMb.m_psBbi, 0, sizeof( struct SGMvBraneBufInfo ) );
	sg_cMb.m_psBbi->u16TotalSize = DG_KERNEL_PAGE_SIZE;
#endif

LG_UNLOCK_AND_RETURN:
//up_wrtie( &sg_cMb.m_psBbTask->mm->mmap_sem ); // Make Kernel Panic!
//up_read( &sg_cMb.m_psBbTask->mm->mmap_sem ); // Make Kernel Panic!
mutex_unlock( &sg_cMb.m_sBbMutex );
mutex_unlock( &sg_cMb.m_sPublicMutex );
DG_DBG_PRINT_INFO( "1" );
	return s32Rst;
}

static void SGMvBraneMmapOpen( struct vm_area_struct *psVma )
{
mutex_lock( &sg_cMb.m_sPublicMutex ); // goto LG_UNLOCK_AND_RETURN;

//LG_UNLOCK_AND_RETURN:
mutex_unlock( &sg_cMb.m_sPublicMutex );
}
#endif

// Multiverse Packet
static void SGMvBraneCheckAck( struct SGMvPktHdr *psPktHdr )
{
	sg_cMb.m_ePktAck = (EG_MV_PKT_ARG1_ACK)psPktHdr->u8Arg1;
}

static void SGMvBraneMuxData( struct SGMvPktHdr *psPktHdr )
{
	uint8 *pu8Cur;
	uint16 u16PktSize;
	struct SGMvBraneBufInfo *psBbi;
	sint32 s32HdrSize, s32PldSize, s32BufSize, s32WriteSize, s32WritableSizeLinear, s32WritableSize;

mutex_lock( &sg_cMb.m_sBbMutex ); // goto LG_ERROR, LG_UNLOCK_AND_RETURN;
	DG_SAFE_GET_POINTER( psBbi, sg_cMb.m_psBbi, goto LG_ERROR );
	DG_SAFE_IS_ZERO( pid_task( sg_cMb.m_psBbPid, PIDTYPE_PID ), goto LG_ERROR );
#if !DG_MV_BRANE_BUFFER_MMAP_INTERCEPT
// !. Let LMK wait this operation
//down_write( &sg_cMb.m_psBbTask->mm->mmap_sem ); // Make Kernel Panic
mutex_lock( &sg_cMb.m_psBbBb->file->f_path.dentry->d_inode->i_mapping->i_mmap_mutex ); // Lock 'unmap_mapping_range()' in 'unmap_bin_file()' in fs/sysfs/bin.c
mutex_lock( &sg_cMb.m_psBbBb->mutex ); // goto LG_UNLOCK_AND_RETURN;
#endif

	//
	s32HdrSize = sizeof( struct SGMvPktHdr ) + DG_MV_PKT_PARITY_SIZE_HEADER;
#if defined( DG_MV_PKT_PARITY_RS_10_6 )
	s32PldSize = psPktHdr->u16PldSize + GMvRsCalculateOverheadSize( psPktHdr->u16PldSize );
#else
	s32PldSize = psPktHdr->u16PldSize + DG_MV_PKT_PARITY_SIZE_HEADER;
#endif
	DG_SAFE_IS_GREATER( s32PldSize, sizeof( sg_cMb.m_pu8PktRxBuf ) - s32HdrSize, DG_DBG_PRINT_ERRORX( _T("s32PldSize(=%ld) is too big!"), s32PldSize ); goto LG_UNLOCK_AND_RETURN );
	GMvGtReadI2cRegister( DG_MV_I2C_REG_PKT_PAYLOAD_RX, (uint8*)&sg_cMb.m_pu8PktRxBuf[s32HdrSize], s32PldSize );

	//
	u16PktSize = s32HdrSize + s32PldSize;
	s32BufSize = psBbi->u16TotalSize - sizeof( struct SGMvBraneBufInfo );
	s32WriteSize = 2 + u16PktSize;
	if( psBbi->u16WPos < psBbi->u16RPos )
	{
		s32WritableSizeLinear = psBbi->u16RPos - psBbi->u16WPos;
		s32WritableSize = s32WritableSizeLinear;
	}
	else
	{
		s32WritableSizeLinear = s32BufSize - psBbi->u16WPos;
		s32WritableSize = s32WritableSizeLinear + psBbi->u16RPos;
	}
	DG_SAFE_IS_GREATER( s32WriteSize, s32WritableSize, DG_DBG_PRINT_ERRORX( _T("Overflow or Wrong I2C? - %ld, %ld"), s32WriteSize, s32WritableSize ); goto LG_UNLOCK_AND_RETURN ); // let multiverse be faster?

	//
	pu8Cur = &sg_cMb.m_pu8Bb[psBbi->u16WPos];
	if( s32WriteSize > s32WritableSizeLinear )
	{
		switch( s32WritableSizeLinear )
		{
		case 0: // psBbi->u16WPos == s32BufSize
			pu8Cur = sg_cMb.m_pu8Bb;
			*(uint16*)pu8Cur = u16PktSize;
			pu8Cur += 2;
			memcpy( pu8Cur, sg_cMb.m_pu8PktRxBuf, u16PktSize );
			psBbi->u16WPos = s32WriteSize;
			break;
		case 1:
			*pu8Cur = (u16PktSize >> 8) & 0xFF; // MSB
			pu8Cur = sg_cMb.m_pu8Bb;
			*pu8Cur++ = u16PktSize & 0xFF; // LSB
			memcpy( pu8Cur, sg_cMb.m_pu8PktRxBuf, u16PktSize );
			psBbi->u16WPos = 1 + u16PktSize;
			break;
		case 2:
			*(uint16*)pu8Cur = u16PktSize;
			pu8Cur = sg_cMb.m_pu8Bb;
			memcpy( pu8Cur, sg_cMb.m_pu8PktRxBuf, u16PktSize );
			psBbi->u16WPos = u16PktSize;
			break;
		default:
			*(uint16*)pu8Cur = u16PktSize;
			pu8Cur += 2;
			s32WritableSizeLinear -= 2;
			memcpy( pu8Cur, sg_cMb.m_pu8PktRxBuf, s32WritableSizeLinear );
			pu8Cur = sg_cMb.m_pu8Bb;
			memcpy( pu8Cur, &sg_cMb.m_pu8PktRxBuf[s32WritableSizeLinear], u16PktSize - s32WritableSizeLinear );
			psBbi->u16WPos = u16PktSize - s32WritableSizeLinear;
			break;
		}
	}
	else
	{
		*(uint16*)pu8Cur = u16PktSize;
		pu8Cur += 2;
		memcpy( pu8Cur, sg_cMb.m_pu8PktRxBuf, u16PktSize );
		psBbi->u16WPos += s32WriteSize;
	}

	//
	psBbi->u16RSize += s32WriteSize;

LG_UNLOCK_AND_RETURN:
#if !DG_MV_BRANE_BUFFER_MMAP_INTERCEPT
mutex_unlock( &sg_cMb.m_psBbBb->mutex );
mutex_unlock( &sg_cMb.m_psBbBb->file->f_path.dentry->d_inode->i_mapping->i_mmap_mutex );
//up_write( &sg_cMb.m_psBbTask->mm->mmap_sem ); // Make Kernel Panic
#endif
mutex_unlock( &sg_cMb.m_sBbMutex );
	return;

LG_ERROR:
	SGMvBraneSendResetPacket();
	sg_cMb.m_s32BbMvPid = DG_NONE;
	sg_cMb.m_psBbPid = NULL;
	sg_cMb.m_psBbTask = NULL;
	sg_cMb.m_psBbBb = NULL;
	sg_cMb.m_psBbi = NULL;
	sg_cMb.m_pu8Bb = NULL;
	DG_DBG_PRINT_ERROR( _T("close or unmmap, becase of the Multiverse was dead by LMK?!") );
mutex_unlock( &sg_cMb.m_sBbMutex );
}

static sint32 SGMvBraneSendPacket( uint8 *pu8Pkt, sint32 s32PktSize, sint32 s32NoneBlocking )
{
	sint32 i, s32HdrSize, s32RetryCount;

	s32HdrSize = sizeof( struct SGMvPktHdr ) + DG_MV_PKT_PARITY_SIZE_HEADER;
	DG_SAFE_IS_LESS( s32PktSize, s32HdrSize, DG_DBG_PRINT_ERROR( _T("s32PktSize < s32HdrSize") ); return -1 );

	s32RetryCount = DG_MV_I2C_ACK_RETRY_COUNT;

LG_RETRY:
	DG_SAFE_IS_LESS_OR_SAME( s32RetryCount, 0, DG_DBG_PRINT_ERROR( _T("[!!!] Reset TSP IC! [!!!]") ); return -1 );
	s32RetryCount--;

	if( s32PktSize > s32HdrSize )
		GMvGtWriteI2cRegister( DG_MV_I2C_REG_PKT_PAYLOAD_TX, &pu8Pkt[s32HdrSize], s32PktSize - s32HdrSize );
	GMvGtWriteI2cRegister( DG_MV_I2C_REG_PKT_HEADER_TX, pu8Pkt, s32HdrSize );

	sg_cMb.m_ePktAck = EG_MV_PKT_ARG1_ACK_WAIT;
	DG_SAFE_IS_NOT_ZERO( s32NoneBlocking, return s32PktSize );

	// Wait for ACK or NACK
	for( i=0; i<DG_MV_I2C_ACK_WAIT_COUNT; i++ )
	{
        usleep_range(DG_MV_I2C_ACK_WAIT_US, DG_MV_I2C_ACK_WAIT_US);
		if( sg_cMb.m_ePktAck == EG_MV_PKT_ARG1_ACK_ACK )
			return s32PktSize;
		else if( sg_cMb.m_ePktAck == EG_MV_PKT_ARG1_ACK_NACK )
		{
			DG_DBG_PRINT_ERROR( _T("NACK!") );
			goto LG_RETRY;
		}
	}
	goto LG_RETRY;

	return -1;
}

static void SGMvBraneSendResetPacket( void )
{
	uint8 pu8PktBuf[sizeof( struct SGMvPktHdr ) + DG_MV_PKT_PARITY_SIZE_HEADER];

	SGMvBraneSendPacket( pu8PktBuf, GMvPacketMakeHeader( pu8PktBuf, EG_MV_PKT_ID_ACK, EG_MV_PKT_ARG1_ACK_RESET, 0, 0 ), 0 );
}

// ISR of TSP IC
void GMvBraneIsr( void )
{
	if( !sg_cMb.m_s32BtSemaLock )
	{
		sg_cMb.m_s32BtSemaLock = 1;
		up( &sg_cMb.m_sBtSema );
	}
}

// ! Worker Thread
static int SGMvBraneThreadProcedure( void *pvArg )
{
	sint32 s32HdrSize;
	struct SGMvPktHdr *psPktHdr;
DG_DBG_PRINT_INFO( "0" );

	s32HdrSize = sizeof( struct SGMvPktHdr ) + DG_MV_PKT_PARITY_SIZE_HEADER;

	while( sg_cMb.m_s32BtRun && !kthread_should_stop() )
	{
		sg_cMb.m_s32BtSemaLock = 0;
		if( -EINTR == down_interruptible( &sg_cMb.m_sBtSema ) )
			continue;
		DG_SAFE_IS_ZERO( sg_cMb.m_s32BtRun, break );

		//
		GMvGtReadI2cRegister( DG_MV_I2C_REG_PKT_HEADER_RX, sg_cMb.m_pu8PktRxBuf, s32HdrSize );
		DG_SAFE_GET_POINTER( psPktHdr, GMvPacketCheckHeader( sg_cMb.m_pu8PktRxBuf, s32HdrSize ), continue );

		//
		switch( psPktHdr->u8PktId )
		{
		case EG_MV_PKT_ID_ACK:
			SGMvBraneCheckAck( psPktHdr );
			break;
		default: // Data
			SGMvBraneMuxData( psPktHdr );

#if DG_MV_BRANE_SIGACTION_ENABLE
			mutex_lock( &sg_cMb.m_sBbMutex );
			if( sg_cMb.m_psBbTask )
			{
				//sg_cMb.m_sBbSi.si_int = 0;
				if( send_sig_info( DG_MV_BRANE_SIGACTION_ID, &sg_cMb.m_sBbSi, sg_cMb.m_psBbTask ) < 0 )
				{
					sg_cMb.m_s32BbMvPid = DG_NONE;
					sg_cMb.m_psBbPid = NULL;
					sg_cMb.m_psBbTask = NULL;
					sg_cMb.m_psBbBb = NULL;
					sg_cMb.m_psBbi = NULL;
					sg_cMb.m_pu8Bb = NULL;
					DG_DBG_PRINT_ERROR( _T("send_sig_info() failed!") );
				}
			}
			mutex_unlock( &sg_cMb.m_sBbMutex );
#endif
			break;
		}
	}
	sg_cMb.m_s32BtRun = 0;

DG_DBG_PRINT_INFO( "1" );
	return 0;
}

static EG_RESULT SGMvBraneThreadStart( void )
{
DG_DBG_PRINT_INFOX( "0 - %d", (int)sg_cMb.m_s32BtRun );
mutex_lock( &sg_cMb.m_sBtMutex ); // goto LG_SUCCESS;

	DG_SAFE_IS_NOT_ZERO( sg_cMb.m_s32BtRun, goto LG_SUCCESS );
#if 0 // kthread_stop() makes Kernel Panic when thread was already finished.
	DG_SAFE_IS_NOT_ZERO( sg_cMb.m_psBtTask, kthread_stop( sg_cMb.m_psBtTask ) );
#endif

	sg_cMb.m_s32BtRun = 1;
	sg_cMb.m_psBtTask = kthread_run( SGMvBraneThreadProcedure, NULL, "SGMvBraneThreadProcedure" );
	DG_SAFE_IS_NOT_ZERO( IS_ERR( sg_cMb.m_psBtTask ), DG_DBG_PRINT_ERROR( "Failed to create the thread" ); goto LG_ERROR );

LG_SUCCESS:
mutex_unlock( &sg_cMb.m_sBtMutex );
DG_DBG_PRINT_INFO( "1" );
	return EG_RESULT_SUCCESS;

LG_ERROR:
	sg_cMb.m_psBtTask = NULL;
	sg_cMb.m_s32BtRun = 0;

mutex_unlock( &sg_cMb.m_sBtMutex );
DG_DBG_PRINT_INFO( "2" );
	return EG_RESULT_ERROR;
}

static EG_RESULT SGMvBraneThreadStop( void )
{
DG_DBG_PRINT_INFO( "0" );
mutex_lock( &sg_cMb.m_sBtMutex ); // goto LG_UNLOCK_AND_RETURN;

	if( sg_cMb.m_psBtTask )
	{
		sg_cMb.m_s32BtRun = 0;
		up( &sg_cMb.m_sBtSema );
#if 0 // kthread_stop() makes Kernel Panic when thread was already finished.
		kthread_stop( sg_cMb.m_psBtTask );
#endif
		sg_cMb.m_psBtTask = NULL;
	}

//LG_UNLOCK_AND_RETURN:
DG_DBG_PRINT_INFO( "1" );
mutex_unlock( &sg_cMb.m_sBtMutex );
	return EG_RESULT_SUCCESS;
}

// ! Kernel Module
#if 0
static int SGMvBraneModuleDeviceMatch( struct device *psDev, void *pvData )
{
	//DG_DBG_PRINT_INFOX( "%s", dev_name( psDev ) );
	return !strcmp( dev_name( psDev ), (char*)pvData );
}
#endif

static int SGMvBraneModuleInit( void )
{
#if 0
	char *szParentName = "tsp";

	sg_psMvDev = class_find_device( sec_class, NULL, szParentName, SGMvBraneModuleDeviceMatch );
	if( !sg_psMvDev )
	{
		DG_DBG_PRINT_ERROR( "Failed to find a parent device!" );
		sg_psMvDev = device_create( sec_class, NULL, 0, NULL, szParentName );
		DG_SAFE_IS_ZERO( sg_psMvDev, DG_DBG_PRINT_ERROR( "Failed to create a parent device!" ); return 0 );
	}
#else
	sg_psMvDev = device_create( sec_class, NULL, 0, NULL, "Multiverse" );
	DG_SAFE_IS_ZERO( sg_psMvDev, DG_DBG_PRINT_ERROR( "sg_psMvDev == NULL" ); return 0 );
#endif
#if DG_MV_BRANE_BUFFER_MMAP_ENABLE
	DG_SAFE_IS_NOT_ZERO( device_create_bin_file( sg_psMvDev, &dev_attr_Brane ), DG_DBG_PRINT_ERROR( "Failed to create a node of the Brane" ); return 0 );
#else
	DG_SAFE_IS_NOT_ZERO( device_create_file( sg_psMvDev, &dev_attr_Brane ), DG_DBG_PRINT_ERROR( "Failed to create a node of the Brane" ); return 0 );
#endif
	put_device( sg_psMvDev );

	//system/core/include/private/android_filesystem_config.h
	//sys_chown( DG_MV_BRANE_PATH, AID_SYSTEM, AID_SYSTEM );

	//
DG_DBG_PRINT_INFO( "0" );
	mutex_init( &sg_cMb.m_sPublicMutex );
mutex_lock( &sg_cMb.m_sPublicMutex ); // goto LG_UNLOCK_AND_RETURN;

	sg_cMb.m_s32Width = DG_MV_WIDTH;
	sg_cMb.m_s32Height = DG_MV_HEIGHT;

	mutex_init( &sg_cMb.m_sBtMutex );
	sg_cMb.m_psBtTask = NULL;
	sg_cMb.m_s32BtRun = 0;
	sema_init( &sg_cMb.m_sBtSema, 0 ); // init_MUTEX( &sg_cMb.m_sBtSema );
	sg_cMb.m_s32BtSemaLock = 1;

#if DG_MV_BRANE_BUFFER_MMAP_ENABLE
	mutex_init( &sg_cMb.m_sBbMutex );
#if DG_MV_BRANE_SIGACTION_ENABLE
	memset( &sg_cMb.m_sBbSi, 0, sizeof( sg_cMb.m_sBbSi ) );
	sg_cMb.m_sBbSi.si_signo = DG_MV_BRANE_SIGACTION_ID;
	sg_cMb.m_sBbSi.si_code = SI_QUEUE;
#endif
	sg_cMb.m_s32BbMvPid = DG_NONE;
	sg_cMb.m_psBbPid = NULL;
	sg_cMb.m_psBbTask = NULL;
	sg_cMb.m_psBbBb = NULL;
	sg_cMb.m_psBbi = NULL;
	sg_cMb.m_pu8Bb = NULL;
#if DG_MV_BRANE_BUFFER_MMAP_INTERCEPT
	sg_cMb.m_s32BbInterceptedSize = 0;
	sg_cMb.m_pu8BbIntercepted = NULL;
#endif
#endif

	GMvCrc16Open();
	SGMvBraneThreadStart();

//LG_UNLOCK_AND_RETURN:
DG_DBG_PRINT_INFO( "1" );
mutex_unlock( &sg_cMb.m_sPublicMutex );
	return 0;
}

static void SGMvBraneModuleExit( void )
{
DG_DBG_PRINT_INFO( "0" );
mutex_lock( &sg_cMb.m_sPublicMutex ); // goto LG_UNLOCK_AND_RETURN;

	SGMvBraneThreadStop();
	if( sg_psMvDev )
	{
#if DG_MV_BRANE_BUFFER_MMAP_ENABLE
		device_remove_bin_file( sg_psMvDev, &dev_attr_Brane );
#else
		device_remove_file( sg_psMvDev, &dev_attr_Brane );
#endif
		sg_psMvDev = NULL;
	}
#if DG_MV_BRANE_BUFFER_MMAP_INTERCEPT
	if( sg_cMb.m_pu8BbIntercepted )
	{
		kfree( sg_cMb.m_pu8BbIntercepted );
		sg_cMb.m_pu8BbIntercepted = NULL;
	}
#endif

//LG_UNLOCK_AND_RETURN:
DG_DBG_PRINT_INFO( "1" );
mutex_unlock( &sg_cMb.m_sPublicMutex );
}

//
module_init( SGMvBraneModuleInit );
module_exit( SGMvBraneModuleExit );
MODULE_DESCRIPTION( "Brane" );
MODULE_AUTHOR( "gaiama, byeongjae.kim@samsung.com" );
MODULE_LICENSE( "GPL" );
