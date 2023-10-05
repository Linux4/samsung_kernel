
/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *
 * Author: Beobki Jung <beobki.jung@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for exynos display.
*/

#ifndef _DT_BINDINGS_EXYNOS_MIPI_DSI_DISPLAY_H
#define _DT_BINDINGS_EXYNOS_MIPI_DSI_DISPLAY_H

/* DSI Interface Type */
#define MIPI_DSI_INTERFACE_DPHY		0
#define MIPI_DSI_INTERFACE_CPHY		1

/* DSI mode flags */

/* video mode */
#define MIPI_DSI_MODE_VIDEO		0
/* video burst mode */
#define MIPI_DSI_MODE_VIDEO_BURST	1
/* video pulse mode */
#define MIPI_DSI_MODE_VIDEO_SYNC_PULSE	2
/* enable auto vertical count mode */
#define MIPI_DSI_MODE_VIDEO_AUTO_VERT	3
/* enable hsync-end packets in vsync-pulse and v-porch area */
#define MIPI_DSI_MODE_VIDEO_HSE		4
/* disable hfront-porch area */
#define MIPI_DSI_MODE_VIDEO_HFP		5
/* disable hback-porch area */
#define MIPI_DSI_MODE_VIDEO_HBP		6
/* disable hsync-active area */
#define MIPI_DSI_MODE_VIDEO_HSA		7
/* flush display FIFO on vsync pulse */
#define MIPI_DSI_MODE_VSYNC_FLUSH	8
/* disable EoT packets in HS mode */
#define MIPI_DSI_MODE_EOT_PACKET	9
/* device supports non-continuous clock behavior (DSI spec 5.6.1) */
#define MIPI_DSI_CLOCK_NON_CONTINUOUS	10
/* transmit data in low power */
#define MIPI_DSI_MODE_LPM		11
/* disable BLLP area */
#define MIPI_DSI_MODE_VIDEO_BLLP	12
/* disable EOF BLLP area */
#define MIPI_DSI_MODE_VIDEO_EOF_BLLP	13

#endif  /*_DT_BINDINGS_EXYNOS_MIPI_DSI_DISPLAY_H */
