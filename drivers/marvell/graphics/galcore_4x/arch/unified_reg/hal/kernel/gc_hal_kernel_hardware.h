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




#ifndef __gc_hal_kernel_hardware_h_
#define __gc_hal_kernel_hardware_h_

#if gcdENABLE_VG
#include "gc_hal_kernel_hardware_vg.h"

#endif

#ifdef __cplusplus
extern "C" {

#endif

#if MRVL_PULSE_EATER_COUNT_NUM
typedef struct _gckPulseEaterCounter * gckPulseEaterCounter;
typedef struct _gckPulseEaterDB * gckPulseEaterDB;
typedef struct _pulseEater_frequency_table * pulseEaterFTL;

struct _pulseEater_frequency_table
{
    gctUINT32    index;
    gctUINT32    hwPeriod;
    gctUINT32    swPeriod;
    gctUINT8     hwMs;
    gctUINT32    frequency;
};

struct _gckPulseEaterCounter
{
    gctUINT32                   sfPeriod;
    gctUINT32                   freq;
    gctUINT32                   time;
    gctUINT8                    hwMs;
    gctUINT8                    pulseCount[4];
};

struct _gckPulseEaterDB
{
    gctUINT32                   lastIndex;
    gctUINT32                   timeStamp;
    gctUINT32                   timeStampInLastRound;
    struct _gckPulseEaterCounter data[MRVL_PULSE_EATER_COUNT_NUM];
    gctUINT32                   startTime;
    gctUINT32                   busyRatio[4];
    gctUINT32                   totalTime[4];
    gctUINT32                   startIndex;
    gctBOOL                     moreRound;
};

#endif

/* gckHARDWARE object. */
struct _gckHARDWARE
{
    /* Object. */
    gcsOBJECT                   object;

    /* Pointer to gctKERNEL object. */
    gckKERNEL                   kernel;

    /* Pointer to gctOS object. */
    gckOS                       os;

    /* Core */
    gceCORE                     core;

    /* Chip characteristics. */
    gcsHAL_QUERY_CHIP_IDENTITY  identity;
    gctBOOL                     allowFastClear;
    gctBOOL                     allowCompression;
    gctUINT32                   powerBaseAddress;
    gctBOOL                     extraEventStates;

    /* Big endian */
    gctBOOL                     bigEndian;

    /* Chip status */
    gctPOINTER                  powerMutex;
    gctUINT32                   powerProcess;
    gctUINT32                   powerThread;
    gceCHIPPOWERSTATE           chipPowerState;
    gctUINT32                   lastWaitLink;
    gctUINT32                   lastWaitLinkB;

    gctBOOL                     clockState;
    gctBOOL                     powerState;
    gctPOINTER                  globalSemaphore;
    gckRecursiveMutex           recMutexPower;
    gctBOOL                     clk2D3D_Enable;
    gctUINT32                   refExtClock;
    gctUINT32                   refExtPower;

    /* flag for auto_pulse_eater(dvfs) */
    gceDVFS                     dvfs;

    gctISRMANAGERFUNC           startIsr;
    gctISRMANAGERFUNC           stopIsr;
    gctPOINTER                  isrContext;

    gctUINT32                   mmuVersion;

    /* Type */
    gceHARDWARE_TYPE            type;

#if gcdPOWEROFF_TIMEOUT
    gctUINT32                   powerOffTime;
    gctUINT32                   powerOffTimeout;
    gctPOINTER                  powerOffTimer;

#endif

#if gcdENABLE_FSCALE_VAL_ADJUST
    gctUINT32                   powerOnFscaleVal;

#endif
    gctPOINTER                  pageTableDirty;
#if MRVL_CONFIG_ENABLE_GPUFREQ
    struct gcsDEVOBJECT         devObj;
#   if MRVL_CONFIG_SHADER_CLK_CONTROL
    struct gcsDEVOBJECT         devShObj;
#   endif

#endif

#if gcdLINK_QUEUE_SIZE
    struct _gckLINKQUEUE        linkQueue;

#endif
    gctUINT32                   outstandingRequest;
    gctUINT32                   outstandingLimitation;

    gctBOOL                     powerManagement;

#if MRVL_PULSE_EATER_COUNT_NUM
    gckPulseEaterDB             pulseEaterDB[2];
    gctPOINTER                  pulseEaterMutex;
    gctPOINTER                  pulseEaterTimer;
    gctINT                      hwPeriod;
    gctUINT32                   sfPeriod;
    gctUINT8                    hwMs;
    gctUINT32                   freq;

#endif
};

gceSTATUS
gckHARDWARE_GetBaseAddress(
    IN gckHARDWARE Hardware,
    OUT gctUINT32_PTR BaseAddress
    );

gceSTATUS
gckHARDWARE_NeedBaseAddress(
    IN gckHARDWARE Hardware,
    IN gctUINT32 State,
    OUT gctBOOL_PTR NeedBase
    );

gceSTATUS
gckHARDWARE_GetFrameInfo(
    IN gckHARDWARE Hardware,
    OUT gcsHAL_FRAME_INFO * FrameInfo
    );

#ifdef __cplusplus
}

#endif


#endif /* __gc_hal_kernel_hardware_h_ */
