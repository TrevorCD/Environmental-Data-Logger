#include <stdio.h>
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "sdcard.h"

void vStartSDCardWriteTask( UbaseType_t uxPriority )
{
	xTaskCreate( vSDCardWriteTask, "SDCardWrite", sdcardSTACK_SIZE, NULL,
				 uxPriority, ( TaskHandle_t * ) NULL );
}

static portTASK_FUNCTION( vSDCardWriteTask, pvParameters )
{

	static SPI_HandleTypeDef hspi = {0};

	hspi1.Instance               = SPI1;
	hspi1.Init.Mode              = SPI_MODE_MASTER;
	hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW; // CPOL = 0
	hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE; // CPHA = 0 → MODE0
	hspi1.Init.NSS               = SPI_NSS_SOFT; // Use software CS (GPIO)
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256; // ~300 kHz
	hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial     = 7;

	if(HAL_SPI_Init(&hspi) != HAL_OK)
	{
		printf("SDCardWrite Task: FAILED TO INIT SPI HANDLE\n");
		return -1;
	}
	
}
