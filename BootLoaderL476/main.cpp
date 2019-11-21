#include "hal.h"
#include "MsgQ.h"
#include "Sequences.h"
#include "shell.h"
#include "led.h"
#include "kl_fs_utils.h"
#include "Keys.h"

// Setup this
#define FILENAME_PATTERN        "Firmware*.bin"
#define BOOTLOADER_RSRVD_SPACE  0x8000UL    // 32768 bytes for bootloader
#define TOTAL_FLASH_SZ          256000UL
#define PAGE_SZ                 2048UL      // bytes in page, see datasheet
// Encrypted file related
#define TAG_SZ                  16L         // bytes, hardcoded in utility
#define HEADER_SZ               32L         // bytes, choosen once
// Do not touch
#define AES_BLCK_SZ             16UL        // bytes, constant by standard
#define FLASH_START_ADDR        0x08000000UL
#define APP_START_ADDR          (FLASH_START_ADDR + BOOTLOADER_RSRVD_SPACE)
#define APP_START_PAGE          (BOOTLOADER_RSRVD_SPACE / PAGE_SZ)
#define TOTAL_PAGE_CNT          (TOTAL_FLASH_SZ / PAGE_SZ)
#define APP_PAGE_CNT            ((TOTAL_FLASH_SZ - BOOTLOADER_RSRVD_SPACE) / PAGE_SZ)

static const UartParams_t CmdUartParams(256000, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
void OnCmd(Shell_t *PShell);

LedBlinker_t LedBtn{LED_BTN};
LedBlinker_t LedBtn2{LED_BTN2};

void JumpToApp();
void ErasePage(uint32_t PageAddr);
void WriteToMemory(uint32_t *PAddr, uint64_t *Buf, uint32_t Len);
static inline bool AppIsEmpty() {
    uint32_t FirstWord = *(uint32_t*)APP_START_ADDR;
    return (FirstWord == 0xFFFFFFFF);
}

void OnError() {
    Iwdg::Reload();
    if(AppIsEmpty()) {
        LedBtn.StartOrRestart(lsqError); // Display error
        LedBtn2.StartOrRestart(lsqError); // Display error
        while(true);
    }
    else JumpToApp();
}

// ==== AES related ====
AESCCMctx_stt AESctx;
const char Key[] = CRYPTO_KEY;
const char IV[] = CRYPTO_IV;  // Initialization Vector
uint8_t Header[HEADER_SZ];   // Header message, authenticated but not encrypted
uint8_t Input[PAGE_SZ];
// Buffers for decrypted firmware
uint64_t Page1[(PAGE_SZ / 8)];
uint64_t Pagex[(PAGE_SZ / 8)];

int main(void) {
#if 1 // ==== Init ====
    Iwdg::InitAndStart(4500);
    // Setup clock frequency
    Clk.SetCoreClk(cclk48MHz);
    Clk.Select48MhzSrc(src48PllQ);
    Clk.UpdateFreqValues();

    // Init OS
    halInit();
    chSysInit();
    // ==== Init hardware ====
    Uart.Init();
    Printf("\r%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
    Clk.PrintFreqs();

    // Disable dualbank if enabled
    if(Flash::DualbankIsEnabled()) {
        Printf("Dualbank enabled, disabling\r");
        chThdSleepMilliseconds(45);
        Flash::DisableDualbank();   // Will reset inside
    }

    // Disable Reset-on-entering-sleep-modes
    if(Flash::SleepInResetIsEnabled()) {
        Printf("Enabling sleep-out reset\r");
        chThdSleepMilliseconds(45);
        Flash::DisableSleepInReset();
    }

    // Lock flash if not locked
    if(!Flash::FirmwareIsLocked()) {
        Printf("Flash not locked, locking\r");
        chThdSleepMilliseconds(45);
        Flash::LockFirmware();
        // Will hang here, reboot cycling power
    }

    // Enter app if was in standby and app area is not empty
    //if(Sleep::WasInStandby() and !AppIsEmpty()) JumpToApp();

    // Init button LED
    LedBtn.Init();
    LedBtn2.Init();

    SD.Init();
    if(!SD.IsReady) OnError();

    // Enable CRC to make library work correctly
    RCC->AHB1ENR |= (RCC_AHB1ENR_CRCEN);
#endif
#if 1 // =========================== Preparations ==============================
    // Try open file, jump to main app if not found
    if(f_findfirst(&Dir, &FileInfo, "", FILENAME_PATTERN) != FR_OK) {
        Printf("File search fail\r");
        OnError();
    }
    if(FileInfo.fname[0] == 0) {
        Printf("%S not found\r", FILENAME_PATTERN);
        OnError();
    }
    Printf("Found: %S\r", FileInfo.fname);
    if(TryOpenFileRead(FileInfo.fname, &CommonFile) != retvOk) OnError();

    // Read header
    if(TryRead(&CommonFile, Header, HEADER_SZ) != retvOk) {
        Printf("Error reading header\r");
        OnError();
    }

    // Calculate length of encrypted part
    int32_t TotalEncLen = f_size(&CommonFile) - HEADER_SZ - TAG_SZ;
    if(TotalEncLen < 1) {
        Printf("Empty data section\r");
        OnError();
    }

    // Init context
    AESctx.mFlags = E_SK_DEFAULT;       // Set flag field to default value
    AESctx.mKeySize = CRL_AES128_KEY;   // Set key size to 16 (corresponding to AES-128)
    AESctx.mNonceSize = sizeof(IV) - 1; // Set nonce size field to IvLength, note that valid values are 7,8,9,10,11,12,13. -1 to remove EOL symbol \0
    AESctx.mTagSize = TAG_SZ;           // Size of returned authentication TAG
    AESctx.mAssDataSize = HEADER_SZ;    // Set the size of the header
    AESctx.mPayloadSize = TotalEncLen;  // Set the size of thepayload

    // Prepare to decrypt
    uint32_t error_status = AES_CCM_Decrypt_Init(&AESctx, (const uint8_t*)Key, (const uint8_t*)IV);
    if (error_status != AES_SUCCESS) {
        Printf("Decrypt_Init failed: %u\r", error_status);
        OnError();
    }

    // Append header
    error_status = AES_CCM_Header_Append(&AESctx, Header, HEADER_SZ);
    if (error_status != AES_SUCCESS) {
        Printf("HeaderAppend failed: %u\r", error_status);
        OnError();
    }
#endif
#if 1 // ======= Reading and flashing =======
    LedBtn.StartOrRestart(lsqWriting);
    LedBtn2.StartOrRestart(lsqWriting);
    // ==== Read file block by block, do not write first page ====
    Flash::UnlockFlash();
    bool FirstPage = true;
    uint32_t BytesCnt, CurrentAddr = APP_START_ADDR;
    int32_t OutputSz;
    while(TotalEncLen != 0) {
        Iwdg::Reload();
        // Check flash address
        if(CurrentAddr >= (FLASH_START_ADDR + TOTAL_FLASH_SZ)) {
            Printf("Error: too large file\r");
            break;
        }

        // How many bytes to read?
        BytesCnt = MIN(TotalEncLen, PAGE_SZ);
        TotalEncLen -= BytesCnt;
        // Read block
        if(f_read(&CommonFile, Input, BytesCnt, &BytesCnt) != FR_OK) {
            Printf("Read error\r");
            OnError();
        }

        // Decrypt block
        uint8_t *POutput = FirstPage? (uint8_t*)Page1 : (uint8_t*)Pagex;
        error_status = AES_CCM_Decrypt_Append(&AESctx, Input, BytesCnt, POutput, &OutputSz);
        if(error_status != AES_SUCCESS) {
            Printf("Decrypt failed: %u\r", error_status);
            OnError();
        }

        // Erase page
        ErasePage(CurrentAddr / PAGE_SZ);
        // Write page if not first
        if(FirstPage) {
            FirstPage = false;
            CurrentAddr += PAGE_SZ;
        }
        else WriteToMemory(&CurrentAddr, Pagex, OutputSz);
    } // while

    // ==== Check TAG ====
    // Read Tag
    if(f_read(&CommonFile, Input, TAG_SZ, &BytesCnt) != FR_OK) {
        Printf("Read TAG error\r");
        OnError();
    }
    AESctx.pmTag = Input; // Set the pointer to the TAG to be checked
    error_status = AES_CCM_Decrypt_Finish(&AESctx, NULL, &OutputSz);
    if (error_status != AUTHENTICATION_SUCCESSFUL) {
        Printf("TAG check failed: %u\r", error_status);
        Flash::LockFlash();
        // Remove firmware file
        f_unlink(FileInfo.fname);
        OnError();
    }

    // ==== Check ok, write first page ====
    Printf("\rTAG check ok\r");
    CurrentAddr = APP_START_ADDR;
    WriteToMemory(&CurrentAddr, Page1, PAGE_SZ);
    Flash::LockFlash();
#endif
    Printf("\rWriting done\r");
    f_close(&CommonFile);
    // Remove firmware file
    f_unlink(FileInfo.fname);
    Iwdg::Reload();
    JumpToApp();

    // Forever
    while(true);
}

void JumpToApp() {
    // Check if writing successfull
    if(AppIsEmpty()) {  // Unsuccessful, loop forever
        LedBtn.StartOrRestart(lsqError);
        LedBtn2.StartOrRestart(lsqError);
        while(true);
    }
    else {
        LedBtn.Stop();
        LedBtn2.Stop();
    }
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

void WriteToMemory(uint32_t *PAddr, uint64_t *Buf, uint32_t Len) {
    uint32_t DWordCnt = (Len + 7) / 8;
    for(uint32_t i=0; i<DWordCnt; i++) {
        if(Flash::ProgramDWord(*PAddr, Buf[i]) != retvOk) {
            Printf("Write Fail\r");
            chThdSleepMilliseconds(450);
            REBOOT();
        }
        *PAddr = *PAddr + 8;
    }
    Printf(".");
}
