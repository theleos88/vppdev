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

extern flowcount_t *  nodet[256][24];
extern activelist_t * head_af[24];
extern activelist_t * tail_af[24];
extern flowcount_t *  head [24];
//extern costs_t * costtable;
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

