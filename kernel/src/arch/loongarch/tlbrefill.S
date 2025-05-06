/*
 * Copyright 2022, tyyteam(Qingtao Liu, Yang Lei, Yang Chen)
 * qtliu@mail.ustc.edu.cn, le24@mail.ustc.edu.cn, chenyangcs@mail.ustc.edu.cn
 * 
 * Derived from:
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 *
 * SPDX-License-Identifier: GPL-2.0
 */

 #include <arch/machine.h>
 #include <arch/machine/hardware.h>
 #include <hardware.h>
 #include <plat/machine/devices_gen.h>

 #define REGBYTES 8

.section kernel_tlbrefillentry
.global handle_tlb_refill
.global my_tlb_refill
.global tlb_context
handle_tlb_refill:
	csrwr	$t0, LOONGARCH_CSR_TLBRSAVE
	csrrd	$t0, LOONGARCH_CSR_PGD
	lddir	$t0, $t0, 3
	beq     $t0, $r0, refill_unmap
	lddir	$t0, $t0, 1
	beq     $t0, $r0, refill_unmap
	ldpte	$t0, 0
	ldpte	$t0, 1
	tlbfill
	csrrd	$t0, LOONGARCH_CSR_TLBRSAVE
	ertn
refill_unmap:
	csrwr	$r0, LOONGARCH_CSR_TLBRELO0
	csrwr	$r0, LOONGARCH_CSR_TLBRELO1
	tlbfill
	csrrd	$t0, LOONGARCH_CSR_TLBRSAVE
	ertn
my_tlb_refill:
	csrwr	$t0, LOONGARCH_CSR_TLBRSAVE
	csrrd	$t0, LOONGARCH_CSR_PGD
	lddir	$t0, $t0, 3
	beq     $t0, $r0, my_refill_unmap
	lddir	$t0, $t0, 1
	beq     $t0, $r0, my_refill_unmap
	ldpte	$t0, 0
	ldpte	$t0, 1
	tlbfill
	csrrd	$t0, LOONGARCH_CSR_TLBRSAVE
	b		finish
my_refill_unmap:
	csrwr	$r0, LOONGARCH_CSR_TLBRELO0
	csrwr	$r0, LOONGARCH_CSR_TLBRELO1
	tlbfill
	csrrd	$t0, LOONGARCH_CSR_TLBRSAVE
finish:
	csrwr 	$t0, LOONGARCH_CSR_TLBRSAVE
	csrwr 	$t1, LOONGARCH_CSR_KS6
	la.abs 	$t0, tlb_context
	li.d 	$t1, KERNEL_ELF_BASE_OFFSET
	sub.d	$t0, $t0, $t1
	csrrd 	$t1, LOONGARCH_CSR_KS6

	st.d 	$ra, $t0, 0*REGBYTES
    st.d 	$tp, $t0, 1*REGBYTES
	st.d 	$sp, $t0, 2*REGBYTES
	st.d 	$a0, $t0, 3*REGBYTES
    st.d 	$a1, $t0, 4*REGBYTES
    st.d 	$a2, $t0, 5*REGBYTES
    st.d 	$a3, $t0, 6*REGBYTES
    st.d 	$a4, $t0, 7*REGBYTES
    st.d 	$a5, $t0, 8*REGBYTES
    st.d 	$a6, $t0, 9*REGBYTES
    st.d 	$a7, $t0, 10*REGBYTES
    st.d 	$t1, $t0, 12*REGBYTES
    st.d 	$t2, $t0, 13*REGBYTES
    st.d 	$t3, $t0, 14*REGBYTES
    st.d 	$t4, $t0, 15*REGBYTES
    st.d 	$t5, $t0, 16*REGBYTES
    st.d 	$t6, $t0, 17*REGBYTES
    st.d 	$t7, $t0, 18*REGBYTES
    st.d 	$t8, $t0, 19*REGBYTES
    st.d 	$r21, $t0,20*REGBYTES
    st.d 	$s0, $t0, 21*REGBYTES
    st.d 	$s1, $t0, 22*REGBYTES
    st.d 	$s2, $t0, 23*REGBYTES
    st.d 	$s3, $t0, 24*REGBYTES
    st.d 	$s4, $t0, 25*REGBYTES
    st.d 	$s5, $t0, 26*REGBYTES
    st.d 	$s6, $t0, 27*REGBYTES
    st.d 	$s7, $t0, 28*REGBYTES
    st.d 	$s8, $t0, 29*REGBYTES
    st.d 	$fp, $t0, 30*REGBYTES
	csrrd 	$t8, LOONGARCH_CSR_TLBRSAVE
    st.d 	$t8, $t0, 11*REGBYTES

	li.d 	$sp, 0x90000000
	b tlboost