/********************************************************************/
//  Name          : STC3115_Config.h
//  Description :  Configuration of the STC3115 internal registers header file
//  Version       : 0
//  Author         : STMicroelectronics
//
/********************************************************************/

#define STC_CONFIG_CLIENT_FAIL 1
#define STC_CONFIG_IDCODE_NOTMATCH 2
#define STC_CONFIG_TEST_FAIL 3
/********************************************************************/
//Global Sector table definition
/********************************************************************/
  unsigned char Sector0[8];
  unsigned char Sector1[8];
  unsigned char Sector2[8];
  unsigned char Sector3[8];
  unsigned char Sector4[8];
  unsigned char Sector5[8];
  unsigned char Sector6[8];
  unsigned char Sector7[8];
/********************************************************************/
//Function declaration
/********************************************************************/
void ReadRAM(void);
void PreWriteNVN(unsigned char ErasedSector);
void ExitTest(void);
void ReadSector( char SectorNum, unsigned char *SectorData);
void WriteSector(char SectorNum, unsigned char *SectorData);
