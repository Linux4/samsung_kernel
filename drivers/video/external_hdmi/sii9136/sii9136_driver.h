
#ifndef __HDMI_TX_DRIVER_H__
#define __HDMI_TX_DRIVER_H__

#include "def.h"

#define ClearInterrupt(x)                   WriteByteTPI(TPI_INTERRUPT_STATUS_REG, x)         

bool TPI_Init (void);			// Document purpose, usage
void TPI_Poll (void);			// Document purpose, usage, rename

bool EnableInterrupts(byte Interrupt_Pattern);

void EnableTMDS (void);			// Document purpose, usage
void DisableTMDS (void);		// Document purpose, usage

void RestartHDCP (void);		// Document purpose, usage
void SetAudioMute (byte audioMute);

void ReadModifyWriteTPI(byte Offset, byte Mask, byte Value);
byte ReadByteTPI(byte);
void WriteByteTPI(byte, byte);
void ReadBlockTPI(byte TPI_Offset, word NBytes, byte * pData);
void WriteBlockTPI(byte TPI_Offset, word NBytes, byte * pData);

int hdmi_read_reg(uint8_t slave_addr, uint8_t offset);
int hdmi_write_reg(uint8_t slave_addr, uint8_t offset, uint8_t value);

int hdmi_read_block_reg(uint8_t slave_addr, uint8_t offset,
				u8 len, u8 *values);
int hdmi_ReadSegmentBlock(uint8_t slave_addr,uint8_t segment,uint8_t offset,
				u8 len, u8 *values);				
int hdmi_write_block_reg(uint8_t slave_addr, uint8_t offset,
				u8 len, u8 *values);

bool SetAudioInfoFrames(byte, byte, byte, byte, byte);

bool SetChannelLayout(byte);


#endif
