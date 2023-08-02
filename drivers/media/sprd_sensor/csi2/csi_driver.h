/*                                                                                      
 * csi_driver                                                                           
 * 12/05/2010                                                                           
 * Synopsys Inc. PT02                                                                   
 */                                                                                     
                                                                                        
#ifndef CSI_DRIVER_H_                                                                   
#define CSI_DRIVER_H_                                                                   
                                                                                        
#include "csi_types.h"                                                                      
                                                                                        
//#define CURRENT_VERSION 0x3130302A                                                      
#ifdef __SIMULATOR__
#define CURRENT_VERSION 0     
#else // __SIMULATOR__

#if defined (CONFIG_ARCH_SC8825)
#define CURRENT_VERSION 0x3130322A     
#elif defined (CONFIG_ARCH_SCX35)
#define CURRENT_VERSION 0x3130332A
#endif

#endif // __SIMULATOR__

#define PHY_TESTCLR 0
#define PHY_TESTCLK 1
#define PHY_TESTDIN 0
#define PHY_TESTDOUT 8
#define PHY_TESTEN 16

#define PHY_TESTDIN_W 8
#define PHY_TESTDOUT_W 8

#define CSI_PCLK_CFG_COUNTER 29

typedef enum                                                                            
{                                                                                       
    VERSION =       0x00,                                                               
    N_LANES =       0x04,                                                               
    PHY_SHUTDOWNZ = 0x08,                                                               
    DPHY_RSTZ =     0x0C,                                                               
    CSI2_RESETN =   0x10,                                                               
    PHY_STATE =     0x14,                                                               
    DATA_IDS_1 =    0x18,                                                               
    DATA_IDS_2 =    0x1C,                                                               
    ERR1 =          0x20,                                                               
    ERR2 =          0x24,                                                               
    MASK1 =         0x28,                                                               
    MASK2 =         0x2C,
    PHY_TST_CRTL0 = 0x30,
    PHY_TST_CRTL1 = 0x34
} csi_registers_t;                                                                      
                                                                                        
typedef enum                                                                            
{                                                                                       
    CSI_LANE_OFF = 0,                                                                   
    CSI_LANE_ON,                                                                        
    CSI_LANE_ULTRA_LOW_POWER,                                                           
    CSI_LANE_STOP,                                                                      
    CSI_LANE_HIGH_SPEED                                                                 
} csi_lane_state_t;                                                                     
                                                                                        
typedef enum                                                                            
{                                                                                       
    ERR_NOT_INIT = 0xFE,                                                                
    ERR_ALREADY_INIT = 0xFD,                                                            
    ERR_NOT_COMPATIBLE = 0xFC,                                                          
    ERR_UNDEFINED = 0xFB,                                                               
    ERR_OUT_OF_BOUND = 0xFA,                                                            
    SUCCESS = 0                                                                         
} csi_error_t;                                                                          
                                                                                        
typedef enum                                                                            
{                                                                                       
    NULL_PACKET = 0x10,                                                                 
    BLANKING_DATA = 0x11,                                                               
    EMBEDDED_8BIT_NON_IMAGE_DATA = 0x12,                                                
    YUV420_8BIT = 0x18,                                                                 
    YUV420_10BIT = 0x19,                                                                
    LEGACY_YUV420_8BIT = 0x1A,                                                          
    YUV420_8BIT_CHROMA_SHIFTED = 0x1C,                                                  
    YUV420_10BIT_CHROMA_SHIFTED = 0x1D,                                                 
    YUV422_8BIT = 0x1E,                                                                 
    YUV422_10BIT = 0x1F,                                                                
    RGB444 = 0x20,                                                                      
    RGB555 = 0x21,                                                                      
    RGB565 = 0x22,                                                                      
    RGB666 = 0x23,                                                                      
    RGB888 = 0x24,                                                                      
    RAW6 = 0x28,                                                                        
    RAW7 = 0x29,                                                                        
    RAW8 = 0x2A,                                                                        
    RAW10 = 0x2B,                                                                       
    RAW12 = 0x2C,                                                                       
    RAW14 = 0x2D,                                                                       
    USER_DEFINED_8BIT_DATA_TYPE_1 = 0x30,                                               
    USER_DEFINED_8BIT_DATA_TYPE_2 = 0x31,                                               
    USER_DEFINED_8BIT_DATA_TYPE_3 = 0x32,                                               
    USER_DEFINED_8BIT_DATA_TYPE_4 = 0x33,                                               
    USER_DEFINED_8BIT_DATA_TYPE_5 = 0x34,                                               
    USER_DEFINED_8BIT_DATA_TYPE_6 = 0x35,                                               
    USER_DEFINED_8BIT_DATA_TYPE_7 = 0x36,                                               
    USER_DEFINED_8BIT_DATA_TYPE_8 = 0x37                                                
} csi_data_type_t;                                                                      

struct csi_pclk_cfg {
	u32 pclk_start;
	u32 pclk_end;
	u8    hsfreqrange;
	u8    hsrxthssettle;
};

void dphy_init(u32 pclk, u32 phy_id);
u8 csi_init(u32 base_address);
u8 csi_close(void);
u8 csi_get_on_lanes(void);
u8 csi_set_on_lanes(u8 lanes);
u8 csi_shut_down_phy(u8 shutdown);
u8 csi_reset_phy(void);
u8 csi_reset_controller(void);
csi_lane_state_t csi_lane_module_state(u8 lane);
csi_lane_state_t csi_clk_state(void);
u8 csi_payload_bypass(u8 on);
u8 csi_register_line_event(u8 virtual_channel_no, csi_data_type_t data_type, u8 offset);
u8 csi_unregister_line_event(u8 offset);
u8 csi_get_registered_line_event(u8 offset);
u8 csi_event_disable(u32 mask, u8 err_reg_no);
u8 csi_event_enable(u32 mask, u8 err_reg_no);
u32 csi_event_get_source(u8 err_reg_no);
                                                                                        
/******************************************************************************         
 * register access methods                                                              
******************************************************************************/         
u32 csi_core_read(csi_registers_t address);
u8 csi_core_write(csi_registers_t address, u32 data);
u8 csi_core_write_part(csi_registers_t address, u32 data, u8 shift, u8 width);
u32 csi_core_read_part(csi_registers_t address, u8 shift, u8 width);
                                                                                        
#endif /* CSI_DRIVER_H_ */                                                              
                                                                                              
