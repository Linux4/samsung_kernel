/*
 * @file mipi_dsih_hal.h
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */
/*
	The Synopsys Software Driver and documentation (hereinafter "Software")
	is an unsupported proprietary work of Synopsys, Inc. unless otherwise
	expressly agreed to in writing between	Synopsys and you.

	The Software IS NOT an item of Licensed Software or Licensed Product under
	any End User Software License Agreement or Agreement for Licensed Product
	with Synopsys or any supplement	thereto.  Permission is hereby granted,
	free of charge, to any person obtaining a copy of this software annotated
	with this license and the Software, to deal in the Software without
	restriction, including without limitation the rights to use, copy, modify,
	merge, publish, distribute, sublicense,	and/or sell copies of the Software,
	and to permit persons to whom the Software is furnished to do so, subject
	to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
	AND ANY	EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
	INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT	LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR	PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE	POSSIBILITY OF SUCH
	DAMAGE.
 */

#ifndef MIPI_DSIH_HAL_H_
#define MIPI_DSIH_HAL_H_

#include "mipi_dsih_local.h"

#define R_DSI_HOST_VERSION     			(0x00)
#define R_DSI_HOST_PWR_UP     			(0x04)
#define R_DSI_HOST_CLK_MGR     			(0x08)
#define R_DSI_HOST_DPI_VCID    			(0x0C)
#define R_DSI_HOST_DPI_COLOR_CODE		(0x10)
#define R_DSI_HOST_DPI_CFG_POL 			(0x14)
#define R_DSI_HOST_DPI_LP_CMD_TIM		(0x18)
#define R_DSI_HOST_DBI_VCID    			(0x1C)
#define R_DSI_HOST_DBI_CFG     			(0x20)
#define R_DSI_HOST_DBI_PARTITION_EN 	(0x24)
#define R_DSI_HOST_DBI_CMDSIZE  		(0x28)
#define R_DSI_HOST_PCKHDL_CFG   		(0x2C)
#define R_DSI_HOST_GEN_VCID    			(0x30)
#define R_DSI_HOST_MODE_CFG				(0x34)
#define R_DSI_HOST_VID_MODE_CFG			(0x38)
#define R_DSI_HOST_VID_PKT_SIZE			(0x3C)
#define R_DSI_HOST_VID_NUM_CHUNKS		(0x40)
#define R_DSI_HOST_VID_NULL_SIZE		(0x44)
#define R_DSI_HOST_VID_HSA_TIME			(0x48)
#define R_DSI_HOST_VID_HBP_TIME			(0x4C)
#define R_DSI_HOST_VID_HLINE_TIME		(0x50)
#define R_DSI_HOST_VID_VSA_LINES		(0x54)
#define R_DSI_HOST_VID_VBP_LINES		(0x58)
#define R_DSI_HOST_VID_VFP_LINES		(0x5C)
#define R_DSI_HOST_VID_VACTIVE_LINES 	(0x60)
#define R_DSI_HOST_EDPI_CMD_SIZE     	(0x64)
#define R_DSI_HOST_CMD_MODE_CFG			(0x68)
#define R_DSI_HOST_GEN_HDR	     		(0x6C)
#define R_DSI_HOST_GEN_PLD_DATA 	 	(0x70)
#define R_DSI_HOST_CMD_PKT_STATUS 		(0x74)
#define R_DSI_HOST_TO_CNT_CFG    		(0x78)
#define R_DSI_HOST_HS_RD_TO_CNT			(0x7C)
#define R_DSI_HOST_LP_RD_TO_CNT			(0x80)
#define R_DSI_HOST_HS_WR_TO_CNT			(0x84)
#define R_DSI_HOST_LP_WR_TO_CNT			(0x88)
#define R_DSI_HOST_BTA_TO_CNT			(0x8C)

#define R_DSI_HOST_SDF_3D				(0x90)

#define R_DSI_HOST_LPCLK_CTRL			(0x94)
#define R_DSI_HOST_PHY_TMR_LPCLK_CFG	(0x98)
#define R_DSI_HOST_PHY_TMR_CFG			(0x9C)
#define R_DSI_HOST_INT_ST0  	   		(0xBC)
#define R_DSI_HOST_INT_ST1     			(0xC0)
#define R_DSI_HOST_INT_MSK0     		(0xC4)
#define R_DSI_HOST_INT_MSK1     		(0xC8)
#define R_DSI_HOST_PHY_STATUS			(0xB0)



typedef enum _Dsi_Int0_Type_ {
    ack_with_err_0,
    ack_with_err_1,
    ack_with_err_2,
    ack_with_err_3,
    ack_with_err_4,
    ack_with_err_5,
    ack_with_err_6,
    ack_with_err_7,
    ack_with_err_8,
    ack_with_err_9,
    ack_with_err_10,
    ack_with_err_11,
    ack_with_err_12,
    ack_with_err_13,
    ack_with_err_14,
    ack_with_err_15,
    dphy_errors_0,
    dphy_errors_1,
    dphy_errors_2,
    dphy_errors_3,
    dphy_errors_4,
    DSI_INT0_MAX,
}Dsi_Int0_Type;

typedef enum _Dsi_Int1_Type_ {
    to_hs_tx,
    to_lp_rx,
    ecc_single_err,
    ecc_multi_err,
    crc_err,
    pkt_size_err,
    eopt_err,
    dpi_pld_wr_err,
    gen_cmd_wr_err,
    gen_pld_wr_err,
    gen_pld_send_err,
    gen_pld_rd_err,
    gen_pld_recv_err,
    dbi_cmd_wr_err,
    dbi_pld_wr_err,
    dbi_pld_rd_err,
    dbi_pld_recv_err,
    dbi_illegal_comm_err,
    DSI_INT1_MAX,
}Dsi_Int1_Type;
#define DSI_INT_MASK0_SET(bit,val)\
{\
	uint32_t reg_val = dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_INT_MSK0);\
	reg_val = (val == 1)?(reg_val | (1UL<<bit)):(reg_val & (~(1UL<<bit)));\
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_INT_MSK0, reg_val);\
}

#define DSI_INT_MASK1_SET(bit,val)\
{\
	uint32_t reg_val = dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_INT_MSK1);\
	reg_val = (val == 1)?(reg_val | (1UL<<bit)):(reg_val & (~(1UL<<bit)));\
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_INT_MSK1, reg_val);\
}


//base :0x2180 0000
typedef struct _DSIH1P21A_REG_T_
{
    union _DSIH1P21A_VERSION_tag_t {
        struct _DSIH1P21A_VERSION_map_t
        {
volatile unsigned int version :
            32;  //0x3132302A
        }
        mBits;
        volatile  unsigned int   dwVersion;
    }VERSION;// 0x0000

    union _DSIH1P21A_PWR_UP_tag_t {
        struct _DSIH1P21A_PWR_UP_map_t
        {
volatile unsigned int power_up             :
            1;    //[0]  1 power up , 0 reset core
volatile unsigned int reserved_0                :
            31; //[31:1] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }PWR_UP;// 0x0004


    union _DSIH1P21A_CLKMGR_CFG_tag_t {
        struct _DSIH1P21A_CLKMGR_CFG_map_t
        {
volatile unsigned int tx_esc_clk_division             :
            8;    //[7:0]  This field indicates the division factor for the TX Escape clock source (lanebyteclk). The values 0 and 1 stop the TX_ESC clock generation
volatile unsigned int to_clk_division             :
            8;    //[15:8]  This field indicates the division factor for the Time Out clock used as the timing unit in the configuration of HS to LP and LP to HS transition error.
volatile unsigned int reserved_0                :
            16; //[31:16] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }CLKMGR_CFG;// 0x0008

    union _DSIH1P21A_DPI_VCID_tag_t {
        struct _DSIH1P21A_DPI_VCID_map_t
        {
volatile unsigned int dpi_vcid             :
            2;    //[1:0]  This field configures the DPI virtual channel id that is indexed to the Video mode packets.
volatile unsigned int reserved_0                :
            30; //[31:2] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }DPI_VCID;// 0x000C
    //======================

    union _DSIH1P21A_DPI_COLOR_CODING_tag_t {
        struct _DSIH1P21A_DPI_COLOR_CODING_map_t
        {
volatile unsigned int dpi_color_coding             :
            4;    //[3:0]
            /*
            This field configures the DPI color coding as follows:
            0000: 16-bit configuration 1
            0001: 16-bit configuration 2
            0010: 16-bit configuration 3
            0011: 18-bit configuration 1
            0100: 18-bit configuration 2
            0101: 24-bit
            0110: 20-bit YCbCr 4:2:2 loosely packed
            0111: 24-bit YCbCr 4:2:2
            1000: 16-bit YCbCr 4:2:2
            1001: 30-bit
            1010: 36-bit
            1011-1111: 12-bit YCbCr 4:2:0
            Note: If the eDPI interface is chosen and currently works in the
            Command mode (cmd_video_mode = 1), then
            0110-1111: 24-bit
            */
volatile unsigned int reserved_0                :
            4; //[7:4] Reserved
volatile unsigned int loosely18_en             :
            1;    //[8] When set to 1, this bit activates loosely packed variant to 18-bit configurations.
volatile unsigned int reserved_1                :
            23; //[31:9] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }DPI_COLOR_CODING;// 0x0010


    union _DSIH1P21A_DPI_CFG_POL_tag_t {
        struct _DSIH1P21A_DPI_CFG_POL_map_t
        {
volatile unsigned int dataen_active_low             :
            1;    //[0] When set to 1, this bit configures the data enable pin (dpidataen) asactive low.
volatile unsigned int vsync_active_low             :
            1;    //[1] When set to 1, this bit configures the vertical synchronism pin (dpivsync) as active low.
volatile unsigned int hsync_active_low             :
            1;    //[2] When set to 1, this bit configures the horizontal synchronism pin (dpihsync) as active low.
volatile unsigned int shutd_active_low             :
            1;    //[3] When set to 1, this bit configures the shutdown pin (dpishutdn) as active low
volatile unsigned int colorm_active_low             :
            1;    //[4] When set to 1, this bit configures the color mode pin (dpicolorm) as active low.
volatile unsigned int reserved_0             :
            27;    //[31:5]
        }
        mBits;
        volatile unsigned int dValue;
    }DPI_CFG_POL;// 0x0014


    union _DSIH1P21A_DPI_LP_CMD_TIM_tag_t {
        struct _DSIH1P21A_DPI_LP_CMD_TIM_map_t
        {
volatile unsigned int invact_lpcmd_time             :
            8;    //[7:0] This field is used for the transmission of commands in low-power mode. It defines the size, in bytes, of the largest packet that can fit in a line during the VACT region.
volatile unsigned int reserved_0                :
            8; //[15:8] Reserved
volatile unsigned int outvact_lpcmd_time             :
            8;    //[23:16] This field is used for the transmission of commands in low-power mode. It defines the size, in bytes, of the largest packet that can fit in a line during the VSA, VBP, and VFP regions.
volatile unsigned int reserved_1                :
            8; //[31:24] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }DPI_LP_CMD_TIM;// 0x0018

    union _DSIH1P21A_DBI_VCID_tag_t {
        struct _DSIH1P21A_DBI_VCID_map_t
        {
volatile unsigned int dbi_vcid             :
            2;    //[1:0] This field configures the virtual channel id that is indexed to the DCS packets from DBI.
volatile unsigned int reserved_0                :
            30; //[31:2] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }DBI_VCID;// 0x001C

    union _DSIH1P21A_DBI_CFG_tag_t {
        struct _DSIH1P21A_DBI_CFG_map_t
        {
volatile unsigned int in_dbi_conf             :
            4;    //[3:0]
            /*
            This field configures the DBI input pixel data as follows:
            0000: 8-bit 8 bpp
            0001: 8-bit 12 bpp
            0010: 8-bit 16 bpp
            0011: 8-bit 18 bpp
            0100: 8-bit 24 bpp
            0101: 9-bit 18 bpp
            0110: 16-bit 8 bpp
            0111: 16-bit 12 bpp
            1000: 16-bit 16 bpp
            1001: 16-bit 18 bpp, option 1
            1010: 16-bit 18 bpp, option 2
            1011: 16-bit 24 bpp, option 1
            1100: 16-bit 24 bpp, option 2
            */
volatile unsigned int reserved_0                :
            4; //[7:4] Reserved
volatile unsigned int out_dbi_conf             :
            4;    //[11:8]
            /*
            This field configures the DBI output pixel data as follows:
            0000: 8-bit 8 bpp
            0001: 8-bit 12 bpp
            0010: 8-bit 16 bpp
            0011: 8-bit 18 bpp
            0100: 8-bit 24 bpp
            0101: 9-bit 18 bpp
            0110: 16-bit 8 bpp
            0111: 16-bit 12 bpp
            1000: 16-bit 16 bpp
            1001: 16-bit 18 bpp, option 1
            1010: 16-bit 18 bpp, option 2
            1011: 16-bit 24 bpp, option 1
            1100: 16-bit 24 bpp, option 2
            */
volatile unsigned int reserved_1                :
            4; //[15:12] Reserved

volatile unsigned int lut_size_conf             :
            2;    //[17:16]
            /*
            This field configures the size used to transport the write Lut
            commands as follows:
            00: 16-bit color display
            01: 18-bit color display
            10: 24-bit color display
            11: 16-bit color display
            */

volatile unsigned int reserved_2                :
            14; //[31:18] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }DBI_CFG;// 0x0020

    union _DSIH1P21A_DBI_PARTITIONING_EN_tag_t {
        struct _DSIH1P21A_DBI_PARTITIONING_EN_map_t
        {
volatile unsigned int partitioning_en             :
            1;    //[0]
            /*
            When set to 1, this bit enables the use of write_memory_continue
            input commands (system needs to ensure correct partitioning of Long
            Write commands). When not set, partitioning is automatically
            performed in the DWC_mipi_dsi_host.
            */
volatile unsigned int reserved_0                :
            31; //[31:1] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }DBI_PARTITIONING_EN;// 0x0024

    union _DSIH1P21A_DBI_CMDSIZE_tag_t {
        struct _DSIH1P21A_DBI_CMDSIZE_map_t
        {
volatile unsigned int wr_cmd_size             :
            16;    //[15:0]
            /*
            This field configures the size of the DCS write memory commands.
            The size of DSI packet payload is the actual payload size minus 1,
            because the DCS command is in the DSI packet payload
            */
volatile unsigned int allowed_cmd_size             :
            16;    //[31:16]
            /*
            This field configures the maximum allowed size for a DCS write
            memory command. This field is used to partition a write memory
            command into one write_memory_start and a variable number of
            write_memory_continue commands. It is only used if the
            partitioning_en bit of the DBI_CFG register is disabled.
            The size of the DSI packet payload is the actual payload size minus 1,
            because the DCS command is in the DSI packet payload.
            */

        }
        mBits;
        volatile unsigned int dValue;
    }DBI_CMDSIZE;// 0x0028

    union _DSIH1P21A_PCKHDL_CFG_tag_t {
        struct _DSIH1P21A_PCKHDL_CFG_map_t
        {
volatile unsigned int eotp_tx_en             :
            1;    //[0] When set to 1, this bit enables the EoTp transmission
volatile unsigned int eotp_rx_en             :
            1;    //[1] When set to 1, this bit enables the EoTp reception.
volatile unsigned int bta_en             :
            1;    //[2] When set to 1, this bit enables the Bus Turn-Around (BTA) request.
volatile unsigned int ecc_rx_en             :
            1;    //[3] When set to 1, this bit enables the ECC reception, error correction, and reporting.
volatile unsigned int crc_rx_en             :
            1;    //[4] When set to 1, this bit enables the CRC reception and error reporting. Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3 or DSI_GENERIC = 1. Otherwise, this bit is reserved.
volatile unsigned int reserved_0             :
            27;    //[31:5]
        }
        mBits;
        volatile unsigned int dValue;
    }PCKHDL_CFG;// 0x002C

    union _DSIH1P21A_GEN_VCID_tag_t {
        struct _DSIH1P21A_GEN_VCID_map_t
        {
volatile unsigned int gen_vcid_rx             :
            2;    //[1:0]  This field indicates the Generic interface read-back virtual channel  identification.
volatile unsigned int reserved_0                :
            30; //[31:2] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }GEN_VCID;// 0x0030

    union _DSIH1P21A_MODE_CFG_tag_t {
        struct _DSIH1P21A_MODE_CFG_map_t
        {
volatile unsigned int cmd_video_mode             :
            1;    //[0]  This bit configures the operation mode:0: Video mode ;   1: Command mode
volatile unsigned int reserved_0                :
            31; //[31:1] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }MODE_CFG;// 0x0034

    union _DSIH1P21A_VID_MODE_CFG_tag_t {
        struct _DSIH1P21A_VID_MODE_CFG_map_t
        {
volatile unsigned int vid_mode_type             :
            2;    //[1:0]
            /*
            This field indicates the video mode transmission type as follows:
            00: Non-burst with sync pulses
            01: Non-burst with sync events
            10 and 11: Burst mode
            */
volatile unsigned int reserved_0             :
            6;    //[7:2]
volatile unsigned int lp_vsa_en             :
            1;    //[8] When set to 1, this bit enables the return to low-power inside the VSA period when timing allows.
volatile unsigned int lp_vbp_en             :
            1;    //[9] When set to 1, this bit enables the return to low-power inside the VBP period when timing allows.
volatile unsigned int lp_vfp_en             :
            1;    //[10] When set to 1, this bit enables the return to low-power inside the VFP period when timing allows.
volatile unsigned int lp_vact_en             :
            1;    //[11] When set to 1, this bit enables the return to low-power inside the VACT period when timing allows.
volatile unsigned int lp_hbp_en             :
            1;    //[12] When set to 1, this bit enables the return to low-power inside the HBP period when timing allows.
volatile unsigned int lp_hfp_en             :
            1;    //[13] When set to 1, this bit enables the return to low-power inside the HFP period when timing allows.
volatile unsigned int frame_bta_ack_en             :
            1;    //[14] When set to 1, this bit enables the request for an acknowledgeresponse at the end of a frame
volatile unsigned int lp_cmd_en             :
            1;    //[15] When set to 1, this bit enables the command transmission only in lowpower mode.
volatile unsigned int reserved_1             :
            16;    //[31:16]
        }
        mBits;
        volatile unsigned int dValue;
    }VID_MODE_CFG;// 0x0038

    union _DSIH1P21A_VID_PKT_SIZE_tag_t {
        struct _DSIH1P21A_VID_PKT_SIZE_map_t
        {
volatile unsigned int vid_pkt_size             :
            14;    //[13:0]  This field configures the number of pixels in a single video packet. For 18-bit not loosely packed data types, this number must be a multiple of 4. For YCbCr data types, it must be a multiple of 2, as described in the DSI specification.
volatile unsigned int reserved_0                :
            18; //[31:14] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }VID_PKT_SIZE;// 0x003C

    union _DSIH1P21A_VID_NUM_CHUNKS_tag_t {
        struct _DSIH1P21A_VID_NUM_CHUNKS_map_t
        {
volatile unsigned int vid_num_chunks             :
            13;    //[12:0]
            /*
            This register configures the number of chunks to be transmitted during
            a Line period (a chunk consists of a video packet and a null packet).
            If set to 0 or 1, the video line is transmitted in a single packet.
            If set to 1, the packet is part of a chunk, so a null packet follows it if
            vid_null_size > 0. Otherwise, multiple chunks are used to transmit each video line.
            */
volatile unsigned int reserved_0                :
            19; //[31:13] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }VID_NUM_CHUNKS;// 0x0040

    union _DSIH1P21A_VID_NULL_SIZE_tag_t {
        struct _DSIH1P21A_VID_NULL_SIZE_map_t
        {
volatile unsigned int vid_null_size             :
            13;    //[12:0]
            /*
            This register configures the number of bytes inside a null packet.
            Setting it to 0 disables the null packets.
            */
volatile unsigned int reserved_0                :
            19; //[31:13] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }VID_NULL_SIZE;// 0x0044

    union _DSIH1P21A_VID_HSA_TIME_tag_t {
        struct _DSIH1P21A_VID_HSA_TIME_map_t
        {
volatile unsigned int vid_hsa_time             :
            12;    //[11:0]  This field configures the Horizontal Synchronism Active period in lane byte clock cycles
volatile unsigned int reserved_0                :
            20; //[31:12] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }VID_HSA_TIME;// 0x0048

    union _DSIH1P21A_VID_HBP_TIME_tag_t {
        struct _DSIH1P21A_VID_HBP_TIME_map_t
        {
volatile unsigned int vid_hbp_time             :
            12;    //[11:0]  This field configures the Horizontal Back Porch period in lane byte clock cycles.
volatile unsigned int reserved_0                :
            20; //[31:12] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }VID_HBP_TIME;// 0x004C

    union _DSIH1P21A_VID_HLINE_TIME_tag_t {
        struct _DSIH1P21A_VID_HLINE_TIME_map_t
        {
volatile unsigned int vid_hline_time             :
            15;    //[14:0]  This field configures the size of the total line time (HSA+HBP+HACT+HFP) counted in lane byte clock cycles.
volatile unsigned int reserved_0                :
            17; //[31:15] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }VID_HLINE_TIME;// 0x0050

    union _DSIH1P21A_VID_VSA_LINES_tag_t {
        struct _DSIH1P21A_VID_VSA_LINES_map_t
        {
volatile unsigned int vsa_lines             :
            10;    //[9:0]  This field configures the Vertical Synchronism Active period measured in number of horizontal lines
volatile unsigned int reserved_0                :
            22; //[31:10] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }VID_VSA_LINES;// 0x0054

    union _DSIH1P21A_VID_VBP_LINES_tag_t {
        struct _DSIH1P21A_VID_VBP_LINES_map_t
        {
volatile unsigned int vbp_lines             :
            10;    //[9:0]  This field configures the Vertical Back Porch period measured in number of horizontal lines.
volatile unsigned int reserved_0                :
            22; //[31:10] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }VID_VBP_LINES;// 0x0058

    union _DSIH1P21A_VID_VFP_LINES_tag_t {
        struct _DSIH1P21A_VID_VFP_LINES_map_t
        {
volatile unsigned int vfp_lines             :
            10;    //[9:0]  This field configures the Vertical Front Porch period measured in number of horizontal lines.
volatile unsigned int reserved_0                :
            22; //[31:10] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }VID_VFP_LINES;// 0x005C

    union _DSIH1P21A_VID_VACTIVE_LINES_tag_t {
        struct _DSIH1P21A_VID_VACTIVE_LINES_map_t
        {
volatile unsigned int v_active_lines             :
            14;    //[13:0]  This field configures the Vertical Active period measured in number of horizontal lines.
volatile unsigned int reserved_0                :
            18; //[31:14] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }VID_VACTIVE_LINES;// 0x0060

    union _DSIH1P21A_EDPI_CMD_SIZE_tag_t {
        struct _DSIH1P21A_EDPI_CMD_SIZE_map_t
        {
volatile unsigned int edpi_allowed_cmd_size             :
            16;    //[15:0]  This field configures the maximum allowed size for an eDPI write memory command, measured in pixels. Automatic partitioning of data obtained from eDPI is permanently enabled.
volatile unsigned int reserved_0                :
            16; //[31:16] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }EDPI_CMD_SIZE;// 0x0064

    union _DSIH1P21A_CMD_MODE_CFG_tag_t {
        struct _DSIH1P21A_CMD_MODE_CFG_map_t
        {
volatile unsigned int tear_fx_en             :
            1;    //[0] When set to 1, this bit enables the tearing effect acknowledge request.
volatile unsigned int ack_rqst_en             :
            1;    //[1] When set to 1, this bit enables the acknowledge request after each packet transmission.
volatile unsigned int reserved_0             :
            6;    //[7:2]
volatile unsigned int gen_sw_0p_tx             :
            1;    //[8] This bit configures the Generic short write packet with zero parameter command transmission type:0: High-speed 1: Low-power
volatile unsigned int gen_sw_1p_tx             :
            1;    //[9] This bit configures the Generic short write packet with one parameter command transmission type: 0: High-speed 1: Low-power
volatile unsigned int gen_sw_2p_tx             :
            1;    //[10] This bit configures the Generic short write packet with two parameters command transmission type:0: High-speed 1: Low-power
volatile unsigned int gen_sr_0p_tx             :
            1;    //[11] This bit configures the Generic short read packet with zero parameter command transmission type:0: High-speed 1: Low-power
volatile unsigned int gen_sr_1p_tx             :
            1;    //[12] This bit configures the Generic short read packet with one parameter command transmission type:0: High-speed 1: Low-power
volatile unsigned int gen_sr_2p_tx             :
            1;    //[13] This bit configures the Generic short read packet with two parameters command transmission type:0: High-speed 1: Low-power
volatile unsigned int gen_lw_tx             :
            1;    //[14] This bit configures the Generic long write packet command transmission type:0: High-speed 1: Low-power
volatile unsigned int reserved_1             :
            1;    //[15]
volatile unsigned int dcs_sw_0p_tx             :
            1;    //[16] This bit configures the DCS short write packet with zero parameter command transmission type:0: High-speed 1: Low-power
volatile unsigned int dcs_sw_1p_tx             :
            1;    //[17] This bit configures the DCS short write packet with one parameter command transmission type:0: High-speed 1: Low-power
volatile unsigned int dcs_sr_0p_tx             :
            1;    //[18] This bit configures the DCS short read packet with zero parameter command transmission type:0: High-speed 1: Low-power
volatile unsigned int dcs_lw_tx             :
            1;    //[19] This bit configures the DCS long write packet command transmission type:0: High-speed 1: Low-power
volatile unsigned int reserved_2             :
            4;    //[23:20]
volatile unsigned int max_rd_pkt_size             :
            1;    //[24] This bit configures the maximum read packet size command transmission type:0: High-speed 1: Low-power
volatile unsigned int reserved_3             :
            7;    //[31:25]
        }
        mBits;
        volatile unsigned int dValue;
    }CMD_MODE_CFG;// 0x0068

    union _DSIH1P21A_GEN_HDR_tag_t {
        struct _DSIH1P21A_GEN_HDR_map_t
        {
volatile unsigned int gen_dt             :
            6;    //[5:0]  This field configures the packet data type of the header packet.
volatile unsigned int gen_vc                :
            2; //[7:6] Reserved
volatile unsigned int gen_wc_lsbyte             :
            8;    //[15:8]  This field configures the least significant byte of the header packet's Word count for long packets or data 0 for short packets.
volatile unsigned int gen_wc_msbyte                :
            8; //[23:16] This field configures the most significant byte of the header packet's word count for long packets or data 1 for short packets.
volatile unsigned int reserved_0                :
            8; //[31:24] Reserved
        }
        mBits;
        volatile unsigned int dValue;
    }GEN_HDR;// 0x006C

    union _DSIH1P21A_GEN_PLD_DATA_tag_t {
        struct _DSIH1P21A_GEN_PLD_DATA_map_t
        {
volatile unsigned int gen_pld_b1             :
            8;    //[7:0]  This field indicates byte 1 of the packet payload.
volatile unsigned int gen_pld_b2             :
            8;    //[15:8]  This field indicates byte 2 of the packet payload.
volatile unsigned int gen_pld_b3             :
            8;    //[23:16]  This field indicates byte 3 of the packet payload.
volatile unsigned int gen_pld_b4             :
            8;    //[31:24]  This field indicates byte 4 of the packet payload.
        }
        mBits;
        volatile unsigned int dValue;
    }GEN_PLD_DATA;// 0x0070

    union _DSIH1P21A_CMD_PKT_STATUS_tag_t {
        struct _DSIH1P21A_CMD_PKT_STATUS_map_t
        {
volatile unsigned int gen_cmd_empty             :
            1;    //[0]
            /*
            This bit indicates the empty status of the generic command FIFO.
            Dependency: DSI_GENERIC = 1. Otherwise, this bit is reserved.
            Value after reset: 0x1
            */
volatile unsigned int gen_cmd_full             :
            1;    //[1]
            /*
            This bit indicates the full status of the generic command FIFO.
            Dependency: DSI_GENERIC = 1. Otherwise, this bit is reserved.
            Value after reset: 0x0
            */
volatile unsigned int gen_pld_w_empty             :
            1;    //[2]
            /*
            This bit indicates the empty status of the generic write payload FIFO.
            Dependency: DSI_GENERIC = 1. Otherwise, this bit is reserved.
            Value after reset: 0x1
            */
volatile unsigned int gen_pld_w_full             :
            1;    //[3]
            /*
            This bit indicates the full status of the generic write payload FIFO.
            Dependency: DSI_GENERIC = 1. Otherwise, this bit is reserved.
            Value after reset: 0x0
            */
volatile unsigned int gen_pld_r_empty             :
            1;    //[4]
            /*
            This bit indicates the empty status of the generic read payload FIFO.
            Dependency: DSI_GENERIC = 1. Otherwise, this bit is reserved.
            Value after reset: 0x1
            */
volatile unsigned int gen_pld_r_full             :
            1;    //[5]
            /*
            This bit indicates the full status of the generic read payload FIFO.
            Dependency: DSI_GENERIC = 1. Otherwise, this bit is reserved.
            Value after reset: 0x0
            */
volatile unsigned int gen_rd_cmd_busy             :
            1;    //[6]
            /*
            This bit is set when a read command is issued and cleared when the
            entire response is stored in the FIFO.
            Dependency: DSI_GENERIC = 1. Otherwise, this bit is reserved.
            Value after reset: 0x0
            */
volatile unsigned int reserved_0             :
            1;    //[7]
            /*
            */
volatile unsigned int dbi_cmd_empy             :
            1;    //[8]
            /*
            This bit indicates the empty status of the DBI command FIFO.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3.
            Otherwise, this bit is reserved.
            Value after reset: 0x1
            */
volatile unsigned int dbi_cmd_full             :
            1;    //[9]
            /*
            This bit indicates the full status of the DBI command FIFO.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3.
            Otherwise, this bit is reserved.
            Value after reset: 0x0
            */
volatile unsigned int dbi_pld_w_empty             :
            1;    //[10]
            /*
            This bit indicates the empty status of the DBI write payload FIFO.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3.
            Otherwise, this bit is reserved.
            Value after reset: 0x1
            */
volatile unsigned int dbi_pld_w_full             :
            1;    //[11]
            /*
            This bit indicates the full status of the DBI write payload FIFO.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3.
            Otherwise, this bit is reserved.
            Value after reset: 0x0
            */
volatile unsigned int dbi_pld_r_empty             :
            1;    //[12]
            /*
            This bit indicates the empty status of the DBI read payload FIFO.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3.
            Otherwise, this bit is reserved.
            Value after reset: 0x1
            */
volatile unsigned int dbi_pld_r_full             :
            1;    //[13]
            /*
            This bit indicates the full status of the DBI read payload FIFO.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3.
            Otherwise, this bit is reserved.
            Value after reset: 0x0
            */
volatile unsigned int dbi_rd_cmd_busy             :
            1;    //[14]
            /*
            This bit is set when a read command is issued and cleared when the
            entire response is stored in the FIFO.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3.
            Otherwise, this bit is reserved.
            Value after reset: 0x0
            */
volatile unsigned int reserved_1             :
            17;    //[31:15]
        }
        mBits;
        volatile unsigned int dValue;
    }
    CMD_PKT_STATUS;// 0x0074

    union _DSIH1P21A_TO_CNT_CFG_tag_t {
        struct _DSIH1P21A_TO_CNT_CFG_map_t
        {
volatile unsigned int lprx_to_cnt             :
            16;    //[15:0]
            /*
            This field configures the timeout counter that triggers a low-power
            reception timeout contention detection (measured in
            TO_CLK_DIVISION cycles).
            */
volatile unsigned int hstx_to_cnt                :
            16; //[31:16]
            /*
            This field configures the timeout counter that triggers a high-speed
            transmission timeout contention detection (measured in
            TO_CLK_DIVISION cycles).
            If using the non-burst mode and there is no sufficient time to switch
            from HS to LP and back in the period which is from one line data
            finishing to the next line sync start, the DSI link returns the LP state
            once per frame, then you should configure the TO_CLK_DIVISION
            and hstx_to_cnt to be in accordance with:
            hstx_to_cnt * lanebyteclkperiod * TO_CLK_DIVISION >= the time of
            one FRAME data transmission * (1 + 10%)
            In burst mode, RGB pixel packets are time-compressed, leaving more
            time during a scan line. Therefore, if in burst mode and there is
            sufficient time to switch from HS to LP and back in the period of time
            from one line data finishing to the next line sync start, the DSI link can
            return LP mode and back in this time interval to save power. For this,
            configure the TO_CLK_DIVISION and hstx_to_cnt to be in accordance
            with:
            hstx_to_cnt * lanebyteclkperiod * TO_CLK_DIVISION >= the time of
            one LINE data transmission * (1 + 10%)
            */
        }
        mBits;
        volatile unsigned int dValue;
    }
    TO_CNT_CFG;// 0x0078

    union _DSIH1P21A_HS_RD_TO_CNT_tag_t {
        struct _DSIH1P21A_HS_RD_TO_CNT_map_t
        {
volatile unsigned int hs_rd_to_cnt             :
            16;    //[15:0]
            /*
            This field sets a period for which the DWC_mipi_dsi_host keeps the
            link still, after sending a high-speed read operation. This period is
            measured in cycles of lanebyteclk. The counting starts when the
            D-PHY enters the Stop state and causes no interrupts.
            */
volatile unsigned int reserved_0                :
            16; //[31:16]
        }
        mBits;
        volatile unsigned int dValue;
    }
    HS_RD_TO_CNT;// 0x007C

    union _DSIH1P21A_LP_RD_TO_CNT_tag_t {
        struct _DSIH1P21A_LP_RD_TO_CNT_map_t
        {
volatile unsigned int lp_rd_to_cnt             :
            16;    //[15:0]
            /*
            This field sets a period for which the DWC_mipi_dsi_host keeps the
            link still, after sending a low-power read operation. This period is
            measured in cycles of lanebyteclk. The counting starts when the
            D-PHY enters the Stop state and causes no interrupts.
            */
volatile unsigned int reserved_0                :
            16; //[31:16]
        }
        mBits;
        volatile unsigned int dValue;
    }
    LP_RD_TO_CNT;// 0x0080

    union _DSIH1P21A_HS_WR_TO_CNT_tag_t {
        struct _DSIH1P21A_HS_WR_TO_CNT_map_t
        {
volatile unsigned int hs_wr_to_cnt             :
            16;    //[15:0]
            /*
            This field sets a period for which the DWC_mipi_dsi_host keeps the
            link inactive after sending a high-speed write operation. This period is
            measured in cycles of lanebyteclk. The counting starts when the
            D-PHY enters the Stop state and causes no interrupts.
            */
volatile unsigned int reserved_0                :
            8; //[23:16]
volatile unsigned int presp_to_mode             :
            1;    //[24]
            /*
            When set to 1, this bit ensures that the peripheral response timeout
            caused by hs_wr_to_cnt is used only once per eDPI frame, when both
            the following conditions are met:
            dpivsync_edpiwms has risen and fallen.
            Packets originated from eDPI have been transmitted and its FIFO
            is empty again.
            In this scenario no non-eDPI requests are sent to the D-PHY, even if
            there is traffic from generic or DBI ready to be sent, making it return to
            stop state. When it does so, PRESP_TO counter is activated and only
            when it finishes does the controller send any other traffic that is ready.
            Dependency: DSI_DATAINTERFACE = 4. Otherwise, this bit is
            reserved.
            */
volatile unsigned int reserved_1                :
            7; //[31:25]
        }
        mBits;
        volatile unsigned int dValue;
    }
    HS_WR_TO_CNT;// 0x0084

    union _DSIH1P21A_LP_WR_TO_CNT_tag_t {
        struct _DSIH1P21A_LP_WR_TO_CNT_map_t
        {
volatile unsigned int lp_wr_to_cnt             :
            16;    //[15:0]
            /*
            This field sets a period for which the DWC_mipi_dsi_host keeps the
            link still, after sending a low-power write operation. This period is
            measured in cycles of lanebyteclk. The counting starts when the
            D-PHY enters the Stop state and causes no interrupts
            */
volatile unsigned int reserved_0                :
            16; //[31:16]
        }
        mBits;
        volatile unsigned int dValue;
    }
    LP_WR_TO_CNT;// 0x0088

    union _DSIH1P21A_BTA_TO_CNT_tag_t {
        struct _DSIH1P21A_BTA_TO_CNT_map_t
        {
volatile unsigned int bta_to_cnt             :
            16;    //[15:0]
            /*
            This field sets a period for which the DWC_mipi_dsi_host keeps the
            link still, after completing a Bus Turn-Around. This period is measured
            in cycles of lanebyteclk. The counting starts when the D-PHY enters
            the Stop state and causes no interrupts.
            */
volatile unsigned int reserved_0                :
            16; //[31:16]
        }
        mBits;
        volatile unsigned int dValue;
    }
    BTA_TO_CNT;// 0x008C

    union _DSIH1P21A_SDF_3D_tag_t {
        struct _DSIH1P21A_SDF_3D_map_t
        {
volatile unsigned int mode_3d             :
            2;    //[1:0]
            /*
            This field defines the 3D mode on/off and display orientation:
            00: 3D mode off (2D mode on)
            01: 3D mode on, portrait orientation
            10: 3D mode on, landscape orientation
            11: Reserved

            */
volatile unsigned int ormat_3d             :
            2;    //[3:2]
            /*
            This field defines the 3D image format:
            00: Line (alternating lines of left and right data)
            01: Frame (alternating frames of left and right data)
            10: Pixel (alternating pixels of left and right data)
            11: Reserved

            */
volatile unsigned int second_vsync             :
            1;    //[4]
            /*
            This field defines whether there is a second VSYNC pulse between
            Left and Right Images, when 3D Image Format is Frame-based:
            0: No sync pulses between left and right data
            1: Sync pulse (HSYNC, VSYNC, blanking) between left and right
            data
            */
volatile unsigned int right_first             :
            1;    //[5]
            /*
            This bit defines the left or right order:
            0: Left eye data is sent first, and then the right eye data is sent.
            1: Right eye data is sent first, and then the left eye data is sent.
            */
volatile unsigned int reserved_0             :
            10;    //[15:6]
volatile unsigned int send_3d_cfg             :
            1;    //[16]
            /*
            When set, causes the next VSS packet to include 3D control payload
            in every VSS packet.
            */
volatile unsigned int reserved_1                :
            15; //[31:17]
        }
        mBits;
        volatile unsigned int dValue;
    }
    SDF_3D;// 0x0090

    union _DSIH1P21A_LPCLK_CTRL_tag_t {
        struct _DSIH1P21A_LPCLK_CTRL_map_t
        {
volatile unsigned int phy_txrequestclkhs             :
            1;    //[0] This bit controls the D-PHY PPI txrequestclkhs signal
volatile unsigned int auto_clklane_ctrl             :
            1;    //[0] This bit enables the automatic mechanism to stop providing clock in the clock lane when time allows
volatile unsigned int reserved_0                :
            30; //[31:2]
        }
        mBits;
        volatile unsigned int dValue;
    }
    LPCLK_CTRL;// 0x0094

    union _DSIH1P21A_PHY_TMR_LPCLK_CFG_tag_t {
        struct _DSIH1P21A_PHY_TMR_LPCLK_CFG_map_t
        {
volatile unsigned int phy_clklp2hs_time             :
            10;    //[9:0]
            /*
            This field configures the maximum time that the D-PHY clock lane
            takes to go from low-power to high-speed transmission measured in
            lane byte clock cycles.
            */
volatile unsigned int reserved_0                :
            6; //[15:10]
volatile unsigned int phy_clkhs2lp_time             :
            10;    //[25:16]
            /*
            This field configures the maximum time that the D-PHY clock lane
            takes to go from high-speed to low-power transmission measured in
            lane byte clock cycles.
            */
volatile unsigned int reserved_1                :
            6; //[31:26]
        }
        mBits;
        volatile unsigned int dValue;
    }
    PHY_TMR_LPCLK_CFG;// 0x0098

    union _DSIH1P21A_PHY_TMR_CFG_tag_t {
        struct _DSIH1P21A_PHY_TMR_CFG_map_t
        {
volatile unsigned int max_rd_time             :
            15;    //[14:0]
            /*
            This field configures the maximum time required to perform a read
            command in lane byte clock cycles. This register can only be modified
            when no read command is in progress.
            */
volatile unsigned int reserved_0                :
            1; //[15]
volatile unsigned int phy_lp2hs_time             :
            8;    //[23:16]
            /*
            This field configures the maximum time that the D-PHY data lanes
            take to go from low-power to high-speed transmission measured in
            lane byte clock cycles.
            */
volatile unsigned int phy_hs2lp_time                :
            8; //[31:24]
            /*
            This field configures the maximum time that the D-PHY data lanes
            take to go from high-speed to low-power transmission measured in
            lane byte clock cycles.
            */
        }
        mBits;
        volatile unsigned int dValue;
    }    PHY_TMR_CFG;// 0x009C

    union _DSIH1P21A_PHY_RSTZ_tag_t {
        struct _DSIH1P21A_PHY_RSTZ_map_t
        {
volatile unsigned int phy_shutdownz             :
            1;    //[0] When set to 0, this bit places the D-PHY macro in power-down state
volatile unsigned int phy_rstz                :
            1; //[1] When set to 0, this bit places the digital section of the D-PHY in the reset state.
volatile unsigned int phy_enableclk             :
            1;    //[2] When set to1, this bit enables the D-PHY Clock Lane module.
volatile unsigned int phy_forcepll                :
            1; //[3]
            /*
            When the D-PHY is in ULPS, this bit enables the D-PHY PLL.
            Dependency: DSI_HOST_FPGA = 0. Otherwise, this bit is reserved
            */
volatile unsigned int reserved_0                :
            28; //[31:4]
        }
        mBits;
        volatile unsigned int dValue;

    }    PHY_RSTZ;// 0x00A0

    union _DSIH1P21A_PHY_IF_CFG_tag_t {
        struct _DSIH1P21A_PHY_IF_CFG_map_t
        {
volatile unsigned int n_lanes             :
            2;    //[1:0]
            /*
            This field configures the number of active data lanes:
            00: One data lane (lane 0)
            01: Two data lanes (lanes 0 and 1)
            10: Three data lanes (lanes 0, 1, and 2)
            11: Four data lanes (lanes 0, 1, 2, and 3)
            */
volatile unsigned int reserved_0                :
            6; //[7:2]
volatile unsigned int phy_stop_wait_time                :
            8; //[15:8] This field configures the minimum wait period to request a high-speed transmission after the Stop state.
volatile unsigned int reserved_1                :
            16; //[31:16]
        }
        mBits;
        volatile unsigned int dValue;
    }PHY_IF_CFG;// 0x00A4

    union _DSIH1P21A_PHY_ULPS_CTRL_tag_t {
        struct _DSIH1P21A_PHY_ULPS_CTRL_map_t
        {
volatile unsigned int phy_txrequlpsclk             :
            1;    //[0] ULPS mode Request on clock lane.
volatile unsigned int phy_txexitulpsclk                :
            1; //[1] ULPS mode Exit on clock lane.
volatile unsigned int phy_txrequlpslan             :
            1;    //[2] ULPS mode Request on all active data lanes.
volatile unsigned int phy_txexitulpslan                :
            1; //[3] ULPS mode Exit on all active data lanes.
volatile unsigned int reserved_0                :
            28; //[31:4]
        }
        mBits;
        volatile unsigned int dValue;
    }
    PHY_ULPS_CTRL;// 0x00A8

    union _DSIH1P21A_PHY_TX_TRIGGERS_tag_t {
        struct _DSIH1P21A_PHY_TX_TRIGGERS_map_t
        {
volatile unsigned int phy_tx_triggers             :
            4;    //[3:0] This field controls the trigger transmissions
volatile unsigned int reserved_0                :
            28; //[31:4]
        }
        mBits;
        volatile unsigned int dValue;
    }
    PHY_TX_TRIGGERS;// 0x00AC

    union _DSIH1P21A_PHY_STATUS_tag_t {
        struct _DSIH1P21A_PHY_STATUS_map_t
        {
volatile unsigned int phy_lock             :
            1;    //[0] This bit indicates the status of phylock D-PHY signal.
volatile unsigned int phy_direction                :
            1; //[1] This bit indicates the status of phydirection D-PHY signal.
volatile unsigned int phy_stopstateclklane             :
            1;    //[2] This bit indicates the status of phystopstateclklane D-PHY signal.
volatile unsigned int phy_ulpsactivenotclk                :
            1; //[3] This bit indicates the status of phyulpsactivenotclk D-PHY signal.
volatile unsigned int phy_stopstate0lane             :
            1;    //[4] This bit indicates the status of phystopstate0lane D-PHY signal.
volatile unsigned int phy_ulpsactivenot0lane                :
            1; //[5] This bit indicates the status of ulpsactivenot0lane D-PHY signal.
volatile unsigned int phy_rxulpsesc0lane             :
            1;    //[6] This bit indicates the status of rxulpsesc0lane D-PHY signal.
volatile unsigned int phy_stopstate1lane                :
            1; //[7]
            /*
            This bit indicates the status of phystopstate1lane D-PHY signal.
            Dependency: DSI_HOST_NUMBER_OF_LANES > 1
            If DSI_HOST_NUMBER_OF_LANES <= 1, this bit is reserved.
            */
volatile unsigned int phy_ulpsactivenot1lane             :
            1;    //[8]
            /*
            This bit indicates the status of ulpsactivenot1lane D-PHY signal.
            Dependency: DSI_HOST_NUMBER_OF_LANES > 1
            If DSI_HOST_NUMBER_OF_LANES <= 1, this bit is reserved.
            */
volatile unsigned int phy_stopstate2lane                :
            1; //[9]
            /*
            This bit indicates the status of phystopstate2lane D-PHY signal.
            Dependency: DSI_HOST_NUMBER_OF_LANES > 2
            If DSI_HOST_NUMBER_OF_LANES <= 2, this bit is reserved.
            */
volatile unsigned int phy_ulpsactivenot2lane             :
            1;    //[10]
            /*
            This bit indicates the status of ulpsactivenot2lane D-PHY signal.
            Dependency: DSI_HOST_NUMBER_OF_LANES > 2
            If DSI_HOST_NUMBER_OF_LANES <= 2, this bit is reserved.
            */
volatile unsigned int phy_stopstate3lane                :
            1; //[11]
            /*
            This bit indicates the status of phystopstate3lane D-PHY signal.
            Dependency: DSI_HOST_NUMBER_OF_LANES > 3
            If DSI_HOST_NUMBER_OF_LANES <= 3, this bit is reserved
            */
volatile unsigned int phy_ulpsactivenot3lane             :
            1;    //[12]
            /*
            This bit indicates the status of ulpsactivenot3lane D-PHY signal.
            Dependency: DSI_HOST_NUMBER_OF_LANES > 3
            If DSI_HOST_NUMBER_OF_LANES <= 3, this bit is reserved
            */
volatile unsigned int reserved_0                :
            19; //[31:13]
        }
        mBits;
        volatile unsigned int dValue;
    } PHY_STATUS;// 0x00B0

    union _DSIH1P21A_PHY_TST_CTRL0_tag_t {
        struct _DSIH1P21A_PHY_TST_CTRL0_map_t
        {
volatile unsigned int phy_testclr             :
            1;    //[0] PHY test interface clear (active high).
volatile unsigned int phy_testclk             :
            1;    //[1] This bit is used to clock the TESTDIN bus into the D-PHY.
volatile unsigned int reserved_0                :
            30; //[31:2]
        }
        mBits;
        volatile unsigned int dValue;
    }
    PHY_TST_CTRL0;// 0x00B4

    union _DSIH1P21A_PHY_TST_CTRL1_tag_t {
        struct _DSIH1P21A_PHY_TST_CTRL1_map_t
        {
volatile unsigned int phy_testdin             :
            8;    //[7:0] PHY test interface input 8-bit data bus for internal register programming and test functionalities access.
volatile unsigned int pht_testdout             :
            8;    //[15:8] PHY output 8-bit data bus for read-back and internal probing functionalities.
volatile unsigned int phy_testen                :
            1; //[16]
            /*
            PHY test interface operation selector:
            1: The address write operation is set on the falling edge of the testclk signal.
            0: The data write operation is set on the rising edge of the testclk signal.
            */
volatile unsigned int reserved_0                :
            15; //[31:17]
        }
        mBits;
        volatile unsigned int dValue;
    }
    PHY_TST_CTRL1;// 0x00B8

    union _DSIH1P21A_INT_ST0_tag_t {
        struct _DSIH1P21A_INT_ST0_map_t
        {
volatile unsigned int ack_with_err_0             :
            1;    //[0] This bit retrieves the SoT error from the Acknowledge error report.
volatile unsigned int ack_with_err_1                :
            1; //[1] This bit retrieves the SoT Sync error from the Acknowledge error report.
volatile unsigned int ack_with_err_2             :
            1;    //[2] This bit retrieves the EoT Sync error from the Acknowledge error report.
volatile unsigned int ack_with_err_3                :
            1; //[3] This bit retrieves the Escape Mode Entry Command error from the Acknowledge error report.
volatile unsigned int ack_with_err_4             :
            1;    //[4] This bit retrieves the LP Transmit Sync error from the Acknowledge error report.
volatile unsigned int ack_with_err_5                :
            1; //[5] This bit retrieves the Peripheral Timeout error from the Acknowledge Error report.
volatile unsigned int ack_with_err_6             :
            1;    //[6] This bit retrieves the False Control error from the Acknowledge error report.
volatile unsigned int ack_with_err_7                :
            1; //[7] This bit retrieves the reserved (specific to device) from the Acknowledge error report.
volatile unsigned int ack_with_err_8             :
            1;    //[8] This bit retrieves the ECC error, single-bit (detected and corrected) from the Acknowledge error report.
volatile unsigned int ack_with_err_9                :
            1; //[9] This bit retrieves the ECC error, multi-bit (detected, not corrected) from the Acknowledge error report.
volatile unsigned int ack_with_err_10             :
            1;    //[10]This bit retrieves the checksum error (long packet only) from the Acknowledge error report.
volatile unsigned int ack_with_err_11                :
            1; //[11] This bit retrieves the not recognized DSI data type from the Acknowledge error report.
volatile unsigned int ack_with_err_12             :
            1;    //[12] This bit retrieves the DSI VC ID Invalid from the Acknowledge error report.
volatile unsigned int ack_with_err_13                :
            1; //[13] This bit retrieves the invalid transmission length from the Acknowledge error report.
volatile unsigned int ack_with_err_14             :
            1;    //[14] This bit retrieves the reserved (specific to device) from the Acknowledge error report
volatile unsigned int ack_with_err_15                :
            1; //[15] This bit retrieves the DSI protocol violation from the Acknowledge error report.
volatile unsigned int dphy_errors_0             :
            1;    //[16] This bit indicates ErrEsc escape entry error from Lane 0.
volatile unsigned int dphy_errors_1                :
            1; //[17] This bit indicates ErrSyncEsc low-power data transmission synchronization error from Lane 0.
volatile unsigned int dphy_errors_2             :
            1;    //[18] This bit indicates the ErrControl error from Lane 0.
volatile unsigned int dphy_errors_3                :
            1; //[19] This bit indicates the LP0 contention error ErrContentionLP0 from Lane 0.
volatile unsigned int dphy_errors_4             :
            1;    //[20] This bit indicates the LP1 contention error ErrContentionLP1 from Lane 0.
volatile unsigned int reserved_0                :
            11; //[31:21]
        }
        mBits;
        volatile unsigned int dValue;
    }
    INT_ST0;// 0x00BC

    union _DSIH1P21A_INT_ST1_tag_t {
        struct _DSIH1P21A_INT_ST1_map_t
        {
volatile unsigned int to_hs_tx             :
            1;    //[0]
            /*
            This bit indicates that the high-speed transmission timeout counter
            reached the end and contention is detected.
            */
volatile unsigned int to_lp_rx                :
            1; //[1]
            /*
            This bit indicates that the low-power reception timeout counter reached
            the end and contention is detected.
            */
volatile unsigned int ecc_single_err             :
            1;    //[2]
            /*
            This bit indicates that the ECC single error is detected and corrected in a
            received packet.
            */
volatile unsigned int ecc_multi_err                :
            1; //[3]
            /*
            This bit indicates that the ECC multiple error is detected in a received
            packet.
            */
volatile unsigned int crc_err             :
            1;    //[4]
            /*
            This bit indicates that the CRC error is detected in the received packet
            payload.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3 or
            DSI_GENERIC = 1. Otherwise, this bit is reserved.
            */
volatile unsigned int pkt_size_err                :
            1; //[5]
            /*
            This bit indicates that the packet size error is detected during the packet
            reception.
            */
volatile unsigned int eopt_err             :
            1;    //[6]
            /*
            This bit indicates that the EoTp packet is not received at the end of the
            incoming peripheral transmission
            */
volatile unsigned int dpi_pld_wr_err                :
            1; //[7]
            /*
            This bit indicates that during a DPI pixel line storage, the payload FIFO
            becomes full and the data stored is corrupted.
            Dependency: DSI_DATAINTERFACE = 2 or DSI_DATAINTERFACE = 3 or
            DSI_DATAINTERFACE = 4. Otherwise, this bit is reserved.
            */
volatile unsigned int gen_cmd_wr_err             :
            1;    //[8]
            /*
            This bit indicates that the system tried to write a command through the
            Generic interface and the FIFO is full. Therefore, the command is not
            written.
            Dependency: DSI_GENERIC = 1. Otherwise, this bit is reserved
            */
volatile unsigned int gen_pld_wr_err                :
            1; //[9]
            /*
            This bit indicates that the system tried to write a payload data through the
            Generic interface and the FIFO is full. Therefore, the payload is not
            written.
            Dependency: DSI_GENERIC = 1. Otherwise, this bit is reserved
            */
volatile unsigned int gen_pld_send_err             :
            1; //[10]
            /*
            This bit indicates that during a Generic interface packet build, the payload
            FIFO becomes empty and corrupt data is sent.
            Dependency: DSI_GENERIC = 1. Otherwise, this bit is reserved
            */
volatile unsigned int gen_pld_rd_err                :
            1; //[11]
            /*
            This bit indicates that during a DCS read data, the payload FIFO becomes
            empty and the data sent to the interface is corrupted.
            Dependency: DSI_GENERIC = 1. Otherwise, this bit is reserved.
            */
volatile unsigned int gen_pld_recev_err             :
            1;    //[12]
            /*
            This bit indicates that during a generic interface packet read back, the
            payload FIFO becomes full and the received data is corrupted.
            Dependency: DSI_GENERIC = 1
            If DSI_GENERIC = 0, this bit is reserved.
            */
volatile unsigned int dbi_cmd_wr_err                :
            1; //[13]
            /*
            This bit indicates that the system tried to write a command through the
            DBI but the command FIFO is full. Therefore, the command is not written.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3.
            Otherwise, this bit is reserved.
            */
volatile unsigned int dbi_pld_wr_err             :
            1;    //[14]
            /*
            This bit indicates that the system tried to write the payload data through
            the DBI interface and the FIFO is full. Therefore, the command is not
            written.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3.
            Otherwise, this bit is reserved.
            */
volatile unsigned int dbi_pld_rd_err                :
            1; //[15]
            /*
            This bit indicates that during a DCS read data, the payload FIFO goes
            empty and the data sent to the interface is corrupted.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3.
            Otherwise, this bit is reserved.
            */
volatile unsigned int dbi_pld_recv_err             :
            1;    //[16]
            /*
            This bit indicates that during a DBI read back packet, the payload FIFO
            becomes full and the received data is corrupted.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3.
            Otherwise, this bit is reserved.
            */

volatile unsigned int dbi_ilegal_comm_err                :
            1; //[17]
            /*
            This bit indicates that an attempt to write an illegal command on the DBI
            interface is made and the core is blocked by transmission.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3.
            Otherwise, this bit is reserved.
            */
volatile unsigned int reserved_0                :
            14; //[31:18]
        }
        mBits;
        volatile unsigned int dValue;
    }
    INT_ST1;// 0x00C0


    union _DSIH1P21A_INT_MSK0_tag_t {
        struct _DSIH1P21A_INT_MSK0_map_t
        {
volatile unsigned int ack_with_err_0             :
            1;    //[0] This bit retrieves the SoT error from the Acknowledge error report.
volatile unsigned int ack_with_err_1                :
            1; //[1] This bit retrieves the SoT Sync error from the Acknowledge error report.
volatile unsigned int ack_with_err_2             :
            1;    //[2] This bit retrieves the EoT Sync error from the Acknowledge error report.
volatile unsigned int ack_with_err_3                :
            1; //[3] This bit retrieves the Escape Mode Entry Command error from the Acknowledge error report.
volatile unsigned int ack_with_err_4             :
            1;    //[4] This bit retrieves the LP Transmit Sync error from the Acknowledge error report.
volatile unsigned int ack_with_err_5                :
            1; //[5] This bit retrieves the Peripheral Timeout error from the Acknowledge Error report.
volatile unsigned int ack_with_err_6             :
            1;    //[6] This bit retrieves the False Control error from the Acknowledge error report.
volatile unsigned int ack_with_err_7                :
            1; //[7] This bit retrieves the reserved (specific to device) from the Acknowledge error report.
volatile unsigned int ack_with_err_8             :
            1;    //[8] This bit retrieves the ECC error, single-bit (detected and corrected) from the Acknowledge error report.
volatile unsigned int ack_with_err_9                :
            1; //[9] This bit retrieves the ECC error, multi-bit (detected, not corrected) from the Acknowledge error report.
volatile unsigned int ack_with_err_10             :
            1;    //[10]This bit retrieves the checksum error (long packet only) from the Acknowledge error report.
volatile unsigned int ack_with_err_11                :
            1; //[11] This bit retrieves the not recognized DSI data type from the Acknowledge error report.
volatile unsigned int ack_with_err_12             :
            1;    //[12] This bit retrieves the DSI VC ID Invalid from the Acknowledge error report.
volatile unsigned int ack_with_err_13                :
            1; //[13] This bit retrieves the invalid transmission length from the Acknowledge error report.
volatile unsigned int ack_with_err_14             :
            1;    //[14] This bit retrieves the reserved (specific to device) from the Acknowledge error report
volatile unsigned int ack_with_err_15                :
            1; //[15] This bit retrieves the DSI protocol violation from the Acknowledge error report.
volatile unsigned int dphy_errors_0             :
            1;    //[16] This bit indicates ErrEsc escape entry error from Lane 0.
volatile unsigned int dphy_errors_1                :
            1; //[17] This bit indicates ErrSyncEsc low-power data transmission synchronization error from Lane 0.
volatile unsigned int dphy_errors_2             :
            1;    //[18] This bit indicates the ErrControl error from Lane 0.
volatile unsigned int dphy_errors_3                :
            1; //[19] This bit indicates the LP0 contention error ErrContentionLP0 from Lane 0.
volatile unsigned int dphy_errors_4             :
            1;    //[20] This bit indicates the LP1 contention error ErrContentionLP1 from Lane 0.
volatile unsigned int reserved_0                :
            11; //[31:21]
        }
        mBits;
        volatile unsigned int dValue;
    }
    INT_MSK0;// 0x00C4


    union _DSIH1P21A_INT_MSK1_tag_t {
        struct _DSIH1P21A_INT_MSK1_map_t
        {
volatile unsigned int to_hs_tx             :
            1;    //[0]
            /*
            This bit indicates that the high-speed transmission timeout counter
            reached the end and contention is detected.
            */
volatile unsigned int to_lp_rx                :
            1; //[1]
            /*
            This bit indicates that the low-power reception timeout counter reached
            the end and contention is detected.
            */
volatile unsigned int ecc_single_err             :
            1;    //[2]
            /*
            This bit indicates that the ECC single error is detected and corrected in a
            received packet.
            */
volatile unsigned int ecc_multi_err                :
            1; //[3]
            /*
            This bit indicates that the ECC multiple error is detected in a received
            packet.
            */
volatile unsigned int crc_err             :
            1;    //[4]
            /*
            This bit indicates that the CRC error is detected in the received packet
            payload.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3 or
            DSI_GENERIC = 1. Otherwise, this bit is reserved.
            */
volatile unsigned int pkt_size_err                :
            1; //[5]
            /*
            This bit indicates that the packet size error is detected during the packet
            reception.
            */
volatile unsigned int eopt_err             :
            1;    //[6]
            /*
            This bit indicates that the EoTp packet is not received at the end of the
            incoming peripheral transmission
            */
volatile unsigned int dpi_pld_wr_err                :
            1; //[7]
            /*
            This bit indicates that during a DPI pixel line storage, the payload FIFO
            becomes full and the data stored is corrupted.
            Dependency: DSI_DATAINTERFACE = 2 or DSI_DATAINTERFACE = 3 or
            DSI_DATAINTERFACE = 4. Otherwise, this bit is reserved.
            */
volatile unsigned int gen_cmd_wr_err             :
            1;    //[8]
            /*
            This bit indicates that the system tried to write a command through the
            Generic interface and the FIFO is full. Therefore, the command is not
            written.
            Dependency: DSI_GENERIC = 1. Otherwise, this bit is reserved
            */
volatile unsigned int gen_pld_wr_err                :
            1; //[9]
            /*
            This bit indicates that the system tried to write a payload data through the
            Generic interface and the FIFO is full. Therefore, the payload is not
            written.
            Dependency: DSI_GENERIC = 1. Otherwise, this bit is reserved
            */
volatile unsigned int gen_pld_send_err             :
            1; //[10]
            /*
            This bit indicates that during a Generic interface packet build, the payload
            FIFO becomes empty and corrupt data is sent.
            Dependency: DSI_GENERIC = 1. Otherwise, this bit is reserved
            */
volatile unsigned int gen_pld_rd_err                :
            1; //[11]
            /*
            This bit indicates that during a DCS read data, the payload FIFO becomes
            empty and the data sent to the interface is corrupted.
            Dependency: DSI_GENERIC = 1. Otherwise, this bit is reserved.
            */
volatile unsigned int gen_pld_recev_err             :
            1;    //[12]
            /*
            This bit indicates that during a generic interface packet read back, the
            payload FIFO becomes full and the received data is corrupted.
            Dependency: DSI_GENERIC = 1
            If DSI_GENERIC = 0, this bit is reserved.
            */
volatile unsigned int dbi_cmd_wr_err                :
            1; //[13]
            /*
            This bit indicates that the system tried to write a command through the
            DBI but the command FIFO is full. Therefore, the command is not written.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3.
            Otherwise, this bit is reserved.
            */
volatile unsigned int dbi_pld_wr_err             :
            1;    //[14]
            /*
            This bit indicates that the system tried to write the payload data through
            the DBI interface and the FIFO is full. Therefore, the command is not
            written.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3.
            Otherwise, this bit is reserved.
            */
volatile unsigned int dbi_pld_rd_err                :
            1; //[15]
            /*
            This bit indicates that during a DCS read data, the payload FIFO goes
            empty and the data sent to the interface is corrupted.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3.
            Otherwise, this bit is reserved.
            */
volatile unsigned int dbi_pld_recv_err             :
            1;    //[16]
            /*
            This bit indicates that during a DBI read back packet, the payload FIFO
            becomes full and the received data is corrupted.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3.
            Otherwise, this bit is reserved.
            */

volatile unsigned int dbi_ilegal_comm_err                :
            1; //[17]
            /*
            This bit indicates that an attempt to write an illegal command on the DBI
            interface is made and the core is blocked by transmission.
            Dependency: DSI_DATAINTERFACE = 1 or DSI_DATAINTERFACE = 3.
            Otherwise, this bit is reserved.
            */
volatile unsigned int reserved_0                :
            14; //[31:18]
        }
        mBits;
        volatile unsigned int dValue;
    }
    INT_MSK1;// 0x00C8
}DSIH1P21A_REG_T;

uint32_t mipi_dsih_hal_get_version(dsih_ctrl_t * instance);

void mipi_dsih_hal_power(dsih_ctrl_t * instance, int on);

int mipi_dsih_hal_get_power(dsih_ctrl_t * instance);

void mipi_dsih_hal_tx_escape_division(dsih_ctrl_t * instance, uint8_t tx_escape_division);

void mipi_dsih_hal_dpi_video_vc(dsih_ctrl_t * instance, uint8_t vc);

uint8_t mipi_dsih_hal_dpi_get_video_vc(dsih_ctrl_t * instance);

dsih_error_t mipi_dsih_hal_dpi_color_coding(dsih_ctrl_t * instance, dsih_color_coding_t color_coding);
dsih_color_coding_t mipi_dsih_hal_dpi_get_color_coding(dsih_ctrl_t * instance);
uint8_t mipi_dsih_hal_dpi_get_color_depth(dsih_ctrl_t * instance);
uint8_t mipi_dsih_hal_dpi_get_color_config(dsih_ctrl_t * instance);
void mipi_dsih_hal_dpi_18_loosely_packet_en(dsih_ctrl_t * instance, int enable);
void mipi_dsih_hal_dpi_color_mode_pol(dsih_ctrl_t * instance, int active_low);
void mipi_dsih_hal_dpi_shut_down_pol(dsih_ctrl_t * instance, int active_low);
void mipi_dsih_hal_dpi_hsync_pol(dsih_ctrl_t * instance, int active_low);
void mipi_dsih_hal_dpi_vsync_pol(dsih_ctrl_t * instance, int active_low);
void mipi_dsih_hal_dpi_dataen_pol(dsih_ctrl_t * instance, int active_low);
void mipi_dsih_hal_dpi_frame_ack_en(dsih_ctrl_t * instance, int enable);
void mipi_dsih_hal_dpi_null_packet_en(dsih_ctrl_t * instance, int enable);
void mipi_dsih_hal_dpi_multi_packet_en(dsih_ctrl_t * instance, int enable);
void mipi_dsih_hal_dpi_lp_during_hfp(dsih_ctrl_t * instance, int enable);
void mipi_dsih_hal_dpi_lp_during_hbp(dsih_ctrl_t * instance, int enable);
void mipi_dsih_hal_dpi_lp_during_vactive(dsih_ctrl_t * instance, int enable);
void mipi_dsih_hal_dpi_lp_during_vfp(dsih_ctrl_t * instance, int enable);
void mipi_dsih_hal_dpi_lp_during_vbp(dsih_ctrl_t * instance, int enable);
void mipi_dsih_hal_dpi_lp_during_vsync(dsih_ctrl_t * instance, int enable);

dsih_error_t mipi_dsih_hal_dpi_video_mode_type(dsih_ctrl_t * instance, dsih_video_mode_t type);
void mipi_dsih_hal_dpi_video_mode_en(dsih_ctrl_t * instance, int enable);
int mipi_dsih_hal_dpi_is_video_mode(dsih_ctrl_t * instance);
dsih_error_t mipi_dsih_hal_dpi_null_packet_size(dsih_ctrl_t * instance, uint16_t size);
dsih_error_t mipi_dsih_hal_dpi_chunks_no(dsih_ctrl_t * instance, uint16_t no);
dsih_error_t mipi_dsih_hal_dpi_video_packet_size(dsih_ctrl_t * instance, uint16_t size);

void mipi_dsih_hal_tear_effect_ack_en(dsih_ctrl_t * instance, int enable);

void mipi_dsih_hal_cmd_ack_en(dsih_ctrl_t * instance, int enable);
dsih_error_t mipi_dsih_hal_dcs_wr_tx_type(dsih_ctrl_t * instance, unsigned no_of_param, int lp);
dsih_error_t mipi_dsih_hal_dcs_rd_tx_type(dsih_ctrl_t * instance, unsigned no_of_param, int lp);
/*Jessica add to support max rd packet size command*/
dsih_error_t mipi_dsih_hal_max_rd_packet_size_type(dsih_ctrl_t * instance, int lp);
dsih_error_t mipi_dsih_hal_gen_wr_tx_type(dsih_ctrl_t * instance, unsigned no_of_param, int lp);
dsih_error_t mipi_dsih_hal_gen_rd_tx_type(dsih_ctrl_t * instance, unsigned no_of_param, int lp);
void mipi_dsih_hal_max_rd_size_type(dsih_ctrl_t * instance, int lp);
void mipi_dsih_hal_gen_cmd_mode_en(dsih_ctrl_t * instance, int enable);
int mipi_dsih_hal_gen_is_cmd_mode(dsih_ctrl_t * instance);

void mipi_dsih_hal_dpi_hline(dsih_ctrl_t * instance, uint16_t time);
void mipi_dsih_hal_dpi_hbp(dsih_ctrl_t * instance, uint16_t time);
void mipi_dsih_hal_dpi_hsa(dsih_ctrl_t * instance, uint16_t time);
void mipi_dsih_hal_dpi_vactive(dsih_ctrl_t * instance, uint16_t lines);
void mipi_dsih_hal_dpi_vfp(dsih_ctrl_t * instance, uint16_t lines);
void mipi_dsih_hal_dpi_vbp(dsih_ctrl_t * instance, uint16_t lines);
void mipi_dsih_hal_dpi_vsync(dsih_ctrl_t * instance, uint16_t lines);

void mipi_dsih_hal_edpi_max_allowed_size(dsih_ctrl_t * instance, uint16_t size);

dsih_error_t mipi_dsih_hal_gen_packet_header(dsih_ctrl_t * instance, uint8_t vc, uint8_t packet_type, uint8_t ms_byte, uint8_t ls_byte);
dsih_error_t mipi_dsih_hal_gen_packet_payload(dsih_ctrl_t * instance, uint32_t payload);
dsih_error_t mipi_dsih_hal_gen_read_payload(dsih_ctrl_t * instance, uint32_t* payload);

void mipi_dsih_hal_timeout_clock_division(dsih_ctrl_t * instance, uint8_t byte_clk_division_factor);
void mipi_dsih_hal_lp_rx_timeout(dsih_ctrl_t * instance, uint16_t count);
void mipi_dsih_hal_hs_tx_timeout(dsih_ctrl_t * instance, uint16_t count);

uint32_t mipi_dsih_hal_int_status_0(dsih_ctrl_t * instance, uint32_t mask);
uint32_t mipi_dsih_hal_int_status_1(dsih_ctrl_t * instance, uint32_t mask);
void mipi_dsih_hal_int_mask_0(dsih_ctrl_t * instance, uint32_t mask);
void mipi_dsih_hal_int_mask_1(dsih_ctrl_t * instance, uint32_t mask);
uint32_t mipi_dsih_hal_int_get_mask_0(dsih_ctrl_t * instance, uint32_t mask);
uint32_t mipi_dsih_hal_int_get_mask_1(dsih_ctrl_t * instance, uint32_t mask);
/* DBI command interface */
void mipi_dsih_hal_dbi_out_color_coding(dsih_ctrl_t * instance, uint8_t color_depth, uint8_t option);
void mipi_dsih_hal_dbi_in_color_coding(dsih_ctrl_t * instance, uint8_t color_depth, uint8_t option);
void mipi_dsih_hal_dbi_lut_size(dsih_ctrl_t * instance, uint8_t size);
void mipi_dsih_hal_dbi_partitioning_en(dsih_ctrl_t * instance, int enable);
void mipi_dsih_hal_dbi_dcs_vc(dsih_ctrl_t * instance, uint8_t vc);

void mipi_dsih_hal_dbi_cmd_size(dsih_ctrl_t * instance, uint16_t size);
void mipi_dsih_hal_dbi_max_cmd_size(dsih_ctrl_t * instance, uint16_t size);
int mipi_dsih_hal_dbi_rd_cmd_busy(dsih_ctrl_t * instance);
int mipi_dsih_hal_dbi_read_fifo_full(dsih_ctrl_t * instance);
int mipi_dsih_hal_dbi_read_fifo_empty(dsih_ctrl_t * instance);
int mipi_dsih_hal_dbi_write_fifo_full(dsih_ctrl_t * instance);
int mipi_dsih_hal_dbi_write_fifo_empty(dsih_ctrl_t * instance);
int mipi_dsih_hal_dbi_cmd_fifo_full(dsih_ctrl_t * instance);
int mipi_dsih_hal_dbi_cmd_fifo_empty(dsih_ctrl_t * instance);
/* Generic command interface */
void mipi_dsih_hal_gen_rd_vc(dsih_ctrl_t * instance, uint8_t vc);
void mipi_dsih_hal_gen_eotp_rx_en(dsih_ctrl_t * instance, int enable);
void mipi_dsih_hal_gen_eotp_tx_en(dsih_ctrl_t * instance, int enable);
void mipi_dsih_hal_bta_en(dsih_ctrl_t * instance, int enable);
void mipi_dsih_hal_gen_ecc_rx_en(dsih_ctrl_t * instance, int enable);
void mipi_dsih_hal_gen_crc_rx_en(dsih_ctrl_t * instance, int enable);
int mipi_dsih_hal_gen_rd_cmd_busy(dsih_ctrl_t * instance);
int mipi_dsih_hal_gen_read_fifo_full(dsih_ctrl_t * instance);
int mipi_dsih_hal_gen_read_fifo_empty(dsih_ctrl_t * instance);
int mipi_dsih_hal_gen_write_fifo_full(dsih_ctrl_t * instance);
int mipi_dsih_hal_gen_write_fifo_empty(dsih_ctrl_t * instance);
int mipi_dsih_hal_gen_cmd_fifo_full(dsih_ctrl_t * instance);
int mipi_dsih_hal_gen_cmd_fifo_empty(dsih_ctrl_t * instance);

/* only if DPI */
dsih_error_t mipi_dsih_phy_hs2lp_config(dsih_ctrl_t * instance, uint8_t no_of_byte_cycles);
dsih_error_t mipi_dsih_phy_lp2hs_config(dsih_ctrl_t * instance, uint8_t no_of_byte_cycles);
dsih_error_t mipi_dsih_phy_clk_lp2hs_config(dsih_ctrl_t * instance, uint8_t no_of_byte_cycles);
dsih_error_t mipi_dsih_phy_clk_hs2lp_config(dsih_ctrl_t * instance, uint8_t no_of_byte_cycles);
dsih_error_t mipi_dsih_phy_bta_time(dsih_ctrl_t * instance, uint16_t no_of_byte_cycles);
void mipi_dsih_non_continuous_clock(dsih_ctrl_t * instance, int enable);
int mipi_dsih_non_continuous_clock_status(dsih_ctrl_t * instance);
/* PRESP Time outs */
void mipi_dsih_hal_presp_timeout_low_power_write(dsih_ctrl_t * instance, uint16_t no_of_byte_cycles);
void mipi_dsih_hal_presp_timeout_low_power_read(dsih_ctrl_t * instance, uint16_t no_of_byte_cycles);
void mipi_dsih_hal_presp_timeout_high_speed_write(dsih_ctrl_t * instance, uint16_t no_of_byte_cycles);
void mipi_dsih_hal_presp_timeout_high_speed_read(dsih_ctrl_t * instance, uint16_t no_of_byte_cycles);
void mipi_dsih_hal_presp_timeout_bta(dsih_ctrl_t * instance, uint16_t no_of_byte_cycles);
/* bsp abstraction */
void mipi_dsih_write_word(dsih_ctrl_t * instance, uint32_t reg_address, uint32_t data);
void mipi_dsih_write_part(dsih_ctrl_t * instance, uint32_t reg_address, uint32_t data, uint8_t shift, uint8_t width);
uint32_t mipi_dsih_read_word(dsih_ctrl_t * instance, uint32_t reg_address);
uint32_t mipi_dsih_read_part(dsih_ctrl_t * instance, uint32_t reg_address, uint8_t shift, uint8_t width);

#endif /* MIPI_DSI_API_H_ */
