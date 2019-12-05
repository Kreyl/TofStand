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

LedBlinker_t Led{LED_PIN, omPushPull};
L6470_t Motor{M_SPI};
#define dirUP               dirForward
#define dirDOWN             dirReverse
#define STAND_HEIGHT_mm     625
#define TOP_ES_H_mm         623 // Put this when top EP touched
#define ACCELERATION        135
#define CURRENT4RUN         99
#define CURRENT4HOLD        18
#define RUN_SPEED_HIGH      40000
#define RUN_SPEED_SLOW      9000

// Distance
#define STEPS_PER_MM        1280 // Experimental
#define MM2STEPS(mm)        ((mm) * STEPS_PER_MM)
#define STEPS2MM(steps)     ((steps) / STEPS_PER_MM)
#define DIST_BOTTOM2STOP_mm 18

static TmrKL_t TmrHeight {TIME_MS2I(450), evtIdHeightMeasure, tktPeriodic};
static int32_t Height;

void TopEndstopHandler();
void BottomEndstopHandler();
void TouchEndstopHandler();
void BusyPinHandler();
PinIrq_t EndstopTop{ENDSTOP_TOP, pudPullDown, TopEndstopHandler};
PinIrq_t EndstopBottom{ENDSTOP_BOTTOM, pudPullDown, BottomEndstopHandler};
PinIrq_t EndstopTouch{ENDSTOP_TOUCH, pudPullDown, TouchEndstopHandler};
PinIrq_t BusyPin{M_AUX_GPIO, M_BUSY_SYNC1, pudPullUp, BusyPinHandler};

bool ChamberIsClosed() { return PinIsHi(CHAMBER_CLOSED_PIN); }
bool UsbIsConnected = false;
FATFS FlashFS;
#endif

#if 1 // =============== Statemachine ==============
#define SETTINGS_FNAME      "Settings.ini"

#define STEP_SPD    40000

enum State_t { lgsIdle, lgsGoingUp, lgsGoingDown, lgsGoingToTouch, lgsTouching, lgsNoDevice };

class Logic_t {
public:
    void IPrint(const char *format, ...) {
        va_list args;
        va_start(args, format);
        Uart.IVsPrintf(format, args);
        RpiUart.IVsPrintf(format, args);
        if(UsbMsdCdc.IsActive()) UsbMsdCdc.IVsPrintf(format, args);
        va_end(args);
    }

    void PrintHeightIfRelevant() {
        if(Calibrated) {
            Height = STEPS2MM(Motor.GetAbsPos());
            IPrint("Height %u\r\n", Height);
        }
        else IPrint("Ready\r\n");
    }

    State_t State = lgsIdle;
    bool Calibrated = false;
    bool WaitBusy = false;
    // Params
    uint32_t Acceleration = ACCELERATION, Deceleration = ACCELERATION;
    StepMode_t StepMode = sm128;
    uint32_t Current4Run = CURRENT4RUN, Current4Hold = CURRENT4HOLD;
    uint32_t SpeedFast = RUN_SPEED_HIGH;
    uint32_t SpeedSlow = RUN_SPEED_SLOW;

    void MotorSetup() {
        Motor.Reset();
        Motor.SetAcceleration(Acceleration);
        Motor.SetDeceleration(Deceleration);
        Motor.SetStepMode(StepMode);
        Motor.SetCurrent4Run(Current4Run);
        Motor.SetCurrent4Acc(Current4Run);
        Motor.SetCurrent4Dec(Current4Run);
        Motor.SetCurrent4Hold(Current4Hold);
        Motor.SetStallThreshold(95); // 3A; default is 64 => 2A
        Motor.StopSoftAndHold();
    }

    void GoTop() {
        if(EndstopTop.IsHi()) State = lgsIdle;
        else {
            State = lgsGoingUp;
            Motor.Run(dirUP, SpeedFast);
        }
    }

    void GoToTouch() {
        State = lgsGoingDown;
        WaitBusy = false;
        Motor.Run(dirDOWN, SpeedFast);
    }

    void LoadSettings() {
        ini::Read<uint32_t>(SETTINGS_FNAME, "Motor", "Acceleration", &Acceleration);
        if(Acceleration > 0xFFFF) Acceleration = 12000;
        ini::Read<uint32_t>(SETTINGS_FNAME, "Motor", "Deceleration", &Deceleration);
        if(Deceleration > 0xFFFF) Deceleration = 12000;

        uint32_t tmp = 7;
        ini::Read<uint32_t>(SETTINGS_FNAME, "Motor", "StepMode", &tmp);
        if(tmp > 7) tmp = 7;
        StepMode = (StepMode_t)tmp;

        ini::Read<uint32_t>(SETTINGS_FNAME, "Motor", "Current4Run", &Current4Run);
        if(Current4Run > 255) Current4Run = 255;
        ini::Read<uint32_t>(SETTINGS_FNAME, "Motor", "Current4Hold", &Current4Hold);
        if(Current4Hold > 255) Current4Hold = 255;

        ini::Read<uint32_t>(SETTINGS_FNAME, "Motor", "SpeedFast", &SpeedFast);
        if(SpeedFast > RUN_SPEED_HIGH) SpeedFast = RUN_SPEED_HIGH;
        ini::Read<uint32_t>(SETTINGS_FNAME, "Motor", "SpeedSlow", &SpeedSlow);
        if(SpeedSlow > RUN_SPEED_HIGH) SpeedSlow = RUN_SPEED_HIGH;
    }

#if 1  // ==== Events ====
    void OnTopEndstop() {
        State = lgsIdle;
        WaitBusy = false;
        // Set top value just in case
        if(!Calibrated) {
            Motor.SetAbsPos(MM2STEPS(TOP_ES_H_mm));
            Calibrated = true;
        }
        PrintHeightIfRelevant();
    }

    void OnBottomEndstop() {
        if(State == lgsGoingDown) {
            State = lgsGoingToTouch;
            WaitBusy = true;
            Motor.Move(dirDOWN, SpeedSlow, MM2STEPS(DIST_BOTTOM2STOP_mm));
        }
    }

    void OnTouchEndstop() {
        Motor.ResetAbsPos();
        WaitBusy = false;
        State = lgsTouching;
        Calibrated = true;
        PrintHeightIfRelevant();
    }

    void OnBusyFlag() {
        if(WaitBusy and Motor.IsStopped()) {
            WaitBusy = false;
            if(State == lgsGoingToTouch) { // No device touched
                Calibrated = false;
                State = lgsNoDevice;
                PrintfI("No device\r\n");
            }
            else { // Not going-to-touch
                State = lgsIdle;
            }
            PrintHeightIfRelevant();
        }
    }

    void OnChamberOpen() {
        Motor.StopSoftAndHiZ();
        GoTop();
        SegmentShowOPEn();
    }
    void OnChamberClose() {
        SegmentClear();
    }
#endif
} Logic;
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
    if(err == FR_OK) Logic.LoadSettings();
    else Printf("FS error\r");

    UsbMsdCdc.Init();

    chThdSleepMilliseconds(720); // Let power to stabilize

    // Motor
    Motor.Init();
    Logic.MotorSetup();

    // Endstops
    EndstopTop.Init(ttRising);
    if(EndstopTop.IsHi()) Logic.OnTopEndstop();
    EndstopBottom.Init(ttRising);
    EndstopTouch.Init(ttRising);
    EndstopTop.EnableIrq(IRQ_PRIO_MEDIUM);
    EndstopBottom.EnableIrq(IRQ_PRIO_MEDIUM);
    EndstopTouch.EnableIrq(IRQ_PRIO_MEDIUM);
    // Busy
    BusyPin.Init(ttFalling);
    BusyPin.EnableIrq(IRQ_PRIO_MEDIUM);

    Logic.GoTop();
    if(!ChamberIsClosed()) SegmentShowOPEn();
    TmrHeight.StartOrRestart();

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
//                Printf("Btn %u; %u\r", Msg.BtnEvtInfo.Type, Msg.BtnEvtInfo.BtnID);
                Motor.StopSoftAndHiZ();
                uint32_t Speed;
                switch(Msg.BtnEvtInfo.BtnID) {
                    case 0: // Up
                        Speed = (GetBtnState(2) == BTN_HOLDDOWN_STATE)? Logic.SpeedFast : Logic.SpeedSlow;
                        if(Msg.BtnEvtInfo.Type == beShortPress) {
                            if(EndstopTop.IsHi()) Logic.State = lgsIdle;
                            else {
                                Logic.State = lgsGoingUp;
                                Motor.Run(dirUP, Speed);
                            }
                        }
                        break;
                    case 1: // Down
                        Speed = (GetBtnState(2) == BTN_HOLDDOWN_STATE)? Logic.SpeedFast : Logic.SpeedSlow;
                        if(Msg.BtnEvtInfo.Type == beShortPress) {
                            if(EndstopTouch.IsHi()) Logic.State = lgsTouching;
                            else {
                                Logic.State = lgsGoingDown;
                                Motor.Run(dirDOWN, Speed);
                            }
                        }
                        break;
                    case 2: // Move Fast
                        Speed = (Msg.BtnEvtInfo.Type == beShortPress)? Logic.SpeedFast : Logic.SpeedSlow;
                        if(GetBtnState(0) == BTN_HOLDDOWN_STATE) {
                            if(EndstopTop.IsHi()) Logic.State = lgsIdle;
                            else {
                                Logic.State = lgsGoingUp;
                                Motor.Run(dirUP, Speed);
                            }
                        }
                        else if(GetBtnState(1) == BTN_HOLDDOWN_STATE) {
                            if(EndstopTouch.IsHi()) Logic.State = lgsTouching;
                            else {
                                Logic.State = lgsGoingDown;
                                Motor.Run(dirDOWN, Speed);
                            }
                        }
                        break;
                } // switch
            } break;

            case evtIdHeightMeasure: {
                Height = Motor.GetAbsPos();
                if(ChamberIsClosed()) {
                    Height = STEPS2MM(Height);
                    if(Height < 0 or Height > 700) SegmentClear();
                    else SegmentPutUint(Height, 10);
                }
            } break;

#if 1       // ======= Logic Events =======
            case evtIdEndstopTop:    Logic.OnTopEndstop();    break;
            case evtIdEndstopBottom: Logic.OnBottomEndstop(); break;
            case evtIdEndstopTouch:  Logic.OnTouchEndstop();  break;
            case evtIdBusyFlag:      Logic.OnBusyFlag();      break;
            case evtIdChamberOpen:   Logic.OnChamberOpen();   break;
            case evtIdChamberClose:  Logic.OnChamberClose();  break;
#endif

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

#if 1 // ==== Small utils ====
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
#endif

#if 1 // ======= Endstops =======
void TopEndstopHandler() {
    Motor.StopNow();
//    PrintfI("%S\r", __FUNCTION__);
    EvtQMain.SendNowOrExitI(EvtMsg_t(evtIdEndstopTop));
}

void BottomEndstopHandler() {
    if(Logic.State != lgsGoingUp) Motor.StopNow();
//    PrintfI("%S\r", __FUNCTION__);
    EvtQMain.SendNowOrExitI(EvtMsg_t(evtIdEndstopBottom));
}

void TouchEndstopHandler() {
    Motor.StopNow();
//    PrintfI("%S\r", __FUNCTION__);
    EvtQMain.SendNowOrExitI(EvtMsg_t(evtIdEndstopTouch));
}

void BusyPinHandler() {
//    PrintfI("%S\r", __FUNCTION__);
    EvtQMain.SendNowOrExitI(EvtMsg_t(evtIdBusyFlag));
}
#endif

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

#if 1 // ==== Motor control ====
    else if(PCmd->NameIs("MRst")) {
        Logic.MotorSetup();
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("RstAbs")) {
        Motor.ResetAbsPos();
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("Touch")) {
        Logic.GoToTouch();
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("Down")) {
        Motor.StopSoftAndHold();
        // Check if touch endstop reached
        if(EndstopTouch.IsHi()) { PShell->Print("Height 0\r\n"); return; }
        uint32_t h_mm = STAND_HEIGHT_mm, ISpd = SPD_MAX;
        // Get speed (may absent)
        if(PCmd->GetNext<uint32_t>(&ISpd) == retvOk) {
            if(ISpd == 0)      { PShell->Ack(retvBadValue); return; }
            if(ISpd > SPD_MAX) { PShell->Ack(retvBadValue); return; }
            // Get h (may absent)
            if(PCmd->GetNext<uint32_t>(&h_mm) == retvOk) {
                if(h_mm == 0) { PShell->Ack(retvBadValue); return; }
                Logic.State = lgsGoingDown;
                Logic.WaitBusy = true;
                Motor.Move(dirDOWN, ISpd, MM2STEPS(h_mm));
                PShell->Ack(retvOk);
                return;
            }
        }
        Logic.State = lgsGoingDown;
        Logic.WaitBusy = false;
        Motor.Run(dirDOWN, ISpd);
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("Up")) {
        Motor.StopSoftAndHold();
        // Check if top endstop reached
        if(EndstopTop.IsHi()) { Logic.PrintHeightIfRelevant(); return; }
        uint32_t h_mm = STAND_HEIGHT_mm, ISpd = SPD_MAX;
        // Get speed (may absent)
        if(PCmd->GetNext<uint32_t>(&ISpd) == retvOk) {
            if(ISpd == 0)      { PShell->Ack(retvBadValue); return; }
            if(ISpd > SPD_MAX) { PShell->Ack(retvBadValue); return; }
            // Get h (may absent)
            if(PCmd->GetNext<uint32_t>(&h_mm) == retvOk) {
                if(h_mm == 0) { PShell->Ack(retvBadValue); return; }
                Logic.State = lgsGoingUp;
                Logic.WaitBusy = true;
                Motor.Move(dirUP, ISpd, MM2STEPS(h_mm));
                PShell->Ack(retvOk);
                return;
            }
        }
        Logic.State = lgsGoingUp;
        Logic.WaitBusy = false;
        Motor.Run(dirUP, ISpd);
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("Stop")) {
        Motor.StopSoftAndHold();
        Logic.State = lgsIdle;
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("HiZ")) {
        Motor.StopSoftAndHiZ();
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("SetCur")) {
        uint8_t FCurr = 0;
        if(PCmd->GetNext<uint8_t>(&FCurr) != retvOk) { PShell->Ack(retvCmdError); return; }
        Motor.SetCurrent4Run(FCurr);
//        Motor.SetCurrent4Hold(FCurr);
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("SetSM")) {
        uint8_t FSm = 0;
        if(PCmd->GetNext<uint8_t>(&FSm) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(FSm > 7) FSm = 7;
        Motor.StopSoftAndHiZ(); // Otherwise will be ignored
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
    else if(PCmd->NameIs("Set24")) {
        uint8_t Addr = 0;
        uint32_t Value;
        if(PCmd->GetNext<uint8_t>(&Addr) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<uint32_t>(&Value) != retvOk) { PShell->Ack(retvCmdError); return; }
        Motor.SetParam24(Addr, Value);
        PShell->Ack(retvOk);
    }
#endif

    else {
        Printf("%S\r\n", PCmd->Name);
        PShell->Ack(retvCmdUnknown);
    }
}
#endif
