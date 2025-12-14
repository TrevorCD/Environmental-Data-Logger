//#include <stdio.h>
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "sdcard.h"

extern SPI_HandleTypeDef hspi;

static Mutex xSPI_Mutex;

#define sdcardSTACK_SIZE configMINIMAL_STACK_SIZE

void vStartSDCardWriteTask( UbaseType_t uxPriority )
{
	xTaskCreate( vSDCardWriteTask, "SDCardWrite", sdcardSTACK_SIZE, NULL,
				 uxPriority, ( TaskHandle_t * ) NULL );
}

static portTASK_FUNCTION( vSDCardWriteTask, pvParameters )
{
    FATFS fs;
    FIL f;
    FRESULT fr;

    if (xSPI_Mutex == NULL) xSPI_Mutex = xSemaphoreCreateMutex();

    // try to init & mount
    xSemaphoreTake(xSPI_Mutex, portMAX_DELAY);
    if (disk_initialize(0) == RES_OK) {
        f_mount(&fs, "", 1);
    }
    xSemaphoreGive(xSPI_Mutex);

    for (;;) {
        // write data
        xSemaphoreTake(xSPI_Mutex, portMAX_DELAY);
        fr = f_open(&f, "log.txt", FA_OPEN_APPEND | FA_WRITE);
        if (fr == FR_OK) {
            UINT bw;
            char msg[] = "hello\n";
            f_write(&f, msg, sizeof(msg)-1, &bw);
            f_close(&f);
        }
        xSemaphoreGive(xSPI_Mutex);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

