#include <plugins/dpdk/device/flow_table_cpu.h>
flowcount_t *  nodet[256][24] ;
activelist_t * head_af[24];
activelist_t * tail_af[24];
flowcount_t *  head[24] ;
flowcount_t *  previousnode;
flowcount_t *  tail[24];
int numflows;
u32 r_qtotal;
u32 nbl[24];
u64 t[24] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
u64 old_t[24];
u8 hello_world[24] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
costlen_t * costtable[24];
costpernode_t * costpernode[24];
