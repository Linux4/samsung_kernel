/*
 * Copyright (c) 2019 HUAQIN 
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Usage in C/C++ files
 *
 * #include <kernel_project_defines.h>
 * ...
 * #if CONFIG_ENABLE_FUNC
 *   // Do enable
 * #else
 *   // Do disable
 * #endif
 *
 */
#ifndef __KERNEL_PROJECT_DEFINES_H
#define __KERNEL_PROJECT_DEFINES_H

#if defined (HUAQIN_KERNEL_PROJECT_HS60)
    /* Add kernel functional macro here for HS60 project
       example:
       #define CONFIG_ENABLE_FUNC 1 */
/*HS60 code add for HS60-3438 add camera node by xuxianwei at 20191029 start*/
#define CONFIG_MSM_CAMERA_HS60_ADDBOARD 1
/*HS60 code add for HS60-3438 add camera node by xuxianwei at 20191029 end*/

    /* HS60 code for SR-ZQL1871-01-235 by lijingang at 2019/11/01 start */
    #define CONFIG_WCNSS_WLAN_NV_BIN_HS60 1
    /* HS60 code for SR-ZQL1871-01-235 by lijingang at 2019/11/01 end */

#elif defined (HUAQIN_KERNEL_PROJECT_HS70)
    /* Add kernel functional macro here for HS70 project
       example:
       #define CONFIG_ENABLE_FUNC 0 */
    /* HS60 code for SR-ZQL1871-01-235 by lijingang at 2019/11/01 start */
    #define CONFIG_WCNSS_WLAN_NV_BIN_HS70 1
    /* HS60 code for SR-ZQL1871-01-235 by lijingang at 2019/11/01 end */
    /*HS70 code for HS70-3706 by wuhongwen at 2020/01/08 start*/
    #define CONFIG_IRQ_SHOW_RESUME_HS70
    /*HS70 code for HS70-3706 by wuhongwen at 2020/01/08 end*/
#elif defined (HUAQIN_KERNEL_PROJECT_HS71)
    /* Add kernel functional macro here for HS71 project
       example:
       #define CONFIG_ENABLE_FUNC 0 */
    /* HS60 code for SR-ZQL1871-01-235 by lijingang at 2019/11/01 start */
    #define CONFIG_WCNSS_WLAN_NV_BIN_HS71 1
    /* HS60 code for SR-ZQL1871-01-235 by lijingang at 2019/11/01 end */
    /*HS70 code for HS70-3706 by wuhongwen at 2020/01/08 start*/
    #define CONFIG_IRQ_SHOW_RESUME_HS71
    /*HS70 code for HS70-3706 by wuhongwen at 2020/01/08 end*/
#elif defined (HUAQIN_KERNEL_PROJECT_HS50)
    /* Add kernel functional macro here for HS50 project
       example:
       #define CONFIG_ENABLE_FUNC 1 */
    /* HS50 code for SR-QL3095-01-182 by weilong at 2020/08/05 start */
    #define CONFIG_WCNSS_WLAN_NV_BIN_HS50 1
    /* HS50 code for SR-QL3095-01-182 by weilong at 2020/08/05 end */
    /*Huaqin add for HS50-SR-QL3095-01-24 add camera node by xuxianwei at 2020/08/13 start*/
    #define CONFIG_MSM_CAMERA_HS50_ADDBOARD 1
    /*Huaqin add for HS50-SR-QL3095-01-24 add camera node by xuxianwei at 2020/08/13 end*/
#endif

#endif

