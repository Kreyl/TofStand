#include "hal.h"
#include "MsgQ.h"
#include "kl_lib.h"
#include "Sequences.h"
#include "uart.h"
#include "shell.h"
#include "usb_cdc.h"
#include "L6470.h"

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
#define SPD_MAX     2700000

void EndstopHandler();
PinIrq_t EndstopTop{ENDSTOP1, pudPullDown, EndstopHandler};
PinIrq_t EndstopBottom{ENDSTOP2, pudPullDown, EndstopHandler};
#endif

int main() {
    // ==== Setup clock ====
    Clk.SetCoreClk(cclk48MHz);
    Clk.UpdateFreqValues();

    // ==== Init OS ====
    halInit();
    chSysInit();

    // ==== Init Hard & Soft ====
    EvtQMain.Init();
    Uart.Init();
    Printf("\r%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
    Clk.PrintFreqs();

    UsbCDC.Init();
    PinSetupInput(USB_DETECT_PIN, pudPullDown); // Usb detect pin

    // Motor
    Motor.Init();
    Motor.SetAcceleration(120);
    Motor.SetDeceleration(1200);
    Motor.SetStepMode(sm128);

    // Endstops
    EndstopTop.Init(ttRising);
    EndstopBottom.Init(ttRising);
    EndstopTop.EnableIrq(IRQ_PRIO_MEDIUM);

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

            case evtIdEndstop:
                PrintfI("Endstop\r");
                break;

#if 1 // ======= USB =======
            case evtIdUsbConnect:
                Printf("USB connect\r");
                UsbCDC.Connect();
                break;
            case evtIdUsbDisconnect:
                UsbCDC.Disconnect();
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
    Motor.StopSoftAndHiZ();
    PrintfI("EndstopHandler\r");
    EvtQMain.SendNowOrExitI(EvtMsg_t(evtIdEndstop));
}

#if 1 // ======================= Command processing ============================
void OnCmd(Shell_t *PShell) {
    Cmd_t *PCmd = &PShell->Cmd;
//    Printf("%S  ", PCmd->Name);

    // Handle command
    if(PCmd->NameIs("Ping")) PShell->Ack(retvOk);
    else if(PCmd->NameIs("Version")) PShell->Print("%S %S\r", APP_NAME, XSTRINGIFY(BUILD_TIME));

    else if(PCmd->NameIs("Home")) {
        Motor.Run(dirForward, SPD_MAX);
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("Down")) {
        // Check if bottom endstop reched
        if(EndstopBottom.IsHi()) { PShell->Ack(retvBadState); return; }
        uint32_t ISpd = 0;
        if(PCmd->GetNext<uint32_t>(&ISpd) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(ISpd == 0) { PShell->Ack(retvBadValue); return; }
        if(ISpd > SPD_MAX) { PShell->Ack(retvBadValue); return; }
        Motor.Run(dirForward, ISpd);
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("Up")) {
        // Check if top endstop reched
        if(EndstopTop.IsHi()) { PShell->Ack(retvBadState); return; }
        uint32_t ISpd = 0;
        if(PCmd->GetNext<uint32_t>(&ISpd) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(ISpd == 0) { PShell->Ack(retvBadValue); return; }
        if(ISpd > SPD_MAX) { PShell->Ack(retvBadValue); return; }
        Motor.Run(dirReverse, ISpd);
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("Stop")) {
        Motor.StopSoftAndHiZ();
        PShell->Ack(retvOk);
    }

    else PShell->Ack(retvCmdUnknown);
}
#endif
