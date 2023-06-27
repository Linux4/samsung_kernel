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




#ifndef __gc_hal_user_h_
#define __gc_hal_user_h_

#include "gc_hal.h"
#include "gc_hal_driver.h"
#include "gc_hal_enum.h"
#include "gc_hal_dump.h"
#include "gc_hal_base.h"
#include "gc_hal_raster.h"
#include "gc_hal_user_math.h"
#include "gc_hal_user_debug.h"

#ifndef VIVANTE_NO_3D
#if gcdENABLE_VG
#include "gc_hal_vg.h"
#include "gc_hal_user_vg.h"
#endif
#include "gc_hal_engine.h"
#include "gc_hal_compiler.h"
#endif

#define OPT_VERTEX_ARRAY                  1

#ifdef __cplusplus
extern "C" {
#endif

#define gcdSTREAM_CACHE_SIZE_HWUI  (96 << 10)
#define gcdINDEX_CACHE_SIZE_HWUI   (32 << 10)
#define gcdSTREAM_CACHE_SIZE       (512 << 10)
#define gcdINDEX_CACHE_SIZE        (128 << 10)

/******************************************************************************\
********************************* Private Macro ********************************
\******************************************************************************/

#define gcmGETFENCEENABLE(fenceEnable) \
{ \
    gcsTLS_PTR __tls__; \
    \
    gcoOS_GetTLS(&__tls__); \
    fenceEnable = __tls__ ? __tls__->fenceEnable : gcvFALSE; \
}
#define gcmENABLEFENCE() \
{ \
    gcsTLS_PTR __tls__; \
    \
    gcoOS_GetTLS(&__tls__); \
    if (__tls__); \
        __tls__->fenceEnable = gcvTRUE; \
}

#define gcmGETHARDWARE(Hardware) \
{ \
    gcsTLS_PTR __tls__; \
    \
    gcmONERROR(gcoOS_GetTLS(&__tls__)); \
    \
    if (__tls__->currentType == gcvHARDWARE_2D \
        && gcvSTATUS_TRUE == gcoHAL_QuerySeparated3D2D(gcvNULL) \
        ) \
    { \
        if (__tls__->hardware2D == gcvNULL) \
        { \
            gcmONERROR(gcoHARDWARE_Construct(gcPLS.hal, __tls__->currentType, &__tls__->hardware2D)); \
            \
            gcmTRACE_ZONE( \
                gcvLEVEL_VERBOSE, gcvZONE_HARDWARE, \
                "%s(%d): hardware object 0x%08X constructed.\n", \
                __FUNCTION__, __LINE__, __tls__->hardware2D \
                ); \
        } \
        \
        Hardware = __tls__->hardware2D; \
    } \
    else \
    { \
        if (__tls__->hardware == gcvNULL) \
        { \
            gcmONERROR(gcoHARDWARE_Construct(gcPLS.hal, __tls__->currentType, &__tls__->hardware)); \
            \
            gcmTRACE_ZONE( \
                gcvLEVEL_VERBOSE, gcvZONE_HARDWARE, \
                "%s(%d): hardware object 0x%08X constructed.\n", \
                __FUNCTION__, __LINE__, __tls__->hardware \
                ); \
        } \
        \
        Hardware = __tls__->hardware; \
    } \
}

#define gcmGETVGHARDWARE(Hardware) \
{ \
    gcsTLS_PTR __tls__; \
    gceSTATUS verifyStatus; \
    \
    verifyStatus = gcoOS_GetTLS(&__tls__); \
    if (gcmIS_ERROR(verifyStatus)) \
    { \
        gcmFOOTER_ARG("status = %d", verifyStatus); \
        return verifyStatus;                  \
    }                                   \
    \
    if (__tls__->vg == gcvNULL) \
    { \
        verifyStatus = gcoVGHARDWARE_Construct(gcPLS.hal, &__tls__->vg); \
        if (gcmIS_ERROR(verifyStatus))            \
        {                                   \
            gcmFOOTER_ARG("status = %d", verifyStatus); \
            return verifyStatus;                  \
        }                                   \
        \
        gcmTRACE_ZONE( \
            gcvLEVEL_VERBOSE, gcvZONE_HARDWARE, \
            "%s(%d): hardware object 0x%08X constructed.\n", \
            __FUNCTION__, __LINE__, __tls__->vg \
            ); \
    } \
    \
    Hardware = __tls__->vg; \
}

#define gcmGETCURRENTHARDWARE(hw) \
{ \
    gcmVERIFY_OK(gcoHAL_GetHardwareType(gcvNULL, &hw));\
    gcmASSERT(hw != gcvHARDWARE_INVALID);\
}

#ifndef VIVANTE_NO_3D

/******************************************************************************\
******************************* Multicast Values *******************************
\******************************************************************************/

/* Value types. */
typedef enum _gceVALUE_TYPE
{
    gcvVALUE_UINT,
    gcvVALUE_FIXED,
    gcvVALUE_FLOAT,
}
gceVALUE_TYPE;

/* Value unions. */
typedef union _gcuVALUE
{
    gctUINT                     uintValue;
    gctFIXED_POINT              fixedValue;
    gctFLOAT                    floatValue;
}
gcuVALUE;

/******************************************************************************\
*******************************Clear Range of HZ ******************************
\******************************************************************************/
typedef enum _gceHZCLEAR_RANGE
{
    gcvHZCLEAR_ALL   = 0x1,
    gcvHZCLEAR_RECT  = 0x2,
}
gceHZCLEAR_TYPE;

/******************************************************************************\
***************************** gcoSAMPLER Structure *****************************
\******************************************************************************/

/* Sampler structure. */
typedef struct _gcsSAMPLER
{
    gctUINT32               width;
    gctUINT32               height;
    gctUINT32               depth;
    gctUINT32               faces;

    gceSURF_FORMAT          format;
    gceTILING               tiling;

    gctBOOL                 endianHint;

    gctUINT32               lodNum;
    gctUINT32               lodAddr[30];
    gctUINT32               lodStride[30];

    gcsTEXTURE_PTR          textureInfo;
    gctINT                  roundUV;

    gceSURF_ALIGNMENT       hAlignment;
    gceSURF_ADDRESSING      addressing;

    gctBOOL                 hasTileStatus;
    gctUINT32               tileStatusAddr;
    gctUINT32               tileStatusClearValue;
}
gcsSAMPLER;

typedef gcsSAMPLER * gcsSAMPLER_PTR;

/******************************************************************************\
***************************** gcsSAMPLES Structure *****************************
\******************************************************************************/

typedef struct _gcsSAMPLES
{
    gctUINT8 x;
    gctUINT8 y;
}
gcsSAMPLES;
#endif /* VIVANTE_NO_3D */

/******************************************************************************\
****************************** Object Declarations *****************************
\******************************************************************************/

typedef struct _gcoBUFFER *             gcoBUFFER;
typedef struct _gcs2D_State*            gcs2D_State_PTR;

#ifndef VIVANTE_NO_3D

/* Internal dynamic index buffers. */
gceSTATUS
gcoINDEX_UploadDynamicEx(
    IN gcoINDEX Index,
    IN gceINDEX_TYPE IndexType,
    IN gctCONST_POINTER Data,
    IN gctSIZE_T Bytes,
    IN gctBOOL ConvertToIndexedTriangleList
    );

gceSTATUS
gcoINDEX_BindDynamic(
    IN gcoINDEX Index,
    IN gceINDEX_TYPE Type
    );

gceSTATUS
gcoINDEX_GetDynamicIndexRange(
    IN gcoINDEX Index,
    OUT gctUINT32 * MinimumIndex,
    OUT gctUINT32 * MaximumIndex
    );

/******************************************************************************\
********************************** gco3D Object ********************************
\******************************************************************************/

gceSTATUS
gco3D_ClearHzTileStatus(
    IN gco3D Engine,
    IN gcsSURF_INFO_PTR Surface,
    IN gcsSURF_NODE_PTR TileStatus
    );

#endif /* VIVANTE_NO_3D */

/******************************************************************************\
******************************* gcoHARDWARE Object ******************************
\******************************************************************************/

/*----------------------------------------------------------------------------*/
/*----------------------------- gcoHARDWARE Common ----------------------------*/

/* Construct a new gcoHARDWARE object. */
gceSTATUS
gcoHARDWARE_Construct(
    IN gcoHAL Hal,
    IN gceHARDWARE_TYPE HardwareType,
    OUT gcoHARDWARE * Hardware
    );

/* Destroy an gcoHARDWARE object. */
gceSTATUS
gcoHARDWARE_Destroy(
    IN gcoHARDWARE Hardware
    );

/* Query the identity of the hardware. */
gceSTATUS
gcoHARDWARE_QueryChipIdentity(
    OUT gceCHIPMODEL* ChipModel,
    OUT gctUINT32* ChipRevision,
    OUT gctUINT32* ChipFeatures,
    OUT gctUINT32* ChipMinorFeatures,
    OUT gctUINT32* ChipMinorFeatures1,
    OUT gctUINT32* ChipMinorFeatures2
    );

/* Verify whether the specified feature is available in hardware. */
gceSTATUS
gcoHARDWARE_IsFeatureAvailable(
    IN gceFEATURE Feature
    );

/* Query command buffer requirements. */
gceSTATUS
gcoHARDWARE_QueryCommandBuffer(
    OUT gctSIZE_T * Alignment,
    OUT gctSIZE_T * ReservedHead,
    OUT gctSIZE_T * ReservedTail
    );

gceSTATUS gcoHARDWARE_QueryPixelPipesInfo(
    OUT gctUINT32 * pixelPipes,
    OUT gctUINT32 * resolveAlignmentX,
    OUT gctUINT32 * resolveAlignmentY
    );

/* Select a graphics pipe. */
gceSTATUS
gcoHARDWARE_SelectPipe(
    IN gcoHARDWARE Hardware,
    IN gcePIPE_SELECT Pipe
    );

/* Flush the current graphics pipe. */
gceSTATUS
gcoHARDWARE_FlushPipe(
    );

#if !defined(VIVANTE_NO_3D)
/* Send semaphore down the current pipe. */
gceSTATUS
gcoHARDWARE_Semaphore(
    IN gceWHERE From,
    IN gceWHERE To,
    IN gceHOW How
    );
#endif

/* Load a number of load states. */
gceSTATUS
gcoHARDWARE_LoadState(
    IN gctUINT32 Address,
    IN gctSIZE_T Count,
    IN gctPOINTER States
    );

#if !defined(VIVANTE_NO_3D)
/* Load a number of load states in fixed-point (3D pipe). */
gceSTATUS
gcoHARDWARE_LoadStateX(
    IN gctUINT32 Address,
    IN gctSIZE_T Count,
    IN gctPOINTER States
    );
#endif

/* Commit the current command buffer. */
gceSTATUS
gcoHARDWARE_Commit(
    );

/* Stall the pipe. */
gceSTATUS
gcoHARDWARE_Stall(
    );

#ifndef VIVANTE_NO_3D
/* Load the vertex and pixel shaders. */
gceSTATUS
gcoHARDWARE_LoadShaders(
    IN gctSIZE_T StateBufferSize,
    IN gctPOINTER StateBuffer,
    IN gcsHINT_PTR Hints
    );

/* Resolve. */
gceSTATUS
gcoHARDWARE_ResolveRect(
    IN gcsSURF_INFO_PTR SrcInfo,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize
    );

/* Get current surface */
gceSTATUS
gcoHARDWARE_GetCurrentSurface(
    OUT gcsSURF_INFO_PTR * Surface
    );

/* Set current surface */
gceSTATUS
gcoHARDWARE_SetCurrentSurface(
    IN gcsSURF_INFO_PTR Surface
    );

/* Resolve depth buffer. */
gceSTATUS
gcoHARDWARE_ResolveDepth(
    IN gctUINT32 SrcTileStatusAddress,
    IN gcsSURF_INFO_PTR SrcInfo,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize
    );
#endif /* VIVANTE_NO_3D */

/* Query tile sizes. */
gceSTATUS
gcoHARDWARE_QueryTileSize(
    OUT gctINT32 * TileWidth2D,
    OUT gctINT32 * TileHeight2D,
    OUT gctINT32 * TileWidth3D,
    OUT gctINT32 * TileHeight3D,
    OUT gctUINT32 * StrideAlignment
    );

#ifndef VIVANTE_NO_3D
/* Get tile status sizes for a surface. */
gceSTATUS
gcoHARDWARE_QueryTileStatus(
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctSIZE_T Bytes,
    OUT gctSIZE_T_PTR Size,
    OUT gctUINT_PTR Alignment,
    OUT gctUINT32_PTR Filler
    );

/* Set tile status for a surface. */
gceSTATUS
gcoHARDWARE_SetTileStatus(
    IN gcsSURF_INFO_PTR Surface,
    IN gctUINT32 TileStatusAddress,
    IN gcsSURF_NODE_PTR HzTileStatus
    );

/* Enable tile status for a surface. */
gceSTATUS
gcoHARDWARE_EnableTileStatus(
    IN gcsSURF_INFO_PTR Surface,
    IN gctUINT32 TileStatusAddress,
    IN gcsSURF_NODE_PTR HzTileStatus
    );

/* Disable tile status for a surface. */
gceSTATUS
gcoHARDWARE_DisableTileStatus(
    IN gcsSURF_INFO_PTR Surface,
    IN gctBOOL CpuAccess
    );

/* Flush tile status cache. */
gceSTATUS
gcoHARDWARE_FlushTileStatus(
    IN gcsSURF_INFO_PTR Surface,
    IN gctBOOL Decompress
    );

gceSTATUS
gcoHARDWARE_FillFromTileStatus(
    IN gcsSURF_INFO_PTR Surface
    );
#endif /* VIVANTE_NO_3D */

/* Lock a surface. */
gceSTATUS
gcoHARDWARE_Lock(
    IN gcsSURF_NODE_PTR Node,
    OUT gctUINT32 * Address,
    OUT gctPOINTER * Memory
    );

/* Unlock a surface. */
gceSTATUS
gcoHARDWARE_Unlock(
    IN gcsSURF_NODE_PTR Node,
    IN gceSURF_TYPE Type
    );

/* Call kernel for event. */
gceSTATUS
gcoHARDWARE_CallEvent(
    IN OUT gcsHAL_INTERFACE * Interface
    );

/* Schedule destruction for the specified video memory node. */
gceSTATUS
gcoHARDWARE_ScheduleVideoMemory(
    IN gcsSURF_NODE_PTR Node
    );

/* Allocate a temporary surface with specified parameters. */
gceSTATUS
gcoHARDWARE_AllocateTemporarySurface(
    IN gcoHARDWARE Hardware,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gcsSURF_FORMAT_INFO_PTR Format,
    IN gceSURF_TYPE Type
    );

/* Free the temporary surface. */
gceSTATUS
gcoHARDWARE_FreeTemporarySurface(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Synchronized
    );

/* Get a 2D temporary surface with specified parameters. */
gceSTATUS
gcoHARDWARE_Get2DTempSurface(
    IN gctUINT Width,
    IN gctUINT Height,
    IN gceSURF_FORMAT Format,
    OUT gcsSURF_INFO_PTR *SurfInfo
    );

/* Put back the 2D temporary surface from gcoHARDWARE_Get2DTempSurface. */
gceSTATUS
gcoHARDWARE_Put2DTempSurface(
    IN gcsSURF_INFO_PTR SurfInfo
    );

#ifndef VIVANTE_NO_3D
/* Copy a rectangular area with format conversion. */
gceSTATUS
gcoHARDWARE_CopyPixels(
    IN gcsSURF_INFO_PTR Source,
    IN gcsSURF_INFO_PTR Target,
    IN gctINT SourceX,
    IN gctINT SourceY,
    IN gctINT TargetX,
    IN gctINT TargetY,
    IN gctINT Width,
    IN gctINT Height
    );

/* Enable or disable anti-aliasing. */
gceSTATUS
gcoHARDWARE_SetAntiAlias(
    IN gctBOOL Enable
    );

/* Write data into the command buffer. */
gceSTATUS
gcoHARDWARE_WriteBuffer(
    IN gcoHARDWARE Hardware,
    IN gctCONST_POINTER Data,
    IN gctSIZE_T Bytes,
    IN gctBOOL Aligned
    );
#endif /* VIVANTE_NO_3D */

/* Convert RGB8 color value to YUV color space. */
void gcoHARDWARE_RGB2YUV(
    gctUINT8 R,
    gctUINT8 G,
    gctUINT8 B,
    gctUINT8_PTR Y,
    gctUINT8_PTR U,
    gctUINT8_PTR V
    );

/* Convert YUV color value to RGB8 color space. */
void gcoHARDWARE_YUV2RGB(
    gctUINT8 Y,
    gctUINT8 U,
    gctUINT8 V,
    gctUINT8_PTR R,
    gctUINT8_PTR G,
    gctUINT8_PTR B
    );

/* Convert an API format. */
gceSTATUS
gcoHARDWARE_ConvertFormat(
    IN gceSURF_FORMAT Format,
    OUT gctUINT32 * BitsPerPixel,
    OUT gctUINT32 * BytesPerTile
    );

/* Align size to tile boundary. */
gceSTATUS
gcoHARDWARE_AlignToTile(
    IN gceSURF_TYPE Type,
    IN gceSURF_FORMAT Format,
    IN OUT gctUINT32_PTR Width,
    IN OUT gctUINT32_PTR Height,
    OUT gctBOOL_PTR SuperTiled
    );

/* Align size compatible for all core in hardware. */
gceSTATUS
gcoHARDWARE_AlignToTileCompatible(
    IN gceSURF_TYPE Type,
    IN gceSURF_FORMAT Format,
    IN OUT gctUINT32_PTR Width,
    IN OUT gctUINT32_PTR Height,
    OUT gceTILING * Tiling,
    OUT gctBOOL_PTR SuperTiled
    );

gceSTATUS
gcoHARDWARE_QueryTileAlignment(
    IN gceSURF_TYPE Type,
    IN gceSURF_FORMAT Format,
    OUT gceSURF_ALIGNMENT * HAlignment,
    OUT gceSURF_ALIGNMENT * VAlignment
    );

gceSTATUS
gcoHARDWARE_SetDepthOnly(
    IN gctBOOL Enable
    );

gceSTATUS
gcoHARDWARE_QueryAlphaBlend(
    );


/*----------------------------------------------------------------------------*/
/*------------------------------- gcoHARDWARE 2D ------------------------------*/

/* Verifies whether 2D engine is available. */
gceSTATUS
gcoHARDWARE_Is2DAvailable(
    );

/* Translate API destination color format to its hardware value. */
gceSTATUS
gcoHARDWARE_TranslateDestinationFormat(
    IN gceSURF_FORMAT APIValue,
    OUT gctUINT32* HwValue,
    OUT gctUINT32* HwSwizzleValue,
    OUT gctUINT32* HwIsYUVValue
    );

#ifndef VIVANTE_NO_3D
gceSTATUS
gcoHARDWARE_SetStream(
    IN gctUINT32 Index,
    IN gctUINT32 Address,
    IN gctUINT32 Stride
    );

gceSTATUS
gcoHARDWARE_SetAttributes(
    IN gcsVERTEX_ATTRIBUTES_PTR Attributes,
    IN gctUINT32 AttributeCount
    );

/*----------------------------------------------------------------------------*/
/*----------------------- gcoHARDWARE Fragment Processor ---------------------*/

/* Set the fragment processor configuration. */
gceSTATUS
gcoHARDWARE_SetFragmentConfiguration(
    IN gctBOOL ColorFromStream,
    IN gctBOOL EnableFog,
    IN gctBOOL EnableSmoothPoint,
    IN gctUINT32 ClipPlanes
    );

/* Enable/disable texture stage operation. */
gceSTATUS
gcoHARDWARE_EnableTextureStage(
    IN gctINT Stage,
    IN gctBOOL Enable
    );

/* Program the channel enable masks for the color texture function. */
gceSTATUS
gcoHARDWARE_SetTextureColorMask(
    IN gctINT Stage,
    IN gctBOOL ColorEnabled,
    IN gctBOOL AlphaEnabled
    );

/* Program the channel enable masks for the alpha texture function. */
gceSTATUS
gcoHARDWARE_SetTextureAlphaMask(
    IN gctINT Stage,
    IN gctBOOL ColorEnabled,
    IN gctBOOL AlphaEnabled
    );

/* Program the constant fragment color. */
gceSTATUS
gcoHARDWARE_SetFragmentColorX(
    IN gctFIXED_POINT Red,
    IN gctFIXED_POINT Green,
    IN gctFIXED_POINT Blue,
    IN gctFIXED_POINT Alpha
    );

gceSTATUS
gcoHARDWARE_SetFragmentColorF(
    IN gctFLOAT Red,
    IN gctFLOAT Green,
    IN gctFLOAT Blue,
    IN gctFLOAT Alpha
    );

/* Program the constant fog color. */
gceSTATUS
gcoHARDWARE_SetFogColorX(
    IN gctFIXED_POINT Red,
    IN gctFIXED_POINT Green,
    IN gctFIXED_POINT Blue,
    IN gctFIXED_POINT Alpha
    );

gceSTATUS
gcoHARDWARE_SetFogColorF(
    IN gcoHARDWARE Hardware,
    IN gctFLOAT Red,
    IN gctFLOAT Green,
    IN gctFLOAT Blue,
    IN gctFLOAT Alpha
    );

/* Program the constant texture color. */
gceSTATUS
gcoHARDWARE_SetTetxureColorX(
    IN gctINT Stage,
    IN gctFIXED_POINT Red,
    IN gctFIXED_POINT Green,
    IN gctFIXED_POINT Blue,
    IN gctFIXED_POINT Alpha
    );

gceSTATUS
gcoHARDWARE_SetTetxureColorF(
    IN gctINT Stage,
    IN gctFLOAT Red,
    IN gctFLOAT Green,
    IN gctFLOAT Blue,
    IN gctFLOAT Alpha
    );

/* Configure color texture function. */
gceSTATUS
gcoHARDWARE_SetColorTextureFunction(
    IN gctINT Stage,
    IN gceTEXTURE_FUNCTION Function,
    IN gceTEXTURE_SOURCE Source0,
    IN gceTEXTURE_CHANNEL Channel0,
    IN gceTEXTURE_SOURCE Source1,
    IN gceTEXTURE_CHANNEL Channel1,
    IN gceTEXTURE_SOURCE Source2,
    IN gceTEXTURE_CHANNEL Channel2,
    IN gctINT Scale
    );

/* Configure alpha texture function. */
gceSTATUS
gcoHARDWARE_SetAlphaTextureFunction(
    IN gctINT Stage,
    IN gceTEXTURE_FUNCTION Function,
    IN gceTEXTURE_SOURCE Source0,
    IN gceTEXTURE_CHANNEL Channel0,
    IN gceTEXTURE_SOURCE Source1,
    IN gceTEXTURE_CHANNEL Channel1,
    IN gceTEXTURE_SOURCE Source2,
    IN gceTEXTURE_CHANNEL Channel2,
    IN gctINT Scale
    );

/* Flush the evrtex caches. */
gceSTATUS
gcoHARDWARE_FlushVertex(
    );

#if gcdUSE_WCLIP_PATCH
gceSTATUS
gcoHARDWARE_SetWClipEnable(
    IN gctBOOL Enable
    );

gceSTATUS
gcoHARDWARE_SetWPlaneLimit(
    IN gcoHARDWARE Hardware,
    IN gctFLOAT Value
    );
#endif

/* Draw a number of primitives. */
gceSTATUS
gcoHARDWARE_DrawPrimitives(
    IN gcePRIMITIVE Type,
    IN gctINT StartVertex,
    IN gctSIZE_T PrimitiveCount
    );

gceSTATUS
gcoHARDWARE_DrawPrimitivesCount(
    IN gcePRIMITIVE Type,
    IN gctINT* StartVertex,
    IN gctSIZE_T* VertexCount,
    IN gctSIZE_T PrimitiveCount
    );

/* Draw a number of primitives using offsets. */
gceSTATUS
gcoHARDWARE_DrawPrimitivesOffset(
    IN gcePRIMITIVE Type,
    IN gctINT32 StartOffset,
    IN gctSIZE_T PrimitiveCount
    );

/* Draw a number of indexed primitives. */
gceSTATUS
gcoHARDWARE_DrawIndexedPrimitives(
    IN gcePRIMITIVE Type,
    IN gctINT BaseVertex,
    IN gctINT StartIndex,
    IN gctSIZE_T PrimitiveCount
    );

/* Draw a number of indexed primitives using offsets. */
gceSTATUS
gcoHARDWARE_DrawIndexedPrimitivesOffset(
    IN gcePRIMITIVE Type,
    IN gctINT32 BaseOffset,
    IN gctINT32 StartOffset,
    IN gctSIZE_T PrimitiveCount
    );

/* Flush the texture cache. */
gceSTATUS
gcoHARDWARE_FlushTexture(
    );

/* Disable a specific texture sampler. */
gceSTATUS
gcoHARDWARE_DisableTextureSampler(
    IN gctINT Sampler
    );

/* Set sampler registers. */
gceSTATUS
gcoHARDWARE_BindTexture(
    IN gctINT Sampler,
    IN gcsSAMPLER_PTR SamplerInfo
    );


/* Query the index capabilities. */
gceSTATUS
gcoHARDWARE_QueryIndexCaps(
    OUT gctBOOL * Index8,
    OUT gctBOOL * Index16,
    OUT gctBOOL * Index32,
    OUT gctUINT * MaxIndex
    );

/* Query the target capabilities. */
gceSTATUS
gcoHARDWARE_QueryTargetCaps(
    OUT gctUINT * MaxWidth,
    OUT gctUINT * MaxHeight,
    OUT gctUINT * MultiTargetCount,
    OUT gctUINT * MaxSamples
    );

/* Query the texture capabilities. */
gceSTATUS
gcoHARDWARE_QueryTextureCaps(
    OUT gctUINT * MaxWidth,
    OUT gctUINT * MaxHeight,
    OUT gctUINT * MaxDepth,
    OUT gctBOOL * Cubic,
    OUT gctBOOL * NonPowerOfTwo,
    OUT gctUINT * VertexSamplers,
    OUT gctUINT * PixelSamplers,
    OUT gctUINT * MaxAnisoValue
    );

/* Query the shader support. */
gceSTATUS
gcoHARDWARE_QueryShaderCaps(
    OUT gctUINT * VertexUniforms,
    OUT gctUINT * FragmentUniforms,
    OUT gctUINT * Varyings,
    OUT gctUINT * ShaderCoreCount,
    OUT gctUINT * ThreadCount,
    OUT gctUINT * VertexInstructionCount,
    OUT gctUINT * FragmentInstructionCount
    );

/* Query the shader support. */
gceSTATUS
gcoHARDWARE_QueryShaderCapsEx(
    OUT gctUINT64 * LocalMemSize,
    OUT gctUINT * AddressBits,
    OUT gctUINT * GlobalMemCachelineSize,
    OUT gctUINT * GlobalMemCacheSize,
    OUT gctUINT * ClockFrequency
    );

/* Query the texture support. */
gceSTATUS
gcoHARDWARE_QueryTexture(
    IN gceSURF_FORMAT Format,
    IN gceTILING Tiling,
    IN gctUINT Level,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctUINT Depth,
    IN gctUINT Faces,
    OUT gctUINT * WidthAlignment,
    OUT gctUINT * HeightAlignment
    );

/* Query the stream capabilities. */
gceSTATUS
gcoHARDWARE_QueryStreamCaps(
    OUT gctUINT * MaxAttributes,
    OUT gctUINT * MaxStreamSize,
    OUT gctUINT * NumberOfStreams,
    OUT gctUINT * Alignment
    );

gceSTATUS
gcoHARDWARE_GetClosestTextureFormat(
    IN gceSURF_FORMAT InFormat,
    OUT gceSURF_FORMAT* OutFormat
    );

/* Upload data into a texture. */
gceSTATUS
gcoHARDWARE_UploadTexture(
    IN gceSURF_FORMAT TargetFormat,
    IN gctUINT32 Address,
    IN gctPOINTER Logical,
    IN gctUINT32 Offset,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride,
    IN gceSURF_FORMAT SourceFormat
    );

gceSTATUS
gcoHARDWARE_UploadCompressedTexture(
    IN gcsSURF_INFO_PTR TargetInfo,
    IN gctCONST_POINTER Logical,
    IN gctUINT32 Offset,
    IN gctUINT32 XOffset,
    IN gctUINT32 YOffset,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctUINT ImageSize
    );

/* Query if the format of surface is renderable or not. */
gceSTATUS
gcoHARDWARE_IsSurfaceFormatRenderable(
    IN gcsSURF_INFO_PTR Surface
    );

/* Query if a surface is renderable or not. */
gceSTATUS
gcoHARDWARE_IsSurfaceRenderable(
    IN gcsSURF_INFO_PTR Surface
    );

#if gcdSYNC
gceSTATUS
gcoHARDWARE_ConstructFence(
     IN gcoHARDWARE Hardware,
     IN OUT gcoFENCE * Fence
     );

gceSTATUS
gcoHARDWARE_DestroyFence(gcoFENCE fence);

gceSTATUS
gcoHARDWARE_WaitFence(
    IN gcsSYNC_CONTEXT_PTR ctx
    );

gceSTATUS
gcoHARDWARE_SendFenceByCommand();

gceSTATUS
gcoHARDWARE_SendFenceByCommit();

gceSTATUS
gcoHARDWARE_SendFence();

gceSTATUS
gcoHARDWARE_GetFence(
    IN gcsSYNC_CONTEXT_PTR *ctx
    );

#endif

#endif /* VIVANTE_NO_3D */

/* Copy data into video memory. */
gceSTATUS
gcoHARDWARE_CopyData(
    IN gcsSURF_NODE_PTR Memory,
    IN gctUINT32 Offset,
    IN gctCONST_POINTER Buffer,
    IN gctSIZE_T Bytes
    );

/* Sets the software 2D renderer force flag. */
gceSTATUS
gcoHARDWARE_UseSoftware2D(
    IN gctBOOL Enable
    );

/* Clear one or more rectangular areas. */
gceSTATUS
gcoHARDWARE_Clear2D(
    IN gcs2D_State_PTR State,
    IN gctUINT32 RectCount,
    IN gcsRECT_PTR Rect
    );

/* Draw one or more Bresenham lines using solid color(s). */
gceSTATUS
gcoHARDWARE_Line2DEx(
    IN gcs2D_State_PTR State,
    IN gctUINT32 LineCount,
    IN gcsRECT_PTR Position,
    IN gctUINT32 ColorCount,
    IN gctUINT32_PTR Color32
    );

/* Determines the usage of 2D resources (source/pattern/destination). */
void
gcoHARDWARE_Get2DResourceUsage(
    IN gctUINT8 FgRop,
    IN gctUINT8 BgRop,
    IN gce2D_TRANSPARENCY Transparency,
    OUT gctBOOL_PTR UseSource,
    OUT gctBOOL_PTR UsePattern,
    OUT gctBOOL_PTR UseDestination
    );

/* Translate SURF API transparency mode to PE 2.0 transparency values. */
gceSTATUS
gcoHARDWARE_TranslateSurfTransparency(
    IN gceSURF_TRANSPARENCY APIValue,
    OUT gce2D_TRANSPARENCY* srcTransparency,
    OUT gce2D_TRANSPARENCY* dstTransparency,
    OUT gce2D_TRANSPARENCY* patTransparency
    );

gceSTATUS
gcoHARDWARE_ColorPackToARGB8(
    IN gceSURF_FORMAT Format,
    IN gctUINT32 Color,
    OUT gctUINT32_PTR Color32
    );

gceSTATUS
gcoHARDWARE_SetDither(
    IN gctBOOL Enable
    );

/* Calculate stretch factor. */
gctUINT32
gcoHARDWARE_GetStretchFactor(
    IN gctINT32 SrcSize,
    IN gctINT32 DestSize
    );

/* Calculate the stretch factors. */
gceSTATUS
gcoHARDWARE_GetStretchFactors(
    IN gcsRECT_PTR SrcRect,
    IN gcsRECT_PTR DestRect,
    OUT gctUINT32 * HorFactor,
    OUT gctUINT32 * VerFactor
    );

gceSTATUS gcoHARDWARE_SetGDIStretch(
    IN gctBOOL Enable
    );

/* Start a DE command. */
gceSTATUS
gcoHARDWARE_StartDE(
    IN gcs2D_State_PTR State,
    IN gce2D_COMMAND Command,
    IN gctUINT32 SrcRectCount,
    IN gcsRECT_PTR SrcRect,
    IN gctUINT32 DestRectCount,
    IN gcsRECT_PTR DestRect
    );

/* Start a DE command to draw one or more Lines,
   with a common or individual color. */
gceSTATUS
gcoHARDWARE_StartDELine(
    IN gcs2D_State_PTR State,
    IN gce2D_COMMAND Command,
    IN gctUINT32 RectCount,
    IN gcsRECT_PTR DestRect,
    IN gctUINT32 ColorCount,
    IN gctUINT32_PTR Color32
    );

/* Start a DE command with a monochrome stream. */
gceSTATUS
gcoHARDWARE_StartDEStream(
    IN gcs2D_State_PTR State,
    IN gcsRECT_PTR DestRect,
    IN gctUINT32 StreamSize,
    OUT gctPOINTER * StreamBits
    );

/* Frees the temporary buffer allocated by filter blit operation. */
gceSTATUS
gcoHARDWARE_FreeFilterBuffer(
    );

/* Filter blit. */
gceSTATUS
gcoHARDWARE_FilterBlit(
    IN gcs2D_State_PTR State,
    IN gcsSURF_INFO_PTR SrcSurface,
    IN gcsSURF_INFO_PTR DestSurface,
    IN gcsRECT_PTR SrcRect,
    IN gcsRECT_PTR DestRect,
    IN gcsRECT_PTR DestSubRect
    );

gceSTATUS gcoHARDWARE_MultiPlanarYUVConvert(
    IN gcsSURF_INFO_PTR SrcSurface,
    IN gcsSURF_INFO_PTR DestSurface,
    IN gcsRECT_PTR DestRect
    );

/* Monochrome blit. */
gceSTATUS
gcoHARDWARE_MonoBlit(
    IN gcs2D_State_PTR State,
    IN gctPOINTER StreamBits,
    IN gcsPOINT_PTR StreamSize,
    IN gcsRECT_PTR StreamRect,
    IN gceSURF_MONOPACK SrcStreamPack,
    IN gceSURF_MONOPACK DestStreamPack,
    IN gcsRECT_PTR DestRect
    );

/* Set the GPU clock cycles, after which the idle 2D engine
   will trigger a flush. */
gceSTATUS
gcoHARDWARE_SetAutoFlushCycles(
    IN gctUINT32 Cycles
    );

#ifndef VIVANTE_NO_3D
/*----------------------------------------------------------------------------*/
/*------------------------------- gcoHARDWARE 3D ------------------------------*/

gceSTATUS
gcoHARDWARE_BindIndex(
    IN gctUINT32 Address,
    IN gceINDEX_TYPE IndexType
    );

/* Initialize the 3D hardware. */
gceSTATUS
gcoHARDWARE_Initialize3D(
    );

gceSTATUS
gcoHARDWARE_SetRenderTarget(
    IN gcsSURF_INFO_PTR Surface
    );

gceSTATUS
gcoHARDWARE_SetDepthBuffer(
    IN gcsSURF_INFO_PTR Surface
    );

gceSTATUS
gcoHARDWARE_SetAPI(
    IN gceAPI Api
    );

/* Clear wrapper to distinguish between software and resolve(3d) clear cases. */
gceSTATUS
gcoHARDWARE_ClearRect(
    IN gctUINT32 Address,
    IN gctPOINTER LogicalAddress,
    IN gctUINT32 Stride,
    IN gctINT Left,
    IN gctINT Top,
    IN gctINT Right,
    IN gctINT Bottom,
    IN gceSURF_FORMAT Format,
    IN gctUINT32 ClearValue,
    IN gctUINT8 ClearMask,
    IN gctUINT32 AlignedWidth,
    IN gctUINT32 AlignedHeight
    );

/* Append a TILE STATUS CLEAR command to a command queue. */
gceSTATUS
gcoHARDWARE_ClearTileStatus(
    IN gcsSURF_INFO_PTR Surface,
    IN gctUINT32 Address,
    IN gctSIZE_T Bytes,
    IN gceSURF_TYPE Type,
    IN gctUINT32 ClearValue,
    IN gctUINT8 ClearMask
    );

gceSTATUS
gcoHARDWARE_ComputeClearWindow(
    IN gctSIZE_T Bytes,
    gctUINT32 *Width,
    gctUINT32 *Height
    );


gceSTATUS
gcoHARDWARE_SetViewport(
    IN gctINT32 Left,
    IN gctINT32 Top,
    IN gctINT32 Right,
    IN gctINT32 Bottom
    );

gceSTATUS
gcoHARDWARE_SetScissors(
    IN gctINT32 Left,
    IN gctINT32 Top,
    IN gctINT32 Right,
    IN gctINT32 Bottom
    );

gceSTATUS
gcoHARDWARE_SetShading(
    IN gceSHADING Shading
    );

gceSTATUS
gcoHARDWARE_SetBlendEnable(
    IN gctBOOL Enabled
    );

gceSTATUS
gcoHARDWARE_SetBlendFunctionSource(
    IN gceBLEND_FUNCTION FunctionRGB,
    IN gceBLEND_FUNCTION FunctionAlpha
    );

gceSTATUS
gcoHARDWARE_SetBlendFunctionTarget(
    IN gceBLEND_FUNCTION FunctionRGB,
    IN gceBLEND_FUNCTION FunctionAlpha
    );

gceSTATUS
gcoHARDWARE_SetBlendMode(
    IN gceBLEND_MODE ModeRGB,
    IN gceBLEND_MODE ModeAlpha
    );

gceSTATUS
gcoHARDWARE_SetBlendColor(
    IN gctUINT8 Red,
    IN gctUINT8 Green,
    IN gctUINT8 Blue,
    IN gctUINT8 Alpha
    );

gceSTATUS
gcoHARDWARE_SetBlendColorX(
    IN gctFIXED_POINT Red,
    IN gctFIXED_POINT Green,
    IN gctFIXED_POINT Blue,
    IN gctFIXED_POINT Alpha
    );

gceSTATUS
gcoHARDWARE_SetBlendColorF(
    IN gctFLOAT Red,
    IN gctFLOAT Green,
    IN gctFLOAT Blue,
    IN gctFLOAT Alpha
    );

gceSTATUS
gcoHARDWARE_SetCulling(
    IN gceCULL Mode
    );

gceSTATUS
gcoHARDWARE_SetPointSizeEnable(
    IN gctBOOL Enable
    );

gceSTATUS
gcoHARDWARE_SetPointSprite(
    IN gctBOOL Enable
    );

gceSTATUS
gcoHARDWARE_SetFill(
    IN gceFILL Mode
    );

gceSTATUS
gcoHARDWARE_SetDepthCompare(
    IN gceCOMPARE DepthCompare
    );

gceSTATUS
gcoHARDWARE_SetDepthWrite(
    IN gctBOOL Enable
    );

gceSTATUS
gcoHARDWARE_SetDepthMode(
    IN gceDEPTH_MODE DepthMode
    );

gceSTATUS
gcoHARDWARE_SetDepthRangeX(
    IN gceDEPTH_MODE DepthMode,
    IN gctFIXED_POINT Near,
    IN gctFIXED_POINT Far
    );

gceSTATUS
gcoHARDWARE_SetDepthRangeF(
    IN gceDEPTH_MODE DepthMode,
    IN gctFLOAT Near,
    IN gctFLOAT Far
    );

gceSTATUS
gcoHARDWARE_SetDepthScaleBiasX(
    IN gctFIXED_POINT DepthScale,
    IN gctFIXED_POINT DepthBias
    );

gceSTATUS
gcoHARDWARE_SetDepthScaleBiasF(
    IN gctFLOAT DepthScale,
    IN gctFLOAT DepthBias
    );

gceSTATUS
gcoHARDWARE_SetDepthPlaneF(
    IN gctFLOAT Near,
    IN gctFLOAT Far
    );

gceSTATUS
gcoHARDWARE_SetColorWrite(
    IN gctUINT8 Enable
    );

gceSTATUS
gcoHARDWARE_SetEarlyDepth(
    IN gctBOOL Enable
    );

gceSTATUS
gcoHARDWARE_SetAllEarlyDepthModes(
    IN gctBOOL Disable
    );

gceSTATUS
gcoHARDWARE_SwitchDynamicEarlyDepthMode(
    );

gceSTATUS
gcoHARDWARE_DisableDynamicEarlyDepthMode(
    IN gctBOOL Disable
    );

gceSTATUS
gcoHARDWARE_SetStencilMode(
    IN gceSTENCIL_MODE Mode
    );

gceSTATUS
gcoHARDWARE_SetStencilMask(
    IN gctUINT8 Mask
    );

gceSTATUS
gcoHARDWARE_SetStencilMaskBack(
    IN gctUINT8 Mask
    );

gceSTATUS
gcoHARDWARE_SetStencilWriteMask(
    IN gctUINT8 Mask
    );

gceSTATUS
gcoHARDWARE_SetStencilWriteMaskBack(
    IN gctUINT8 Mask
    );

gceSTATUS
gcoHARDWARE_SetStencilReference(
    IN gctUINT8 Reference,
    IN gctBOOL  Front
    );

gceSTATUS
gcoHARDWARE_SetStencilCompare(
    IN gceSTENCIL_WHERE Where,
    IN gceCOMPARE Compare
    );

gceSTATUS
gcoHARDWARE_SetStencilPass(
    IN gceSTENCIL_WHERE Where,
    IN gceSTENCIL_OPERATION Operation
    );

gceSTATUS
gcoHARDWARE_SetStencilFail(
    IN gceSTENCIL_WHERE Where,
    IN gceSTENCIL_OPERATION Operation
    );

gceSTATUS
gcoHARDWARE_SetStencilDepthFail(
    IN gceSTENCIL_WHERE Where,
    IN gceSTENCIL_OPERATION Operation
    );

gceSTATUS
gcoHARDWARE_SetStencilAll(
    IN gcsSTENCIL_INFO_PTR Info
    );

gceSTATUS
gcoHARDWARE_SetAlphaTest(
    IN gctBOOL Enable
    );

gceSTATUS
gcoHARDWARE_SetAlphaCompare(
    IN gceCOMPARE Compare
    );

gceSTATUS
gcoHARDWARE_SetAlphaReference(
    IN gctUINT8 Reference
    );

gceSTATUS
gcoHARDWARE_SetAlphaReferenceX(
    IN gctFIXED_POINT Reference
    );

gceSTATUS
gcoHARDWARE_SetAlphaReferenceF(
    IN gctFLOAT Reference
    );

gceSTATUS
gcoHARDWARE_SetAlphaAll(
    IN gcsALPHA_INFO_PTR Info
    );

gceSTATUS
gcoHARDWARE_SetAntiAliasLine(
    IN gctBOOL Enable
    );

gceSTATUS
gcoHARDWARE_SetAALineTexSlot(
    IN gctUINT TexSlot
    );

gceSTATUS
gcoHARDWARE_SetAALineWidth(
    IN gctFLOAT Width
    );

/* Append a CLEAR command to a command queue. */
gceSTATUS
gcoHARDWARE_Clear(
    IN gctUINT32 Address,
    IN gctUINT32 Stride,
    IN gctINT32 Left,
    IN gctINT32 Top,
    IN gctINT32 Right,
    IN gctINT32 Bottom,
    IN gceSURF_FORMAT Format,
    IN gctUINT32 ClearValue,
    IN gctUINT8 ClearMask,
    IN gctUINT32 AlignedWidth,
    IN gctUINT32 AlignedHeight
    );

gceSTATUS
gcoHARDWARE_SetLastPixelEnable(
    IN gctBOOL Enable
    );

gceSTATUS
gcoHARDWARE_SetLogicOp(
    IN gctUINT8     Rop
    );

gceSTATUS
gcoHARDWARE_SetCentroids(
    IN gctUINT32    Index,
    IN gctPOINTER   Centroids
    );

gceSTATUS
gcoHARDWARE_QuerySamplerBase(
    OUT gctSIZE_T * VertexCount,
    OUT gctINT_PTR VertexBase,
    OUT gctSIZE_T * FragmentCount,
    OUT gctINT_PTR FragmentBase
    );
#endif /* VIVANTE_NO_3D */
/******************************************************************************\
******************************** gcoBUFFER Object *******************************
\******************************************************************************/

/* Construct a new gcoBUFFER object. */
gceSTATUS
gcoBUFFER_Construct(
    IN gcoHAL Hal,
    IN gcoHARDWARE Hardware,
    IN gckCONTEXT Context,
    IN gctSIZE_T MaxSize,
    OUT gcoBUFFER * Buffer
    );

/* Destroy an gcoBUFFER object. */
gceSTATUS
gcoBUFFER_Destroy(
    IN gcoBUFFER Buffer
    );

/* Reserve space in a command buffer. */
gceSTATUS
gcoBUFFER_Reserve(
    IN gcoBUFFER Buffer,
    IN gctSIZE_T Bytes,
    IN gctBOOL Aligned,
    OUT gcoCMDBUF * Reserve
    );

/* Write data into the command buffer. */
gceSTATUS
gcoBUFFER_Write(
    IN gcoBUFFER Buffer,
    IN gctCONST_POINTER Data,
    IN gctSIZE_T Bytes,
    IN gctBOOL Aligned
    );

/* Doesn't have implementation */
/* Write 32-bit data into the command buffer. */
gceSTATUS
gcoBUFFER_Write32(
    IN gcoBUFFER Buffer,
    IN gctUINT32 Data,
    IN gctBOOL Aligned
    );
/* Doesn't have implementation */
/* Write 64-bit data into the command buffer. */
gceSTATUS
gcoBUFFER_Write64(
    IN gcoBUFFER Buffer,
    IN gctUINT64 Data,
    IN gctBOOL Aligned
    );

/* Commit the command buffer. */
gceSTATUS
gcoBUFFER_Commit(
    IN gcoBUFFER Buffer,
    IN gcePIPE_SELECT CurrentPipe,
    IN gcsSTATE_DELTA_PTR StateDelta,
    IN gcoQUEUE Queue
    );

/******************************************************************************\
******************************** gcoCMDBUF Object *******************************
\******************************************************************************/

typedef struct _gcsCOMMAND_INFO * gcsCOMMAND_INFO_PTR;

/* Construct a new gcoCMDBUF object. */
gceSTATUS
gcoCMDBUF_Construct(
    IN gcoOS Os,
    IN gcoHARDWARE Hardware,
    IN gctSIZE_T Bytes,
    IN gcsCOMMAND_INFO_PTR Info,
    OUT gcoCMDBUF * Buffer
    );

/* Destroy an gcoCMDBUF object. */
gceSTATUS
gcoCMDBUF_Destroy(
    IN gcoCMDBUF Buffer
    );

#ifdef LINUX
#define PENDING_FREED_MEMORY_SIZE_LIMIT     (4 * 1024 * 1024)
#endif

#if gcdSYNC
typedef enum _gceFENCE_STATUS
{
    gcvFENCE_DISABLE,
    gcvFENCE_GET,
    gcvFENCE_ENABLE,
}
gceFENCE_STATUS;
#endif

/******************************************************************************\
********************************* gcoHAL object *********************************
\******************************************************************************/

struct _gcoHAL
{
    gcsOBJECT               object;

    gcoDUMP                 dump;

#if VIVANTE_PROFILER
    gcsPROFILER             profiler;
#endif

    gcsATOM_PTR             reference;

#if gcdFRAME_DB
    gctINT                  frameDBIndex;
    gcsHAL_FRAME_INFO       frameDB[gcdFRAME_DB];
#endif

    gctINT32                chipCount;
    gceHARDWARE_TYPE        chipTypes[gcdCHIP_COUNT];
    gctBOOL                 separated3D2D;

    gctUINT32               timeOut;
    gcsSTATISTICS           statistics;
    gcsUSER_DEBUG_OPTION    *userDebugOption;

    gcsAPIBENCH             apiBench;
};


/******************************************************************************\
********************************* gcoSURF object ********************************
\******************************************************************************/
#if gcdSYNC
typedef struct _gcsSYNC_CONTEXT
{
    gctUINT64               fenceID;
    gctUINT32               oqSlot;
    gctPOINTER              fence;
    gctBOOL                 mark;
    gcsSYNC_CONTEXT_PTR     next;
}
gcsSYNC_CONTEXT;
#endif

typedef struct _gcsSURF_NODE
{
    /* Surface memory pool. */
    gcePOOL                 pool;

    /* Lock count for the surface. */
    gctINT                  lockCount;

    /* If not zero, the node is locked in the kernel. */
    gctBOOL                 lockedInKernel;

    /* Locked hardware type */
    gceHARDWARE_TYPE        lockedHardwareType;

    /* Number of planes in the surface for planar format support. */
    gctUINT                 count;

    /* Node valid flag for the surface pointers. */
    gctBOOL                 valid;

    /* The physical addresses of the surface. */
    gctUINT32               physical;
    gctUINT32               physical2;
    gctUINT32               physical3;

    /* The logical addresses of the surface. */
    gctUINT8_PTR            logical;
    gctUINT8_PTR            logical2;
    gctUINT8_PTR            logical3;

    /* Linear size and filler for tile status. */
    gctSIZE_T               size;
    gctUINT32               filler;
    gctBOOL                 firstLock;

    union _gcuSURF_NODE_LIST
    {
        /* Allocated through HAL. */
        struct _gcsMEM_NODE_NORMAL
        {
            gcuVIDMEM_NODE_PTR  node;
#ifdef __QNXNTO__
            gctUINT32           bytes;
#endif
            gctBOOL             cacheable;
        }
        normal;

        /* Wrapped around user-allocated surface (gcvPOOL_USER). */
        struct _gcsMEM_NODE_WRAPPED
        {
            gctBOOL             logicalMapped;
            gctPOINTER          mappingInfo;
            gceHARDWARE_TYPE    mappingHardwareType;
        }
        wrapped;
    }
    u;
}
gcsSURF_NODE;

typedef struct _gcsSURF_INFO
{
    /* Type usage and format of surface. */
    gceSURF_TYPE            type;
    gceSURF_TYPE            hints;
    gceSURF_FORMAT          format;
    gceTILING               tiling;

    /* Surface size. */
    gcsRECT                 rect;
    gctUINT                 alignedWidth;
    gctUINT                 alignedHeight;
    gctBOOL                 is16Bit;

    /* Rotation flag. */
    gceSURF_ROTATION        rotation;
    gceORIENTATION          orientation;

    /* Dither flag. */
    gctBOOL                 dither;

    /* Surface stride and size. */
    gctUINT                 stride;
    gctUINT                 size;
    gctBOOL                 dirty;

    /* YUV planar surface parameters. */
    gctUINT                 uOffset;
    gctUINT                 vOffset;
    gctUINT                 uStride;
    gctUINT                 vStride;

    /* Video memory node for surface. */
    gcsSURF_NODE            node;

        /* Surface color type. */
    gceSURF_COLOR_TYPE      colorType;

#ifndef VIVANTE_NO_3D
    /* Samples. */
    gcsSAMPLES              samples;
    gctBOOL                 vaa;

    /* Tile status. */
    gctBOOL                 tileStatusDisabled;
    gctBOOL                 superTiled;
    gctUINT32               clearValue;

    /* Hierarchical Z buffer pointer. */
    gcsSURF_NODE            hzNode;
    gctUINT32               clearValueHz;

    /* Video memory node for tile status. */
    gcsSURF_NODE            tileStatusNode;
    gcsSURF_NODE            hzTileStatusNode;
    gctBOOL                 TSDirty;

#if gcdANDROID_UNALIGNED_LINEAR_COMPOSITION_ADJUST
    /* resolve physical and logical address, Only for 3D composition*/
    gctUINT32               linearResolvePhysical;
    gctUINT8_PTR            linearResolveLogical;
#endif

#endif /* VIVANTE_NO_3D */

    gctUINT                 offset;

    gceSURF_USAGE           usage;

    /* 2D related members. */
    gce2D_TILE_STATUS_CONFIG    tileStatusConfig;
    gceSURF_FORMAT              tileStatusFormat;
    gctUINT32                   tileStatusClearValue;
    gctUINT32                   tileStatusGpuAddress;

#if gcdSYNC
    gceFENCE_STATUS             fenceStatus;
    gcsSYNC_CONTEXT_PTR         fenceCtx;
#endif
}
gcsSURF_INFO;

struct _gcoSURF
{
    /* Object. */
    gcsOBJECT               object;

    /* Surface information structure. */
    struct _gcsSURF_INFO    info;

    /* Depth of the surface in planes. */
    gctUINT                 depth;

#ifndef VIVANTE_NO_3D
    gctBOOL                 resolvable;

#if defined(ANDROID)
    /* Used for 3D app - SF sync. */
    gctSIGNAL               resolveSubmittedSignal;
#endif
#endif /* VIVANTE_NO_3D */

    /* Automatic stride calculation. */
    gctBOOL                 autoStride;

    /* User pointers for the surface wrapper. */
    gctPOINTER              logical;
    gctUINT32               physical;

    /* Reference count of surface. */
    gctINT32                referenceCount;
};

#define gcdMULTI_SOURCE_NUM 8

typedef struct _gcs2D_MULTI_SOURCE
{
    gce2D_SOURCE srcType;
    gcsSURF_INFO srcSurface;
    gceSURF_MONOPACK srcMonoPack;
    gctUINT32 srcMonoTransparencyColor;
    gctBOOL   srcColorConvert;
    gctUINT32 srcFgColor;
    gctUINT32 srcBgColor;
    gctUINT32 srcColorKeyLow;
    gctUINT32 srcColorKeyHigh;
    gctBOOL srcRelativeCoord;
    gctBOOL srcStream;
    gcsRECT srcRect;
    gce2D_YUV_COLOR_MODE srcYUVMode;

    gce2D_TRANSPARENCY srcTransparency;
    gce2D_TRANSPARENCY dstTransparency;
    gce2D_TRANSPARENCY patTransparency;

    gctBOOL enableDFBColorKeyMode;

    gctUINT8 fgRop;
    gctUINT8 bgRop;

    gctBOOL enableAlpha;
    gceSURF_PIXEL_ALPHA_MODE  srcAlphaMode;
    gceSURF_PIXEL_ALPHA_MODE  dstAlphaMode;
    gceSURF_GLOBAL_ALPHA_MODE srcGlobalAlphaMode;
    gceSURF_GLOBAL_ALPHA_MODE dstGlobalAlphaMode;
    gceSURF_BLEND_FACTOR_MODE srcFactorMode;
    gceSURF_BLEND_FACTOR_MODE dstFactorMode;
    gceSURF_PIXEL_COLOR_MODE  srcColorMode;
    gceSURF_PIXEL_COLOR_MODE  dstColorMode;
    gce2D_PIXEL_COLOR_MULTIPLY_MODE srcPremultiplyMode;
    gce2D_PIXEL_COLOR_MULTIPLY_MODE dstPremultiplyMode;
    gce2D_GLOBAL_COLOR_MULTIPLY_MODE srcPremultiplyGlobalMode;
    gce2D_PIXEL_COLOR_MULTIPLY_MODE dstDemultiplyMode;
    gctUINT32 srcGlobalColor;
    gctUINT32 dstGlobalColor;

} gcs2D_MULTI_SOURCE, *gcs2D_MULTI_SOURCE_PTR;

/* FilterBlt information. */
#define gcvMAXKERNELSIZE        9
#define gcvSUBPIXELINDEXBITS    5

#define gcvSUBPIXELCOUNT \
    (1 << gcvSUBPIXELINDEXBITS)

#define gcvSUBPIXELLOADCOUNT \
    (gcvSUBPIXELCOUNT / 2 + 1)

#define gcvWEIGHTSTATECOUNT \
    (((gcvSUBPIXELLOADCOUNT * gcvMAXKERNELSIZE + 1) & ~1) / 2)

#define gcvKERNELTABLESIZE \
    (gcvSUBPIXELLOADCOUNT * gcvMAXKERNELSIZE * sizeof(gctUINT16))

#define gcvKERNELSTATES \
    (gcmALIGN(gcvKERNELTABLESIZE + 4, 8))

typedef struct _gcsFILTER_BLIT_ARRAY
{
    gceFILTER_TYPE              filterType;
    gctUINT8                    kernelSize;
    gctUINT32                   scaleFactor;
    gctBOOL                     kernelChanged;
    gctUINT32_PTR               kernelStates;
}
gcsFILTER_BLIT_ARRAY;

typedef gcsFILTER_BLIT_ARRAY *  gcsFILTER_BLIT_ARRAY_PTR;

typedef struct _gcs2D_State
{
    gctUINT             currentSrcIndex;
    gcs2D_MULTI_SOURCE  multiSrc[gcdMULTI_SOURCE_NUM];
    gctUINT32           srcMask;

    /* dest info. */
    gcsSURF_INFO dstSurface;
    gctUINT32    dstColorKeyLow;
    gctUINT32    dstColorKeyHigh;

    gcsRECT      clipRect;

    /* brush info. */
    gce2D_PATTERN brushType;
    gctUINT32 brushOriginX;
    gctUINT32 brushOriginY;
    gctUINT32 brushColorConvert;
    gctUINT32 brushFgColor;
    gctUINT32 brushBgColor;
    gctUINT64 brushBits;
    gctUINT64 brushMask;
    gctUINT32 brushAddress;
    gceSURF_FORMAT brushFormat;

    /* Stretch factors. */
    gctUINT32 horFactor;
    gctUINT32 verFactor;

    /* Mirror. */
    gctBOOL horMirror;
    gctBOOL verMirror;

    /* Dithering enabled. */
    gctBOOL dither;

    /* Clear color. */
    gctUINT32 clearColor;

    /* Palette Table for source. */
    gctUINT32  paletteIndexCount;
    gctUINT32  paletteFirstIndex;
    gctBOOL    paletteConvert;
    gctBOOL    paletteProgram;
    gctPOINTER paletteTable;
    gceSURF_FORMAT paletteConvertFormat;


    /* Filter blit. */
    gceFILTER_TYPE              newFilterType;
    gctUINT8                    newHorKernelSize;
    gctUINT8                    newVerKernelSize;

    gctBOOL                     horUserFilterPass;
    gctBOOL                     verUserFilterPass;

    gcsFILTER_BLIT_ARRAY        horSyncFilterKernel;
    gcsFILTER_BLIT_ARRAY        verSyncFilterKernel;

    gcsFILTER_BLIT_ARRAY        horBlurFilterKernel;
    gcsFILTER_BLIT_ARRAY        verBlurFilterKernel;

    gcsFILTER_BLIT_ARRAY        horUserFilterKernel;
    gcsFILTER_BLIT_ARRAY        verUserFilterKernel;

    gctBOOL                     specialFilterMirror;
#if MRVL_GCU_TUNING_OPF
    gctBOOL                     OPFBWConfigChanged;
    gctBOOL                     OPFBWConfigCustom;
    gctBOOL                     OPFBWLinearRotOpt;
    gctBOOL                     OPFBWBlockDirT2B;
    gctBOOL                     OPFBWPixelDirT2B;
    gctUINT32                   OPFBWBlockWSize;
    gctUINT32                   OPFBWBlockHSize;
#endif
} gcs2D_State;

/******************************************************************************\
******************************** gcoQUEUE Object *******************************
\******************************************************************************/

/* Construct a new gcoQUEUE object. */
gceSTATUS
gcoQUEUE_Construct(
    IN gcoOS Os,
    OUT gcoQUEUE * Queue
    );

/* Destroy a gcoQUEUE object. */
gceSTATUS
gcoQUEUE_Destroy(
    IN gcoQUEUE Queue
    );

/* Append an event to a gcoQUEUE object. */
gceSTATUS
gcoQUEUE_AppendEvent(
    IN gcoQUEUE Queue,
    IN gcsHAL_INTERFACE * Interface
    );

/* Commit and event queue. */
gceSTATUS
gcoQUEUE_Commit(
    IN gcoQUEUE Queue
    );

/* Commit and event queue. */
gceSTATUS
gcoQUEUE_Free(
    IN gcoQUEUE Queue
    );

#ifndef VIVANTE_NO_3D
/*******************************************************************************
***** Vertex Management *******************************************************/

typedef struct _gcsVERTEXARRAY_ATTRIBUTE    gcsVERTEXARRAY_ATTRIBUTE;
typedef struct _gcsVERTEXARRAY_ATTRIBUTE *  gcsVERTEXARRAY_ATTRIBUTE_PTR;

struct _gcsVERTEXARRAY_ATTRIBUTE
{
    /* Pointer to vertex information. */
    gcsVERTEXARRAY_PTR                      vertexPtr;

    /* Pointer to next attribute in a stream. */
    gcsVERTEXARRAY_ATTRIBUTE_PTR            next;

    /* Logical address of this attribute. */
    gctCONST_POINTER                        logical;

    /* Format of this attribute. */
    gceVERTEX_FORMAT                        format;

    /* Offset into the stream of this attribute. */
    gctUINT                                 offset;

    /* Number of bytes of this attribute. */
    gctUINT                                 bytes;
};

typedef struct _gcsSTREAM_SUBSTREAM    gcsSTREAM_SUBSTREAM;
typedef struct _gcsSTREAM_SUBSTREAM *  gcsSTREAM_SUBSTREAM_PTR;
struct _gcsSTREAM_SUBSTREAM
{
    /* Current range for the sub-stream. */
    gctUINT                                 start;
    gctUINT                                 end;

    /* Maximum range of the window for the sub-stream. */
    gctUINT                                 minStart;
    gctUINT                                 maxEnd;

    /* Stride for the sub-stream. */
    gctUINT                                 stride;

    /* Pointer to associated gcoSTREAM object. */
    gcoSTREAM                               stream;

    /* Pointer to next sub-stream. */
    gcsSTREAM_SUBSTREAM_PTR                 next;
};

typedef struct _gcsVERTEXARRAY_STREAM
{
    /* Pointer to gcoSTREAM object. */
    gcoSTREAM                               stream;

    /* Logical address of stream data. */
    gctUINT8_PTR                            logical;

    /* Physical address of the stream data. */
    gctUINT32                               physical;

    /* Pointer to first attribute part of this stream. */
    gcsVERTEXARRAY_ATTRIBUTE_PTR            attribute;

    /* Pointer to sub-streams. */
    gcsSTREAM_SUBSTREAM_PTR                 subStream;
}
gcsVERTEXARRAY_STREAM,
* gcsVERTEXARRAY_STREAM_PTR;

#if OPT_VERTEX_ARRAY
typedef struct _gcsVERTEXARRAY_BUFFER
{
    /* Attribute Maps */
    gctUINT                                 map[gcdATTRIBUTE_COUNT];
    gctUINT                                 count;

    /* Indicate a buffer is combined from multi-buffer. */
    gctBOOL                                 combined;

    /* Logical address of stream data. */
    gctUINT8_PTR                            start;
    gctUINT8_PTR                            end;

    gctUINT8_PTR                            minStart;
    gctUINT8_PTR                            maxEnd;

    /* Stride for the sub-buffer. */
    gctUINT                                 stride;

    /* Buffer offset in a stream. */
    gctUINT32                               offset;
}gcsVERTEXARRAY_BUFFER,
* gcsVERTEXARRAY_BUFFER_PTR;

#endif

gceSTATUS
gcoSTREAM_SetAttribute(
    IN gcoSTREAM Stream,
    IN gctUINT Offset,
    IN gctUINT Bytes,
    IN gctUINT Stride,
    IN OUT gcsSTREAM_SUBSTREAM_PTR * SubStream
    );

gceSTATUS
gcoSTREAM_QuerySubStreams(
    IN gcoSTREAM Stream,
    IN gcsSTREAM_SUBSTREAM_PTR SubStream,
    OUT gctUINT_PTR SubStreamCount
    );

gceSTATUS
gcoSTREAM_Rebuild(
    IN gcoSTREAM Stream,
    IN gctUINT First,
    IN gctUINT Count,
    OUT gctUINT_PTR SubStreamCount
    );

gceSTATUS
gcoSTREAM_SetCacheSize(
    IN gcoSTREAM Stream,
    IN gctUINT CacheSize
    );

gceSTATUS
gcoSTREAM_SetCache(
    IN gcoSTREAM Stream
    );

gceSTATUS
gcoSTREAM_SetIndexCacheSize(
    IN gcoINDEX Index,
    IN gctUINT32 IndexCacheSize
    );

#if OPT_VERTEX_ARRAY
gceSTATUS
gcoSTREAM_CacheAttributes(
    IN gcoSTREAM Stream,
    IN gctUINT First,
    IN gctUINT Count,
    IN gctUINT Bytes,
    IN gctUINT BufferCount,
    IN gcsVERTEXARRAY_BUFFER_PTR Buffers,
    IN gctUINT AttributeCount,
    IN gcsVERTEXARRAY_ATTRIBUTE_PTR Attributes,
    OUT gctUINT32_PTR Physical
    );
gceSTATUS
gcoSTREAM_UploadUnCacheableAttributes(
    IN gcoSTREAM Stream,
    IN gctUINT First,
    IN gctUINT Count,
    IN gctUINT Bytes,
    IN gctUINT BufferCount,
    IN gcsVERTEXARRAY_BUFFER_PTR Buffers,
    IN gctUINT AttributeCount,
    IN gcsVERTEXARRAY_ATTRIBUTE_PTR Attributes,
    OUT gctUINT32_PTR Physical,
    OUT gcoSTREAM * OutStream
);

gceSTATUS
gcoVERTEX_CheckStreamPool(
    IN gctUINT hwStreamCount,
    IN gctINT streamCount,
    IN gcsVERTEXARRAY_STREAM_PTR Streams,
    IN gcoSTREAM cacheStream,
    IN gctUINT32 copyPhysical,
    IN gcoSTREAM unCacheableStream
);

#else

gceSTATUS
gcoSTREAM_CacheAttributes(
    IN gcoSTREAM Stream,
    IN gctUINT First,
    IN gctUINT Count,
    IN gctUINT Stride,
    IN gctUINT AttributeCount,
    IN gcsVERTEXARRAY_ATTRIBUTE_PTR Attributes,
    OUT gctUINT32_PTR Physical
    );
#endif

gceSTATUS
gcoSTREAM_UnAlias(
    IN gcoSTREAM Stream,
    IN gcsVERTEXARRAY_ATTRIBUTE_PTR Attributes,
    OUT gcsSTREAM_SUBSTREAM_PTR * SubStream,
    OUT gctUINT8_PTR * Logical,
    OUT gctUINT32 * Physical
    );

#if gcdUSE_WCLIP_PATCH
gctSIZE_T
gcoSTREAM_GetSize(
        IN gcoSTREAM Stream
        );
#endif

#if OPT_VERTEX_ARRAY
gceSTATUS
gcoHARDWARE_SetVertexArray(
    IN gcoHARDWARE Hardware,
    IN gctUINT First,
    IN gctUINT32 Physical,
    IN gctUINT BufferCount,
    IN gcsVERTEXARRAY_BUFFER_PTR Buffers,
    IN gctUINT AttributeCount,
    IN gcsVERTEXARRAY_ATTRIBUTE_PTR Attributes,
    IN gctUINT StreamCount,
    IN gcsVERTEXARRAY_STREAM_PTR Streams
    );

#else
gceSTATUS
gcoHARDWARE_SetVertexArray(
    IN gcoHARDWARE Hardware,
    IN gctUINT First,
    IN gctUINT32 Physical,
    IN gctUINT Stride,
    IN gctUINT AttributeCount,
    IN gcsVERTEXARRAY_ATTRIBUTE_PTR Attributes,
    IN gctUINT StreamCount,
    IN gcsVERTEXARRAY_STREAM_PTR Streams
    );
#endif

gceSTATUS
gcoSTREAM_MergeStreams(
    IN gcoSTREAM Stream,
    IN gctUINT First,
    IN gctUINT Count,
    IN gctUINT StreamCount,
    IN gcsVERTEXARRAY_STREAM_PTR Streams,
    OUT gcoSTREAM * MergedStream,
    OUT gctPOINTER * Logical,
    OUT gctUINT32 * Physical,
    OUT gcsVERTEXARRAY_ATTRIBUTE_PTR * Attributes,
    OUT gcsSTREAM_SUBSTREAM_PTR * SubStream
    );

gceSTATUS
gcoHARDWARE_HzClearValueControl(
    IN gceSURF_FORMAT Format,
    IN gctUINT32 ZClearValue,
    OUT gctUINT32 * HzClearValue OPTIONAL,
    OUT gctUINT32 * Control OPTIONAL
    );

gceSTATUS
gcoHARDWARE_ProgramUniform(
    IN gctUINT32 Address,
    IN gctUINT Columns,
    IN gctUINT Rows,
    IN gctPOINTER Values,
    IN gctBOOL FixedPoint
    );

gceSTATUS
gco3D_PatchTarget(
    IN gco3D Engine
    );

#endif /* VIVANTE_NO_3D */

gceSTATUS
gcoOS_PrintStrVSafe(
    OUT gctSTRING String,
    IN gctSIZE_T StringSize,
    IN OUT gctUINT * Offset,
    IN gctCONST_STRING Format,
    IN gctARGUMENTS Arguments
    );

#ifdef __cplusplus
}
#endif

#endif /* __gc_hal_user_h_ */
