/****************************************************************************
*
*    Copyright (c) 2005 - 2015 by Vivante Corp.
*    
*    This program is free software; you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation; either version 2 of the license, or
*    (at your option) any later version.
*    
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*    GNU General Public License for more details.
*    
*    You should have received a copy of the GNU General Public License
*    along with this program; if not write to the Free Software
*    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*
*****************************************************************************/


#ifndef __gc_hal_kernel_device_h_
#define __gc_hal_kernel_device_h_

#include "gc_hal_kernel_debugfs.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
#define FREQ_TABLE_MAX 10
struct freq_table {
    unsigned int    index;
    unsigned int    frequency;
    unsigned int    busfreq;
};
#endif

/******************************************************************************\
******************************* gckGALDEVICE Structure *******************************
\******************************************************************************/
struct gpu_pm_context_info {
    struct mutex        lock;
};

typedef struct _gckGALDEVICE
{
    /* Objects. */
    gckOS               os;
    gckKERNEL           kernels[gcdMAX_GPU_COUNT];

    gcsPLATFORM*        platform;

    struct platform_device *pdev[gcdMAX_GPU_COUNT];

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
    struct freq_table   *ft;
#endif
    gctUINT32           min_freq[gcdMAX_GPU_COUNT];
    gctUINT32           max_freq[gcdMAX_GPU_COUNT];

    /* Attributes. */
    gctSIZE_T           internalSize;
    gctPHYS_ADDR        internalPhysical;
    gctUINT32           internalPhysicalName;
    gctPOINTER          internalLogical;
    gckVIDMEM           internalVidMem;
    gctSIZE_T           externalSize;
    gctPHYS_ADDR        externalPhysical;
    gctUINT32           externalPhysicalName;
    gctPOINTER          externalLogical;
    gckVIDMEM           externalVidMem;
    gckVIDMEM           contiguousVidMem;
    gctPOINTER          contiguousBase;
    gctPHYS_ADDR        contiguousPhysical;
    gctUINT32           contiguousPhysicalName;
    gctSIZE_T           contiguousSize;
    gctBOOL             contiguousMapped;
    gctPOINTER          contiguousMappedUser;
    gctBOOL             contiguousRequested;
    gctSIZE_T           systemMemorySize;
    gctUINT32           systemMemoryBaseAddress;
#if gcdMULTI_GPU
    gctPOINTER          registerBase3D[gcdMULTI_GPU];
    gctSIZE_T           registerSize3D[gcdMULTI_GPU];
#endif
    gctPOINTER          registerBases[gcdMAX_GPU_COUNT];
    gctSIZE_T           registerSizes[gcdMAX_GPU_COUNT];
    gctUINT32           baseAddress;
    gctUINT32           physBase;
    gctUINT32           physSize;
    gctBOOL             mmu;
#if gcdMULTI_GPU
    gctUINT32           requestedRegisterMemBase3D[gcdMULTI_GPU];
    gctSIZE_T           requestedRegisterMemSize3D[gcdMULTI_GPU];
#endif
    gctUINT32           requestedRegisterMemBases[gcdMAX_GPU_COUNT];
    gctSIZE_T           requestedRegisterMemSizes[gcdMAX_GPU_COUNT];
    gctUINT32           requestedContiguousBase;
    gctSIZE_T           requestedContiguousSize;

    /* IRQ management. */
#if gcdMULTI_GPU
    gctINT              irqLine3D[gcdMULTI_GPU];
    gctBOOL             isrInitialized3D[gcdMULTI_GPU];
    gctBOOL             dataReady3D[gcdMULTI_GPU];
#endif
    gctINT              irqLines[gcdMAX_GPU_COUNT];
    gctBOOL             isrInitializeds[gcdMAX_GPU_COUNT];

    /* Thread management. */
#if gcdMULTI_GPU
    struct task_struct  *threadCtxt3D[gcdMULTI_GPU];
    wait_queue_head_t   intrWaitQueue3D[gcdMULTI_GPU];
    gctBOOL             threadInitialized3D[gcdMULTI_GPU];
#endif
    struct task_struct  *threadCtxts[gcdMAX_GPU_COUNT];
    struct semaphore    semas[gcdMAX_GPU_COUNT];
    gctBOOL             threadInitializeds[gcdMAX_GPU_COUNT];
    gctBOOL             killThread;

    gctBOOL             powerOffWhenIdle;
    gctBOOL             needPowerOff;
    gctUINT32           profileStep;
    gctUINT32           profileTimeSlice;
    gctUINT32           profileTailTimeSlice;
    gctUINT32           idleThreshold;

    struct gpu_pm_context_info pm_ctx;

    /* debug flags. */
    gctBOOL             powerDebug;
    gctBOOL             pmrtDebug;
    gctBOOL             profilerDebug;
    gctBOOL             printPID;

    /* Signal management. */
    gctINT              signal;

    /* Core mapping */
    gceCORE             coreMapping[8];

    /* GC memory profile*/
    gctSIZE_T           reservedMem;
    gctINT32            vidMemUsage;
    gctINT32            contiguousNonPagedMemUsage;
    gctINT32            contiguousPagedMemUsage;
    gctINT32            virtualPagedMemUsage;
    gctINT32            wastBytes;
    /* GC video memory usage in detail. */
    gctINT32            indexVidMemUsage;
    gctINT32            vertexVidMemUsage;
    gctINT32            textureVidMemUsage;
    gctINT32            renderTargetVidMemUsage;
    gctINT32            depthVidMemUsage;
    gctINT32            bitmapVidMemUsage;
    gctINT32            tileStatusVidMemUsage;
    gctINT32            imageVidMemUsage;
    gctINT32            maskVidMemUsage;
    gctINT32            scissorVidMemUsage;
    gctINT32            hierarchicalDepthVidMemUsage;
    gctINT32            othersVidMemUsage;

    /* States before suspend. */
    gceCHIPPOWERSTATE   statesStored[gcdMAX_GPU_COUNT];

    /* Device Debug File System Entry in kernel. */
    struct _gcsDEBUGFS_Node * dbgNode;

    gcsDEBUGFS_DIR      debugfsDir;
}
* gckGALDEVICE;

typedef struct _gcsHAL_PRIVATE_DATA
{
    gckGALDEVICE        device;
    gctPOINTER          mappedMemory;
    gctPOINTER          contiguousLogical;
    /* The process opening the device may not be the same as the one that closes it. */
    gctUINT32           pidOpen;
#if MRVL_CONFIG_PROC
    struct proc_dir_entry * gc_procmem_file;
#endif
}
gcsHAL_PRIVATE_DATA, * gcsHAL_PRIVATE_DATA_PTR;

typedef struct _gcsDEVICE_CONSTRUCT_ARGS
{
    gctBOOL             recovery;
    gctUINT             stuckDump;
    gctUINT             gpu3DMinClock;

    gctBOOL             contiguousRequested;
    gcsPLATFORM*        platform;
    gctBOOL             mmu;
    gctBOOL             registerMemMapped;
    gctPOINTER             registerMemAddress;
}
gcsDEVICE_CONSTRUCT_ARGS;

gceSTATUS gckGALDEVICE_Setup_ISR(
    IN gckGALDEVICE Device
    );

gceSTATUS gckGALDEVICE_Setup_ISR_2D(
    IN gckGALDEVICE Device
    );

gceSTATUS gckGALDEVICE_Setup_ISR_VG(
    IN gckGALDEVICE Device
    );

gceSTATUS gckGALDEVICE_Release_ISR(
    IN gckGALDEVICE Device
    );

gceSTATUS gckGALDEVICE_Release_ISR_2D(
    IN gckGALDEVICE Device
    );

gceSTATUS gckGALDEVICE_Release_ISR_VG(
    IN gckGALDEVICE Device
    );

gceSTATUS gckGALDEVICE_Start_Threads(
    IN gckGALDEVICE Device
    );

gceSTATUS gckGALDEVICE_Stop_Threads(
    gckGALDEVICE Device
    );

gceSTATUS gckGALDEVICE_Start(
    IN gckGALDEVICE Device
    );

gceSTATUS gckGALDEVICE_Stop(
    gckGALDEVICE Device
    );

gceSTATUS gckGALDEVICE_Construct(
#if gcdMULTI_GPU || gcdMULTI_GPU_AFFINITY
    IN gctINT IrqLine3D0,
    IN gctUINT32 RegisterMemBase3D0,
    IN gctSIZE_T RegisterMemSize3D0,
    IN gctINT IrqLine3D1,
    IN gctUINT32 RegisterMemBase3D1,
    IN gctSIZE_T RegisterMemSize3D1,
#else
    IN gctINT IrqLine,
    IN gctUINT32 RegisterMemBase,
    IN gctSIZE_T RegisterMemSize,
#endif
    IN gctINT IrqLine2D,
    IN gctUINT32 RegisterMemBase2D,
    IN gctSIZE_T RegisterMemSize2D,
    IN gctINT IrqLineVG,
    IN gctUINT32 RegisterMemBaseVG,
    IN gctSIZE_T RegisterMemSizeVG,
    IN gctUINT32 ContiguousBase,
    IN gctSIZE_T ContiguousSize,
    IN gctSIZE_T BankSize,
    IN gctINT FastClear,
    IN gctINT Compression,
    IN gctUINT32 PhysBaseAddr,
    IN gctUINT32 PhysSize,
    IN gctINT Signal,
    IN gctUINT LogFileSize,
    IN gctINT PowerManagement,
    IN gctINT GpuProfiler,
    IN gcsDEVICE_CONSTRUCT_ARGS * Args,
    IN gckGALDEVICE Device
    );

gceSTATUS gckGALDEVICE_Destroy(
    IN gckGALDEVICE Device
    );

static gcmINLINE gckKERNEL
_GetValidKernel(
    gckGALDEVICE Device
    )
{
    if (Device->kernels[gcvCORE_MAJOR])
    {
        return Device->kernels[gcvCORE_MAJOR];
    }
    else
    if (Device->kernels[gcvCORE_2D])
    {
        return Device->kernels[gcvCORE_2D];
    }
    else
    if (Device->kernels[gcvCORE_VG])
    {
        return Device->kernels[gcvCORE_VG];
    }
    else
    {
        gcmkASSERT(gcvFALSE);
        return gcvNULL;
    }
}

#endif /* __gc_hal_kernel_device_h_ */
