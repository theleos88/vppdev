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
#define ALPHACPU (1)   // ALPHA = Output/Input

#define FIXED_CREDITS   (4000)


//#define BETA 0.1    // BETA = Output/Input
//#define BUFFER 38400000 //just a random number. Update the value with proper theoritical approach.
#define THRESHOLD (3840000) //just a random number. Update the value with proper theoritical approach.

/*Node in the flow table. srcdst is 64 bit divided as |32bitsrcip|32bitdstip| ; swsrcdstport is divided as |32bit swifindex|16bit srcport|16bit dstport|*/
typedef struct flowcount{
    u32 hash;
    u32 vqueue;
    struct flowcount * branchnext;
    struct flowcount * update;
}flowcount_t;

typedef struct activelist{
    struct flowcount * flow;
    struct activelist * next;
}activelist_t;

extern flowcount_t *  nodet[TABLESIZE];
extern activelist_t * head_af;
extern activelist_t * tail_af;
extern flowcount_t *  head ;
extern int numflows;
extern u32 r_qtotal;
extern u32 nbl;
extern u64 t;
extern u64 old_t;

/* Flow classification function */
always_inline flowcount_t *
flow_table_classify(u32 modulox,u32 hashx, u16 pktlenx){

    flowcount_t * flow;
	if(PREDICT_FALSE(nodet[modulox]==NULL))
        	nodet[modulox] = malloc(sizeof(flowcount_t));
        flow = nodet[modulox];
    return flow;
}

/* function to insert the flow in blacklogged flows list. The flow is inserted at the end of the list i.e tail.*/
void flowin(flowcount_t * flow){
    activelist_t * temp;
    temp = malloc(sizeof(activelist_t));
    temp->flow = flow;
    temp->next = NULL;
    if (head_af == NULL){
        head_af = temp;
        tail_af = temp;
    }
    else{
        tail_af->next = temp;
        tail_af = temp;
    }
}

/* function to extract the flow from the blacklogged flows list. The flow is taken from the head of the list. */
flowcount_t * flowout(){
    flowcount_t * temp;
    activelist_t * next;
    temp = head_af->flow;
    next = head_af->next;
    free(head_af);
    head_af = next;
    return temp;
}


/* vstate algorithm */
always_inline void vstate(flowcount_t * flow, u16 pktlenx,u8 update){

    if(PREDICT_FALSE(update == 1)){
        flowcount_t * j;
        u32 served,credit;
        int oldnbl=nbl+1;
        //credit = (t - old_t)*ALPHACPU;//ALPHACPU;
        credit = FIXED_CREDITS;
        //printf("VSTATE  | T %u, Credt %u\n", (uint32_t)t, credit);
        //printf("%u\n",credit);
//        if(PREDICT_FALSE(update == 1)){
        while (oldnbl>nbl && nbl > 0){
            oldnbl = nbl;
            served = credit/nbl;
            credit = 0;
            for (int k=0;k<oldnbl;k++){
                j = flowout();
                if(j->vqueue > served){
                    j->vqueue -= served;
                    flowin(j);
                }
                else{
                    credit += served - j->vqueue;
                    j->vqueue = 0;
                    nbl--;
                }
            }
        }
    }

    if (flow != NULL){
        if (flow->vqueue == 0){
            nbl++;
            flowin(flow);
        }
        flow->vqueue += pktlenx;
    }
}

/* arrival function for each packet */
always_inline u8 arrival(flowcount_t * flow, u16 pktlenx){
u8 drop;
    if(flow->vqueue <= THRESHOLD /*&& r_qtotal < BUFFER*/){
        vstate(flow,pktlenx,0); 
        //r_qtotal += pktlenx;
        drop = 0;
    }
    else {
        drop = 1;
        //update vstate is only after a vector. So no update before dropping a packet here.
    }
return drop;
}

always_inline u8 fq (u8 classip4, u8 classip6, u8 classl2, u16 pktlenx){
    flowcount_t * i;
    u8 drop;
    i = flow_table_classify(classip4,classip6,classl2,pktlenx);
    drop = arrival(i,pktlenx);
    return drop;
}

/*vstate update function before sending the vector. This function is after processing all the packets in the vector and runs only once per vector */
always_inline void departure (){
    vstate(NULL,0,1);
}
#endif /*FLOW_TABLE_H*/

/*
*   "Gather ye rosebuds while ye may"
*                  - Mike Portnoy
*
*   End
*
*/

