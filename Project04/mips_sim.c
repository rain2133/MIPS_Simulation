#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include "mips_sim.h"
#include "common.h"
#include "types.h"
#include "mainmem.h"
#include "reg.h"
#include "pipeline.h"
#include "function_sim.h"
#include "decode.h"
#include "Dcache.h"

long pc;

long sigle_routing, mult_cycle, pipeline_cycle;

sem_t instnRAW;

struct INSTR_TIMSIM INSTR_TIMSIM_NOP;

#define TEXT_SZ 64
struct shared_mem{
    char finish;
    int inst_begin,inst_end;
    int mips_pipeline_cycle;
    // struct INSTR_FUCSIM insts[TEXT_SZ];//记录写入和读取的文本
    struct MIPS_pipeline pipeline;//inst_begin进入stageF后该流水线各部分和各部件的状态
    struct INSTR_TIMSIM insts[TEXT_SZ];
};



struct CacheBlk { 
    int tag; 
    int status; //0 表示无效 invalid 1 表示有效valid 2 表示 脏 dirty
    int trdy;   //最近一次使用时间
} dCache[DC_NUM_SETS][DC_SET_SIZE];

struct WriteBuffer{ 
    int tag;
    int index;
    int trdy; 
} dcWrBuff[DC_WR_BUFF_SIZE]; 
int bufsize = 0;
//描述cache行为的变量
int  WrMergeFlag, WrFlushFlag, HitFlag;
/*
WrBack(int tag, int index,int cycle)
执行写回策略
写合并功能
写命中，需要将脏块写入buffer
读未命中，需要将替换脏块写入buffer
写未命中，需要将新的块写入buffer
WrBack执行流程可总结为
先执行写合并
然后检查buffer是不是满了，满了就增加latency然后清空buffer
如果没有写合并则将块放入buffer
*/
int WrBack(int tag, int index , int cycle, int opcode){
    WrMergeFlag = false,WrFlushFlag = false;
    int WB_latency = 0;
    
    if(opcode == LOAD){
        bufsize = 0;
        WB_latency += FlushWrBfLtcy;
        WrFlushFlag = true;    
    }
    //将写回缓存刷新,避免从主存拿到旧数据
    
    for(int i = 0; i < bufsize; i++)
        if(dcWrBuff[i].index == index && dcWrBuff[i].tag == tag){
            WrMergeFlag = true;
            WB_latency += WrMergLtcy;
            break;
        }
    
    if(bufsize >= DC_WR_BUFF_SIZE){//清空写回缓存批量写回
        WrFlushFlag = true;
        WB_latency += FlushWrBfLtcy;
        bufsize = 0;
    }
    //未执行写合并
    if(WrMergeFlag == false){
        WB_latency += AddBlk2WrBfLtcy;
        dcWrBuff[bufsize].tag   = tag;
        dcWrBuff[bufsize].index = index;
        dcWrBuff[bufsize].trdy  = cycle + WB_latency;
        bufsize++;
    }
    return WB_latency;
}

/*
accessDCache( int opcode, int addr, int cycle)
在时序模拟中模拟cache的行为
返回值为当前LW,SW指令的延迟时间
该时间将会影响MIPS组件 DM_的属性 从而实现数据cache未命中流水线暂停等功能
*/
int accessdCache(int opcode, int addr, int cycle){
    int blkOffsetBits, indexMask, tagMask, index, tag, blk;
    int hit = false;
    int lruTime = cycle, slot, lruSlot;
    int latency = 0;
    //划分字段
    blkOffsetBits = log2(DC_BLOCK_SIZE); //块偏移
    indexMask = (unsigned)(DC_NUM_SETS - 1); //0xF 组索引
    tagMask = ~indexMask; //0xFFFFFFF0 标记

    blk = ((unsigned)addr) >> blkOffsetBits; 
    index = (int)(blk & indexMask); 
    tag = (int)(blk & tagMask);

    for(int i = 0; i < DC_SET_SIZE; i++) { 
        if((dCache[index][i].tag == tag) && (dCache[index][i].status != DC_INVALID) ) { 
            slot = i, hit = true, HitFlag = true;
            break;
        } 
        else if (dCache[index][i].trdy < lruTime){ // Find a possible replacement line 
            lruTime = dCache[index][i].trdy; 
            lruSlot = i; 
        } 
    }
    //replace cacheblk whose index is lruSlot
    
    struct CacheBlk *dcBlock;
    if(hit){
        dcBlock = &dCache[index][slot];
        int trdy;
        trdy = HitdCLtcy;
        //  读命中
        dcBlock->trdy = cycle;
        //  写命中 
        if(opcode == STORE){
            dcBlock->status = 2;
            trdy += WrBack(dcBlock->tag, index, cycle, opcode);
        }
        latency = trdy;
    }
    else{
        dcBlock = &dCache[index][lruSlot];
        int trdy;
        if(opcode == LOAD){
            //读不命中
            trdy = MemRdLatency; 
            if(dcBlock -> status == 2)   //  如果被换出的块为脏块 
                trdy += WrBack(dcBlock->tag, index, cycle, opcode); 

            dcBlock -> tag = tag; 
            dcBlock -> trdy = cycle + trdy; 
            dcBlock -> status = 1;
            latency = trdy;
        }
        if(opcode == STORE){
            //写不命中
            trdy = MemRdLatency; 
            if (dcBlock->status == 2) /* Must remote write-back old data */ 
                trdy += WrBack(dcBlock->tag, index, cycle, opcode);
            /* Read in cache line we wish to update */ 
            dcBlock->tag = tag; 
            dcBlock->trdy = cycle + trdy;
            dcBlock->status = 2;
            //将新的脏块写入写回缓存
            trdy += WrBack(dcBlock->tag, index, cycle, opcode);
            latency = trdy;
        }
    }
    return latency;
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
    int dc_delay = 0;
    //每个周期各个段的时序模拟在时钟上沿发生，判断流水线是否满足暂停
    again:
    *M_P_cycle += 1;
    // if(*M_P_cycle >= 100)exit(-1);

    printf("cycle %7d before clock rising edge | IF:%s\t| ID:%s\t | EX:%s\t | MEM:%s\t | WB:%s\t |\n",
           *M_P_cycle, stage_type(StageF), stage_type(StageD),stage_type(StageE), stage_type(StageM), stage_type(StageW));

    //StageW
    StageW.stall = false;
    if(!false){
        StageW = StageM;
        StageW.valid = true; 
        //WB always not being stallE
    }
    
    //模拟cache行为
    if((StageM.inst.OPCODE == LOAD || StageM.inst.OPCODE == STORE) && StageM.valid && !(P->DM_.busy)){//这个LW,SW指令第一次计算delay
        printf("Note:dCache access\n");
        WrMergeFlag = false, WrFlushFlag = false, HitFlag = false;
        dc_delay = accessdCache(StageM.inst.OPCODE, StageM.inst.ADDR, *M_P_cycle);
        if(WrMergeFlag)                              printf("Note:Write Merge happend\n");
        if(WrFlushFlag && StageM.inst.OPCODE == LOAD)printf("Note:Write Cache Flush due to Load inst\n");
        else if(WrFlushFlag)                         printf("Note:Write Cache Flush , it`s full\n");
        if(HitFlag && StageM.inst.OPCODE == LOAD)    printf("Note:Load  Hit\n");
        else if (StageM.inst.OPCODE == LOAD)         printf("Note:Load  Miss\n");
        if(HitFlag && StageM.inst.OPCODE == STORE)   printf("Note:Store  Hit\n");
        else if (StageM.inst.OPCODE == STORE)        printf("Note:Store  Miss\n");
    }
    //每条指令流入MEM段首先刷新DM->处理若干cycle->解除DM的busy状态
    //clock before rising edge
    if(dc_delay > 0)P->DM_.busy = true;
    
    //clock after2 rsing edge
    if(P->DM_.busy)dc_delay--;
    if(dc_delay <= 0)P->DM_.busy = false;
    if(P->DM_.busy)printf("dCache busy, delay = %d\n",dc_delay);
    //StageM

    if(!P->DM_.busy)StageM.stall = false;
    if(!P->DM_.busy && StageW.valid){
        StageM = StageE;
        StageM.valid = true;
    }
    else{
        //等引入cache延迟才会进入这个分支
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
      ||(StageF.inst.OPCODE == JR    && StageF.valid)
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
    printf("cycle %7d after2 clock rising edge | IF:%s\t| ID:%s\t | EX:%s\t | MEM:%s\t | WB:%s\t |\n",
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
            struct INSTR_FUCSIM *inst = InstDecode(insn);
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
