#pragma once
#include "Arduino.h"
#define JD_FORMAT 1
#define TJPGD_WORKSPACE_SIZE 3500
enum JRESULT { JDR_OK, JDR_INP };
struct JDEC { void* device; uint8_t swap; uint16_t width, height; };
struct JRECT { uint16_t left, right, top, bottom; };
inline JRESULT jd_prepare(JDEC*, size_t (*)(JDEC*, uint8_t*, size_t), void*, size_t, void*) { return JDR_INP; }
inline JRESULT jd_decomp(JDEC*, int (*)(JDEC*, void*, JRECT*), uint8_t) { return JDR_INP; }
