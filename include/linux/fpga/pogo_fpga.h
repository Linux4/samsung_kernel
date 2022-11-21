#ifndef __pogo_fpga_H__
#define __pogo_fpga_H__
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#define SPI_CLK_1MHZ		1000000
#define SPI_CLK_12MHZ		12000000
#define SPI_CLK_24MHZ		24000000
#define SPI_CLK_48MHZ		48000000
#define SPI_CLK_FREQ		SPI_CLK_12MHZ

#define I2C_MAX_CH_NUM		4
#define DEVICE_I2C_WR		0x80
#define DEVICE_I2C_RD		0x40
#define DEVICE_I2C_1M_RD	0x20
#define FPGA_BASE		0x00
#define FPGA_READ		0xC0
#define FPGA_STATUS		0x86
#define FPGA_CH_OFFSET		0x08
#define FPGA_I2C_BUSY		0x80
#define FPGA_I2C_TX_DONE	0x40
#define FPGA_I2C_RX_DONE	0x20
#define FPGA_I2C_TX_ERR		0x10
#define FPGA_I2C_RX_ERR		0x08
#define FPGA_I2C_ABORT_ACK	0x04
#define FPGA_FW_VERSION         0x02
#define FW_VERSION_MISMATCH     7763

#define GPIO_LEVEL_HIGH		1
#define GPIO_LEVEL_LOW		0

struct pogo_fpga
{
	struct spi_device *spi_client;
	struct pogo_fpga_platform_data *pdata;
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

	uint32_t spi_bus_num;
	struct regulator *vdd;
	uint8_t *image_data;
	size_t image_size;
	struct mutex download_lock;
};

struct pogo_fpga_platform_data {
	int fpga_cdone;	/* GPIO used by FPGA CDONE */
	int fpga_pogo_ldo_en;	/* GPIO used by FPGA POGO_LDO_EN */
	int fpga_gpio_reset;	/* GPIO used by FPGA RESET */
	int fpga_gpio_crst_b;	/* GPIO used by FPGA CRST_B */
	int fpga_gpio_spi_cs;	/* GPIO used by FPGA SPI CS */
	int fpga_gpio_spi_miso;
	int fpga_gpio_spi_mosi;
	int fpga_gpio_spi_clk;
	const char *fw_name;
};

int pogo_fpga_rx_data(struct pogo_fpga *pogo_fpga, char *txbuf, char *rxbuf, int len);
int pogo_fpga_tx_data(struct pogo_fpga *pogo_fpga, char *txbuf, int len);

#endif // __pogo_fpga_H__
