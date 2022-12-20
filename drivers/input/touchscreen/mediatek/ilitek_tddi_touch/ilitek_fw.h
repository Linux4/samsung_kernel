#ifndef __ILITEK_FW_H
#define __ILITEK_FW_H

unsigned char CTPM_FW_NONE[] = {
	#include "./FW_TDDI_TRUNK_FB.ili"
};

unsigned char CTPM_FW_XL[] = {
	#include "./FW_ILI7807G_Wingtek_N21_Test_FB_XL.ili"
 };

unsigned char CTPM_FW_XL_ROW[] = {
	#include "./FW_ILI7807G_Wingtek_N21_Test_FB_XL_ROW.ili"
};

unsigned char CTPM_FW_XL_NA[] = {
	#include "./FW_ILI7807G_Wingtek_N21_Test_FB_XL_NA.ili"
};
  
unsigned char CTPM_FW_TXD_ROW[] = {
	#include "./FW_ILI7807G_Wingtek_N21_Test_FB_TXD_ROW.ili"
};

unsigned char CTPM_FW_TXD_NA[] = {
	#include "./FW_ILI7807G_Wingtek_N21_Test_FB_TXD_NA.ili"
};
#endif
