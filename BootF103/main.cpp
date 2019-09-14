#include "hal.h"
#include "MsgQ.h"
#include "kl_lib.h"
#include "Sequences.h"
#include "uart.h"
#include "shell.h"
#include "led.h"
#include "ff.h"
#include "kl_fs_utils.h"

#if 1 // =============== Low level ================
// Forever
static const UartParams_t CmdUartParams(256000, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};

LedBlinker_t Led(GPIOB, 2, omPushPull);

// Setup this
#define FILENAME_PATTERN        "Fw*.bin"
#define BOOTLOADER_RSRVD_SPACE  0x5000UL    // 20480 bytes for bootloader
#define TOTAL_FLASH_SZ          256000UL
#define PAGE_SZ                 2048L      // bytes in page, see datasheet
// Do not touch
#define APP_START_ADDR          (FLASH_START_ADDR + BOOTLOADER_RSRVD_SPACE)

void JumpToApp();
void ErasePage(uint32_t PageAddr);
void WriteToMemory(uint32_t *PAddr, uint32_t *PBuf, uint32_t ALen);
static inline bool AppIsEmpty() {
    uint32_t FirstWord = *(uint32_t*)APP_START_ADDR;
    return (FirstWord == 0xFFFFFFFF);
}

void OnError() {
    if(AppIsEmpty()) {
        Led.StartOrRestart(lsqError); // Display error
        while(true);
    }
    else JumpToApp();
}

FATFS FlashFS;
uint32_t Buf[(PAGE_SZ / sizeof(uint32_t))];
#endif

int main() {
#if 1 // ==== Init ====
    // ===== Setup clock =====
    Clk.EnablePrefetch();
    Clk.UpdateFreqValues();

    // ==== Init OS ====
    halInit();
    chSysInit();

    Led.Init();
    Led.On();

    // ==== Init Hard & Soft ====
//    JtagDisable();
    Uart.Init();
    AFIO->MAPR |= AFIO_MAPR_USART1_REMAP; // Remap UART to PB6/PB7 pins
    Printf("\r%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
    Clk.PrintFreqs();

    FRESULT err;
    err = f_mount(&FlashFS, "", 0);
    if(err != FR_OK) {
        Printf("FS error\r");
        chThdSleepMilliseconds(99);
        JumpToApp();
    }
#endif

#if 1 // =========================== Preparations ==============================
    // Try open file, jump to main app if not found
    if(f_findfirst(&Dir, &FileInfo, "", FILENAME_PATTERN) != FR_OK) {
        Printf("File search fail\r");
        chThdSleepMilliseconds(99);
        OnError();
    }
    if(FileInfo.fname[0] == 0) {
        Printf("%S not found\r", FILENAME_PATTERN);
        chThdSleepMilliseconds(99);
        OnError();
    }
    Printf("Found: %S\r", FileInfo.fname);
    if(TryOpenFileRead(FileInfo.fname, &CommonFile) != retvOk) OnError();
    int32_t TotalLen = f_size(&CommonFile);
#endif
#if 1 // ======= Reading and flashing =======
    Led.StartOrRestart(lsqWriting);
    // ==== Read file block by block, do not write first page ====
    Flash::UnlockFlash();
    uint32_t BytesCnt, CurrentAddr = APP_START_ADDR;
    while(TotalLen != 0) {
        // Check flash address
        if(CurrentAddr >= (FLASH_START_ADDR + TOTAL_FLASH_SZ)) {
            Printf("Error: too large file\r");
            break;
        }

        // How many bytes to read?
        BytesCnt = MIN_(TotalLen, PAGE_SZ);
        TotalLen -= BytesCnt;
        // Read block
        if(f_read(&CommonFile, Buf, BytesCnt, &BytesCnt) != FR_OK) {
            Printf("Read error\r");
            OnError();
        }

        ErasePage(CurrentAddr);
        WriteToMemory(&CurrentAddr, Buf, BytesCnt);
    } // while
    Flash::LockFlash();
#endif
    Printf("\rWriting done\r");
    f_close(&CommonFile);
    // Remove firmware file
//    f_unlink(FileInfo.fname);
    JumpToApp();

    // Forever
    while(true);
}

void JumpToApp() {
    // Check if writing successfull
    if(AppIsEmpty()) {  // Unsuccessful, loop forever
        Led.StartOrRestart(lsqError);
        while(true);
    }
    Led.Stop();
    chSysLock();
    __disable_irq();
    // Start app
    void (*app)(void);
    uint32_t *p = (uint32_t*)APP_START_ADDR; // get a pointer to app
    SCB->VTOR = APP_START_ADDR; // offset the vector table
    __set_MSP(*p++);            // set main stack pointer to app Reset_Handler
    app = (void (*)(void))(*p);
    app();
    chSysUnlock();
}

void ErasePage(uint32_t PageAddr) {
    if(Flash::ErasePage(PageAddr) != retvOk) {
        Printf("\rErase fail\r");
        chThdSleepMilliseconds(450);
        REBOOT();
    }
}

void WriteToMemory(uint32_t *PAddr, uint32_t *PBuf, uint32_t ALen) {
    uint32_t Word32Cnt = (ALen + 3) / 4;
    for(uint32_t i=0; i<Word32Cnt; i++) {
        if(Flash::ProgramWord(*PAddr, Buf[i]) != retvOk) {
            Printf("Write Fail\r");
            chThdSleepMilliseconds(450);
            REBOOT();
        }
        *PAddr = *PAddr + 4;
    }
    Printf(".");
}
