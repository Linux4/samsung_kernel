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


#include "gc_hal_kernel_linux.h"
#define CREATE_TRACE_POINTS
#include "gc_trace.h"

#include <linux/pagemap.h>
#include <linux/seq_file.h>
#include <linux/mman.h>
#include <asm/atomic.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/irqflags.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,23)
#include <linux/math64.h>
#endif
#include <linux/delay.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#include <linux/anon_inodes.h>
#endif

#if gcdANDROID_NATIVE_FENCE_SYNC
#include <linux/file.h>
#include "gc_hal_kernel_sync.h"
#endif

#include "gc_hal_kernel_plat.h"

#define _GC_OBJ_ZONE    gcvZONE_OS

#include "gc_hal_kernel_allocator.h"

#if MRVL_DFC_PROTECT_REG_ACCESS
extern void get_gc3d_reg_lock(unsigned int lock, unsigned long *flags);
extern void get_gc2d_reg_lock(unsigned int lock, unsigned long *flags);
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
#define GC_CLK_ENABLE       clk_prepare_enable
#define GC_CLK_DISABLE      clk_disable_unprepare
#else
#define GC_CLK_ENABLE       clk_enable
#define GC_CLK_DISABLE      clk_disable
#endif

#define MEMORY_MAP_LOCK(os) \
    gcmkVERIFY_OK(gckOS_AcquireMutex( \
                                (os), \
                                (os)->memoryMapLock, \
                                gcvINFINITE))

#define MEMORY_MAP_UNLOCK(os) \
    gcmkVERIFY_OK(gckOS_ReleaseMutex((os), (os)->memoryMapLock))

/******************************************************************************\
******************************* Private Functions ******************************
\******************************************************************************/
static gctINT
_GetThreadID(
    void
    )
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
    return task_pid_vnr(current);
#else
    return current->pid;
#endif
}

static PLINUX_MDL
_CreateMdl(
    void
    )
{
    PLINUX_MDL  mdl;

    gcmkHEADER();

    mdl = (PLINUX_MDL)kzalloc(sizeof(struct _LINUX_MDL), GFP_KERNEL | gcdNOWARN);

    gcmkFOOTER_ARG("0x%X", mdl);
    return mdl;
}

static gceSTATUS
_DestroyMdlMap(
    IN PLINUX_MDL Mdl,
    IN PLINUX_MDL_MAP MdlMap
    );

static gceSTATUS
_DestroyMdl(
    IN PLINUX_MDL Mdl
    )
{
    PLINUX_MDL_MAP mdlMap, next;

    gcmkHEADER_ARG("Mdl=0x%X", Mdl);

    /* Verify the arguments. */
    gcmkVERIFY_ARGUMENT(Mdl != gcvNULL);

    mdlMap = Mdl->maps;

    while (mdlMap != gcvNULL)
    {
        next = mdlMap->next;

        gcmkVERIFY_OK(_DestroyMdlMap(Mdl, mdlMap));

        mdlMap = next;
    }

    kfree(Mdl);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

static PLINUX_MDL_MAP
_CreateMdlMap(
    IN PLINUX_MDL Mdl,
    IN gctINT ProcessID
    )
{
    PLINUX_MDL_MAP  mdlMap;

    gcmkHEADER_ARG("Mdl=0x%X ProcessID=%d", Mdl, ProcessID);

    mdlMap = (PLINUX_MDL_MAP)vmalloc(sizeof(struct _LINUX_MDL_MAP));
    if (mdlMap == gcvNULL)
    {
        gcmkFOOTER_NO();
        return gcvNULL;
    }

    mdlMap->pid     = ProcessID;
    mdlMap->vmaAddr = gcvNULL;
    mdlMap->vma     = gcvNULL;
    mdlMap->count   = 0;

    mdlMap->next    = Mdl->maps;
    Mdl->maps       = mdlMap;

    gcmkFOOTER_ARG("0x%X", mdlMap);
    return mdlMap;
}

static gceSTATUS
_DestroyMdlMap(
    IN PLINUX_MDL Mdl,
    IN PLINUX_MDL_MAP MdlMap
    )
{
    PLINUX_MDL_MAP  prevMdlMap;

    gcmkHEADER_ARG("Mdl=0x%X MdlMap=0x%X", Mdl, MdlMap);

    /* Verify the arguments. */
    gcmkVERIFY_ARGUMENT(MdlMap != gcvNULL);
    gcmkVERIFY_ARGUMENT(Mdl != gcvNULL);
    gcmkASSERT(Mdl->maps != gcvNULL);

    if ((MdlMap == gcvNULL) || (Mdl == gcvNULL) || (Mdl->maps == gcvNULL))
    {
        gcmkFOOTER_NO();
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    if (Mdl->maps == MdlMap)
    {
        Mdl->maps = MdlMap->next;
    }
    else
    {
        prevMdlMap = Mdl->maps;

        while (prevMdlMap->next != MdlMap)
        {
            prevMdlMap = prevMdlMap->next;

            gcmkASSERT(prevMdlMap != gcvNULL);

            /* Not found. */
            if (prevMdlMap == gcvNULL)
            {
                gcmkFOOTER_NO();
                return gcvSTATUS_NOT_FOUND;
            }

        }

        prevMdlMap->next = MdlMap->next;
    }

    vfree(MdlMap);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

extern PLINUX_MDL_MAP
FindMdlMap(
    IN PLINUX_MDL Mdl,
    IN gctINT ProcessID
    )
{
    PLINUX_MDL_MAP  mdlMap;

    gcmkHEADER_ARG("Mdl=0x%X ProcessID=%d", Mdl, ProcessID);
    if(Mdl == gcvNULL)
    {
        gcmkFOOTER_NO();
        return gcvNULL;
    }
    mdlMap = Mdl->maps;

    while (mdlMap != gcvNULL)
    {
        if (mdlMap->pid == ProcessID)
        {
            gcmkFOOTER_ARG("0x%X", mdlMap);
            return mdlMap;
        }

        mdlMap = mdlMap->next;
    }

    gcmkFOOTER_NO();
    return gcvNULL;
}


#if MRVL_CONFIG_ENABLE_GPUFREQ
gceSTATUS
__srcu_notifier_list_init(
    IN gckOS Os,
    IN gctPOINTER NotifierListHead
    )
{
    gcmkHEADER_ARG("Os = 0x%08X", Os);
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(NotifierListHead != gcvNULL);

    /* initialize notifier list head */
    srcu_init_notifier_head((struct srcu_notifier_head *)NotifierListHead);

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}
#endif

/*******************************************************************************
** Integer Id Management.
*/
gceSTATUS
_AllocateIntegerId(
    IN gcsINTEGER_DB_PTR Database,
    IN gctPOINTER KernelPointer,
    OUT gctUINT32 *Id
    )
{
    int result;
    gctINT next;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)
    idr_preload(GFP_KERNEL | gcdNOWARN);

    spin_lock(&Database->lock);

    next = (Database->curr + 1 <= 0) ? 1 : Database->curr + 1;

    result = idr_alloc(&Database->idr, KernelPointer, next, 0, GFP_ATOMIC);

    /* ID allocated should not be 0. */
    gcmkASSERT(result != 0);

    if (result > 0)
    {
        Database->curr = *Id = result;
    }

    spin_unlock(&Database->lock);

    idr_preload_end();

    if (result < 0)
    {
        return gcvSTATUS_OUT_OF_RESOURCES;
    }
#else
again:
    if (idr_pre_get(&Database->idr, GFP_KERNEL | gcdNOWARN) == 0)
    {
        return gcvSTATUS_OUT_OF_MEMORY;
    }

    spin_lock(&Database->lock);

    next = (Database->curr + 1 <= 0) ? 1 : Database->curr + 1;

    /* Try to get a id greater than 0. */
    result = idr_get_new_above(&Database->idr, KernelPointer, next, Id);

    if (!result)
    {
        Database->curr = *Id;
    }

    spin_unlock(&Database->lock);

    if (result == -EAGAIN)
    {
        goto again;
    }

    if (result != 0)
    {
        return gcvSTATUS_OUT_OF_RESOURCES;
    }
#endif

    return gcvSTATUS_OK;
}

gceSTATUS
_QueryIntegerId(
    IN gcsINTEGER_DB_PTR Database,
    IN gctUINT32  Id,
    OUT gctPOINTER * KernelPointer
    )
{
    gctPOINTER pointer;

    spin_lock(&Database->lock);

    pointer = idr_find(&Database->idr, Id);

    spin_unlock(&Database->lock);

    if(pointer)
    {
        *KernelPointer = pointer;
        return gcvSTATUS_OK;
    }
    else
    {
        gcmkTRACE_ZONE(
                gcvLEVEL_ERROR, gcvZONE_OS,
                "%s(%d) Id = %d is not found",
                __FUNCTION__, __LINE__, Id);

        return gcvSTATUS_NOT_FOUND;
    }
}

gceSTATUS
_DestroyIntegerId(
    IN gcsINTEGER_DB_PTR Database,
    IN gctUINT32 Id
    )
{
    spin_lock(&Database->lock);

    idr_remove(&Database->idr, Id);

    spin_unlock(&Database->lock);

    return gcvSTATUS_OK;
}

gceSTATUS
_QueryProcessPageTable(
    IN gctPOINTER Logical,
    OUT gctPHYS_ADDR_T * Address
    )
{
    /* Logical mapped in kernel space*/
    if(is_vmalloc_addr(Logical))
    {
        *Address = (vmalloc_to_pfn(Logical) << PAGE_SHIFT) | ((gctUINTPTR_T)Logical & ~PAGE_MASK);
    }
    /* Logical allocated by kmalloc*/
    else if ((gctUINTPTR_T)Logical >= PAGE_OFFSET)
    {
        *Address = virt_to_phys(Logical);
    }
    /* Logical from userspace*/
    else
    {
        spinlock_t *lock;
        gctUINTPTR_T logical = (gctUINTPTR_T)Logical;
        pgd_t *pgd;
        pud_t *pud;
        pmd_t *pmd;
        pte_t *pte;

        if (!current->mm)
        {
            return gcvSTATUS_NOT_FOUND;
        }

        pgd = pgd_offset(current->mm, logical);
        if (pgd_none(*pgd) || pgd_bad(*pgd))
        {
            return gcvSTATUS_NOT_FOUND;
        }

        pud = pud_offset(pgd, logical);
        if (pud_none(*pud) || pud_bad(*pud))
        {
            return gcvSTATUS_NOT_FOUND;
        }

        pmd = pmd_offset(pud, logical);
        if (pmd_none(*pmd) || pmd_bad(*pmd))
        {
            return gcvSTATUS_NOT_FOUND;
        }

        pte = pte_offset_map_lock(current->mm, pmd, logical, &lock);
        if (!pte)
        {
            return gcvSTATUS_NOT_FOUND;
        }

        if (!pte_present(*pte))
        {
            pte_unmap_unlock(pte, lock);
            return gcvSTATUS_NOT_FOUND;
        }

        *Address = (pte_pfn(*pte) << PAGE_SHIFT) | (logical & ~PAGE_MASK);
        pte_unmap_unlock(pte, lock);
    }

    return gcvSTATUS_OK;
}

#if !gcdCACHE_FUNCTION_UNIMPLEMENTED && defined(CONFIG_OUTER_CACHE)
static inline gceSTATUS
outer_func(
    gceCACHEOPERATION Type,
    unsigned long Start,
    unsigned long End
    )
{
    switch (Type)
    {
        case gcvCACHE_CLEAN:
            outer_clean_range(Start, End);
            break;
        case gcvCACHE_INVALIDATE:
            outer_inv_range(Start, End);
            break;
        case gcvCACHE_FLUSH:
            outer_flush_range(Start, End);
            break;
        default:
            return gcvSTATUS_INVALID_ARGUMENT;
            break;
    }
    return gcvSTATUS_OK;
}

#if gcdENABLE_OUTER_CACHE_PATCH
/*******************************************************************************
**  _HandleOuterCache
**
**  Handle the outer cache for the specified addresses.
**
**  ARGUMENTS:
**
**      gckOS Os
**          Pointer to gckOS object.
**
**      gctPOINTER Physical
**          Physical address to flush.
**
**      gctPOINTER Logical
**          Logical address to flush.
**
**      gctSIZE_T Bytes
**          Size of the address range in bytes to flush.
**
**      gceOUTERCACHE_OPERATION Type
**          Operation need to be execute.
*/
gceSTATUS
_HandleOuterCache(
    IN gckOS Os,
    IN gctUINT32 Physical,
    IN gctPOINTER Logical,
    IN gctSIZE_T Bytes,
    IN gceCACHEOPERATION Type
    )
{
    gceSTATUS status;
    gctPHYS_ADDR_T paddr;
    gctPOINTER vaddr;
    gctUINT32 offset, bytes, left;

    gcmkHEADER_ARG("Os=0x%X Logical=0x%X Bytes=%lu",
                   Os, Logical, Bytes);

    if (Physical != gcvINVALID_ADDRESS)
    {
        /* Non paged memory or gcvPOOL_USER surface */
        paddr = (unsigned long) Physical;
        gcmkONERROR(outer_func(Type, paddr, paddr + Bytes));
    }
    else
    {
        /* Non contiguous virtual memory */
        vaddr = Logical;
        left = Bytes;

        while (left)
        {
            /* Handle (part of) current page. */
            offset = (gctUINTPTR_T)vaddr & ~PAGE_MASK;

            bytes = gcmMIN(left, PAGE_SIZE - offset);

            gcmkONERROR(_QueryProcessPageTable(vaddr, &paddr));
            gcmkONERROR(outer_func(Type, paddr, paddr + bytes));

            vaddr = (gctUINT8_PTR)vaddr + bytes;
            left -= bytes;
        }
    }

    mb();

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}
#endif
#endif

gctBOOL
_AllowAccess(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctUINT32 Address
    )
{
    gctUINT32 data;

    /* Check external clock state. */
    if (Os->clockStates[Core] == gcvFALSE)
    {
        gcmkPRINT("[galcore]: %s(%d) External clock off", __FUNCTION__, __LINE__);
        return gcvFALSE;
    }

    /* Check internal clock state. */
    if (Address == 0)
    {
        return gcvTRUE;
    }

#if gcdMULTI_GPU
    if (Core == gcvCORE_MAJOR)
    {
        data = readl((gctUINT8 *)Os->device->registerBase3D[gcvCORE_3D_0_ID] + 0x0);
    }
    else
#endif
    {
        data = readl((gctUINT8 *)Os->device->registerBases[Core] + 0x0);
    }

    if ((data & 0x3) == 0x3)
    {
        gcmkPRINT("[galcore]: %s(%d) Internal clock off", __FUNCTION__, __LINE__);
        return gcvFALSE;
    }

    return gcvTRUE;
}

static gceSTATUS
_ShrinkMemory(
    IN gckOS Os
    )
{
    gcsPLATFORM * platform;

    gcmkHEADER_ARG("Os=0x%X", Os);
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    platform = Os->device->platform;

    if (platform && platform->ops->shrinkMemory)
    {
        platform->ops->shrinkMemory(platform);
    }
    else
    {
        gcmkFOOTER_NO();
        return gcvSTATUS_NOT_SUPPORTED;
    }

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_Construct
**
**  Construct a new gckOS object.
**
**  INPUT:
**
**      gctPOINTER Context
**          Pointer to the gckGALDEVICE class.
**
**  OUTPUT:
**
**      gckOS * Os
**          Pointer to a variable that will hold the pointer to the gckOS object.
*/
gceSTATUS
gckOS_Construct(
    IN gctPOINTER Context,
    OUT gckOS * Os
    )
{
    gckOS os;
    gceSTATUS status;
    gctINT i;

    gcmkHEADER_ARG("Context=0x%X", Context);

    /* Verify the arguments. */
    gcmkVERIFY_ARGUMENT(Os != gcvNULL);

    /* Allocate the gckOS object. */
    os = (gckOS) kmalloc(gcmSIZEOF(struct _gckOS), GFP_KERNEL | gcdNOWARN);

    if (os == gcvNULL)
    {
        /* Out of memory. */
        gcmkFOOTER_ARG("status=%d", gcvSTATUS_OUT_OF_MEMORY);
        return gcvSTATUS_OUT_OF_MEMORY;
    }

    /* Zero the memory. */
    gckOS_ZeroMemory(os, gcmSIZEOF(struct _gckOS));

    /* Initialize the gckOS object. */
    os->object.type = gcvOBJ_OS;

    /* Set device device. */
    os->device = Context;

    /* Set allocateCount to 0, gckOS_Allocate has not been used yet. */
    atomic_set(&os->allocateCount, 0);

    /* Initialize the memory lock. */
    gcmkONERROR(gckOS_CreateMutex(os, &os->memoryLock));
    gcmkONERROR(gckOS_CreateMutex(os, &os->memoryMapLock));

    /* Create debug lock mutex. */
    gcmkONERROR(gckOS_CreateMutex(os, &os->debugLock));

    os->mdlHead = os->mdlTail = gcvNULL;

#if MRVL_CONFIG_ENABLE_GPUFREQ
    gcmkONERROR(__srcu_notifier_list_init(os, (gctPOINTER)&os->nb_list_head));
#endif

    /* Get the kernel process ID. */
    gcmkONERROR(gckOS_GetProcessID(&os->kernelProcessID));

    /*
     * Initialize the signal manager.
     */

    /* Initialize mutex. */
    gcmkONERROR(gckOS_CreateMutex(os, &os->signalMutex));

    /* Initialize signal id database lock. */
    spin_lock_init(&os->signalDB.lock);

    /* Initialize signal id database. */
    idr_init(&os->signalDB.idr);

#if gcdANDROID_NATIVE_FENCE_SYNC
    /*
     * Initialize the sync point manager.
     */

    /* Initialize mutex. */
    gcmkONERROR(gckOS_CreateMutex(os, &os->syncPointMutex));

    /* Initialize sync point id database lock. */
    spin_lock_init(&os->syncPointDB.lock);

    /* Initialize sync point id database. */
    idr_init(&os->syncPointDB.idr);
#endif

    os->clockDepth  = MRVL_MAX_CLOCK_DEPTH;
    os->powerDepth  = MRVL_MAX_POWER_DEPTH;

    os->clkOffWhenIdle  = gcvTRUE;

    init_rwsem(&os->rwsem_clk_pwr);

    /* Create a workqueue for os timer. */
    os->workqueue = alloc_workqueue("galcore workqueue", WQ_HIGHPRI | WQ_MEM_RECLAIM,  0);;

    if (os->workqueue == gcvNULL)
    {
        /* Out of memory. */
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    os->paddingPage = alloc_page(GFP_KERNEL | __GFP_HIGHMEM | gcdNOWARN);
    if (os->paddingPage == gcvNULL)
    {
        /* Out of memory. */
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }
    else
    {
        SetPageReserved(os->paddingPage);
    }

    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        mutex_init(&os->registerAccessLocks[i]);
    }

    gckOS_ImportAllocators(os);

#ifdef CONFIG_IOMMU_SUPPORT
    if (((gckGALDEVICE)(os->device))->mmu == gcvFALSE)
    {
        /* Only use IOMMU when internal MMU is not enabled. */
        status = gckIOMMU_Construct(os, &os->iommu);

        if (gcmIS_ERROR(status))
        {
            gcmkTRACE_ZONE(
                gcvLEVEL_INFO, gcvZONE_OS,
                "%s(%d): Fail to setup IOMMU",
                __FUNCTION__, __LINE__
                );
        }
    }
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
    /* get frequency table*/
    for(i = 0; i<gcdMAX_GPU_COUNT; i++)
    {
        os->freqTable[i] = (gctPOINTER)(os->device->ft+i*10);
    }
#endif

    /* Return pointer to the gckOS object. */
    *Os = os;

    /* Success. */
    gcmkFOOTER_ARG("*Os=0x%X", *Os);
    return gcvSTATUS_OK;

OnError:

#if gcdANDROID_NATIVE_FENCE_SYNC
    if (os->syncPointMutex != gcvNULL)
    {
        gcmkVERIFY_OK(
            gckOS_DeleteMutex(os, os->syncPointMutex));
    }
#endif

    if (os->signalMutex != gcvNULL)
    {
        gcmkVERIFY_OK(
            gckOS_DeleteMutex(os, os->signalMutex));
    }

    if (os->memoryMapLock != gcvNULL)
    {
        gcmkVERIFY_OK(
            gckOS_DeleteMutex(os, os->memoryMapLock));
    }

    if (os->memoryLock != gcvNULL)
    {
        gcmkVERIFY_OK(
            gckOS_DeleteMutex(os, os->memoryLock));
    }

    if (os->debugLock != gcvNULL)
    {
        gcmkVERIFY_OK(
            gckOS_DeleteMutex(os, os->debugLock));
    }

    if (os->workqueue != gcvNULL)
    {
        destroy_workqueue(os->workqueue);
    }

    kfree(os);

    /* Return the error. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_Destroy
**
**  Destroy an gckOS object.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object that needs to be destroyed.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_Destroy(
    IN gckOS Os
    )
{
    gcmkHEADER_ARG("Os=0x%X", Os);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    if (Os->paddingPage != gcvNULL)
    {
        ClearPageReserved(Os->paddingPage);
        __free_page(Os->paddingPage);
        Os->paddingPage = gcvNULL;
    }

#if gcdANDROID_NATIVE_FENCE_SYNC
    /*
     * Destroy the sync point manager.
     */

    /* Destroy the mutex. */
    gcmkVERIFY_OK(gckOS_DeleteMutex(Os, Os->syncPointMutex));
#endif

    /*
     * Destroy the signal manager.
     */

    /* Destroy the mutex. */
    gcmkVERIFY_OK(gckOS_DeleteMutex(Os, Os->signalMutex));

    /* Destroy the memory lock. */
    gcmkVERIFY_OK(gckOS_DeleteMutex(Os, Os->memoryMapLock));
    gcmkVERIFY_OK(gckOS_DeleteMutex(Os, Os->memoryLock));

    /* Destroy debug lock mutex. */
    gcmkVERIFY_OK(gckOS_DeleteMutex(Os, Os->debugLock));

    /* Wait for all works done. */
    flush_workqueue(Os->workqueue);

    /* Destory work queue. */
    destroy_workqueue(Os->workqueue);

    gckOS_FreeAllocators(Os);

#ifdef CONFIG_IOMMU_SUPPORT
    if (Os->iommu)
    {
        gckIOMMU_Destory(Os, Os->iommu);
    }
#endif

    /* Flush the debug cache. */
    gcmkDEBUGFLUSH(~0U);

    /* Mark the gckOS object as unknown. */
    Os->object.type = gcvOBJ_UNKNOWN;


    /* Free the gckOS object. */
    kfree(Os);

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_CreateKernelVirtualMapping(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T Bytes,
    OUT gctPOINTER * Logical,
    OUT gctSIZE_T * PageCount
    )
{
    gceSTATUS status;
    PLINUX_MDL mdl = (PLINUX_MDL)Physical;
    gckALLOCATOR allocator = mdl->allocator;

    gcmkHEADER();

    *PageCount = mdl->numPages;

    gcmkONERROR(allocator->ops->MapKernel(allocator, mdl, Logical));

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
}

gceSTATUS
gckOS_DestroyKernelVirtualMapping(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T Bytes,
    IN gctPOINTER Logical
    )
{
    PLINUX_MDL mdl = (PLINUX_MDL)Physical;
    gckALLOCATOR allocator = mdl->allocator;

    gcmkHEADER();

    allocator->ops->UnmapKernel(allocator, mdl, Logical);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_CreateUserVirtualMapping(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T Bytes,
    OUT gctPOINTER * Logical,
    OUT gctSIZE_T * PageCount
    )
{
    return gckOS_LockPages(Os, Physical, Bytes, gcvFALSE, Logical, PageCount);
}

gceSTATUS
gckOS_DestroyUserVirtualMapping(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T Bytes,
    IN gctPOINTER Logical
    )
{
    return gckOS_UnlockPages(Os, Physical, Bytes, Logical);
}

/*******************************************************************************
**
**  gckOS_Allocate
**
**  Allocate memory.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctSIZE_T Bytes
**          Number of bytes to allocate.
**
**  OUTPUT:
**
**      gctPOINTER * Memory
**          Pointer to a variable that will hold the allocated memory location.
*/
gceSTATUS
gckOS_Allocate(
    IN gckOS Os,
    IN gctSIZE_T Bytes,
    OUT gctPOINTER * Memory
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%X Bytes=%lu", Os, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Bytes > 0);
    gcmkVERIFY_ARGUMENT(Memory != gcvNULL);

    gcmkONERROR(gckOS_AllocateMemory(Os, Bytes, Memory));

    /* Success. */
    gcmkFOOTER_ARG("*Memory=0x%X", *Memory);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_Free
**
**  Free allocated memory.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPOINTER Memory
**          Pointer to memory allocation to free.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_Free(
    IN gckOS Os,
    IN gctPOINTER Memory
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%X Memory=0x%X", Os, Memory);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Memory != gcvNULL);

    gcmkONERROR(gckOS_FreeMemory(Os, Memory));

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_AllocateMemory
**
**  Allocate memory wrapper.
**
**  INPUT:
**
**      gctSIZE_T Bytes
**          Number of bytes to allocate.
**
**  OUTPUT:
**
**      gctPOINTER * Memory
**          Pointer to a variable that will hold the allocated memory location.
*/
gceSTATUS
gckOS_AllocateMemory(
    IN gckOS Os,
    IN gctSIZE_T Bytes,
    OUT gctPOINTER * Memory
    )
{
    gctPOINTER memory;
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%X Bytes=%lu", Os, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_ARGUMENT(Bytes > 0);
    gcmkVERIFY_ARGUMENT(Memory != gcvNULL);

    if (Bytes > PAGE_SIZE)
    {
        memory = (gctPOINTER) vmalloc(Bytes);
    }
    else
    {
        memory = (gctPOINTER) kmalloc(Bytes, GFP_KERNEL | gcdNOWARN);
    }

    if (memory == gcvNULL)
    {
        /* Out of memory. */
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    /* Increase count. */
    atomic_inc(&Os->allocateCount);

    /* Return pointer to the memory allocation. */
    *Memory = memory;

    /* Success. */
    gcmkFOOTER_ARG("*Memory=0x%X", *Memory);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_FreeMemory
**
**  Free allocated memory wrapper.
**
**  INPUT:
**
**      gctPOINTER Memory
**          Pointer to memory allocation to free.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_FreeMemory(
    IN gckOS Os,
    IN gctPOINTER Memory
    )
{
    gcmkHEADER_ARG("Memory=0x%X", Memory);

    /* Verify the arguments. */
    gcmkVERIFY_ARGUMENT(Memory != gcvNULL);

    /* Free the memory from the OS pool. */
    if (is_vmalloc_addr(Memory))
    {
        vfree(Memory);
    }
    else
    {
        kfree(Memory);
    }

    /* Decrease count. */
    atomic_dec(&Os->allocateCount);

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_MapMemory
**
**  Map physical memory into the current process.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPHYS_ADDR Physical
**          Start of physical address memory.
**
**      gctSIZE_T Bytes
**          Number of bytes to map.
**
**  OUTPUT:
**
**      gctPOINTER * Memory
**          Pointer to a variable that will hold the logical address of the
**          mapped memory.
*/
gceSTATUS
gckOS_MapMemory(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T Bytes,
    OUT gctPOINTER * Logical
    )
{
    PLINUX_MDL_MAP  mdlMap;
    PLINUX_MDL      mdl = (PLINUX_MDL)Physical;

    gcmkHEADER_ARG("Os=0x%X Physical=0x%X Bytes=%lu", Os, Physical, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != 0);
    gcmkVERIFY_ARGUMENT(Bytes > 0);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);

    MEMORY_LOCK(Os);

    mdlMap = FindMdlMap(mdl, _GetProcessID());

    if (mdlMap == gcvNULL)
    {
        mdlMap = _CreateMdlMap(mdl, _GetProcessID());

        if (mdlMap == gcvNULL)
        {
            MEMORY_UNLOCK(Os);

            gcmkFOOTER_ARG("status=%d", gcvSTATUS_OUT_OF_MEMORY);
            return gcvSTATUS_OUT_OF_MEMORY;
        }
    }

    if (mdlMap->vmaAddr == gcvNULL)
    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
        mdlMap->vmaAddr = (char *)vm_mmap(gcvNULL,
                    0L,
                    mdl->numPages * PAGE_SIZE,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED,
                    0);
#else
        down_write(&current->mm->mmap_sem);

        mdlMap->vmaAddr = (char *)do_mmap_pgoff(gcvNULL,
                    0L,
                    mdl->numPages * PAGE_SIZE,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED,
                    0);

        up_write(&current->mm->mmap_sem);
#endif

        if (IS_ERR(mdlMap->vmaAddr))
        {
            gcmkTRACE(
                gcvLEVEL_ERROR,
                "%s(%d): do_mmap_pgoff error",
                __FUNCTION__, __LINE__
                );

            gcmkTRACE(
                gcvLEVEL_ERROR,
                "%s(%d): mdl->numPages: %d mdl->vmaAddr: 0x%X",
                __FUNCTION__, __LINE__,
                mdl->numPages,
                mdlMap->vmaAddr
                );

            mdlMap->vmaAddr = gcvNULL;

            MEMORY_UNLOCK(Os);

            gcmkFOOTER_ARG("status=%d", gcvSTATUS_OUT_OF_MEMORY);
            return gcvSTATUS_OUT_OF_MEMORY;
        }

        down_write(&current->mm->mmap_sem);

        mdlMap->vma = find_vma(current->mm, (unsigned long)mdlMap->vmaAddr);

        if (!mdlMap->vma)
        {
            gcmkTRACE(
                gcvLEVEL_ERROR,
                "%s(%d): find_vma error.",
                __FUNCTION__, __LINE__
                );

            mdlMap->vmaAddr = gcvNULL;

            up_write(&current->mm->mmap_sem);

            MEMORY_UNLOCK(Os);

            gcmkFOOTER_ARG("status=%d", gcvSTATUS_OUT_OF_RESOURCES);
            return gcvSTATUS_OUT_OF_RESOURCES;
        }

#ifndef NO_DMA_COHERENT
        if (dma_mmap_writecombine(gcvNULL,
                    mdlMap->vma,
                    mdl->addr,
                    mdl->dmaHandle,
                    mdl->numPages * PAGE_SIZE) < 0)
        {
            up_write(&current->mm->mmap_sem);

            gcmkTRACE(
                gcvLEVEL_ERROR,
                "%s(%d): dma_mmap_coherent error.",
                __FUNCTION__, __LINE__
                );

            mdlMap->vmaAddr = gcvNULL;

            MEMORY_UNLOCK(Os);

            gcmkFOOTER_ARG("status=%d", gcvSTATUS_OUT_OF_RESOURCES);
            return gcvSTATUS_OUT_OF_RESOURCES;
        }
#else
#if !gcdPAGED_MEMORY_CACHEABLE
        mdlMap->vma->vm_page_prot = gcmkPAGED_MEMROY_PROT(mdlMap->vma->vm_page_prot);
        mdlMap->vma->vm_flags |= gcdVM_FLAGS;
#   endif
        mdlMap->vma->vm_pgoff = 0;

        if (remap_pfn_range(mdlMap->vma,
                            mdlMap->vma->vm_start,
                            mdl->dmaHandle >> PAGE_SHIFT,
                            mdl->numPages*PAGE_SIZE,
                            mdlMap->vma->vm_page_prot) < 0)
        {
            up_write(&current->mm->mmap_sem);

            gcmkTRACE(
                gcvLEVEL_ERROR,
                "%s(%d): remap_pfn_range error.",
                __FUNCTION__, __LINE__
                );

            mdlMap->vmaAddr = gcvNULL;

            MEMORY_UNLOCK(Os);

            gcmkFOOTER_ARG("status=%d", gcvSTATUS_OUT_OF_RESOURCES);
            return gcvSTATUS_OUT_OF_RESOURCES;
        }
#endif

        up_write(&current->mm->mmap_sem);
    }

    MEMORY_UNLOCK(Os);

    *Logical = mdlMap->vmaAddr;

    gcmkFOOTER_ARG("*Logical=0x%X", *Logical);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_UnmapMemory
**
**  Unmap physical memory out of the current process.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPHYS_ADDR Physical
**          Start of physical address memory.
**
**      gctSIZE_T Bytes
**          Number of bytes to unmap.
**
**      gctPOINTER Memory
**          Pointer to a previously mapped memory region.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_UnmapMemory(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T Bytes,
    IN gctPOINTER Logical
    )
{
    gcmkHEADER_ARG("Os=0x%X Physical=0x%X Bytes=%lu Logical=0x%X",
                   Os, Physical, Bytes, Logical);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != 0);
    gcmkVERIFY_ARGUMENT(Bytes > 0);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);

    gckOS_UnmapMemoryEx(Os, Physical, Bytes, Logical, _GetProcessID());

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}


/*******************************************************************************
**
**  gckOS_UnmapMemoryEx
**
**  Unmap physical memory in the specified process.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPHYS_ADDR Physical
**          Start of physical address memory.
**
**      gctSIZE_T Bytes
**          Number of bytes to unmap.
**
**      gctPOINTER Memory
**          Pointer to a previously mapped memory region.
**
**      gctUINT32 PID
**          Pid of the process that opened the device and mapped this memory.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_UnmapMemoryEx(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T Bytes,
    IN gctPOINTER Logical,
    IN gctUINT32 PID
    )
{
    PLINUX_MDL_MAP          mdlMap;
    PLINUX_MDL              mdl = (PLINUX_MDL)Physical;

    gcmkHEADER_ARG("Os=0x%X Physical=0x%X Bytes=%lu Logical=0x%X PID=%d",
                   Os, Physical, Bytes, Logical, PID);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != 0);
    gcmkVERIFY_ARGUMENT(Bytes > 0);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);
    gcmkVERIFY_ARGUMENT(PID != 0);

    MEMORY_LOCK(Os);

    if (Logical)
    {
        mdlMap = FindMdlMap(mdl, PID);

        if (mdlMap == gcvNULL || mdlMap->vmaAddr == gcvNULL)
        {
            MEMORY_UNLOCK(Os);

            gcmkFOOTER_ARG("status=%d", gcvSTATUS_INVALID_ARGUMENT);
            return gcvSTATUS_INVALID_ARGUMENT;
        }

        _UnmapUserLogical(mdlMap->vmaAddr, mdl->numPages * PAGE_SIZE);

        gcmkVERIFY_OK(_DestroyMdlMap(mdl, mdlMap));
    }

    MEMORY_UNLOCK(Os);

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_UnmapUserLogical
**
**  Unmap user logical memory out of physical memory.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPHYS_ADDR Physical
**          Start of physical address memory.
**
**      gctSIZE_T Bytes
**          Number of bytes to unmap.
**
**      gctPOINTER Memory
**          Pointer to a previously mapped memory region.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_UnmapUserLogical(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T Bytes,
    IN gctPOINTER Logical
    )
{
    gcmkHEADER_ARG("Os=0x%X Physical=0x%X Bytes=%lu Logical=0x%X",
                   Os, Physical, Bytes, Logical);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != 0);
    gcmkVERIFY_ARGUMENT(Bytes > 0);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);

    gckOS_UnmapMemory(Os, Physical, Bytes, Logical);

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

}

/*******************************************************************************
**
**  gckOS_AllocateNonPagedMemory
**
**  Allocate a number of pages from non-paged memory.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctBOOL InUserSpace
**          gcvTRUE if the pages need to be mapped into user space.
**
**      gctSIZE_T * Bytes
**          Pointer to a variable that holds the number of bytes to allocate.
**
**  OUTPUT:
**
**      gctSIZE_T * Bytes
**          Pointer to a variable that hold the number of bytes allocated.
**
**      gctPHYS_ADDR * Physical
**          Pointer to a variable that will hold the physical address of the
**          allocation.
**
**      gctPOINTER * Logical
**          Pointer to a variable that will hold the logical address of the
**          allocation.
*/
gceSTATUS
gckOS_AllocateNonPagedMemory(
    IN gckOS Os,
    IN gctBOOL InUserSpace,
    IN OUT gctSIZE_T * Bytes,
    OUT gctPHYS_ADDR * Physical,
    OUT gctPOINTER * Logical
    )
{
    gctSIZE_T bytes;
    gctINT numPages;
    PLINUX_MDL mdl = gcvNULL;
    PLINUX_MDL_MAP mdlMap = gcvNULL;
    gctSTRING addr;
    gckKERNEL kernel;
#ifdef NO_DMA_COHERENT
    struct page * page;
    long size, order;
    gctPOINTER vaddr;
#endif
    gctBOOL locked = gcvFALSE;
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%X InUserSpace=%d *Bytes=%lu",
                   Os, InUserSpace, gcmOPT_VALUE(Bytes));

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Bytes != gcvNULL);
    gcmkVERIFY_ARGUMENT(*Bytes > 0);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);

    /* Align number of bytes to page size. */
    bytes = gcmALIGN(*Bytes, PAGE_SIZE);

    /* Get total number of pages.. */
    numPages = GetPageCount(bytes, 0);

    /* Allocate mdl+vector structure */
    mdl = _CreateMdl();
    if (mdl == gcvNULL)
    {
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    mdl->pagedMem = 0;
    mdl->numPages = numPages;
    mdl->wastSize  = bytes - (*Bytes);

#ifndef NO_DMA_COHERENT
#ifdef CONFIG_ARM64
    addr = dma_alloc_coherent(gcvNULL,
#else
    addr = dma_alloc_writecombine(gcvNULL,
#endif
            mdl->numPages * PAGE_SIZE,
            &mdl->dmaHandle,
            GFP_KERNEL | gcdNOWARN);
#else
    size    = mdl->numPages * PAGE_SIZE;
    order   = get_order(size);

    page = alloc_pages(GFP_KERNEL | gcdNOWARN, order);

    if (page == gcvNULL)
    {
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    vaddr           = (gctPOINTER)page_address(page);
    mdl->contiguous = gcvTRUE;
    mdl->u.contiguousPages = page;
    addr            = _CreateKernelVirtualMapping(mdl);
    mdl->dmaHandle  = virt_to_phys(vaddr);
    mdl->kaddr      = vaddr;

    /* Trigger a page fault. */
    memset(addr, 0, numPages * PAGE_SIZE);

#if !defined(CONFIG_PPC)
    /* Cache invalidate. */
    dma_sync_single_for_device(
                gcvNULL,
                page_to_phys(page),
                bytes,
                DMA_FROM_DEVICE);
#endif

    while (size > 0)
    {
        SetPageReserved(virt_to_page(vaddr));

        vaddr   += PAGE_SIZE;
        size    -= PAGE_SIZE;
    }
#endif

    if (addr == gcvNULL)
    {
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    kernel = Os->device->kernels[gcvCORE_MAJOR] != gcvNULL ?
                Os->device->kernels[gcvCORE_MAJOR] : Os->device->kernels[gcvCORE_2D];

#ifdef CONFLICT_BETWEEN_BASE_AND_PHYS
    if (((Os->device->baseAddress & 0x80000000) != (mdl->dmaHandle & 0x80000000)) &&
          kernel->hardware->mmuVersion == 0)
    {
        mdl->dmaHandle = (mdl->dmaHandle & ~0x80000000)
                       | (Os->device->baseAddress & 0x80000000);
    }
#endif

    mdl->addr = addr;

    if (InUserSpace)
    {
        mdlMap = _CreateMdlMap(mdl, _GetProcessID());

        if (mdlMap == gcvNULL)
        {
            gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
        }

        /* Only after mmap this will be valid. */

        /* We need to map this to user space. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
        mdlMap->vmaAddr = (gctSTRING) vm_mmap(gcvNULL,
                0L,
                mdl->numPages * PAGE_SIZE,
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                0);
#else
        down_write(&current->mm->mmap_sem);

        mdlMap->vmaAddr = (gctSTRING) do_mmap_pgoff(gcvNULL,
                0L,
                mdl->numPages * PAGE_SIZE,
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                0);

        up_write(&current->mm->mmap_sem);
#endif

        if (IS_ERR(mdlMap->vmaAddr))
        {
            gcmkTRACE_ZONE(
                gcvLEVEL_WARNING, gcvZONE_OS,
                "%s(%d): do_mmap_pgoff error",
                __FUNCTION__, __LINE__
                );

            mdlMap->vmaAddr = gcvNULL;

            gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
        }

        down_write(&current->mm->mmap_sem);

        mdlMap->vma = find_vma(current->mm, (unsigned long)mdlMap->vmaAddr);

        if (mdlMap->vma == gcvNULL)
        {
            gcmkTRACE_ZONE(
                gcvLEVEL_WARNING, gcvZONE_OS,
                "%s(%d): find_vma error",
                __FUNCTION__, __LINE__
                );

            up_write(&current->mm->mmap_sem);

            gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
        }

#ifndef NO_DMA_COHERENT
        if (dma_mmap_coherent(gcvNULL,
                mdlMap->vma,
                mdl->addr,
                mdl->dmaHandle,
                mdl->numPages * PAGE_SIZE) < 0)
        {
            gcmkTRACE_ZONE(
                gcvLEVEL_WARNING, gcvZONE_OS,
                "%s(%d): dma_mmap_coherent error",
                __FUNCTION__, __LINE__
                );

            up_write(&current->mm->mmap_sem);

            gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
        }
#else
#if !gcdSECURITY
        mdlMap->vma->vm_page_prot = gcmkNONPAGED_MEMROY_PROT(mdlMap->vma->vm_page_prot);
#endif
        mdlMap->vma->vm_flags |= gcdVM_FLAGS;
        mdlMap->vma->vm_pgoff = 0;

        if (remap_pfn_range(mdlMap->vma,
                            mdlMap->vma->vm_start,
                            mdl->dmaHandle >> PAGE_SHIFT,
                            mdl->numPages * PAGE_SIZE,
                            mdlMap->vma->vm_page_prot))
        {
            gcmkTRACE_ZONE(
                gcvLEVEL_WARNING, gcvZONE_OS,
                "%s(%d): remap_pfn_range error",
                __FUNCTION__, __LINE__
                );

            up_write(&current->mm->mmap_sem);

            gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
        }
#endif /* NO_DMA_COHERENT */

        up_write(&current->mm->mmap_sem);

        *Logical = mdlMap->vmaAddr;
    }
    else
    {
#if gcdSECURITY
        *Logical = (gctPOINTER)mdl->kaddr;
#else
        *Logical = (gctPOINTER)mdl->addr;
#endif
    }

    /*
     * Add this to a global list.
     * Will be used by get physical address
     * and mapuser pointer functions.
     */
    MEMORY_LOCK(Os);
    locked = gcvTRUE;

    if (!Os->mdlHead)
    {
        /* Initialize the queue. */
        Os->mdlHead = Os->mdlTail = mdl;
    }
    else
    {
        /* Add to the tail. */
        mdl->prev = Os->mdlTail;
        Os->mdlTail->next = mdl;
        Os->mdlTail = mdl;
    }

    if(*Bytes < Os->device->contiguousSize)
    {
        Os->device->contiguousNonPagedMemUsage += bytes;
    }
    Os->device->wastBytes += mdl->wastSize;

    MEMORY_UNLOCK(Os);
    locked = gcvFALSE;
    if(*Bytes < Os->device->contiguousSize)
        gckOS_UpdateVidMemUsage(Os, gcvTRUE, bytes, gcvSURF_TYPE_UNKNOWN);

    /* Return allocated memory. */
    *Bytes = bytes;
    *Physical = (gctPHYS_ADDR) mdl;

    /* Success. */
    gcmkFOOTER_ARG("*Bytes=%lu *Physical=0x%X *Logical=0x%X",
                   *Bytes, *Physical, *Logical);
    return gcvSTATUS_OK;

OnError:
    if (mdlMap != gcvNULL)
    {
        /* Free LINUX_MDL_MAP. */
        gcmkVERIFY_OK(_DestroyMdlMap(mdl, mdlMap));
    }

    if (mdl != gcvNULL)
    {
        /* Free LINUX_MDL. */
        gcmkVERIFY_OK(_DestroyMdl(mdl));
    }

    if (locked)
    {
        /* Unlock memory. */
        MEMORY_UNLOCK(Os);
    }

    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_FreeNonPagedMemory
**
**  Free previously allocated and mapped pages from non-paged memory.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctSIZE_T Bytes
**          Number of bytes allocated.
**
**      gctPHYS_ADDR Physical
**          Physical address of the allocated memory.
**
**      gctPOINTER Logical
**          Logical address of the allocated memory.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gckOS_FreeNonPagedMemory(
    IN gckOS Os,
    IN gctSIZE_T Bytes,
    IN gctPHYS_ADDR Physical,
    IN gctPOINTER Logical
    )
{
    PLINUX_MDL mdl;
    PLINUX_MDL_MAP mdlMap;
#ifdef NO_DMA_COHERENT
    unsigned size;
    gctPOINTER vaddr;
#endif /* NO_DMA_COHERENT */

    gcmkHEADER_ARG("Os=0x%X Bytes=%lu Physical=0x%X Logical=0x%X",
                   Os, Bytes, Physical, Logical);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Bytes > 0);
    gcmkVERIFY_ARGUMENT(Physical != 0);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);

    /* Convert physical address into a pointer to a MDL. */
    mdl = (PLINUX_MDL) Physical;

    MEMORY_LOCK(Os);

#ifndef NO_DMA_COHERENT
#ifdef CONFIG_ARM64
    dma_free_coherent(gcvNULL,
#else
    dma_free_writecombine(gcvNULL,
#endif
            mdl->numPages * PAGE_SIZE,
            mdl->addr,
            mdl->dmaHandle);
#else
    size    = mdl->numPages * PAGE_SIZE;
    vaddr   = mdl->kaddr;

    while (size > 0)
    {
        ClearPageReserved(virt_to_page(vaddr));

        vaddr   += PAGE_SIZE;
        size    -= PAGE_SIZE;
    }

    free_pages((unsigned long)mdl->kaddr, get_order(mdl->numPages * PAGE_SIZE));

    _DestoryKernelVirtualMapping(mdl->addr);
#endif /* NO_DMA_COHERENT */

    mdlMap = mdl->maps;

    while (mdlMap != gcvNULL)
    {
        /* No mapped memory exists when free nonpaged memory */
        gcmkASSERT(mdlMap->vmaAddr == gcvNULL);

        mdlMap = mdlMap->next;
    }

    /* Remove the node from global list.. */
    if (mdl == Os->mdlHead)
    {
        if ((Os->mdlHead = mdl->next) == gcvNULL)
        {
            Os->mdlTail = gcvNULL;
        }
    }
    else
    {
        mdl->prev->next = mdl->next;
        if (mdl == Os->mdlTail)
        {
            Os->mdlTail = mdl->prev;
        }
        else
        {
            mdl->next->prev = mdl->prev;
        }
    }

    if(Bytes < Os->device->contiguousSize)
    {
        Os->device->contiguousNonPagedMemUsage -= mdl->numPages * PAGE_SIZE;
    }
    Os->device->wastBytes -= mdl->wastSize;
    MEMORY_UNLOCK(Os);
    if(Bytes < Os->device->contiguousSize)
        gckOS_UpdateVidMemUsage(Os, gcvFALSE, (mdl->numPages * PAGE_SIZE), gcvSURF_TYPE_UNKNOWN);

    gcmkVERIFY_OK(_DestroyMdl(mdl));

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_ReadRegister
**
**  Read data from a register.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctUINT32 Address
**          Address of register.
**
**  OUTPUT:
**
**      gctUINT32 * Data
**          Pointer to a variable that receives the data read from the register.
*/
gceSTATUS
gckOS_ReadRegister(
    IN gckOS Os,
    IN gctUINT32 Address,
    OUT gctUINT32 * Data
    )
{
    return gckOS_ReadRegisterEx(Os, gcvCORE_MAJOR, Address, Data);
}

gceSTATUS
gckOS_ReadRegisterEx(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctUINT32 Address,
    OUT gctUINT32 * Data
    )
{
    gckKERNEL         kernel;
    gctINT32          clockStateValue = gcvFALSE;
#if MRVL_DFC_PROTECT_REG_ACCESS
    unsigned long flags;
#endif
    gcmkHEADER_ARG("Os=0x%X Core=%d Address=0x%X", Os, Core, Address);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
#if !gcdMULTI_GPU
    gcmkVERIFY_ARGUMENT(Address < Os->device->requestedRegisterMemSizes[Core]);
#endif
    gcmkVERIFY_ARGUMENT(Data != gcvNULL);

#if !MRVL_ENABLE_GC_POWER_CLOCK
    if (!in_interrupt())
    {
        mutex_lock(&Os->registerAccessLocks[Core]);
    }

    BUG_ON(!_AllowAccess(Os, Core, Address));
#endif

    kernel = Os->device->kernels[Core];

    down_read(&Os->rwsem_clk_pwr);

    if(kernel && kernel->hardware)
    {
        /* make sure both external and internal clock are ON. */
        gckOS_AtomGet(Os, kernel->hardware->clockState, &clockStateValue);

        if ((Os->clockDepth != 0 && clockStateValue == gcvTRUE)
           )
        {
            trace_gc_reg_acc(TRACE_LOG_ENTRY, TRACE_REG_READ, Address, 0);
#if MRVL_DFC_PROTECT_REG_ACCESS
            if (Core == gcvCORE_MAJOR)
            {
                get_gc3d_reg_lock(gcvTRUE, &flags);
            }
            else if (Core == gcvCORE_2D)
            {
                get_gc2d_reg_lock(gcvTRUE, &flags);
            }
#endif

#if gcdMULTI_GPU
            if (Core == gcvCORE_MAJOR)
            {
               *Data = readl((gctUINT8 *)Os->device->registerBase3D[gcvCORE_3D_0_ID] + Address);
             }
            else
#endif
            {
                *Data = readl((gctUINT8 *)Os->device->registerBases[Core] + Address);
            }

#if MRVL_DFC_PROTECT_REG_ACCESS
            if (Core == gcvCORE_MAJOR)
            {
                get_gc3d_reg_lock(gcvFALSE, &flags);
            }
            else if (Core == gcvCORE_2D)
            {
                get_gc2d_reg_lock(gcvFALSE, &flags);
            }
#endif
            trace_gc_reg_acc(TRACE_LOG_EXIT, TRACE_REG_READ, Address, *Data);
        }
        else
        {
            *Data = 0;
            gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_OS, "GPU%d_REG[%#x] (%d, %d) reading failure!",
                Core, Address, Os->clockDepth, clockStateValue);
        }
    }
    else
    {
        trace_gc_reg_acc(TRACE_LOG_ENTRY, TRACE_REG_READ, Address, 0);
#if gcdMULTI_GPU
        if (Core == gcvCORE_MAJOR)
        {
           *Data = readl((gctUINT8 *)Os->device->registerBase3D[gcvCORE_3D_0_ID] + Address);
         }
        else
#endif
        {
            *Data = readl((gctUINT8 *)Os->device->registerBases[Core] + Address);
        }
        trace_gc_reg_acc(TRACE_LOG_EXIT, TRACE_REG_READ, Address, *Data);
    }

    up_read(&Os->rwsem_clk_pwr);

#if !MRVL_ENABLE_GC_POWER_CLOCK
    if (!in_interrupt())
    {
        mutex_unlock(&Os->registerAccessLocks[Core]);
    }
#endif

    /* Success. */
    gcmkFOOTER_ARG("*Data=0x%08x", *Data);
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_DirectReadRegister(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctUINT32 Address,
    OUT gctUINT32 * Data
    )
{
#if MRVL_DFC_PROTECT_REG_ACCESS
    unsigned long flags;
#endif
    gcmkHEADER_ARG("Os=0x%X, Address=0x%X", Os, Address);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Data != gcvNULL);

#if !MRVL_ENABLE_GC_POWER_CLOCK
    if (!in_interrupt())
    {
        mutex_lock(&Os->registerAccessLocks[Core]);
    }

    BUG_ON(!_AllowAccess(Os, Core, Address));
#endif

#if MRVL_DFC_PROTECT_REG_ACCESS
    if (Core == gcvCORE_MAJOR)
    {
        get_gc3d_reg_lock(gcvTRUE, &flags);
    }
    else if (Core == gcvCORE_2D)
    {
        get_gc2d_reg_lock(gcvTRUE, &flags);
    }
#endif

    /* read register directly */
#if gcdMULTI_GPU
    if (Core == gcvCORE_MAJOR)
    {
        *Data = readl((gctUINT8 *)Os->device->registerBase3D[gcvCORE_3D_0_ID] + Address);
     }
    else
#endif
    {
        *Data = readl((gctUINT8 *)Os->device->registerBases[Core] + Address);
    }

#if MRVL_DFC_PROTECT_REG_ACCESS
    if (Core == gcvCORE_MAJOR)
    {
        get_gc3d_reg_lock(gcvFALSE, &flags);
    }
    else if (Core == gcvCORE_2D)
    {
        get_gc2d_reg_lock(gcvFALSE, &flags);
    }
#endif

#if !MRVL_ENABLE_GC_POWER_CLOCK
    if (!in_interrupt())
    {
        mutex_unlock(&Os->registerAccessLocks[Core]);
    }
#endif

    /* Success. */
    gcmkFOOTER_ARG("*Data=0x%08x", *Data);
    return gcvSTATUS_OK;
}

#if gcdMULTI_GPU
gceSTATUS
gckOS_ReadRegisterByCoreId(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctUINT32 CoreId,
    IN gctUINT32 Address,
    OUT gctUINT32 * Data
    )
{
    gckKERNEL       kernel;
    gctINT32        clockStateValue = gcvFALSE;
#if MRVL_DFC_PROTECT_REG_ACCESS
    unsigned long   flags;
#endif

    gcmkHEADER_ARG("Os=0x%X Core=%d CoreId=%d Address=0x%X",
                   Os, Core, CoreId, Address);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Data != gcvNULL);

    if (Core != gcvCORE_MAJOR)
    {
        *Data = 0;
        gcmkFOOTER_ARG("*Data=0x%08x", *Data);
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    kernel = Os->device->kernels[Core];

    down_read(&Os->rwsem_clk_pwr);

    if(kernel && kernel->hardware)
    {
        /* make sure both external and internal clock are ON. */
        gckOS_AtomGet(Os, kernel->hardware->clockState, &clockStateValue);

        if ((Os->clockDepth != 0) && (clockStateValue == gcvTRUE))
        {
            trace_gc_reg_acc(TRACE_LOG_ENTRY, TRACE_REG_READ, Address, 0);
#if MRVL_DFC_PROTECT_REG_ACCESS
            get_gc3d_reg_lock(gcvTRUE, &flags);
#endif
            *Data = readl((gctUINT8 *)Os->device->registerBase3D[CoreId] + Address);

#if MRVL_DFC_PROTECT_REG_ACCESS
            get_gc3d_reg_lock(gcvFALSE, &flags);
#endif
            trace_gc_reg_acc(TRACE_LOG_EXIT, TRACE_REG_READ, Address, *Data);
        }
        else
        {
            *Data = 0;
            gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_OS, "GPU%d[%d]_REG[%#x] (%d, %d) reading failure!",
                Core, CoreId, Address, Os->clockDepth, clockStateValue);
        }
    }
    else
    {
        trace_gc_reg_acc(TRACE_LOG_ENTRY, TRACE_REG_READ, Address, 0);
       *Data = readl((gctUINT8 *)Os->device->registerBase3D[CoreId] + Address);
        trace_gc_reg_acc(TRACE_LOG_EXIT, TRACE_REG_READ, Address, *Data);
    }

    up_read(&Os->rwsem_clk_pwr);

    /* Success. */
    gcmkFOOTER_ARG("*Data=0x%08x", *Data);
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_DirectReadRegisterByCoreId(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctUINT32 CoreId,
    IN gctUINT32 Address,
    OUT gctUINT32 * Data
    )
{
#if MRVL_DFC_PROTECT_REG_ACCESS
    unsigned long   flags;
#endif
    gcmkHEADER_ARG("Os=0x%X Core=%d CoreId=%d Address=0x%X",
                   Os, Core, CoreId, Address);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Data != gcvNULL);

#if MRVL_DFC_PROTECT_REG_ACCESS
    get_gc3d_reg_lock(gcvTRUE, &flags);
#endif

    *Data = readl((gctUINT8 *)Os->device->registerBase3D[CoreId] + Address);

#if MRVL_DFC_PROTECT_REG_ACCESS
    get_gc3d_reg_lock(gcvFALSE, &flags);
#endif

    /* Success. */
    gcmkFOOTER_ARG("*Data=0x%08x", *Data);
    return gcvSTATUS_OK;
}
#endif

/*******************************************************************************
**
**  gckOS_WriteRegister
**
**  Write data to a register.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctUINT32 Address
**          Address of register.
**
**      gctUINT32 Data
**          Data for register.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_WriteRegister(
    IN gckOS Os,
    IN gctUINT32 Address,
    IN gctUINT32 Data
    )
{
    return gckOS_WriteRegisterEx(Os, gcvCORE_MAJOR, Address, Data);
}

gceSTATUS
gckOS_WriteRegisterEx(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctUINT32 Address,
    IN gctUINT32 Data
    )
{
    gckKERNEL         kernel;
    gctINT32          clockStateValue = gcvFALSE;
    gceSTATUS         status = gcvSTATUS_OK;
#if MRVL_DFC_PROTECT_REG_ACCESS
    unsigned long flags;
#endif

    gcmkHEADER_ARG("Os=0x%X Core=%d Address=0x%X Data=0x%08x", Os, Core, Address, Data);

#if !gcdMULTI_GPU
    gcmkVERIFY_ARGUMENT(Address < Os->device->requestedRegisterMemSizes[Core]);
#endif

    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

#if !MRVL_ENABLE_GC_POWER_CLOCK
    if (!in_interrupt())
    {
        mutex_lock(&Os->registerAccessLocks[Core]);
    }

    BUG_ON(!_AllowAccess(Os, Core, Address));
#endif

    kernel = Os->device->kernels[Core];

    down_write(&Os->rwsem_clk_pwr);

    if(kernel && kernel->hardware)
    {
        /* make sure both external and internal clock are ON.
            Exceptions:
            * REG[ AQHiClockControlRegAddrs ]
        */
        gckOS_AtomGet(Os, kernel->hardware->clockState, &clockStateValue);

        if (Address == 0x0000 ||
            (Os->clockDepth != 0 && clockStateValue == gcvTRUE)
           )
        {
            trace_gc_reg_acc(TRACE_LOG_ENTRY, TRACE_REG_WRITE, Address, Data);
#if MRVL_DFC_PROTECT_REG_ACCESS
            if (Core == gcvCORE_MAJOR)
            {
                get_gc3d_reg_lock(gcvTRUE, &flags);
            }
            else if (Core == gcvCORE_2D)
            {
                get_gc2d_reg_lock(gcvTRUE, &flags);
            }
#endif

#if gcdMULTI_GPU
            if (Core == gcvCORE_MAJOR)
            {
                writel(Data, (gctUINT8 *)Os->device->registerBase3D[gcvCORE_3D_0_ID] + Address);
#if gcdMULTI_GPU > 1
                writel(Data, (gctUINT8 *)Os->device->registerBase3D[gcvCORE_3D_1_ID] + Address);
#endif
            }
            else
#endif
            {
                writel(Data, (gctUINT8 *)Os->device->registerBases[Core] + Address);
            }

#if MRVL_DFC_PROTECT_REG_ACCESS
            if (Core == gcvCORE_MAJOR)
            {
                get_gc3d_reg_lock(gcvFALSE, &flags);
            }
            else if (Core == gcvCORE_2D)
            {
                get_gc2d_reg_lock(gcvFALSE, &flags);
            }
#endif
            trace_gc_reg_acc(TRACE_LOG_EXIT, TRACE_REG_WRITE, Address, 0);
        }
        else
        {
            /* For registers 1)GC_PULSE_EATER_Address, 2)GC_PULSE_EATER_DEBUG0_Address,
            ** 3)GC_PULSE_EATER_DEBUG1_Address, this is a known issue. And return a specified status
            ** for error handle.
            */
            if ((Address != 0x0010C)
            &&  (Address != 0x00110)
            &&  (Address != 0x00114)
            )
            {
                gcmkPRINT("GPU%d_REG[%#x] (%d, %d) writing data(0x%08x) failure!",
                           Core, Address, Os->clockDepth, clockStateValue, Data);
            }
            else
            {
                status = gcvSTATUS_CHIP_NOT_READY;
            }
        }
    }
    else
    {
        trace_gc_reg_acc(TRACE_LOG_ENTRY, TRACE_REG_WRITE, Address, Data);
#if gcdMULTI_GPU
        if (Core == gcvCORE_MAJOR)
        {
            writel(Data, (gctUINT8 *)Os->device->registerBase3D[gcvCORE_3D_0_ID] + Address);
#if gcdMULTI_GPU > 1
            writel(Data, (gctUINT8 *)Os->device->registerBase3D[gcvCORE_3D_1_ID] + Address);
#endif
         }
        else
#endif
        {
            writel(Data, (gctUINT8 *)Os->device->registerBases[Core] + Address);
        }
        trace_gc_reg_acc(TRACE_LOG_EXIT, TRACE_REG_WRITE, Address, 0);
    }

    up_write(&Os->rwsem_clk_pwr);

#if !MRVL_ENABLE_GC_POWER_CLOCK
    if (!in_interrupt())
    {
        mutex_unlock(&Os->registerAccessLocks[Core]);
    }
#endif

    /* Success. */
    gcmkFOOTER_NO();
    return status;
}

gceSTATUS
gckOS_DirectWriteRegister(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctUINT32 Address,
    IN gctUINT32 Data
    )
{
#if MRVL_DFC_PROTECT_REG_ACCESS
    unsigned long flags;
#endif

    gcmkHEADER_ARG("Os=0x%X Core=%d Address=0x%X Data=0x%08x", Os, Core, Address, Data);

#if !gcdMULTI_GPU
    gcmkVERIFY_ARGUMENT(Address < Os->device->requestedRegisterMemSizes[Core]);
#endif

    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

#if !MRVL_ENABLE_GC_POWER_CLOCK
    if (!in_interrupt())
    {
        mutex_lock(&Os->registerAccessLocks[Core]);
    }

    BUG_ON(!_AllowAccess(Os, Core, Address));
#endif

#if MRVL_DFC_PROTECT_REG_ACCESS
    if (Core == gcvCORE_MAJOR)
    {
        get_gc3d_reg_lock(gcvTRUE, &flags);
    }
    else if (Core == gcvCORE_2D)
    {
        get_gc2d_reg_lock(gcvTRUE, &flags);
    }
#endif

#if gcdMULTI_GPU
    if (Core == gcvCORE_MAJOR)
    {
        writel(Data, (gctUINT8 *)Os->device->registerBase3D[gcvCORE_3D_0_ID] + Address);
#if gcdMULTI_GPU > 1
        writel(Data, (gctUINT8 *)Os->device->registerBase3D[gcvCORE_3D_1_ID] + Address);
#endif
    }
    else
#endif
    {
        writel(Data, (gctUINT8 *)Os->device->registerBases[Core] + Address);
    }

#if MRVL_DFC_PROTECT_REG_ACCESS
    if (Core == gcvCORE_MAJOR)
    {
        get_gc3d_reg_lock(gcvFALSE, &flags);
    }
    else if (Core == gcvCORE_2D)
    {
        get_gc2d_reg_lock(gcvFALSE, &flags);
    }
#endif

#if !MRVL_ENABLE_GC_POWER_CLOCK
    if (!in_interrupt())
    {
        mutex_unlock(&Os->registerAccessLocks[Core]);
    }
#endif

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

#if gcdMULTI_GPU
gceSTATUS
gckOS_WriteRegisterByCoreId(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctUINT32 CoreId,
    IN gctUINT32 Address,
    IN gctUINT32 Data
    )
{
    gcmkHEADER_ARG("Os=0x%X Core=%d CoreId=%d Address=0x%X Data=0x%08x",
                   Os, Core, CoreId, Address, Data);

    trace_gc_reg_acc(TRACE_LOG_ENTRY, TRACE_REG_WRITE, Address, Data);
    writel(Data, (gctUINT8 *)Os->device->registerBase3D[CoreId] + Address);
    trace_gc_reg_acc(TRACE_LOG_EXIT, TRACE_REG_WRITE, Address, 0);

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}
#endif

/*******************************************************************************
**
**  gckOS_GetPageSize
**
**  Get the system's page size.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**  OUTPUT:
**
**      gctSIZE_T * PageSize
**          Pointer to a variable that will receive the system's page size.
*/
gceSTATUS gckOS_GetPageSize(
    IN gckOS Os,
    OUT gctSIZE_T * PageSize
    )
{
    gcmkHEADER_ARG("Os=0x%X", Os);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(PageSize != gcvNULL);

    /* Return the page size. */
    *PageSize = (gctSIZE_T) PAGE_SIZE;

    /* Success. */
    gcmkFOOTER_ARG("*PageSize", *PageSize);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_GetPhysicalAddressProcess
**
**  Get the physical system address of a corresponding virtual address for a
**  given process.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to gckOS object.
**
**      gctPOINTER Logical
**          Logical address.
**
**      gctUINT32 ProcessID
**          Process ID.
**
**  OUTPUT:
**
**      gctUINT32 * Address
**          Poinetr to a variable that receives the 32-bit physical adress.
*/
gceSTATUS
_GetPhysicalAddressProcess(
    IN gckOS Os,
    IN gctPOINTER Logical,
    IN gctUINT32 ProcessID,
    OUT gctPHYS_ADDR_T * Address
    )
{
    PLINUX_MDL mdl;
    gctINT8_PTR base;
    gckALLOCATOR allocator = gcvNULL;
    gceSTATUS status = gcvSTATUS_INVALID_ADDRESS;

    gcmkHEADER_ARG("Os=0x%X Logical=0x%X ProcessID=%d", Os, Logical, ProcessID);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Address != gcvNULL);

    MEMORY_LOCK(Os);

    /* First try the contiguous memory pool. */
    if (Os->device->contiguousMapped)
    {
        base = (gctINT8_PTR) Os->device->contiguousBase;

        if (((gctINT8_PTR) Logical >= base)
        &&  ((gctINT8_PTR) Logical <  base + Os->device->contiguousSize)
        )
        {
            /* Convert logical address into physical. */
            *Address = Os->device->contiguousVidMem->baseAddress
                     + (gctINT8_PTR) Logical - base;
            status   = gcvSTATUS_OK;
        }
    }
    else
    {
        /* Try the contiguous memory pool. */
        mdl = (PLINUX_MDL) Os->device->contiguousPhysical;
        status = _ConvertLogical2Physical(Os,
                                          Logical,
                                          ProcessID,
                                          mdl,
                                          Address);
    }

    if (gcmIS_ERROR(status))
    {
        /* Walk all MDLs. */
        for (mdl = Os->mdlHead; mdl != gcvNULL; mdl = mdl->next)
        {
            /* Try this MDL. */
            allocator = mdl->allocator;

            if (allocator)
            {
                status = allocator->ops->LogicalToPhysical(
                            allocator,
                            mdl,
                            Logical,
                            ProcessID,
                            Address
                            );
            }
            else
            {
                status = _ConvertLogical2Physical(Os,
                            Logical,
                            ProcessID,
                            mdl,
                            Address);
            }

            if (gcmIS_SUCCESS(status))
            {
                break;
            }
        }
    }

    MEMORY_UNLOCK(Os);

    gcmkONERROR(status);

    /* Success. */
    gcmkFOOTER_ARG("*Address=0x%08x", *Address);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}



/*******************************************************************************
**
**  gckOS_GetPhysicalAddress
**
**  Get the physical system address of a corresponding virtual address.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPOINTER Logical
**          Logical address.
**
**  OUTPUT:
**
**      gctUINT32 * Address
**          Poinetr to a variable that receives the 32-bit physical adress.
*/
gceSTATUS
gckOS_GetPhysicalAddress(
    IN gckOS Os,
    IN gctPOINTER Logical,
    OUT gctPHYS_ADDR_T * Address
    )
{
    gceSTATUS status;
    gctUINT32 processID;

    gcmkHEADER_ARG("Os=0x%X Logical=0x%X", Os, Logical);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Address != gcvNULL);

    /* Query page table of current process first. */
    status = _QueryProcessPageTable(Logical, Address);

    if (gcmIS_ERROR(status))
    {
        /* Get current process ID. */
        processID = _GetProcessID();

        /* Route through other function. */
        gcmkONERROR(
            _GetPhysicalAddressProcess(Os, Logical, processID, Address));
    }

    gcmkVERIFY_OK(gckOS_CPUPhysicalToGPUPhysical(Os, *Address, Address));

    /* Success. */
    gcmkFOOTER_ARG("*Address=0x%08x", *Address);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_UserLogicalToPhysical
**
**  Get the physical system address of a corresponding user virtual address.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPOINTER Logical
**          Logical address.
**
**  OUTPUT:
**
**      gctUINT32 * Address
**          Pointer to a variable that receives the 32-bit physical address.
*/
gceSTATUS gckOS_UserLogicalToPhysical(
    IN gckOS Os,
    IN gctPOINTER Logical,
    OUT gctPHYS_ADDR_T * Address
    )
{
    return gckOS_GetPhysicalAddress(Os, Logical, Address);
}

#if gcdSECURE_USER
static gceSTATUS
gckOS_AddMapping(
    IN gckOS Os,
    IN gctUINT32 Physical,
    IN gctPOINTER Logical,
    IN gctSIZE_T Bytes
    )
{
    gceSTATUS status;
    gcsUSER_MAPPING_PTR map;

    gcmkHEADER_ARG("Os=0x%X Physical=0x%X Logical=0x%X Bytes=%lu",
                   Os, Physical, Logical, Bytes);

    gcmkONERROR(gckOS_Allocate(Os,
                               gcmSIZEOF(gcsUSER_MAPPING),
                               (gctPOINTER *) &map));

    map->next     = Os->userMap;
    map->physical = Physical - Os->device->baseAddress;
    map->logical  = Logical;
    map->bytes    = Bytes;
    map->start    = (gctINT8_PTR) Logical;
    map->end      = map->start + Bytes;

    Os->userMap = map;

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
}

static gceSTATUS
gckOS_RemoveMapping(
    IN gckOS Os,
    IN gctPOINTER Logical,
    IN gctSIZE_T Bytes
    )
{
    gceSTATUS status;
    gcsUSER_MAPPING_PTR map, prev;

    gcmkHEADER_ARG("Os=0x%X Logical=0x%X Bytes=%lu", Os, Logical, Bytes);

    for (map = Os->userMap, prev = gcvNULL; map != gcvNULL; map = map->next)
    {
        if ((map->logical == Logical)
        &&  (map->bytes   == Bytes)
        )
        {
            break;
        }

        prev = map;
    }

    if (map == gcvNULL)
    {
        gcmkONERROR(gcvSTATUS_INVALID_ADDRESS);
    }

    if (prev == gcvNULL)
    {
        Os->userMap = map->next;
    }
    else
    {
        prev->next = map->next;
    }

    gcmkONERROR(gcmkOS_SAFE_FREE(Os, map));

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
}
#endif

gceSTATUS
_ConvertLogical2Physical(
    IN gckOS Os,
    IN gctPOINTER Logical,
    IN gctUINT32 ProcessID,
    IN PLINUX_MDL Mdl,
    OUT gctPHYS_ADDR_T * Physical
    )
{
    gctINT8_PTR base, vBase;
    gctUINT32 offset;
    PLINUX_MDL_MAP map;
    gcsUSER_MAPPING_PTR userMap;

#if gcdSECURITY
    base = (Mdl == gcvNULL) ? gcvNULL : (gctINT8_PTR) Mdl->kaddr;
#else
    base = (Mdl == gcvNULL) ? gcvNULL : (gctINT8_PTR) Mdl->addr;
#endif

    /* Check for the logical address match. */
    if ((base != gcvNULL)
    &&  ((gctINT8_PTR) Logical >= base)
    &&  ((gctINT8_PTR) Logical <  base + Mdl->numPages * PAGE_SIZE)
    )
    {
        offset = (gctINT8_PTR) Logical - base;

        if (Mdl->dmaHandle != 0)
        {
            /* The memory was from coherent area. */
            *Physical = (gctUINT32) Mdl->dmaHandle + offset;
        }
        else if (Mdl->pagedMem && !Mdl->contiguous)
        {
            /* paged memory is not mapped to kernel space. */
            return gcvSTATUS_INVALID_ADDRESS;
        }
        else
        {
            *Physical = gcmPTR2INT32(virt_to_phys(base)) + offset;
        }

        return gcvSTATUS_OK;
    }

    /* Walk user maps. */
    for (userMap = Os->userMap; userMap != gcvNULL; userMap = userMap->next)
    {
        if (((gctINT8_PTR) Logical >= userMap->start)
        &&  ((gctINT8_PTR) Logical <  userMap->end)
        )
        {
            *Physical = userMap->physical
                      + (gctUINT32) ((gctINT8_PTR) Logical - userMap->start);

            return gcvSTATUS_OK;
        }
    }

    if (ProcessID != Os->kernelProcessID)
    {
        map   = FindMdlMap(Mdl, (gctINT) ProcessID);
        vBase = (map == gcvNULL) ? gcvNULL : (gctINT8_PTR) map->vmaAddr;

        /* Is the given address within that range. */
        if ((vBase != gcvNULL)
        &&  ((gctINT8_PTR) Logical >= vBase)
        &&  ((gctINT8_PTR) Logical <  vBase + Mdl->numPages * PAGE_SIZE)
        )
        {
            offset = (gctINT8_PTR) Logical - vBase;

            if (Mdl->dmaHandle != 0)
            {
                /* The memory was from coherent area. */
                *Physical = (gctUINT32) Mdl->dmaHandle + offset;
            }
            else if (Mdl->pagedMem && !Mdl->contiguous)
            {
                *Physical = _NonContiguousToPhys(Mdl->u.nonContiguousPages, offset/PAGE_SIZE) + (offset%PAGE_SIZE);
            }
            else
            {
                *Physical = page_to_phys(Mdl->u.contiguousPages) + offset;
            }

            return gcvSTATUS_OK;
        }
    }

    /* Address not yet found. */
    return gcvSTATUS_INVALID_ADDRESS;
}

/*******************************************************************************
**
**  gckOS_MapPhysical
**
**  Map a physical address into kernel space.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctUINT32 Physical
**          Physical address of the memory to map.
**
**      gctSIZE_T Bytes
**          Number of bytes to map.
**
**  OUTPUT:
**
**      gctPOINTER * Logical
**          Pointer to a variable that receives the base address of the mapped
**          memory.
*/
gceSTATUS
gckOS_MapPhysical(
    IN gckOS Os,
    IN gctUINT32 Physical,
    IN gctSIZE_T Bytes,
    OUT gctPOINTER * Logical
    )
{
    gctPOINTER logical;
    PLINUX_MDL mdl;
    gctUINT32 physical = Physical;

    gcmkHEADER_ARG("Os=0x%X Physical=0x%X Bytes=%lu", Os, Physical, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Bytes > 0);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);

    MEMORY_LOCK(Os);

    /* Go through our mapping to see if we know this physical address already. */
    mdl = Os->mdlHead;

    while (mdl != gcvNULL)
    {
        if (mdl->dmaHandle != 0)
        {
            if ((physical >= mdl->dmaHandle)
            &&  (physical < mdl->dmaHandle + mdl->numPages * PAGE_SIZE)
            )
            {
                *Logical = mdl->addr + (physical - mdl->dmaHandle);
                break;
            }
        }

        mdl = mdl->next;
    }

    MEMORY_UNLOCK(Os);

    if (mdl == gcvNULL)
    {
        struct page * page = pfn_to_page(physical >> PAGE_SHIFT);

        if (pfn_valid(page_to_pfn(page)))
        {
            gctUINT32 offset = physical & ~PAGE_MASK;
            struct page ** pages;
            gctUINT numPages;
            gctINT i;

            numPages = GetPageCount(PAGE_ALIGN(offset + Bytes), 0);

            pages = kmalloc(sizeof(struct page *) * numPages, GFP_KERNEL | gcdNOWARN);

            if (!pages)
            {
                gcmkFOOTER_ARG("status=%d", gcvSTATUS_OUT_OF_MEMORY);
                return gcvSTATUS_OUT_OF_MEMORY;
            }

            for (i = 0; i < numPages; i++)
            {
                pages[i] = nth_page(page, i);
            }

            logical = vmap(pages, numPages, 0, gcmkNONPAGED_MEMROY_PROT(PAGE_KERNEL));

            kfree(pages);

            if (logical == gcvNULL)
            {
                gcmkTRACE_ZONE(
                    gcvLEVEL_INFO, gcvZONE_OS,
                    "%s(%d): Failed to vmap",
                    __FUNCTION__, __LINE__
                    );

                /* Out of resources. */
                gcmkFOOTER_ARG("status=%d", gcvSTATUS_OUT_OF_RESOURCES);
                return gcvSTATUS_OUT_OF_RESOURCES;
            }

            logical += offset;
        }
        else
        {
            /* Map memory as cached memory. */
            request_mem_region(physical, Bytes, "MapRegion");
            logical = (gctPOINTER) ioremap_nocache(physical, Bytes);

            if (logical == gcvNULL)
            {
                gcmkTRACE_ZONE(
                    gcvLEVEL_INFO, gcvZONE_OS,
                    "%s(%d): Failed to ioremap",
                    __FUNCTION__, __LINE__
                    );

                /* Out of resources. */
                gcmkFOOTER_ARG("status=%d", gcvSTATUS_OUT_OF_RESOURCES);
                return gcvSTATUS_OUT_OF_RESOURCES;
            }
        }

        /* Return pointer to mapped memory. */
        *Logical = logical;
    }


    /* Success. */
    gcmkFOOTER_ARG("*Logical=0x%X", *Logical);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_UnmapPhysical
**
**  Unmap a previously mapped memory region from kernel memory.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPOINTER Logical
**          Pointer to the base address of the memory to unmap.
**
**      gctSIZE_T Bytes
**          Number of bytes to unmap.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_UnmapPhysical(
    IN gckOS Os,
    IN gctPOINTER Logical,
    IN gctSIZE_T Bytes
    )
{
    PLINUX_MDL  mdl;

    gcmkHEADER_ARG("Os=0x%X Logical=0x%X Bytes=%lu", Os, Logical, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Bytes > 0);

    MEMORY_LOCK(Os);

    mdl = Os->mdlHead;

    while (mdl != gcvNULL)
    {
        if (mdl->addr != gcvNULL)
        {
            if (Logical >= (gctPOINTER)mdl->addr
                    && Logical < (gctPOINTER)((gctSTRING)mdl->addr + mdl->numPages * PAGE_SIZE))
            {
                break;
            }
        }

        mdl = mdl->next;
    }

    if (mdl == gcvNULL)
    {
        /* Unmap the memory. */
        vunmap((void *)((unsigned long)Logical & PAGE_MASK));
    }

    MEMORY_UNLOCK(Os);

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_DeleteMutex
**
**  Delete a mutex.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPOINTER Mutex
**          Pointer to the mute to be deleted.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_DeleteMutex(
    IN gckOS Os,
    IN gctPOINTER Mutex
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%X Mutex=0x%X", Os, Mutex);

    /* Validate the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Mutex != gcvNULL);

    /* Destroy the mutex. */
    mutex_destroy((struct mutex *)Mutex);

    /* Free the mutex structure. */
    gcmkONERROR(gckOS_Free(Os, Mutex));

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_AcquireMutex
**
**  Acquire a mutex.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPOINTER Mutex
**          Pointer to the mutex to be acquired.
**
**      gctUINT32 Timeout
**          Timeout value specified in milliseconds.
**          Specify the value of gcvINFINITE to keep the thread suspended
**          until the mutex has been acquired.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_AcquireMutex(
    IN gckOS Os,
    IN gctPOINTER Mutex,
    IN gctUINT32 Timeout
    )
{
    gcmkHEADER_ARG("Os=0x%X Mutex=0x%0x Timeout=%u", Os, Mutex, Timeout);

    /* Validate the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Mutex != gcvNULL);

    if (Timeout == gcvINFINITE)
    {
        /* Lock the mutex. */
        mutex_lock(Mutex);

        /* Success. */
        gcmkFOOTER_NO();
        return gcvSTATUS_OK;
    }

    for (;;)
    {
        /* Try to acquire the mutex. */
        if (mutex_trylock(Mutex))
        {
            /* Success. */
            gcmkFOOTER_NO();
            return gcvSTATUS_OK;
        }

        if (Timeout-- == 0)
        {
            break;
        }

        /* Wait for 1 millisecond. */
        gcmkVERIFY_OK(gckOS_Delay(Os, 1));
    }

    /* Timeout. */
    gcmkFOOTER_ARG("status=%d", gcvSTATUS_TIMEOUT);
    return gcvSTATUS_TIMEOUT;
}

/*******************************************************************************
**
**  gckOS_ReleaseMutex
**
**  Release an acquired mutex.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPOINTER Mutex
**          Pointer to the mutex to be released.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_ReleaseMutex(
    IN gckOS Os,
    IN gctPOINTER Mutex
    )
{
    gcmkHEADER_ARG("Os=0x%X Mutex=0x%0x", Os, Mutex);

    /* Validate the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Mutex != gcvNULL);

    /* Release the mutex. */
    mutex_unlock(Mutex);

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_CreateRecMutex
**
**  Create a new recursive mutex.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**  OUTPUT:
**
**      gckRecursiveMutex * Mutex
**          Pointer to a variable that will hold a pointer to the mutex.
*/
gceSTATUS
gckOS_CreateRecMutex(
    IN gckOS Os,
    OUT gckRecursiveMutex * Mutex
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmkHEADER_ARG("Os=0x%X RecMutex=0x%0x", Os, Mutex);

    /* Validate the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Mutex != gcvNULL);

    /* Allocate a _gckRecursiveMutex structure. */
    *Mutex = (gctPOINTER)kmalloc(sizeof(struct _gckRecursiveMutex), GFP_KERNEL);
    if (*Mutex)
    {
        (*Mutex)->accMutex = (*Mutex)->undMutex = gcvNULL;
        gcmkONERROR(gckOS_CreateMutex(Os, &(*Mutex)->accMutex));
        gcmkONERROR(gckOS_CreateMutex(Os, &(*Mutex)->undMutex));
        (*Mutex)->nReference = 0;
        (*Mutex)->pThread = -1;
    }
    else
    {
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    /* Return status. */
    gcmkFOOTER_ARG("*Mutex=0x%X", *Mutex);
    return status;

OnError:
    if (*Mutex)
    {
        /* Free the underlying mutex. */
        if ((*Mutex)->accMutex)
            gckOS_DeleteMutex(Os, (*Mutex)->accMutex);
        if ((*Mutex)->undMutex)
            gckOS_DeleteMutex(Os, (*Mutex)->undMutex);
        /* free _gckRecursiveMutex structure. */
        kfree(*Mutex);
        *Mutex = gcvNULL;
    }

    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_DeleteRecMutex
**
**  Delete a recursive mutex.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gckRecursiveMutex Mutex
**          Pointer to the mute to be deleted.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gckOS_DeleteRecMutex(
    IN gckOS Os,
    IN gckRecursiveMutex Mutex
    )
{
    gcmkHEADER_ARG("Os=0x%X RecMutex=0x%0x", Os, Mutex);

    /* Validate the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Mutex != gcvNULL);

    /* Delete the underlying mutex. */
    if (Mutex->accMutex)
        gckOS_DeleteMutex(Os, Mutex->accMutex);
    if (Mutex->undMutex)
        gckOS_DeleteMutex(Os, Mutex->undMutex);

    /* Delete _gckRecursiveMutex structure. */
    kfree(Mutex);
    Mutex = gcvNULL;

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_AcquireRecMutex
**
**  Acquire a recursive mutex.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gckRecursiveMutex Mutex
**          Pointer to the mutex to be acquired.
**
**      gctUINT32 Timeout
**          Timeout value specified in milliseconds.
**          Specify the value of gcvINFINITE to keep the thread suspended
**          until the mutex has been acquired.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_AcquireRecMutex(
    IN gckOS Os,
    IN gckRecursiveMutex Mutex,
    IN gctUINT32 Timeout
    )
{
    gceSTATUS status = gcvSTATUS_TIMEOUT;
    gctUINT32 tid;

    gcmkHEADER_ARG("Os=0x%X RecMutex=0x%0x", Os, Mutex);

    /* Validate the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Mutex != gcvNULL);

    gckOS_AcquireMutex(Os, Mutex->accMutex, gcvINFINITE);
    /* Get current thread ID. */
    gckOS_GetThreadID(&tid);
    /* Locked by itself. */
    if (Mutex->pThread == tid)
    {
        Mutex->nReference++;
        gckOS_ReleaseMutex(Os, Mutex->accMutex);
        status = gcvSTATUS_OK;
    }
    else
    {
        gckOS_ReleaseMutex(Os, Mutex->accMutex);
        /* Try lock. */
        status = gckOS_AcquireMutex(Os, Mutex->undMutex, Timeout);
        /* First time get the lock . */
        if (status == gcvSTATUS_OK)
        {
            gckOS_AcquireMutex(Os, Mutex->accMutex, gcvINFINITE);
            Mutex->pThread = tid;
            Mutex->nReference = 1;
            gckOS_ReleaseMutex(Os, Mutex->accMutex);
        }

    }

    /* Timeout. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_ReleaseRecMutex
**
**  Release an acquired mutex.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gckRecursiveMutex Mutex
**          Pointer to the mutex to be released.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gckOS_ReleaseRecMutex(
    IN gckOS Os,
    IN gckRecursiveMutex Mutex
    )
{
    gctUINT32 tid;

    gcmkHEADER_ARG("Os=0x%X RecMutex=0x%0x", Os, Mutex);

    /* Validate the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Mutex != gcvNULL);

    gckOS_AcquireMutex(Os, Mutex->accMutex, gcvINFINITE);

    /* Get current thread ID. */
    gckOS_GetThreadID(&tid);
    /* Locked by itself. */
    if (Mutex->pThread == tid)
    {
        Mutex->nReference--;
        if(Mutex->nReference == 0)
        {
            Mutex->pThread = -1;
            /* Unlock. */
            gckOS_ReleaseMutex(Os, Mutex->undMutex);
        }
    }

    gckOS_ReleaseMutex(Os, Mutex->accMutex);
    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_AcquireClockMutex(
    IN gckOS Os,
    IN gceCORE Core
)
{
    gckKERNEL kernel;

    gcmkHEADER_ARG("Os=0x%X Core=0x%0x", Os, Core);

    if (Os && Os->device)
    {
        kernel = Os->device->kernels[Core];

        if (kernel && kernel->hardware)
        {
            gckOS_AcquireMutex(Os, kernel->hardware->clockMutex, gcvINFINITE);
        }
    }

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_ReleaseClockMutex(
    IN gckOS Os,
    IN gceCORE Core
)
{
    gckKERNEL kernel;

    gcmkHEADER_ARG("Os=0x%X Core=0x%0x", Os, Core);

    if (Os && Os->device)
    {
        kernel = Os->device->kernels[Core];

        if (kernel && kernel->hardware)
        {
            gckOS_ReleaseMutex(Os, kernel->hardware->clockMutex);
        }
    }

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_AtomicExchange
**
**  Atomically exchange a pair of 32-bit values.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      IN OUT gctINT32_PTR Target
**          Pointer to the 32-bit value to exchange.
**
**      IN gctINT32 NewValue
**          Specifies a new value for the 32-bit value pointed to by Target.
**
**      OUT gctINT32_PTR OldValue
**          The old value of the 32-bit value pointed to by Target.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_AtomicExchange(
    IN gckOS Os,
    IN OUT gctUINT32_PTR Target,
    IN gctUINT32 NewValue,
    OUT gctUINT32_PTR OldValue
    )
{
    gctUINT32 oldValue = 0;
    gcmkHEADER_ARG("Os=0x%X Target=0x%X NewValue=%u", Os, Target, NewValue);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(OldValue != gcvNULL);

    /* Exchange the pair of 32-bit values. */
    oldValue = (gctUINT32) atomic_xchg((atomic_t *) Target, (int) NewValue);
    if (OldValue)
    {
        *OldValue = oldValue;
    }

    /* Success. */
    gcmkFOOTER_ARG("*OldValue=%u", oldValue);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_AtomicExchangePtr
**
**  Atomically exchange a pair of pointers.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      IN OUT gctPOINTER * Target
**          Pointer to the 32-bit value to exchange.
**
**      IN gctPOINTER NewValue
**          Specifies a new value for the pointer pointed to by Target.
**
**      OUT gctPOINTER * OldValue
**          The old value of the pointer pointed to by Target.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_AtomicExchangePtr(
    IN gckOS Os,
    IN OUT gctPOINTER * Target,
    IN gctPOINTER NewValue,
    OUT gctPOINTER * OldValue
    )
{
    gctPOINTER oldValue = gcvNULL;
    gcmkHEADER_ARG("Os=0x%X Target=0x%X NewValue=0x%X", Os, Target, NewValue);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(OldValue != gcvNULL);

    /* Exchange the pair of pointers. */
    oldValue = (gctPOINTER)(gctUINTPTR_T) atomic_xchg((atomic_t *) Target, (int)(gctUINTPTR_T) NewValue);
    if (OldValue)
    {
        *OldValue = oldValue;
    }

    /* Success. */
    gcmkFOOTER_ARG("*OldValue=0x%X", oldValue);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_AtomicSetMask
**
**  Atomically set mask to Atom
**
**  INPUT:
**      IN OUT gctPOINTER Atom
**          Pointer to the atom to set.
**
**      IN gctUINT32 Mask
**          Mask to set.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_AtomSetMask(
    IN gctPOINTER Atom,
    IN gctUINT32 Mask
    )
{
    gctUINT32 oval, nval;

    gcmkHEADER_ARG("Atom=0x%0x", Atom);
    gcmkVERIFY_ARGUMENT(Atom != gcvNULL);

    do
    {
        oval = atomic_read((atomic_t *) Atom);
        nval = oval | Mask;
    } while (atomic_cmpxchg((atomic_t *) Atom, oval, nval) != oval);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_AtomClearMask
**
**  Atomically clear mask from Atom
**
**  INPUT:
**      IN OUT gctPOINTER Atom
**          Pointer to the atom to clear.
**
**      IN gctUINT32 Mask
**          Mask to clear.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_AtomClearMask(
    IN gctPOINTER Atom,
    IN gctUINT32 Mask
    )
{
    gctUINT32 oval, nval;

    gcmkHEADER_ARG("Atom=0x%0x", Atom);
    gcmkVERIFY_ARGUMENT(Atom != gcvNULL);

    do
    {
        oval = atomic_read((atomic_t *) Atom);
        nval = oval & ~Mask;
    } while (atomic_cmpxchg((atomic_t *) Atom, oval, nval) != oval);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_AtomConstruct
**
**  Create an atom.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to a gckOS object.
**
**  OUTPUT:
**
**      gctPOINTER * Atom
**          Pointer to a variable receiving the constructed atom.
*/
gceSTATUS
gckOS_AtomConstruct(
    IN gckOS Os,
    OUT gctPOINTER * Atom
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%X", Os);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Atom != gcvNULL);

    /* Allocate the atom. */
    gcmkONERROR(gckOS_Allocate(Os, gcmSIZEOF(atomic_t), Atom));

    /* Initialize the atom. */
    atomic_set((atomic_t *) *Atom, 0);

    /* Success. */
    gcmkFOOTER_ARG("*Atom=0x%X", *Atom);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_AtomDestroy
**
**  Destroy an atom.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to a gckOS object.
**
**      gctPOINTER Atom
**          Pointer to the atom to destroy.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_AtomDestroy(
    IN gckOS Os,
    OUT gctPOINTER Atom
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%X Atom=0x%0x", Os, Atom);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Atom != gcvNULL);

    /* Free the atom. */
    gcmkONERROR(gcmkOS_SAFE_FREE(Os, Atom));

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_AtomGet
**
**  Get the 32-bit value protected by an atom.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to a gckOS object.
**
**      gctPOINTER Atom
**          Pointer to the atom.
**
**  OUTPUT:
**
**      gctINT32_PTR Value
**          Pointer to a variable the receives the value of the atom.
*/
gceSTATUS
gckOS_AtomGet(
    IN gckOS Os,
    IN gctPOINTER Atom,
    OUT gctINT32_PTR Value
    )
{
    gcmkHEADER_ARG("Os=0x%X Atom=0x%0x", Os, Atom);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Atom != gcvNULL);

    /* Return the current value of atom. */
    *Value = atomic_read((atomic_t *) Atom);

    /* Success. */
    gcmkFOOTER_ARG("*Value=%d", *Value);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_AtomSet
**
**  Set the 32-bit value protected by an atom.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to a gckOS object.
**
**      gctPOINTER Atom
**          Pointer to the atom.
**
**      gctINT32 Value
**          The value of the atom.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_AtomSet(
    IN gckOS Os,
    IN gctPOINTER Atom,
    IN gctINT32 Value
    )
{
    gcmkHEADER_ARG("Os=0x%X Atom=0x%0x Value=%d", Os, Atom);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Atom != gcvNULL);

    /* Set the current value of atom. */
    atomic_set((atomic_t *) Atom, Value);

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_AtomIncrement
**
**  Atomically increment the 32-bit integer value inside an atom.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to a gckOS object.
**
**      gctPOINTER Atom
**          Pointer to the atom.
**
**  OUTPUT:
**
**      gctINT32_PTR Value
**          Pointer to a variable that receives the original value of the atom.
*/
gceSTATUS
gckOS_AtomIncrement(
    IN gckOS Os,
    IN gctPOINTER Atom,
    OUT gctINT32_PTR Value
    )
{
    gcmkHEADER_ARG("Os=0x%X Atom=0x%0x", Os, Atom);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Atom != gcvNULL);

    /* Increment the atom. */
    *Value = atomic_inc_return((atomic_t *) Atom) - 1;

    /* Success. */
    gcmkFOOTER_ARG("*Value=%d", *Value);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_AtomDecrement
**
**  Atomically decrement the 32-bit integer value inside an atom.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to a gckOS object.
**
**      gctPOINTER Atom
**          Pointer to the atom.
**
**  OUTPUT:
**
**      gctINT32_PTR Value
**          Pointer to a variable that receives the original value of the atom.
*/
gceSTATUS
gckOS_AtomDecrement(
    IN gckOS Os,
    IN gctPOINTER Atom,
    OUT gctINT32_PTR Value
    )
{
    gcmkHEADER_ARG("Os=0x%X Atom=0x%0x", Os, Atom);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Atom != gcvNULL);

    /* Decrement the atom. */
    *Value = atomic_dec_return((atomic_t *) Atom) + 1;

    /* Success. */
    gcmkFOOTER_ARG("*Value=%d", *Value);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_Delay
**
**  Delay execution of the current thread for a number of milliseconds.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctUINT32 Delay
**          Delay to sleep, specified in milliseconds.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_Delay(
    IN gckOS Os,
    IN gctUINT32 Delay
    )
{
    gcmkHEADER_ARG("Os=0x%X Delay=%u", Os, Delay);

    if (Delay > 0)
    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28)
        ktime_t delay = ktime_set((Delay / MSEC_PER_SEC), (Delay % MSEC_PER_SEC) * NSEC_PER_MSEC);
        __set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_hrtimeout(&delay, HRTIMER_MODE_REL);
#else
        msleep(Delay);
#endif
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_Udelay
**
**  Delay execution of the current thread for a number of microseconds.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctUINT32 Delay
**          Delay to sleep, specified in microseconds.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_Udelay(
    IN gckOS Os,
    IN gctUINT32 Delay
    )
{
    gcmkHEADER_ARG("Os=0x%X Delay=%u", Os, Delay);

    if (Delay > 0)
    {
        udelay(Delay);
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_GetTicks
**
**  Get the number of milliseconds since the system started.
**
**  INPUT:
**
**  OUTPUT:
**
**      gctUINT32_PTR Time
**          Pointer to a variable to get time.
**
*/
gceSTATUS
gckOS_GetTicks(
    OUT gctUINT32_PTR Time
    )
{
     gcmkHEADER();

    *Time = jiffies_to_msecs(jiffies);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_TicksAfter
**
**  Compare time values got from gckOS_GetTicks.
**
**  INPUT:
**      gctUINT32 Time1
**          First time value to be compared.
**
**      gctUINT32 Time2
**          Second time value to be compared.
**
**  OUTPUT:
**
**      gctBOOL_PTR IsAfter
**          Pointer to a variable to result.
**
*/
gceSTATUS
gckOS_TicksAfter(
    IN gctUINT32 Time1,
    IN gctUINT32 Time2,
    OUT gctBOOL_PTR IsAfter
    )
{
    gcmkHEADER();

    *IsAfter = time_after((unsigned long)Time1, (unsigned long)Time2);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_GetTime
**
**  Get the number of microseconds since the system started.
**
**  INPUT:
**
**  OUTPUT:
**
**      gctUINT64_PTR Time
**          Pointer to a variable to get time.
**
*/
gceSTATUS
gckOS_GetTime(
    OUT gctUINT64_PTR Time
    )
{
    struct timeval tv;
    gcmkHEADER();

    /* Return the time of day in microseconds. */
    do_gettimeofday(&tv);
    *Time = (tv.tv_sec * 1000000ULL) + tv.tv_usec;

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_MemoryBarrier
**
**  Make sure the CPU has executed everything up to this point and the data got
**  written to the specified pointer.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPOINTER Address
**          Address of memory that needs to be barriered.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_MemoryBarrier(
    IN gckOS Os,
    IN gctPOINTER Address
    )
{
    gcmkHEADER_ARG("Os=0x%X Address=0x%X", Os, Address);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

#if gcdNONPAGED_MEMORY_BUFFERABLE \
    && defined (CONFIG_ARM) \
    && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34))
    /* drain write buffer */
    dsb();

    /* drain outer cache's write buffer? */
#else
    mb();
#endif

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_AllocatePagedMemory
**
**  Allocate memory from the paged pool.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctSIZE_T Bytes
**          Number of bytes to allocate.
**
**  OUTPUT:
**
**      gctPHYS_ADDR * Physical
**          Pointer to a variable that receives the physical address of the
**          memory allocation.
*/
gceSTATUS
gckOS_AllocatePagedMemory(
    IN gckOS Os,
    IN gctSIZE_T Bytes,
    OUT gctPHYS_ADDR * Physical
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%X Bytes=%lu", Os, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Bytes > 0);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);

    /* Allocate the memory. */
    gcmkONERROR(gckOS_AllocatePagedMemoryEx(Os, gcvALLOC_FLAG_NONE, Bytes, gcvNULL, Physical));

    /* Success. */
    gcmkFOOTER_ARG("*Physical=0x%X", *Physical);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_AllocatePagedMemoryEx
**
**  Allocate memory from the paged pool.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctUINT32 Flag
**          Allocation attribute.
**
**      gctSIZE_T Bytes
**          Number of bytes to allocate.
**
**  OUTPUT:
**
**      gctUINT32 * Gid
**          Save the global ID for the piece of allocated memory.
**
**      gctPHYS_ADDR * Physical
**          Pointer to a variable that receives the physical address of the
**          memory allocation.
*/
gceSTATUS
gckOS_AllocatePagedMemoryEx(
    IN gckOS Os,
    IN gctUINT32 Flag,
    IN gctSIZE_T Bytes,
    OUT gctUINT32 * Gid,
    OUT gctPHYS_ADDR * Physical
    )
{
    gctINT numPages;
    PLINUX_MDL mdl = gcvNULL;
    gctSIZE_T bytes;
    gceSTATUS status = gcvSTATUS_OUT_OF_MEMORY;
    gckALLOCATOR allocator;

    gcmkHEADER_ARG("Os=0x%X Flag=%x Bytes=%lu", Os, Flag, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Bytes > 0);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);

    bytes = gcmALIGN(Bytes, PAGE_SIZE);

    numPages = GetPageCount(bytes, 0);

    mdl = _CreateMdl();
    if (mdl == gcvNULL)
    {
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    /* Walk all allocators. */
    list_for_each_entry(allocator, &Os->allocatorList, head)
    {
        gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_OS,
                       "%s(%d) flag = %x allocator->capability = %x",
                        __FUNCTION__, __LINE__, Flag, allocator->capability);

        if ((Flag & allocator->capability) != Flag)
        {
            continue;
        }

        status = allocator->ops->Alloc(allocator, mdl, numPages, Flag);

        if (gcmIS_SUCCESS(status))
        {
            mdl->allocator = allocator;
            break;
        }
    }

    /* Check status. */
    if(gcmIS_ERROR(status))
    {
        goto OnError;
    }

    mdl->dmaHandle  = 0;
    mdl->addr       = 0;
    mdl->numPages   = numPages;
    mdl->pagedMem   = 1;
    mdl->contiguous = Flag & gcvALLOC_FLAG_CONTIGUOUS;
    mdl->wastSize   = bytes - Bytes;

    if (Gid != gcvNULL)
    {
        *Gid = mdl->gid;
    }

    MEMORY_LOCK(Os);

    /*
     * Add this to a global list.
     * Will be used by get physical address
     * and mapuser pointer functions.
     */
    if (!Os->mdlHead)
    {
        /* Initialize the queue. */
        Os->mdlHead = Os->mdlTail = mdl;
    }
    else
    {
        /* Add to tail. */
        mdl->prev           = Os->mdlTail;
        Os->mdlTail->next   = mdl;
        Os->mdlTail         = mdl;
    }

    if(Flag & gcvALLOC_FLAG_CONTIGUOUS)
    {
        Os->device->contiguousPagedMemUsage += bytes;
    }
    else
    {
        Os->device->virtualPagedMemUsage += bytes;
    }
    Os->device->wastBytes += mdl->wastSize;

    MEMORY_UNLOCK(Os);

    /* Return physical address. */
    *Physical = (gctPHYS_ADDR) mdl;

    /* Success. */
    gcmkFOOTER_ARG("*Physical=0x%X", *Physical);
    return gcvSTATUS_OK;

OnError:
    if (mdl != gcvNULL)
    {
        /* Free the memory. */
        _DestroyMdl(mdl);
    }

    /* Return the status. */
    gcmkFOOTER_ARG("Os=0x%X Flag=%x Bytes=%lu", Os, Flag, Bytes);
    return status;
}

/*******************************************************************************
**
**  gckOS_FreePagedMemory
**
**  Free memory allocated from the paged pool.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPHYS_ADDR Physical
**          Physical address of the allocation.
**
**      gctSIZE_T Bytes
**          Number of bytes of the allocation.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_FreePagedMemory(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T Bytes
    )
{
    PLINUX_MDL mdl = (PLINUX_MDL) Physical;
    gckALLOCATOR allocator = (gckALLOCATOR)mdl->allocator;

    gcmkHEADER_ARG("Os=0x%X Physical=0x%X Bytes=%lu", Os, Physical, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Bytes > 0);

    MEMORY_LOCK(Os);

    /* Remove the node from global list. */
    if (mdl == Os->mdlHead)
    {
        if ((Os->mdlHead = mdl->next) == gcvNULL)
        {
            Os->mdlTail = gcvNULL;
        }
    }
    else
    {
        mdl->prev->next = mdl->next;

        if (mdl == Os->mdlTail)
        {
            Os->mdlTail = mdl->prev;
        }
        else
        {
            mdl->next->prev = mdl->prev;
        }
    }

    if(mdl != gcvNULL)
    {
        if(mdl->contiguous)
        {
            Os->device->contiguousPagedMemUsage -= mdl->numPages * PAGE_SIZE;
        }
        else
        {
            Os->device->virtualPagedMemUsage -= mdl->numPages * PAGE_SIZE;
        }
        Os->device->wastBytes -= mdl->wastSize;
    }


    MEMORY_UNLOCK(Os);

    allocator->ops->Free(allocator, mdl);

    /* Free the structure... */
    gcmkVERIFY_OK(_DestroyMdl(mdl));

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_LockPages
**
**  Lock memory allocated from the paged pool.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPHYS_ADDR Physical
**          Physical address of the allocation.
**
**      gctSIZE_T Bytes
**          Number of bytes of the allocation.
**
**      gctBOOL Cacheable
**          Cache mode of mapping.
**
**  OUTPUT:
**
**      gctPOINTER * Logical
**          Pointer to a variable that receives the address of the mapped
**          memory.
**
**      gctSIZE_T * PageCount
**          Pointer to a variable that receives the number of pages required for
**          the page table according to the GPU page size.
*/
gceSTATUS
gckOS_LockPages(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T Bytes,
    IN gctBOOL Cacheable,
    OUT gctPOINTER * Logical,
    OUT gctSIZE_T * PageCount
    )
{
    gceSTATUS       status;
    PLINUX_MDL      mdl;
    PLINUX_MDL_MAP  mdlMap;
    gckALLOCATOR    allocator;

    gcmkHEADER_ARG("Os=0x%X Physical=0x%X Bytes=%lu", Os, Physical, Logical);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);
    gcmkVERIFY_ARGUMENT(PageCount != gcvNULL);

    mdl = (PLINUX_MDL) Physical;
    allocator = mdl->allocator;

    MEMORY_LOCK(Os);

    mdlMap = FindMdlMap(mdl, _GetProcessID());

    if (mdlMap == gcvNULL)
    {
        mdlMap = _CreateMdlMap(mdl, _GetProcessID());

        if (mdlMap == gcvNULL)
        {
            MEMORY_UNLOCK(Os);

            gcmkFOOTER_ARG("*status=%d", gcvSTATUS_OUT_OF_MEMORY);
            return gcvSTATUS_OUT_OF_MEMORY;
        }
    }

    if (mdlMap->vmaAddr == gcvNULL)
    {
        status = allocator->ops->MapUser(allocator, mdl, mdlMap, Cacheable);

        if (gcmIS_ERROR(status))
        {
            MEMORY_UNLOCK(Os);

            gcmkFOOTER_ARG("*status=%d", status);
            return status;
        }
    }

    mdlMap->count++;

    /* Convert pointer to MDL. */
    *Logical = mdlMap->vmaAddr;

    /* Return the page number according to the GPU page size. */
    gcmkASSERT((PAGE_SIZE % 4096) == 0);
    gcmkASSERT((PAGE_SIZE / 4096) >= 1);

    *PageCount = mdl->numPages * (PAGE_SIZE / 4096);

    MEMORY_UNLOCK(Os);

    gcmkVERIFY_OK(gckOS_CacheFlush(
        Os,
        _GetProcessID(),
        Physical,
        gcvINVALID_ADDRESS,
        (gctPOINTER)mdlMap->vmaAddr,
        mdl->numPages * PAGE_SIZE
        ));

    /* Success. */
    gcmkFOOTER_ARG("*Logical=0x%X *PageCount=%lu", *Logical, *PageCount);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_MapPages
**
**  Map paged memory into a page table.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPHYS_ADDR Physical
**          Physical address of the allocation.
**
**      gctSIZE_T PageCount
**          Number of pages required for the physical address.
**
**      gctPOINTER PageTable
**          Pointer to the page table to fill in.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_MapPages(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T PageCount,
    IN gctPOINTER PageTable
    )
{
    return gckOS_MapPagesEx(Os,
                            gcvCORE_MAJOR,
                            Physical,
                            PageCount,
                            0,
                            PageTable);
}

gceSTATUS
gckOS_MapPagesEx(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T PageCount,
    IN gctUINT32 Address,
    IN gctPOINTER PageTable
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    PLINUX_MDL  mdl;
    gctUINT32*  table;
    gctUINT32   offset;
#if gcdNONPAGED_MEMORY_CACHEABLE
    gckMMU      mmu;
    PLINUX_MDL  mmuMdl;
    gctUINT32   bytes;
    gctPHYS_ADDR pageTablePhysical;
#endif

#if gcdPROCESS_ADDRESS_SPACE
    gckKERNEL kernel = Os->device->kernels[Core];
    gckMMU      mmu;
#endif
    gckALLOCATOR allocator;

    gcmkHEADER_ARG("Os=0x%X Core=%d Physical=0x%X PageCount=%u PageTable=0x%X",
                   Os, Core, Physical, PageCount, PageTable);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);
    gcmkVERIFY_ARGUMENT(PageCount > 0);
    gcmkVERIFY_ARGUMENT(PageTable != gcvNULL);

    /* Convert pointer to MDL. */
    mdl = (PLINUX_MDL)Physical;

    allocator = mdl->allocator;

    gcmkTRACE_ZONE(
        gcvLEVEL_INFO, gcvZONE_OS,
        "%s(%d): Physical->0x%X PageCount->0x%X PagedMemory->?%d",
        __FUNCTION__, __LINE__,
        (gctUINT32)(gctUINTPTR_T)Physical,
        (gctUINT32)(gctUINTPTR_T)PageCount,
        mdl->pagedMem
        );

#if gcdPROCESS_ADDRESS_SPACE
    gcmkONERROR(gckKERNEL_GetProcessMMU(kernel, &mmu));
#endif

    table = (gctUINT32 *)PageTable;
#if gcdNONPAGED_MEMORY_CACHEABLE
    mmu = Os->device->kernels[Core]->mmu;
    bytes = PageCount * sizeof(*table);
    mmuMdl = (PLINUX_MDL)mmu->pageTablePhysical;
#endif

     /* Get all the physical addresses and store them in the page table. */

    offset = 0;
    PageCount = PageCount / (PAGE_SIZE / 4096);

    /* Try to get the user pages so DMA can happen. */
    while (PageCount-- > 0)
    {
        gctUINT i;
        gctPHYS_ADDR_T phys = ~0U;

        if (mdl->pagedMem && !mdl->contiguous)
        {
            allocator->ops->Physical(allocator, mdl, offset, &phys);
        }
        else
        {
            if (!mdl->pagedMem)
            {
                gcmkTRACE_ZONE(
                    gcvLEVEL_INFO, gcvZONE_OS,
                    "%s(%d): we should not get this call for Non Paged Memory!",
                    __FUNCTION__, __LINE__
                    );
            }

            phys = page_to_phys(nth_page(mdl->u.contiguousPages, offset));
        }

        gcmkVERIFY_OK(gckOS_CPUPhysicalToGPUPhysical(Os, phys, &phys));

#ifdef CONFIG_IOMMU_SUPPORT
        if (Os->iommu)
        {
            gcmkTRACE_ZONE(
                gcvLEVEL_INFO, gcvZONE_OS,
                "%s(%d): Setup mapping in IOMMU %x => %x",
                __FUNCTION__, __LINE__,
                Address + (offset * PAGE_SIZE), phys
                );

            /* When use IOMMU, GPU use system PAGE_SIZE. */
            gcmkONERROR(gckIOMMU_Map(
                Os->iommu, Address + (offset * PAGE_SIZE), phys, PAGE_SIZE));
        }
        else
#endif
        {

#if gcdENABLE_VG
            if (Core == gcvCORE_VG)
            {
                for (i = 0; i < (PAGE_SIZE / 4096); i++)
                {
                    gcmkONERROR(
                        gckVGMMU_SetPage(Os->device->kernels[Core]->vg->mmu,
                            phys + (i * 4096),
                            table++));
                }
            }
            else
#endif
            {
                for (i = 0; i < (PAGE_SIZE / 4096); i++)
                {
#if gcdPROCESS_ADDRESS_SPACE
                    gctUINT32_PTR pageTableEntry;
                    gckMMU_GetPageEntry(mmu, Address + (offset * 4096), &pageTableEntry);
                    gcmkONERROR(
                        gckMMU_SetPage(mmu,
                            phys + (i * 4096),
                            pageTableEntry));
#else
                    gcmkONERROR(
                        gckMMU_SetPage(Os->device->kernels[Core]->mmu,
                            phys + (i * 4096),
                            table++));
#endif
                }
            }
        }

        offset += 1;
    }

#if gcdNONPAGED_MEMORY_CACHEABLE
    /* Get physical address of pageTable */
    pageTablePhysical = (gctPHYS_ADDR)(mmuMdl->dmaHandle +
                        ((gctUINT32 *)PageTable - mmu->pageTableLogical));

    /* Flush the mmu page table cache. */
    gcmkONERROR(gckOS_CacheClean(
        Os,
        _GetProcessID(),
        gcvNULL,
        pageTablePhysical,
        PageTable,
        bytes
        ));
#endif

OnError:

    /* Return the status. */
    gcmkFOOTER();
    return status;
}

gceSTATUS
gckOS_UnmapPages(
    IN gckOS Os,
    IN gctSIZE_T PageCount,
    IN gctUINT32 Address
    )
{
#ifdef CONFIG_IOMMU_SUPPORT
    if (Os->iommu)
    {
        gcmkVERIFY_OK(gckIOMMU_Unmap(
            Os->iommu, Address, PageCount * PAGE_SIZE));
    }
#endif

    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_UnlockPages
**
**  Unlock memory allocated from the paged pool.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPHYS_ADDR Physical
**          Physical address of the allocation.
**
**      gctSIZE_T Bytes
**          Number of bytes of the allocation.
**
**      gctPOINTER Logical
**          Address of the mapped memory.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_UnlockPages(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T Bytes,
    IN gctPOINTER Logical
    )
{
    PLINUX_MDL_MAP          mdlMap;
    PLINUX_MDL              mdl = (PLINUX_MDL)Physical;
    gckALLOCATOR            allocator = mdl->allocator;

    gcmkHEADER_ARG("Os=0x%X Physical=0x%X Bytes=%u Logical=0x%X",
                   Os, Physical, Bytes, Logical);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);

    MEMORY_LOCK(Os);

    mdlMap = mdl->maps;

    while (mdlMap != gcvNULL)
    {
        if ((mdlMap->vmaAddr != gcvNULL) && (_GetProcessID() == mdlMap->pid))
        {
            if (--mdlMap->count == 0)
            {
                allocator->ops->UnmapUser(
                    allocator,
                    mdlMap->vmaAddr,
                    mdl->numPages * PAGE_SIZE);

                mdlMap->vmaAddr = gcvNULL;
            }
        }

        mdlMap = mdlMap->next;
    }

    MEMORY_UNLOCK(Os);

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_AllocateVidmemFromMemblock
**
**  @return the base physical address of video memory that was allocated by
**          using memblock from linux kernel
**
*/
#if MRVL_USE_GPU_RESERVE_MEM
gceSTATUS
gckOS_AllocateVidmemFromMemblock(
    IN gckOS Os,
    IN gctSIZE_T Bytes,
    IN gctPHYS_ADDR Base,
    OUT gctPHYS_ADDR * Physical
    )
{
    gctINT          numPages;
    PLINUX_MDL      mdl     = gcvNULL;
    gctSTRING       addr    = gcvNULL;
    gceSTATUS       status  = gcvSTATUS_OK;
    gctBOOL         locked  = gcvFALSE;

    gcmkHEADER_ARG("Os=0x%X Bytes=%d Base=0x%p", Os, Bytes, Base);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);

    gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_OS, "in %s", __func__);

    /* Get total number of pages.. */
    numPages = GetPageCount(Bytes, 0);

    /* Allocate mdl+vector structure */
    mdl = _CreateMdl();
    if (mdl == gcvNULL)
    {
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    mdl->pagedMem = 0;
    mdl->numPages = numPages;

    /* Fetch physical from GPU_PLATFORM_DATA directly. */
    mdl->dmaHandle  = (dma_addr_t)Base;
    addr            = phys_to_virt(mdl->dmaHandle);

    if (addr == gcvNULL)
    {
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    MEMORY_LOCK(Os);
    locked = gcvTRUE;

    mdl->kaddr = addr;
    mdl->addr = gcvNULL;
    mdl->wastSize  = 0;
    /* Return allocated memory. */
    *Physical = (gctPHYS_ADDR) mdl;

    /*
     * Add this to a global list.
     * Will be used by get physical address
     * and mapuser pointer functions.
     */

    if (!Os->mdlHead)
    {
        /* Initialize the queue. */
        Os->mdlHead = Os->mdlTail = mdl;
    }
    else
    {
        /* Add to the tail. */
        mdl->prev = Os->mdlTail;
        Os->mdlTail->next = mdl;
        Os->mdlTail = mdl;
    }

    MEMORY_UNLOCK(Os);
    locked = gcvFALSE;

    gcmkTRACE_ZONE(gcvLEVEL_INFO,
                gcvZONE_OS,
                "%s: Bytes->0x%x, Mdl->%p, Logical->0x%lx dmaHandle->0x%x",
                __func__,
                (gctUINT32)Bytes,
                mdl,
                mdl->addr,
                mdl->dmaHandle);

    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    if(locked)
    {
        MEMORY_UNLOCK(Os);
    }

    /* Destroy the mdl if necessary*/
    if(mdl)
    {
        gcmkVERIFY_OK(_DestroyMdl(mdl));
        mdl = gcvNULL;
    }

    return status;
}

/*******************************************************************************
**
**  gckOS_FreeVidmemFromMemblock
**
**  remove all nodes that was allocated from the reserved video memory.
**
*/
gceSTATUS
gckOS_FreeVidmemFromMemblock(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical
    )
{
    PLINUX_MDL      mdl     = gcvNULL;
    gceSTATUS       status  = gcvSTATUS_OK;

    gcmkHEADER_ARG("Os=0x%X Physical=0x%x", Os, Physical);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);

    gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_OS, "in %s", __func__);

    MEMORY_LOCK(Os);

    mdl = (PLINUX_MDL)Physical;

    /* Remove the node from global list.. */
    if (mdl == Os->mdlHead)
    {
        if ((Os->mdlHead = mdl->next) == gcvNULL)
        {
            Os->mdlTail = gcvNULL;
        }
    }
    else
    {
        mdl->prev->next = mdl->next;
        if (mdl == Os->mdlTail)
        {
            Os->mdlTail = mdl->prev;
        }
        else
        {
            mdl->next->prev = mdl->prev;
        }
    }

    MEMORY_UNLOCK(Os);

    gcmkVERIFY_OK(_DestroyMdl(mdl));
    mdl = gcvNULL;

    gcmkFOOTER();
    return status;
}
#endif


/*******************************************************************************
**
**  gckOS_AllocateContiguous
**
**  Allocate memory from the contiguous pool.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctBOOL InUserSpace
**          gcvTRUE if the pages need to be mapped into user space.
**
**      gctSIZE_T * Bytes
**          Pointer to the number of bytes to allocate.
**
**  OUTPUT:
**
**      gctSIZE_T * Bytes
**          Pointer to a variable that receives the number of bytes allocated.
**
**      gctPHYS_ADDR * Physical
**          Pointer to a variable that receives the physical address of the
**          memory allocation.
**
**      gctPOINTER * Logical
**          Pointer to a variable that receives the logical address of the
**          memory allocation.
*/
gceSTATUS
gckOS_AllocateContiguous(
    IN gckOS Os,
    IN gctBOOL InUserSpace,
    IN OUT gctSIZE_T * Bytes,
    OUT gctPHYS_ADDR * Physical,
    OUT gctPOINTER * Logical
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%X InUserSpace=%d *Bytes=%lu",
                   Os, InUserSpace, gcmOPT_VALUE(Bytes));

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Bytes != gcvNULL);
    gcmkVERIFY_ARGUMENT(*Bytes > 0);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);

    /* Same as non-paged memory for now. */
    gcmkONERROR(gckOS_AllocateNonPagedMemory(Os,
                                             InUserSpace,
                                             Bytes,
                                             Physical,
                                             Logical));

    /* Success. */
    gcmkFOOTER_ARG("*Bytes=%lu *Physical=0x%X *Logical=0x%X",
                   *Bytes, *Physical, *Logical);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_FreeContiguous
**
**  Free memory allocated from the contiguous pool.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPHYS_ADDR Physical
**          Physical address of the allocation.
**
**      gctPOINTER Logical
**          Logicval address of the allocation.
**
**      gctSIZE_T Bytes
**          Number of bytes of the allocation.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_FreeContiguous(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical,
    IN gctPOINTER Logical,
    IN gctSIZE_T Bytes
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%X Physical=0x%X Logical=0x%X Bytes=%lu",
                   Os, Physical, Logical, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Bytes > 0);

    /* Same of non-paged memory for now. */
    gcmkONERROR(gckOS_FreeNonPagedMemory(Os, Bytes, Physical, Logical));

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

#if gcdENABLE_VG
/******************************************************************************
**
**  gckOS_GetKernelLogical
**
**  Return the kernel logical pointer that corresponods to the specified
**  hardware address.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctUINT32 Address
**          Hardware physical address.
**
**  OUTPUT:
**
**      gctPOINTER * KernelPointer
**          Pointer to a variable receiving the pointer in kernel address space.
*/
gceSTATUS
gckOS_GetKernelLogical(
    IN gckOS Os,
    IN gctUINT32 Address,
    OUT gctPOINTER * KernelPointer
    )
{
    return gckOS_GetKernelLogicalEx(Os, gcvCORE_MAJOR, Address, KernelPointer);
}

gceSTATUS
gckOS_GetKernelLogicalEx(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctUINT32 Address,
    OUT gctPOINTER * KernelPointer
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%X Core=%d Address=0x%08x", Os, Core, Address);

    do
    {
        gckGALDEVICE device;
        gckKERNEL kernel;
        gcePOOL pool;
        gctUINT32 offset;
        gctPOINTER logical;

        /* Extract the pointer to the gckGALDEVICE class. */
        device = (gckGALDEVICE) Os->device;

        /* Kernel shortcut. */
        kernel = device->kernels[Core];
#if gcdENABLE_VG
       if (Core == gcvCORE_VG)
       {
           gcmkERR_BREAK(gckVGHARDWARE_SplitMemory(
                kernel->vg->hardware, Address, &pool, &offset
                ));
       }
       else
#endif
       {
        /* Split the memory address into a pool type and offset. */
            gcmkERR_BREAK(gckHARDWARE_SplitMemory(
                kernel->hardware, Address, &pool, &offset
                ));
       }

        /* Dispatch on pool. */
        switch (pool)
        {
        case gcvPOOL_LOCAL_INTERNAL:
            /* Internal memory. */
            logical = device->internalLogical;
            break;

        case gcvPOOL_LOCAL_EXTERNAL:
            /* External memory. */
            logical = device->externalLogical;
            break;

        case gcvPOOL_SYSTEM:
            /* System memory. */
            logical = device->contiguousBase;
            break;

        default:
            /* Invalid memory pool. */
            gcmkFOOTER();
            return gcvSTATUS_INVALID_ARGUMENT;
        }

        /* Build logical address of specified address. */
        * KernelPointer = ((gctUINT8_PTR) logical) + offset;

        /* Success. */
        gcmkFOOTER_ARG("*KernelPointer=0x%X", *KernelPointer);
        return gcvSTATUS_OK;
    }
    while (gcvFALSE);

    /* Return status. */
    gcmkFOOTER();
    return status;
}
#endif

/*******************************************************************************
**
**  gckOS_MapUserPointer
**
**  Map a pointer from the user process into the kernel address space.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPOINTER Pointer
**          Pointer in user process space that needs to be mapped.
**
**      gctSIZE_T Size
**          Number of bytes that need to be mapped.
**
**  OUTPUT:
**
**      gctPOINTER * KernelPointer
**          Pointer to a variable receiving the mapped pointer in kernel address
**          space.
*/
gceSTATUS
gckOS_MapUserPointer(
    IN gckOS Os,
    IN gctPOINTER Pointer,
    IN gctSIZE_T Size,
    OUT gctPOINTER * KernelPointer
    )
{
    gctPOINTER buf = gcvNULL;
    gctUINT32 len;

    gcmkHEADER_ARG("Os=0x%X Pointer=0x%X Size=%lu", Os, Pointer, Size);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Pointer != gcvNULL);
    gcmkVERIFY_ARGUMENT(Size > 0);
    gcmkVERIFY_ARGUMENT(KernelPointer != gcvNULL);

    buf = kmalloc(Size, GFP_KERNEL | gcdNOWARN);
    if (buf == gcvNULL)
    {
        gcmkTRACE(
            gcvLEVEL_ERROR,
            "%s(%d): Failed to allocate memory.",
            __FUNCTION__, __LINE__
            );

        gcmkFOOTER_ARG("*status=%d", gcvSTATUS_OUT_OF_MEMORY);
        return gcvSTATUS_OUT_OF_MEMORY;
    }

    len = copy_from_user(buf, Pointer, Size);
    if (len != 0)
    {
        gcmkTRACE(
            gcvLEVEL_ERROR,
            "%s(%d): Failed to copy data from user.",
            __FUNCTION__, __LINE__
            );

        if (buf != gcvNULL)
        {
            kfree(buf);
        }

        gcmkFOOTER_ARG("*status=%d", gcvSTATUS_GENERIC_IO);
        return gcvSTATUS_GENERIC_IO;
    }

    *KernelPointer = buf;

    gcmkFOOTER_ARG("*KernelPointer=0x%X", *KernelPointer);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_UpdateVidMemUsage
**
**  Update video memory usage.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to the gckOS object.
**
**      gctBOOL IsAllocated
**          Flag whether allocate or free.
**
**      gctSIZE_T Bytes
**          Bytes to allocate or free.
**
**      gceSURF_TYPE Type
**          The type of gcoSURF object.
**
**  OUTPUT:
**
**      Nothing.
*/

gceSTATUS
gckOS_UpdateVidMemUsage(
    IN gckOS Os,
    IN gctBOOL IsAllocated,
    IN gctSIZE_T Bytes,
    IN gceSURF_TYPE Type
)
{
    gctINT32 factor = IsAllocated ? 1 : -1;

    gcmkHEADER_ARG("Os=0x%x,IsAllocated=%d,Bytes=%d,Type=%d", Os, IsAllocated, (gctUINT)Bytes, Type);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    MEMORY_LOCK(Os);

    /* Update video memory usage. */
    switch(Type)
    {
    case gcvSURF_NUM_TYPES:
        Os->device->vidMemUsage                     += (factor * Bytes);
        break;

    case gcvSURF_INDEX:
        Os->device->indexVidMemUsage                += (factor * Bytes);
        break;

    case gcvSURF_VERTEX:
        Os->device->vertexVidMemUsage               += (factor * Bytes);
        break;

    case gcvSURF_TEXTURE:
        Os->device->textureVidMemUsage              += (factor * Bytes);
        break;

    case gcvSURF_RENDER_TARGET:
        Os->device->renderTargetVidMemUsage         += (factor * Bytes);
        break;

    case gcvSURF_DEPTH:
        Os->device->depthVidMemUsage                += (factor * Bytes);
        break;

    case gcvSURF_BITMAP:
        Os->device->bitmapVidMemUsage               += (factor * Bytes);
        break;

    case gcvSURF_TILE_STATUS:
        Os->device->tileStatusVidMemUsage           += (factor * Bytes);
        break;

    case gcvSURF_IMAGE:
        Os->device->imageVidMemUsage                += (factor * Bytes);
        break;

    case gcvSURF_MASK:
        Os->device->maskVidMemUsage                 += (factor * Bytes);
        break;

    case gcvSURF_SCISSOR:
        Os->device->scissorVidMemUsage              += (factor * Bytes);
        break;

    case gcvSURF_HIERARCHICAL_DEPTH:
        Os->device->hierarchicalDepthVidMemUsage    += (factor * Bytes);
        break;
    default:
        Os->device->othersVidMemUsage               += (factor * Bytes);
        break;
    }

    MEMORY_UNLOCK(Os);

    gcmkFOOTER_NO();

    /* Success. */
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_ShowVidMemUsage(
    IN gckOS Os,
    IN gctPOINTER Buffer,
    OUT gctUINT32_PTR Length
)
{
    gctCHAR * buf = (gctCHAR *) Buffer;
    gckGALDEVICE device = gcvNULL;
    gctUINT32 sum = 0, len = 0;
    gctUINT32 GCsum = 0;

    gcmkHEADER_ARG("Os=0x%X", Os);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Buffer != gcvNULL);

    device = Os->device;

    sum += device->indexVidMemUsage
         + device->vertexVidMemUsage
         + device->textureVidMemUsage
         + device->renderTargetVidMemUsage
         + device->depthVidMemUsage
         + device->bitmapVidMemUsage
         + device->tileStatusVidMemUsage
         + device->imageVidMemUsage
         + device->maskVidMemUsage
         + device->scissorVidMemUsage
         + device->hierarchicalDepthVidMemUsage
         + device->othersVidMemUsage;
    GCsum +=  device->vidMemUsage
           +device->contiguousPagedMemUsage
           +device->virtualPagedMemUsage
           +device->contiguousNonPagedMemUsage;

    len += sprintf(buf+len, "Total reserved video mem:  %7ld KB\n"
                            "  - used video mem:        %7d KB\n"
                            "  - contiguousPaged:       %7d KB\n"
                            "  - virtualPaged:          %7d KB\n"
                            "  - contiguousNonPaged:    %7d KB\n"
                            "GC Memory Sum:             %7d KB\n"
                            "wastBytes:                 %7d KB\n"
                            "Video memory usage in details:\n"
                            "  - Index:                 %7d KB\n"
                            "  - Vertex:                %7d KB\n"
                            "  - Texture:               %7d KB\n"
                            "  - RenderTarget:          %7d KB\n"
                            "  - Depth:                 %7d KB\n"
                            "  - Bitmap:                %7d KB\n"
                            "  - TileStatus:            %7d KB\n"
                            "  - Image:                 %7d KB\n"
                            "  - Mask:                  %7d KB\n"
                            "  - Scissor:               %7d KB\n"
                            "  - HierarchicalDepth:     %7d KB\n"
                            "  - Others:                %7d KB\n"
                            "  - Sum:                   %7d KB\n",
                            (long int)device->reservedMem/1024,
                            device->vidMemUsage/1024,
                            device->contiguousPagedMemUsage/1024,
                            device->virtualPagedMemUsage/1024,
                            device->contiguousNonPagedMemUsage/1024,
                            GCsum/1024,
                            device->wastBytes/1024,
                            device->indexVidMemUsage/1024,
                            device->vertexVidMemUsage/1024,
                            device->textureVidMemUsage/1024,
                            device->renderTargetVidMemUsage/1024,
                            device->depthVidMemUsage/1024,
                            device->bitmapVidMemUsage/1024,
                            device->tileStatusVidMemUsage/1024,
                            device->imageVidMemUsage/1024,
                            device->maskVidMemUsage/1024,
                            device->scissorVidMemUsage/1024,
                            device->hierarchicalDepthVidMemUsage/1024,
                            device->othersVidMemUsage/1024,
                            sum/1024);

    *Length = len;

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}


/*******************************************************************************
**
**  gckOS_UnmapUserPointer
**
**  Unmap a user process pointer from the kernel address space.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPOINTER Pointer
**          Pointer in user process space that needs to be unmapped.
**
**      gctSIZE_T Size
**          Number of bytes that need to be unmapped.
**
**      gctPOINTER KernelPointer
**          Pointer in kernel address space that needs to be unmapped.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_UnmapUserPointer(
    IN gckOS Os,
    IN gctPOINTER Pointer,
    IN gctSIZE_T Size,
    IN gctPOINTER KernelPointer
    )
{
    gctUINT32 len;

    gcmkHEADER_ARG("Os=0x%X Pointer=0x%X Size=%lu KernelPointer=0x%X",
                   Os, Pointer, Size, KernelPointer);


    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Pointer != gcvNULL);
    gcmkVERIFY_ARGUMENT(Size > 0);
    gcmkVERIFY_ARGUMENT(KernelPointer != gcvNULL);

    len = copy_to_user(Pointer, KernelPointer, Size);

    kfree(KernelPointer);

    if (len != 0)
    {
        gcmkTRACE(
            gcvLEVEL_ERROR,
            "%s(%d): Failed to copy data to user.",
            __FUNCTION__, __LINE__
            );

        gcmkFOOTER_ARG("status=%d", gcvSTATUS_GENERIC_IO);
        return gcvSTATUS_GENERIC_IO;
    }

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_QueryNeedCopy
**
**  Query whether the memory can be accessed or mapped directly or it has to be
**  copied.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctUINT32 ProcessID
**          Process ID of the current process.
**
**  OUTPUT:
**
**      gctBOOL_PTR NeedCopy
**          Pointer to a boolean receiving gcvTRUE if the memory needs a copy or
**          gcvFALSE if the memory can be accessed or mapped dircetly.
*/
gceSTATUS
gckOS_QueryNeedCopy(
    IN gckOS Os,
    IN gctUINT32 ProcessID,
    OUT gctBOOL_PTR NeedCopy
    )
{
    gcmkHEADER_ARG("Os=0x%X ProcessID=%d", Os, ProcessID);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(NeedCopy != gcvNULL);

    /* We need to copy data. */
    *NeedCopy = gcvTRUE;

    /* Success. */
    gcmkFOOTER_ARG("*NeedCopy=%d", *NeedCopy);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_CopyFromUserData
**
**  Copy data from user to kernel memory.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPOINTER KernelPointer
**          Pointer to kernel memory.
**
**      gctPOINTER Pointer
**          Pointer to user memory.
**
**      gctSIZE_T Size
**          Number of bytes to copy.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_CopyFromUserData(
    IN gckOS Os,
    IN gctPOINTER KernelPointer,
    IN gctPOINTER Pointer,
    IN gctSIZE_T Size
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%X KernelPointer=0x%X Pointer=0x%X Size=%lu",
                   Os, KernelPointer, Pointer, Size);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(KernelPointer != gcvNULL);
    gcmkVERIFY_ARGUMENT(Pointer != gcvNULL);
    gcmkVERIFY_ARGUMENT(Size > 0);

    /* Copy data from user. */
    if (copy_from_user(KernelPointer, Pointer, Size) != 0)
    {
        /* Could not copy all the bytes. */
        gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_CopyToUserData
**
**  Copy data from kernel to user memory.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPOINTER KernelPointer
**          Pointer to kernel memory.
**
**      gctPOINTER Pointer
**          Pointer to user memory.
**
**      gctSIZE_T Size
**          Number of bytes to copy.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_CopyToUserData(
    IN gckOS Os,
    IN gctPOINTER KernelPointer,
    IN gctPOINTER Pointer,
    IN gctSIZE_T Size
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%X KernelPointer=0x%X Pointer=0x%X Size=%lu",
                   Os, KernelPointer, Pointer, Size);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(KernelPointer != gcvNULL);
    gcmkVERIFY_ARGUMENT(Pointer != gcvNULL);
    gcmkVERIFY_ARGUMENT(Size > 0);

    /* Copy data to user. */
    if (copy_to_user(Pointer, KernelPointer, Size) != 0)
    {
        /* Could not copy all the bytes. */
        gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_WriteMemory
**
**  Write data to a memory.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctPOINTER Address
**          Address of the memory to write to.
**
**      gctUINT32 Data
**          Data for register.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_WriteMemory(
    IN gckOS Os,
    IN gctPOINTER Address,
    IN gctUINT32 Data
    )
{
    gceSTATUS status;
    gcmkHEADER_ARG("Os=0x%X Address=0x%X Data=%u", Os, Address, Data);

    /* Verify the arguments. */
    gcmkVERIFY_ARGUMENT(Address != gcvNULL);

    /* Write memory. */
    if (access_ok(VERIFY_WRITE, Address, 4))
    {
        /* User address. */
        if(put_user(Data, (gctUINT32*)Address))
        {
            gcmkONERROR(gcvSTATUS_INVALID_ADDRESS);
        }
    }
    else
    {
        /* Kernel address. */
        *(gctUINT32 *)Address = Data;
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_MapUserMemory
**
**  Lock down a user buffer and return an DMA'able address to be used by the
**  hardware to access it.
**
**  INPUT:
**
**      gctPOINTER Memory
**          Pointer to memory to lock down.
**
**      gctSIZE_T Size
**          Size in bytes of the memory to lock down.
**
**  OUTPUT:
**
**      gctPOINTER * Info
**          Pointer to variable receiving the information record required by
**          gckOS_UnmapUserMemory.
**
**      gctUINT32_PTR Address
**          Pointer to a variable that will receive the address DMA'able by the
**          hardware.
*/
gceSTATUS
gckOS_MapUserMemory(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctPOINTER Memory,
    IN gctUINT32 Physical,
    IN gctSIZE_T Size,
    OUT gctPOINTER * Info,
    OUT gctUINT32_PTR Address
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%x Core=%d Memory=0x%x Size=%lu", Os, Core, Memory, Size);

#if gcdSECURE_USER
    gcmkONERROR(gckOS_AddMapping(Os, *Address, Memory, Size));

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
#else
{
    gctSIZE_T pageCount, i, j;
    gctUINT32_PTR pageTable;
    gctUINT32 address = 0, physical = ~0U;
    gctUINTPTR_T start, end, memory;
    gctUINT32 offset;
    gctINT result = 0;
#if gcdPROCESS_ADDRESS_SPACE
    gckMMU mmu;
#endif

    gcsPageInfo_PTR info = gcvNULL;
    struct page **pages = gcvNULL;

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Memory != gcvNULL || Physical != ~0U);
    gcmkVERIFY_ARGUMENT(Size > 0);
    gcmkVERIFY_ARGUMENT(Info != gcvNULL);
    gcmkVERIFY_ARGUMENT(Address != gcvNULL);

    do
    {
        gctSIZE_T extraPage;

        memory = (gctUINTPTR_T) Memory;

        /* Get the number of required pages. */
        end = (memory + Size + PAGE_SIZE - 1) >> PAGE_SHIFT;
        start = memory >> PAGE_SHIFT;
        pageCount = end - start;

        /* Allocate extra 64 bytes to avoid cache overflow */
        extraPage = (((memory + gcmALIGN(Size + 64, 64) + PAGE_SIZE - 1) >> PAGE_SHIFT) > end) ? 1 : 0;

        gcmkTRACE_ZONE(
            gcvLEVEL_INFO, gcvZONE_OS,
            "%s(%d): pageCount: %d.",
            __FUNCTION__, __LINE__,
            pageCount
            );

        /* Overflow. */
        if ((memory + Size) < memory)
        {
            gcmkFOOTER_ARG("status=%d", gcvSTATUS_INVALID_ARGUMENT);
            return gcvSTATUS_INVALID_ARGUMENT;
        }

        MEMORY_MAP_LOCK(Os);

        /* Allocate the Info struct. */
        info = (gcsPageInfo_PTR)kmalloc(sizeof(gcsPageInfo), GFP_KERNEL | gcdNOWARN);

        if (info == gcvNULL)
        {
            status = gcvSTATUS_OUT_OF_MEMORY;
            break;
        }

        info->extraPage = 0;

        /* Allocate the array of page addresses. */
        gcmkONERROR(gckOS_Allocate(
            Os, (pageCount + extraPage) * sizeof(struct page *), (gctPOINTER *) &pages
            ));

        if (Physical != ~0U)
        {
            for (i = 0; i < pageCount; i++)
            {
                pages[i] = pfn_to_page((Physical >> PAGE_SHIFT) + i);

                if (pfn_valid(page_to_pfn(pages[i])))
                {
                    get_page(pages[i]);
                }
            }
        }
        else
        {
            /* Get the user pages. */
            down_read(&current->mm->mmap_sem);

            result = get_user_pages(current,
                    current->mm,
                    memory & PAGE_MASK,
                    pageCount,
                    1,
                    0,
                    pages,
                    gcvNULL
                    );

            up_read(&current->mm->mmap_sem);
            info->pfnMap = gcvFALSE;

            if (result <=0 || result < pageCount)
            {
                struct vm_area_struct *vma;

                /* Release the pages if any. */
                if (result > 0)
                {
                    for (i = 0; i < result; i++)
                    {
                        if (pages[i] == gcvNULL)
                        {
                            break;
                        }

                        page_cache_release(pages[i]);
                        pages[i] = gcvNULL;
                    }

                    result = 0;
                }

                vma = find_vma(current->mm, memory);

                if (vma && (vma->vm_flags & VM_PFNMAP))
                {
                    gctUINTPTR_T logical = memory;

                    for(i = 0; i < pageCount; i++)
                    {
                        unsigned long pfn;
                        int ret = follow_pfn(vma, logical, &pfn);
                        if(ret)
                        {
                           gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
                        }
                        pages[i] = pfn_to_page(pfn);
                        logical += PAGE_SIZE;
                    }
                    info->pfnMap = gcvTRUE;
                }
                else
                {
                    gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
                }

                /* Check if this memory is contiguous for old mmu. */
                if (Os->device->kernels[Core]->hardware->mmuVersion == 0)
                {
                    for (i = 1; i < pageCount; i++)
                    {
                        if (pages[i] != nth_page(pages[0], i))
                        {
                            /* Non-contiguous. */
                            break;
                        }
                    }

                    if (i == pageCount)
                    {
                        /* Contiguous memory. */
                        physical = page_to_phys(pages[0]) | (memory & ~PAGE_MASK);

                        if (!((physical - Os->device->baseAddress) & 0x80000000))
                        {
                            gctPHYS_ADDR_T gpuPhysical;
                            gcmkVERIFY_OK(gckOS_Free(Os, pages));
                            pages = gcvNULL;

                            info->pages = gcvNULL;
                            info->pageTable = gcvNULL;

                            MEMORY_MAP_UNLOCK(Os);

                            *Address = physical - Os->device->baseAddress;
                            *Info    = info;

                            gcmkVERIFY_OK(
                                gckOS_CPUPhysicalToGPUPhysical(Os, *Address, &gpuPhysical));

                            gcmkSAFECASTPHYSADDRT(*Address, gpuPhysical);

                            gcmkFOOTER_ARG("*Info=0x%X *Address=0x%08x",
                                           *Info, *Address);

                            return gcvSTATUS_OK;
                        }
                    }
                }
            }
        }

        for (i = 0; i < pageCount; i++)
        {
#ifdef CONFIG_ARM
            gctUINT32 data;
            get_user(data, (gctUINT32*)((memory & PAGE_MASK) + i * PAGE_SIZE));
#endif
            /* Flush(clean) the data cache. */
            gcmkONERROR(gckOS_CacheFlush(Os, _GetProcessID(), gcvNULL,
                             page_to_phys(pages[i]),
                             (gctPOINTER)(memory & PAGE_MASK) + i*PAGE_SIZE,
                             PAGE_SIZE));
        }

#if gcdPROCESS_ADDRESS_SPACE
        gcmkONERROR(gckKERNEL_GetProcessMMU(Os->device->kernels[Core], &mmu));
#endif

        if (extraPage)
        {
            pages[pageCount++] = Os->paddingPage;
            info->extraPage = 1;
        }

#if gcdSECURITY
    {
        gctPHYS_ADDR physicalArrayPhysical;
        gctPOINTER physicalArrayLogical;
        gctUINT32_PTR logical;
        gctSIZE_T bytes = pageCount * gcmSIZEOF(gctUINT32);
        pageTable = gcvNULL;

        gcmkONERROR(gckOS_AllocateNonPagedMemory(
            Os,
            gcvFALSE,
            &bytes,
            &physicalArrayPhysical,
            &physicalArrayLogical
            ));

        logical = physicalArrayLogical;

        /* Fill the page table. */
        for (i = 0; i < pageCount; i++)
        {
            gctUINT32 phys;
            phys = page_to_phys(pages[i]);

            logical[i] = phys;
        }
        j = 0;


        gcmkONERROR(gckKERNEL_SecurityMapMemory(
            Os->device->kernels[Core],
            physicalArrayLogical,
            pageCount,
            &address
            ));

        gcmkONERROR(gckOS_FreeNonPagedMemory(
            Os,
            1,
            physicalArrayPhysical,
            physicalArrayLogical
            ));
    }

#else
#if gcdENABLE_VG
        if (Core == gcvCORE_VG)
        {
            /* Allocate pages inside the page table. */
            gcmkERR_BREAK(gckVGMMU_AllocatePages(Os->device->kernels[Core]->vg->mmu,
                                              pageCount * (PAGE_SIZE/4096),
                                              (gctPOINTER *) &pageTable,
                                              &address));
        }
        else
#endif
        {
#if gcdPROCESS_ADDRESS_SPACE
            /* Allocate pages inside the page table. */
            gcmkERR_BREAK(gckMMU_AllocatePages(mmu,
                                              pageCount * (PAGE_SIZE/4096),
                                              (gctPOINTER *) &pageTable,
                                              &address));
#else
            /* Allocate pages inside the page table. */
            gcmkERR_BREAK(gckMMU_AllocatePages(Os->device->kernels[Core]->mmu,
                                              pageCount * (PAGE_SIZE/4096),
                                              (gctPOINTER *) &pageTable,
                                              &address));
#endif
        }

        /* Fill the page table. */
        for (i = 0; i < pageCount; i++)
        {
            gctPHYS_ADDR_T phys;
            gctUINT32_PTR tab = pageTable + i * (PAGE_SIZE/4096);

#if gcdPROCESS_ADDRESS_SPACE
            gckMMU_GetPageEntry(mmu, address + i * 4096, &tab);
#endif
            phys = page_to_phys(pages[i]);

#ifdef CONFIG_IOMMU_SUPPORT
            if (Os->iommu)
            {
                gcmkTRACE_ZONE(
                    gcvLEVEL_INFO, gcvZONE_OS,
                    "%s(%d): Setup mapping in IOMMU %x => %x",
                    __FUNCTION__, __LINE__,
                    Address + (i * PAGE_SIZE), phys
                    );

                gcmkONERROR(gckIOMMU_Map(
                    Os->iommu, address + i * PAGE_SIZE, phys, PAGE_SIZE));
            }
            else
#endif
            {

#if gcdENABLE_VG
                if (Core == gcvCORE_VG)
                {
                    gcmkVERIFY_OK(
                        gckOS_CPUPhysicalToGPUPhysical(Os, phys, &phys));

                    /* Get the physical address from page struct. */
                    gcmkONERROR(
                        gckVGMMU_SetPage(Os->device->kernels[Core]->vg->mmu,
                                       phys,
                                       tab));
                }
                else
#endif
                {
                    /* Get the physical address from page struct. */
                    gcmkONERROR(
                        gckMMU_SetPage(Os->device->kernels[Core]->mmu,
                                       phys,
                                       tab));
                }

                for (j = 1; j < (PAGE_SIZE/4096); j++)
                {
                    pageTable[i * (PAGE_SIZE/4096) + j] = pageTable[i * (PAGE_SIZE/4096)] + 4096 * j;
                }
            }

#if !gcdPROCESS_ADDRESS_SPACE
            gcmkTRACE_ZONE(
                gcvLEVEL_INFO, gcvZONE_OS,
                "%s(%d): pageTable[%d]: 0x%X 0x%X.",
                __FUNCTION__, __LINE__,
                i, phys, pageTable[i]);
#endif
        }

#if gcdENABLE_VG
        if (Core == gcvCORE_VG)
        {
            gcmkONERROR(gckVGMMU_Flush(Os->device->kernels[Core]->vg->mmu));
        }
        else
#endif
        {
#if gcdPROCESS_ADDRESS_SPACE
            info->mmu = mmu;
            gcmkONERROR(gckMMU_Flush(mmu));
#else
            gcmkONERROR(gckMMU_Flush(Os->device->kernels[Core]->mmu, gcvSURF_TYPE_UNKNOWN));
#endif
        }
#endif
        info->address = address;

        /* Save pointer to page table. */
        info->pageTable = pageTable;
        info->pages = pages;

        *Info = (gctPOINTER) info;

        gcmkTRACE_ZONE(
            gcvLEVEL_INFO, gcvZONE_OS,
            "%s(%d): info->pages: 0x%X, info->pageTable: 0x%X, info: 0x%X.",
            __FUNCTION__, __LINE__,
            info->pages,
            info->pageTable,
            info
            );

        offset = (Physical != ~0U)
               ? (Physical & ~PAGE_MASK)
               : (memory & ~PAGE_MASK);

        /* Return address. */
        *Address = address + offset;

        gcmkTRACE_ZONE(
            gcvLEVEL_INFO, gcvZONE_OS,
            "%s(%d): Address: 0x%X.",
            __FUNCTION__, __LINE__,
            *Address
            );

        /* Success. */
        status = gcvSTATUS_OK;
    }
    while (gcvFALSE);

OnError:

    if (gcmIS_ERROR(status))
    {
        gcmkTRACE(
            gcvLEVEL_ERROR,
            "%s(%d): error occured: %d.",
            __FUNCTION__, __LINE__,
            status
            );

        /* Release page array. */
        if (result > 0 && pages != gcvNULL)
        {
            gcmkTRACE(
                gcvLEVEL_ERROR,
                "%s(%d): error: page table is freed.",
                __FUNCTION__, __LINE__
                );

            for (i = 0; i < result; i++)
            {
                if (pages[i] == gcvNULL)
                {
                    break;
                }
                page_cache_release(pages[i]);
            }
        }

        if (info!= gcvNULL && pages != gcvNULL)
        {
            gcmkTRACE(
                gcvLEVEL_ERROR,
                "%s(%d): error: pages is freed.",
                __FUNCTION__, __LINE__
                );

            /* Free the page table. */
            gckOS_Free(Os, pages);
            info->pages = gcvNULL;
        }

        /* Release page info struct. */
        if (info != gcvNULL)
        {
            gcmkTRACE(
                gcvLEVEL_ERROR,
                "%s(%d): error: info is freed.",
                __FUNCTION__, __LINE__
                );

            /* Free the page info struct. */
            kfree(info);
            *Info = gcvNULL;
        }
    }

    MEMORY_MAP_UNLOCK(Os);

    /* Return the status. */
    if (gcmIS_SUCCESS(status))
    {
        gcmkFOOTER_ARG("*Info=0x%X *Address=0x%08x", *Info, *Address);
    }
    else
    {
        gcmkFOOTER();
    }

    return status;
}
#endif
}

/*******************************************************************************
**
**  gckOS_UnmapUserMemory
**
**  Unlock a user buffer and that was previously locked down by
**  gckOS_MapUserMemory.
**
**  INPUT:
**
**      gctPOINTER Memory
**          Pointer to memory to unlock.
**
**      gctSIZE_T Size
**          Size in bytes of the memory to unlock.
**
**      gctPOINTER Info
**          Information record returned by gckOS_MapUserMemory.
**
**      gctUINT32_PTR Address
**          The address returned by gckOS_MapUserMemory.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_UnmapUserMemory(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctPOINTER Memory,
    IN gctSIZE_T Size,
    IN gctPOINTER Info,
    IN gctUINT32 Address
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%X Core=%d Memory=0x%X Size=%lu Info=0x%X Address0x%08x",
                   Os, Core, Memory, Size, Info, Address);

#if gcdSECURE_USER
    gcmkONERROR(gckOS_RemoveMapping(Os, Memory, Size));

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
#else
{
    gctUINTPTR_T memory, start, end;
    gcsPageInfo_PTR info;
    gctSIZE_T pageCount, i;
    struct page **pages;

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Memory != gcvNULL);
    gcmkVERIFY_ARGUMENT(Size > 0);
    gcmkVERIFY_ARGUMENT(Info != gcvNULL);

    do
    {
        info = (gcsPageInfo_PTR) Info;

        pages = info->pages;

        gcmkTRACE_ZONE(
            gcvLEVEL_INFO, gcvZONE_OS,
            "%s(%d): info=0x%X, pages=0x%X.",
            __FUNCTION__, __LINE__,
            info, pages
            );

        /* Invalid page array. */
        if (pages == gcvNULL && info->pageTable == gcvNULL)
        {
            kfree(info);

            gcmkFOOTER_NO();
            return gcvSTATUS_OK;
        }

        memory = (gctUINTPTR_T)Memory;
        end = (memory + Size + PAGE_SIZE - 1) >> PAGE_SHIFT;
        start = memory >> PAGE_SHIFT;
        pageCount = end - start;

        /* Overflow. */
        if ((memory + Size) < memory)
        {
            gcmkFOOTER_ARG("status=%d", gcvSTATUS_INVALID_ARGUMENT);
            return gcvSTATUS_INVALID_ARGUMENT;
        }

        gcmkTRACE_ZONE(
            gcvLEVEL_INFO, gcvZONE_OS,
            "%s(%d): memory: 0x%X, pageCount: %d, pageTable: 0x%X.",
            __FUNCTION__, __LINE__,
            memory, pageCount, info->pageTable
            );

        MEMORY_MAP_LOCK(Os);

#if !gcdSECURITY
        gcmkASSERT(info->pageTable != gcvNULL);
#endif

        if (info->extraPage)
        {
            pageCount += 1;
        }

#if gcdSECURITY
        if (info->address > 0x80000000)
        {
            gckKERNEL_SecurityUnmapMemory(
                Os->device->kernels[Core],
                info->address,
                pageCount
                );
        }
        else
        {
            gcmkPRINT("Wrong address %s(%d) %x", __FUNCTION__, __LINE__, info->address);
        }
#else
#if gcdENABLE_VG
        if (Core == gcvCORE_VG)
        {
            /* Free the pages from the MMU. */
            gcmkERR_BREAK(gckVGMMU_FreePages(Os->device->kernels[Core]->vg->mmu,
                                          info->pageTable,
                                          pageCount * (PAGE_SIZE/4096)
                                          ));
        }
        else
#endif
        {
            /* Free the pages from the MMU. */
#if gcdPROCESS_ADDRESS_SPACE
            gcmkERR_BREAK(gckMMU_FreePagesEx(info->mmu,
                info->address,
                pageCount * (PAGE_SIZE/4096)
                ));

#else
            gcmkERR_BREAK(gckMMU_FreePages(Os->device->kernels[Core]->mmu,
                                          info->pageTable,
                                          pageCount * (PAGE_SIZE/4096)
                                          ));
#endif

            gcmkERR_BREAK(gckOS_UnmapPages(
                Os,
                pageCount * (PAGE_SIZE/4096),
                info->address
                ));
        }
#endif

        if (info->extraPage)
        {
            pageCount -= 1;
            info->extraPage = 0;
        }

        /* Release the page cache. */
        if (pages && !info->pfnMap)
        {
            for (i = 0; i < pageCount; i++)
            {
                gcmkTRACE_ZONE(
                    gcvLEVEL_INFO, gcvZONE_OS,
                    "%s(%d): pages[%d]: 0x%X.",
                    __FUNCTION__, __LINE__,
                    i, pages[i]
                    );

                if (!PageReserved(pages[i]))
                {
                     SetPageDirty(pages[i]);
                }

                if (pfn_valid(page_to_pfn(pages[i])))
                {
                    page_cache_release(pages[i]);
                }
            }
        }

        /* Success. */
        status = gcvSTATUS_OK;
    }
    while (gcvFALSE);

    if (info != gcvNULL)
    {
        /* Free the page array. */
        if (info->pages != gcvNULL)
        {
            status = gckOS_Free(Os, (info->pages));
        }

        kfree(info);
    }

    MEMORY_MAP_UNLOCK(Os);

    /* Return the status. */
    gcmkFOOTER();
    return status;
}
#endif
}

/*******************************************************************************
**
**  gckOS_GetBaseAddress
**
**  Get the base address for the physical memory.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to the gckOS object.
**
**  OUTPUT:
**
**      gctUINT32_PTR BaseAddress
**          Pointer to a variable that will receive the base address.
*/
gceSTATUS
gckOS_GetBaseAddress(
    IN gckOS Os,
    OUT gctUINT32_PTR BaseAddress
    )
{
    gcmkHEADER_ARG("Os=0x%X", Os);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(BaseAddress != gcvNULL);

    /* Return base address. */
    *BaseAddress = Os->device->baseAddress;

    /* Success. */
    gcmkFOOTER_ARG("*BaseAddress=0x%08x", *BaseAddress);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_GetMmuVersion
**
**  Get mmu version.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to the gckOS object.
**
**  OUTPUT:
**
**      gctUINT32_PTR mmuVersion
**          Pointer to a variable that will receive the mmu version.
*/
gceSTATUS
gckOS_GetMmuVersion(
    IN gckOS Os,
    OUT gctUINT32_PTR mmuVersion
    )
{
    gcmkHEADER_ARG("Os=0x%X", Os);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(mmuVersion != gcvNULL);

    /* Return mmu version. */
    *mmuVersion = Os->device->kernels[gcvCORE_MAJOR]->hardware->mmuVersion ;

    /* Success. */
    gcmkFOOTER_ARG("*mmuVersion=0x%08x", *mmuVersion);
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_SuspendInterrupt(
    IN gckOS Os
    )
{
    return gckOS_SuspendInterruptEx(Os, gcvCORE_MAJOR);
}

#if gcdMULTI_GPU
gceSTATUS
gckOS_SuspendInterruptEx(
    IN gckOS Os,
    IN gceCORE Core
    )
{
    gcmkHEADER_ARG("Os=0x%X Core=%d", Os, Core);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    if (Core == gcvCORE_MAJOR)
    {
        disable_irq(Os->device->irqLine3D[gcvCORE_3D_0_ID]);
#if gcdMULTI_GPU > 1
        disable_irq(Os->device->irqLine3D[gcvCORE_3D_1_ID]);
#endif
    }
    else
    {
        disable_irq(Os->device->irqLines[Core]);
    }

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}
#else
gceSTATUS
gckOS_SuspendInterruptEx(
    IN gckOS Os,
    IN gceCORE Core
    )
{
    gcmkHEADER_ARG("Os=0x%X Core=%d", Os, Core);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    disable_irq(Os->device->irqLines[Core]);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}
#endif

gceSTATUS
gckOS_ResumeInterrupt(
    IN gckOS Os
    )
{
    return gckOS_ResumeInterruptEx(Os, gcvCORE_MAJOR);
}

#if gcdMULTI_GPU
gceSTATUS
gckOS_ResumeInterruptEx(
    IN gckOS Os,
    IN gceCORE Core
    )
{
    gcmkHEADER_ARG("Os=0x%X Core=%d", Os, Core);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    if (Core == gcvCORE_MAJOR)
    {
        enable_irq(Os->device->irqLine3D[gcvCORE_3D_0_ID]);
#if gcdMULTI_GPU > 1
        enable_irq(Os->device->irqLine3D[gcvCORE_3D_1_ID]);
#endif
    }
    else
    {
        enable_irq(Os->device->irqLines[Core]);
    }

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}
#else
gceSTATUS
gckOS_ResumeInterruptEx(
    IN gckOS Os,
    IN gceCORE Core
    )
{
    gcmkHEADER_ARG("Os=0x%X Core=%d", Os, Core);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    enable_irq(Os->device->irqLines[Core]);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}
#endif

gceSTATUS
gckOS_MemCopy(
    IN gctPOINTER Destination,
    IN gctCONST_POINTER Source,
    IN gctSIZE_T Bytes
    )
{
    gcmkHEADER_ARG("Destination=0x%X Source=0x%X Bytes=%lu",
                   Destination, Source, Bytes);

    gcmkVERIFY_ARGUMENT(Destination != gcvNULL);
    gcmkVERIFY_ARGUMENT(Source != gcvNULL);
    gcmkVERIFY_ARGUMENT(Bytes > 0);

    memcpy(Destination, Source, Bytes);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_ZeroMemory(
    IN gctPOINTER Memory,
    IN gctSIZE_T Bytes
    )
{
    gcmkHEADER_ARG("Memory=0x%X Bytes=%lu", Memory, Bytes);

    gcmkVERIFY_ARGUMENT(Memory != gcvNULL);
    gcmkVERIFY_ARGUMENT(Bytes > 0);

    memset(Memory, 0, Bytes);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
********************************* Cache Control ********************************
*******************************************************************************/

# if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
static gceSTATUS
_HandleCache(
    IN gckOS Os,
    IN gctUINT32 ProcessID,
    IN gctPHYS_ADDR Handle,
    IN gctUINT32 Physical,
    IN gctPOINTER Logical,
    IN gctSIZE_T Bytes,
    IN enum dma_data_direction Dir
    )
{
    gceSTATUS status;
    gctUINT32 i, pageNum;
    gctPHYS_ADDR_T paddr;
    gctPOINTER vaddr;
    gctBOOL locked = gcvFALSE;

    gcmkHEADER_ARG("Os=0x%X ProcessID=%d Handle=0x%X Logical=0x%X Bytes=%lu",
                   Os, ProcessID, Handle, Logical, Bytes);

    if (Physical != gcvINVALID_ADDRESS)
    {
        /* Non paged memory or gcvPOOL_USER surface */
        dma_sync_single_for_device(
                  gcvNULL,
                  (dma_addr_t)(gctUINTPTR_T)Physical,
                  Bytes,
                  Dir);
    }
    else if ((Handle == gcvNULL)
    || (Handle != gcvNULL && ((PLINUX_MDL)Handle)->contiguous)
    )
    {
        /* Video Memory or contiguous virtual memory */
        gcmkONERROR(gckOS_GetPhysicalAddress(Os, Logical, &paddr));
        dma_sync_single_for_device(
                  gcvNULL,
                  (dma_addr_t)(gctUINTPTR_T)paddr,
                  Bytes,
                  Dir);
    }
    else
    {
        /* Non contiguous virtual memory */
        vaddr = (gctPOINTER)gcmALIGN_BASE((gctUINTPTR_T)Logical, PAGE_SIZE);
        pageNum = GetPageCount(Bytes, 0);

        MEMORY_LOCK(Os);
        locked = gcvTRUE;
        for (i = 0; i < pageNum; i += 1)
        {
            gcmkONERROR(_ConvertLogical2Physical(
                Os,
                vaddr + PAGE_SIZE * i,
                ProcessID,
                (PLINUX_MDL)Handle,
                &paddr
                ));

            dma_sync_single_for_device(
                      gcvNULL,
                      (dma_addr_t)(gctUINTPTR_T)paddr,
                      PAGE_SIZE,
                      Dir);
        }
        MEMORY_UNLOCK(Os);
        locked = gcvFALSE;
    }

    mb();

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    if(locked)
    {
        /* Unlock memory. */
        MEMORY_UNLOCK(Os);
    }
    return status;
}

# else

#if !gcdCACHE_FUNCTION_UNIMPLEMENTED && defined(CONFIG_OUTER_CACHE)
static inline gceSTATUS
outer_func(
    gceCACHEOPERATION Type,
    unsigned long Start,
    unsigned long End
    )
{
    switch (Type)
    {
        case gcvCACHE_CLEAN:
            outer_clean_range(Start, End);
            break;
        case gcvCACHE_INVALIDATE:
            outer_inv_range(Start, End);
            break;
        case gcvCACHE_FLUSH:
            outer_flush_range(Start, End);
            break;
        default:
            return gcvSTATUS_INVALID_ARGUMENT;
            break;
    }
    return gcvSTATUS_OK;
}

#if gcdENABLE_OUTER_CACHE_PATCH
/*******************************************************************************
**  _HandleOuterCache
**
**  Handle the outer cache for the specified addresses.
**
**  ARGUMENTS:
**
**      gckOS Os
**          Pointer to gckOS object.
**
**      gctPOINTER Physical
**          Physical address to flush.
**
**      gctPOINTER Logical
**          Logical address to flush.
**
**      gctSIZE_T Bytes
**          Size of the address range in bytes to flush.
**
**      gceOUTERCACHE_OPERATION Type
**          Operation need to be execute.
*/
static gceSTATUS
_HandleOuterCache(
    IN gckOS Os,
    IN gctUINT32 Physical,
    IN gctPOINTER Logical,
    IN gctSIZE_T Bytes,
    IN gceCACHEOPERATION Type
    )
{
    gceSTATUS status;
    gctUINT32 i, pageNum;
    unsigned long paddr;
    gctPOINTER vaddr;

    gcmkHEADER_ARG("Os=0x%X Logical=0x%X Bytes=%lu",
                   Os, Logical, Bytes);

    if (Physical != gcvINVALID_ADDRESS)
    {
        /* Non paged memory or gcvPOOL_USER surface */
        paddr = (unsigned long) Physical;
        gcmkONERROR(outer_func(Type, paddr, paddr + Bytes));
    }
    else
    {
        /* Non contiguous virtual memory */
        vaddr = (gctPOINTER)gcmALIGN_BASE((gctUINTPTR_T)Logical, PAGE_SIZE);
        pageNum = GetPageCount(Bytes, 0);

        for (i = 0; i < pageNum; i += 1)
        {
            gcmkONERROR(_QueryProcessPageTable(
                vaddr + PAGE_SIZE * i,
                (gctUINT32*)&paddr
                ));

            gcmkONERROR(outer_func(Type, paddr, paddr + PAGE_SIZE));
        }
    }

    mb();

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}
#endif
#endif

# endif

/*******************************************************************************
**  gckOS_CacheClean
**
**  Clean the cache for the specified addresses.  The GPU is going to need the
**  data.  If the system is allocating memory as non-cachable, this function can
**  be ignored.
**
**  ARGUMENTS:
**
**      gckOS Os
**          Pointer to gckOS object.
**
**      gctUINT32 ProcessID
**          Process ID Logical belongs.
**
**      gctPHYS_ADDR Handle
**          Physical address handle.  If gcvNULL it is video memory.
**
**      gctPOINTER Physical
**          Physical address to flush.
**
**      gctPOINTER Logical
**          Logical address to flush.
**
**      gctSIZE_T Bytes
**          Size of the address range in bytes to flush.
*/
gceSTATUS
gckOS_CacheClean(
    IN gckOS Os,
    IN gctUINT32 ProcessID,
    IN gctPHYS_ADDR Handle,
    IN gctPHYS_ADDR_T Physical,
    IN gctPOINTER Logical,
    IN gctSIZE_T Bytes
    )
{
    gcsPLATFORM * platform;

    gcmkHEADER_ARG("Os=0x%X ProcessID=%d Handle=0x%X Logical=0x%X Bytes=%lu",
                   Os, ProcessID, Handle, Logical, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Bytes > 0);

    platform = Os->device->platform;

    if (platform && platform->ops->cache)
    {
        platform->ops->cache(
            platform,
            ProcessID,
            Handle,
            Physical,
            Logical,
            Bytes,
            gcvCACHE_CLEAN
            );

        /* Success. */
        gcmkFOOTER_NO();
        return gcvSTATUS_OK;
    }

#if !gcdCACHE_FUNCTION_UNIMPLEMENTED
#if (defined CONFIG_ARM) || (defined CONFIG_ARM64)
# if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))

    _HandleCache(Os, ProcessID, Handle, Physical, Logical, Bytes, DMA_TO_DEVICE);

# else

    /* Inner cache. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
    dmac_map_area(Logical, Bytes, DMA_TO_DEVICE);
#      else
    dmac_clean_range(Logical, Logical + Bytes);
#      endif

#if defined(CONFIG_OUTER_CACHE)
    /* Outer cache. */
#if gcdENABLE_OUTER_CACHE_PATCH
    _HandleOuterCache(Os, Physical, Logical, Bytes, gcvCACHE_CLEAN);
#else
    outer_clean_range((unsigned long) Handle, (unsigned long) Handle + Bytes);
#endif
#endif

# endif

#elif defined(CONFIG_MIPS)

    dma_cache_wback((unsigned long) Logical, Bytes);

#elif defined(CONFIG_PPC)

    /* TODO */

#else
    dma_sync_single_for_device(
              gcvNULL,
              (dma_addr_t)Physical,
              Bytes,
              DMA_TO_DEVICE);
#endif
#endif

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**  gckOS_CacheInvalidate
**
**  Invalidate the cache for the specified addresses. The GPU is going to need
**  data.  If the system is allocating memory as non-cachable, this function can
**  be ignored.
**
**  ARGUMENTS:
**
**      gckOS Os
**          Pointer to gckOS object.
**
**      gctUINT32 ProcessID
**          Process ID Logical belongs.
**
**      gctPHYS_ADDR Handle
**          Physical address handle.  If gcvNULL it is video memory.
**
**      gctPOINTER Logical
**          Logical address to flush.
**
**      gctSIZE_T Bytes
**          Size of the address range in bytes to flush.
*/
gceSTATUS
gckOS_CacheInvalidate(
    IN gckOS Os,
    IN gctUINT32 ProcessID,
    IN gctPHYS_ADDR Handle,
    IN gctPHYS_ADDR_T Physical,
    IN gctPOINTER Logical,
    IN gctSIZE_T Bytes
    )
{
    gcsPLATFORM * platform;

    gcmkHEADER_ARG("Os=0x%X ProcessID=%d Handle=0x%X Logical=0x%X Bytes=%lu",
                   Os, ProcessID, Handle, Logical, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Bytes > 0);

    platform = Os->device->platform;

    if (platform && platform->ops->cache)
    {
        platform->ops->cache(
            platform,
            ProcessID,
            Handle,
            Physical,
            Logical,
            Bytes,
            gcvCACHE_INVALIDATE
            );

        /* Success. */
        gcmkFOOTER_NO();
        return gcvSTATUS_OK;
    }

#if !gcdCACHE_FUNCTION_UNIMPLEMENTED
#if (defined CONFIG_ARM) || (defined CONFIG_ARM64)

# if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))

    _HandleCache(Os, ProcessID, Handle, Physical, Logical, Bytes, DMA_FROM_DEVICE);

# else

    /* Inner cache. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
    dmac_map_area(Logical, Bytes, DMA_FROM_DEVICE);
#      else
    dmac_inv_range(Logical, Logical + Bytes);
#      endif

#if defined(CONFIG_OUTER_CACHE)
    /* Outer cache. */
#if gcdENABLE_OUTER_CACHE_PATCH
    _HandleOuterCache(Os, Physical, Logical, Bytes, gcvCACHE_INVALIDATE);
#else
    outer_inv_range((unsigned long) Handle, (unsigned long) Handle + Bytes);
#endif
#endif

# endif

#elif defined(CONFIG_MIPS)
    dma_cache_inv((unsigned long) Logical, Bytes);
#elif defined(CONFIG_PPC)
    /* TODO */
#else
    dma_sync_single_for_device(
              gcvNULL,
              (dma_addr_t)Physical,
              Bytes,
              DMA_FROM_DEVICE);
#endif
#endif

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**  gckOS_CacheFlush
**
**  Clean the cache for the specified addresses and invalidate the lines as
**  well.  The GPU is going to need and modify the data.  If the system is
**  allocating memory as non-cachable, this function can be ignored.
**
**  ARGUMENTS:
**
**      gckOS Os
**          Pointer to gckOS object.
**
**      gctUINT32 ProcessID
**          Process ID Logical belongs.
**
**      gctPHYS_ADDR Handle
**          Physical address handle.  If gcvNULL it is video memory.
**
**      gctPOINTER Logical
**          Logical address to flush.
**
**      gctSIZE_T Bytes
**          Size of the address range in bytes to flush.
*/
gceSTATUS
gckOS_CacheFlush(
    IN gckOS Os,
    IN gctUINT32 ProcessID,
    IN gctPHYS_ADDR Handle,
    IN gctPHYS_ADDR_T Physical,
    IN gctPOINTER Logical,
    IN gctSIZE_T Bytes
    )
{
    gcsPLATFORM * platform;

    gcmkHEADER_ARG("Os=0x%X ProcessID=%d Handle=0x%X Logical=0x%X Bytes=%lu",
                   Os, ProcessID, Handle, Logical, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Bytes > 0);

    platform = Os->device->platform;

    if (platform && platform->ops->cache)
    {
        platform->ops->cache(
            platform,
            ProcessID,
            Handle,
            Physical,
            Logical,
            Bytes,
            gcvCACHE_FLUSH
            );

        /* Success. */
        gcmkFOOTER_NO();
        return gcvSTATUS_OK;
    }

#if !gcdCACHE_FUNCTION_UNIMPLEMENTED
#if (defined CONFIG_ARM) || (defined CONFIG_ARM64)
# if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))

    _HandleCache(Os, ProcessID, Handle, Physical, Logical, Bytes, DMA_BIDIRECTIONAL);

# else

    /* Inner cache. */
    dmac_flush_range(Logical, Logical + Bytes);

#if defined(CONFIG_OUTER_CACHE)
    /* Outer cache. */
#if gcdENABLE_OUTER_CACHE_PATCH
    _HandleOuterCache(Os, Physical, Logical, Bytes, gcvCACHE_FLUSH);
#else
    outer_flush_range((unsigned long) Handle, (unsigned long) Handle + Bytes);
#endif
#endif

# endif

#elif defined(CONFIG_MIPS)
    dma_cache_wback_inv((unsigned long) Logical, Bytes);
#elif defined(CONFIG_PPC)
    /* TODO */
#else
    dma_sync_single_for_device(
              gcvNULL,
              (dma_addr_t)Physical,
              Bytes,
              DMA_BIDIRECTIONAL);
#endif
#endif

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
********************************* Broadcasting *********************************
*******************************************************************************/

/*******************************************************************************
**
**  gckOS_Broadcast
**
**  System hook for broadcast events from the kernel driver.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to the gckOS object.
**
**      gckHARDWARE Hardware
**          Pointer to the gckHARDWARE object.
**
**      gceBROADCAST Reason
**          Reason for the broadcast.  Can be one of the following values:
**
**              gcvBROADCAST_GPU_IDLE
**                  Broadcasted when the kernel driver thinks the GPU might be
**                  idle.  This can be used to handle power management.
**
**              gcvBROADCAST_GPU_COMMIT
**                  Broadcasted when any client process commits a command
**                  buffer.  This can be used to handle power management.
**
**              gcvBROADCAST_GPU_STUCK
**                  Broadcasted when the kernel driver hits the timeout waiting
**                  for the GPU.
**
**              gcvBROADCAST_FIRST_PROCESS
**                  First process is trying to connect to the kernel.
**
**              gcvBROADCAST_LAST_PROCESS
**                  Last process has detached from the kernel.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_Broadcast(
    IN gckOS Os,
    IN gckHARDWARE Hardware,
    IN gceBROADCAST Reason
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%X Hardware=0x%X Reason=%d", Os, Hardware, Reason);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    switch (Reason)
    {
    case gcvBROADCAST_FIRST_PROCESS:
        gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_OS, "First process has attached");
        break;

    case gcvBROADCAST_LAST_PROCESS:
        gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_OS, "Last process has detached");

    case gcvBROADCAST_IDLE_PROFILE:
        /* Put GPU OFF. */
        gcmkONERROR(
            gckHARDWARE_SetPowerManagementState(Hardware,
                                                gcvPOWER_OFF_BROADCAST));
        break;

    case gcvBROADCAST_GPU_IDLE:
        gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_OS, "GPU idle.");

        if(has_feat_policy_clock_off_when_idle()
           && (Os->clkOffWhenIdle == gcvTRUE))
        {
            /* Put GPU SUSPEND. */
            status = gckHARDWARE_SetPowerManagementState(Hardware,
                                                    gcvPOWER_SUSPEND_BROADCAST);
        }
        else
        {
            /* Put GPU IDLE. */
            status = gckHARDWARE_SetPowerManagementState(Hardware,
                                                    gcvPOWER_IDLE_BROADCAST);
        }

        if(gcmIS_ERROR(status) && status != gcvSTATUS_INVALID_REQUEST)
            gcmkONERROR(status);

        /* Add idle process DB. */
        gcmkONERROR(gckKERNEL_AddProcessDB(Hardware->kernel,
                                           1,
                                           gcvDB_IDLE,
                                           gcvNULL, gcvNULL, 0));
        break;

    case gcvBROADCAST_GPU_COMMIT:
        gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_OS, "COMMIT has arrived.");

        /* Add busy process DB. */
        gcmkONERROR(gckKERNEL_AddProcessDB(Hardware->kernel,
                                           0,
                                           gcvDB_IDLE,
                                           gcvNULL, gcvNULL, 0));

        /* Put GPU ON. */
        gcmkONERROR(
            gckHARDWARE_SetPowerManagementState(Hardware, gcvPOWER_ON_AUTO));
        break;

    case gcvBROADCAST_GPU_STUCK:
        gcmkTRACE_N(gcvLEVEL_ERROR, 0, "gcvBROADCAST_GPU_STUCK\n");
        gcmkONERROR(gckKERNEL_Recovery(Hardware->kernel));
        break;

    case gcvBROADCAST_AXI_BUS_ERROR:
        gcmkTRACE_N(gcvLEVEL_ERROR, 0, "gcvBROADCAST_AXI_BUS_ERROR\n");
        gcmkONERROR(gckHARDWARE_DumpGPUState(Hardware));
        gcmkONERROR(gckKERNEL_Recovery(Hardware->kernel));
        break;

    case gcvBROADCAST_OUT_OF_MEMORY:
        gcmkTRACE_N(gcvLEVEL_INFO, 0, "gcvBROADCAST_OUT_OF_MEMORY\n");

        status = _ShrinkMemory(Os);

        if (status == gcvSTATUS_NOT_SUPPORTED)
        {
            goto OnError;
        }

        gcmkONERROR(status);

        break;

    default:
        /* Skip unimplemented broadcast. */
        break;
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_BroadcastHurry
**
**  The GPU is running too slow.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to the gckOS object.
**
**      gckHARDWARE Hardware
**          Pointer to the gckHARDWARE object.
**
**      gctUINT Urgency
**          The higher the number, the higher the urgency to speed up the GPU.
**          The maximum value is defined by the gcdDYNAMIC_EVENT_THRESHOLD.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_BroadcastHurry(
    IN gckOS Os,
    IN gckHARDWARE Hardware,
    IN gctUINT Urgency
    )
{
    gcmkHEADER_ARG("Os=0x%x Hardware=0x%x Urgency=%u", Os, Hardware, Urgency);

    /* Do whatever you need to do to speed up the GPU now. */

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_BroadcastCalibrateSpeed
**
**  Calibrate the speed of the GPU.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to the gckOS object.
**
**      gckHARDWARE Hardware
**          Pointer to the gckHARDWARE object.
**
**      gctUINT Idle, Time
**          Idle/Time will give the percentage the GPU is idle, so you can use
**          this to calibrate the working point of the GPU.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_BroadcastCalibrateSpeed(
    IN gckOS Os,
    IN gckHARDWARE Hardware,
    IN gctUINT Idle,
    IN gctUINT Time
    )
{
    gcmkHEADER_ARG("Os=0x%x Hardware=0x%x Idle=%u Time=%u",
                   Os, Hardware, Idle, Time);

    /* Do whatever you need to do to callibrate the GPU speed. */

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
********************************** Semaphores **********************************
*******************************************************************************/

/*******************************************************************************
**
**  gckOS_CreateSemaphore
**
**  Create a semaphore.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to the gckOS object.
**
**  OUTPUT:
**
**      gctPOINTER * Semaphore
**          Pointer to the variable that will receive the created semaphore.
*/
gceSTATUS
gckOS_CreateSemaphore(
    IN gckOS Os,
    OUT gctPOINTER * Semaphore
    )
{
    gceSTATUS status;
    struct semaphore *sem = gcvNULL;

    gcmkHEADER_ARG("Os=0x%X", Os);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Semaphore != gcvNULL);

    /* Allocate the semaphore structure. */
    sem = (struct semaphore *)kmalloc(gcmSIZEOF(struct semaphore), GFP_KERNEL | gcdNOWARN);
    if (sem == gcvNULL)
    {
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    /* Initialize the semaphore. */
    sema_init(sem, 1);

    /* Return to caller. */
    *Semaphore = (gctPOINTER) sem;

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_AcquireSemaphore
**
**  Acquire a semaphore.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to the gckOS object.
**
**      gctPOINTER Semaphore
**          Pointer to the semaphore thet needs to be acquired.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_AcquireSemaphore(
    IN gckOS Os,
    IN gctPOINTER Semaphore
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%08X Semaphore=0x%08X", Os, Semaphore);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Semaphore != gcvNULL);

    /* Acquire the semaphore. */
    if (down_interruptible((struct semaphore *) Semaphore))
    {
        gcmkONERROR(gcvSTATUS_INTERRUPTED);
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_AcquireSemaphoreTimeout
**
**  Acquire a semaphore within a given time period
**
**  INPUT:
**
**      gckOS Os
**          Pointer to the gckOS object.
**
**      gctPOINTER Semaphore
**          Pointer to the semaphore that needs to be acquired.
**
**      gctUINT32 Wait
**          Time in milliseconds to wait for the semaphore
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_AcquireSemaphoreTimeout(
    IN gckOS Os,
    IN gctPOINTER Semaphore,
    IN gctUINT32 Wait
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%08X Semaphore=0x%08X", Os, Semaphore);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Semaphore != gcvNULL);

    /* Acquire the semaphore. */
    if (down_timeout((struct semaphore *) Semaphore, Wait * HZ / 1000))
    {
        gcmkONERROR(gcvSTATUS_INTERRUPTED);
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_TryAcquireSemaphore
**
**  Try to acquire a semaphore.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to the gckOS object.
**
**      gctPOINTER Semaphore
**          Pointer to the semaphore thet needs to be acquired.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_TryAcquireSemaphore(
    IN gckOS Os,
    IN gctPOINTER Semaphore
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%x", Os);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Semaphore != gcvNULL);

    /* Acquire the semaphore. */
    if (down_trylock((struct semaphore *) Semaphore))
    {
        /* Timeout. */
        status = gcvSTATUS_TIMEOUT;
        gcmkFOOTER();
        return status;
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_ReleaseSemaphore
**
**  Release a previously acquired semaphore.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to the gckOS object.
**
**      gctPOINTER Semaphore
**          Pointer to the semaphore thet needs to be released.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_ReleaseSemaphore(
    IN gckOS Os,
    IN gctPOINTER Semaphore
    )
{
    gcmkHEADER_ARG("Os=0x%X Semaphore=0x%X", Os, Semaphore);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Semaphore != gcvNULL);

    /* Release the semaphore. */
    up((struct semaphore *) Semaphore);

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_DestroySemaphore
**
**  Destroy a semaphore.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to the gckOS object.
**
**      gctPOINTER Semaphore
**          Pointer to the semaphore thet needs to be destroyed.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_DestroySemaphore(
    IN gckOS Os,
    IN gctPOINTER Semaphore
    )
{
    gcmkHEADER_ARG("Os=0x%X Semaphore=0x%X", Os, Semaphore);

     /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Semaphore != gcvNULL);

    /* Free the sempahore structure. */
    kfree(Semaphore);

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_GetProcessID
**
**  Get current process ID.
**
**  INPUT:
**
**      Nothing.
**
**  OUTPUT:
**
**      gctUINT32_PTR ProcessID
**          Pointer to the variable that receives the process ID.
*/
gceSTATUS
gckOS_GetProcessID(
    OUT gctUINT32_PTR ProcessID
    )
{
    /* Get process ID. */
    if (ProcessID != gcvNULL)
    {
        *ProcessID = _GetProcessID();
    }

    /* Success. */
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_GetThreadID
**
**  Get current thread ID.
**
**  INPUT:
**
**      Nothing.
**
**  OUTPUT:
**
**      gctUINT32_PTR ThreadID
**          Pointer to the variable that receives the thread ID.
*/
gceSTATUS
gckOS_GetThreadID(
    OUT gctUINT32_PTR ThreadID
    )
{
    /* Get thread ID. */
    if (ThreadID != gcvNULL)
    {
        *ThreadID = _GetThreadID();
    }

    /* Success. */
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_SetGPUPower
**
**  Set the power of the GPU on or off.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to a gckOS object.
**
**      gceCORE Core
**          GPU whose power is set.
**
**      gctBOOL Clock
**          gcvTRUE to turn on the clock, or gcvFALSE to turn off the clock.
**
**      gctBOOL Power
**          gcvTRUE to turn on the power, or gcvFALSE to turn off the power.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_SetGPUPower(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctBOOL Clock,
    IN gctBOOL Power
    )
{
#if !MRVL_ENABLE_GC_POWER_CLOCK
    gcsPLATFORM * platform;

    gctBOOL powerChange = gcvFALSE;
    gctBOOL clockChange = gcvFALSE;

    gcmkHEADER_ARG("Os=0x%X Core=%d Clock=%d Power=%d", Os, Core, Clock, Power);
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    platform = Os->device->platform;

    powerChange = (Power != Os->powerStates[Core]);

    clockChange = (Clock != Os->clockStates[Core]);

    if (powerChange && (Power == gcvTRUE))
    {
        if (platform && platform->ops->setPower)
        {
            gcmkVERIFY_OK(platform->ops->setPower(platform, Core, Power));
        }

        Os->powerStates[Core] = Power;
    }

    if (clockChange)
    {
        mutex_lock(&Os->registerAccessLocks[Core]);

        if (platform && platform->ops->setClock)
        {
            gcmkVERIFY_OK(platform->ops->setClock(platform, Core, Clock));
        }

        Os->clockStates[Core] = Clock;

        mutex_unlock(&Os->registerAccessLocks[Core]);
    }

    if (powerChange && (Power == gcvFALSE))
    {
        if (platform && platform->ops->setPower)
        {
            gcmkVERIFY_OK(platform->ops->setPower(platform, Core, Power));
        }

        Os->powerStates[Core] = Power;
    }

    gcmkFOOTER_NO();
#endif
    return gcvSTATUS_OK;
}

static gceSTATUS
_TryToClkOffGPU(
    IN gckOS Os
)
{
    gceSTATUS status = gcvSTATUS_OK;
    gctBOOL powerLocked = gcvFALSE;
    gceCHIPPOWERSTATE currentPwrState;
    gckKERNEL           kernel;
    gctUINT32           gpu;

    gcmkHEADER_ARG("Os=0x%X", Os);
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    for(gpu = 0; gpu < gcdMAX_GPU_COUNT; gpu++)
    {
        kernel = Os->device->kernels[gpu];

        if(kernel && kernel->hardware)
        {
            status = gckOS_AcquireRecMutex(kernel->hardware->os, kernel->hardware->recMutexPower, 0);
            if (status == gcvSTATUS_TIMEOUT)
            {
                continue;
            }
            else
            {
                powerLocked = gcvTRUE;
            }

            currentPwrState = kernel->hardware->chipPowerState;

            if (powerLocked)
            {
                gcmkONERROR(gckOS_ReleaseRecMutex(kernel->hardware->os, kernel->hardware->recMutexPower));
                powerLocked = gcvFALSE;
            }

            if (currentPwrState == gcvPOWER_IDLE)
            {
                /* Inform the system to clock off GPU. */
                gcmkONERROR(gckOS_Broadcast(kernel->hardware->os,
                                            kernel->hardware,
                                            gcvBROADCAST_GPU_IDLE));
            }
        }
    }

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:

    if (powerLocked)
    {
        gcmkVERIFY_OK(gckOS_ReleaseRecMutex(kernel->hardware->os, kernel->hardware->recMutexPower));
    }

    gcmkFOOTER();
    return status;
}

gceSTATUS
gckOS_SetClkOffWhenIdle(
    IN gckOS Os,
    IN gctBOOL enable
    )
{
    gcmkHEADER_ARG("Os=0x%X enable=%d", Os, enable);
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    Os->clkOffWhenIdle = enable;

    if(enable == gcvTRUE)
    {
        gcmkVERIFY_OK(_TryToClkOffGPU(Os));
    }

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_ResetGPU
**
**  Reset the GPU.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to a gckOS object.
**
**      gckCORE Core
**          GPU whose power is set.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_ResetGPU(
    IN gckOS Os,
    IN gceCORE Core
    )
{
    gceSTATUS status = gcvSTATUS_NOT_SUPPORTED;
    gcsPLATFORM * platform;

    gcmkHEADER_ARG("Os=0x%X Core=%d", Os, Core);
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    platform = Os->device->platform;

    if (platform && platform->ops->reset)
    {
        status = platform->ops->reset(platform, Core);
    }

    gcmkFOOTER_NO();
    return status;
}

/*******************************************************************************
**
**  gckOS_PrepareGPUFrequency
**
**  Prepare to set GPU frequency and voltage.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to a gckOS object.
**
**      gckCORE Core
**          GPU whose frequency and voltage will be set.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_PrepareGPUFrequency(
    IN gckOS Os,
    IN gceCORE Core
    )
{
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_FinishGPUFrequency
**
**  Finish GPU frequency setting.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to a gckOS object.
**
**      gckCORE Core
**          GPU whose frequency and voltage is set.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_FinishGPUFrequency(
    IN gckOS Os,
    IN gceCORE Core
    )
{
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_QueryGPUFrequency
**
**  Query the current frequency of the GPU.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to a gckOS object.
**
**      gckCORE Core
**          GPU whose power is set.
**
**      gctUINT32 * Frequency
**          Pointer to a gctUINT32 to obtain current frequency, in MHz.
**
**      gctUINT8 * Scale
**          Pointer to a gctUINT8 to obtain current scale(1 - 64).
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_QueryGPUFrequency(
    IN gckOS Os,
    IN gceCORE Core,
    OUT gctUINT32 * Frequency,
    OUT gctUINT8 * Scale
    )
{
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_SetGPUFrequency
**
**  Set frequency and voltage of the GPU.
**
**      1. DVFS manager gives the target scale of full frequency, BSP must find
**         a real frequency according to this scale and board's configure.
**
**      2. BSP should find a suitable voltage for this frequency.
**
**      3. BSP must make sure setting take effect before this function returns.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to a gckOS object.
**
**      gckCORE Core
**          GPU whose power is set.
**
**      gctUINT8 Scale
**          Target scale of full frequency, range is [1, 64]. 1 means 1/64 of
**          full frequency and 64 means 64/64 of full frequency.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_SetGPUFrequency(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctUINT8 Scale
    )
{
    return gcvSTATUS_OK;
}

/*----------------------------------------------------------------------------*/
/*----- Profile --------------------------------------------------------------*/

gceSTATUS
gckOS_GetProfileTick(
    OUT gctUINT64_PTR Tick
    )
{
    struct timespec time;

    ktime_get_ts(&time);

    *Tick = time.tv_nsec + time.tv_sec * 1000000000ULL;

    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_QueryProfileTickRate(
    OUT gctUINT64_PTR TickRate
    )
{
    struct timespec res;

    hrtimer_get_res(CLOCK_MONOTONIC, &res);

    *TickRate = res.tv_nsec + res.tv_sec * 1000000000ULL;

    return gcvSTATUS_OK;
}

gctUINT32
gckOS_ProfileToMS(
    IN gctUINT64 Ticks
    )
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,23)
    return div_u64(Ticks, 1000000);
#else
    gctUINT64 rem = Ticks;
    gctUINT64 b = 1000000;
    gctUINT64 res, d = 1;
    gctUINT32 high = rem >> 32;

    /* Reduce the thing a bit first */
    res = 0;
    if (high >= 1000000)
    {
        high /= 1000000;
        res   = (gctUINT64) high << 32;
        rem  -= (gctUINT64) (high * 1000000) << 32;
    }

    while (((gctINT64) b > 0) && (b < rem))
    {
        b <<= 1;
        d <<= 1;
    }

    do
    {
        if (rem >= b)
        {
            rem -= b;
            res += d;
        }

        b >>= 1;
        d >>= 1;
    }
    while (d);

    return (gctUINT32) res;
#endif
}

/******************************************************************************\
******************************* Signal Management ******************************
\******************************************************************************/

#undef _GC_OBJ_ZONE
#define _GC_OBJ_ZONE    gcvZONE_SIGNAL

/*******************************************************************************
**
**  gckOS_CreateSignal
**
**  Create a new signal.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctBOOL ManualReset
**          If set to gcvTRUE, gckOS_Signal with gcvFALSE must be called in
**          order to set the signal to nonsignaled state.
**          If set to gcvFALSE, the signal will automatically be set to
**          nonsignaled state by gckOS_WaitSignal function.
**
**  OUTPUT:
**
**      gctSIGNAL * Signal
**          Pointer to a variable receiving the created gctSIGNAL.
*/
gceSTATUS
gckOS_CreateSignal(
    IN gckOS Os,
    IN gctBOOL ManualReset,
    OUT gctSIGNAL * Signal
    )
{
    gceSTATUS status;
    gcsSIGNAL_PTR signal;

    gcmkHEADER_ARG("Os=0x%X ManualReset=%d", Os, ManualReset);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Signal != gcvNULL);

    /* Create an event structure. */
    signal = (gcsSIGNAL_PTR) kmalloc(sizeof(gcsSIGNAL), GFP_KERNEL | gcdNOWARN);

    if (signal == gcvNULL)
    {
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    /* Save the process ID. */
    signal->process = (gctHANDLE)(gctUINTPTR_T) _GetProcessID();
    signal->manualReset = ManualReset;
    signal->hardware = gcvNULL;
    init_completion(&signal->obj);
    atomic_set(&signal->ref, 1);

    gcmkONERROR(_AllocateIntegerId(&Os->signalDB, signal, &signal->id));

    *Signal = (gctSIGNAL)(gctUINTPTR_T)signal->id;

    gcmkFOOTER_ARG("*Signal=0x%X", *Signal);
    return gcvSTATUS_OK;

OnError:
    if (signal != gcvNULL)
    {
        kfree(signal);
    }

    gcmkFOOTER_NO();
    return status;
}

gceSTATUS
gckOS_SignalQueryHardware(
    IN gckOS Os,
    IN gctSIGNAL Signal,
    OUT gckHARDWARE * Hardware
    )
{
    gceSTATUS status;
    gcsSIGNAL_PTR signal;

    gcmkHEADER_ARG("Os=0x%X Signal=0x%X Hardware=0x%X", Os, Signal, Hardware);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Signal != gcvNULL);
    gcmkVERIFY_ARGUMENT(Hardware != gcvNULL);

    gcmkONERROR(_QueryIntegerId(&Os->signalDB, (gctUINT32)(gctUINTPTR_T)Signal, (gctPOINTER)&signal));

    *Hardware = signal->hardware;

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
OnError:
    gcmkFOOTER();
    return status;
}

gceSTATUS
gckOS_SignalSetHardware(
    IN gckOS Os,
    IN gctSIGNAL Signal,
    IN gckHARDWARE Hardware
    )
{
    gceSTATUS status;
    gcsSIGNAL_PTR signal;

    gcmkHEADER_ARG("Os=0x%X Signal=0x%X Hardware=0x%X", Os, Signal, Hardware);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Signal != gcvNULL);

    gcmkONERROR(_QueryIntegerId(&Os->signalDB, (gctUINT32)(gctUINTPTR_T)Signal, (gctPOINTER)&signal));

    signal->hardware = Hardware;

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
OnError:
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_DestroySignal
**
**  Destroy a signal.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctSIGNAL Signal
**          Pointer to the gctSIGNAL.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_DestroySignal(
    IN gckOS Os,
    IN gctSIGNAL Signal
    )
{
    gceSTATUS status;
    gcsSIGNAL_PTR signal;
    gctBOOL acquired = gcvFALSE;

    gcmkHEADER_ARG("Os=0x%X Signal=0x%X", Os, Signal);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Signal != gcvNULL);

    gcmkONERROR(gckOS_AcquireMutex(Os, Os->signalMutex, gcvINFINITE));
    acquired = gcvTRUE;

    gcmkONERROR(_QueryIntegerId(&Os->signalDB, (gctUINT32)(gctUINTPTR_T)Signal, (gctPOINTER)&signal));

    gcmkASSERT(signal->id == (gctUINT32)(gctUINTPTR_T)Signal);

    if (atomic_dec_and_test(&signal->ref))
    {
        gcmkVERIFY_OK(_DestroyIntegerId(&Os->signalDB, signal->id));

        /* Free the sgianl. */
        kfree(signal);
    }

    gcmkVERIFY_OK(gckOS_ReleaseMutex(Os, Os->signalMutex));
    acquired = gcvFALSE;

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    if (acquired)
    {
        /* Release the mutex. */
        gcmkVERIFY_OK(gckOS_ReleaseMutex(Os, Os->signalMutex));
    }

    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_Signal
**
**  Set a state of the specified signal.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctSIGNAL Signal
**          Pointer to the gctSIGNAL.
**
**      gctBOOL State
**          If gcvTRUE, the signal will be set to signaled state.
**          If gcvFALSE, the signal will be set to nonsignaled state.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_Signal(
    IN gckOS Os,
    IN gctSIGNAL Signal,
    IN gctBOOL State
    )
{
    gceSTATUS status;
    gcsSIGNAL_PTR signal;
    gctBOOL acquired = gcvFALSE;

    gcmkHEADER_ARG("Os=0x%X Signal=0x%X State=%d", Os, Signal, State);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Signal != gcvNULL);

    gcmkONERROR(gckOS_AcquireMutex(Os, Os->signalMutex, gcvINFINITE));
    acquired = gcvTRUE;

    gcmkONERROR(_QueryIntegerId(&Os->signalDB, (gctUINT32)(gctUINTPTR_T)Signal, (gctPOINTER)&signal));

    gcmkASSERT(signal->id == (gctUINT32)(gctUINTPTR_T)Signal);

    if (State)
    {
        /* unbind the signal from hardware. */
        signal->hardware = gcvNULL;

        /* Set the event to a signaled state. */
        complete(&signal->obj);
    }
    else
    {
        /* Set the event to an unsignaled state. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0)
        reinit_completion(&signal->obj);
#else
        INIT_COMPLETION(signal->obj);
#endif
    }

    gcmkVERIFY_OK(gckOS_ReleaseMutex(Os, Os->signalMutex));
    acquired = gcvFALSE;

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    if (acquired)
    {
        /* Release the mutex. */
        gcmkVERIFY_OK(gckOS_ReleaseMutex(Os, Os->signalMutex));
    }

    gcmkFOOTER();
    return status;
}

#if gcdENABLE_VG
gceSTATUS
gckOS_SetSignalVG(
    IN gckOS Os,
    IN gctHANDLE Process,
    IN gctSIGNAL Signal
    )
{
    gceSTATUS status;
    gctINT result;
    struct task_struct * userTask;
    struct siginfo info;

    userTask = FIND_TASK_BY_PID((pid_t)(gctUINTPTR_T) Process);

    if (userTask != gcvNULL)
    {
        info.si_signo = 48;
        info.si_code  = __SI_CODE(__SI_RT, SI_KERNEL);
        info.si_pid   = 0;
        info.si_uid   = 0;
        info.si_ptr   = (gctPOINTER) Signal;

        /* Signals with numbers between 32 and 63 are real-time,
           send a real-time signal to the user process. */
        result = send_sig_info(48, &info, userTask);

        printk("gckOS_SetSignalVG:0x%x\n", result);
        /* Error? */
        if (result < 0)
        {
            status = gcvSTATUS_GENERIC_IO;

            gcmkTRACE(
                gcvLEVEL_ERROR,
                "%s(%d): an error has occurred.\n",
                __FUNCTION__, __LINE__
                );
        }
        else
        {
            status = gcvSTATUS_OK;
        }
    }
    else
    {
        status = gcvSTATUS_GENERIC_IO;

        gcmkTRACE(
            gcvLEVEL_ERROR,
            "%s(%d): an error has occurred.\n",
            __FUNCTION__, __LINE__
            );
    }

    /* Return status. */
    return status;
}
#endif

/*******************************************************************************
**
**  gckOS_UserSignal
**
**  Set the specified signal which is owned by a process to signaled state.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctSIGNAL Signal
**          Pointer to the gctSIGNAL.
**
**      gctHANDLE Process
**          Handle of process owning the signal.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_UserSignal(
    IN gckOS Os,
    IN gctSIGNAL Signal,
    IN gctHANDLE Process
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%X Signal=0x%X Process=%d",
                   Os, Signal, (gctINT32)(gctUINTPTR_T)Process);

    /* MapSignal and increase the ref has been done in gckEVENT_AddList*/

    /* Signal. */
    gcmkONERROR(gckOS_Signal(Os, Signal, gcvTRUE));

    /* Unmap the signal */
    gcmkVERIFY_OK(gckOS_UnmapSignal(Os, Signal));

    gcmkFOOTER();
    return status;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_WaitSignal
**
**  Wait for a signal to become signaled.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctSIGNAL Signal
**          Pointer to the gctSIGNAL.
**
**      gctUINT32 Wait
**          Number of milliseconds to wait.
**          Pass the value of gcvINFINITE for an infinite wait.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_WaitSignal(
    IN gckOS Os,
    IN gctSIGNAL Signal,
    IN gctUINT32 Wait
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsSIGNAL_PTR signal;

    gcmkHEADER_ARG("Os=0x%X Signal=0x%X Wait=0x%08X", Os, Signal, Wait);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Signal != gcvNULL);

    gcmkONERROR(_QueryIntegerId(&Os->signalDB, (gctUINT32)(gctUINTPTR_T)Signal, (gctPOINTER)&signal));

    gcmkASSERT(signal->id == (gctUINT32)(gctUINTPTR_T)Signal);

    might_sleep();

    spin_lock_irq(&signal->obj.wait.lock);

    if (signal->obj.done)
    {
        if (!signal->manualReset)
        {
            signal->obj.done = 0;
        }

        status = gcvSTATUS_OK;
    }
    else if (Wait == 0)
    {
        status = gcvSTATUS_TIMEOUT;
    }
    else
    {
        /* Convert wait to milliseconds. */
        long timeout = (Wait == gcvINFINITE)
            ? MAX_SCHEDULE_TIMEOUT
            : Wait * HZ / 1000;

        DECLARE_WAITQUEUE(wait, current);
        wait.flags |= WQ_FLAG_EXCLUSIVE;
        __add_wait_queue_tail(&signal->obj.wait, &wait);

        while (gcvTRUE)
        {
            /* MRVL: for stall timer, we should not check signal_pending like during command stall */
            if ((Wait != gcdGPU_ADVANCETIMER_STALL) && signal_pending(current))
            {
                /* Interrupt received. */
                status = gcvSTATUS_INTERRUPTED;
                break;
            }

            __set_current_state(TASK_INTERRUPTIBLE);
            spin_unlock_irq(&signal->obj.wait.lock);
            timeout = schedule_timeout(timeout);
            spin_lock_irq(&signal->obj.wait.lock);

            if (signal->obj.done)
            {
                if (!signal->manualReset)
                {
                    signal->obj.done = 0;
                }

                status = gcvSTATUS_OK;
                break;
            }

            if (timeout == 0)
            {

                status = gcvSTATUS_TIMEOUT;
                break;
            }
        }

        __remove_wait_queue(&signal->obj.wait, &wait);
    }

    spin_unlock_irq(&signal->obj.wait.lock);

OnError:
    /* Return status. */
    gcmkFOOTER_ARG("Signal=0x%X status=%d", Signal, status);
    return status;
}

/*******************************************************************************
**
**  gckOS_MapSignal
**
**  Map a signal in to the current process space.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctSIGNAL Signal
**          Pointer to tha gctSIGNAL to map.
**
**      gctHANDLE Process
**          Handle of process owning the signal.
**
**  OUTPUT:
**
**      gctSIGNAL * MappedSignal
**          Pointer to a variable receiving the mapped gctSIGNAL.
*/
gceSTATUS
gckOS_MapSignal(
    IN gckOS Os,
    IN gctSIGNAL Signal,
    IN gctHANDLE Process,
    OUT gctSIGNAL * MappedSignal
    )
{
    gceSTATUS status;
    gcsSIGNAL_PTR signal;
    gcmkHEADER_ARG("Os=0x%X Signal=0x%X Process=0x%X", Os, Signal, Process);

    gcmkVERIFY_ARGUMENT(Signal != gcvNULL);
    gcmkVERIFY_ARGUMENT(MappedSignal != gcvNULL);

    status = _QueryIntegerId(&Os->signalDB, (gctUINT32)(gctUINTPTR_T)Signal, (gctPOINTER)&signal);

    /* gcvSTATUS_NOT_FOUND is a normal return value,
    ** since user level may already delete it.
    */
    if (status == gcvSTATUS_NOT_FOUND)
    {
        gcmkFOOTER();
        return gcvSTATUS_NOT_FOUND;
    }
    else
    {
        gcmkONERROR(status);
    }

    if(atomic_inc_return(&signal->ref) <= 1)
    {
        /* The previous value is 0, it has been deleted. */
        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    *MappedSignal = (gctSIGNAL) Signal;

    /* Success. */
    gcmkFOOTER_ARG("*MappedSignal=0x%X", *MappedSignal);
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER_NO();
    return status;
}

/*******************************************************************************
**
**  gckOS_UnmapSignal
**
**  Unmap a signal .
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctSIGNAL Signal
**          Pointer to that gctSIGNAL mapped.
*/
gceSTATUS
gckOS_UnmapSignal(
    IN gckOS Os,
    IN gctSIGNAL Signal
    )
{
    return gckOS_DestroySignal(Os, Signal);
}

/*******************************************************************************
**
**  gckOS_CreateUserSignal
**
**  Create a new signal to be used in the user space.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctBOOL ManualReset
**          If set to gcvTRUE, gckOS_Signal with gcvFALSE must be called in
**          order to set the signal to nonsignaled state.
**          If set to gcvFALSE, the signal will automatically be set to
**          nonsignaled state by gckOS_WaitSignal function.
**
**  OUTPUT:
**
**      gctINT * SignalID
**          Pointer to a variable receiving the created signal's ID.
*/
gceSTATUS
gckOS_CreateUserSignal(
    IN gckOS Os,
    IN gctBOOL ManualReset,
    OUT gctINT * SignalID
    )
{
    gceSTATUS status;
    gctSIZE_T signal;

    /* Create a new signal. */
    gcmkONERROR(gckOS_CreateSignal(Os, ManualReset, (gctSIGNAL *) &signal));
    *SignalID = (gctINT) signal;

OnError:
    return status;
}

/*******************************************************************************
**
**  gckOS_DestroyUserSignal
**
**  Destroy a signal to be used in the user space.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctINT SignalID
**          The signal's ID.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_DestroyUserSignal(
    IN gckOS Os,
    IN gctINT SignalID
    )
{
    return gckOS_DestroySignal(Os, (gctSIGNAL)(gctUINTPTR_T)SignalID);
}

/*******************************************************************************
**
**  gckOS_WaitUserSignal
**
**  Wait for a signal used in the user mode to become signaled.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctINT SignalID
**          Signal ID.
**
**      gctUINT32 Wait
**          Number of milliseconds to wait.
**          Pass the value of gcvINFINITE for an infinite wait.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_WaitUserSignal(
    IN gckOS Os,
    IN gctINT SignalID,
    IN gctUINT32 Wait
    )
{
    return gckOS_WaitSignal(Os, (gctSIGNAL)(gctUINTPTR_T)SignalID, Wait);
}

/*******************************************************************************
**
**  gckOS_SignalUserSignal
**
**  Set a state of the specified signal to be used in the user space.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**      gctINT SignalID
**          SignalID.
**
**      gctBOOL State
**          If gcvTRUE, the signal will be set to signaled state.
**          If gcvFALSE, the signal will be set to nonsignaled state.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_SignalUserSignal(
    IN gckOS Os,
    IN gctINT SignalID,
    IN gctBOOL State
    )
{
    return gckOS_Signal(Os, (gctSIGNAL)(gctUINTPTR_T)SignalID, State);
}

#if gcdENABLE_VG
gceSTATUS
gckOS_CreateSemaphoreVG(
    IN gckOS Os,
    OUT gctSEMAPHORE * Semaphore
    )
{
    gceSTATUS status;
    struct semaphore * newSemaphore;

    gcmkHEADER_ARG("Os=0x%X Semaphore=0x%x", Os, Semaphore);
    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Semaphore != gcvNULL);

    do
    {
        /* Allocate the semaphore structure. */
        newSemaphore = (struct semaphore *)kmalloc(gcmSIZEOF(struct semaphore), GFP_KERNEL | gcdNOWARN);
        if (newSemaphore == gcvNULL)
        {
            gcmkERR_BREAK(gcvSTATUS_OUT_OF_MEMORY);
        }

        /* Initialize the semaphore. */
        sema_init(newSemaphore, 0);

        /* Set the handle. */
        * Semaphore = (gctSEMAPHORE) newSemaphore;

        /* Success. */
        status = gcvSTATUS_OK;
    }
    while (gcvFALSE);

    gcmkFOOTER();
    /* Return the status. */
    return status;
}


gceSTATUS
gckOS_IncrementSemaphore(
    IN gckOS Os,
    IN gctSEMAPHORE Semaphore
    )
{
    gcmkHEADER_ARG("Os=0x%X Semaphore=0x%x", Os, Semaphore);
    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Semaphore != gcvNULL);

    /* Increment the semaphore's count. */
    up((struct semaphore *) Semaphore);

    gcmkFOOTER_NO();
    /* Success. */
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_DecrementSemaphore(
    IN gckOS Os,
    IN gctSEMAPHORE Semaphore
    )
{
    gceSTATUS status;
    gctINT result;

    gcmkHEADER_ARG("Os=0x%X Semaphore=0x%x", Os, Semaphore);
    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Semaphore != gcvNULL);

    do
    {
        /* Decrement the semaphore's count. If the count is zero, wait
           until it gets incremented. */
        result = down_interruptible((struct semaphore *) Semaphore);

        /* Signal received? */
        if (result != 0)
        {
            status = gcvSTATUS_TERMINATE;
            break;
        }

        /* Success. */
        status = gcvSTATUS_OK;
    }
    while (gcvFALSE);

    gcmkFOOTER();
    /* Return the status. */
    return status;
}

/*******************************************************************************
**
**  gckOS_SetSignal
**
**  Set the specified signal to signaled state.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to the gckOS object.
**
**      gctHANDLE Process
**          Handle of process owning the signal.
**
**      gctSIGNAL Signal
**          Pointer to the gctSIGNAL.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_SetSignal(
    IN gckOS Os,
    IN gctHANDLE Process,
    IN gctSIGNAL Signal
    )
{
    gceSTATUS status;
    gctINT result;
    struct task_struct * userTask;
    struct siginfo info;

    userTask = FIND_TASK_BY_PID((pid_t)(gctUINTPTR_T) Process);

    if (userTask != gcvNULL)
    {
        info.si_signo = 48;
        info.si_code  = __SI_CODE(__SI_RT, SI_KERNEL);
        info.si_pid   = 0;
        info.si_uid   = 0;
        info.si_ptr   = (gctPOINTER) Signal;

        /* Signals with numbers between 32 and 63 are real-time,
           send a real-time signal to the user process. */
        result = send_sig_info(48, &info, userTask);

        /* Error? */
        if (result < 0)
        {
            status = gcvSTATUS_GENERIC_IO;

            gcmkTRACE(
                gcvLEVEL_ERROR,
                "%s(%d): an error has occurred.\n",
                __FUNCTION__, __LINE__
                );
        }
        else
        {
            status = gcvSTATUS_OK;
        }
    }
    else
    {
        status = gcvSTATUS_GENERIC_IO;

        gcmkTRACE(
            gcvLEVEL_ERROR,
            "%s(%d): an error has occurred.\n",
            __FUNCTION__, __LINE__
            );
    }

    /* Return status. */
    return status;
}

/******************************************************************************\
******************************** Thread Object *********************************
\******************************************************************************/

gceSTATUS
gckOS_StartThread(
    IN gckOS Os,
    IN gctTHREADFUNC ThreadFunction,
    IN gctPOINTER ThreadParameter,
    OUT gctTHREAD * Thread
    )
{
    gceSTATUS status;
    struct task_struct * thread;

    gcmkHEADER_ARG("Os=0x%X ", Os);
    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(ThreadFunction != gcvNULL);
    gcmkVERIFY_ARGUMENT(Thread != gcvNULL);

    do
    {
        /* Create the thread. */
        thread = kthread_create(
            ThreadFunction,
            ThreadParameter,
            "Vivante Kernel Thread"
            );

        /* Failed? */
        if (IS_ERR(thread))
        {
            status = gcvSTATUS_GENERIC_IO;
            break;
        }

        /* Start the thread. */
        wake_up_process(thread);

        /* Set the thread handle. */
        * Thread = (gctTHREAD) thread;

        /* Success. */
        status = gcvSTATUS_OK;
    }
    while (gcvFALSE);

    gcmkFOOTER();
    /* Return the status. */
    return status;
}

gceSTATUS
gckOS_StopThread(
    IN gckOS Os,
    IN gctTHREAD Thread
    )
{
    gcmkHEADER_ARG("Os=0x%X Thread=0x%x", Os, Thread);
    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Thread != gcvNULL);

    /* Thread should have already been enabled to terminate. */
    kthread_stop((struct task_struct *) Thread);

    gcmkFOOTER_NO();
    /* Success. */
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_VerifyThread(
    IN gckOS Os,
    IN gctTHREAD Thread
    )
{
    gcmkHEADER_ARG("Os=0x%X Thread=0x%x", Os, Thread);
    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Thread != gcvNULL);

    gcmkFOOTER_NO();
    /* Success. */
    return gcvSTATUS_OK;
}
#endif

/******************************************************************************\
**************************** Power Management **********************************
\******************************************************************************/

gceSTATUS
gckOS_GetIfaceMapping(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctBOOL Axi,
    OUT gctPOINTER *Iface
    )
{
    struct gc_iface *iface;

    gcmkHEADER_ARG("Os=0x%X Core=%d", Os, Core);

    iface = gpu_get_iface_mapping(Core);
    if(iface == gcvNULL)
        return gcvSTATUS_NOT_SUPPORTED;

    if(Axi && has_feat_axi_freq_change())
    {
        if(gcvNULL == iface->axi_clk)
            return gcvSTATUS_NOT_SUPPORTED;

        *Iface = (gctPOINTER) iface->axi_clk;
    }
    else
    {
        *Iface = (gctPOINTER) iface;
    }

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_QueryClkRate(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctBOOL AXI,
    OUT gctUINT32_PTR Rate
    )
{
    gceSTATUS status;
    unsigned long rate;
    struct gc_iface *iface;

    gcmkONERROR(gckOS_GetIfaceMapping(Os, Core, AXI, (gctPOINTER *)&iface));

    gpu_clk_getrate(iface, &rate);
    *Rate = rate;
    return gcvSTATUS_OK;

OnError:
    return status;
}

gceSTATUS
gckOS_SetClkRate(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctBOOL Axi,
    IN gctUINT32 Rate
    )
{
    gceSTATUS status;
    struct gc_iface *iface;

    gcmkONERROR(gckOS_GetIfaceMapping(Os, Core, Axi, (gctPOINTER *)&iface));

    gpu_clk_setrate(iface, (unsigned long)Rate);

    return gcvSTATUS_OK;

OnError:
    return status;
}

gceSTATUS
gckOS_SetGPUPowerOnBeforeInit(
    IN gctPOINTER Ptr4dev,
    IN gceCORE Core,
    IN gctBOOL EnableClk,
    IN gctBOOL EnablePwr,
    IN gctUINT clkRate
)
{
    gceSTATUS status;
    struct gc_iface *iface;
    struct gc_iface *ifaceShader = gcvNULL;
    int ret = 0;
    gcmkHEADER_ARG("Core=%d EnableClk=%d EnablePwr=%d", Core, EnableClk, EnablePwr);

    gcmkONERROR(gckOS_GetIfaceMapping(NULL, Core, gcvFALSE, (gctPOINTER *)&iface));

    ret = gpu_clk_init(iface, Ptr4dev);
    if(ret == -1)
        gcmkONERROR(gcvSTATUS_NOT_FOUND);

    if(EnablePwr)
    {
        gpu_pwr_enable(iface);
    }

    if(EnableClk)
    {
        gpu_clk_enable(iface);
    }

    if (clkRate)
    {
        /*
            no need to add has_feat_dfc_protect_clk_op() check
            for set clock rate here, since we just initialized.
        */
        gpu_clk_setrate(iface, clkRate);

        if (has_feat_3d_shader_clock()
            && (Core == gcvCORE_MAJOR))
        {
            gcmkONERROR(gckOS_GetIfaceMapping(NULL, gcvCORE_SH, gcvFALSE, (gctPOINTER *)&ifaceShader));
            gpu_clk_setrate(ifaceShader, clkRate);
        }
    }

OnError:
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_SetGPUPowerOnMRVL(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctBOOL EnableClk,
    IN gctBOOL EnablePwr,
    IN gctBOOL Hint
    )
{
    gceSTATUS status;
    struct gc_iface *iface;
    gcmkHEADER_ARG("Os=0x%X Core=%d EnableClk=%d EnablePwr=%d", Os, Core, EnableClk, EnablePwr);

    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkONERROR(gckOS_GetIfaceMapping(Os, Core, gcvFALSE, (gctPOINTER *)&iface));

    down_write(&Os->rwsem_clk_pwr);

    if (has_feat_dfc_protect_clk_op())
        gcmkVERIFY_OK(gckOS_AcquireClockMutex(Os, Core));

    /* if we enabled power domain,
     * do not touch power in suspend/resume context,
     * Power domain will handle that for us.
     */
    if(EnablePwr && !Hint)
    {
        int retval = 0;

        gpu_pwr_enable_prepare(iface);
        retval = gpu_pwr_enable(iface);
        if(retval < 0)
            gcmkPRINT("galcore: GPU %d power ops failed, retval %d @ %s(%d)\n",
                        Core, retval, __FUNCTION__, __LINE__);
        gpu_pwr_enable_unprepare(iface);
        trace_gc_power(Core, 1);
    }

    if(EnableClk)
    {
        gpu_clk_enable(iface);
        trace_gc_clock(Core, 1);
    }

    if (has_feat_dfc_protect_clk_op())
        gcmkVERIFY_OK(gckOS_ReleaseClockMutex(Os, Core));

    up_write(&Os->rwsem_clk_pwr);

OnError:
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_SetGPUPowerOffMRVL(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctBOOL DisableClk,
    IN gctBOOL DisablePwr,
    IN gctBOOL Hint
    )
{
    gceSTATUS status;
    struct gc_iface *iface;
    gcmkHEADER_ARG("Os=0x%X Core=%d DisableClk=%d DisablePwr=%d", Os, Core, DisableClk, DisablePwr);

    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkONERROR(gckOS_GetIfaceMapping(Os, Core, gcvFALSE, (gctPOINTER *)&iface));

    down_write(&Os->rwsem_clk_pwr);

    if (has_feat_dfc_protect_clk_op())
        gcmkVERIFY_OK(gckOS_AcquireClockMutex(Os, Core));

    if (DisableClk)
    {
        gpu_clk_disable(iface);
        trace_gc_clock(Core, 0);
    }

    /* if we enabled power domain,
     * do not touch power in suspend/resume context,
     * Power domain will handle that for us.
     */
    if (DisablePwr && !Hint)
    {
        int retval = 0;
        gpu_pwr_disable_prepare(iface);
        retval = gpu_pwr_disable(iface);
        if(retval < 0)
            gcmkPRINT("galcore: GPU %d power ops failed, retval %d @ %s(%d)\n",
                        Core, retval, __FUNCTION__, __LINE__);
        gpu_pwr_disable_unprepare(iface);
        trace_gc_power(Core, 0);
    }

    if (has_feat_dfc_protect_clk_op())
        gcmkVERIFY_OK(gckOS_ReleaseClockMutex(Os, Core));

    up_write(&Os->rwsem_clk_pwr);

OnError:
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

static gceSTATUS
_IdleProfile(
    IN gckOS Os,
    OUT gctBOOL_PTR NeedPowerOff
    )
{
    gctUINT64   ticks;
    gctUINT32   time;
    gceSTATUS   status;
    gckKERNEL   kernel;
    gctBOOL     acquired = gcvFALSE;
    gctBOOL     needPowerOff = gcvFALSE;
    gctUINT32   timeSlice, idleTime, tailTimeSlice, tailIdleTime;

    gcmkHEADER_ARG("Os=0x%x", Os);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    kernel          = Os->device->kernels[gcvCORE_MAJOR];
    gcmkVERIFY_OBJECT(kernel, gcvOBJ_KERNEL);

    /* Initialize time interval constants. */
    timeSlice       = Os->device->profileTimeSlice;
    tailTimeSlice   = Os->device->profileTailTimeSlice;

    /* Grab the mutex. */
    gcmkONERROR(gckOS_AcquireMutex(Os, kernel->db->profMutex, gcvINFINITE));
    acquired = gcvTRUE;

    gcmkONERROR(gckOS_GetProfileTick(&ticks));
    time = gckOS_ProfileToMS(ticks);

    /* 3D core query idle data */
    gcmkONERROR(gckKERNEL_QueryIdleProfile(kernel, time, &timeSlice, &idleTime));
    if(Os->device->profilerDebug)
    {
        gcmkPRINT("[3D]idle:total [%d, %d]\n",idleTime, timeSlice);
    }

    gcmkONERROR(gckKERNEL_QueryIdleProfile(kernel, time, &tailTimeSlice, &tailIdleTime));
    if(Os->device->profilerDebug)
    {
        gcmkPRINT("[3D]idle:total [%d, %d]\n", tailIdleTime, tailTimeSlice);
    }

    /* idle profile policy goes here. */
    needPowerOff  = ((idleTime * 100 > timeSlice * Os->device->idleThreshold)
                    && (tailIdleTime == tailTimeSlice));

    *NeedPowerOff = needPowerOff;

    /* Release the mutex. */
    gcmkVERIFY_OK(gckOS_ReleaseMutex(Os, kernel->db->profMutex));

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    if (acquired)
    {
        gcmkVERIFY_OK(gckOS_ReleaseMutex(Os, kernel->db->profMutex));
    }

    gcmkFOOTER();
    return status;
}

gceSTATUS
gckOS_QueryIdleProfile(
    IN     gckOS         Os,
    IN     gceCORE       Core,
    IN OUT gctUINT32_PTR Timeslice,
    OUT    gctUINT32_PTR IdleTime
    )
{
    gctUINT64   ticks;
    gctUINT32   time;
    gceSTATUS   status;
    gckKERNEL   kernel;
    gctBOOL     acquired = gcvFALSE;

    gcmkHEADER_ARG("Os=0x%x", Os);
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    kernel = Os->device->kernels[Core];
    if(kernel == gcvNULL)
    {
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    /* Grab the mutex. */
    gcmkONERROR(gckOS_AcquireMutex(Os, kernel->db->profMutex, gcvINFINITE));
    acquired = gcvTRUE;

    gcmkONERROR(gckOS_GetProfileTick(&ticks));
    time = gckOS_ProfileToMS(ticks);

    gcmkONERROR(gckKERNEL_QueryIdleProfile(kernel, time, Timeslice, IdleTime));

    /* Release the mutex. */
    gcmkVERIFY_OK(gckOS_ReleaseMutex(Os, kernel->db->profMutex));

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    if (acquired)
    {
        gcmkVERIFY_OK(gckOS_ReleaseMutex(Os, kernel->db->profMutex));
    }

    gcmkFOOTER();
    return status;
}

gceSTATUS
gckOS_QueryPulseCountProfile(
    IN gckOS Os,
    IN gceCORE Core,
    IN gcePulseEaterDomain Domain,
    OUT gctUINT32_PTR DutyCycle)
{
    if(has_feat_pulse_eater_profiler())
    {
        gctUINT32 busyRatio, totalTime, timeSlice;
        gceSTATUS status;
        gckKERNEL kernel;

        gcmkHEADER_ARG("Os=0x%x", Os);
        gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
        gcmkVERIFY_ARGUMENT(DutyCycle != gcvNULL);

        kernel = Os->device->kernels[Core];
        timeSlice = DEF_MIN_SAMPLING_RATE;

        gcmkONERROR(gckKERNEL_QueryPulseEaterIdleProfile(kernel,
                                                         gcvFALSE,
                                                         Domain,
                                                         &busyRatio,
                                                         &timeSlice,
                                                         &totalTime));

        *DutyCycle = totalTime?((busyRatio * 100)/totalTime):0;

OnError:
        gcmkFOOTER();
        return status;
    }
    else
    {
        *DutyCycle = 100;

        return gcvSTATUS_OK;
    }
}

gceSTATUS
gckOS_PowerOffWhenIdle(
    IN gckOS Os,
    IN gctBOOL NeedProfile
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%x NeedProfile=%d", Os, NeedProfile);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    if(NeedProfile)
    {
        gcmkONERROR(_IdleProfile(Os, &Os->device->needPowerOff));

        if(Os->device->profilerDebug)
        {
            gcmkPRINT("needPowerOff: %d\n", Os->device->needPowerOff);
        }
    }

    if(Os->device->needPowerOff)
    {
        if(Os->device->profilerDebug)
        {
            gcmkPRINT("power off 3D, %d\n", Os->device->kernels[gcvCORE_MAJOR]->hardware->chipPowerState);
        }
        gcmkONERROR(gckOS_Broadcast(Os, Os->device->kernels[gcvCORE_MAJOR]->hardware, gcvBROADCAST_IDLE_PROFILE));

        if(Os->device->powerDebug)
        {
            gcmkPRINT("done.\n");
        }
    }

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:

    /* Return status. */
    gcmkFOOTER();
    return status;
}

#if MRVL_CONFIG_ENABLE_GPUFREQ
gceSTATUS
gckOS_GPUFreqNotifierRegister(
    IN gckOS Os,
    IN gctPOINTER NotifierBlockHandler
    )
{
    gceSTATUS status;
    struct notifier_block *nb = gcvNULL;

    gcmkHEADER_ARG("Os = 0x%08X, NotifierBlockHandler = 0x%08X", Os, NotifierBlockHandler);
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(NotifierBlockHandler != gcvNULL);

    nb = (struct notifier_block *) NotifierBlockHandler;

    status = srcu_notifier_chain_register(&Os->nb_list_head, nb);
    if(status)
    {
        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
}

gceSTATUS
gckOS_GPUFreqNotifierUnregister(
    IN gckOS Os,
    IN gctPOINTER NotifierBlockHandler
    )
{
    gceSTATUS status;
    struct notifier_block *nb = gcvNULL;

    gcmkHEADER_ARG("Os = 0x%08X, NotifierBlockHandler = 0x%08X", Os, NotifierBlockHandler);
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(NotifierBlockHandler != gcvNULL);

    nb = (struct notifier_block *) NotifierBlockHandler;

    status = srcu_notifier_chain_unregister(&Os->nb_list_head, nb);
    if(status)
    {
        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
}

gceSTATUS
gckOS_GPUFreqNotifierCallChain(
    IN gckOS Os,
    IN gceGPUFreqEvent Event,
    IN gctPOINTER Data
)
{
    gcmkHEADER_ARG("Os = 0x%08X, Event = %d, Data = 0x%08X", Os, Event, Data);
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    srcu_notifier_call_chain(&Os->nb_list_head, (unsigned long)Event, Data);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}
#endif

/******************************************************************************\
******************************** Flush Cache **********************************
\******************************************************************************/
# if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))

#if MRVL_OLD_FLUSHCACHE
gceSTATUS gckOS_FlushCache(
    IN gctSIZE_T start,
    IN gctSIZE_T length,
    IN gctINT direction
    )
{
    return gcvSTATUS_OK;
}
#else
gceSTATUS
gckOS_FlushCache(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctSIZE_T start,
    IN gctSIZE_T length,
    IN gctINT direction
    )
{
    return gcvSTATUS_OK;
}
#endif

# else

#define BMM_HAS_PTE_PAGE
#if MRVL_OLD_FLUSHCACHE
static gctSIZE_T uva_to_pa(struct mm_struct *mm, gctSIZE_T addr)
{
    gctSIZE_T ret = 0UL;
    pgd_t *pgd;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;

    pgd = pgd_offset(mm, addr);
    if (!pgd_none(*pgd))
    {
        pud = pud_offset(pgd, addr);
        if (!pud_none(*pud))
        {
            pmd = pmd_offset(pud, addr);
            if (!pmd_none(*pmd))
            {
                pte = pte_offset_map(pmd, addr);
                if (!pte_none(*pte) && pte_present(*pte))
                {
#ifdef BMM_HAS_PTE_PAGE
                    /* Use page struct */
                    struct page *page = pte_page(*pte);
                    if(page)
                    {
                        ret = page_to_phys(page);
                        ret |= (addr & (PAGE_SIZE-1));
                    }
#else
                    /* Use hard PTE */
                    pte = (pte_t *)((u32)pte - 2048);
                    if(pte)
                    {
                        ret = (*pte & 0xfffff000)
                            | (addr & 0xfff);
                    }
#endif
                }
            }
        }
    }
    return ret;
}

static void map_area(gctSIZE_T start, gctSIZE_T size, gctINT dir)
{
    gctSIZE_T end = start + size;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
    if(dir == DMA_BIDIRECTIONAL)
    {
        dmac_flush_range((void *)start, (void *)end);
    }
    else
    {
        dmac_map_area((void *)start, size, dir);
    }
#else
    switch (dir)
    {
    case DMA_FROM_DEVICE:   /* invalidate only */
        dmac_inv_range((void *)start, (void *)end);
        break;
    case DMA_TO_DEVICE:     /* writeback only */
        dmac_clean_range((void *)start, (void *)end);
        break;
    case DMA_BIDIRECTIONAL: /* writeback and invalidate */
        dmac_flush_range((void *)start, (void *)end);
        break;
    default:
        BUG();
    }
#endif
}

gceSTATUS gckOS_FlushCache(
    IN gctSIZE_T start,
    IN gctSIZE_T length,
    IN gctINT direction
    )
{
    /* flush cache which comes form user space */
    gctSIZE_T paddr = 0;
    gctINT len = 0;
    gctINT size = length;
    struct mm_struct *mm = current->mm;

    if ((start == 0) || (size == 0))
    {
        BUG();
    }

    if (start != PAGE_ALIGN(start))
    {
        len = PAGE_ALIGN(start) - start;
        /* in case length is samller than the first offset */
        len = (size < len) ? size : len;
    }
    else if (size >= PAGE_SIZE)
    {
        len = PAGE_SIZE;
    }
    else
    {
        len = size;
    }

    map_area(start, length, direction);

    do
    {
        spin_lock(&mm->page_table_lock);
        paddr = uva_to_pa(mm, start);
        spin_unlock(&mm->page_table_lock);

        switch (direction)
        {
        case DMA_FROM_DEVICE:   /* invalidate only */
            outer_inv_range(paddr, paddr + len);
            break;
        case DMA_TO_DEVICE:     /* writeback only */
            outer_clean_range(paddr, paddr + len);
            break;
        case DMA_BIDIRECTIONAL: /* writeback and invalidate */
            outer_flush_range(paddr, paddr + len);
            break;
        default:
            BUG();
        }

        size -= len;
        start += len;
        len = (size >= PAGE_SIZE) ? PAGE_SIZE : size;
    } while (size > 0);

    return gcvSTATUS_OK;
}

#else

gceSTATUS gckOS_FlushCache(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctSIZE_T Memory,
    IN gctSIZE_T length,
    IN gctINT direction
    )
{
    gctSIZE_T pageCount, i;
    gctUINT32 physical = ~0U;
    gctUINTPTR_T start, end, memory;
    gctINT result = 0;
    gceSTATUS status;
    gctBOOL isLocked = gcvFALSE;
    gctBOOL isStartChaDir = gcvFALSE;
    gctBOOL isEndChaDir = gcvFALSE;

    struct page **pages = gcvNULL;
    unsigned long pfn;
    gcmkHEADER_ARG("Memory=0x%x length=%d direction=0x%x ", Memory, length, direction);
    do
    {
            memory = (gctUINTPTR_T) Memory;
            /* Get the number of required pages. */
            end = (memory + length + PAGE_SIZE - 1) >> PAGE_SHIFT;
            start = memory >> PAGE_SHIFT;
            pageCount = end - start;

            /*If the start or end of address isn't page align, GC would get the align address,
                    which would enlarge the scope of buffer, for the invalidate flush operation,
                    it is risk for  needless flsuh buffer when do invalidate operation,
                    it is safe for needless flush buffer  when do DMA_BIDIRECTIONAL flush,*/
            isStartChaDir  = ((memory&(~PAGE_MASK)))&&(direction==DMA_FROM_DEVICE);
            isEndChaDir    = ((memory+length)&(~PAGE_MASK))&&(direction==DMA_FROM_DEVICE);

            /* Overflow. */
            if ((memory + length) < memory)
            {
                gcmkFOOTER_ARG("status=%d", gcvSTATUS_INVALID_ARGUMENT);
                return gcvSTATUS_INVALID_ARGUMENT;
            }


            /* Allocate the array of page addresses. */
            pages = (struct page **)kmalloc(pageCount * sizeof(struct page *), GFP_KERNEL | __GFP_NOWARN);

            if (pages == gcvNULL)
            {
                status = gcvSTATUS_OUT_OF_MEMORY;
                break;
            }

            MEMORY_MAP_LOCK(Os);
            isLocked = gcvTRUE;
            {
                /* Get the user pages. */
                down_read(&current->mm->mmap_sem);
                result = get_user_pages(current,
                        current->mm,
                        memory & PAGE_MASK,
                        pageCount,
                        1,
                        0,
                        pages,
                        gcvNULL
                        );
                up_read(&current->mm->mmap_sem);

                if (result <=0 || result < pageCount)
                {
                    struct vm_area_struct *vma;

                    /* Free the page table. */
                    if (pages != gcvNULL)
                    {
                        /* Release the pages if any. */
                        if (result > 0)
                        {
                            for (i = 0; i < result; i++)
                            {
                                if (pages[i] == gcvNULL)
                                {
                                    break;
                                }

                                page_cache_release(pages[i]);
                            }
                        }

                        gckOS_ZeroMemory(pages, pageCount * sizeof(struct page *));
                    }

                    vma = find_vma(current->mm, memory);

                    if (vma && (vma->vm_flags & VM_PFNMAP) )
                    {
                        gctINT ret;
                        start = memory & PAGE_MASK;
                        ret = follow_pfn(vma, start, &pfn);
                        if(ret)
                        {
                            gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
                        }

                        pages[0] = pfn_to_page(pfn);
                        get_page(pages[0]);
                        physical = (pfn << PAGE_SHIFT) | (memory & ~PAGE_MASK);

                        if (((Os->device->kernels[Core]->hardware->mmuVersion == 0)
                            && !((physical - Os->device->baseAddress) & 0x80000000))
                            || (Os->device->kernels[Core]->hardware->mmuVersion != 0) )
                        {
#if MRVL_GC_FLUSHCACHE_PFN
                            ;
#else
                            kfree(pages);
                            pages = gcvNULL;
                            status = gcvSTATUS_OK;
                            break;

#endif
                        }
                        else
                        {
                            gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
                        }

                        for (i = 1, start += PAGE_SIZE; i < pageCount; ++i, start += PAGE_SIZE)
                        {
                            follow_pfn(vma, start, &pfn);
                            if(ret)
                            {
                                gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
                            }
                            pages[i] = pfn_to_page(pfn);
                            get_page(pages[i]);
                        }
                    }
                    else
                    {
                        gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
                    }
                }
            }

            if (pages)
            {

                for (i = 0; i < pageCount; i++)
                {

                   if(pages[i] != gcvNULL)
                   {
                       if(i==0&&isStartChaDir)
                       {
                            __dma_page_cpu_to_dev(pages[i], 0, PAGE_SIZE, DMA_BIDIRECTIONAL);
                       }
                       else if(i!=0&&i==pageCount-1&&isEndChaDir)
                       {
                            __dma_page_cpu_to_dev(pages[i], 0, PAGE_SIZE, DMA_BIDIRECTIONAL);
                       }
                       else
                       {
                            __dma_page_cpu_to_dev(pages[i], 0, PAGE_SIZE, DMA_BIDIRECTIONAL);
                       }

                       page_cache_release(pages[i]);
                  }

                }

                kfree(pages);
                pages = gcvNULL;
            }
            else if(physical != ~0U)
            {
                gckOS_Log(_GFX_LOG_NOTIFY_, ">>>>WARN gckOS_FlushCache physical patch should not come in >>>>");
            }
            status = gcvSTATUS_OK;
    }while (gcvFALSE);
OnError:

    if (gcmIS_ERROR(status))
    {
        /* Release page array. */
        if (result > 0 && pages != gcvNULL)
        {
            for (i = 0; i < result; i++)
            {
                if (pages[i] == gcvNULL)
                {
                    break;
                }
                page_cache_release(pages[i]);
            }
        }

        if (pages != gcvNULL)
        {
            /* Free the page table. */
            kfree(pages);
            pages = gcvNULL;
        }

    }
    if(isLocked)
    {
        MEMORY_MAP_UNLOCK(Os);
    }
    /* Return the status. */
    gcmkFOOTER();

    return status;
}

#endif

# endif

/******************************************************************************\
******************************** Software Timer ********************************
\******************************************************************************/

void
_TimerFunction(
    struct work_struct * work
    )
{
    gcsOSTIMER_PTR timer = (gcsOSTIMER_PTR)work;

    gctTIMERFUNCTION function = timer->function;

    function(timer->data);
}

/*******************************************************************************
**
**  gckOS_CreateTimer
**
**  Create a software timer.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to the gckOS object.
**
**      gctTIMERFUNCTION Function.
**          Pointer to a call back function which will be called when timer is
**          expired.
**
**      gctPOINTER Data.
**          Private data which will be passed to call back function.
**
**  OUTPUT:
**
**      gctPOINTER * Timer
**          Pointer to a variable receiving the created timer.
*/
gceSTATUS
gckOS_CreateTimer(
    IN gckOS Os,
    IN gctTIMERFUNCTION Function,
    IN gctPOINTER Data,
    OUT gctPOINTER * Timer
    )
{
    gceSTATUS status;
    gcsOSTIMER_PTR pointer;
    gcmkHEADER_ARG("Os=0x%X Function=0x%X Data=0x%X", Os, Function, Data);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Timer != gcvNULL);

    gcmkONERROR(gckOS_Allocate(Os, sizeof(gcsOSTIMER), (gctPOINTER)&pointer));

    pointer->function = Function;
    pointer->data = Data;

    INIT_DELAYED_WORK(&pointer->work, _TimerFunction);

    *Timer = pointer;

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckOS_DestroyTimer
**
**  Destory a software timer.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to the gckOS object.
**
**      gctPOINTER Timer
**          Pointer to the timer to be destoryed.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_DestroyTimer(
    IN gckOS Os,
    IN gctPOINTER Timer
    )
{
    gcsOSTIMER_PTR timer;
    gcmkHEADER_ARG("Os=0x%X Timer=0x%X", Os, Timer);

    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Timer != gcvNULL);

    timer = (gcsOSTIMER_PTR)Timer;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)
    cancel_delayed_work_sync(&timer->work);
#else
    cancel_delayed_work(&timer->work);
    flush_workqueue(Os->workqueue);
#endif

    gcmkVERIFY_OK(gcmkOS_SAFE_FREE(Os, Timer));

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_StartTimer
**
**  Schedule a software timer.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to the gckOS object.
**
**      gctPOINTER Timer
**          Pointer to the timer to be scheduled.
**
**      gctUINT32 Delay
**          Delay in milliseconds.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_StartTimer(
    IN gckOS Os,
    IN gctPOINTER Timer,
    IN gctUINT32 Delay
    )
{
    gcsOSTIMER_PTR timer;

    gcmkHEADER_ARG("Os=0x%X Timer=0x%X Delay=%u", Os, Timer, Delay);

    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Timer != gcvNULL);
    gcmkVERIFY_ARGUMENT(Delay != 0);

    timer = (gcsOSTIMER_PTR)Timer;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
    mod_delayed_work(Os->workqueue, &timer->work, msecs_to_jiffies(Delay));
#else
    if (unlikely(delayed_work_pending(&timer->work)))
    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)
        cancel_delayed_work_sync(&timer->work);
#else
        cancel_delayed_work(&timer->work);
        flush_workqueue(Os->workqueue);
#endif
    }

    queue_delayed_work(Os->workqueue, &timer->work, msecs_to_jiffies(Delay));
#endif

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_StopTimer
**
**  Cancel a unscheduled timer.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to the gckOS object.
**
**      gctPOINTER Timer
**          Pointer to the timer to be cancel.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckOS_StopTimer(
    IN gckOS Os,
    IN gctPOINTER Timer
    )
{
    gcsOSTIMER_PTR timer;
    gcmkHEADER_ARG("Os=0x%X Timer=0x%X", Os, Timer);

    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Timer != gcvNULL);

    timer = (gcsOSTIMER_PTR)Timer;

    cancel_delayed_work(&timer->work);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_StopTimerSync(
    IN gckOS Os,
    IN gctPOINTER Timer
    )
{
    gcsOSTIMER_PTR timer;
    gcmkHEADER_ARG("Os=0x%X Timer=0x%X", Os, Timer);

    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Timer != gcvNULL);

    timer = (gcsOSTIMER_PTR)Timer;

    cancel_delayed_work_sync(&timer->work);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_GetProcessNameByPid(
    IN gctINT Pid,
    IN gctSIZE_T Length,
    OUT gctUINT8_PTR String
    )
{
    struct task_struct *task;

    /* Get the task_struct of the task with pid. */
    rcu_read_lock();

    task = FIND_TASK_BY_PID(Pid);

    if (task == gcvNULL)
    {
        rcu_read_unlock();
        return gcvSTATUS_NOT_FOUND;
    }

    /* Get name of process. */
    strncpy(String, task->comm, Length);

    rcu_read_unlock();

    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_ModifyPulseEaterPollingPeriod(
    IN gckOS Os,
    IN gctUINT32 Freq,
    IN gctUINT32 Core
    )
{
    if(has_feat_pulse_eater_profiler())
    {
        gckHARDWARE hardware;
        gctUINT32 core;

        core = (Core == gcvCORE_SH)? gcvCORE_MAJOR: Core;

        hardware = Os->device->kernels[core]->hardware;

        gcmkVERIFY_OK(gckOS_AcquireMutex(Os, hardware->pulseEaterMutex, gcvINFINITE));
        gckHARDWARE_ModifyPeriod(hardware, Freq);
        gcmkVERIFY_OK(gckOS_ReleaseMutex(Os, hardware->pulseEaterMutex));
    }

    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_DumpCallStack(
    IN gckOS Os
    )
{
    gcmkHEADER_ARG("Os=0x%X", Os);

    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    dump_stack();

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_DetectProcessByName
**
**      task->comm maybe part of process name, so this function
**      can only be used for debugging.
**
**  INPUT:
**
**      gctCONST_POINTER Name
**          Pointer to a string to hold name to be check. If the length
**          of name is longer than TASK_COMM_LEN (16), use part of name
**          to detect.
**
**  OUTPUT:
**
**      gcvSTATUS_TRUE if name of current process matches Name.
**
*/
gceSTATUS
gckOS_DetectProcessByName(
    IN gctCONST_POINTER Name
    )
{
    char comm[sizeof(current->comm)];

    memset(comm, 0, sizeof(comm));

    gcmkVERIFY_OK(
        gckOS_GetProcessNameByPid(_GetProcessID(), sizeof(current->comm), comm));

    return strstr(comm, Name) ? gcvSTATUS_TRUE
                              : gcvSTATUS_FALSE;
}

#if gcdANDROID_NATIVE_FENCE_SYNC

gceSTATUS
gckOS_CreateSyncPoint(
    IN gckOS Os,
    OUT gctSYNC_POINT * SyncPoint
    )
{
    gceSTATUS status;
    gcsSYNC_POINT_PTR syncPoint;

    gcmkHEADER_ARG("Os=0x%X", Os);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    /* Create an sync point structure. */
    syncPoint = (gcsSYNC_POINT_PTR) kmalloc(
            sizeof(gcsSYNC_POINT), GFP_KERNEL | gcdNOWARN);

    if (syncPoint == gcvNULL)
    {
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    /* Initialize the sync point. */
    atomic_set(&syncPoint->ref, 1);
    atomic_set(&syncPoint->state, 0);

    gcmkONERROR(_AllocateIntegerId(&Os->syncPointDB, syncPoint, &syncPoint->id));

    *SyncPoint = (gctSYNC_POINT)(gctUINTPTR_T)syncPoint->id;

    gcmkFOOTER_ARG("*SyncPonint=%d", syncPoint->id);
    return gcvSTATUS_OK;

OnError:
    if (syncPoint != gcvNULL)
    {
        kfree(syncPoint);
    }

    gcmkFOOTER();
    return status;
}

gceSTATUS
gckOS_ReferenceSyncPoint(
    IN gckOS Os,
    IN gctSYNC_POINT SyncPoint
    )
{
    gceSTATUS status;
    gcsSYNC_POINT_PTR syncPoint;

    gcmkHEADER_ARG("Os=0x%X", Os);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(SyncPoint != gcvNULL);

    status = _QueryIntegerId(&Os->syncPointDB,
                        (gctUINT32)(gctUINTPTR_T)SyncPoint,
                        (gctPOINTER)&syncPoint);

    /* gcvSTATUS_NOT_FOUND is a normal return value,
    ** since user level may already delete it.
    */
    if (status == gcvSTATUS_NOT_FOUND)
    {
        gcmkFOOTER();
        return gcvSTATUS_NOT_FOUND;
    }
    else
    {
        gcmkONERROR(status);
    }

    /* Initialize the sync point. */
    atomic_inc(&syncPoint->ref);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
}

gceSTATUS
gckOS_DestroySyncPoint(
    IN gckOS Os,
    IN gctSYNC_POINT SyncPoint
    )
{
    gceSTATUS status;
    gcsSYNC_POINT_PTR syncPoint;
    gctBOOL acquired = gcvFALSE;

    gcmkHEADER_ARG("Os=0x%X SyncPoint=%d", Os, (gctUINT32)(gctUINTPTR_T)SyncPoint);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(SyncPoint != gcvNULL);

    gcmkONERROR(gckOS_AcquireMutex(Os, Os->syncPointMutex, gcvINFINITE));
    acquired = gcvTRUE;

    gcmkONERROR(
        _QueryIntegerId(&Os->syncPointDB,
                        (gctUINT32)(gctUINTPTR_T)SyncPoint,
                        (gctPOINTER)&syncPoint));

    gcmkASSERT(syncPoint->id == (gctUINT32)(gctUINTPTR_T)SyncPoint);

    if (atomic_dec_and_test(&syncPoint->ref))
    {
        gcmkVERIFY_OK(_DestroyIntegerId(&Os->syncPointDB, syncPoint->id));

        /* Free the sgianl. */
        syncPoint->timeline = gcvNULL;
        kfree(syncPoint);
    }

    gcmkVERIFY_OK(gckOS_ReleaseMutex(Os, Os->syncPointMutex));
    acquired = gcvFALSE;

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    if (acquired)
    {
        /* Release the mutex. */
        gcmkVERIFY_OK(gckOS_ReleaseMutex(Os, Os->syncPointMutex));
    }

    gcmkFOOTER();
    return status;
}

gceSTATUS
gckOS_SignalSyncPoint(
    IN gckOS Os,
    IN gctSYNC_POINT SyncPoint
    )
{
    gceSTATUS status;
    gcsSYNC_POINT_PTR syncPoint;
    struct sync_timeline * timeline;
    gctBOOL acquired = gcvFALSE;

    gcmkHEADER_ARG("Os=0x%X SyncPoint=%d", Os, (gctUINT32)(gctUINTPTR_T)SyncPoint);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(SyncPoint != gcvNULL);

    gcmkONERROR(gckOS_AcquireMutex(Os, Os->syncPointMutex, gcvINFINITE));
    acquired = gcvTRUE;

    gcmkONERROR(
        _QueryIntegerId(&Os->syncPointDB,
                        (gctUINT32)(gctUINTPTR_T)SyncPoint,
                        (gctPOINTER)&syncPoint));

    gcmkASSERT(syncPoint->id == (gctUINT32)(gctUINTPTR_T)SyncPoint);

    /* Set signaled state. */
    atomic_set(&syncPoint->state, 1);

    /* Get parent timeline. */
    timeline = syncPoint->timeline;

    gcmkVERIFY_OK(gckOS_ReleaseMutex(Os, Os->syncPointMutex));
    acquired = gcvFALSE;

    /* Signal timeline. */
    if (timeline)
    {
        sync_timeline_signal(timeline);
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    if (acquired)
    {
        /* Release the mutex. */
        gcmkVERIFY_OK(gckOS_ReleaseMutex(Os, Os->syncPointMutex));
    }

    gcmkFOOTER();
    return status;
}

gceSTATUS
gckOS_QuerySyncPoint(
    IN gckOS Os,
    IN gctSYNC_POINT SyncPoint,
    OUT gctBOOL_PTR State
    )
{
    gceSTATUS status;
    gcsSYNC_POINT_PTR syncPoint;

    gcmkHEADER_ARG("Os=0x%X SyncPoint=%d", Os, (gctUINT32)(gctUINTPTR_T)SyncPoint);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(SyncPoint != gcvNULL);

    gcmkONERROR(
        _QueryIntegerId(&Os->syncPointDB,
                        (gctUINT32)(gctUINTPTR_T)SyncPoint,
                        (gctPOINTER)&syncPoint));

    gcmkASSERT(syncPoint->id == (gctUINT32)(gctUINTPTR_T)SyncPoint);

    /* Get state. */
    *State = atomic_read(&syncPoint->state);

    /* Success. */
    gcmkFOOTER_ARG("*State=%d", *State);
    return gcvSTATUS_OK;

OnError:
    gcmkFOOTER();
    return status;
}

gceSTATUS
gckOS_CreateSyncTimeline(
    IN gckOS Os,
    OUT gctHANDLE * Timeline
    )
{
    struct viv_sync_timeline * timeline;

    /* Create viv sync timeline. */
    timeline = viv_sync_timeline_create("viv-timeline", Os);

    if (timeline == gcvNULL)
    {
        /* Out of memory. */
        return gcvSTATUS_OUT_OF_MEMORY;
    }

    *Timeline = (gctHANDLE) timeline;
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_DestroySyncTimeline(
    IN gckOS Os,
    IN gctHANDLE Timeline
    )
{
    struct viv_sync_timeline * timeline;
    gcmkASSERT(Timeline != gcvNULL);

    /* Destroy timeline. */
    timeline = (struct viv_sync_timeline *) Timeline;
    sync_timeline_destroy(&timeline->obj);

    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_CreateNativeFence(
    IN gckOS Os,
    IN gctHANDLE Timeline,
    IN gctSYNC_POINT SyncPoint,
    OUT gctINT * FenceFD
    )
{
    int fd = -1;
    struct viv_sync_timeline *timeline;
    struct sync_pt * pt = gcvNULL;
    struct sync_fence * fence;
    char name[32];
    gcsSYNC_POINT_PTR syncPoint;
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%X Timeline=0x%X SyncPoint=%d",
                   Os, Timeline, (gctUINT)(gctUINTPTR_T)SyncPoint);

    gcmkONERROR(
        _QueryIntegerId(&Os->syncPointDB,
                        (gctUINT32)(gctUINTPTR_T)SyncPoint,
                        (gctPOINTER)&syncPoint));

    /* Cast timeline. */
    timeline = (struct viv_sync_timeline *) Timeline;

    fd = get_unused_fd();

    if (fd < 0)
    {
        /* Out of resources. */
        gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
    }

    /* Create viv_sync_pt. */
    pt = viv_sync_pt_create(timeline, SyncPoint);

    if (pt == gcvNULL)
    {
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    /* Reference sync_timeline. */
    syncPoint->timeline = &timeline->obj;

    /* Build fence name. */
    snprintf(name, 32, "viv-sync_fence-%u", (gctUINT)(gctUINTPTR_T)SyncPoint);

    /* Create sync_fence. */
    fence = sync_fence_create(name, pt);

    if (fence == NULL)
    {
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    /* Install fence to fd. */
    sync_fence_install(fence, fd);

    *FenceFD = fd;
    gcmkFOOTER_ARG("*FenceFD=%d", fd);
    return gcvSTATUS_OK;

OnError:
    /* Error roll back. */
    if (pt)
    {
        sync_pt_free(pt);
    }

    if (fd > 0)
    {
        put_unused_fd(fd);
    }

    gcmkFOOTER();
    return status;
}
#endif

typedef int (*PFUNC_GET_FREQS_TBL)(unsigned long *, unsigned int *, unsigned int);
#define HIGH_PERFORMANCE_FREQ   797333
gceSTATUS gckOS_QueryPlatPerformance(
    IN  gckOS Os,
    OUT gctBOOL* isHighPerfPlat
    )
{
    unsigned long freqs[10];
    unsigned int freqItemCount = 0;
    unsigned long maxFreq = 0;
    unsigned int idx;
    PFUNC_GET_FREQS_TBL get_freq_table;

    if (cpu_is_pxa1928())
    {
#if CONFIG_KALLSYMS
        get_freq_table = (PFUNC_GET_FREQS_TBL)kallsyms_lookup_name("get_gc3d_freqs_table");

#elif defined(CONFIG_ARM64)
extern int get_gc3d_freqs_table(unsigned long *gcu3d_freqs_table,
                        unsigned int *item_counts, unsigned int max_item_counts);

        get_freq_table = get_gc3d_freqs_table;
#else

        *isHighPerfPlat = gcvTRUE;
        return gcvSTATUS_OK;
#endif
        /*Get max freq in Hz.*/
        get_freq_table(freqs, &freqItemCount, 10);

        for ( idx = 0; idx < freqItemCount; idx++)
        {
            if (maxFreq < freqs[idx])
            {
                maxFreq = freqs[idx];
            }
        }

        if (maxFreq < HIGH_PERFORMANCE_FREQ * 1000)
        {
            *isHighPerfPlat = gcvFALSE;
        }
        else
        {
            *isHighPerfPlat = gcvTRUE;
        }
    }
    else
    {
        *isHighPerfPlat = gcvTRUE;
    }

    return gcvSTATUS_OK;
}

#if gcdSECURITY
gceSTATUS
gckOS_AllocatePageArray(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T PageCount,
    OUT gctPOINTER * PageArrayLogical,
    OUT gctPHYS_ADDR * PageArrayPhysical
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    PLINUX_MDL  mdl;
    gctUINT32*  table;
    gctUINT32   offset;
    gctSIZE_T   bytes;
    gckALLOCATOR allocator;

    gcmkHEADER_ARG("Os=0x%X Physical=0x%X PageCount=%u",
                   Os, Physical, PageCount);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);
    gcmkVERIFY_ARGUMENT(PageCount > 0);

    bytes = PageCount * gcmSIZEOF(gctUINT32);
    gcmkONERROR(gckOS_AllocateNonPagedMemory(
        Os,
        gcvFALSE,
        &bytes,
        PageArrayPhysical,
        PageArrayLogical
        ));

    table = *PageArrayLogical;

    /* Convert pointer to MDL. */
    mdl = (PLINUX_MDL)Physical;

    allocator = mdl->allocator;

     /* Get all the physical addresses and store them in the page table. */

    offset = 0;
    PageCount = PageCount / (PAGE_SIZE / 4096);

    /* Try to get the user pages so DMA can happen. */
    while (PageCount-- > 0)
    {
        unsigned long phys = ~0;

        if (mdl->pagedMem && !mdl->contiguous)
        {
            if (allocator)
            {
                gctPHYS_ADDR_T phys_addr;
                allocator->ops->Physical(allocator, mdl, offset, &phys_addr);
                phys = (unsigned long)phys_addr;
            }
        }
        else
        {
            if (!mdl->pagedMem)
            {
                gcmkTRACE_ZONE(
                    gcvLEVEL_INFO, gcvZONE_OS,
                    "%s(%d): we should not get this call for Non Paged Memory!",
                    __FUNCTION__, __LINE__
                    );
            }

            phys = page_to_phys(nth_page(mdl->u.contiguousPages, offset));
        }

        table[offset] = phys;

        offset += 1;
    }

OnError:

    /* Return the status. */
    gcmkFOOTER();
    return status;
}
#endif

gceSTATUS
gckOS_CPUPhysicalToGPUPhysical(
    IN gckOS Os,
    IN gctPHYS_ADDR_T CPUPhysical,
    IN gctPHYS_ADDR_T * GPUPhysical
    )
{
    gcsPLATFORM * platform;
    gcmkHEADER_ARG("CPUPhysical=0x%X", CPUPhysical);

    platform = Os->device->platform;

    if (platform && platform->ops->getGPUPhysical)
    {
        gcmkVERIFY_OK(
            platform->ops->getGPUPhysical(platform, CPUPhysical, GPUPhysical));
    }
    else
    {
        *GPUPhysical = CPUPhysical;
    }

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_GPUPhysicalToCPUPhysical(
    IN gckOS Os,
    IN gctUINT32 GPUPhysical,
    IN gctUINT32_PTR CPUPhysical
    )
{
    gcmkHEADER_ARG("GPUPhysical=0x%X", GPUPhysical);

    *CPUPhysical = GPUPhysical;

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_PhysicalToPhysicalAddress(
    IN gckOS Os,
    IN gctPOINTER Physical,
    OUT gctPHYS_ADDR_T * PhysicalAddress
    )
{
    PLINUX_MDL mdl = (PLINUX_MDL)Physical;
    gckALLOCATOR allocator = mdl->allocator;

    if (allocator)
    {
        return allocator->ops->Physical(allocator, mdl, 0, PhysicalAddress);
    }

    return gcvSTATUS_NOT_SUPPORTED;
}

gceSTATUS
gckOS_GetFreqTablePointer(
    IN gckOS Os,
    gctPOINTER *tf,
    gceCORE Core)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
    *tf = Os->freqTable[Core];
#else
    *tf = gcvNULL;
#endif

    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_QueryOption(
    IN gckOS Os,
    IN gctCONST_STRING Option,
    OUT gctUINT32 * Value
    )
{
    gckGALDEVICE device = Os->device;

    if (!strcmp(Option, "physBase"))
    {
        *Value = device->physBase;
        return gcvSTATUS_OK;
    }
    else if (!strcmp(Option, "physSize"))
    {
        *Value = device->physSize;
        return gcvSTATUS_OK;
    }
    else if (!strcmp(Option, "mmu"))
    {
#if gcdSECURITY
        *Value = 0;
#else
        *Value = device->mmu;
#endif
        return gcvSTATUS_OK;
    }
    else if (!strcmp(Option, "contiguousSize"))
    {
        *Value = device->contiguousSize;
        return gcvSTATUS_OK;
    }

    return gcvSTATUS_NOT_SUPPORTED;
}

static int
fd_release(
    struct inode *inode,
    struct file *file
    )
{
    gcsFDPRIVATE_PTR private = (gcsFDPRIVATE_PTR)file->private_data;

    if (private && private->release)
    {
        return private->release(private);
    }

    return 0;
}

static const struct file_operations fd_fops = {
    .release = fd_release,
};

gceSTATUS
gckOS_GetFd(
    IN gctSTRING Name,
    IN gcsFDPRIVATE_PTR Private,
    OUT gctINT *Fd
    )
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
    *Fd = anon_inode_getfd(Name, &fd_fops, Private, O_RDWR);

    if (*Fd < 0)
    {
        return gcvSTATUS_OUT_OF_RESOURCES;
    }

    return gcvSTATUS_OK;
#else
    return gcvSTATUS_NOT_SUPPORTED;
#endif
}

