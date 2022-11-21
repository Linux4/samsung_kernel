/******************************************************************************\
|*                                                                            *|
|* Copyright (c) 2005 - 2012 by Vivante Corp.                                 *|
|*                                                                            *|
|* This program is free software; you can redistribute it and/or modify       *|
|* it under the terms of the GNU General Public License as published by       *|
|* the Free Software Foundation; either version 2 of the license, or          *|
|* (at your option) any later version.                                        *|
|*                                                                            *|
|* This program is distributed in the hope that it will be useful,            *|
|* but WITHOUT ANY WARRANTY; without even the implied warranty of             *|
|* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the               *|
|* GNU General Public License for more details.                               *|
|*                                                                            *|
|* You should have received a copy of the GNU General Public License          *|
|* along with this program; if not write to the Free Software                 *|
|* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                  *|
|*                                                                            *|
|*                                                                            *|
\******************************************************************************/




#ifndef __gc_hal_options_h_
#define __gc_hal_options_h_

/* pmem or ion definition. */
#define gcdMEM_TYPE_NONE                        0x0
#define gcdMEM_TYPE_PMEM                        0x1
#define gcdMEM_TYPE_ION                         0x2

#if (defined ANDROID || defined X11) && !defined(__QNXNTO__)
#include <linux/version.h>
#   if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0))
#       define MRVL_VIDEO_MEMORY_USE_TYPE       gcdMEM_TYPE_ION
#   else
#       define MRVL_VIDEO_MEMORY_USE_TYPE       gcdMEM_TYPE_PMEM
#       define MRVL_PMEM_MINOR_FLAG             1
#   endif
#   if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,39))
#       define gcdMEM_TYPE_IONAF_3_4_39      1
#   else
#       define gcdMEM_TYPE_IONAF_3_4_39      0
#   endif
#else
#define MRVL_VIDEO_MEMORY_USE_TYPE              gcdMEM_TYPE_NONE
#endif

/* macro to define wmmx support */
#ifdef SUPPORT_WMMX
#define MRVL_SUPPORT_WMMX                   1
#else
#define MRVL_SUPPORT_WMMX                   0
#endif

/* macro to define neon support */
#ifdef SUPPORT_NEON
#define MRVL_SUPPORT_NEON                   1
#else
#define MRVL_SUPPORT_NEON                   0
#endif

/*
    gcdPRINT_VERSION

        Print HAL version.
*/
#ifndef gcdPRINT_VERSION
#   define gcdPRINT_VERSION                     0
#endif

/*
    USE_NEW_LINUX_SIGNAL

        This define enables the Linux kernel signaling between kernel and user.
*/
#ifndef USE_NEW_LINUX_SIGNAL
#   define USE_NEW_LINUX_SIGNAL                 0
#endif

/*
    OPTION_ENABLE_GPUTEX

        This define enables gpu tex.
*/
#ifdef CONFIG_ENABLE_GPUTEX
#define MRVL_ENABLE_GPUTEX                 1
#else
#define MRVL_ENABLE_GPUTEX                 0
#endif


/*
    VIVANTE_PROFILER

        This define enables the profiler.
*/
#ifndef VIVANTE_PROFILER
#   define VIVANTE_PROFILER                     0
#endif

#ifndef VIVANTE_PROFILER_PERDRAW
#   define  VIVANTE_PROFILER_PERDRAW    0
#endif

/*
    gcdUSE_VG

        Enable VG HAL layer (only for GC350).
*/
#ifndef gcdUSE_VG
#   define gcdUSE_VG                            0
#endif

/*
    USE_SW_FB

        Set to 1 if the frame buffer memory cannot be accessed by the GPU.
*/
#ifndef USE_SW_FB
#   define USE_SW_FB                            0
#endif

/*
    USE_SUPER_SAMPLING

        This define enables super-sampling support.
*/
#define USE_SUPER_SAMPLING                      0

/*
    PROFILE_HAL_COUNTERS

        This define enables HAL counter profiling support.  HW and SHADER
        counter profiling depends on this.
*/
#ifndef PROFILE_HAL_COUNTERS
#   define PROFILE_HAL_COUNTERS                 1
#endif

/*
    PROFILE_HW_COUNTERS

        This define enables HW counter profiling support.
*/
#ifndef PROFILE_HW_COUNTERS
#   define PROFILE_HW_COUNTERS                  1
#endif

/*
    PROFILE_SHADER_COUNTERS

        This define enables SHADER counter profiling support.
*/
#ifndef PROFILE_SHADER_COUNTERS
#   define PROFILE_SHADER_COUNTERS              1
#endif

/*
    COMMAND_PROCESSOR_VERSION

        The version of the command buffer and task manager.
*/
#define COMMAND_PROCESSOR_VERSION               1

/*
    gcdDUMP_KEY

        Set this to a string that appears in 'cat /proc/<pid>/cmdline'. E.g. 'camera'.
        HAL will create dumps for the processes matching this key.
*/
#ifndef gcdDUMP_KEY
#   define gcdDUMP_KEY                          "process"
#endif

/*
    gcdDUMP_PATH

        The dump file location. Some processes cannot write to the sdcard.
        Try apps' data dir, e.g. /data/data/com.android.launcher
*/
#ifndef gcdDUMP_PATH
#if defined(ANDROID)
#   define gcdDUMP_PATH                         "/mnt/sdcard/"
#else
#   define gcdDUMP_PATH                         "./"
#endif
#endif

/*
    gcdDUMP

        When set to 1, a dump of all states and memory uploads, as well as other
        hardware related execution will be printed to the debug console.  This
        data can be used for playing back applications.
*/
#ifndef gcdDUMP
#   define gcdDUMP                              0
#endif

/*
    gcdDUMP_API

        When set to 1, a high level dump of the EGL and GL/VG APs's are
        captured.
*/
#ifndef gcdDUMP_API
#   define gcdDUMP_API                          0
#endif

/*
    gcdDUMP_FRAMERATE
        When set to a value other than zero, averaqe frame rate will be dumped.
        The value set is the starting frame that the average will be calculated.
        This is needed because sometimes first few frames are too slow to be included
        in the average. Frame count starts from 1.
*/
#ifndef gcdDUMP_FRAMERATE
#   define gcdDUMP_FRAMERATE                    0
#endif


/*
    gcdENABLE_FSCALE_VAL_ADJUST
        When non-zero, FSCALE_VAL when gcvPOWER_ON can be adjusted externally.
 */
#ifndef gcdENABLE_FSCALE_VAL_ADJUST
#   define gcdENABLE_FSCALE_VAL_ADJUST          1
#endif

/*
    gcdDUMP_IN_KERNEL

        When set to 1, all dumps will happen in the kernel.  This is handy if
        you want the kernel to dump its command buffers as well and the data
        needs to be in sync.
*/
#ifndef gcdDUMP_IN_KERNEL
#   define gcdDUMP_IN_KERNEL                    0
#endif

/*
    gcdDUMP_COMMAND

        When set to non-zero, the command queue will dump all incoming command
        and context buffers as well as all other modifications to the command
        queue.
*/
#ifndef gcdDUMP_COMMAND
#   define gcdDUMP_COMMAND                      0
#endif

/*
    gcdDUMP_FRAME_TGA

    When set to a value other than 0, a dump of the frame specified by the value,
    will be done into frame.tga. Frame count starts from 1.
 */
#ifndef gcdDUMP_FRAME_TGA
#define gcdDUMP_FRAME_TGA                       0
#endif
/*
    gcdNULL_DRIVER

    Set to 1 for infinite speed hardware.
    Set to 2 for bypassing the HAL.
    Set to 3 for bypassing the drivers.
*/
#ifndef gcdNULL_DRIVER
#   define gcdNULL_DRIVER                       0
#endif

/*
    gcdENABLE_TIMEOUT_DETECTION

        Enable timeout detection.
*/
#ifndef gcdENABLE_TIMEOUT_DETECTION
#   define gcdENABLE_TIMEOUT_DETECTION          0
#endif

/*
    gcdCMD_BUFFER_SIZE

        Number of bytes in a command buffer.
*/
#ifndef gcdCMD_BUFFER_SIZE
#   define gcdCMD_BUFFER_SIZE                   (128 << 10)
#endif

/*
    gcdCMD_BUFFERS

        Number of command buffers to use per client.
*/
#ifndef gcdCMD_BUFFERS
#   define gcdCMD_BUFFERS                       2
#endif

/*
    gcdMAX_CMD_BUFFERS

        Maximum number of command buffers to use per client.
*/
#ifndef gcdMAX_CMD_BUFFERS
#   define gcdMAX_CMD_BUFFERS                   8
#endif

/*
    gcdCOMMAND_QUEUES

        Number of command queues in the kernel.
*/
#ifndef gcdCOMMAND_QUEUES
#   define gcdCOMMAND_QUEUES                    2
#endif

/*
    gcdPOWER_CONTROL_DELAY

        The delay in milliseconds required to wait until the GPU has woke up
        from a suspend or power-down state.  This is system dependent because
        the bus clock also needs to stabalize.
*/
#ifndef gcdPOWER_CONTROL_DELAY
#   define gcdPOWER_CONTROL_DELAY               10
#endif

/*
    gcdMIRROR_PAGETABLE

        Enable it when GPUs with old MMU and new MMU exist at same SoC. It makes
        each GPU use same virtual address to access same physical memory.
*/
#ifndef gcdMIRROR_PAGETABLE
#   define gcdMIRROR_PAGETABLE                  0
#endif

/*
    gcdMMU_SIZE

        Size of the MMU page table in bytes.  Each 4 bytes can hold 4kB worth of
        virtual data.
*/
#ifndef gcdMMU_SIZE
#if gcdMIRROR_PAGETABLE
#   define gcdMMU_SIZE                          0x200000
#else
#   define gcdMMU_SIZE                          (1024 << 10)
#endif
#endif

/*
    gcdSECURE_USER

        Use logical addresses instead of physical addresses in user land.  In
        this case a hint table is created for both command buffers and context
        buffers, and that hint table will be used to patch up those buffers in
        the kernel when they are ready to submit.
*/
#ifndef gcdSECURE_USER
#   define gcdSECURE_USER                       0
#endif

/*
    gcdSECURE_CACHE_SLOTS

        Number of slots in the logical to DMA address cache table.  Each time a
        logical address needs to be translated into a DMA address for the GPU,
        this cache will be walked.  The replacement scheme is LRU.
*/
#ifndef gcdSECURE_CACHE_SLOTS
#   define gcdSECURE_CACHE_SLOTS                1024
#endif

/*
    gcdSECURE_CACHE_METHOD

        Replacement scheme used for Secure Cache.  The following options are
        available:

            gcdSECURE_CACHE_LRU
                A standard LRU cache.

            gcdSECURE_CACHE_LINEAR
                A linear walker with the idea that an application will always
                render the scene in a similar way, so the next entry in the
                cache should be a hit most of the time.

            gcdSECURE_CACHE_HASH
                A 256-entry hash table.

            gcdSECURE_CACHE_TABLE
                A simple cache but with potential of a lot of cache replacement.
*/
#ifndef gcdSECURE_CACHE_METHOD
#   define gcdSECURE_CACHE_METHOD               gcdSECURE_CACHE_HASH
#endif

/*
    gcdREGISTER_ACCESS_FROM_USER

        Set to 1 to allow IOCTL calls to get through from user land.  This
        should only be in debug or development drops.
*/
#ifndef gcdREGISTER_ACCESS_FROM_USER
#   define gcdREGISTER_ACCESS_FROM_USER         1
#endif

/*
    gcdHEAP_SIZE

        Set the allocation size for the internal heaps.  Each time a heap is
        full, a new heap will be allocated with this minmimum amount of bytes.
        The bigger this size, the fewer heaps there are to allocate, the better
        the performance.  However, heaps won't be freed until they are
        completely free, so there might be some more memory waste if the size is
        too big.
*/
#ifndef gcdHEAP_SIZE
#   define gcdHEAP_SIZE                         (64 << 10)
#endif

/*
    gcdPOWER_SUSPEND_WHEN_IDLE

        Set to 1 to make GPU enter gcvPOWER_SUSPEND when idle detected,
        otherwise GPU will enter gcvPOWER_IDLE.

        NOTICE: this flag not longer be applied for *MRVL* platforms
*/
#ifndef gcdPOWER_SUSPEND_WHEN_IDLE
#   define gcdPOWER_SUSPEND_WHEN_IDLE          1
#endif

/*
    gcdFPGA_BUILD

        This define enables work arounds for FPGA images.
*/
#ifndef gcdFPGA_BUILD
#   define gcdFPGA_BUILD                        0
#endif

/*
    gcdGPU_TIMEOUT

        This define specified the number of milliseconds the system will wait
        before it broadcasts the GPU is stuck.  In other words, it will define
        the timeout of any operation that needs to wait for the GPU.

        If the value is 0, no timeout will be checked for.
*/
#ifndef gcdGPU_TIMEOUT
#   if gcdFPGA_BUILD
#       define gcdGPU_TIMEOUT                   0
#   else
#       define gcdGPU_TIMEOUT                   5000
#   endif
#endif

/*
    gcdGPU_ADVANCETIMER

        it is advance timer.
*/
#ifndef gcdGPU_ADVANCETIMER
#   define gcdGPU_ADVANCETIMER                  250
#   define gcdGPU_ADVANCETIMERUSER              1000
#endif

/*
    gcdGPU_ADVANCETIMER_STALL

        it is advance timer for stall operation like 'command stall' as it should not be interrupted.
        should be different from gcdGPU_ADVANCETIMER
*/
#ifndef gcdGPU_ADVANCETIMER_STALL
#   define gcdGPU_ADVANCETIMER_STALL            200
#endif

/*
    gcdSTATIC_LINK

        This define disalbes static linking;
*/
#ifndef gcdSTATIC_LINK
#   define gcdSTATIC_LINK                       0
#endif

/*
    gcdUSE_NEW_HEAP

        Setting this define to 1 enables new heap.
*/
#ifndef gcdUSE_NEW_HEAP
#   define gcdUSE_NEW_HEAP                      0
#endif

/*
    gcdCMD_NO_2D_CONTEXT

        This define enables no-context 2D command buffer.
*/
#ifndef gcdCMD_NO_2D_CONTEXT
#   define gcdCMD_NO_2D_CONTEXT                 1
#endif

/*
    gcdENABLE_BANK_ALIGNMENT

    When enabled, video memory is allocated bank aligned. The vendor can modify
    _GetSurfaceBankAlignment() and gcoSURF_GetBankOffsetBytes() to define how
    different types of allocations are bank and channel aligned.
    When disabled (default), no bank alignment is done.
*/
#ifndef gcdENABLE_BANK_ALIGNMENT
#   define gcdENABLE_BANK_ALIGNMENT             1
#endif

/*
    gcdBANK_BIT_START

    Specifies the start bit of the bank (inclusive).
*/
#ifndef gcdBANK_BIT_START
#   define gcdBANK_BIT_START                    12
#endif

/*
    gcdBANK_BIT_END

    Specifies the end bit of the bank (inclusive).
*/
#ifndef gcdBANK_BIT_END
#   define gcdBANK_BIT_END                      14
#endif

/*
    gcdBANK_CHANNEL_BIT

    When set, video memory when allocated bank aligned is allocated such that
    render and depth buffer addresses alternate on the channel bit specified.
    This option has an effect only when gcdENABLE_BANK_ALIGNMENT is enabled.
    When disabled (default), no alteration is done.
*/
#ifndef gcdBANK_CHANNEL_BIT
#   define gcdBANK_CHANNEL_BIT                  7
#endif

/*
    gcdDYNAMIC_SPEED

        When non-zero, it informs the kernel driver to use the speed throttling
        broadcasting functions to inform the system the GPU should be spet up or
        slowed down. It will send a broadcast for slowdown each "interval"
        specified by this define in milliseconds
        (gckOS_BroadcastCalibrateSpeed).
*/
#ifndef gcdDYNAMIC_SPEED
#    define gcdDYNAMIC_SPEED                    2000
#endif

/*
    gcdDYNAMIC_EVENT_THRESHOLD

        When non-zero, it specifies the maximum number of available events at
        which the kernel driver will issue a broadcast to speed up the GPU
        (gckOS_BroadcastHurry).
*/
#ifndef gcdDYNAMIC_EVENT_THRESHOLD
#    define gcdDYNAMIC_EVENT_THRESHOLD          5
#endif

/*
    gcdENABLE_PROFILING

        Enable profiling macros.
*/
#ifndef gcdENABLE_PROFILING
#   define gcdENABLE_PROFILING                  0
#endif

#ifndef gcdENABLE_KERNEL_PROFILING
#   define gcdENABLE_KERNEL_PROFILING           0
#endif

/*
    gcdENABLE_128B_MERGE

        Enable 128B merge for the BUS control.
*/
#ifndef gcdENABLE_128B_MERGE
#   define gcdENABLE_128B_MERGE                 0
#endif

/*
    gcdFRAME_DB

        When non-zero, it specified the number of frames inside the frame
        database. The frame DB will collect per-frame timestamps and hardware
        counters.
*/
#ifndef gcdFRAME_DB
#   define gcdFRAME_DB                          0
#   define gcdFRAME_DB_RESET                    0
#   define gcdFRAME_DB_NAME                     "/var/log/frameDB.log"
#endif

/*
    gcdENABLE_VG
            enable the 2D openVG
*/

#ifndef gcdENABLE_VG
#   define gcdENABLE_VG                         0
#endif

/*
    gcdDYNAMIC_MAP_RESERVED_MEMORY

        When gcvPOOL_SYSTEM is constructed from RESERVED memory,
        driver can map the whole reserved memory to kernel space
        at the beginning, or just map a piece of memory when need
        to access.

        Notice:
        -  It's only for the 2D openVG. For other cores, there is
           _NO_ need to map reserved memory to kernel.
        -  It's meaningless when memory is allocated by
           gckOS_AllocateContiguous, in that case, memory is always
           mapped by system when allocated.
*/
#ifndef gcdDYNAMIC_MAP_RESERVED_MEMORY
#   define gcdDYNAMIC_MAP_RESERVED_MEMORY      1
#endif

/*
   gcdPAGED_MEMORY_CACHEABLE

        When non-zero, paged memory will be cacheable.

        Normally, driver will detemines whether a video memory
        is cacheable or not. When cacheable is not neccessary,
        it will be writecombine.

        This option is only for those SOC which can't enable
        writecombine without enabling cacheable.
*/

#ifndef gcdPAGED_MEMORY_CACHEABLE
#   define gcdPAGED_MEMORY_CACHEABLE            0
#endif

/*
   gcdNONPAGED_MEMORY_CACHEABLE

        When non-zero, non paged memory will be cacheable.
*/

#ifndef gcdNONPAGED_MEMORY_CACHEABLE
#   define gcdNONPAGED_MEMORY_CACHEABLE         0
#endif

/*
   gcdNONPAGED_MEMORY_BUFFERABLE

        When non-zero, non paged memory will be bufferable.
        gcdNONPAGED_MEMORY_BUFFERABLE and gcdNONPAGED_MEMORY_CACHEABLE
        can't be set 1 at same time
*/

#ifndef gcdNONPAGED_MEMORY_BUFFERABLE
#   define gcdNONPAGED_MEMORY_BUFFERABLE        1
#endif

/*
    gcdENABLE_INFINITE_SPEED_HW
            enable the Infinte HW , this is for 2D openVG
*/

#ifndef gcdENABLE_INFINITE_SPEED_HW
#   define gcdENABLE_INFINITE_SPEED_HW          0
#endif

/*
    gcdENABLE_TS_DOUBLE_BUFFER
            enable the TS double buffer, this is for 2D openVG
*/

#ifndef gcdENABLE_TS_DOUBLE_BUFFER
#   define gcdENABLE_TS_DOUBLE_BUFFER           1
#endif

/*
    gcd6000_SUPPORT

    Temporary define to enable/disable 6000 support.
 */
#ifndef gcd6000_SUPPORT
#   define gcd6000_SUPPORT                      0
#endif

/*
    gcdUSE_VIDMEM_PER_PID
*/
#ifndef gcdUSE_VIDMEM_PER_PID
#   define gcdUSE_VIDMEM_PER_PID                0
#endif

/*
    QNX_SINGLE_THREADED_DEBUGGING
*/
#ifndef QNX_SINGLE_THREADED_DEBUGGING
#   define QNX_SINGLE_THREADED_DEBUGGING        0
#endif

/*
    gcdENABLE_RECOVERY

        This define enables the recovery code.
*/
#ifndef gcdENABLE_RECOVERY
#   define gcdENABLE_RECOVERY                   1
#endif

/*
    gcdRENDER_THREADS

        Number of render threads. Make it zero, and there will be no render
        threads.
*/
#ifndef gcdRENDER_THREADS
#   define gcdRENDER_THREADS                    0
#endif

/*
    gcdSMP

        This define enables SMP support.

        Currently, it only works on Linux/Android,
        Kbuild will config it according to whether
        CONFIG_SMP is set.

*/
#ifndef gcdSMP
#   define gcdSMP                               0
#endif

/*
    gcdSUPPORT_SWAP_RECTANGLE

        Support swap with a specific rectangle.

        Set the rectangle with eglSetSwapRectangleANDROID api.
*/
#if defined(ANDROID)
#define gcdSUPPORT_SWAP_RECTANGLE               1
#else
#define gcdSUPPORT_SWAP_RECTANGLE               0
#endif

/*
    gcdDEFER_RESOLVES

        Support deferred resolves for 3D apps.
*/
#ifndef gcdDEFER_RESOLVES
#   define gcdDEFER_RESOLVES                    0
#endif

/*
    gcdCOPYBLT_OPTIMIZATION

        Combine dirty areas resulting from Android's copyBlt.
*/
#ifndef gcdCOPYBLT_OPTIMIZATION
#   define gcdCOPYBLT_OPTIMIZATION              0
#endif

/*
    gcdGPU_LINEAR_BUFFER_ENABLED

        Use linear buffer for GPU apps so HWC can do 2D composition.
*/
#ifndef gcdGPU_LINEAR_BUFFER_ENABLED
#   define gcdGPU_LINEAR_BUFFER_ENABLED         1
#endif

/*
    gcdSHARED_RESOLVE_BUFFER_ENABLED

        Use shared resolve buffer for all app buffers.
*/
#ifndef gcdSHARED_RESOLVE_BUFFER_ENABLED
#   define gcdSHARED_RESOLVE_BUFFER_ENABLED         0
#endif

/*
     gcdUSE_TRIANGLE_STRIP_PATCH
 */
#ifndef gcdUSE_TRIANGLE_STRIP_PATCH
#   define gcdUSE_TRIANGLE_STRIP_PATCH            1
#endif

/*
    gcdENABLE_OUTER_CACHE_PATCH

        Enable the outer cache patch.
*/
#ifndef gcdENABLE_OUTER_CACHE_PATCH
#   define gcdENABLE_OUTER_CACHE_PATCH          1
#endif

#if defined(ANDROID)
    #define  gcdANDROID_UNALIGNED_LINEAR_COMPOSITION_ADJUST    1
#else
    #define  gcdANDROID_UNALIGNED_LINEAR_COMPOSITION_ADJUST    0
#endif

#ifndef gcdENABLE_PE_DITHER_FIX
#   define gcdENABLE_PE_DITHER_FIX              1
#endif

#ifndef gcdSHARED_PAGETABLE
#   define gcdSHARED_PAGETABLE                  1
#endif
#ifndef gcdUSE_PVR
#   define gcdUSE_PVR                           1
#endif

/*
    gcdSMALL_BLOCK_SIZE

        When non-zero, a part of VIDMEM will be reserved for requests
        whose requesting size is less than gcdSMALL_BLOCK_SIZE.

        For Linux, it's the size of a page. If this requeset fallbacks
        to gcvPOOL_CONTIGUOUS or gcvPOOL_VIRTUAL, memory will be wasted
        because they allocate a page at least.
 */
#ifndef gcdSMALL_BLOCK_SIZE
#   define gcdSMALL_BLOCK_SIZE                  4096
#   define gcdRATIO_FOR_SMALL_MEMORY            32
#endif

/*
    gcdCONTIGUOUS_SIZE_LIMIT
        When non-zero, size of video node from gcvPOOL_CONTIGUOUS is
        limited by gcdCONTIGUOUS_SIZE_LIMIT.
 */
#ifndef gcdCONTIGUOUS_SIZE_LIMIT
#   define gcdCONTIGUOUS_SIZE_LIMIT             0
#endif

#ifndef gcdDISALBE_EARLY_EARLY_Z
#   define gcdDISALBE_EARLY_EARLY_Z             1
#endif

#ifndef gcdSHADER_SRC_BY_MACHINECODE
#   define gcdSHADER_SRC_BY_MACHINECODE         1
#endif

/*
    gcdLINK_QUEUE_SIZE

        When non-zero, driver maintains a queue to record information of
        latest lined context buffer and command buffer. Data in this queue
        is be used to debug.
*/
#ifndef gcdLINK_QUEUE_SIZE
#   define gcdLINK_QUEUE_SIZE                  0
#endif

/*  gcdALPHA_KILL_IN_SHADER
 *
 *  Enable alpha kill inside the shader. This will be set automatically by the
 *  HAL if certain states match a criteria.
 */
#ifndef gcdALPHA_KILL_IN_SHADER
#   define gcdALPHA_KILL_IN_SHADER              1
#endif

#ifndef gcdUSE_WCLIP_PATCH
#   define gcdUSE_WCLIP_PATCH                   1
#endif

#ifndef gcdUSE_NPOT_PATCH
#define gcdUSE_NPOT_PATCH                       0
#endif


#ifndef gcdHZ_L2_DISALBE
#   define gcdHZ_L2_DISALBE                     1
#endif

#ifndef gcdBUGFIX15_DISABLE
#   define gcdBUGFIX15_DISABLE                  1
#endif

#ifndef gcdDISABLE_HZ_FAST_CLEAR
#   define gcdDISABLE_HZ_FAST_CLEAR             1
#endif

#define gcdCONTEXT_CLIENT_VERSION_OES11         0x00000001
#define gcdCONTEXT_CLIENT_VERSION_OES20         0x00000002

/********************************************************************************\
************************* MRVL Macro Definition *************************************
\********************************************************************************/
#define MRVL_DISABLE_FASTCLEAR                  0

/* API log enable */
#define MRVL_ENABLE_ERROR_LOG                   1
#define MRVL_ENABLE_API_LOG                     0
#define MRVL_ENABLE_EGL_API_LOG                 0
#define MRVL_ENABLE_OES1_API_LOG                0
#define MRVL_ENABLE_OES2_API_LOG                0
#define MRVL_ENABLE_OVG_API_LOG                 0
#define MRVL_ENABLE_OCL_API_LOG                 0

/* enable it can dump logs to file
 * for android can stop dump by "setprop marvell.graphics.dump_log 0"
*/
#define MRVL_DUMP_LOG_TO_FILE                   0
/*
    Force disable super tiling feature.

       Use to confirm render error issue whether related with supertile.
*/
#define MRVL_DISABLE_SUPERTILED                 0

/* Use pmem flag */

#if defined(CONFIG_GPU_RESERVE_MEM)
#define MRVL_USE_GPU_RESERVE_MEM                1
#else
#define MRVL_USE_GPU_RESERVE_MEM                0
#endif

/*
    gcdMAX_ALLOC_PAGES_RETRY

        Maximum retry times to allocate non-paged memory.
*/
#ifndef gcdMAX_ALLOC_PAGES_RETRY
#   define gcdMAX_ALLOC_PAGES_RETRY             10
#endif


/* Enable user mode heap allocation
 * if enable, all internal structure should be allocated from pre-allocated heap
 * if disable, all internal structure should be allocated by malloc directly
 */

#define MRVL_ENABLE_USERMODE_HEAP_ALLOCATION    0

/* Enable kernel mode heap allocation
 * if enable, all internal structure should be allocated from pre-allocated heap
 * if disable, all internal structure should be allocated by kmalloc directly
 */
#define MRVL_ENABLE_KERNELMODE_HEAP_ALLOCATION  0


/* Marvell dump surface flag */
#define MRVL_ENABLE_DUMP_SURFACE                1
#define MRVL_ENABLE_DUMP_TEXTURE_SURFACE        0
#define MRVL_ENABLE_DUMP_EGLIMAGE_SURFACE       0
#define MRVL_ENABLE_DUMP_RT_SURFACE             0

#define MRVL_DUMP_SHADER_SOURCE                 0

/* Dump command flag, use to dump all commands in every commit. */
#define MRVL_ENABLE_DUMP_CMD                    0

#define MRVL_SWAP_BUFFER_IN_EVERY_DRAW          0

#if ((defined CONFIG_CPU_PXA988) || \
     (defined CONFIG_CPU_PXA1088) || \
     (defined CONFIG_CPU_PXA1L88))
#define MRVL_PLATFORM_PXA988_FAMILY             1
#else
#define MRVL_PLATFORM_PXA988_FAMILY             0
#endif

/* Do not align u/v stride to 16 */
#define VIVANTE_ALIGN_UVSTRIDE                  0

/*
 * Power Management
 */
#if defined(ANDROID)
#define MRVL_ENABLE_GC_POWER_CLOCK              1
#else
#define MRVL_ENABLE_GC_POWER_CLOCK              0
#endif

/*
 * MRVL_CONFIG_SHADER_CLK_CONTROL
 *      default is enable, and let has_feat_gc_shader() to check
 *      whether current platform supports it or not.
 */
#if MRVL_PLATFORM_PXA988_FAMILY
#define MRVL_CONFIG_SHADER_CLK_CONTROL          1
#else
#define MRVL_CONFIG_SHADER_CLK_CONTROL          0
#endif

/*
    gcdPOWEROFF_TIMEOUT

        When non-zero, GPU will power off automatically from
        idle state, and gcdPOWEROFF_TIMEOUT is also the default
        timeout in milliseconds.
 */

#ifndef gcdPOWEROFF_TIMEOUT
#define gcdPOWEROFF_TIMEOUT                     2000
#endif

/*
    MRVL_CONFIG_PASS_FROM_UBOOT
        config GC get default parameter from uboot or init.xxx.rc
*/
#define MRVL_CONFIG_PASS_FROM_UBOOT             0

/*
    MRVL_CONFIG_USE_PM_RUNTIME
        -- use PM_RUNTIME power management
        -- 2013/12/18 defeature for all platforms
*/
#if (defined CONFIG_PM_RUNTIME)
#define MRVL_CONFIG_USE_PM_RUNTIME              0
#else
#define MRVL_CONFIG_USE_PM_RUNTIME              0
#endif

/*
    MRVL_CONFIG_SYSFS
        -- use sysfs file system
*/
#define MRVL_CONFIG_SYSFS                       1

/*
    MRVL_CONFIG_ENABLE_GC_TRACE
        -- enable GC trace support
*/
#if (defined ANDROID) && MRVL_PLATFORM_PXA988_FAMILY
#define MRVL_CONFIG_ENABLE_GC_TRACE             1
#else
#define MRVL_CONFIG_ENABLE_GC_TRACE             0
#endif

/*
    MRVL_CONFIG_POWER_VALIDATION
        -- Marco for power validation
*/
#if (MRVL_CONFIG_SYSFS)
#define MRVL_CONFIG_POWER_VALIDATION            0
#else
#define MRVL_CONFIG_POWER_VALIDATION            0
#endif

/*
    MRVL_CONFIG_ENABLE_GPUFREQ
        -- Marco for enabling GPUFREQ
*/
#if (defined ANDROID || defined X11) && (USE_GPU_FREQ) && (MRVL_CONFIG_SYSFS) \
    && MRVL_PLATFORM_PXA988_FAMILY
#define MRVL_CONFIG_ENABLE_GPUFREQ              1
# else
#define MRVL_CONFIG_ENABLE_GPUFREQ              0
#endif

#if (defined ANDROID) && MRVL_PLATFORM_PXA988_FAMILY
#define MRVL_CONFIG_ENABLE_QOS_SUPPORT          1
# else
#define MRVL_CONFIG_ENABLE_QOS_SUPPORT          0
#endif

/*Pulse Eater counter --- record Nums*/
#if MRVL_CONFIG_ENABLE_GPUFREQ && MRVL_PLATFORM_PXA988_FAMILY
#define MRVL_PULSE_EATER_COUNT_NUM              200
#else
#define MRVL_PULSE_EATER_COUNT_NUM              0
#endif

#ifndef gcdPulseEaterPeriode
#define gcdPulseEaterPeriode                  10
#endif

/* GPUDump -- Enable GPU Dump*/
#define MRVL_DUMP_GC_INFO                       0

/* Enable dvfm control for certain platform */
#if (defined ANDROID)
#define MRVL_CONFIG_ENABLE_DVFM                 0
#else
#define MRVL_CONFIG_ENABLE_DVFM                 0
#endif

#ifdef CONFIG_SMP
#define gcdSMP                                  1
#else
#define gcdSMP                                  0
#endif

#define MRVL_ALWAYS_PRINT_ERROR                 1

/*
 *   use hw readpixel path
 */
#define MRVL_READ_PIXEL_HW                      1

/* Used for benchmark GC performance on critial section. */
#define MRVL_BENCH                              0
/*USR leak need to cowork with ps -t, this hook start&end_report at gcoOS_Construct/ _TLSDestructor*/
/*when you found the one thread exit. But mm alloc record exist in Process & come from that thread. Bingo Leak*/
#define MRVL_ENABLE_USER_LEAK_CHECK             0
#if (defined ANDROID)&&(!MRVL_ENABLE_USER_LEAK_CHECK)
#define MRVL_ENABLE_WHITE_LIST                  1
#else
#define MRVL_ENABLE_WHITE_LIST                  0
#endif

#ifndef MRVL_ENABLE_VIV_POWER_MANAGEMENT
#define MRVL_ENABLE_VIV_POWER_MANAGEMENT        0
#endif

/*
* Tune OPF using gcuSet() interface.
*/
#define MRVL_GCU_TUNING_OPF                     0

/*
#define MRVL_OLD_FLUSHCACHE                   1
*/
#define MRVL_GC_FLUSHCACHE_PFN                1

/*
*  Definitions for vendor, renderer and version strings
*/
/* #################### [START ==DO NOT CHANGE THIS MARCRO== START] #################### */
/* @Ziyi: This string is checked by skia-neon related code to identify Marvell silicon */
#define _VENDOR_STRING_                         "Marvell Technology Group Ltd"
/* @Ziyi: If any change happened between these 2 comments please contact zyxu@marvell.com, Thanks. */
/* #################### [START ==DO NOT CHANGE THIS MARCRO== START] #################### */

#define _EGL_VERSION_STRING_                    "EGL 1.3"

#if defined(COMMON_LITE)
#define _OES11_VERSION_STRING_                  "OpenGL ES-CL 1.1"
#else
#define _OES11_VERSION_STRING_                  "OpenGL ES-CM 1.1"
#endif

#define _OES20_VERSION_STRING_                  "OpenGL ES 2.0"
#define _GLSL_ES_VERSION_STRING_                "OpenGL ES GLSL ES 1.00"

#define _OPENVG_VERSION_STRING_                 "OpenVG 1.1"

#define _GC_VERSION_STRING_                     "GC Ver rls_pxa988_KK44_GC13.20"

#ifndef gcdSYNC
#   define gcdSYNC                              1
#endif

#ifndef gcdENABLE_SPECIAL_HINT3
#   define gcdENABLE_SPECIAL_HINT3               1
#endif

/*
    gcdDVFS

        When non-zero, software will make use of dynamic voltage and
        frequency feature.
 */
#ifndef gcdDVFS
#   define gcdDVFS                               0
#   define gcdDVFS_ANAYLSE_WINDOW                4
#   define gcdDVFS_POLLING_TIME                  (gcdDVFS_ANAYLSE_WINDOW * 4)
#endif

/*
    gcdANDROID_NATIVE_FENCE_SYNC

        Enable android native fence sync. It is introduced since jellybean-4.2.
        Depends on linux kernel option: CONFIG_SYNC.

        0: Disabled
        1: Build framework for native fence sync feature, and EGL extension
        2: Enable async swap buffers for client
           * Native fence sync for client 'queueBuffer' in EGL, which is
             'acquireFenceFd' for layer in compositor side.
        3. Enable async hwcomposer composition.
           * 'releaseFenceFd' for layer in compositor side, which is native
             fence sync when client 'dequeueBuffer'
           * Native fence sync for compositor 'queueBuffer' in EGL, which is
             'acquireFenceFd' for framebuffer target for DC
 */
#ifndef gcdANDROID_NATIVE_FENCE_SYNC
#   define gcdANDROID_NATIVE_FENCE_SYNC        0
#endif

#ifndef gcdFORCE_MIPMAP
#   define gcdFORCE_MIPMAP                     0
#endif

/*
    gcdFORCE_GAL_LOAD_TWICE

        When non-zero, each thread except the main one will load libGAL.so twice to avoid potential segmetantion fault when app using dlopen/dlclose.
        If threads exit arbitrarily, libGAL.so may not unload until the process quit.
 */
#ifndef gcdFORCE_GAL_LOAD_TWICE
#   define gcdFORCE_GAL_LOAD_TWICE             0
#endif

#endif /* __gc_hal_options_h_ */
