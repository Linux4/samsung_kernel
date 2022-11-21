/****************************************************************************
*
*    Copyright (c) 2005 - 2012 by Vivante Corp.
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
#include "gc_trace.h"

#include <linux/pagemap.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <asm/atomic.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/idr.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,23)
#include <linux/math64.h>
#endif
#include <linux/delay.h>

#if MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
#include "ion.h"
#include "pxa/pxa_ion.h"
#   else
#include <linux/ion.h>
#include <linux/pxa_ion.h>
#   endif
#elif (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_PMEM)
#include <linux/android_pmem.h>
#endif

extern void gc_pwr(int boolEnable);
extern void gc2d_pwr(int power_on);

#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
extern struct ion_client *gc_ion_client;
#endif

#if gcdANDROID_NATIVE_FENCE_SYNC
#include <linux/file.h>
#include "gc_hal_kernel_sync.h"
#endif

#define _GC_OBJ_ZONE    gcvZONE_OS

/*******************************************************************************
***** Version Signature *******************************************************/

#ifdef ANDROID
const char * _PLATFORM = "\n\0$PLATFORM$Android$\n";
#else
const char * _PLATFORM = "\n\0$PLATFORM$Linux$\n";
#endif

#define USER_SIGNAL_TABLE_LEN_INIT  64
#define gcdSUPPRESS_OOM_MESSAGE 1
#define DUMP_LIMITATION         3

#define MEMORY_LOCK(os) \
    gcmkVERIFY_OK(gckOS_AcquireMutex( \
                                (os), \
                                (os)->memoryLock, \
                                gcvINFINITE))

#define MEMORY_UNLOCK(os) \
    gcmkVERIFY_OK(gckOS_ReleaseMutex((os), (os)->memoryLock))

#define MEMORY_MAP_LOCK(os) \
    gcmkVERIFY_OK(gckOS_AcquireMutex( \
                                (os), \
                                (os)->memoryMapLock, \
                                gcvINFINITE))

#define MEMORY_MAP_UNLOCK(os) \
    gcmkVERIFY_OK(gckOS_ReleaseMutex((os), (os)->memoryMapLock))

#define DUMP_CONTROL(Hardware) \
{ \
    static gctINT dump_cnt[gcdMAX_GPU_COUNT] = {0}; \
\
    if(dump_cnt[Hardware->core] < DUMP_LIMITATION) \
    { \
        dump_cnt[Hardware->core]++; \
        gcmkONERROR(gckHARDWARE_DumpGPUState(Hardware)); \
        gcmkVERIFY_OK(gckEVENT_Dump(Hardware->kernel->eventObj)); \
        gcmkVERIFY_OK(gckKERNEL_DumpProcessDB(Hardware->kernel)); \
    } \
}

/* Protection bit when mapping memroy to user sapce */
#define gcmkPAGED_MEMROY_PROT(x)    pgprot_writecombine(x)

#if gcdNONPAGED_MEMORY_BUFFERABLE
#define gcmkIOREMAP                 ioremap_wc
#define gcmkNONPAGED_MEMROY_PROT(x) pgprot_writecombine(x)
#elif !gcdNONPAGED_MEMORY_CACHEABLE
#define gcmkIOREMAP                 ioremap_nocache
#define gcmkNONPAGED_MEMROY_PROT(x) pgprot_noncached(x)
#endif

#if gcdSUPPRESS_OOM_MESSAGE
#define gcdNOWARN __GFP_NOWARN
#else
#define gcdNOWARN 0
#endif

#define gcdINFINITE_TIMEOUT     (60 * 1000)
#define gcdDETECT_TIMEOUT       0
#define gcdDETECT_DMA_ADDRESS   1
#define gcdDETECT_DMA_STATE     1

#define gcdUSE_NON_PAGED_MEMORY_CACHE 50

/******************************************************************************\
********************************** Structures **********************************
\******************************************************************************/
#if gcdUSE_NON_PAGED_MEMORY_CACHE
typedef struct _gcsNonPagedMemoryCache
{
#ifndef NO_DMA_COHERENT
    gctINT                           size;
    gctSTRING                        addr;
    dma_addr_t                       dmaHandle;
#else
    long                             order;
    struct page *                    page;
#endif

    struct _gcsNonPagedMemoryCache * prev;
    struct _gcsNonPagedMemoryCache * next;
}
gcsNonPagedMemoryCache;
#endif /* gcdUSE_NON_PAGED_MEMORY_CACHE */

typedef struct _gcsUSER_MAPPING * gcsUSER_MAPPING_PTR;
typedef struct _gcsUSER_MAPPING
{
    /* Pointer to next mapping structure. */
    gcsUSER_MAPPING_PTR         next;

    /* Physical address of this mapping. */
    gctUINT32                   physical;

    /* Logical address of this mapping. */
    gctPOINTER                  logical;

    /* Number of bytes of this mapping. */
    gctSIZE_T                   bytes;

    /* Starting address of this mapping. */
    gctINT8_PTR                 start;

    /* Ending address of this mapping. */
    gctINT8_PTR                 end;
}
gcsUSER_MAPPING;

typedef struct _gcsINTEGER_DB * gcsINTEGER_DB_PTR;
typedef struct _gcsINTEGER_DB
{
    struct idr                  idr;
    spinlock_t                  lock;
    gctINT                      curr;
}
gcsINTEGER_DB;

struct _gckRecursiveMutex
{
    /* Thread lock the mutex. */
    gctINT32                    pThread;
    /* Lock times. */
    gctUINT32                   nReference;
    /* Access mutex. */
    gctPOINTER                  accMutex;
    /* Underly mutex. */
    gctPOINTER                  undMutex;
};

struct _gcsSPINLOCK
{
    spinlock_t                  lock;
    gctSIZE_T                   flags;
};

struct _gckOS
{
    /* Object. */
    gcsOBJECT                   object;

    /* Heap. */
    gckHEAP                     heap;

    /* Pointer to device */
    gckGALDEVICE                device;

    /* Memory management */
    gctPOINTER                  memoryLock;
    gctPOINTER                  memoryMapLock;

    struct _LINUX_MDL           *mdlHead;
    struct _LINUX_MDL           *mdlTail;

    gctUINT32                   baseAddress;
    /* Nested power/clock enable/disable of 2D/3D chip. */
    gctUINT32                   clockDepth;
    gctUINT32                   powerDepth;
    gctUINT32                   maxClockDepth;
    gctUINT32                   maxPowerDepth;

    gctPOINTER                  pmCtrlAtom;
    gctPOINTER                  pmCtrlLock;

#if MRVL_CONFIG_ENABLE_GPUFREQ
    struct srcu_notifier_head   nb_list_head;
#endif

    /* Kernel process ID. */
    gctUINT32                   kernelProcessID;

    /* Signal management. */

    /* Lock. */
    gctPOINTER                  signalMutex;

    /* signal id database. */
    gcsINTEGER_DB               signalDB;

#if gcdANDROID_NATIVE_FENCE_SYNC
    /* Lock. */
    gctPOINTER                  syncPointMutex;

    /* sync point id database. */
    gcsINTEGER_DB               syncPointDB;
#endif

    gcsUSER_MAPPING_PTR         userMap;
    gctPOINTER                  debugLock;

#if gcdUSE_NON_PAGED_MEMORY_CACHE
    gctUINT                      cacheSize;
    gcsNonPagedMemoryCache *     cacheHead;
    gcsNonPagedMemoryCache *     cacheTail;
#endif

    /* critical power/clock operations */
    struct rw_semaphore         rwsem_clk_pwr;

    /* workqueue for os timer. */
    struct workqueue_struct *   workqueue;

    /* record idle stamp*/
    gctUINT32                   idleStamp;

#if MRVL_DUMP_GC_INFO
    /* vars for dump mutex info*/
    spinlock_t mutexDumpLock;
    struct list_head headAcquired;
    struct list_head headBlocked;
#endif
};

typedef struct _gcsSIGNAL * gcsSIGNAL_PTR;
typedef struct _gcsSIGNAL
{
    /* Kernel sync primitive. */
    struct completion obj;

    /* Manual reset flag. */
    gctBOOL manualReset;

    /* The reference counter. */
    atomic_t ref;

    /* The owner of the signal. */
    gctHANDLE process;

    gckHARDWARE hardware;

    /* ID. */
    gctUINT32 id;
}
gcsSIGNAL;

#if gcdANDROID_NATIVE_FENCE_SYNC
typedef struct _gcsSYNC_POINT * gcsSYNC_POINT_PTR;
typedef struct _gcsSYNC_POINT
{
    /* The reference counter. */
    atomic_t ref;

    /* State. */
    atomic_t state;

    /* timeline. */
    struct sync_timeline * timeline;

    /* ID. */
    gctUINT32 id;
}
gcsSYNC_POINT;
#endif

typedef struct _gcsPageInfo * gcsPageInfo_PTR;
typedef struct _gcsPageInfo
{
    struct page **pages;
    gctUINT32_PTR pageTable;
}
gcsPageInfo;

typedef struct _gcsOSTIMER * gcsOSTIMER_PTR;
typedef struct _gcsOSTIMER
{
    struct delayed_work     work;
    gctTIMERFUNCTION        function;
    gctPOINTER              data;
} gcsOSTIMER;

/******************************************************************************\
******************************* Private Functions ******************************
\******************************************************************************/

static gctINT
_GetProcessID(
    void
    )
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
    return task_tgid_vnr(current);
#else
    return current->tgid;
#endif
}

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
    IN gctINT ProcessID
    )
{
    PLINUX_MDL  mdl;

    gcmkHEADER_ARG("ProcessID=%d", ProcessID);

    mdl = (PLINUX_MDL)kmalloc(sizeof(struct _LINUX_MDL), GFP_KERNEL | __GFP_NOWARN);
    if (mdl == gcvNULL)
    {
        gcmkFOOTER_NO();
        return gcvNULL;
    }

    gcmkVERIFY_OK(gckOS_ZeroMemory(mdl, gcmSIZEOF(struct _LINUX_MDL)));
    mdl->pid    = ProcessID;
    mdl->maps   = gcvNULL;
    mdl->prev   = gcvNULL;
    mdl->next   = gcvNULL;
#if (MRVL_VIDEO_MEMORY_USE_TYPE != gcdMEM_TYPE_NONE)
    mdl->region     = gcvNULL;
    mdl->numPages   = 0;
    mdl->pagedMem   = 0;
    mdl->bPmem      = gcvFALSE;
    mdl->contiguous = gcvTRUE;
#   if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
    mdl->ionHandle  = gcvNULL;
#   endif
#endif

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

#if (MRVL_VIDEO_MEMORY_USE_TYPE != gcdMEM_TYPE_NONE)
    if(Mdl->region != gcvNULL)
    {
        kfree(Mdl->region);
        Mdl->region = gcvNULL;
    }

#   if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
    if (Mdl->ionHandle)
    {
        ion_free(gc_ion_client, Mdl->ionHandle);
        Mdl->ionHandle = gcvNULL;
    }
#   endif
#endif
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

    mdlMap = (PLINUX_MDL_MAP)kmalloc(sizeof(struct _LINUX_MDL_MAP), GFP_KERNEL | gcdNOWARN);
    if (mdlMap == gcvNULL)
    {
        gcmkFOOTER_NO();
        return gcvNULL;
    }

    mdlMap->pid     = ProcessID;
    mdlMap->vmaAddr = gcvNULL;
    mdlMap->vma     = gcvNULL;
    mdlMap->lockCount = 0;

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
    gcmkVERIFY_ARGUMENT(Mdl != gcvNULL);
    gcmkVERIFY_ARGUMENT(MdlMap != gcvNULL);
    gcmkASSERT(Mdl->maps != gcvNULL);

    if((Mdl != gcvNULL) && (Mdl->maps == gcvNULL))
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
            if(prevMdlMap == gcvNULL)
            {
                gcmkFOOTER_NO();
                return gcvSTATUS_NOT_FOUND;
            }
        }

        prevMdlMap->next = MdlMap->next;
    }

    kfree(MdlMap);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

static PLINUX_MDL_MAP
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

void
OnProcessExit(
    IN gckOS Os,
    IN gckKERNEL Kernel
    )
{
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
static inline int
is_vmalloc_addr(
    void *Addr
    )
{
    unsigned long addr = (unsigned long)Addr;

    return addr >= VMALLOC_START && addr < VMALLOC_END;
}
#endif

static void
_NonContiguousFree(
    IN struct page ** Pages,
    IN gctUINT32 NumPages
    )
{
    gctINT i;

    gcmkHEADER_ARG("Pages=0x%X, NumPages=%d", Pages, NumPages);

    gcmkASSERT(Pages != gcvNULL);

    for (i = 0; i < NumPages; i++)
    {
        __free_page(Pages[i]);
    }

    if (is_vmalloc_addr(Pages))
    {
        vfree(Pages);
    }
    else
    {
        kfree(Pages);
    }

    gcmkFOOTER_NO();
}

static struct page **
_NonContiguousAlloc(
    IN gctUINT32 NumPages
    )
{
    struct page ** pages;
    struct page *p;
    gctINT i, size;

    gcmkHEADER_ARG("NumPages=%lu", NumPages);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    if (NumPages > totalram_pages)
#else
    if (NumPages > num_physpages)
#endif
    {
        gcmkFOOTER_NO();
        return gcvNULL;
    }

    size = NumPages * sizeof(struct page *);

    pages = kmalloc(size, GFP_KERNEL | gcdNOWARN);

    if (!pages)
    {
        pages = vmalloc(size);

        if (!pages)
        {
            gcmkFOOTER_NO();
            return gcvNULL;
        }
    }

    for (i = 0; i < NumPages; i++)
    {

        p = alloc_page(GFP_KERNEL | __GFP_NOWARN | __GFP_MOVABLE | __GFP_HIGHMEM );

        if (!p)
        {
            _NonContiguousFree(pages, i);
            gcmkFOOTER_NO();
            return gcvNULL;
        }

        pages[i] = p;
    }

    gcmkFOOTER_ARG("pages=0x%X", pages);
    return pages;
}

static inline struct page *
_NonContiguousToPage(
    IN struct page ** Pages,
    IN gctUINT32 Index
    )
{
    gcmkASSERT(Pages != gcvNULL);
    return Pages[Index];
}

static inline unsigned long
_NonContiguousToPfn(
    IN struct page ** Pages,
    IN gctUINT32 Index
    )
{
    gcmkASSERT(Pages != gcvNULL);
    return page_to_pfn(_NonContiguousToPage(Pages, Index));
}

static inline unsigned long
_NonContiguousToPhys(
    IN struct page ** Pages,
    IN gctUINT32 Index
    )
{
    gcmkASSERT(Pages != gcvNULL);
    return page_to_phys(_NonContiguousToPage(Pages, Index));
}

#if gcdUSE_NON_PAGED_MEMORY_CACHE

static gctBOOL
_AddNonPagedMemoryCache(
    gckOS Os,
#ifndef NO_DMA_COHERENT
    gctINT Size,
    gctSTRING Addr,
    dma_addr_t DmaHandle
#else
    long Order,
    struct page * Page
#endif
    )
{
    gcsNonPagedMemoryCache *cache;

    if (Os->cacheSize >= gcdUSE_NON_PAGED_MEMORY_CACHE)
    {
        return gcvFALSE;
    }

    /* Allocate the cache record */
    cache = (gcsNonPagedMemoryCache *)kmalloc(sizeof(gcsNonPagedMemoryCache), GFP_ATOMIC);

    if (cache == gcvNULL) return gcvFALSE;

#ifndef NO_DMA_COHERENT
    cache->size  = Size;
    cache->addr  = Addr;
    cache->dmaHandle = DmaHandle;
#else
    cache->order = Order;
    cache->page  = Page;
#endif

    /* Add to list */
    if (Os->cacheHead == gcvNULL)
    {
        cache->prev   = gcvNULL;
        cache->next   = gcvNULL;
        Os->cacheHead =
        Os->cacheTail = cache;
    }
    else
    {
        /* Add to the tail. */
        cache->prev         = Os->cacheTail;
        cache->next         = gcvNULL;
        Os->cacheTail->next = cache;
        Os->cacheTail       = cache;
    }

    Os->cacheSize++;

    return gcvTRUE;
}

#ifndef NO_DMA_COHERENT
static gctSTRING
_GetNonPagedMemoryCache(
    gckOS Os,
    gctINT Size,
    dma_addr_t * DmaHandle
    )
#else
static struct page *
_GetNonPagedMemoryCache(
    gckOS Os,
    long Order
    )
#endif
{
    gcsNonPagedMemoryCache *cache;
#ifndef NO_DMA_COHERENT
    gctSTRING addr;
#else
    struct page * page;
#endif

    if (Os->cacheHead == gcvNULL) return gcvNULL;

    /* Find the right cache */
    cache = Os->cacheHead;

    while (cache != gcvNULL)
    {
#ifndef NO_DMA_COHERENT
        if (cache->size == Size) break;
#else
        if (cache->order == Order) break;
#endif

        cache = cache->next;
    }

    if (cache == gcvNULL) return gcvNULL;

    /* Remove the cache from list */
    if (cache == Os->cacheHead)
    {
        Os->cacheHead = cache->next;

        if (Os->cacheHead == gcvNULL)
        {
            Os->cacheTail = gcvNULL;
        }
    }
    else
    {
        cache->prev->next = cache->next;

        if (cache == Os->cacheTail)
        {
            Os->cacheTail = cache->prev;
        }
        else
        {
            cache->next->prev = cache->prev;
        }
    }

    /* Destroy cache */
#ifndef NO_DMA_COHERENT
    addr       = cache->addr;
    *DmaHandle = cache->dmaHandle;
#else
    page       = cache->page;
#endif

    kfree(cache);

    Os->cacheSize--;

#ifndef NO_DMA_COHERENT
    return addr;
#else
    return page;
#endif
}

static void
_FreeAllNonPagedMemoryCache(
    gckOS Os
    )
{
    gcsNonPagedMemoryCache *cache, *nextCache;

    MEMORY_LOCK(Os);

    cache = Os->cacheHead;

    while (cache != gcvNULL)
    {
        if (cache != Os->cacheTail)
        {
            nextCache = cache->next;
        }
        else
        {
            nextCache = gcvNULL;
        }

        /* Remove the cache from list */
        if (cache == Os->cacheHead)
        {
            Os->cacheHead = cache->next;

            if (Os->cacheHead == gcvNULL)
            {
                Os->cacheTail = gcvNULL;
            }
        }
        else
        {
            cache->prev->next = cache->next;

            if (cache == Os->cacheTail)
            {
                Os->cacheTail = cache->prev;
            }
            else
            {
                cache->next->prev = cache->prev;
            }
        }

#ifndef NO_DMA_COHERENT
    dma_free_coherent(gcvNULL,
                    cache->size,
                    cache->addr,
                    cache->dmaHandle);
#else
    free_pages((unsigned long)page_address(cache->page), cache->order);
#endif

        kfree(cache);

        cache = nextCache;
    }

    MEMORY_UNLOCK(Os);
}

#endif /* gcdUSE_NON_PAGED_MEMORY_CACHE */

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

    if (!result)
    {
        Database->curr = *Id;
    }

    spin_unlock(&Database->lock);

    idr_preload_end();

    if (result < 0)
    {
        return gcvSTATUS_OUT_OF_RESOURCES;
    }

    *Id = result;
#else
again:
    if (idr_pre_get(&Database->idr, GFP_KERNEL | gcdNOWARN) == 0)
    {
        return gcvSTATUS_OUT_OF_MEMORY;
    }

    spin_lock(&Database->lock);

    next = (Database->curr + 1 <= 0) ? 1 : Database->curr + 1;

    /* Try to get a id greater than current id. */
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

static void
_UnmapUserLogical(
    IN gctINT Pid,
    IN gctPOINTER Logical,
    IN gctUINT32  Size
)
{
    if (unlikely(current->mm == gcvNULL))
    {
        /* Do nothing if process is exiting. */
        return;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
    if (vm_munmap((unsigned long)Logical, Size) < 0)
    {
        gcmkTRACE_ZONE(
                gcvLEVEL_WARNING, gcvZONE_OS,
                "%s(%d): vm_munmap failed",
                __FUNCTION__, __LINE__
                );
    }
#else
    down_write(&current->mm->mmap_sem);
    if (do_munmap(current->mm, (unsigned long)Logical, Size) < 0)
    {
        gcmkTRACE_ZONE(
                gcvLEVEL_WARNING, gcvZONE_OS,
                "%s(%d): do_munmap failed",
                __FUNCTION__, __LINE__
                );
    }
    up_write(&current->mm->mmap_sem);
#endif
}

#if MRVL_DUMP_GC_INFO
#ifndef MaxLen
#define MaxLen 162
#endif

typedef struct _MutexRecord
{
    gctCHAR             funcName[50];
    gctUINT32           lineNum;
    gctPOINTER          mutex;
    gctUINT32           pid;
    gctUINT32           tid;
    struct list_head    list;
}gcsMutexRecord;

typedef enum _gceMutexStatus
{
    gcvMutexBlocked     = 0,
    gcvMutexAcquired    = 1,
    gcvMutexFailed      = 2
}gceMutexStatus;

/* Mutex Dump */
void AddMutexRecord(gckOS Os,
                            gctCONST_STRING FuncName,
                            gctUINT32 LineNum,
                            gctPOINTER Mutex,
                            gceMutexStatus Status)
{
    gctINT pid, tid;
    gcsMutexRecord * newRecord = gcvNULL;
    gcsMutexRecord * pos =gcvNULL;
    gcsMutexRecord * tmp =gcvNULL;

    gckOS_GetProcessID(&pid);
    gckOS_GetThreadID(&tid);

    if(Status == gcvMutexBlocked)
    {
        /* Allocate memory for new record*/
        gckOS_AllocateMemory(gcvNULL, gcmSIZEOF(gcsMutexRecord), (gctPOINTER)&newRecord);
        gckOS_Sprintf(newRecord->funcName, "%s", FuncName);
        newRecord->lineNum  = LineNum;
        newRecord->mutex    = Mutex;
        newRecord->pid      = pid;
        newRecord->tid      = tid;

        /* if mutex is blocked, it'll be add in Blocked list first*/
        spin_lock(&(Os->mutexDumpLock));
        list_add(&newRecord->list, &(Os->headBlocked));
        spin_unlock(&(Os->mutexDumpLock));
    }
    else if(Status == gcvMutexAcquired)
    {
        /* if mutex is acquired, it'll be deleted in Blocked list*/
        spin_lock(&(Os->mutexDumpLock));
        list_for_each_entry_safe(pos, tmp, &(Os->headBlocked), list)
        {
            if(pos->mutex == Mutex && pos->tid == tid && pos->pid == pid)
            {
                list_del(&pos->list);
                break;
            }
        }

        /* And'll be added in Acquired list*/
        list_add(&pos->list, &(Os->headAcquired));
        spin_unlock(&(Os->mutexDumpLock));
    }
    else if(Status == gcvMutexFailed)
    {
        /* No need, delete and free it*/
        spin_lock(&(Os->mutexDumpLock));
        list_for_each_entry_safe(pos, tmp, &(Os->headBlocked), list)
        {
            if(pos->mutex == Mutex && pos->tid == tid && pos->pid == pid)
            {
                list_del(&pos->list);
                gckOS_FreeMemory(gcvNULL, pos);
                pos = gcvNULL;
                break;
            }
        }
        spin_unlock(&(Os->mutexDumpLock));
    }
}

void DelMutexRecord(gckOS Os, gctCONST_STRING FuncName, gctPOINTER Mutex)
{
    gcsMutexRecord * pos =gcvNULL;
    gcsMutexRecord * tmp =gcvNULL;
    gctUINT32        tid, pid;

    gckOS_GetProcessID(&pid);
    gckOS_GetThreadID(&tid);

    /*if mutex released, traverse the record list to
                find it and delete it*/
    spin_lock(&(Os->mutexDumpLock));
    list_for_each_entry_safe(pos, tmp, &(Os->headAcquired), list)
    {
        if(pos->mutex == Mutex && pos->tid == tid && pos->pid == pid)
        {
            list_del(&pos->list);
            gckOS_FreeMemory(gcvNULL, pos);
            pos = gcvNULL;
            break;
        }
    }
    spin_unlock(&(Os->mutexDumpLock));
}

gceSTATUS
_DumpMutexInfo(gckOS Os)
{
    gctCHAR filename[28], record[MaxLen], i = 0;
    gcsMutexRecord * pos =gcvNULL;
    gcsMutexRecord * tmp =gcvNULL;
    gctPOINTER file = gcvNULL;
    struct task_struct * task = gcvNULL;
    gctSIZE_T old_fs = 0, len = 0;
    struct list_head *head;
    gceSTATUS status;
    gctBOOL   isAcquired = gcvFALSE;

    gcmkHEADER_ARG("Os=0x%x", Os);

    head = &(Os->headAcquired);

    for(i=0; i<2; i++)
    {
        if( i == 0)
        {
            gcmkONERROR(gckOS_Sprintf(filename, "/data/gpudump/MutexAcquire"));
        }
        else
        {
            gcmkONERROR(gckOS_Sprintf(filename, "/data/gpudump/MutexBlocked"));
        }

        /* Open File */
        gcmkONERROR(gckOS_OpenFile(Os, filename, &file, &old_fs));

        /* Acquire lock*/
        spin_lock(&(Os->mutexDumpLock));
        isAcquired = gcvTRUE;

        /* Dump All Mutex in chain table*/
        list_for_each_entry_safe(pos, tmp, head, list)
        {
            gctCHAR taskname[30];
            gctINT j;

            if(i == 0)
            {
                gcmkONERROR(gckOS_Sprintf(record, "Grab mutex function: "));
            }
            else
            {
                gcmkONERROR(gckOS_Sprintf(record, "Mutex is blocked in: "));
            }

            gcmkONERROR(gckOS_StrLen(record, &len));
            gcmkONERROR(gckOS_Sprintf(record+len, "%s", pos->funcName));
            gcmkONERROR(gckOS_StrLen(record, &len));

            gcmkONERROR(gckOS_Sprintf(record+len, " Line:%4d, pid: %4d, tid: %4d ",
                                                                pos->lineNum,
                                                                pos->pid,
                                                                pos->tid));
            gcmkONERROR(gckOS_StrLen(record, &len));

            /* Find task_struct pointer by pid*/
            task   =  FIND_TASK_BY_PID((pid_t)pos->pid);
            if(task!=gcvNULL)
            {
                /* Get Process Name */
                j = 0;
                strcpy(taskname, task->comm);
                gcmkONERROR(gckOS_StrLen((gctCONST_STRING)taskname, (gctSIZE_T *)&j));
                taskname[j++] = '\n';
                taskname[j]   = '\0';

                gcmkONERROR(gckOS_Sprintf(record+len, "%s", taskname));
                gcmkONERROR(gckOS_StrLen(record, &len));
            }
            else
            {
                gcmkONERROR(gckOS_Sprintf(record+len, "Process name not found...  \n"));
                gcmkONERROR(gckOS_StrLen(record, &len));
            }

            /* Dump Data*/
            gcmkONERROR(gckOS_WriteFile(Os, file, record, len));

            /* Delete it in table after dump*/
            list_del(&pos->list);

            /* Free it */
            gckOS_FreeMemory(Os, pos);
            pos= gcvNULL;
        }

        spin_unlock(&Os->mutexDumpLock);
        isAcquired = gcvFALSE;

        /* Close File */
        gcmkONERROR(gckOS_CloseFile(Os, file, old_fs));
        file = gcvNULL;

        head = &(Os->headBlocked);
    }

    head = gcvNULL;

    /* Success */
    gcmkFOOTER();
    return gcvSTATUS_OK;
OnError:

    if(isAcquired)
    {
        spin_unlock(&Os->mutexDumpLock);
    }

    if(file != gcvNULL)
    {
        gckOS_CloseFile(Os, file, old_fs);
        file =gcvNULL;
    }

    /* Return the status. */
    gcmkFOOTER();
    return status;
}
#endif

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
_QueryProcessPageTable(
    IN gctPOINTER Logical,
    OUT gctUINT32 * Address
    )
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

    /* IMPORTANT! No heap yet. */
    os->heap = gcvNULL;

    /* Initialize the memory lock. */
    gcmkONERROR(gckOS_CreateMutex(os, &os->memoryLock));
    gcmkONERROR(gckOS_CreateMutex(os, &os->memoryMapLock));

    /* Create debug lock mutex. */
    gcmkONERROR(gckOS_CreateMutex(os, &os->debugLock));
    gcmkONERROR(gckOS_CreateMutex(os, &os->pmCtrlLock));
    gcmkONERROR(gckOS_AtomConstruct(os, &os->pmCtrlAtom));

    os->mdlHead = os->mdlTail = gcvNULL;

    os->baseAddress = os->device->baseAddress;

    os->maxClockDepth = 1;
    if(cpu_is_pxa1088())
        os->maxPowerDepth = 2;
    else
        os->maxPowerDepth = 1;

    os->clockDepth  = os->maxClockDepth;
    os->powerDepth  = os->maxPowerDepth;

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

#if MRVL_DUMP_GC_INFO
    /* Initialize mutex dump mutex*/
    spin_lock_init(&(os->mutexDumpLock));
    INIT_LIST_HEAD(&(os->headAcquired));
    INIT_LIST_HEAD(&(os->headBlocked));
#endif

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

#if gcdUSE_NON_PAGED_MEMORY_CACHE
    os->cacheSize = 0;
    os->cacheHead = gcvNULL;
    os->cacheTail = gcvNULL;
#endif

    init_rwsem(&os->rwsem_clk_pwr);

    /* Create a workqueue for os timer. */
    os->workqueue = alloc_workqueue("galcore workqueue", WQ_HIGHPRI | WQ_MEM_RECLAIM,  0);;

    if (os->workqueue == gcvNULL)
    {
        /* Out of memory. */
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

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

    if (os->heap != gcvNULL)
    {
        gcmkVERIFY_OK(
            gckHEAP_Destroy(os->heap));
    }

    if (os->pmCtrlAtom!= gcvNULL)
    {
        gcmkVERIFY_OK(
            gckOS_AtomDestroy(os, os->pmCtrlAtom));
    }

    if(os->pmCtrlLock != gcvNULL)
    {
        gcmkVERIFY_OK(
            gckOS_DeleteMutex(os, os->pmCtrlLock));
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
    gckHEAP heap;

    gcmkHEADER_ARG("Os=0x%X", Os);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

#if gcdUSE_NON_PAGED_MEMORY_CACHE
    _FreeAllNonPagedMemoryCache(Os);
#endif

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

    if (Os->heap != gcvNULL)
    {
        /* Mark gckHEAP as gone. */
        heap     = Os->heap;
        Os->heap = gcvNULL;

        /* Destroy the gckHEAP object. */
        gcmkVERIFY_OK(gckHEAP_Destroy(heap));
    }

    /* Destroy the memory lock. */
    gcmkVERIFY_OK(gckOS_DeleteMutex(Os, Os->memoryMapLock));
    gcmkVERIFY_OK(gckOS_DeleteMutex(Os, Os->memoryLock));
    gcmkVERIFY_OK(gckOS_AtomDestroy(Os, Os->pmCtrlAtom));
    gcmkVERIFY_OK(gckOS_DeleteMutex(Os, Os->pmCtrlLock));

    /* Destroy debug lock mutex. */
    gcmkVERIFY_OK(gckOS_DeleteMutex(Os, Os->debugLock));

    /* Wait for all works done. */
    flush_workqueue(Os->workqueue);

    /* Destory work queue. */
    destroy_workqueue(Os->workqueue);

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
#if 0
#ifdef NO_DMA_COHERENT
static gctSTRING
_CreateKernelVirtualMapping(
    IN struct page * Page,
    IN gctINT NumPages
    )
{
    gctSTRING addr = 0;

#if gcdNONPAGED_MEMORY_CACHEABLE
    addr = page_address(Page);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
    struct page ** pages;
    gctINT i;

    pages = kmalloc(sizeof(struct page *) * NumPages, GFP_KERNEL | __GFP_NOWARN);

    if (!pages)
    {
        return gcvNULL;
    }

    for (i = 0; i < NumPages; i++)
    {
        pages[i] = nth_page(Page, i);
    }

    /* ioremap() can't work on system memory since 2.6.38. */
    addr = vmap(pages, NumPages, 0, gcmkNONPAGED_MEMROY_PROT(PAGE_KERNEL));

    kfree(pages);
#else
    addr = gcmkIOREMAP(page_to_phys(Page), NumPages * PAGE_SIZE);
#endif

    return addr;
}

static void
_DestoryKernelVirtualMapping(
    IN gctSTRING Addr
    )
{
#if !gcdNONPAGED_MEMORY_CACHEABLE
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
    vunmap(Addr);
#   else
    iounmap(Addr);
#   endif
#endif
}
#endif
#endif
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

    /* Do we have a heap? */
    if (Os->heap != gcvNULL)
    {
        /* Allocate from the heap. */
        gcmkONERROR(gckHEAP_Allocate(Os->heap, Bytes, Memory));
    }
    else
    {
        gcmkONERROR(gckOS_AllocateMemory(Os, Bytes, Memory));
    }

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

    /* Do we have a heap? */
    if (Os->heap != gcvNULL)
    {
        /* Free from the heap. */
        gcmkONERROR(gckHEAP_Free(Os->heap, Memory));
    }
    else
    {
        gcmkONERROR(gckOS_FreeMemory(Os, Memory));
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
                "%s(%d): do_mmap error",
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
        if (dma_mmap_coherent(gcvNULL,
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

    /* Increment the lock count */
    mdlMap->lockCount++;

    *Logical = mdlMap->vmaAddr;

    MEMORY_UNLOCK(Os);

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

        if (--mdlMap->lockCount == 0)
        {
            _UnmapUserLogical(PID, mdlMap->vmaAddr, mdl->numPages * PAGE_SIZE);

            gcmkVERIFY_OK(_DestroyMdlMap(mdl, mdlMap));
        }
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
gceSTATUS
gckOS_UpdateVidMemUsage(
    IN gckOS Os,
    IN gctBOOL IsAllocated,
    IN gctSIZE_T Bytes,
    IN gceSURF_TYPE Type
);
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
#ifdef NO_DMA_COHERENT
    struct page * page = gcvNULL;
    long size, order, i;
    gctUINT32 retry = 0;
    gctPOINTER vaddr;
    struct page **pages;
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
    mdl = _CreateMdl(_GetProcessID());
    if (mdl == gcvNULL)
    {
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    mdl->pagedMem = 0;
    mdl->numPages = numPages;
    mdl->gcAddress = gcvINVALID_ADDRESS;
    mdl->wastSize  = bytes - (*Bytes);
    MEMORY_LOCK(Os);
    locked = gcvTRUE;

#ifndef NO_DMA_COHERENT
#if gcdUSE_NON_PAGED_MEMORY_CACHE
    addr = _GetNonPagedMemoryCache(Os,
                mdl->numPages * PAGE_SIZE,
                &mdl->dmaHandle);

    if (addr == gcvNULL)
#endif
    {
        addr = dma_alloc_coherent(gcvNULL,
                mdl->numPages * PAGE_SIZE,
                &mdl->dmaHandle,
                GFP_KERNEL | gcdNOWARN);
    }
#else
    size    = mdl->numPages * PAGE_SIZE;
    order   = get_order(size);
#if gcdUSE_NON_PAGED_MEMORY_CACHE
    page = _GetNonPagedMemoryCache(Os, order);

    if (page == gcvNULL)
#endif
    {
        /* If alloc_pages failes, retry it again. */
        do
        {
            page = alloc_pages(GFP_KERNEL | __GFP_NOWARN, order);

            /* Succeed then bail out; otherwise, retry. */
            if(page != gcvNULL)
            {
                break;
            }
        }while(++retry < gcdMAX_ALLOC_PAGES_RETRY);
    }

    if (page == gcvNULL)
    {
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    vaddr           = (gctPOINTER)page_address(page);
    mdl->dmaHandle  = virt_to_phys(vaddr);
    mdl->kaddr      = vaddr;

#if gcdNONPAGED_MEMORY_CACHEABLE
    addr = vaddr;
#else
    pages = vmalloc(sizeof(struct page*) * mdl->numPages);
    if(pages == gcvNULL)
    {
        gcmkPRINT("%s(%d): Out of memory to alloc pages* struct!", __FUNCTION__, __LINE__);
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    for(i = 0; i < mdl->numPages; i++)
    {
        pages[i] = phys_to_page(virt_to_phys(vaddr) + (i << PAGE_SHIFT));
    }

    /* For GC reserved memory, don't vmap the whole memory when calling this function at the first time. */
    if(*Bytes >= Os->device->contiguousSize)
    {
        addr = gcvNULL;
    }
    else
    {
#if gcdNONPAGED_MEMORY_BUFFERABLE
        addr = vmap(pages, mdl->numPages, 0, pgprot_writecombine(pgprot_kernel));
#else
        addr = vmap(pages, mdl->numPages, 0, pgprot_noncached(pgprot_kernel));
#endif
    }

    vfree(pages);
#endif

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

    if ((addr == gcvNULL) && (*Bytes < Os->device->contiguousSize))
    {
#ifdef NO_DMA_COHERENT
        gcmkPRINT("%s(%d): vmap allocation for size (%d pages) failed", __FUNCTION__, __LINE__, mdl->numPages);
#endif
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

// this codes needs refine
#if 0
    if ((Os->device->baseAddress & 0x80000000) != (mdl->dmaHandle & 0x80000000))
    {
        mdl->dmaHandle = (mdl->dmaHandle & ~0x80000000)
                       | (Os->device->baseAddress & 0x80000000);
    }
#endif

    mdl->addr = addr;

    /* Return allocated memory. */
    *Bytes = bytes;
    *Physical = (gctPHYS_ADDR) mdl;

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
                "%s(%d): do_mmap error",
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
        mdlMap->vma->vm_page_prot = gcmkNONPAGED_MEMROY_PROT(mdlMap->vma->vm_page_prot);
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

        /* Increment the lock count */
        mdlMap->lockCount++;

        *Logical = mdlMap->vmaAddr;
    }
    else
    {
        *Logical = (gctPOINTER)mdl->addr;
    }

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

    if(*Bytes < Os->device->contiguousSize)
    {
        Os->device->contiguousNonPagedMemUsage += bytes;
    }
    Os->device->wastBytes += mdl->wastSize;
    MEMORY_UNLOCK(Os);
    if(*Bytes < Os->device->contiguousSize)
        gckOS_UpdateVidMemUsage(Os, gcvTRUE, bytes, gcvSURF_TYPE_UNKNOWN);

    /* Success. */
    gcmkFOOTER_ARG("*Bytes=%lu *Physical=0x%X *Logical=0x%X",
                   *Bytes, *Physical, *Logical);
    return gcvSTATUS_OK;

OnError:
    if (mdl != gcvNULL)
    {
        if (mdlMap != gcvNULL)
        {
            /* Free LINUX_MDL_MAP. */
            gcmkVERIFY_OK(_DestroyMdlMap(mdl, mdlMap));
        }

        if (mdl->kaddr != gcvNULL)
        {
            /* Free pages if fails in mapping. */
            free_pages((gctUINT32)mdl->kaddr, get_order(mdl->numPages * PAGE_SIZE));
        }

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

#if (MRVL_VIDEO_MEMORY_USE_TYPE != gcdMEM_TYPE_NONE)
    gcmkVERIFY_ARGUMENT(mdl->bPmem != gcvTRUE);
#endif

    MEMORY_LOCK(Os);

#ifndef NO_DMA_COHERENT
#if gcdUSE_NON_PAGED_MEMORY_CACHE
    if (!_AddNonPagedMemoryCache(Os,
                                 mdl->numPages * PAGE_SIZE,
                                 mdl->addr,
                                 mdl->dmaHandle))
#endif
    {
        dma_free_coherent(gcvNULL,
                mdl->numPages * PAGE_SIZE,
                mdl->addr,
                mdl->dmaHandle);
    }
#else
    size    = mdl->numPages * PAGE_SIZE;
    vaddr   = mdl->kaddr;

    while (size > 0)
    {
        ClearPageReserved(virt_to_page(vaddr));

        vaddr   += PAGE_SIZE;
        size    -= PAGE_SIZE;
    }

#if gcdUSE_NON_PAGED_MEMORY_CACHE
    if (!_AddNonPagedMemoryCache(Os,
                                 get_order(mdl->numPages * PAGE_SIZE),
                                 virt_to_page(mdl->kaddr)))
#endif
    {
        free_pages((unsigned long)mdl->kaddr, get_order(mdl->numPages * PAGE_SIZE));
    }

    /*_DestoryKernelVirtualMapping(mdl->addr);*/
#if !gcdNONPAGED_MEMORY_CACHEABLE
    /* release virtual mapping obtained by vmap(). */
    if(mdl->addr != gcvNULL)
    {
        vunmap(mdl->addr);
    }
#endif
#endif /* NO_DMA_COHERENT */

    mdlMap = mdl->maps;

    while (mdlMap != gcvNULL)
    {
        if (mdlMap->vmaAddr != gcvNULL)
        {
            /* No mapped memory exists when free nonpaged memory */
            gcmkASSERT(0);
        }

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
    gcmkHEADER_ARG("Os=0x%X Core=%d Address=0x%X", Os, Core, Address);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Data != gcvNULL);

    kernel = Os->device->kernels[Core];

    down_read(&Os->rwsem_clk_pwr);

    if(kernel && kernel->hardware)
    {
        /* make sure both external and internal clock are ON.
            Exceptions:
            * REG[ AQHiClockControlRegAddrs ]
        */
        if ((Os->clockDepth != 0 && kernel->hardware->clk2D3D_Enable == gcvTRUE)
           )
        {
            trace_gc_reg_acc(TRACE_LOG_ENTRY, TRACE_REG_READ, Address, 0);
            *Data = readl((gctUINT8 *)Os->device->registerBases[Core] + Address);
            trace_gc_reg_acc(TRACE_LOG_EXIT, TRACE_REG_READ, Address, *Data);
        }
        else
        {
            *Data = 0;
            gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_OS, "GPU%d_REG[%#x] (%d, %d) reading failure!",
                Core, Address, Os->clockDepth, kernel->hardware->clk2D3D_Enable);
        }
    }
    else
    {
        trace_gc_reg_acc(TRACE_LOG_ENTRY, TRACE_REG_READ, Address, 0);
        *Data = readl((gctUINT8 *)Os->device->registerBases[Core] + Address);
        trace_gc_reg_acc(TRACE_LOG_EXIT, TRACE_REG_READ, Address, *Data);
    }

    up_read(&Os->rwsem_clk_pwr);

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
    gcmkHEADER_ARG("Os=0x%X, Address=0x%X", Os, Address);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Data != gcvNULL);

    /* read register directly */
    *Data = readl((gctUINT8 *)Os->device->registerBases[Core] + Address);

    /* Success. */
    gcmkFOOTER_ARG("*Data=0x%08x", *Data);
    return gcvSTATUS_OK;
}

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
    gcmkHEADER_ARG("Os=0x%X Core=%d Address=0x%X Data=0x%08x", Os, Core, Address, Data);

    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    kernel = Os->device->kernels[Core];

    down_write(&Os->rwsem_clk_pwr);

    if(kernel && kernel->hardware)
    {
        /* make sure both external and internal clock are ON.
            Exceptions:
            * REG[ AQHiClockControlRegAddrs ]
        */
        if (Address == 0x0000 ||
            (Os->clockDepth != 0 && kernel->hardware->clk2D3D_Enable == gcvTRUE)
           )
        {
            trace_gc_reg_acc(TRACE_LOG_ENTRY, TRACE_REG_WRITE, Address, Data);
            writel(Data, (gctUINT8 *)Os->device->registerBases[Core] + Address);
            trace_gc_reg_acc(TRACE_LOG_EXIT, TRACE_REG_WRITE, Address, 0);

            /* read out AQ_HI_CLOCK_CONTROL register, update clk2D3D_Enable to *TRUE*
                if and only if the last two bits, a.k.a. CLK2D_DIS and CLK3D_DIS are
                equal to *ZERO*
            */
            if(Address == 0x0000)
            {
                gctUINT32 value;
                trace_gc_reg_acc(TRACE_LOG_ENTRY, TRACE_REG_READ, Address, 0);
                value = readl((gctUINT8 *)Os->device->registerBases[Core] + Address);
                trace_gc_reg_acc(TRACE_LOG_EXIT, TRACE_REG_READ, Address, value);
                kernel->hardware->clk2D3D_Enable = ((value & 0x03) == 0x0);
            }
        }
        else
        {
            gcmkPRINT("GPU%d_REG[%#x] (%d, %d) writing data(0x%08x) failure!",
                Core, Address, Os->clockDepth, kernel->hardware->clk2D3D_Enable, Data);
        }
    }
    else
    {
        trace_gc_reg_acc(TRACE_LOG_ENTRY, TRACE_REG_WRITE, Address, Data);
        writel(Data, (gctUINT8 *)Os->device->registerBases[Core] + Address);
        trace_gc_reg_acc(TRACE_LOG_EXIT, TRACE_REG_WRITE, Address, 0);
    }

    up_write(&Os->rwsem_clk_pwr);

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

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
    OUT gctUINT32 * Address
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
            gckOS_GetPhysicalAddressProcess(Os, Logical, processID, Address));
    }

    /* Success. */
    gcmkFOOTER_ARG("*Address=0x%08x", *Address);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
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

static gceSTATUS
_ConvertLogical2Physical(
    IN gckOS Os,
    IN gctPOINTER Logical,
    IN gctUINT32 ProcessID,
    IN PLINUX_MDL Mdl,
    OUT gctUINT32_PTR Physical
    )
{
    gctINT8_PTR base, vBase;
    gctUINT32 offset;
    PLINUX_MDL_MAP map;
    gcsUSER_MAPPING_PTR userMap;

    base = (Mdl == gcvNULL) ? gcvNULL : (gctINT8_PTR) Mdl->addr;

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
            *Physical = _NonContiguousToPhys(Mdl->u.nonContiguousPages, offset/PAGE_SIZE);
            /*return gcvSTATUS_INVALID_ADDRESS;*/
        }
        else
        {
            *Physical = gcmPTR2INT(page_to_phys(Mdl->u.contiguousPages)) + offset;
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
                *Physical = _NonContiguousToPhys(Mdl->u.nonContiguousPages, offset/PAGE_SIZE);
            }
            else
            {
                *Physical = gcmPTR2INT(page_to_phys(Mdl->u.contiguousPages)) + offset;
            }

            return gcvSTATUS_OK;
        }
    }

    /* Address not yet found. */
    return gcvSTATUS_INVALID_ADDRESS;
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
gckOS_GetPhysicalAddressProcess(
    IN gckOS Os,
    IN gctPOINTER Logical,
    IN gctUINT32 ProcessID,
    OUT gctUINT32 * Address
    )
{
    PLINUX_MDL mdl;
    gctINT8_PTR base;
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
            status = _ConvertLogical2Physical(Os,
                                              Logical,
                                              ProcessID,
                                              mdl,
                                              Address);
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
**  gckOS_GetPhysicalAddressByHandle
**
**  Get the physical system address of a corresponding virtual address for a
**  given process by searching first in the passed mdl handle.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to gckOS object.
**
**      gctPOINTER Logical
**          Logical address.
**
**      gctPHYS_ADDR Handle
**          mdl handle of the memory, is NULL if it is from reserved video memory.
**
**  OUTPUT:
**
**      gctUINT32 * Address
**          Poinetr to a variable that receives the 32-bit physical adress.
*/

gceSTATUS gckOS_GetPhysicalAddressByHandle(
    IN gckOS Os,
    IN gctPOINTER Logical,
    IN gctPHYS_ADDR Handle,
    OUT gctUINT32 * Address
    )
{
    gceSTATUS status = gcvSTATUS_INVALID_ADDRESS;
    gctUINT32 processID;

    gcmkHEADER_ARG("Os=0x%X Logical=0x%X Handle=0x%X", Os, Logical, Handle);

    processID = _GetProcessID();


    if(Handle != gcvNULL)
    {
        MEMORY_LOCK(Os);

        status = _ConvertLogical2Physical(Os,
                                          Logical,
                                          processID,
                                          (PLINUX_MDL)Handle,
                                          Address);
        MEMORY_UNLOCK(Os);

        /* if convert fail, continue to convert by iterating all mdl handles in the os */
        if(gcmIS_ERROR(status))
        {
            status = gckOS_GetPhysicalAddress(Os,
                                              Logical,
                                              Address);
        }
    }
    else
    {
        status = gckOS_GetPhysicalAddress(Os,
                                          Logical,
                                          Address);
    }

    gcmkFOOTER_ARG("*Address=0x%08x", *Address);
    return status;
}

/*******************************************************************************
**
**  gckOS_GetGCPhysByMdl
**
**  Get the GC physical address from already save member of Mdl.
**
**  Limtion of gckOS_GetPhysicalAddressProcess, we are limited to only access
**  continues physical, allocpage or getfree_page physical. And gckOS_GetPhysicalAddressByHandle
**  enquick this logic to phys founding
**
**  By gckOS_SetGCPhysByMdl & gckOS_GetGCPhysByMdl support we could extend access
**  PMEM/ION physical,and Vmalloc's fake GCphysical(mmu setting table address)
**  That means ALL GCphys memory support. because we use save & read mood. IF mdl->gcAddress
**  gcvINVALID_ADDRESS . keep gckOS_GetPhysicalAddressByHandle logic.
**
**  We unify calling gckOS_SetGCPhysByMdl gckVIDMEM_Lock for user mode. For kernel
**  mode native alloc.such as cntx/command/queue we could setting after its allocate.
**  Together with MMU right setting .
**  Then USER allocate gcvHAL_ALLOCATE_LINEAR_VIDEO_MEMORY,and detail special kernel
**  alloc memory , we also choose PMEM and VIRTUAL.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to gckOS object.
**
**      gctPOINTER Logical
**          Logical address.
**
**      gctPHYS_ADDR PhysMdl
**          mdl handle of the memory, if NULL keep gckOS_GetPhysicalAddressProcess logic.
**                                    if exist but mdl->gcAddress invalidate gckOS_GetPhysicalAddressByHandle
**      gctUINT32 GCAddress
**          This GC Address will be setting on .
**
*/
gceSTATUS
gckOS_GetGCPhysByMdl(
    IN gckOS Os,
    IN gctPOINTER Logical,
    IN gctPHYS_ADDR PhysMdl,
    OUT gctUINT32 * Address
    )
{
    gceSTATUS status;
    PLINUX_MDL mdl;

    gcmkHEADER_ARG("Os=0x%X Logical=0x%X PhysMdl:0x%X", Os, Logical,PhysMdl);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Address != gcvNULL);
    mdl = (PLINUX_MDL) PhysMdl;
    MEMORY_LOCK(Os);
    if((PhysMdl != gcvNULL)&&(mdl->gcAddress != gcvINVALID_ADDRESS))
    {
        *Address = mdl->gcAddress;
        MEMORY_UNLOCK(Os);
    }
    else{
        MEMORY_UNLOCK(Os);
        gcmkONERROR(gckOS_GetPhysicalAddressByHandle(Os,Logical,PhysMdl,Address));
    }
    return gcvSTATUS_OK;

OnError:

    /* Return the status. */
    gcmkFOOTER();
    return status;
}
/*******************************************************************************
**
**  gckOS_SetGCPhysByMdl
**
**  Set the GC physical address to Mdl, then we could get by gckOS_GetGCPhysByMdl.
**
**  Limtion of gckOS_GetPhysicalAddressProcess, we are limited to only access
**  continues physical, allocpage or getfree_page physical. And gckOS_GetPhysicalAddressByHandle
**  enquick this logic to phys founding
**
**  By gckOS_SetGCPhysByMdl & gckOS_GetGCPhysByMdl support we could extend access
**  PMEM/ION physical,and Vmalloc's fake GCphysical(mmu setting table address)
**  That means ALL GCphys memory support. because we use save & read mood. IF mdl->gcAddress
**  gcvINVALID_ADDRESS . keep gckOS_GetPhysicalAddressByHandle logic.
**
**  We unify calling gckOS_SetGCPhysByMdl gckVIDMEM_Lock for user mode. For kernel
**  mode native alloc.such as cntx/command/queue we could setting after its allocate.
**  Together with MMU right setting .
**  Then USER allocate gcvHAL_ALLOCATE_LINEAR_VIDEO_MEMORY,and detail special kernel
**  alloc memory , we also choose PMEM and VIRTUAL.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to gckOS object.
**
**      gctPHYS_ADDR PhysMdl
**          mdl handle of the memory, if NULL we will do nothing.
**
**      gctUINT32 GCAddress
**          This GC Address will be setting on .
**
*/
gceSTATUS
gckOS_SetGCPhysByMdl(
    IN gckOS Os,
    IN gctPHYS_ADDR PhysMdl,
    IN gctUINT32 GCAddress
    )
{
    gceSTATUS status;
    PLINUX_MDL mdl;
    gcmkHEADER_ARG("Os=0x%X PhysMdl=0x%X", Os, PhysMdl);
    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(GCAddress != 0);
    if(GCAddress == gcvINVALID_ADDRESS)
        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    if(PhysMdl != gcvNULL)
    {
        mdl = (PLINUX_MDL) PhysMdl;
        MEMORY_LOCK(Os);
        mdl->gcAddress = GCAddress;
        MEMORY_UNLOCK(Os);
    }
    /* Success. */
    gcmkFOOTER_ARG("Address=0x%08x", GCAddress);
    return gcvSTATUS_OK;
OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
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

    if (mdl == gcvNULL)
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

            MEMORY_UNLOCK(Os);

            /* Out of resources. */
            gcmkFOOTER_ARG("status=%d", gcvSTATUS_OUT_OF_RESOURCES);
            return gcvSTATUS_OUT_OF_RESOURCES;
        }

        /* Return pointer to mapped memory. */
        *Logical = logical;
    }

    MEMORY_UNLOCK(Os);

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
        iounmap(Logical);
    }

    MEMORY_UNLOCK(Os);

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_FindMdlMap(
    IN gckOS Os,
    IN PLINUX_MDL Mdl,
    IN gctINT ProcessID,
    OUT PLINUX_MDL_MAP *MdlMap
    )
{
    PLINUX_MDL_MAP mdlMap = gcvNULL;

    gcmkHEADER_ARG("Os=0x%X, Mdl=0x%X, ProcessID=%d", Os, Mdl, ProcessID);

    /* Validate the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Mdl != gcvNULL);

    MEMORY_LOCK(Os);

    mdlMap = FindMdlMap(Mdl, ProcessID);

    if(mdlMap == gcvNULL)
    {
        MEMORY_UNLOCK(Os);
        *MdlMap = gcvNULL;

        gcmkFOOTER_ARG("MdlMap=0x%X", gcvNULL);
        return gcvSTATUS_NOT_FOUND;
    }

    MEMORY_UNLOCK(Os);

    *MdlMap = mdlMap;

    gcmkFOOTER_ARG("MdlMap=0x%X", *MdlMap);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_CreateMutex
**
**  Create a new mutex.
**
**  INPUT:
**
**      gckOS Os
**          Pointer to an gckOS object.
**
**  OUTPUT:
**
**      gctPOINTER * Mutex
**          Pointer to a variable that will hold a pointer to the mutex.
*/
gceSTATUS
gckOS_CreateMutexEx(
    IN struct lock_class_key *Key,
    IN gckOS Os,
    OUT gctPOINTER * Mutex
    )
{
    gceSTATUS status;

    gcmkHEADER_ARG("Os=0x%X", Os);

    /* Validate the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Mutex != gcvNULL);

    /* Allocate the mutex structure. */
    gcmkONERROR(gckOS_Allocate(Os, gcmSIZEOF(struct mutex), Mutex));

    /* Initialize the mutex. */
    mutex_init(*Mutex);
    lockdep_set_class((struct mutex*)(*Mutex),Key);
    /* Return status. */
    gcmkFOOTER_ARG("*Mutex=0x%X", *Mutex);
    return gcvSTATUS_OK;

OnError:
    /* Return status. */
    gcmkFOOTER();
    return status;
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
    mutex_destroy(Mutex);

#if MRVL_DUMP_GC_INFO
    DelMutexRecord(Os, gcvNULL, Mutex);
#endif

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
#if MRVL_DUMP_GC_INFO
gceSTATUS
gckOS_AcquireMutexEx(
    IN gckOS Os,
    IN gctPOINTER Mutex,
    IN gctUINT32 Timeout,
    IN gctCONST_STRING FuncName,
    IN gctUINT32 LineNum
    )
#else
gceSTATUS
gckOS_AcquireMutex(
    IN gckOS Os,
    IN gctPOINTER Mutex,
    IN gctUINT32 Timeout
    )
#endif
{
#if gcdDETECT_TIMEOUT
    gctUINT32 timeout;
#endif

    gcmkHEADER_ARG("Os=0x%X Mutex=0x%0x Timeout=%u", Os, Mutex, Timeout);

    /* Validate the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Mutex != gcvNULL);

#if gcdDETECT_TIMEOUT
    timeout = 0;

    for (;;)
    {
        /* Try to acquire the mutex. */
        if (mutex_trylock(Mutex))
        {
            /* Success. */
            gcmkFOOTER_NO();
            return gcvSTATUS_OK;
        }

        /* Advance the timeout. */
        timeout += 1;

        if (Timeout == gcvINFINITE)
        {
            if (timeout == gcdINFINITE_TIMEOUT)
            {
                gctUINT32 dmaAddress1, dmaAddress2;
                gctUINT32 dmaState1, dmaState2;

                dmaState1   = dmaState2   =
                dmaAddress1 = dmaAddress2 = 0;

                /* Verify whether DMA is running. */
                gcmkVERIFY_OK(_VerifyDMA(
                    Os, &dmaAddress1, &dmaAddress2, &dmaState1, &dmaState2
                    ));

#if gcdDETECT_DMA_ADDRESS
                /* Dump only if DMA appears stuck. */
                if (
                    (dmaAddress1 == dmaAddress2)
#if gcdDETECT_DMA_STATE
                 && (dmaState1   == dmaState2)
#      endif
                )
#   endif
                {
                    gcmkVERIFY_OK(_DumpGPUState(Os, gcvCORE_MAJOR));

                    gcmkPRINT(
                        "%s(%d): mutex 0x%X; forced message flush.",
                        __FUNCTION__, __LINE__, Mutex
                        );

                    /* Flush the debug cache. */
                    gcmkDEBUGFLUSH(dmaAddress2);
                }

                timeout = 0;
            }
        }
        else
        {
            /* Timedout? */
            if (timeout >= Timeout)
            {
                break;
            }
        }

        /* Wait for 1 millisecond. */
        gcmkVERIFY_OK(gckOS_Delay(Os, 1));
    }
#else

#if MRVL_DUMP_GC_INFO
    AddMutexRecord(Os, FuncName, LineNum, Mutex, gcvMutexBlocked);
#endif

    if (Timeout == gcvINFINITE)
    {
        /* Lock the mutex. */
        mutex_lock(Mutex);

#if MRVL_DUMP_GC_INFO
        AddMutexRecord(Os, FuncName, LineNum, Mutex, gcvMutexAcquired);
#endif

        /* Success. */
        gcmkFOOTER_NO();
        return gcvSTATUS_OK;
    }

    for (;;)
    {
        /* Try to acquire the mutex. */
        if (mutex_trylock(Mutex))
        {
#if MRVL_DUMP_GC_INFO
            AddMutexRecord(Os, FuncName, LineNum, Mutex, gcvMutexAcquired);
#endif
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

#if MRVL_DUMP_GC_INFO
        AddMutexRecord(Os, FuncName, LineNum, Mutex, gcvMutexFailed);
#endif

#endif

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
#if MRVL_DUMP_GC_INFO
gceSTATUS gckOS_ReleaseMutexEx(
    IN gckOS Os,
    IN gctPOINTER Mutex,
    IN gctCONST_STRING FuncName
    )
#else
gceSTATUS gckOS_ReleaseMutex(
    IN gckOS Os,
    IN gctPOINTER Mutex
    )
#endif
{
    gcmkHEADER_ARG("Os=0x%X Mutex=0x%0x", Os, Mutex);

    /* Validate the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Mutex != gcvNULL);

    /* Release the mutex. */
    mutex_unlock(Mutex);

#if MRVL_DUMP_GC_INFO
    DelMutexRecord(Os, FuncName, Mutex);
#endif

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/* Create a new spinlock. */
gceSTATUS
gckOS_CreateSpinlockEx(
    IN struct lock_class_key *Key,
    IN gckOS Os,
    OUT gcsSPINLOCK * Spinlock
    )
{
    gcmkHEADER_ARG("Os=0x%X", Os);

    /* Validate the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Spinlock != gcvNULL);

    /* Allocate a FAST_MUTEX structure. */
    *Spinlock = (gcsSPINLOCK)kmalloc(sizeof(struct _gcsSPINLOCK), GFP_KERNEL);

    if (*Spinlock == gcvNULL)
    {
        gcmkFOOTER_ARG("status=%d", gcvSTATUS_OUT_OF_MEMORY);
        return gcvSTATUS_OUT_OF_MEMORY;
    }

    /* Initialize the spin lock .. Come up in unlocked state. */
    spin_lock_init(&((*Spinlock)->lock));
    lockdep_set_class(&((*Spinlock)->lock),Key);

    /* Return status. */
    gcmkFOOTER_ARG("*Mutex=0x%X", *Spinlock);
    return gcvSTATUS_OK;
}

/* Delete a spinlock. */
gceSTATUS
gckOS_DeleteSpinlock(
    IN gckOS Os,
    IN gcsSPINLOCK Spinlock
    )
{
    gcmkHEADER_ARG("Os=0x%X Mutex=0x%X", Os, Spinlock);

    /* Validate the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Spinlock != gcvNULL);

    /* Delete the fast mutex. */
    kfree(Spinlock);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/* Acquire a spinlock. */
gceSTATUS
gckOS_Spinlock(
    IN gckOS Os,
    IN gcsSPINLOCK Spinlock,
    IN gceSPINLOCK_TYPE Type
    )
{
    gcmkHEADER_ARG("Os=0x%X Spinlock=0x%0x Type=%u", Os, Spinlock, Type);

    /* Validate the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Spinlock != gcvNULL);

    switch(Type)
    {
        case gcvSPINLOCK:
            spin_lock(&Spinlock->lock);
            break;
        case gcvSPINLOCK_BH:
            spin_lock_bh(&Spinlock->lock);
            break;
        case gcvSPINLOCK_IRQ:
            spin_lock_irq(&Spinlock->lock);
            break;
        case gcvSPINLOCK_IRQSAVE:
            spin_lock_irqsave(&Spinlock->lock, Spinlock->flags);
            break;
        default:
            /* Invalid format. */
            gcmkFOOTER_ARG("status=%d", gcvSTATUS_INVALID_ARGUMENT);
            return gcvSTATUS_INVALID_ARGUMENT;
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/* Release a spinlock. */
gceSTATUS
gckOS_Spinunlock(
    IN gckOS Os,
    IN gcsSPINLOCK Spinlock,
    IN gceSPINLOCK_TYPE Type
    )
{
    gcmkHEADER_ARG("Os=0x%X Spinlock=0x%0x Type=%u", Os, Spinlock, Type);

    /* Validate the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Spinlock != gcvNULL);

    switch(Type)
    {
        case gcvSPINLOCK:
            spin_unlock(&Spinlock->lock);
            break;
        case gcvSPINLOCK_BH:
            spin_unlock_bh(&Spinlock->lock);
            break;
        case gcvSPINLOCK_IRQ:
            spin_unlock_irq(&Spinlock->lock);
            break;
        case gcvSPINLOCK_IRQSAVE:
            spin_unlock_irqrestore(&Spinlock->lock, Spinlock->flags);
            break;
        default:
            /* Invalid format. */
            gcmkFOOTER_ARG("status=%d", gcvSTATUS_INVALID_ARGUMENT);
            return gcvSTATUS_INVALID_ARGUMENT;
    }

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
#if MRVL_DUMP_GC_INFO
gceSTATUS
gckOS_AcquireRecMutexEx(
    IN gckOS Os,
    IN gckRecursiveMutex Mutex,
    IN gctUINT32 Timeout,
    IN gctCONST_STRING FuncName,
    IN gctUINT  LineNum)
#else
gceSTATUS
gckOS_AcquireRecMutex(
    IN gckOS Os,
    IN gckRecursiveMutex Mutex,
    IN gctUINT32 Timeout)
#endif
{
    gceSTATUS status = gcvSTATUS_TIMEOUT;
    gctUINT32 tid;

    gcmkHEADER_ARG("Os=0x%X RecMutex=0x%0x", Os, Mutex);

    /* Validate the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Mutex != gcvNULL);

#if MRVL_DUMP_GC_INFO
    gckOS_AcquireMutexEx(Os, Mutex->accMutex, gcvINFINITE, FuncName, LineNum);
#else
    gckOS_AcquireMutex(Os, Mutex->accMutex, gcvINFINITE);
#endif

    /* Get current thread ID. */
    gckOS_GetThreadID(&tid);
    /* Locked by itself. */
    if (Mutex->pThread == tid)
    {
        Mutex->nReference++;
#if MRVL_DUMP_GC_INFO
        gckOS_ReleaseMutexEx(Os, Mutex->accMutex, FuncName);
#else
        gckOS_ReleaseMutex(Os, Mutex->accMutex);
#endif
        status = gcvSTATUS_OK;
    }
    else
    {
#if MRVL_DUMP_GC_INFO
        gckOS_ReleaseMutexEx(Os, Mutex->accMutex, FuncName);
        /* Try lock. */
        status = gckOS_AcquireMutexEx(Os, Mutex->undMutex, Timeout, FuncName, LineNum);
#else
        gckOS_ReleaseMutex(Os, Mutex->accMutex);
        /* Try lock. */
        status = gckOS_AcquireMutex(Os, Mutex->undMutex, Timeout);
#endif
        /* First time get the lock . */
        if (status == gcvSTATUS_OK)
        {
#if MRVL_DUMP_GC_INFO
            gckOS_AcquireMutexEx(Os, Mutex->accMutex, gcvINFINITE, FuncName, LineNum);
            Mutex->pThread = tid;
            Mutex->nReference = 1;
            gckOS_ReleaseMutexEx(Os, Mutex->accMutex, FuncName);
#else
            gckOS_AcquireMutex(Os, Mutex->accMutex, gcvINFINITE);
            Mutex->pThread = tid;
            Mutex->nReference = 1;
            gckOS_ReleaseMutex(Os, Mutex->accMutex);
#endif
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
#if MRVL_DUMP_GC_INFO
    gceSTATUS gckOS_ReleaseRecMutexEx(
        IN gckOS Os,
        IN gckRecursiveMutex Mutex,
        IN gctCONST_STRING FuncName,
        IN gctUINT  LineNum
        )
#else
    gceSTATUS gckOS_ReleaseRecMutex(
        IN gckOS Os,
        IN gckRecursiveMutex Mutex
        )
#endif
{
    gctUINT32 tid;

    gcmkHEADER_ARG("Os=0x%X RecMutex=0x%0x", Os, Mutex);

    /* Validate the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Mutex != gcvNULL);

#if MRVL_DUMP_GC_INFO
        gckOS_AcquireMutexEx(Os, Mutex->accMutex, gcvINFINITE, FuncName, LineNum);
#else
        gckOS_AcquireMutex(Os, Mutex->accMutex, gcvINFINITE);
#endif

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
#if MRVL_DUMP_GC_INFO
            gckOS_ReleaseMutexEx(Os, Mutex->undMutex, FuncName);
#else
            gckOS_ReleaseMutex(Os, Mutex->undMutex);
#endif
        }
    }

#if MRVL_DUMP_GC_INFO
    gckOS_ReleaseMutexEx(Os, Mutex->accMutex, FuncName);
#else
    gckOS_ReleaseMutex(Os, Mutex->accMutex);
#endif

    /* Success. */
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
    gcmkHEADER_ARG("Os=0x%X Target=0x%X NewValue=%u", Os, Target, NewValue);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    /* Exchange the pair of 32-bit values. */
    *OldValue = (gctUINT32) atomic_xchg((atomic_t *) Target, (int) NewValue);

    /* Success. */
    gcmkFOOTER_ARG("*OldValue=%u", *OldValue);
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
    gcmkHEADER_ARG("Os=0x%X Target=0x%X NewValue=0x%X", Os, Target, NewValue);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    /* Exchange the pair of pointers. */
    *OldValue = (gctPOINTER)(gctUINTPTR_T) atomic_xchg((atomic_t *) Target, (int)(gctUINTPTR_T) NewValue);

    /* Success. */
    gcmkFOOTER_ARG("*OldValue=0x%X", *OldValue);
    return gcvSTATUS_OK;
}

#if gcdSMP
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
#endif

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
        ktime_t delay = ktime_set(0, Delay * NSEC_PER_MSEC);
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
    gcmkHEADER();

    *Time = 0;

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
    gcmkONERROR(gckOS_AllocatePagedMemoryEx(Os, gcvFALSE, Bytes, Physical));

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
**      gctBOOL Contiguous
**          Need contiguous memory or not.
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
gckOS_AllocatePagedMemoryEx(
    IN gckOS Os,
    IN gctBOOL Contiguous,
    IN gctSIZE_T Bytes,
    OUT gctPHYS_ADDR * Physical
    )
{
    gctINT numPages;
    gctINT i;
    PLINUX_MDL mdl = gcvNULL;
    gctSIZE_T bytes;
    gctBOOL locked = gcvFALSE;
    gceSTATUS status;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27) && 0
    gctPOINTER addr = gcvNULL;
#endif

    gcmkHEADER_ARG("Os=0x%X Contiguous=%d Bytes=%lu", Os, Contiguous, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Bytes > 0);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);

    bytes = gcmALIGN(Bytes, PAGE_SIZE);
    numPages = GetPageCount(bytes, 0);

    mdl = _CreateMdl(_GetProcessID());
    if (mdl == gcvNULL)
    {
        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    if (Contiguous)
    {
        /* Get free pages, and suppress warning (stack dump) from kernel when
           we run out of memory. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27) && 0
        addr =
            alloc_pages_exact(numPages * PAGE_SIZE, GFP_KERNEL | __GFP_NOWARN | __GFP_NORETRY);

        mdl->u.contiguousPages = addr
                               ? virt_to_page(addr)
                               : gcvNULL;

        mdl->exact = gcvTRUE;
#else
        mdl->u.contiguousPages =
            alloc_pages(GFP_IOFS | __GFP_NOMEMALLOC | __GFP_NOWARN | __GFP_NORETRY | __GFP_NO_KSWAPD | GFP_NOWAIT, GetOrder(numPages));
#if 1
        if (mdl->u.contiguousPages == gcvNULL)
        {
            mdl->u.contiguousPages =
                alloc_pages(GFP_IOFS | __GFP_HIGHMEM | __GFP_MOVABLE | __GFP_NOWARN | __GFP_NORETRY | __GFP_NO_KSWAPD | GFP_NOWAIT, GetOrder(numPages));
        }
#endif
#endif
    }
    else
    {
        mdl->u.nonContiguousPages = _NonContiguousAlloc(numPages);
        if(mdl->u.nonContiguousPages == gcvNULL)
        {
            gcmkPRINT("%s: nonContiguousPages failure, size: %#x\n", __FUNCTION__, bytes);
        }
    }

    if (mdl->u.contiguousPages == gcvNULL && mdl->u.nonContiguousPages == gcvNULL)
    {
        status = gcvSTATUS_OUT_OF_MEMORY;
        goto OnError;
    }

    mdl->dmaHandle  = 0;
    mdl->addr       = 0;
    mdl->numPages   = numPages;
    mdl->pagedMem   = 1;
    mdl->contiguous = Contiguous;
    mdl->gcAddress  = gcvINVALID_ADDRESS;
    mdl->wastSize   = bytes - Bytes;
    for (i = 0; i < mdl->numPages; i++)
    {
        struct page *page;

        if (mdl->contiguous)
        {
            page = nth_page(mdl->u.contiguousPages, i);
        }
        else
        {
            page = _NonContiguousToPage(mdl->u.nonContiguousPages, i);
        }

        SetPageReserved(page);

        if (!PageHighMem(page) && page_to_phys(page))
        {
            gcmkVERIFY_OK(
                gckOS_CacheFlush(Os, _GetProcessID(), gcvNULL,
                                 (gctPOINTER)(gctUINTPTR_T)page_to_phys(page),
                                 page_address(page),
                                 PAGE_SIZE));
        }
    }

    /* Return physical address. */
    *Physical = (gctPHYS_ADDR) mdl;

    MEMORY_LOCK(Os);
    locked = gcvTRUE;

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

    if(Contiguous)
    {
        Os->device->contiguousPagedMemUsage += bytes;
    }
    else
    {
        Os->device->virtualPagedMemUsage += bytes;
    }
    Os->device->wastBytes += mdl->wastSize;

    MEMORY_UNLOCK(Os);

    /* Success. */
    gcmkFOOTER_ARG("*Physical=0x%X", *Physical);
    return gcvSTATUS_OK;

OnError:
    if (mdl != gcvNULL)
    {
        /* Free the memory. */
        _DestroyMdl(mdl);
    }

    if (locked)
    {
        /* Unlock the memory. */
        MEMORY_UNLOCK(Os);
    }

    /* Return the status. */
    gcmkFOOTER();
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
    gctSTRING addr;
    gctINT i;

    gcmkHEADER_ARG("Os=0x%X Physical=0x%X Bytes=%lu", Os, Physical, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Bytes > 0);

    addr = mdl->addr;


    for (i = 0; i < mdl->numPages; i++)
    {
        if (mdl->contiguous)
        {
            ClearPageReserved(nth_page(mdl->u.contiguousPages, i));
        }
        else
        {
            ClearPageReserved(_NonContiguousToPage(mdl->u.nonContiguousPages, i));
        }
    }

    if (mdl->contiguous)
    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27) && 0
        if (mdl->exact == gcvTRUE)
        {
            free_pages_exact(page_address(mdl->u.contiguousPages), mdl->numPages * PAGE_SIZE);
        }
        else
#endif
        {
            __free_pages(mdl->u.contiguousPages, GetOrder(mdl->numPages));
        }
    }
    else
    {
        _NonContiguousFree(mdl->u.nonContiguousPages, mdl->numPages);
    }

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
    PLINUX_MDL      mdl;
    PLINUX_MDL_MAP  mdlMap;
    gctSTRING       addr;
    unsigned long   start;
    unsigned long   pfn;
    gctINT          i;

    gcmkHEADER_ARG("Os=0x%X Physical=0x%X Bytes=%lu", Os, Physical, Logical);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);
    gcmkVERIFY_ARGUMENT(PageCount != gcvNULL);

    mdl = (PLINUX_MDL) Physical;

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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
        mdlMap->vmaAddr = (gctSTRING)vm_mmap(gcvNULL,
                        0L,
                        mdl->numPages * PAGE_SIZE,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        0);
#else
        down_write(&current->mm->mmap_sem);
        mdlMap->vmaAddr = (gctSTRING)do_mmap_pgoff(gcvNULL,
                        0L,
                        mdl->numPages * PAGE_SIZE,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        0);
        up_write(&current->mm->mmap_sem);
#endif

        gcmkTRACE_ZONE(
            gcvLEVEL_INFO, gcvZONE_OS,
            "%s(%d): vmaAddr->0x%X for phys_addr->0x%X",
            __FUNCTION__, __LINE__,
            (gctUINT32)(gctUINTPTR_T)mdlMap->vmaAddr,
            (gctUINT32)(gctUINTPTR_T)mdl
            );

        if (IS_ERR(mdlMap->vmaAddr))
        {
            gcmkTRACE_ZONE(
                gcvLEVEL_INFO, gcvZONE_OS,
                "%s(%d): do_mmap error",
                __FUNCTION__, __LINE__
                );

            mdlMap->vmaAddr = gcvNULL;

            MEMORY_UNLOCK(Os);

            gcmkFOOTER_ARG("*status=%d", gcvSTATUS_OUT_OF_MEMORY);
            return gcvSTATUS_OUT_OF_MEMORY;
        }

        down_write(&current->mm->mmap_sem);

        mdlMap->vma = find_vma(current->mm, (unsigned long)mdlMap->vmaAddr);

        if (mdlMap->vma == gcvNULL)
        {
            up_write(&current->mm->mmap_sem);

            gcmkTRACE_ZONE(
                gcvLEVEL_INFO, gcvZONE_OS,
                "%s(%d): find_vma error",
                __FUNCTION__, __LINE__
                );

            mdlMap->vmaAddr = gcvNULL;

            MEMORY_UNLOCK(Os);

            gcmkFOOTER_ARG("*status=%d", gcvSTATUS_OUT_OF_RESOURCES);
            return gcvSTATUS_OUT_OF_RESOURCES;
        }

        mdlMap->vma->vm_flags |= gcdVM_FLAGS;

        if (Cacheable == gcvFALSE)
        {
            /* Make this mapping non-cached. */
            mdlMap->vma->vm_page_prot = gcmkPAGED_MEMROY_PROT(mdlMap->vma->vm_page_prot);
        }

        addr = mdl->addr;

        /* Now map all the vmalloc pages to this user address. */
        if (mdl->contiguous)
        {
#if (MRVL_VIDEO_MEMORY_USE_TYPE != gcdMEM_TYPE_NONE)
            /* Now map all the vmalloc pages to this user address. */
            unsigned long phyaddr = 0;

            /* Pmem video memory physical address is valid, no kernel virtual addr. */
            phyaddr = mdl->bPmem ? mdl->dmaHandle : page_to_phys(mdl->u.contiguousPages);

            /* map kernel memory to user space. */
            if (remap_pfn_range(mdlMap->vma,
                                mdlMap->vma->vm_start,
                                phyaddr >> PAGE_SHIFT,
                                mdlMap->vma->vm_end - mdlMap->vma->vm_start,
                                mdlMap->vma->vm_page_prot) < 0)
#else
            /* map kernel memory to user space. */
            if (remap_pfn_range(mdlMap->vma,
                                mdlMap->vma->vm_start,
                                page_to_pfn(mdl->u.contiguousPages),
                                mdlMap->vma->vm_end - mdlMap->vma->vm_start,
                                mdlMap->vma->vm_page_prot) < 0)
#endif
            {
                up_write(&current->mm->mmap_sem);

                gcmkTRACE_ZONE(
                    gcvLEVEL_INFO, gcvZONE_OS,
                    "%s(%d): unable to mmap ret",
                    __FUNCTION__, __LINE__
                    );

                mdlMap->vmaAddr = gcvNULL;

                MEMORY_UNLOCK(Os);

                gcmkFOOTER_ARG("*status=%d", gcvSTATUS_OUT_OF_MEMORY);
                return gcvSTATUS_OUT_OF_MEMORY;
            }
        }
        else
        {
            start = mdlMap->vma->vm_start;

            for (i = 0; i < mdl->numPages; i++)
            {
                pfn = _NonContiguousToPfn(mdl->u.nonContiguousPages, i);

                if (remap_pfn_range(mdlMap->vma,
                                    start,
                                    pfn,
                                    PAGE_SIZE,
                                    mdlMap->vma->vm_page_prot) < 0)
                {
                    up_write(&current->mm->mmap_sem);

                    gcmkTRACE_ZONE(
                        gcvLEVEL_INFO, gcvZONE_OS,
                        "%s(%d): gctPHYS_ADDR->0x%X Logical->0x%X Unable to map addr->0x%X to start->0x%X",
                        __FUNCTION__, __LINE__,
                        (gctUINT32)(gctUINTPTR_T)Physical,
                        (gctUINT32)(gctUINTPTR_T)*Logical,
                        (gctUINT32)(gctUINTPTR_T)addr,
                        (gctUINT32)(gctUINTPTR_T)start
                        );

                    mdlMap->vmaAddr = gcvNULL;

                    MEMORY_UNLOCK(Os);

                    gcmkFOOTER_ARG("*status=%d", gcvSTATUS_OUT_OF_MEMORY);
                    return gcvSTATUS_OUT_OF_MEMORY;
                }

                start += PAGE_SIZE;
                addr += PAGE_SIZE;
            }
        }

        up_write(&current->mm->mmap_sem);
    }
#if 0
    else
    {
        /* mdlMap->vmaAddr != gcvNULL means current process has already locked this node. */
        MEMORY_UNLOCK(Os);

        gcmkFOOTER_ARG("*status=%d, mdlMap->vmaAddr=%x", gcvSTATUS_MEMORY_LOCKED, mdlMap->vmaAddr);
        return gcvSTATUS_MEMORY_LOCKED;
    }
#endif

    /* Increment the lock count */
    mdlMap->lockCount++;

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
        gcvNULL,
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
                            PageTable);
}

gceSTATUS
gckOS_MapPagesEx(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T PageCount,
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

    gcmkHEADER_ARG("Os=0x%X Core=%d Physical=0x%X PageCount=%u PageTable=0x%X",
                   Os, Core, Physical, PageCount, PageTable);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);
    gcmkVERIFY_ARGUMENT(PageCount > 0);
    gcmkVERIFY_ARGUMENT(PageTable != gcvNULL);

    /* Convert pointer to MDL. */
    mdl = (PLINUX_MDL)Physical;

    gcmkTRACE_ZONE(
        gcvLEVEL_INFO, gcvZONE_OS,
        "%s(%d): Physical->0x%X PageCount->0x%X PagedMemory->?%d",
        __FUNCTION__, __LINE__,
        (gctUINT32)(gctUINTPTR_T)Physical,
        (gctUINT32)(gctUINTPTR_T)PageCount,
        mdl->pagedMem
        );

    MEMORY_LOCK(Os);

    table = (gctUINT32 *)PageTable;
#if gcdNONPAGED_MEMORY_CACHEABLE
    mmu = Os->device->kernels[Core]->mmu;
    bytes = PageCount * sizeof(*table);
    mmuMdl = (PLINUX_MDL)mmu->pageTablePhysical;
#endif

     /* Get all the physical addresses and store them in the page table. */

    offset = 0;

    if (mdl->pagedMem)
    {
        /* Try to get the user pages so DMA can happen. */
        while (PageCount-- > 0)
        {
#if gcdENABLE_VG
            if (Core == gcvCORE_VG)
            {
                if (mdl->contiguous)
                {
                    gcmkONERROR(
                        gckVGMMU_SetPage(Os->device->kernels[Core]->vg->mmu,
                             page_to_phys(nth_page(mdl->u.contiguousPages, offset)),
                             table));
                }
                else
                {
                    gcmkONERROR(
                        gckVGMMU_SetPage(Os->device->kernels[Core]->vg->mmu,
                             _NonContiguousToPhys(mdl->u.nonContiguousPages, offset),
                             table));
                }
            }
            else
#endif
            {
                if (mdl->contiguous)
                {
                    gcmkONERROR(
                        gckMMU_SetPage(Os->device->kernels[Core]->mmu,
                             page_to_phys(nth_page(mdl->u.contiguousPages, offset)),
                             table));
                }
                else
                {
                    gcmkONERROR(
                        gckMMU_SetPage(Os->device->kernels[Core]->mmu,
                             _NonContiguousToPhys(mdl->u.nonContiguousPages, offset),
                             table));
                }
            }

            table++;
            offset += 1;
        }
    }
    else
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_INFO, gcvZONE_OS,
            "%s(%d): we should not get this call for Non Paged Memory!",
            __FUNCTION__, __LINE__
            );

        while (PageCount-- > 0)
        {
#if gcdENABLE_VG
            if (Core == gcvCORE_VG)
            {
                gcmkONERROR(
                        gckVGMMU_SetPage(Os->device->kernels[Core]->vg->mmu,
                                         page_to_phys(nth_page(mdl->u.contiguousPages, offset)),
                                         table));
            }
            else
#endif
            {
                gcmkONERROR(
                        gckMMU_SetPage(Os->device->kernels[Core]->mmu,
                                         page_to_phys(nth_page(mdl->u.contiguousPages, offset)),
                                         table));
            }
            table++;
            offset += 1;
        }
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

    MEMORY_UNLOCK(Os);

    /* Return the status. */
    gcmkFOOTER();
    return status;
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
    IN gctUINT Pid,
    IN gctSIZE_T Bytes,
    IN gctPOINTER Logical
    )
{
    PLINUX_MDL_MAP          mdlMap;
    PLINUX_MDL              mdl = (PLINUX_MDL)Physical;
    struct task_struct * task;

    gcmkHEADER_ARG("Os=0x%X Physical=0x%X Bytes=%u Logical=0x%X",
                   Os, Physical, Bytes, Logical);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);

#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_NONE)
    /* Make sure there is already a mapping...*/
    gcmkVERIFY_ARGUMENT(mdl->u.nonContiguousPages != gcvNULL
                       || mdl->u.contiguousPages != gcvNULL);
#endif

    MEMORY_LOCK(Os);

    mdlMap = mdl->maps;

    while (mdlMap != gcvNULL)
    {
        if ((mdlMap->vmaAddr != gcvNULL) && (Pid == mdlMap->pid))
        {
            if (--mdlMap->lockCount == 0)
            {
                /* Get the current pointer for the task with stored pid. */
                task = FIND_TASK_BY_PID(mdlMap->pid);

                if (task != gcvNULL && task->mm != gcvNULL)
                {
                    _UnmapUserLogical(mdlMap->pid, mdlMap->vmaAddr, mdl->numPages * PAGE_SIZE);
                }

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

    gcmkHEADER_ARG("Os=0x%X", Os);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);

    gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_OS, "in %s", __func__);

    /* Get total number of pages.. */
    numPages = GetPageCount(Bytes, 0);

    /* Allocate mdl+vector structure */
    mdl = _CreateMdl(_GetProcessID());

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
    mdl->gcAddress = gcvINVALID_ADDRESS;
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
                "%s: Bytes->0x%x, Mdl->%p, Logical->0x%x dmaHandle->0x%x",
                __func__,
                (gctUINT32)Bytes,
                mdl,
                (gctUINT32)mdl->addr,
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

gceSTATUS
gckOS_FreeVidmemFromMemblock(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    PLINUX_MDL      mdl     = gcvNULL;

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

#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
gceSTATUS
gckOS_AllocatePmemMemory(
    IN gckOS Os,
    IN gctSIZE_T Bytes,
    OUT gctPHYS_ADDR * Physical)
{
    gctINT numPages;
    PLINUX_MDL mdl;
    unsigned long pAddr;
    gctSIZE_T bytes;
    size_t size;
    struct ion_handle *handle = NULL;
    gcmkHEADER_ARG("Os=0x%x, Physical=%x, Bytes=%lu", Os, Physical, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Bytes > 0);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);

    bytes = gcmALIGN(Bytes, PAGE_SIZE);
    numPages = GetPageCount(bytes, 0);

    MEMORY_LOCK(Os);

    /* if touch the upon limit of PMEM, roll back virtual memory path. */
    if((Os->device->pmemUsage + bytes) > Os->device->reservedPmemMem)
    {
        MEMORY_UNLOCK(Os);
        gcmkTRACE(gcvLEVEL_WARNING, "%s(%d), Forbit to allocate memory for pmem pool.", __FUNCTION__, __LINE__);
        return gcvSTATUS_OUT_OF_MEMORY;
    }

    MEMORY_UNLOCK(Os);

    if (!gc_ion_client)
    {
        return gcvSTATUS_OUT_OF_MEMORY;
    }

    handle = ion_alloc(gc_ion_client,
                        bytes,
                        PAGE_SIZE,
                        ION_HEAP_CARVEOUT_MASK,
                        ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC);
    if (IS_ERR(handle))
    {
        return gcvSTATUS_OUT_OF_MEMORY;
    }

    if (ion_phys(gc_ion_client, handle, &pAddr, &size) < 0)
    {
        ion_free(gc_ion_client, handle);
        return gcvSTATUS_OUT_OF_MEMORY;
    }

    mdl = _CreateMdl(_GetProcessID());
    if(mdl == gcvNULL)
    {
        ion_free(gc_ion_client, handle);

        gcmkTRACE(gcvLEVEL_ERROR, "%s(%d), Cannot create the mdl.", __FUNCTION__, __LINE__);
        gcmkFOOTER_ARG("status = %d", gcvSTATUS_OUT_OF_MEMORY);
        return gcvSTATUS_OUT_OF_MEMORY;
    }

    mdl->dmaHandle  = (dma_addr_t)pAddr;
    mdl->numPages   = numPages;
    mdl->pagedMem   = 1;
    mdl->bPmem      = gcvTRUE;
    mdl->region     = gcvNULL;
    mdl->addr       = gcvNULL;
    mdl->contiguous = gcvTRUE;
    mdl->ionHandle  = handle;
    mdl->wastSize   = bytes - Bytes;

    /* Return the physical address. */
    *Physical = (gctPHYS_ADDR)mdl;

    MEMORY_LOCK(Os);

    /*Add this to a global list, will be used by get physical address and map user pointer functions. */
    if(!Os->mdlHead)
    {
        /* Initialize the list. */
        Os->mdlHead = Os->mdlTail = mdl;
    }
    else
    {
        /* Add to tail. */
        mdl->prev = Os->mdlTail;
        Os->mdlTail->next = mdl;
        Os->mdlTail = mdl;
    }

    Os->device->pmemUsage += bytes;
    Os->device->wastBytes += mdl->wastSize;

    MEMORY_UNLOCK(Os);
    /* Success. */
    gcmkFOOTER_ARG("*Physcial = 0x%08x", *Physical);
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_FreePmemMemory(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T Bytes)
{
    PLINUX_MDL mdl = (PLINUX_MDL)Physical;

    //gctSTRING vaddr = mdl->addr;
    gcmkHEADER_ARG("Os=0x%x, Physical=%x, Bytes=%lu", Os, Physical, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);
    gcmkVERIFY_ARGUMENT(mdl->bPmem == gcvTRUE);
    gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_OS, "in gckOS_FreePmemMemory");

    if (mdl->ionHandle)
    {
        ion_free(gc_ion_client, mdl->ionHandle);
        mdl->ionHandle = NULL;
    }

    MEMORY_LOCK(Os);

    /* Remove the node from the global list. */
    if(mdl == Os->mdlHead)
    {
        if((Os->mdlHead = mdl->next) == gcvNULL)
        {
            Os->mdlTail = gcvNULL;
        }
    }
    else
    {
        mdl->prev->next = mdl->next;
        if(mdl == Os->mdlTail)
        {
            Os->mdlTail = mdl->prev;
        }
        else
        {
            mdl->next->prev = mdl->prev;
        }
    }

    /* Update pmem memory usage. */
    Os->device->pmemUsage -= (mdl->numPages * PAGE_SIZE);
    Os->device->wastBytes -= mdl->wastSize;

    MEMORY_UNLOCK(Os);

    /* Free the structure. */
    gcmkVERIFY_OK(_DestroyMdl(mdl));
    mdl = gcvNULL;

    gcmkFOOTER_NO();
    /* Success. */
    return gcvSTATUS_OK;
}

/* Get physical address from pmem, just return the physical address. */
gceSTATUS
gckOS_GetPmemPhysicalAddress(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical,
    OUT gctUINT32 * Address)
{
    PLINUX_MDL mdl = (PLINUX_MDL)Physical;
    gcmkHEADER_ARG("Os=0x%x, Physical=%x", Os, Physical);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Address != gcvNULL);
    gcmkVERIFY_ARGUMENT(mdl->bPmem == gcvTRUE);
    gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_OS, "in gckOS_GetPmemPhysicalAddress");

    MEMORY_LOCK(Os);

    *Address = mdl->dmaHandle - Os->baseAddress;

    MEMORY_UNLOCK(Os);

    gcmkFOOTER_ARG("*Address=0x%x", *Address);
    return gcvSTATUS_OK;
}

#elif (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_PMEM)
gceSTATUS
gckOS_AllocatePmemMemory(
    IN gckOS Os,
    IN gctSIZE_T Bytes,
    OUT gctPHYS_ADDR * Physical)
{
    gctINT numPages;
    PLINUX_MDL mdl;
    unsigned long pAddr;
    gctSIZE_T bytes;
    struct pmem_region* region = gcvNULL;

    gcmkHEADER_ARG("Os=0x%x, Physical=%x, Bytes=%lu", Os, Physical, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Bytes > 0);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);

    bytes = gcmALIGN(Bytes, PAGE_SIZE);
    numPages = GetPageCount(bytes, 0);

    MEMORY_LOCK(Os);

    /* if touch the upon limit of PMEM, roll back virtual memory path. */
    if((Os->device->pmemUsage + bytes) > Os->device->reservedPmemMem)
    {
        MEMORY_UNLOCK(Os);
        gcmkTRACE(gcvLEVEL_WARNING, "%s(%d), Forbit to allocate memory for pmem pool.", __FUNCTION__, __LINE__);
        return gcvSTATUS_OUT_OF_MEMORY;
    }

    region = (struct pmem_region *)kmalloc(sizeof(struct pmem_region), GFP_ATOMIC);
    if(region == gcvNULL)
    {
        MEMORY_UNLOCK(Os);
        gcmkTRACE(gcvLEVEL_ERROR, "%s(%d), allocate memory failed.", __FUNCTION__, __LINE__);
        return gcvSTATUS_OUT_OF_MEMORY;
    }

    region->len = Bytes;/*numPages * PAGE_SIZE*/

    if(get_pmem_area(MRVL_PMEM_MINOR_FLAG, region, &pAddr, gcvNULL) < 0)
    {
        gcmkTRACE(gcvLEVEL_ERROR, "%s(%d), get_pmem_area failed.", __FUNCTION__, __LINE__);
        gcmkTRACE_ZONE(gcvLEVEL_INFO,
                    gcvZONE_OS,
                    "get_pmem_area: "
                    "Can't allocate memory for size %d",
                    (gctUINT32)bytes);

        if(region != gcvNULL)
        {
            kfree(region);
            region = gcvNULL;
        }

        MEMORY_UNLOCK(Os);

        gcmkFOOTER_ARG("status = %d", gcvSTATUS_OUT_OF_MEMORY);
        return gcvSTATUS_OUT_OF_MEMORY;
    }

    mdl = _CreateMdl(_GetProcessID());

    if(mdl == gcvNULL)
    {
        put_pmem_area(MRVL_PMEM_MINOR_FLAG, region, gcvNULL);

        if(region != gcvNULL)
        {
            kfree(region);
            region = gcvNULL;
        }

        MEMORY_UNLOCK(Os);

        gcmkTRACE(gcvLEVEL_ERROR, "%s(%d), Cannot create the mdl.", __FUNCTION__, __LINE__);
        gcmkFOOTER_ARG("status = %d", gcvSTATUS_OUT_OF_MEMORY);
        return gcvSTATUS_OUT_OF_MEMORY;
    }

    mdl->dmaHandle  = (dma_addr_t)pAddr;
    mdl->numPages   = numPages;
    mdl->pagedMem   = 1;
    mdl->bPmem      = gcvTRUE;
    mdl->region     = region;
    mdl->addr       = gcvNULL;
    mdl->contiguous = gcvTRUE;
    mdl->gcAddress  = gcvINVALID_ADDRESS;
    mdl->wastSize   = bytes - Bytes;
#if 0
    for(i = 0; i < mdl->numPages; i++)
    {
        struct page *page;
        page = virt_to_page((void*)(((unsigned long)vAddr) + i * PAGE_SIZE));

        SetPageReserved(page);
        /*flush_dcache_page(page);*/
    }
#endif

    /* Return the physical address. */
    *Physical = (gctPHYS_ADDR)mdl;

    /*Add this to a global list, will be used by get physical address and map user pointer functions. */
    if(!Os->mdlHead)
    {
        /* Initialize the list. */
        Os->mdlHead = Os->mdlTail = mdl;
    }
    else
    {
        /* Add to tail. */
        mdl->prev = Os->mdlTail;
        Os->mdlTail->next = mdl;
        Os->mdlTail = mdl;
    }

    Os->device->pmemUsage += bytes;
    Os->device->wastBytes += mdl->wastSize;

    MEMORY_UNLOCK(Os);

    /* Success. */
    gcmkFOOTER_ARG("*Physcial = 0x%08x", *Physical);
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_FreePmemMemory(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T Bytes)
{
    PLINUX_MDL mdl = (PLINUX_MDL)Physical;
    gctSTRING vaddr = mdl->addr;

    gcmkHEADER_ARG("Os=0x%x, Physical=%x, Bytes=%lu", Os, Physical, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);
    gcmkVERIFY_ARGUMENT(mdl->bPmem == gcvTRUE);

    gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_OS, "in gckOS_FreePmemMemory");

    MEMORY_LOCK(Os);

#if 0
    for(i = 0; i < mdl->numPages; i++)
    {
        if(mdl->contiguous)
        {
            ClearPageReserved(virt_to_page((void*)(((unsigned long)vAddr) + i * PAGE_SIZE)));
        }
        else
        {
            ClearPageReserved(vmalloc_to_page((void*)(((unsigned long)vAddr) + i * PAGE_SIZE)));
        }
    }
#endif

    if(put_pmem_area(MRVL_PMEM_MINOR_FLAG, mdl->region, vaddr) < 0)
    {
        gcmkTRACE(gcvLEVEL_ERROR,
            "%s(%d), put_pmem_area failed at region offset %x, region len %x, vaddr %x",
            __FUNCTION__,
            __LINE__,
            mdl->region->offset,
            mdl->region->len,
            vaddr);

        MEMORY_UNLOCK(Os);
        return gcvSTATUS_INVALID_ADDRESS;
    }

    /* Remove the node from the global list. */
    if(mdl == Os->mdlHead)
    {
        if((Os->mdlHead = mdl->next) == gcvNULL)
        {
            Os->mdlTail = gcvNULL;
        }
    }
    else
    {
        mdl->prev->next = mdl->next;

        if(mdl == Os->mdlTail)
        {
            Os->mdlTail = mdl->prev;
        }
        else
        {
            mdl->next->prev = mdl->prev;
        }
    }

    /* Update pmem memory usage. */
    Os->device->pmemUsage -= (mdl->numPages * PAGE_SIZE);
    Os->device->wastBytes -= mdl->wastSize;

    MEMORY_UNLOCK(Os);

    /* Free the structure. */
    gcmkVERIFY_OK(_DestroyMdl(mdl));
    mdl = gcvNULL;

    gcmkFOOTER_NO();
    /* Success. */
    return gcvSTATUS_OK;
}

/* Get physical address from pmem, just return the physical address. */
gceSTATUS
gckOS_GetPmemPhysicalAddress(
    IN gckOS Os,
    IN gctPHYS_ADDR Physical,
    OUT gctUINT32 * Address)
{
    PLINUX_MDL mdl = (PLINUX_MDL)Physical;

    gcmkHEADER_ARG("Os=0x%x, Physical=%x", Os, Physical);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Physical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Address != gcvNULL);
    gcmkVERIFY_ARGUMENT(mdl->bPmem == gcvTRUE);

    gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_OS, "in gckOS_GetPmemPhysicalAddress");

    MEMORY_LOCK(Os);

    *Address = mdl->dmaHandle - Os->baseAddress;

    MEMORY_UNLOCK(Os);

    gcmkFOOTER_ARG("*Address=0x%x", *Address);
    return gcvSTATUS_OK;
}
#endif

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
    gctBOOL isLocked = gcvFALSE;

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
        memory = (gctUINTPTR_T) Memory;

        /* Get the number of required pages. */
        end = (memory + Size + PAGE_SIZE - 1) >> PAGE_SHIFT;
        start = memory >> PAGE_SHIFT;
        pageCount = end - start;

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

        /* Allocate the Info struct. */
        info = (gcsPageInfo_PTR)kmalloc(sizeof(gcsPageInfo), GFP_KERNEL | gcdNOWARN);

        if (info == gcvNULL)
        {
            status = gcvSTATUS_OUT_OF_MEMORY;
            break;
        }

        /* Allocate the array of page addresses. */
        pages = (struct page **)kmalloc(pageCount * sizeof(struct page *), GFP_KERNEL | gcdNOWARN);

        if (pages == gcvNULL)
        {
            status = gcvSTATUS_OUT_OF_MEMORY;
            break;
        }

        MEMORY_MAP_LOCK(Os);
        isLocked = gcvTRUE;

        if (Physical != ~0U)
        {
            for (i = 0; i < pageCount; i++)
            {
                pages[i] = pfn_to_page((Physical >> PAGE_SHIFT) + i);
                get_page(pages[i]);
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

                    kfree(pages);
                    pages = gcvNULL;
                }

                vma = find_vma(current->mm, memory);

                if (vma && (vma->vm_flags & VM_PFNMAP) )
                {
                    pte_t       * pte;
                    spinlock_t  * ptl;
                    unsigned long pfn;

                    pgd_t * pgd = pgd_offset(current->mm, memory);
                    pud_t * pud = pud_offset(pgd, memory);
                    if (pud)
                    {
                        pmd_t * pmd = pmd_offset(pud, memory);
                        pte = pte_offset_map_lock(current->mm, pmd, memory, &ptl);
                        if (!pte)
                        {
                            gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
                        }
                    }
                    else
                    {
                        gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
                    }

                    pfn      = pte_pfn(*pte);

                    physical = (pfn << PAGE_SHIFT) | (memory & ~PAGE_MASK);

                    pte_unmap_unlock(pte, ptl);

                if (((Os->device->kernels[Core]->hardware->mmuVersion == 0)
                     && !((physical - Os->device->baseAddress) & 0x80000000))
                    || (Os->device->kernels[Core]->hardware->mmuVersion != 0) )
                {
                    /* Release page info struct. */
                    if (info != gcvNULL)
                    {
                        info->pages = gcvNULL;
                        info->pageTable = gcvNULL;

                        MEMORY_MAP_UNLOCK(Os);

                        *Address = physical - Os->device->baseAddress;
                        *Info    = info;

                        gcmkFOOTER_ARG("*Info=0x%X *Address=0x%08x",
                                *Info, *Address);

                        return gcvSTATUS_OK;
                    }
                }
                else
                {
                    gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
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
#if 0
                /* Flush(clean) the data cache. */
                gcmkONERROR(gckOS_CacheFlush(Os, _GetProcessID(), gcvNULL,
                                 (gctPOINTER)(gctUINTPTR_T)page_to_phys(pages[i]),
                                 (gctPOINTER)(memory & PAGE_MASK) + i*PAGE_SIZE,
                                 PAGE_SIZE));
#else
                if (pages[i] != gcvNULL)
                {
                    /* FIXME: comment out flush operation for kernel3.10 bringup */
//                    __dma_page_cpu_to_dev(pages[i], 0, PAGE_SIZE, DMA_BIDIRECTIONAL);
                }
#endif
            }
        }
        else
        {
            /* Flush(clean) the data cache. */
            gcmkONERROR(gckOS_CacheFlush(Os, _GetProcessID(), gcvNULL,
                             (gctPOINTER)(gctUINTPTR_T)(physical & PAGE_MASK),
                             (gctPOINTER)(memory & PAGE_MASK),
                             PAGE_SIZE * pageCount));
        }

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
            /* Allocate pages inside the page table. */
            gcmkERR_BREAK(gckMMU_AllocatePages(Os->device->kernels[Core]->mmu,
                                              pageCount * (PAGE_SIZE/4096),
                                              (gctPOINTER *) &pageTable,
                                              &address));
        }
        /* Fill the page table. */
        for (i = 0; i < pageCount; i++)
        {
            gctUINT32 phys;
            gctUINT32_PTR tab = pageTable + i * (PAGE_SIZE/4096);

            if (pages)
            {
                phys = page_to_phys(pages[i]);
            }
            else
            {
                phys = (physical & PAGE_MASK) + i * PAGE_SIZE;
            }

#if gcdENABLE_VG
            if (Core == gcvCORE_VG)
            {
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

            gcmkTRACE_ZONE(
                gcvLEVEL_INFO, gcvZONE_OS,
                "%s(%d): pageTable[%d]: 0x%X 0x%X.",
                __FUNCTION__, __LINE__,
                i, phys, pageTable[i]);
        }

#if gcdENABLE_VG
        if (Core == gcvCORE_VG)
        {
            gcmkONERROR(gckVGMMU_Flush(Os->device->kernels[Core]->vg->mmu));
        }
        else
#endif
        {
            gcmkONERROR(gckMMU_Flush(Os->device->kernels[Core]->mmu));
        }

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
    }while (gcvFALSE);

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
            kfree(pages);
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

    if(isLocked)
    {
        MEMORY_MAP_UNLOCK(Os);
    }

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

        gcmkASSERT(info->pageTable != gcvNULL);

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
            gcmkERR_BREAK(gckMMU_FreePages(Os->device->kernels[Core]->mmu,
                                          info->pageTable,
                                          pageCount * (PAGE_SIZE/4096)
                                          ));
        }

        /* Release the page cache. */
        if (pages)
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

                if (pages[i] != gcvNULL)
                {
                    /* FIXME: comment out flush operation for kernel3.10 bringup */
//                    __dma_page_cpu_to_dev(pages[i], 0, PAGE_SIZE, DMA_BIDIRECTIONAL);
                }
                page_cache_release(pages[i]);
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
            kfree(info->pages);
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
#if (MRVL_VIDEO_MEMORY_USE_TYPE != gcdMEM_TYPE_NONE)
           +device->pmemUsage
#endif
           +device->contiguousPagedMemUsage
           +device->virtualPagedMemUsage
           +device->contiguousNonPagedMemUsage;

    len += sprintf(buf+len, "Video mem:  %7ld KB\n"
#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
                            "ION as vidmem:    %7ld KB\n"
                            "  - ION used as vidmem:    %7d KB\n"
#elif (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_PMEM)
                            "PMEM as vidmem:   %7ld KB\n"
                            "  - PMEM used as vidmem:   %7d KB\n"
#endif
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
                            device->reservedMem/1024,
#if (MRVL_VIDEO_MEMORY_USE_TYPE != gcdMEM_TYPE_NONE)
                            device->reservedPmemMem/1024,
                            device->pmemUsage/1024,
#endif
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

gceSTATUS
gckOS_ResumeInterrupt(
    IN gckOS Os
    )
{
    return gckOS_ResumeInterruptEx(Os, gcvCORE_MAJOR);
}

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
    IN gctPOINTER Physical,
    IN gctPOINTER Logical,
    IN gctSIZE_T Bytes,
    IN enum dma_data_direction Dir
    )
{
    gceSTATUS status;
    gctUINT32 i, pageNum;
    unsigned long paddr;
    gctPOINTER vaddr;

    gcmkHEADER_ARG("Os=0x%X ProcessID=%d Handle=0x%X Logical=0x%X Bytes=%lu",
                   Os, ProcessID, Handle, Logical, Bytes);

    if (Physical != gcvNULL)
    {
        /* Non paged memory or gcvPOOL_USER surface */
        paddr = (unsigned long) Physical;
        dma_sync_single_for_device(
                  gcvNULL,
                  (dma_addr_t)paddr,
                  Bytes,
                  Dir);
    }
    else if ((Handle == gcvNULL)
    || (Handle != gcvNULL && ((PLINUX_MDL)Handle)->contiguous)
    )
    {
        /* Video Memory or contiguous virtual memory */
        gcmkONERROR(gckOS_GetPhysicalAddress(Os, Logical, (gctUINT32*)&paddr));
        dma_sync_single_for_device(
                  gcvNULL,
                  (dma_addr_t)paddr,
                  Bytes,
                  Dir);
    }
    else
    {
        /* Non contiguous virtual memory */
        vaddr = (gctPOINTER)gcmALIGN_BASE((gctUINTPTR_T)Logical, PAGE_SIZE);
        pageNum = GetPageCount(Bytes, 0);

        for (i = 0; i < pageNum; i += 1)
        {
            gcmkONERROR(_ConvertLogical2Physical(
                Os,
                vaddr + PAGE_SIZE * i,
                ProcessID,
                (PLINUX_MDL)Handle,
                (gctUINT32*)&paddr
                ));

            dma_sync_single_for_device(
                      gcvNULL,
                      (dma_addr_t)paddr,
                      PAGE_SIZE,
                      Dir);
        }
    }

    mb();

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
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
**
**      gceOUTERCACHE_OPERATION Type
**          Operation need to be execute.
*/
static gceSTATUS
_HandleOuterCache(
    IN gckOS Os,
    IN gctUINT32 ProcessID,
    IN gctPHYS_ADDR Handle,
    IN gctPOINTER Physical,
    IN gctPOINTER Logical,
    IN gctSIZE_T Bytes,
    IN gceCACHEOPERATION Type
    )
{
    gceSTATUS status;
    gctUINT32 i, pageNum;
    unsigned long paddr;
    gctPOINTER vaddr;

    gcmkHEADER_ARG("Os=0x%X ProcessID=%d Handle=0x%X Logical=0x%X Bytes=%lu",
                   Os, ProcessID, Handle, Logical, Bytes);

    if (Physical != gcvNULL)
    {
        /* Non paged memory or gcvPOOL_USER surface */
        paddr = (unsigned long) Physical;
        gcmkONERROR(outer_func(Type, paddr, paddr + Bytes));
    }
    else if ((Handle == gcvNULL)
    || (Handle != gcvNULL && ((PLINUX_MDL)Handle)->contiguous)
    )
    {
        /* Video Memory or contiguous virtual memory */
        gcmkONERROR(gckOS_GetPhysicalAddressByHandle(Os, Logical, Handle, (gctUINT32*)&paddr));
        gcmkONERROR(outer_func(Type, paddr, paddr + Bytes));
    }
    else
    {
        /* Non contiguous virtual memory */
        vaddr = (gctPOINTER)gcmALIGN_BASE((gctUINTPTR_T)Logical, PAGE_SIZE);
        pageNum = GetPageCount(Bytes, 0);

        for (i = 0; i < pageNum; i += 1)
        {
            gcmkONERROR(_ConvertLogical2Physical(
                Os,
                vaddr + PAGE_SIZE * i,
                ProcessID,
                (PLINUX_MDL)Handle,
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
    IN gctPOINTER Physical,
    IN gctPOINTER Logical,
    IN gctSIZE_T Bytes
    )
{
    gcmkHEADER_ARG("Os=0x%X ProcessID=%d Handle=0x%X Logical=0x%X Bytes=%lu",
                   Os, ProcessID, Handle, Logical, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Bytes > 0);

#if !gcdCACHE_FUNCTION_UNIMPLEMENTED
#ifdef CONFIG_ARM
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
    _HandleOuterCache(Os, ProcessID, Handle, Physical, Logical, Bytes, gcvCACHE_CLEAN);
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
              Physical,
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
    IN gctPOINTER Physical,
    IN gctPOINTER Logical,
    IN gctSIZE_T Bytes
    )
{
    gcmkHEADER_ARG("Os=0x%X ProcessID=%d Handle=0x%X Logical=0x%X Bytes=%lu",
                   Os, ProcessID, Handle, Logical, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Bytes > 0);

#if !gcdCACHE_FUNCTION_UNIMPLEMENTED
#ifdef CONFIG_ARM

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
    _HandleOuterCache(Os, ProcessID, Handle, Physical, Logical, Bytes, gcvCACHE_INVALIDATE);
#else
    outer_inv_range((unsigned long) Handle, (unsigned long) Handle + Bytes);
#endif
#endif

# endif

#elif defined(CONFIG_MIPS)
    dma_cache_inv((unsigned long) Logical, Bytes);
#elif defined(CONFIG_PPC)
#else
    dma_sync_single_for_device(
              gcvNULL,
              Physical,
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
    IN gctPOINTER Physical,
    IN gctPOINTER Logical,
    IN gctSIZE_T Bytes
    )
{
    gcmkHEADER_ARG("Os=0x%X ProcessID=%d Handle=0x%X Logical=0x%X Bytes=%lu",
                   Os, ProcessID, Handle, Logical, Bytes);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Bytes > 0);

#if !gcdCACHE_FUNCTION_UNIMPLEMENTED
#ifdef CONFIG_ARM
# if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))

    _HandleCache(Os, ProcessID, Handle, Physical, Logical, Bytes, DMA_BIDIRECTIONAL);

# else

    /* Inner cache. */
    dmac_flush_range(Logical, Logical + Bytes);

#if defined(CONFIG_OUTER_CACHE)
    /* Outer cache. */
#if gcdENABLE_OUTER_CACHE_PATCH
    _HandleOuterCache(Os, ProcessID, Handle, Physical, Logical, Bytes, gcvCACHE_FLUSH);
#else
    outer_flush_range((unsigned long) Handle, (unsigned long) Handle + Bytes);
#endif
#endif

# endif

#elif defined(CONFIG_MIPS)
    dma_cache_wback_inv((unsigned long) Logical, Bytes);
#else
    dma_sync_single_for_device(
              gcvNULL,
              Physical,
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

#if gcdPOWEROFF_TIMEOUT
    case gcvBROADCAST_POWER_OFF_TIMEOUT:
        gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_OS, "Power off GPU if idle for a period of time");

        gcmkONERROR(
            gckHARDWARE_SetPowerManagementState(Hardware,
                                                gcvPOWER_OFF_TIMEOUT_BROADCAST));
        break;
#endif

    case gcvBROADCAST_GPU_IDLE:
        gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_OS, "GPU idle.");

        if(has_feat_policy_clock_off_when_idle())
        {
            /* Clock gate GPU when it's IDLE, mark DB as IDLE as well. */
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
        gckOS_GPURuntimePut(Os);
        break;

    case gcvBROADCAST_GPU_COMMIT:
        gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_OS, "COMMIT has arrived.");

        gckOS_GPURuntimeGet(Os);

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
        DUMP_CONTROL(Hardware);
        gcmkONERROR(gckKERNEL_Recovery(Hardware->kernel));
        break;

    case gcvBROADCAST_AXI_BUS_ERROR:
        gcmkTRACE_N(gcvLEVEL_ERROR, 0, "gcvBROADCAST_AXI_BUS_ERROR\n");
        gcmkONERROR(gckHARDWARE_DumpGPUState(Hardware));
        gcmkONERROR(gckKERNEL_Recovery(Hardware->kernel));
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
**  gckOS_AcquireSemaphore_Timeout
**
**  Acquire a semahore uninterupted timeout. currently fix setting 20s.
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
gckOS_AcquireSemaphore_Timeout(
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
    if (down_timeout(((struct semaphore *) Semaphore), (Wait * HZ / 1000)))
    {
        gckOS_Log(_GFX_LOG_NOTIFY_, "ONERR: acquireSemaphore by down_timeout exit time of %d",Wait);
        gcmkONERROR(gcvSTATUS_INTERRUPTED_TIMEOUT);
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
**  gckOS_GetProcessName
**
**  Get current process Name.
**
**  INPUT:
**
**      Nothing.
**
**  OUTPUT:
**
**      gctSTRING ProcessName
**          Pointer to the variable that receives the process NAME.
**          Its size must be at least gcdPROC_NAME_LEN in size.
*/
gceSTATUS
gckOS_GetProcessName(
    OUT gctSTRING ProcessName
    )
{
    struct task_struct *tsk;

    /* Get process NAME. */
    if (ProcessName == gcvNULL)
        return gcvSTATUS_INVALID_ARGUMENT;

    tsk = current;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)
    get_task_comm(ProcessName, tsk);
#else
    task_lock(tsk);
    strncpy(ProcessName, tsk->comm, gcdPROC_NAME_LEN);
    task_unlock(tsk);
#endif

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
**      gckCORE Core
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
    gcmkHEADER_ARG("Os=0x%X Core=%d Clock=%d Power=%d", Os, Core, Clock, Power);

    /* TODO: Put your code here. */

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
    gcmkHEADER_ARG("Os=0x%X Core=%d", Os, Core);

    /* TODO: Put your code here. */

    gcmkFOOTER_NO();
    return gcvSTATUS_NOT_SUPPORTED;
}

gceSTATUS inline
_ToDisableClock(
    IN gckOS Os,
    IN gceCORE Core,
    OUT gctBOOL * DisableClk
    )
{
    gctBOOL disableClk = gcvFALSE;

    if(has_feat_separated_gc_clock())
    {
        gckHARDWARE hardware = Os->device->kernels[Core]->hardware;

        switch(hardware->refExtClock)
        {
        case 0:
            gcmkPRINT("WARNING: too much disable for GPU %d clock!", Core);
            break;
        case 1:
            disableClk = gcvTRUE;
            /* fall through */
        default:
            hardware->refExtClock--;
            if(Os->device->powerDebug)
            {
                gcmkPRINT("[%d]%s: ref_clk %d, disable_clk %d", Core, __func__,
                          hardware->refExtClock, disableClk);
            }
        }
    }
    else
    {
        /* 2D&3D clocks are combined */
        switch(Os->clockDepth)
        {
        case 0:
            gcmkPRINT("WARNING: too much disable for GPU clock!");
            break;
        case 1:
            disableClk = gcvTRUE;
            /* fall through */
        default:
            Os->clockDepth--;
            if(Os->device->powerDebug)
            {
                gcmkPRINT("[%d]%s: depth %d, disable_clk %d", Core, __func__,
                          Os->clockDepth, disableClk);
            }
        }
    }

    *DisableClk = disableClk;

    return gcvSTATUS_OK;
}

gceSTATUS inline
_ToEnableClock(
    IN gckOS Os,
    IN gceCORE Core,
    OUT gctBOOL * EnableClk
    )
{
    gctBOOL enableClk = gcvFALSE;

    if(has_feat_separated_gc_clock())
    {
        gckHARDWARE hardware = Os->device->kernels[Core]->hardware;

        switch(hardware->refExtClock)
        {
        case 0:
            enableClk = gcvTRUE;
            /* fall through */
        default:
            hardware->refExtClock++;
            if(Os->device->powerDebug)
            {
                gcmkPRINT("[%d]%s: ref_clk %d, enable_clk %d", Core, __func__,
                          hardware->refExtClock, enableClk);
            }
        }

        if(hardware->refExtClock > 1)
        {
            hardware->refExtClock--;
            gcmkPRINT("WARNING: too much enable for GPU %d clock!", Core);
        }
    }
    else
    {
        /* 2D&3D clocks are combined */
        switch(Os->clockDepth)
        {
        case 0:
            enableClk = gcvTRUE;
            /* fall through */
        default:
            Os->clockDepth++;
            if(Os->device->powerDebug)
            {
                gcmkPRINT("[%d]%s: depth %d, enable_clk %d", Core, __func__,
                          Os->clockDepth, enableClk);
            }
        }

        if(Os->clockDepth > Os->maxClockDepth)
        {
            Os->clockDepth--;
            gcmkPRINT("WARNING: too much enable for GPU clock!");
        }
    }

    *EnableClk = enableClk;

    return gcvSTATUS_OK;
}

gceSTATUS inline
_ToDisablePower(
    IN gckOS Os,
    IN gceCORE Core,
    OUT gctBOOL * DisablePwr
    )
{
    gctBOOL disablePwr = gcvFALSE;

    if(has_feat_separated_gc_power())
    {
        gckHARDWARE hardware = Os->device->kernels[Core]->hardware;

        switch(hardware->refExtPower)
        {
        case 0:
            gcmkPRINT("WARNING: too much disable for GPU %d power!", Core);
            break;
        case 1:
            disablePwr = gcvTRUE;
            /* fall through */
        default:
            hardware->refExtPower--;
            if(Os->device->powerDebug)
            {
                gcmkPRINT("[%d]%s: ref_pwr %d, disable_pwr %d", Core, __func__,
                          hardware->refExtPower, disablePwr);
            }
        }
    }
    else
    {
        switch(Os->powerDepth)
        {
        case 0:
            gcmkPRINT("WARNING: too much disable for GPU power!");
            break;
        case 1:
            disablePwr = gcvTRUE;
            /* fall through */
        default:
            Os->powerDepth--;
            if(Os->device->powerDebug)
            {
                gcmkPRINT("[%d]%s: depth %d, disable_pwr %d", Core, __func__,
                          Os->powerDepth, disablePwr);
            }
        }
    }

    *DisablePwr = disablePwr;

    return gcvSTATUS_OK;
}


gceSTATUS inline
_ToEnablePower(
    IN gckOS Os,
    IN gceCORE Core,
    OUT gctBOOL * EnablePwr
    )
{
    gctBOOL enablePwr = gcvFALSE;

    if(has_feat_separated_gc_power())
    {
        gckHARDWARE hardware = Os->device->kernels[Core]->hardware;

        switch(hardware->refExtPower)
        {
        case 0:
            enablePwr = gcvTRUE;
            /* fall through */
        default:
            hardware->refExtPower++;
            if(Os->device->powerDebug)
            {
                gcmkPRINT("[%d]%s: ref_pwr %d, enable_pwr %d", Core, __func__,
                          hardware->refExtPower, enablePwr);
            }
        }

        if(hardware->refExtPower > 1)
        {
            hardware->refExtPower--;
            gcmkPRINT("WARNING: too much enable for GPU %d power!", Core);
        }
    }
    else
    {
        switch(Os->powerDepth)
        {
        case 0:
            enablePwr = gcvTRUE;
            /* fall through */
        default:
            Os->powerDepth++;
            if(Os->device->powerDebug)
            {
                gcmkPRINT("[%d]%s: depth %d, enable_pwr %d", Core, __func__,
                          Os->powerDepth, enablePwr);
            }
        }

        if(Os->powerDepth > Os->maxPowerDepth)
        {
            Os->powerDepth--;
            gcmkPRINT("WARNING: too much enable for GPU power!");
        }
    }

    *EnablePwr = enablePwr;

    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_SetGPUPowerOnMRVL(
    IN gckOS Os,
    IN gckHARDWARE Hardware,
    IN gctBOOL EnableClk,
    IN gctBOOL EnablePwr
    )
{
    gctBOOL enableClk = gcvFALSE;
    gctBOOL enablePwr = gcvFALSE;
    gcmkHEADER_ARG("Os=0x%X Hardware=0x%X EnableClk=%d EnablePwr=%d", Os, Hardware, EnableClk, EnablePwr);

    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    down_write(&Os->rwsem_clk_pwr);

    if(EnableClk)
    {
        _ToEnableClock(Os, Hardware->core, &enableClk);
    }

    if(EnablePwr)
    {
        _ToEnablePower(Os, Hardware->core, &enablePwr);
    }


    if(enableClk || enablePwr)
    {
        if(Os->device->powerDebug)
        {
            gcmkPRINT("%s(%#X, %#X, %s, %d, %d) ##### %s [Clk:%d][%d][Pwr:%d][%d] #####", __func__,
                        Os, Hardware, Hardware->core?"2D":"3D", EnableClk, EnablePwr,
                        (enablePwr == gcvTRUE) ? "power on" : "clock on",
                        enableClk, Os->clockDepth, enablePwr, Os->powerDepth);
        }

        if(has_feat_separated_power_clock() || enablePwr)
        {
            gctUINT32 nextRate = 0;
            if(Os->device->powerDebug)
            {
                gcmkPRINT("%s(%#X, %#X, %s, %d, %d) nextRate:%d", __func__,
                            Os, Hardware, Hardware->core?"2D":"3D", EnableClk, EnablePwr, nextRate);
            }

            if(has_feat_separated_power_clock())
                gckOS_GpuPowerEnable(Os, Hardware->core, enableClk, enablePwr, nextRate*1000*1000);
            else
                gckOS_GpuPowerEnable(Os, Hardware->core, gcvTRUE, gcvTRUE, nextRate*1000*1000);

            if(Os->device->powerDebug)
            {
                gcmkPRINT("gckOS_GpuPowerEnable done.");
            }
        }
    }

    up_write(&Os->rwsem_clk_pwr);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_SetGPUPowerOffMRVL(
    IN gckOS Os,
    IN gckHARDWARE Hardware,
    IN gctBOOL DisableClk,
    IN gctBOOL DisablePwr
    )
{
    gctBOOL disableClk = gcvFALSE;
    gctBOOL disablePwr = gcvFALSE;
    gcmkHEADER_ARG("Os=0x%X Hardware=0x%X DisableClk=%d DisablePwr=%d", Os, Hardware, DisableClk, DisablePwr);

    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    down_write(&Os->rwsem_clk_pwr);

    if (DisableClk)
    {
        _ToDisableClock(Os, Hardware->core, &disableClk);
    }

    if (DisablePwr)
    {
        _ToDisablePower(Os, Hardware->core, &disablePwr);
    }

    if (disableClk || disablePwr)
    {
        if(Os->device->powerDebug)
        {
            gcmkPRINT("%s(%#X, %#X, %s, %d, %d) @@@@@ %s [Clk:%d][%d][Pwr:%d][%d] @@@@@", __func__,
                       Os, Hardware, Hardware->core?"2D":"3D", DisableClk, DisablePwr,
                       (Os->clockDepth == 0 && Os->powerDepth == 0) ? "power off" : "suspend",
                       disableClk, Os->clockDepth, disablePwr, Os->powerDepth);
        }

        if(has_feat_separated_power_clock() ||
            (Os->clockDepth == 0 && Os->powerDepth == 0))
        {
            if(Os->device->powerDebug)
            {
                gcmkPRINT("%s(%#X, %#X, %s, %d, %d) <== gckOS_GpuPowerDisable ==>", __func__,
                           Os, Hardware, Hardware->core?"2D":"3D", DisableClk, DisablePwr);
            }

            if(has_feat_separated_power_clock())
                gckOS_GpuPowerDisable(Os, Hardware->core, disableClk, disablePwr);
            else
                gckOS_GpuPowerDisable(Os, Hardware->core, gcvTRUE, gcvTRUE);

            if(Os->device->powerDebug)
            {
                gcmkPRINT("gckOS_GpuPowerDisable done.");
            }
        }
    }

    up_write(&Os->rwsem_clk_pwr);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
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
    signal = (gcsSIGNAL_PTR) kmalloc(sizeof(gcsSIGNAL), GFP_KERNEL | __GFP_NOWARN);

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
        INIT_COMPLETION(signal->obj);
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
#if gcdDETECT_TIMEOUT
        gctINT timeout = (Wait == gcvINFINITE)
            ? gcdINFINITE_TIMEOUT * HZ / 1000
            : Wait * HZ / 1000;

        gctUINT complained = 0;
#else
        gctINT timeout = (Wait == gcvINFINITE)
            ? MAX_SCHEDULE_TIMEOUT
            : Wait * HZ / 1000;
#endif

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

#if gcdDETECT_TIMEOUT
            if ((Wait == gcvINFINITE) && (timeout == 0))
            {
                gctUINT32 dmaAddress1, dmaAddress2;
                gctUINT32 dmaState1, dmaState2;

                dmaState1   = dmaState2   =
                dmaAddress1 = dmaAddress2 = 0;

                /* Verify whether DMA is running. */
                gcmkVERIFY_OK(_VerifyDMA(
                    Os, &dmaAddress1, &dmaAddress2, &dmaState1, &dmaState2
                    ));

#if gcdDETECT_DMA_ADDRESS
                /* Dump only if DMA appears stuck. */
                if (
                    (dmaAddress1 == dmaAddress2)
#if gcdDETECT_DMA_STATE
                 && (dmaState1   == dmaState2)
#endif
                )
#endif
                {
                    /* Increment complain count. */
                    complained += 1;

                    gcmkVERIFY_OK(_DumpGPUState(Os, gcvCORE_MAJOR));

                    gcmkPRINT(
                        "%s(%d): signal 0x%X; forced message flush (%d).",
                        __FUNCTION__, __LINE__, Signal, complained
                        );

                    /* Flush the debug cache. */
                    gcmkDEBUGFLUSH(dmaAddress2);
                }

                /* Reset timeout. */
                timeout = gcdINFINITE_TIMEOUT * HZ / 1000;
            }
#endif

            if (timeout == 0)
            {

                status = gcvSTATUS_TIMEOUT;
                break;
            }
        }

        __remove_wait_queue(&signal->obj.wait, &wait);

#if gcdDETECT_TIMEOUT
        if (complained)
        {
            gcmkPRINT(
                "%s(%d): signal=0x%X; waiting done; status=%d",
                __FUNCTION__, __LINE__, Signal, status
                );
        }
#endif
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

    gcmkONERROR(_QueryIntegerId(&Os->signalDB, (gctUINT32)(gctUINTPTR_T)Signal, (gctPOINTER)&signal));

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
    /* Create a new signal. */
    return gckOS_CreateSignal(Os, ManualReset, (gctSIGNAL *) SignalID);
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
        newSemaphore = (struct semaphore *)kmalloc(gcmSIZEOF(struct semaphore), GFP_KERNEL | __GFP_NOWARN);
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

static gceSTATUS
_GetGcClock(
    IN gceCORE Core,
    OUT struct clk** Clk
    )
{
    static struct clk * clkGC[gcdMAX_GPU_COUNT] = {gcvNULL, gcvNULL, gcvNULL};

    if(clkGC[Core] == gcvNULL)
    {
        struct clk * tmp = gcvNULL;
        *Clk = gcvNULL;

        if(has_feat_separated_gc_clock())
        {
            if(Core == gcvCORE_MAJOR)
            {
                tmp = clk_get(gcvNULL, "GCCLK");
            }
            else if(Core == gcvCORE_2D)
            {
                tmp = clk_get(gcvNULL, "GC2DCLK");
            }
        }
        else
        {
            tmp = clk_get(gcvNULL, "GCCLK");
        }

        if (IS_ERR(tmp))
        {
            gcmkPRINT("%4dL: clk get error: %d\n", __LINE__, PTR_ERR(tmp));
            return gcvSTATUS_GENERIC_IO;
        }

        clkGC[Core] = tmp;
    }

    *Clk = clkGC[Core];
    return gcvSTATUS_OK;
}

#if MRVL_CONFIG_SHADER_CLK_CONTROL
static gceSTATUS
_GetShaderClock(
    IN gceCORE Core,
    OUT struct clk** Clk
    )
{
    static struct clk * clkShader = gcvNULL;

    if(Core != gcvCORE_MAJOR)
        return gcvSTATUS_NOT_SUPPORTED;

    if(clkShader == gcvNULL)
    {
        struct clk * tmp = gcvNULL;
        *Clk = gcvNULL;

        tmp = clk_get(gcvNULL, "GC_SHADER_CLK");

        if (IS_ERR(tmp))
        {
            gcmkPRINT("%4dL: clk get error: %d\n", __LINE__, PTR_ERR(tmp));
            return gcvSTATUS_GENERIC_IO;
        }

        clkShader = tmp;
    }

    *Clk = clkShader;
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_QueryShClkRate(
    IN gckOS Os,
    OUT gctUINT32_PTR Rate
    )
{
    gceSTATUS status;
    gctUINT32 rate = 0;
    struct clk * clkGC = gcvNULL;

    gcmkHEADER_ARG("Os=0x%X Rate=0x%X", Os, Os, Rate);
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Rate != gcvNULL);

    gcmkONERROR(_GetShaderClock(gcvCORE_MAJOR, &clkGC));
    rate = clk_get_rate(clkGC);

    if(rate == (unsigned long)-1)
    {
        *Rate = 0;
        status = gcvSTATUS_INVALID_DATA;
        goto OnError;
    }

    *Rate = rate;
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
OnError:
    gcmkFOOTER();
    return status;
}

gceSTATUS
gckOS_SetShClkRate(
    IN gckOS Os,
    IN gctUINT32 Rate
    )
{
    gceSTATUS status;
    struct clk * clkGC = gcvNULL;

    gcmkHEADER_ARG("Os=0x%X Rate=0x%X", Os, Os, Rate);
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    gcmkONERROR(_GetShaderClock(gcvCORE_MAJOR, &clkGC));
    clk_set_rate(clkGC, Rate);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
OnError:
    gcmkFOOTER();
    return status;
}
#endif

gceSTATUS
gckOS_QueryClkRate(
    IN gckOS Os,
    IN gceCORE Core,
    OUT gctUINT32_PTR Rate
    )
{
    gceSTATUS status;
    gctUINT32 rate = 0;
    struct clk * clkGC = gcvNULL;

    gcmkHEADER_ARG("Os=0x%X Rate=0x%X", Os, Os, Rate);
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);
    gcmkVERIFY_ARGUMENT(Rate != gcvNULL);

    gcmkONERROR(_GetGcClock(Core, &clkGC));
    rate = clk_get_rate(clkGC);

    if(rate == (unsigned long)-1)
    {
        *Rate = 0;
        status = gcvSTATUS_INVALID_DATA;
        goto OnError;
    }

    *Rate = rate;
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
OnError:
    gcmkFOOTER();
    return status;
}

gceSTATUS
gckOS_SetClkRate(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctUINT32 Rate
    )
{
    gceSTATUS status;
    struct clk * clkGC = gcvNULL;

    gcmkHEADER_ARG("Os=0x%X Rate=0x%X", Os, Os, Rate);
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    gcmkONERROR(_GetGcClock(Core, &clkGC));
    clk_set_rate(clkGC, Rate);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
OnError:
    gcmkFOOTER();
    return status;
}

inline static void
_gc_power(
    IN gceCORE Core,
    IN gctBOOL Power_on
    )
{
    if(has_feat_separated_gc_power())
    {
        switch(Core)
        {
        case gcvCORE_MAJOR:
            gc_pwr(Power_on);
            break;
        case gcvCORE_2D:
            gc2d_pwr(Power_on);
            break;
        default:
            gcmkPRINT("%s: invalid core %d val %d\n", __func__, Core, Power_on);
        }
    }
    else
    {
        gc_pwr(Power_on);
    }
}

gceSTATUS
gckOS_GpuPowerEnable(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctBOOL enableClk,
    IN gctBOOL enablePwr,
    IN gctUINT32 Frequency
    )
{
    gceSTATUS status;

    if(has_feat_separated_power_clock() && enablePwr)
    {
        _gc_power(Core, 1);
    }

    if(enableClk)
    {
        struct clk * clkGC = gcvNULL;

        gcmkONERROR(_GetGcClock(Core, &clkGC));

        if(Frequency != 0)
        {
            if (clk_set_rate(clkGC, Frequency))
            {
                gcmkPRINT("Set gpu core clock rate error.");
                return -EAGAIN;
            }
        }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
        clk_prepare_enable(clkGC);
#else
        clk_enable(clkGC);
#endif

        if(Frequency != 0)
        {
            gctUINT32 running_freq = 0;
            running_freq = clk_get_rate(clkGC);
            printk("\n[galcore] GC %d clk1x %p running at %d MHz\n\n", Core, clkGC, (gctUINT32)running_freq/1000/1000);
        }

#if MRVL_CONFIG_SHADER_CLK_CONTROL
        if(has_feat_gc_shader())
        {
            struct clk * clkSH = gcvNULL;

            status = _GetShaderClock(Core, &clkSH);
            if(gcmIS_SUCCESS(status))
            {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
                clk_prepare_enable(clkSH);
#else
                clk_enable(clkSH);
#endif
            }
        }
#endif
    }


    return gcvSTATUS_OK;
OnError:
    return status;
}

gceSTATUS
gckOS_GpuPowerDisable(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctBOOL disableClk,
    IN gctBOOL disablePwr
    )
{
    gceSTATUS status;

    if(disableClk)
    {
#if MRVL_CONFIG_SHADER_CLK_CONTROL
        if(has_feat_gc_shader())
        {
            struct clk * clkSH = gcvNULL;

            status = _GetShaderClock(Core, &clkSH);
            if(gcmIS_SUCCESS(status))
            {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
                clk_disable_unprepare(clkSH);
#else
                clk_disable(clkSH);
#endif
            }
        }
#endif

        {
            struct clk * clkGC = gcvNULL;
            gcmkONERROR(_GetGcClock(Core, &clkGC));
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
            clk_disable_unprepare(clkGC);
#else
            clk_disable(clkGC);
#endif
        }
    }

    if(has_feat_separated_power_clock() && disablePwr)
    {
        _gc_power(Core, 0);
    }

    return gcvSTATUS_OK;
OnError:
    return status;
}

static gceSTATUS
_IdleProfile(
    IN gckOS Os,
    IN gceCORE Core,
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

    kernel          = Os->device->kernels[Core];
    gcmkVERIFY_OBJECT(kernel, gcvOBJ_KERNEL);

    /* Initialize time interval constants. */
    timeSlice       = Os->device->profileTimeSlice;
    tailTimeSlice   = Os->device->profileTailTimeSlice;

    /* Grab the mutex. */
    gcmkONERROR(gckOS_AcquireMutex(Os, kernel->db->profMutex, gcvINFINITE));
    acquired = gcvTRUE;

    gcmkONERROR(gckOS_GetProfileTick(&ticks));
    time = gckOS_ProfileToMS(ticks);

    /* query idle data in a period */
    gcmkONERROR(gckKERNEL_QueryIdleProfile(kernel, time, &timeSlice, &idleTime));
    if(Os->device->profilerDebug)
    {
        gcmkPRINT("[%d]idle:total [%d, %d]\n", Core, idleTime, timeSlice);
    }

    /* query latest idle data */
    gcmkONERROR(gckKERNEL_QueryIdleProfile(kernel, time, &tailTimeSlice, &tailIdleTime));
    if(Os->device->profilerDebug)
    {
        gcmkPRINT("[%d]idle:total [%d, %d]\n", Core, tailIdleTime, tailTimeSlice);
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
#if MRVL_PULSE_EATER_COUNT_NUM
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
#endif
    {
        *DutyCycle = 100;

        return gcvSTATUS_OK;
    }
}

gceSTATUS
gckOS_PowerOffWhenIdle(
    IN gckOS Os,
    IN gceCORE Core,
    IN gctBOOL NeedProfile
    )
{
    gceSTATUS status;
    gckKERNEL   kernel;

    gcmkHEADER_ARG("Os=0x%x NeedProfile=%d", Os, NeedProfile);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    kernel = Os->device->kernels[Core];

    gcmkVERIFY_OBJECT(kernel, gcvOBJ_KERNEL);

    if(NeedProfile)
    {
        gcmkONERROR(_IdleProfile(Os, Core, &Os->device->needPowerOff));

        if(Os->device->profilerDebug)
        {
            gcmkPRINT("[%d]needPowerOff: %d\n", Core, Os->device->needPowerOff);
        }
    }

    if(Os->device->needPowerOff)
    {
        if(Os->device->profilerDebug)
        {
            gcmkPRINT("[%d]power off from current state %d\n", Core, kernel->hardware->chipPowerState);
        }

        gcmkONERROR(gckOS_Broadcast(Os, kernel->hardware, gcvBROADCAST_IDLE_PROFILE));

        if(Os->device->profilerDebug)
        {
            gcmkPRINT("[%d]power off when idle done.\n", Core);
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

#if MRVL_CONFIG_USE_PM_RUNTIME
gceSTATUS
gckOS_GPURuntimeGet(
    IN gckOS Os
)
{
    gcmkHEADER_ARG("Os = 0x%08X", Os);
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    gckOS_AcquireMutex(Os, Os->pmCtrlLock, gcvINFINITE);
    if(!(Os && Os->device && Os->device->dev))
        goto out;

    /* original value is larger than 0. */
    if(atomic_inc_return((atomic_t *)Os->pmCtrlAtom) > 1)
        goto out;

    pm_runtime_get_sync(Os->device->dev);

    if(atomic_read(&Os->device->dev->power.usage_count) > 1)
        Os->device->currentPMode = gcvPM_NORMAL;
    else
        Os->device->currentPMode = gcvPM_AUTO_SUSPEND;

    /* HACK: to separate log levels */
    if(Os->device->pmrtDebug == 2)
    {
        gcmkPRINT("%s: +++++ COMMIT has arrived, %p, %d.\n", __func__,
            Os->device->dev,
            atomic_read(&Os->device->dev->power.usage_count));
    }
out:
    gckOS_ReleaseMutex(Os, Os->pmCtrlLock);
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_GPURuntimePut(
    IN gckOS Os
)
{
    gcmkHEADER_ARG("Os = 0x%08X", Os);
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    gckOS_AcquireMutex(Os, Os->pmCtrlLock, gcvINFINITE);
    if(!(Os && Os->device && Os->device->dev))
        goto out;

    /* pm_runtime_get was not called. */
    if(!atomic_read((atomic_t *)Os->pmCtrlAtom))
        goto out;

    pm_runtime_put_sync_suspend(Os->device->dev);

    atomic_xchg((atomic_t *)Os->pmCtrlAtom, 0);

    /* HACK: to separate log levels */
    if(Os->device->pmrtDebug == 2)
    {
        gcmkPRINT("%s: ----- GPU idle, %p, %d.\n", __func__,
            Os->device->dev,
            atomic_read(&Os->device->dev->power.usage_count));
    }
out:
    gckOS_ReleaseMutex(Os, Os->pmCtrlLock);
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}
#else
gceSTATUS
gckOS_GPURuntimeGet(
    IN gckOS Os
)
{
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_GPURuntimePut(
    IN gckOS Os
)
{
    return gcvSTATUS_OK;
}
#endif

typedef void (*work_func)(struct work_struct *);
gceSTATUS
gckOS_InitDeferrableWork(
    IN gckOS Os,
    IN gctPOINTER Work,
    IN gctPOINTER WorkFunc
    )
{
    gcmkHEADER_ARG("Os = 0x%08X", Os);
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
        INIT_DEFERRABLE_WORK((struct delayed_work *)Work, ((work_func)WorkFunc));
#else
        INIT_DELAYED_WORK_DEFERRABLE((struct delayed_work *)Work, ((work_func)WorkFunc));
#endif

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_ScheduleDelayedWork(
    IN gckOS Os,
    IN gceCHIPPOWERSTATE State,
    IN gctUINT32 Delay
    )
{
    gcmkHEADER_ARG("Os = 0x%08X", Os);
    gcmkVERIFY_OBJECT(Os, gcvOBJ_OS);

    switch(State)
    {
    case gcvPOWER_OFF:
        schedule_delayed_work(&Os->device->pm_work, Delay);
        break;
    default:
        break;
    }

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/******************************************************************************\
******************************** Flush Cache **********************************
\******************************************************************************/
#define BMM_HAS_PTE_PAGE
#ifdef MRVL_OLD_FLUSHCACHE
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

                        kfree(pages);
                        pages = gcvNULL;
                    }

                    vma = find_vma(current->mm, memory);

                    if (vma && (vma->vm_flags & VM_PFNMAP) )
                    {
                        pte_t       * pte;
                        spinlock_t  * ptl;
                        unsigned long pfn;

                        pgd_t * pgd = pgd_offset(current->mm, memory);
                        pud_t * pud = pud_offset(pgd, memory);
                        if (pud)
                        {
                            pmd_t * pmd = pmd_offset(pud, memory);
                            pte = pte_offset_map_lock(current->mm, pmd, memory, &ptl);
                            if (!pte)
                            {
                                gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
                            }
                        }
                        else
                        {
                            gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
                        }

                        pfn      = pte_pfn(*pte);

                        physical = (pfn << PAGE_SHIFT) | (memory & ~PAGE_MASK);

                        pte_unmap_unlock(pte, ptl);

                        if (((Os->device->kernels[Core]->hardware->mmuVersion == 0)
                             && !((physical - Os->device->baseAddress) & 0x80000000))
                            || (Os->device->kernels[Core]->hardware->mmuVersion != 0) )
                        {
#ifdef MRVL_GC_FLUSHCACHE_PFN
                            ;
#else
                            status = gcvSTATUS_OK;
                            break;
#endif
                        }
                        else
                        {
                            gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
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
                           /* FIXME: comment out flush operation for kernel3.10 bringup */
//                            __dma_page_cpu_to_dev(pages[i], 0, PAGE_SIZE, DMA_BIDIRECTIONAL);
                       }
                       else if(i!=0&&i==pageCount-1&&isEndChaDir)
                       {
                           /* FIXME: comment out flush operation for kernel3.10 bringup */
//                            __dma_page_cpu_to_dev(pages[i], 0, PAGE_SIZE, DMA_BIDIRECTIONAL);
                       }
                       else
                       {
                           /* FIXME: comment out flush operation for kernel3.10 bringup */
//                            __dma_page_cpu_to_dev(pages[i], 0, PAGE_SIZE, DMA_BIDIRECTIONAL);
                       }

                       page_cache_release(pages[i]);
                  }

                }
                kfree(pages);
                pages = gcvNULL;
            }
            else if(physical != ~0U)
            {
                /* Flush(clean) the data cache. */
                gcmkONERROR(gckOS_CacheFlush(Os, _GetProcessID(), gcvNULL,
                                             (gctPOINTER)(gctUINTPTR_T)(physical & PAGE_MASK),
                                             (gctPOINTER)(memory & PAGE_MASK),
                                             PAGE_SIZE * pageCount));
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
    //gcmkVERIFY_ARGUMENT(Delay != 0);

    timer = (gcsOSTIMER_PTR)Timer;

    if (unlikely(delayed_work_pending(&timer->work)))
    {
        cancel_delayed_work(&timer->work);
    }

    queue_delayed_work(Os->workqueue, &timer->work, msecs_to_jiffies(Delay));

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
gckOS_Sprintf(
    IN gctCHAR * Buffer,
    IN gctCONST_STRING Message,
    ...
    )
{
    va_list arguments;
    gcmkHEADER_ARG("Buffer=0x%X Message= %s", Buffer, Message);

    gcmkVERIFY_ARGUMENT(Buffer != gcvNULL);
    gcmkVERIFY_ARGUMENT(Message != gcvNULL);

    va_start(arguments, Message);
    if(vsprintf(Buffer, Message, arguments) < 0)
    {
        return gcvSTATUS_OUT_OF_RESOURCES;
    }
    va_end(arguments);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gckOS_StrCmp
**
**  Compare two strings and return whether they match or not.
**
**  INPUT:
**
**      gctCONST_STRING String1
**          Pointer to the first string to compare.
**
**      gctCONST_STRING String2
**          Pointer to the second string to compare.
**
**  OUTPUT:
**
**      Nothing.
**
**  RETURNS:
**
**      gcvSTATUS_OK if the strings match
**      gcvSTATUS_LARGER if String1 > String2
**      gcvSTATUS_SMALLER if String1 < String2
*/
gceSTATUS
gckOS_StrCmp(
    IN gctCONST_STRING String1,
    IN gctCONST_STRING String2
    )
{
    int result;
    gceSTATUS status;

    gcmkHEADER_ARG("String1=0x%x String2=0x%x", String1, String2);

    /* Verify the arguments. */
    gcmkVERIFY_ARGUMENT(String1 != gcvNULL);
    gcmkVERIFY_ARGUMENT(String2 != gcvNULL);

    /* Compare the strings and return proper status. */
    result = strcmp(String1, String2);

    status = (result == 0) ? gcvSTATUS_OK
           : (result >  0) ? gcvSTATUS_LARGER
                           : gcvSTATUS_SMALLER;

    gcmkFOOTER();
    return status;
}

gceSTATUS
gckOS_StrStr(
    IN gctCONST_STRING String,
    IN gctCONST_STRING SubString,
    OUT gctSTRING * Output
    )
{
    gcmkHEADER_ARG("String=0x%x SubString=0x%x", String, SubString);

    /* Verify the arguments. */
    gcmkVERIFY_ARGUMENT(String != gcvNULL);
    gcmkVERIFY_ARGUMENT(SubString != gcvNULL);

    /* Call C. */
    *Output = strstr(String, SubString);

    /* Success. */
    gcmkFOOTER_ARG("*Output=0x%x", *Output);
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_StrLen(
    IN gctCONST_STRING String,
    OUT gctSIZE_T * Length
    )
{
    gcmkHEADER_ARG("String=0x%s Length= %d", String, *Length);

    gcmkVERIFY_ARGUMENT(String != gcvNULL);
    gcmkVERIFY_ARGUMENT(Length != gcvNULL);

    *Length = (gctSIZE_T) strlen(String);

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_OpenFile(
    IN  gckOS Os,
    IN  gctCONST_STRING Filename,
    OUT gctPOINTER* File,
    OUT gctSIZE_T * Old_fs
    )
{
    struct file * tmp  = gcvNULL;
    gcmkHEADER_ARG("Os=0x%X Filename= %s", Os, Filename);

    gcmkVERIFY_ARGUMENT(Filename != gcvNULL);
    gcmkVERIFY_ARGUMENT(File != gcvNULL);
    gcmkVERIFY_ARGUMENT(Old_fs != gcvNULL);

    tmp = filp_open(Filename, O_WRONLY|O_CREAT|O_APPEND, 0644);
    if(tmp == gcvNULL)
    {
        return gcvSTATUS_NOT_SUPPORTED;
    }

    *File   = (gctPOINTER)tmp;
    *Old_fs = (gctSIZE_T)get_fs();
    set_fs(KERNEL_DS);

    /* Success*/
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_WriteFile(
    IN gckOS Os,
    IN gctPOINTER File,
    IN gctPOINTER Logical,
    IN gctSIZE_T  Size)
{
    struct file * tmp  = gcvNULL;
    gcmkHEADER_ARG("Os=0x%X File= 0x%X", Os, File);

    gcmkVERIFY_ARGUMENT(File != gcvNULL);
    gcmkVERIFY_ARGUMENT(Logical != gcvNULL);
    gcmkVERIFY_ARGUMENT(Size > 0);

    tmp = (struct file *)File;
    if((tmp->f_op->write(tmp, Logical, Size, &tmp->f_pos)) < 0)
    {
        return gcvSTATUS_NOT_SUPPORTED;
    }

    /*Success*/
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckOS_CloseFile(
    IN gckOS Os,
    IN gctPOINTER File,
    IN gctSIZE_T Old_fs
    )
{
    struct file * tmp  = gcvNULL;
    gcmkHEADER_ARG("Os=0x%X File= 0x%X", Os, File);

    gcmkVERIFY_ARGUMENT(File != gcvNULL);

    tmp = (struct file *)File;
    set_fs((mm_segment_t)Old_fs);
    filp_close(tmp, gcvNULL);

    /* Success*/
    gcmkFOOTER_NO();
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

#if MRVL_PULSE_EATER_COUNT_NUM
gceSTATUS
gckOS_ModifyPulseEaterPollingPeriod(
    IN gckOS Os,
    IN gctUINT32 Freq,
    IN gctUINT32 Core
    )
{
    gckHARDWARE hardware;
    gctUINT32 core;

    core = (Core == gcvCORE_SH)? gcvCORE_MAJOR: Core;

    hardware = Os->device->kernels[core]->hardware;

    gcmkVERIFY_OK(gckOS_AcquireMutex(Os, hardware->pulseEaterMutex, gcvINFINITE));
    gckHARDWARE_ModifyPeriod(hardware, Freq);
    gcmkVERIFY_OK(gckOS_ReleaseMutex(Os, hardware->pulseEaterMutex));

    return gcvSTATUS_OK;
}
#endif

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

    gcmkONERROR(
        _QueryIntegerId(&Os->syncPointDB,
                        (gctUINT32)(gctUINTPTR_T)SyncPoint,
                        (gctPOINTER)&syncPoint));

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

    /* Get state. */
    atomic_set(&syncPoint->state, gcvTRUE);

    /* Signal timeline. */
    if (syncPoint->timeline)
    {
        sync_timeline_signal(syncPoint->timeline);
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
    timeline = viv_sync_timeline_create("viv timeline", Os);

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
    snprintf(name, 32, "viv sync_fence-%u", (gctUINT)(gctUINTPTR_T)SyncPoint);

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

