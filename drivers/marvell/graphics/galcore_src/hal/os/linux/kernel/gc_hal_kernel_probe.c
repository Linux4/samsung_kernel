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


#include <linux/device.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>

#include "gc_hal_kernel_linux.h"
#include "gc_hal_driver.h"
#include "gc_hal_kernel_sysfs.h"

#if MRVL_CONFIG_ENABLE_GPUFREQ
#include "gpufreq.h"
#endif

#if USE_PLATFORM_DRIVER
#   include <linux/platform_device.h>
#endif

#ifdef CONFIG_PXA_DVFM
#   include <mach/dvfm.h>
#   include <mach/pxa3xx_dvfm.h>
#endif

#if MRVL_GPU_RESOURCE_DT
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

/* Zone used for header/footer. */
#define _GC_OBJ_ZONE    gcvZONE_DRIVER

MODULE_DESCRIPTION("Vivante Graphics Driver");
MODULE_LICENSE("GPL");

static struct class* gpuClass;

static gcsPLATFORM platform;

static gckGALDEVICE galDevice;

static uint major = 199;
module_param(major, uint, 0644);

#if gcdMULTI_GPU || gcdMULTI_GPU_AFFINITY
static int irqLine3D0 = -1;
module_param(irqLine3D0, int, 0644);

static ulong registerMemBase3D0 = 0;
module_param(registerMemBase3D0, ulong, 0644);

static ulong registerMemSize3D0 = 2 << 10;
module_param(registerMemSize3D0, ulong, 0644);

static int irqLine3D1 = -1;
module_param(irqLine3D1, int, 0644);

static ulong registerMemBase3D1 = 0;
module_param(registerMemBase3D1, ulong, 0644);

static ulong registerMemSize3D1 = 2 << 10;
module_param(registerMemSize3D1, ulong, 0644);
#else
static int irqLine = -1;
module_param(irqLine, int, 0644);

static ulong registerMemBase = 0x80000000;
module_param(registerMemBase, ulong, 0644);

static ulong registerMemSize = 2 << 10;
module_param(registerMemSize, ulong, 0644);
#endif

static int irqLine2D = -1;
module_param(irqLine2D, int, 0644);

static ulong registerMemBase2D = 0x00000000;
module_param(registerMemBase2D, ulong, 0644);

static ulong registerMemSize2D = 2 << 10;
module_param(registerMemSize2D, ulong, 0644);

static int irqLineVG = -1;
module_param(irqLineVG, int, 0644);

static ulong registerMemBaseVG = 0x00000000;
module_param(registerMemBaseVG, ulong, 0644);

static ulong registerMemSizeVG = 2 << 10;
module_param(registerMemSizeVG, ulong, 0644);

#ifndef gcdDEFAULT_CONTIGUOUS_SIZE
#define gcdDEFAULT_CONTIGUOUS_SIZE (4 << 20)
#endif
static ulong contiguousSize = gcdDEFAULT_CONTIGUOUS_SIZE;
module_param(contiguousSize, ulong, 0644);

static ulong contiguousBase = 0;
module_param(contiguousBase, ulong, 0644);

static ulong bankSize = 0;
module_param(bankSize, ulong, 0644);

static int fastClear = -1;
module_param(fastClear, int, 0644);

static int compression = -1;
module_param(compression, int, 0644);

#if gcdFPGA_BUILD
static int powerManagement = 0;
#else
static int powerManagement = 1;
#endif
module_param(powerManagement, int, 0644);

#if VIVANTE_PROFILER
static int gpuProfiler = 1;
#else
static int gpuProfiler = 0;
#endif

module_param(gpuProfiler, int, 0644);

static int signal = 48;
module_param(signal, int, 0644);

static ulong baseAddress = 0;
module_param(baseAddress, ulong, 0644);

#if MRVL_PLATFORM_TTD2_FPGA
static ulong physSize = 0x20000000;
#else
static ulong physSize = 0x40000000;
#endif
module_param(physSize, ulong, 0644);

static uint logFileSize = 0;
module_param(logFileSize,uint, 0644);

static uint recovery = 0;
module_param(recovery, uint, 0644);
MODULE_PARM_DESC(recovery, "Recover GPU from stuck (1: Enable, 0: Disable)");

/* Middle needs about 40KB buffer, Maximal may need more than 200KB buffer. */
static uint stuckDump = 2;
module_param(stuckDump, uint, 0644);
MODULE_PARM_DESC(stuckDump, "Level of stuck dump content (1: Minimal, 2: Middle, 3: Maximal)");

static int showArgs = 1;
module_param(showArgs, int, 0644);

static int mmu = 1;
module_param(mmu, int, 0644);

static int gpu3DMinClock = 1;

static int contiguousRequested = 0;


static gctBOOL registerMemMapped = gcvFALSE;
static gctPOINTER registerMemAddress = gcvNULL;

#if ENABLE_GPU_CLOCK_BY_DRIVER
static unsigned long coreClock = 624;
module_param(coreClock, ulong, 0644);

static unsigned long coreClock2D = 312;
module_param(coreClock2D, ulong, 0644);
#endif

static long pmemSize = 0;
module_param(pmemSize, long, 0644);

/******************************************************************************\
* Create a data entry system using proc for GC
\******************************************************************************/

#if MRVL_CONFIG_PROC
#include <linux/proc_fs.h>

#define GC_PROC_FILE        "driver/gc"
#define GC_PROC_MEMFOLD     "driver/gcmem"
#define _GC_OBJ_ZONE        gcvZONE_DRIVER

static struct proc_dir_entry * gc_proc_file;
static struct proc_dir_entry * gc_procmem_folder;

/* cat /proc/driver/gc will print gc related msg */
static ssize_t gc_proc_read(
    struct file *file,
    char __user *buffer,
    size_t count,
    loff_t *offset);

/* echo xx > /proc/driver/gc set ... */
static ssize_t gc_proc_write(
    struct file *file,
    const char *buff,
    size_t len,
    loff_t *off);

static struct file_operations gc_proc_ops = {
    .read = gc_proc_read,
    .write = gc_proc_write,
};

static int proc_show(struct seq_file *m, void *buf)
{
    gctUINT32 i;
    gctUINT32_PTR pid = (gctUINT32_PTR)m->private;
    gctUINT32 length = 0;

    for(i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if(galDevice->kernels[i])
        {
            gctUINT32 tmp = 0;

            gckKERNEL_ShowProcessVidMemUsage((char *)buf+length, galDevice->kernels[i], *pid, &tmp);
            length += tmp;
        }
    }
    return length;
}

static int gc_procmem_open(struct inode *inode, struct file *file)
{
    gctUINT32_PTR data = PDE_DATA(file_inode(file));

    file->private_data = gcvNULL;
    return single_open(file, &proc_show, data);
}

static ssize_t gc_proc_mem_read(
    struct file *file,
    char __user *buffer,
    size_t count,
    loff_t *offset)
{
    ssize_t len = 0;
    char buf[500];

    len = proc_show((struct seq_file *)file->private_data, buf);

    return simple_read_from_buffer(buffer, count, offset, buf, len);
}

static const struct file_operations gc_procmem_ops = {
    .open    = gc_procmem_open,
    .read    = gc_proc_mem_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

static gceSTATUS _gc_gather_infomation(char *buf, ssize_t* length)
{
    gceSTATUS status;
    ssize_t len = 0;
    gctUINT32 pid = 0;

    /* #################### [START ==DO NOT CHANGE THE FIRST LINE== START] #################### */
    /* @Ziyi: This string is checked by skia-neon related code to identify Marvell silicon,
              please do not change it and always keep it at the first line of /proc/driver/gc ! */
    gckOS_GetProcessID(&pid);
    len += sprintf(buf+len, "[%3d] %s (%s)\n", pid, _VENDOR_STRING_, _GC_VERSION_STRING_);
    /* @Ziyi: If any change happened between these 2 comments please contact zyxu@marvell.com, Thanks. */
    /* #################### [END ====DO NOT CHANGE THE FIRST LINE==== END] #################### */

    if(1)
    {
        gctUINT32 tmpLen = 0;
        gcmkONERROR(gckOS_ShowVidMemUsage(galDevice->os, buf+len, &tmpLen));
        len += tmpLen;
    }

    *length = len;
    return gcvSTATUS_OK;

OnError:
    return status;
}

static ssize_t gc_proc_read(
    struct file *file,
    char __user *buffer,
    size_t count,
    loff_t *offset)
{
    gceSTATUS status;
    ssize_t len = 0;
    char buf[1000];

    gcmkONERROR(_gc_gather_infomation(buf, &len));

    return simple_read_from_buffer(buffer, count, offset, buf, len);

OnError:
    return 0;
}

static ssize_t gc_proc_write(
    struct file *file,
    const char *buff,
    size_t len,
    loff_t *off)
{
    char messages[256];

    if(len > 256)
        len = 256;

    if(copy_from_user(messages, buff, len))
        return -EFAULT;

    printk("\n");
    if(strncmp(messages, "reg", 3) == 0)
    {
        gctINT32 option;
        gctUINT32 idle, address, clockControl;

        sscanf(messages+3, "%d", &option);

        switch(option) {
        case 1:
            /* Read the current FE address. */
            gckOS_ReadRegisterEx(galDevice->os,
                                 galDevice->kernels[0]->hardware->core,
                                 0x00664,
                                 &address);
            gcmkPRINT("address: 0x%2x\n", address);
            break;
        case 2:
            /* Read idle register. */
            gckOS_ReadRegisterEx(galDevice->os,
                                 galDevice->kernels[0]->hardware->core,
                                 0x00004,
                                 &idle);
            gcmkPRINT("idle: 0x%2x\n", idle);
            break;
        case 3:
            gckOS_ReadRegisterEx(galDevice->os,
                                 gcvCORE_MAJOR,
                                 0x00000,
                                 &clockControl);
            gcmkPRINT("clockControl: 0x%2x\n", clockControl);
            break;
        default:
            printk(KERN_INFO "usage: echo reg [1|2|3] > /proc/driver/gc\n");
        }
    }
    else if(strncmp(messages, "help", 4) == 0)
    {
        printk("Supported options:\n"
                "dutycycle       measure dutycycle in a period\n"
                "reg             enable debugging for register reading\n"
                "help            show this help page\n"
                "\n");
    }
    else
    {
        gcmkPRINT("unknown echo\n");
    }
    printk("\n");

    return len;
}

static void create_gc_proc_file(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
    gc_proc_file = proc_create(GC_PROC_FILE, 0644, gcvNULL, &gc_proc_ops);
    gc_procmem_folder = proc_mkdir(GC_PROC_MEMFOLD,gcvNULL);
    if(!gc_procmem_folder)
    {
        gcmkPRINT("[galcore] procmem folder create failed!\n");
    }
    if(!gc_proc_file)
#else
    gc_proc_file = create_proc_entry(GC_PROC_FILE, 0644, gcvNULL);
    gc_procmem_folder = proc_mkdir(GC_PROC_MEMFOLD,gcvNULL);
    if(!gc_procmem_folder)
    {
        gcmkPRINT("[galcore] procmem folder create failed!\n");
    }

    if (gc_proc_file)
    {
        gc_proc_file->proc_fops = &gc_proc_ops;
    } else
#endif
    gcmkPRINT("[galcore] proc file create failed!\n");
}

static void remove_gc_proc_file(void)
{
    if(gc_proc_file)
    {
        remove_proc_entry(GC_PROC_FILE, gcvNULL);
    }
    if(gc_procmem_folder)
    {
        remove_proc_entry(GC_PROC_MEMFOLD, gcvNULL);
    }
}

static void create_gc_procmem_file(IN struct proc_dir_entry **gc_procmem_file, IN gctUINT32_PTR pid)
{
    char procname[50];

    sprintf(procname, "%s-%d","gcmem" ,*pid);
    *gc_procmem_file = proc_create_data(procname, 0644, gc_procmem_folder, &gc_procmem_ops, pid);
    if (!(*gc_procmem_file))
    {
        gcmkPRINT("[galcore] proc file create failed!\n");
    }
}

static void remove_gc_procmem_file(IN struct proc_dir_entry **gc_procmem_file, IN gctUINT32_PTR pid)
{
    char procname[50];
    sprintf(procname, "%s-%d","gcmem" ,*pid);

    if(*gc_procmem_file)
    {
        remove_proc_entry(procname, gc_procmem_folder);
    }
}

#endif

static int drv_open(
    struct inode* inode,
    struct file* filp
    );

static int drv_release(
    struct inode* inode,
    struct file* filp
    );

static long drv_ioctl(
    struct file* filp,
    unsigned int ioctlCode,
    unsigned long arg
    );

static int drv_mmap(
    struct file* filp,
    struct vm_area_struct* vma
    );

static struct file_operations driver_fops =
{
    .owner      = THIS_MODULE,
    .open       = drv_open,
    .release    = drv_release,
    .unlocked_ioctl = drv_ioctl,
#ifdef HAVE_COMPAT_IOCTL
    .compat_ioctl = drv_ioctl,
#endif
    .mmap       = drv_mmap,
};

void
_UpdateModuleParam(
    gcsMODULE_PARAMETERS *Param
    )
{
#if gcdMULTI_GPU || gcdMULTI_GPU_AFFINITY
#else
    irqLine           = Param->irqLine ;
    registerMemBase   = Param->registerMemBase;
    registerMemSize   = Param->registerMemSize;
#endif
    irqLine2D         = Param->irqLine2D      ;
    registerMemBase2D = Param->registerMemBase2D;
    registerMemSize2D = Param->registerMemSize2D;
#if gcdENABLE_VG
    irqLineVG         = Param->irqLineVG;
    registerMemBaseVG = Param->registerMemBaseVG;
    registerMemSizeVG = Param->registerMemSizeVG;
#endif
    contiguousSize    = Param->contiguousSize;
    contiguousBase    = Param->contiguousBase;
    bankSize          = Param->bankSize;
    fastClear         = Param->fastClear;
    compression       = Param->compression;
    powerManagement   = Param->powerManagement;
    gpuProfiler       = Param->gpuProfiler;
    signal            = Param->signal;
    baseAddress       = Param->baseAddress;
    physSize          = Param->physSize;
    logFileSize       = Param->logFileSize;
    recovery          = Param->recovery;
    stuckDump         = Param->stuckDump;
    showArgs          = Param->showArgs;
    contiguousRequested = Param->contiguousRequested;
    gpu3DMinClock     = Param->gpu3DMinClock;
    registerMemMapped    = Param->registerMemMapped;
    registerMemAddress    = Param->registerMemAddress;
}

void
gckOS_DumpParam(
    void
    )
{
    printk("Galcore options:\n");
#if gcdMULTI_GPU || gcdMULTI_GPU_AFFINITY
    printk("  irqLine3D0         = %d\n",      irqLine3D0);
    printk("  registerMemBase3D0 = 0x%08lX\n", registerMemBase3D0);
    printk("  registerMemSize3D0 = 0x%08lX\n", registerMemSize3D0);

    if (irqLine3D1 != -1)
    {
        printk("  irqLine3D1         = %d\n",      irqLine3D1);
        printk("  registerMemBase3D1 = 0x%08lX\n", registerMemBase3D1);
        printk("  registerMemSize3D1 = 0x%08lX\n", registerMemSize3D1);
    }
#else
    printk("  irqLine           = %d\n",      irqLine);
    printk("  registerMemBase   = 0x%08lX\n", registerMemBase);
    printk("  registerMemSize   = 0x%08lX\n", registerMemSize);
#endif

    if (irqLine2D != -1)
    {
        printk("  irqLine2D         = %d\n",      irqLine2D);
        printk("  registerMemBase2D = 0x%08lX\n", registerMemBase2D);
        printk("  registerMemSize2D = 0x%08lX\n", registerMemSize2D);
    }

    if (irqLineVG != -1)
    {
        printk("  irqLineVG         = %d\n",      irqLineVG);
        printk("  registerMemBaseVG = 0x%08lX\n", registerMemBaseVG);
        printk("  registerMemSizeVG = 0x%08lX\n", registerMemSizeVG);
    }

    printk("  contiguousSize    = %ld\n",     contiguousSize);
    printk("  contiguousBase    = 0x%08lX\n", contiguousBase);
    printk("  bankSize          = 0x%08lX\n", bankSize);
    printk("  fastClear         = %d\n",      fastClear);
    printk("  compression       = %d\n",      compression);
    printk("  signal            = %d\n",      signal);
    printk("  powerManagement   = %d\n",      powerManagement);
    printk("  baseAddress       = 0x%08lX\n", baseAddress);
    printk("  physSize          = 0x%08lX\n", physSize);
    printk("  logFileSize       = %d KB \n",  logFileSize);
    printk("  recovery          = %d\n",      recovery);
    printk("  stuckDump         = %d\n",      stuckDump);
#if ENABLE_GPU_CLOCK_BY_DRIVER
    printk("  coreClock         = %lu\n",     coreClock);
    printk("  coreClock2D       = %lu\n",     coreClock2D);
#endif
    printk("  gpuProfiler       = %d\n",      gpuProfiler);
}

int drv_open(
    struct inode* inode,
    struct file* filp
    )
{
    gceSTATUS status;
    gctBOOL attached = gcvFALSE;
    gcsHAL_PRIVATE_DATA_PTR data = gcvNULL;
    gctINT i;

    gcmkHEADER_ARG("inode=0x%08X filp=0x%08X", inode, filp);

    if (filp == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): filp is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    data = kmalloc(sizeof(gcsHAL_PRIVATE_DATA), GFP_KERNEL | __GFP_NOWARN);

    if (data == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): private_data is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    data->device             = galDevice;
    data->mappedMemory       = gcvNULL;
    data->contiguousLogical  = gcvNULL;
    gcmkONERROR(gckOS_GetProcessID(&data->pidOpen));

#if MRVL_CONFIG_PROC
    create_gc_procmem_file(&(data->gc_procmem_file),&(data->pidOpen));
#endif

    /* Attached the process. */
    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (galDevice->kernels[i] != gcvNULL)
        {
            gcmkONERROR(gckKERNEL_AttachProcess(galDevice->kernels[i], gcvTRUE));
        }
    }
    attached = gcvTRUE;

    if (!galDevice->contiguousMapped)
    {
        if (galDevice->contiguousPhysical != gcvNULL)
        {
            gcmkONERROR(gckOS_MapMemory(
                galDevice->os,
                galDevice->contiguousPhysical,
                galDevice->contiguousSize,
                &data->contiguousLogical
                ));
        }
    }

    filp->private_data = data;

    /* Success. */
    gcmkFOOTER_NO();
    return 0;

OnError:
    if (data != gcvNULL)
    {
        if (data->contiguousLogical != gcvNULL)
        {
            gcmkVERIFY_OK(gckOS_UnmapMemory(
                galDevice->os,
                galDevice->contiguousPhysical,
                galDevice->contiguousSize,
                data->contiguousLogical
                ));
        }

        kfree(data);
    }

    if (attached)
    {
        for (i = 0; i < gcdMAX_GPU_COUNT; i++)
        {
            if (galDevice->kernels[i] != gcvNULL)
            {
                gcmkVERIFY_OK(gckKERNEL_AttachProcess(galDevice->kernels[i], gcvFALSE));
            }
        }
    }

    gcmkFOOTER();
    return -ENOTTY;
}

int drv_release(
    struct inode* inode,
    struct file* filp
    )
{
    gceSTATUS status;
    gcsHAL_PRIVATE_DATA_PTR data;
    gckGALDEVICE device;
    gctINT i;
    gctBOOL acquired = gcvFALSE;
    struct gpu_pm_context_info *pm_ctx = gcvNULL;

    gcmkHEADER_ARG("inode=0x%08X filp=0x%08X", inode, filp);

    if (filp == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): filp is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    data = filp->private_data;

    if (data == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): private_data is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    device = data->device;

    if (device == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): device is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    pm_ctx = &device->pm_ctx;

    /* acquire mutex to prevent suspend concurrently */
    mutex_lock(&pm_ctx->lock);
    acquired = gcvTRUE;

    if (!device->contiguousMapped)
    {
        if (data->contiguousLogical != gcvNULL)
        {
            gcmkONERROR(gckOS_UnmapMemoryEx(
                galDevice->os,
                galDevice->contiguousPhysical,
                galDevice->contiguousSize,
                data->contiguousLogical,
                data->pidOpen
                ));

            data->contiguousLogical = gcvNULL;
        }
    }

    /* A process gets detached. */
    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (galDevice->kernels[i] != gcvNULL)
        {
            gcmkONERROR(gckKERNEL_AttachProcessEx(galDevice->kernels[i], gcvFALSE, data->pidOpen));
        }
    }

#if MRVL_CONFIG_PROC
    remove_gc_procmem_file(&(data->gc_procmem_file),&(data->pidOpen));
    data->gc_procmem_file = gcvNULL;
#endif

    mutex_unlock(&pm_ctx->lock);

    kfree(data);
    filp->private_data = NULL;

    /* Success. */
    gcmkFOOTER_NO();
    return 0;

OnError:
    if(acquired)
        mutex_unlock(&pm_ctx->lock);

    gcmkFOOTER();
    return -ENOTTY;
}

long drv_ioctl(
    struct file* filp,
    unsigned int ioctlCode,
    unsigned long arg
    )
{
    gceSTATUS status;
    gcsHAL_INTERFACE iface;
    gctUINT32 copyLen;
    DRIVER_ARGS drvArgs;
    gckGALDEVICE device;
    gcsHAL_PRIVATE_DATA_PTR data;
    gctINT32 i, count;
    gckVIDMEM_NODE nodeObject;

    gcmkHEADER_ARG(
        "filp=0x%08X ioctlCode=0x%08X arg=0x%08X",
        filp, ioctlCode, arg
        );

    if (filp == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): filp is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    data = filp->private_data;

    if (data == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): private_data is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    device = data->device;

    if (device == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): device is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    if ((ioctlCode != IOCTL_GCHAL_INTERFACE)
    &&  (ioctlCode != IOCTL_GCHAL_KERNEL_INTERFACE)
    )
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): unknown command %d\n",
            __FUNCTION__, __LINE__,
            ioctlCode
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    /* Get the drvArgs. */
    copyLen = copy_from_user(
        &drvArgs, (void *) arg, sizeof(DRIVER_ARGS)
        );

    if (copyLen != 0)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): error copying of the input arguments.\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    /* Now bring in the gcsHAL_INTERFACE structure. */
    if ((drvArgs.InputBufferSize  != sizeof(gcsHAL_INTERFACE))
    ||  (drvArgs.OutputBufferSize != sizeof(gcsHAL_INTERFACE))
    )
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): input %lu or/and output %lu structures are invalid with %lu.\n",
            __FUNCTION__, __LINE__,drvArgs.InputBufferSize,drvArgs.OutputBufferSize,sizeof(gcsHAL_INTERFACE)
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    copyLen = copy_from_user(
        &iface, gcmUINT64_TO_PTR(drvArgs.InputBuffer), sizeof(gcsHAL_INTERFACE)
        );

    if (copyLen != 0)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): error copying of input HAL interface.\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    if (iface.command == gcvHAL_CHIP_INFO)
    {
        count = 0;
        for (i = 0; i < gcdMAX_GPU_COUNT; i++)
        {
            if (device->kernels[i] != gcvNULL)
            {
#if gcdENABLE_VG
                if (i == gcvCORE_VG)
                {
                    iface.u.ChipInfo.types[count] = gcvHARDWARE_VG;
                }
                else
#endif
                {
                    gcmkVERIFY_OK(gckHARDWARE_GetType(device->kernels[i]->hardware,
                                                      &iface.u.ChipInfo.types[count]));
                }
                count++;
            }
        }

        iface.u.ChipInfo.count = count;
        iface.status = status = gcvSTATUS_OK;
    }
    else
    {
        if (iface.hardwareType > 7)
        {
            gcmkTRACE_ZONE(
                gcvLEVEL_ERROR, gcvZONE_DRIVER,
                "%s(%d): unknown hardwareType %d\n",
                __FUNCTION__, __LINE__,
                iface.hardwareType
                );

            gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }

#if gcdENABLE_VG
        if (device->coreMapping[iface.hardwareType] == gcvCORE_VG)
        {
            status = gckVGKERNEL_Dispatch(device->kernels[gcvCORE_VG],
                                        (ioctlCode == IOCTL_GCHAL_INTERFACE),
                                        &iface);
        }
        else
#endif
        {
            status = gckKERNEL_Dispatch(device->kernels[device->coreMapping[iface.hardwareType]],
                                        (ioctlCode == IOCTL_GCHAL_INTERFACE),
                                        &iface);
        }
    }

    /* Redo system call after pending signal is handled. */
    if (status == gcvSTATUS_INTERRUPTED)
    {
        gcmkFOOTER();
        return -ERESTARTSYS;
    }

    if (gcmIS_SUCCESS(status) && (iface.command == gcvHAL_LOCK_VIDEO_MEMORY))
    {
        gcuVIDMEM_NODE_PTR node;
        gctUINT32 processID;

        gckOS_GetProcessID(&processID);

        gcmkONERROR(gckVIDMEM_HANDLE_Lookup(device->kernels[device->coreMapping[iface.hardwareType]],
                                processID,
                                (gctUINT32)iface.u.LockVideoMemory.node,
                                &nodeObject));
        node = nodeObject->node;

        /* Special case for mapped memory. */
        if ((data->mappedMemory != gcvNULL)
        &&  (node->VidMem.memory->object.type == gcvOBJ_VIDMEM)
        )
        {
            /* Compute offset into mapped memory. */
            gctUINT32 offset
                = (gctUINT8 *) gcmUINT64_TO_PTR(iface.u.LockVideoMemory.memory)
                - (gctUINT8 *) device->contiguousBase;

            /* Compute offset into user-mapped region. */
            iface.u.LockVideoMemory.memory =
                gcmPTR_TO_UINT64((gctUINT8 *) data->mappedMemory + offset);
        }
    }

    /* Copy data back to the user. */
    copyLen = copy_to_user(
        gcmUINT64_TO_PTR(drvArgs.OutputBuffer), &iface, sizeof(gcsHAL_INTERFACE)
        );

    if (copyLen != 0)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): error copying of output HAL interface.\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    /* Success. */
    gcmkFOOTER_NO();
    return 0;

OnError:
    gcmkFOOTER();
    return -ENOTTY;
}

static int drv_mmap(
    struct file* filp,
    struct vm_area_struct* vma
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsHAL_PRIVATE_DATA_PTR data;
    gckGALDEVICE device;

    gcmkHEADER_ARG("filp=0x%08X vma=0x%08X", filp, vma);

    if (filp == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): filp is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    data = filp->private_data;

    if (data == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): private_data is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    device = data->device;

    if (device == gcvNULL)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): device is NULL\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

#if !gcdPAGED_MEMORY_CACHEABLE
    vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
    vma->vm_flags    |= gcdVM_FLAGS;
#endif
    vma->vm_pgoff     = 0;

    if (device->contiguousMapped)
    {
        unsigned long size = vma->vm_end - vma->vm_start;
        int ret = 0;

        if (size > device->contiguousSize)
        {
            gcmkTRACE_ZONE(
                gcvLEVEL_ERROR, gcvZONE_DRIVER,
                "%s(%d): Invalid mapping size.\n",
                __FUNCTION__, __LINE__
                );

            gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }

        ret = io_remap_pfn_range(
            vma,
            vma->vm_start,
            device->requestedContiguousBase >> PAGE_SHIFT,
            size,
            vma->vm_page_prot
            );

        if (ret != 0)
        {
            gcmkTRACE_ZONE(
                gcvLEVEL_ERROR, gcvZONE_DRIVER,
                "%s(%d): io_remap_pfn_range failed %d\n",
                __FUNCTION__, __LINE__,
                ret
                );

            data->mappedMemory = gcvNULL;

            gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
        }

        data->mappedMemory = (gctPOINTER) vma->vm_start;

        /* Success. */
        gcmkFOOTER_NO();
        return 0;
    }

OnError:
    gcmkFOOTER();
    return -ENOTTY;
}


#if !USE_PLATFORM_DRIVER
static int __init drv_init(void)
#else
static int drv_init(void)
#endif
{
    int ret, i;
    int result = -EINVAL;
    gceSTATUS status;
    gckGALDEVICE device = galDevice;
    struct class* device_class = gcvNULL;

    gcsDEVICE_CONSTRUCT_ARGS args = {
        .recovery           = recovery,
        .stuckDump          = stuckDump,
        .gpu3DMinClock      = gpu3DMinClock,
        .contiguousRequested = contiguousRequested,
        .platform           = &platform,
        .mmu                = mmu,
        .registerMemMapped = registerMemMapped,
        .registerMemAddress = registerMemAddress,
    };

    gcmkHEADER();

#if ENABLE_GPU_CLOCK_BY_DRIVER && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
    {
        gcmkONERROR(gckOS_SetGPUPowerOnBeforeInit((gctPOINTER)galDevice->pdev[gcvCORE_MAJOR],
                                                  gcvCORE_MAJOR,
                                                  gcvTRUE,
                                                  gcvTRUE,
                                                  0 * 1000));

        if (has_feat_separated_gc_clock())
        {

            gcmkONERROR(gckOS_SetGPUPowerOnBeforeInit((gctPOINTER)galDevice->pdev[gcvCORE_2D],
                                                       gcvCORE_2D,
                                                       gcvTRUE,
                                                       (has_feat_separated_gc_power()
                                                         ? gcvTRUE
                                                         : gcvFALSE),
                                                       0 * 1000));
        }
    }
#endif

    printk(KERN_INFO "Galcore version %d.%d.%d.%d\n",
        gcvVERSION_MAJOR, gcvVERSION_MINOR, gcvVERSION_PATCH, gcvVERSION_BUILD);

#if !VIVANTE_PROFILER_PM
    /* when enable gpu profiler, we need to turn off gpu powerMangement */
    if (gpuProfiler)
    {
        powerManagement = 0;
    }
#endif

    if (showArgs)
    {
        gckOS_DumpParam();
    }
#if MRVL_MMU_FLATMAP_2G_ADDRESS
    physSize = 0x80000000;
    printk("  physSize modified to          = 0x%08lX\n", physSize);
#endif

    if (logFileSize != 0)
    {
        gckDEBUGFS_Initialize();
    }

    /* Create the GAL device. */
    status = gckGALDEVICE_Construct(
#if gcdMULTI_GPU || gcdMULTI_GPU_AFFINITY
        irqLine3D0,
        registerMemBase3D0, registerMemSize3D0,
        irqLine3D1,
        registerMemBase3D1, registerMemSize3D1,
#else
        irqLine,
        registerMemBase, registerMemSize,
#endif
        irqLine2D,
        registerMemBase2D, registerMemSize2D,
        irqLineVG,
        registerMemBaseVG, registerMemSizeVG,
        contiguousBase, contiguousSize,
        bankSize, fastClear, compression, baseAddress, physSize, signal,
        logFileSize,
        powerManagement,
        gpuProfiler,
        &args,
        device
    );

    if (gcmIS_ERROR(status))
    {
        gcmkTRACE_ZONE(gcvLEVEL_ERROR, gcvZONE_DRIVER,
                       "%s(%d): Failed to create the GAL device: status=%d\n",
                       __FUNCTION__, __LINE__, status);

        goto OnError;
    }

    /* Start the GAL device. */
    gcmkONERROR(gckGALDEVICE_Start(device));

    if ((physSize != 0)
       && (device->kernels[gcvCORE_MAJOR] != gcvNULL)
       && (device->kernels[gcvCORE_MAJOR]->hardware->mmuVersion != 0))
    {
        /* Reset the base address */
        device->baseAddress = 0;
    }

    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if(device->kernels[i] != gcvNULL)
        {
            device->kernels[i]->bTryIdleGPUEnable = gcvTRUE;
        }
    }

    /* Register the character device. */
    ret = register_chrdev(major, DEVICE_NAME, &driver_fops);

    if (ret < 0)
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): Could not allocate major number for mmap.\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_OUT_OF_MEMORY);
    }

    if (major == 0)
    {
        major = ret;
    }

    /* Create the device class. */
    device_class = class_create(THIS_MODULE, CLASS_NAME);

    if (IS_ERR(device_class))
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): Failed to create the class.\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
    device_create(device_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
#else
    device_create(device_class, NULL, MKDEV(major, 0), DEVICE_NAME);
#endif

    gpuClass  = device_class;

#if gcdMULTI_GPU || gcdMULTI_GPU_AFFINITY
    gcmkTRACE_ZONE(
        gcvLEVEL_INFO, gcvZONE_DRIVER,
        "%s(%d): irqLine3D0=%d, contiguousSize=%lu, memBase3D0=0x%lX\n",
        __FUNCTION__, __LINE__,
        irqLine3D0, contiguousSize, registerMemBase3D0
        );
#else
    gcmkTRACE_ZONE(
        gcvLEVEL_INFO, gcvZONE_DRIVER,
        "%s(%d): irqLine=%d, contiguousSize=%lu, memBase=0x%lX\n",
        __FUNCTION__, __LINE__,
        irqLine, contiguousSize, registerMemBase
        );
#endif

    /* Success. */
    gcmkFOOTER_NO();
    return 0;

OnError:
    /* Roll back. */
    if (device_class != gcvNULL)
    {
        device_destroy(device_class, MKDEV(major, 0));
        class_destroy(device_class);
    }

    if (device != gcvNULL)
    {
        gcmkVERIFY_OK(gckGALDEVICE_Stop(device));
        gcmkVERIFY_OK(gckGALDEVICE_Destroy(device));
    }

    gcmkFOOTER();
    return result;
}

#if !USE_PLATFORM_DRIVER
static void __exit drv_exit(void)
#else
static void drv_exit(void)
#endif
{
    gcmkHEADER();

    gcmkASSERT(gpuClass != gcvNULL);
    device_destroy(gpuClass, MKDEV(major, 0));
    class_destroy(gpuClass);

    unregister_chrdev(major, DEVICE_NAME);

    gcmkVERIFY_OK(gckGALDEVICE_Stop(galDevice));
    gcmkVERIFY_OK(gckGALDEVICE_Destroy(galDevice));

    if(gckDEBUGFS_IsEnabled())
    {
        gckDEBUGFS_Terminate();
    }

#if ENABLE_GPU_CLOCK_BY_DRIVER && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
    {
        if(has_feat_separated_gc_clock())
        {
            gcmkVERIFY_OK(gckOS_SetGPUPowerOffMRVL(galDevice->os,
                                                   gcvCORE_2D,
                                                   gcvTRUE,
                                                   (has_feat_separated_gc_power()
                                                        ? gcvTRUE
                                                        : gcvFALSE),
                                                        gcvFALSE));
        }
        gcmkVERIFY_OK(gckOS_SetGPUPowerOffMRVL(galDevice->os, gcvCORE_MAJOR, gcvTRUE, gcvTRUE, gcvFALSE));
    }
#endif

    gcmkFOOTER_NO();
}

#if !USE_PLATFORM_DRIVER
    module_init(drv_init);
    module_exit(drv_exit);
#else

#if MRVL_CONFIG_ENABLE_GPUFREQ
static int __enable_gpufreq(gckGALDEVICE device)
{
    gctUINT32 clockRate = 0;
    gceSTATUS status;
    int i;

    status = gckOS_QueryClkRate(device->os, gcvCORE_MAJOR, gcvFALSE, &clockRate);

    if(gcmIS_SUCCESS(status) && clockRate != 0)
    {
        gpufreq_init(device->os);
        printk("[galcore] gpufreq inited\n");
    }

    for(i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if(device->kernels[i] != gcvNULL)
        {
            gckHARDWARE hardware = device->kernels[i]->hardware;

            if(!hardware->devObj.kobj)
                continue;

            gckOS_GPUFreqNotifierCallChain(
                            device->os,
                            GPUFREQ_GPU_EVENT_INIT,
                            (gctPOINTER) &hardware->devObj);

#if MRVL_CONFIG_SHADER_CLK_CONTROL
            if(has_feat_shader_indept_dfc() && i == gcvCORE_MAJOR)
            {
                if(!hardware->devShObj.kobj)
                    continue;

                gckOS_GPUFreqNotifierCallChain(
                                device->os,
                                GPUFREQ_GPU_EVENT_INIT,
                                (gctPOINTER) &hardware->devShObj);
            }
#endif
        }
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
    kfree(device->ft);
    device->ft = gcvNULL;
    memset(device->os->freqTable, 0, sizeof(device->os->freqTable));
#endif

    return 0;
}

static int __disable_gpufreq(gckGALDEVICE device)
{
    gctUINT32 clockRate = 0;
    gceSTATUS status;
    int i;

    for(i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if(device->kernels[i] != gcvNULL)
        {
            gckHARDWARE hardware = device->kernels[i]->hardware;

            if(!hardware->devObj.kobj)
                continue;

            gckOS_GPUFreqNotifierCallChain(
                            device->os,
                            GPUFREQ_GPU_EVENT_DESTORY,
                            (gctPOINTER) &hardware->devObj);

#if MRVL_CONFIG_SHADER_CLK_CONTROL
            if(has_feat_shader_indept_dfc() && i == gcvCORE_MAJOR)
            {
                if(!hardware->devShObj.kobj)
                    continue;

                gckOS_GPUFreqNotifierCallChain(
                                device->os,
                                GPUFREQ_GPU_EVENT_DESTORY,
                                (gctPOINTER) &hardware->devShObj);
            }
#endif
        }
    }

    status = gckOS_QueryClkRate(device->os, gcvCORE_MAJOR, gcvFALSE, &clockRate);

    if(gcmIS_SUCCESS(status) && clockRate != 0)
    {
        gpufreq_exit(device->os);
        printk("[galcore] gpufreq exited\n");
    }

    return 0;
}
#endif

#if MRVL_GPU_RESOURCE_DT
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
#define MEM_BASE_STR "marvell,gpu-mem-base"
#define MEM_SIZE_STR "marvell,gpu-mem-size"
#else
#define MEM_BASE_STR "gpu-mem-base"
#define MEM_SIZE_STR "gpu-mem-size"
#endif

# if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
static int parse_freq_table(const __be32 *prop, unsigned int proplen, struct freq_table *FreqTable)

{
    unsigned size;
    int i = 0;
    struct freq_table *ft = FreqTable;

    size = proplen / sizeof(u32);

    if ((proplen % sizeof(u32)) || size % 2)
    {
        gcmkPRINT("mmp-clk-freq-combine has wrong value\n");
        goto out;
    }

    for (;i < size / 2; i++)
    {
        (ft+i)->index     = i;
        (ft+i)->frequency = be32_to_cpup(prop + i*2);
        (ft+i)->busfreq   = be32_to_cpup(prop + i*2 + 1);
    }
    /* end of the freq table*/
    (ft+i)->index = ~1;

    /* return freq table length*/
    return i;
out:
    /* failed to get freq table*/
    ft[i].index = ~0;

    return -1;
}
# endif

static int gpu2D_dt_probe(struct platform_device *pdev)
{
    struct device_node *np = pdev->dev.of_node;
    gctUINT32 irq = 0;
    struct resource res;
    gctINT rnt = 0;
# if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
    const __be32 *prop = gcvNULL;
    unsigned int proplen;
# endif

    irq = irq_of_parse_and_map(np, 0);
    if (!irq)
    {
        gcmkPRINT("interrupt line missing");
        return -EINVAL;
    }
    irqLine2D = irq;

    if (of_address_to_resource(np, 0, &res))
    {
        gcmkPRINT("register address missing");
        return -EINVAL;
    }
    registerMemBase2D = res.start;
    registerMemSize2D = resource_size(&res);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
    /* get 2D core & Axi Bus frequency table head*/
    prop = of_get_property(pdev->dev.of_node,
               "marvell,2d-clk-freq-combine", &proplen);
    if(!prop)
    {
        gcmkPRINT(" get 2D frequency table failed\n");
        return -EINVAL;
    }

    if(!galDevice->ft)
    {
        /* allocate memory for 3D/2D/Shader table, each table has length "FREQ_TABLE_MAX" */
        galDevice->ft = kzalloc(sizeof(struct freq_table)*gcdMAX_GPU_COUNT*FREQ_TABLE_MAX, GFP_KERNEL);
        if(!galDevice->ft)
        {
            gcmkPRINT("allocate frequency table memory failed\n");
            return -EINVAL;
        }
    }

    /* pass 2D freq table head ptr in*/
    rnt = parse_freq_table(prop, proplen, galDevice->ft + gcvCORE_2D*FREQ_TABLE_MAX);

    if(rnt > 0)
    {
        galDevice->min_freq[gcvCORE_2D] = (galDevice->ft + gcvCORE_2D*FREQ_TABLE_MAX)->frequency / 1000;
        galDevice->max_freq[gcvCORE_2D] = (galDevice->ft + gcvCORE_2D*FREQ_TABLE_MAX + rnt - 1)->frequency / 1000;
        rnt = 0;
    }
#endif

    return rnt;

}

static int gpu2D_probe(struct platform_device *pdev)
{
    if (gpu2D_dt_probe(pdev))
    {
        gcmkPRINT(KERN_ERR "%s: Failed to get GC resource in Device Tree.\n",__FUNCTION__);
        return -ENODEV;
    }

    if(!galDevice)
        return -EINVAL;

    /*save pdev for 2D core*/
    galDevice->pdev[gcvCORE_2D] = pdev;

    if(has_feat_power_domain())
    {
        /* register gc 2D to power domain*/
        pm_runtime_enable(&pdev->dev);
    }

    platform_set_drvdata(pdev, galDevice);

    return 0;
}

static int gpu2D_remove(struct platform_device *pdev)
{
    if(has_feat_power_domain())
    {
        /* unregister 2D to power domain*/
        pm_runtime_disable(&pdev->dev);
    }

    return 0;
}

static int gpu_dt_probe(struct platform_device *pdev)
{
    struct device_node *np = pdev->dev.of_node;
    gctUINT32 memBase = 0;
    gctUINT32 memSize = 0;
    gctUINT32 irq = 0;
    struct resource res;
    gctINT rnt = 0;
# if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
    const __be32 *prop = gcvNULL;
    unsigned int proplen;
# endif

    irq = irq_of_parse_and_map(np, 0);
    if (!irq)
    {
        gcmkPRINT("interrupt line missing");
        return -EINVAL;
    }
#if gcdMULTI_GPU || gcdMULTI_GPU_AFFINITY
    irqLine3D0 = irq;
#else
    irqLine = irq;
#endif

    if (of_address_to_resource(np, 0, &res))
    {
        gcmkPRINT("register address missing");
        return -EINVAL;
    }
#if gcdMULTI_GPU || gcdMULTI_GPU_AFFINITY
    registerMemBase3D0 = res.start;
    registerMemSize3D0 = resource_size(&res);
#else
    registerMemBase = res.start;
    registerMemSize = resource_size(&res);
#endif

    if (of_property_read_u32(np, MEM_BASE_STR, &memBase))
    {
        gcmkPRINT("continuous memory base address missing");
        return -EINVAL;
    }
    contiguousBase = memBase;

    if (of_property_read_u32(np, MEM_SIZE_STR, &memSize))
    {
        gcmkPRINT("continuous memory size missing");
        return -EINVAL;
    }
    contiguousSize = memSize;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
    if(!galDevice->ft)
    {
        gcmkPRINT("frequency table is not allocated\n");
        return -EINVAL;
    }

    /* get 3D core/Axi Bus frequency table head*/
    prop = of_get_property(pdev->dev.of_node,
               "marvell,3d-clk-freq-combine", &proplen);
    if(!prop)
    {
        gcmkPRINT(" get 3D frequency table failed\n");
        return -EINVAL;
    }

    /* pass 3D freq table head ptr in*/
    rnt = parse_freq_table(prop, proplen, galDevice->ft + gcvCORE_MAJOR*FREQ_TABLE_MAX);

    if(rnt > 0)
    {
        galDevice->min_freq[gcvCORE_MAJOR] = (galDevice->ft + gcvCORE_MAJOR*FREQ_TABLE_MAX)->frequency / 1000;
        galDevice->max_freq[gcvCORE_MAJOR] = (galDevice->ft + gcvCORE_MAJOR*FREQ_TABLE_MAX + rnt - 1)->frequency / 1000;
        rnt = 0;
    }
    else
    {
        goto out;
    }

    /* get shader core/Axi Bus frequency table head*/
    prop = of_get_property(pdev->dev.of_node,
               "marvell,sh-clk-freq-combine", &proplen);
    if(!prop)
    {
        gcmkPRINT(" get Shader frequency table failed\n");
        return -EINVAL;
    }

    /* pass shader freq table head ptr in*/
    rnt = parse_freq_table(prop, proplen, galDevice->ft + gcvCORE_SH*FREQ_TABLE_MAX);

    if(rnt <= 0)
    {
        goto out;
    }

    if(gcdMAX_GPU_COUNT-1 >= gcvCORE_SH)
    {
        galDevice->min_freq[gcvCORE_SH] = (galDevice->ft + gcvCORE_SH*FREQ_TABLE_MAX)->frequency / 1000;
        galDevice->max_freq[gcvCORE_SH] = (galDevice->ft + gcvCORE_SH*FREQ_TABLE_MAX + rnt - 1)->frequency / 1000;
        rnt = 0;
    }
out:
#endif

    return rnt;
}
#endif

static int gpu_pm_notify(struct notifier_block *nb,
                         unsigned long event,
                         void *dummy)
{
    struct gpu_pm_context_info *pm_ctx = &galDevice->pm_ctx;

    if(event == PM_SUSPEND_PREPARE)
    {
        pr_info("%s: notify gpu before suspend\n", __func__);

#if gcdPOWEROFF_TIMEOUT
        {
            int gpu = 0;

            for (gpu = 0; gpu < gcdMAX_GPU_COUNT; gpu++)
            {
                gckKERNEL kernel = galDevice->kernels[gpu];

                if(kernel != gcvNULL)
                {
                    /* disable schedule new timer to power off GPU */
                    kernel->hardware->enablePowerOffTimeout = gcvFALSE;

                    /* cancel pending power-off timer and wait for it to finish */
                    gcmkVERIFY_OK(gckOS_StopTimerSync(
                                        kernel->os,
                                        kernel->hardware->powerOffTimer
                                        ));
                }
            }
        }
#endif

        mutex_lock(&pm_ctx->lock);
    }
    else if(event == PM_POST_SUSPEND)
    {
        pr_info("%s: notify gpu after resume\n", __func__);

#if gcdPOWEROFF_TIMEOUT
        {
            int gpu = 0;

            for (gpu = 0; gpu < gcdMAX_GPU_COUNT; gpu++)
            {
                gckKERNEL kernel = galDevice->kernels[gpu];

                if(kernel != gcvNULL)
                {
                    /* re-enable power off GPU timer scheduling */
                    kernel->hardware->enablePowerOffTimeout = gcvTRUE;
                }
            }
        }
#endif

        mutex_unlock(&pm_ctx->lock);
    }

    return NOTIFY_OK;
}

static struct notifier_block gpu_pm_notifier = {
    .notifier_call = gpu_pm_notify,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
static int gpu_probe(struct platform_device *pdev)
#else
static int __devinit gpu_probe(struct platform_device *pdev)
#endif
{
    int ret = -ENODEV;
    gcsMODULE_PARAMETERS moduleParam = {
#if gcdMULTI_GPU || gcdMULTI_GPU_AFFINITY
#else
        .irqLine            = irqLine,
        .registerMemBase    = registerMemBase,
        .registerMemSize    = registerMemSize,
#endif
        .irqLine2D          = irqLine2D,
        .registerMemBase2D  = registerMemBase2D,
        .registerMemSize2D  = registerMemSize2D,
        .irqLineVG          = irqLineVG,
        .registerMemBaseVG  = registerMemBaseVG,
        .registerMemSizeVG  = registerMemSizeVG,
        .contiguousSize     = contiguousSize,
        .contiguousBase     = contiguousBase,
        .bankSize           = bankSize,
        .fastClear          = fastClear,
        .compression        = compression,
        .powerManagement    = powerManagement,
        .gpuProfiler        = gpuProfiler,
        .signal             = signal,
        .baseAddress        = baseAddress,
        .physSize           = physSize,
        .logFileSize        = logFileSize,
        .recovery           = recovery,
        .stuckDump          = stuckDump,
        .showArgs           = showArgs,
        .gpu3DMinClock      = gpu3DMinClock,
        .registerMemMapped    = registerMemMapped,
    };

    gcmkHEADER();

    platform.device = pdev;

    if (platform.ops->getPower)
    {
        if (gcmIS_ERROR(platform.ops->getPower(&platform)))
        {
            gcmkFOOTER_NO();
            return ret;
        }
    }

    if (platform.ops->adjustParam)
    {
        /* Override default module param. */
        platform.ops->adjustParam(&platform, &moduleParam);

        /* Update module param because drv_init() uses them directly. */
        _UpdateModuleParam(&moduleParam);
    }

#if MRVL_GPU_RESOURCE_DT
    gcmkPRINT(KERN_INFO "[galcore] info: GC use DT to reserve video memory.\n");
    if (gpu_dt_probe(pdev))
    {
        gcmkPRINT(KERN_ERR "%s: Failed to get GC resource in Device Tree.\n",__FUNCTION__);
        gcmkFOOTER_NO();
        return ret;
    }

    if(!galDevice)
        return -EINVAL;

    /*save pdev ptr for 3D core*/
    galDevice->pdev[gcvCORE_MAJOR] = pdev;

    if(has_feat_power_domain())
    {
        /* register 3D core to power domain*/
        pm_runtime_enable(&pdev->dev);
    }

    /*
       register to pm notifier chain only for 3D,
       due to 2D have not registered suspend callback
   */
    register_pm_notifier(&gpu_pm_notifier);

#endif

    printk("\n[galcore] GC Version: %s\n", _GC_VERSION_STRING_);
    printk("\ncontiguousBase: 0x%08x, contiguousSize: 0x%08x\n",
        (gctUINT32)contiguousBase, (gctUINT32)contiguousSize);

    ret = drv_init();

    if (!ret)
    {
        platform_set_drvdata(pdev, galDevice);

#if MRVL_CONFIG_PROC
        create_gc_proc_file();
#endif

        create_gc_sysfs_file(pdev);

#if MRVL_CONFIG_ENABLE_GPUFREQ
        __enable_gpufreq(galDevice);
#endif

        gcmkFOOTER_NO();
        return ret;
    }

    gcmkFOOTER_ARG(KERN_INFO "Failed to register gpu driver: %d\n", ret);
    return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
static int gpu_remove(struct platform_device *pdev)
#else
static int __devexit gpu_remove(struct platform_device *pdev)
#endif
{
    gcmkHEADER();

#if MRVL_CONFIG_ENABLE_GPUFREQ
        __disable_gpufreq(galDevice);
#endif

    remove_gc_sysfs_file(pdev);

    drv_exit();

    if (platform.ops->putPower)
    {
        platform.ops->putPower(&platform);
    }

#if MRVL_CONFIG_PROC
    remove_gc_proc_file();
#endif

    /* unregister from pm notifier chain */
    unregister_pm_notifier(&gpu_pm_notifier);

    if(has_feat_power_domain())
    {
        /* unregister 3D to power domain*/
        pm_runtime_disable(&pdev->dev);
    }

    gcmkFOOTER_NO();
    return 0;
}

static int _suspend(struct platform_device *dev, pm_message_t state, gceCORE core)
{
    gceSTATUS status;
    gckGALDEVICE device;

    if(core < 0 && core >= gcdMAX_GPU_COUNT)
        return -1;

    device = platform_get_drvdata(dev);

    if (!device)
    {
        return -1;
    }

    status = gckHARDWARE_SetPowerManagementState(device->kernels[core]->hardware,
                                                (has_feat_power_domain()
                                                 ? gcvPOWER_OFF_HINT
                                                 : gcvPOWER_OFF)
                                                );
    if (gcmIS_ERROR(status))
    {
        return -1;
    }

    return 0;
}

static int _resume(struct platform_device *dev, gceCORE core)
{
    gceSTATUS status;
    gckGALDEVICE device;

    if(core < 0 && core >= gcdMAX_GPU_COUNT)
        return -1;

    device = platform_get_drvdata(dev);

    if (!device)
    {
        return -1;
    }

    status = gckHARDWARE_SetPowerManagementState(device->kernels[core]->hardware,
                                                (has_feat_power_domain()
                                                 ? gcvPOWER_ON_HINT
                                                 : gcvPOWER_ON)
                                                );

    if (gcmIS_ERROR(status))
    {
        return -1;
    }

    return 0;
}

#if defined(CONFIG_PM) && LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
#ifdef CONFIG_PM_SLEEP
static int gpu_system_suspend(struct device *dev)
{
    pm_message_t state={0};
    printk("galcore: gpu3d suspend ...\n");
    return _suspend(to_platform_device(dev), state, gcvCORE_MAJOR);
}

static int gpu_system_resume(struct device *dev)
{
    printk("galcore: gpu3d resume ...\n");
    return _resume(to_platform_device(dev), gcvCORE_MAJOR);
}
#endif

static const struct dev_pm_ops gpu_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(gpu_system_suspend, gpu_system_resume)
};

#ifdef CONFIG_PM_SLEEP
static int gpu2d_system_suspend(struct device *dev)
{
    pm_message_t state={0};
    printk("galcore: gpu2d suspend ...\n");
    return _suspend(to_platform_device(dev), state, gcvCORE_2D);
}

static int gpu2d_system_resume(struct device *dev)
{
    printk("galcore: gpu2d resume ...\n");
    return _resume(to_platform_device(dev), gcvCORE_2D);
}
#endif

static const struct dev_pm_ops gpu2d_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(gpu2d_system_suspend, gpu2d_system_resume)
};
#endif

#if MRVL_GPU_RESOURCE_DT
static struct of_device_id gpu_dt_ids[] = {
    { .compatible = "marvell,gpu", },
    {}
};

static struct of_device_id gpu2D_dt_ids[] = {
    { .compatible = "marvell,gpu2d", },
    {}
};

static struct platform_driver gpu2D_driver = {
    .probe      = gpu2D_probe,
    .remove     = gpu2D_remove,

    .driver     = {
        .name   = "galcore2D",
#if defined(CONFIG_PM) && LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
        .pm     = &gpu2d_pm_ops,
#endif
        .of_match_table = of_match_ptr(gpu2D_dt_ids),
    }
};
#endif

static struct platform_driver gpu_driver = {
    .probe      = gpu_probe,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
    .remove     = gpu_remove,
#else
    .remove     = __devexit_p(gpu_remove),
#endif

    .driver     = {
        .owner = THIS_MODULE,
        .name   = DEVICE_NAME,
#if defined(CONFIG_PM) && LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
        .pm     = &gpu_pm_ops,
#endif
#if MRVL_GPU_RESOURCE_DT
        .of_match_table = of_match_ptr(gpu_dt_ids),
#endif
    }
};

static int __init gpu_init(void)
{
    int ret = 0;

    memset(&platform, 0, sizeof(gcsPLATFORM));

    gckPLATFORM_QueryOperations(&platform.ops);

    if (platform.ops == gcvNULL)
    {
        printk(KERN_ERR "galcore: No platform specific operations.\n");
        ret = -ENODEV;
        goto out;
    }

    if (platform.ops->allocPriv)
    {
        /* Allocate platform private data. */
        if (gcmIS_ERROR(platform.ops->allocPriv(&platform)))
        {
            ret = -ENOMEM;
            goto out;
        }
    }

    if (platform.ops->needAddDevice
     && platform.ops->needAddDevice(&platform))
    {
        /* Allocate device */
        platform.device = platform_device_alloc(DEVICE_NAME, -1);
        if (!platform.device)
        {
            printk(KERN_ERR "galcore: platform_device_alloc failed.\n");
            ret = -ENOMEM;
            goto out;
        }

        /* Add device */
        ret = platform_device_add(platform.device);
        if (ret)
        {
            printk(KERN_ERR "galcore: platform_device_add failed.\n");
            goto put_dev;
        }
    }

    platform.driver = &gpu_driver;

    if (platform.ops->adjustDriver)
    {
        /* Override default platform_driver struct. */
        platform.ops->adjustDriver(&platform);
    }

    /* Allocate device structure. */
    galDevice = kzalloc(sizeof(struct _gckGALDEVICE), GFP_KERNEL | __GFP_NOWARN);

    if(!galDevice)
    {
        printk(KERN_ERR "galcore: failed to allocate galDevice\n");
        goto out;
    }

#if MRVL_GPU_RESOURCE_DT
    /* register 2D driver before 3D, we need to get 2D resource first, and init in 3D probe. */
    ret = platform_driver_register(&gpu2D_driver);
    if (ret)
    {
        goto out;
    }
#endif

    ret = platform_driver_register(&gpu_driver);
    if (!ret)
    {
        goto out;
    }

    platform_device_del(platform.device);
put_dev:
    platform_device_put(platform.device);

out:
    return ret;
}

static void __exit gpu_exit(void)
{
    platform_driver_unregister(&gpu_driver);
#if MRVL_GPU_RESOURCE_DT
    platform_driver_unregister(&gpu2D_driver);
#endif

    if (platform.ops->needAddDevice
     && platform.ops->needAddDevice(&platform))
    {
        platform_device_unregister(platform.device);
    }

    if (platform.priv)
    {
        /* Free platform private data. */
        platform.ops->freePriv(&platform);
    }

    if(galDevice)
    {
        kfree(galDevice);
        galDevice = gcvNULL;
    }
}

module_init(gpu_init);
module_exit(gpu_exit);

#endif
