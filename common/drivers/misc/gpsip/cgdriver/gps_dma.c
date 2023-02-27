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

#include <linux/kthread.h>
#include <linux/signal.h>
#include <mach/board.h>
#include <mach/hardware.h>
#include <mach/dma.h>
#include <linux/slab.h>

#include "CgCpu.h"
#include "platform.h"


#define INVALIDE_DMA_CHN (-1)

static struct sci_dma_cfg *cfg_list;
static struct reg_cfg_addr cfg_addr;
static int node_size;
static  int cg_GpsDma_channel = INVALIDE_DMA_CHN;
static char *CgGps_Dev_name = "CgGps_Dev";

extern void clear_buf_data(void);
extern void CCgCpuDmaHandle(int irq,void* dev_id);


TCgReturnCode CgCpuDmaCreate(U32 aDmaChannel, U32 aIpDataSourceAddress)
{
	DBG_FUNC_NAME("CgCpuDmaCreate");
	DBGMSG("entry");
	if(cg_GpsDma_channel == INVALIDE_DMA_CHN)
	{
		cg_GpsDma_channel = sci_dma_request(CgGps_Dev_name, FULL_DMA_CHN);

		if(cg_GpsDma_channel < 0)
		{

			printk("CgCpuDmaCreate fail err code = %d\n",cg_GpsDma_channel);
			return cg_GpsDma_channel;

		}
		printk("CgCpuDmaCreate succ chn = %d\n",cg_GpsDma_channel);
	}

	return ECgOk;
}


TCgReturnCode CgCpuDmaDestroy(U32 aDmaChannel, U32 aIpDataSourceAddress)
{
	int rc;
	printk("%s cg_GpsDma_channel:%d\n",__func__,cg_GpsDma_channel);

	if(cg_GpsDma_channel != INVALIDE_DMA_CHN)
	{
		rc = sci_dma_stop(cg_GpsDma_channel, DMA_GPS);
		if(rc < 0)
		{
			printk("CgCpuDmaDestroy stop chn fail, chn num = %d, err code = %d\n",cg_GpsDma_channel,rc);
		}

		rc = sci_dma_free(cg_GpsDma_channel);
		if(rc < 0)
		{
			printk("CgCpuDmaDestroy free chn fail, chn num = %d, err code = %d\n",cg_GpsDma_channel,rc);
		}

		cg_GpsDma_channel = INVALIDE_DMA_CHN;
	}
	return ECgOk ;
}



TCgReturnCode CgCpuDmaIsReady(U32 aDmaChannel)
{
	TCgReturnCode rc = ECgOk;
	DBG_FUNC_NAME("CgCpuDmaIsReady");
	DBGMSG("entry");

	return rc;
}

TCgReturnCode CgCpuDmaStop(U32 aDmaChannel)
{
	int rc;

	if(cfg_list != NULL)
	{
		kfree(cfg_list);
		cfg_list = NULL;
	}

	if(cfg_addr.virt_addr)
	{
		dma_free_coherent(NULL, sizeof(struct sci_dma_cfg) * node_size, (void *)cfg_addr.virt_addr, cfg_addr.phys_addr);
		cfg_addr.virt_addr = 0;
		cfg_addr.phys_addr = 0;
		node_size = 0;

	}

	rc = sci_dma_stop(cg_GpsDma_channel, DMA_GPS);
	if(rc < 0)
	{
		printk("CgCpuDmaStop stop chn fail, chn num = %d, err code = %d\n",cg_GpsDma_channel,rc);
	}

	return ECgOk;

}

TCgReturnCode CgCpuDmaCurCount(U32 aDmaChannel, U32 *apCount)
{
    TCgReturnCode rc = ECgOk;
	DBG_FUNC_NAME("CgCpuDmaCurCount");
	DBGMSG("entry");

	return rc;
}

// Get the DMA channel requested bytes count.
TCgReturnCode CgCpuDmaRequestedCount(U32 aDmaChannel, U32 *apCount)
{

	TCgReturnCode rc = ECgOk;
	DBG_FUNC_NAME("CgCpuDmaRequestedCount");
	DBGMSG("entry");

	return rc;
}

TCgReturnCode CgCpuDmaStart(U32 aDmaChannel)
{
	int rc;
	printk("%s\n",__func__);
	CgCpuCacheSync();
    rc = sci_dma_start(cg_GpsDma_channel, DMA_GPS);
	if(rc < 0)
	{
		printk("CgCpuDmaStart start chn fail, chn num = %d, err code = %d\n",cg_GpsDma_channel,rc);
	}
	//CgCpuCacheSync();

	return ECgOk;
}

// Configure DMA to read data from GPS device.
TCgReturnCode CgCpuDmaSetupFromGps(TCgCpuDmaTask *apDmaTask, U32 aDmaTaskCount, U32 aDmaChannel, U32 aDeviceAddress)
{
	dma_addr_t cfg_p;
	void * cfg_v;
	int i,ret;

	clear_buf_data();

	if(aDmaTaskCount == 0)
	{
		return ECgBadArgument;
	}

	if(cfg_list != NULL)
	{
		kfree(cfg_list);
		cfg_list = NULL;
	}
	if(cfg_addr.virt_addr != 0)
	{
		dma_free_coherent(NULL, sizeof(struct sci_dma_cfg) * node_size, (void *)cfg_addr.virt_addr, cfg_addr.phys_addr);
		cfg_addr.virt_addr = 0;
		cfg_addr.phys_addr = 0;
		node_size = 0;
	}

	node_size = aDmaTaskCount;
	if(cfg_list == NULL)
	{
		cfg_list = kzalloc(sizeof(struct sci_dma_cfg) * node_size, GFP_KERNEL);
		if (!cfg_list) {
			printk("alloc memory failed!\n");
			return -ENOMEM;
		}
	}

	cfg_v = dma_alloc_coherent(NULL, sizeof(struct sci_dma_cfg) * node_size,
		&cfg_p, GFP_KERNEL);
	if (!cfg_v) {
		printk("alloc cfg list mem failed!\n");
		return -ENOMEM;
	}

	cfg_addr.phys_addr = (u32)cfg_p;
	cfg_addr.virt_addr = (u32)cfg_v;

	/*must init the config with 0x0*/
	memset((void *)cfg_list, 0x0, sizeof(struct sci_dma_cfg) * node_size);

	for (i = 0; i < node_size; i++) {
		/*the data width, byte short or word*/
		cfg_list[i].datawidth = WORD_WIDTH;
		cfg_list[i].src_step = 4;
		cfg_list[i].des_step = 4;
		cfg_list[i].wrap_ptr = 0x21c0007c;
		cfg_list[i].wrap_to  = 0x21c00070;
		cfg_list[i].fragmens_len = 16;


		/* request mode:
		  * when receive an hardware or software dma request
		  * the dma will transfer a fragment, block or a transcation data
		  */
		cfg_list[i].req_mode = FRAG_REQ_MODE;
		cfg_list[i].src_addr = aDeviceAddress;


		cfg_list[i].des_addr = apDmaTask[i].address;
		cfg_list[i].block_len = apDmaTask[i].length >= 65536 ? 65536 :apDmaTask[i].length;
		cfg_list[i].transcation_len = apDmaTask[i].length;

		//printk("\n cfg_list[%d].block_len = %d\n",i,cfg_list[i].block_len);
		//printk("\n cfg_list[%d].transcation_len = %d\n",i,cfg_list[i].transcation_len);
		//printk("\n cfg_list[%d].src_addr = 0x%x\n",i,cfg_list[i].src_addr);
		//printk("\n cfg_list[%d].des_addr = 0x%x\n",i,cfg_list[i].des_addr);
	}

	/*indicate this the last node*/
	cfg_list[node_size - 1].is_end = 1;

	ret = sci_dma_config(cg_GpsDma_channel, cfg_list, node_size, &cfg_addr);
	if (ret < 0) {
		printk("dma config failed!\n");
	}

	sci_dma_register_irqhandle(cg_GpsDma_channel,TRANS_DONE,CCgCpuDmaHandle,NULL);

	return ECgOk;
}

#ifdef TRACE_ON
void DbgStatDMA(const char *aTitle)
{
	//DBG_FUNC_NAME("DbgStatDMA");
	//DBGMSG1("%s", aTitle);


}
#endif


