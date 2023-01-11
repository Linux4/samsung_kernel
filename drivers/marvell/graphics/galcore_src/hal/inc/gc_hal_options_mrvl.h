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


#ifndef __gc_hal_options_mrvl_h_
#define __gc_hal_options_mrvl_h_

#if (defined ANRDOID || defined LINUX || defined X11)
#include <linux/version.h>
#endif

/********************************************************************************\
************************* MRVL Macro Definition *************************************
\********************************************************************************/

#define GC_DRIVER_VERSION 5

/* Eden/TTD2 */
#if (defined CONFIG_MACH_TTD2_FPGA) || (defined CONFIG_MACH_EDEN_FPGA) || \
    (defined CONFIG_MACH_PXA1928_FPGA)
#define MRVL_PLATFORM_TTD2_FPGA                 1
#else
#define MRVL_PLATFORM_TTD2_FPGA                 0
#endif

#ifdef CONFIG_CPU_PXA1986
#define MRVL_PLATFORM_ADIR                      1
#else
#define MRVL_PLATFORM_ADIR                      0
#endif

/*
 * Power Management
 */
#if defined(ANDROID)
#define MRVL_ENABLE_GC_POWER_CLOCK              1
#else
#define MRVL_ENABLE_GC_POWER_CLOCK              0
#endif

#if defined(LINUX)
/*
    Reserve GPU memory resource by device tree.
*/
#if (defined CONFIG_OF) && ((defined CONFIG_ARM64)|| (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)))
#define MRVL_GPU_RESOURCE_DT                    1
#else
#define MRVL_GPU_RESOURCE_DT                    0
#endif

/* Reserve memory by using memblock when board inits */
#if (MRVL_GPU_RESOURCE_DT) || (defined CONFIG_GPU_RESERVE_MEM)
#define MRVL_USE_GPU_RESERVE_MEM                1
#else
#define MRVL_USE_GPU_RESERVE_MEM                0
#endif
#endif

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

#define MRVL_ENABLE_USER_LEAK_CHECK             0
#if (defined ANDROID)&&(!MRVL_ENABLE_USER_LEAK_CHECK)
#define MRVL_ENABLE_WHITE_LIST                  1
#else
#define MRVL_ENABLE_WHITE_LIST                  0
#endif

#define MRVL_MAX_CLOCK_DEPTH                    1
#define MRVL_MAX_POWER_DEPTH                    1

#ifndef MRVL_ENABLE_DUMP_SURFACE
#define MRVL_ENABLE_DUMP_SURFACE                1
#endif

#define MRVL_ALWAYS_PRINT_ERROR                 1

/*
    MRVL_CONFIG_SYSFS
        -- use sysfs file system
*/
#define MRVL_CONFIG_SYSFS                       1

/*
    MRVL_CONFIG_ENABLE_GC_TRACE
        -- enable GC trace support
*/
#define MRVL_CONFIG_ENABLE_GC_TRACE             1

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
#if (defined ANDROID || defined X11) && (USE_GPU_FREQ) && (MRVL_CONFIG_SYSFS)
#define MRVL_CONFIG_ENABLE_GPUFREQ              1
#else
#define MRVL_CONFIG_ENABLE_GPUFREQ              0
#endif

/*
 * MRVL_CONFIG_SHADER_CLK_CONTROL
 *      default is enable, and let has_feat_shader_indept_dfc() to check
 *      whether current platform supports it or not.
 */
#define MRVL_CONFIG_SHADER_CLK_CONTROL          1

#define MRVL_CONFIG_ENABLE_QOS_SUPPORT          1

/*
    MRVL_DFC_PROTECT_REG_ACCESS
        -- Protect register access when DFC to workaround Eden Z1 GC DFC issue
*/
#if MRVL_CONFIG_ENABLE_GPUFREQ && \
    ((defined CONFIG_CPU_PXA1928) && !defined(CONFIG_ARM64))
#define MRVL_DFC_PROTECT_REG_ACCESS             1
#else
#define MRVL_DFC_PROTECT_REG_ACCESS             0
#endif

/*Pulse Eater counter --- record Nums*/
#ifndef PULSE_EATER_COUNT
#define PULSE_EATER_COUNT                       200
#endif

#ifndef gcdPulseEaterPeriode
#define gcdPulseEaterPeriode                  10
#endif

/*
    MRVL_ENABLE_S3TC_TEXTURE
        -- Marco for S3TC compression texture
*/
#define MRVL_ENABLE_S3TC_TEXTURE                1

/*
    MRVL_ENABLE_LINEAR_TEXTURE
        -- Marco for linear texture
*/
#define MRVL_ENABLE_LINEAR_TEXTURE              1

/*  MRVL_ENABLE_LINEAR_TEXTURE_YUV
        -- Marco for linear texture of YUV
        -- use three linear A8 textures to replace one YUV texture
*/
#define MRVL_ENABLE_LINEAR_TEXTURE_YUV          1

/*
    MRVL_DISABLE_USER_SIGNAL_TIMEOUT
        -- Macro to disable timeout mechanism for user signal
*/
#define MRVL_DISABLE_USER_SIGNAL_TIMEOUT        1

/*
    for cl_khr_gl_sharing
*/
#define MRVL_CL_KHR_GL_SHARING                  1

/*
    MRVL_MMU_FLATMAP_2G_ADDRESS
        Workaround for GC MMU 2G address error access issue
*/
#define MRVL_MMU_FLATMAP_2G_ADDRESS             1

/*
    OPTION_ENABLE_GPUTEX

        This define enables gpu tex.
*/
#ifdef CONFIG_ENABLE_GPUTEX
#define MRVL_ENABLE_GPUTEX                 1
#else
#define MRVL_ENABLE_GPUTEX                 0
#endif

#ifndef MRVL_USE_MRVL_SUPER_TILE_UPLOAD
#define MRVL_USE_MRVL_SUPER_TILE_UPLOAD    1
#endif

/*
    MRVL_USE_VIV_ICD_DISPATCH
        If 0, use marvll dispatch table for CL, otherwise use vivante's introduced in 5.0.6.
*/
#ifndef MRVL_USE_VIV_ICD_DISPATCH
#define MRVL_USE_VIV_ICD_DISPATCH               0
#endif

/*
    MRVL_FINISH_PER_DRAW
        -- Marco for calling glFinish after each draw
*/

#ifndef MRVL_FINISH_PER_DRAW
#define MRVL_FINISH_PER_DRAW 0
#endif

#ifndef MRVL_ENABLE_ES11_SHARECONTEXT
#define MRVL_ENABLE_ES11_SHARECONTEXT 1
#endif

/*
    MRVL_DISABLE_TEXTURE_YUV_ASSEMBLER
        -- disable TX__YUV_ASSEMBLER(gcvFEATURE_TEXTURE_YUV_ASSEMBLER)
           since some format can't work
           e.g. I420 and NV21 can't work while YV12 can for camera on adir
*/
#ifndef MRVL_DISABLE_TEXTURE_YUV_ASSEMBLER
#define MRVL_DISABLE_TEXTURE_YUV_ASSEMBLER 1
#endif


#define MRVL_OLD_FLUSHCACHE                   0

#define MRVL_GC_FLUSHCACHE_PFN                1


/*
*  Definitions for vendor, renderer and version strings
*/
/* #################### [START ==DO NOT CHANGE THIS MARCRO== START] #################### */
/* @Ziyi: This string is checked by skia-neon related code to identify Marvell silicon */
#define _VENDOR_STRING_                         "Marvell Technology Group Ltd"
/* @Ziyi: If any change happened between these 2 comments please contact zyxu@marvell.com, Thanks. */
/* #################### [START ==DO NOT CHANGE THIS MARCRO== START] #################### */

#define _GC_VERSION_STRING_                     "GC version rls_5011p6_GC5.5.14"

/* Do not align u/v stride to 16 */
#define VIVANTE_ALIGN_UVSTRIDE                  0

/* Enable single layer RGB bypass in HWC-GCU */
#define RGB_BYPASS                              0

/*
    Disable 2D block size setting
        for adir get bad performance with this on.
*/
#define DISABLE_2D_BLOCK_SIZE_SETTING           0

/* Block size and split rect tuning */
#define MRVL_BLOCK_SIZE_TUNING                  1

/* Enable NEON memcpy to replace default memcpy */
#if (!defined MRVL_ENABLE_GPUTEX_MEMCPY) && (defined ANDROID) \
    && (!gcdFPGA_BUILD) && (MRVL_ENABLE_GPUTEX)
#define MRVL_ENABLE_GPUTEX_MEMCPY               1
#else
#define MRVL_ENABLE_GPUTEX_MEMCPY               0
#endif

#ifndef MRVL_DISABLE_INTERNAL_DFS
#define MRVL_DISABLE_INTERNAL_DFS 0
#endif

#define MRVL_CONFIG_PROC                        1

#define MRVL_DUMP_COMMAND                        1

#endif /* __gc_hal_options_mrvl_h_*/
