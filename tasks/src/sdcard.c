//#include <stdio.h>
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "sdcard.h"
#include "stm32f4xx_hal_spi.h"
#include "stm32f4xx_hal_gpio.h"
#include "ff.h"

#define FOREVER for( ; ; )

/* Pins */
#define SCK  GPIOA, GPIO_PIN_5
#define MISO GPIOA, GPIO_PIN_6
#define MOSI GPIOA, GPIO_PIN_7
#define CS   GPIOB, GPIO_PIN_6
#define DET  GPIOC, GPIO_PIN_7

#define sdcardSTACK_SIZE ((unsigned short) 512)

/* Globals -------------------------------------------------------------------*/
extern SPI_HandleTypeDef hspi;

/* Private Prototypes ------------------------------------------------------- */
static portTASK_FUNCTION_PROTO( vSDCardWriteTask, pvParameters );
static void Error_Handler( void );

void vStartSDCardWriteTask( UBaseType_t uxPriority )
{
	xTaskCreate( vSDCardWriteTask, "SDWrite", sdcardSTACK_SIZE, NULL,
				 uxPriority, ( TaskHandle_t * ) NULL );
}

static portTASK_FUNCTION( vSDCardWriteTask, pvParameters )
{	
	FATFS fs;
	FIL fil;
	FRESULT fres;
	UINT bytesWritten;
	
	if( HAL_GPIO_ReadPin( DET ) != GPIO_PIN_SET )
	{
		/* SD CARD NOT INSERTED! */
		vTaskDelete( NULL );
	}
	
    /* Mount the file system */
    fres = f_mount(&fs, "", 1);  // "" = logical drive 0, 1 = mount now
    
    if(fres != FR_OK)
	{
        // Mount failed - check error code
        // FR_DISK_ERR = hardware problem
        if(fres == FR_DISK_ERR)
		{
			Error_Handler();
		}
		// FR_NO_FILESYSTEM = needs formatting
		else if(fres == FR_NO_FILESYSTEM)
		{
			BYTE work[_MAX_SS];  // Work area for formatting
			fres = f_mkfs("", FM_FAT32, 0, work, sizeof(work));    
			if(fres != FR_OK)
			{
				Error_Handler();
			}
		
			// Try mounting again
			fres = f_mount(&fs, "", 1);
			if(fres != FR_OK)
			{
				Error_Handler();
			}
		}
		// other errors
		else
		{
			Error_Handler();
		}
    }
	/* Successfully mounted! */
        
	/* Create/open a file for writing */
	fres = f_open(&fil, "data.txt",
				  FA_CREATE_ALWAYS | FA_OPEN_APPEND | FA_WRITE);
	if(fres != FR_OK)
	{
		/* Not sure what would be wrong */
		Error_Handler();
	}
    
    FOREVER
	{
		/* Wait on binary sem for enviro data */
		
		/* Write data to SD Card */
		const char *text = "Hello from STM32 + FreeRTOS!\n";
		fres = f_write(&fil, text, sizeof(text), &bytesWritten);
        
    }

	f_close(&fil);
	vTaskDelete( NULL );
}

static void Error_Handler( void )
{
	FOREVER { }
}

