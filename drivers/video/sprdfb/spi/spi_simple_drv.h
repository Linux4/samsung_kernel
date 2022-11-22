#ifndef _SPI_TEST_H
#define _SPI_TEST_H

#include <linux/kernel.h>

#define SPI0_ID        0
#define SPI1_ID        1
#define SPI2_ID        2
#define SPI_USED_ID    SPI2_ID

#define CHN_SPI_INT			9
#define DMA_SPI_TX			0x13

#define SPI_TX_FIFO_DEPTH 		16
#define SPI_RX_FIFO_DEPTH 		16
#define SPI_INT_DIS_ALL		0
#define SPI_RX_FULL_INT_EN		BIT_6
#define SPI_RX_FULL_INT_STS 	BIT_6
#define SPI_RX_FULL_INT_CLR 	BIT_0
#define SPI_RX_FIFO_REAL_EMPTY	BIT_5
#define SPI_S8_MODE_EN			BIT_7
#define SPI_CD_SEL				0x2	//cs1 is sel as cd signal

/*
   Define the SPI interface mode for LCM
 */
#define  SPIMODE_DISABLE           0
#define  SPIMODE_3WIRE_9BIT_SDA    1  // 3 wire 9 bit, cd bit, SDI/SDO share  one IO
#define  SPIMODE_3WIRE_9BIT_SDIO   2  // 3 wire 9 bit, cd bit, SDI, SDO
#define  SPIMODE_4WIRE_8BIT_SDA    3  // 4 wire 8 bit, cd pin, SDI/SDO share one IO
#define  SPIMODE_4WIRE_8BIT_SDIO   4  // 4 wire 8 bit, cd pin, SDI, SDO

/*
   Define the clk src for SPI mode
 */
#define SPICLK_SEL_192M    0
#define SPICLK_SEL_154M    1
#define SPICLK_SEL_96M   2
#define SPICLK_SEL_26M    3

/*
   SPI CS sel in master mode
 */
#define SPI_SEL_CS0 0x0E  //2'B1110
#define SPI_SEL_CS1 0x0D  //2'B1101
#define SPI_SEL_CS2 0x0B  //2'B1011
#define SPI_SEL_CS3 0x07  //2'B0111

#define TX_MAX_LEN_MASK     0xFFFFF
#define TX_DUMY_LEN_MASK    0x3F    //[09:04]
#define TX_DATA_LEN_H_MASK  0x0F    //[03:00]
#define TX_DATA_LEN_L_MASK  0xFFFF  //[15:00]
#define SPI_MODE_SHIFT    3
#define SPI_MODE_MASK     (0x07<<SPI_MODE_SHIFT)
#define SPI_CD_MASK  BIT(15)
#define SPI_SEL_CS_SHIFT 8

#define SPI_DEF_SRC_CLK (26 * 1000 *1000)
#define SPI_DEF_CLK (13 * 1000 * 1000)

typedef enum
{
	TX_POS_EDGE = 0,
	TX_NEG_EDGE
}TX_EDGE;

typedef enum
{
	RX_POS_EDGE = 0,
	RX_NEG_EDGE
}RX_EDGE;

typedef enum
{
	TX_RX_MSB = 0,
	TX_RX_LSB
}MSB_LSB_SEL;

typedef enum
{
	IDLE_MODE = 0,
	RX_MODE,
	TX_MODE,
	RX_TX_MODE
}TRANCIEVE_MODE;

typedef enum
{
	NO_SWITCH    = 0,
	BYTE_SWITCH  = 1,
	HWORD_SWITCH = 2
}SWT_MODE;

typedef enum
{
	MASTER_MODE = 0,
	SLAVE_MODE = 1
}SPI_OPERATE_MODE_E;

typedef enum
{
	DMA_DISABLE = 0,
	DMA_ENABLE
}DMA_EN;

typedef enum
{
	CS_LOW = 0,
	CS_HIGH
}CS_SIGNAL;

#define SW_RX_REQ_MASK BIT(0)
#define SW_TX_REQ_MASK BIT(1)
#define RX_MAX_LEN_MASK     0xFFFFFF
#define RX_DUMY_LEN_MASK    0x3F
#define RX_DATA_LEN_H_MASK  0x0F
#define RX_DATA_LEN_L_MASK  0xFFFF

typedef struct _init_param
{
	TX_EDGE tx_edge;
	RX_EDGE rx_edge;
	MSB_LSB_SEL msb_lsb_sel;
	TRANCIEVE_MODE tx_rx_mode;
	SWT_MODE switch_mode;
	SPI_OPERATE_MODE_E op_mode;
	uint32_t DMAsrcSize;
	uint32_t DMAdesSize;
	uint32_t clk_div;
	uint8_t data_width;
	uint8_t tx_empty_watermark;
	uint8_t tx_full_watermark;
	uint8_t rx_empty_watermark;
	uint8_t rx_full_watermark;
}SPI_INIT_PARM,*SPI_INIT_PARM_P;

typedef struct
{
	uint32_t data;				// Transmit word or Receive word
	uint32_t clkd;				// clock dividor register
	uint32_t ctl0;				// control register
	uint32_t ctl1;				// Receive Data full threshold/Receive Data full threshold
	uint32_t ctl2;				// 2-wire mode reigster
	uint32_t ctl3;				// transmit data interval
	uint32_t ctl4;				// transmit data interval
	uint32_t ctl5;				// transmit data interval
	uint32_t ien;				// interrutp enable register
	uint32_t iclr;				// interrupt clear register
	uint32_t iraw;				// interrupt clear register
	uint32_t ists;				// interrupt clear register
	uint32_t sts1;				// fifo cnt register, bit[5:0] for RX and [13:8] for TX
	uint32_t sts2;				// masked interrupt status register
	uint32_t dsp_wait;   		// Used for DSP control
	uint32_t sts3;				// tx_empty_threshold and tx_full_threshold
	uint32_t ctl6;
	uint32_t sts4;
	uint32_t fifo_rst;
	uint32_t ctl7;               // SPI_RX_HLD_EN : SPI_TX_HLD_EN : SPI_MODE
	uint32_t sts5;               // CSN_IN_ERR_SYNC2
	uint32_t ctl8;               // SPI_CD_BIT : SPI_TX_DUMY_LEN : SPI_TX_DATA_LEN_H
	uint32_t ctl9;               // SPI_TX_DATA_LEN_L
	uint32_t ctl10;              // SPI_RX_DATA_LEN_H : SPI_RX_DUMY_LEN
	uint32_t ctl11;              // SPI_RX_DATA_LEN_L
	uint32_t ctl12;              // SW_TX_REQ : SW_RX_REQ
} SPI_CTL_REG_T;

void SPI_SetCsLow( uint32_t spi_sel_csx , bool is_low);
void SPI_SetCd( uint32_t cd);
void SPI_SetDatawidth(uint32_t datawidth);
#ifdef CONFIG_OF
void SPI_Init(struct device *device, u32 spi_id, SPI_INIT_PARM *spi_parm);
#else
void SPI_Init(u32 spi_id, SPI_INIT_PARM *spi_parm);
#endif
void SPI_WriteData(uint32_t data, uint32_t data_len, uint32_t dummy_bitlen);
uint32_t SPI_ReadData( uint32_t data_len, uint32_t dummy_bitlen );

#endif /*_SPI_TEST_H*/
