#include "hal.h"
#include "MsgQ.h"
#include "kl_lib.h"
#include "Sequences.h"
#include "uart.h"
#include "shell.h"


#if 1 // =============== Low level ================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
static void ITask();
static void OnCmd(Shell_t *PShell);

//static TmrKL_t TmrOneSecond {MS2ST(999), evtIdEverySecond, tktPeriodic};

#endif

int main() {
#if 1 // Low level init
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

//    SimpleSensors::Init();
//    UsbMsdCdc.Init();

//    TmrOneSecond.StartOrRestart();

#endif

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
                break;

            case evtIdAdcRslt:
//                Printf("Adc: %u; ExtPwr: %u; Charging: %u\r", Msg.Value, Power.ExternalPwrOn(), Power.IsCharging());
                break;

#if 0 // ======= USB =======
            case evtIdUsbConnect:
                Printf("USB connect\r");
                Clk.SetupFlashLatency(48);
                Clk.SetupBusDividers(ahbDiv1, apbDiv2, apbDiv2); // 48 MHz AHB, 24 MHz APB1, 24 MHz APB2
                Clk.UpdateFreqValues();
                Uart.OnClkChange();
                Clk.PrintFreqs();
                chThdSleepMilliseconds(270);
                UsbMsdCdc.Connect();
                break;
            case evtIdUsbDisconnect:
                UsbMsdCdc.Disconnect();
                Clk.SetupBusDividers(ahbDiv2, apbDiv1, apbDiv1); // 24 MHz AHB, 24 MHz APB1, 24 MHz APB2
                Clk.UpdateFreqValues();
                Clk.SetupFlashLatency(Clk.AHBFreqHz/1000000);
                Uart.OnClkChange();
                Printf("USB disconnect\r");
                Clk.PrintFreqs();
                break;
            case evtIdUsbReady:
                Printf("USB ready\r");
                break;
#endif

            default: break;
        } // switch
    } // while true
}

//void ProcessUsbDetect(PinSnsState_t *PState, uint32_t Len) {
//    if(*PState == pssRising) EvtQMain.SendNowOrExit(EvtMsg_t(evtIdUsbConnect));
//    else if(*PState == pssFalling) EvtQMain.SendNowOrExit(EvtMsg_t(evtIdUsbDisconnect));
//}

#if 1 // ======================= Command processing ============================
void OnCmd(Shell_t *PShell) {
    Cmd_t *PCmd = &PShell->Cmd;
//    Printf("%S  ", PCmd->Name);

    // Handle command
    if(PCmd->NameIs("Ping")) PShell->Ack(retvOk);
    else if(PCmd->NameIs("Version")) PShell->Print("%S %S\r", APP_NAME, XSTRINGIFY(BUILD_TIME));

    else if(PCmd->NameIs("Start")) {
        uint8_t Slot;
        uint16_t Volume;
        char *S;
        if(PCmd->GetNext<uint8_t>(&Slot) != retvOk)    { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNext<uint16_t>(&Volume) != retvOk) { PShell->Ack(retvCmdError); return; }
        if(PCmd->GetNextString(&S) != retvOk) { PShell->Ack(retvCmdError); return; }
    }

    else PShell->Ack(retvCmdUnknown);
}
#endif
