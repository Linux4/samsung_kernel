#pragma once

///////////////////////////////////////////////////////////////////////////////////
// GMvSystem.h
//---------------------------------------------------------------------------------
// Created by Byeongjae Gim
// Email: gaiama78@gmail.com, byeongjae.kim@samsung.com
///////////////////////////////////////////////////////////////////////////////////

// ! include
#include "GSystem.h"
#include "GMvCommon.h"

// ! extern
void GMvGtReadI2cRegister( uint16, void*, sint32 );
void GMvGtWriteI2cRegister( uint16, void*, sint32 );

void GMvBraneIsr( void );

sint32 GMvPacketMakeHeader( uint8*, EG_MV_PKT_ID, uint8, uint8, uint16 );
sint32 GMvPacketMakePayload( uint8*, sint32, uint8*, sint32 );
struct SGMvPktHdr* GMvPacketCheckHeader( uint8*, sint32 );

void GMvCrc16Open( void );
uint16 GMvCrc16Encode( uint8*, sint32 );
