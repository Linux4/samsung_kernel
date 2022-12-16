/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef _MDNIE_LITE_TUNING_DATA_H_
#define _MDNIE_LITE_TUNING_DATA_H_
////////////////// UI /// /////////////////////

static char STANDARD_UI_1[] = {
};


static char DYNAMIC_UI_1[] = {
};


static char MOVIE_UI_1[] = {
};


char AUTO_UI_1[] = {
};




////////////////// GALLERY /////////////////////
static char STANDARD_GALLERY_1[] = {
};


static char DYNAMIC_GALLERY_1[] = {
};


static char MOVIE_GALLERY_1[] = {
};


char AUTO_GALLERY_1[] = {
};




////////////////// VIDEO /////////////////////

static char STANDARD_VIDEO_1[] = {
};


static char DYNAMIC_VIDEO_1[] = {
};


static char MOVIE_VIDEO_1[] = {
};


char AUTO_VIDEO_1[] = {
};




////////////////// VT /////////////////////

static char STANDARD_VT_1[] = {
};


static char DYNAMIC_VT_1[] = {
};


static char MOVIE_VT_1[] = {
};


char AUTO_VT_1[] = {
};




////////////////// CAMERA /////////////////////

static char CAMERA_1[] = {
};


char AUTO_CAMERA_1[] = {
};




static char NEGATIVE_1[] = {
};


char COLOR_BLIND_1[] = {
};




////////////////// BROWSER /////////////////////

char STANDARD_BROWSER_1[] = {
};


char DYNAMIC_BROWSER_1[] = {
};


char MOVIE_BROWSER_1[] = {
};


char AUTO_BROWSER_1[] = {
};



////////////////// eBOOK /////////////////////

char AUTO_EBOOK_1[] = {
};


char AUTO_EMAIL_1[] = {
};



char *blind_tune_value[ACCESSIBILITY_MAX][1] = {
		/*
			ACCESSIBILITY_OFF,
			NEGATIVE,
			COLOR_BLIND,
		*/
		{NULL, NULL, NULL, NULL},
		{NEGATIVE_1, NEGATIVE_2, NEGATIVE_3, NEGATIVE_4},
		{COLOR_BLIND_1, COLOR_BLIND_2, COLOR_BLIND_3, COLOR_BLIND_4},
};

char *mdnie_tune_value[MAX_mDNIe_MODE][MAX_BACKGROUND_MODE][MAX_OUTDOOR_MODE][1] = {
		/*
			DYNAMIC_MODE (outdoor off/on)
			STANDARD_MODE (outdoor off/on)
			NATURAL_MODE (outdoor off/on)
			MOVIE_MODE (outdoor off/on)
			AUTO_MODE (outdoor off/on)
		*/
		// UI_APP
		{
			{{DYNAMIC_UI_1, DYNAMIC_UI_2, DYNAMIC_UI_3, DYNAMIC_UI_4}, {NULL, NULL, NULL, NULL}},
			{{STANDARD_UI_1, STANDARD_UI_2, STANDARD_UI_3, STANDARD_UI_4}, {NULL, NULL, NULL, NULL}},
			{{MOVIE_UI_1, MOVIE_UI_2, MOVIE_UI_3, MOVIE_UI_4}, {NULL, NULL, NULL, NULL}},
			{{AUTO_UI_1, AUTO_UI_2, AUTO_UI_3, AUTO_UI_4}, {NULL, NULL, NULL, NULL}},
		},
		// VIDEO_APP
		{
			{{DYNAMIC_VIDEO_1, DYNAMIC_VIDEO_2, DYNAMIC_VIDEO_3, DYNAMIC_VIDEO_4}, {NULL, NULL, NULL, NULL}},
			{{STANDARD_VIDEO_1, STANDARD_VIDEO_2, STANDARD_VIDEO_3, STANDARD_VIDEO_4}, {NULL, NULL, NULL, NULL}},
			{{MOVIE_VIDEO_1, MOVIE_VIDEO_2, MOVIE_VIDEO_3, MOVIE_VIDEO_4}, {NULL, NULL, NULL, NULL}},
			{{AUTO_VIDEO_1, AUTO_VIDEO_2, AUTO_VIDEO_3, AUTO_VIDEO_4}, {NULL, NULL, NULL, NULL}},
		},
		// VIDEO_WARM_APP
		{
			{{NULL, NULL, NULL, NULL}, {NULL, NULL, NULL, NULL}},
			{{NULL, NULL, NULL, NULL}, {NULL, NULL, NULL, NULL}},
			{{NULL, NULL, NULL, NULL}, {NULL, NULL, NULL, NULL}},
			{{NULL, NULL, NULL, NULL}, {NULL, NULL, NULL, NULL}},
		},
		// VIDEO_COLD_APP
		{
			{{NULL, NULL, NULL, NULL}, {NULL, NULL, NULL, NULL}},
			{{NULL, NULL, NULL, NULL}, {NULL, NULL, NULL, NULL}},
			{{NULL, NULL, NULL, NULL}, {NULL, NULL, NULL, NULL}},
			{{NULL, NULL, NULL, NULL}, {NULL, NULL, NULL, NULL}},

		},
		// CAMERA_APP
		{
			{{CAMERA_1, CAMERA_2, CAMERA_3, CAMERA_4}, {NULL, NULL, NULL, NULL}},
			{{CAMERA_1, CAMERA_2, CAMERA_3, CAMERA_4}, {NULL, NULL, NULL, NULL}},
			{{CAMERA_1, CAMERA_2, CAMERA_3, CAMERA_4}, {NULL, NULL, NULL, NULL}},
			{{AUTO_CAMERA_1, AUTO_CAMERA_2, AUTO_CAMERA_3, AUTO_CAMERA_4}, {NULL, NULL, NULL, NULL}},
		},
		// NAVI_APP
		{
			{{NULL, NULL, NULL, NULL}, {NULL, NULL, NULL, NULL}},
			{{NULL, NULL, NULL, NULL}, {NULL, NULL, NULL, NULL}},
			{{NULL, NULL, NULL, NULL}, {NULL, NULL, NULL, NULL}},
			{{NULL, NULL, NULL, NULL}, {NULL, NULL, NULL, NULL}},
		},
		// GALLERY_APP
		{
			{{DYNAMIC_GALLERY_1, DYNAMIC_GALLERY_2, DYNAMIC_GALLERY_3, DYNAMIC_GALLERY_4}, {NULL, NULL, NULL, NULL}},
			{{STANDARD_GALLERY_1, STANDARD_GALLERY_2, STANDARD_GALLERY_3, STANDARD_GALLERY_4}, {NULL, NULL, NULL, NULL}},
			{{MOVIE_GALLERY_1, MOVIE_GALLERY_2, MOVIE_GALLERY_3, MOVIE_GALLERY_4}, {NULL, NULL, NULL, NULL}},
			{{AUTO_GALLERY_1, AUTO_GALLERY_2, AUTO_GALLERY_3, AUTO_GALLERY_4}, {NULL, NULL, NULL, NULL}},
		},
		// VT_APP
		{
			{{DYNAMIC_VT_1, DYNAMIC_VT_2, DYNAMIC_VT_3, DYNAMIC_VT_4}, {NULL, NULL, NULL, NULL}},
			{{STANDARD_VT_1, STANDARD_VT_2, STANDARD_VT_3, STANDARD_VT_4}, {NULL, NULL, NULL, NULL}},
			{{MOVIE_VT_1, MOVIE_VT_2, MOVIE_VT_3, MOVIE_VT_4}, {NULL, NULL, NULL, NULL}},
			{{AUTO_VT_1, AUTO_VT_2, AUTO_VT_3, AUTO_VT_4}, {NULL, NULL, NULL, NULL}},
		},
		// BROWSER_APP
		{
			{{DYNAMIC_BROWSER_1, DYNAMIC_BROWSER_2, DYNAMIC_BROWSER_3, DYNAMIC_BROWSER_4}, {NULL, NULL, NULL, NULL}},
			{{STANDARD_BROWSER_1, STANDARD_BROWSER_2, STANDARD_BROWSER_3, STANDARD_BROWSER_4}, {NULL, NULL, NULL, NULL}},
			{{MOVIE_BROWSER_1, MOVIE_BROWSER_2, MOVIE_BROWSER_3, MOVIE_BROWSER_4}, {NULL, NULL, NULL, NULL}},
			{{AUTO_BROWSER_1, AUTO_BROWSER_2, AUTO_BROWSER_3, AUTO_BROWSER_4}, {NULL, NULL, NULL, NULL}},
		},
		// eBOOK_APP
		{
			{{DYNAMIC_UI_1, DYNAMIC_UI_2, DYNAMIC_UI_3, DYNAMIC_UI_4}, {NULL, NULL, NULL, NULL}},
			{{STANDARD_UI_1, STANDARD_UI_2, STANDARD_UI_3, STANDARD_UI_4}, {NULL, NULL, NULL, NULL}},
			{{MOVIE_UI_1, MOVIE_UI_2, MOVIE_UI_3, MOVIE_UI_4}, {NULL, NULL, NULL, NULL}},
			{{AUTO_EBOOK_1, AUTO_EBOOK_2, AUTO_EBOOK_3, AUTO_EBOOK_4}, {NULL, NULL, NULL, NULL}},
		},
		// EMAIL_APP
		{
			{{AUTO_EMAIL_1, AUTO_EMAIL_2, AUTO_EMAIL_3, AUTO_EMAIL_4}, {NULL, NULL, NULL, NULL}},
			{{AUTO_EMAIL_1, AUTO_EMAIL_2, AUTO_EMAIL_3, AUTO_EMAIL_4}, {NULL, NULL, NULL, NULL}},
			{{AUTO_EMAIL_1, AUTO_EMAIL_2, AUTO_EMAIL_3, AUTO_EMAIL_4}, {NULL, NULL, NULL, NULL}},
			{{AUTO_EMAIL_1, AUTO_EMAIL_2, AUTO_EMAIL_3, AUTO_EMAIL_4}, {NULL, NULL, NULL, NULL}},
		},
};
#endif
