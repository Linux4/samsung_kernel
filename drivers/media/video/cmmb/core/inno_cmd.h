
/*
 * Copyright (C) 2010 Innofidei Corporation
 * Author:      sean <zhaoguangyu@innofidei.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 */

#ifndef __INNO_CMD_H_
#define __INNO_CMD_H_

/* interrupt and fetch cmmb data cmd */
#define READ_INT_STATUS                         0x6e

#define READ_INT0_STATUS                        0x0A    
#define READ_INT1_STATUS                        0x0B
#define READ_INT2_STATUS                        0x0C    

#define READ_TS0_LEN_LOW                        0x1b
#define READ_TS0_LEN_MID                        0x1c
#define READ_TS0_LEN_HIGH                       0x1d

#define READ_LG0_LEN_LOW                        0x15
#define READ_LG0_LEN_MID                        0x16
#define READ_LG0_LEN_HIGH                       0x17

#define READ_LG1_LEN_LOW                        0x18
#define READ_LG1_LEN_MID                        0x19
#define READ_LG1_LEN_HIGH                       0x1A

#define READ_LG0_LEN                            0x62
#define READ_LG1_LEN                            0x63
#define READ_LG2_LEN                            0x64
#define READ_LG3_LEN                            0x65
#define READ_LG4_LEN                            0x66
#define READ_LG5_LEN                            0x67
#define READ_LG6_LEN                            0x68
#define READ_LG7_LEN                            0x69
#define READ_LG8_LEN                            0x6a
#define READ_LG9_LEN                            0x6b
#define READ_LG10_LEN                           0x6c
#define READ_LG11_LEN                           0x6d

/* INT_STATUS Bit Mask */
#define LG0_DATA_RDY                            (0x01)
#define LG1_DATA_RDY                            (0x02)
#define LG2_DATA_RDY                            (0x04)    //100
#define LG3_DATA_RDY                            (0x08)    //1000
#define LG4_DATA_RDY                            (0x10)    //10000
#define LG5_DATA_RDY                            (0x20)    //100000
#define LG6_DATA_RDY                            (0x40)    //1000000
#define LG7_DATA_RDY                            (0x80)    //10000000

#define FETCH_TS0_DATA                          0x9d
#define FETCH_LG0_DATA                          0x99
#define FETCH_LG1_DATA                          0x9b
#define FETCH_LG2_DATA                          0x80
#define FETCH_LG3_DATA                          0x81
#define FETCH_LG4_DATA                          0x82
#define FETCH_LG5_DATA                          0x83
#define FETCH_LG6_DATA                          0x84
#define FETCH_LG7_DATA                          0x85
#define FETCH_LG8_DATA                          0x86
#define FETCH_LG9_DATA                          0x87
#define FETCH_LG10_DATA                         0x88
#define FETCH_LG11_DATA                         0x89

/*   communication cmd */
#define RSP_DATA_VALID                          0x80
#define CMD_BUSY                                        0x80

#define CMD_SET_FREQUENCY                       0x1  
#define CMD_SET_CHANNEL_CONFIG          0x2
#define CMD_CLOSE_ALL_CHANNEL           0x3
#define CMD_GET_CHANNEL_CONFIG          0x4
#define CMD_GET_FW_VER                          0x5
#define CMD_GET_FW_CHECK_SUM            0x6    
#define CMD_SET_SLEEP			           0x9  

#ifdef IF206
#define CMD_GET_CASRDSN			 0xCE
#define CMD_GET_CASID               		0xCC
#define CMD_SET_EMM_CHANNEL         0xCB
#define CMD_SET_CA_TABLE            	0xCD  
#endif

#define CMD_CW_DETECT_MODE_ON		        0x8A
#define CMD_CW_DETECT_MODE_OFF			0x8B

#endif
