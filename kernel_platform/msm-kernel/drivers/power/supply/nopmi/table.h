/*****************************************************************************
* Copyright(c) O2Micro, 2014. All rights reserved.
*
* O2Micro battery gauge driver
* File: Phenix_Q6008_3486mAhr.h
*
* $Source: /data/code/CVS
* $Revision: 4.00.01 $
*
* This program is free software and can be edistributed and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This Source Code Reference Design for O2MICRO Battery Gauge access (\u201cReference Design\u201d)
* is sole for the use of PRODUCT INTEGRATION REFERENCE ONLY, and contains confidential
* and privileged information of O2Micro International Limited. O2Micro shall have no
* liability to any PARTY FOR THE RELIABILITY, SERVICEABILITY FOR THE RESULT OF PRODUCT
* INTEGRATION, or results from: (i) any modification or attempted modification of the
* Reference Design by any party, or (ii) the combination, operation or use of the
* Reference Design with non-O2Micro Reference Design.
*
* Battery Manufacture: LQ
* Battery Model: LQM269_GB526
* Absolute Max Capacity(mAhr): 5044
* Limited Charge Voltage(mV): 4400
* Cutoff Discharge Voltage(mV): 3100
* Equipment: "JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","Chroma17010","Chroma17010","Chroma17010","Chroma17010","Chroma17010","Chroma17010","Chroma17010","Chroma17010","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY"
* Tester: "CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD"
* Battery ID: "LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ"
* Version = V00.B6.00
* Date = 2023.5.9
* Comment =
*           
*****************************************************************************/

#ifndef _TABLE_H_
#define _TABLE_H_

/****************************************************************************
* Struct section
*  add struct #define here if any
***************************************************************************/
typedef struct tag_one_latitude_data {
 	 int32_t 			 x;//
 	 int32_t 			 y;//
} one_latitude_data_t;

enum battery_type {
	BATTERY_MODEL_MAIN,
	BATTERY_MODEL_SECOND,
	BATTERY_MODEL_UNKNOWN
} ;

#define OCV_DATA_NUM 	 65
//#define CHARGE_DATA_NUM 		 65

#define XAxis 		 83
#define YAxis 		 8
#define ZAxis 		 7

#define BATTERY_ID_NUM 		 58

#define BATTERY_DESIGN_CAPACITY_MAIN	    5044
#define BATTERY_CV_MAIN			4400
/****************************************************************************
* extern variable declaration section
***************************************************************************/
extern const char *table_version;
extern const char *battery_id[BATTERY_ID_NUM];
extern one_latitude_data_t ocv_data[OCV_DATA_NUM];
//extern one_latitude_data_t	charge_data[CHARGE_DATA_NUM];
extern int32_t	XAxisElement[XAxis];
extern int32_t	YAxisElement[YAxis];
extern int32_t	ZAxisElement[ZAxis];
extern int32_t	RCtable[YAxis*ZAxis][XAxis];

/*****************************************************************************
* Copyright(c) O2Micro, 2014. All rights reserved.
*
* O2Micro battery gauge driver
* File: table.h
*
* $Source: /data/code/CVS
* $Revision: 4.00.01 $
*
* This program is free software and can be edistributed and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This Source Code Reference Design for O2MICRO Battery Gauge access (\u201cReference Design\u201d) 
* is sole for the use of PRODUCT INTEGRATION REFERENCE ONLY, and contains confidential 
* and privileged information of O2Micro International Limited. O2Micro shall have no 
* liability to any PARTY FOR THE RELIABILITY, SERVICEABILITY FOR THE RESULT OF PRODUCT 
* INTEGRATION, or results from: (i) any modification or attempted modification of the 
* Reference Design by any party, or (ii) the combination, operation or use of the 
* Reference Design with non-O2Micro Reference Design.
*
* Battery Manufacture: LQ
* Battery Model: LQM269_CSL526
* Absolute Max Capacity(mAhr): 5129
* Limited Charge Voltage(mV): 4400
* Cutoff Discharge Voltage(mV): 3100
* Equipment: "JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","Chroma17010","Chroma17010","Chroma17010","Chroma17010","Chroma17010","Chroma17010","Chroma17010","Chroma17010","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY","JY"
* Tester: "CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD","CD"
* Battery ID: "LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ","LQ"
* Version = V00.B8.00
* Date = 2023.5.9
* Comment =
*           
*****************************************************************************/
#define OCV_DATA_NUM_SECOND 	 65
//#define CHARGE_DATA_NUM_SECOND 		 65

#define XAxis_SECOND 		 83
#define YAxis_SECOND 		 8
#define ZAxis_SECOND 		 7

#define BATTERY_ID_NUM_SECOND 		 58

#define BATTERY_DESIGN_CAPACITY_SECOND	5129
#define BATTERY_CV_SECOND			4400

/****************************************************************************
* extern variable declaration section
***************************************************************************/
extern const char *table_version_second;
extern const char *battery_id_second[BATTERY_ID_NUM_SECOND] ;
extern one_latitude_data_t ocv_data_second[OCV_DATA_NUM_SECOND];
//extern one_latitude_data_t	charge_data_second[CHARGE_DATA_NUM_SECOND];
extern int32_t	XAxisElement_second[XAxis_SECOND];
extern int32_t	YAxisElement_second[YAxis_SECOND];
extern int32_t	ZAxisElement_second[ZAxis_SECOND];
extern int32_t	RCtable_second[YAxis_SECOND*ZAxis_SECOND][XAxis_SECOND];
#endif

