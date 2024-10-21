#ifndef __STM32_POGO_MODEL_H__
#define __STM32_POGO_MODEL_H__

#define ROW_MODEL_LIST_SIZE		2
static struct stm32_model_info row_model_list[MODEL_TYPE_MAX][ROW_MODEL_LIST_SIZE] = {
/*
 *		id	name		tpad	dcdc	bypass	oem	ai-key
 */
	{
		/* MODEL_TYPE_BASIC */
		{0x00,	"EF-DT870",	true,	false,	false,	false,	false},
		{0x01,	"EF-DT630",	false,	false,	false,	false,	false},
	},
	{
		/* MODEL_TYPE_PLUS */
		{0x00,	"EF-DT970",	true,	false,	false,	false,	false},
		{0x01,	"EF-DT730",	false,	false,	false,	false,	false},
	},
};

#define BYPASS_MODEL_LIST_SIZE		5
static struct stm32_model_info bypass_model_list[MODEL_TYPE_MAX][BYPASS_MODEL_LIST_SIZE] = {
/*
 *		id	name		tpad	dcdc	bypass	oem	ai-key
 */
	{
		/* MODEL_TYPE_BASIC */
		{0x02,	"EF-DX710",	false,	false,	true,	false,	false}, /* ID is 0xFC, but read as 02 (L0 mcu) */
		{0xFD,	"EF-DX715",	true,	false,	true,	false,	false},
		{0x03,	"EF-DX720",	false,	false,	true,	false,	true}, /* ID is 0xD4, but read as 03 (L0 mcu) */
		{0xD1,	"EF-DX725",	true,	false,	true,	false,	true},
	},
	{
		/* MODEL_TYPE_PLUS */
		{0xFA,	"EF-DX810",	false,	false,	true,	false,	false},
		{0xFB,	"EF-DX815",	true,	false,	true,	false,	false},
		{0xD5,	"EF-DX820",	false,	false,	true,	false,	true},
		{0xD2,	"EF-DX825",	true,	false,	true,	false,	true},
	},
	{
		/* MODEL_TYPE_ULTRA */
		{0xFE,	"EF-DX900",	true,	false,	true,	false,	false},
		{0xF8,	"EF-DX910",	false,	false,	true,	false,	false},
		{0xF9,	"EF-DX915",	true,	true,	true,	false,	false},
		{0xD6,	"EF-DX920",	false,	false,	true,	false,	true},
		{0xD3,	"EF-DX925",	true,	true,	true,	false,	true},
	},

};

static struct stm32_model_info oem_model_list[] = {
/*
 *	id	name			tpad	dcdc	bypass	oem	ai-key
 */
	{0x51,	"Neos Keyboard",	false,	false,	true,	true,	false},
};
#define OEM_MODEL_LIST_SIZE	(sizeof(oem_model_list) / sizeof(struct stm32_model_info))
#endif
