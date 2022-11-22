#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/projector/beam_gpio_i2c.h>

//--------------------------------------------------
// "H/W Porting"
//--------------------------------------------------

#define BEAM_SDA_SET_OUTPUT(a)				gpio_direction_output(BEAM_SDA, a)
#define BEAM_SDA_SET_INPUT()				gpio_direction_input(BEAM_SDA)
										
#define BEAM_SDA_VAL_HIGH()				gpio_set_value(BEAM_SDA, 1)
#define BEAM_SDA_VAL_LOW()				gpio_set_value(BEAM_SDA, 0)

#define BEAM_SCL_SET_OUTPUT(a)				gpio_direction_output(BEAM_SCL,a)
#define BEAM_SCL_SET_INPUT()				gpio_direction_input(BEAM_SCL)

#define BEAM_SCL_VAL_HIGH()				gpio_set_value(BEAM_SCL, 1)
#define BEAM_SCL_VAL_LOW()				gpio_set_value(BEAM_SCL, 0)

#define BEAM_SDA_IS_HIGH()					gpio_get_value(BEAM_SDA)
#define BEAM_SCL_IS_HIGH()		  	  		gpio_get_value(BEAM_SCL)

//--------------------------------------------------
// "Delay porting" for I2C Speed & Pull-up rising time
//--------------------------------------------------

#define BEAM_SCL_HOLD_INPUT_DELAY()		udelay(15)
#define BEAM_SCL_HOLD_CHECK_DELAY()		udelay(15) 
#define BEAM_GPIO_DELAY_TO_LOW()			//udelay(1)
#define BEAM_GPIO_DELAY_TO_HIGH()		//udelay(1)



//--------------------------------------------------
// Main signal
//--------------------------------------------------

#define BEAM_SDA_INPUT()					BEAM_SDA_SET_INPUT();		BEAM_GPIO_DELAY_TO_HIGH()
#define BEAM_SDA_OUTPUT(a)				BEAM_SDA_SET_OUTPUT(a);	BEAM_GPIO_DELAY_TO_HIGH()
#define BEAM_SDA_HIGH()					BEAM_SDA_VAL_HIGH();		BEAM_GPIO_DELAY_TO_HIGH()
#define BEAM_SDA_LOW()					BEAM_SDA_VAL_LOW();		BEAM_GPIO_DELAY_TO_LOW()

#define BEAM_SCL_INPUT()					BEAM_SCL_SET_INPUT();		BEAM_GPIO_DELAY_TO_HIGH()
#define BEAM_SCL_OUTPUT(a)					BEAM_SCL_SET_OUTPUT(a);	BEAM_GPIO_DELAY_TO_HIGH()
#define BEAM_SCL_HIGH()					BEAM_SCL_VAL_HIGH();		BEAM_GPIO_DELAY_TO_HIGH()
#define BEAM_SCL_LOW()					BEAM_SCL_VAL_LOW();		BEAM_GPIO_DELAY_TO_LOW()

#define CLK_ON()							BEAM_SCL_HIGH(); 			BEAM_SCL_LOW()	
//--------------------------------------------------
// Functions
//--------------------------------------------------
#undef SCL_LOW_CHECK
#ifdef SCL_LOW_CHECK
unsigned char beam_gpio_i2c_check_scl_hold(void);
#endif

#define FORCE_DELAY
unsigned int need_check;

//static DEFINE_MUTEX(beam_mutex);



unsigned char beam_gpio_i2c_write( unsigned char ucAddress, unsigned char *pucData , int nLength )
{
	int i;
	unsigned int data_count=0;
	unsigned char bNack;
	unsigned char bRet=false;

	unsigned char ucBit;
	unsigned char ucData;

	ucAddress <<= 1;

		
	BEAM_SCL_OUTPUT(1);
	BEAM_SDA_OUTPUT(1);
		
	//---------------
	//	Start
	//---------------

	BEAM_SDA_LOW();
	BEAM_SCL_LOW();
	
	//---------------
	//	Slave Address
	//---------------

	for( ucBit=0x80;ucBit>0x01;ucBit>>=1 )
	{
		if( ucAddress & ucBit ) { BEAM_SDA_HIGH(); }
		else				    { BEAM_SDA_LOW();  }

		CLK_ON();

	}
	data_count++;

	//---------------
	//	Write bit
	//---------------

	BEAM_SDA_LOW();
	CLK_ON();

	
	//---------------
	//	ACK
	//---------------
	BEAM_SCL_HIGH();
	BEAM_SDA_INPUT();		
	bNack = BEAM_SDA_IS_HIGH();		
	BEAM_SCL_LOW();	//CLK_ON();

	
	if( bNack )
		goto BEAM_I2C_WRITE_FINISH;

	//----------------------------
	//	Writing DATA
	//----------------------------
		
	for( i=0; i<nLength; i++)
	{
		ucData = pucData[i];

		//----------------------------
		//	First bit & Check SCL hold
		//----------------------------

		if( ucData & 0x80 ) { BEAM_SDA_SET_OUTPUT(1); }
		else				{ BEAM_SDA_SET_OUTPUT(0);  }

#ifdef SCL_LOW_CHECK
		if( !BEAM_gpio_i2c_check_scl_hold() )
			goto BEAM_I2C_WRITE_FINISH;
#else
		BEAM_SCL_HIGH();
#endif
		BEAM_SCL_LOW();

		//----------------------------
		//	Last 7 bits
		//----------------------------

		for(ucBit=0x40;ucBit>0x00;ucBit>>=1)
		{
			if( ucData & ucBit) { BEAM_SDA_HIGH(); }
			else				{ BEAM_SDA_LOW();  }
			
			CLK_ON();

		}
		data_count++;

		//---------------
		//	ACK
		//---------------
		BEAM_SCL_HIGH();
		BEAM_SDA_INPUT();		
		bNack = BEAM_SDA_IS_HIGH();
		BEAM_SCL_LOW();	//	CLK_ON();

#ifdef FORCE_DELAY
		if((data_count%4==0) && need_check){
			if(data_count == 4)	mdelay(110);
			else 				udelay(120);
		}
#endif			
		if( bNack && i != (nLength-1) )
			goto BEAM_I2C_WRITE_FINISH;
	}

	//---------------
	//	STOP
	//---------------

	BEAM_SDA_SET_OUTPUT(0);
	
#ifdef SCL_LOW_CHECK
	if( ! beam_gpio_i2c_check_scl_hold() )
		goto BEAM_I2C_WRITE_FINISH;
#else
	BEAM_SCL_HIGH();
#endif
	BEAM_SDA_HIGH();

	bRet = true;

BEAM_I2C_WRITE_FINISH :

	if(!bRet)
	{
		BEAM_SCL_LOW();
		BEAM_SDA_LOW();

		BEAM_SCL_HIGH();
		BEAM_SDA_HIGH();
	}
	mdelay(1);	
	return bRet;
}


unsigned char beam_gpio_i2c_read( unsigned char ucAddress, unsigned char *pucData, int nLength )
{
	int i;

	unsigned char  bNack;
	unsigned char  bRet=false;

	unsigned char  ucBit;
	unsigned char  ucData;

	ucAddress <<= 1;

	BEAM_SDA_OUTPUT(1);
	BEAM_SCL_OUTPUT(1);	

	//---------------
	//	Start
	//---------------

	BEAM_SDA_LOW();
	BEAM_SCL_LOW();
	
	//---------------
	//	Slave Address
	//---------------

	for( ucBit=0x80;ucBit>0x01;ucBit>>=1 )
	{
		if( ucAddress & ucBit ) { BEAM_SDA_HIGH(); }
		else				    { BEAM_SDA_LOW();  }

		CLK_ON();
	}

	//---------------
	//	Read bit
	//---------------

	BEAM_SDA_HIGH();
	CLK_ON();
	
	//---------------
	//	ACK
	//---------------

	BEAM_SCL_HIGH();
	BEAM_SDA_INPUT();		
	bNack = BEAM_SDA_IS_HIGH();
	BEAM_SCL_LOW();  //	CLK_ON();


	udelay(20);
	
	if( bNack )
		goto BEAM_I2C_READ_FINISH;

	//----------------------------
	//	Writing DATA
	//----------------------------
			
	for( i=0; i<nLength; i++)
	{
		ucData = 0;
		if(i>0) BEAM_SDA_INPUT();			

		//----------------------------
		//	First bit & Check SCL hold
		//----------------------------
		
#ifdef SCL_LOW_CHECK
		if( !beam_gpio_i2c_check_scl_hold() )
			goto BEAM_I2C_READ_FINISH;
#else
		BEAM_SCL_HIGH(); 
#endif
		if( BEAM_SDA_IS_HIGH() ) ucData = 0x80;
		BEAM_SCL_LOW();

		//----------------------------
		//	Last 7 bits
		//----------------------------

		for(ucBit=0x40;ucBit>0x00;ucBit>>=1)
		{

			BEAM_SCL_HIGH(); 
			if(BEAM_SDA_IS_HIGH()) ucData |= ucBit;
			BEAM_SCL_LOW();
		}

		//---------------
		//	ACK
		//---------------

		if( i == nLength-1 ) { BEAM_SDA_OUTPUT(1); }
		else				 { BEAM_SDA_OUTPUT(0);  } 
	
		CLK_ON();
		BEAM_SDA_HIGH();

		pucData[i] = ucData;
	}

	//---------------
	//	STOP
	//---------------

	BEAM_SDA_LOW();

#ifdef SCL_LOW_CHECK
	if( !beam_gpio_i2c_check_scl_hold() )
		goto BEAM_I2C_READ_FINISH;
#else
	BEAM_SCL_HIGH();
#endif
	BEAM_SDA_HIGH();
	

	bRet = true;

BEAM_I2C_READ_FINISH :

	if( !bRet)
	{
		BEAM_SCL_LOW();
		BEAM_SDA_LOW();

		BEAM_SCL_HIGH();
		BEAM_SDA_HIGH();
	}
	
	mdelay(1);	
	return bRet;
}


#ifdef SCL_LOW_CHECK
unsigned char beam_gpio_i2c_check_scl_hold(void)
{
	int nCount=0;

	BEAM_SCL_INPUT();
			
	while(!BEAM_SCL_IS_HIGH())
	{
		if(nCount++>10000){
			BEAM_SCL_OUTPUT(1);
			return false;
		}

		BEAM_SCL_HOLD_CHECK_DELAY();
	}

	BEAM_SCL_OUTPUT(1);
	return true;
}
#endif

#ifdef BEAM_I2C_EMUL

int beam_i2c_read( struct i2c_client *client, unsigned short  typeaddr, char *buf, char *bufR, int count, int countR)
{
#if 1
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg;
	int ret;

	msg.addr = typeaddr;
	msg.flags = 0x00;
	msg.len = count;
	msg.buf = buf;
	
	//mutex_lock(&beam_mutex);
      ret = i2c_transfer(adapter, &msg, 1);
	//ret = mfs_gpio_i2c_write(client->addr, ((u8 *) & addr), 1 );
	//mutex_unlock(&beam_mutex);
	if (ret < 0){
	    pr_err("[%s] : write error : [%d]", __func__, ret);
	}
	//else{
		msg.addr = typeaddr;
		msg.flags = I2C_M_RD;
		msg.len = countR;
		msg.buf = bufR;

		//mutex_lock(&beam_mutex);
    		ret = i2c_transfer(adapter, &msg, 1);
	  	//ret= mfs_gpio_i2c_read(client->addr, value, length);
		//mutex_unlock(&beam_mutex);
		if (ret < 0){
    			pr_err("[%s] : read error : [%d]", __func__, ret);
		}

	//}

    return ret;
#else
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;
	int ret;

	msg.addr = typeaddr;
	msg.flags = client->flags & I2C_M_TEN;
	msg.flags |= I2C_M_RD;
	msg.len = count;
	msg.buf = buf;

//	mutex_lock(&beam_mutex);
	ret = i2c_transfer(adap, &msg, 1);
//	mutex_unlock(&beam_mutex);

	/*
	 * If everything went ok (i.e. 1 msg received), return #bytes received,
	 * else error code.
	 */
	return (ret == 1) ? count : ret;
#endif
}

int beam_i2c_write(struct i2c_client *client, unsigned short  typeaddr, char *buf, int count)
{
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg;
	int ret;

	msg.addr = typeaddr;
	msg.flags = client->flags & I2C_M_TEN;
	msg.len = count;
	msg.buf = (char *)buf;

//	mutex_lock(&beam_mutex);
	ret = i2c_transfer(adapter, &msg, 1);
//	mutex_unlock(&beam_mutex);
	if (ret < 0){
	    pr_err("[%s] : write error : [%d]", __func__, ret);
	}

	return (ret == 1) ? count : ret;
}

#else

int beam_i2c_read(unsigned short  addr, int length, char *value)
{
	int ret;

	mutex_lock(&beam_mutex);
	ret = beam_gpio_i2c_write(addr, ((u8 *) & addr), 1 );
	mutex_unlock(&beam_mutex);

	if (ret >= 0)
	{
		mutex_lock(&beam_mutex);
		ret= beam_gpio_i2c_read(addr, value, length);
		mutex_unlock(&beam_mutex);
	}

	if (ret < 0)
	{
		pr_err("[BEAM] : read error : [%d]", ret);
	}

	return ret;
}

int beam_i2c_write(unsigned short  addr, char *buf, int length)
{

	int i;
	
	mutex_lock(&beam_mutex);
	i = beam_gpio_i2c_write(addr, buf, length );
	mutex_unlock(&beam_mutex);

	if(i<0) return -EIO;
	else return 1;
	}
#endif
