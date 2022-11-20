/**
 * @file mipi_dsih_api.c
 * @brief DWC MIPI DSI Host driver
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include "mipi_dsih_api.h"
#include "mipi_dsih_hal.h"
#include "mipi_dsih_dphy.h"
#include "../sprdfb.h"
#include <linux/delay.h>

/* whether to get debug messages (1) or not (0) */
#define DEBUG 					0

#define PRECISION_FACTOR 		1000
#define VIDEO_PACKET_OVERHEAD 	6
#define NULL_PACKET_OVERHEAD 	6
#define SHORT_PACKET			4
#define BLANKING_PACKET         6
/** Version supported by this driver */
static const uint32_t mipi_dsih_supported_versions[] = {0x3132302A, 0x3132312A};
static const uint32_t mipi_dsih_no_of_versions = sizeof(mipi_dsih_supported_versions) / sizeof(uint32_t);

/**
 * Open controller instance
 * - Check if driver is compatible with core version
 * - Check if instance context data structure is not NULL
 * - Bring up PHY for any transmissions in low power
 * 	+ Includes programming PLL to DEFAULT_BYTE_CLOCK
 * - Bring up controller and perform initial configuration
 * 	+ Includes start all commands transmision in LP
 * 	+ Controller will not turn the bus around after commands
 * 	+ program counters
 * @param instance pointer to structure holding the DSI Host core information
 * @return dsih_error_t
 * @note this function must be called before any other function in this API
 */
dsih_error_t mipi_dsih_open(dsih_ctrl_t * instance)
{
	dsih_error_t err = OK;
	uint32_t version = 0;
	int i = 0;

	if (instance == 0)
	{
		return ERR_DSI_INVALID_INSTANCE;
	}
	else if ((instance->core_read_function == 0) || (instance->core_write_function == 0))
	{
		return ERR_DSI_INVALID_IO;
	}
	else if (instance->status == INITIALIZED)
	{
		return ERR_DSI_INVALID_INSTANCE;
	}
	else if (mipi_dsih_dphy_open(&(instance->phy_instance)))
	{
		return ERR_DSI_PHY_INVALID;
	}
	else
	{
		instance->status = NOT_INITIALIZED;
		version = mipi_dsih_hal_get_version(instance);
		for (i = 0; i < mipi_dsih_no_of_versions; i++)
		{
			if (version == mipi_dsih_supported_versions[i])
			{
				break;
			}
		}
		/* no matching supported version has been found*/
		if (i >= mipi_dsih_no_of_versions)
		{
			if (instance->log_info != 0)
			{
				instance->log_info("sprdfb: driver does not support this core version 0x%lX", version);
			}
			return ERR_DSI_CORE_INCOMPATIBLE;
		}
	}
	mipi_dsih_hal_power(instance, 0);
    //mipi_dsih_hal_power(instance, 0);//Jessica
    //mipi_dsih_hal_power(instance, 1);//Jessica
	mipi_dsih_hal_dpi_color_mode_pol(instance, !instance->color_mode_polarity);
	mipi_dsih_hal_dpi_shut_down_pol(instance, !instance->shut_down_polarity);
    //err = mipi_dsih_phy_hs2lp_config(instance, instance->max_hs_to_lp_cycles);
    //err |=  mipi_dsih_phy_lp2hs_config(instance, instance->max_lp_to_hs_cycles);
	mipi_dsih_hal_int_mask_0(instance, 0xffffff);
	mipi_dsih_hal_int_mask_1(instance, 0xffffff);
	err = mipi_dsih_phy_bta_time(instance, instance->max_bta_cycles);
	if (err)
	{
		return ERR_DSI_OVERFLOW;
	}
	/* by default, return to LP during ALL, unless otherwise specified*/
	mipi_dsih_hal_dpi_lp_during_hfp(instance, 1);
	mipi_dsih_hal_dpi_lp_during_hbp(instance, 1);
	mipi_dsih_hal_dpi_lp_during_vactive(instance, 1);
	mipi_dsih_hal_dpi_lp_during_vfp(instance, 1);
	mipi_dsih_hal_dpi_lp_during_vbp(instance, 1);
	mipi_dsih_hal_dpi_lp_during_vsync(instance, 1);
	/* by default, all commands are sent in LP */
	mipi_dsih_hal_dcs_wr_tx_type(instance, 0, 1);
	mipi_dsih_hal_dcs_wr_tx_type(instance, 1, 1);
	mipi_dsih_hal_dcs_wr_tx_type(instance, 3, 1); /* long packet*/
	mipi_dsih_hal_dcs_rd_tx_type(instance, 0, 1);
    /*Jessica add to support max rd packet size command*/
    mipi_dsih_hal_max_rd_packet_size_type(instance, 1);
	mipi_dsih_hal_gen_wr_tx_type(instance, 0, 1);
	mipi_dsih_hal_gen_wr_tx_type(instance, 1, 1);
	mipi_dsih_hal_gen_wr_tx_type(instance, 2, 1);
	mipi_dsih_hal_gen_wr_tx_type(instance, 3, 1); /* long packet*/
	mipi_dsih_hal_gen_rd_tx_type(instance, 0, 1);
	mipi_dsih_hal_gen_rd_tx_type(instance, 1, 1);
	mipi_dsih_hal_gen_rd_tx_type(instance, 2, 1);
	/* by default, RX_VC = 0, NO EOTp, EOTn, BTA, ECC rx and CRC rx */
	mipi_dsih_hal_gen_rd_vc(instance, 0);
	mipi_dsih_hal_gen_eotp_rx_en(instance, 0);
	mipi_dsih_hal_gen_eotp_tx_en(instance, 0);
	mipi_dsih_hal_bta_en(instance, 0);
	mipi_dsih_hal_gen_ecc_rx_en(instance, 0);
	mipi_dsih_hal_gen_crc_rx_en(instance, 0);
    //mipi_dsih_hal_power(instance, 0);//Jessica
    mipi_dsih_hal_power(instance, 1);//Jessica

    /* dividing by 6 is aimed for max PHY frequency, 1GHz */
    mipi_dsih_hal_tx_escape_division(instance, 4); //6    //Jessica
    instance->status = INITIALIZED;
    /* initialize pll so escape clocks could be generated at 864MHz, 1 lane */
    /* however the high speed clock will not be requested */
    //err = mipi_dsih_dphy_configure(&(instance->phy_instance), 1, DEFAULT_BYTE_CLOCK);
    return err;
}
/**
 * Close DSI Host driver
 * - Free up resources and shutdown host controller and PHY
 * @param instance pointer to structure holding the DSI Host core information
 * @return dsih_error_t
 */
dsih_error_t mipi_dsih_close(dsih_ctrl_t * instance)
{
	if (instance == 0)
	{
		return ERR_DSI_INVALID_INSTANCE;
	}
	if (instance->status != INITIALIZED)
	{
		return ERR_DSI_INVALID_INSTANCE;
	}
	mipi_dsih_hal_int_mask_0(instance, 0xffffff);
	mipi_dsih_hal_int_mask_1(instance, 0xffffff);
	mipi_dsih_dphy_close(&(instance->phy_instance));
	mipi_dsih_hal_power(instance, 0);
	instance->status = NOT_INITIALIZED;
	return OK;
}
/**
 * Enable return to low power mode inside video periods when timing allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param hfp allow to return to lp inside horizontal front porch pixels
 * @param hbp allow to return to lp inside horizontal back porch pixels
 * @param vactive allow to return to lp inside vertical active lines
 * @param vfp allow to return to lp inside vertical front porch lines
 * @param vbp allow to return to lp inside vertical back porch lines
 * @param vsync allow to return to lp inside vertical sync lines
 */
void mipi_dsih_allow_return_to_lp(dsih_ctrl_t * instance, int hfp, int hbp, int vactive, int vfp, int vbp, int vsync)
{
    if(0 == instance)
    {
        return;
    }

    if (instance->status == INITIALIZED)
    {
        mipi_dsih_hal_dpi_lp_during_hfp(instance, hfp);
        mipi_dsih_hal_dpi_lp_during_hbp(instance, hbp);
        mipi_dsih_hal_dpi_lp_during_vactive(instance, vactive);
        mipi_dsih_hal_dpi_lp_during_vfp(instance, vfp);
        mipi_dsih_hal_dpi_lp_during_vbp(instance, vbp);
        mipi_dsih_hal_dpi_lp_during_vsync(instance, vsync);
        return;
    }
    if (instance->log_error != 0)
    {
        instance->log_error("sprdfb: [mipi_dsih_allow_return_to_lp] invalid instance");
    }

}
/**
 * Set DCS command packet transmission to low power
 * @param instance pointer to structure holding the DSI Host core information
 * @param long_write command packets
 * @param short_write command packets with none and one parameters
 * @param short_read command packets with none parameters
 */
void mipi_dsih_dcs_cmd_lp_transmission(dsih_ctrl_t * instance, int long_write, int short_write, int short_read)
{
    if(0 == instance)
    {
        return;
    }

    if (instance->status == INITIALIZED)
    {
        mipi_dsih_hal_dcs_wr_tx_type(instance, 0, short_write);
        mipi_dsih_hal_dcs_wr_tx_type(instance, 1, short_write);
        mipi_dsih_hal_dcs_wr_tx_type(instance, 3, long_write); /* long packet*/
        mipi_dsih_hal_dcs_rd_tx_type(instance, 0, short_read);
        return;
    }
    if (instance->log_error != 0)
    {
        instance->log_error("sprdfb: [mipi_dsih_dcs_cmd_lp_transmission] invalid instance");
    }

}
/**
 * Set Generic interface packet transmission to low power
 * @param instance pointer to structure holding the DSI Host core information
 * @param long_write command packets
 * @param short_write command packets with none, one and two parameters
 * @param short_read command packets with none, one and two parameters
 */
void mipi_dsih_gen_cmd_lp_transmission(dsih_ctrl_t * instance, int long_write, int short_write, int short_read)
{
    if(0 == instance)
    {
        return;
    }

    if (instance->status == INITIALIZED)
    {
        mipi_dsih_hal_gen_wr_tx_type(instance, 0, short_write);
        mipi_dsih_hal_gen_wr_tx_type(instance, 1, short_write);
        mipi_dsih_hal_gen_wr_tx_type(instance, 2, short_write);
        mipi_dsih_hal_gen_wr_tx_type(instance, 3, long_write); /* long packet*/
        mipi_dsih_hal_gen_rd_tx_type(instance, 0, short_read);
        mipi_dsih_hal_gen_rd_tx_type(instance, 1, short_read);
        mipi_dsih_hal_gen_rd_tx_type(instance, 2, short_read);
        return;
    }
    if (instance->log_error != 0)
    {
        instance->log_error("sprdfb: [mipi_dsih_gen_cmd_lp_transmission] invalid instance");
    }

}
/* packet handling */
/**
 *  Enable all receiving activities (applying a Bus Turn Around).
 *  - Disabling using this function will stop all acknowledges by the
 *  peripherals and no interrupts from low-level protocol error reporting
 *  will be able to rise.
 *  - Enabling any receiving function (command or frame ACKs, ECC,
 *  tear effect ACK or EoTp receiving) will enable this automatically,
 *  but it must be EXPLICITLY be disabled to disabled all kinds of
 *  receiving functionality.
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable or disable
 * @return error code
 */
dsih_error_t mipi_dsih_enable_rx(dsih_ctrl_t * instance, int enable)
{
	mipi_dsih_hal_bta_en(instance, enable);
	return OK;
}
/**
 *  Enable command packet acknowledges by the peripherals
 *  - For interrupts to rise the monitored event must be registered
 *  using the event_register function
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable or disable
 * @return error code
 */
dsih_error_t mipi_dsih_peripheral_ack(dsih_ctrl_t * instance, int enable)
{
	if (instance != 0)
	{
		if (instance->status == INITIALIZED)
		{
			mipi_dsih_hal_cmd_ack_en(instance, enable);
			if (enable)
			{
				mipi_dsih_hal_bta_en(instance, 1);
			}
			return OK;
		}
	}
	return ERR_DSI_INVALID_INSTANCE;
}
/**
 * Enable tearing effect acknowledges by the peripherals (wait for TE)
 * - It enables the following from the DSI specification
 * "Since the timing of a TE event is, by definition, unknown to the host
 * processor, the host processor shall give bus possession to the display
 * module and then wait for up to one video frame period for the TE response.
 * During this time, the host processor cannot send new commands, or requests
 * to the display module, because it does not have bus possession."
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable or disable
 * @return error code
 */
dsih_error_t mipi_dsih_tear_effect_ack(dsih_ctrl_t * instance, int enable)
{
	if (instance != 0)
	{
		if (instance->status == INITIALIZED)
		{
			mipi_dsih_hal_tear_effect_ack_en(instance, enable);
			if (enable)
			{
				mipi_dsih_hal_bta_en(instance, 1);
			}
			return OK;
		}
	}
	return ERR_DSI_INVALID_INSTANCE;
}
/**
 * Enable the receiving of EoT packets at the end of LS transmission.
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable or disable
 * @return error code
 */
dsih_error_t mipi_dsih_eotp_rx(dsih_ctrl_t * instance, int enable)
{
	if (instance != 0)
	{
		if (instance->status == INITIALIZED)
		{
			mipi_dsih_hal_gen_eotp_rx_en(instance, enable);
			if (enable)
			{
				mipi_dsih_hal_bta_en(instance, 1);
			}
			return OK;
		}
	}
	return ERR_DSI_INVALID_INSTANCE;
}
/**
 * Enable the listening to ECC bytes. This allows for recovering from
 * 1 bit errors. To report ECC events, the ECC events should be registered
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable or disable
 * @return error code
 */
dsih_error_t mipi_dsih_ecc_rx(dsih_ctrl_t * instance, int enable)
{
	if (instance != 0)
	{
		if (instance->status == INITIALIZED)
		{
			mipi_dsih_hal_gen_ecc_rx_en(instance, enable);
			if (enable)
			{
				mipi_dsih_hal_bta_en(instance, 1);
			}
			return OK;
		}
	}
	return ERR_DSI_INVALID_INSTANCE;
}
/**
 * Enable the sending of EoT (End of Transmission) packets at the end of HS
 * transmission. It was made optional in the DSI spec. for retro-compatibility.
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable or disable
 * @return error code
 */
dsih_error_t mipi_dsih_eotp_tx(dsih_ctrl_t * instance, int enable)
{
	if (instance != 0)
	{
		if (instance->status == INITIALIZED)
		{
			mipi_dsih_hal_gen_eotp_tx_en(instance, enable);
			return OK;
		}
	}
	return ERR_DSI_INVALID_INSTANCE;
}
/**
 * Configure DPI video interface
 * - If not in burst mode, it will compute the video and null packet sizes
 * according to necessity
 * - Configure timers for data lanes and/or clock lane to return to LP when
 * bandwidth is not filled by data
 * @param instance pointer to structure holding the DSI Host core information
 * @param video_params pointer to video stream-to-send information
 * @return error code
 */
dsih_error_t mipi_dsih_dpi_video(dsih_ctrl_t * instance, dsih_dpi_video_t * video_params)
{
	dsih_error_t err_code = OK;
	uint16_t bytes_per_pixel_x100 = 0; /* bpp x 100 because it can be 2.25 */
	uint16_t video_size = 0;
	uint32_t ratio_clock_xPF = 0; /* holds dpi clock/byte clock times precision factor */
	uint16_t null_packet_size = 0;
	uint8_t video_size_step = 1;
	uint32_t hs_timeout = 0;
	uint32_t total_bytes = 0;
	uint32_t bytes_per_chunk = 0;
	uint32_t no_of_chunks = 0;
	uint32_t bytes_left = 0;
	uint32_t chunk_overhead = 0;
	int counter = 0;
	/* check DSI controller instance */
	if ((instance == 0) || (video_params == 0))
	{
		return ERR_DSI_INVALID_INSTANCE;
	}
	if (instance->status != INITIALIZED)
	{
		return ERR_DSI_INVALID_INSTANCE;
	}
	if (video_params->no_of_lanes > instance->max_lanes)
	{
		return ERR_DSI_OUT_OF_BOUND;
	}
    /* set up phy pll to required lane clock */
	err_code = mipi_dsih_phy_hs2lp_config(instance, video_params->max_hs_to_lp_cycles);
	err_code |=	mipi_dsih_phy_lp2hs_config(instance, video_params->max_lp_to_hs_cycles);
	err_code = mipi_dsih_phy_clk_hs2lp_config(instance, video_params->max_clk_hs_to_lp_cycles);
	err_code |=	mipi_dsih_phy_clk_lp2hs_config(instance, video_params->max_clk_lp_to_hs_cycles);
	video_params->non_continuous_clock = 0; // 1.21a new feature, set 1 result in half-bottom snow screen , disbale it
	mipi_dsih_non_continuous_clock(instance,video_params->non_continuous_clock);
    //begin--frank
    //err_code = mipi_dsih_dphy_configure(&(instance->phy_instance), video_params->no_of_lanes, video_params->byte_clock * 8);
    //if (err_code)
    //{
    //    return err_code;
    //}
    //end--frank
	ratio_clock_xPF = (video_params->byte_clock * PRECISION_FACTOR) / (video_params->pixel_clock);
	video_size = video_params->h_active_pixels;
	/* set up ACKs and error reporting */
	mipi_dsih_hal_dpi_frame_ack_en(instance, video_params->receive_ack_packets);
	if (video_params->receive_ack_packets)
	{ /* if ACK is requested, enable BTA, otherwise leave as is */
		mipi_dsih_hal_bta_en(instance, 1);
	}
	/* mipi_dsih_hal_gen_cmd_mode_en(instance, 0); */
	mipi_dsih_hal_dpi_video_mode_en(instance, 1);
	/* get bytes per pixel and video size step (depending if loosely or not */
	switch (video_params->color_coding)
	{
	case COLOR_CODE_16BIT_CONFIG1:
	case COLOR_CODE_16BIT_CONFIG2:
	case COLOR_CODE_16BIT_CONFIG3:
		bytes_per_pixel_x100 = 200;
		video_size_step = 1;
		break;
	case COLOR_CODE_18BIT_CONFIG1:
	case COLOR_CODE_18BIT_CONFIG2:
		mipi_dsih_hal_dpi_18_loosely_packet_en(instance, video_params->is_18_loosely);
		bytes_per_pixel_x100 = 225;
		if (!video_params->is_18_loosely)
		{ /* 18bits per pixel and NOT loosely, packets should be multiples of 4 */
			video_size_step = 4;
			/* round up active H pixels to a multiple of 4 */
			for (; (video_size % 4) != 0; video_size++)
			{
				;
			}
		}
		else
		{
			video_size_step = 1;
		}
		break;
	case COLOR_CODE_24BIT:
		bytes_per_pixel_x100 = 300;
		video_size_step = 1;
		break;
	case COLOR_CODE_20BIT_YCC422_LOOSELY:
		bytes_per_pixel_x100 = 250;
		video_size_step = 2;
		/* round up active H pixels to a multiple of 2 */
		if ((video_size % 2) != 0)
		{
			video_size += 1;
		}
		break;
	case COLOR_CODE_24BIT_YCC422:
		bytes_per_pixel_x100 = 300;
		video_size_step = 2;
		/* round up active H pixels to a multiple of 2 */
		if ((video_size % 2) != 0)
		{
			video_size += 1;
		}
		break;
	case COLOR_CODE_16BIT_YCC422:
		bytes_per_pixel_x100 = 200;
		video_size_step = 2;
		/* round up active H pixels to a multiple of 2 */
		if ((video_size % 2) != 0)
		{
			video_size += 1;
		}
		break;
	case COLOR_CODE_30BIT:
		bytes_per_pixel_x100 = 375;
		video_size_step = 2;
		break;
	case COLOR_CODE_36BIT:
		bytes_per_pixel_x100 = 450;
		video_size_step = 2;
		break;
	case COLOR_CODE_12BIT_YCC420:
		bytes_per_pixel_x100 = 150;
		video_size_step = 2;
		/* round up active H pixels to a multiple of 2 */
		if ((video_size % 2) != 0)
		{
			video_size += 1;
		}
		break;
	default:
		if (instance->log_error != 0)
		{
			instance->log_error("sprdfb: invalid color coding");
		}
		err_code = ERR_DSI_COLOR_CODING;
		break;
	}
	if (err_code == OK)
	{
		err_code = mipi_dsih_hal_dpi_color_coding(instance, video_params->color_coding);
	}
	if (err_code != OK)
	{
		return err_code;
	}
	mipi_dsih_hal_dpi_video_mode_type(instance, video_params->video_mode);
	mipi_dsih_hal_dpi_hline(instance, (uint16_t)((video_params->h_total_pixels * ratio_clock_xPF) / PRECISION_FACTOR));
	mipi_dsih_hal_dpi_hbp(instance, ((video_params->h_back_porch_pixels * ratio_clock_xPF) / PRECISION_FACTOR));
	mipi_dsih_hal_dpi_hsa(instance, ((video_params->h_sync_pixels * ratio_clock_xPF) / PRECISION_FACTOR));
	mipi_dsih_hal_dpi_vactive(instance, video_params->v_active_lines);
	mipi_dsih_hal_dpi_vfp(instance, video_params->v_total_lines - (video_params->v_back_porch_lines + video_params->v_sync_lines + video_params->v_active_lines));
	mipi_dsih_hal_dpi_vbp(instance, video_params->v_back_porch_lines);
	mipi_dsih_hal_dpi_vsync(instance, video_params->v_sync_lines);
	mipi_dsih_hal_dpi_hsync_pol(instance, !video_params->h_polarity);
	mipi_dsih_hal_dpi_vsync_pol(instance, !video_params->v_polarity);
	mipi_dsih_hal_dpi_dataen_pol(instance, !video_params->data_en_polarity);
	/* HS timeout */
	hs_timeout = ((video_params->h_total_pixels * video_params->v_active_lines) + (DSIH_PIXEL_TOLERANCE * bytes_per_pixel_x100) / 100);
	for (counter = 0x80; (counter < hs_timeout) && (counter > 2); counter--)
	{
		if ((hs_timeout % counter) == 0)
		{
			mipi_dsih_hal_timeout_clock_division(instance, counter);
			mipi_dsih_hal_lp_rx_timeout(instance, (uint16_t)(hs_timeout / counter));
			mipi_dsih_hal_hs_tx_timeout(instance, (uint16_t)(hs_timeout / counter));
			break;
		}
	}
    /* TX_ESC_CLOCK_DIV must be less than 20000KHz */
    mipi_dsih_hal_tx_escape_division(instance, 4); //6 //Jessia
	/* video packetisation */
	if (video_params->video_mode == VIDEO_BURST_WITH_SYNC_PULSES)
	{ /* BURST */
		mipi_dsih_hal_dpi_null_packet_en(instance, 0);
		mipi_dsih_hal_dpi_multi_packet_en(instance, 0);
		err_code = mipi_dsih_hal_dpi_null_packet_size(instance, 0);
		err_code = err_code? err_code: mipi_dsih_hal_dpi_chunks_no(instance, 1);
		err_code = err_code? err_code: mipi_dsih_hal_dpi_video_packet_size(instance, video_size);
		if (err_code != OK)
		{
			return err_code;
		}
		/* BURST by default, returns to LP during ALL empty periods - energy saving */
		mipi_dsih_hal_dpi_lp_during_hfp(instance, 1);
#if defined(CONFIG_FB_SCX35) && defined(CONFIG_FB_LCD_SSD2075_MIPI)
		mipi_dsih_hal_dpi_lp_during_hbp(instance, 0);//forbit return to lp during hbp
#else
        mipi_dsih_hal_dpi_lp_during_hbp(instance, 1);
#endif
		mipi_dsih_hal_dpi_lp_during_vactive(instance, 1);
		mipi_dsih_hal_dpi_lp_during_vfp(instance, 1);
		mipi_dsih_hal_dpi_lp_during_vbp(instance, 1);
		mipi_dsih_hal_dpi_lp_during_vsync(instance, 1);
#if DEBUG
		/*      D E B U G 		*/
		if (instance->log_info != 0)
		{
			instance->log_info("sprdfb: burst video");
			instance->log_info("sprdfb: h line time %ld", (uint16_t)((video_params->h_total_pixels * ratio_clock_xPF) / PRECISION_FACTOR));
			instance->log_info("sprdfb: video_size %ld", video_size);
		}
#endif
	}
	else
	{	/* non burst transmission */
		null_packet_size = 0;
		/* bytes to be sent - first as one chunk */
		bytes_per_chunk = (bytes_per_pixel_x100 * video_params->h_active_pixels) / 100 + VIDEO_PACKET_OVERHEAD;
		/* bytes being received through the DPI interface per byte clock cycle */
		total_bytes = (ratio_clock_xPF * video_params->no_of_lanes * (video_params->h_total_pixels - video_params->h_back_porch_pixels - video_params->h_sync_pixels)) / PRECISION_FACTOR;
		/* check if the in pixels actually fit on the DSI link */
		if (total_bytes >= bytes_per_chunk)
		{
			chunk_overhead = total_bytes - bytes_per_chunk;
			/* overhead higher than 1 -> enable multi packets */
			if (chunk_overhead > 1)
			{	/* MULTI packets */
				for (video_size = video_size_step; video_size < video_params->h_active_pixels; video_size += video_size_step)
				{	/* determine no of chunks */
					if ((((video_params->h_active_pixels * PRECISION_FACTOR) / video_size) % PRECISION_FACTOR) == 0)
					{
						no_of_chunks = video_params->h_active_pixels / video_size;
						bytes_per_chunk = (bytes_per_pixel_x100 * video_size) / 100 + VIDEO_PACKET_OVERHEAD;
						if (total_bytes >= (bytes_per_chunk * no_of_chunks))
						{
							bytes_left = total_bytes - (bytes_per_chunk * no_of_chunks);
							break;
						}
					}
				}
				/* prevent overflow (unsigned - unsigned) */
				if (bytes_left > (NULL_PACKET_OVERHEAD * no_of_chunks))
				{
					null_packet_size = (bytes_left - (NULL_PACKET_OVERHEAD * no_of_chunks)) / no_of_chunks;
					if (null_packet_size > MAX_NULL_SIZE)
					{	/* avoid register overflow */
						null_packet_size = MAX_NULL_SIZE;
					}
				}
			}
			else
			{	/* no multi packets */
				no_of_chunks = 1;
#if DEBUG
				/*      D E B U G 		*/
				if (instance->log_info != 0)
				{
					instance->log_info("sprdfb: no multi no null video");
					instance->log_info("sprdfb: h line time %ld", (uint16_t)((video_params->h_total_pixels * ratio_clock_xPF) / PRECISION_FACTOR));
					instance->log_info("sprdfb: video_size %ld", video_size);
				}
				/************************/
#endif
				/* video size must be a multiple of 4 when not 18 loosely */
				for (video_size = video_params->h_active_pixels; (video_size % video_size_step) != 0; video_size++)
				{
					;
				}
			}
		}
		else
		{
			instance->log_error("sprdfb: resolution cannot be sent to display through current settings");
			err_code = ERR_DSI_OVERFLOW;
		}
	}
	err_code = err_code? err_code: mipi_dsih_hal_dpi_chunks_no(instance, no_of_chunks);
	err_code = err_code? err_code: mipi_dsih_hal_dpi_video_packet_size(instance, video_size);
	err_code = err_code? err_code: mipi_dsih_hal_dpi_null_packet_size(instance, null_packet_size);
	mipi_dsih_hal_dpi_null_packet_en(instance, null_packet_size > 0? 1: 0);
	mipi_dsih_hal_dpi_multi_packet_en(instance, (no_of_chunks > 1)? 1: 0);
#if DEBUG
	/*      D E B U G 		*/
	if (instance->log_info != 0)
	{
		instance->log_info("sprdfb: total_bytes %d", total_bytes);
		instance->log_info("sprdfb: bytes_per_chunk %d", bytes_per_chunk);
		instance->log_info("sprdfb: bytes left %d", bytes_left);
		instance->log_info("sprdfb: null packets %d", null_packet_size);
		instance->log_info("sprdfb: chunks %ld", no_of_chunks);
		instance->log_info("sprdfb: video_size %ld", video_size);
	}
	/************************/
#endif
	mipi_dsih_hal_dpi_video_vc(instance, video_params->virtual_channel);
	mipi_dsih_dphy_no_of_lanes(&(instance->phy_instance), video_params->no_of_lanes);
	/* enable high speed clock */
	//mipi_dsih_dphy_enable_hs_clk(&(instance->phy_instance), 1);//jessica, panel not init yet, could not set to hs, or result in err to some panel
    return err_code;
}
/**
 * Send a DCS write command
 * It sends the User Command Set commands listed in the DCS specification and
 * not the Manufacturer Command Set. To send the Manufacturer Commands use the
 * packet on the generic packets sending function
 * function sets the packet data type automatically
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc destination virtual channel
 * @param params byte-addressed array of command parameters, including the
 * command itself
 * @param param_length length of the above array
 * @return error code
 * @note this function has an active delay to wait for the buffer to clear.
 * The delay is limited to DSIH_FIFO_ACTIVE_WAIT x register access time
 */
dsih_error_t mipi_dsih_dcs_wr_cmd(dsih_ctrl_t * instance, uint8_t vc, uint8_t* params, uint16_t param_length)
{
	uint8_t packet_type = 0;
//    int i = 0;
	if (params == 0)
	{
		return ERR_DSI_OUT_OF_BOUND;
	}
#if 0
	if (param_length > 2)
	{
		i = 2;
	}
	switch (params[i])
	{
	case 0x39:
	case 0x38:
	case 0x34:
	case 0x29:
	case 0x28:
	case 0x21:
	case 0x20:
	case 0x13:
	case 0x12:
	case 0x11:
	case 0x10:
	case 0x01:
	case 0x00:
		packet_type = 0x05; /* DCS short write no param */
		break;
	case 0x3A:
	case 0x36:
	case 0x35:
	case 0x26:
		packet_type = 0x15; /* DCS short write 1 param */
		break;
	case 0x44:
	case 0x3C:
	case 0x37:
	case 0x33:
	case 0x30:
	case 0x2D:
	case 0x2C:
	case 0x2B:
	case 0x2A:
		packet_type = 0x39; /* DCS long write/write_LUT command packet */
		break;
	default:
		if (instance->log_error != 0)
		{
			instance->log_error("invalid DCS command");
		}
		return ERR_DSI_INVALID_COMMAND;
	}
#else
	switch(param_length)
	{
		case 1:
			packet_type = 0x05; /* DCS short write no param */
			break;
		case 2:
			packet_type = 0x15; /* DCS short write 1 param */
			break;
		default:
			packet_type = 0x39; /* DCS long write/write_LUT command packet */
			break;
	}
#endif
    return mipi_dsih_gen_wr_packet(instance, vc, packet_type, params, param_length);
}
/**
 * Enable command mode
 * - This function shall be explicitly called before commands are send if they
 * are to be sent in command mode and not interlaced with video
 * - If video mode is ON, it will be turned off automatically
 * @param instance pointer to structure holding the DSI Host core information
 * @param en enable/disable
 */
void mipi_dsih_cmd_mode(dsih_ctrl_t * instance, int en)
{
    if(0 == instance)
    {
        return;
    }

    if (instance->status == INITIALIZED)
    {
		mipi_dsih_hal_gen_cmd_mode_en(instance, en);
                  return;
    }
    if (instance->log_error != 0)
    {
        instance->log_error("sprdfb: [mipi_dsih_cmd_mode] invalid instance");
    }

}
/**
 * Enable video mode
 * - If command mode is ON, it will be turned off automatically
 * @param instance pointer to structure holding the DSI Host core information
 * @param en enable/disable
 */
void mipi_dsih_video_mode(dsih_ctrl_t * instance, int en)
{
    if(0 == instance)
    {
        return;
    }

    if (instance->status == INITIALIZED)
    {
		mipi_dsih_hal_dpi_video_mode_en(instance, en);
                  return;
    }
    if (instance->log_error != 0)
    {
        instance->log_error("sprdfb: [mipi_dsih_video_mode] invalid instance");
    }

}

/**
 * Get the current active mode
 * - 1 command mode
 * - 2 DPI video mode
 */
int mipi_dsih_active_mode(dsih_ctrl_t * instance)
{
	if (mipi_dsih_hal_gen_is_cmd_mode(instance))
	{
		return 1;
	}
	else if (mipi_dsih_hal_dpi_is_video_mode(instance))
	{
		return 2;
	}
	return 0;
}
/**
 * Send a generic write command
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc destination virtual channel
 * @param params byte-addressed array of command parameters
 * @param param_length length of the above array
 * @return error code
 * @note this function has an active delay to wait for the buffer to clear.
 * The delay is limited to DSIH_FIFO_ACTIVE_WAIT x register access time
 */
dsih_error_t mipi_dsih_gen_wr_cmd(dsih_ctrl_t * instance, uint8_t vc, uint8_t* params, uint16_t param_length)
{
	uint8_t data_type = 0;
	switch(param_length)
	{
	case 0:
		data_type = 0x03;
		break;
	case 1:
		data_type = 0x13;
		break;
	case 2:
		data_type = 0x23;
		break;
	default:
		data_type = 0x29;
		break;
	}
	return mipi_dsih_gen_wr_packet(instance, vc, data_type, params, param_length);
}
/**
 * Send a packet on the generic interface
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc destination virtual channel
 * @param data_type type of command, inserted in first byte of header
 * @param params byte array of command parameters
 * @param param_length length of the above array
 * @return error code
 * @note this function has an active delay to wait for the buffer to clear.
 * The delay is limited to:
 * (param_length / 4) x DSIH_FIFO_ACTIVE_WAIT x register access time
 * @note the controller restricts the sending of .
 * This function will not be able to send Null and Blanking packets due to
 *  controller restriction
 */
dsih_error_t mipi_dsih_gen_wr_packet(dsih_ctrl_t * instance, uint8_t vc, uint8_t data_type, uint8_t* params, uint16_t param_length)
{
	dsih_error_t err_code = OK;
	/* active delay iterator */
	int timeout = 0;
	/* iterators */
	int i = 0;
	int j = 0;
	/* holds padding bytes needed */
	int compliment_counter = 0;
	uint8_t* payload = 0;
	/* temporary variable to arrange bytes into words */
	uint32_t temp = 0;
	uint16_t word_count = 0;
	if (instance == 0)
	{
		return ERR_DSI_INVALID_INSTANCE;
	}
	if (instance->status != INITIALIZED)
	{
		return ERR_DSI_INVALID_INSTANCE;
	}
	if ((params == 0) && (param_length != 0)) /* pointer NULL */
	{
		return ERR_DSI_OUT_OF_BOUND;
	}
	if (param_length > 2)
	{	/* long packet - write word count to header, and the rest to payload */
		payload = params + (2 * sizeof(params[0]));
		word_count = (params[1] << 8) | params[0];
		if ((param_length - 2) < word_count)
		{
			if (instance->log_error != 0)
			{
				instance->log_error("sprdfb: sent > input payload. complemented with zeroes");
			}
			compliment_counter = (param_length - 2) - word_count;
		}
		else if ((param_length - 2) > word_count)
		{
			if (instance->log_error != 0)
			{
				instance->log_error("sprdfb: Overflow - input > sent. payload truncated");
			}
		}
		for (i = 0; i < (param_length - 2); i += j)
		{
			temp = 0;
			for (j = 0; (j < 4) && ((j + i) < (param_length - 2)); j++)
			{	/* temp = (payload[i + 3] << 24) | (payload[i + 2] << 16) | (payload[i + 1] << 8) | payload[i]; */
				temp |= payload[i + j] << (j * 8);
			}
			/* check if payload Tx fifo is not full */
			for (timeout = 0; timeout < DSIH_FIFO_ACTIVE_WAIT; timeout++)
			{
				if (!mipi_dsih_hal_gen_packet_payload(instance, temp))
				{
					break;
				}
			}
			if (!(timeout < DSIH_FIFO_ACTIVE_WAIT))
			{
				return ERR_DSI_TIMEOUT;
			}
		}
		/* if word count entered by the user more than actual parameters received
		 * fill with zeroes - a fail safe mechanism, otherwise controller will
		 * want to send data from an empty buffer */
		for (i = 0; i < compliment_counter; i++)
		{
			/* check if payload Tx fifo is not full */
			for (timeout = 0; timeout < DSIH_FIFO_ACTIVE_WAIT; timeout++)
			{
				if (!mipi_dsih_hal_gen_packet_payload(instance, 0x00))
				{
					break;
				}
			}
			if (!(timeout < DSIH_FIFO_ACTIVE_WAIT))
			{
				return ERR_DSI_TIMEOUT;
			}
		}
	}
	for (timeout = 0; timeout < DSIH_FIFO_ACTIVE_WAIT; timeout++)
	{
		/* check if payload Tx fifo is not full */
		if (!mipi_dsih_hal_gen_cmd_fifo_full(instance))
		{
			if (param_length == 0)
			{
				err_code |= mipi_dsih_hal_gen_packet_header(instance, vc, data_type, 0x0, 0x0);
			}
			else if (param_length == 1)
			{
				err_code |= mipi_dsih_hal_gen_packet_header(instance, vc, data_type, 0x0, params[0]);
			}
			else
			{
				err_code |= mipi_dsih_hal_gen_packet_header(instance, vc, data_type, params[1], params[0]);
			}
			break;
		}
	}
	if (!(timeout < DSIH_FIFO_ACTIVE_WAIT))
	{
		err_code = ERR_DSI_TIMEOUT;
	}
	return err_code;
}
/**
 * Send a packet on the generic interface
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc destination virtual channel
 * @param data_type type of command, inserted in first byte of header
 * @param params byte array of command parameters
 * @param param_length length of the above array
 * @return error code
 * @note this function has an active delay to wait for the buffer to clear.
 * The delay is limited to:
 * (param_length / 4) x DSIH_FIFO_ACTIVE_WAIT x register access time
 * @note the controller restricts the sending of .
 * This function will not be able to send Null and Blanking packets due to
 *  controller restriction
 */
dsih_error_t mipi_dsih_wr_packet(dsih_ctrl_t * instance, uint8_t vc,
		uint8_t data_type, uint8_t *params, uint16_t count)
{
	dsih_error_t err_code = OK;
	int timeout, i;
	uint32_t send_data = 0;

	if (unlikely(!instance || instance->status != INITIALIZED))
		return ERR_DSI_INVALID_INSTANCE;

	if (unlikely(!params && count))
		return ERR_DSI_OUT_OF_BOUND;

	if (count > 2) {
		for (i = 0; i < count; i++) {
			send_data |= params[i] << ((i % 4) * 8);
			if (!((i + 1) % 4) || ((i + 1) == count)) {
				timeout = DSIH_FIFO_ACTIVE_WAIT;
				while ((mipi_dsih_hal_gen_packet_payload(instance,
								send_data) && timeout))
					timeout--;

				if (unlikely(!timeout)) {
					pr_warn("%s, dsi write payload timeout\n", __func__);
					return ERR_DSI_TIMEOUT;
				}
				send_data = 0;
			}
		}
	}

	timeout = DSIH_FIFO_ACTIVE_WAIT;
	while ((mipi_dsih_hal_gen_cmd_fifo_full(instance) && timeout))
			timeout--;
	if (unlikely(!timeout)) {
		pr_warn("%s, dsi cmd fifo full\n", __func__);
		return ERR_DSI_TIMEOUT;
	}

	switch (data_type) {
	case 0x03:
		err_code |= mipi_dsih_hal_gen_packet_header(
				instance, vc, data_type,
				0x0, 0x0);
		break;
	case 0x05:
	case 0x13:
		err_code |= mipi_dsih_hal_gen_packet_header(
				instance, vc, data_type,
				0x0, params[0]);
		break;
	case 0x15:
	case 0x23:
		err_code |= mipi_dsih_hal_gen_packet_header(
				instance, vc, data_type,
				params[1], params[0]);
		break;
	case 0x29:
	case 0x39:
		err_code |= mipi_dsih_hal_gen_packet_header(
				instance, vc, data_type,
				(count >> 8) & 0xFF, count & 0xFF);
		break;
	}

	return err_code;
}
/**
 * Send a DCS READ command to peripheral
 * function sets the packet data type automatically
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc destination virtual channel
 * @param command DCS code
 * @param bytes_to_read no of bytes to read (expected to arrive at buffer)
 * @param read_buffer pointer to 8-bit array to hold the read buffer words
 * return read_buffer_length
 * @note this function has an active delay to wait for the buffer to clear.
 * The delay is limited to 2 x DSIH_FIFO_ACTIVE_WAIT
 * (waiting for command buffer, and waiting for receiving)
 * @note this function will enable BTA
 */
uint16_t mipi_dsih_dcs_rd_cmd(dsih_ctrl_t * instance, uint8_t vc, uint8_t command, uint8_t bytes_to_read, uint8_t* read_buffer)
{
	if (instance == 0)
	{
		return 0;
	}
	if (instance->status != INITIALIZED)
	{
		return 0;
	}
	switch (command)
	{
	case 0xA8:
	case 0xA1:
	case 0x45:
	case 0x3E:
	case 0x2E:
	case 0x0F:
	case 0x0E:
	case 0x0D:
	case 0x0C:
	case 0x0B:
	case 0x0A:
	case 0x08:
	case 0x07:
	case 0x06:
		/* COMMAND_TYPE 0x06 - DCS Read no params refer to DSI spec p.47 */
		return mipi_dsih_gen_rd_packet(instance, vc, 0x06, 0x0, command, bytes_to_read, read_buffer);
	default:
		if (instance->log_error != 0)
		{
			instance->log_error("sprdfb: invalid DCS command");
		}
		return 0;
	}
	return 0;
}
/**
 * Send Generic READ command to peripheral
 * - function sets the packet data type automatically
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc destination virtual channel
 * @param params byte array of command parameters
 * @param param_length length of the above array
 * @param bytes_to_read no of bytes to read (expected to arrive at buffer)
 * @param read_buffer pointer to 8-bit array to hold the read buffer words
 * return read_buffer_length
 * @note this function has an active delay to wait for the buffer to clear.
 * The delay is limited to 2 x DSIH_FIFO_ACTIVE_WAIT
 * (waiting for command buffer, and waiting for receiving)
 * @note this function will enable BTA
 */
uint16_t mipi_dsih_gen_rd_cmd(dsih_ctrl_t * instance, uint8_t vc, uint8_t* params, uint16_t param_length, uint8_t bytes_to_read, uint8_t* read_buffer)
{
	uint8_t data_type = 0;
	if (instance == 0)
	{
		return 0;
	}
	if (instance->status != INITIALIZED)
	{
		return 0;
	}
	switch(param_length)
	{
	case 0:
		data_type = 0x04;
		return mipi_dsih_gen_rd_packet(instance, vc, data_type, 0x00, 0x00, bytes_to_read, read_buffer);
	case 1:
		data_type = 0x14;
		return mipi_dsih_gen_rd_packet(instance, vc, data_type, 0x00, params[0], bytes_to_read, read_buffer);
	case 2:
		data_type = 0x24;
		return mipi_dsih_gen_rd_packet(instance, vc, data_type, params[1], params[0], bytes_to_read, read_buffer);
	default:
		return 0;
	}
}
/**
 * Send READ packet to peripheral using the generic interface
 * This will force command mode and stop video mode (because of BTA)
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc destination virtual channel
 * @param data_type generic command type
 * @param lsb_byte first command parameter
 * @param msb_byte second command parameter
 * @param bytes_to_read no of bytes to read (expected to arrive at buffer)
 * @param read_buffer pointer to 8-bit array to hold the read buffer words
 * return read_buffer_length
 * @note this function has an active delay to wait for the buffer to clear.
 * The delay is limited to 2 x DSIH_FIFO_ACTIVE_WAIT
 * (waiting for command buffer, and waiting for receiving)
 * @note this function will enable BTA
 */
uint16_t mipi_dsih_gen_rd_packet(dsih_ctrl_t * instance, uint8_t vc, uint8_t data_type, uint8_t msb_byte, uint8_t lsb_byte, uint8_t bytes_to_read, uint8_t* read_buffer)
{
	dsih_error_t err_code = OK;
	int timeout = 0;
	int counter = 0;
	int i = 0;
	int last_count = 0;
	uint32_t temp[1] = {0};
	if (instance == 0)
	{
		return 0;
	}
	if (instance->status != INITIALIZED)
	{
		return 0;
	}
	if (bytes_to_read < 1)
	{
		return 0;
	}
	if (read_buffer == 0)
	{
		return 0;
	}
#ifndef FB_CHECK_ESD_IN_VFP
	/* make sure command mode is on */
	mipi_dsih_cmd_mode(instance, 1);
#endif
	/* make sure receiving is enabled */
	mipi_dsih_hal_bta_en(instance, 1);
	/* listen to the same virtual channel as the one sent to */
	mipi_dsih_hal_gen_rd_vc(instance, vc);
	for (timeout = 0; timeout < DSIH_FIFO_ACTIVE_WAIT; timeout++)
	{	/* check if payload Tx fifo is not full */
		if (!mipi_dsih_hal_gen_cmd_fifo_full(instance))
		{
			mipi_dsih_hal_gen_packet_header(instance, vc, data_type, msb_byte, lsb_byte);
			break;
		}
	}
	if (!(timeout < DSIH_FIFO_ACTIVE_WAIT))
	{
		if (instance->log_error != 0)
		{
			instance->log_error("sprdfb: tx rd command timed out");
		}
		return 0;
	}
	/* loop for the number of words to be read */
	for (timeout = 0; timeout < DSIH_FIFO_ACTIVE_WAIT; timeout++)
	{	/* check if command transaction is done */
		if (!mipi_dsih_hal_gen_rd_cmd_busy(instance))
		{
			if (!mipi_dsih_hal_gen_read_fifo_empty(instance))
			{
				for (counter = 0; (!mipi_dsih_hal_gen_read_fifo_empty(instance)); counter += 4)
				{
					err_code = mipi_dsih_hal_gen_read_payload(instance, temp);
					if (err_code)
					{
						return 0;
					}
					if (counter < bytes_to_read)
					{
						for (i = 0; i < 4; i++)
						{
							if ((counter + i) < bytes_to_read)
							{
								/* put 32 bit temp in 4 bytes of buffer passed by user*/
								read_buffer[counter + i] = (uint8_t)(temp[0] >> (i * 8));
								last_count = i + counter;
							}
							else
							{
								if ((uint8_t)(temp[0] >> (i * 8)) != 0x00)
								{
									last_count = i + counter;
								}
							}
						}
					}
					else
					{
						last_count = counter;
						for (i = 0; i < 4; i++)
						{
							if ((uint8_t)(temp[0] >> (i * 8)) != 0x00)
							{
								last_count = i + counter;
							}
						}
					}
				}
				return last_count + 1;
			}
#ifdef FB_CHECK_ESD_IN_VFP
                       else if(timeout == (DSIH_FIFO_ACTIVE_WAIT - 1))//else
#else
                       else
#endif
			{
				if (instance->log_error != 0)
				{
					instance->log_error("sprdfb: rx buffer empty");
				}
				return 0;
			}
		}
#ifdef FB_CHECK_ESD_IN_VFP
                udelay(5);
#endif
	}
	if (instance->log_error != 0)
	{
		instance->log_error("sprdfb: rx command timed out");
	}
	return 0;
}
/**
 * Dump values stored in the DSI host core registers
 * @param instance pointer to structure holding the DSI Host core information
 * @param all whether to dump all the registers, no register_config array need
 * be provided if dump is to standard IO
 * @param config array of register_config_t type where addresses and values are
 * stored
 * @param config_length the length of the config array
 * @return the number of the registers that were read
 */
uint32_t mipi_dsih_dump_register_configuration(dsih_ctrl_t * instance, int all, register_config_t *config, uint16_t config_length)
{
	uint32_t current0 = 0;
	uint16_t count = 0;
	if (instance == 0)
	{
		return ERR_DSI_INVALID_INSTANCE;
	}
	if (instance->status != INITIALIZED)
	{
		return ERR_DSI_INVALID_INSTANCE;
	}
	if (all)
	{ 	/* dump all registers */
		for (current0 = R_DSI_HOST_VERSION; current0 <= R_DSI_HOST_PHY_TMR_LPCLK_CFG; count++, current0 += (R_DSI_HOST_PWR_UP - R_DSI_HOST_VERSION))
		{
			if ((config_length == 0) || (config == 0) || count >= config_length)
			{ 	/* no place to write - write to STD IO */
				if (instance->log_info != 0)
				{
					instance->log_info("sprdfb: DSI 0x%lX:0x%lX", current0, mipi_dsih_read_word(instance, current0));
				}
			}
			else
			{
				config[count].addr = current0;
				config[count].data = mipi_dsih_read_word(instance, current0);
			}
		}
	}
	else
	{
		if(config == 0)
		{
			if (instance->log_error != 0)
			{
				instance->log_error("sprdfb: invalid buffer");
			}
		}
		else
		{
			for (count = 0; count < config_length; count++)
			{
				config[count].data = mipi_dsih_read_word(instance, config[count].addr);
			}
		}
	}
	return count;
}
/**
 * Write values to DSI host core registers
 * @param instance pointer to structure holding the DSI Host core information
 * @param config array of register_config_t type where register addresses and
 * their new values are stored
 * @param config_length the length of the config array
 * @return the number of the registers that were written to
 */
uint32_t mipi_dsih_write_register_configuration(dsih_ctrl_t * instance, register_config_t *config, uint16_t config_length)
{
	uint16_t count = 0;
	if (instance == 0)
	{
		return ERR_DSI_INVALID_INSTANCE;
	}
	if (instance->status != INITIALIZED)
	{
		return ERR_DSI_INVALID_INSTANCE;
	}
	for (count = 0; count < config_length; count++)
	{
		mipi_dsih_write_word(instance, config[count].addr, config[count].data);
	}
	return count;
}
/**
 * Register a handler for a specific event
 * - The handler will be called when this specific event occurs
 * @param instance pointer to structure holding the DSI Host core information
 * @param event enum
 * @param handler call back to handler function to handle the event
 * @return error code
 */
dsih_error_t mipi_dsih_register_event(dsih_ctrl_t * instance, dsih_event_t event, void (*handler)(dsih_ctrl_t *, void *))
{
	uint32_t mask = 1;
	uint32_t temp = 0;
	if (event >= DSI_MAX_EVENT)
	{
		return ERR_DSI_INVALID_EVENT;
	}
	if (handler == 0)
	{
		return ERR_DSI_INVALID_HANDLE;
	}
	if (instance == 0)
	{
		return ERR_DSI_INVALID_INSTANCE;
	}
	if (instance->status != INITIALIZED)
	{
		return ERR_DSI_INVALID_INSTANCE;
	}
	instance->event_registry[event] = handler;
	if (event < HS_CONTENTION)
	{
		temp = mipi_dsih_hal_int_get_mask_0(instance, 0xffffffff);
		temp &= ~(mask << event);
		temp |= (0 & mask) << event;
		mipi_dsih_hal_int_mask_0(instance, temp);
	}
	else
	{
		temp = mipi_dsih_hal_int_get_mask_1(instance, 0xffffffff);
		temp &= ~(mask << (event - HS_CONTENTION));
		temp |= (0 & mask) << (event - HS_CONTENTION);
		mipi_dsih_hal_int_mask_1(instance, temp);
		if (event == RX_CRC_ERR)
		{	/* automatically enable CRC reporting */
			mipi_dsih_hal_gen_crc_rx_en(instance, 1);
		}
	}
	return OK;
}
/**
 * Clear an already registered event
 * - Callback of the handler will be removed
 * @param instance pointer to structure holding the DSI Host core information
 * @param event enum
 * @return error code
 */
dsih_error_t mipi_dsih_unregister_event(dsih_ctrl_t * instance, dsih_event_t event)
{
	uint32_t mask = 1;
	uint32_t temp = 0;
    if (event >= DSI_MAX_EVENT)
	{
		return ERR_DSI_INVALID_EVENT;
	}
	if (instance == 0)
	{
		return ERR_DSI_INVALID_INSTANCE;
	}
	if (instance->status != INITIALIZED)
	{
		return ERR_DSI_INVALID_INSTANCE;
	}
	instance->event_registry[event] = 0;
	if (event < HS_CONTENTION)
	{
		temp = mipi_dsih_hal_int_get_mask_0(instance, 0xffffffff);
		temp &= ~(mask << event);
		temp |= (1 & mask) << event;
		mipi_dsih_hal_int_mask_0(instance, temp);
	}
	else
	{
		temp = mipi_dsih_hal_int_get_mask_1(instance, 0xffffffff);
		temp &= ~(mask << (event - HS_CONTENTION));
		temp |= (1 & mask) << (event - HS_CONTENTION);
		mipi_dsih_hal_int_mask_1(instance, temp);
		if (event == RX_CRC_ERR)
		{	/* automatically disable CRC reporting */
			mipi_dsih_hal_gen_crc_rx_en(instance, 0);
		}
	}
	return OK;
}
/**
 * Clear all registered events at once
 * @param instance pointer to structure holding the DSI Host core information
 * @return error code
 */
dsih_error_t mipi_dsih_unregister_all_events(dsih_ctrl_t * instance)
{
	int i = 0;
	if (instance == 0)
	{
		return ERR_DSI_INVALID_INSTANCE;
	}
	if (instance->status != INITIALIZED)
	{
		return ERR_DSI_INVALID_INSTANCE;
	}
	for (i = 0; i < DSI_MAX_EVENT; i++)
	{
		instance->event_registry[i] = 0;
	}
	mipi_dsih_hal_int_mask_0(instance, 0xffffff);
	mipi_dsih_hal_int_mask_1(instance, 0xffffff);
	/* automatically disable CRC reporting */
	mipi_dsih_hal_gen_crc_rx_en(instance, 0);
	return OK;
}
/**
 * This event handler shall be called upon receiving any event
 * it will call the specific callback (handler) registered for the invoking
 * event. Registration is done beforehand using mipi_dsih_register_event
 * its signature is void * so it could be OS agnostic (for it to be
 * registered in any OS/Interrupt controller)
 * @param param pointer to structure holding the DSI Host core information
 * @note This function must be registered with the DSI IRQs
 */
void mipi_dsih_event_handler(void * param)
{
	dsih_ctrl_t * instance = (dsih_ctrl_t *)(param);
	uint8_t i = 0;
    uint32_t status_0;
    uint32_t status_1;

	if (instance == 0)
	{
		return;
	}

    status_0 = mipi_dsih_hal_int_status_0(instance, 0xffffffff);
    status_1 = mipi_dsih_hal_int_status_1(instance, 0xffffffff);

	for (i = 0; i < DSI_MAX_EVENT; i++)
	{
		if (instance->event_registry[i] != 0)
		{
			if (i < HS_CONTENTION)
			{
				if ((status_0 & (1 << i)) != 0)
				{
					instance->event_registry[i](instance, &i);
				}
			}
			else
			{
				if ((status_1 & (1 << (i - HS_CONTENTION))) != 0)
				{
					instance->event_registry[i](instance, &i);
				}
			}
		}
	}
}
/**
 * Reset the DSI Host controller
 * - Sends a pulse to the shut down register
 * @param instance pointer to structure holding the DSI Host core information
 */
void mipi_dsih_reset_controller(dsih_ctrl_t * instance)
{
	mipi_dsih_hal_power(instance, 0);
	mipi_dsih_hal_power(instance, 1);
}
/**
 * Shut down the DSI Host controller
 * @param instance pointer to structure holding the DSI Host core information
 * @param shutdown (1) power up (0)
 */
void mipi_dsih_shutdown_controller(dsih_ctrl_t * instance, int shutdown)
{
	mipi_dsih_hal_power(instance, !shutdown);
}
/**
 * Reset the PHY module being controlled by the DSI Host controller
 * - Sends a pulse to the PPI reset signal
 * @param instance pointer to structure holding the DSI Host core information
 */
void mipi_dsih_reset_phy(dsih_ctrl_t * instance)
{
	mipi_dsih_dphy_reset(&(instance->phy_instance), 0);
	mipi_dsih_dphy_reset(&(instance->phy_instance), 1);
}
/**
 * Shut down the PHY module being controlled by the DSI Host controller
 * @param instance pointer to structure holding the DSI Host core information
 * @param shutdown (1) power up (0)
 */
void mipi_dsih_shutdown_phy(dsih_ctrl_t * instance, int shutdown)
{
	mipi_dsih_dphy_shutdown(&(instance->phy_instance), !shutdown);
}

/*
sprd Aoke added for power saving while suspend
*/
void mipi_dsih_ulps_mode(dsih_ctrl_t * instance, int en)
{
	mipi_dsih_dphy_ulps_data_lanes(&(instance->phy_instance),en);
	mipi_dsih_dphy_ulps_clk_lane(&(instance->phy_instance),en);
}
/**
 * Configure the eDPI interface
 * - Programs the controller to receive pixels on the DPI interface and send them
 * as commands (write memory start and write memory continue) instead of a
 * video stream.
 * @param instance pointer to structure holding the DSI Host core information
 * @param video_params pointer to the command mode video parameters context
 * @param send_setup_packets whether to send the setup packets from given info. These are: 1 - set pixel format; 2 - set column address; 3 - set page address
 * @return error code
 */
dsih_error_t mipi_dsih_edpi_video(dsih_ctrl_t * instance, dsih_cmd_mode_video_t *video_params, int send_setup_packets)
{
	dsih_error_t err_code = OK;
	uint8_t buf[7] = {0};
	uint32_t bytes_per_pixel_x100 = 0;
	mipi_dsih_hal_dpi_video_vc(instance, video_params->virtual_channel);
	mipi_dsih_cmd_mode(instance, 1);
	err_code = mipi_dsih_hal_dpi_color_coding(instance, video_params->color_coding);
	/* define whether write memory commands will be sent in LP or HS */
	err_code = err_code? err_code: mipi_dsih_hal_dcs_wr_tx_type(instance, 3, video_params->lp); /* long packet*/
	if (send_setup_packets)
	{
		/* define pixel packing format - 1 param */
		buf[0] = 0x3A;
		/* colour depth: table 6 DCS spec. 3:1| 8:2| 12:3| 16:5| 18:6| 24:7*/
		switch (video_params->color_coding)
		{
		case 0:
		case 1:
		case 2:
			buf[1] = 5;
			break;
		case 3:
		case 4:
			buf[1] = 6;
			break;
		default:
			buf[1] = 7;
			break;
		}
		err_code = err_code? err_code: mipi_dsih_dcs_wr_cmd(instance, video_params->virtual_channel, buf, 2);
		if (err_code)
		{
			instance->log_error("sprdfb: error setting up command video - colour depth");
		}
		/* set column address (left to right) - 4 param */
		buf[0] = 0x05; /* cmd length */
		buf[1] = 0x00;
		buf[2] = 0x2A; /* cmd opcode */
		buf[3] = (uint8_t)(video_params->h_start >> 8); /* payload start */
		buf[4] = (uint8_t)(video_params->h_start);
		buf[5] = (uint8_t)(video_params->h_active_pixels >> 8);
		buf[6] = (uint8_t)(video_params->h_active_pixels);
		err_code = err_code? err_code: mipi_dsih_dcs_wr_cmd(instance, video_params->virtual_channel, buf, 7);
		if (err_code)
		{
			instance->log_error("sprdfb: error setting up command video - set column address");
		}
		/* set page address (top to bottom) 4 - param*/
		buf[0] = 0x05; /* cmd length */
		buf[1] = 0x00;
		buf[2] = 0x2B; /* cmd opcode */
		buf[3] = (uint8_t)(video_params->v_start >> 8); /* payload start */
		buf[4] = (uint8_t)(video_params->v_start);
		buf[5] = (uint8_t)(video_params->v_active_lines >> 8);
		buf[6] = (uint8_t)(video_params->v_active_lines);
		err_code = err_code? err_code: mipi_dsih_dcs_wr_cmd(instance, video_params->virtual_channel, buf, 7);
		if (err_code)
		{
			instance->log_error("sprdfb: error setting up command video - set page address");
		}
	}
	switch (video_params->color_coding)
	{
	case COLOR_CODE_16BIT_CONFIG1:
	case COLOR_CODE_16BIT_CONFIG2:
	case COLOR_CODE_16BIT_CONFIG3:
		bytes_per_pixel_x100 = 200;
		break;
	case COLOR_CODE_18BIT_CONFIG1:
	case COLOR_CODE_18BIT_CONFIG2:
		bytes_per_pixel_x100 = 225;
		break;
	case COLOR_CODE_24BIT:
		bytes_per_pixel_x100 = 300;
		break;
	case COLOR_CODE_20BIT_YCC422_LOOSELY:
		bytes_per_pixel_x100 = 250;
		break;
	case COLOR_CODE_24BIT_YCC422:
		bytes_per_pixel_x100 = 300;
		break;
	case COLOR_CODE_16BIT_YCC422:
		bytes_per_pixel_x100 = 200;
		break;
	case COLOR_CODE_30BIT:
		bytes_per_pixel_x100 = 375;
		break;
	case COLOR_CODE_36BIT:
		bytes_per_pixel_x100 = 450;
		break;
	case COLOR_CODE_12BIT_YCC420:
		bytes_per_pixel_x100 = 150;
                  break;
	default:
		if (instance->log_error != 0)
		{
			instance->log_error("sprdfb: invalid color coding");
		}
		err_code = ERR_DSI_COLOR_CODING;
		break;
	}
	if (video_params->te)
	{
		mipi_dsih_hal_bta_en(instance, video_params->te);
		/* enable tearing effect */
		mipi_dsih_tear_effect_ack(instance, video_params->te);
		buf[0] = 0x35;
		err_code = mipi_dsih_dcs_wr_cmd(instance, video_params->virtual_channel, buf, 1);
	}
	mipi_dsih_dphy_enable_hs_clk(&(instance->phy_instance), 1);
	if ((((WORD_LENGTH * FIFO_DEPTH) * 100) / bytes_per_pixel_x100) > video_params->h_active_pixels)
	{
		mipi_dsih_hal_edpi_max_allowed_size(instance, video_params->h_active_pixels);
	}
	else
	{
		mipi_dsih_hal_edpi_max_allowed_size(instance, (((WORD_LENGTH * FIFO_DEPTH) * 100) / bytes_per_pixel_x100));
	}
	return err_code;
}

/* PRESP Time outs */
/**
 * Timeout for peripheral (for controller to stay still) after LP data
 * transmission write requests
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
 * link still, after sending a low power write operation. This period is
 * measured in cycles of lanebyteclk
 */
void mipi_dsih_presp_timeout_low_power_write(dsih_ctrl_t * instance, uint16_t no_of_byte_cycles)
{
	mipi_dsih_hal_presp_timeout_low_power_write(instance, no_of_byte_cycles);
}
/**
 * Timeout for peripheral (for controller to stay still) after LP data
 * transmission read requests
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
 * link still, after sending a low power read operation. This period is
 * measured in cycles of lanebyteclk
 */
void mipi_dsih_presp_timeout_low_power_read(dsih_ctrl_t * instance, uint16_t no_of_byte_cycles)
{
	mipi_dsih_hal_presp_timeout_low_power_read(instance, no_of_byte_cycles);
}
/**
 * Timeout for peripheral (for controller to stay still) after HS data
 * transmission write requests
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
 * link still, after sending a high-speed write operation. This period is
 * measured in cycles of lanebyteclk
 */
void mipi_dsih_presp_timeout_high_speed_write(dsih_ctrl_t * instance, uint16_t no_of_byte_cycles)
{
	mipi_dsih_hal_presp_timeout_high_speed_write(instance, no_of_byte_cycles);
}
/**
 * Timeout for peripheral between HS data transmission read requests
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
 * link still, after sending a high-speed read operation. This period is
 * measured in cycles of lanebyteclk
 */
void mipi_dsih_presp_timeout_high_speed_read(dsih_ctrl_t * instance, uint16_t no_of_byte_cycles)
{
	mipi_dsih_hal_presp_timeout_high_speed_read(instance, no_of_byte_cycles);
}
/**
 * Timeout for peripheral (for controller to stay still) after bus turn around
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles period for which the DWC_mipi_dsi_host keeps the
 * link still, after sending a BTA operation. This period is
 * measured in cycles of lanebyteclk
 */
void mipi_dsih_presp_timeout_bta(dsih_ctrl_t * instance, uint16_t no_of_byte_cycles)
{
	mipi_dsih_hal_presp_timeout_bta(instance, no_of_byte_cycles);
}
uint16_t mipi_dsih_check_dbi_fifos_state(dsih_ctrl_t * instance)
{
	uint16_t cnt = 0;
	while( cnt < 5000 && mipi_dsih_read_word(instance, R_DSI_HOST_CMD_PKT_STATUS) != 0x15)
	{
		cnt ++;
	}
	return (mipi_dsih_read_word(instance, R_DSI_HOST_CMD_PKT_STATUS) != 0x15) ? -1 : 1;
}
uint16_t mipi_dsih_check_ulpm_mode(dsih_ctrl_t * instance)
{
	return (mipi_dsih_read_word(instance, R_DSI_HOST_PHY_STATUS) != 0x1528) ? -1 : 1;
}
