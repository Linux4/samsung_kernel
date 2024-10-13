/*****************************************************************************
* Copyright(c) BMT, 2021. All rights reserved.
*       
* BMT [oz8806] Source Code Reference Design
* File:[bmulib.c]
*       
* This Source Code Reference Design for BMT [oz8806] access 
* ("Reference Design") is solely for the use of PRODUCT INTEGRATION REFERENCE ONLY, 
* and contains confidential and privileged information of BMT International 
* Limited. BMT shall have no liability to any PARTY FOR THE RELIABILITY, 
* SERVICEABILITY FOR THE RESULT OF PRODUCT INTEGRATION, or results from: (i) any 
* modification or attempted modification of the Reference Design by any party, or 
* (ii) the combination, operation or use of the Reference Design with non-BMT 
* Reference Design. Use of the Reference Design is at user's discretion to qualify 
* the final work result.
*****************************************************************************/
#include <linux/kernel.h>

#include "parameter.h"
#include "table.h"


uint32_t calculate_version = 0x20210824;

uint16_t _b8207028918cf67b818b933cec55307f = 1;
uint16_t _9fdd6f346013b8418267169afad72f39 = 1;
uint16_t _5e42b9ba1b3fed25687bccaa5c7b0721 = 5;
uint16_t _c577340b7519f6dbfbaef3050b811bfc = 9; 
uint16_t _f3e83602c93d1a9ad346ade68b5191ab = 15;
uint16_t _1194a93f1b0bda9f2e5774719c508b0f = 20; 
uint16_t _f9839088a030a3cf3b1971064986f69c = 100; 

static int _d0be2b3e92e7dfacbc78a82184fda19d(int input_data);
static int _2736028ff10591dfcc7c7f7b0a2b1cbf(int input_data);
static void _f18c3f29d0ea6c42e9b2a70a25a267d6(void);
static void _65d9b46fdcc8434db6b550341c6ffd42(void);
static int _222e4f4be6fa6a344a8cb15fa11806fe(void);

static int _d0be2b3e92e7dfacbc78a82184fda19d(int input_data)
{
    int cc = input_data / 2;	//
    int jj = input_data + 9;	//
    int hh = input_data * 7;    //

    int nn = cc - 13;	//
    int pp = jj / 15;	//
    int nnn = hh - 13;	//
    int ff = cc + 5;	//
    int oo = jj * 14;	//
    int uu = hh - 20;	//

    return cc + jj + hh + nn + pp + nnn + ff + oo + uu;
}

static int _2736028ff10591dfcc7c7f7b0a2b1cbf(int input_data)
{
    uint32_t bb = input_data +12; //
    uint32_t a = input_data *4;
    uint32_t c = 0;
    uint32_t i = 0;

    for(i=1; i<=input_data; i++)
    {
        c *= a;
    }

    bb = (bb>>3);
    a <<= 4;
    c <<= 6;

    return bb + a + c;
}

static void _f18c3f29d0ea6c42e9b2a70a25a267d6(void)
{
    int i,j,flag,n;
    n = 10000;   
    flag = 1; 
 
    for(i = 2; i <= 10000; i++)  
    {
        flag = 1;
        for(j = 2; j*j <= i; j++) 
        {
            if(i % j == 0)
            {
                flag = 0;
                break;
            }
        }
    }
}

static void _65d9b46fdcc8434db6b550341c6ffd42(void)
{
    char c[35] = "65d9b46fdcc8434db6b550341c6ffd42";
    int letters=0,space=0,digit=0,others=0;
    int i = 0;

    while(c[i]!='\n')
    {
        if((c[i]>='a' && c[i]<='z') || (c[i]>='A' && c[i]<='Z'))
            letters++;
    else if(c[i]==' ')
        space++;
    else if(c[i]>='0'&&c[i]<='9')
        digit++;
    else
        others++;

    i++;
    }
}

static int _222e4f4be6fa6a344a8cb15fa11806fe(void)
{
    int i,j,k=0,s;
    int sum;
    int a[100]={0};
    for(i=2;i<1000;i++)
    {
        sum=0;k=0;
        for(j=1;j<i;j++)
        {
            if(i%j==0) 
            {
                a[k++]=j;
            }   
        }
        for(j=0;j<k;j++)
        sum+=a[j];
        if(sum==i) 
        {
            for(s=0;s<k-1;s++)
               sum += a[s];
        }   
    }
    
    return sum;
} 

void age_update(void)
{

    if(_b8207028918cf67b818b933cec55307f == 1)
    {
        if((cycle_soh == -1) || (age_soh == -1))
        {
            return;
        }
        else
        {
            if(batt_info->fRCPrev > batt_info->fRC)
                accmulate_dsg_data += batt_info->fRCPrev - batt_info->fRC;
            cycle_soh = accmulate_dsg_data / config_data.design_capacity;
            age_soh = 100 - 20 * cycle_soh / 1000;
            if(age_soh < 50)
                age_soh = 50;
        }
	afe_write_cycleCount(cycle_soh);
	afe_write_accmulate_dsg_data(accmulate_dsg_data);
    }
    else if(_b8207028918cf67b818b933cec55307f == 2)
    {
        accmulate_dsg_data += batt_info->fRCPrev - batt_info->fRC;  
        _d0be2b3e92e7dfacbc78a82184fda19d(34);    
        _2736028ff10591dfcc7c7f7b0a2b1cbf(1134);

		_b8207028918cf67b818b933cec55307f = 1;
    }
    else if(_b8207028918cf67b818b933cec55307f == 3)
    {
    	if(age_soh < 50)
        	age_soh = 50;
            
        _d0be2b3e92e7dfacbc78a82184fda19d(44);
        _222e4f4be6fa6a344a8cb15fa11806fe();

		_b8207028918cf67b818b933cec55307f = 4;
    }
    else if(_b8207028918cf67b818b933cec55307f == 4)
    {
            cycle_soh = (accmulate_dsg_data * 100) / config_data.design_capacity;
        age_soh = 100 - 20 * cycle_soh / 100000;
        if(age_soh < 50)
        age_soh = 50;
        
        _d0be2b3e92e7dfacbc78a82184fda19d(38);
        _222e4f4be6fa6a344a8cb15fa11806fe();

		_b8207028918cf67b818b933cec55307f = 3;
    }
    else if(_b8207028918cf67b818b933cec55307f == 5)
    {
            if((cycle_soh == -1) || (age_soh == -1))
        {
            age_soh = 100 - 20 * cycle_soh / 100000;
            if(age_soh < 50)
            age_soh = 50;
        }
        
        _d0be2b3e92e7dfacbc78a82184fda19d(99);
        _65d9b46fdcc8434db6b550341c6ffd42();

		_b8207028918cf67b818b933cec55307f = 4;
    }
}

void accmulate_data_update(void)
{
    if(_9fdd6f346013b8418267169afad72f39 == 1)
    {
        accmulate_dsg_data = batt_info->dsg_data;
    }
    else if(_9fdd6f346013b8418267169afad72f39 == 2)
    {
        accmulate_dsg_data = 100;
        _d0be2b3e92e7dfacbc78a82184fda19d(9);
        _2736028ff10591dfcc7c7f7b0a2b1cbf(6543);

		_9fdd6f346013b8418267169afad72f39 = 3;
    }
    else if(_9fdd6f346013b8418267169afad72f39 == 3)
    {
        accmulate_dsg_data = cycle_soh * config_data.design_capacity;
        _d0be2b3e92e7dfacbc78a82184fda19d(1478);
        _f18c3f29d0ea6c42e9b2a70a25a267d6();
        _65d9b46fdcc8434db6b550341c6ffd42();

		_9fdd6f346013b8418267169afad72f39 = 4;
    }
    else if(_9fdd6f346013b8418267169afad72f39 == 4)
    {
        accmulate_dsg_data =  config_data.design_capacity / 100;
        _d0be2b3e92e7dfacbc78a82184fda19d(999);
        _65d9b46fdcc8434db6b550341c6ffd42();

		_9fdd6f346013b8418267169afad72f39 = 2;
    }
}

int32_t calculate_soc_result(void)
{
    int32_t _c13fbfe70b60448f0c4d931c0ed5ce4e = 0;
    int32_t current_temp = 0;
    int32_t _974abc4b8280dd5eefc77cd62892555e = 0,_974abc4b8280dd5eefc77cd62892555e_end = 0;
    uint8_t rc_result = 0;
    int32_t _020db6490a6d217be29c059f390dedc9 = config_data.discharge_end_voltage;
    int32_t soc_temp = 0;
    int32_t mah_temp = 0;

    if(_5e42b9ba1b3fed25687bccaa5c7b0721 == 5)
    {
    
        if(batt_info->fCurr >= 0)
        {
            _c13fbfe70b60448f0c4d931c0ed5ce4e =  batt_info->fVolt - (gas_gauge->ri+ battery_ri) * batt_info->fCurr / 1000;
            bmt_dbg("current >= 0 infVolt: %d \n",_c13fbfe70b60448f0c4d931c0ed5ce4e);
            soc_temp = one_latitude_table(parameter->ocv_data_num,parameter->ocv,_c13fbfe70b60448f0c4d931c0ed5ce4e);
            mah_temp = soc_temp * config_data.design_capacity / num_100;
            if(mah_temp <= 0)
                mah_temp =  one_percent_rc - 1;
        }
        else
        {
            _c13fbfe70b60448f0c4d931c0ed5ce4e =  batt_info->fVolt - gas_gauge->ri * batt_info->fCurr / 1000;
            current_temp = batt_info->fCurr;

            bmt_dbg("infVolt: %d \n",_c13fbfe70b60448f0c4d931c0ed5ce4e);
            bmt_dbg("current_temp: %d \n",current_temp);

            rc_result = OZ8806_LookUpRCTable(
                        _c13fbfe70b60448f0c4d931c0ed5ce4e,
                        current_temp,
                        batt_info->fCellTemp * 10,
                        &_974abc4b8280dd5eefc77cd62892555e);

            bmt_dbg("result: %d\n",rc_result);
            bmt_dbg("infCal: %d\n",_974abc4b8280dd5eefc77cd62892555e);

            if(_c13fbfe70b60448f0c4d931c0ed5ce4e > xaxis_table[X_AXIS - 1]){
                soc_temp = _974abc4b8280dd5eefc77cd62892555e * 100 / config_data.design_capacity;
                bmt_dbg("vi_soc: %d\n",soc_temp );
                return soc_temp;
            }

            if(!rc_result)
            {
                rc_result = OZ8806_LookUpRCTable(
                                _020db6490a6d217be29c059f390dedc9,
                                current_temp ,
                                batt_info->fCellTemp * 10,
                                &_974abc4b8280dd5eefc77cd62892555e_end);

                bmt_dbg("result: %d\n",rc_result);
                bmt_dbg("end: %d\n",_974abc4b8280dd5eefc77cd62892555e_end);

                _974abc4b8280dd5eefc77cd62892555e = _974abc4b8280dd5eefc77cd62892555e - _974abc4b8280dd5eefc77cd62892555e_end;
                _974abc4b8280dd5eefc77cd62892555e = config_data.design_capacity * _974abc4b8280dd5eefc77cd62892555e / 10000; //remain capacity
                _974abc4b8280dd5eefc77cd62892555e +=     one_percent_rc + 1;    // 1% capacity can't use

                if(_974abc4b8280dd5eefc77cd62892555e <= 0)
                    _974abc4b8280dd5eefc77cd62892555e =     one_percent_rc - 1;

                mah_temp = _974abc4b8280dd5eefc77cd62892555e;
                soc_temp = _974abc4b8280dd5eefc77cd62892555e * 100 / config_data.design_capacity;
            }
        }
        bmt_dbg("vi_mah: %d\n",mah_temp);
        bmt_dbg("vi_soc: %d\n",soc_temp );
    }
    else if(_5e42b9ba1b3fed25687bccaa5c7b0721 == 15)
    {
        if(batt_info->fCurr >= 0)
        {
            _c13fbfe70b60448f0c4d931c0ed5ce4e =  batt_info->fVolt - (gas_gauge->ri+ battery_ri) * batt_info->fCurr / 1000;
            bmt_dbg("current >= infVolt: %d \n",_c13fbfe70b60448f0c4d931c0ed5ce4e);
            soc_temp = one_latitude_table(parameter->ocv_data_num,parameter->ocv,_c13fbfe70b60448f0c4d931c0ed5ce4e);
            mah_temp = soc_temp * config_data.design_capacity / num_100;
            if(mah_temp <= 0) {
                mah_temp = one_percent_rc - 1;
            }
         }
        _d0be2b3e92e7dfacbc78a82184fda19d(3007);
        _222e4f4be6fa6a344a8cb15fa11806fe();

		_5e42b9ba1b3fed25687bccaa5c7b0721 = 14;
    }
    else if(_5e42b9ba1b3fed25687bccaa5c7b0721 == 22)
    {
        if(batt_info->fCurr >= 0)
        {
            _c13fbfe70b60448f0c4d931c0ed5ce4e =  batt_info->fVolt - (gas_gauge->ri+ battery_ri) * batt_info->fCurr / 1000;
            bmt_dbg("current >= infVolt: %d \n",_c13fbfe70b60448f0c4d931c0ed5ce4e);
            soc_temp = one_latitude_table(parameter->ocv_data_num,parameter->ocv,_c13fbfe70b60448f0c4d931c0ed5ce4e);
            mah_temp = soc_temp * config_data.design_capacity / num_100;
            if(mah_temp <= 0)
                mah_temp = one_percent_rc - 1;
        }
        else
        {
            _c13fbfe70b60448f0c4d931c0ed5ce4e =  batt_info->fVolt - gas_gauge->ri * batt_info->fCurr / 1000;
            current_temp = batt_info->fCurr;

            bmt_dbg("infVolt %d \n",_c13fbfe70b60448f0c4d931c0ed5ce4e);
            bmt_dbg("current_temp: %d \n",current_temp);
        }
        
        _d0be2b3e92e7dfacbc78a82184fda19d(147);
        _2736028ff10591dfcc7c7f7b0a2b1cbf(7651);
        _f18c3f29d0ea6c42e9b2a70a25a267d6();

		_5e42b9ba1b3fed25687bccaa5c7b0721 = 67;
    }
    else if(_5e42b9ba1b3fed25687bccaa5c7b0721 == 14)
    {
        _c13fbfe70b60448f0c4d931c0ed5ce4e =  batt_info->fVolt - gas_gauge->ri * batt_info->fCurr / 1000;
        current_temp = batt_info->fCurr;

        bmt_dbg("infVolt: %d \n",_c13fbfe70b60448f0c4d931c0ed5ce4e);
        bmt_dbg("current_temp: %d \n",current_temp);

        rc_result = OZ8806_LookUpRCTable(
                _c13fbfe70b60448f0c4d931c0ed5ce4e,
                current_temp,
                batt_info->fCellTemp * 10,
                &_974abc4b8280dd5eefc77cd62892555e);

        bmt_dbg("result: %d\n",rc_result);
        bmt_dbg("infCal: %d\n",_974abc4b8280dd5eefc77cd62892555e);

        if(_c13fbfe70b60448f0c4d931c0ed5ce4e > xaxis_table[X_AXIS - 1]){
            soc_temp = _974abc4b8280dd5eefc77cd62892555e * 100 / config_data.design_capacity;
            bmt_dbg("vi_soc: %d\n",soc_temp );
            return soc_temp;
        }
        
        _d0be2b3e92e7dfacbc78a82184fda19d(3012);
        _65d9b46fdcc8434db6b550341c6ffd42();

		_5e42b9ba1b3fed25687bccaa5c7b0721 = 145;
    }
    else if(_5e42b9ba1b3fed25687bccaa5c7b0721 == 67)
    {
        if(_c13fbfe70b60448f0c4d931c0ed5ce4e > xaxis_table[X_AXIS - 1]){
            soc_temp = _974abc4b8280dd5eefc77cd62892555e * 100 / config_data.design_capacity;
            bmt_dbg("vi_soc: %d\n",soc_temp );
            return soc_temp;
        }

        if(!rc_result)
        {
            rc_result = OZ8806_LookUpRCTable(
                            _020db6490a6d217be29c059f390dedc9,
                            current_temp ,
                            batt_info->fCellTemp * 10,
                            &_974abc4b8280dd5eefc77cd62892555e_end);

            bmt_dbg("result: %d\n",rc_result);
            bmt_dbg("end: %d\n",_974abc4b8280dd5eefc77cd62892555e_end);

            _974abc4b8280dd5eefc77cd62892555e = _974abc4b8280dd5eefc77cd62892555e - _974abc4b8280dd5eefc77cd62892555e_end;
            _974abc4b8280dd5eefc77cd62892555e = config_data.design_capacity * _974abc4b8280dd5eefc77cd62892555e / 10000; //remain capacity
            _974abc4b8280dd5eefc77cd62892555e += one_percent_rc + 1;    // 1% capacity can't use

            if(_974abc4b8280dd5eefc77cd62892555e <= 0)
                _974abc4b8280dd5eefc77cd62892555e = one_percent_rc - 1;

            mah_temp = _974abc4b8280dd5eefc77cd62892555e;
            soc_temp = _974abc4b8280dd5eefc77cd62892555e * 100 / config_data.design_capacity;
        }
        
        _d0be2b3e92e7dfacbc78a82184fda19d(302);
        _222e4f4be6fa6a344a8cb15fa11806fe();

		_5e42b9ba1b3fed25687bccaa5c7b0721 = 5;
    }
    else if(_5e42b9ba1b3fed25687bccaa5c7b0721 == 145)
    {
        _c13fbfe70b60448f0c4d931c0ed5ce4e =  batt_info->fVolt - (gas_gauge->ri+ battery_ri) * batt_info->fCurr / 1000;
        bmt_dbg("current >= 0 infVolt: %d \n",_c13fbfe70b60448f0c4d931c0ed5ce4e);
        soc_temp = one_latitude_table(parameter->ocv_data_num,parameter->ocv,_c13fbfe70b60448f0c4d931c0ed5ce4e);
        mah_temp = soc_temp * config_data.design_capacity / num_100;
        if(mah_temp <= 0)
            mah_temp = one_percent_rc - 1;
        _974abc4b8280dd5eefc77cd62892555e = _974abc4b8280dd5eefc77cd62892555e - _974abc4b8280dd5eefc77cd62892555e_end;
        _974abc4b8280dd5eefc77cd62892555e = config_data.design_capacity * _974abc4b8280dd5eefc77cd62892555e / 10000; //remain capacity
        _974abc4b8280dd5eefc77cd62892555e += one_percent_rc + 1;    // 1% capacity can't use

        if(_974abc4b8280dd5eefc77cd62892555e <= 0)
        _974abc4b8280dd5eefc77cd62892555e = one_percent_rc - 1;

        mah_temp = _974abc4b8280dd5eefc77cd62892555e;
            soc_temp = _974abc4b8280dd5eefc77cd62892555e * 100 / config_data.design_capacity;
        
        _d0be2b3e92e7dfacbc78a82184fda19d(847);
        _f18c3f29d0ea6c42e9b2a70a25a267d6();
        _65d9b46fdcc8434db6b550341c6ffd42();

		_5e42b9ba1b3fed25687bccaa5c7b0721 = 5;
    }

    return soc_temp;
}

int32_t one_latitude_table(int32_t number,one_latitude_data_t *data,int32_t value)
{
    int32_t j = 0;
    int32_t res = 0;

    if(_c577340b7519f6dbfbaef3050b811bfc == 9)
    {
        for (j = num_0;j < number;j++)
        {
            if (data[j].x ==value)
            {
                res = data[j].y;
                return res;
            }
            if(data[j].x > value)
                break;
        }

        if(j == num_0)
            res = data[j].y;
        else if(j == number)
            res = data[j -num_1].y;
        else
        {
            res = ((value - data[j -num_1].x) * (data[j].y - data[j -num_1].y));
            
            if((data[j].x - data[j -num_1].x) != num_0)
                res = res / (data[j].x  - data[j-num_1].x );
            res += data[j-num_1].y;
        }
    }
    else if(_c577340b7519f6dbfbaef3050b811bfc == 45)
    {
        for (j = num_0;j < number;j++)
        {
            if (data[j].x ==value)
            {
                res = data[j].y;
                return res;
            }
            if(data[j].x > value)
                break;
        }

        if(j == num_0)
            res = data[j].y;
        _d0be2b3e92e7dfacbc78a82184fda19d(92);
        _2736028ff10591dfcc7c7f7b0a2b1cbf(543);

		_c577340b7519f6dbfbaef3050b811bfc = 3;
    }
    else if(_c577340b7519f6dbfbaef3050b811bfc == 39)
    {
        for (j = num_0;j < number;j++){}
       
  
        if(j == num_0)
            res = data[j].y;
        else if(j == number)
            res = data[j -num_1].y;
        else
        {
            res = ((value - data[j -num_1].x) * (data[j].y - data[j -num_1].y));
            
            if((data[j].x - data[j -num_1].x) != num_0)
                res = res / (data[j].x  - data[j-num_1].x );
            res += data[j-num_1].y;
        }
        
        _d0be2b3e92e7dfacbc78a82184fda19d(654);
        _f18c3f29d0ea6c42e9b2a70a25a267d6();

		_c577340b7519f6dbfbaef3050b811bfc = 12;
    }
    else if(_c577340b7519f6dbfbaef3050b811bfc == 12)
    {
        for (j = num_0;j < number;j++)
        {
            if (data[j].x ==value)
            {
                res = data[j].y;
                return res;
            }
        }
        res = ((value - data[j -num_1].x) * (data[j].y - data[j -num_1].y));
        
        _d0be2b3e92e7dfacbc78a82184fda19d(3874);
        _65d9b46fdcc8434db6b550341c6ffd42();

		_c577340b7519f6dbfbaef3050b811bfc = 39;
    }
    else if(_c577340b7519f6dbfbaef3050b811bfc == 3)
    {
        for (j = num_0;j < number;j++) 
        {
            if(j == num_0)
                res = data[j].y;
            else if(j == number)
                res = data[j -num_1].y;
            else
            {
                res = ((value - data[j -num_1].x) * (data[j].y - data[j -num_1].y));
            }
        }
        
        _d0be2b3e92e7dfacbc78a82184fda19d(354);
        _222e4f4be6fa6a344a8cb15fa11806fe();

		_c577340b7519f6dbfbaef3050b811bfc = 39;
    }

    return res;
}

uint8_t OZ8806_LookUpRCTable(int32_t _c13fbfe70b60448f0c4d931c0ed5ce4e,int32_t _bac5cf2506ca2fd3c3349a84a27a630e, int32_t _c19b68bf15fb9e7171a7e04be2d6728f, int32_t *_974abc4b8280dd5eefc77cd62892555e)
{
    int32_t    _bef3223f9aea5bd1e9229ecad7e3b4a9 = 0, _bef15de250084adfa7d561d640b78d97 = 0, _885f263491149f5890ec4ccc95198024 = 0;
    long    _fe77ce2b5015a9378c6d2f4d5eccd876 = 0, _9450a86c42296bb7fd85f4a38d8a7a35 = 0, _485ceaf01c6d67a62bf9682eee3cf908 = 0, _d5397a2a4f251ddc37970747b1af4800 = 0, _36b2c12ed0bd6e4891982138372efa91 = 0;
    long    ftemp_x = 0, ftemp_y = 0, ftemp_z = 0;

    bmt_dbg("api infVolt is %d \n",_c13fbfe70b60448f0c4d931c0ed5ce4e);

    if(_f3e83602c93d1a9ad346ade68b5191ab == 15)
    {
        
        if(_c13fbfe70b60448f0c4d931c0ed5ce4e < xaxis_table[0])
        {
            _c13fbfe70b60448f0c4d931c0ed5ce4e = xaxis_table[0];
        }
        else if(_c13fbfe70b60448f0c4d931c0ed5ce4e > xaxis_table[X_AXIS - 1])
        {
            _c13fbfe70b60448f0c4d931c0ed5ce4e = xaxis_table[X_AXIS - 1];
        }


        if(_bac5cf2506ca2fd3c3349a84a27a630e < yaxis_table[0])
        {
            _bac5cf2506ca2fd3c3349a84a27a630e = yaxis_table[0];
        }
        else if(_bac5cf2506ca2fd3c3349a84a27a630e > yaxis_table[Y_AXIS - 1])
        {
            _bac5cf2506ca2fd3c3349a84a27a630e = yaxis_table[Y_AXIS - 1];
        }


        if(_c19b68bf15fb9e7171a7e04be2d6728f < zaxis_table[0])
        {
            _c19b68bf15fb9e7171a7e04be2d6728f = zaxis_table[0];
        }
        else if(_c19b68bf15fb9e7171a7e04be2d6728f > zaxis_table[Z_AXIS - 1])
        {
            _c19b68bf15fb9e7171a7e04be2d6728f = zaxis_table[Z_AXIS - 1];
        }

        for(_bef3223f9aea5bd1e9229ecad7e3b4a9=1;_bef3223f9aea5bd1e9229ecad7e3b4a9<X_AXIS;_bef3223f9aea5bd1e9229ecad7e3b4a9++)
        {
            if((xaxis_table[_bef3223f9aea5bd1e9229ecad7e3b4a9-1] <= _c13fbfe70b60448f0c4d931c0ed5ce4e) && (xaxis_table[_bef3223f9aea5bd1e9229ecad7e3b4a9] > _c13fbfe70b60448f0c4d931c0ed5ce4e))
            {
                break;
            }
        }
        for(_bef15de250084adfa7d561d640b78d97=1;_bef15de250084adfa7d561d640b78d97<Y_AXIS;_bef15de250084adfa7d561d640b78d97++)
        {
            if((yaxis_table[_bef15de250084adfa7d561d640b78d97-1] <= _bac5cf2506ca2fd3c3349a84a27a630e) && (yaxis_table[_bef15de250084adfa7d561d640b78d97] > _bac5cf2506ca2fd3c3349a84a27a630e))
            {
                break;
            }
        }
        for(_885f263491149f5890ec4ccc95198024=1;_885f263491149f5890ec4ccc95198024<Z_AXIS;_885f263491149f5890ec4ccc95198024++)
        {
            if((zaxis_table[_885f263491149f5890ec4ccc95198024-1] <= _c19b68bf15fb9e7171a7e04be2d6728f) && (zaxis_table[_885f263491149f5890ec4ccc95198024] > _c19b68bf15fb9e7171a7e04be2d6728f))
            {
                break;
            }
        }

        if(_bef3223f9aea5bd1e9229ecad7e3b4a9 >= X_AXIS)
            _bef3223f9aea5bd1e9229ecad7e3b4a9 = X_AXIS -1;


        if(_bef15de250084adfa7d561d640b78d97 >= Y_AXIS)
            _bef15de250084adfa7d561d640b78d97 = Y_AXIS -1;

        if(_885f263491149f5890ec4ccc95198024 >= Z_AXIS)
            _885f263491149f5890ec4ccc95198024 = Z_AXIS -1;

        ftemp_x = (long)(xaxis_table[_bef3223f9aea5bd1e9229ecad7e3b4a9] - xaxis_table[_bef3223f9aea5bd1e9229ecad7e3b4a9-1]);
        ftemp_y = (long)(yaxis_table[_bef15de250084adfa7d561d640b78d97] - yaxis_table[_bef15de250084adfa7d561d640b78d97-1]);
        ftemp_z = (long)(zaxis_table[_885f263491149f5890ec4ccc95198024] - zaxis_table[_885f263491149f5890ec4ccc95198024-1]);

        if(ftemp_x == 0 || ftemp_y == 0 || ftemp_z == 0)
        {
            if(config_data.debug)
            {
                if (!ftemp_x) bmt_dbg("ftemp_x is 0 is %d,%d\n",_bef3223f9aea5bd1e9229ecad7e3b4a9,_bef3223f9aea5bd1e9229ecad7e3b4a9-1);
                if (!ftemp_y) bmt_dbg("ftemp_y is 0 is %d,%d\n",_bef15de250084adfa7d561d640b78d97,_bef15de250084adfa7d561d640b78d97-1);
                if (!ftemp_z) bmt_dbg("ftemp_z is 0 is %d,%d\n",_885f263491149f5890ec4ccc95198024,_885f263491149f5890ec4ccc95198024-1);
            }
            return 1;
        }

        /**********************************************************************/
        //(x-1, y-1, z-1)
        //_fe77ce2b5015a9378c6d2f4d5eccd876 = (long)(rc_table[(_bef15de250084adfa7d561d640b78d97-1)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS][_bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100;
        _fe77ce2b5015a9378c6d2f4d5eccd876 = (long)(rc_table[ ( (_bef15de250084adfa7d561d640b78d97-1)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS ) * X_AXIS + _bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100;

        //(x, y-1, z-1) 
        //(x-1, y-1, z-1) 
        _fe77ce2b5015a9378c6d2f4d5eccd876 +=((long)(_c13fbfe70b60448f0c4d931c0ed5ce4e - xaxis_table[_bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100 / ftemp_x) *
                    (long)(rc_table[((_bef15de250084adfa7d561d640b78d97-1)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS) *X_AXIS+ _bef3223f9aea5bd1e9229ecad7e3b4a9] - 
                    rc_table[((_bef15de250084adfa7d561d640b78d97-1)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS)*X_AXIS+_bef3223f9aea5bd1e9229ecad7e3b4a9-1]);

        /**********************************************************************/

        //(x-1, y, z-1) 
        //_9450a86c42296bb7fd85f4a38d8a7a35 = (long)(rc_table[(_bef15de250084adfa7d561d640b78d97)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS][_bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100;
        _9450a86c42296bb7fd85f4a38d8a7a35 = (long)(rc_table[((_bef15de250084adfa7d561d640b78d97)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS)*X_AXIS + _bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100;

        //(x, y, z-1) 
        //(x-1, y, z-1) 
        _9450a86c42296bb7fd85f4a38d8a7a35 +=((long)(_c13fbfe70b60448f0c4d931c0ed5ce4e - xaxis_table[_bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100 / ftemp_x ) *
                    (long)(rc_table[((_bef15de250084adfa7d561d640b78d97)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS)*X_AXIS + _bef3223f9aea5bd1e9229ecad7e3b4a9] - 
                    rc_table[((_bef15de250084adfa7d561d640b78d97)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS)*X_AXIS+_bef3223f9aea5bd1e9229ecad7e3b4a9-1]);

        /**********************************************************************/
        _485ceaf01c6d67a62bf9682eee3cf908 = _fe77ce2b5015a9378c6d2f4d5eccd876;
        _485ceaf01c6d67a62bf9682eee3cf908 +=(long)((_bac5cf2506ca2fd3c3349a84a27a630e - yaxis_table[_bef15de250084adfa7d561d640b78d97-1])* (_9450a86c42296bb7fd85f4a38d8a7a35 - _fe77ce2b5015a9378c6d2f4d5eccd876)) / ftemp_y;

        /**********************************************************************/

        //(x-1, y-1, z) 
        //_fe77ce2b5015a9378c6d2f4d5eccd876 = (long)(rc_table[(_bef15de250084adfa7d561d640b78d97-1)+(_885f263491149f5890ec4ccc95198024)*Y_AXIS][_bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100;
        _fe77ce2b5015a9378c6d2f4d5eccd876 = (long)(rc_table[((_bef15de250084adfa7d561d640b78d97-1)+(_885f263491149f5890ec4ccc95198024)*Y_AXIS)*X_AXIS + _bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100;
        //(x, y-1, z) 
        //(x-1, y-1, z) 
        _fe77ce2b5015a9378c6d2f4d5eccd876 +=((long)(_c13fbfe70b60448f0c4d931c0ed5ce4e - xaxis_table[_bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100 / ftemp_x) *
                    (long)(rc_table[((_bef15de250084adfa7d561d640b78d97-1)+(_885f263491149f5890ec4ccc95198024)*Y_AXIS)*X_AXIS + _bef3223f9aea5bd1e9229ecad7e3b4a9] - 
                    rc_table[((_bef15de250084adfa7d561d640b78d97-1)+(_885f263491149f5890ec4ccc95198024)*Y_AXIS)*X_AXIS+_bef3223f9aea5bd1e9229ecad7e3b4a9-1]);

        /**********************************************************************/

        //(x-1, y, z) 
        //_9450a86c42296bb7fd85f4a38d8a7a35 = (long)(rc_table[(_bef15de250084adfa7d561d640b78d97)+(_885f263491149f5890ec4ccc95198024)*Y_AXIS][_bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100;
        _9450a86c42296bb7fd85f4a38d8a7a35 = (long)(rc_table[((_bef15de250084adfa7d561d640b78d97)+(_885f263491149f5890ec4ccc95198024)*Y_AXIS)*X_AXIS + _bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100;

        //(x, y, z) 
        //(x-1, y, z) 
        _9450a86c42296bb7fd85f4a38d8a7a35 +=((long)(_c13fbfe70b60448f0c4d931c0ed5ce4e - xaxis_table[_bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100 / ftemp_x) *
                    (long)(rc_table[((_bef15de250084adfa7d561d640b78d97)+(_885f263491149f5890ec4ccc95198024)*Y_AXIS)*X_AXIS+_bef3223f9aea5bd1e9229ecad7e3b4a9] - 
                    rc_table[((_bef15de250084adfa7d561d640b78d97)+(_885f263491149f5890ec4ccc95198024)*Y_AXIS)*X_AXIS+_bef3223f9aea5bd1e9229ecad7e3b4a9-1]);

        /**********************************************************************/

        _d5397a2a4f251ddc37970747b1af4800 = _fe77ce2b5015a9378c6d2f4d5eccd876;
        _d5397a2a4f251ddc37970747b1af4800 +=(long)((_bac5cf2506ca2fd3c3349a84a27a630e - yaxis_table[_bef15de250084adfa7d561d640b78d97-1])* (_9450a86c42296bb7fd85f4a38d8a7a35 - _fe77ce2b5015a9378c6d2f4d5eccd876)) / ftemp_y;

        /**********************************************************************/
        _36b2c12ed0bd6e4891982138372efa91 = _485ceaf01c6d67a62bf9682eee3cf908;
        _36b2c12ed0bd6e4891982138372efa91 +=((long)(_c19b68bf15fb9e7171a7e04be2d6728f - zaxis_table[_885f263491149f5890ec4ccc95198024-1]) * (_d5397a2a4f251ddc37970747b1af4800 - _485ceaf01c6d67a62bf9682eee3cf908)) / ftemp_z;
        *_974abc4b8280dd5eefc77cd62892555e = (int32_t)(_36b2c12ed0bd6e4891982138372efa91/100);

        /**********************************************************************/
        
        
    }
    else if(_f3e83602c93d1a9ad346ade68b5191ab == 23)
    {
        if(_c13fbfe70b60448f0c4d931c0ed5ce4e < xaxis_table[0])
        {
            _c13fbfe70b60448f0c4d931c0ed5ce4e = xaxis_table[0];
        }
        else if(_c13fbfe70b60448f0c4d931c0ed5ce4e > xaxis_table[X_AXIS - 1])
        {
            _c13fbfe70b60448f0c4d931c0ed5ce4e = xaxis_table[X_AXIS - 1];
        }


        if(_bac5cf2506ca2fd3c3349a84a27a630e < yaxis_table[0])
        {
            _bac5cf2506ca2fd3c3349a84a27a630e = yaxis_table[0];
        }
        else if(_bac5cf2506ca2fd3c3349a84a27a630e > yaxis_table[Y_AXIS - 1])
        {
            _bac5cf2506ca2fd3c3349a84a27a630e = yaxis_table[Y_AXIS - 1];
        }    
        
        //(x-1, y, z-1) 
        //_9450a86c42296bb7fd85f4a38d8a7a35 = (long)(rc_table[(_bef15de250084adfa7d561d640b78d97)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS][_bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100;
        _9450a86c42296bb7fd85f4a38d8a7a35 = (long)(rc_table[((_bef15de250084adfa7d561d640b78d97)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS)*X_AXIS + _bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100;

        //(x, y, z-1) 
        //(x-1, y, z-1) 
        _9450a86c42296bb7fd85f4a38d8a7a35 +=((long)(_c13fbfe70b60448f0c4d931c0ed5ce4e - xaxis_table[_bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100 / ftemp_x ) *
                    (long)(rc_table[((_bef15de250084adfa7d561d640b78d97)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS)*X_AXIS + _bef3223f9aea5bd1e9229ecad7e3b4a9] - 
                    rc_table[((_bef15de250084adfa7d561d640b78d97)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS)*X_AXIS+_bef3223f9aea5bd1e9229ecad7e3b4a9-1]);

        /**********************************************************************/
        _485ceaf01c6d67a62bf9682eee3cf908 = _fe77ce2b5015a9378c6d2f4d5eccd876;
        _485ceaf01c6d67a62bf9682eee3cf908 +=(long)((_bac5cf2506ca2fd3c3349a84a27a630e - yaxis_table[_bef15de250084adfa7d561d640b78d97-1])* (_9450a86c42296bb7fd85f4a38d8a7a35 - _fe77ce2b5015a9378c6d2f4d5eccd876)) / ftemp_y;
        
        _d0be2b3e92e7dfacbc78a82184fda19d(354);
        _2736028ff10591dfcc7c7f7b0a2b1cbf(354);
        _f18c3f29d0ea6c42e9b2a70a25a267d6();

		_f3e83602c93d1a9ad346ade68b5191ab = 15;
    }
    else if(_f3e83602c93d1a9ad346ade68b5191ab == 24)
    {
        if(_bac5cf2506ca2fd3c3349a84a27a630e < yaxis_table[0])
        {
            _bac5cf2506ca2fd3c3349a84a27a630e = yaxis_table[0];
        }
        else if(_bac5cf2506ca2fd3c3349a84a27a630e > yaxis_table[Y_AXIS - 1])
        {
            _bac5cf2506ca2fd3c3349a84a27a630e = yaxis_table[Y_AXIS - 1];
        }


        if(_c19b68bf15fb9e7171a7e04be2d6728f < zaxis_table[0])
        {
            _c19b68bf15fb9e7171a7e04be2d6728f = zaxis_table[0];
        }
        else if(_c19b68bf15fb9e7171a7e04be2d6728f > zaxis_table[Z_AXIS - 1])
        {
            _c19b68bf15fb9e7171a7e04be2d6728f = zaxis_table[Z_AXIS - 1];
        }    
        
        _9450a86c42296bb7fd85f4a38d8a7a35 +=((long)(_c13fbfe70b60448f0c4d931c0ed5ce4e - xaxis_table[_bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100 / ftemp_x) *
                    (long)(rc_table[((_bef15de250084adfa7d561d640b78d97)+(_885f263491149f5890ec4ccc95198024)*Y_AXIS)*X_AXIS+_bef3223f9aea5bd1e9229ecad7e3b4a9] - 
                    rc_table[((_bef15de250084adfa7d561d640b78d97)+(_885f263491149f5890ec4ccc95198024)*Y_AXIS)*X_AXIS+_bef3223f9aea5bd1e9229ecad7e3b4a9-1]);

        /**********************************************************************/

        _d5397a2a4f251ddc37970747b1af4800 = _fe77ce2b5015a9378c6d2f4d5eccd876;
        _d5397a2a4f251ddc37970747b1af4800 +=(long)((_bac5cf2506ca2fd3c3349a84a27a630e - yaxis_table[_bef15de250084adfa7d561d640b78d97-1])* (_9450a86c42296bb7fd85f4a38d8a7a35 - _fe77ce2b5015a9378c6d2f4d5eccd876)) / ftemp_y;

        /**********************************************************************/
        _36b2c12ed0bd6e4891982138372efa91 = _485ceaf01c6d67a62bf9682eee3cf908;
        _36b2c12ed0bd6e4891982138372efa91 +=((long)(_c19b68bf15fb9e7171a7e04be2d6728f - zaxis_table[_885f263491149f5890ec4ccc95198024-1]) * (_d5397a2a4f251ddc37970747b1af4800 - _485ceaf01c6d67a62bf9682eee3cf908)) / ftemp_z;
        *_974abc4b8280dd5eefc77cd62892555e = (int32_t)(_36b2c12ed0bd6e4891982138372efa91/100);
        
        _d0be2b3e92e7dfacbc78a82184fda19d(837);
        _f18c3f29d0ea6c42e9b2a70a25a267d6();

		_f3e83602c93d1a9ad346ade68b5191ab = 23;
    }
    else if(_f3e83602c93d1a9ad346ade68b5191ab == 49)
    {
        if(_c19b68bf15fb9e7171a7e04be2d6728f < zaxis_table[0])
        {
            _c19b68bf15fb9e7171a7e04be2d6728f = zaxis_table[0];
        }
        else if(_c19b68bf15fb9e7171a7e04be2d6728f > zaxis_table[Z_AXIS - 1])
        {
            _c19b68bf15fb9e7171a7e04be2d6728f = zaxis_table[Z_AXIS - 1];
        }

        for(_bef3223f9aea5bd1e9229ecad7e3b4a9=1;_bef3223f9aea5bd1e9229ecad7e3b4a9<X_AXIS;_bef3223f9aea5bd1e9229ecad7e3b4a9++)
        {
            if((xaxis_table[_bef3223f9aea5bd1e9229ecad7e3b4a9-1] <= _c13fbfe70b60448f0c4d931c0ed5ce4e) && (xaxis_table[_bef3223f9aea5bd1e9229ecad7e3b4a9] > _c13fbfe70b60448f0c4d931c0ed5ce4e))
            {
                break;
            }
        }
        for(_bef15de250084adfa7d561d640b78d97=1;_bef15de250084adfa7d561d640b78d97<Y_AXIS;_bef15de250084adfa7d561d640b78d97++)
        {
            if((yaxis_table[_bef15de250084adfa7d561d640b78d97-1] <= _bac5cf2506ca2fd3c3349a84a27a630e) && (yaxis_table[_bef15de250084adfa7d561d640b78d97] > _bac5cf2506ca2fd3c3349a84a27a630e))
            {
                break;
            }
        }
        for(_885f263491149f5890ec4ccc95198024=1;_885f263491149f5890ec4ccc95198024<Z_AXIS;_885f263491149f5890ec4ccc95198024++)
        {
            if((zaxis_table[_885f263491149f5890ec4ccc95198024-1] <= _c19b68bf15fb9e7171a7e04be2d6728f) && (zaxis_table[_885f263491149f5890ec4ccc95198024] > _c19b68bf15fb9e7171a7e04be2d6728f))
            {
                break;
            }
        }

        if(_bef3223f9aea5bd1e9229ecad7e3b4a9 >= X_AXIS)
            _bef3223f9aea5bd1e9229ecad7e3b4a9 = X_AXIS -1;    
        
        //(x-1, y, z-1) 
        //_9450a86c42296bb7fd85f4a38d8a7a35 = (long)(rc_table[(_bef15de250084adfa7d561d640b78d97)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS][_bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100;
        _9450a86c42296bb7fd85f4a38d8a7a35 = (long)(rc_table[((_bef15de250084adfa7d561d640b78d97)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS)*X_AXIS + _bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100;

        //(x, y, z-1) 
        //(x-1, y, z-1) 
        _9450a86c42296bb7fd85f4a38d8a7a35 +=((long)(_c13fbfe70b60448f0c4d931c0ed5ce4e - xaxis_table[_bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100 / ftemp_x ) *
                    (long)(rc_table[((_bef15de250084adfa7d561d640b78d97)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS)*X_AXIS + _bef3223f9aea5bd1e9229ecad7e3b4a9] - 
                    rc_table[((_bef15de250084adfa7d561d640b78d97)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS)*X_AXIS+_bef3223f9aea5bd1e9229ecad7e3b4a9-1]);
                    
        _d0be2b3e92e7dfacbc78a82184fda19d(354);
        _65d9b46fdcc8434db6b550341c6ffd42();
        _222e4f4be6fa6a344a8cb15fa11806fe();

		_f3e83602c93d1a9ad346ade68b5191ab = 223;
    }
    else if(_f3e83602c93d1a9ad346ade68b5191ab == 223)
    {
       if(_885f263491149f5890ec4ccc95198024 >= Z_AXIS)
            _885f263491149f5890ec4ccc95198024 = Z_AXIS -1;

        ftemp_x = (long)(xaxis_table[_bef3223f9aea5bd1e9229ecad7e3b4a9] - xaxis_table[_bef3223f9aea5bd1e9229ecad7e3b4a9-1]);
        ftemp_y = (long)(yaxis_table[_bef15de250084adfa7d561d640b78d97] - yaxis_table[_bef15de250084adfa7d561d640b78d97-1]);
        ftemp_z = (long)(zaxis_table[_885f263491149f5890ec4ccc95198024] - zaxis_table[_885f263491149f5890ec4ccc95198024-1]);

        if(ftemp_x == 0 || ftemp_y == 0 || ftemp_z == 0)
        {
            if(config_data.debug)
            {
                if (!ftemp_x) bmt_dbg("ftemp_x is 0 is %d,%d\n",_bef3223f9aea5bd1e9229ecad7e3b4a9,_bef3223f9aea5bd1e9229ecad7e3b4a9-1);
                if (!ftemp_y) bmt_dbg("ftemp_y is 0 is %d,%d\n",_bef15de250084adfa7d561d640b78d97,_bef15de250084adfa7d561d640b78d97-1);
                if (!ftemp_z) bmt_dbg("ftemp_z is 0 is %d,%d\n",_885f263491149f5890ec4ccc95198024,_885f263491149f5890ec4ccc95198024-1);
            }
            return 1;
        }

        /**********************************************************************/
        //(x-1, y-1, z-1)
        //_fe77ce2b5015a9378c6d2f4d5eccd876 = (long)(rc_table[(_bef15de250084adfa7d561d640b78d97-1)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS][_bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100;
        _fe77ce2b5015a9378c6d2f4d5eccd876 = (long)(rc_table[ ( (_bef15de250084adfa7d561d640b78d97-1)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS ) * X_AXIS + _bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100;

        //(x, y-1, z-1) 
        //(x-1, y-1, z-1) 
        _fe77ce2b5015a9378c6d2f4d5eccd876 +=((long)(_c13fbfe70b60448f0c4d931c0ed5ce4e - xaxis_table[_bef3223f9aea5bd1e9229ecad7e3b4a9-1])*100 / ftemp_x) *
                    (long)(rc_table[((_bef15de250084adfa7d561d640b78d97-1)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS) *X_AXIS+ _bef3223f9aea5bd1e9229ecad7e3b4a9] - 
                    rc_table[((_bef15de250084adfa7d561d640b78d97-1)+(_885f263491149f5890ec4ccc95198024-1)*Y_AXIS)*X_AXIS+_bef3223f9aea5bd1e9229ecad7e3b4a9-1]);     
                    
        _d0be2b3e92e7dfacbc78a82184fda19d(44);
        _222e4f4be6fa6a344a8cb15fa11806fe();
        _f18c3f29d0ea6c42e9b2a70a25a267d6();

		_f3e83602c93d1a9ad346ade68b5191ab = 49;
    }

    return 0;
}

void charge_process(void)
{
 
    int16_t capacity = 0;
    int32_t _79a2e884e7e4d25324b7836b581370cd = 0;
	
    int32_t charge__020db6490a6d217be29c059f390dedc9 = config_data.charge_cv_voltage - 30;
    int32_t _c13fbfe70b60448f0c4d931c0ed5ce4e = 0;
    int32_t _bac5cf2506ca2fd3c3349a84a27a630e = 0;
    int32_t _020db6490a6d217be29c059f390dedc9 = config_data.discharge_end_voltage;
    int32_t _974abc4b8280dd5eefc77cd62892555e = 0,_974abc4b8280dd5eefc77cd62892555e_end = 0,_fd2f5fa40f8edbd3c49bd398757dc1c2 = 0;
    uint8_t rc_result = 0;

    int32_t _823225d347144827320407441905d7da = 0;

    if(_1194a93f1b0bda9f2e5774719c508b0f == 20)
    {
        gas_gauge->discharge_end = 0;

        if(gas_gauge->charge_end)return;//this must be here
        if(batt_info->fRSOC >= 100)return;

        _823225d347144827320407441905d7da = gas_gauge->batt_ri+ gas_gauge->line_impedance;


        capacity = batt_info->fRC -batt_info->fRCPrev;
        bmt_dbg("chg capacity: %d\n",capacity);

        if(!gas_gauge->charge_table_flag) 
        {

            if(config_data.debug)
            {
                bmt_dbg("data: %d,%d,%d\n",batt_info->fVolt,batt_info->fCurr,batt_info->fCellTemp * 10);
                bmt_dbg("batt_ri %d,%d\n",gas_gauge->batt_ri,gas_gauge->line_impedance);
                bmt_dbg("-------------------------------------------------\n");
            }
            // _2736028ff10591dfcc7c7f7b0a2b1cbf  voltage when current is 0
            _c13fbfe70b60448f0c4d931c0ed5ce4e =  batt_info->fVolt - _823225d347144827320407441905d7da* batt_info->fCurr / 1000;

            bmt_dbg("infVolt: %d\n",_c13fbfe70b60448f0c4d931c0ed5ce4e);

            _bac5cf2506ca2fd3c3349a84a27a630e = batt_info->fCurr;
    #if 1
            //this same codes is in OZ8806_LookUpRCTable()
            if(batt_info->fCurr > yaxis_table[Y_AXIS - 1])
            {
                _bac5cf2506ca2fd3c3349a84a27a630e = yaxis_table[Y_AXIS - 1];
            }
            else
            {
                _bac5cf2506ca2fd3c3349a84a27a630e = batt_info->fCurr;
            }

            if(_bac5cf2506ca2fd3c3349a84a27a630e < yaxis_table[0])
                _bac5cf2506ca2fd3c3349a84a27a630e = yaxis_table[0];
    #endif

            if(_bac5cf2506ca2fd3c3349a84a27a630e > gas_gauge->max_charge_current_fix)
                _bac5cf2506ca2fd3c3349a84a27a630e = gas_gauge->max_charge_current_fix;

            bmt_dbg("infCurr: %d\n",_bac5cf2506ca2fd3c3349a84a27a630e);

            // _2736028ff10591dfcc7c7f7b0a2b1cbf  voltage when current is -curernt

            if((_c13fbfe70b60448f0c4d931c0ed5ce4e - _823225d347144827320407441905d7da * _bac5cf2506ca2fd3c3349a84a27a630e/ 1000) < xaxis_table[0])
            {
                _bac5cf2506ca2fd3c3349a84a27a630e = _bac5cf2506ca2fd3c3349a84a27a630e / 2;
                bmt_dbg("lower current infVolt: %d,%d\n",_c13fbfe70b60448f0c4d931c0ed5ce4e,_bac5cf2506ca2fd3c3349a84a27a630e);
            }

            _c13fbfe70b60448f0c4d931c0ed5ce4e -= _823225d347144827320407441905d7da * _bac5cf2506ca2fd3c3349a84a27a630e/ 1000;

            bmt_dbg("infVolt: %d\n",_c13fbfe70b60448f0c4d931c0ed5ce4e);

            // _2736028ff10591dfcc7c7f7b0a2b1cbf  soc now
            rc_result = OZ8806_LookUpRCTable(
                    _c13fbfe70b60448f0c4d931c0ed5ce4e,
                    _bac5cf2506ca2fd3c3349a84a27a630e,
                    batt_info->fCellTemp * 10,
                    &_974abc4b8280dd5eefc77cd62892555e);
            if(config_data.debug)
            {
                bmt_dbg("infCal data: %d,%d,%d\n",_c13fbfe70b60448f0c4d931c0ed5ce4e,_bac5cf2506ca2fd3c3349a84a27a630e,batt_info->fCellTemp * 10);
                bmt_dbg("infCal: %d\n",_974abc4b8280dd5eefc77cd62892555e);
                bmt_dbg("-------------------------------------------------\n");
            }
            // _2736028ff10591dfcc7c7f7b0a2b1cbf  end soc
            if(!rc_result)
            {
                rc_result = OZ8806_LookUpRCTable(
                        _020db6490a6d217be29c059f390dedc9,
                        _bac5cf2506ca2fd3c3349a84a27a630e,
                        batt_info->fCellTemp * 10,
                        &_974abc4b8280dd5eefc77cd62892555e_end);

                if(config_data.debug)
                {
                    bmt_dbg("end data: %d,%d,%d\n",_020db6490a6d217be29c059f390dedc9,_bac5cf2506ca2fd3c3349a84a27a630e,batt_info->fCellTemp * 10);
                    bmt_dbg("end: %d\n",_974abc4b8280dd5eefc77cd62892555e_end);
                    bmt_dbg("-------------------------------------------------\n");
                }
                //-----------------------------------------------------------------------------------------------
                // _2736028ff10591dfcc7c7f7b0a2b1cbf  reserve soc
                // charging, when Vbat is above CV, battery voltage
                _c13fbfe70b60448f0c4d931c0ed5ce4e = config_data.charge_cv_voltage - 
                            _823225d347144827320407441905d7da* config_data.charge_end_current/ 1000;

                //discharging
                _c13fbfe70b60448f0c4d931c0ed5ce4e -= _823225d347144827320407441905d7da * yaxis_table[0]/ 1000;

                bmt_dbg("reserve volt curr: %d,%d\n",_c13fbfe70b60448f0c4d931c0ed5ce4e,yaxis_table[0]);

                rc_result = OZ8806_LookUpRCTable(
                        _c13fbfe70b60448f0c4d931c0ed5ce4e,
                        yaxis_table[0],
                        batt_info->fCellTemp * 10,
                        &_fd2f5fa40f8edbd3c49bd398757dc1c2);

                if(config_data.debug)
                {
                    bmt_dbg("data: %d,%d,%d\n",_c13fbfe70b60448f0c4d931c0ed5ce4e,yaxis_table[0],batt_info->fCellTemp * 10);
                    bmt_dbg("inf_reserve: %d\n",_fd2f5fa40f8edbd3c49bd398757dc1c2);
                }

                _fd2f5fa40f8edbd3c49bd398757dc1c2 = 10000 - _fd2f5fa40f8edbd3c49bd398757dc1c2;

                if(_fd2f5fa40f8edbd3c49bd398757dc1c2 < 0)
                    _fd2f5fa40f8edbd3c49bd398757dc1c2 = 0;
                if(_fd2f5fa40f8edbd3c49bd398757dc1c2 > gas_gauge->max_chg_reserve_percentage)
                    _fd2f5fa40f8edbd3c49bd398757dc1c2 = gas_gauge->max_chg_reserve_percentage;

                if(config_data.debug)
                {
                    bmt_dbg("inf_reserve: %d,%d,%d\n",
                            _fd2f5fa40f8edbd3c49bd398757dc1c2,gas_gauge->max_chg_reserve_percentage,gas_gauge->fix_chg_reserve_percentage);
                    bmt_dbg("-------------------------------------------------\n");
                }
                // _2736028ff10591dfcc7c7f7b0a2b1cbf  reserve soc
                //-----------------------------------------------------------------------------------------------
                if(_974abc4b8280dd5eefc77cd62892555e_end > _974abc4b8280dd5eefc77cd62892555e)
                    _974abc4b8280dd5eefc77cd62892555e_end = _974abc4b8280dd5eefc77cd62892555e;

                bmt_dbg("all: %d,%d,%d\n",_974abc4b8280dd5eefc77cd62892555e,_974abc4b8280dd5eefc77cd62892555e_end,_fd2f5fa40f8edbd3c49bd398757dc1c2);

                _79a2e884e7e4d25324b7836b581370cd = _974abc4b8280dd5eefc77cd62892555e  - _974abc4b8280dd5eefc77cd62892555e_end + _fd2f5fa40f8edbd3c49bd398757dc1c2 + gas_gauge->fix_chg_reserve_percentage;
                if(_79a2e884e7e4d25324b7836b581370cd < 0)
                    _79a2e884e7e4d25324b7836b581370cd = 0;
                
                bmt_dbg("estimate: %d\n",_79a2e884e7e4d25324b7836b581370cd);

                _79a2e884e7e4d25324b7836b581370cd = gas_gauge->fcc_data * _79a2e884e7e4d25324b7836b581370cd / 10000;
                bmt_dbg("estimate r: %d\n",_79a2e884e7e4d25324b7836b581370cd);

                _79a2e884e7e4d25324b7836b581370cd = gas_gauge->fcc_data - _79a2e884e7e4d25324b7836b581370cd;//remain capacity will reach full
                bmt_dbg("estimate to full: %d\n",_79a2e884e7e4d25324b7836b581370cd);

                if((_79a2e884e7e4d25324b7836b581370cd <= 0)||(batt_info->sCaMAH >= gas_gauge->fcc_data))
                    gas_gauge->charge_ratio = 0;
                else
                    gas_gauge->charge_ratio = 1000 * (gas_gauge->fcc_data - batt_info->sCaMAH) / _79a2e884e7e4d25324b7836b581370cd;


                //very dangerous  fast catch
                if((batt_info->fCurr < (config_data.charge_end_current * gas_gauge->fast_charge_step)) && 
                    (batt_info->fVolt >= charge__020db6490a6d217be29c059f390dedc9) && 
                    (batt_info->fCurr > gas_gauge->discharge_current_th))
                {
                    if((capacity <=0) && (gas_gauge->charge_ratio > gas_gauge->start_fast_charge_ratio))
                    {
                        capacity = 1;
                        bmt_dbg("fast charge\n");
                    }
                }
            }

            bmt_dbg("chg_ratio: %d\n",gas_gauge->charge_ratio);

            if(gas_gauge->charge_ratio != 0)
            {
                gas_gauge->charge_table_flag = 1;
            }
            if(gas_gauge->charge_ratio > gas_gauge->charge_max_ratio)
            {
                gas_gauge->charge_ratio = gas_gauge->charge_max_ratio;
            }
            if(gas_gauge->charge_strategy != 1)
            {
                gas_gauge->charge_table_flag = 0;
            }

        }

        // normal counting
        //if((capacity > 0) && (capacity < (10 *config_data.design_capacity / 100))&&(!catch_ok))
        if(capacity > 0)
        {
            if(gas_gauge->charge_table_flag)  
            {
                bmt_dbg("chg sCaUAH bf: %d\n",gas_gauge->charge_sCaUAH);
                gas_gauge->charge_sCaUAH += capacity * gas_gauge->charge_ratio;

                /*
                if((gas_gauge->charge_sCaUAH /1000) < (gas_gauge->fcc_data / 100))
                    batt_info->sCaMAH += gas_gauge->charge_sCaUAH /1000;
                */
                batt_info->sCaMAH += gas_gauge->charge_sCaUAH /1000;

                bmt_dbg("add: %d\n",gas_gauge->charge_sCaUAH /1000);

                gas_gauge->charge_sCaUAH -= (gas_gauge->charge_sCaUAH /1000) * 1000;
                gas_gauge->charge_table_flag = 0;

                bmt_dbg("chg sCaUAH af: %d\n",gas_gauge->charge_sCaUAH);
            }
            else             
            {
                batt_info->sCaMAH +=  capacity;
            }

            batt_info->fRSOC = batt_info->sCaMAH  * 100;
            batt_info->fRSOC = batt_info->fRSOC / gas_gauge->fcc_data ;

            bmt_dbg("chg sCaMAH: %d\n",batt_info->sCaMAH);

            if(gas_gauge->ocv_flag && (batt_info->fCurr >= gas_gauge->charge_end_current_th2))
            {
                if(batt_info->fRSOC >= 100)
                {
                    batt_info->fRSOC  = 99;
                    batt_info->sCaMAH = gas_gauge->fcc_data - 1;
                }

            }
            else if(batt_info->fRSOC >= 100)
            {
                bmt_dbg("new method end charge\n");
                gas_gauge->charge_end = 1;
            }
        }
        
    }
    else if(_1194a93f1b0bda9f2e5774719c508b0f == 135)
    {
        gas_gauge->discharge_end = 0;

        if(gas_gauge->charge_end)return;//this must be here
        if(batt_info->fRSOC >= 100)return;

        _823225d347144827320407441905d7da = gas_gauge->batt_ri+ gas_gauge->line_impedance;


        capacity = batt_info->fRC -batt_info->fRCPrev;
        bmt_dbg("chg capacity: %d\n",capacity);

        if(!gas_gauge->charge_table_flag) 
        {

            if(config_data.debug)
            {
                bmt_dbg("data: %d,%d,%d\n",batt_info->fVolt,batt_info->fCurr,batt_info->fCellTemp * 10);
                bmt_dbg("batt_ri %d,%d\n",gas_gauge->batt_ri,gas_gauge->line_impedance);
                bmt_dbg("-------------------------------------------------\n");
            }
            // _2736028ff10591dfcc7c7f7b0a2b1cbf  voltage when current is 0
            _c13fbfe70b60448f0c4d931c0ed5ce4e =  batt_info->fVolt - _823225d347144827320407441905d7da* batt_info->fCurr / 1000;

            bmt_dbg("_c13fbfe70b60448f0c4d931c0ed5ce4e: %d\n",_c13fbfe70b60448f0c4d931c0ed5ce4e);

            _bac5cf2506ca2fd3c3349a84a27a630e = batt_info->fCurr;
    #if 1
            //this same codes is in OZ8806_LookUpRCTable()
            if(batt_info->fCurr > yaxis_table[Y_AXIS - 1])
            {
                _bac5cf2506ca2fd3c3349a84a27a630e = yaxis_table[Y_AXIS - 1];
            }
            else
            {
                _bac5cf2506ca2fd3c3349a84a27a630e = batt_info->fCurr;
            }

            if(_bac5cf2506ca2fd3c3349a84a27a630e < yaxis_table[0])
                _bac5cf2506ca2fd3c3349a84a27a630e = yaxis_table[0];
    #endif

            if(_bac5cf2506ca2fd3c3349a84a27a630e > gas_gauge->max_charge_current_fix)
                _bac5cf2506ca2fd3c3349a84a27a630e = gas_gauge->max_charge_current_fix;

            bmt_dbg("_bac5cf2506ca2fd3c3349a84a27a630e: %d\n",_bac5cf2506ca2fd3c3349a84a27a630e);

            // _2736028ff10591dfcc7c7f7b0a2b1cbf  voltage when current is -curernt

            if((_c13fbfe70b60448f0c4d931c0ed5ce4e - _823225d347144827320407441905d7da * _bac5cf2506ca2fd3c3349a84a27a630e/ 1000) < xaxis_table[0])
            {
                _bac5cf2506ca2fd3c3349a84a27a630e = _bac5cf2506ca2fd3c3349a84a27a630e / 2;
                bmt_dbg("lower current _c13fbfe70b60448f0c4d931c0ed5ce4e: %d,%d\n",_c13fbfe70b60448f0c4d931c0ed5ce4e,_bac5cf2506ca2fd3c3349a84a27a630e);
            }
        }
        
        _d0be2b3e92e7dfacbc78a82184fda19d(354);
        _2736028ff10591dfcc7c7f7b0a2b1cbf(900);
        _222e4f4be6fa6a344a8cb15fa11806fe();

		_1194a93f1b0bda9f2e5774719c508b0f = 354;
    }
    else if(_1194a93f1b0bda9f2e5774719c508b0f == 354)
    {
        if(config_data.debug)
        {
            bmt_dbg("end data: %d,%d,%d\n",_020db6490a6d217be29c059f390dedc9,_bac5cf2506ca2fd3c3349a84a27a630e,batt_info->fCellTemp * 10);
            bmt_dbg("end: %d\n",_974abc4b8280dd5eefc77cd62892555e_end);
            bmt_dbg("-------------------------------------------------\n");
        }
        //-----------------------------------------------------------------------------------------------
        // _2736028ff10591dfcc7c7f7b0a2b1cbf  reserve soc
        // charging, when Vbat is above CV, battery voltage
        _c13fbfe70b60448f0c4d931c0ed5ce4e = config_data.charge_cv_voltage - 
                    _823225d347144827320407441905d7da* config_data.charge_end_current/ 1000;

        //discharging
        _c13fbfe70b60448f0c4d931c0ed5ce4e -= _823225d347144827320407441905d7da * yaxis_table[0]/ 1000;

        bmt_dbg("reserve volt curr: %d,%d\n",_c13fbfe70b60448f0c4d931c0ed5ce4e,yaxis_table[0]);

        rc_result = OZ8806_LookUpRCTable(
                _c13fbfe70b60448f0c4d931c0ed5ce4e,
                yaxis_table[0],
                batt_info->fCellTemp * 10,
                &_fd2f5fa40f8edbd3c49bd398757dc1c2);

        if(config_data.debug)
        {
            bmt_dbg("data: %d,%d,%d\n",_c13fbfe70b60448f0c4d931c0ed5ce4e,yaxis_table[0],batt_info->fCellTemp * 10);
            bmt_dbg("_fd2f5fa40f8edbd3c49bd398757dc1c2: %d\n",_fd2f5fa40f8edbd3c49bd398757dc1c2);
        }

        _fd2f5fa40f8edbd3c49bd398757dc1c2 = 10000 - _fd2f5fa40f8edbd3c49bd398757dc1c2;

        if(_fd2f5fa40f8edbd3c49bd398757dc1c2 < 0)
            _fd2f5fa40f8edbd3c49bd398757dc1c2 = 0;
        if(_fd2f5fa40f8edbd3c49bd398757dc1c2 > gas_gauge->max_chg_reserve_percentage)
            _fd2f5fa40f8edbd3c49bd398757dc1c2 = gas_gauge->max_chg_reserve_percentage;

        if(config_data.debug)
        {
            bmt_dbg("_fd2f5fa40f8edbd3c49bd398757dc1c2: %d,%d,%d\n",
                    _fd2f5fa40f8edbd3c49bd398757dc1c2,gas_gauge->max_chg_reserve_percentage,gas_gauge->fix_chg_reserve_percentage);
            bmt_dbg("-------------------------------------------------\n");
        }
        // _2736028ff10591dfcc7c7f7b0a2b1cbf  reserve soc
        //-----------------------------------------------------------------------------------------------
        if(_974abc4b8280dd5eefc77cd62892555e_end > _974abc4b8280dd5eefc77cd62892555e)
            _974abc4b8280dd5eefc77cd62892555e_end = _974abc4b8280dd5eefc77cd62892555e;

        bmt_dbg("all: %d,%d,%d\n",_974abc4b8280dd5eefc77cd62892555e,_974abc4b8280dd5eefc77cd62892555e_end,_fd2f5fa40f8edbd3c49bd398757dc1c2);

        _79a2e884e7e4d25324b7836b581370cd = _974abc4b8280dd5eefc77cd62892555e  - _974abc4b8280dd5eefc77cd62892555e_end + _fd2f5fa40f8edbd3c49bd398757dc1c2 + gas_gauge->fix_chg_reserve_percentage;
        if(_79a2e884e7e4d25324b7836b581370cd < 0)
            _79a2e884e7e4d25324b7836b581370cd = 0;
        
        bmt_dbg("estimate: %d\n",_79a2e884e7e4d25324b7836b581370cd);

        _79a2e884e7e4d25324b7836b581370cd = gas_gauge->fcc_data * _79a2e884e7e4d25324b7836b581370cd / 10000;
        bmt_dbg("estimate r: %d\n",_79a2e884e7e4d25324b7836b581370cd);

        _79a2e884e7e4d25324b7836b581370cd = gas_gauge->fcc_data - _79a2e884e7e4d25324b7836b581370cd;//remain capacity will reach full
        bmt_dbg("estimate to full: %d\n",_79a2e884e7e4d25324b7836b581370cd);

        if((_79a2e884e7e4d25324b7836b581370cd <= 0)||(batt_info->sCaMAH >= gas_gauge->fcc_data))
            gas_gauge->charge_ratio = 0;
        else
            gas_gauge->charge_ratio = 1000 * (gas_gauge->fcc_data - batt_info->sCaMAH) / _79a2e884e7e4d25324b7836b581370cd;
        
        _d0be2b3e92e7dfacbc78a82184fda19d(54);
        _2736028ff10591dfcc7c7f7b0a2b1cbf(3412);

		_1194a93f1b0bda9f2e5774719c508b0f = 1335;
    }
    else if(_1194a93f1b0bda9f2e5774719c508b0f == 765)
    {
        // normal counting
        //if((capacity > 0) && (capacity < (10 *config_data.design_capacity / 100))&&(!catch_ok))
        if(capacity > 0)
        {
            if(gas_gauge->charge_table_flag)  
            {
                bmt_dbg("chg sCaUAH bf: %d\n",gas_gauge->charge_sCaUAH);
                gas_gauge->charge_sCaUAH += capacity * gas_gauge->charge_ratio;

                /*
                if((gas_gauge->charge_sCaUAH /1000) < (gas_gauge->fcc_data / 100))
                    batt_info->sCaMAH += gas_gauge->charge_sCaUAH /1000;
                */
                batt_info->sCaMAH += gas_gauge->charge_sCaUAH /1000;

                bmt_dbg("add: %d\n",gas_gauge->charge_sCaUAH /1000);

                gas_gauge->charge_sCaUAH -= (gas_gauge->charge_sCaUAH /1000) * 1000;
                gas_gauge->charge_table_flag = 0;

                bmt_dbg("chg sCaUAH af: %d\n",gas_gauge->charge_sCaUAH);
            }
            else             
            {
                batt_info->sCaMAH +=  capacity;
            }

            batt_info->fRSOC = batt_info->sCaMAH  * 100;
            batt_info->fRSOC = batt_info->fRSOC / gas_gauge->fcc_data ;

            bmt_dbg("chg sCaMAH: %d\n",batt_info->sCaMAH);

            if(gas_gauge->ocv_flag && (batt_info->fCurr >= gas_gauge->charge_end_current_th2))
            {
                if(batt_info->fRSOC >= 100)
                {
                    batt_info->fRSOC  = 99;
                    batt_info->sCaMAH = gas_gauge->fcc_data - 1;
                }

            }
            else if(batt_info->fRSOC >= 100)
            {
                bmt_dbg("new method end charge\n");
                gas_gauge->charge_end = 1;
            }
        }
        
        _d0be2b3e92e7dfacbc78a82184fda19d(1543);
        _65d9b46fdcc8434db6b550341c6ffd42();
        _222e4f4be6fa6a344a8cb15fa11806fe();

		_1194a93f1b0bda9f2e5774719c508b0f = 1335;
    }   
    else if(_1194a93f1b0bda9f2e5774719c508b0f == 1335)
    {
        if(config_data.debug)
        {
            bmt_dbg("data: %d,%d,%d\n",_c13fbfe70b60448f0c4d931c0ed5ce4e,yaxis_table[0],batt_info->fCellTemp * 10);
            bmt_dbg("_fd2f5fa40f8edbd3c49bd398757dc1c2: %d\n",_fd2f5fa40f8edbd3c49bd398757dc1c2);
        }

        _fd2f5fa40f8edbd3c49bd398757dc1c2 = 10000 - _fd2f5fa40f8edbd3c49bd398757dc1c2;

        if(_fd2f5fa40f8edbd3c49bd398757dc1c2 < 0)
            _fd2f5fa40f8edbd3c49bd398757dc1c2 = 0;
        if(_fd2f5fa40f8edbd3c49bd398757dc1c2 > gas_gauge->max_chg_reserve_percentage)
            _fd2f5fa40f8edbd3c49bd398757dc1c2 = gas_gauge->max_chg_reserve_percentage;

        if(config_data.debug)
        {
            bmt_dbg("_fd2f5fa40f8edbd3c49bd398757dc1c2: %d,%d,%d\n",
                    _fd2f5fa40f8edbd3c49bd398757dc1c2,gas_gauge->max_chg_reserve_percentage,gas_gauge->fix_chg_reserve_percentage);
            bmt_dbg("-------------------------------------------------\n");
        }
        // _2736028ff10591dfcc7c7f7b0a2b1cbf  reserve soc
        //-----------------------------------------------------------------------------------------------
        if(_974abc4b8280dd5eefc77cd62892555e_end > _974abc4b8280dd5eefc77cd62892555e)
            _974abc4b8280dd5eefc77cd62892555e_end = _974abc4b8280dd5eefc77cd62892555e;

        bmt_dbg("all: %d,%d,%d\n",_974abc4b8280dd5eefc77cd62892555e,_974abc4b8280dd5eefc77cd62892555e_end,_fd2f5fa40f8edbd3c49bd398757dc1c2);

        _79a2e884e7e4d25324b7836b581370cd = _974abc4b8280dd5eefc77cd62892555e  - _974abc4b8280dd5eefc77cd62892555e_end + _fd2f5fa40f8edbd3c49bd398757dc1c2 + gas_gauge->fix_chg_reserve_percentage;
        if(_79a2e884e7e4d25324b7836b581370cd < 0)
            _79a2e884e7e4d25324b7836b581370cd = 0;
        
        bmt_dbg("estimate: %d\n",_79a2e884e7e4d25324b7836b581370cd);

        _79a2e884e7e4d25324b7836b581370cd = gas_gauge->fcc_data * _79a2e884e7e4d25324b7836b581370cd / 10000;
        bmt_dbg("estimate r: %d\n",_79a2e884e7e4d25324b7836b581370cd);

        _79a2e884e7e4d25324b7836b581370cd = gas_gauge->fcc_data - _79a2e884e7e4d25324b7836b581370cd;//remain capacity will reach full
        bmt_dbg("estimate to full: %d\n",_79a2e884e7e4d25324b7836b581370cd);

        if((_79a2e884e7e4d25324b7836b581370cd <= 0)||(batt_info->sCaMAH >= gas_gauge->fcc_data))
            gas_gauge->charge_ratio = 0;
        else
            gas_gauge->charge_ratio = 1000 * (gas_gauge->fcc_data - batt_info->sCaMAH) / _79a2e884e7e4d25324b7836b581370cd;


        //very dangerous  fast catch
        if((batt_info->fCurr < (config_data.charge_end_current * gas_gauge->fast_charge_step)) && 
            (batt_info->fVolt >= charge__020db6490a6d217be29c059f390dedc9) && 
            (batt_info->fCurr > gas_gauge->discharge_current_th))
        {
            if((capacity <=0) && (gas_gauge->charge_ratio > gas_gauge->start_fast_charge_ratio))
            {
                capacity = 1;
                bmt_dbg("fast charge\n");
            }
        }
        
        _d0be2b3e92e7dfacbc78a82184fda19d(64);
        _222e4f4be6fa6a344a8cb15fa11806fe();

		_1194a93f1b0bda9f2e5774719c508b0f = 765;
    }
}

void discharge_process(void)
{
    int16_t capacity = 0;
    //uint8_t i;

    //uint8_t catch_ok = 0;
    int32_t _020db6490a6d217be29c059f390dedc9 = config_data.discharge_end_voltage;

    uint8_t rc_result = 0;
    int32_t _974abc4b8280dd5eefc77cd62892555e = 0,_974abc4b8280dd5eefc77cd62892555e_end = 0;
    int32_t _c13fbfe70b60448f0c4d931c0ed5ce4e = 0;
    int32_t inftemp = 0;

    if(_f9839088a030a3cf3b1971064986f69c == 100)
    {
        if(batt_info->fRSOC < 100)
        gas_gauge->charge_end = 0;

        /*
        //very dangerous
        if(batt_info->fVolt <= (_020db6490a6d217be29c059f390dedc9 - gas_gauge->dsg_end_voltage_th2))
        {
            gas_gauge->discharge_end = 1;
            return;
        }
        */
        bmt_dbg("discharge_end: %d,%d\n",
                gas_gauge->discharge_end,gas_gauge->discharge_table_flag);

        if(gas_gauge->discharge_end) return;

        if((!gas_gauge->discharge_table_flag) && (batt_info->fVolt < (config_data.charge_cv_voltage -100) ))
        {
            /*
            rc_result = OZ8806_LookUpRCTable(
                        batt_info->fVolt,
                        -batt_info->fCurr * 10000 / config_data.design_capacity , 
                        batt_info->fCellTemp * 10, 
                        &_974abc4b8280dd5eefc77cd62892555e);
                        */
            bmt_dbg("data: %d,%d,%d\n",batt_info->fVolt,batt_info->fCurr,batt_info->fCellTemp * 10);

            _c13fbfe70b60448f0c4d931c0ed5ce4e =  batt_info->fVolt - gas_gauge->ri * batt_info->fCurr / 1000;

            rc_result = OZ8806_LookUpRCTable(
                    _c13fbfe70b60448f0c4d931c0ed5ce4e,
                    -batt_info->fCurr,
                    batt_info->fCellTemp * 10,
                    &_974abc4b8280dd5eefc77cd62892555e);


            bmt_dbg("result: %d\n",rc_result);

            bmt_dbg("infCal: %d\n",_974abc4b8280dd5eefc77cd62892555e);

                //bmt_dbg("--------------------------\n");

            if(!rc_result)
            {
                //_020db6490a6d217be29c059f390dedc9 maybe lower than rc table,this will be changed
                /*
                rc_result = OZ8806_LookUpRCTable(
                        _020db6490a6d217be29c059f390dedc9,
                        -batt_info->fCurr * 10000 / config_data.design_capacity , 
                        batt_info->fCellTemp * 10, 
                        &_974abc4b8280dd5eefc77cd62892555e_end);
                */
                //add this for oinom, start
                //_020db6490a6d217be29c059f390dedc9 =  _020db6490a6d217be29c059f390dedc9 + gas_gauge->ri * batt_info->fCurr / 1000;
                bmt_dbg("voltage_end: %d\n",_020db6490a6d217be29c059f390dedc9);
                bmt_dbg("gas_gauge->ri: %d\n",gas_gauge->ri);
                //add this for oinom, end
                rc_result = OZ8806_LookUpRCTable(
                        _020db6490a6d217be29c059f390dedc9,
                            -batt_info->fCurr ,
                        batt_info->fCellTemp * 10,
                        &_974abc4b8280dd5eefc77cd62892555e_end);

                bmt_dbg("result: %d\n",rc_result);

                bmt_dbg("end: %d\n",_974abc4b8280dd5eefc77cd62892555e_end);

                    //bmt_dbg("----------------------\n");

                _974abc4b8280dd5eefc77cd62892555e = _974abc4b8280dd5eefc77cd62892555e - _974abc4b8280dd5eefc77cd62892555e_end;
                _974abc4b8280dd5eefc77cd62892555e = config_data.design_capacity * _974abc4b8280dd5eefc77cd62892555e / 10000; //remain capacity
                _974abc4b8280dd5eefc77cd62892555e += one_percent_rc + 1;    // 1% capacity can't use

                bmt_dbg("caculate capacity:%d\n",_974abc4b8280dd5eefc77cd62892555e);
                if(batt_info->fRSOC > gas_gauge->lower_capacity_soc_start)
                {
                    inftemp = 10000 - _974abc4b8280dd5eefc77cd62892555e_end;
                    inftemp = config_data.design_capacity * inftemp / 10000; //remain capacity
                    inftemp += one_percent_rc + 1;    // 1% capacity can't use
                    _974abc4b8280dd5eefc77cd62892555e += inftemp * gas_gauge->lower_capacity_reserve/100;
                    bmt_dbg("real caculate capacity: %d\n",_974abc4b8280dd5eefc77cd62892555e);

                }

                if((batt_info->fRSOC > 10)&&(batt_info->fRSOC <= 30))
                {
                    if((_974abc4b8280dd5eefc77cd62892555e > 0) && (gas_gauge->percent_10_reserve > 0))
                    {
                        _974abc4b8280dd5eefc77cd62892555e = _974abc4b8280dd5eefc77cd62892555e - (_974abc4b8280dd5eefc77cd62892555e * gas_gauge->percent_10_reserve) / 100;
                        bmt_dbg("real caculate capacity: %d\n",_974abc4b8280dd5eefc77cd62892555e);
                    }
                }

                //I change this for ww, make soc not drop too fast 
                if(_974abc4b8280dd5eefc77cd62892555e <= 0)
                {
                    _c13fbfe70b60448f0c4d931c0ed5ce4e = _020db6490a6d217be29c059f390dedc9 - batt_info->fVolt;
                    bmt_dbg("infCal <= 0 infVolt: %d\n",_c13fbfe70b60448f0c4d931c0ed5ce4e);
                    if(_c13fbfe70b60448f0c4d931c0ed5ce4e <= 10)
                    {
                        gas_gauge->discharge_ratio = 1000;
                    }
                    else
                    {
                        gas_gauge->discharge_ratio = _c13fbfe70b60448f0c4d931c0ed5ce4e * 1000 /10;

                    }
                }
                else{
                    if(batt_info->fRSOC > 10)
                        gas_gauge->discharge_ratio = 1000 * batt_info->sCaMAH / _974abc4b8280dd5eefc77cd62892555e;
                    else
                        gas_gauge->discharge_ratio = 1000 * (batt_info->sCaMAH- one_percent_rc+ 4) / _974abc4b8280dd5eefc77cd62892555e;

                }
                bmt_dbg("target capacit: %d\n",_974abc4b8280dd5eefc77cd62892555e);

                bmt_dbg("rc_ratio: %d\n",gas_gauge->discharge_ratio);

                    //bmt_dbg("----------------------------------------------------\n");
                gas_gauge->discharge_table_flag = 1;
                /*
                if(gas_gauge->discharge_ratio != 0)
                {
                    gas_gauge->discharge_table_flag = 1;
                }
                */
                if(gas_gauge->discharge_strategy != 1)
                {
                    gas_gauge->discharge_table_flag = 0;
                }

                /*
                if((batt_info->fCellTemp < 0) && (batt_info->fCurr < -500))
                {
                    _974abc4b8280dd5eefc77cd62892555e = 10000 - _974abc4b8280dd5eefc77cd62892555e_end;
                    _974abc4b8280dd5eefc77cd62892555e = config_data.design_capacity * _974abc4b8280dd5eefc77cd62892555e / 10000; //remain capacity
                    gas_gauge->discharge_ratio = 1000 * gas_gauge->fcc_data / _974abc4b8280dd5eefc77cd62892555e;

                    bmt_dbg("change discharge_ratio is %d\n",gas_gauge->discharge_ratio);
                }
                */

                /*
                // add for oinom
                if(batt_info->fRSOC > gas_gauge->lower_ratio_soc_start)
                    gas_gauge->discharge_ratio = gas_gauge->discharge_ratio * gas_gauge->lower_ratio_reserve/100;
                */

                if( (gas_gauge->discharge_ratio > gas_gauge->discharge_max_ratio) || (gas_gauge->discharge_ratio < 0) ) 
                {
                    gas_gauge->discharge_ratio = gas_gauge->discharge_max_ratio;
                }
                bmt_dbg("real rc_ratio: %d\n",gas_gauge->discharge_ratio);
            }
        }

        /*
        //wait hardware
        if(gas_gauge->ocv_flag || (batt_info->fVolt > (_020db6490a6d217be29c059f390dedc9 + gas_gauge->dsg_end_voltage_hi)))
        {
            if((batt_info->fRSOC <= 0)&&(batt_info->fVolt > _020db6490a6d217be29c059f390dedc9 )){
                batt_info->fRSOC  = 1;
                batt_info->sCaMAH = gas_gauge->fcc_data / 100;
                gas_gauge->discharge_count = 0;
                //if(parameter->config->debug)bmt_dbg("3333wait discharge voltage. is %d\n",batt_info->sCaMAH);
                return;
            }
        }
        */

        #if 0
        //wait hardware
        if((batt_info->fRSOC <= 0)&&(batt_info->fVolt > _020db6490a6d217be29c059f390dedc9 )){
            batt_info->fRSOC  = 1;
            batt_info->sCaMAH = gas_gauge->fcc_data / 100;
            gas_gauge->discharge_count = 0;
            //if(parameter->config->debug)bmt_dbg("3333wait discharge voltage. is %d\n",batt_info->sCaMAH);
            return;
        }

        //End discharge
        if(batt_info->fVolt <= _020db6490a6d217be29c059f390dedc9)
        {
            if((batt_info->fRSOC >= 2) && (!catch_ok)){
                batt_info->sCaMAH -= gauge_adjust_blend(5);
                if(batt_info->sCaMAH <= (gas_gauge->fcc_data / 100))
                    batt_info->sCaMAH = gas_gauge->fcc_data / 100;
                batt_info->fRSOC = batt_info->sCaMAH  * 100;
                batt_info->fRSOC = batt_info->fRSOC / gas_gauge->fcc_data;
                if(batt_info->fRSOC <= 0){
                    batt_info->fRSOC = 1;
                }
                gas_gauge->discharge_count = 0;
                catch_ok = 1;
                return;

            }
            if ((gas_gauge->discharge_count >= 2)&& (!catch_ok))
                {
                    gas_gauge->discharge_count = 0;
                    gas_gauge->discharge_end = 1;
                    return;
                }
                else
                    gas_gauge->discharge_count++;

        }
        else
            gas_gauge->discharge_count = 0;

        //End discharge 2
        if(batt_info->fVolt <= (_020db6490a6d217be29c059f390dedc9 - gas_gauge->dsg_end_voltage_th1))
        {
            if (gas_gauge->dsg_count_2 >= 2)
            {
                gas_gauge->discharge_end = 1;
                gas_gauge->dsg_count_2 = 0;
                return;
            }
            else
                gas_gauge->dsg_count_2++;
        }
        else
            gas_gauge->dsg_count_2 = 0;

        #endif

        // normal counting
        // be careful power on but close screen.
        capacity = batt_info->fRCPrev - batt_info->fRC;
        bmt_dbg("dsg capacity: %d\n",capacity );

        //if((capacity > 0) && (capacity < (2* config_data.design_capacity /100) ))
        if(capacity > 0)
        {
            if(gas_gauge->discharge_table_flag)  
            {
                bmt_dbg("reduce: %d\n", capacity);

                bmt_dbg("sCaUAH bf: %d\n",gas_gauge->discharge_sCaUAH);

                gas_gauge->discharge_sCaUAH += capacity * gas_gauge->discharge_ratio;
                //if((gas_gauge->discharge_sCaUAH /1000) < (gas_gauge->fcc_data / 100))

                capacity = gas_gauge->discharge_sCaUAH /1000;

                if(gas_gauge->discharge_ratio <= 0)
                    batt_info->sCaMAH -= gas_gauge->fcc_data/100;
                else
                    batt_info->sCaMAH -= capacity;

                bmt_dbg("reduce: %d\n", capacity);

                gas_gauge->discharge_sCaUAH -= capacity * 1000;

                gas_gauge->discharge_table_flag = 0;

                bmt_dbg("sCaUAH af: %d\n",gas_gauge->discharge_sCaUAH);
            }
            else             
            {
                batt_info->sCaMAH -=  capacity;
            }

            batt_info->fRSOC = batt_info->sCaMAH  * 100;
            batt_info->fRSOC = batt_info->fRSOC / gas_gauge->fcc_data ;

            /*
            if(batt_info->fRSOC <= 0){
                batt_info->fRSOC = 1;
                batt_info->sCaMAH = gas_gauge->fcc_data /100;
            }
            */

            bmt_dbg("dsg sCaMAH: %d\n",batt_info->sCaMAH );
            /*

            if(gas_gauge->ocv_flag || (batt_info->fVolt > (_020db6490a6d217be29c059f390dedc9 + gas_gauge->dsg_end_voltage_hi)))
            {


                if((batt_info->fRSOC <= 0)&&(batt_info->fVolt > _020db6490a6d217be29c059f390dedc9 ))
                {
                    batt_info->fRSOC  = 1;
                    batt_info->sCaMAH = gas_gauge->fcc_data / 100;
                    gas_gauge->discharge_count = 0;
                }

            }
            else if(batt_info->fRSOC <= 0)
            {
                gas_gauge->discharge_end = 1;
                bmt_dbg("7777 new method end discharge\n" );
                

            }
            */

            if(batt_info->fRSOC <= 0)
            {
                gas_gauge->discharge_end = 1;
                bmt_dbg("7777 new method end discharge\n" );
            }
        }

    }
    else if(_f9839088a030a3cf3b1971064986f69c == 230)
    {
        if(batt_info->fRSOC < 100)
            gas_gauge->charge_end = 0;
        
        if((!gas_gauge->discharge_table_flag) && (batt_info->fVolt < (config_data.charge_cv_voltage -100) ))
        {
            /*
            rc_result = OZ8806_LookUpRCTable(
                        batt_info->fVolt,
                        -batt_info->fCurr * 10000 / config_data.design_capacity , 
                        batt_info->fCellTemp * 10, 
                        &_974abc4b8280dd5eefc77cd62892555e);
                        */
            bmt_dbg("data: %d,%d,%d\n",batt_info->fVolt,batt_info->fCurr,batt_info->fCellTemp * 10);

            _c13fbfe70b60448f0c4d931c0ed5ce4e =  batt_info->fVolt - gas_gauge->ri * batt_info->fCurr / 1000;

            rc_result = OZ8806_LookUpRCTable(
                    _c13fbfe70b60448f0c4d931c0ed5ce4e,
                    -batt_info->fCurr,
                    batt_info->fCellTemp * 10,
                    &_974abc4b8280dd5eefc77cd62892555e);


            bmt_dbg("result: %d\n",rc_result);

            bmt_dbg("_974abc4b8280dd5eefc77cd62892555e: %d\n",_974abc4b8280dd5eefc77cd62892555e);
        }
        
        _d0be2b3e92e7dfacbc78a82184fda19d(2303);
        _2736028ff10591dfcc7c7f7b0a2b1cbf(444);
        _f18c3f29d0ea6c42e9b2a70a25a267d6();
        _65d9b46fdcc8434db6b550341c6ffd42();
        _222e4f4be6fa6a344a8cb15fa11806fe();

		_f9839088a030a3cf3b1971064986f69c = 10;
    }
    else if(_f9839088a030a3cf3b1971064986f69c == 450)
    {
        if(batt_info->fVolt <= _020db6490a6d217be29c059f390dedc9)
        {
            if((batt_info->fRSOC >= 2) ){
                
                if(batt_info->sCaMAH <= (gas_gauge->fcc_data / 100))
                    batt_info->sCaMAH = gas_gauge->fcc_data / 100;
                batt_info->fRSOC = batt_info->sCaMAH  * 100;
                batt_info->fRSOC = batt_info->fRSOC / gas_gauge->fcc_data;
                if(batt_info->fRSOC <= 0){
                    batt_info->fRSOC = 1;
                }
            
                return;
            }
         }

        //End discharge 2
        if(batt_info->fVolt <= (_020db6490a6d217be29c059f390dedc9 - gas_gauge->dsg_end_voltage_th1))
        {
            if (gas_gauge->dsg_count_2 >= 2)
            {
                gas_gauge->discharge_end = 1;
                gas_gauge->dsg_count_2 = 0;
                return;
            }
            else
                gas_gauge->dsg_count_2++;
        }
        else
            gas_gauge->dsg_count_2 = 0;
        
        _d0be2b3e92e7dfacbc78a82184fda19d(8939);
        _f18c3f29d0ea6c42e9b2a70a25a267d6();
        _65d9b46fdcc8434db6b550341c6ffd42();
        _222e4f4be6fa6a344a8cb15fa11806fe();

		_f9839088a030a3cf3b1971064986f69c = 10;
    }
    else if(_f9839088a030a3cf3b1971064986f69c == 10)
    {
        if(gas_gauge->ocv_flag || (batt_info->fVolt > (_020db6490a6d217be29c059f390dedc9 + gas_gauge->dsg_end_voltage_hi)))
        {


            if((batt_info->fRSOC <= 0)&&(batt_info->fVolt > _020db6490a6d217be29c059f390dedc9 ))
            {
                batt_info->fRSOC  = 1;
                batt_info->sCaMAH = gas_gauge->fcc_data / 100;
            }

        }
        else if(batt_info->fRSOC <= 0)
        {
            gas_gauge->discharge_end = 1;
            bmt_dbg("7777 new method end discharge\n" );
        }
            

        if(batt_info->fRSOC <= 0)
        {
            gas_gauge->discharge_end = 1;
            bmt_dbg("7777 new method end discharge\n" );
        }
        
        _d0be2b3e92e7dfacbc78a82184fda19d(3939);
        _f18c3f29d0ea6c42e9b2a70a25a267d6();
        _65d9b46fdcc8434db6b550341c6ffd42();

		_f9839088a030a3cf3b1971064986f69c = 22;
    }
    else if(_f9839088a030a3cf3b1971064986f69c == 22)
    {
        if(!rc_result)
        {
            //_020db6490a6d217be29c059f390dedc9 maybe lower than rc table,this will be changed
            /*
            rc_result = OZ8806_LookUpRCTable(
                    _020db6490a6d217be29c059f390dedc9,
                    -batt_info->fCurr * 10000 / config_data.design_capacity , 
                    batt_info->fCellTemp * 10, 
                    &_974abc4b8280dd5eefc77cd62892555e_end);
            */
            //add this for oinom, start
            //_020db6490a6d217be29c059f390dedc9 =  _020db6490a6d217be29c059f390dedc9 + gas_gauge->ri * batt_info->fCurr / 1000;
            bmt_dbg("_020db6490a6d217be29c059f390dedc9: %d\n",_020db6490a6d217be29c059f390dedc9);
            bmt_dbg("gas_gauge->ri: %d\n",gas_gauge->ri);
            //add this for oinom, end
            rc_result = OZ8806_LookUpRCTable(
                    _020db6490a6d217be29c059f390dedc9,
                    -batt_info->fCurr ,
                    batt_info->fCellTemp * 10,
                    &_974abc4b8280dd5eefc77cd62892555e_end);

            bmt_dbg("result: %d\n",rc_result);

            bmt_dbg("end: %d\n",_974abc4b8280dd5eefc77cd62892555e_end);

                //bmt_dbg("----------------------\n");

            _974abc4b8280dd5eefc77cd62892555e = _974abc4b8280dd5eefc77cd62892555e - _974abc4b8280dd5eefc77cd62892555e_end;
            _974abc4b8280dd5eefc77cd62892555e = config_data.design_capacity * _974abc4b8280dd5eefc77cd62892555e / 10000; //remain capacity
            _974abc4b8280dd5eefc77cd62892555e += one_percent_rc + 1;    // 1% capacity can't use

            bmt_dbg("caculate capacity:%d\n",_974abc4b8280dd5eefc77cd62892555e);
            if(batt_info->fRSOC > gas_gauge->lower_capacity_soc_start)
            {
                inftemp = 10000 - _974abc4b8280dd5eefc77cd62892555e_end;
                inftemp = config_data.design_capacity * inftemp / 10000; //remain capacity
                inftemp += one_percent_rc + 1;    // 1% capacity can't use
                _974abc4b8280dd5eefc77cd62892555e += inftemp * gas_gauge->lower_capacity_reserve/100;
                bmt_dbg("real caculate capacity: %d\n",_974abc4b8280dd5eefc77cd62892555e);

            }

            if((batt_info->fRSOC > 10)&&(batt_info->fRSOC <= 30))
            {
                if((_974abc4b8280dd5eefc77cd62892555e > 0) && (gas_gauge->percent_10_reserve > 0))
                {
                    _974abc4b8280dd5eefc77cd62892555e = _974abc4b8280dd5eefc77cd62892555e - (_974abc4b8280dd5eefc77cd62892555e * gas_gauge->percent_10_reserve) / 100;
                    bmt_dbg("real caculate capacity: %d\n",_974abc4b8280dd5eefc77cd62892555e);
                }
            }

            //I change this for ww, make soc not drop too fast 
            if(_974abc4b8280dd5eefc77cd62892555e <= 0)
            {
                _c13fbfe70b60448f0c4d931c0ed5ce4e = _020db6490a6d217be29c059f390dedc9 - batt_info->fVolt;
                bmt_dbg("_974abc4b8280dd5eefc77cd62892555e <= 0 _c13fbfe70b60448f0c4d931c0ed5ce4e: %d\n",_c13fbfe70b60448f0c4d931c0ed5ce4e);
                if(_c13fbfe70b60448f0c4d931c0ed5ce4e <= 10)
                {
                    gas_gauge->discharge_ratio = 1000;
                }
                else
                {
                    gas_gauge->discharge_ratio = _c13fbfe70b60448f0c4d931c0ed5ce4e * 1000 /10;

                }
            }
            else{
                if(batt_info->fRSOC > 10)
                    gas_gauge->discharge_ratio = 1000 * batt_info->sCaMAH / _974abc4b8280dd5eefc77cd62892555e;
                else 
                    gas_gauge->discharge_ratio = 1000 * (batt_info->sCaMAH-one_percent_rc + 4) / _974abc4b8280dd5eefc77cd62892555e;

            }
            bmt_dbg("target capacit: %d\n",_974abc4b8280dd5eefc77cd62892555e);

            bmt_dbg("rc_ratio: %d\n",gas_gauge->discharge_ratio);

                //bmt_dbg("----------------------------------------------------\n");
            gas_gauge->discharge_table_flag = 1;
            /*
            if(gas_gauge->discharge_ratio != 0)
            {
                gas_gauge->discharge_table_flag = 1;
            }
            */
            if(gas_gauge->discharge_strategy != 1)
            {
                gas_gauge->discharge_table_flag = 0;
            }
        }
        
        _d0be2b3e92e7dfacbc78a82184fda19d(34939);
        _2736028ff10591dfcc7c7f7b0a2b1cbf(10144);
        _f18c3f29d0ea6c42e9b2a70a25a267d6();

		_f9839088a030a3cf3b1971064986f69c = 10;
    }
  
}

MODULE_DESCRIPTION("FG Charger Driver");
MODULE_LICENSE("GPL v2");