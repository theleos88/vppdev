#include <plugins/dpdk/device/flow_table_cpu.h>
flowcount_t *  nodet[256][2] ;
activelist_t * head_af[2];
activelist_t * tail_af[2];
flowcount_t *  head[2] ;
flowcount_t *  previousnode;
flowcount_t *  tail[2];
int numflows;
u32 r_qtotal;
u32 nbl[2];
u64 t[2] = {0,0};//{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
u64 old_t[2];
u8 hello_world[2] = {0,0};//{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
costlen_t * costtable[2];
costpernode_t * costpernode[2];
