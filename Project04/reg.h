#define REGSIZE 32
#define FEGSIZE 16

int   ireg_size = 32;       // 通用寄存器文件的大小
int  *r;                  // 通用寄存器文件起始地址指针

int   freg_size = 16;       // 浮点寄存器文件的大小
float  *f; 