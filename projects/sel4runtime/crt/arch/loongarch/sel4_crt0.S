/*
 * Copyright 2022, tyyteam(Qingtao Liu, Yang Lei and Yang Chen)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
 #include <sel4runtime/gen_config.h>
 
 .section .text
 .global _sel4_start
 _sel4_start:
    la $sp, __stack_top
    bl __sel4_start_root
    /* should not return */
1:
    b 1b

.section .bss
__stack_base:
    .align 16
	.space CONFIG_SEL4RUNTIME_ROOT_STACK
__stack_top: