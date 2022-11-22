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




#ifndef __gc_hal_user_precomp_h__
#define __gc_hal_user_precomp_h__

#if !defined(HAL_EXPORTS)
#   define HAL_EXPORTS
#endif

#include "gc_hal_user.h"
#include "gc_hal_user_math.h"

#if gcdENABLE_VG
#include "gc_hal_user_vg.h"
#endif

#ifndef VIVANTE_NO_3D
#   include "gc_hal_compiler.h"
#   include "gc_hal_user_compiler.h"
#endif

#include "gc_hal_profiler.h"

/* Workaround GCC optimizer's bug */
#if defined(__GNUC__) && !gcmIS_DEBUG(gcdDEBUG_TRACE)
    gcmINLINE static void
    __do_nothing(
        IN gctUINT32 Level,
        IN gctUINT32 Zone,
        IN gctCONST_STRING Message,
        ...
        )
    {
        static volatile int c;

        c++;
    }

#   undef  gcmTRACE_ZONE
#   define gcmTRACE_ZONE            __do_nothing
#endif

#endif /* __gc_hal_user_precomp_h__ */
