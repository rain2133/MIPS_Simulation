#include<cstdio>

#define MEMSIZE 1024
#define REGSIZE 16

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;

//假设机器是64位的 小端序

//0x12345678
//0x100 0x101 0x103 0x104
// 78    56    34     12

uint8 MEM[MEMSIZE];
uint32 Reg[REGSIZE] = {0,};
//默认0号寄存器值为0

void storebyte(uint32 addr, uint8 v){
    if(addr > MEMSIZE){
        printf("storeword error!\n");
        return;
    }
    MEM[addr] = v;
}

void storehalfword(uint32 addr, uint16 v){
    if((addr & 0x01) || addr > MEMSIZE){
        printf("storehalfword error!\n");
        return;
    }
    for(uint32 i = 0;i < 2;i++){
        MEM[addr + i] = (v >> (8 * i))& 0xFF; 
    }
}

void storeword(uint32 addr, uint32 v){
    if((addr & 0x03) || addr > MEMSIZE){
        printf("storeword error!\n");
        return;
    }
    for(uint32 i = 0;i < 4;i++){
        MEM[addr + i] = (v >> (8 * i))& 0xFF; 
        printf("%x\n",MEM[addr + i]);
    }
}


void storedoubleword(uint32 addr, uint64 v){
    if((addr & 0x07) || addr > MEMSIZE){
        printf("storeword error!\n");
        return;
    }
    for(uint32 i = 0;i < 8;i++){
        MEM[addr + i] = (v >> (8 * i))& 0xFF; 
    }
}
uint8 loadbyte(uint32 addr){
    if(addr > MEMSIZE){
        printf("loadbyte error!\n");
        return 0;
    }
    return MEM[addr];
}

uint16 loadhalfword(uint32 addr){
    if((addr & 0x01) || addr > MEMSIZE){
        printf("loadhalfword error!\n");
        return 0;
    }
    uint16 v = 0;
    for(uint32 i = 0;i < 2;i++){
        v |= (uint16)MEM[addr + i] << (8 * i);
    }
    return v;
}

uint32 loadword(uint32 addr){
    if((addr & 0x03) || addr > MEMSIZE){
        printf("loadword error!\n");
        return 0;
    }
    uint32 v = 0;
    for(uint32 i = 0;i < 4;i++){
        v |= (uint32)MEM[addr + i] << (8 * i);
    }
    return v;
}


uint64 loaddoubleword(uint32 addr){
    if((addr & 0x07) || addr > MEMSIZE){
        printf("loaddoubleword error!\n");
        return 0;
    }
    uint64 v = 0;
    for(uint32 i = 0;i < 8;i++){
        v |= (uint64)MEM[addr + i] << (8 * i);
    }
    return v;
}


void storebyte_reg(uint32 regid, uint8 v){
    if(regid > REGSIZE || regid < 1){
        printf("storebyte to reg %x error!\n", regid);
        return;
    }
    Reg[regid] = v;
}

void storehalfword_reg(uint32 regid, uint16 v){
    if(regid > REGSIZE || regid < 1){
        printf("storehalfword to reg %x error!\n", regid);
        return;
    }
    Reg[regid] = v;
}

void storeword_reg(uint32 regid, uint32 v){
    if(regid > REGSIZE || regid < 1){
        printf("storeword to reg %x error!\n", regid);
        return;
    }
    Reg[regid] = v;
}


void storedoubleword_reg(uint32 regid, uint64 v){
    if(regid > REGSIZE || regid < 1){
        printf("storeword to reg %x error!\n", regid);
        return;
    }
    Reg[regid] = v;
}

uint8 loadbyte_reg(uint32 regid){
    if(regid > REGSIZE || regid < 1){
        printf("loadbyte to reg %x error!\n",regid);
        return -1;
    }
    return (uint8)(Reg[regid]& 0xFF);
}

uint16 loadhalfword_reg(uint32 regid){
    if(regid > REGSIZE || regid < 1){
        printf("loadhalfword to reg %x error!\n",regid);
        return -1;
    }
    return (uint16)(Reg[regid]& 0xFFFF);
}

uint32 loadword_reg(uint32 regid){
    if(regid > REGSIZE || regid < 1){
        printf("loadword to reg %x error!\n",regid);
        return -1;
    }
    return (uint32)(Reg[regid]& 0xFFFFFFFF);
}


uint64 loaddoubleword_reg(uint32 regid){
    if(regid > REGSIZE || regid < 1){
        printf("loaddoubleword to reg %x error!\n",regid);
        return -1;
    }
    return (uint64)(Reg[regid]& 0xFFFFFFFFFFFFFFFF);
}


const int N = 0x100;

int main(){
    storedoubleword(N,0x1234567812345678);
    printf("%x\n",loadbyte(N));
    printf("%x\n",loadhalfword(N));
    printf("%x\n",loadword(N));
    printf("%lx\n",loaddoubleword(N));

    storedoubleword_reg(1,0x1234567812345678);
    printf("%x\n",loadbyte_reg(1));
    printf("%x\n",loadhalfword_reg(1));
    printf("%x\n",loadword_reg(1));
    printf("%lx\n",loaddoubleword_reg(1));
    
    float *a = (float *) malloc(sizeof(float)89uij);
    printf("%d\n",a);
    printf("%f\n",(float)a);
}