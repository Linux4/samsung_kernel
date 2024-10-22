/**
 * \copyright
 * MIT License
 *
 * Copyright (c) 2024 Infineon Technologies AG
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE
 *
 * \endcopyright
 */
/**
 * @file   bitbang.c
 * @date   January, 2021
 * @brief  Implementation of SWI bitbang using GPIO
 */


#include "bitbang.h"
#include "helper.h"
#include "../authon_api.h"
#include "../authon_config.h"

//todo System porting required: implements platform dependency header for GPIO, if required.
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>

extern int SWI_GPIO_PIN;
extern AuthOn_Capability authon_capability;

//On PSoC platform, 1us Tau -> 7, 35, 60 (For direct power mode only)
//On PSoC platform, 3us Tau -> 24, 72, 120 (Minimum Tau for indirect power mode)
//On PSoC platform, 5us Tau -> 40, 120, 180

/**
 * @brief Time distance code representation of logic '0' is known as a Tau.
  <BR>This is configured with higher speed.
  <BR>This value needs to be configured with a number of NOP counts tuned for different platform.
  \todo System porting required: Tune the highspeed SWI logic '0' value also known as 1*Tau
 */
extern uint32_t Highspeed_BaudLow;

/**
 * @brief Time distance code representation of logic '1' has a value of 3 * Tau.
 <BR>This is configured with higher speed.
 <BR>This value needs to be configured with a number of NOP counts tuned for different platform.
 \todo System porting required: Tune the highspeed SWI logic '1' value also known as 3*Tau
 */
extern uint32_t Highspeed_BaudHigh;

/**
 * @brief Time distance code representation of logic 'Stop' has a value of 5 * Tau.
  <BR>This is configured with higher speed.
  <BR>This value needs to be configured with a number of NOP counts tuned for different platform.
  \todo System porting required: Tune the highspeed SWI logic Stop value also known as 5*Tau
 */
extern uint32_t Highspeed_BaudStop;

/**
 * @brief Device response time out. The device has a timeout value of 10 * Tau.
  <BR>This is configured with higher speed.
  <BR>This value needs to be configured with a number of NOP counts tuned for different platform.
\todo System porting required: Tune the highspeed SWI logic response timeout period
 */
extern uint32_t Highspeed_ResponseTimeOut;

/**
 * @brief Time distance code representation of logic '0' is known as a Tau.
  <BR>This is configured with lower speed.
  <BR>This value needs to be configured with a number of NOP counts tuned for different platform.
\todo System porting required: Tune the lowspeed SWI logic '0' value also known as 1*Tau
 */
#define Lowspeed_BaudLow 90 /*!< 1 * TAU(15us) */

/**
 * @brief Time distance code representation of logic '1' has a value of 3 * Tau.
 <BR>This is configured with lower speed.
 <BR>This value needs to be configured with a number of NOP counts tuned for different platform.
  \todo System porting required: Tune the lowspeed SWI logic '1' value also known as 3*Tau
 */
#define Lowspeed_BaudHigh 310 /*!< 3 * TAU */

/**
 * @brief Time distance code representation of logic 'Stop' has a value of 5 * Tau.
  <BR>This is configured with lower speed.
  <BR>This value needs to be configured with a number of NOP counts tuned for different platform.
  \todo System porting required: Tune the lowspeed SWI Stop value also known as 5*Tau
 */
#define Lowspeed_BaudStop 400 /*!< 5 * TAU */

/**
 * @brief Device response time out. The device has a timeout value of 10 * Tau.
  <BR>This is configured with lower speed.
  <BR>This value needs to be configured with a number of NOP counts tuned for different platform.
  \todo System porting required: Tune the lowspeed SWI timeout value
 */
#define Lowspeed_ResponseTimeOut 3000 /*!< 10 * TAU */

/**
 * @brief Wake up time in us
  <BR>This value needs to be configured for device wake up time.
  \todo System porting required: Tune the SWI time to be approximately 7000us
 */
#define WakeUpDelay_Time        7000 /*!< Must be greater than 7000us */

/**
 * @brief Wake up Low time in us
  <BR>This value needs to be configured for device wake up low time.
  \todo System porting required: Tune the SWI time to be approximately 10us
 */
#define WakeUpLow_Time        10  /*!< Must be greater than 10us */

/**
 * @brief Wake up filter time in us
  <BR>This value needs to be configured for device wake up filter.
  \todo System porting required: Tune the SWI time to be approximately 28us
 */
#define WakeUpFilter_Time        28  /*!< Must be greater than 28us */

/**
 * @brief Power down time in us
  <BR>This value needs to be configured for power down time.
  \todo System porting required: Tune delay timing
 */
extern uint32_t PowerDownDelay_Time; /*! 2000us for direct power mode; >2500 for indirect power mode */

/**
 * @brief Power up time time in us
  <BR>This value needs to be configured for power up time.
  \todo System porting required: Tune delay timing
 */
extern uint32_t PowerUpDelay_Time; /*! 8000us for direct power mode; 10000 for indirect power mode */

/**
 * @brief Device reset time in us
  <BR>This value needs to be configured for reset time.
  \todo System porting required: Tune the SWI time to approximately 1000us
 */
#define ResetDelay_Time         1000 /*!< >1000us for reset time */

/**
 * @brief Number of NVM retries before timeout. Maximum for 5100us.
  <BR>This value needs to be configured with a number of retry counts for different platform.
  \todo System porting required: Tune the SWI time to be approximately 5100us.
 */
#define NvmRetryCount            10000 /*!< NVM retry loop > 5100us */

/**
 * @brief Retry count for ECC Response. Maximum for 100ms.
  <BR>This value needs to be configured with a number of retry counts for different platform.
  \todo System porting required: Tune the SWI time to approximately 100ms
 */
#define ECCResponseLongRetry     200000 /*!< >100ms for ECC max time */

static uint32_t ulBaudLow;               /*!< 1 Tau */
static uint32_t ulBaudHigh;              /*!< 3 Tau */
static uint32_t ulBaudStop;              /*!< 5 Tau */
static uint32_t ulResponseTimeOut;       /*!< 10 Tau */

static uint32_t ulPowerDownDelay_Time;   /*! 2000us for direct power mode; >2500 for indirect power mode */
static uint32_t ulPowerUpDelay_Time;     /*! 8000us for direct power mode; 10000 for indirect power mode */
static uint32_t ulWakeUpDelay_Time = WakeUpDelay_Time;       /*! tWK,DELAY >7000us */
static uint32_t ulWakeUpLow_Time = WakeUpLow_Time;           /*! tWK,LOW 10us */
static uint32_t ulWakeUpFilter_Time = WakeUpFilter_Time;     /*! tWK 28us */
static uint32_t ulResetDelay_Time = ResetDelay_Time;         /*! tSRD >1000us */

/**
* @brief This is the number of NVM retries and it should takes longer than 5100us
*/
static uint32_t ulNvmRetryCount = NvmRetryCount; /*! operation of the total NVM loop retries should within 5.1ms */

/**
* @brief This is the number of ECC retries and it should takes longer than 100ms
*/
static uint32_t ulResponseLongRetry = ECCResponseLongRetry; /*!< >Retry count should last longer than 100ms for ECC max time */

/**
* @brief Set SWI Default baud rate to highspeed
*/
void Set_SWIDefaultHighSpeed(void) {
  ulBaudLow = Highspeed_BaudLow;
  ulBaudHigh = Highspeed_BaudHigh;
  ulBaudStop = Highspeed_BaudStop;
  ulResponseTimeOut = Highspeed_ResponseTimeOut;
  ulPowerDownDelay_Time = PowerDownDelay_Time;
  if(PowerUpDelay_Time>ulWakeUpFilter_Time){
    ulPowerUpDelay_Time = PowerUpDelay_Time;
  }
}

/**
* @brief Set SWI Default baud rate to lowspeed
*/
void Set_SWIDefaultLowSpeed(void) {
  ulBaudLow = Lowspeed_BaudLow;
  ulBaudHigh = Lowspeed_BaudHigh;
  ulBaudStop = Lowspeed_BaudStop;
  ulResponseTimeOut = Lowspeed_ResponseTimeOut;
  ulPowerDownDelay_Time = PowerDownDelay_Time;
  ulPowerUpDelay_Time = PowerUpDelay_Time;
  if(PowerUpDelay_Time>ulWakeUpFilter_Time){
    ulPowerUpDelay_Time = PowerUpDelay_Time;
  }
}

/**
* @brief Read GPIO value.
* \todo System porting required: Modify this function to read GPIO state.
*/
uint8_t GPIO_read(void) {
	uint8_t ret = 0xFF;
//todo System porting required: implements platform read GPIO
	ret =  gpio_get_value(SWI_GPIO_PIN);
	return ret;
}


/**
* @brief Writes GPIO level.
* \todo System porting required: Modify this function to change GPIO state.
*/
void GPIO_write(uint8_t level) {  
  if (level == GPIO_LEVEL_HIGH) {
//todo System porting required: implements platform write GPIO high
	 gpio_set_value(SWI_GPIO_PIN, 1);	// Output High

  } else if (level == GPIO_LEVEL_LOW) {
//todo System porting required: implements platform write GPIO low
      gpio_set_value(SWI_GPIO_PIN, 0);	// Output Low  
  }
}

/**
* @brief Change the GPIO direction.
* @param dir Set the direction of the GPIO GPIO_DIRECTION_IN(0) or GPIO_DIRECTION_OUT(1)
* @param output_state Set the initial output value
* \todo System porting required: Modify this function to change direction of GPIO function.
*/
void GPIO_set_dir(uint8_t dir, uint8_t output_state) {
  if (dir == GPIO_DIRECTION_IN) {
//todo System porting required: implements platform set GPIO as input
    //gpiod_direction_input(authon_swi_gpio);
	gpio_direction_input(SWI_GPIO_PIN);	// set gpio as input
  } else {
//todo System porting required: implements platform set GPIO as output
    if(output_state==1){
        gpio_direction_output(SWI_GPIO_PIN, 1); // set gpio as output 1
    }else if(output_state==0){
        gpio_direction_output(SWI_GPIO_PIN, 0); // set gpio as output 0
    }
  }
}

/**
* @brief Change the Tau low baud rate.
* @param value Set the value of the low baud rate.
*/
void Set_SWILowBaud(uint32_t value) {
  ASSERT(value != 0); /*! SWI timing is not valid */
  ulBaudLow = value;
}

/**
* @brief Change the Tau high baud rate.
* @param value Set the value of the high baud rate.
*/
void Set_SWIHighBaud(uint32_t value) {
  ASSERT(value != 0); /*! SWI timing is not valid */
  ulBaudHigh = value;
}

/**
* @brief Change the Tau stop baud rate.
* @param value Set the value of the stop baud rate.
*/
void Set_SWIStopBaud(uint32_t value) {
  ASSERT(value != 0); /*! SWI timing is not valid */
  ulBaudStop = value;
}
/**
* @brief Change the Tau timeout baud rate.
* @param value Set the value of the timeout baud rate.
*/
void Set_SWITimeoutBaud(uint32_t value) {
  ASSERT(value != 0); /*! SWI timing is not valid */
  ulResponseTimeOut = value;
}
/**
* @brief Change the ECC timeout baud rate.
* @param value Set the value of the ECC timeout.
*/
void EccRetryCount(uint32_t value) {
  ASSERT(value != 0); /*! SWI timing is not valid */
  ulResponseLongRetry = value;
}

/**
* @brief Change the power down time in us.
* @param value Set the value of the power down time.
*/
void Set_PowerdownTime(uint32_t value) {
  ASSERT(value != 0); /*! SWI timing is not valid */
  ulPowerDownDelay_Time = value;
}

/**
* @brief Change the power up time in us.
* @param value Set the value of the power up time.
*/
void Set_PowerupTime(uint32_t value) {
  ASSERT(value != 0); /*! SWI timing is not valid */
  ulPowerUpDelay_Time = value;
}

/**
* @brief Change the reset time in us.
* @param value Set the value of the reset time.
*/
void Set_ResetTime(uint32_t value) {
  ASSERT(value != 0); /*! SWI timing is not valid */
  ulResetDelay_Time = value;
}

/**
* @brief Change the NVM timeout loop count.
* @param value Set the value of the NVM timeout loop count.
*/
void Set_NVMTimeOutCount(uint32_t value) {
  ASSERT(value != 0); /*! SWI timing is not valid */
  ulNvmRetryCount = value;
}
/**
* @brief Returns the Low baud rate.
*/
uint32_t Get_SWILowBaud(void) {
  ASSERT(ulBaudLow != 0); /*! SWI timing is not initialized */
  return Highspeed_BaudLow;
}

/**
* @brief Returns the High baud rate.
*/
uint32_t Get_SWIHighBaud(void) {
  ASSERT(ulBaudHigh != 0); /*! SWI timing is not initialized */
  return Highspeed_BaudHigh;
}

/**
* @brief Returns the Stop baud rate.
*/
uint32_t Get_SWIStopBaud(void) {
  ASSERT(ulBaudStop != 0); /*! SWI timing is not initialized */
  return Highspeed_BaudStop;
}

/**
* @brief Returns the timeout baud rate.
*/
uint32_t Get_SWITimeoutBaud(void) {
  ASSERT(ulResponseTimeOut != 0); /*! SWI timing is not initialized */
  return Highspeed_ResponseTimeOut;
}

/**
* @brief Returns the Wake up time in us.
*/
uint32_t Get_WakeupTime(void){
  return ulWakeUpDelay_Time;
}

/**
* @brief Returns the Wake up Low time in us.
*/
uint32_t Get_WakeupLowTime(void){
  return ulWakeUpLow_Time;
}

/**
* @brief Change the wakeup time in us.
* @param value Set the value of the wakeup time.
*/
void Set_WakeupTime(uint32_t value) {
  ulWakeUpDelay_Time = value;
}

/**
* @brief Returns the Power down time in us.
*/
uint32_t Get_PowerdownTime(void){
  return PowerDownDelay_Time;
  //return ulPowerDownDelay_Time;
}

/**
* @brief Returns the Power up time in us
*/
uint32_t Get_PowerupTime(void){
  return PowerUpDelay_Time;
  //return ulPowerUpDelay_Time;
}

/**
* @brief Returns the Reset time in us.
*/
uint32_t Get_ResetTime(void){
  return ulResetDelay_Time;
}

/**
 * @brief Returns the NVM time out count.
 */
uint32_t Get_NVMTimeOutCount(void) {
	return ulNvmRetryCount;
}

/**
* @brief Returns the ECC retry count.
*/
uint32_t Get_EccRetryCount(void) {
  ASSERT(ulResponseLongRetry != 0); /*! SWI timing is not initialized */
  return ulResponseLongRetry;
}
