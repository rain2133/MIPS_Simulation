#include "decode.h"
#include "reg.h"
#include <stdlib.h>

struct INSTR_FUCSIM *InstDecode(unsigned int insn){
    int OPCODE 	= (insn >> 26) & 0x3f;
    int RS 	    = (insn >> 21) & 0x1f;
    int RT		= (insn >> 16) & 0x1f;
    int RD		= (insn >> 11) & 0x1f;
    int IMM		= (int)((short)(insn & 0xffff)); 		// 这个表达式实现了符号扩展
    int OFFSET	= (int)((short)(insn & 0xffff)); 
    unsigned int UIMM	= (unsigned int)(insn & 0xffff); 	// 这个表达式实现了0扩展
    int SHAMT   = (insn >>6) & 0x1f;
    int FUNC	= insn & 0x3f;
    int LABEL	= insn & 0x3ffffff;
    struct INSTR_FUCSIM *inst = (struct INSTR_FUCSIM *)malloc(sizeof(struct INSTR_FUCSIM));
    *inst = (struct INSTR_FUCSIM){OPCODE,RS,RT,RD,IMM,OFFSET,SHAMT,FUNC,LABEL,UIMM};
    return inst;
}

struct INSTR_TIMSIM *FUCDecode2TIM(struct INSTR_FUCSIM *inst){
    //将功能指令序列转化为时序指令序列(减少共享内存空间开销)
    struct INSTR_TIMSIM *inst_timsim = (struct INSTR_TIMSIM *)malloc(sizeof(struct INSTR_TIMSIM));
    inst_timsim->OPCODE = inst->opcode;
    inst_timsim->RS = inst->RS;
    inst_timsim->RT = inst->RT;
    inst_timsim->RD = inst->RD;
    if(inst_timsim->OPCODE == FPU){
        //把浮点寄存器和整型寄存器区分开
        inst_timsim->RS += ireg_size;
        inst_timsim->RT += ireg_size;
        inst_timsim->RD += ireg_size;
    }
    inst_timsim->IMM = inst->IMM;
    inst_timsim->ADDR = (int)*(r + inst->RS) + inst->IMM;//只有在lw,sw,lwc1,swc1指令中才需要计算地址
    // printf("rs = %d, rt = %d, rd = %d, imm = %d, addr = %d\n",inst->RS,inst->RT,inst->RD,inst->IMM,inst_timsim->ADDR);
    return inst_timsim;
}

