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
#define FEGSIZE 32

int   ireg_size = 32;       // 通用寄存器文件的大小
int  *r;                  // 通用寄存器文件起始地址指针

int   freg_size = 32;       // 浮点寄存器文件的大小
float  *f; 

long pc;

bool dbgMode;
long sigle_routing,mult_routing;

sem_t instnRAW;
//指令相关
struct INSTR_FUCSIM{
    int OPCODE,RS,RT,RD,IMM,OFFSET,SHAMT,FUNC,LABEL;
    //         FS     FT FD
    uint32 UIMM;
};

#define TEXT_SZ 64
struct shared_mem{
    int finish;
    int inst_begin,inst_end,mult_routing;
    struct INSTR_FUCSIM insts[TEXT_SZ];//记录写入和读取的文本
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
    // if(debug){
        // printf("%d %d %d %d %d %d %d %d %d %d\n",OPCODE,RS,RT,RD,IMM,OFFSET,SHAMT,FUNC,LABEL,UIMM);
    // }
    return inst;
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
    printf("lwc1:rt = %d , fd = %f, addr = %x\n",inst->RT,*fd,rs + imm);
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
    printf("adds: fd = %f fs = %f ft = %f\n",*fd,fs,ft);
    return pc+4;
}

long INSN_MULS(long pc, struct INSTR_FUCSIM *inst){
    float *fd   = f + inst->RD;
    float fs    = (float)*(f + inst->RS);
    float ft    = (float)*(f + inst->RT);
    *fd = fs * ft;
    printf("muls: fd = %f fs = %f ft = %f\n",*fd,fs,ft);
    return pc+4;
}

long INSN_NOP(long pc, struct INSTR_FUCSIM *inst){
    return pc+4;
}

/*
对于不同的指令类型，它们在多周期处理器中的执行周期数是不同的。常见指令类型及其在多周期MIPS处理器中的周期数界定如下可能有所变动：
R类型指令（如add, sub）5个周期
I类型指令（如lw, sw）5个周期
J类型，分支指令（如j, jal，bne，beq）3个周期
F类型，adds 6个周期,muls 8个周期
*/
void mult_compute(struct shared_mem *data){
    //根据单周期MIPS指令序列计算多周期MIPS的周期数
    //就是根据指令类型累加周期数就行了

    // printf("inst_begin = %d\n",data->inst_begin);
    // printf("inst_end = %d\n",data->inst_end);
    for (data->inst_begin;data->inst_begin < data->inst_end;data->inst_begin++){
        struct INSTR_FUCSIM *inst = &data->insts[data->inst_begin % TEXT_SZ];
        /**/
        // printf("op = %d\n",inst->OPCODE);
        if (inst->OPCODE != OP_RTYPE && inst->OPCODE != OP_FTYPE){// 若不是R and F型指令
            switch (inst->OPCODE)
            {
                case OP_J:
                data->mult_routing+=3;break;
                case OP_BEQ:
                data->mult_routing+=3;break;
                case OP_BNE:
                data->mult_routing+=3;break;
                case OP_ADDI:
                data->mult_routing+=4;break;
                case OP_SLTI:
                data->mult_routing+=4;break;
                default: data->mult_routing+=5;break;
            }

        }
        else if(inst->OPCODE == OP_RTYPE){
            // R-type --- 5 cycle
            data->mult_routing+=5;
        }
        else{
            switch (inst->FUNC)
            {
                case OP_ADDS:
                data->mult_routing+=6;break;
                case OP_MULS:
                data->mult_routing+=8;break;
                default: data->mult_routing+=5;break;
            }
        }
    }    
}

// shmget：申请共享内存
// shmat:建立用户进程空间到共享内存的映射
// shmdt:解除映射关系
// shmctl:回收共享内存空间

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
            mult_compute(data);
            if(data->finish == 1)
                break;
            
            Sem_post(&instnRAW);
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
        memset(data, 0, sizeof(struct shared_mem));
        
        int i = 0;
        for (;pc;){    

            // printf("pc = %lx\n",pc);
            uint32 insn = *(uint32 *)read_mem(pc,sizeof(int)); 	// 读第一条指令
            //把机器代码放入int数组中
            struct INSTR_FUCSIM *inst = InstDecode(insn,1);
            Sem_wait(&instnRAW);
            while((data->inst_begin % TEXT_SZ) == (data->inst_end % TEXT_SZ) && data->inst_begin < data->inst_end){
                Sem_post(&instnRAW);
                Sem_wait(&instnRAW);
            }
            memcpy((void *)&data->insts[data->inst_end % TEXT_SZ],inst,sizeof(struct INSTR_FUCSIM));
            // printf("op = %d\n",data->insts[data->inst_end].OPCODE)
            data->inst_end++;
            mult_routing = data->mult_routing;
            Sem_post(&instnRAW);
            // printf("r2 = %d\n",r[2]);
            // printf("fp = %d\n",r[30]);
            // printf("sp = %d\n",r[29]);
            // 指令译码
            /**/
            if (inst->OPCODE != OP_RTYPE && inst->OPCODE != OP_FTYPE){// 若不是R and F型指令
                switch (inst->OPCODE)
                {
                    case OP_J:
                    pc = INSN_J(pc,inst);break;
                    case OP_BEQ:
                    pc = INSN_BEQ(pc,inst);break;
                    case OP_BNE:
                    pc = INSN_BNE(pc,inst);break;
                    case OP_ADDI:
                    pc = INSN_ADDI(pc,inst);break;
                    case OP_SLTI:
                    pc = INSN_SLTI(pc,inst);break;
                    case OP_LW:
                    pc = INSN_LW(pc,inst);break;
                    case OP_SW:
                    pc = INSN_SW(pc,inst);break;
                    case OP_LWC1:
                    pc = INSN_LWC1(pc,inst);break;
                    case OP_SWC1:
                    pc = INSN_SWC1(pc,inst);break;
                    case OP_MOVE:
                    pc = INSN_MOVE(pc,inst);break;
                    default: printf("error: unimplemented instruction\n"); exit(-1);
                }

            }
            else if(inst->OPCODE == OP_RTYPE){
                // printf("R-type\n");
                switch (inst->FUNC){
                    case OP_ADD:
                    pc = INSN_ADD(pc, inst); break;
                    case OP_JR:
                    pc = INSN_JR(pc, inst); break;
                    case OP_SLL:
                    pc = INSN_SLL(pc, inst); break;
                    default: printf("error: unimplemented instruction\n"); exit(-1);
                }
            }
            else{
                // printf("F-type\n");
                switch (inst->FUNC)
                {
                    case OP_ADDS:
                    pc = INSN_ADDS(pc, inst); break;
                    case OP_MULS:
                    pc = INSN_MULS(pc, inst); break;
                    default: printf("error: unimplemented instruction\n"); exit(-1);
                }
            }
            // if(pc == 0x1050){
            //     void *value = read_mem((uint32)(r[2] + 36), sizeof(float));
            //     printf("%f\n",*(float *)value);
            //     printf("%f\n",f[0]);
            //     free(value);
            // }
            
            free(inst);
            sigle_routing++;
            // printf("!\n");
        }
        Sem_wait(&instnRAW);
        data->finish = 1;
        // printf("cycle %d opcode %d\n",data->mult_routing,data->insts[(data->inst_end - 1) % TEXT_SZ].OPCODE);
        Sem_post(&instnRAW);

        if (waitpid(pid, NULL, 0) < 0)
        {
            perror("waitpid");
            exit(1);
        }

        //记录多周期mips数;
        mult_routing = data->mult_routing;

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
    test();
    readinst_data();
    Execution();

    free(mem);
    free(r);
    free(f);

    printf("Single-cycle MIPS : %ld\n Multi-cycle MIPS : %ld\n",sigle_routing,mult_routing);
    return 0;
}
