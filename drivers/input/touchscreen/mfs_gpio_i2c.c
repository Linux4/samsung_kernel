#include "mfs_gpio_i2c.h"
#include <linux/gpio.h>
#include <linux/delay.h>

//--------------------------------------------------
// "H/W Porting"
//--------------------------------------------------

#include <linux/input/mms136_ts.h>

#define GPIO_TSP_SCL    		TSP_SCL//35
#define GPIO_TSP_SDA      		TSP_SDA//40


#define MFS_SDA_SET_OUTPUT(a)			gpio_direction_output(GPIO_TSP_SDA, a)
#define MFS_SDA_SET_INPUT()				gpio_direction_input(GPIO_TSP_SDA)
										
#define MFS_SDA_VAL_HIGH()				gpio_set_value(GPIO_TSP_SDA, 1)
#define MFS_SDA_VAL_LOW()				gpio_set_value(GPIO_TSP_SDA, 0)

#define MFS_SCL_SET_OUTPUT(a)			gpio_direction_output(GPIO_TSP_SCL,a)
#define MFS_SCL_SET_INPUT()				gpio_direction_input(GPIO_TSP_SCL)

#define MFS_SCL_VAL_HIGH()				gpio_set_value(GPIO_TSP_SCL, 1)
#define MFS_SCL_VAL_LOW()				gpio_set_value(GPIO_TSP_SCL, 0)

#define MFS_SDA_IS_HIGH()				gpio_get_value(GPIO_TSP_SDA)
#define MFS_SCL_IS_HIGH()		  	  	gpio_get_value(GPIO_TSP_SCL)

//--------------------------------------------------
// "Delay porting" for I2C Speed & Pull-up rising time
//--------------------------------------------------

#define MFS_SCL_HOLD_INPUT_DELAY()		udelay(15)
#define MFS_SCL_HOLD_CHECK_DELAY()		udelay(15) 
#define MFS_GPIO_DELAY_TO_LOW()			//udelay(1)
#define MFS_GPIO_DELAY_TO_HIGH()		//udelay(1)



//--------------------------------------------------
// Main signal
//--------------------------------------------------

#define MFS_SDA_INPUT()					MFS_SDA_SET_INPUT();	MFS_GPIO_DELAY_TO_HIGH()
#define MFS_SDA_OUTPUT(a)				MFS_SDA_SET_OUTPUT(a);	MFS_GPIO_DELAY_TO_HIGH()
#define MFS_SDA_HIGH()					MFS_SDA_VAL_HIGH();		MFS_GPIO_DELAY_TO_HIGH()
#define MFS_SDA_LOW()					MFS_SDA_VAL_LOW();		MFS_GPIO_DELAY_TO_LOW()

#define MFS_SCL_INPUT()					MFS_SCL_SET_INPUT();	MFS_GPIO_DELAY_TO_HIGH()
#define MFS_SCL_OUTPUT(a)				MFS_SCL_SET_OUTPUT(a);	MFS_GPIO_DELAY_TO_HIGH()
#define MFS_SCL_HIGH()					MFS_SCL_VAL_HIGH();		MFS_GPIO_DELAY_TO_HIGH()
#define MFS_SCL_LOW()					MFS_SCL_VAL_LOW();		MFS_GPIO_DELAY_TO_LOW()

#define CLK_ON()						MFS_SCL_HIGH(); 		MFS_SCL_LOW()	
//--------------------------------------------------
// Functions
//--------------------------------------------------
#undef SCL_LOW_CHECK
#ifdef SCL_LOW_CHECK
unsigned char mfs_gpio_i2c_check_scl_hold(void);
#endif

#define FORCE_DELAY
unsigned int need_check;


unsigned char mfs_gpio_i2c_write( unsigned char ucAddress, unsigned char *pucData , int nLength )
{
	int i;
	unsigned int data_count=0;
	unsigned char bNack;
	unsigned char bRet=false;

	unsigned char ucBit;
	unsigned char ucData;

	ucAddress <<= 1;

		
	MFS_SCL_OUTPUT(1);
	MFS_SDA_OUTPUT(1);
		
	//---------------
	//	Start
	//---------------

	MFS_SDA_LOW();
	MFS_SCL_LOW();
	
	//---------------
	//	Slave Address
	//---------------

	for( ucBit=0x80;ucBit>0x01;ucBit>>=1 )
	{
		if( ucAddress & ucBit ) { MFS_SDA_HIGH(); }
		else				    { MFS_SDA_LOW();  }

		CLK_ON();

	}
	data_count++;

	//---------------
	//	Write bit
	//---------------

	MFS_SDA_LOW();
	CLK_ON();

	
	//---------------
	//	ACK
	//---------------
	MFS_SCL_HIGH();
	MFS_SDA_INPUT();		
	bNack = MFS_SDA_IS_HIGH();		
	MFS_SCL_LOW();	//CLK_ON();

	
	if( bNack )
		goto MFS_I2C_WRITE_FINISH;

	//----------------------------
	//	Writing DATA
	//----------------------------
		
	for( i=0; i<nLength; i++)
	{
		ucData = pucData[i];

		//----------------------------
		//	First bit & Check SCL hold
		//----------------------------

		if( ucData & 0x80 ) { MFS_SDA_SET_OUTPUT(1); }
		else				{ MFS_SDA_SET_OUTPUT(0);  }

#ifdef SCL_LOW_CHECK
		if( !mfs_gpio_i2c_check_scl_hold() )
			goto MFS_I2C_WRITE_FINISH;
#else
		MFS_SCL_HIGH();
#endif
		MFS_SCL_LOW();

		//----------------------------
		//	Last 7 bits
		//----------------------------

		for(ucBit=0x40;ucBit>0x00;ucBit>>=1)
		{
			if( ucData & ucBit) { MFS_SDA_HIGH(); }
			else				{ MFS_SDA_LOW();  }
			
			CLK_ON();

		}
		data_count++;

		//---------------
		//	ACK
		//---------------
		MFS_SCL_HIGH();
		MFS_SDA_INPUT();		
		bNack = MFS_SDA_IS_HIGH();
		MFS_SCL_LOW();	//	CLK_ON();

#ifdef FORCE_DELAY
		if((data_count%4==0) && need_check){
			if(data_count == 4)	mdelay(110);
			else 				udelay(120);
		}
#endif			
		if( bNack && i != (nLength-1) )
			goto MFS_I2C_WRITE_FINISH;
	}

	//---------------
	//	STOP
	//---------------

	MFS_SDA_SET_OUTPUT(0);
	
#ifdef SCL_LOW_CHECK
	if( !mfs_gpio_i2c_check_scl_hold() )
		goto MFS_I2C_WRITE_FINISH;
#else
	MFS_SCL_HIGH();
#endif
	MFS_SDA_HIGH();

	bRet = true;

MFS_I2C_WRITE_FINISH :

	if(!bRet)
	{
		MFS_SCL_LOW();
		MFS_SDA_LOW();

		MFS_SCL_HIGH();
		MFS_SDA_HIGH();
	}
	mdelay(1);	
	return bRet;
}


unsigned char mfs_gpio_i2c_read( unsigned char ucAddress, unsigned char *pucData, int nLength )
{
	int i;

	unsigned char  bNack;
	unsigned char  bRet=false;

	unsigned char  ucBit;
	unsigned char  ucData;

	ucAddress <<= 1;

	MFS_SDA_OUTPUT(1);
	MFS_SCL_OUTPUT(1);	

	//---------------
	//	Start
	//---------------

	MFS_SDA_LOW();
	MFS_SCL_LOW();
	
	//---------------
	//	Slave Address
	//---------------

	for( ucBit=0x80;ucBit>0x01;ucBit>>=1 )
	{
		if( ucAddress & ucBit ) { MFS_SDA_HIGH(); }
		else				    { MFS_SDA_LOW();  }

		CLK_ON();
	}

	//---------------
	//	Read bit
	//---------------

	MFS_SDA_HIGH();
	CLK_ON();
	
	//---------------
	//	ACK
	//---------------

	MFS_SCL_HIGH();
	MFS_SDA_INPUT();		
	bNack = MFS_SDA_IS_HIGH();
	MFS_SCL_LOW();  //	CLK_ON();


	udelay(20);
	
	if( bNack )
		goto MFS_I2C_READ_FINISH;

	//----------------------------
	//	Writing DATA
	//----------------------------
			
	for( i=0; i<nLength; i++)
	{
		ucData = 0;
		if(i>0) MFS_SDA_INPUT();			

		//----------------------------
		//	First bit & Check SCL hold
		//----------------------------
		
#ifdef SCL_LOW_CHECK
		if( !mfs_gpio_i2c_check_scl_hold() )
			goto MFS_I2C_READ_FINISH;
#else
		MFS_SCL_HIGH(); 
#endif
		if( MFS_SDA_IS_HIGH() ) ucData = 0x80;
		MFS_SCL_LOW();

		//----------------------------
		//	Last 7 bits
		//----------------------------

		for(ucBit=0x40;ucBit>0x00;ucBit>>=1)
		{

			MFS_SCL_HIGH(); 
			if(MFS_SDA_IS_HIGH()) ucData |= ucBit;
			MFS_SCL_LOW();
		}

		//---------------
		//	ACK
		//---------------

		if( i == nLength-1 ) { MFS_SDA_OUTPUT(1); }
		else				 { MFS_SDA_OUTPUT(0);  } 
	
		CLK_ON();
		MFS_SDA_HIGH();

		pucData[i] = ucData;
	}

	//---------------
	//	STOP
	//---------------

	MFS_SDA_LOW();

#ifdef SCL_LOW_CHECK
	if( !mfs_gpio_i2c_check_scl_hold() )
		goto MFS_I2C_READ_FINISH;
#else
	MFS_SCL_HIGH();
#endif
	MFS_SDA_HIGH();
	

	bRet = true;

MFS_I2C_READ_FINISH :

	if( !bRet)
	{
		MFS_SCL_LOW();
		MFS_SDA_LOW();

		MFS_SCL_HIGH();
		MFS_SDA_HIGH();
	}
	
	mdelay(1);	
	return bRet;
}


#ifdef SCL_LOW_CHECK
unsigned char mfs_gpio_i2c_check_scl_hold(void)
{
	int nCount=0;

	MFS_SCL_INPUT();
			
	while(!MFS_SCL_IS_HIGH())
	{
		if(nCount++>10000){
			MFS_SCL_OUTPUT(1);
			return false;
		}

		MFS_SCL_HOLD_CHECK_DELAY();
	}

	MFS_SCL_OUTPUT(1);
	return true;
}
#endif
