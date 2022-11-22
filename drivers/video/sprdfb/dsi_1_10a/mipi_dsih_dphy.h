/*                                                                                                                   
 * @file mipi_dsih_dphy.h                                                                                            
 *                                                                                                                   
 *  Synopsys Inc.                                                                                                    
 *  SG DWC PT02                                                                                                      
 */                                                                                                                  
                                                                                                                     
#ifndef MIPI_DSIH_DPHY_H_                                                                                            
#define MIPI_DSIH_DPHY_H_                                                                                            
                                                                                                                     
#include "mipi_dsih_local.h"                                                                                         
                                                                                                                     
#define R_DSI_HOST_PHY_RSTZ         0x54UL                                                                           
#define R_DSI_HOST_PHY_IF_CFG       0x58UL                                                                           
#define R_DSI_HOST_PHY_IF_CTRL      0x5CUL                                                                           
#define R_DSI_HOST_PHY_STATUS       0x60UL                                                                           
#define R_DSI_HOST_PHY_TST_CRTL0    0x64UL                                                                           
#define R_DSI_HOST_PHY_TST_CRTL1    0x68UL                                                                           
                                                                                                                     
/* obligatory functions - code can be changed for different phys*/                                                   
dsih_error_t mipi_dsih_dphy_open(dphy_t * phy);                                                                      
dsih_error_t mipi_dsih_dphy_configure(dphy_t * phy, uint8_t no_of_lanes, uint32_t output_freq);                      
dsih_error_t mipi_dsih_dphy_close(dphy_t * phy);                                                                     
                                                                                                                     
void mipi_dsih_dphy_clock_en(dphy_t * instance, int en);                                                             
void mipi_dsih_dphy_reset(dphy_t * instance, int reset);                                                             
void mipi_dsih_dphy_shutdown(dphy_t * instance, int powerup);                                                        
                                                                                                                     
void mipi_dsih_dphy_stop_wait_time(dphy_t * instance, uint8_t no_of_byte_cycles);                                    
void mipi_dsih_dphy_no_of_lanes(dphy_t * instance, uint8_t no_of_lanes);                                             
uint8_t mipi_dsih_dphy_get_no_of_lanes(dphy_t * instance);                                                           
void mipi_dsih_dphy_enable_hs_clk(dphy_t * instance, int enable);                                                    
dsih_error_t mipi_dsih_dphy_escape_mode_trigger(dphy_t * instance, uint8_t trigger_request);                         
#ifdef GEN_2
dsih_error_t mipi_dsih_dphy_ulps_data_lanes(dphy_t * instance, int enable);
dsih_error_t mipi_dsih_dphy_ulps_clk_lane(dphy_t * instance, int enable);
#else
void mipi_dsih_dphy_ulps_data_lanes(dphy_t * instance, int enable);                                                  
void mipi_dsih_dphy_ulps_clk_lane(dphy_t * instance, int enable);                                                    
#endif
uint32_t mipi_dsih_dphy_status(dphy_t * instance, uint16_t mask);                                                    
/* end of obligatory functions*/                                                                                     
void mipi_dsih_dphy_test_clock(dphy_t * instance, int value);                                                        
void mipi_dsih_dphy_test_clear(dphy_t * instance, int value);                                                        
void mipi_dsih_dphy_test_en(dphy_t * instance, uint8_t on_falling_edge);                                             
uint8_t mipi_dsih_dphy_test_data_out(dphy_t * instance);                                                             
void mipi_dsih_dphy_test_data_in(dphy_t * instance, uint8_t test_data);                                              
                                                                                                                     
void mipi_dsih_dphy_write(dphy_t * instance, uint8_t address, uint8_t * data, uint8_t data_length);                  
                                                                                                                     
void mipi_dsih_dphy_write_word(dphy_t * instance, uint32_t reg_address, uint32_t data);                              
void mipi_dsih_dphy_write_part(dphy_t * instance, uint32_t reg_address, uint32_t data, uint8_t shift, uint8_t width);
uint32_t mipi_dsih_dphy_read_word(dphy_t * instance, uint32_t reg_address);                                          
uint32_t mipi_dsih_dphy_read_part(dphy_t * instance, uint32_t reg_address, uint8_t shift, uint8_t width);            
#endif /* MIPI_DSIH_DPHY_H_ */                                                                                       
