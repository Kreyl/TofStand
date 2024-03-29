#include "hal.h"
#include "MsgQ.h"
#include "kl_lib.h"
#include "Sequences.h"
#include "uart.h"
#include "shell.h"
#include "usb_msdcdc.h"
#include "L6470.h"
#include "led.h"
#include "ff.h"
#include "kl_fs_utils.h"

#if 1 // =============== Low level ================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(256000, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
static void ITask();
static void OnCmd(Shell_t *PShell);

static bool UsbPinWasHi = false;
static TmrKL_t TmrOneSecond {TIME_MS2I(999), evtIdEverySecond, tktPeriodic};

LedBlinker_t Led(GPIOB, 2, omPushPull);

L6470_t Motor{M_SPI};
#define STEPS_IN_STAND  1000008

void EndstopHandler();
PinIrq_t EndstopTop{ENDSTOP2, pudPullDown, EndstopHandler};
PinIrq_t EndstopBottom{ENDSTOP1, pudPullDown, EndstopHandler};

void BusyHandler();
PinIrq_t Busy{M_AUX_GPIO, M_BUSY_SYNC1, pudPullUp, BusyHandler};

// Filesystem etc.
FATFS FlashFS;
#define SETTINGS_FNAME      "Settings.ini"
struct Settings_t {
    uint32_t Acceleration = 1200, Deceleration = 12000;
    StepMode_t StepMode = sm128;
    uint32_t Current4Run = 18, Current4Hold = 18;
    uint32_t StartSpeed = 27000;
    void Load();
} Settings;
#endif

int main() {
    // ===== Setup clock =====
    Clk.EnablePrefetch();
    if(Clk.EnableHSE() == retvOk) {
        Clk.SetupPLLSrc(pllSrcPrediv);
        Clk.SetupFlashLatency(24);
        // 12MHz * 4 = 48MHz
        if(Clk.SetupPllMulDiv(pllMul4, preDiv1) == retvOk) {
            Clk.SetupBusDividers(ahbDiv2, apbDiv1, apbDiv1);
            if(Clk.EnablePLL() == retvOk) Clk.SwitchToPLL();
            Clk.SetUsbPrescalerTo1(); // This bit must be valid before enabling the USB clock in the RCC_APB1ENR register
        }
    }
    Clk.UpdateFreqValues();

    // ==== Init OS ====
    halInit();
    chSysInit();

    Led.Init();
    Led.On();

    // ==== Init Hard & Soft ====
    EvtQMain.Init();
    Uart.Init();
    AFIO->MAPR |= AFIO_MAPR_USART1_REMAP; // Remap UART to PB6/PB7 pins
    Printf("\r%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
    Clk.PrintFreqs();

    // Init filesystem
    FRESULT err;
    err = f_mount(&FlashFS, "", 0);
    if(err == FR_OK) Settings.Load();
    else Printf("FS error\r");

    UsbMsdCdc.Init();
    PinSetupAnalog(USB_PULLUP);
    PinSetupInput(USB_DETECT_PIN, pudPullDown); // Usb detect pin

    // Motor
    Motor.Init();
    Motor.SetAcceleration(Settings.Acceleration);
    Motor.SetDeceleration(Settings.Deceleration);
    Motor.SetStepMode(Settings.StepMode);
    // Current
    Motor.SetCurrent4Run(Settings.Current4Run);
    Motor.SetCurrent4Hold(Settings.Current4Hold);
    Motor.StopSoftAndHold();

    // Endstops
    EndstopTop.Init(ttRising);
    EndstopBottom.Init(ttRising);
    EndstopTop.EnableIrq(IRQ_PRIO_MEDIUM);
    // Busy
    Busy.Init(ttRising);
    Busy.EnableIrq(IRQ_PRIO_MEDIUM);

    chThdSleepMilliseconds(720); // Let power to stabilize
    // Go top if not yet
    if(!EndstopTop.IsHi()) Motor.Move(dirForward, Settings.StartSpeed, STEPS_IN_STAND);

    TmrOneSecond.StartOrRestart();

    // ==== Main cycle ====
    ITask();
}

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
//        Printf("Msg.ID %u\r", Msg.ID);
        switch(Msg.ID) {
            case evtIdShellCmd:
                Led.StartOrContinue(lsqCmd);
                OnCmd((Shell_t*)Msg.Ptr);
                ((Shell_t*)Msg.Ptr)->SignalCmdProcessed();
                break;

            case evtIdEverySecond:
//                Printf("Second\r");
                // Check if USB connected/disconnected
                if(PinIsHi(USB_DETECT_PIN) and !UsbPinWasHi) {
                    UsbPinWasHi = true;
                    EvtQMain.SendNowOrExit(EvtMsg_t(evtIdUsbConnect));
                }
                else if(!PinIsHi(USB_DETECT_PIN) and UsbPinWasHi) {
                    UsbPinWasHi = false;
                    EvtQMain.SendNowOrExit(EvtMsg_t(evtIdUsbDisconnect));
                }
                break;

            case evtIdBusyFlagHi:
                PrintfI("Ready\r\n");
                UsbMsdCdc.Print("Ready\r\n");
                break;

#if 1 // ======= USB =======
            case evtIdUsbConnect:
                Printf("USB connect\r");
                UsbMsdCdc.Connect();
                break;
            case evtIdUsbDisconnect:
                UsbMsdCdc.Disconnect();
                Printf("USB disconnect\r");
                break;
            case evtIdUsbReady:
                Printf("USB ready\r");
                break;
#endif

            default: break;
        } // switch
    } // while true
}


void EndstopHandler() {
//    PrintfI("Endstop\r");
    Motor.SwitchLoHi(); // HardStop the motor using switch. Way faster than SPI cmd.
}

void BusyHandler() {
//    PrintfI("BusyHandler\r");
    EvtQMain.SendNowOrExitI(EvtMsg_t(evtIdBusyFlagHi));
}


void Settings_t::Load() {
    ini::Read<uint32_t>(SETTINGS_FNAME, "Motor", "Acceleration", &Settings.Acceleration);
    if(Settings.Acceleration > 0xFFFF) Settings.Acceleration = 1200;

    ini::Read<uint32_t>(SETTINGS_FNAME, "Motor", "Deceleration", &Settings.Deceleration);
    if(Settings.Acceleration > 0xFFFF) Settings.Acceleration = 12000;

    uint32_t tmp = 7;
    ini::Read<uint32_t>(SETTINGS_FNAME, "Motor", "StepMode", &tmp);
    if(tmp > 7) tmp = 7;
    Settings.StepMode = (StepMode_t)tmp;

    ini::Read<uint32_t>(SETTINGS_FNAME, "Motor", "Current4Run", &Settings.Current4Run);
    if(Settings.Current4Run > 0xFF) Settings.Current4Run = 18;
    ini::Read<uint32_t>(SETTINGS_FNAME, "Motor", "Current4Hold", &Settings.Current4Hold);
    if(Settings.Current4Hold > 0xFF) Settings.Current4Hold = 18;

    ini::Read<uint32_t>(SETTINGS_FNAME, "Motor", "StartSpeed", &Settings.StartSpeed);
    if(Settings.StartSpeed > 90000) Settings.Current4Hold = 90000;
}

#if 1 // ======================= Command processing ============================
void OnCmd(Shell_t *PShell) {
    Cmd_t *PCmd = &PShell->Cmd;
//    Printf("%S  ", PCmd->Name);
    if(PCmd->NameIs("Ping")) PShell->Ack(retvOk);
    else if(PCmd->NameIs("Version")) PShell->Print("%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
    else if(PCmd->NameIs("mem")) PrintMemoryInfo();

    else if(PCmd->NameIs("in")) {
        if(EndstopBottom.IsHi()) PShell->Print("Bottom Hi\r");
        else PShell->Print("Bottom Lo\r");
        if(EndstopTop.IsHi()) PShell->Print("Top Hi\r");
        else PShell->Print("Top Lo\r");
    }

    else if(PCmd->NameIs("Rst")) {
        REBOOT();
    }

#if 1 // ==== Motor control ====
    else if(PCmd->NameIs("Down")) {
//        Motor.SwitchLoHi();
        // Check if top endstop reched
        if(EndstopBottom.IsHi()) { PShell->Ack(retvBadState); return; }
        uint32_t ISteps = STEPS_IN_STAND, ISpd = SPD_MAX;
        PCmd->GetNext<uint32_t>(&ISpd); // May absent
        PCmd->GetNext<uint32_t>(&ISteps); // May absent
        if(ISteps == 0) { PShell->Ack(retvBadValue); return; }
        if(ISpd == 0) { PShell->Ack(retvBadValue); return; }
        if(ISpd > SPD_MAX) { PShell->Ack(retvBadValue); return; }
        Motor.Move(dirReverse, ISpd, ISteps);
        PShell->Ack(retvOk);
    }
    else if(PCmd->NameIs("Up")) {
//        Motor.SwitchLoHi();
        // Check if top endstop reched
        if(EndstopTop.IsHi()) { PShell->Ack(retvBadState); return; }
        uint32_t ISteps = STEPS_IN_STAND, ISpd = SPD_MAX;
        PCmd->GetNext<uint32_t>(&ISpd); // May absent
        PCmd->GetNext<uint32_t>(&ISteps); // May absent
        if(ISteps == 0) { PShell->Ack(retvBadValue); return; }
        if(ISpd == 0) { PShell->Ack(retvBadValue); return; }
        if(ISpd > SPD_MAX) { PShell->Ack(retvBadValue); return; }
        Motor.Move(dirForward, ISpd, ISteps);
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("Stop")) {
        Motor.StopSoftAndHold();
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("SetCur")) {
        uint8_t FCurr = 0;
        if(PCmd->GetNext<uint8_t>(&FCurr) != retvOk) { PShell->Ack(retvCmdError); return; }
        Motor.SetCurrent4Run(FCurr);
        Motor.SetCurrent4Hold(FCurr);
        PShell->Ack(retvOk);
    }
#endif

#if 1 // ==== Read/Write regs ====
    else if(PCmd->NameIs("Get8")) {
        uint8_t Addr = 0;
        if(PCmd->GetNext<uint8_t>(&Addr) != retvOk) { PShell->Ack(retvCmdError); return; }
        uint32_t Value = Motor.GetParam8(Addr);
        PShell->Print("%u  0x%X\r", Value, Value);
    }
    else if(PCmd->NameIs("Get16")) {
        uint8_t Addr = 0;
        if(PCmd->GetNext<uint8_t>(&Addr) != retvOk) { PShell->Ack(retvCmdError); return; }
        uint32_t Value = Motor.GetParam16(Addr);
        PShell->Print("%u  0x%X\r", Value, Value);
    }
    else if(PCmd->NameIs("Get32")) {
        uint8_t Addr = 0;
        if(PCmd->GetNext<uint8_t>(&Addr) != retvOk) { PShell->Ack(retvCmdError); return; }
        uint32_t Value = Motor.GetParam32(Addr);
        PShell->Print("%u  0x%X\r", Value, Value);
    }

    else if(PCmd->NameIs("Set8")) {
        uint8_t Addr = 0;
        uint32_t Value;
        if(PCmd->GetNext<uint8_t>(&Addr) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<uint32_t>(&Value) != retvOk) { PShell->Ack(retvCmdError); return; }
        Motor.SetParam8(Addr, Value);
        PShell->Ack(retvOk);
    }
    else if(PCmd->NameIs("Set16")) {
        uint8_t Addr = 0;
        uint32_t Value;
        if(PCmd->GetNext<uint8_t>(&Addr) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<uint32_t>(&Value) != retvOk) { PShell->Ack(retvCmdError); return; }
        Motor.SetParam16(Addr, Value);
        PShell->Ack(retvOk);
    }
#endif

    else {
        PShell->Print("%S\r\n", PCmd->Name);
//        if(UsbCDC.IsActive()) UsbCDC.Print("%S\r\n", PCmd->Name);
        PShell->Ack(retvCmdUnknown);
    }
}
#endif
