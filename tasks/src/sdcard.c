#include "FreeRTOS.h"
#include "task.h"
#include "sdcard.h"

/* Hardware Includes */
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_spi.h"
#include "stm32f4xx_hal_gpio.h"

/* FatFs Includes */
#include "ff_gen_drv.h"
#include "ff.h"
/* Custom SPI driver under FatFs layer */
#include "sd_spi.h"

#define forever for(; ;)

/* Pins */
#define SCK  GPIOA, GPIO_PIN_5
#define MISO GPIOA, GPIO_PIN_6
#define MOSI GPIOA, GPIO_PIN_7
#define CS   GPIOB, GPIO_PIN_6
#define DET  GPIOC, GPIO_PIN_7

#define sdcardSTACK_SIZE ((unsigned short) 1024)

/* Globals -------------------------------------------------------------------*/
extern SPI_HandleTypeDef hspi; /* from main.c */
extern Diskio_drvTypeDef SD_SPI_Driver;

/* Private Prototypes --------------------------------------------------------*/
static portTASK_FUNCTION_PROTO(vSDCardWriteTask, pvParameters);
static void Error_Handler(void);

void vStartSDCardWriteTask(UBaseType_t uxPriority)
{
	xTaskCreate(vSDCardWriteTask, "SDWrite", sdcardSTACK_SIZE, NULL,
				 uxPriority, (TaskHandle_t *)NULL);
}

static portTASK_FUNCTION(vSDCardWriteTask, pvParameters)
{	
	FATFS fs;
	FIL fil;
	FRESULT fres;
	UINT bytesWritten;
	char SDPath[4];
	
	if(HAL_GPIO_ReadPin(DET) != GPIO_PIN_SET)
	{
		/* SD CARD NOT INSERTED! */
		Error_Handler();
	}

	if(FATFS_LinkDriver(&SD_SPI_Driver, SDPath) != 0)
	{
		Error_Handler();
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
    
    forever
	{
		/* Wait on binary sem for enviro data */
		
		/* Write data to SD Card */
		const char *text = "Hello from STM32 + FreeRTOS!\n";
		fres = f_write(&fil, text, sizeof(text), &bytesWritten);
        
    }

	f_close(&fil);
	vTaskDelete(NULL);
}

static void Error_Handler(void)
{
	forever { }
}

