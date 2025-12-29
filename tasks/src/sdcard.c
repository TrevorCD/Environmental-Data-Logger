#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
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
/* Needed for BME680_OutputTypeDef */
#include "bme680.h"

#define forever for(;;)

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
extern QueueHandle_t queue;

/* Private Prototypes --------------------------------------------------------*/
static portTASK_FUNCTION_PROTO(vSDCardWriteTask, pvParameters);
static void Error_Handler(void);
static uint8_t i32toa(uint32_t num, uint8_t *buf);
static int lWriteOutput(BME680_OutputTypeDef *data, FIL *fil);

static uint32_t str_len(const char *text)
{
	uint32_t count = 0;
	while(text[count] != 0) count++;
	return count;
}

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
	FSIZE_t fsize;
	char SDPath[4];
	
	SD_SetSPIHandle(&hspi);
	
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
        /* Mount failed - check error code */
        /* FR_DISK_ERR = hardware problem */
        if(fres == FR_DISK_ERR)
		{
			Error_Handler();
		}
		/* FR_NO_FILESYSTEM = needs formatting */
		/* may be exFAT file system */
		else if(fres == FR_NO_FILESYSTEM)
		{
			BYTE work[_MAX_SS];  /* Work area for formatting */
			fres = f_mkfs("", FM_FAT32, 0, work, sizeof(work));    
			if(fres != FR_OK)
			{
				Error_Handler();
			}
		
			/* Try mounting again */
			fres = f_mount(&fs, "", 1);
			if(fres != FR_OK)
			{
				Error_Handler();
			}
		}
		/* other errors */
		else
		{
			Error_Handler();
		}
    }
	/* Successfully mounted! */
        
	/* Create/open a file for writing, with write pointer set to EOF position */
	if(f_open(&fil, "data.csv", FA_OPEN_APPEND | FA_WRITE) != FR_OK)
	{
		/* Not sure what would be wrong */
		Error_Handler();
	}
	
	BME680_OutputTypeDef bme680Data;
    BaseType_t xStatus;
	
    forever
	{
		/* Wait on queue for bme680 output data */
		xStatus = xQueueReceive(queue, &bme680Data, portMAX_DELAY);
        if(xStatus != pdPASS)
        {
			Error_Handler();
		}
		/* Write data to SD Card */
		if(lWriteOutput(&bme680Data, &fil) != 0)
		{
			Error_Handler();
		}
		/// need some way to exit this loop (button?)
    }

	f_close(&fil);
	/* First param NULL unmounts current filesystem */
	fres = f_mount(NULL, "", 1);
	
	vTaskDelete(NULL);
}

static void Error_Handler(void)
{
	forever { }
}

/* BUFFER MUST BE 12 BYTES WIDE! */
static uint8_t i32toa(uint32_t num, uint8_t *buf)
{
	const uint8_t ASCII_OFFSET = 48;
	uint32_t x;
	uint32_t primary_divisor = 10;
	uint32_t secondary_divisor = 1;
	uint32_t len = 0;
	/* convert numbers to string with lsd on lhs of buf */
	/* loop through number, removing decimal digit starting from 1s place */
	for(int i = 11; i > 1; i++)
	{
		len++;
		x = num % primary_divisor; /* remove more significant digits */
		x /= secondary_divisor;    /* decimal shift to one's place */
		buf[i] = ((uint8_t)x) + ASCII_OFFSET;

		if(x / primary_divisor == 0)
		{
			/* No more digits */
			break;
		}
		
		primary_divisor *= 10;
		secondary_divisor *= 10;
	}
	/* reverse buf */
	uint8_t swap;
	for(int i = 0; i < 6; i++)
	{
		swap = buf[i];
		buf[i] = buf[11-i];
		buf[11-i] = swap;
	}
	
	return len;
}

/* Writes "hum, temp, press, gas_r\n" to SD Card in CSV format */
static int lWriteOutput(BME680_OutputTypeDef *data, FIL *fil)
{
	UINT bytesWritten;
	uint8_t buf[12] = {0}; /* MAX_INT32 is 10 characters long */
	uint8_t len;
	FRESULT fres;
	
	len = i32toa(data->humidity, buf);
	buf[len] = ',';
	fres = f_write(fil, buf, len+1, &bytesWritten);
	if(fres != FR_OK)
	{
		return -1;
	}
	len = i32toa(data->temperature, buf);
	buf[len] = ',';
	fres = f_write(fil, buf, len+1, &bytesWritten);
	if(fres != FR_OK)
	{
		return -1;
	}
	len = i32toa(data->pressure, buf);
	buf[len] = ',';
	fres = f_write(fil, buf, len+1, &bytesWritten);
	if(fres != FR_OK)
	{
		return -1;
	}
	len = i32toa(data->gas_resistance, buf);
	buf[len] = '\n';
	fres = f_write(fil, buf, len+1, &bytesWritten);
	if(fres != FR_OK)
	{
		return -1;
	}
	
	fres = f_sync(fil);
	if(fres != FR_OK)
	{
		return -1;
	}

	return 0;
}
