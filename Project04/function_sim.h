// function.h
#ifndef FOO
#include "inst.h"
#endif
#ifndef FUNCTION_H
#define FUNCTION_H

//R-type 
long INSN_SLL (long, struct INSTR_FUCSIM *);
long INSN_ADD (long, struct INSTR_FUCSIM *);
//I-type
long INSN_ADDI(long, struct INSTR_FUCSIM *);
long INSN_BEQ (long, struct INSTR_FUCSIM *);
long INSN_BNE (long, struct INSTR_FUCSIM *);
long INSN_SLTI(long, struct INSTR_FUCSIM *);
long INSN_LW  (long, struct INSTR_FUCSIM *);
long INSN_SW  (long, struct INSTR_FUCSIM *);
long INSN_LWC1(long, struct INSTR_FUCSIM *);
long INSN_SWC1(long, struct INSTR_FUCSIM *);
long INSN_MOVE(long, struct INSTR_FUCSIM *);
//J-type
long INSN_J   (long, struct INSTR_FUCSIM *);
long INSN_JR  (long, struct INSTR_FUCSIM *);
//F-type
long INSN_ADDS(long, struct INSTR_FUCSIM *);
long INSN_MULS(long, struct INSTR_FUCSIM *);
long INSN_NOP (long, struct INSTR_FUCSIM *);

#endif // FUNCTION_H

