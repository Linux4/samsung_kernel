/*                                                                                                                                      
 * @file mipi_dsih_hal.h                                                                                                                
 *                                                                                                                                      
 *  Synopsys Inc.                                                                                                                       
 *  SG DWC PT02                                                                                                                         
 */                                                                                                                                     
/*                                                                                                                                      
    The Synopsys Software Driver and documentation (hereinafter "Software")                                                             
    is an unsupported proprietary work of Synopsys, Inc. unless otherwise                                                               
    expressly agreed to in writing between  Synopsys and you.                                                                           
                                                                                                                                        
    The Software IS NOT an item of Licensed Software or Licensed Product under                                                          
    any End User Software License Agreement or Agreement for Licensed Product                                                           
    with Synopsys or any supplement thereto.  Permission is hereby granted,                                                             
    free of charge, to any person obtaining a copy of this software annotated                                                           
    with this license and the Software, to deal in the Software without                                                                 
    restriction, including without limitation the rights to use, copy, modify,                                                          
    merge, publish, distribute, sublicense, and/or sell copies of the Software,                                                         
    and to permit persons to whom the Software is furnished to do so, subject                                                           
    to the following conditions:                                                                                                        
                                                                                                                                        
    The above copyright notice and this permission notice shall be included in                                                          
    all copies or substantial portions of the Software.                                                                                 
                                                                                                                                        
    THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS                                                           
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE                                                           
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE                                                          
    ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,                                                         
    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES                                                                  
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR                                                                  
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER                                                          
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT                                                                  
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY                                                           
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH                                                         
    DAMAGE.                                                                                                                             
 */                                                                                                                                     
                                                                                                                                        
#ifndef MIPI_DSIH_HAL_H_                                                                                                                
#define MIPI_DSIH_HAL_H_                                                                                                                
                                                                                                                                        
#include "mipi_dsih_local.h"                                                                                                            
                                                                                                                                        
#define R_DSI_HOST_VERSION          0x00UL                                                                                              
#define R_DSI_HOST_PWR_UP           0x04UL                                                                                              
#define R_DSI_HOST_CLK_MGR          0x08UL                                                                                              
#define R_DSI_HOST_DPI_CFG          0x0CUL                                                                                              
#define R_DSI_HOST_DBI_CFG          0x10UL                                                                                              
#define R_DSI_HOST_DBI_CMDSIZE      0x14UL                                                                                              
#define R_DSI_HOST_PCKHDL_CFG       0x18UL                                                                                              
#define R_DSI_HOST_VID_MODE_CFG     0x1CUL                                                                                              
#define R_DSI_HOST_VID_PKT_CFG      0x20UL                                                                                              
#define R_DSI_HOST_CMD_MODE_CFG     0x24UL                                                                                              
#define R_DSI_HOST_TMR_LINE_CFG     0x28UL                                                                                              
#define R_DSI_HOST_VTIMING_CFG      0x2CUL                                                                                              
#define R_DSI_HOST_PHY_TMR_CFG      0x30UL                                                                                              
#define R_DSI_HOST_GEN_HDR          0x34UL                                                                                              
#define R_DSI_HOST_GEN_PLD_DATA     0x38UL                                                                                              
#define R_DSI_HOST_CMD_PKT_STATUS   0x3CUL                                                                                              
#define R_DSI_HOST_TO_CNT_CFG       0x40UL                                                                                              
#define R_DSI_HOST_ERROR_ST0        0x44UL                                                                                              
#define R_DSI_HOST_ERROR_ST1        0x48UL                                                                                              
#define R_DSI_HOST_ERROR_MSK0       0x4CUL                                                                                              
#define R_DSI_HOST_ERROR_MSK1       0x50UL                                                                                              


typedef enum _Dsi_Int0_Type_ {
    ack_with_err_0,
    ack_with_err_1,
    ack_with_err_2,
    ack_with_err_3,
    ack_with_err_4,
    ack_with_err_5,
    ack_with_err_6,
    ack_with_err_7,
    ack_with_err_8,
    ack_with_err_9,
    ack_with_err_10,
    ack_with_err_11,
    ack_with_err_12,
    ack_with_err_13,
    ack_with_err_14,
    ack_with_err_15,
    dphy_errors_0,
    dphy_errors_1,
    dphy_errors_2,
    dphy_errors_3,
    dphy_errors_4,
    DSI_INT0_MAX,
}Dsi_Int0_Type;

typedef enum _Dsi_Int1_Type_ {
    to_hs_tx,
    to_lp_rx,
    ecc_single_err,
    ecc_multi_err,
    crc_err,
    pkt_size_err,
    eopt_err,
    dpi_pld_wr_err,
    gen_cmd_wr_err,
    gen_pld_wr_err,
    gen_pld_send_err,
    gen_pld_rd_err,
    gen_pld_recv_err,
    dbi_cmd_wr_err,
    dbi_pld_wr_err,
    dbi_pld_rd_err,
    dbi_pld_recv_err,
    dbi_illegal_comm_err,
    DSI_INT1_MAX,
}Dsi_Int1_Type;
#define DSI_INT_MASK0_SET(bit,val)\
{\
	uint32_t reg_val = dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_ERROR_MSK0);\
	reg_val = (val == 1)?(reg_val | (1UL<<bit)):(reg_val & (~(1UL<<bit)));\
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_ERROR_MSK0, reg_val);\
}

#define DSI_INT_MASK1_SET(bit,val)\
{\
	uint32_t reg_val = dsi_core_read_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_ERROR_MSK1);\
	reg_val = (val == 1)?(reg_val | (1UL<<bit)):(reg_val & (~(1UL<<bit)));\
	dsi_core_write_function(SPRD_MIPI_DSIC_BASE, R_DSI_HOST_ERROR_MSK1, reg_val);\
}


uint32_t mipi_dsih_hal_get_version(dsih_ctrl_t * instance);                                                                             
void mipi_dsih_hal_power(dsih_ctrl_t * instance, int on);                                                                               
int mipi_dsih_hal_get_power(dsih_ctrl_t * instance);                                                                                    
void mipi_dsih_hal_tx_escape_division(dsih_ctrl_t * instance, uint8_t tx_escape_division);                                              
void mipi_dsih_hal_dpi_video_vc(dsih_ctrl_t * instance, uint8_t vc);                                                                    
uint8_t mipi_dsih_hal_dpi_get_video_vc(dsih_ctrl_t * instance);                                                                         
                                                                                                                                        
dsih_error_t mipi_dsih_hal_dpi_color_coding(dsih_ctrl_t * instance, dsih_color_coding_t color_coding);                                  
dsih_color_coding_t mipi_dsih_hal_dpi_get_color_coding(dsih_ctrl_t * instance);                                                         
uint8_t mipi_dsih_hal_dpi_get_color_depth(dsih_ctrl_t * instance);                                                                      
uint8_t mipi_dsih_hal_dpi_get_color_config(dsih_ctrl_t * instance);                                                                     
void mipi_dsih_hal_dpi_18_loosely_packet_en(dsih_ctrl_t * instance, int enable);                                                        
void mipi_dsih_hal_dpi_color_mode_pol(dsih_ctrl_t * instance, int active_low);                                                          
void mipi_dsih_hal_dpi_shut_down_pol(dsih_ctrl_t * instance, int active_low);                                                           
void mipi_dsih_hal_dpi_hsync_pol(dsih_ctrl_t * instance, int active_low);                                                               
void mipi_dsih_hal_dpi_vsync_pol(dsih_ctrl_t * instance, int active_low);                                                               
void mipi_dsih_hal_dpi_dataen_pol(dsih_ctrl_t * instance, int active_low);                                                              
void mipi_dsih_hal_dpi_frame_ack_en(dsih_ctrl_t * instance, int enable);                                                                
void mipi_dsih_hal_dpi_null_packet_en(dsih_ctrl_t * instance, int enable);                                                              
void mipi_dsih_hal_dpi_multi_packet_en(dsih_ctrl_t * instance, int enable);                                                             
void mipi_dsih_hal_dpi_lp_during_hfp(dsih_ctrl_t * instance, int enable);                                                               
void mipi_dsih_hal_dpi_lp_during_hbp(dsih_ctrl_t * instance, int enable);                                                               
void mipi_dsih_hal_dpi_lp_during_vactive(dsih_ctrl_t * instance, int enable);                                                           
void mipi_dsih_hal_dpi_lp_during_vfp(dsih_ctrl_t * instance, int enable);                                                               
void mipi_dsih_hal_dpi_lp_during_vbp(dsih_ctrl_t * instance, int enable);                                                               
void mipi_dsih_hal_dpi_lp_during_vsync(dsih_ctrl_t * instance, int enable);                                                             
                                                                                                                                        
dsih_error_t mipi_dsih_hal_dpi_video_mode_type(dsih_ctrl_t * instance, dsih_video_mode_t type);                                         
void mipi_dsih_hal_dpi_video_mode_en(dsih_ctrl_t * instance, int enable);                                                               
int mipi_dsih_hal_dpi_is_video_mode(dsih_ctrl_t * instance);                                                                            
dsih_error_t mipi_dsih_hal_dpi_null_packet_size(dsih_ctrl_t * instance, uint16_t size);                                                 
dsih_error_t mipi_dsih_hal_dpi_chunks_no(dsih_ctrl_t * instance, uint16_t no);                                                          
dsih_error_t mipi_dsih_hal_dpi_video_packet_size(dsih_ctrl_t * instance, uint16_t size);                                                
                                                                                                                                        
void mipi_dsih_hal_tear_effect_ack_en(dsih_ctrl_t * instance, int enable);                                                              
                                                                                                                                        
void mipi_dsih_hal_cmd_ack_en(dsih_ctrl_t * instance, int enable);                                                                      
dsih_error_t mipi_dsih_hal_dcs_wr_tx_type(dsih_ctrl_t * instance, unsigned no_of_param, int lp);                                        
dsih_error_t mipi_dsih_hal_dcs_rd_tx_type(dsih_ctrl_t * instance, unsigned no_of_param, int lp);
/*Jessica add to support max rd packet size command*/
dsih_error_t mipi_dsih_hal_max_rd_packet_size_type(dsih_ctrl_t * instance, int lp);
dsih_error_t mipi_dsih_hal_gen_wr_tx_type(dsih_ctrl_t * instance, unsigned no_of_param, int lp);                                        
dsih_error_t mipi_dsih_hal_gen_rd_tx_type(dsih_ctrl_t * instance, unsigned no_of_param, int lp);                                        
void mipi_dsih_hal_max_rd_size_type(dsih_ctrl_t * instance, int lp);                                                                    
void mipi_dsih_hal_gen_cmd_mode_en(dsih_ctrl_t * instance, int enable);                                                                 
int mipi_dsih_hal_gen_is_cmd_mode(dsih_ctrl_t * instance);                                                                              
                                                                                                                                        
void mipi_dsih_hal_dpi_hline(dsih_ctrl_t * instance, uint16_t time);                                                                    
void mipi_dsih_hal_dpi_hbp(dsih_ctrl_t * instance, uint16_t time);                                                                      
void mipi_dsih_hal_dpi_hsa(dsih_ctrl_t * instance, uint16_t time);                                                                      
void mipi_dsih_hal_dpi_vactive(dsih_ctrl_t * instance, uint16_t lines);                                                                 
void mipi_dsih_hal_dpi_vfp(dsih_ctrl_t * instance, uint16_t lines);                                                                     
void mipi_dsih_hal_dpi_vbp(dsih_ctrl_t * instance, uint16_t lines);                                                                     
void mipi_dsih_hal_dpi_vsync(dsih_ctrl_t * instance, uint16_t lines);                                                                   
dsih_error_t mipi_dsih_hal_gen_packet_header(dsih_ctrl_t * instance, uint8_t vc, uint8_t packet_type, uint8_t ms_byte, uint8_t ls_byte);
/*dsih_error_t mipi_dsih_hal_gen_packet_payload(dsih_ctrl_t * instance, uint32_t* payload, uint16_t payload_size);*/                    
dsih_error_t mipi_dsih_hal_gen_packet_payload(dsih_ctrl_t * instance, uint32_t payload);                                                
dsih_error_t mipi_dsih_hal_gen_read_payload(dsih_ctrl_t * instance, uint32_t* payload);                                                 
                                                                                                                                        
void mipi_dsih_hal_timeout_clock_division(dsih_ctrl_t * instance, uint8_t byte_clk_division_factor);                                    
void mipi_dsih_hal_lp_rx_timeout(dsih_ctrl_t * instance, uint16_t count);                                                               
void mipi_dsih_hal_hs_tx_timeout(dsih_ctrl_t * instance, uint16_t count);                                                               
                                                                                                                                        
uint32_t mipi_dsih_hal_error_status_0(dsih_ctrl_t * instance, uint32_t mask);                                                           
uint32_t mipi_dsih_hal_error_status_1(dsih_ctrl_t * instance, uint32_t mask);                                                           
void mipi_dsih_hal_error_mask_0(dsih_ctrl_t * instance, uint32_t mask);                                                                 
void mipi_dsih_hal_error_mask_1(dsih_ctrl_t * instance, uint32_t mask);                                                                 
uint32_t mipi_dsih_hal_get_error_mask_0(dsih_ctrl_t * instance, uint32_t mask);                                                         
uint32_t mipi_dsih_hal_get_error_mask_1(dsih_ctrl_t * instance, uint32_t mask);                                                         
                                                                                                                                        
void mipi_dsih_hal_dbi_out_color_coding(dsih_ctrl_t * instance, uint8_t color_depth, uint8_t option);                                   
void mipi_dsih_hal_dbi_in_color_coding(dsih_ctrl_t * instance, uint8_t color_depth, uint8_t option);                                    
void mipi_dsih_hal_dbi_lut_size(dsih_ctrl_t * instance, uint8_t size);                                                                  
void mipi_dsih_hal_dbi_partitioning_en(dsih_ctrl_t * instance, int enable);                                                             
void mipi_dsih_hal_dbi_dcs_vc(dsih_ctrl_t * instance, uint8_t vc);                                                                      
                                                                                                                                        
void mipi_dsih_hal_dbi_cmd_size(dsih_ctrl_t * instance, uint16_t size);                                                                 
void mipi_dsih_hal_dbi_max_cmd_size(dsih_ctrl_t * instance, uint16_t size);                                                             
int mipi_dsih_hal_dbi_rd_cmd_busy(dsih_ctrl_t * instance);                                                                              
int mipi_dsih_hal_dbi_read_fifo_full(dsih_ctrl_t * instance);                                                                           
int mipi_dsih_hal_dbi_read_fifo_empty(dsih_ctrl_t * instance);                                                                          
int mipi_dsih_hal_dbi_write_fifo_full(dsih_ctrl_t * instance);                                                                          
int mipi_dsih_hal_dbi_write_fifo_empty(dsih_ctrl_t * instance);                                                                         
int mipi_dsih_hal_dbi_cmd_fifo_full(dsih_ctrl_t * instance);                                                                            
int mipi_dsih_hal_dbi_cmd_fifo_empty(dsih_ctrl_t * instance);                                                                           
                                                                                                                                        
void mipi_dsih_hal_gen_rd_vc(dsih_ctrl_t * instance, uint8_t vc);                                                                       
void mipi_dsih_hal_gen_eotp_rx_en(dsih_ctrl_t * instance, int enable);                                                                  
void mipi_dsih_hal_gen_eotp_tx_en(dsih_ctrl_t * instance, int enable);                                                                  
void mipi_dsih_hal_bta_en(dsih_ctrl_t * instance, int enable);                                                                          
void mipi_dsih_hal_gen_ecc_rx_en(dsih_ctrl_t * instance, int enable);                                                                   
void mipi_dsih_hal_gen_crc_rx_en(dsih_ctrl_t * instance, int enable);                                                                   
int mipi_dsih_hal_gen_rd_cmd_busy(dsih_ctrl_t * instance);                                                                              
int mipi_dsih_hal_gen_read_fifo_full(dsih_ctrl_t * instance);                                                                           
int mipi_dsih_hal_gen_read_fifo_empty(dsih_ctrl_t * instance);                                                                          
int mipi_dsih_hal_gen_write_fifo_full(dsih_ctrl_t * instance);                                                                          
int mipi_dsih_hal_gen_write_fifo_empty(dsih_ctrl_t * instance);                                                                         
int mipi_dsih_hal_gen_cmd_fifo_full(dsih_ctrl_t * instance);                                                                            
int mipi_dsih_hal_gen_cmd_fifo_empty(dsih_ctrl_t * instance);                                                                           
                                                                                                                                        
/* only if DPI */                                                                                                                       
dsih_error_t mipi_dsih_phy_hs2lp_config(dsih_ctrl_t * instance, uint8_t no_of_byte_cycles);                                             
dsih_error_t mipi_dsih_phy_lp2hs_config(dsih_ctrl_t * instance, uint8_t no_of_byte_cycles);                                             
dsih_error_t mipi_dsih_phy_bta_time(dsih_ctrl_t * instance, uint16_t no_of_byte_cycles);                                                
/* */                                                                                                                                   
                                                                                                                                        
void mipi_dsih_write_word(dsih_ctrl_t * instance, uint32_t reg_address, uint32_t data);                                                 
void mipi_dsih_write_part(dsih_ctrl_t * instance, uint32_t reg_address, uint32_t data, uint8_t shift, uint8_t width);                   
uint32_t mipi_dsih_read_word(dsih_ctrl_t * instance, uint32_t reg_address);                                                             
uint32_t mipi_dsih_read_part(dsih_ctrl_t * instance, uint32_t reg_address, uint8_t shift, uint8_t width);                               
                                                                                                                                        
#endif /* MIPI_DSI_API_H_ */                                                                                                            
                                                                                                                                              
