/*
*   Flow classification in VPP
*
*         flow_table.h
*
*
*
*/
#include <vlib/vlib.h>
#include <vnet/ip/ip.h>
#include <vnet/vnet.h>
#include <stdlib.h>
#include <math.h>
#ifndef FLOW_TABLE_H
#define FLOW_TABLE_H
#define TABLESIZE 4096
#define ALPHA 0.4   // ALPHA = Output/Input
#define ALPHACPU 1.0   // ALPHA = Output/Input

#define FIXED_CREDITS   (4000)


//#define BETA 0.1    // BETA = Output/Input
//#define BUFFER 38400000 //just a random number. Update the value with proper theoritical approach.
#define THRESHOLD 128000 //81920 //just a random number. Update the value with proper theoritical approach.

/*Node in the flow table. srcdst is 64 bit divided as |32bitsrcip|32bitdstip| ; swsrcdstport is divided as |32bit swifindex|16bit srcport|16bit dstport|*/
typedef struct flowcount{
    u32 n_packets;
    u32 vqueue;
    struct flowcount * branchnext;
    struct flowcount * update;
}flowcount_t;

typedef struct activelist{
    struct flowcount * flow;
    struct activelist * next;
}activelist_t;

typedef struct costlen{
    f64 costip4;
    f64 costip6;
}costlen_t;

typedef struct costpernode
{
  union{
    struct
    {
      u64 clocks;
      u64 vectors;
    }ip4_input_no_checksum;
    struct
      {
          u64 clocks;
          u64 vectors;
      }ip4_load_balance;
    struct
      {
          u64 clocks;
          u64 vectors;
      }ip4_lookup;
    struct
      {
          u64 clocks;
          u64 vectors;
      }ip4_rewrite;
  }ip4;

  union{
    struct
    {
        u64 clocks;
        u64 vectors;
    }ip6_input;
  struct
    {
        u64 clocks;
        u64 vectors;
    }ip6_lookup;
  struct
    {
        u64 clocks;
        u64 vectors;
    }ip6_rewrite;
  struct
    {
        u64 clocks;
        u64 vectors;
    }interface_output;
  }ip6;

  union{
    struct
    {
        u64 clocks;
        u64 vectors;
    }dpdk_input;
  struct
    {
        u64 clocks;
        u64 vectors;
    }tge_output;
  struct
    {
        u64 clocks;
        u64 vectors;
    }tge_tx;
  }inout;
}costpernode_t;


extern flowcount_t *  nodet[256][24];
extern activelist_t * head_af[24];
extern activelist_t * tail_af[24];
extern flowcount_t *  head [24];
extern costlen_t * costtable[24];
extern costpernode_t * costpernode[24];
extern int numflows;
extern u32 r_qtotal;
extern u32 nbl[24];
extern u64 t[24];
extern u64 old_t[24];
extern u8 hello_world[24];
/* Flow classification function */
always_inline flowcount_t *
flow_table_classify(u8 modulox,u32 cpu_index){

    flowcount_t * flow;

    if(PREDICT_FALSE(nodet[modulox][cpu_index]==NULL)){
        nodet[modulox][cpu_index] = malloc(sizeof(flowcount_t));
        nodet[modulox][cpu_index]->vqueue=0;
        nodet[modulox][cpu_index]->n_packets=0;
    }
        flow = nodet[modulox][cpu_index];

    return flow;
}

/* function to insert the flow in blacklogged flows list. The flow is inserted at the end of the list i.e tail.*/
void flowin(flowcount_t * flow,u32 cpu_index){
    activelist_t * temp;
    temp = malloc(sizeof(activelist_t));
    temp->flow = flow;
    temp->next = NULL;
    if (head_af[cpu_index] == NULL){
        head_af[cpu_index] = temp;
        tail_af[cpu_index] = temp;
    }
    else{
        tail_af[cpu_index]->next = temp;
        tail_af[cpu_index] = temp;
    }
}

/* function to extract the flow from the blacklogged flows list. The flow is taken from the head of the list. */
flowcount_t * flowout(u32 cpu_index){
    flowcount_t * temp;
    activelist_t * next;
    temp = head_af[cpu_index]->flow;
    next = head_af[cpu_index]->next;
    free(head_af[cpu_index]);
    head_af[cpu_index] = next;
    return temp;
}

/* vstate algorithm */
always_inline void vstate(flowcount_t * flow,u8 update,u32 cpu_index){

    if(PREDICT_FALSE(update == 1)){
        flowcount_t * j;
        f32 served,credit;
        int oldnbl=nbl[cpu_index]+1;
        credit = ((t[cpu_index]-old_t[cpu_index])*ALPHACPU);///(2.6);
        while (oldnbl>nbl[cpu_index] && nbl[cpu_index] > 0){
            oldnbl = nbl[cpu_index];
            served = credit/nbl[cpu_index];
            credit = 0;
            for (int k=0;k<oldnbl;k++){
                j = flowout(cpu_index);
                if(j->vqueue > served){
                    j->vqueue -= served;
                    flowin(j,cpu_index);
                }
                else{
                    credit += served - j->vqueue;
                    j->vqueue = 0;
                    nbl[cpu_index]--;
                }
            }
        }
    }

    if (flow != NULL){
        if (flow->vqueue == 0){
            nbl[cpu_index]++;
            flowin(flow,cpu_index);
        }
        //flow->vqueue += pktlenx;
		flow->n_packets+=1;
    }
}

/* arrival function for each packet */
always_inline u8 arrival(flowcount_t * flow,u32 cpu_index){
u8 drop;
    //printf("%d\n",flow->vqueue);
    if(flow->vqueue <= THRESHOLD /*&& r_qtotal < BUFFER*/){
        vstate(flow,0,cpu_index);
        //r_qtotal += pktlenx;
        drop = 0;
    }
    else {
        drop = 1;
        //update vstate is only after a vector. So no update before dropping a packet here.
    }
return drop;
}

always_inline u8 fq (u8 modulox,u32 cpu_index){
    flowcount_t * i;
    u8 drop;
    i = flow_table_classify(modulox,cpu_index);
    drop = arrival(i,cpu_index);
    return drop;
}

/*Function to update costs*/
always_inline void update_costs(vlib_main_t *vm,u32 index){

    if(PREDICT_FALSE(costtable[index]==NULL)){
        costtable[index] = malloc(sizeof(costlen_t));
        memset(costtable[index], 0, sizeof (costlen_t));
    }
    costlen_t *cost = costtable[index];
    if(PREDICT_FALSE(costpernode[index]==NULL)){
        costpernode[index]=malloc(sizeof(costpernode_t));
        memset(costpernode[index],0,sizeof(costpernode_t));
    }
    costpernode_t * cost_node = costpernode[index];

    f64 costip4;
    f64 costip6;
    f64 dpdk,out,tx;
    costip4 = costip6 = 0;

    vlib_node_t *ip4_chain = vlib_get_node_by_name (vm, (u8 *) "dpdk-input");
    vlib_node_sync_stats (vm, ip4_chain);
    dpdk = (f64)(ip4_chain->stats_total.clocks - cost_node->inout.dpdk_input.clocks)/(f64)(ip4_chain->stats_total.vectors - cost_node->inout.dpdk_input.vectors);
    costip4 += dpdk;
    cost_node->inout.dpdk_input.clocks = ip4_chain->stats_total.clocks;
    cost_node->inout.dpdk_input.vectors = ip4_chain->stats_total.vectors;

    ip4_chain = vlib_get_node_by_name (vm, (u8 *) "ip4-input-no-checksum");
    vlib_node_sync_stats (vm, ip4_chain);
    costip4 += (f64)(ip4_chain->stats_total.clocks - cost_node->ip4.ip4_input_no_checksum.clocks)/(f64)(ip4_chain->stats_total.vectors - cost_node->ip4.ip4_input_no_checksum.vectors);
    cost_node->ip4.ip4_input_no_checksum.clocks = ip4_chain->stats_total.clocks;
    cost_node->ip4.ip4_input_no_checksum.vectors = ip4_chain->stats_total.vectors;

    ip4_chain = vlib_get_node_by_name (vm, (u8 *) "ip4-load-balance");
    vlib_node_sync_stats (vm, ip4_chain);
    costip4 += (f64)(ip4_chain->stats_total.clocks - cost_node->ip4.ip4_load_balance.clocks)/(f64)(ip4_chain->stats_total.vectors - cost_node->ip4.ip4_load_balance.vectors);
    cost_node->ip4.ip4_load_balance.clocks = ip4_chain->stats_total.clocks;
    cost_node->ip4.ip4_load_balance.vectors = ip4_chain->stats_total.vectors;

    ip4_chain = vlib_get_node_by_name (vm, (u8 *) "ip4-lookup");
    vlib_node_sync_stats (vm, ip4_chain);
    costip4 += (f64)(ip4_chain->stats_total.clocks - cost_node->ip4.ip4_lookup.clocks)/(f64)(ip4_chain->stats_total.vectors - cost_node->ip4.ip4_lookup.vectors);
    cost_node->ip4.ip4_lookup.clocks = ip4_chain->stats_total.clocks;
    cost_node->ip4.ip4_lookup.vectors = ip4_chain->stats_total.vectors;

    ip4_chain = vlib_get_node_by_name (vm, (u8 *) "ip4-rewrite");
    vlib_node_sync_stats (vm, ip4_chain);
    costip4 += (f64)(ip4_chain->stats_total.clocks - cost_node->ip4.ip4_rewrite.clocks)/(f64)(ip4_chain->stats_total.vectors - cost_node->ip4.ip4_rewrite.vectors);
    cost_node->ip4.ip4_rewrite.clocks = ip4_chain->stats_total.clocks;
    cost_node->ip4.ip4_rewrite.vectors = ip4_chain->stats_total.vectors;

    ip4_chain = vlib_get_node_by_name (vm, (u8 *) "TenGigabitEthernet84/0/1-output");
    vlib_node_sync_stats (vm, ip4_chain);
    out = (f64)(ip4_chain->stats_total.clocks - cost_node->inout.tge_output.clocks)/(f64)(ip4_chain->stats_total.vectors - cost_node->inout.tge_output.vectors);
    costip4 += out;
    cost_node->inout.tge_output.clocks = ip4_chain->stats_total.clocks;
    cost_node->inout.tge_output.vectors = ip4_chain->stats_total.vectors;

    ip4_chain = vlib_get_node_by_name (vm, (u8 *) "TenGigabitEthernet84/0/1-tx");
    vlib_node_sync_stats (vm, ip4_chain);
    tx = (f64)(ip4_chain->stats_total.clocks - cost_node->inout.tge_tx.clocks)/(f64)(ip4_chain->stats_total.vectors - cost_node->inout.tge_tx.vectors);
    costip4 += tx;
    cost_node->inout.tge_tx.clocks = ip4_chain->stats_total.clocks;
    cost_node->inout.tge_tx.vectors = ip4_chain->stats_total.vectors;

    costip6 += dpdk+out+tx;
    vlib_node_t *ip6_chain = vlib_get_node_by_name (vm, (u8 *) "ip6-input");
    vlib_node_sync_stats (vm, ip6_chain);
    costip6 += (f64)(ip6_chain->stats_total.clocks - cost_node->ip6.ip6_input.clocks)/(f64)(ip6_chain->stats_total.vectors - cost_node->ip6.ip6_input.vectors);
    cost_node->ip6.ip6_input.clocks = ip6_chain->stats_total.clocks;
    cost_node->ip6.ip6_input.vectors = ip6_chain->stats_total.vectors;

    ip6_chain = vlib_get_node_by_name (vm, (u8 *) "ip6-lookup");
    vlib_node_sync_stats (vm, ip6_chain);
    costip6 += (f64)(ip6_chain->stats_total.clocks - cost_node->ip6.ip6_lookup.clocks)/(f64)(ip6_chain->stats_total.vectors - cost_node->ip6.ip6_lookup.vectors);
    cost_node->ip6.ip6_lookup.clocks = ip6_chain->stats_total.clocks;
    cost_node->ip6.ip6_lookup.vectors = ip6_chain->stats_total.vectors;

    ip6_chain = vlib_get_node_by_name (vm, (u8 *) "ip6-rewrite");
    vlib_node_sync_stats (vm, ip6_chain);
    costip6 += (f64)(ip6_chain->stats_total.clocks - cost_node->ip6.ip6_rewrite.clocks)/(f64)(ip6_chain->stats_total.vectors - cost_node->ip6.ip6_rewrite.vectors);
    cost_node->ip6.ip6_rewrite.clocks = ip6_chain->stats_total.clocks;
    cost_node->ip6.ip6_rewrite.vectors = ip6_chain->stats_total.vectors;

    ip6_chain = vlib_get_node_by_name (vm, (u8 *) "interface-output");
    vlib_node_sync_stats (vm, ip6_chain);
    costip6 += (f64)(ip6_chain->stats_total.clocks - cost_node->ip6.interface_output.clocks)/(f64)(ip6_chain->stats_total.vectors - cost_node->ip6.interface_output.vectors);
    cost_node->ip6.interface_output.clocks = ip6_chain->stats_total.clocks;
    cost_node->ip6.interface_output.vectors = ip6_chain->stats_total.vectors;

    cost->costip4 = costip4;
    cost->costip6 = costip6;
}

/*function to increment vqueues using the updated costs*/
always_inline void update_vstate(vlib_main_t * vm,u32 index){
    costlen_t *cost = costtable[index];
    if(PREDICT_TRUE(nodet[0][index]!=NULL)){
    nodet[0][index]->vqueue += nodet[0][index]->n_packets*(cost->costip4);
    nodet[0][index]->n_packets=0;
    }
    if(PREDICT_TRUE(nodet[1][index]!=NULL)){
    nodet[1][index]->vqueue += nodet[1][index]->n_packets*(cost->costip6);
    nodet[1][index]->n_packets=0;
    }
}


/*vstate update function before sending the vector. This function is after processing all the packets in the vector and runs only once per vector */
always_inline void departure (u32 cpu_index){
    vstate(NULL,1,cpu_index);
}


#endif /*FLOW_TABLE_H*/

/*
*   "Gather ye rosebuds while ye may"
*                  - Mike Portnoy
*
*   End
*
*/

