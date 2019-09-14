/*
 * mem_msd_glue.cpp
 *
 *  Created on: 30 џэт. 2016 у.
 *      Author: Kreyl
 */

#include "mem_msd_glue.h"
#include "uart.h"
#include "kl_lib.h"

#if MSD_USE_INNER_FLASH
// Data inside the Flash. Init value is dummy.
// For some reason "aligned" attribute does not work, therefore array used here.
#ifdef STM32L4XX
#define WORD64_CNT     (FLASH_PAGE_SIZE/8)
__attribute__ ((section("MSDStorage")))
volatile const uint64_t IData[WORD64_CNT] = { 0xCA115EA1 };
#else
__attribute__ ((section("MSDStorage")))
volatile const uint32_t IData[(MSD_STORAGE_SZ_BYTES/4)] = { 0xCA115EA1 };
#endif
#endif

uint8_t MSDRead(uint32_t BlockAddress, uint32_t *Ptr, uint32_t BlocksCnt) {
//    Printf("RD %u; %u\r", BlockAddress, BlocksCnt);
#if MSD_USE_SD
    if(disk_read(0, Ptr, BlockAddress, BlocksCnt) == RES_OK) return retvOk;
    else return retvFail;
#elif MSD_USE_EXT_SPI_FLASH
    Mem.Read(BlockAddress * MSD_BLOCK_SZ, Ptr, BlocksCnt * MSD_BLOCK_SZ);
#elif MSD_USE_INNER_FLASH
    uint32_t Addr = (uint32_t)&IData + (BlockAddress * MSD_BLOCK_SZ);
    memcpy(Ptr, (const void*)Addr, BlocksCnt * MSD_BLOCK_SZ);
//    Printf("RD %u %X; %u\r", BlockAddress, Addr, BlocksCnt);
//    Printf("RD %X\r%A\r\r", Addr, (uint8_t*)Addr, (BlocksCnt * MSD_BLOCK_SZ), ' ');
//    chThdSleepMilliseconds(99);
    return retvOk;
#else
    return retvOk;
#endif
}

uint8_t MSDWrite(uint32_t BlockAddress, uint32_t *Ptr, uint32_t BlocksCnt) {
//    Printf("WR %u; %u\r", BlockAddress, BlocksCnt);
#if MSD_USE_SD
    if(disk_write(0, Ptr, BlockAddress, BlocksCnt) == RES_OK) return retvOk;
    else return retvFail;
#elif MSD_USE_EXT_SPI_FLASH
    while(BlocksCnt != 0)
        // Calculate Mem Sector addr
        uint32_t SectorStartAddr = BlockAddress * MEM_SECTOR_SZ;
        // Write renewed sector
        if(Mem.EraseAndWriteSector4k(SectorStartAddr, Ptr) != OK) return FAILURE;
        // Process variables
        BlockAddress += MSD_BLOCK_SZ;
        BlocksCnt--;
    }
#elif MSD_USE_INNER_FLASH
    uint8_t Rslt = retvOk;
    uint32_t Addr = (uint32_t)&IData + (BlockAddress * MSD_BLOCK_SZ);
    uint32_t DWordCnt = (BlocksCnt * MSD_BLOCK_SZ) / sizeof(uint32_t);
#if defined STM32F1XX
//    Printf("WR %u %X; %u; %u\r", BlockAddress, Addr, BlocksCnt, DWordCnt);
//    Printf("WR 0x%X\r%A\r\r", Addr, Ptr, (BlocksCnt * MSD_BLOCK_SZ), ' ');
    // Unlock flash
    chSysLock();
    Flash::LockFlash();
    Flash::UnlockFlash();
    chSysUnlock();
    Flash::ClearPendingFlags();
    // Erase flash
    for(uint32_t i=0; i<BlocksCnt; i++) {
        if(Flash::ErasePage(Addr + (FLASH_PAGE_SIZE * i)) != retvOk) {
            Printf("\rPage %X Erase fail\r", Addr + (FLASH_PAGE_SIZE * i));
            chThdSleepMilliseconds(45);
            Rslt = retvFail;
            goto End;
        }
    }
    // Write flash
    for(uint32_t i=0; i<DWordCnt; i++) {
        if(Flash::ProgramWord(Addr, Ptr[i]) != retvOk) {
            Printf("Write Fail\r");
            chThdSleepMilliseconds(45);
            Rslt = retvFail;
            goto End;
        }
        Addr += sizeof(uint32_t);
    }
    chThdSleepMilliseconds(9);
    Flash::wa

    End:
    chSysLock();
    Flash::LockFlash();
    chSysUnlock();
    return Rslt;
#else
    uint32_t PageNum = (Addr - FLASH_START_ADDR) / FLASH_PAGE_SIZE;
#endif

#else
    return retvOk;
#endif
}
