#include <dpdk/device/flow_table.h>
flowcount_t *  nodet[3] ;
activelist_t * head_af;
activelist_t * tail_af;
flowcount_t *  head ;
flowcount_t *  previousnode;
flowcount_t *  tail;
int numflows;
u32 r_qtotal;
u32 nbl;
u64 t = 0;
u64 old_t;
