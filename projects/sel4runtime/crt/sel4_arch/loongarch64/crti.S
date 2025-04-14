/*
 * Copyright 2022, tyyteam(Qingtao Liu, Yang Lei and Yang Chen)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
.section .init
.global _init
_init:
    addi.d  $sp, $sp, -16
    st.d    $fp, $sp, 0
    st.d    $ra, $sp, 8
    addi.d  $fp, $sp, 0

.section .fini
.global _fini
_fini:
    addi.d  $sp, $sp, -16
    st.d    $fp, $sp, 0
    st.d    $ra, $sp, 8
    addi.d  $fp, $sp, 0