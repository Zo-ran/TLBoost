/*
 * Copyright 2018, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <stdint.h>
#include <sel4/sel4.h>

#define EXCEPT_IPC_SYS_MR_IP EXCEPT_IPC_SYS_MR_RIP
#define ARCH_SYSCALL_INSTRUCTION_SIZE 4

static inline int sel4utils_is_read_fault(void)
{
    seL4_Word fsr = seL4_GetMR(seL4_VMFault_FSR);
    /* 目前没考虑特权等级无效例外，这可能是read或write触发的 */
    return (fsr == 1 || fsr == 3 || fsr == 5 || fsr == 6);
}

static inline void sel4utils_set_instruction_pointer(seL4_UserContext *regs,
                                                     seL4_Word value)
{
    regs->pc = value;
}

static inline seL4_Word sel4utils_get_instruction_pointer(seL4_UserContext regs)
{
    return regs.pc;
}

static inline seL4_Word sel4utils_get_sp(seL4_UserContext regs)
{
    return regs.sp;
}

static inline void sel4utils_set_stack_pointer(seL4_UserContext *regs,
                                               seL4_Word value)
{
    regs->sp = value;
}
