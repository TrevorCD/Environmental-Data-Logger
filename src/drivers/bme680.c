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
 * This device driver is for the BME680 Environmental Sensor.
 * Written for FreeRTOS on STM32F4xx
 *
 *----------------------------------------------------------------------------*/



/* Kernel Includes */
#include "FreeRTOS.h"

/* Hardware Includes */
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"

#include <stdio.h> /* ONLY USE printf()! */
#include "bme680.h"

/*----------------------------------------------------------------------------*/

#define TARGET_TEMP    (?)
#define AMB_TEMP       (?)

/* I2C GPIO Pins */
#define BME680_SDA     GPIO_PIN_9
#define BME680_SCL     GPIO_PIN_8

#define SLAVE_ADDR     (0x77U << 1) /* 0x77 << 1 (7 bit addr; HAL expects 8 */

#define READ_TIMEOUT   (100) /* timeout on HAL_I2C_Mem_Read */
#define POLL_DELAY     (10)

/* BME680 Read/Write Registers */
#define CTRL_MEAS      (0x74U) /* osrs_t<7:5> osrs_p<4:2> mode<1:0> */
#define CTRL_HUM       (0x72U) /* spi_3w_int_en<6> osrs_h<2:0> */
#define CTRL_GAS_1     (0x71U) /* run_gas<4> nb_conv<3:0> */
#define GAS_WAIT_0     (0x64U)
#define RES_HEAT_0     (0x5AU)

/* BME680 Read Registers */
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
	if(BME680_Get_Calibration(dev) != 0)
		{
			printf("BME680_Init: Failed to get calibrations parameters!\n");
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

	dev->amb_temp = AMB_TEMP;
	dev->target_temp = TARGET_TEMP;
	
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

	///
	
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
	uint8_t data, old_ctrl_meas;
	HAL_StatusTypeDef status;
	uint8_t eas_status;
	uint16_t gas_r, hum, temp, press;

	// Set MODE<1:0> to 0b01 to trigger a single measurement
	if(BME680_Read(dev, CTRL_MEAS, &old_ctrl_meas) == -1)
		{
			printf("BME680_Poll: Failed to read CTRL_MEAS!\n");
			return -1;
		}
	if(BME680_Transmit(dev, CTRL_MEAS, old_ctrl_meas | MODE_FORCED) != 0)
		{
			printf("BME680_Poll: CTRL_MEAS Transmit FAIL!\n");
			return -1;
		}
	
	/* wait for new data */
	while((eas_status = BME680_Data_Ready(dev)) != 1)
		{
			if(eas_status == -1)
				{
					/* BME680_Data_Ready already prints failure msg */
					return -1;
				}
			HAL_Delay(POLL_DELAY);
		}

	/* move data from BME680 registers to output struct */
	
	/* get bits <9:2> of gas_r from GAS_R_MSB */
	if(BME680_Read(dev, GAS_R_MSB, &data) == -1)
		{
			printf("BME680_Poll: Failed to read GAS_R_MSB!\n");
			return -1;
		}
	gas_r = ((uint16_t)data) << 2;
	/* get bits <1:0> of gas_r from GAS_R_MSB */
	if(BME680_Read(dev, GAS_R_LSB, &data) == -1)
		{
			printf("BME680_Poll: Failed to read GAS_R_LSB!\n");
			return -1;
		}
	gas_r |= (((uint16_t)data) >> 6) & 0x0003;

	/* read MSB then LSB for humidity */
	if(BME680_Read(dev, HUM_MSB, &data) == -1)
		{
			printf("BME680_Poll: Failed to read HUM_MSB!\n");
			return -1;
		}
	hum = ((uint16_t)data) << 8;
	if(BME680_Read(dev, HUM_LSB, &data) == -1)
		{
			printf("BME680_Poll: Failed to read HUM_LSB!\n");
			return -1;
		}
	hum |= (uint16_t)data;
	
	/* read MSB then LSB for temperature */
	if(BME680_Read(dev, TEMP_MSB, &data) == -1)
		{
			printf("BME680_Poll: Failed to read TEMP_MSB!\n");
			return -1;
		}
	temp = ((uint16_t)data) << 8;
	if(BME680_Read(dev, TEMP_LSB, &data) == -1)
		{
			printf("BME680_Poll: Failed to read TEMP_LSB!\n");
			return -1;
		}
	temp |= (uint16_t)data;

	/* read MSB then LSB for pressure */
	if(BME680_Read(dev, PRESS_MSB, &data) == -1)
		{
			printf("BME680_Poll: Failed to read PRESS_MSB!\n");
			return -1;
		}
	press = ((uint16_t)data) << 8;
	if(BME680_Read(dev, PRESS_LSB, &data) == -1)
		{
			printf("BME680_Poll: Failed to read PRESS_LSB!\n");
			return -1;
		}
	press |= (uint16_t)data;
	
	///
	
	dev->output.humidity = hum;
	dev->output.temperature = temp;
	dev->output.pressure = press;
	dev->output.gas_resistance = gas_r;
	
	return 0;
}
	
/*----------------------------------------------------------------------------*/

/* returns -1 on read fail, 1 if ready, 0 if not ready */
static int BME680_Data_Ready(BME680_HandleTypeDef *dev)
{
	uint8_t status;
	if(BME680_Read(dev, EAS_STATUS_0, &status) == -1)
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
	uint8_t par_temp; /* temporary variable for reading 2 byte parameters */

	/* Read par_g1 */
	if(BME680_Read(dev, PAR_G1, &dev->calib.par_g1) != 0)
		{
			return -1;
		}
	/* Read both bytes of par_g2 */
	if(BME680_Read(dev, PAR_G2_MSB, &par_temp) != 0)
		{
			return -1;
		}
	/* Add MSB to MSB of par_g2 var */
	dev->calib.par_g2 = ((uint16_t) par_temp) << 8;
	if(BME680_Read(dev, PAR_G2_LSB, &par_temp) != 0)
		{
			return -1;
		}
	/* Add LSB from temp to LSB of par_g2 */
	dev->calib.par_g2 |= (uint16_t) par_temp;
	/* Read par_g3 */
	if(BME680_Read(dev, PAR_G3, &dev->calib.par_g3) != 0)
		{
			return -1;
		}


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
