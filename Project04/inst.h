#define FOO 0
enum _CODE{
    NOP, ALU, FPU, BRANCH, JMP, JR, LOAD, STORE, MOVE, COM_CODE_NUM
};
//指令相关
struct INSTR_FUCSIM{
    int OPCODE,RS,RT,RD,IMM,OFFSET,SHAMT,FUNC,LABEL;
    //         FS FT FD
    unsigned int UIMM;
    char opcode;//for timing simulation insts seq
};


struct INSTR_TIMSIM{
    char OPCODE,RS,RT,RD;
    int IMM,ADDR;
};
