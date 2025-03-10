### 编译运行介绍
解压文件到本地后运行以下指令即可:
gcc -o mips_sim mips_sim.c
./mips_sim

建议运行
./mips_sim > res.txt
将结果输出到txt文件中查看

在res.txt中可以看到指令的执行情况，包括流进流水线的指令OPCODE,周期数,每个流水段的指令类型


### MIPS模拟一些规定

主存地址范围对应数据段：

```
data
0x00000-0x01000
text
0x01000-0x03000
heap
...
stack
0x06000-0x80000
```

$gp指针被用作指向data段初始为0，模拟器会从data文件中读取数据并将存到模拟主存的开始位置，这样能实现常数到寄存器的赋值

### 代码框架

通过共享内存和信号量实现了父子进程的通信
父进程主要实现指令的功能模拟并且为子进程生成更简短的时序模拟指令序列
子进程实现了时序模拟并且能够正确打印流水线运行状态，并且加入了数据cache的行为模拟，打印了一些cache的行为
在模拟完父进程提供的指令序列后子进程将会不断将NOP指令送入流水线，直到流水线空闲，计算出正确的周期数

更改DataCache结构参数的宏定义其逻辑结构
```
#define DC_NUM_SETS     8 //组数
#define DC_SET_SIZE     4 //组内行数
#define DC_BLOCK_SIZE   8  //每行的块大小
#define DC_WR_BUFF_SIZE 8 //写缓冲区大小
```
例如将cache增大（增加组数行数块大小）会发现未命中相对减少等行为，符合我们对cache行为变化的预期
