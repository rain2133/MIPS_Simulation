// decode.h
#ifndef DECODE_H
#define DECODE_H

#ifndef FOO
#include "inst.h"
#endif

struct INSTR_FUCSIM *InstDecode(unsigned int);
struct INSTR_TIMSIM *FUCDecode2TIM(struct INSTR_FUCSIM *);

#endif // DECODE_H

