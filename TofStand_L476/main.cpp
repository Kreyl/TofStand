#include "hal.h"
#include "MsgQ.h"
#include "Sequences.h"
#include "shell.h"
#include "led.h"
#include "SimpleSensors.h"
#include "buttons.h"
#include "L6470.h"
#include "7segment.h"
#include "usb_msdcdc.h"
#include "ff.h"
#include "kl_fs_utils.h"

#if 1 // =============== Defines ================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
static const UartParams_t RpiUartParams(115200, RPI_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
CmdUart_t RpiUart{&RpiUartParams};

static void ITask();
static void OnCmd(Shell_t *PShell);

static TmrKL_t TmrOneSecond {TIME_MS2I(999), evtIdEverySecond, tktPeriodic};

LedBlinker_t Led{LED_PIN, omPushPull};
L6470_t Motor{M_SPI};
#define STEPS_IN_STAND      1000008
#define BTN_RUN_SPEED_HIGH  45000
#define BTN_RUN_SPEED_LOW   9000

bool UsbIsConnected = false;

// Filesystem etc.
FATFS FlashFS;
#define SETTINGS_FNAME      "Settings.ini"
struct Settings_t {
    uint32_t Acceleration = 1200, Deceleration = 12000;
    StepMode_t StepMode = smFull; //sm128;
    uint32_t Current4Run = 18, Current4Hold = 18;
    uint32_t StartSpeed = 27000;
    void Load();
} Settings;
#endif

int main(void) {
    // Start Watchdog. Will reset in main thread by periodic 1 sec events.
//    Iwdg::InitAndStart(4500);
//    Iwdg::DisableInDebug();
    // Setup clock frequency
    Clk.SetCoreClk(cclk48MHz);
    // 48MHz clock
    Clk.SetupSai1Qas48MhzSrc();
    Clk.UpdateFreqValues();

    // Init OS
    halInit();
    chSysInit();

    // ==== Init hardware ====
    EvtQMain.Init();
    Uart.Init();
    Printf("\r%S %S\r", APP_NAME, XSTRINGIFY(BUILD_TIME));
    Clk.PrintFreqs();

    // Disable dualbank if enabled
    if(Flash::DualbankIsEnabled()) {
        Printf("Dualbank enabled, disabling\r");
        chThdSleepMilliseconds(45);
        Flash::DisableDualbank();   // Will reset inside
    }

    RpiUart.Init();

    // LEDs
    Led.Init();
    Led.StartOrRestart(lsqCmd);
    SegmentInit();

    SimpleSensors::Init();

    // Init filesystem
    FRESULT err;
    err = f_mount(&FlashFS, "", 0);
    if(err == FR_OK) Settings.Load();
    else Printf("FS error\r");

    UsbMsdCdc.Init();

    // Motor
    Motor.Init();
    Motor.SetAcceleration(Settings.Acceleration);
    Motor.SetDeceleration(Settings.Deceleration);
    Motor.SetStepMode(Settings.StepMode);
    // Current
    Motor.SetCurrent4Run(Settings.Current4Run);
    Motor.SetCurrent4Hold(Settings.Current4Hold);
    Motor.StopSoftAndHold();

//    TmrOneSecond.StartOrRestart();

    // ==== Main cycle ====
    ITask();
}

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
        switch(Msg.ID) {
            case evtIdShellCmd:
                Led.StartOrContinue(lsqCmd);
                OnCmd((Shell_t*)Msg.Ptr);
                ((Shell_t*)Msg.Ptr)->SignalCmdProcessed();
                break;

            case evtIdButtons: {
                Printf("Btn %u; %u\r", Msg.BtnEvtInfo.Type, Msg.BtnEvtInfo.BtnID);
                Motor.StopSoftAndHiZ();
                uint32_t Speed;
                switch(Msg.BtnEvtInfo.BtnID) {
                    case 0: // Up
                        Speed = (GetBtnState(2) == BTN_HOLDDOWN_STATE)? BTN_RUN_SPEED_HIGH : BTN_RUN_SPEED_LOW;
                        if(Msg.BtnEvtInfo.Type == beShortPress) Motor.Run(dirForward, Speed);
                        break;
                    case 1: // Down
                        Speed = (GetBtnState(2) == BTN_HOLDDOWN_STATE)? BTN_RUN_SPEED_HIGH : BTN_RUN_SPEED_LOW;
                        if(Msg.BtnEvtInfo.Type == beShortPress) Motor.Run(dirReverse, Speed);
                        break;
                    case 2: // Move Fast
                        Speed = (Msg.BtnEvtInfo.Type == beShortPress)? BTN_RUN_SPEED_HIGH : BTN_RUN_SPEED_LOW;
                        if(GetBtnState(0) == BTN_HOLDDOWN_STATE) Motor.Run(dirForward, Speed);
                        else if(GetBtnState(1) == BTN_HOLDDOWN_STATE) Motor.Run(dirReverse, Speed);
                        break;
                } // switch
            } break;

#if 1       // ======= USB =======
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

void ProcessChamberClosed(PinSnsState_t *PState, uint32_t Len) {
    if(*PState == pssRising) EvtQMain.SendNowOrExit(EvtMsg_t(evtIdChamberClose));
    else if(*PState == pssFalling) EvtQMain.SendNowOrExit(EvtMsg_t(evtIdChamberOpen));
}

void ProcessUsbConnect(PinSnsState_t *PState, uint32_t Len) {
    if((*PState == pssRising or *PState == pssHi) and !UsbIsConnected) {
        UsbIsConnected = true;
        EvtQMain.SendNowOrExit(EvtMsg_t(evtIdUsbConnect));
    }
    else if((*PState == pssFalling or *PState == pssLo) and UsbIsConnected) {
        UsbIsConnected = false;
        EvtQMain.SendNowOrExit(EvtMsg_t(evtIdUsbDisconnect));
    }
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

    // Handle command
    if(PCmd->NameIs("Ping")) PShell->Ack(retvOk);
    else if(PCmd->NameIs("Version")) PShell->Print("%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));

    else if(PCmd->NameIs("Seg")) {
        uint8_t v1, v2, v3, v4, pointmsk = 0;
        if(PCmd->GetNext<uint8_t>(&v1) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<uint8_t>(&v2) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<uint8_t>(&v3) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<uint8_t>(&v4) != retvOk) { PShell->Ack(retvCmdError); return; }
        PCmd->GetNext<uint8_t>(&pointmsk);
        SegmentShow(v1, v2, v3, v4, pointmsk);
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("Rst")) {
        REBOOT();
    }

    else if(PCmd->NameIs("del")) {
        PShell->Ack(f_unlink("Settings.ini"));
    }

#if 1 // ==== Motor control ====
    else if(PCmd->NameIs("MRst")) {
        Motor.Reset();
        Motor.SetAcceleration(Settings.Acceleration);
        Motor.SetDeceleration(Settings.Deceleration);
        Motor.SetStepMode(Settings.StepMode);
        // Current
        Motor.SetCurrent4Run(Settings.Current4Run);
        Motor.SetCurrent4Hold(Settings.Current4Hold);
        Motor.StopSoftAndHold();
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("Down")) {
//        Motor.SwitchLoHi();
        // Check if top endstop reched
//        if(EndstopBottom.IsHi()) { PShell->Ack(retvBadState); return; }
        uint32_t ISteps = STEPS_IN_STAND, ISpd = SPD_MAX;
        PCmd->GetNext<uint32_t>(&ISpd); // May absent
        PCmd->GetNext<uint32_t>(&ISteps); // May absent
        if(ISteps == 0)    { PShell->Ack(retvBadValue); return; }
        if(ISpd == 0)      { PShell->Ack(retvBadValue); return; }
        if(ISpd > SPD_MAX) { PShell->Ack(retvBadValue); return; }
        Motor.Move(dirReverse, ISpd, ISteps);
        PShell->Ack(retvOk);
    }
    else if(PCmd->NameIs("Up")) {
//        Motor.SwitchLoHi();
        // Check if top endstop reched
//        if(EndstopTop.IsHi()) { PShell->Ack(retvBadState); return; }
        uint32_t ISteps = STEPS_IN_STAND, ISpd = SPD_MAX;
        PCmd->GetNext<uint32_t>(&ISpd); // May absent
        PCmd->GetNext<uint32_t>(&ISteps); // May absent
        if(ISteps == 0)    { PShell->Ack(retvBadValue); return; }
        if(ISpd == 0)      { PShell->Ack(retvBadValue); return; }
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

    else if(PCmd->NameIs("SetSM")) {
        uint8_t FSm = 0;
        if(PCmd->GetNext<uint8_t>(&FSm) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(FSm > 7) FSm = 7;
        Motor.SetStepMode((StepMode_t)FSm);
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
        Printf("%S\r\n", PCmd->Name);
        PShell->Ack(retvCmdUnknown);
    }
}
#endif

