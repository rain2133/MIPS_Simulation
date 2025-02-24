// pipeline.h
#ifndef PIPELINE_H
#define PIPELINE_H

#ifndef FOO
#include "inst.h"
#endif
struct STAGE{
    char valid,stall;
    //valid定义是该段没有暂停被继承到下一段
    //如果是true表明该段或者后续段处于stall状态下一个时钟上沿将stall状态取消
    struct INSTR_TIMSIM inst;
};

struct COMPONENT{
    char busy, cyctim, busytim;
};

struct MIPS_pipeline{
    struct COMPONENT 
        ALU_, FPU_, DM_, IM_;
    struct STAGE stages[5];
};

//stage type string
char alu[]      = "ALU   ";
char branch[]   = "BRANCH";
char jmp[]      = "JMP   ";
char move[]     = "MOVE  ";
char load[]     = "LOAD  ";
char store[]    = "STORE ";
char jr[]       = "JR    ";
char fpu[]      = "FPU   ";
char error[]    = "ERR   ";
char stall[]    = "STALL ";
char nop[]      = "------";



// 返回指令类型的函数，用于打印
char *stage_type(struct STAGE stg) {
    if (stg.stall)
        return stall;
    switch (stg.inst.OPCODE) {//ALU, FPU, BRANCH, JMP, JR, LOAD, STORE, MOVE
        case ALU:
            return alu;
        case BRANCH:
            return branch;
        case JMP:
            return jmp;
        case LOAD:
            return load;
        case STORE:
            return store;
        case FPU:
            return fpu;
        case JR:
            return jr;
        case MOVE:
            return move;
        case NOP:
            return nop;
        default:
            return error;
    }
}

#endif //PIPELINE_H