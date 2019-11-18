/*
 * LoadCtrl.h
 *
 *  Created on: Oct 3, 2019
 *      Author: laurelindo
 */

#pragma once

#include <inttypes.h>

namespace Load {

void Init();
void SetRaw(uint16_t W16);

void ConnectCapacitors(uint8_t CapCnt);

void SetResLoad(uint8_t LoadValue);

};
