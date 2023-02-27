/*
   The Synopsys Software Driver and documentation (hereinafter "Software")
   is an unsupported proprietary work of Synopsys, Inc. unless otherwise
   expressly agreed to in writing between  Synopsys and you.

   The Software IS NOT an item of Licensed Software or Licensed Product under
   any End User Software License Agreement or Agreement for Licensed Product
   with Synopsys or any supplement thereto.  Permission is hereby granted,
   free of charge, to any person obtaining a copy of this software annotated
   with this license and the Software, to deal in the Software without
   restriction, including without limitation the rights to use, copy, modify,
   merge, publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so, subject
   to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
   DAMAGE.
   */

#ifndef MIPI_DSIH_LOCAL_H_
#define MIPI_DSIH_LOCAL_H_

//#include <stdint.h>
#include <linux/kernel.h>
#include "../sprdfb_chip_common.h"


//typedef unsigned char   uint8_t;
//typedef unsigned short  uint16_t;
//typedef unsigned int    uint32_t;

#define DSIH_PIXEL_TOLERANCE  2
#define DSIH_FIFO_ACTIVE_WAIT 5000    /* no of tries to access the fifo*/
#define DSIH_PHY_ACTIVE_WAIT  200
#define ONE_MS_ACTIVE_WAIT    50000 /* 50MHz processor */
#define DEFAULT_BYTE_CLOCK    864000 /* a value to start PHY PLL - random */

/** Define D-PHY type */

#ifdef SPRD_MIPI_DPHY_GEN1
/** DWC_MIPI_DPHY_BIDIR_TSMC40LP 4 Lanes Gen 1 1GHz */
#define DWC_MIPI_DPHY_BIDIR_TSMC40LP
#endif
#ifdef SPRD_MIPI_DPHY_GEN2
/** DWC_MIPI_DPHY_BIDIR_TSMC40LP / GF28LP 4 Lanes Gen 2 1.5GHz */
#define GEN_2
#endif
/** 4 Lanes Gen 2 1.5GHz testchips */
//#define TESTCHIP
/** TQL 2 Lane test chip */
/* #define DPHY2Btql */
typedef enum
{
	OK = 0,
	ERR_DSI_COLOR_CODING,
	ERR_DSI_OUT_OF_BOUND,
	ERR_DSI_OVERFLOW,
	ERR_DSI_INVALID_INSTANCE,
	ERR_DSI_INVALID_IO,
	ERR_DSI_CORE_INCOMPATIBLE,
	ERR_DSI_VIDEO_MODE,
	ERR_DSI_INVALID_COMMAND,
	ERR_DSI_INVALID_EVENT,
	ERR_DSI_INVALID_HANDLE,
	ERR_DSI_PHY_POWERUP,
	ERR_DSI_PHY_INVALID,
	ERR_DSI_PHY_FREQ_OUT_OF_BOUND,
#ifdef GEN_2
	ERR_DSI_PHY_PLL_NOT_LOCKED,
#endif
	ERR_DSI_TIMEOUT
}
dsih_error_t;
typedef enum
{
	VIDEO_NON_BURST_WITH_SYNC_PULSES = 0,
	VIDEO_NON_BURST_WITH_SYNC_EVENTS,
	VIDEO_BURST_WITH_SYNC_PULSES
}
dsih_video_mode_t;
typedef enum
{
	COLOR_CODE_16BIT_CONFIG1,
	COLOR_CODE_16BIT_CONFIG2,
	COLOR_CODE_16BIT_CONFIG3,
	COLOR_CODE_18BIT_CONFIG1,
	COLOR_CODE_18BIT_CONFIG2,
	COLOR_CODE_24BIT
}
dsih_color_coding_t;
typedef enum
{
	ACK_SOT_ERR = 0,
	ACK_SOT_SYNC,
	ACK_EOT_SYNC,
	ACK_ESCAPE_CMD_ERR,
	ACK_LP_TX_SYNC_ERR,
	ACK_HS_RX_TIMEOUT_ERR,
	ACK_FALSE_CONTROL_ERR,
	ACK_RSVD_DEVICE_ERR_7,
	ACK_ECC_SINGLE_BIT_ERR,
	ACK_ECC_MULTI_BIT_ERR,
	ACK_CHECKSUM_ERR,
	ACK_DSI_TYPE_NOT_RECOGNIZED_ERR,
	ACK_VC_ID_INVALID_ERR,
	ACK_INVALID_TX_LENGTH_ERR,
	ACK_RSVD_DEVICE_ERR_14,
	ACK_DSI_PROTOCOL_ERR,

	DPHY_ESC_ENTRY_ERR,
	DPHY_SYNC_ESC_LP_ERR,
	DPHY_CONTROL_LANE0_ERR,
	DPHY_CONTENTION_LP0_ERR,
	DPHY_CONTENTION_LP1_ERR,
	/* start of st1*/
	HS_CONTENTION,
	LP_CONTENTION,
	RX_ECC_SINGLE_ERR,
	RX_ECC_MULTI_ERR,
	RX_CRC_ERR,
	RX_PKT_SIZE_ERR,
	RX_EOTP_ERR,
	DPI_PLD_FIFO_FULL_ERR,
	GEN_TX_CMD_FIFO_FULL_ERR,
	GEN_TX_PLD_FIFO_FULL_ERR,
	GEN_TX_PLD_FIFO_EMPTY_ERR,
	GEN_RX_PLD_FIFO_EMPTY_ERR,
	GEN_RX_PLD_FIFO_FULL_ERR,

	DBI_TX_CMD_FIFO_FULL_ERR,
	DBI_TX_PLD_FIFO_FULL_ERR,
	DBI_RX_PLD_FIFO_EMPTY_ERR,
	DBI_RX_PLD_FIFO_FULL_ERR,
	DBI_ILLEGAL_CMD_ERR,
	DSI_MAX_EVENT
}
dsih_event_t;
typedef enum
{
	NOT_INITIALIZED = 0,
	INITIALIZED,
	ON,
	OFF
}
dsih_state_t;

typedef struct dphy_t
{
	uint32_t address;
	uint32_t reference_freq;
#ifdef CONFIG_FB_DYNAMIC_FREQ_SCALING
	/** mark if D-PHY should keep work or not */
	uint32_t phy_keep_work;
#endif
	dsih_state_t status;
	void (*bsp_pre_config)(struct dphy_t *instance, void* param);
	uint32_t (*core_read_function)(uint32_t addr, uint32_t offset);
	void (*core_write_function)(uint32_t addr, uint32_t offset, uint32_t data);
	void (*log_error)(const char * string);
	void (*log_info)(const char *fmt, ...);
}
dphy_t;

typedef struct dsih_ctrl_t
{
	uint32_t address;
	dphy_t phy_instance;
	uint32_t phy_feq;
	uint8_t max_lanes;
	uint8_t max_hs_to_lp_cycles;
	uint8_t max_lp_to_hs_cycles;
	uint16_t max_bta_cycles;
	int color_mode_polarity;
	int shut_down_polarity;
	dsih_state_t status;
	uint32_t (*core_read_function)(uint32_t addr, uint32_t offset);
	void (*core_write_function)(uint32_t addr, uint32_t offset, uint32_t data);
	void (*log_error)(const char * string);
	void (*log_info)(const char *fmt, ...);
	void (*event_registry[DSI_MAX_EVENT])(struct dsih_ctrl_t *instance, void *handler);
}
dsih_ctrl_t;
typedef struct
{
	uint8_t  no_of_lanes;
	uint8_t  virtual_channel;
	dsih_video_mode_t video_mode;
	int      receive_ack_packets;
	uint32_t byte_clock;
	uint32_t pixel_clock;
	dsih_color_coding_t color_coding;
	int      is_18_loosely;
	int      data_en_polarity;
	int      h_polarity;
	uint16_t h_active_pixels; /* hadr*/
	uint16_t h_sync_pixels;
	uint16_t h_back_porch_pixels;   /* hbp */
	uint16_t h_total_pixels;  /* h_total */
	int      v_polarity;
	uint16_t v_active_lines; /* vadr*/
	uint16_t v_sync_lines;
	uint16_t v_back_porch_lines;   /* vbp */
	uint16_t v_total_lines;  /* v_total */
}
dsih_dpi_video_t;
typedef struct
{
	uint32_t addr;
	uint32_t data;
}
register_config_t;

#endif /* MIPI_DSIH_LOCAL_H_ */
