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
 *----------------------------------------------------------------------------
 *
 * This device driver is for the BME680 Environmental Sensor and STM23f4xx
 *
 *----------------------------------------------------------------------------*/

/* Hardware Includes */
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"

#include <stdio.h> /* ONLY USE printf()! */

#include "bme680.h"

/*----------------------------------------------------------------------------*/

#define TARGET_TEMP    300
#define AMB_TEMP       25 /* Initial ambient temperature */

/* I2C GPIO Pins */
#define BME680_SDA     GPIO_PIN_9
#define BME680_SCL     GPIO_PIN_8

#define SLAVE_ADDR     (0x76U << 1) /* 0x77 << 1 (7 bit addr; HAL expects 8 */

/* Timing values */
#define READ_TIMEOUT   (100) /* timeout on HAL_I2C_Mem_Read */
#define POLL_DELAY     (10)  /* delay on BME680_Data_Ready in BME680_Poll */

/* BME680 Read/Write Registers */
#define CTRL_MEAS      (0x74U) /* osrs_t<7:5> osrs_p<4:2> mode<1:0> */
#define CTRL_HUM       (0x72U) /* spi_3w_int_en<6> osrs_h<2:0> */
#define CTRL_GAS_1     (0x71U) /* run_gas<4> nb_conv<3:0> */
#define GAS_WAIT_0     (0x64U)
#define RES_HEAT_0     (0x5AU)

/* BME680 Read Registers */
#define PAR_T1_LSB     (0xE9U)
#define PAR_T1_MSB     (0xEAU)
#define PAR_T2_LSB     (0x8AU)
#define PAR_T2_MSB     (0x8BU)
#define PAR_T3         (0x8CU)
#define PAR_P1_LSB     (0x8EU)
#define PAR_P1_MSB     (0x8FU)
#define PAR_P2_LSB     (0x90U)
#define PAR_P2_MSB     (0x91U)
#define PAR_P3         (0x92U)
#define PAR_P4_LSB     (0x94U)
#define PAR_P4_MSB     (0x95U)
#define PAR_P5_LSB     (0x96U)
#define PAR_P5_MSB     (0x97U)
#define PAR_P6         (0x99U)
#define PAR_P7         (0x98U)
#define PAR_P8_LSB     (0x9CU)
#define PAR_P8_MSB     (0x9DU)
#define PAR_P9_LSB     (0x9EU)
#define PAR_P9_MSB     (0x9FU)
#define PAR_P10        (0xA0U)
#define PAR_H1_LSB     (0xE2U) /* <3:0> */
#define PAR_H1_MSB     (0xE3U)
#define PAR_H2_LSB     (0xE2U) /* <7:4> */
#define PAR_H2_MSB     (0xE1U)
#define PAR_H3         (0xE4U)
#define PAR_H4         (0xE5U)
#define PAR_H5         (0xE6U)
#define PAR_H6         (0xE7U)
#define PAR_H7         (0xE8U)
#define PAR_G1         (0xEDU)
#define PAR_G2_LSB     (0xEBU)
#define PAR_G2_MSB     (0xECU)
#define PAR_G3         (0xEEU)
#define RES_HEAT_RANGE (0x02U) /* ONLY <5:4> */
#define RES_HEAT_VAL   (0x00)

/* BME680 Status Read Registers */
#define CHIP_ID        (0xD0U)
#define EAS_STATUS_0   (0x1DU) /* new_data_0<7> gas_measuring<6> measuring<5>
								* reserved<4> gas_meas_index_0<3:0> */
/* BME680 Data Read Registers */
#define GAS_R_LSB      (0x2BU) /* <7:6>gas_r<1:0> gas_valid_r<5>
								* heat_stab_r<4> gas_range_r<3:0>*/
#define GAS_R_MSB      (0x2AU) /* <7:0>gas_r<9:2> */
#define HUM_LSB        (0x26U)
#define HUM_MSB        (0x25U)
#define TEMP_XLSB      (0x24U) /* ONLY <7:4> */
#define TEMP_LSB       (0x23U)
#define TEMP_MSB       (0x22U)
#define PRESS_XLSB     (0x21U) /* ONLY <7:4> */
#define PRESS_LSB      (0x20U)
#define PRESS_MSB      (0x20U)

/* Oversampling values */
#define OVERSAMPLE_H   (0b001)
#define OVERSAMPLE_T   (0b010)
#define OVERSAMPLE_P   (0b101) /* translates to x16 oversample */

/* Mode values: write to CTRL_MEAS<1:0> */
#define MODE_SLEEP     (0U)
#define MODE_FORCED    (1U)

/* Public function prototypes ------------------------------------------------*/
int BME680_Init(BME680_HandleTypeDef *dev);
int BME680_Poll(BME680_HandleTypeDef *dev);

/* Private function prototypes -----------------------------------------------*/
static int BME680_Read(BME680_HandleTypeDef *dev, uint8_t reg, uint8_t *data);
static int BME680_Transmit(BME680_HandleTypeDef *dev, uint8_t reg, uint8_t data);
static int BME680_Data_Ready(BME680_HandleTypeDef *dev);
static int BME680_Calc_Res_Heat(BME680_HandleTypeDef *dev);
static int BME680_Get_Calibration(BME680_HandleTypeDef *dev);
static int BME680_Get_Temp(BME680_HandleTypeDef *dev);
static int BME680_Get_Press(BME680_HandleTypeDef *dev);
static int BME680_Get_Hum(BME680_HandleTypeDef *dev);
static int BME680_Get_Gas_R(BME680_HandleTypeDef *dev);
static int BME680_Read_2(BME680_HandleTypeDef *dev, uint8_t msb, uint8_t lsb,
						 uint16_t *data);
static const char* HAL_StatusToString(HAL_StatusTypeDef status);
/*----------------------------------------------------------------------------*/

/* BME680_Init:
 *
 * Initializes the BME680 device registers
 *
 * Pre Requirements:
 *  - __HAL_RCC_GPIOB_CLK_ENABLE();
 *  - __HAL_RCC_I2C1_CLK_ENABLE();
 *
 *  - dev->sda_pin & dev->scl_pin initialized with GPIO_InitTypeDef fields:
 *    - Pin = dev->sda_pin | dev->scl_pin
 *    - Mode = GPIO_MODE_AF_OD
 *    - Pull = GPIO_PULLUP\
 *    - Speed = GPIO_SPEED_FREQ_VERY_HIGH
 *    - Alternate = GPIO_AF4_I2Cx, where I2Cx corresponds to the Instance
 *      field on dev->hi2c.Instance
 *
 *  - dev->hi2c must be initialized:
 *    - dev->hi2c->Instance, and all dev->hi2c->Init fields must be set as:
 *      - ClockSpeed = 100000;
 *      - DutyCycle = I2C_DUTYCYCLE_2
 *      - OwnAddress1 = 0
 *      - AddressingMode = I2C_ADDRESSINGMODE_7BIT
 *      - DualAddressMode = I2C_DUALADDRESS_DISABLE
 *      - OwnAddress2 = 0
 *      - GeneralCallMode = I2C_GENERALCALL_DISABLE
 *      - NoStretchMode = I2C_NOSTRETCH_DISABLE
 *    - HAL_I2C_Init(dev->hi2c) must return HAL_OK.
 *
 * Returns:
 *   0 on success
 *  -1 on failure
 */
int BME680_Init(BME680_HandleTypeDef *dev)
{
	HAL_StatusTypeDef status;
	
	if(dev->initialized == 1)
	{
		printf("BME680_Init: Device already initialized!\n");
		return -1;
	}

	if((status = HAL_I2C_IsDeviceReady(dev->hi2c, SLAVE_ADDR, 3, 50)) != HAL_OK)
	{
		printf("BME680_Init: Device not ready! Hal returned: %s\n",
			   HAL_StatusToString(status));
		return -1;
	}

	
	/* Set output data to INT32_MIN to represent uncalculated values */
	dev->output.humidity = INT32_MIN;
	dev->output.temperature = INT32_MIN;
	dev->output.pressure = INT32_MIN;
	dev->output.gas_resistance = INT32_MIN;

	/* Set initial temperature values (target_temp does not change) */
	dev->amb_temp = AMB_TEMP;
	dev->old_amb_temp = AMB_TEMP;
	dev->target_temp = TARGET_TEMP;
	
	if(BME680_Get_Calibration(dev) != 0)
	{
		printf("BME680_Init: Failed to get calibration parameters!\n");
		return -1;
	}
	
  	/* Set humidity oversample to 1 by writing 0b001 to CTRL_HUM */
	/* This also sets spi_3w_int_en to 0 */
	if(BME680_Transmit(dev, CTRL_HUM, OVERSAMPLE_H) != 0)
	{
		printf("BME680_Init: Humidity oversample transmit FAIL!\n");
		return -1;
	}
	
	/* Set CTRL_MEAS. This reg contains OSRS_T<7:5> OSRS_P<4:2> MODE<1:0> */
	/* Mode bits left as 0b00 to leave in sleep mode */
	if(BME680_Transmit(dev, CTRL_MEAS,
					   (OVERSAMPLE_P << 2) | (OVERSAMPLE_T << 5)) != 0)
	{
		printf("BME680_Init: Pressure & temp oversample transmit FAIL!\n");
		return -1;
	}
	
	/* Set GAS_WAIT_0<7:0> to 0x59 to select 100 ms heat up duration */
	if(BME680_Transmit(dev, GAS_WAIT_0, (0x59U)) != 0)
	{
		printf("BME680_Init: gas_wait_0 transmit FAIL!\n");
		return -1;
	}
	
	/* Set corresponding heater set-point by writing target heater resistance to
	 * RES_HEAT_0<7:0> */
	if(BME680_Calc_Res_Heat(dev) != 0)
	{
		printf("BME680_Init: cal_res_heat failure!\n");
		return -1;
	}
	if(BME680_Transmit(dev, RES_HEAT_0, dev->res_heat_0) != 0)
	{
		printf("BME680_Init: res_heat_0 transmit FAIL!\n");
		return -1;
	}
	
	/* In CTRL_GAS, set nb_conv<3:0> to 0x0 and run_gas<4> to 1 */
	if(BME680_Transmit(dev, CTRL_GAS_1, (1U << 4)) != 0)
	{
		printf("BME680_Init: ctrl_gas_1 transmit FAIL!\n");
		return -1;
	}

	dev->initialized = 1;
	
	return 0;
}

/*----------------------------------------------------------------------------*/

/* Polls the BME680 for data on each sensor, and fills the dev->output fields
 * with the new data.
 *
 * Pre Requirements:
 *  - dev is initialized with BME680_Init()
 *
 * Returns:
 *   0 on success
 *  -1 on failure
 */
int BME680_Poll(BME680_HandleTypeDef *dev)
{
	int32_t temp, press, hum, gas_r;
	uint8_t old_ctrl_meas, eas_status;

	if(dev->initialized != 1)
	{
		printf("BME680_Poll: Device not initialized!\n");
		return -1;
	}
	
	/* Set MODE<1:0> to 0b01 (MODE_FORCED) to trigger a single measurement */
	if(BME680_Read(dev, CTRL_MEAS, &old_ctrl_meas) != 0)
	{
		printf("BME680_Poll: Failed to read CTRL_MEAS!\n");
		return -1;
	}
	if(BME680_Transmit(dev, CTRL_MEAS, old_ctrl_meas | MODE_FORCED) != 0)
	{
		printf("BME680_Poll: CTRL_MEAS Transmit FAIL!\n");
		return -1;
	}

	/* 100 ms delay to wait for heat up duration */
	HAL_Delay(100);
	
	/* wait for new data */
	while((eas_status = BME680_Data_Ready(dev)) != 1)
	{
		if(eas_status != 0)
		{
			/* BME680_Data_Ready already prints failure msg */
			return -1;
		}
		HAL_Delay(POLL_DELAY);
	}

	/* process data from BME680 output registers and move to output struct */
	if(BME680_Get_Temp(dev) != 0)
	{
		return -1;
	}
	if(BME680_Get_Press(dev) != 0)
	{
		return -1;
	}
	if(BME680_Get_Hum(dev) != 0)
	{
		return -1;
	}
	if(BME680_Get_Gas_R(dev) != 0)
	{
		return -1;
	}

	/* Update the target heater resistance if the ambient temperature changes */
	if(dev->amb_temp != dev->old_amb_temp)
	{
		if(BME680_Calc_Res_Heat(dev) != 0)
		{
			printf("BME680_Poll: cal_res_heat failure!\n");
			return -1;
		}
		if(BME680_Transmit(dev, RES_HEAT_0, dev->res_heat_0) != 0)
		{
			printf("BME680_Poll: res_heat_0 transmit FAIL!\n");
			return -1;
		}	
	}
	
	return 0;
}

/*----------------------------------------------------------------------------*/

static int BME680_Get_Hum(BME680_HandleTypeDef *dev)
{
	uint8_t msb, lsb;
	uint16_t hum_adc;
	int32_t temp_comp, temp_scaled, hum_comp;
	int32_t var1, var2, var3, var4, var5, var6;
	
	/* read MSB then LSB for humidity */
	if(BME680_Read(dev, HUM_MSB, &msb) != 0)
	{
		printf("BME680_Get_Hum: Failed to read HUM_MSB!\n");
		return -1;
	}
	if(BME680_Read(dev, HUM_LSB, &lsb) != 0)
	{
		printf("BME680_Get_Hum: Failed to read HUM_LSB!\n");
		return -1;
	}
	hum_adc = ((uint16_t)msb << 8) | (uint16_t)lsb;

	temp_comp = dev->output.temperature;

	/* following code is from BME680 datasheet page 20 */
	temp_scaled = (int32_t)temp_comp;
	var1 = (int32_t)hum_adc - (int32_t)((int32_t)dev->calib.par_h1 << 4) -
		(((temp_scaled * (int32_t)dev->calib.par_h3) / ((int32_t)100)) >> 1);
	var2 = ((int32_t)dev->calib.par_h2 *
			(((temp_scaled * (int32_t)dev->calib.par_h4) / ((int32_t)100)) +
			 (((temp_scaled * ((temp_scaled * (int32_t)dev->calib.par_h5) /
							   ((int32_t)100))) >> 6) / ((int32_t)100)) +
			 ((int32_t)(1 << 14)))) >> 10;
	var3 = var1 * var2;
	var4 = (((int32_t)dev->calib.par_h6 << 7) +
			((temp_scaled * (int32_t)dev->calib.par_h7) / ((int32_t)100))) >> 4;
	var5 = ((var3 >> 14) * (var3 >> 14)) >> 10;
	var6 = (var4 * var5) >> 1;
	hum_comp = (((var3 + var6) >> 10) * ((int32_t) 1000)) >> 12;

	dev->output.humidity = hum_comp;
	
	return 0;
}

/*----------------------------------------------------------------------------*/

static int BME680_Get_Gas_R(BME680_HandleTypeDef *dev)
{
	uint8_t msb, lsb, gas_range, range_switching_error;
	uint16_t gas_adc; /* raw gas resistance value */

	/* hardcoded const_array1_int and const_array2_int from datasheet page 23 */
	const uint32_t const_array1_int[16] = {2147483647, 2147483647, 2147483647,
		2147483647, 2147483647, 2126008810, 2147483647, 2130303777, 2147483647,
		2147483647, 2143188679, 2136746228, 2147483647, 2126008810, 2147483647,
		2147483647};
	const uint32_t const_array2_int[16] = {4096000000, 2048000000, 1024000000,
		512000000, 2557442255, 127110228, 64000000, 32258064, 16016016,
		8000000, 4000000, 2000000, 1000000, 500000, 250000, 125000};
	
	/* get bits <9:2> of gas_adc from GAS_R_MSB<7:0> */
	if(BME680_Read(dev, GAS_R_MSB, &msb) != 0)
	{
		printf("BME680_Get_Gas_R: Failed to read GAS_R_MSB!\n");
		return -1;
	}
	/* get bits <1:0> of gas_adc from GAS_R_LSB<7:6> */
	if(BME680_Read(dev, GAS_R_LSB, &lsb) != 0)
	{
		printf("BME680_Get_Gas_R: Failed to read GAS_R_LSB!\n");
		return -1;
	}
	gas_adc = ((uint16_t)msb << 2) | (((uint16_t)lsb >> 6) & 0x3U);
	
	gas_range = lsb & 0xFU; /* gas_range is bits <3:0> of GAS_R_LSB */
	
	/* get range_switching_error */
	if(BME680_Read(dev, (0x04U), &range_switching_error) != 0)
	{
		printf("BME_Get_Gas_R: Failed to read RANGE_SWITCHING_ERROR!\n");
		return -1;
	}
	
	/* following code is from BME680 datasheet page 23 */
	int64_t var1 = (int64_t)(((1340 + (5 * (int64_t)range_switching_error)) *
							  ((int64_t)const_array1_int[gas_range])) >> 16);
	int64_t var2 = (int64_t)(gas_adc << 15) - (int64_t)(1 << 24) + var1;
	int32_t gas_res = (int32_t)
		((((int64_t)(const_array2_int[gas_range] *(int64_t)var1) >> 9)
		  + (var2 >> 1)) / var2);

	dev->output.gas_resistance = gas_res;
	
	return 0;
}

/*----------------------------------------------------------------------------*/

static int BME680_Get_Press(BME680_HandleTypeDef *dev)
{
	uint32_t press_adc; /* raw pressure output data */
	int32_t var1, var2, var3, press_comp;
	uint8_t msb, lsb, xlsb;
	
	/* read MSB, LSB, XLSB for pressure */
	if(BME680_Read(dev, PRESS_MSB, &msb) != 0)
	{
		printf("BME680_Get_Press: Failed to read PRESS_MSB!\n");
		return -1;
	}
	if(BME680_Read(dev, PRESS_LSB, &lsb) != 0)
	{
		printf("BME680_Get_Press: Failed to read PRESS_LSB!\n");
		return -1;
	}
	if(BME680_Read(dev, PRESS_XLSB, &xlsb) != 0)
	{
		printf("BME680_Get_Press: Failed to read PRESS_XLSB!\n");
		return -1;
	}
	
	/* construct press_adc from msb lsb and xlsb */
	press_adc = ((uint32_t)msb << 12)
		| ((uint32_t)lsb << 4)
		| ((uint32_t)xlsb >> 4);
	
	/* following code is from BME680 datasheet page 18/19 */
	var1 = ((int32_t)dev->t_fine >> 1) - 64000;
	var2 = ((((var1 >> 2) * (var1 >> 2)) >> 11) *
			(int32_t)dev->calib.par_p6) >> 2;
	var2 = var2 + ((var1 * (int32_t)dev->calib.par_p5) << 1);
	var2 = (var2 >> 2) + ((int32_t)dev->calib.par_p4 << 16);
	var1 = (((((var1 >> 2) * (var1 >> 2)) >> 13) *
			 ((int32_t)dev->calib.par_p3 << 5)) >> 3) +
		(((int32_t)dev->calib.par_p2 * var1) >> 1);
	var1 = var1 >> 18;
	var1 = ((32768 + var1) * (int32_t)dev->calib.par_p1) >> 15;
	press_comp = 1048576 - press_adc; /* datasheet says press_raw??? */
	press_comp = (uint32_t)((press_comp) - (var2 >> 12)) * ((uint32_t)3125);
	if(press_comp >= (1 << 30))
		press_comp = ((press_comp / (uint32_t)var1) << 1);
	else
		press_comp = ((press_comp << 1) / (uint32_t)var1);
	var1 = ((int32_t)dev->calib.par_p9 *
			(int32_t)(((press_comp >> 3) * (press_comp >> 3)) >> 13)) >> 12;
	var2 = ((int32_t)(press_comp >> 2) * (int32_t)dev->calib.par_p8) >> 13;
	var3 = ((int32_t)(press_comp >> 8) * (int32_t)(press_comp >> 8) *
			(int32_t)(press_comp >> 8) * (int32_t)dev->calib.par_p10) >> 17;
	press_comp = (int32_t)(press_comp) +
		((var1+ var2 + var3 + ((int32_t)dev->calib.par_p7 << 7)) >> 4);
	/* what */
	dev->output.pressure = press_comp;
	
	return 0;
}

/*----------------------------------------------------------------------------*/

static int BME680_Get_Temp(BME680_HandleTypeDef *dev)
{
	uint32_t temp_adc; /* raw (uncompensated) temperature reading */
	int32_t var1, var2, var3, t_fine, temp_comp;
	uint8_t msb, lsb, xlsb; /* bytes of temp_adc */

	/* read MSB, LSB, XLSB for temperature */
	if(BME680_Read(dev, TEMP_MSB, &msb) != 0)
	{
		printf("BME680_Get_Temp: Failed to read TEMP_MSB!\n");
		return -1;
	}
	if(BME680_Read(dev, TEMP_LSB, &lsb) != 0)
	{
		printf("BME680_Get_Temp: Failed to read TEMP_LSB!\n");
		return -1;
	}
	if(BME680_Read(dev, TEMP_XLSB, &xlsb) != 0)
	{
		printf("BME680_Get_Temp: Failed to read TEMP_XLSB!\n");
		return -1;
	}
	
	/* construct temp_adc from msb lsb and xlsb */
	temp_adc = ((uint32_t)msb << 12)
		| ((uint32_t)lsb << 4)
		| ((uint32_t)xlsb >> 4);
	
	/* Following code is from the BME680 Datasheet page 17 */
	var1 = ((int32_t)temp_adc >> 3) - ((int32_t)dev->calib.par_t1 << 1);
	var2 = (var1 * (int32_t)dev->calib.par_t2) >> 11;
	var3 = ((((var1 >> 1) * (var1 >> 1)) >> 12)
			* ((int32_t)dev->calib.par_t3 << 4)) >> 14;
	t_fine = var2 + var3;
	temp_comp = ((t_fine * 5) + 128) >> 8;

	/* t_fine value used for other sensor reading compensations */
	dev->t_fine = t_fine;
	dev->output.temperature = temp_comp;

	/* Update and cycle ambient temperature values */
	dev->old_amb_temp = dev->amb_temp;
	dev->amb_temp = temp_comp;
	
	return 0;
}

/*----------------------------------------------------------------------------*/

/* returns -1 on read fail, 1 if ready, 0 if not ready */
static int BME680_Data_Ready(BME680_HandleTypeDef *dev)
{
	uint8_t status;
	if(BME680_Read(dev, EAS_STATUS_0, &status) != 0)
	{
		printf("BME680_Data_Ready: Failed to read EAS_STATUS!\n");
		return -1;
	}
	uint8_t new_data = (status & (1 << 7));
	uint8_t measuring = (status & (1 << 5));

	return (new_data && (!measuring));
}

/*----------------------------------------------------------------------------*/

/* returns -1 on fail, 0 on success. Places byte at 'reg' in 'data' */
static int BME680_Read(BME680_HandleTypeDef *dev, uint8_t reg, uint8_t *data)
{
	if(HAL_I2C_Mem_Read(dev->hi2c, SLAVE_ADDR, reg, 1, data, 1, READ_TIMEOUT)
	   != HAL_OK)
	{
		return -1;
	}
	return 0;
}

/*----------------------------------------------------------------------------*/

/* returns -1 on fail, 0 on success.
 * constructs *data with the bytes at msb and lsb
 * only works on two byte long values that are aligned to byte boundaries
 */
static int BME680_Read_2(BME680_HandleTypeDef *dev, uint8_t msb, uint8_t lsb,
						 uint16_t *data)
{
	uint8_t temp;
	
	if(HAL_I2C_Mem_Read(dev->hi2c, SLAVE_ADDR, lsb, 1, &temp, 1, READ_TIMEOUT)
	   != HAL_OK)
	{
		return -1;
	}
	*data = ((uint16_t)temp);
	if(HAL_I2C_Mem_Read(dev->hi2c, SLAVE_ADDR, msb, 1, &temp, 1, READ_TIMEOUT)
	   != HAL_OK)
	{
		return -1;
	}
	
	*data |= ((uint16_t)temp << 8);
	
	return 0;
}

/*----------------------------------------------------------------------------*/

/* returns -1 on fail, 0 on success. Sends [reg][data] to BME680.
 * The BME680 writes data to reg. */
static int BME680_Transmit(BME680_HandleTypeDef *dev, uint8_t reg, uint8_t data)
{
	uint8_t msg[2];
	msg[0] = reg;
	msg[1] = data;
	if(HAL_I2C_Master_Transmit_DMA(dev->hi2c, SLAVE_ADDR, msg, 2) != HAL_OK)
	{
		return -1;
	}
	return 0;
}

/*----------------------------------------------------------------------------*/

static int BME680_Get_Calibration(BME680_HandleTypeDef *dev)
{
	uint8_t par_temp; /* temporary var for reading unaligned 2 byte params */
	
	/* Read par_t1-3 */
	if(BME680_Read_2(dev, PAR_T1_MSB, PAR_T1_LSB, &dev->calib.par_t1) != 0)
		return -1;
	if(BME680_Read_2(dev, PAR_T2_MSB, PAR_T2_LSB, &dev->calib.par_t2) != 0)
		return -1;
	if(BME680_Read(dev, PAR_T3, &dev->calib.par_t3) != 0)
		return -1;
	
	/* read par_p1-10 */
	if(BME680_Read_2(dev, PAR_P1_MSB, PAR_P1_LSB, &dev->calib.par_p1) != 0)
		return -1;
	if(BME680_Read_2(dev, PAR_P2_MSB, PAR_P2_LSB, &dev->calib.par_p2) != 0)
		return -1;
	if(BME680_Read(dev, PAR_P3, &dev->calib.par_p3) != 0)
		return -1;
	if(BME680_Read_2(dev, PAR_P4_MSB, PAR_P4_LSB, &dev->calib.par_p4) != 0)
		return -1;
	if(BME680_Read_2(dev, PAR_P5_MSB, PAR_P5_LSB, &dev->calib.par_p5) != 0)
		return -1;
	if(BME680_Read(dev, PAR_P6, &dev->calib.par_p6) != 0)
		return -1;
	if(BME680_Read(dev, PAR_P7, &dev->calib.par_p7) != 0)
		return -1;
	if(BME680_Read_2(dev, PAR_P8_MSB, PAR_P8_LSB, &dev->calib.par_p8) != 0)
		return -1;
	if(BME680_Read_2(dev, PAR_P9_MSB, PAR_P9_LSB, &dev->calib.par_p9) != 0)
		return -1;
	if(BME680_Read(dev, PAR_P10, &dev->calib.par_p10) != 0)
		return -1;

	/* read par_h1-7 */
	if(BME680_Read(dev, PAR_H1_LSB, &par_temp) != 0)
		return -1;
	dev->calib.par_h1 = ((uint16_t)par_temp & 0xFU);
	if(BME680_Read(dev, PAR_H1_MSB, &par_temp) != 0)
		return -1;
	dev->calib.par_h1 |= ((uint16_t)par_temp << 4);

	if(BME680_Read(dev, PAR_H2_LSB, &par_temp) != 0)
		return -1;
	dev->calib.par_h2 = ((uint16_t)par_temp >> 4) & 0xFU;
	if(BME680_Read(dev, PAR_H2_MSB, &par_temp) != 0)
		return -1;
	dev->calib.par_h2 |= ((uint16_t)par_temp << 4);

	if(BME680_Read(dev, PAR_H3, &dev->calib.par_h3) != 0)
		return -1;
	if(BME680_Read(dev, PAR_H4, &dev->calib.par_h4) != 0)
		return -1;
	if(BME680_Read(dev, PAR_H5, &dev->calib.par_h5) != 0)
		return -1;
	if(BME680_Read(dev, PAR_H6, &dev->calib.par_h6) != 0)
		return -1;
	if(BME680_Read(dev, PAR_H7, &dev->calib.par_h7) != 0)
		return -1;
	
	/* Read par_g1-3 */
	if(BME680_Read(dev, PAR_G1, &dev->calib.par_g1) != 0)
		return -1;
	if(BME680_Read_2(dev, PAR_G2_MSB, PAR_G2_LSB, &dev->calib.par_g2) != 0)
		return -1;
	if(BME680_Read(dev, PAR_G3, &dev->calib.par_g3) != 0)
		return -1;
	
	return 0;
}
 
/*----------------------------------------------------------------------------*/
 
/* places value to be written to res_heat_x in 'output' */
/* target_temp is the target heater temp in degrees Celsius */
/* amb_temp is the ambient temperature (hardcoded or read from temp sensor) */
/* par_g1, par_g2, par_g3 are calibration params */
static int BME680_Calc_Res_Heat(BME680_HandleTypeDef *dev)
{
	int32_t var1, var2, var3, var4, var5, res_heat_x100;
	int8_t res_heat_range, res_heat_val;
	
	/* Read res_heat_range */
	if(BME680_Read(dev, RES_HEAT_RANGE, &res_heat_range) != 0)
	{
		return -1;
	}
	/* res_heat_range is only bits <5:4> from the RES_HEAT_RANGE register */
	res_heat_range >>= 4;
	res_heat_range &= 0x03; /* two least sig bits */
	
	/* Read res_heat_val */
	if(BME680_Read(dev, RES_HEAT_VAL, &res_heat_val) != 0)
	{
		return -1;
	}
	
	/* The following code is from the BME680 datasheet page 21 */
	var1 = (((int32_t)(dev->amb_temp) * (dev->calib.par_g3)) / 10) << 8;
	var2 = (dev->calib.par_g1 + 784);
	var2 *= ((((dev->calib.par_g2 + 154009)
			   * dev->target_temp * 5) / 100) + 3276800) / 10;
	var3 = var1 + (var2 >> 1);
	var4 = (var3 / (res_heat_range + 4));
	var5 = (131 * res_heat_val) + 65536;
	res_heat_x100 = (int32_t)(((var4 / var5) - 250) * 34);
	dev->res_heat_0 = (uint8_t)((res_heat_x100 + 50) / 100);
	
	return 0;
}

static const char* HAL_StatusToString(HAL_StatusTypeDef status)
{
    switch (status)
    {
	case HAL_OK:
		return "HAL_OK";
	case HAL_ERROR:
		return "HAL_ERROR";
	case HAL_BUSY:
		return "HAL_BUSY";
	case HAL_TIMEOUT:
		return "HAL_TIMEOUT";
	default:
		return "HAL_UNKNOWN";
    }
}
