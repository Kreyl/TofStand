/*
 * EvtMsgIDs.h
 *
 *  Created on: 21 ���. 2017 �.
 *      Author: Kreyl
 */

#pragma once

enum EvtMsgId_t {
    evtIdNone = 0, // Always

    // Pretending to eternity
    evtIdShellCmd,
    evtIdEverySecond,

    // USB
    evtIdUsbConnect,
    evtIdUsbDisconnect,
    evtIdUsbReady,

    evtIdButtons,

    evtIdChamberOpen,
    evtIdChamberClose,
    evtIdEndstopTop,
    evtIdEndstopBottom,
    evtIdEndstopTouch,
    evtIdBusyFlag,

    evtIdHeightMeasure,
    evtIdDelayEnd,
};
