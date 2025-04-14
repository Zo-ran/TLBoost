
/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230).
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * seL4 tutorial part 4: application to be run in a process
 */

 #include <stdio.h>
 #include <assert.h>
 
 #include <sel4/sel4.h>
 #include <sel4utils/process.h>
 
 #include <utils/zf_log.h>
 #include <sel4utils/sel4_zf_logif.h>
 
 /* constants */
 #define MSG_DATA 0x6161 //  arbitrary data to send
#define CALL 1

 static inline uint64_t read_stable_counter(void) {
    uint64_t time;
    register uint64_t tmp __asm__ ("r1");  // 临时保存CounterID（如果需要）

    __asm__ volatile (
        "rdtime.d %0, %1 \n\t"
        : "=r"(time), "=r"(tmp)
    );

    return time;
}

 int main(int argc, char **argv) {
     seL4_MessageInfo_t tag;
     seL4_Word msg;
    uint64_t t0,t1,n,avg;
     printf("process_2: hey hey hey\n");
 
     int a=0;
     for (int i = 0; i < 10000; i++)
     {
         a=a+1;
         continue;
     }
     
     /*
      * send a message to our parent, and wait for a reply
      */
 
     /* set the data to send. We send it in the first message register */
     tag = seL4_MessageInfo_new(0, 0, 0, 1);
     seL4_SetMR(0, MSG_DATA);
 
     
  /* TASK 8: send and wait for a reply */
     /* hint 1: seL4_Call()
      * seL4_MessageInfo_t seL4_Call(seL4_CPtr dest, seL4_MessageInfo_t msgInfo)
      * @param dest The capability to be invoked.
      * @param msgInfo The messageinfo structure for the IPC.  This specifies information about the message to send (such as the number of message registers to send).
      * @return A seL4_MessageInfo_t structure.  This is information about the repy message.
      *
      * hint 2: send the endpoint cap using argv (see TASK 6 in the other main.c)
      */
     ZF_LOGF_IF(argc < 1,
                "Missing arguments.\n");
     seL4_CPtr ep = (seL4_CPtr) atol(argv[0]);
     
    printf("\n\n\n\n");
     printf("---SYSTEM STANDBY FOR TESTING---\n");
     n=100;
     #ifndef CALL
     tag = seL4_CallBoost(ep, tag,10);
     #else
     seL4_Call(ep, tag);
     #endif    
     t0=read_stable_counter();
     
     for (uint64_t i = 0; i < n; i++)
     {
        #ifndef CALL
        seL4_CallBoost(ep, tag,10);
        #else
        seL4_Call(ep, tag);
        #endif
     }
     t1=read_stable_counter();   
     avg=(t1-t0)/n;
     printf("----Time tick raw data----\nt0:%u\tt1:%u\tn:%u\n",t0,t1,n);
     printf("---Time tick PROCESSED data---\navg rtt:%u\n",avg);
     /* check that we got the expected reply */
     ZF_LOGF_IF(seL4_MessageInfo_get_length(tag) != 1,
                "Length of the data send from root thread was not what was expected.\n"
                "\tHow many registers did you set with seL4_SetMR, within the root thread?\n");
 
     msg = seL4_GetMR(0);
    //  ZF_LOGF_IF(msg != ~MSG_DATA,
    //             "Unexpected response from root thread.\n");
 
    //  printf("process_2: got a reply: %#" PRIxPTR "\n", msg);
     printf("---TEST COMPLETED---\n");
     return 0;
 }