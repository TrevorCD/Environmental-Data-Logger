#include "ff_gen_drv.h"
#include "diskio.h"

#define DEV_SD  0
static volatile DSTATUS Stat = STA_NOINIT;

extern uint8_t SD_Init(void);
extern uint8_t SD_ReadSingleBlock(uint8_t *buff, uint32_t sector);
extern uint8_t SD_WriteSingleBlock(const uint8_t *buff, uint32_t sector);

DSTATUS SD_SPI_initialize(BYTE pdrv) {
    if(pdrv != DEV_SD) return STA_NOINIT;
    
    if(SD_Init() == 0) {
        Stat &= ~STA_NOINIT;
        return 0;
    }
    return STA_NOINIT;
}

DSTATUS SD_SPI_status(BYTE pdrv) {
    if(pdrv != DEV_SD) return STA_NOINIT;
    return Stat;
}

DRESULT SD_SPI_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count) {
    if(pdrv != DEV_SD) return RES_PARERR;
    if(Stat & STA_NOINIT) return RES_NOTRDY;
    
    for(UINT i = 0; i < count; i++) {
        if(SD_ReadSingleBlock(buff + (i * 512), sector + i) != 0) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT SD_SPI_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count) {
    if(pdrv != DEV_SD) return RES_PARERR;
    if(Stat & STA_NOINIT) return RES_NOTRDY;
    
    for(UINT i = 0; i < count; i++) {
        if(SD_WriteSingleBlock(buff + (i * 512), sector + i) != 0) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT SD_SPI_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    if(pdrv != DEV_SD) return RES_PARERR;
    if(Stat & STA_NOINIT) return RES_NOTRDY;
    
    switch(cmd) {
	case CTRL_SYNC:
		return RES_OK;
            
	case GET_SECTOR_SIZE:
		*(WORD*)buff = 512;
		return RES_OK;
            
	case GET_BLOCK_SIZE:
		*(DWORD*)buff = 1;
		return RES_OK;
            
	default:
		return RES_PARERR;
    }
}

Diskio_drvTypeDef SD_SPI_Driver = {
    SD_SPI_initialize,
    SD_SPI_status,
    SD_SPI_read,
    SD_SPI_write,
    SD_SPI_ioctl,
};
