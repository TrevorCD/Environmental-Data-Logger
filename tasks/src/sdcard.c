#include <stdio.h>
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "sdcard.h"

extern SPI_HandleTypeDef hspi;

void vStartSDCardWriteTask( UbaseType_t uxPriority )
{
	xTaskCreate( vSDCardWriteTask, "SDCardWrite", sdcardSTACK_SIZE, NULL,
				 uxPriority, ( TaskHandle_t * ) NULL );
}

static portTASK_FUNCTION( vSDCardWriteTask, pvParameters )
{
	
}
