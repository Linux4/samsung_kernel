#ifndef GPS_DMA_H
#define GPS_DMA_H

#include <asm/io.h>

#ifdef __cplusplus
    extern "C"
    {
#endif

#define MAX_DMA_CHUNK_SIZE (0x80000)

#define MAX_DMA_TRANSFER_TASKS (64)

#define DMAC_STATUS_SUCCESS (0)
#define DMAC_STATUS_ERROR (-1)

#define SPRD_GPS_DMA_IRQ (44)

#define DMAC_BLOCK_SIZE (0xffff)

#define CH_STAT_MASK (0x0000000f)

#define CHANNEL_ENABLE (1)
#define CHANNEL_DISABLE (0)

#define DMAC_REMAP_NOACP 1

#define REG_PERI_CTRL0 (IO_ADDRESS(0xFCA09000))

#define RCHAIN_EN (0x02)
#define RCHAIN_EN_MASK (0xfffffffc)

//#define NUMBER_1K (1024)

#define NUMBER_16K (16*1024)

#define NUMBER_64K (64*1024)

#define REG_DMAC_BASE CG_DRIVER_DMA_BASE_PA


#define REG_DMAC_CX_CURR_CNT0  (IO_ADDRESS(REG_DMAC_BASE) + 0x0704)

typedef struct DMA_CFG_S
{
	unsigned int  src_addr;
	unsigned int  dma_phys;
	unsigned int  dma_size;
	void __iomem  *dma_virt;

	unsigned int  lli_count;
	unsigned int  lli_size;
	unsigned int  lli_addr_pa;
	void          *lli_addr_va;

}GPS_DMAC_CFG;

typedef union{
	unsigned int cx_config_arg;
	struct cx_config_bits
	{
		unsigned int ch_en : 1;
		unsigned int itc_en : 1;
		unsigned int flow_ctrl : 2;
		unsigned int peri : 6;
		unsigned int reserve1 : 2;
		unsigned int dw : 3;
		unsigned int reserve2 : 1;
		unsigned int sw : 3;
		unsigned int reserve3 : 1;
		unsigned int dl : 4;
		unsigned int sl : 4;
		unsigned int dmode : 1;
		unsigned int smode : 1;
		unsigned int di : 1;
		unsigned int si : 1;
	}bits;
}cx_config_reg_s;

int CgCpuDmaLLIDataCopyFromGps(TCgCpuDmaTask *apDmaTask, U32 aDmaTaskCount, U32 aDeviceAddress, U32 aDestAddress);

int CgCpuDmaDataCopyFromGps(TCgCpuDmaTask *apDmaTask, U32 aDmaTaskCount, U32 aDeviceAddress, U32 aDestAddress);

#ifdef __cplusplus
    }
#endif

#endif
