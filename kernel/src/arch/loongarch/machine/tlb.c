/*
 * Copyright 2022, tyyteam
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include <arch/machine.h>
#include <arch/machine/tlb.h>
#include <linker.h>
#include <string.h>
#include <arch/kernel/traps.h>
#include <arch/machine/cache.h>
#include <hardware.h>

void local_flush_tlb_all(void)
{
	invtlb_all(INVTLB_CURRENT_ALL, 0, 0);
}

word_t tlb_context[n_contextRegisters];

extern void handle_tlb_refill(void);

extern void my_tlb_refill(void);
/* 
    !!!! real mode in this function !!!! 
    **** all non-inline functions forbidden ****
*/
void tlboost(void) {
    asm volatile(
        "la.abs 	$t0, tlb_context\n"
        "li.d 	$t1, %[OFFSET]\n"
	    "sub.d	$t0, $t0, $t1\n"
        "ld.d  $ra, $t0, 0*%[REGSIZE]   \n"
        "ld.d  $tp, $t0, 1*%[REGSIZE]   \n"
        "ld.d  $sp, $t0, 2*%[REGSIZE]   \n"
        "ld.d  $a0, $t0, 3*%[REGSIZE]   \n"
        "ld.d  $a1, $t0, 4*%[REGSIZE]   \n"
        "ld.d  $a2, $t0, 5*%[REGSIZE]   \n"
        "ld.d  $a3, $t0, 6*%[REGSIZE]   \n"
        "ld.d  $a4, $t0, 7*%[REGSIZE]   \n"
        "ld.d  $a5, $t0, 8*%[REGSIZE]   \n"
        "ld.d  $a6, $t0, 9*%[REGSIZE]   \n"
        "ld.d  $a7, $t0, 10*%[REGSIZE]  \n"
        "ld.d  $t1, $t0, 12*%[REGSIZE]  \n"
        "ld.d  $t2, $t0, 13*%[REGSIZE]  \n"
        "ld.d  $t3, $t0, 14*%[REGSIZE]  \n"
        "ld.d  $t4, $t0, 15*%[REGSIZE]  \n"
        "ld.d  $t5, $t0, 16*%[REGSIZE]  \n"
        "ld.d  $t6, $t0, 17*%[REGSIZE]  \n"
        "ld.d  $t7, $t0, 18*%[REGSIZE]  \n"
        "ld.d  $t8, $t0, 19*%[REGSIZE]  \n"
        "ld.d  $r21, $t0, 20*%[REGSIZE] \n"
        "ld.d  $s0, $t0, 21*%[REGSIZE]  \n"
        "ld.d  $s1, $t0, 22*%[REGSIZE]  \n"
        "ld.d  $s2, $t0, 23*%[REGSIZE]  \n"
        "ld.d  $s3, $t0, 24*%[REGSIZE]  \n"
        "ld.d  $s4, $t0, 25*%[REGSIZE]  \n"
        "ld.d  $s5, $t0, 26*%[REGSIZE]  \n"
        "ld.d  $s6, $t0, 27*%[REGSIZE]  \n"
        "ld.d  $s7, $t0, 28*%[REGSIZE]  \n"
        "ld.d  $s8, $t0, 29*%[REGSIZE]  \n"
        "ld.d  $fp, $t0, 30*%[REGSIZE]  \n"
        "ld.d  $t0, $t0, 11*%[REGSIZE]  \n"
        "ertn"
        :
        : [REGSIZE] "i"(8), [OFFSET] "i"(KERNEL_ELF_BASE_OFFSET)
        : "memory"
    );
}

void setup_tlb_handler(void)
{
    // write_csr_tlbrentry(kpptr_to_paddr(handle_tlb_refill));
    write_csr_tlbrentry(kpptr_to_paddr(my_tlb_refill));
}

BOOT_CODE void init_tlb(void)
{
    write_csr_pagesize(PS_DEFAULT_SIZE);
    write_csr_stlbpgsize(PS_DEFAULT_SIZE);
    write_csr_tlbrefill_pagesize(PS_HUGE_SIZE);
    
    if (read_csr_pagesize() != PS_DEFAULT_SIZE)
    printf("MMU doesn't support PAGE_SIZE\n");
    
    setup_tlb_handler();
    // local_flush_tlb_all();
}
