/*
 * 7segment.h
 *
 *  Created on: Oct 2, 2019
 *      Author: laurelindo
 */

#pragma once

#include <inttypes.h>

#define SEGMENT_COMMA    10

void SegmentInit();

void SegmentShow(uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4, uint8_t PointMsk=0);
void SegmentPutUint(uint32_t n, uint32_t base, uint8_t PointMsk=0);

void SegmentShowT(uint32_t N);
void SegmentShowGood();
void SegmentShowBad();

void SegmentShowPoints(uint8_t PointMsk);
