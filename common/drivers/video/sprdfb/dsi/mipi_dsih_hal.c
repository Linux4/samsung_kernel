                                                                                                                                       
#include "mipi_dsih_hal.h"                                                                                                             
                                                                                                                                       
void mipi_dsih_write_word(dsih_ctrl_t * instance, uint32_t reg_address, uint32_t data)                                                 
{                                                                                                                                      
                                                                                                                                       
    instance->core_write_function(instance->address, reg_address, data);                                                               
}                                                                                                                                      
void mipi_dsih_write_part(dsih_ctrl_t * instance, uint32_t reg_address, uint32_t data, uint8_t shift, uint8_t width)                   
{                                                                                                                                      
    uint32_t mask = (1 << width) - 1;                                                                                                  
    uint32_t temp = mipi_dsih_read_word(instance, reg_address);                                                                        
                                                                                                                                       
    temp &= ~(mask << shift);                                                                                                          
    temp |= (data & mask) << shift;                                                                                                    
    mipi_dsih_write_word(instance, reg_address, temp);                                                                                 
}                                                                                                                                      
uint32_t mipi_dsih_read_word(dsih_ctrl_t * instance, uint32_t reg_address)                                                             
{                                                                                                                                      
    return instance->core_read_function(instance->address, reg_address);                                                               
}                                                                                                                                      
uint32_t mipi_dsih_read_part(dsih_ctrl_t * instance, uint32_t reg_address, uint8_t shift, uint8_t width)                               
{                                                                                                                                      
    return (mipi_dsih_read_word(instance, reg_address) >> shift) & ((1 << width) - 1);                                                 
}                                                                                                                                      
uint32_t mipi_dsih_hal_get_version(dsih_ctrl_t * instance)                                                                             
{                                                                                                                                      
    return mipi_dsih_read_word(instance, R_DSI_HOST_VERSION);                                                                          
}                                                                                                                                      
void mipi_dsih_hal_power(dsih_ctrl_t * instance, int on)                                                                               
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_PWR_UP, on, 0, 1);                                                                       
}                                                                                                                                      
int mipi_dsih_hal_get_power(dsih_ctrl_t * instance)                                                                                    
{                                                                                                                                      
    return (int)(mipi_dsih_read_word(instance, R_DSI_HOST_PWR_UP));                                                                    
}                                                                                                                                      
void mipi_dsih_hal_tx_escape_division(dsih_ctrl_t * instance, uint8_t tx_escape_division)                                              
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_CLK_MGR, tx_escape_division, 0, 8);                                                      
}                                                                                                                                      
void mipi_dsih_hal_dpi_video_vc(dsih_ctrl_t * instance, uint8_t vc)                                                                    
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_DPI_CFG, (uint32_t)(vc), 0, 2);                                                          
}                                                                                                                                      
uint8_t mipi_dsih_hal_dpi_get_video_vc(dsih_ctrl_t * instance)                                                                         
{                                                                                                                                      
    return mipi_dsih_read_part(instance, R_DSI_HOST_DPI_CFG, 0, 2);                                                                    
}                                                                                                                                      
dsih_error_t mipi_dsih_hal_dpi_color_coding(dsih_ctrl_t * instance, dsih_color_coding_t color_coding)                                  
{                                                                                                                                      
    dsih_error_t err = OK;                                                                                                             
    if (color_coding > 7)                                                                                                              
    {                                                                                                                                  
        if (instance->log_error != 0)                                                                                                  
        {                                                                                                                              
            instance->log_error("invalid colour configuration");                                                                       
        }                                                                                                                              
        err = ERR_DSI_COLOR_CODING;                                                                                                    
    }                                                                                                                                  
    else                                                                                                                               
    {                                                                                                                                  
        mipi_dsih_write_part(instance, R_DSI_HOST_DPI_CFG, color_coding, 2, 3);                                                        
    }                                                                                                                                  
    return err;                                                                                                                        
}                                                                                                                                      
dsih_color_coding_t mipi_dsih_hal_dpi_get_color_coding(dsih_ctrl_t * instance)                                                         
{                                                                                                                                      
    return (dsih_color_coding_t)(mipi_dsih_read_part(instance, R_DSI_HOST_DPI_CFG, 2, 3));                                             
}                                                                                                                                      
uint8_t mipi_dsih_hal_dpi_get_color_depth(dsih_ctrl_t * instance)                                                                      
{                                                                                                                                      
    uint8_t color_depth = 0;                                                                                                           
    switch (mipi_dsih_read_part(instance, R_DSI_HOST_DPI_CFG, 2, 3))                                                                   
    {                                                                                                                                  
        case 0:                                                                                                                        
        case 1:                                                                                                                        
        case 2:                                                                                                                        
            color_depth = 16;                                                                                                          
            break;                                                                                                                     
        case 3:                                                                                                                        
        case 4:                                                                                                                        
            color_depth = 18;                                                                                                          
            break;                                                                                                                     
        default:                                                                                                                       
            color_depth = 24;                                                                                                          
            break;                                                                                                                     
    }                                                                                                                                  
    return color_depth;                                                                                                                
}                                                                                                                                      
uint8_t mipi_dsih_hal_dpi_get_color_config(dsih_ctrl_t * instance)                                                                     
{                                                                                                                                      
    uint8_t color_config = 0;                                                                                                          
    switch (mipi_dsih_read_part(instance, R_DSI_HOST_DPI_CFG, 2, 3))                                                                   
    {                                                                                                                                  
        case 0:                                                                                                                        
            color_config = 1;   
            break;
        case 1:                                                                                                                        
            color_config = 2;                                                                                                          
            break;                                                                                                                     
        case 2:                                                                                                                        
            color_config = 3;                                                                                                          
            break;                                                                                                                     
        case 3:                                                                                                                        
            color_config = 1;                                                                                                          
            break;                                                                                                                     
        case 4:                                                                                                                        
            color_config = 2;                                                                                                          
            break;                                                                                                                     
        default:                                                                                                                       
            color_config = 0;                                                                                                          
            break;                                                                                                                     
    }                                                                                                                                  
    return color_config;                                                                                                               
}                                                                                                                                      
void mipi_dsih_hal_dpi_18_loosely_packet_en(dsih_ctrl_t * instance, int enable)                                                        
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_DPI_CFG, enable, 10, 1);                                                                 
}                                                                                                                                      
void mipi_dsih_hal_dpi_color_mode_pol(dsih_ctrl_t * instance, int active_low)                                                          
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_DPI_CFG, active_low, 9, 1);                                                              
}                                                                                                                                      
void mipi_dsih_hal_dpi_shut_down_pol(dsih_ctrl_t * instance, int active_low)                                                           
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_DPI_CFG, active_low, 8, 1);                                                              
}                                                                                                                                      
void mipi_dsih_hal_dpi_hsync_pol(dsih_ctrl_t * instance, int active_low)                                                               
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_DPI_CFG, active_low, 7, 1);                                                              
}                                                                                                                                      
void mipi_dsih_hal_dpi_vsync_pol(dsih_ctrl_t * instance, int active_low)                                                               
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_DPI_CFG, active_low, 6, 1);                                                              
}                                                                                                                                      
void mipi_dsih_hal_dpi_dataen_pol(dsih_ctrl_t * instance, int active_low)                                                              
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_DPI_CFG, active_low, 5, 1);                                                              
}                                                                                                                                      
void mipi_dsih_hal_dpi_frame_ack_en(dsih_ctrl_t * instance, int enable)                                                                
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_VID_MODE_CFG, enable, 11, 1);                                                            
}                                                                                                                                      
void mipi_dsih_hal_dpi_null_packet_en(dsih_ctrl_t * instance, int enable)                                                              
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_VID_MODE_CFG, enable, 10, 1);                                                            
}                                                                                                                                      
void mipi_dsih_hal_dpi_multi_packet_en(dsih_ctrl_t * instance, int enable)                                                             
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_VID_MODE_CFG, enable, 9, 1);                                                             
}                                                                                                                                      
void mipi_dsih_hal_dpi_lp_during_hfp(dsih_ctrl_t * instance, int enable)                                                               
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_VID_MODE_CFG, enable, 8, 1);                                                             
}                                                                                                                                      
void mipi_dsih_hal_dpi_lp_during_hbp(dsih_ctrl_t * instance, int enable)                                                               
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_VID_MODE_CFG, enable, 7, 1);                                                             
}                                                                                                                                      
void mipi_dsih_hal_dpi_lp_during_vactive(dsih_ctrl_t * instance, int enable)                                                           
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_VID_MODE_CFG, enable, 6, 1);                                                             
}                                                                                                                                      
void mipi_dsih_hal_dpi_lp_during_vfp(dsih_ctrl_t * instance, int enable)                                                               
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_VID_MODE_CFG, enable, 5, 1);                                                             
}                                                                                                                                      
void mipi_dsih_hal_dpi_lp_during_vbp(dsih_ctrl_t * instance, int enable)                                                               
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_VID_MODE_CFG, enable, 4, 1);                                                             
}                                                                                                                                      
void mipi_dsih_hal_dpi_lp_during_vsync(dsih_ctrl_t * instance, int enable)                                                             
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_VID_MODE_CFG, enable, 3, 1);                                                             
}                                                                                                                                      
dsih_error_t mipi_dsih_hal_dpi_video_mode_type(dsih_ctrl_t * instance, dsih_video_mode_t type)                                         
{                                                                                                                                      
    if (type < 3)                                                                                                                      
    {                                                                                                                                  
        mipi_dsih_write_part(instance, R_DSI_HOST_VID_MODE_CFG, type, 1, 2);                                                           
        return OK;                                                                                                                     
    }                                                                                                                                  
    else                                                                                                                               
    {                                                                                                                                  
        if (instance->log_error != 0)                                                                                                  
        {                                                                                                                              
            instance->log_error("undefined type");                                                                                     
        }                                                                                                                              
        return ERR_DSI_OUT_OF_BOUND;                                                                                                   
    }                                                                                                                                  
}                                                                                                                                      
void mipi_dsih_hal_dpi_video_mode_en(dsih_ctrl_t * instance, int enable)                                                               
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_VID_MODE_CFG, enable, 0, 1);                                                             
}                                                                                                                                      
int mipi_dsih_hal_dpi_is_video_mode(dsih_ctrl_t * instance)                                                                            
{                                                                                                                                      
    return mipi_dsih_read_part(instance, R_DSI_HOST_VID_MODE_CFG, 0, 1);                                                               
}                                                                                                                                      
dsih_error_t mipi_dsih_hal_dpi_null_packet_size(dsih_ctrl_t * instance, uint16_t size)                                                 
{                                                                                                                                      
    if (size < 0x3ff) /* 10-bit field */                                                                                               
    {                                                                                                                                  
        mipi_dsih_write_part(instance, R_DSI_HOST_VID_PKT_CFG, size, 21, 10);                                                          
        return OK;                                                                                                                     
    }                                                                                                                                  
    else                                                                                                                               
    {                                                                                                                                  
        return ERR_DSI_OUT_OF_BOUND;                                                                                                   
    }                                                                                                                                  
}                                                                                                                                      
dsih_error_t mipi_dsih_hal_dpi_chunks_no(dsih_ctrl_t * instance, uint16_t no)                                                          
{                                                                                                                                      
    if (no < 0x3ff)                                                                                                                    
    {                                                                                                                                  
        mipi_dsih_write_part(instance, R_DSI_HOST_VID_PKT_CFG, no, 11, 10);                                                            
        return OK;                                                                                                                     
    }                                                                                                                                  
    else                                                                                                                               
    {                                                                                                                                  
        return ERR_DSI_OUT_OF_BOUND;                                                                                                   
    }                                                                                                                                  
}                                                                                                                                      
dsih_error_t mipi_dsih_hal_dpi_video_packet_size(dsih_ctrl_t * instance, uint16_t size)                                                
{                                                                                                                                      
    if (size < 0x7ff) /* 11-bit field */                                                                                               
    {                                                                                                                                  
        mipi_dsih_write_part(instance, R_DSI_HOST_VID_PKT_CFG, size, 0, 11);                                                           
        return OK;                                                                                                                     
    }                                                                                                                                  
    else                                                                                                                               
    {                                                                                                                                  
        return ERR_DSI_OUT_OF_BOUND;                                                                                                   
    }                                                                                                                                  
}                                                                                                                                      
void mipi_dsih_hal_tear_effect_ack_en(dsih_ctrl_t * instance, int enable)                                                              
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_CMD_MODE_CFG, enable, 14, 1);                                                            
}                                                                                                                                      
void mipi_dsih_hal_cmd_ack_en(dsih_ctrl_t * instance, int enable)                                                                      
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_CMD_MODE_CFG, enable, 13, 1);                                                            
}                                                                                                                                      
dsih_error_t mipi_dsih_hal_dcs_wr_tx_type(dsih_ctrl_t * instance, unsigned no_of_param, int lp)                                        
{                                                                                                                                      
    switch (no_of_param)                                                                                                               
    {                                                                                                                                  
        case 0:                                                                                                                        
            mipi_dsih_write_part(instance, R_DSI_HOST_CMD_MODE_CFG, lp, 7, 1);                                                         
            break;                                                                                                                     
        case 1:                                                                                                                        
            mipi_dsih_write_part(instance, R_DSI_HOST_CMD_MODE_CFG, lp, 8, 1);                                                         
            break;                                                                                                                     
        default:                                                                                                                       
            mipi_dsih_write_part(instance, R_DSI_HOST_CMD_MODE_CFG, lp, 12, 1);                                                        
            break;                                                                                                                     
    }                                                                                                                                  
    return OK;                                                                                                                         
}                                                                                                                                      
dsih_error_t mipi_dsih_hal_dcs_rd_tx_type(dsih_ctrl_t * instance, unsigned no_of_param, int lp)                                        
{                                                                                                                                      
    dsih_error_t err = OK;                                                                                                             
    switch (no_of_param)                                                                                                               
    {                                                                                                                                  
        case 0:                                                                                                                        
            mipi_dsih_write_part(instance, R_DSI_HOST_CMD_MODE_CFG, lp, 9, 1);                                                         
            break;                                                                                                                     
        default:                                                                                                                       
            if (instance->log_error != 0)                                                                                              
            {                                                                                                                          
                instance->log_error("undefined DCS Read packet type");                                                                 
            }                                                                                                                          
            err = ERR_DSI_OUT_OF_BOUND;                                                                                                
            break;                                                                                                                     
    }                                                                                                                                  
    return err;                                                                                                                        
}

/*Jessica add begin: to support max read packet size command */
dsih_error_t mipi_dsih_hal_max_rd_packet_size_type(dsih_ctrl_t * instance, int lp)
{
    dsih_error_t err = OK;
    mipi_dsih_write_part(instance, R_DSI_HOST_CMD_MODE_CFG, lp, 10, 1);
    return err;
}
/*Jessica add end*/

dsih_error_t mipi_dsih_hal_gen_wr_tx_type(dsih_ctrl_t * instance, unsigned no_of_param, int lp)                                        
{                                                                                                                                      
    switch (no_of_param)                                                                                                               
    {                                                                                                                                  
        case 0:                                                                                                                        
            mipi_dsih_write_part(instance, R_DSI_HOST_CMD_MODE_CFG, lp, 1, 1);                                                         
            break;                                                                                                                     
        case 1:                                                                                                                        
            mipi_dsih_write_part(instance, R_DSI_HOST_CMD_MODE_CFG, lp, 2, 1);                                                         
            break;                                                                                                                     
        case 2:                                                                                                                        
            mipi_dsih_write_part(instance, R_DSI_HOST_CMD_MODE_CFG, lp, 3, 1);                                                         
            break;                                                                                                                     
        default:                                                                                                                       
            mipi_dsih_write_part(instance, R_DSI_HOST_CMD_MODE_CFG, lp, 11, 1);                                                        
            break;                                                                                                                     
    }                                                                                                                                  
    return OK;                                                                                                                         
}                                                                                                                                      
dsih_error_t mipi_dsih_hal_gen_rd_tx_type(dsih_ctrl_t * instance, unsigned no_of_param, int lp)                                        
{                                                                                                                                      
    dsih_error_t err = OK;                                                                                                             
    switch (no_of_param)                                                                                                               
    {                                                                                                                                  
        case 0:                                                                                                                        
            mipi_dsih_write_part(instance, R_DSI_HOST_CMD_MODE_CFG, lp, 4, 1);                                                         
            break;                                                                                                                     
        case 1:                                                                                                                        
            mipi_dsih_write_part(instance, R_DSI_HOST_CMD_MODE_CFG, lp, 5, 1);                                                         
            break;                                                                                                                     
        case 2:                                                                                                                        
            mipi_dsih_write_part(instance, R_DSI_HOST_CMD_MODE_CFG, lp, 6, 1);                                                         
            break;                                                                                                                     
        default:                                                                                                                       
            if (instance->log_error != 0)                                                                                              
            {                                                                                                                          
                instance->log_error("undefined Generic Read packet type");                                                             
            }                                                                                                                          
            err = ERR_DSI_OUT_OF_BOUND;                                                                                                
            break;                                                                                                                     
    }                                                                                                                                  
    return err;                                                                                                                        
}                                                                                                                                      
void mipi_dsih_hal_max_rd_size_tx_type(dsih_ctrl_t * instance, int lp)                                                                 
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_CMD_MODE_CFG, lp, 10, 1);                                                                
}                                                                                                                                      
void mipi_dsih_hal_gen_cmd_mode_en(dsih_ctrl_t * instance, int enable)                                                                 
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_CMD_MODE_CFG, enable, 0, 1);                                                             
}                                                                                                                                      
int mipi_dsih_hal_gen_is_cmd_mode(dsih_ctrl_t * instance)                                                                              
{                                                                                                                                      
    return mipi_dsih_read_part(instance, R_DSI_HOST_CMD_MODE_CFG, 0, 1);                                                               
}                                                                                                                                      
void mipi_dsih_hal_dpi_hline(dsih_ctrl_t * instance, uint16_t time)                                                                    
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_TMR_LINE_CFG, time, 18, 14);                                                             
}                                                                                                                                      
void mipi_dsih_hal_dpi_hbp(dsih_ctrl_t * instance, uint16_t time)                                                                      
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_TMR_LINE_CFG, time, 9, 9);                                                               
}                                                                                                                                      
void mipi_dsih_hal_dpi_hsa(dsih_ctrl_t * instance, uint16_t time)                                                                      
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_TMR_LINE_CFG, time, 0, 9);                                                               
}                                                                                                                                      
void mipi_dsih_hal_dpi_vactive(dsih_ctrl_t * instance, uint16_t lines)                                                                 
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_VTIMING_CFG, lines, 16, 11);                                                             
}                                                                                                                                      
void mipi_dsih_hal_dpi_vfp(dsih_ctrl_t * instance, uint16_t lines)                                                                     
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_VTIMING_CFG, lines, 10, 6);                                                              
}                                                                                                                                      
void mipi_dsih_hal_dpi_vbp(dsih_ctrl_t * instance, uint16_t lines)                                                                     
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_VTIMING_CFG, lines, 4, 6);                                                               
}                                                                                                                                      
void mipi_dsih_hal_dpi_vsync(dsih_ctrl_t * instance, uint16_t lines)                                                                   
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_VTIMING_CFG, lines, 0, 4);                                                               
}                                                                                                                                      
void mipi_dsih_hal_timeout_clock_division(dsih_ctrl_t * instance, uint8_t byte_clk_division_factor)                                    
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_CLK_MGR, byte_clk_division_factor, 8, 8);                                                
}                                                                                                                                      
void mipi_dsih_hal_lp_rx_timeout(dsih_ctrl_t * instance, uint16_t count)                                                               
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_TO_CNT_CFG, count, 16, 16);                                                              
}                                                                                                                                      
void mipi_dsih_hal_hs_tx_timeout(dsih_ctrl_t * instance, uint16_t count)                                                               
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_TO_CNT_CFG, count, 0, 16);                                                               
}                                                                                                                                      
uint32_t mipi_dsih_hal_error_status_0(dsih_ctrl_t * instance, uint32_t mask)                                                           
{                                                                                                                                      
    return (mipi_dsih_read_word(instance, R_DSI_HOST_ERROR_ST0) & mask);                                                               
}                                                                                                                                      
uint32_t mipi_dsih_hal_error_status_1(dsih_ctrl_t * instance, uint32_t mask)                                                           
{                                                                                                                                      
    return (mipi_dsih_read_word(instance, R_DSI_HOST_ERROR_ST1) & mask);                                                               
}                                                                                                                                      
void mipi_dsih_hal_error_mask_0(dsih_ctrl_t * instance, uint32_t mask)                                                                 
{                                                                                                                                      
    mipi_dsih_write_word(instance, R_DSI_HOST_ERROR_MSK0, mask);                                                                       
}                                                                                                                                      
uint32_t mipi_dsih_hal_get_error_mask_0(dsih_ctrl_t * instance, uint32_t mask)                                                         
{                                                                                                                                      
    return (mipi_dsih_read_word(instance, R_DSI_HOST_ERROR_MSK0) & mask);                                                              
}                                                                                                                                      
void mipi_dsih_hal_error_mask_1(dsih_ctrl_t * instance, uint32_t mask)                                                                 
{                                                                                                                                      
    mipi_dsih_write_word(instance, R_DSI_HOST_ERROR_MSK1, mask);                                                                       
}                                                                                                                                      
uint32_t mipi_dsih_hal_get_error_mask_1(dsih_ctrl_t * instance, uint32_t mask)                                                         
{                                                                                                                                      
    return (mipi_dsih_read_word(instance, R_DSI_HOST_ERROR_MSK1) & mask);                                                              
}                                                                                                                                      
/* DBI NOT IMPLEMENTED */                                                                                                              
void mipi_dsih_hal_dbi_out_color_coding(dsih_ctrl_t * instance, uint8_t color_depth, uint8_t option);                                  
void mipi_dsih_hal_dbi_in_color_coding(dsih_ctrl_t * instance, uint8_t color_depth, uint8_t option);                                   
void mipi_dsih_hal_dbi_lut_size(dsih_ctrl_t * instance, uint8_t size);                                                                 
void mipi_dsih_hal_dbi_partitioning_en(dsih_ctrl_t * instance, int enable);                                                            
void mipi_dsih_hal_dbi_dcs_vc(dsih_ctrl_t * instance, uint8_t vc);                                                                     
void mipi_dsih_hal_dbi_max_cmd_size(dsih_ctrl_t * instance, uint16_t size);                                                            
void mipi_dsih_hal_dbi_cmd_size(dsih_ctrl_t * instance, uint16_t size);                                                                
void mipi_dsih_hal_dbi_max_cmd_size(dsih_ctrl_t * instance, uint16_t size);                                                            
int mipi_dsih_hal_dbi_rd_cmd_busy(dsih_ctrl_t * instance);                                                                             
int mipi_dsih_hal_dbi_read_fifo_full(dsih_ctrl_t * instance);                                                                          
int mipi_dsih_hal_dbi_read_fifo_empty(dsih_ctrl_t * instance);                                                                         
int mipi_dsih_hal_dbi_write_fifo_full(dsih_ctrl_t * instance);                                                                         
int mipi_dsih_hal_dbi_write_fifo_empty(dsih_ctrl_t * instance);                                                                        
int mipi_dsih_hal_dbi_cmd_fifo_full(dsih_ctrl_t * instance);                                                                           
int mipi_dsih_hal_dbi_cmd_fifo_empty(dsih_ctrl_t * instance);                                                                          
                                                                                                                                       
dsih_error_t mipi_dsih_hal_gen_packet_header(dsih_ctrl_t * instance, uint8_t vc, uint8_t packet_type, uint8_t ms_byte, uint8_t ls_byte)
{                                                                                                                                      
    if (vc < 4)                                                                                                                        
    {                                                                                                                                  
        mipi_dsih_write_part(instance, R_DSI_HOST_GEN_HDR, (ms_byte <<  16) | (ls_byte << 8 ) | ((vc << 6) | packet_type), 0, 24);     
        return OK;                                                                                                                     
    }                                                                                                                                  
    return  ERR_DSI_OVERFLOW;                                                                                                          
}                                                                                                                                      
dsih_error_t mipi_dsih_hal_gen_packet_payload(dsih_ctrl_t * instance, uint32_t payload)                                                
{                                                                                                                                      
    if (mipi_dsih_hal_gen_write_fifo_full(instance))                                                                                   
    {                                                                                                                                  
        return ERR_DSI_OVERFLOW;                                                                                                       
    }                                                                                                                                  
    mipi_dsih_write_word(instance, R_DSI_HOST_GEN_PLD_DATA, payload);                                                                  
    return OK;                                                                                                                         
                                                                                                                                       
}                                                                                                                                      
dsih_error_t  mipi_dsih_hal_gen_read_payload(dsih_ctrl_t * instance, uint32_t* payload)                                                
{                                                                                                                                      
    *payload = mipi_dsih_read_word(instance, R_DSI_HOST_GEN_PLD_DATA);                                                                 
    return OK;                                                                                                                         
}                                                                                                                                      
                                                                                                                                       
void mipi_dsih_hal_gen_rd_vc(dsih_ctrl_t * instance, uint8_t vc)                                                                       
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_PCKHDL_CFG, vc, 5, 2);                                                                   
}                                                                                                                                      
void mipi_dsih_hal_gen_eotp_rx_en(dsih_ctrl_t * instance, int enable)                                                                  
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_PCKHDL_CFG, enable, 1, 1);                                                               
}                                                                                                                                      
void mipi_dsih_hal_gen_eotp_tx_en(dsih_ctrl_t * instance, int enable)                                                                  
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_PCKHDL_CFG, enable, 0, 1);                                                               
}                                                                                                                                      
void mipi_dsih_hal_bta_en(dsih_ctrl_t * instance, int enable)                                                                          
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_PCKHDL_CFG, enable, 2, 1);                                                               
}                                                                                                                                      
void mipi_dsih_hal_gen_ecc_rx_en(dsih_ctrl_t * instance, int enable)                                                                   
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_PCKHDL_CFG, enable, 3, 1);                                                               
}                                                                                                                                      
void mipi_dsih_hal_gen_crc_rx_en(dsih_ctrl_t * instance, int enable)                                                                   
{                                                                                                                                      
    mipi_dsih_write_part(instance, R_DSI_HOST_PCKHDL_CFG, enable, 4, 1);                                                               
}                                                                                                                                      
int mipi_dsih_hal_gen_rd_cmd_busy(dsih_ctrl_t * instance)                                                                              
{                                                                                                                                      
    return mipi_dsih_read_part(instance, R_DSI_HOST_CMD_PKT_STATUS, 6, 1);                                                             
}                                                                                                                                      
int mipi_dsih_hal_gen_read_fifo_full(dsih_ctrl_t * instance)                                                                           
{                                                                                                                                      
    return mipi_dsih_read_part(instance, R_DSI_HOST_CMD_PKT_STATUS, 5, 1);                                                             
}                                                                                                                                      
int mipi_dsih_hal_gen_read_fifo_empty(dsih_ctrl_t * instance)                                                                          
{                                                                                                                                      
    return mipi_dsih_read_part(instance, R_DSI_HOST_CMD_PKT_STATUS, 4, 1);                                                             
}                                                                                                                                      
int mipi_dsih_hal_gen_write_fifo_full(dsih_ctrl_t * instance)                                                                          
{                                                                                                                                      
    return mipi_dsih_read_part(instance, R_DSI_HOST_CMD_PKT_STATUS, 3, 1);                                                             
}                                                                                                                                      
int mipi_dsih_hal_gen_write_fifo_empty(dsih_ctrl_t * instance)                                                                         
{                                                                                                                                      
    return mipi_dsih_read_part(instance, R_DSI_HOST_CMD_PKT_STATUS, 2, 1);                                                             
}                                                                                                                                      
int mipi_dsih_hal_gen_cmd_fifo_full(dsih_ctrl_t * instance)                                                                            
{                                                                                                                                      
    return mipi_dsih_read_part(instance, R_DSI_HOST_CMD_PKT_STATUS, 1, 1);                                                             
}                                                                                                                                      
int mipi_dsih_hal_gen_cmd_fifo_empty(dsih_ctrl_t * instance)                                                                           
{                                                                                                                                      
    return mipi_dsih_read_part(instance, R_DSI_HOST_CMD_PKT_STATUS, 0, 1);                                                             
}                                                                                                                                      
/* only if DPI */                                                                                                                      
dsih_error_t mipi_dsih_phy_hs2lp_config(dsih_ctrl_t * instance, uint8_t no_of_byte_cycles)                                             
{                                                                                                                                      
    //mipi_dsih_write_part(instance, R_DSI_HOST_PHY_TMR_CFG, no_of_byte_cycles, 20, 8);  
    mipi_dsih_write_part(instance, R_DSI_HOST_PHY_TMR_CFG, no_of_byte_cycles, 24, 8);
    return OK;                                                                                                                         
}                                                                                                                                      
dsih_error_t mipi_dsih_phy_lp2hs_config(dsih_ctrl_t * instance, uint8_t no_of_byte_cycles)                                             
{                                                                                                                                      
    //mipi_dsih_write_part(instance, R_DSI_HOST_PHY_TMR_CFG, no_of_byte_cycles, 12, 8);   
    mipi_dsih_write_part(instance, R_DSI_HOST_PHY_TMR_CFG, no_of_byte_cycles, 16, 8); 
    return OK;                                                                                                                         
}                                                                                                                                      
dsih_error_t mipi_dsih_phy_bta_time(dsih_ctrl_t * instance, uint16_t no_of_byte_cycles)                                                
{ 
	/*Jessica modified: From ASIC, the second table in spec is correct, this 15 bits are max rd time*/
    if (no_of_byte_cycles < 0x8000) /* 15-bit field */
    {
        //mipi_dsih_write_part(instance, R_DSI_HOST_PHY_TMR_CFG, no_of_byte_cycles, 0, 12);
	mipi_dsih_write_part(instance, R_DSI_HOST_PHY_TMR_CFG, no_of_byte_cycles, 0, 15);
    }                                                                                                                                  
    else                                                                                                                               
    {                                                                                                                                  
        return ERR_DSI_OVERFLOW;                                                                                                       
    }                                                                                                                                  
    return OK;                                                                                                                         
}                                                                                                                                      
/* */                                                                                                                                  