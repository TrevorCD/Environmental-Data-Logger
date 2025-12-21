#include "stm32f4xx_hal.h"

#ifndef SD_SPI_H
#define SD_SPI_H

uint8_t SD_Init(void);
uint8_t SD_ReadSingleBlock(uint8_t *buff, uint32_t sector);
uint8_t SD_WriteSingleBlock(const uint8_t *buff, uint32_t sector);
void    SD_SetSPIHandle(SPI_HandleTypeDef *hspi);

#endif
