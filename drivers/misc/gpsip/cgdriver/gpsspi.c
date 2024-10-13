#include <linux/init.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/delay.h>
#include <soc/sprd/dma.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/gpio.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>

#include "CgCpuOs.h"
#include "platform.h"
#include "CgxDriverCore.h"
#include "CgxDriverCoreCommon.h"



#define SPI_MODE_MASK		(SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
				| SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP)


#define SPI_READ_CMD(len,addr)    ((1 << 30) | (addr & 0x1FFFF)|(((len-1) & 0x1FF) << 20))
#define SPI_WRITE_CMD(len,addr)   ((1 << 30) |(1 << 31) | (addr & 0x1FFFF)|(((len-1) & 0x1FF) << 20))
#define SPI_BLOCK_READ_CMD(len) (                   (len-1) & 0xF )
#define SPI_BLOCK_WRITE_CMD(len) (                (1 << 9) |  (len-1) & 0xF )

#define SPI_BLOCK_DATA_ENC(len,addr)   ((  ((len-1)&0x7FFF) << 17 ) | ( addr & 0x1FFFF))
//#define SPI_BLOCK_DATA_ENC(len,addr)   ((  ((len-1)&0x7FFF) << 17 ) | ( addr & 0x1FFFF))


#define SPI_READ_SYS_CMD(len,addr)    ((1 << 30) | (1 << 29) | (addr & 0x1FFFF)|(((len-1) & 0x1FF) << 20))
#define SPI_WRITE_SYS_CMD(len,addr)   ((1 << 30) | (1 << 29) | (1 << 31) | (addr & 0x1FFFF)|(((len-1) & 0x1FF) << 20))

extern void DataReady_TH_start (void);
extern int get_blocksize (void);
extern unsigned int get_block_num (void);
extern void* Data_Get_DrvContext(void);
extern u32 GetRequestLen(void);


struct spi_device *gps_spi_dev = NULL;
static int irq_number, addr_flag_side;
//static u32 count_block = 0;
static u32 Revcount_block = 0;
static u32 RevtotalLen  = 0;


//#define FIX_MEMORY
#ifdef FIX_MEMORY
extern unsigned char buf_data[6 * 1024 * 1024];
static u8 *read_buf = (u8 *) buf_data;

#else
extern void *buf_data;
static u8 *read_buf;
#endif


static void
spi_transfer (unsigned char *tx1_buf, int write_len,
			  unsigned char *tx2_buf, int tx2_len,
			  unsigned char *rx_buf, int rx_len, bool cs_change)
{
	int status;
	struct spi_message m;

	struct spi_transfer _tx_1 = {
		.tx_buf = tx1_buf,
		.rx_buf = NULL,
		.len = write_len,
		.cs_change = 0,
		.bits_per_word = 32,
		// .delay_usecs = 200,
	};
	struct spi_transfer _tx_2 = {
		.tx_buf = tx2_buf,
		.rx_buf = NULL,
		.len = tx2_len,
		.cs_change = 0,
		.bits_per_word = 32,
		// .delay_usecs = 200,
	};

	struct spi_transfer _rx = {
		.rx_buf = rx_buf,
		.tx_buf = NULL,
		.len = rx_len,

		.bits_per_word = 32,
		.cs_change = 0,
	};
	gpio_direction_output(CG_DRIVER_GPIO_GPS_SPI_CS, 0);

	spi_message_init (&m);
	if (write_len)
		spi_message_add_tail (&_tx_1, &m);
	if (tx2_len)
		spi_message_add_tail (&_tx_2, &m);
	if (rx_len)
		spi_message_add_tail (&_rx, &m);

	if (!list_empty (&m.transfers))
	{
		status = spi_sync (gps_spi_dev, &m);
	}
	gpio_direction_output(CG_DRIVER_GPIO_GPS_SPI_CS, 1);
}


int gps_spi_write_bytes (u32 len, u32 addr, u32 data)
{
	u32 write_cmd[2];

	//printk ("%s addr:%x len:%x data:0x%x\n",__func__, addr, len, data);

	//Because of the HwTest not enable the gps dma req,so force to set it.
	if((addr == 0x88)&&(data == 0x06))
		data = 0x6016;
	if (addr > 0x6000)
	{
		BUG_ON (1);
		return 0;
	}
	if (len == 1)
	{
		write_cmd[0] = SPI_WRITE_CMD (len * 1, (addr / 4 + 0x3000));
		write_cmd[1] = data;
		spi_transfer ((u8 *)&write_cmd[0], 8, NULL, 0, NULL, 0, true);
	}
	return 0;
}

EXPORT_SYMBOL_GPL (gps_spi_write_bytes);



/*make sure len is word units*/
int
gps_spi_read_bytes (u32 len, u32 addr, u32 * data)
{
	u32 read_cmd[2] = {0};

	if (len == 1)	/*one block all way */
	{
		read_cmd[0] = SPI_READ_CMD (len, (addr / 4 + 0x3000));
		spi_transfer (NULL, 0, (u8 *) & read_cmd[0], 4,
					  (u8 *) & read_cmd[1], 4, true);

		*data = read_cmd[1];
	}
	else	/*more than one word,block */
	{
		u32 read_cmd_block[2];

		/*len word unit */
		read_cmd_block[0] = SPI_BLOCK_READ_CMD (1);
		read_cmd_block[1] = SPI_BLOCK_DATA_ENC (len, addr);

		spi_transfer (NULL, 0, (u8 *)&read_cmd_block[0], 8, (u8 *) data,len*4,
					  true);

		read_cmd[1] = data[0];
	}
	return 0;
}

EXPORT_SYMBOL_GPL (gps_spi_read_bytes);


int gps_spi_sysreg_write_bytes (u32 len, u32 addr, u32 data)
{
	u32 write_cmd[2];

	//printk ("%s addr:%x len:%x data:0x%x\n",__func__, addr, len, data);
	if (len == 1)
	{
		write_cmd[0] = SPI_WRITE_SYS_CMD (len, addr);
		write_cmd[1] = data;
		spi_transfer ((u8 *)&write_cmd[0], 8, NULL, 0, NULL, 0, true);
	}
	return 0;
}


/*make sure len is word units*/
int gps_spi_sysreg_read_bytes (u32 len, u32 addr, u32 * data)
{
	u32 read_cmd[2] = {0};

	if (len == 1)	/*one block all way */
	{
		read_cmd[0] = SPI_READ_SYS_CMD(len, addr);
		spi_transfer (NULL, 0, (u8 *) & read_cmd[0], 4,
					  (u8 *) & read_cmd[1], 4, true);

		*data = read_cmd[1];
	}

	return 0;
}
#define SYSREG_BIT3_BIT4_BIT5_BIT7 (1<<3|1<<4|1<<5|1<<7)
#define SYS_BIT10			(1<<10)

void PowerUpOtherClk(void)
{
	u32 value = 0;

	gps_spi_sysreg_read_bytes(1,0x03,&value);
	value |= SYSREG_BIT3_BIT4_BIT5_BIT7;
	value &= ~SYS_BIT10;
	gps_spi_sysreg_write_bytes(1,0x03,value);
	gps_spi_sysreg_read_bytes(1,0x03,&value);
	printk("PowerUpOtherClk:0x03=0x%x\n",(unsigned int)value);

}

void PowerDownOtherClk(void)
{
	u32 value = 0;

	gps_spi_sysreg_read_bytes(1,0x03,&value);
	value &= ~SYSREG_BIT3_BIT4_BIT5_BIT7;
	value |= SYS_BIT10;
	gps_spi_sysreg_write_bytes(1,0x03,value );
	gps_spi_sysreg_read_bytes(1,0x03,&value);
	printk("PowerDownOtherClk :0x03=0x%x\n",(unsigned int)value);

}

void Reset_GPS_RecvAddress(void)
{
    irq_number  = 0;
}

int irq_count = 0;

void init_spi_block_var(void)
{
	addr_flag_side = 0;
	irq_number = 0;
	Revcount_block = 0;
	RevtotalLen  = 0;
	buf_data = Data_Get_DrvContext();
}

void gps_GetDataFromFPGA(void)
{
    u32 read_len=1024*4;
    u32  block_size = get_blocksize();
    
    // the use space reset rcv address and spi has rcv part data ,not conside the block szie less 4K size
    if(irq_number ==1)
     {
        
         u32  len = RevtotalLen - Revcount_block * block_size;
         void *  pStart = Data_Get_DrvContext();

         if(len)
        {
             printk ("  SPI  has  rcv but not submit  len = %d  $$$$$$$\n", len);  
             memcpy(pStart, (void*)(read_buf + read_len*4 -len) ,len);
        }
         buf_data = (u8*)(pStart+ len);
     }
    
    read_buf = (u8*)((u8*)buf_data + (irq_number  -1)* read_len*4);
    gps_spi_read_bytes(read_len,0x5000+addr_flag_side*read_len,(u32*)read_buf);
       if(addr_flag_side)
       {
            addr_flag_side = 0 ;
       }
       else
        {
              addr_flag_side = 1 ;
        }
       // recv all data len from FPGA
      RevtotalLen  =  RevtotalLen  +  read_len*4;
    // GPS_DEBUG_TRACE ("gps_GetDataFromFPGA buf=0x%p 0x%p  rcvtotalLen = 0x%x, irq_number = %d &&&&& \n",
                     //buf_data,read_buf,RevtotalLen,irq_number);  
}

// BlockNum: all total number 
static int  submitLastBlock(int BlockNum)
{
    int lastFlag = FALSE;
    u32 requestLen = GetRequestLen(); // user requset  data len

    if(Revcount_block ==(BlockNum -1))
    {
        if((requestLen < RevtotalLen ) ||(requestLen ==RevtotalLen))
        {
            Revcount_block++;
            //GPS_DEBUG_TRACE ("sbumit  last blocks Rev count_block =%d requestLen = 0x%x ,RevtotalLen = 0x%x&&&&&& \n",
                                                 //Revcount_block,requestLen,RevtotalLen);  
            DataReady_TH_start();
            Revcount_block = 0;
            lastFlag = TRUE;
        }
    }
    return lastFlag;
}

void gps_DealWithBlocks(void)
{
    u32 dataLen = 0; 
    int lastfrag = 0;
    int count = 0;
    int i;

    u32  block_size = get_blocksize();
    u32 block_num = get_block_num();

    if(0 == Revcount_block)
    {
        if((RevtotalLen >  block_size) || (RevtotalLen == block_size))
        {
            count= RevtotalLen /block_size;
            for(i = 0; i < count; i++)
            {
                Revcount_block ++;
                DataReady_TH_start();
                //GPS_DEBUG_TRACE ("gps_DealWithBocks Rev count_block = %d  &&& \n",Revcount_block);  
            }
            //check  last blocks 
            submitLastBlock(block_num);
        }
        else
        {
          // GPS_DEBUG_TRACE ("gps_DealWithBocks no blocks submit $$$$$$ \n");
        }
    }
    else
    {
        
        dataLen = RevtotalLen - Revcount_block * block_size; 
           //check the last datalen  
           lastfrag = submitLastBlock(block_num);
           if(!lastfrag)
            {
                count= dataLen /block_size;
                for( i = 0; i < count; i++)
               {
                   Revcount_block++;
                   DataReady_TH_start();
                  // GPS_DEBUG_TRACE ("gps_DealWithBocks  sbumit  Revcount_block  = %d  &&&\n",Revcount_block);  
               }
                //check the last block
                submitLastBlock(block_num);
            }
   }
 }

  // the bottle interrupt deal with user data
void gps_spi_recv_handle(void * data)
{
    gps_GetDataFromFPGA();
    gps_DealWithBlocks();  
}

/*gpio inturrupt handle*/
int data_req_handle (int para, void *param)
{
	irq_number++;
	//printk("irq_number:%d\n",irq_number);
	//bxd test
	irq_count++;
	gps_spi_recv_handle (0);
	
	return 0;
}

static int
gpsspidev_probe (struct spi_device *spi)
{
	if(spi->master->bus_num != 2)
		return -EINVAL;
	gps_spi_dev = spi;
	
	spi->max_speed_hz = 24000*1000;
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 32;

	spi_setup(gps_spi_dev);

	return 0;
}
static int
gpsspidev_remove (struct spi_device *spi)
{

	return 0;
}


static struct spi_board_info spi_boardinfo[] = {
	{
	.modalias = "spidev_gps",
	.bus_num = 2,
	.chip_select = 0,
	.max_speed_hz = 24000 * 1000,
	.mode = SPI_MODE_0,
	}
};


static void sprd_spi_device_init(void)
{
	struct spi_board_info *info = spi_boardinfo;
	spi_register_board_info(info, ARRAY_SIZE(spi_boardinfo));
}


static struct spi_driver gpsspidev_spi = {
	.driver = {
			   .name = "spidev_gps",
			   .owner = THIS_MODULE,
			   },
	.probe = gpsspidev_probe,
	.remove = gpsspidev_remove,
};

int  gpsspidev_init (void)
{

	int major;
	sprd_spi_device_init();
	major = spi_register_driver (&gpsspidev_spi);
	if (major < 0)
	{
		unregister_chrdev (major, gpsspidev_spi.driver.name);
		printk ("spi_register_driver fail !!major = %d\n", major);
	}
	printk ("gpsspidev_init end major=%d!\n", major);

	return major;
}


void  gpsspidev_exit (void)
{
	spi_unregister_driver (&gpsspidev_spi);
}


