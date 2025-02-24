//describe the transformation of bus snooping protocol

#include <stdio.h>
#include <stdlib.h>

//nee an edge <Exclusive,Share>

enum{
    EXCLUSIVE,
    SHARE,
    INVALID
};

enum{
    READ,
    WRITE,
    ALU,
    BRANCH,
    JMP
};

struct Instruction{
    int process_cnumber;
    int address;
    int opcode;
    int data;
};

const int N = 1000;
struct bus{
    int state;
    int data;
    int process_cnumber;
}Bus[N];

void bus_snooping_protocol(struct Instruction inst){
    
    switch (Bus[inst.address].state){
        case INVALID:
            if(inst.opcode == READ){
                Bus[inst.address].state = SHARE;
            }
            else if(inst.opcode == WRITE){
                Bus[inst.address].state = EXCLUSIVE;
            }
            break;
        case EXCLUSIVE:
            if(inst.opcode == WRITE){
                Bus[inst.address].state = EXCLUSIVE;
            }
            else if(inst.opcode == READ && inst.process_cnumber == Bus[inst.address].process_cnumber){
                Bus[inst.address].state = EXCLUSIVE;// read hit 且是自己的数据
            }
            else if(inst.opcode == READ && inst.process_cnumber != Bus[inst.address].process_cnumber){
                Bus[inst.address].state = SHARE; //read miss 且不是自己的数据
            }
            break;
        case SHARE:
            if(inst.opcode == WRITE){
                Bus[inst.address].state = INVALID;
            }
            else if(inst.opcode == READ){
                Bus[inst.address].state = SHARE;
            }
            break;
    }
}