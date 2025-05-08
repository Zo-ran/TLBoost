
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
    printf("process_2: hey hey hey\n");
    seL4_CPtr ep = (seL4_CPtr) atol(argv[0]);
    
    seL4_MessageInfo_t tag;
    seL4_Word msg;
    uint64_t t0, t1, n, avg;
 
    tag = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_SetMR(0, MSG_DATA);
     
     
    printf("\n\n\n\n---SYSTEM STANDBY FOR TESTING---\n");
    n = 1;
    tag = seL4_CallBoost(ep, tag, 10);
    t0 = read_stable_counter();
    
    for (uint64_t i = 0; i < n; i++) {
        seL4_CallBoost(ep, tag, 10);
    }

    t1 = read_stable_counter();   
    avg = (t1 - t0) / n;
    printf("----Time tick raw data----\nt0:%u\tt1:%u\tn:%u\n",t0,t1,n);
    printf("---Time tick PROCESSED data---\navg rtt:%u\n",avg);

    printf("---TEST COMPLETED---\n");
    return 0;
 }