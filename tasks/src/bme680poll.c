#include <stdlib.h>
//#include <stdio.h> /* ONLY printf! */

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* BME680 driver include */
#include "bme680.h"

#define bme680STACK_SIZE (configMINIMAL_STACK_SIZE * 2)

static portTASK_FUNCTION_PROTO( vBME680PollTask, pvParameters );

extern BME680_HandleTypeDef hbme;
extern QueueHandle_t queue;

/*-----------------------------------------------------------*/

void vStartBME680PollTask( UBaseType_t uxPriority )
{
	
	xTaskCreate( vBME680PollTask, "BME680Poll", bme680STACK_SIZE, NULL,
				 uxPriority, ( TaskHandle_t * ) NULL );
}

/*-----------------------------------------------------------*/

static portTASK_FUNCTION( vBME680PollTask, pvParameters )
{
	BaseType_t xStatus;
	
	BME680_Init(&hbme);
	
	for( ; ; )
    {
        if(BME680_Poll(&hbme) != 0)
		{
			for(;;) {}
		}
		/* Time stamp data */
		hbme.output.time_stamp = xTaskGetTickCount();
		/* Send data to queue */
		xStatus = xQueueSend(queue, &hbme.output, 0);
            
		if(xStatus != pdPASS)
		{
			/* Queue is full */
			
		}
			
		vTaskDelay(pdMS_TO_TICKS(1000));   // delay 1000 ms
    }
}
