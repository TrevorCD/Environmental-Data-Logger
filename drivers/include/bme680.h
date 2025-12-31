/*
 *   Copyright 2025 Trevor Calderwood
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 *----------------------------------------------------------------------------*/
/*
 * This device driver is for the BME680 Environmental Sensor and  STM32F4xx.
 *
 *----------------------------------------------------------------------------*/

#ifndef BME680_H
#define BME680_H

/* Hardware Includes */
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"

/* Ouput data updated by BME680_Poll */
typedef struct {
	int32_t humidity;
	int32_t temperature;
	int32_t pressure;
	int32_t gas_resistance;
} BME680_OutputTypeDef;

typedef struct {
	/* temperature */
	uint16_t par_t1;
	int16_t par_t2;
	int8_t  par_t3;
	/* pressure */
	uint16_t par_p1;
	int16_t par_p2;
	int8_t  par_p3;
	int16_t par_p4;
	int16_t par_p5;
	int8_t  par_p6;
	int8_t  par_p7;
	int16_t par_p8;
	int16_t par_p9;
	uint8_t  par_p10;
	/* humidity */
	uint16_t par_h1;
	uint16_t par_h2;
	int8_t  par_h3;
	int8_t  par_h4;
	int8_t  par_h5;
	uint8_t  par_h6;
	int8_t  par_h7;
	/* gas */
	int8_t  par_g1;
	int16_t par_g2;
	int8_t  par_g3;

	int8_t range_sw_err;
	
} BME680_CalibrationTypeDef;

/* BME680_HandleTypeDef: Device Context.
 * Contains I2C handle, output, and calibration parameters. */
typedef struct {
	/* I2C Handle must be initialized prior to device initialization */
	I2C_HandleTypeDef *hi2c;
	
	/* temperature values for BME680_Calc_Res_Heat */
	int32_t amb_temp;
	int32_t old_amb_temp;
	int32_t target_temp;

	/* value used by BME680_Get... functions */
	int32_t t_fine;

	/*  */
	uint8_t res_heat_0;

	/* SET THIS TO 0 BEFORE CALLING BME680_Init! */
	uint8_t initialized;
	
	/* Output that is updated by BME680_Poll */
	BME680_OutputTypeDef output;
	
	/* Calibration Parameters */
	BME680_CalibrationTypeDef calib;
	
} BME680_HandleTypeDef;

/* Public functions ----------------------------------------------------------*/

/* BME680_Init:
 *
 * Initializes the BME680 device registers
 *
 * Pre Requirements:
 *
 *  - dev->initialized != 1
 *
 *  - __HAL_RCC_GPIOB_CLK_ENABLE();
 *  - __HAL_RCC_I2C1_CLK_ENABLE();
 *
 *  - dev->sda_pin & dev->scl_pin initialized with GPIO_InitTypeDef fields:
 *    - Pin = dev->sda_pin | dev->scl_pin
 *    - Mode = GPIO_MODE_AF_OD
 *    - Pull = GPIO_PULLUP
 *    - Speed = GPIO_SPEED_FREQ_VERY_HIGH
 *    - Alternate = GPIO_AF4_I2Cx, where I2Cx corresponds to the Instance
 *      field on dev->hi2c.Instance
 *
 *  - dev->hi2c must be initialized:
 *    - dev->hi2c->Instance, and all dev->hi2c->Init fields must be set as:
 *      - ClockSpeed = 100000
 *      - DutyCycle = I2C_DUTYCYCLE_2
 *      - OwnAddress1 = 0
 *      - AddressingMode = I2C_ADDRESSINGMODE_7BIT
 *      - DualAddressMode = I2C_DUALADDRESS_DISABLE
 *      - OwnAddress2 = 0
 *      - GeneralCallMode = I2C_GENERALCALL_DISABLE
 *      - NoStretchMode = I2C_NOSTRETCH_DISABLE
 *    - HAL_I2C_Init(&dev->hi2c) must return HAL_OK.
 *
 * Returns:
 *   0 on success
 *  -1 on failure
 */
int BME680_Init(BME680_HandleTypeDef *dev);

/* BME680_Poll:
 * 
 * Polls the BME680 for data on each sensor, and fills the dev->output fields
 * with the new data.
 *
 * Pre Requirements:
 *  - dev is initialized with BME680_Init()
 *
 * Returns:
 *   0 on success
 *  -1 on failure
 */
int BME680_Poll(BME680_HandleTypeDef *dev);

#endif /* BME680_H */
