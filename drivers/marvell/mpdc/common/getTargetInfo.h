/*
** (C) Copyright 2011 Marvell International Ltd.
**  		All Rights Reserved

** This software file (the "File") is distributed by Marvell International Ltd.
** under the terms of the GNU General Public License Version 2, June 1991 (the "License").
** You may use, redistribute and/or modify this File in accordance with the terms and
** conditions of the License, a copy of which is available along with the File in the
** license.txt file or by writing to the Free Software Foundation, Inc., 59 Temple Place,
** Suite 330, Boston, MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
** THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES
** OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY DISCLAIMED.
** The License provides additional details about this warranty disclaimer.
*/

#ifndef GETTARGETINFO_H_
#define GETTARGETINFO_H_

#ifdef PX_CPU_PJ4
#include "../common/getTargetInfo_pj4.h"
#elif defined PX_CPU_PJ4B
#include "getTargetInfo_pj4b.h"
#ifdef PX_SOC_BG2
#include "../common/getTargetInfo_pj4b_bg2.h"
#endif
#ifdef PX_SOC_MMP3
#include "../common/getTargetInfo_pj4b_mmp3.h"
#endif
#elif defined(PX_CPU_PJ1)
#include "../common/getTargetInfo_pj1.h"
#endif

#endif /* GETTARGETINFO_H_ */
