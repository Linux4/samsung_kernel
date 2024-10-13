/*
 * SquricleFlash-an30259a_tuning.c - LED tuning parameters
 *
 * Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 */

//#define MAX_CURRENT_TUNING		1
//#define LED_BRIGHTNESS_ADJUST	0
//#define BRIGHTNESS_STEPS_3	0
#define BRIGHTNESS_STEPS_2	1

#define BACKWARD_COMPATIBLE_COLORS	1

#define COLOR_EQUALIZER_MODE	1

#define AN30259A_USE_HW_BLINK	1


#define	UX_8_COLOR_MAP		1
#define	UX_5_COLOR_MAP		1
#define	UX_5_COLOR_MAP_1	1
#define	UX_5_COLOR_MAP_2	1

#ifdef MAX_CURRENT_TUNING
/* Provided Imax = 01h */
#define RED_LED_MAX_CURRENT		200	
#define GREEN_LED_MAX_CURRENT	100
#define BLUE_LED_MAX_CURRENT	100
#endif

#define LED_MAX_CURRENT			180
#define LED_LOWER_REAL_COLOR	159


#define AN30259A_EXTRACT_A(ARGB)	(ARGB >> 24)
#define AN30259A_EXTRACT_R(ARGB)	((ARGB >> 16) & 0xFF)
#define AN30259A_EXTRACT_G(ARGB)	((ARGB >> 8) & 0xFF)
#define AN30259A_EXTRACT_B(ARGB)	(ARGB & 0xFF)

#define AN30259A_LIMIT_MAX_CURRENT(color, max_current) \
			color = color < max_current ? color : max_current;

#define AN30259A_MAX_CURRENT_PROPOSTION(color, max_current) \
			color = (color * max_current) / 0xFF;
			
#define AN30259A_BRIGHTNESS_COLOR_MAP(color, brightness) \
			color = (color * brightness) / 0xFF;
			
#define AN30259A_COLOR_SCALE(color) \
			((color * LED_MAX_CURRENT) / 0xFF);	
			

#define AN30259A_FROM_COLOR(R,G,B)		((R << 16) | (G << 8) | B)

#define COLOR_SKYBLUE_1		0x0000A7E5 	// AN30259A_FROM_COLOR(0,167,229)
#define COLOR_SKYBLUE			0x0000D2F1 	// AN30259A_FROM_COLOR(0,210,241)
#define COLOR_HOTPINK			0x00F3004A  // AN30259A_FROM_COLOR(255,0,0)
#define COLOR_YELLOWGREEN_1	0x0069AC00  // AN30259A_FROM_COLOR(105,172,0)
#define COLOR_YELLOWGREEN		0x0070AE0E  // AN30259A_FROM_COLOR(112,174,14)
#define COLOR_YELLOWORANGE		0x00EAB000  // AN30259A_FROM_COLOR(234,176,0)
#define COLOR_YELLOW			0x00EAE200  // AN30259A_FROM_COLOR(234,226,0)
#define COLOR_WHITE			0x00D9D9D6  // AN30259A_FROM_COLOR(217,217,214)
#define COLOR_RED				0x00FF0000  // AN30259A_FROM_COLOR(255,0,0)
#define COLOR_GREEN			0x0000FF00 	// AN30259A_FROM_COLOR(0,255,0)
#define COLOR_BLUE				0x000000FF 	// AN30259A_FROM_COLOR(0,0,255)
#define COLOR_PURPLE			0x009C00B0 	// AN30259A_FROM_COLOR(156,0,176)
#define COLOR_LIGHTBLUE		0x00264DF5 	// AN30259A_FROM_COLOR(38,77,245)
#define COLOR_MEGENTA			0x00DE37B9 	// AN30259A_FROM_COLOR(222,55,185)

// Keep it enabled for Backward compatibility to avoid device damage due to high current value
//#if defined(BRIGHTNESS_STEPS_3)
/* Max Brightness */
#define MAX_CURRENT_SKYBLUE			0x00005A56	// AN30259A_FROM_COLOR(0,90,86)
#define MAX_CURRENT_HOTPINK			0x00960005  // AN30259A_FROM_COLOR(150,0,5)
#define MAX_CURRENT_YELLOWGREEN		0x001E5A00  // AN30259A_FROM_COLOR(30,90,0)
#define MAX_CURRENT_YELLOWORANGE	0x00963200  // AN30259A_FROM_COLOR(150,50,0)
#define MAX_CURRENT_YELLOW			0x005A2800  // AN30259A_FROM_COLOR(90,40,0)
#define MAX_CURRENT_WHITE			0x005A5A5A  // AN30259A_FROM_COLOR(90,90,90)
#define MAX_CURRENT_RED				0x00960000  // AN30259A_FROM_COLOR(150,0,0)
#define MAX_CURRENT_GREEN			0x00005A00  // AN30259A_FROM_COLOR(0,90,0)
#define MAX_CURRENT_BLUE			0x0000005A  // AN30259A_FROM_COLOR(0,0,90)
#define MAX_CURRENT_PURPLE			0x005A003C 	// AN30259A_FROM_COLOR(90,0,60)

/* Mid Brightness */
#define MID_CURRENT_SKYBLUE			0x00003C39	// AN30259A_FROM_COLOR(0,60,57)
#define MID_CURRENT_RED				0x005A0000  // AN30259A_FROM_COLOR(90,0,0)
#define MID_CURRENT_YELLOWGREEN		0x00143C00  // AN30259A_FROM_COLOR(20,60,0)
#define MID_CURRENT_YELLOW			0x003C1B00  // AN30259A_FROM_COLOR(60,27,0)
#define MID_CURRENT_PURPLE			0x003C0028 	// AN30259A_FROM_COLOR(60,0,40)
#define MID_CURRENT_WHITE			0x00414141 	// AN30259A_FROM_COLOR(65,65,65)

/* Min Brightness */
#define MIN_CURRENT_SKYBLUE			0x00002826	// AN30259A_FROM_COLOR(0,40,38)
#define MIN_CURRENT_RED				0x00320000  // AN30259A_FROM_COLOR(50,0,0)
#define MIN_CURRENT_YELLOWGREEN		0x000E2800  // AN30259A_FROM_COLOR(14,40,0)
#define MIN_CURRENT_YELLOW			0x00281200  // AN30259A_FROM_COLOR(40,18,0)
#define MIN_CURRENT_PURPLE			0x0028001B 	// AN30259A_FROM_COLOR(40,0,27)
#define MIN_CURRENT_WHITE			0x00281200 	// AN30259A_FROM_COLOR(40,18,0)

//#elif defined (BRIGHTNESS_STEPS_2)

/* High Brightness */
#define HIGH_CURRENT_SKYBLUE		0x00003C39	// AN30259A_FROM_COLOR(0,60,57)
#define HIGH_CURRENT_RED			0x005A0000  // AN30259A_FROM_COLOR(90,0,0)
#define HIGH_CURRENT_YELLOWGREEN	0x00143C00  // AN30259A_FROM_COLOR(20,60,0)
#define HIGH_CURRENT_YELLOW			0x003C1B00  // AN30259A_FROM_COLOR(60,27,0)
#define HIGH_CURRENT_PURPLE			0x0014003C 	// AN30259A_FROM_COLOR(20,0,60)
#define HIGH_CURRENT_GREEN			0x00005A00  // AN30259A_FROM_COLOR(0,90,0)
#define HIGH_CURRENT_BLUE			0x0000005A  // AN30259A_FROM_COLOR(0,0,90)
#define HIGH_CURRENT_LIGHTBLUE		0x0000005A  // AN30259A_FROM_COLOR(0,0,90)
#define HIGH_CURRENT_MEGENTA		0x003C001E  // AN30259A_FROM_COLOR(60,0,30)


/* Low Brightness */
#define LOW_CURRENT_SKYBLUE			0x00002826	// AN30259A_FROM_COLOR(0,40,38)
#define LOW_CURRENT_RED				0x00320000  // AN30259A_FROM_COLOR(50,0,0)
#define LOW_CURRENT_YELLOWGREEN		0x000E2800  // AN30259A_FROM_COLOR(14,40,0)
#define LOW_CURRENT_YELLOW			0x00281200  // AN30259A_FROM_COLOR(40,18,0)
#define LOW_CURRENT_PURPLE			0x000E0028 	// AN30259A_FROM_COLOR(14,0,40)
#define LOW_CURRENT_GREEN			0x00003200  // AN30259A_FROM_COLOR(0,32,0)
#define LOW_CURRENT_BLUE			0x00000032  // AN30259A_FROM_COLOR(0,0,32)
#define LOW_CURRENT_LIGHTBLUE		0x00000032  // AN30259A_FROM_COLOR(0,0,32)
#define LOW_CURRENT_MEGENTA			0x00280014  // AN30259A_FROM_COLOR(40,0,20)

//#endif


#define COLOR_1	COLOR_SKYBLUE			
#define COLOR_2	COLOR_HOTPINK
#define COLOR_3	COLOR_YELLOWGREEN
#define COLOR_4	COLOR_YELLOW
#define COLOR_5	COLOR_WHITE
#define COLOR_6	COLOR_RED
#define COLOR_7	COLOR_GREEN
#define COLOR_8	COLOR_BLUE

#define CURRENT_1	CURRENT_SKYBLUE		
#define CURRENT_2	CURRENT_HOTPINK			
#define CURRENT_3	CURRENT_YELLOWGREEN	
#define CURRENT_4	CURRENT_YELLOW		
#define CURRENT_5	CURRENT_WHITE		
#define CURRENT_6	CURRENT_RED			
#define CURRENT_7	CURRENT_GREEN		
#define CURRENT_8	CURRENT_BLUE		

// Keep it enabled for Backward compatibility to avoid device damage due to high current value
//#if defined(BRIGHTNESS_STEPS_3)
#define MAX_BRIGHTNESS		0xFF
#define MID_BRIGHTNESS		0x99
#define MIN_BRIGHTNESS		0x66
//#elif defined (BRIGHTNESS_STEPS_2)
#define BRIGHTNESS_LEVEL_1	1  //High
#define BRIGHTNESS_LEVEL_2	2  //LOW
//#endif

#define DEFAULT_BRIGHTNESS	BRIGHTNESS_LEVEL_2


#if defined(COLOR_EQUALIZER_MODE)
static enum led_brightness __inline an30259a_color_scale(
					enum led_brightness color)
{
	return (color < LED_LOWER_REAL_COLOR) ? \
			((color * LED_MAX_CURRENT) / 0xFF) : \
			(((color * 10) / 6) - 245);
}

/**
* an30259a_fixed_color_map - Maps natural colors to LED current 
**/
static enum led_brightness an30259a_fixed_color_map(enum led_brightness color, u8 intensity)
{
	u8 r = AN30259A_EXTRACT_R(color); 
	u8 g = AN30259A_EXTRACT_G(color); 
	u8 b = AN30259A_EXTRACT_B(color); 
	
	r = an30259a_color_scale(r);
	g = an30259a_color_scale(g);
	b = an30259a_color_scale(b);
	
	printk(KERN_ERR "COLOR:0x%X Current 0x%X", color,
					AN30259A_FROM_COLOR(r, g, b));
	
	return AN30259A_FROM_COLOR(r, g, b); 
}
#else
/**
* an30259a_fixed_color_map - Map the required current for the fixed set 
* 		of colors 
**/
static enum led_brightness an30259a_fixed_color_map(enum led_brightness color, u8 intensity)
{
	// Fixed Color mapping as requested by HW team.
	switch(intensity)
	{
#if defined(BRIGHTNESS_STEPS_3)		
		case MAX_BRIGHTNESS:
			switch(color & 0x00FFFFFF)
			{	
				case COLOR_SKYBLUE_1	: return MAX_CURRENT_SKYBLUE		;
				case COLOR_SKYBLUE		: return MAX_CURRENT_SKYBLUE		;
				case COLOR_HOTPINK		: return MAX_CURRENT_HOTPINK		;
				case COLOR_YELLOWGREEN_1: return MAX_CURRENT_YELLOWGREEN	;
				case COLOR_YELLOWGREEN	: return MAX_CURRENT_YELLOWGREEN	;
				case COLOR_YELLOWORANGE: return MAX_CURRENT_YELLOWORANGE	;
				case COLOR_YELLOW		: return MAX_CURRENT_YELLOW			;
				case COLOR_WHITE		: return MAX_CURRENT_WHITE			;
				case COLOR_RED			: return MAX_CURRENT_RED			;
				case COLOR_GREEN		: return MAX_CURRENT_GREEN			;
				case COLOR_BLUE		: return MAX_CURRENT_BLUE			;
				case COLOR_LIGHTBLUE	: return MAX_CURRENT_BLUE			;
				case COLOR_PURPLE		: return MAX_CURRENT_PURPLE			;
			}
			break;
		case MID_BRIGHTNESS:
			switch(color & 0x00FFFFFF)
			{	
				case COLOR_SKYBLUE_1	: return MID_CURRENT_SKYBLUE		;
				case COLOR_SKYBLUE		: return MID_CURRENT_SKYBLUE		;
				case COLOR_YELLOWGREEN_1: return MID_CURRENT_YELLOWGREEN	;
				case COLOR_YELLOWGREEN	: return MID_CURRENT_YELLOWGREEN	;
				case COLOR_YELLOW		: return MID_CURRENT_YELLOW			;
				case COLOR_WHITE		: return MID_CURRENT_WHITE			;
				case COLOR_RED			: return MID_CURRENT_RED			;
				case COLOR_PURPLE		: return MID_CURRENT_PURPLE			;
			}
			break;
		case MIN_BRIGHTNESS:
			switch(color & 0x00FFFFFF)
			{	
				case COLOR_SKYBLUE_1	: return MIN_CURRENT_SKYBLUE		;
				case COLOR_SKYBLUE		: return MIN_CURRENT_SKYBLUE		;
				case COLOR_YELLOWGREEN_1: return MIN_CURRENT_YELLOWGREEN	;
				case COLOR_YELLOWGREEN	: return MIN_CURRENT_YELLOWGREEN	;
				case COLOR_YELLOW		: return MIN_CURRENT_YELLOW			;
				case COLOR_WHITE		: return MIN_CURRENT_WHITE			;
				case COLOR_RED			: return MIN_CURRENT_RED			;
				case COLOR_PURPLE		: return MIN_CURRENT_PURPLE			;
			}
			break;
#elif defined (BRIGHTNESS_STEPS_2)
		case BRIGHTNESS_LEVEL_1:
		default:
			switch(color & 0x00FFFFFF)
			{	
				case COLOR_SKYBLUE_1	: return LOW_CURRENT_SKYBLUE		;
				case COLOR_SKYBLUE		: return LOW_CURRENT_SKYBLUE		;
				case COLOR_RED			: return LOW_CURRENT_RED			;
				case COLOR_YELLOWGREEN_1: return LOW_CURRENT_YELLOWGREEN	;
				case COLOR_YELLOWGREEN	: return LOW_CURRENT_YELLOWGREEN	;
				case COLOR_YELLOW		: return LOW_CURRENT_YELLOW			;
				case COLOR_PURPLE		: return LOW_CURRENT_PURPLE			;
				case COLOR_MEGENTA		: return LOW_CURRENT_MEGENTA		;
				case COLOR_GREEN		: return LOW_CURRENT_GREEN			;
				case COLOR_BLUE		: return LOW_CURRENT_BLUE			;
				case COLOR_LIGHTBLUE	: return LOW_CURRENT_BLUE			;
			}
			break;
		case BRIGHTNESS_LEVEL_2:
			switch(color & 0x00FFFFFF)
			{	
				case COLOR_SKYBLUE_1	: return HIGH_CURRENT_SKYBLUE		;
				case COLOR_SKYBLUE		: return HIGH_CURRENT_SKYBLUE		;
				case COLOR_RED			: return HIGH_CURRENT_RED			;
				case COLOR_YELLOWGREEN_1: return HIGH_CURRENT_YELLOWGREEN	;
				case COLOR_YELLOWGREEN	: return HIGH_CURRENT_YELLOWGREEN	;
				case COLOR_YELLOW		: return HIGH_CURRENT_YELLOW		;
				case COLOR_PURPLE		: return HIGH_CURRENT_PURPLE		;
				case COLOR_MEGENTA		: return HIGH_CURRENT_MEGENTA		;
				case COLOR_GREEN		: return HIGH_CURRENT_GREEN			;
				case COLOR_BLUE		: return HIGH_CURRENT_BLUE			;
				case COLOR_LIGHTBLUE	: return HIGH_CURRENT_BLUE			;
			}
			break;
#endif 			
	}
	
#ifdef BACKWARD_COMPATIBLE_COLORS	
			// For Backward compatibility to avoid device damage due to high current value
			switch(color & 0x00FFFFFF)
			{
				case COLOR_HOTPINK		: return MAX_CURRENT_HOTPINK		;
				case COLOR_YELLOWORANGE: return MAX_CURRENT_YELLOWORANGE	;
				case COLOR_WHITE		: return MID_CURRENT_WHITE			;
			}
#endif	//BACKWARD_COMPATIBLE_COLORS
	
	{
		enum led_brightness R = AN30259A_EXTRACT_R(color);
		enum led_brightness G = AN30259A_EXTRACT_G(color);
		enum led_brightness B = AN30259A_EXTRACT_B(color);		
#ifdef MAX_CURRENT_TUNING 		
#if 0			
		enum led_brightness R = AN30259A_EXTRACT_R(color) * 
									RED_LED_MAX_CURRENT) / 0xFF;
		enum led_brightness G = AN30259A_EXTRACT_G(color) * 
									GREEN_LED_MAX_CURRENT) / 0xFF;
		enum led_brightness B = AN30259A_EXTRACT_B(color)  * 
									BLUE_LED_MAX_CURRENT) / 0xFF;
#else
		AN30259A_MAX_CURRENT_PROPOSTION(R, RED_LED_MAX_CURRENT);
		AN30259A_MAX_CURRENT_PROPOSTION(G, GREEN_LED_MAX_CURRENT);
		AN30259A_MAX_CURRENT_PROPOSTION(B, BLUE_LED_MAX_CURRENT);		
#endif			
		AN30259A_LIMIT_MAX_CURRENT(R, RED_LED_MAX_CURRENT);
		AN30259A_LIMIT_MAX_CURRENT(G, GREEN_LED_MAX_CURRENT);
		AN30259A_LIMIT_MAX_CURRENT(B, BLUE_LED_MAX_CURRENT);
#endif	//MAX_CURRENT_TUNING		
		
#ifdef LED_BRIGHTNESS_ADJUST		
		if(intensity) {
			AN30259A_BRIGHTNESS_COLOR_MAP(R, intensity);
			AN30259A_BRIGHTNESS_COLOR_MAP(G, intensity);
			AN30259A_BRIGHTNESS_COLOR_MAP(B, intensity);
		}
#endif //LED_BRIGHTNESS_ADJUST

		return AN30259A_FROM_COLOR(R,G,B);
	}
	
	return color;
}

#endif

