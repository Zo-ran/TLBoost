/*
 * Copyright 2022, tyyteam(Qingtao Liu, Yang Lei and Yang Chen)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
 .section .text
 .global _start
 _start:
    li.d   $fp, 0
    addi.d $a0, $sp, 0
    la     $t0, __sel4_start_c
    jirl   $ra, $t0, 0

1:
    b 1b