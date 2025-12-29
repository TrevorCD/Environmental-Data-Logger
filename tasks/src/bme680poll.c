#include <stdlib.h>
//#include <stdio.h> /* ONLY printf! */

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"

/* BME680 driver include */
#include "bme680.h"

#define bme680STACK_SIZE (configMINIMAL_STACK_SIZE * 2)
#define bme680Priority (configMAX_PRIORITIES - 2)

static portTASK_FUNCTION_PROTO( vBME680PollTask, pvParameters );

extern BME680_HandleTypeDef hbme;

/*-----------------------------------------------------------*/

void vStartBME680PollTask( UBaseType_t uxPriority )
{
	
	xTaskCreate( vBME680PollTask, "BME680Poll", bme680STACK_SIZE, NULL,
				 bme680Priority, ( TaskHandle_t * ) NULL );
}

/*-----------------------------------------------------------*/

static portTASK_FUNCTION( vBME680PollTask, pvParameters )
{
	
	BME680_Init(&hbme);
	
	for( ; ; )
    {
        BME680_Poll(&hbme);
		/*
		printf("hum: %d\ntemp: %d\npress: %d\ngas_r: %d\n",
			   hbme.output.humidity, hbme.output.temperature,
			   hbme.output.pressure, hbme.output.gas_resistance);
		*/
		vTaskDelay(pdMS_TO_TICKS(1000));   // delay 1000 ms
    }
}
