#include "sd_spi.h"
#include "stm32f4xx_hal.h"

/* SD Commands */
#define CMD0    0   /* GO_IDLE_STATE */
#define CMD8    8   /* SEND_IF_COND */
#define CMD17   17  /* READ_SINGLE_BLOCK */
#define CMD24   24  /* WRITE_BLOCK */
#define CMD55   55  /* APP_CMD */
#define CMD58   58  /* READ_OCR */
#define ACMD41  41  /* SD_SEND_OP_COND */

static SPI_HandleTypeDef *g_hspi = NULL;

static void SD_CS_Low(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
}

static void SD_CS_High(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
}

static uint8_t SD_SendByte(uint8_t byte)
{
    uint8_t rx;
    HAL_SPI_TransmitReceive(g_hspi, &byte, &rx, 1, 100);
    return rx;
}

static uint8_t SD_SendCommand(uint8_t cmd, uint32_t arg)
{
    uint8_t response;
    uint8_t crc = 0x01;
    
    if(cmd == CMD0) crc = 0x95;
    if(cmd == CMD8) crc = 0x87;
    
    SD_CS_Low();
    
    SD_SendByte(0x40 | cmd);
    SD_SendByte((arg >> 24) & 0xFF);
    SD_SendByte((arg >> 16) & 0xFF);
    SD_SendByte((arg >> 8) & 0xFF);
    SD_SendByte(arg & 0xFF);
    SD_SendByte(crc);
    
    /* Wait for response */
    for(int i = 0; i < 10; i++)
	{
        response = SD_SendByte(0xFF);
        if((response & 0x80) == 0) break;
    }
    
    return response;
}

uint8_t SD_Init(void)
{
    /* Power-up sequence */
    SD_CS_High();
    for(int i = 0; i < 10; i++)
	{
        SD_SendByte(0xFF);
    }
    
    /* CMD0: GO_IDLE_STATE */
    if(SD_SendCommand(CMD0, 0) != 0x01)
	{
        SD_CS_High();
        return 1;
    }
    SD_CS_High();
    SD_SendByte(0xFF);
	
    // CMD8: Check voltage
    SD_SendCommand(CMD8, 0x1AA);
    SD_SendByte(0xFF);
    SD_SendByte(0xFF);
    SD_SendByte(0xFF);
    SD_SendByte(0xFF);
    SD_CS_High();
    SD_SendByte(0xFF);
    
    // ACMD41: Initialize
    uint16_t timeout = 1000;
    while(timeout--)
	{
        SD_SendCommand(CMD55, 0);
        SD_CS_High();
        SD_SendByte(0xFF);
        
        if(SD_SendCommand(ACMD41, 0x40000000) == 0x00)
		{
            SD_CS_High();
            SD_SendByte(0xFF);
            return 0;  // Success
        }
        SD_CS_High();
        SD_SendByte(0xFF);
        HAL_Delay(1);
    }

	return 1; /* Timeout */
}

uint8_t SD_ReadSingleBlock(uint8_t *buff, uint32_t sector)
{
    if(SD_SendCommand(CMD17, sector) != 0x00)
	{
        SD_CS_High();
        return 1;
    }
    
    // Wait for data token (0xFE)
    uint16_t timeout = 1000;
    while(SD_SendByte(0xFF) != 0xFE)
	{
        if(--timeout == 0)
		{
            SD_CS_High();
            return 1;
        }
    }
    
    // Read 512 bytes
    for(int i = 0; i < 512; i++)
	{
        buff[i] = SD_SendByte(0xFF);
    }
    
    // Read CRC (ignore)
    SD_SendByte(0xFF);
    SD_SendByte(0xFF);
    
    SD_CS_High();
    SD_SendByte(0xFF);
    
    return 0;
}

uint8_t SD_WriteSingleBlock(const uint8_t *buff, uint32_t sector)
{
    if(SD_SendCommand(CMD24, sector) != 0x00)
	{
        SD_CS_High();
        return 1;
    }
    
    // Send data token
    SD_SendByte(0xFE);
    
    // Write 512 bytes
    for(int i = 0; i < 512; i++)
	{
        SD_SendByte(buff[i]);
    }
    
    // Send dummy CRC
    SD_SendByte(0xFF);
    SD_SendByte(0xFF);
    
    // Check response
    uint8_t response = SD_SendByte(0xFF);
    if((response & 0x1F) != 0x05)
	{
        SD_CS_High();
        return 1;
    }
    
    // Wait for write to complete
    uint16_t timeout = 1000;
    while(SD_SendByte(0xFF) == 0x00)
	{
        if(--timeout == 0)
		{
            SD_CS_High();
            return 1;
        }
    }
    
    SD_CS_High();
    SD_SendByte(0xFF);
    
    return 0;
}


void SD_SetSPIHandle(SPI_HandleTypeDef *hspi)
{
    g_hspi = hspi;
}
