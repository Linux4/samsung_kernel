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




#include <linux/device.h>
#include <linux/slab.h>

#include "gc_hal_kernel_linux.h"
#include "gc_hal_driver.h"
#include "gc_hal_kernel_sysfs.h"

#if MRVL_CONFIG_ENABLE_GPUFREQ
#include "gpufreq.h"
#endif

#define USE_PLATFORM_DRIVER         1

#if USE_PLATFORM_DRIVER
#   include <linux/platform_device.h>
#endif

#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
#include "ion.h"
#include "pxa/pxa_ion.h"
#   else
#include <linux/ion.h>
#include <linux/pxa_ion.h>
#   endif
#endif

#ifdef CONFIG_PXA_DVFM
#   include <mach/dvfm.h>
#   include <mach/pxa3xx_dvfm.h>
#endif

/* Zone used for header/footer. */
#define _GC_OBJ_ZONE    gcvZONE_DRIVER

MODULE_DESCRIPTION("Vivante Graphics Driver");
MODULE_LICENSE("GPL");

static struct class* gpuClass;

static gckGALDEVICE galDevice;
static struct device *pdevice = gcvNULL;

static uint major = 199;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init major_setup(char *str)
{
    uint n;
    if(!get_option(&str, &n))
        return 0;
    major = n;
    return 1;
}
__setup("major =", major_setup);
#else
module_param(major, uint, 0644);
#endif

static int irqLine = -1;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init irqLine_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    irqLine = n;
    return 1;
}
__setup("irqLine=", irqLine_setup);
#else
module_param(irqLine, int, 0644);
#endif

static ulong registerMemBase = 0x0;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init registerMemBase_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    registerMemBase = n;
    return 1;
}
__setup("registerMemBase=", registerMemBase_setup);
#else
module_param(registerMemBase, ulong, 0644);
#endif

static ulong registerMemSize = 8/*256*/ << 10;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init registerMemSize_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    registerMemSize = n;
    return 1;
}
__setup("registerMemSize=", registerMemSize_setup);
#else
module_param(registerMemSize, ulong, 0644);
#endif

static int irqLine2D = -1;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init irqLine2D_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    irqLine2D = n;
    return 1;
}
__setup("irqLine2D =", irqLine2D_setup);
#else
module_param(irqLine2D, int, 0644);
#endif

static ulong registerMemBase2D = 0x00000000;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init registerMemBase2D_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    registerMemBase2D = n;
    return 1;
}
__setup("registerMemBase2D =", registerMemBase2D_setup);
#else
module_param(registerMemBase2D, ulong, 0644);
#endif

static ulong registerMemSize2D = 8/*256*/ << 10;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init registerMemSize2D_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    registerMemSize2D = n;
    return 1;
}
__setup("registerMemSize2D=", registerMemSize2D_setup);
#else
module_param(registerMemSize2D, ulong, 0644);
#endif

static int irqLineVG = -1;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init irqLineVG_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    irqLineVG = n;
    return 1;
}
__setup("irqLineVG =", irqLineVG_setup);
#else
module_param(irqLineVG, int, 0644);
#endif

static ulong registerMemBaseVG = 0x00000000;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init registerMemBaseVG_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    registerMemBaseVG = n;
    return 1;
}
__setup("registerMemBaseVG=", registerMemBaseVG_setup);
#else
module_param(registerMemBaseVG, ulong, 0644);
#endif

static ulong registerMemSizeVG = 2 << 10;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init registerMemSizeVG_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    registerMemSizeVG = n;
    return 1;
}
__setup("registerMemSizeVG=", registerMemSizeVG_setup);
#else
module_param(registerMemSizeVG, ulong, 0644);
#endif

static ulong contiguousSize = 4 << 20;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init contiguousSize_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    contiguousSize = n;
    return 1;
}
__setup("contiguousSize =", contiguousSize_setup);
#else
module_param(contiguousSize, ulong, 0644);
#endif

static ulong contiguousBase = 0;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init contiguousBase_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    contiguousBase = n;
    return 1;
}
__setup("contiguousBase=", contiguousBase_setup);
#else
module_param(contiguousBase, ulong, 0644);
#endif

#if (MRVL_VIDEO_MEMORY_USE_TYPE != gcdMEM_TYPE_NONE)
static long pmemSize = 32 << 20;

#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init pmemSize_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    pmemSize = n;
    return 1;
}
__setup("pmemSize =", pmemSize_setup);
#else
module_param(pmemSize, long, 0644);
#endif

#endif

static long bankSize = 32 << 20;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init bankSize_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    bankSize = n;
    return 1;
}
__setup("bankSize =", bankSize_setup);
#else
module_param(bankSize, ulong, 0644);
#endif

static int fastClear = -1;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init fastClear_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    fastClear = n;
    return 1;
}
__setup("fastClear=", fastClear_setup);
#else
module_param(fastClear, int, 0644);
#endif

static int compression = -1;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init compression_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    compression = n;
    return 1;
}
__setup("compression =", compression_setup);
#else
module_param(compression, int, 0644);
#endif

static int powerManagement = 1;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init powerManagement_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    powerManagement = n;
    return 1;
}
__setup("powerManagement =", powerManagement_setup);
#else
module_param(powerManagement, int, 0644);
#endif

static int signal = 48;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init signal_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    signal = n;
    return 1;
}
__setup("signal =", signal_setup);
#else
module_param(signal, int, 0644);
#endif

static ulong baseAddress = 0;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init baseAddress_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    baseAddress = n;
    return 1;
}
__setup("baseAddress =", baseAddress_setup);
#else
module_param(baseAddress, ulong, 0644);
#endif

static ulong mmuBaseAddress = 0;

#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init mmuBaseAddress_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    mmuBaseAddress = n;
    return 1;
}
__setup("mmuBaseAddress=", mmuBaseAddress_setup);
#else
module_param(mmuBaseAddress, ulong, 0644);
#endif

static ulong physSize = 0x40000000;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init physSize_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    physSize = n;
    return 1;
}
__setup("physSize=", physSize_setup);
#else
module_param(physSize, ulong, 0644);
#endif

static uint logFileSize=0;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init logFileSize_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    logFileSize = n;
    return 1;
}
__setup("logFileSize =", logFileSize_setup);
#else
module_param(logFileSize,uint, 0644);
#endif

static int showArgs = 1;
#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init showArgs_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    showArgs = n;
    return 1;
}
__setup("showArgs=", showArgs_setup);
#else
module_param(showArgs, int, 0644);
#endif

#if ENABLE_GPU_CLOCK_BY_DRIVER
unsigned long coreClock = 416;

#if MRVL_CONFIG_PASS_FROM_UBOOT
static int __init coreClock_setup(char *str)
{
    int n;
    if(!get_option(&str, &n))
        return 0;
    coreClock = n;
    return 1;
}
__setup("coreClock =", coreClock_setup);
#else
module_param(coreClock, ulong, 0644);
#endif
#endif

/******************************************************************************\
* Create a data entry system using proc for GC
\******************************************************************************/
#define MRVL_CONFIG_PROC    1

#if MRVL_CONFIG_PROC
#include <linux/proc_fs.h>

#define GC_PROC_FILE        "driver/gc"
#define _GC_OBJ_ZONE        gcvZONE_DRIVER

static struct proc_dir_entry * gc_proc_file;

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

static gceSTATUS _gc_gather_infomation(char *buf, ssize_t* length)
{
    gceSTATUS status;
    ssize_t len = 0;
    gctUINT32 pid = 0;

    /* #################### [START ==DO NOT CHANGE THE FIRST LINE== START] #################### */
    /* @Ziyi: This string is checked by skia-neon related code to identify Marvell silicon,
              please do not change it and always keep it at the first line of /proc/driver/gc ! */
    gckOS_GetProcessID(&pid);
    len += sprintf(buf+len, "[%3d]%s(%s)\n", pid, _VENDOR_STRING_, _GC_VERSION_STRING_);
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

#if MRVL_DUMP_GC_INFO
extern void
_DumpMutexInfo(
    gckOS os);
#endif

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
    if(strncmp(messages, "dutycycle", 9) == 0)
    {
        gctINT32            option;
        gcuDATABASE_INFO    gpuIdle;
        static gctUINT64    startTime = 0;
        static gctUINT64    endTime = 0;

        gcmkVERIFY_OK(gckOS_ZeroMemory((gctPOINTER)&gpuIdle, gcmSIZEOF(gcuDATABASE_INFO)));

        sscanf(messages+9, "%d", &option);

        switch(option) {
        case 0:
            gcmkVERIFY_OK(gckOS_GetProfileTick(&startTime));
            gcmkVERIFY_OK(gckKERNEL_QueryDutyCycleDB(galDevice->kernels[gcvCORE_MAJOR], gcvTRUE, &gpuIdle));
            gpuIdle.time = 0;
            endTime = 0;
            break;
        case 1:
            gcmkVERIFY_OK(gckKERNEL_QueryDutyCycleDB(galDevice->kernels[gcvCORE_MAJOR], gcvFALSE, &gpuIdle));
            gcmkVERIFY_OK(gckOS_GetProfileTick(&endTime));
            break;
        default:
            printk(KERN_INFO "usage: echo dutycycle [0|1] > /proc/driver/gc\n");
        }

        if(startTime != 0 && endTime != 0)
        {
            gctUINT64   delta = endTime - startTime;
            gctUINT32   per   = 100 - 100 * gckOS_ProfileToMS(gpuIdle.time) / gckOS_ProfileToMS(delta);
            printk(KERN_INFO "\n  %%GPU     START       END     DELTA      IDLE\n");
            printk(KERN_INFO "%5u%%  %8u  %8u  %8u  %8u\n\n",
                per,
                gckOS_ProfileToMS(startTime),
                gckOS_ProfileToMS(endTime),
                gckOS_ProfileToMS(delta),
                gckOS_ProfileToMS(gpuIdle.time));
        }
    }
#if MRVL_DUMP_GC_INFO
    else if(strncmp(messages, "dumpinfo", 8) == 0)
    {
        _DumpMutexInfo(galDevice->os);
    }
#endif
    else if(strncmp(messages, "reg", 3) == 0)
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

    if(!gc_proc_file)
#else
    gc_proc_file = create_proc_entry(GC_PROC_FILE, 0644, gcvNULL);
    if (gc_proc_file) {
        gc_proc_file->proc_fops = &gc_proc_ops;
    } else
#endif
    {
        gcmkPRINT("[galcore] proc file create failed!\n");
    }
}

static void remove_gc_proc_file(void)
{
    if(gc_proc_file)
        remove_proc_entry(GC_PROC_FILE, gcvNULL);
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
    .mmap       = drv_mmap,
};

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
        gcmkONERROR(gckOS_MapMemory(
            galDevice->os,
            galDevice->contiguousPhysical,
            galDevice->contiguousSize,
            &data->contiguousLogical
            ));

        for (i = 0; i < gcdMAX_GPU_COUNT; i++)
        {
            if (galDevice->kernels[i] != gcvNULL)
            {
                gcmkVERIFY_OK(gckKERNEL_AddProcessDB(
                    galDevice->kernels[i],
                    data->pidOpen,
                    gcvDB_MAP_MEMORY,
                    data->contiguousLogical,
                    galDevice->contiguousPhysical,
                    galDevice->contiguousSize));
            }
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

            for(i = 0; i < gcdMAX_GPU_COUNT; i++)
            {
                gcmkVERIFY_OK(
                         gckKERNEL_RemoveProcessDB(galDevice->kernels[i],
                                                   data->pidOpen,
                                                   gcvDB_MAP_MEMORY,
                                                   data->contiguousLogical));
            }
        }

        kfree(data);
        data = gcvNULL;
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
    gcsHAL_PRIVATE_DATA_PTR data = gcvNULL;
    gckGALDEVICE device;
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

            for (i = 0; i < gcdMAX_GPU_COUNT; i++)
            {
                if (galDevice->kernels[i] != gcvNULL)
                {
                    gcmkVERIFY_OK(
                         gckKERNEL_RemoveProcessDB(galDevice->kernels[i],
                                                   data->pidOpen, gcvDB_MAP_MEMORY,
                                                   data->contiguousLogical));
                }
            }

            data->contiguousLogical = gcvNULL;
        }
    }

    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (galDevice->kernels[i] != gcvNULL)
        {
            gcmkONERROR(gckCOMMAND_Stall(galDevice->kernels[i]->command, gcvFALSE));
        }
    }

    /* A process gets detached. */
    gcmkPRINT("Process %d released (%s)\n", data->pidOpen, _GC_VERSION_STRING_);
    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (galDevice->kernels[i] != gcvNULL)
        {
            /*gcmkONERROR(gckKERNEL_ShowProcessMemUsage(galDevice->kernels[i], data->pidOpen));*/
            gcmkONERROR(gckKERNEL_AttachProcessEx(galDevice->kernels[i], gcvFALSE, data->pidOpen));
        }
    }

    kfree(data);
    filp->private_data = gcvNULL;

    /* Print memory information
    _gc_gather_infomation(buf, &len);
    printk("%s", buf);*/

    /* Success. */
    gcmkFOOTER_NO();
    return 0;

OnError:
    if(data!=gcvNULL)
    {
        kfree(data);
        filp->private_data = gcvNULL;
    }
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
    gcsHAL_PRIVATE_DATA_PTR data = gcvNULL;
    gctINT32 i, count;

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
            "%s(%d): input or/and output structures are invalid.\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    copyLen = copy_from_user(
        &iface, drvArgs.InputBuffer, sizeof(gcsHAL_INTERFACE)
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
        if (iface.hardwareType < 0 || iface.hardwareType > 7)
        {
            gcmkTRACE_ZONE(
                gcvLEVEL_ERROR, gcvZONE_DRIVER,
                "%s(%d): unknown hardwareType %d\n",
                __FUNCTION__, __LINE__,
                iface.hardwareType
                );

            gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }

        /* print commands for debugging. */
        if( galDevice->printPID && iface.hardwareType &&
            ((galDevice->printPID & iface.hardwareType)!= 0) )
        {
            gctUINT32 procID;
            char procName[gcdPROC_NAME_LEN];

            gcmkVERIFY_OK(gckOS_GetProcessID(&procID));
            gcmkVERIFY_OK(gckOS_GetProcessName(&procName[0]));

            gcmkPRINT("[%4s] PROC[%5d](%16s) CMD[%d]\n",
                (iface.hardwareType == gcvHARDWARE_3D) ? "3D" :
                ((iface.hardwareType == gcvHARDWARE_2D) ? "2D  " :
                ((iface.hardwareType == gcvHARDWARE_VG) ? "VG" : "2D3D")),
                procID, procName, iface.command);
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
        /* Special case for mapped memory. */
        if ((data->mappedMemory != gcvNULL)
        &&  (iface.u.LockVideoMemory.node->VidMem.memory->object.type == gcvOBJ_VIDMEM)
        )
        {
            /* Compute offset into mapped memory. */
            gctUINT32 offset
                = (gctUINT8 *) iface.u.LockVideoMemory.memory
                - (gctUINT8 *) device->contiguousBase;

            /* Compute offset into user-mapped region. */
            iface.u.LockVideoMemory.memory =
                (gctUINT8 *) data->mappedMemory + offset;
        }
    }

    /* Copy data back to the user. */
    copyLen = copy_to_user(
        drvArgs.OutputBuffer, &iface, sizeof(gcsHAL_INTERFACE)
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
    if(data != gcvNULL)
    {
        kfree(data);
        filp->private_data = gcvNULL;
    }
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
    gckGALDEVICE device = gcvNULL;
    struct class* device_class = gcvNULL;

    gcmkHEADER();


#if ENABLE_GPU_CLOCK_BY_DRIVER && (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28))
    /* All platforms: enable clock and power of MAJOR core */
    gcmkONERROR(gckOS_GpuPowerEnable(gcvNULL, gcvCORE_MAJOR, gcvTRUE, gcvTRUE, coreClock*1000*1000));

    if(has_feat_separated_gc_clock())
    {
        if(has_feat_separated_gc_power())
            /* PXA1L88: enable 2D power, too */
            gcmkONERROR(gckOS_GpuPowerEnable(gcvNULL, gcvCORE_2D, gcvTRUE, gcvTRUE, coreClock*1000*1000));
        else
            /* PXA1088: 2D power is shared with 3D */
            gcmkONERROR(gckOS_GpuPowerEnable(gcvNULL, gcvCORE_2D, gcvTRUE, gcvFALSE, coreClock*1000*1000));
    }
#endif

    printk(KERN_INFO "Galcore version %d.%d.%d.%d\n",
        gcvVERSION_MAJOR, gcvVERSION_MINOR, gcvVERSION_PATCH, gcvVERSION_BUILD);

    if (showArgs)
    {
        printk("galcore options:\n");
        printk("  irqLine           = %d\n",      irqLine);
        printk("  registerMemBase   = 0x%08lX\n", registerMemBase);
        printk("  registerMemSize   = 0x%08lX\n", registerMemSize);

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
#if (MRVL_VIDEO_MEMORY_USE_TYPE != gcdMEM_TYPE_NONE)
        printk("  pmemSize          = %ld\n",     pmemSize);
#endif
        printk("  fastClear         = %d\n",      fastClear);
        printk("  compression       = %d\n",      compression);
        printk("  signal            = %d\n",      signal);
        printk("  baseAddress       = 0x%08lX\n", baseAddress);
        printk("  physSize          = 0x%08lX\n", physSize);
        printk("  logFileSize       = %d KB \n",  logFileSize);
        printk("  powerManagement   = %d\n",      powerManagement);
#if ENABLE_GPU_CLOCK_BY_DRIVER
        printk("  coreClock       = %lu\n",     coreClock);
#endif
    }
    physSize = 0x80000000;
    printk("  physSize modified to          = 0x%08lX\n", physSize);
    if(logFileSize != 0)
    {
        gckDebugFileSystemInitialize();
    }

    /* Create the GAL device. */
    gcmkONERROR(gckGALDEVICE_Construct(
        irqLine,
        registerMemBase, registerMemSize,
        irqLine2D,
        registerMemBase2D, registerMemSize2D,
        irqLineVG,
        registerMemBaseVG, registerMemSizeVG,
        contiguousBase, contiguousSize,
#if (MRVL_VIDEO_MEMORY_USE_TYPE != gcdMEM_TYPE_NONE)
        pmemSize,
#endif
        bankSize, fastClear, compression, baseAddress, physSize, signal,
        logFileSize,
        powerManagement,
        &device
        ));

    /* Start the GAL device. */
    gcmkONERROR(gckGALDEVICE_Start(device));

    if ((physSize != 0)
       && (device->kernels[gcvCORE_MAJOR] != gcvNULL)
       && (device->kernels[gcvCORE_MAJOR]->hardware->mmuVersion != 0))
    {
        status = gckMMU_Enable(device->kernels[gcvCORE_MAJOR]->mmu, mmuBaseAddress, physSize);
        gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_DRIVER,
            "Enable new MMU: status=%d\n", status);

        if ((device->kernels[gcvCORE_2D] != gcvNULL)
            && (device->kernels[gcvCORE_2D]->hardware->mmuVersion != 0))
        {
            status = gckMMU_Enable(device->kernels[gcvCORE_2D]->mmu, mmuBaseAddress, physSize);
            gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_DRIVER,
                "Enable new MMU for 2D: status=%d\n", status);
        }

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
    ret = register_chrdev(major, DRV_NAME, &driver_fops);

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
    device_class = class_create(THIS_MODULE, "graphics_class");

    if (IS_ERR(device_class))
    {
        gcmkTRACE_ZONE(
            gcvLEVEL_ERROR, gcvZONE_DRIVER,
            "%s(%d): Failed to create the class.\n",
            __FUNCTION__, __LINE__
            );

        gcmkONERROR(gcvSTATUS_OUT_OF_RESOURCES);
    }

    if (pdevice == gcvNULL)
        gcmkONERROR(gcvSTATUS_DEVICE);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
    device_create(device_class, pdevice, MKDEV(major, 0), NULL, "galcore");
#else
    device_create(device_class, pdevice, MKDEV(major, 0), "galcore");
#endif

    galDevice = device;
    gpuClass  = device_class;

    gcmkTRACE_ZONE(
        gcvLEVEL_INFO, gcvZONE_DRIVER,
        "%s(%d): irqLine=%d, contiguousSize=%lu, memBase=0x%lX\n",
        __FUNCTION__, __LINE__,
        irqLine, contiguousSize, registerMemBase
        );

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

    unregister_chrdev(major, DRV_NAME);

    gcmkVERIFY_OK(gckGALDEVICE_Stop(galDevice));
    gcmkVERIFY_OK(gckGALDEVICE_Destroy(galDevice));

    if(gckDebugFileSystemIsEnabled())
    {
     gckDebugFileSystemTerminate();
    }

#if ENABLE_GPU_CLOCK_BY_DRIVER && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
    if(has_feat_separated_gc_clock())
    {
        if(has_feat_separated_gc_power())
            gcmkVERIFY_OK(gckOS_GpuPowerDisable(galDevice->os, gcvCORE_2D, gcvTRUE, gcvTRUE));
        else
            gcmkVERIFY_OK(gckOS_GpuPowerDisable(galDevice->os, gcvCORE_2D, gcvTRUE, gcvFALSE));
    }

    gcmkVERIFY_OK(gckOS_GpuPowerDisable(galDevice->os, gcvCORE_MAJOR, gcvTRUE, gcvTRUE));
#endif

    gcmkFOOTER_NO();
}

#if !USE_PLATFORM_DRIVER
    module_init(drv_init);
    module_exit(drv_exit);
#else

#ifdef CONFIG_DOVE_GPU
#   define DEVICE_NAME "dove_gpu"
#else
#   define DEVICE_NAME "galcore"
#endif

#if MRVL_CONFIG_ENABLE_GPUFREQ
static int __enable_gpufreq(gckGALDEVICE device)
{
    gctUINT32 clockRate = 0;
    gceSTATUS status;
    int i;

    status = gckOS_QueryClkRate(device->os, gcvCORE_MAJOR, &clockRate);

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
        }
    }

#if MRVL_CONFIG_SHADER_CLK_CONTROL
        if(has_feat_gc_shader() &&
           device->kernels[gcvCORE_MAJOR]->hardware->devShObj.kobj)
        {
            gckOS_GPUFreqNotifierCallChain(
                            device->os,
                            GPUFREQ_GPU_EVENT_INIT,
                            (gctPOINTER) &device->kernels[gcvCORE_MAJOR]->hardware->devShObj);
        }
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
            if(has_feat_gc_shader() && i == 0)
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

    status = gckOS_QueryClkRate(device->os, gcvCORE_MAJOR, &clockRate);

    if(gcmIS_SUCCESS(status) && clockRate != 0)
    {
        gpufreq_exit(device->os);
        printk("[galcore] gpufreq exited\n");
    }

    return 0;
}
#else
static int __enable_gpufreq(gckGALDEVICE device) { return 0; }
static int __disable_gpufreq(gckGALDEVICE device) { return 0; }
#endif

#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
extern struct ion_device *pxa_ion_dev;
struct ion_client *gc_ion_client = NULL;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
static int gpu_probe(struct platform_device *pdev)
#else
static int __devinit gpu_probe(struct platform_device *pdev)
#endif
{
    int ret = -ENODEV;
    struct resource* res;

    gcmkHEADER();

    res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "gpu_irq");

    if (!res)
    {
        printk(KERN_ERR "%s: No irq line supplied.\n",__FUNCTION__);
        goto gpu_probe_fail;
    }

    irqLine = res->start;

    res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpu_base");

    if (!res)
    {
        printk(KERN_ERR "%s: No register base supplied.\n",__FUNCTION__);
        goto gpu_probe_fail;
    }

    registerMemBase = res->start;
    registerMemSize = res->end - res->start + 1;

#if MRVL_USE_GPU_RESERVE_MEM
    gcmkPRINT(KERN_INFO "[galcore] info: GC use memblock to reserve video memory.\n");
    res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpu_mem");
    if (!res)
    {
        printk(KERN_ERR "%s: No gpu reserved memory supplied. res = %p\n",__FUNCTION__, res);
        goto gpu_probe_fail;
    }

    contiguousBase = res->start;
    contiguousSize = res->end - res->start + 1;
#endif

    printk("\n[galcore] GC Version: %s\n", _GC_VERSION_STRING_);
    printk("\ncontiguousBase:%08x, contiguousSize:%08x\n", (gctUINT32)contiguousBase, (gctUINT32)contiguousSize);

    pdevice = &pdev->dev;

    ret = drv_init();

    if (!ret)
    {
        platform_set_drvdata(pdev, galDevice);

#if MRVL_CONFIG_PROC
        create_gc_proc_file();
#endif

        create_gc_sysfs_file(pdev);

        if(has_feat_gpufreq())
            __enable_gpufreq(galDevice);

#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
        gc_ion_client = ion_client_create(pxa_ion_dev, "gc ion");
#endif

#if MRVL_CONFIG_USE_PM_RUNTIME
        pm_runtime_enable(&pdev->dev);
        pm_runtime_forbid(&pdev->dev);
#endif

        /* save device pointer to GALDEVICE */
        galDevice->dev = &pdev->dev;

        gcmkFOOTER_NO();
        return ret;
    }

gpu_probe_fail:
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
#if MRVL_CONFIG_USE_PM_RUNTIME
    pm_runtime_put_sync(&pdev->dev);
    pm_runtime_disable(&pdev->dev);
#endif

    if(has_feat_gpufreq())
        __disable_gpufreq(galDevice);

    remove_gc_sysfs_file(pdev);

    drv_exit();

#if MRVL_CONFIG_PROC
    remove_gc_proc_file();
#endif

    gcmkFOOTER_NO();
    return 0;
}

static int gpu_suspend(struct platform_device *dev, pm_message_t state)
{
    gceSTATUS status;
    gckGALDEVICE device;
    gctINT i;
    gctINT ret = 0;

    device = platform_get_drvdata(dev);

    printk("[galcore] enter %s\n", __FUNCTION__);

    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (device->kernels[i] != gcvNULL)
        {
            /* Store states. */
#if gcdENABLE_VG
            if (i == gcvCORE_VG)
            {
                status = gckVGHARDWARE_QueryPowerManagementState(device->kernels[i]->vg->hardware, &device->statesStored[i]);
            }
            else
#endif
            {
                status = gckHARDWARE_QueryPowerManagementState(device->kernels[i]->hardware, &device->statesStored[i]);
            }

            if (gcmIS_ERROR(status))
            {
                ret = -1;
                goto err_out;
            }

#if gcdENABLE_VG
            if (i == gcvCORE_VG)
            {
                status = gckVGHARDWARE_SetPowerManagementState(device->kernels[i]->vg->hardware, gcvPOWER_OFF);
            }
            else
#endif
            {
                status = gckHARDWARE_SetPowerManagementState(device->kernels[i]->hardware, gcvPOWER_OFF);
            }

            if (gcmIS_ERROR(status))
            {
                ret = -1;
                goto err_out;
            }

            galDevice->currentPMode = gcvPM_SUSPEND;
        }
    }

err_out:
    printk("[galcore] exit %s, return %d\n", __FUNCTION__, ret);
    return ret;
}

static int gpu_resume(struct platform_device *dev)
{
    gceSTATUS status;
    gckGALDEVICE device;
    gctINT i;
    gctINT ret = 0;
    gceCHIPPOWERSTATE   statesStored;

    device = platform_get_drvdata(dev);

    printk("[galcore] enter %s\n", __FUNCTION__);

    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (device->kernels[i] != gcvNULL)
        {
#if gcdENABLE_VG
            if (i == gcvCORE_VG)
            {
                status = gckVGHARDWARE_SetPowerManagementState(device->kernels[i]->vg->hardware, gcvPOWER_ON);
            }
            else
#endif
            {
                status = gckHARDWARE_SetPowerManagementState(device->kernels[i]->hardware, gcvPOWER_ON);
            }

            if (gcmIS_ERROR(status))
            {
                ret = -1;
                goto err_out;
            }

            /* Convert global state to crossponding internal state. */
            switch(device->statesStored[i])
            {
            case gcvPOWER_OFF:
                statesStored = gcvPOWER_OFF_BROADCAST;
                break;
            case gcvPOWER_IDLE:
                statesStored = gcvPOWER_IDLE_BROADCAST;
                break;
            case gcvPOWER_SUSPEND:
                statesStored = gcvPOWER_SUSPEND_BROADCAST;
                break;
            case gcvPOWER_ON:
                statesStored = gcvPOWER_ON_AUTO;
                break;
            default:
                statesStored = device->statesStored[i];
                break;
            }

            /* Restore states. */
#if gcdENABLE_VG
            if (i == gcvCORE_VG)
            {
                status = gckVGHARDWARE_SetPowerManagementState(device->kernels[i]->vg->hardware, statesStored);
            }
            else
#endif
            {
                status = gckHARDWARE_SetPowerManagementState(device->kernels[i]->hardware, statesStored);
            }

            if (gcmIS_ERROR(status))
            {
                ret = -1;
                goto err_out;
            }

            galDevice->currentPMode = gcvPM_NORMAL;
        }
    }

err_out:
    printk("[galcore] exit %s, return %d\n", __FUNCTION__, ret);
    return ret;
}

#if MRVL_CONFIG_USE_PM_RUNTIME
/*
int (*runtime_suspend)(struct device *dev);
int (*runtime_resume)(struct device *dev);
int (*suspend)(struct device *dev);
int (*resume)(struct device *dev);
*/
static int gpu_runtime_suspend(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    gckGALDEVICE device = (gckGALDEVICE) platform_get_drvdata(pdev);
    gctINT ret = 0;

    if(device->pmrtDebug)
        printk("[galcore] enter %s\n", __FUNCTION__);

    device->currentPMode = gcvPM_AUTO_SUSPEND;

    if(device->is_work_inited)
        gckOS_ScheduleDelayedWork(device->os, gcvPOWER_OFF, 300);

    if(device->pmrtDebug)
        printk("[galcore] leave %s, ret %d\n", __FUNCTION__, ret);

    return ret;
}

static int gpu_runtime_resume(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    gckGALDEVICE device = (gckGALDEVICE) platform_get_drvdata(pdev);
    gctINT ret = 0;

    if(device->pmrtDebug)
        printk("[galcore] enter %s\n", __FUNCTION__);

    if(device->pmrtDebug)
        printk("[galcore] leave %s, ret %d\n", __FUNCTION__, ret);
    return ret;
}

static int gpu_rpm_suspend(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);

    gpu_suspend(pdev, PMSG_SUSPEND);

    return 0;
}

static int gpu_rpm_resume(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);

    gpu_resume(pdev);

    return 0;
}

static const struct dev_pm_ops gpu_pm_ops = {
    .suspend            = gpu_rpm_suspend,
    .resume             = gpu_rpm_resume,

    .runtime_suspend    = gpu_runtime_suspend,
    .runtime_resume     = gpu_runtime_resume,
/*
    SET_RUNTIME_PM_OPS(
        gpu_runtime_suspend,
        gpu_runtime_resume,
        NULL
    )
*/
};
#endif

static struct platform_driver gpu_driver = {
    .probe      = gpu_probe,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
    .remove     = gpu_remove,
#else
    .remove     = __devexit_p(gpu_remove),
#endif

#if !MRVL_CONFIG_USE_PM_RUNTIME
    .suspend    = gpu_suspend,
    .resume     = gpu_resume,
#endif

    .driver     = {
        .name   = DEVICE_NAME,
#if MRVL_CONFIG_USE_PM_RUNTIME
        .pm     = &gpu_pm_ops,
#endif
    }
};

#ifndef CONFIG_DOVE_GPU
#if !MRVL_USE_GPU_RESERVE_MEM
static struct resource gpu_resources[] = {
    {
        .name   = "gpu_irq",
        .flags  = IORESOURCE_IRQ,
    },
    {
        .name   = "gpu_base",
        .flags  = IORESOURCE_MEM,
    },
};

static struct platform_device * gpu_device;
#endif
#endif

static int __init gpu_init(void)
{
    int ret = 0;

#ifndef CONFIG_DOVE_GPU
#if !MRVL_USE_GPU_RESERVE_MEM
    gpu_resources[0].start = gpu_resources[0].end = irqLine;

    gpu_resources[1].start = registerMemBase;
    gpu_resources[1].end   = registerMemBase + registerMemSize - 1;

    /* Allocate device */
    gpu_device = platform_device_alloc(DEVICE_NAME, -1);
    if (!gpu_device)
    {
        printk(KERN_ERR "galcore: platform_device_alloc failed.\n");
        ret = -ENOMEM;
        goto out;
    }

    /* Insert resource */
    ret = platform_device_add_resources(gpu_device, gpu_resources, 2);
    if (ret)
    {
        printk(KERN_ERR "galcore: platform_device_add_resources failed.\n");
        goto put_dev;
    }

    /* Add device */
    ret = platform_device_add(gpu_device);
    if (ret)
    {
        printk(KERN_ERR "galcore: platform_device_add failed.\n");
        goto put_dev;
    }
#endif
#endif

    ret = platform_driver_register(&gpu_driver);
    if (!ret)
    {
        goto out;
    }

#ifndef CONFIG_DOVE_GPU
#if !MRVL_USE_GPU_RESERVE_MEM
    platform_device_del(gpu_device);
put_dev:
    platform_device_put(gpu_device);
#endif
#endif

out:
    return ret;
}

static void __exit gpu_exit(void)
{
    platform_driver_unregister(&gpu_driver);
#ifndef CONFIG_DOVE_GPU
#if !MRVL_USE_GPU_RESERVE_MEM
    platform_device_unregister(gpu_device);
#endif
#endif
}

module_init(gpu_init);
module_exit(gpu_exit);

#endif
