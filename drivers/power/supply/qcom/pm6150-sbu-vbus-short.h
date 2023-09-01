#ifndef __PM6150_SBU_SHORT__
 #define __PM6150_SBU_SHORT__

/**
* pm6150_detect_sbu_short - Detects and return the status of SBU-GND short or 
*			SBU-VBUS short or open. This will be called before and from AFC.
**/
int pm6150_detect_sbu_short(void);

/**
* pm6150_detect_sbu_short_init - Enumerates SBU Short detection resources.
**/
int pm6150_detect_sbu_short_init(struct smb_charger *chg);
 
enum {
	SBUx_VBUS_OPEN	= 0x0,
	
	SBUx_VBUS_SHORT	= 0x10,
	SBU1_VBUS_SHORT,
	SBU2_VBUS_SHORT,
	
	SBUx_GND_SHORT	= 0x20,
	SBU1_GND_SHORT,
	SBU2_GND_SHORT,
	
	CCx_VBUS_SHORT	= 0x30,
	
	SBUx_CCx_SHORT	= 0x40,
	SBU1_CC1_SHORT,
	SBU1_CC2_SHORT,
	SBU2_CC1_SHORT,
	SBU2_CC2_SHORT,
	
	SBUx_SHORT_UNDEF = 0xFF,
};

#endif //__PM6150_SBU_SHORT__