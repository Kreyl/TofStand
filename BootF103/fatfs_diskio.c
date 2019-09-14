/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2007        */
/*-----------------------------------------------------------------------*/
/* This is a stub disk I/O module that acts as front end of the existing */
/* disk I/O modules and attach it to FatFs module with common interface. */
/*-----------------------------------------------------------------------*/

#include "ch.h"
#include "hal.h"
#include "ffconf.h"
#include "diskio.h"
#include "string.h"

// KL
#undef HAL_USE_RTC

#define BLOCK_SZ    _MAX_SS
#define DISK_SZ     (128UL * 1024UL)

extern void PrintfC(const char *format, ...);

/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */

DSTATUS disk_initialize (
    BYTE drv                /* Physical drive nmuber (0..) */
)
{
    return 0;
}

/*-----------------------------------------------------------------------*/
/* Return Disk Status                                                    */

DSTATUS disk_status (
    BYTE drv        /* Physical drive nmuber (0..) */
)
{
    return 0;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
DRESULT disk_read (
    BYTE drv,        /* Physical drive nmuber (0..) */
    BYTE *buff,        /* Data buffer to store read data */
    DWORD sector,    /* Sector address (LBA) */
    BYTE count        /* Number of sectors to read (1..255) */
)
{
    uint32_t Addr = 0x08020000 + (sector * BLOCK_SZ);
    memcpy(buff, (const void*)Addr, count * BLOCK_SZ);
    return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */

#if _READONLY == 0
DRESULT disk_write (
    BYTE drv,            /* Physical drive nmuber (0..) */
    const BYTE *buff,    /* Data to be written */
    DWORD sector,        /* Sector address (LBA) */
    BYTE count            /* Number of sectors to write (1..255) */
)
{
//    PrintfC("\r__DiskW");
//    if (blkGetDriverState(&SDCD1) != BLK_READY) return RES_NOTRDY;
//    if (SDWrite(sector, buff, count)) return RES_ERROR;
    return RES_OK;
}
#endif /* _READONLY */

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */

DRESULT disk_ioctl (
    BYTE drv,        /* Physical drive nmuber (0..) */
    BYTE ctrl,        /* Control code */
    void *buff        /* Buffer to send/receive control data */
)
{
    switch (ctrl) {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_COUNT:
        *((DWORD *)buff) = DISK_SZ / BLOCK_SZ;
        return RES_OK;
    case GET_SECTOR_SIZE:
        *((WORD *)buff) = BLOCK_SZ;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *((DWORD *)buff) = 256; /* 512b blocks in one erase block */
        return RES_OK;
    default:
        return RES_PARERR;
    }
  return RES_PARERR;
}

DWORD get_fattime(void) {
    return ((uint32_t)0 | (1 << 16)) | (1 << 21); /* wrong but valid time */
}
