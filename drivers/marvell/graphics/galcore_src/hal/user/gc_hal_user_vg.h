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





#ifndef _gc_hal_user_vg_h__
#define _gc_hal_user_vg_h__

#include <gc_hal_engine_vg.h>


/******************************************************************************\
******************************** VG Structures *******************************
\******************************************************************************/

/** @ingroup gcoVG
**
**  @brief  Tesselation buffer information structure.
**
**  Tesselation buffers are used by the hardware to write tesselation data
**  out before it goes into VG block. This structure defines the buffers.
*/
typedef struct _gcsTESSELATION * gcsTESSELATION_PTR;
typedef struct _gcsTESSELATION
{
    /* Tesselation buffer memory node. */
    gcuVIDMEM_NODE_PTR          node;

    /* Actual allocated size. */
    gctSIZE_T                   allocatedSize;

    /* Pointers to the buffer. */
    gctUINT8_PTR                tsBufferLogical;
    gctUINT32                   tsBufferPhysical;

    gctUINT8_PTR                L1BufferLogical;
    gctUINT32                   L1BufferPhysical;

    gctUINT8_PTR                L2BufferLogical;
    gctUINT32                   L2BufferPhysical;

    /* Buffer parameters. */
    gctUINT                     stride;
    gctUINT                     shift;
    gctUINT                     clearSize;
#if gcdENABLE_SHARE_TESSELLATION
    gctINT                      reference;
#endif
}
gcsTESSELATION;

/** @ingroup gcoVG
**
**  @brief  The gcoVG object definition.
**
**  The gcoVG object holds all of the states and information required by the
**  OpenVG hardware pipe.
*/
struct _gcoVG
{
    /** The object. */
    gcsOBJECT                   object;

    /** Pointer to the gcoHAL object. */
    gcoHAL                      hal;

    /** Pointer to the gcoOS object. */
    gcoOS                       os;

    /** Pointer to the gcoVGHARDWARE object. */
    gcoVGHARDWARE               hw;

    /** VG versions present. */
    gctBOOL                     vg20;
    gctBOOL                     vg21;

    /** Path command buffer attributes. */
    gcsPATH_BUFFER_INFO         pathInfo;

    /** Generic command buffer attributes. */
    gcsCOMMAND_BUFFER_INFO      bufferInfo;

    /** Pointer to the render target. */
    gcoSURF                     target;
    gctINT                      targetWidth;
    gctINT                      targetHeight;

    /** Pointer to the mask. */
    gcoSURF                     mask;

    /** Current scissor surface. */
    gcoSURF                     scissor;
    gctUINT32                   scissorAddress;
    gctPOINTER                  scissorBits;

    /** Rendering quality. */
    gceRENDER_QUALITY           renderQuality;

#if gcdENABLE_SHARE_TESSELLATION
    gcsTESSELATION_PTR          tsBuffer;
    gcsTESSELATION              tsBuffer2;
#else
    /** Tessellation buffers. */
    gctUINT                     tsBufferIndex;
#if gcdENABLE_TS_DOUBLE_BUFFER
    gcsTESSELATION              tsBuffer[2];
#else
    gcsTESSELATION              tsBuffer[1];
#endif
#endif
    /** Transformation matrices. */
    gctFLOAT                    userToSurface[3 * 3];
    gctFLOAT                    surfaceToImage[3 * 3];

    /** Fill color. */
    gctUINT32                   tileFillColor;
};


/* Construct a new gcoVGHARDWARE object. */
gceSTATUS
gcoVGHARDWARE_Construct(
    IN gcoHAL Hal,
    OUT gcoVGHARDWARE * Hardware
    );

/* Destroy the gcoVGHARDWARE object. */
gceSTATUS
gcoVGHARDWARE_Destroy(
    IN gcoVGHARDWARE Hardware
    );

/* Initialize the context management structures and buffers. */
gceSTATUS
gcoVGHARDWARE_OpenContext(
    IN gcoVGHARDWARE Hardware
    );

/* Release resources associated with context management. */
gceSTATUS
gcoVGHARDWARE_CloseContext(
    IN gcoVGHARDWARE Hardware
    );

/* Merge temporary context buffer changes if any to the main context buffer. */
gceSTATUS
gcoVGHARDWARE_MergeContext(
    IN gcoVGHARDWARE Hardware
    );

/* Verify whether the specified feature is available in hardware. */
gctBOOL
gcoVGHARDWARE_IsFeatureAvailable(
    IN gcoVGHARDWARE Hardware,
    IN gceFEATURE Feature
    );

/* Align size to tile boundary. */
gceSTATUS
gcoVGHARDWARE_AlignToTile(
    IN gcoVGHARDWARE Hardware,
    IN gceSURF_TYPE Type,
    IN OUT gctUINT32_PTR Width,
    IN OUT gctUINT32_PTR Height
    );
/* Flush the current graphics pipe. */
gceSTATUS
gcoVGHARDWARE_FlushPipe(
    IN gcoVGHARDWARE Hardware
    );

/* Send semaphore down the current pipe. */
gceSTATUS
gcoVGHARDWARE_Semaphore(
    IN gcoVGHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gceBLOCK From,
    IN gceBLOCK To,
    IN gceHOW How,
    OUT gctSIZE_T * Bytes
    );

/* Update all 32 bits of a single state in the context. */
gceSTATUS
gcoVGHARDWARE_UpdateState(
    IN gcoVGHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT32 Data
    );

/* Set all 32 bits of a single state. */
gceSTATUS
gcoVGHARDWARE_SetState(
    IN gcoVGHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT32 Data
    );

/* Set all 32 bits of a specified number of states. */
gceSTATUS
gcoVGHARDWARE_SetStates(
    IN gcoVGHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctSIZE_T Count,
    IN gctPOINTER Data
    );

/* Load a number of load states. */
gceSTATUS
gcoVGHARDWARE_LoadState(
    IN gcoVGHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctSIZE_T Count,
    IN gctPOINTER States
    );

/* Load one 32-bit load state. */
gceSTATUS
gcoVGHARDWARE_LoadState32(
    IN gcoVGHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT32 Data
    );

/* Load one 64-bit load state. */
gceSTATUS
gcoVGHARDWARE_LoadState64(
    IN gcoVGHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT64 Data
    );

/* Commit the current command buffer. */
gceSTATUS
gcoVGHARDWARE_Commit(
    IN gcoVGHARDWARE Hardware,
    IN gctBOOL Stall
    );

/* Resolve. */
gceSTATUS
gcoVGHARDWARE_ResolveRect(
    IN gcoVGHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcInfo,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize
    );

/* Query the identity of the hardware. */
gceSTATUS
gcoVGHARDWARE_QueryChipIdentity(
    IN gcoVGHARDWARE Hardware,
    OUT gceCHIPMODEL* ChipModel,
    OUT gctUINT32* ChipRevision,
    OUT gctUINT32* ChipFeatures,
    OUT gctUINT32* ChipMinorFeatures,
    OUT gctUINT32* ChipMinorFeatures2
    );

/* Query command buffer attributes. */
gceSTATUS
gcoVGHARDWARE_QueryCommandBuffer(
    IN gcoVGHARDWARE Hardware,
    OUT gcsCOMMAND_BUFFER_INFO_PTR Information
    );

/* Query the target capabilities. */
gceSTATUS
gcoVGHARDWARE_QueryTargetCaps(
    IN gcoVGHARDWARE Hardware,
    OUT gctUINT * MaxWidth,
    OUT gctUINT * MaxHeight,
    OUT gctUINT * MultiTargetCount,
    OUT gctUINT * MaxSamples
    );

/* Query the tile size of the given surface. */
gceSTATUS
gcoVGHARDWARE_GetSurfaceTileSize(
    IN gcsSURF_INFO_PTR Surface,
    OUT gctINT32 * TileWidth,
    OUT gctINT32 * TileHeight
    );

/* Query tile sizes. */
gceSTATUS
gcoVGHARDWARE_QueryTileSize(
    OUT gctINT32 * TileWidth2D,
    OUT gctINT32 * TileHeight2D,
    OUT gctINT32 * TileWidth3D,
    OUT gctINT32 * TileHeight3D,
    OUT gctUINT32 * StrideAlignment
    );

/* Get tile status sizes for a surface. */
gceSTATUS
gcoVGHARDWARE_QueryTileStatus(
    IN gcoVGHARDWARE Hardware,
    IN gctUINT Width,
    IN gctUINT Height,
    OUT gctSIZE_T * Size,
    OUT gctUINT * Alignment
    );

/* Enable tile status for a surface. */
gceSTATUS
gcoVGHARDWARE_EnableTileStatus(
    IN gcoVGHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctUINT32 TileStatusAddress
    );

/* Disable tile status for a surface. */
gceSTATUS
gcoVGHARDWARE_DisableTileStatus(
    IN gcoVGHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctBOOL CommitAndStall
    );

/* Flush tile status cache. */
gceSTATUS
gcoVGHARDWARE_FlushTileStatus(
    IN gcoVGHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface
    );

/* Mark surface as dirty. */
gceSTATUS
gcoVGHARDWARE_InvalidateSurface(
    IN gcoVGHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface
    );

/* Lock a surface. */
gceSTATUS
gcoVGHARDWARE_Lock(
    IN gcoVGHARDWARE Hardware,
    IN gcsSURF_NODE_PTR Node,
    OUT gctUINT32 * Address,
    OUT gctPOINTER * Memory
    );

/* Unlock a surface. */
gceSTATUS
gcoVGHARDWARE_Unlock(
    IN gcoVGHARDWARE Hardware,
    IN gcsSURF_NODE_PTR Node,
    IN gceSURF_TYPE Type
    );

/* Allocate a task to be perfomed when signaled from the specified block. */
gceSTATUS
gcoVGHARDWARE_ReserveTask(
    IN gcoVGHARDWARE Hardware,
    IN gceBLOCK Block,
    IN gctUINT TaskCount,
    IN gctSIZE_T Bytes,
    OUT gctPOINTER * Memory
    );

/* Split a harwdare address into pool and offset. */
gceSTATUS
gcoVGHARDWARE_SplitAddress(
    IN gcoVGHARDWARE Hardware,
    IN gctUINT32 Address,
    OUT gcePOOL * Pool,
    OUT gctUINT32 * Offset
    );

/* Combine pool and offset into a harwdare address. */
gceSTATUS
gcoVGHARDWARE_CombineAddress(
    IN gcoVGHARDWARE Hardware,
    IN gcePOOL Pool,
    IN gctUINT32 Offset,
    OUT gctUINT32 * Address
    );

/* Allocate and lock a video surface. */
gceSTATUS
gcoVGHARDWARE_AllocateVideoMemory(
    IN gcoVGHARDWARE Hardware,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctUINT Depth,
    IN gceSURF_TYPE Type,
    IN gceSURF_FORMAT Format,
    IN gcePOOL Pool,
    OUT gcuVIDMEM_NODE_PTR * Node,
    OUT gctUINT32 * Address,
    OUT gctPOINTER * Memory,
    OUT gcePOOL * ActualPool
    );

/* Allocate and lock linear video memory. */
gceSTATUS
gcoVGHARDWARE_AllocateLinearVideoMemory(
    IN gcoVGHARDWARE Hardware,
    IN gctUINT Size,
    IN gctUINT Alignment,
    IN gcePOOL Pool,
    OUT gcuVIDMEM_NODE_PTR * Node,
    OUT gctUINT32 * Address,
    OUT gctPOINTER * Memory
    );

/* Free linear video memory allocated with gcoHAL_AllocateLinearVideoMemory. */
gceSTATUS
gcoVGHARDWARE_FreeVideoMemory(
    IN gcoVGHARDWARE Hardware,
    IN gcuVIDMEM_NODE_PTR Node
    );

/* Schedule to free linear video memory allocated. */
gceSTATUS
gcoVGHARDWARE_ScheduleVideoMemory(
    IN gcoVGHARDWARE Hardware,
    IN gcuVIDMEM_NODE_PTR Node,
    IN gctBOOL Unlock
    );

/* Allocate a temporary surface with specified parameters. */
gceSTATUS
gcoVGHARDWARE_AllocateTemporarySurface(
    IN gcoVGHARDWARE Hardware,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gcsSURF_FORMAT_INFO_PTR Format,
    IN gceSURF_TYPE Type
    );

/* Free the temporary surface. */
gceSTATUS
gcoVGHARDWARE_FreeTemporarySurface(
    IN gcoVGHARDWARE Hardware,
    IN gctBOOL Synchronized
    );

/* Convert pixel format. */
gceSTATUS
gcoVGHARDWARE_ConvertPixel(
    IN gcoVGHARDWARE Hardware,
    IN gctPOINTER SrcPixel,
    OUT gctPOINTER TrgPixel,
    IN gctUINT SrcBitOffset,
    IN gctUINT TrgBitOffset,
    IN gcsSURF_FORMAT_INFO_PTR SrcFormat,
    IN gcsSURF_FORMAT_INFO_PTR TrgFormat,
    IN gcsBOUNDARY_PTR SrcBoundary,
    IN gcsBOUNDARY_PTR TrgBoundary
    );

/* Copy a rectangular area with format conversion. */
gceSTATUS
gcoVGHARDWARE_CopyPixels(
    IN gcoVGHARDWARE Hardware,
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
gcoVGHARDWARE_SetAntiAlias(
    IN gcoVGHARDWARE Hardware,
    IN gctBOOL Enable
    );

/* Write data into the command buffer. */
gceSTATUS
gcoVGHARDWARE_WriteBuffer(
    IN gcoVGHARDWARE Hardware,
    IN gctCONST_POINTER Data,
    IN gctSIZE_T Bytes,
    IN gctBOOL Aligned
    );

/* Convert RGB8 color value to YUV color space. */
void
gcoVGHARDWARE_RGB2YUV(
    gctUINT8 R,
    gctUINT8 G,
    gctUINT8 B,
    gctUINT8_PTR Y,
    gctUINT8_PTR U,
    gctUINT8_PTR V
    );

/* Convert YUV color value to RGB8 color space. */
void
gcoVGHARDWARE_YUV2RGB(
    gctUINT8 Y,
    gctUINT8 U,
    gctUINT8 V,
    gctUINT8_PTR R,
    gctUINT8_PTR G,
    gctUINT8_PTR B
    );

/* Convert an API format to hardware parameters. */
gceSTATUS
gcoVGHARDWARE_ConvertFormat(
    IN gcoVGHARDWARE Hardware,
    IN gceSURF_FORMAT Format,
    OUT gctUINT32_PTR BitsPerPixel,
    OUT gctUINT32_PTR BytesPerTile
    );

/* Form a NOP command at the specified location in the command buffer. */
gceSTATUS
gcoVGHARDWARE_NopCommand(
    IN gcoVGHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN OUT gctSIZE_T * Bytes
    );

/* Form a STATE command at the specified location in the command buffer. */
gceSTATUS
gcoVGHARDWARE_StateCommand(
    IN gcoVGHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctUINT32 Address,
    IN gctSIZE_T Count,
    IN OUT gctSIZE_T * Bytes
    );

/* Form a DATA command at the specified location in the command buffer. */
gceSTATUS
gcoVGHARDWARE_DataCommand(
    IN gcoVGHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctSIZE_T Count,
    IN OUT gctSIZE_T * Bytes
    );

/* Form a RESTART command at the specified location in the command buffer. */
gceSTATUS
gcoVGHARDWARE_RestartCommand(
    IN gcoVGHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctUINT32 FetchAddress,
    IN gctUINT FetchCount,
    IN OUT gctSIZE_T * Bytes
    );

/* Form a FETCH command at the specified location in the command buffer. */
gceSTATUS
gcoVGHARDWARE_FetchCommand(
    IN gcoVGHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctUINT32 FetchAddress,
    IN gctUINT FetchCount,
    IN OUT gctSIZE_T * Bytes
    );

/* Form a CALL command at the specified location in the command buffer. */
gceSTATUS
gcoVGHARDWARE_CallCommand(
    IN gcoVGHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctUINT32 FetchAddress,
    IN gctUINT FetchCount,
    IN OUT gctSIZE_T * Bytes
    );

/* Form a RETURN command at the specified location in the command buffer. */
gceSTATUS
gcoVGHARDWARE_ReturnCommand(
    IN gcoVGHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN OUT gctSIZE_T * Bytes
    );

/* Form an END command at the specified location in the command buffer. */
gceSTATUS
gcoVGHARDWARE_EndCommand(
    IN gcoVGHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT32 InterruptId,
    IN OUT gctSIZE_T * Bytes
    );

/******************************************************************************\
******************************* gcoVGHARDWARE Object *****************************
\******************************************************************************/

gctUINT8
gcoVGHARDWARE_PackColorComponent(
    gctFLOAT Value
    );

gctUINT32
gcoVGHARDWARE_PackColor32(
    IN gctFLOAT Red,
    IN gctFLOAT Green,
    IN gctFLOAT Blue,
    IN gctFLOAT Alpha
    );

gctBOOL
gcoVGHARDWARE_IsMaskSupported(
    IN gceSURF_FORMAT Format
    );

gctBOOL
gcoVGHARDWARE_IsTargetSupported(
    IN gceSURF_FORMAT Format
    );

gctBOOL
gcoVGHARDWARE_IsImageSupported(
    IN gceSURF_FORMAT Format
    );

gceSTATUS
gcoVGHARDWARE_Tesselate(
    IN gcoVGHARDWARE Hardware,
    IN gcsPATH_DATA_PTR PathData,
    OUT gcsRECT_PTR BoundingBox
    );

gceSTATUS
gcoVGHARDWARE_QueryPathStorage(
    IN gcoVGHARDWARE Hardware,
    OUT gcsPATH_BUFFER_INFO_PTR PathInfo
    );

gceSTATUS
gcoVGHARDWARE_AssociateCompletion(
    IN gcoVGHARDWARE Hardware,
    IN gcsCMDBUFFER_PTR CommandBuffer
    );

gceSTATUS
gcoVGHARDWARE_DeassociateCompletion(
    IN gcoVGHARDWARE Hardware,
    IN gcsCMDBUFFER_PTR CommandBuffer
    );

gceSTATUS
gcoVGHARDWARE_CheckCompletion(
    IN gcoVGHARDWARE Hardware,
    IN gcsCMDBUFFER_PTR CommandBuffer
    );

gceSTATUS
gcoVGHARDWARE_WaitCompletion(
    IN gcoVGHARDWARE Hardware,
    IN gcsCMDBUFFER_PTR CommandBuffer
    );

gceSTATUS
gcoVGHARDWARE_EnableMask(
    gcoVGHARDWARE Hardware,
    gctBOOL Enable
    );

gceSTATUS
gcoVGHARDWARE_FlushVgMask(
    gcoVGHARDWARE Hardware
    );

gceSTATUS
gcoVGHARDWARE_SetVgMask(
    IN gcoVGHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Mask
    );

gceSTATUS
gcoVGHARDWARE_ProgramControl(
    gcoVGHARDWARE Hardware,
    gctPOINTER Logical,
    gctSIZE_T * Bytes
    );

gceSTATUS
gcoVGHARDWARE_SetPrimitiveMode(
    gcoVGHARDWARE Hardware,
    gceVG_PRIMITIVE Mode
    );

gceSTATUS
gcoVGHARDWARE_SetScissor(
    IN gcoVGHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctINT Stride
    );

gceSTATUS
gcoVGHARDWARE_EnableScissor(
    gcoVGHARDWARE Hardware,
    gctBOOL Enable
    );

gceSTATUS
gcoVGHARDWARE_SetColorTransform(
    IN gcoVGHARDWARE Hardware,
    IN gctFLOAT Scale[4],
    IN gctFLOAT Offset[4]
    );

gceSTATUS
gcoVGHARDWARE_EnableColorTransform(
    gcoVGHARDWARE Hardware,
    gctBOOL Enable
    );

gceSTATUS
gcoVGHARDWARE_SetVgBlendMode(
    gcoVGHARDWARE Hardware,
    gceVG_BLEND Mode
    );

gceSTATUS
gcoVGHARDWARE_SetVgImageMode(
    gcoVGHARDWARE Hardware,
    gceVG_IMAGE Mode
    );

gceSTATUS
gcoVGHARDWARE_SetPaintSolid(
    IN gcoVGHARDWARE Hardware,
    IN gctUINT8 Red,
    IN gctUINT8 Green,
    IN gctUINT8 Blue,
    IN gctUINT8 Alpha
    );

gceSTATUS
gcoVGHARDWARE_SetPaintLinear(
    IN gcoVGHARDWARE Hardware,
    IN gctFLOAT Constant,
    IN gctFLOAT StepX,
    IN gctFLOAT StepY
    );

gceSTATUS
gcoVGHARDWARE_SetPaintRadial(
    IN gcoVGHARDWARE Hardware,
    IN gctFLOAT LinConstant,
    IN gctFLOAT LinStepX,
    IN gctFLOAT LinStepY,
    IN gctFLOAT RadConstant,
    IN gctFLOAT RadStepX,
    IN gctFLOAT RadStepY,
    IN gctFLOAT RadStepXX,
    IN gctFLOAT RadStepYY,
    IN gctFLOAT RadStepXY
    );

gceSTATUS
gcoVGHARDWARE_SetPaintPattern(
    IN gcoVGHARDWARE Hardware,
    IN gctFLOAT UConstant,
    IN gctFLOAT UStepX,
    IN gctFLOAT UStepY,
    IN gctFLOAT VConstant,
    IN gctFLOAT VStepX,
    IN gctFLOAT VStepY,
    IN gctBOOL Linear
    );

gceSTATUS
gcoVGHARDWARE_SetPaintImage(
    IN gcoVGHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Image,
    IN gceTILE_MODE TileMode,
    IN gceIMAGE_FILTER Filter,
    IN gctUINT32 FillColor
    );

gceSTATUS
gcoVGHARDWARE_SetVgTarget(
    IN gcoVGHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Target
    );

gceSTATUS
gcoVGHARDWARE_SetRenderingQuality(
    IN gcoVGHARDWARE Hardware,
    IN gceRENDER_QUALITY Quality
    );

gceSTATUS
gcoVGHARDWARE_SetPathDataType(
    IN gcoVGHARDWARE Hardware,
    IN gcePATHTYPE Type
    );

gceSTATUS
gcoVGHARDWARE_SetFillRule(
    IN gcoVGHARDWARE Hardware,
    IN gceFILL_RULE FillRule
    );

gceSTATUS
gcoVGHARDWARE_EnableTessellation(
    IN gcoVGHARDWARE Hardware,
    IN gctBOOL Enable
    );

gceSTATUS
gcoVGHARDWARE_SetTessellation(
    IN gcoVGHARDWARE Hardware,
    IN gctBOOL SoftwareTesselation,
    IN gctUINT16 TargetWidth,
    IN gctUINT16 TargetHeight,
    IN gctFLOAT Bias,
    IN gctFLOAT Scale,
    IN gctFLOAT_PTR UserToSurface,
    IN gcsTESSELATION_PTR TessellationBuffer
    );

gceSTATUS
gcoVGHARDWARE_DrawVgRect(
    IN gcoVGHARDWARE Hardware,
    IN gctINT X,
    IN gctINT Y,
    IN gctINT Width,
    IN gctINT Height
    );

gceSTATUS
gcoVGHARDWARE_VgClear(
    IN gcoVGHARDWARE Hardware,
    IN gctINT X,
    IN gctINT Y,
    IN gctINT Width,
    IN gctINT Height
    );

gceSTATUS
gcoVGHARDWARE_DrawImage(
    IN gcoVGHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Image,
    IN gcsVG_RECT_PTR SrcRect,
    IN gcsVG_RECT_PTR TrgRect,
    IN gceIMAGE_FILTER Filter,
    IN gctBOOL Mask
    );

gceSTATUS
gcoVGHARDWARE_TesselateImage(
    IN gcoVGHARDWARE Hardware,
    IN gctBOOL SoftwareTesselation,
    IN gcsSURF_INFO_PTR Image,
    IN gcsVG_RECT_PTR Rectangle,
    IN gceIMAGE_FILTER Filter,
    IN gctBOOL Mask,
    IN gctFLOAT UserToSurface[9],
    IN gctFLOAT SurfaceToImage[9],
    IN gcsTESSELATION_PTR TessellationBuffer
    );

gceSTATUS
gcoVGHARDWARE_VgBlit(
    IN gcoVGHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Source,
    IN gcsSURF_INFO_PTR Target,
    IN gcsVG_RECT_PTR SrcRect,
    IN gcsVG_RECT_PTR TrgRect,
    IN gceIMAGE_FILTER Filter,
    IN gceVG_BLEND Mode
    );

gceSTATUS
gcoVGHARDWARE_DrawPath(
    IN gcoVGHARDWARE Hardware,
    IN gctBOOL SoftwareTesselation,
    IN gcsPATH_DATA_PTR PathData,
    IN gcsTESSELATION_PTR TessellationBuffer,
    OUT gctPOINTER * Path
    );

gceSTATUS
gcoVGHARDWARE_SetColorMatrix(
    IN gcoVGHARDWARE Hardware,
    IN const gctFLOAT * Matrix,
    IN gctUINT ColorChannels,
    IN gctBOOL SourceLinear,
    IN gctBOOL TargetLinear,
    IN gctBOOL TargetPremultiplied
    );

gceSTATUS
gcoVGHARDWARE_ColorMatrix(
    IN gcoVGHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Source,
    IN gcsSURF_INFO_PTR Target,
    IN const gctFLOAT * Matrix,
    IN gceCHANNEL ColorChannels,
    IN gctBOOL FilterLinear,
    IN gctBOOL FilterPremultiplied,
    IN gcsPOINT_PTR SourceOrigin,
    IN gcsPOINT_PTR TargetOrigin,
    IN gctINT Width,
    IN gctINT Height
    );

gceSTATUS
gcoVGHARDWARE_SeparableConvolve(
    IN gcoVGHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Source,
    IN gcsSURF_INFO_PTR Target,
    IN gctINT KernelWidth,
    IN gctINT KernelHeight,
    IN gctINT ShiftX,
    IN gctINT ShiftY,
    IN const gctINT16 * KernelX,
    IN const gctINT16 * KernelY,
    IN gctFLOAT Scale,
    IN gctFLOAT Bias,
    IN gceTILE_MODE TilingMode,
    IN gctFLOAT_PTR FillColor,
    IN gceCHANNEL ColorChannels,
    IN gctBOOL FilterLinear,
    IN gctBOOL FilterPremultiplied,
    IN gcsPOINT_PTR SourceOrigin,
    IN gcsPOINT_PTR TargetOrigin,
    IN gcsSIZE_PTR SourceSize,
    IN gctINT Width,
    IN gctINT Height
    );

gceSTATUS
gcoVGHARDWARE_GaussianBlur(
    IN gcoVGHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Source,
    IN gcsSURF_INFO_PTR Target,
    IN gctFLOAT StdDeviationX,
    IN gctFLOAT StdDeviationY,
    IN gceTILE_MODE TilingMode,
    IN gctFLOAT_PTR FillColor,
    IN gceCHANNEL ColorChannels,
    IN gctBOOL FilterLinear,
    IN gctBOOL FilterPremultiplied,
    IN gcsPOINT_PTR SourceOrigin,
    IN gcsPOINT_PTR TargetOrigin,
    IN gcsSIZE_PTR SourceSize,
    IN gctINT Width,
    IN gctINT Height
    );

gceSTATUS
gcoVGHARDWARE_EnableDither(
    IN gcoVGHARDWARE Hardware,
    IN gctBOOL Enable
    );



/******************************************************************************\
******************************** gcoVGBUFFER Object *******************************
\******************************************************************************/

/* Construct a new gcoVGBUFFER object. */
gceSTATUS
gcoVGBUFFER_Construct(
    IN gcoHAL Hal,
    IN gcoVGHARDWARE Hardware,
    IN gcsVGCONTEXT_PTR Context,
    IN gctSIZE_T CommandBufferSize,
    IN gctUINT TaskBufferGranulatiry,
    IN gctUINT QueueEntryCount,
    OUT gcoVGBUFFER * Buffer
    );

/* Destroy an gcoVGBUFFER object. */
gceSTATUS
gcoVGBUFFER_Destroy(
    IN gcoVGBUFFER Buffer
    );

/* Free all memory associated with allocated completion signals. */
gceSTATUS
gcoVGBUFFER_FreeCompletions(
    IN gcoVGBUFFER Buffer
    );

/* Associate a completion signal with the command buffer. */
gceSTATUS
gcoVGBUFFER_AssociateCompletion(
    IN gcoVGBUFFER Buffer,
    IN gcsCMDBUFFER_PTR CommandBuffer
    );

/* Release the current command buffer completion signal. */
gceSTATUS
gcoVGBUFFER_DeassociateCompletion(
    IN gcoVGBUFFER Buffer,
    IN gcsCMDBUFFER_PTR CommandBuffer
    );

/* Verify whether the command buffer is still in use. */
gceSTATUS
gcoVGBUFFER_CheckCompletion(
    IN gcoVGBUFFER Buffer,
    IN gcsCMDBUFFER_PTR CommandBuffer
    );

/* Wait until the command buffer is no longer in use. */
gceSTATUS
gcoVGBUFFER_WaitCompletion(
    IN gcoVGBUFFER Buffer,
    IN gcsCMDBUFFER_PTR CommandBuffer
    );

/* Mark begin/end points of restart area due to a possible TS overlfow. */
gceSTATUS
gcoVGBUFFER_MarkRestart(
    IN gcoVGBUFFER Buffer,
    IN gctPOINTER Logical,
    IN gctBOOL Begin,
    OUT gctSIZE_T * Bytes
    );

/* Return the current address inside the current command buffe. */
gceSTATUS
gcoVGBUFFER_GetCurrentAddress(
    IN gcoVGBUFFER Buffer,
    IN gctBOOL Aligned,
    OUT gctUINT32_PTR Address
    );

/* Ensure there is space for the specified number of bytes. */
gceSTATUS
gcoVGBUFFER_EnsureSpace(
    IN gcoVGBUFFER Buffer,
    IN gctSIZE_T Bytes,
    IN gctBOOL Aligned
    );

/* Reserve space in a command buffer. */
gceSTATUS
gcoVGBUFFER_Reserve(
    IN gcoVGBUFFER Buffer,
    IN gctSIZE_T Bytes,
    IN gctBOOL Aligned,
    OUT gctPOINTER * Memory
    );

/* Write data into the command buffer. */
gceSTATUS
gcoVGBUFFER_Write(
    IN gcoVGBUFFER Buffer,
    IN gctCONST_POINTER Data,
    IN gctSIZE_T Bytes,
    IN gctBOOL Aligned
    );

/* Write 32-bit data into the command buffer. */
gceSTATUS
gcoVGBUFFER_Write32(
    IN gcoVGBUFFER Buffer,
    IN gctUINT32 Data,
    IN gctBOOL Aligned
    );

/* Write 64-bit data into the command buffer. */
gceSTATUS
gcoVGBUFFER_Write64(
    IN gcoVGBUFFER Buffer,
    IN gctUINT64 Data,
    IN gctBOOL Aligned
    );

/* Append an externally allocated command buffer to the command queue. */
gceSTATUS
gcoVGBUFFER_AppendBuffer(
    IN gcoVGBUFFER Buffer,
    IN gctPOINTER Logical,
    IN gcsCMDBUFFER_PTR CommandBuffer,
    OUT gctSIZE_T * Bytes
    );

/* Reserve space for a task. */
gceSTATUS
gcoVGBUFFER_ReserveTask(
    IN gcoVGBUFFER Buffer,
    IN gceBLOCK Block,
    IN gctUINT TaskCount,
    IN gctSIZE_T Bytes,
    OUT gctPOINTER * Memory
    );

/* Commit the command buffer. */
gceSTATUS
gcoVGBUFFER_Commit(
    IN gcoVGBUFFER Buffer,
    IN gctBOOL Stall
    );

#endif  /* _gc_hal_user_vg_h__ */
