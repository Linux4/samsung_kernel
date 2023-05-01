/*
 * msm8960-gpio.h
 *
 * header file supporting gpio functions for Samsung device
 *
 * COPYRIGHT(C) Samsung Electronics Co., Ltd. 2006-2011 All Right Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* MSM8960 GPIO */
#if defined(CONFIG_MACH_JAGUAR)
#include <mach/jaguar-gpio.h>
#elif defined(CONFIG_MACH_M2_VZW)
#include <mach/m2_vzw-gpio.h>
#elif defined(CONFIG_MACH_M2_ATT)
#include <mach/m2_att-gpio.h>
#elif defined(CONFIG_MACH_M2_SPR)
#include <mach/m2_spr-gpio.h>
#elif defined(CONFIG_MACH_JASPER)
#include <mach/jasper-gpio.h>
#elif defined(CONFIG_MACH_APEXQ)
#include <mach/apexq-gpio.h>
#elif defined(CONFIG_MACH_M2_SKT)
#include <mach/m2_skt-gpio.h>
#elif defined(CONFIG_MACH_M2_DCM)
#include <mach/m2_dcm-gpio.h>
#elif defined(CONFIG_MACH_K2_KDI)
#include <mach/k2_kdi-gpio.h>
#elif defined(CONFIG_MACH_GOGH)
#include <mach/gogh-gpio.h>
#elif defined(CONFIG_MACH_INFINITE)
#include <mach/infinite-gpio.h>
#elif defined(CONFIG_MACH_AEGIS2)
#include <mach/aegis2-gpio.h>
#elif defined(CONFIG_MACH_ESPRESSO_VZW)
#include <mach/espresso_vzw-gpio.h>
#elif defined(CONFIG_MACH_ESPRESSO_SPR)
#include <mach/espresso_spr-gpio.h>
#elif defined(CONFIG_MACH_ESPRESSO_ATT)
#include <mach/espresso_att-gpio.h>
#elif defined(CONFIG_MACH_ESPRESSO10_VZW)
#include <mach/espresso10_vzw-gpio.h>
#elif defined(CONFIG_MACH_ESPRESSO10_SPR)
#include <mach/espresso10_spr-gpio.h>
#elif defined(CONFIG_MACH_ESPRESSO10_ATT)
#include <mach/espresso10_att-gpio.h>
#elif defined(CONFIG_MACH_KONA)
#include <mach/kona-gpio.h>
#elif defined(CONFIG_MACH_COMANCHE)
#include <mach/comanche-gpio.h>
#elif defined(CONFIG_MACH_EXPRESS)
#include <mach/express-gpio.h>
#elif defined(CONFIG_MACH_ACCELERATE)
#include <mach/accelerate-gpio.h>
#elif defined(CONFIG_MACH_STRETTO)
#include <mach/stretto-gpio.h>
#elif defined(CONFIG_MACH_SUPERIORLTE_SKT)
#include <mach/superiorlte_skt-gpio.h>

#endif
