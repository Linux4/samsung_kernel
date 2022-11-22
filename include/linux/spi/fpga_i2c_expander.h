#ifndef __FPGA_I2C_EXPANDER_H__
#define __FPGA_I2C_EXPANDER_H__
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
//#define FPGA_I2C_EXPANDER_DBG
#define SPI_CLK_1MHZ		1000000
#define SPI_CLK_12MHZ		12000000
#define SPI_CLK_24MHZ		24000000
#define SPI_CLK_48MHZ		48000000
#define SPI_CLK_FREQ		SPI_CLK_24MHZ
#define HANDLE_TIME             30000000

#define GPIO_CRESET			89
#define GPIO_CS				87
#define I2C_MAX_CH_NUM		4
#define DEVICE_I2C_WR		0x80
#define DEVICE_I2C_RD		0x40
#define DEVICE_I2C_1M_RD	0x20
#define FPGA_BASE			0x00
#define FPGA_READ			0xC0
#define FPGA_STATUS			0x86
#define FPGA_CH_OFFSET		0x08
#define FPGA_I2C_BUSY		0x80
#define FPGA_I2C_TX_DONE	0x40
#define FPGA_I2C_RX_DONE	0x20
#define FPGA_I2C_TX_ERR		0x10
#define FPGA_I2C_RX_ERR		0x08
#define FPGA_I2C_ABORT_ACK  0x04
#define FPGA_FW_VERSION         0x02
#define FW_VERSION_MISMATCH     7763

struct fpga_i2c_expander
{
	struct spi_device *spi_client;
	struct fpga_i2c_expander_platform_data *pdata;
	char *fw_file_name;
	uint8_t is_configured;
	uint8_t is_gpio_allocated;
	/*
	* The mutex prevents a race between the enable_irq()
	* *  in the workqueue and the free_irq() in the remove function.
	*/
	struct mutex mutex;
	struct work_struct	work;
	dev_t				devt;
	spinlock_t			spi_lock;
	struct list_head	device_entry;
	/* buffer is NULL unless this device is open (users > 0) */
	struct mutex	buf_lock;
	unsigned		users;
	uint8_t			*buffer;
	uint8_t			*bufferrx;
	struct spi_xfer_mem	*spi_mem;
	struct work_struct spi_cmd_func;
	struct workqueue_struct *spi_cmd_wqs;
	struct list_head  spi_cmd_queue;
	struct mutex  spi_cmd_lock;
	struct spi_list_s *spi_cmd[I2C_MAX_CH_NUM];
	struct completion device_complete[I2C_MAX_CH_NUM];
	uint32_t spi_bus_num;
	struct regulator *vdd;
};
struct fpga_i2c_expander_platform_data {
	int fpga_gpio_creset;	/* GPIO used by FPGA CRESET */
	int fpga_gpio_spi_cs;	/* GPIO used by FPGA SPI CS */
};
struct spi_cmd_s{
	u8 op_code;
	u16 slave_addr;
	u8 data_tx0[300];
	u8 data_tx1[20];
	u8 data_tx2[20];
	u8 data_tx3[20];
	u8 data_rx0[2040];
	u8 data_rx1[10];
	u8 tx_length0;
	u8 tx_length1;
	u8 tx_length2;
	u8 tx_length3;
	u16 rx_length0;
	u8 rx_length1;
	int ret;
};
struct spi_list_s{
	struct spi_cmd_s *cmds;
	struct mutex  device_lock;
	struct list_head list;
};
struct spi_xfer_mem {
	struct spi_transfer spi_xfer[4];
	struct spi_message spi_cmd;
};
int fpga_i2c_exp_rx_data(
	struct fpga_i2c_expander *fpga_i2c_expander,
	char *txbuf,
	char *rxbuf,
	int len);
int fpga_i2c_exp_tx_data(
	struct fpga_i2c_expander *fpga_i2c_expander,
	char *txbuf,
	int len);
void fpga_spi_queue_handle(struct work_struct *work);
int fpga_i2c_exp_write(	u8 ch_num, u8 s_addr, u16 offset, u8 offset_len, u8 * buf, u16 buf_len);
int fpga_i2c_exp_read(	u8 ch_num, u8 s_addr, u16 offset, u8 offset_len, u8 * buf, u16 buf_len);
void fpga_work(struct fpga_i2c_expander *fpga_i2c_expander,  struct spi_list_s *spi_data);

#endif // __FPGA_I2C_EXPANDER_H__
