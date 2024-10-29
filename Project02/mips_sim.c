#include "mips_sim.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include "common.h"

typedef unsigned long    size_t;
typedef unsigned char    uint8;
typedef unsigned short   uint16;
typedef unsigned int     uint32;
typedef unsigned long    uint64;

char *mem; 		         // 主存起始地址指针
int   mem_size=0x080000;	 // 主存的大小

#define PCINIT  0x020000
#define SPINIT  0x060000

#define MEMSIZE 0x080000
#define REGSIZE 32
#define FEGSIZE 16

int   ireg_size = 32;       // 通用寄存器文件的大小
int  *r;                  // 通用寄存器文件起始地址指针

int   freg_size = 16;       // 浮点寄存器文件的大小
float  *f; 

long pc;

bool dbgMode;
long sigle_routing, mult_cycle, pipeline_cycle;

sem_t instnRAW;
//指令相关
struct INSTR_FUCSIM{
    int OPCODE,RS,RT,RD,IMM,OFFSET,SHAMT,FUNC,LABEL;
    //         FS FT FD
    uint32 UIMM;
    char opcode;//for timing simulation insts seq
};

enum COM_CODE{
    NOP, ALU, FPU, BRANCH, JMP, JR, LOAD, STORE, MOVE, COM_CODE_NUM
};

struct INSTR_TIMSIM{
    char OPCODE,RS,RT,RD;
    int IMM;
}INSTR_TIMSIM_NOP;

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


#define TEXT_SZ 64
struct shared_mem{
    char finish;
    int inst_begin,inst_end;
    int mips_pipeline_cycle;
    // struct INSTR_FUCSIM insts[TEXT_SZ];//记录写入和读取的文本
    struct MIPS_pipeline pipeline;//inst_begin进入stageF后该流水线各部分和各部件的状态
    struct INSTR_TIMSIM insts[TEXT_SZ];
};

struct INSTR_FUCSIM *InstDecode(unsigned int insn,bool debug){
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
    return inst_timsim;
}

/*
读访问，即读出存放在某个主存单元中的数据
写访问，即将数据写入主存中的特定单元
*/

void write_mem(void* value, uint32 address, size_t size) {
    if (address < 0 || address + size > MEMSIZE) {
        printf("Error: Address %d out of bounds\n",address);
        return;
    }
    memcpy(mem + address, value, size);
}


void* read_mem(uint32 address, size_t size){
    if (address < 0 || address + size > MEMSIZE) {
        printf("Error: Address %d out of bounds\n",address);
        return 0;
    }
    void *value = malloc(size);
    memcpy(value, mem + address, size);
    return value;
}


bool test_for_memrw(){
    unsigned char uh1, uh2;
    unsigned long addr=256;

    uh1 = 0x0f5;
    write_mem((void *) &uh1,addr, sizeof(unsigned char));
    uh2 = *(unsigned char *)read_mem(addr,sizeof(unsigned char));
    printf("uh1 = %u, uh2 = %u\n", uh1, uh2);

    float uh3, uh4;
    uh3 = 0.12;
    write_mem((void *) &uh3, addr, sizeof(float));
    uh4 = *(float *)read_mem(addr,sizeof(float));
    printf("uh3 = %f, uh4 = %f\n", uh3, uh4);
    fflush(stdout);
    return uh1 == uh2 && uh3 == uh4;
}


//R-type 

long INSN_SLL(long pc, struct INSTR_FUCSIM *inst){
    int *rd     = r + inst->RD;
    int rt      = (int)*(r + inst->RT);
    int shamt   = inst->SHAMT;
    *rd         = rt << shamt;
    return pc+4;
}

long INSN_ADD(long pc, struct INSTR_FUCSIM *inst){
    int *rd     = r + inst->RD;
    int rs      = (int)*(r + inst->RS);
    int rt      = (int)*(r + inst->RT);
    *rd         = rs + rt;
    return pc+4;
}


//I-type
long INSN_ADDI(long pc, struct INSTR_FUCSIM *inst){
    int *rd     = r + inst->RT;
    int rs      = (int)*(r + inst->RS);
    *rd         = rs + inst->IMM;			
    return pc+4;			
}

long INSN_BEQ(long pc, struct INSTR_FUCSIM *inst){
    int rd      = (int)*(r + inst->RT);
    int rs      = (int)*(r + inst->RS);
    int offset  = (int)(inst->OFFSET);
    if(rs == rd)
        return pc + 4 + offset * 4;
    return pc+4;
}

long INSN_BNE(long pc, struct INSTR_FUCSIM *inst){
    int rd      = (int)*(r + inst->RT);
    int rs      = (int)*(r + inst->RS);
    int offset  = (int)(inst->OFFSET);
    if(rs != rd)
        return pc + 4 + offset * 4;
    return pc+4;
}

long INSN_SLTI(long pc, struct INSTR_FUCSIM *inst){
    int *rd     = r + inst->RT;
    int rs      = (int)*(r + inst->RS);
    int imm     = (int)(inst->IMM);
    *rd = rs < imm ? 1:0;
    return pc+4;
}

long INSN_LW(long pc, struct INSTR_FUCSIM *inst){
    int *rd     = r + inst->RT;
    int rs      = (int)*(r + inst->RS);
    int imm     = (int)(inst->IMM);
    void *value = read_mem((uint32)(rs + imm), sizeof(int));
    *rd = *(int *)value;
    // printf("value = %d\n",*(int *)value);
    // printf("lw  :rt = %d , rd = %d, addr = %x\n",inst->RT,*rd,rs + imm);
    // printf("r2 = %d\n",r[2]);
    free(value);
    return pc+4;
}

long INSN_SW(long pc, struct INSTR_FUCSIM *inst){
    int *rd     = r + inst->RT;
    int rs      = (int)*(r + inst->RS);
    int imm     = (int)(inst->IMM);
    write_mem((void *)rd, (uint32)(rs + imm), sizeof(int));
    return pc+4;
}

long INSN_LWC1(long pc, struct INSTR_FUCSIM *inst){
    float *fd     = f + inst->RT;
    int rs      = (int)*(r + inst->RS);
    int imm     = (int)(inst->IMM);
    void *value = read_mem((uint32)(rs + imm), sizeof(float));
    *fd = *(float *)value;
    // printf("lwc1:rt = %d , fd = %f, addr = %x\n",inst->RT,*fd,rs + imm);
    free(value);
    return pc+4;
}

long INSN_SWC1(long pc, struct INSTR_FUCSIM *inst){
    float *ft   = f + inst->RT;
    int rs      = (int)*(r + inst->RS);
    int imm     = (int)(inst->IMM);
    write_mem((void *)ft, (uint32)(rs + imm), sizeof(float));
    return pc+4;
}

long INSN_MOVE(long pc, struct INSTR_FUCSIM *inst){
    int *rd     = r + inst->RT;
    int rs      = (int)*(r + inst->RS);
    *rd = rs;
    return pc+4;
}
//J-type

long INSN_J(long pc, struct INSTR_FUCSIM *inst){
    // long label = inst->LABEL;
    return inst->LABEL;
}

long INSN_JR(long pc, struct INSTR_FUCSIM *inst){
    int *rs = r + inst->RS;
    return *rs;
}


//F-type

long INSN_ADDS(long pc, struct INSTR_FUCSIM *inst){
    float *fd   = f + inst->RD;
    float fs    = (float)*(f + inst->RS);
    float ft    = (float)*(f + inst->RT);
    *fd = fs + ft;
    // printf("adds: fd = %f fs = %f ft = %f\n",*fd,fs,ft);
    return pc+4;
}

long INSN_MULS(long pc, struct INSTR_FUCSIM *inst){
    float *fd   = f + inst->RD;
    float fs    = (float)*(f + inst->RS);
    float ft    = (float)*(f + inst->RT);
    *fd = fs * ft;
    // printf("muls: fd = %f fs = %f ft = %f\n",*fd,fs,ft);
    return pc+4;
}

long INSN_NOP(long pc, struct INSTR_FUCSIM *inst){
    return pc+4;
}

/*
时序模拟
模拟到当前指令进入stageF,如果finish = 1那么需要将指令模拟到从stageW流出

*/
void timing_simulation(struct INSTR_TIMSIM *inst, struct MIPS_pipeline *P,int *M_P_cycle){
    struct STAGE StageF = P->stages[0],
                 StageD = P->stages[1],
                 StageE = P->stages[2],
                 StageM = P->stages[3],
                 StageW = P->stages[4];
    //每个周期各个段的时序模拟在时钟上沿发生，判断流水线是否满足暂停
    again:
    *M_P_cycle += 1;
    // if(*M_P_cycle >= 50)exit(-1);

    printf("cycle %7d\t | IF:%s\t| ID:%s\t | EX:%s\t | MEM:%s\t | WB:%s\t |\n",
           *M_P_cycle, stage_type(StageF), stage_type(StageD),stage_type(StageE), stage_type(StageM), stage_type(StageW));

    //StageW
    StageW.stall = false;
    if(!false){
        StageW = StageM;
        StageW.valid = true; 
        //WB always not being stallE
    }
        
    //StageM
    StageM.stall = false;
    if(!(P->DM_.busy)){
        StageM = StageE;
        StageM.valid = true;
    }
    else{
        //等引入cache未命中才会进入这个分支
        if(!StageW.stall){
            StageM.stall = true;
        }
        StageM.valid = false;
        //cache未命中处理完成指令流走（后续拓展）
    }

    //StageE
    StageE.stall = false;
    if(!(!StageW.valid || !StageM.valid || P->FPU_.busy || P->ALU_.busy || 
         (StageE.inst.OPCODE == LOAD && StageE.valid &&
         //LW指令需要走到MEM段才能让数据正确定向到ID段
         //如果LW指令流出此处且有冲突那么该条件将会在下一个cycle取消暂停
          (StageD.inst.RS == StageE.inst.RD ||  
           StageD.inst.RT == StageE.inst.RD)
         ) 
        ) ){
        StageE = StageD;
        StageE.valid = true;
    }
    else{
        //暂停流水线等待LOAD进入MEM段读出数据
        if(!StageW.stall && !StageM.stall){
            StageE.stall = true;
        }
        StageE.valid = false;
    }

    //StageD
    StageD.stall = false;
    if(!(!StageW.valid || !StageM.valid|| !StageE.valid //|| 
        //  (StageF.inst.OPCODE == BRANCH && StageF.valid &&
        //   (StageD.inst.RS == StageE.inst.RD || StageD.inst.RT == StageE.inst.RD))
        // || (StageF.inst.OPCODE == JR && StageF.valid &&
        //   (StageD.inst.RS == StageE.inst.RD))
        // || (StageF.inst.OPCODE == JMP && StageF.valid )
        )){
        StageD = StageF;
        StageD.valid = true;
    }
    else{
        if(!StageW.stall && !StageM.stall && !StageE.stall){
            StageD.stall = true;
        }
        StageD.valid = false;
    }

    //StageF
    //如果未继承段是分支指令那么暂停流水线等待IDEX段完成分支跳转
    StageF.stall = false;
    if(!(!StageW.valid || !StageM.valid|| !StageE.valid || !StageD.valid ||
        P->IM_.busy ||
        (StageF.inst.OPCODE == BRANCH && StageF.valid)
      ||(StageF.inst.OPCODE == JMP    && StageF.valid) 
      || (StageF.inst.OPCODE == JR    && StageF.valid)
        )){
        StageF.inst = *inst;
        StageF.valid  = true;
    }else{
        if(!StageW.stall && !StageM.stall && !StageE.stall && !StageD.stall){
            StageF.stall = true;
        }
        StageF.valid = false;
        goto again;
    }

    P->stages[0] = StageF,
    P->stages[1] = StageD,
    P->stages[2] = StageE,
    P->stages[3] = StageM,
    P->stages[4] = StageW;
    struct STAGE StageI={true, false,*inst};
    printf("Stage %s piped in\t\n",stage_type(StageI));
    printf("cycle %7d\t | IF:%s\t| ID:%s\t | EX:%s\t | MEM:%s\t | WB:%s\t |\n",
           *M_P_cycle, stage_type(StageF), stage_type(StageD),stage_type(StageE), stage_type(StageM), stage_type(StageW));
    printf("---------------------------------------------------------------------\n");
    fflush(stdout);
}

// shmget：申请共享内存
// shmat:建立用户进程空间到共享内存的映射
// shmdt:解除映射关系
// shmctl:回收共享内存空间

void function_simulation(struct INSTR_FUCSIM *inst){

    if (inst->OPCODE != OP_RTYPE && inst->OPCODE != OP_FTYPE){// 若不是R and F型指令
        // printf("not R or F tpye\n");
        switch (inst->OPCODE)
        {
            
            case OP_J:
            pc = INSN_J(pc,inst);   mult_cycle+=3;inst->opcode = JMP;   break;
            case OP_BEQ:
            pc = INSN_BEQ(pc,inst); mult_cycle+=3;inst->opcode = BRANCH;break;
            case OP_BNE:
            pc = INSN_BNE(pc,inst); mult_cycle+=3;inst->opcode = BRANCH;break;
            case OP_ADDI:
            // pc = INSN_ADDI(pc,inst);mult_cycle+=5;inst->opcode = ALU;printf("addi\n");   break;
            pc = INSN_ADDI(pc,inst);mult_cycle+=5;inst->opcode = ALU;   break;
            case OP_SLTI:
            pc = INSN_SLTI(pc,inst);mult_cycle+=5;inst->opcode = ALU;   break;
            case OP_LW:
            pc = INSN_LW(pc,inst);  mult_cycle+=5;inst->opcode = LOAD;  break;
            case OP_SW:
            // pc = INSN_SW(pc,inst);  mult_cycle+=5;inst->opcode = STORE;printf("sw\n");  break;
            pc = INSN_SW(pc,inst);  mult_cycle+=5;inst->opcode = STORE;  break;
            case OP_LWC1:
            pc = INSN_LWC1(pc,inst);mult_cycle+=5;inst->opcode = LOAD;  break;
            case OP_SWC1:
            pc = INSN_SWC1(pc,inst);mult_cycle+=5;inst->opcode = STORE; break;
            case OP_MOVE:
            // pc = INSN_MOVE(pc,inst);mult_cycle+=5;inst->opcode = MOVE;printf("move\n");  break;
            pc = INSN_MOVE(pc,inst);mult_cycle+=5;inst->opcode = MOVE;  break;
            default: printf("error: unimplemented instruction\n"); exit(-1);
        }

    }
    else if(inst->OPCODE == OP_RTYPE){
        // printf("R-type\n");
        switch (inst->FUNC){
            case OP_ADD:
            pc = INSN_ADD(pc, inst);mult_cycle+=5;inst->opcode = ALU;   break;
            case OP_JR:
            pc = INSN_JR(pc, inst); mult_cycle+=3;inst->opcode = JR;   break;//待验证opcode
            case OP_SLL:
            pc = INSN_SLL(pc, inst);mult_cycle+=5;inst->opcode = ALU;   break;
            default: printf("error: unimplemented instruction\n"); exit(-1);
        }
    }
    else{
        // printf("F-type\n");
        switch (inst->FUNC){
            case OP_ADDS:
            pc = INSN_ADDS(pc, inst);mult_cycle+=6;inst->opcode = FPU; break;
            case OP_MULS:
            pc = INSN_MULS(pc, inst);mult_cycle+=8;inst->opcode = FPU; break;
            default: printf("error: unimplemented instruction\n"); exit(-1);
        }
    }
    sigle_routing++;
    return;
}

void Execution(){
    pc = 0x1000;									// 将程序的入口地址设为0x1000

    Sem_init(&instnRAW, 1, 1);//初始化信号量在进程中通信

    key_t key = ftok("./input",32);
    int shmid;                // 共享内存标识符
    struct shared_mem *data;               // 共享内存的地址
    pid_t pid = fork();
    // 创建共享内存
    shmid = shmget(key, sizeof(struct shared_mem), 0644 | IPC_CREAT);

    // printf("shmid = %d\n",shmid);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    
    if (pid == 0) {
        // 子进程
        // 将共享内存连接到当前进程的地址空间
        data = (struct shared_mem *)shmat(shmid, (void *)0, 0);
        // printf("data = %lx",data);
        if (data == (struct shared_mem *)(-1)) {
            perror("shmat");
            exit(1);
        }
        while (1) {
            // 读取共享内存中的数据并处理
            Sem_wait(&instnRAW);
            //计算多周期数量
            if(data->finish == 1)
                break;
            if(data->inst_begin < data->inst_end){
                // printf("begin = %d end = %d\n", data->inst_begin, data->inst_end);
                timing_simulation((struct INSTR_TIMSIM *)&data->insts[data->inst_begin % TEXT_SZ],
                              (struct MIPS_pipeline *) &data->pipeline,(int *)&data->mips_pipeline_cycle);
                data->inst_begin++;
            }
                
            Sem_post(&instnRAW);
        }
        while(data->inst_begin < data->inst_end ){
            timing_simulation((struct INSTR_TIMSIM *)&data->insts[data->inst_begin % TEXT_SZ],
                              (struct MIPS_pipeline *) &data->pipeline,(int *)&data->mips_pipeline_cycle);
            data->inst_begin++;
        }
        while(memcmp((struct INSTR_TIMSIM *)&data->pipeline.stages[4].inst,
                     (struct INSTR_TIMSIM *)&INSTR_TIMSIM_NOP,
                     sizeof(struct INSTR_TIMSIM)) != 0){
            timing_simulation((struct INSTR_TIMSIM *)&INSTR_TIMSIM_NOP,
                              (struct MIPS_pipeline *) &data->pipeline,(int *)&data->mips_pipeline_cycle);
        }
        Sem_post(&instnRAW);
        shmdt(data);
        exit(0);
    }
    else{
        // 父进程
        
        // 将共享内存连接到当前进程的地址空间
        data = (struct shared_mem *)shmat(shmid, (void *)0, 0);
        if (data == (struct shared_mem *)(-1)) {
            perror("shmat");
            exit(1);
        }
        // printf("data = %lx",data);
        
        
        //init data pipeline
        Sem_wait(&instnRAW);
        memset(data, 0, sizeof(struct shared_mem));
        for(int i = 0;i < 5;i++)
            data->pipeline.stages[i].valid = true;
        Sem_post(&instnRAW);

        int i = 0;
        for (;pc;){    

            // printf("pc = %lx\n",pc);
            uint32 insn = *(uint32 *)read_mem(pc,sizeof(int)); 	// 读第一条指令
            //把机器代码放入int数组中
            struct INSTR_FUCSIM *inst = InstDecode(insn,1);
            // 指令译码
            
            //功能模拟
            function_simulation(inst);

            struct INSTR_TIMSIM *inst_timsim = FUCDecode2TIM(inst);

            Sem_wait(&instnRAW);
            while((data->inst_begin % TEXT_SZ) == (data->inst_end % TEXT_SZ) && data->inst_begin < data->inst_end){
                Sem_post(&instnRAW);
                Sem_wait(&instnRAW);
            }
            memcpy((void *)&data->insts[data->inst_end % TEXT_SZ],inst_timsim,sizeof(struct INSTR_TIMSIM));
            // printf("op = %d\n",data->insts[data->inst_end].OPCODE);
            data->inst_end++;
            if(pc == 0)data->finish = 1;
            Sem_post(&instnRAW);

            free(inst);
            // printf("!\n");
        }
        
        if (waitpid(pid, NULL, 0) < 0)
        {
            perror("waitpid");
            exit(1);
        }

        pipeline_cycle = data->mips_pipeline_cycle;
        // 父进程结束前，断开共享内存连接
        if (shmdt(data) == -1) {
            perror("shmdt");
            exit(1);
        }

        // 删除共享内存
        if (shmctl(shmid, IPC_RMID, NULL) == -1) {
            perror("shmctl");
            exit(1);
        }
        

    }

}


void init(){
    mem = (char *)malloc(sizeof(char) * mem_size);
    if(mem==NULL){
        printf("error: main memory allocation\n");
        exit(-1);
    }
    r = (int *)malloc(sizeof(int) * ireg_size);
    if(r == NULL){
        printf("error: int. Register file allocation\n");
        exit(-1);
    }
    f = (float *)malloc(sizeof(float) * freg_size);
    if (f == NULL){
        printf("error: float. Register file allocation\n");
        exit(-1);
    }
    //初始化指针
    /*
        $ra = 0 $sp = 0x8000 $gp = 0x0000
    */
    r[31] = 0;
    r[29] = 0x8000;
    r[28] = 0x0000;

}

void test(){
    if(test_for_memrw()){
        printf("accept mem read, mem write\n");
    }
    else{
        printf("warrning: mem read or mem write uncorrect\n");
    }
    fflush(stdout);
}
void readinst_data(){
    freopen("./data","r",stdin);
    pc = r[28];
    int *data = (int *)malloc(sizeof(int));
    while(scanf("%x",data) != EOF){
        // printf("%x\n",*data);
        write_mem((void *)data, pc, sizeof(int));
        // void *value = read_mem(pc, sizeof(float));
        // printf("%f\n",*(float *)value);
        pc+=4;
    }
    fclose(stdin);

    freopen("./input","r",stdin);
    pc = 0x1000;
    while(scanf("%x",data) != EOF){
        // printf("%x\n",*data);
        write_mem((void *)data, pc, sizeof(int));
        pc+=4;
    }
    fclose(stdin);

    int fp = r[29] - 304 + 36;
    float *fainit = (float *)malloc(sizeof(float)); 
    for(int i = 0;i < 64;i++){
        *fainit = (float)i;
        int add = (i << 2) + fp;
        // printf("add = %x , val =%f\n",add,*fainit);
        write_mem((void *)fainit, add, sizeof(float));
    }
    free(data);
    fflush(stdout);
}

int main(int argc,char *argv[]){
    init();
    // test();
    readinst_data();
    Execution();

    free(mem);
    free(r);
    free(f);

    printf("Single-cycle MIPS : %ld\n Multi-cycle MIPS : %ld\nPipline-cycle : %ld\n",sigle_routing,mult_cycle,pipeline_cycle);
    return 0;
}
