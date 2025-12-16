//#include <stdio.h>
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "sdcard.h"
#include "stm32f4xx_hal_spi.h"
#include "stm32f4xx_hal_gpio.h"

static portTASK_FUNCTION_PROTO( vSDCardWriteTask, pvParameters );

extern SPI_HandleTypeDef hspi;

#define sdcardSTACK_SIZE ((unsigned short) 512)

/* Pins used:
 * SCK:  PA_5 (D13) 
 * MISO: PA_6 (D12) 
 * MOSI: PA_7 (D11) 
 * CS:   PB_6 (D10) 
 * DET:  PC_7 (D9)  Connected to GND. Pulled up to 3.3V on chip insert
 */

void vStartSDCardWriteTask( UBaseType_t uxPriority )
{
	xTaskCreate( vSDCardWriteTask, "SDWrite", sdcardSTACK_SIZE, NULL,
				 uxPriority, ( TaskHandle_t * ) NULL );
}

static portTASK_FUNCTION( vSDCardWriteTask, pvParameters )
{
	uint8_t test_tx = 0xAA;
	uint8_t test_rx = 0x00;
	
	uint8_t status;
	
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // CS low
	HAL_SPI_TransmitReceive(&hspi, &test_tx, &test_rx, 1, 100);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // CS high

	if(test_rx == 0xAA) {
		status = 0;
	} else {
		status = 1;
	}

	vTaskDelete(NULL);
}

