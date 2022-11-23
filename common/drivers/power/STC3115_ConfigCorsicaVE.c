/********************************************************************/
//  Name          : STC3115_Config.c
//  Description :  Configuration of the STC3115 internal registers
//  Version       : 1
//  Author         : STMicroelectronics
/********************************************************************/
// STC3115_Config.c : implementation file
//  The MainTrim Function has to be called
/********************************************************************/

#include "STC3115_Config.h"

static struct i2c_client *sav_client;

#define NBRETRY 5
/*******************************************************************************
* Function Name  : I2C_WriteByte
* Description    : utility function to write several bytes
* Input          : NumberOfBytes, RegAddress, TxBuffer
* Return         : error status
*******************************************************************************/
int I2C_WriteByte(unsigned char DevAddr,unsigned char RegAddress,unsigned char *TxBuffer,int NumberOfBytes)
{
	int ret;

	ret = i2c_smbus_write_i2c_block_data(sav_client, RegAddress, NumberOfBytes, TxBuffer);
	if (ret < 0)
		dev_err(&sav_client->dev, "%s: err %d\n", __func__, ret);

	return ret;

#if  0
  int retry;
  int res;

  for (retry=0;retry<NBRETRY;retry++)
  {
    res = I2C_WriteByte(DevAddr,RegAddress,TxBuffer,NumberOfBytes);
    //I2C wryte function has to be updated accordingly to the customer environement
    if (res==0) break;
  }

  return(res);
#endif
}



/*******************************************************************************
* Function Name  : I2C_ReadByte
* Description    : utility function to read several bytes
* Input          : NumberOfBytes, RegAddress, , RxBuffer
* Return         : error status
*******************************************************************************/
int I2C_ReadByte(unsigned char DevAddr,unsigned char RegAddress,unsigned char *RxBuffer,int NumberOfBytes)
{
	int ret;

   ret = i2c_smbus_read_i2c_block_data(sav_client, RegAddress, NumberOfBytes, RxBuffer);
	if (ret < 0)
		dev_err(&sav_client->dev, "%s: err %d\n", __func__, ret);

	return ret;
#if 0
  int retry;
  int res;

  for (retry=0;retry<NBRETRY;retry++)
  {
    res = I2C_ReadByte(DevAddr,RegAddress,RxBuffer,NumberOfBytes);
    //I2C read function has to be updated accordingly to the customer environement
    if (res==0) break;
  }
  return(res);
#endif
}



/********************************************************************/
//  Internal functions descrioption
/********************************************************************/

void PreWriteNVN(unsigned char ErasedSector)
{
	unsigned char RxBuffer[10];
	unsigned char IDCode;

	I2C_ReadByte(0xE0,0x18,&IDCode,1); //read RAM
	if(IDCode == 0x13)
	{
		RxBuffer[0]=0x10;
		I2C_WriteByte(0xE0,0x00,RxBuffer,1);
		RxBuffer[0]=0xBE;
		I2C_WriteByte(0xE0,0x1C,RxBuffer,1);
		RxBuffer[0]=0xEF;
		I2C_WriteByte(0xE0,0x1D,RxBuffer,1);
		RxBuffer[0]=0x09;
		I2C_WriteByte(0xE0,0x43,RxBuffer,1);
		RxBuffer[0]=0xC0;
		I2C_WriteByte(0xE0,0x46,RxBuffer,1);
		msleep(3);
		RxBuffer[0]=ErasedSector;
		I2C_WriteByte(0xE0,0x20,RxBuffer,1);
		RxBuffer[0]=0x01;
		I2C_WriteByte(0xE0,0x45,RxBuffer,1);
		RxBuffer[0]=0xCA; // Load SER FTP opcode //3.2µs
		I2C_WriteByte(0xE0,0x46,RxBuffer,1);
		msleep(3);

		RxBuffer[0]=0x00;  // clear register 45
		I2C_WriteByte(0xE0,0x45,RxBuffer,1);
		RxBuffer[0]=0xCF;
		I2C_WriteByte(0xE0,0x46,RxBuffer,1);
		msleep(5);

		RxBuffer[0]=0xCD;  // Write Erase FTP opcode
		I2C_WriteByte(0xE0,0x46,RxBuffer,1);
		msleep(3);

	}
	else
	{
		RxBuffer[0]=0x10;  // Set GG_RUN = 1
		I2C_WriteByte(0xE0,0x00,RxBuffer,1);
		RxBuffer[0]=0xBE;  // Set TM
		I2C_WriteByte(0xE0,0x1F,RxBuffer,1);
		RxBuffer[0]=0xC0;  // Set FTP_PWR=1, FTP_RST_N=1
		I2C_WriteByte(0xE0,0x46,RxBuffer,1);
		msleep(3);
		RxBuffer[0]=ErasedSector;  // Load FF to erase all sectors of FTP
		I2C_WriteByte(0xE0,0x20,RxBuffer,1);
		RxBuffer[0]=0x01;  // Load SER opcode //3.2µs
		I2C_WriteByte(0xE0,0x45,RxBuffer,1);
		RxBuffer[0]=0xCA; // Load SER FTP opcode //3.2µs
		I2C_WriteByte(0xE0,0x46,RxBuffer,1);
		msleep(3);

		RxBuffer[0]=0x00;  // clear register 45
		I2C_WriteByte(0xE0,0x45,RxBuffer,1);
		RxBuffer[0]=0xCF;
		I2C_WriteByte(0xE0,0x46,RxBuffer,1);
		msleep(3);

		RxBuffer[0]=0xCD;  // Write Erase FTP opcode
		I2C_WriteByte(0xE0,0x46,RxBuffer,1);
		msleep(3);

	}
}

void ReadRAM(void)
{
	unsigned char RxBuffer[10];
	unsigned char IDCode;

	I2C_ReadByte(0xE0,0x18,&IDCode,1); //read RAM
	if(IDCode == 0x13)
	{
		//Enter test mode cut 1.x
		RxBuffer[0]=0x10;
		I2C_WriteByte(0xE0,0x00,RxBuffer,1);
		RxBuffer[0]=0xBE;
		I2C_WriteByte(0xE0,0x1C,RxBuffer,1);
		RxBuffer[0]=0xEF;
		I2C_WriteByte(0xE0,0x1D,RxBuffer,1);
		RxBuffer[0]=0x09;
		I2C_WriteByte(0xE0,0x43,RxBuffer,1);
	}
	else
	{
		//Enter test mode cut 2.0
		RxBuffer[0]=0x10;
		I2C_WriteByte(0xE0,0x00,RxBuffer,1);
		RxBuffer[0]=0xBE;
		I2C_WriteByte(0xE0,0x1F,RxBuffer,1);
	}
}

void ExitTest(void)
{
	unsigned char RxBuffer[10];
	unsigned char IDCode;

	I2C_ReadByte(0xE0,0x18,&IDCode,1); //read RAM
	if(IDCode == 0x13)
	{
		RxBuffer[0]=0x00;
		I2C_WriteByte(0xE0,0x46,RxBuffer,1);
		RxBuffer[0]=0x00;
		I2C_WriteByte(0xE0,0x43,RxBuffer,1);
		RxBuffer[0]=0x00;
		I2C_WriteByte(0xE0,0x1C,RxBuffer,1);
		RxBuffer[0]=0x00;
		I2C_WriteByte(0xE0,0x1D,RxBuffer,1);
		RxBuffer[0]=0x00;
		I2C_WriteByte(0xE0,0x00,RxBuffer,1);
		msleep(5);
	}
	else
	{
		  //exit test mode
		RxBuffer[0]=0x00;
		I2C_WriteByte(0xE0,0x46,RxBuffer,1);
		RxBuffer[0]=0x00;
		I2C_WriteByte(0xE0,0x43,RxBuffer,1);
		/*RxBuffer[0]=0x00;
		I2C_WriteByte(0xE0,0x1C,RxBuffer,1);
		RxBuffer[0]=0x00;
		I2C_WriteByte(0xE0,0x1D,RxBuffer,1); */
		RxBuffer[0]=0x00;
		I2C_WriteByte(0xE0,0x00,RxBuffer,1);
		msleep(5);
	}
}

void ReadSector(char SectorNum, unsigned char *SectorData)
{
	unsigned char RxBuffer[10];

	//read sector
	RxBuffer[0]=SectorNum;
	I2C_WriteByte(0xE0,0x45,RxBuffer,1);
	RxBuffer[0]=0xC9;
	I2C_WriteByte(0xE0,0x46,RxBuffer,1);
	msleep(5); // wait
	I2C_ReadByte(0xE0,0x20,SectorData,8); //read RAM
}

void WriteSector(char SectorNum, unsigned char *SectorData)
{
	unsigned char RxBuffer[10];

	//write sector
	I2C_WriteByte(0xE0,0x20,SectorData,8);
	msleep(5); // wait

	RxBuffer[0]=0x00;
	I2C_WriteByte(0xE0,0x45,RxBuffer,1);

	RxBuffer[0]=0xCA;
	I2C_WriteByte(0xE0,0x46,RxBuffer,1);
	msleep(5); // wait


	RxBuffer[0]=SectorNum;
	I2C_WriteByte(0xE0,0x45,RxBuffer,1);
	RxBuffer[0]=0xCE;
	I2C_WriteByte(0xE0,0x46,RxBuffer,1);
	msleep(5); // wait

}

/********************************************************************/
//  End of the Internal functions descrioption
/********************************************************************/

/*******************************************************************************
* Function Name  : MainTrim
* Description : STC3115 internal register configuration function
* This function has to be called at stable battery voltage during the application magnufacturing
* This function is checking itself the propper configuration of the device.
*******************************************************************************/
void MainTrim(struct i2c_client * client)
{
	unsigned char IDCode,RAMBuffer[16];
	int ii,error,testnbr;

	sav_client = client;

    //Check the cut v ersion
	I2C_ReadByte(0xE0,0x18,&IDCode,1); //read IDcode
	if(IDCode == 0x13)
	{
	  return;
	}

	I2C_ReadByte(0xE0,0x20,RAMBuffer,16); //read RAM memory content

	ReadRAM(); //Enter test mode
	ReadSector(0, Sector0);  //read sector 0
	ExitTest(); //exit test mode

    if ( (Sector0[1] & 0x01) == 0 )
    {
      testnbr=0;
      do
      {
        ReadRAM(); //Enter test mode
		ReadSector(0, Sector0);  //read sector 0
		ExitTest(); //exit test mode

        Sector0	[1] = Sector0[1] | 0x01 ;
        PreWriteNVN(0x40);     //Enter test mode
		WriteSector(6,Sector0);//write sector 0 in 6
        ExitTest();

        ReadRAM(); //Enter test mode
		ReadSector(6, Sector6);  //read sector 6
		ExitTest(); //exit test mode

        error=0;
        for(ii=0;ii<8;ii++)
        {
          if(Sector0[ii] != Sector6[ii]) error++;
        }
        testnbr++;
      }
      while(error > 0 && testnbr < 5);
      if (testnbr >= 3)
	  {
	      I2C_WriteByte(0xE0,0x20,RAMBuffer,16);//restore RAM memory content
          return;
		}

      testnbr=0;

      do
      {
        PreWriteNVN(0x01);     //Enter test mode
		WriteSector(0,Sector6);//write sector 0 in 0
        ExitTest();

        ReadRAM(); //Enter test mode
		ReadSector(0, Sector0);  //read sector 0
		ExitTest(); //exit test mode

        error=0;
        for(ii=0;ii<8;ii++)
        {
          if(Sector0[ii] != Sector6[ii]) error++;
        }
        testnbr++;
      }
      while(error > 0 && testnbr < 5);
    }



    //Check sector 6 status
	Sector6	[	0	]=0x1B	;
	Sector6	[	1	]=0xCD	;
	Sector6	[	2	]=0x1B	;
	Sector6	[	3	]=0x13	;
	Sector6	[	4	]=0x1C	;
	Sector6	[	5	]=0x57	;
	Sector6	[	6	]=0x1C	;
	Sector6	[	7	]=0x09	;

    ReadRAM(); //Enter test mode
	ReadSector(6, Sector0);  //read sector 0
	ExitTest(); //exit test mode


    error=0;
    for(ii=0;ii<8;ii++)
    {
      if(Sector0[ii] != Sector6[ii]) error++;
    }

    testnbr=0;
    while(error > 0 && testnbr < 5)
    {
        PreWriteNVN(0x40);     //Enter test mode
		WriteSector(6,Sector6);//write sector 6 in 6
        ExitTest();

        ReadRAM(); //Enter test mode
	    ReadSector(6, Sector0);  //read sector 0
		ExitTest(); //exit test mode

        error=0;
        for(ii=0;ii<8;ii++)
        {
			if(Sector0[ii] != Sector6[ii]) error++;
        }
        testnbr++;
    }

	I2C_WriteByte(0xE0,0x20,RAMBuffer,16);//restore RAM memory content
}


