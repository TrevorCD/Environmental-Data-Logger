#include <stdlib.h>
#include <stdio.h> /* ONLY printf! */

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"

/* BME680 driver include */
#include "bme680.h"

#define bme680STACK_SIZE configMINIMAL_STACK_SIZE

static portTASK_FUNCTION_PROTO( vBME680PollTask, pvParameters );

/*-----------------------------------------------------------*/

void vStartBME680PollTask( UBaseType_t uxPriority )
{
	xTaskCreate( vBME680PollTask, "BME680Poll", bme680STACK_SIZE, NULL,
				 uxPriority, ( TaskHandle_t * ) NULL );
}

/*-----------------------------------------------------------*/

static portTASK_FUNCTION( vBME680PollTask, pvParameters )
{
	//( void ) pvParameters;

	static BME680_HandleTypeDef hbme = {0};
	static I2C_HandleTypeDef hi2c = {0};
	
	/* Configure HAL I2C Handle */
	hbme.hi2c = &hi2c;
	hbme.hi2c->Instance = I2C1;
	hbme.hi2c->Init.ClockSpeed = 100000;
	hbme.hi2c->Init.DutyCycle = I2C_DUTYCYCLE_2;
	hbme.hi2c->Init.OwnAddress1 = 0;
	hbme.hi2c->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hbme.hi2c->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hbme.hi2c->Init.OwnAddress2 = 0;
	hbme.hi2c->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hbme.hi2c->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	
	if(HAL_I2C_Init(hbme.hi2c) != HAL_OK)
	{
		printf("HAL I2C INIT FAIL!\n");
		for( ; ; )
		{
		}
	}
	
	if(BME680_Init(&hbme) != 0)
	{
		/* BME680_Init prints it's own error messages */
		for( ; ; )
		{
		}
	}
	
	for( ; ; )
    {
        BME680_Poll(&hbme);
		printf("hum: %d\ntemp: %d\npress: %d\ngas_r: %d\n",
			   hbme.output.humidity, hbme.output.temperature,
			   hbme.output.pressure, hbme.output.gas_resistance);
		vTaskDelay(pdMS_TO_TICKS(1000));   // delay 1000 ms
    }
}
