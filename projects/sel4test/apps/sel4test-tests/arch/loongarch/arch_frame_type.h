/*
 * Copyright 2018, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <stdint.h>
#include <sel4/types.h>
#include <sel4/sel4.h>

/* This list must be ordered by size - highest first */
static const frame_type_t frame_types[] = {
    /* Rocket-Chip only has a default 256MiB of RAM in rocketchip.dts,
    * so we can't allocate a 1GiB page for this test.
    * Polarfire has 1GiB of memory can't allocate a 1GiB page for user space */
    
//unset __loongarch_xlen == 64 &&
#if !defined(CONFIG_PLAT_3A5000)
    { seL4_LOONGARCH_Giga_Page, 0, seL4_HugePageBits, },
    { seL4_LOONGARCH_Mega_Page, 0, seL4_LargePageBits, },
#endif
    { seL4_LOONGARCH_16K_Page, BIT(seL4_LargePageBits), seL4_PageBits, },
};
