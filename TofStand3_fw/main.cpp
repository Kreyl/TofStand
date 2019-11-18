#include "hal.h"
#include "MsgQ.h"
#include "kl_lib.h"
#include "Sequences.h"
#include "uart.h"
#include "shell.h"
#include "led.h"
#include "buttons.h"
#include "PinSnsSettings.h"
//#include "7segment.h"
//#include "LoadCtrl.h"
//#include "AnaSns.h"

#if 1 // =============== Defines ================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
static void ITask();
static void OnCmd(Shell_t *PShell);

static TmrKL_t TmrOneSecond {TIME_MS2I(999), evtIdEverySecond, tktPeriodic};

//LedBlinker_t Led[LED_CNT] = {
//        {LED1_PIN, omPushPull},
//        {LED2_PIN, omPushPull},
//        {LED3_PIN, omPushPull},
//        {LED4_PIN, omPushPull},
//        {LED5_PIN, omPushPull},
//        {LED6_PIN, omPushPull},
//        {LED7_PIN, omPushPull},
//        {LED8_PIN, omPushPull},
//};

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

    // LEDs
//    for(LedBlinker_t &FLed : Led) {
//        FLed.Init();
//        FLed.StartOrRestart(lsqBlink);
//        chThdSleepMilliseconds(72);
//    }
//    Led[0].On();

//    SimpleSensors::Init(); // Buttons

//    TmrOneSecond.StartOrRestart();

    // ==== Main cycle ====
    ITask();
}

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
//        Printf("Msg.ID %u\r", Msg.ID);
        switch(Msg.ID) {
            case evtIdButtons:
//                Printf("Btn %u; %u\r", Msg.BtnEvtInfo.Type, Msg.BtnEvtInfo.BtnID);
                break;

            case evtIdShellCmd:
//                Led[0].StartOrContinue(lsqCmd);
                OnCmd((Shell_t*)Msg.Ptr);
                ((Shell_t*)Msg.Ptr)->SignalCmdProcessed();
                break;

            case evtIdEverySecond:
//                Printf("Second\r");
                break;

            default: break;
        } // switch
    } // while true
}

#if 1 // ======================= Command processing ============================
void OnCmd(Shell_t *PShell) {
    Cmd_t *PCmd = &PShell->Cmd;
//    Printf("%S  ", PCmd->Name);

    // Handle command
    if(PCmd->NameIs("Ping")) PShell->Ack(retvOk);
    else if(PCmd->NameIs("Version")) PShell->Print("%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));

//    else if(PCmd->NameIs("SetRaw")) {
//        uint16_t w16 = 0;
//        if(PCmd->GetNext<uint16_t>(&w16) != retvOk) { PShell->Ack(retvCmdError); return; }
//        Load::SetRaw(w16);
//        PShell->Ack(retvOk);
//    }

//    else if(PCmd->NameIs("Seg")) {
//        uint8_t v1, v2, v3, v4;
//        if(PCmd->GetNext<uint8_t>(&v1) != retvOk) { PShell->Ack(retvCmdError); return; }
//        if(PCmd->GetNext<uint8_t>(&v2) != retvOk) { PShell->Ack(retvCmdError); return; }
//        if(PCmd->GetNext<uint8_t>(&v3) != retvOk) { PShell->Ack(retvCmdError); return; }
//        if(PCmd->GetNext<uint8_t>(&v4) != retvOk) { PShell->Ack(retvCmdError); return; }
//        SegmentShow(v1, v2, v3, v4);
//        PShell->Ack(retvOk);
//    }

    else {
        Printf("%S\r\n", PCmd->Name);
        PShell->Ack(retvCmdUnknown);
    }
}
#endif