/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include "../sprdfb_panel.h"

static int32_t nt35516_init(struct panel_spec *self)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	pr_debug("nt35516_init\n");

	send_cmd(0xFF00); send_data(0x00AA); /*AA*/
	send_cmd(0xFF01); send_data(0x0055);/*55*/
	send_cmd(0xFF02); send_data(0x0025);/*08*/
	send_cmd(0xFF03); send_data(0x0001);/*01*/

	send_cmd(0xF300); send_data(0x0002);
	send_cmd(0xF303); send_data(0x0015);

	/*ENABPAGE 0*/
	send_cmd(0xF000); send_data(0x0055); /*Manufacture Command Set Control*/
	send_cmd(0xF001); send_data(0x00AA);
	send_cmd(0xF002); send_data(0x0052);
	send_cmd(0xF003); send_data(0x0008);
	send_cmd(0xF004); send_data(0x0000);

	send_cmd(0xB800); send_data(0x0001);
	send_cmd(0xB801); send_data(0x0002);
	send_cmd(0xB802); send_data(0x0002);
	send_cmd(0xB803); send_data(0x0002);

	send_cmd(0xBC00); send_data(0x0005); /*Zig-Zag Inversion*/
	send_cmd(0xBC01); send_data(0x0005);
	send_cmd(0xBC02); send_data(0x0005);

	send_cmd(0xBD00);send_data(0x0001);
	send_cmd(0xBD01);send_data(0x0063);//54fps
	send_cmd(0xBD02);send_data(0x0010);
	send_cmd(0xBD03);send_data(0x0038);
	send_cmd(0xBD04);send_data(0x0001);


	send_cmd(0x4C00); send_data(0x0011); /*DB4=1,Enable Vivid Color,DB4=0 Disable Vivid Color*/

	/* ENA PAGE 1*/
	send_cmd(0xF000); send_data(0x0055); /*Manufacture Command Set Control */
	send_cmd(0xF001); send_data(0x00AA);
	send_cmd(0xF002); send_data(0x0052);
	send_cmd(0xF003); send_data(0x0008);
	send_cmd(0xF004); send_data(0x0001);/*Page1*/

	send_cmd(0xB000); send_data(0x0005); /*Setting AVDD Voltage 6V*/
	send_cmd(0xB001); send_data(0x0005);
	send_cmd(0xB002); send_data(0x0005);

	send_cmd(0xB600); send_data(0x0044); /* Setting AVEE boosting time 2.5*vpnl */
	send_cmd(0xB601); send_data(0x0044);
	send_cmd(0xB602); send_data(0x0044);

	send_cmd(0xB100); send_data(0x0005); /*Setting AVEE Voltage -6V*/
	send_cmd(0xB101); send_data(0x0005);
	send_cmd(0xB102); send_data(0x0005);

	/*Sett AVEE boosting time -2.5NL*/
	send_cmd(0xB700); send_data(0x0034);
	send_cmd(0xB701); send_data(0x0034);
	send_cmd(0xB702); send_data(0x0034);

	/*Sett VGLX boosting time  AVEVDD */
	send_cmd(0xBA00); send_data(0x0014); /*0x24 --> 0x14*/
	send_cmd(0xBA01); send_data(0x0014);
	send_cmd(0xBA02); send_data(0x0014);

	/*Gammoltage */
	send_cmd(0xBC00); send_data(0x0000);
	send_cmd(0xBC01); send_data(0x00A0);/*VGMP 0x88=4.7V  0x78=4.5V   0xA0=5.0V */
	send_cmd(0xBC02); send_data(0x0000);/*VGSP*/

	//Gammoltage */
	send_cmd(0xBD00); send_data(0x0000);
	send_cmd(0xBD01); send_data(0x00A0);/*VGMN 0x88=-4.7V 0x78=-4.5V   0xA0=-5.0V*/
	send_cmd(0xBD02); send_data(0x0000);/*VGSN*/

	send_cmd(0xBE00); send_data(0x0057); /* Setting VCOM Offset Voltage  0x4E\u017eÄÎª0x57  20111019 LIYAN*/

	/*GAMMED Positive*/
	send_cmd(0xD100); send_data(0x0000);
	send_cmd(0xD101); send_data(0x0032);
	send_cmd(0xD102); send_data(0x0000);
	send_cmd(0xD103); send_data(0x0041);
	send_cmd(0xD104); send_data(0x0000);
	send_cmd(0xD105); send_data(0x0054);
	send_cmd(0xD106); send_data(0x0000);
	send_cmd(0xD107); send_data(0x0067);
	send_cmd(0xD108); send_data(0x0000);
	send_cmd(0xD109); send_data(0x007A);
	send_cmd(0xD10A); send_data(0x0000);
	send_cmd(0xD10B); send_data(0x0098);
	send_cmd(0xD10C); send_data(0x0000);
	send_cmd(0xD10D); send_data(0x00B0);
	send_cmd(0xD10E); send_data(0x0000);
	send_cmd(0xD10F); send_data(0x00DB);
	send_cmd(0xD200); send_data(0x0001);
	send_cmd(0xD201); send_data(0x0001);
	send_cmd(0xD202); send_data(0x0001);
	send_cmd(0xD203); send_data(0x003F);
	send_cmd(0xD204); send_data(0x0001);
	send_cmd(0xD205); send_data(0x0070);
	send_cmd(0xD206); send_data(0x0001);
	send_cmd(0xD207); send_data(0x00B4);
	send_cmd(0xD208); send_data(0x0001);
	send_cmd(0xD209); send_data(0x00EC);
	send_cmd(0xD20A); send_data(0x0001);
	send_cmd(0xD20B); send_data(0x00ED);
	send_cmd(0xD20C); send_data(0x0002);
	send_cmd(0xD20D); send_data(0x001E);
	send_cmd(0xD20E); send_data(0x0002);
	send_cmd(0xD20F); send_data(0x0051);
	send_cmd(0xD300); send_data(0x0002);
	send_cmd(0xD301); send_data(0x006C);
	send_cmd(0xD302); send_data(0x0002);
	send_cmd(0xD303); send_data(0x008D);
	send_cmd(0xD304); send_data(0x0002);
	send_cmd(0xD305); send_data(0x00A5);
	send_cmd(0xD306); send_data(0x0002);
	send_cmd(0xD307); send_data(0x00C9);
	send_cmd(0xD308); send_data(0x0002);
	send_cmd(0xD309); send_data(0x00EA);
	send_cmd(0xD30A); send_data(0x0003);
	send_cmd(0xD30B); send_data(0x0019);
	send_cmd(0xD30C); send_data(0x0003);
	send_cmd(0xD30D); send_data(0x0045);
	send_cmd(0xD30E); send_data(0x0003);
	send_cmd(0xD30F); send_data(0x007A);
	send_cmd(0xD400); send_data(0x0003);
	send_cmd(0xD401); send_data(0x00B0);
	send_cmd(0xD402); send_data(0x0003);
	send_cmd(0xD403); send_data(0x00F4);

	/*Posie Gamma for GREEN*/
	send_cmd(0xD500); send_data(0x0000);
	send_cmd(0xD501); send_data(0x0037);
	send_cmd(0xD502); send_data(0x0000);
	send_cmd(0xD503); send_data(0x0041);
	send_cmd(0xD504); send_data(0x0000);
	send_cmd(0xD505); send_data(0x0054);
	send_cmd(0xD506); send_data(0x0000);
	send_cmd(0xD507); send_data(0x0067);
	send_cmd(0xD508); send_data(0x0000);
	send_cmd(0xD509); send_data(0x007A);
	send_cmd(0xD50A); send_data(0x0000);
	send_cmd(0xD50B); send_data(0x0098);
	send_cmd(0xD50C); send_data(0x0000);
	send_cmd(0xD50D); send_data(0x00B0);
	send_cmd(0xD50E); send_data(0x0000);
	send_cmd(0xD50F); send_data(0x00DB);
	send_cmd(0xD600); send_data(0x0001);
	send_cmd(0xD601); send_data(0x0001);
	send_cmd(0xD602); send_data(0x0001);
	send_cmd(0xD603); send_data(0x003F);
	send_cmd(0xD604); send_data(0x0001);
	send_cmd(0xD605); send_data(0x0070);
	send_cmd(0xD606); send_data(0x0001);
	send_cmd(0xD607); send_data(0x00B4);
	send_cmd(0xD608); send_data(0x0001);
	send_cmd(0xD609); send_data(0x00EC);
	send_cmd(0xD60A); send_data(0x0001);
	send_cmd(0xD60B); send_data(0x00ED);
	send_cmd(0xD60C); send_data(0x0002);
	send_cmd(0xD60D); send_data(0x001E);
	send_cmd(0xD60E); send_data(0x0002);
	send_cmd(0xD60F); send_data(0x0051);
	send_cmd(0xD700); send_data(0x0002);
	send_cmd(0xD701); send_data(0x006C);
	send_cmd(0xD702); send_data(0x0002);
	send_cmd(0xD703); send_data(0x008D);
	send_cmd(0xD704); send_data(0x0002);
	send_cmd(0xD705); send_data(0x00A5);
	send_cmd(0xD706); send_data(0x0002);
	send_cmd(0xD707); send_data(0x00C9);
	send_cmd(0xD708); send_data(0x0002);
	send_cmd(0xD709); send_data(0x00EA);
	send_cmd(0xD70A); send_data(0x0003);
	send_cmd(0xD70B); send_data(0x0019);
	send_cmd(0xD70C); send_data(0x0003);
	send_cmd(0xD70D); send_data(0x0045);
	send_cmd(0xD70E); send_data(0x0003);
	send_cmd(0xD70F); send_data(0x007A);
	send_cmd(0xD800); send_data(0x0003);
	send_cmd(0xD801); send_data(0x00A0);
	send_cmd(0xD802); send_data(0x0003);
	send_cmd(0xD803); send_data(0x00F4);

	/*Posie Gamma for BLUE*/
	send_cmd(0xD900); send_data(0x0000);
	send_cmd(0xD901); send_data(0x0032);
	send_cmd(0xD902); send_data(0x0000);
	send_cmd(0xD903); send_data(0x0041);
	send_cmd(0xD904); send_data(0x0000);
	send_cmd(0xD905); send_data(0x0054);
	send_cmd(0xD906); send_data(0x0000);
	send_cmd(0xD907); send_data(0x0067);
	send_cmd(0xD908); send_data(0x0000);
	send_cmd(0xD909); send_data(0x007A);
	send_cmd(0xD90A); send_data(0x0000);
	send_cmd(0xD90B); send_data(0x0098);
	send_cmd(0xD90C); send_data(0x0000);
	send_cmd(0xD90D); send_data(0x00B0);
	send_cmd(0xD90E); send_data(0x0000);
	send_cmd(0xD90F); send_data(0x00DB);
	send_cmd(0xDD00); send_data(0x0001);
	send_cmd(0xDD01); send_data(0x0001);
	send_cmd(0xDD02); send_data(0x0001);
	send_cmd(0xDD03); send_data(0x003F);
	send_cmd(0xDD04); send_data(0x0001);
	send_cmd(0xDD05); send_data(0x0070);
	send_cmd(0xDD06); send_data(0x0001);
	send_cmd(0xDD07); send_data(0x00B4);
	send_cmd(0xDD08); send_data(0x0001);
	send_cmd(0xDD09); send_data(0x00EC);
	send_cmd(0xDD0A); send_data(0x0001);
	send_cmd(0xDD0B); send_data(0x00ED);
	send_cmd(0xDD0C); send_data(0x0002);
	send_cmd(0xDD0D); send_data(0x001E);
	send_cmd(0xDD0E); send_data(0x0002);
	send_cmd(0xDD0F); send_data(0x0051);
	send_cmd(0xDE00); send_data(0x0002);
	send_cmd(0xDE01); send_data(0x006C);
	send_cmd(0xDE02); send_data(0x0002);
	send_cmd(0xDE03); send_data(0x008D);
	send_cmd(0xDE04); send_data(0x0002);
	send_cmd(0xDE05); send_data(0x00A5);
	send_cmd(0xDE06); send_data(0x0002);
	send_cmd(0xDE07); send_data(0x00C9);
	send_cmd(0xDE08); send_data(0x0002);
	send_cmd(0xDE09); send_data(0x00EA);
	send_cmd(0xDE0A); send_data(0x0003);
	send_cmd(0xDE0B); send_data(0x0019);
	send_cmd(0xDE0C); send_data(0x0003);
	send_cmd(0xDE0D); send_data(0x0045);
	send_cmd(0xDE0E); send_data(0x0003);
	send_cmd(0xDE0F); send_data(0x007A);
	send_cmd(0xDF00); send_data(0x0003);
	send_cmd(0xDF01); send_data(0x00A0);
	send_cmd(0xDF02); send_data(0x0003);
	send_cmd(0xDF03); send_data(0x00F4);

	/*Negae Gamma for RED*/
	send_cmd(0xE000); send_data(0x0000);
	send_cmd(0xE001); send_data(0x0032);
	send_cmd(0xE002); send_data(0x0000);
	send_cmd(0xE003); send_data(0x0041);
	send_cmd(0xE004); send_data(0x0000);
	send_cmd(0xE005); send_data(0x0054);
	send_cmd(0xE006); send_data(0x0000);
	send_cmd(0xE007); send_data(0x0067);
	send_cmd(0xE008); send_data(0x0000);
	send_cmd(0xE009); send_data(0x007A);
	send_cmd(0xE00A); send_data(0x0000);
	send_cmd(0xE00B); send_data(0x0098);
	send_cmd(0xE00C); send_data(0x0000);
	send_cmd(0xE00D); send_data(0x00B0);
	send_cmd(0xE00E); send_data(0x0000);
	send_cmd(0xE00F); send_data(0x00DB);
	send_cmd(0xE100); send_data(0x0001);
	send_cmd(0xE101); send_data(0x0001);
	send_cmd(0xE102); send_data(0x0001);
	send_cmd(0xE103); send_data(0x003F);
	send_cmd(0xE104); send_data(0x0001);
	send_cmd(0xE105); send_data(0x0070);
	send_cmd(0xE106); send_data(0x0001);
	send_cmd(0xE107); send_data(0x00B4);
	send_cmd(0xE108); send_data(0x0001);
	send_cmd(0xE109); send_data(0x00EC);
	send_cmd(0xE10A); send_data(0x0001);
	send_cmd(0xE10B); send_data(0x00ED);
	send_cmd(0xE10C); send_data(0x0002);
	send_cmd(0xE10D); send_data(0x001E);
	send_cmd(0xE10E); send_data(0x0002);
	send_cmd(0xE10F); send_data(0x0051);
	send_cmd(0xE200); send_data(0x0002);
	send_cmd(0xE201); send_data(0x006C);
	send_cmd(0xE202); send_data(0x0002);
	send_cmd(0xE203); send_data(0x008D);
	send_cmd(0xE204); send_data(0x0002);
	send_cmd(0xE205); send_data(0x00A5);
	send_cmd(0xE206); send_data(0x0002);
	send_cmd(0xE207); send_data(0x00C9);
	send_cmd(0xE208); send_data(0x0002);
	send_cmd(0xE209); send_data(0x00EA);
	send_cmd(0xE20A); send_data(0x0003);
	send_cmd(0xE20B); send_data(0x0019);
	send_cmd(0xE20C); send_data(0x0003);
	send_cmd(0xE20D); send_data(0x0045);
	send_cmd(0xE20E); send_data(0x0003);
	send_cmd(0xE20F); send_data(0x007A);
	send_cmd(0xE300); send_data(0x0003);
	send_cmd(0xE301); send_data(0x00A0);
	send_cmd(0xE302); send_data(0x0003);
	send_cmd(0xE303); send_data(0x00F4);

	/*Negae Gamma for GERREN*/
	send_cmd(0xE400); send_data(0x0000);
	send_cmd(0xE401); send_data(0x0032);
	send_cmd(0xE402); send_data(0x0000);
	send_cmd(0xE403); send_data(0x0041);
	send_cmd(0xE404); send_data(0x0000);
	send_cmd(0xE405); send_data(0x0054);
	send_cmd(0xE406); send_data(0x0000);
	send_cmd(0xE407); send_data(0x0067);
	send_cmd(0xE408); send_data(0x0000);
	send_cmd(0xE409); send_data(0x007A);
	send_cmd(0xE40A); send_data(0x0000);
	send_cmd(0xE40B); send_data(0x0098);
	send_cmd(0xE40C); send_data(0x0000);
	send_cmd(0xE40D); send_data(0x00B0);
	send_cmd(0xE40E); send_data(0x0000);
	send_cmd(0xE40F); send_data(0x00DB);
	send_cmd(0xE500); send_data(0x0001);
	send_cmd(0xE501); send_data(0x0001);
	send_cmd(0xE502); send_data(0x0001);
	send_cmd(0xE503); send_data(0x003F);
	send_cmd(0xE504); send_data(0x0001);
	send_cmd(0xE505); send_data(0x0070);
	send_cmd(0xE506); send_data(0x0001);
	send_cmd(0xE507); send_data(0x00B4);
	send_cmd(0xE508); send_data(0x0001);
	send_cmd(0xE509); send_data(0x00EC);
	send_cmd(0xE50A); send_data(0x0001);
	send_cmd(0xE50B); send_data(0x00ED);
	send_cmd(0xE50C); send_data(0x0002);
	send_cmd(0xE50D); send_data(0x001E);
	send_cmd(0xE50E); send_data(0x0002);
	send_cmd(0xE50F); send_data(0x0051);
	send_cmd(0xE600); send_data(0x0002);
	send_cmd(0xE601); send_data(0x006C);
	send_cmd(0xE602); send_data(0x0002);
	send_cmd(0xE603); send_data(0x008D);
	send_cmd(0xE604); send_data(0x0002);
	send_cmd(0xE605); send_data(0x00A5);
	send_cmd(0xE606); send_data(0x0002);
	send_cmd(0xE607); send_data(0x00C9);
	send_cmd(0xE608); send_data(0x0002);
	send_cmd(0xE609); send_data(0x00EA);
	send_cmd(0xE60A); send_data(0x0003);
	send_cmd(0xE60B); send_data(0x0019);
	send_cmd(0xE60C); send_data(0x0003);
	send_cmd(0xE60D); send_data(0x0045);
	send_cmd(0xE60E); send_data(0x0003);
	send_cmd(0xE60F); send_data(0x007A);
	send_cmd(0xE700); send_data(0x0003);
	send_cmd(0xE701); send_data(0x00A0);
	send_cmd(0xE702); send_data(0x0003);
	send_cmd(0xE703); send_data(0x00F4);

	/*Negae Gamma for BLUE*/
	send_cmd(0xE800); send_data(0x0000);
	send_cmd(0xE801); send_data(0x0032);
	send_cmd(0xE802); send_data(0x0000);
	send_cmd(0xE803); send_data(0x0041);
	send_cmd(0xE804); send_data(0x0000);
	send_cmd(0xE805); send_data(0x0054);
	send_cmd(0xE806); send_data(0x0000);
	send_cmd(0xE807); send_data(0x0067);
	send_cmd(0xE808); send_data(0x0000);
	send_cmd(0xE809); send_data(0x007A);
	send_cmd(0xE80A); send_data(0x0000);
	send_cmd(0xE80B); send_data(0x0098);
	send_cmd(0xE80C); send_data(0x0000);
	send_cmd(0xE80D); send_data(0x00B0);
	send_cmd(0xE80E); send_data(0x0000);
	send_cmd(0xE80F); send_data(0x00DB);
	send_cmd(0xE900); send_data(0x0001);
	send_cmd(0xE901); send_data(0x0001);
	send_cmd(0xE902); send_data(0x0001);
	send_cmd(0xE903); send_data(0x003F);
	send_cmd(0xE904); send_data(0x0001);
	send_cmd(0xE905); send_data(0x0070);
	send_cmd(0xE906); send_data(0x0001);
	send_cmd(0xE907); send_data(0x00B4);
	send_cmd(0xE908); send_data(0x0001);
	send_cmd(0xE909); send_data(0x00EC);
	send_cmd(0xE90A); send_data(0x0001);
	send_cmd(0xE90B); send_data(0x00ED);
	send_cmd(0xE90C); send_data(0x0002);
	send_cmd(0xE90D); send_data(0x001E);
	send_cmd(0xE90E); send_data(0x0002);
	send_cmd(0xE90F); send_data(0x0051);
	send_cmd(0xEA00); send_data(0x0002);
	send_cmd(0xEA01); send_data(0x006C);
	send_cmd(0xEA02); send_data(0x0002);
	send_cmd(0xEA03); send_data(0x008D);
	send_cmd(0xEA04); send_data(0x0002);
	send_cmd(0xEA05); send_data(0x00A5);
	send_cmd(0xEA06); send_data(0x0002);
	send_cmd(0xEA07); send_data(0x00C9);
	send_cmd(0xEA08); send_data(0x0002);
	send_cmd(0xEA09); send_data(0x00EA);
	send_cmd(0xEA0A); send_data(0x0003);
	send_cmd(0xEA0B); send_data(0x0019);
	send_cmd(0xEA0C); send_data(0x0003);
	send_cmd(0xEA0D); send_data(0x0045);
	send_cmd(0xEA0E); send_data(0x0003);
	send_cmd(0xEA0F); send_data(0x007A);
	send_cmd(0xEB00); send_data(0x0003);
	send_cmd(0xEB01); send_data(0x00A0);
	send_cmd(0xEB02); send_data(0x0003);
	send_cmd(0xEB03); send_data(0x00F4);

	send_cmd(0x3A00); send_data(0x0007);

	send_cmd(0x3500); send_data(0x0000);

	send_cmd(0x1100); /* Sleep out*/
	LCD_DelayMS(120);
	send_cmd(0x2900); /* Display On*/

	send_cmd(0x2900); /* Display On*/
	return 0;
}

static int32_t nt35516_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	return 0;

	if(is_sleep==1){
	send_cmd(0x2800);
	LCD_DelayMS(200);
	send_cmd(0x1000);
	LCD_DelayMS(200);
	/*Lcd_EnvidOnOff(0);*//*RGB TIMENG OFF*/
	 /*LCD_Delay(200);*/
	}else{
	/*Lcd_EnvidOnOff(1);*//*RGB TIMENG ON*/
	/*LCD_Delay(200);*/
	/*LCDinit_TFT();*/
	/*LCD_Delay(200);*/
	 }

	return 0;
}




static int32_t nt35516_set_window(struct panel_spec *self,
		uint16_t left, uint16_t top, uint16_t right, uint16_t bottom)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	pr_debug("nt35516_set_window: %d, %d, %d, %d\n",left, top, right, bottom);

	send_cmd(0x2A00); send_data((left>>8));// set left address
	send_cmd(0x2A01); send_data((left&0xff));
	send_cmd(0x2A02); send_data((right>>8));// set right address
	send_cmd(0x2A03); send_data((right&0xff));

	send_cmd(0x2B00); send_data((top>>8));// set left address
	send_cmd(0x2B01); send_data((top&0xff));
	send_cmd(0x2B02); send_data((bottom>>8));// set bottom address
	send_cmd(0x2B03); send_data((bottom&0xff));

	send_cmd(0x2C00);

	return 0;
}
static int32_t nt35516_invalidate(struct panel_spec *self)
{
	pr_debug("nt35516_invalidate\n");

	return self->ops->panel_set_window(self, 0, 0,
		self->width - 1, self->height - 1);
}



static int32_t nt35516_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{
	pr_debug("nt35516_invalidate_rect \n");

	return self->ops->panel_set_window(self, left, top,
			right, bottom);
}

static uint32_t nt35516_read_id(struct panel_spec *self)
{
	int32_t id  = 0;
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	read_data_t read_data = self->info.mcu->ops->read_data;
	send_data_t send_data = self->info.mcu->ops->send_data;

	//Get ID
	send_cmd(0xF000); send_data(0x0055); /*Manufacture Command Set Control */
	send_cmd(0xF001); send_data(0x00AA);
	send_cmd(0xF002); send_data(0x0052);
	send_cmd(0xF003); send_data(0x0008);
	send_cmd(0xF004); send_data(0x0001);/*Page1*/

	send_cmd(0xc501); //0xc500
	id = read_data();

	return id;
}

static struct panel_operations lcd_nt35516_operations = {
	.panel_init = nt35516_init,
	.panel_set_window = nt35516_set_window,
	.panel_invalidate_rect= nt35516_invalidate_rect,
	.panel_invalidate = nt35516_invalidate,
	.panel_enter_sleep = nt35516_enter_sleep,
	.panel_readid          = nt35516_read_id
};

static struct timing_mcu lcd_nt35516_timing[] = {
[MCU_LCD_REGISTER_TIMING] = {                    // read/write register timing
		.rcss = 15,  // 15ns
		.rlpw = 60,
		.rhpw = 60,
		.wcss = 10,
		.wlpw = 35,
		.whpw = 35,
	},
[MCU_LCD_GRAM_TIMING] = {                    // read/write gram timing
		.rcss = 15,  // 15ns
		.rlpw = 60,
		.rhpw = 60,
		.wcss = 0,
		.wlpw = 16,
		.whpw = 16,
	},
};

static struct info_mcu lcd_nt35516_info = {
	.bus_mode = LCD_BUS_8080,
	.bus_width = 24,
	.bpp = 24, /*RGB88*/
	.timing = lcd_nt35516_timing,
	.ops = NULL,
};

//lcd_panel_nt35516
struct panel_spec lcd_nt35516_spec = {
	.width = 540,
	.height = 960,
	.type = LCD_MODE_MCU,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.mcu = &lcd_nt35516_info
	},
	.ops = &lcd_nt35516_operations,
};

struct panel_cfg lcd_nt35516 = {
	/* this panel may on both CS0/1 */
	.dev_id = SPRDFB_UNDEFINELCD_ID,
	.lcd_id = 0x16,
	.lcd_name = "lcd_nt35516",
	.panel = &lcd_nt35516_spec,
};

static int __init lcd_nt35516_init(void)
{
	return sprdfb_panel_register(&lcd_nt35516);
}

subsys_initcall(lcd_nt35516_init);
