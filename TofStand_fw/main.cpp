#include "hal.h"
#include "MsgQ.h"
#include "kl_lib.h"
#include "Sequences.h"
#include "uart.h"
#include "shell.h"
#include "usb_cdc.h"
#include "L6470.h"
#include "led.h"

#if 1 // =============== Low level ================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
static void ITask();
static void OnCmd(Shell_t *PShell);

static bool UsbPinWasHi = false;
static TmrKL_t TmrOneSecond {TIME_MS2I(999), evtIdEverySecond, tktPeriodic};

L6470_t Motor{M_SPI};

#define STEPS_IN_STAND  450000

LedBlinker_t Led(GPIOB, 2, omPushPull);

void EndstopHandler();
PinIrq_t EndstopTop{ENDSTOP2, pudPullDown, EndstopHandler};
PinIrq_t EndstopBottom{ENDSTOP1, pudPullDown, EndstopHandler};

void BusyHandler();
PinIrq_t Busy{M_AUX_GPIO, M_BUSY_SYNC1, pudPullUp, BusyHandler};
#endif

int main() {
    // ==== Setup clock ====
    Clk.SetCoreClk(cclk48MHz);
    Clk.UpdateFreqValues();

    // ==== Init OS ====
    halInit();
    chSysInit();

    Led.Init();
    Led.On();

    // ==== Init Hard & Soft ====
    EvtQMain.Init();
    Uart.Init();
    Printf("\r%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
    Clk.PrintFreqs();

    UsbCDC.Init();
    PinSetupInput(USB_DETECT_PIN, pudPullDown); // Usb detect pin

    // Motor
    Motor.Init();
    Motor.SetAcceleration(1200);
    Motor.SetDeceleration(12000);
    Motor.SetStepMode(sm128);
    // Current
    Motor.SetCurrent4Run(4);
    Motor.SetCurrent4Hold(18);

    // Endstops
    EndstopTop.Init(ttRising);
    EndstopBottom.Init(ttRising);
    EndstopTop.EnableIrq(IRQ_PRIO_MEDIUM);
    // Busy
    Busy.Init(ttRising);
    Busy.EnableIrq(IRQ_PRIO_MEDIUM);

    chThdSleepMilliseconds(720); // Let power to stabilize
    // Go top if not yet
    if(!EndstopTop.IsHi()) Motor.Move(dirForward, 54000, STEPS_IN_STAND);

    TmrOneSecond.StartOrRestart();

    Led.Off();

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

//            case evtIdEndstop:
//                PrintfI("Ready e\r\n");
//                if(UsbCDC.IsActive()) UsbCDC.Print("Ready\r\n");
//                break;

            case evtIdBusyFlagHi:
                PrintfI("Ready\r\n");
                if(UsbCDC.IsActive()) UsbCDC.Print("Ready\r\n");
                break;

#if 1 // ======= USB =======
            case evtIdUsbConnect:
//                Printf("USB connect\r");
                UsbCDC.Connect();
                break;
            case evtIdUsbDisconnect:
                UsbCDC.Disconnect();
//                Printf("USB disconnect\r");
                break;
            case evtIdUsbReady:
//                Printf("USB ready\r");
                break;
#endif

            default: break;
        } // switch
    } // while true
}


void EndstopHandler() {
//    Motor.StopSoftAndHold();
//    PrintfI("EndstopHandler\r");
//    EvtQMain.SendNowOrExitI(EvtMsg_t(evtIdEndstop));
    Motor.SwitchLoHi(); // HardStop the motor using switch. Way faster than SPI cmd.
}

void BusyHandler() {
//    PrintfI("BusyHandler\r");
    EvtQMain.SendNowOrExitI(EvtMsg_t(evtIdBusyFlagHi));
}

#if 1 // ======================= Command processing ============================
void OnCmd(Shell_t *PShell) {
    Cmd_t *PCmd = &PShell->Cmd;
//    Printf("%S  ", PCmd->Name);

    // Handle command
    if(PCmd->NameIs("Ping")) PShell->Ack(retvOk);
    else if(PCmd->NameIs("Version")) PShell->Print("%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));

    else if(PCmd->NameIs("Down")) {
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

#if 1 // ==== Read/Write regs ====
    else if(PCmd->NameIs("Get8")) {
        uint8_t Addr = 0;
        if(PCmd->GetNext<uint8_t>(&Addr) != retvOk) { PShell->Ack(retvCmdError); return; }
        uint32_t Value = Motor.GetParam8(Addr);
        Printf("%u  0x%X\r", Value, Value);
    }
    else if(PCmd->NameIs("Get16")) {
        uint8_t Addr = 0;
        if(PCmd->GetNext<uint8_t>(&Addr) != retvOk) { PShell->Ack(retvCmdError); return; }
        uint32_t Value = Motor.GetParam16(Addr);
        Printf("%u  0x%X\r", Value, Value);
    }
    else if(PCmd->NameIs("Get32")) {
        uint8_t Addr = 0;
        if(PCmd->GetNext<uint8_t>(&Addr) != retvOk) { PShell->Ack(retvCmdError); return; }
        uint32_t Value = Motor.GetParam32(Addr);
        Printf("%u  0x%X\r", Value, Value);
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
        if(UsbCDC.IsActive()) UsbCDC.Print("%S\r\n", PCmd->Name);
        PShell->Ack(retvCmdUnknown);
    }
}
#endif
