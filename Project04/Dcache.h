
//cache 相关
#define DC_NUM_SETS     8 //组数
#define DC_SET_SIZE     4 //组内行数
#define DC_BLOCK_SIZE   8  //每行的块大小
#define DC_WR_BUFF_SIZE 8 //写缓冲区大小
#define DC_INVALID      0 
#define DC_VALID        1
#define DC_DIRTY        2

#define MemRdLatency    3
#define HitdCLtcy       1
#define WrMergLtcy      0
#define ClearWrBfLtcy   1
#define AddBlk2WrBfLtcy 0
#define FlushWrBfLtcy   3
