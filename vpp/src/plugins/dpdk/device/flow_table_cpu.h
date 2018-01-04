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
#include <vppinfra/time.h>
#ifndef FLOW_TABLE_H
#define FLOW_TABLE_H
#define ALPHACPU 1.0
#define THRESHOLD 1280000

#define WEIGHT_IP4	320
#define WEIGHT_IP6	417

typedef struct flowcount{
    u32 n_packets;
    u32 vqueue;
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
extern u64 s[24];
extern u64 s_total[24];

/* Flow/class classification function */
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
        credit = ((t[cpu_index]-old_t[cpu_index])*ALPHACPU);
        while (oldnbl>nbl[cpu_index] && nbl[cpu_index] > 0){
            oldnbl = nbl[cpu_index];
            served = credit/(nbl[cpu_index]);
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
			flow->vqueue = 1;
        }
		flow->n_packets++;
    }
}

/* arrival function for each packet */
always_inline u8 arrival(flowcount_t * flow,u32 cpu_index){
u8 drop;
    if(flow->vqueue <= THRESHOLD /*&& r_qtotal < BUFFER*/){
        vstate(flow,0,cpu_index);
        drop = 0;
    }
    else {
        drop = 1;
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

    if(nodet[0][index]!=NULL && nodet[1][index]!=NULL){
		if (nodet[0][index]->n_packets > 0){
			cost->costip4 =  ((f64)(WEIGHT_IP4*s_total[index]))/((f64)((WEIGHT_IP4*nodet[0][index]->n_packets)+(WEIGHT_IP6*nodet[1][index]->n_packets)));
			//printf("%lf\t",cost->costip4);
		}
		else
            cost->costip4 = 0;
		if (nodet[1][index]->n_packets > 0){
			cost->costip6 =  ((f64)(WEIGHT_IP6*s_total[index]))/((f64)((WEIGHT_IP4*nodet[0][index]->n_packets)+(WEIGHT_IP6*nodet[1][index]->n_packets)));
			//printf("%lf\n",cost->costip6);
		}
		else
            cost->costip6 = 0;
	}
    else if(nodet[0][index]!=NULL && nodet[1][index]==NULL){
		if (nodet[0][index]->n_packets > 0){
			cost->costip4 = (s_total[index]/nodet[0][index]->n_packets);
			//printf("Hello:%lf\t",cost->costip4);
		}
		else
			cost->costip4 = 0;
	}
	else if(nodet[0][index]==NULL && nodet[1][index]!=NULL){
		if (nodet[1][index]->n_packets > 0)
			cost->costip6 = (s_total[index]/nodet[1][index]->n_packets);
		else
            cost->costip6 = 0;
	}
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

always_inline void departure (u32 cpu_index){
    vstate(NULL,1,cpu_index);
}

always_inline void sleep_now (u64 t1,u64 old_t1){
	u64 t_sleep = ((t1-old_t1)*(1-ALPHACPU));
	clib_cpu_time_wait(t_sleep);
}


#endif /*FLOW_TABLE_H*/

/*
*   "Gather ye rosebuds while ye may"
*                  - Mike Portnoy
*
*   End
*
*/

