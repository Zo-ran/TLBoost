/*
 * Copyright 2022, tyyteam(Qingtao Liu, Yang Lei, Yang Chen)
 * qtliu@mail.ustc.edu.cn, le24@mail.ustc.edu.cn, chenyangcs@mail.ustc.edu.cn
 * 
 * Derived from:
 * Copyright 2020, DornerWorks
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 * Copyright 2015, 2016 Hesham Almatary <heshamelmatary@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <types.h>
#include <benchmark/benchmark.h>
#include <api/failures.h>
#include <api/syscall.h>
#include <kernel/boot.h>
#include <kernel/cspace.h>
#include <kernel/thread.h>
#include <object/tcb.h>
#include <machine/io.h>
#include <model/preemption.h>
#include <model/statedata.h>
#include <object/cnode.h>
#include <object/untyped.h>
#include <arch/api/invocation.h>
#include <arch/kernel/vspace.h>
#include <linker.h>
#include <arch/machine.h>
#include <plat/machine/hardware.h>
#include <kernel/stack.h>
#include <util.h>

enum PTE_TYPE {
    PTE_NONE,
    PTE_L1,
    PTE_L2,
    PTE_L3
};

struct resolve_ret {
    paddr_t frameBase;
    vm_page_size_t frameSize;
    bool_t valid;
};
typedef struct resolve_ret resolve_ret_t;

static exception_t performPageGetAddress(void *vbase_ptr);

// static word_t CONST LOONGARCHGetWriteFromVMRights(vm_rights_t vm_rights)
// {
//     /* Write-only frame cap rights not currently supported. */
//     return vm_rights == VMReadWrite;
// }

// static inline word_t CONST LOONGARCHGetReadFromVMRights(vm_rights_t vm_rights)
// {
//     /* Write-only frame cap rights not currently supported.
//      * Kernel-only conveys no user rights. */
//     return vm_rights != VMKernelOnly;
// }

// static inline uint64_t PURE
// pte_ptr_get_huge(pte_t *pte_ptr) {
//     uint64_t ret;
//     ret = (pte_ptr->words[0] & 0x3ffffffffffc00ull) >> 10;
//     /* Possibly sign extend */
//     if (__builtin_expect(!!(0 && (ret & (1ull << (38)))), 0)) {
//         ret |= 0x0;
//     }
//     return ret;
// }

static inline uint64_t PURE
pte_ptr_get_ppn(pte_t *pte_ptr) {
    uint64_t ret;
    ret = (pte_ptr->words[0] & 0xffffffffc000ull) >> 14;
    /* Possibly sign extend */
    // if (__builtin_expect(!!(0 && (ret & (1ull << (36)))), 0)) {
    //     ret |= 0x0;
    // }
    return ret;
}

// static inline uint64_t PURE
// pte_ptr_get_execute(pte_t *pte_ptr) {
//     uint64_t ret;
//     ret = (0 || (pte_ptr->words[0] >> 62) & 0x1ull) ? 0x1ull : 0x0ull;
//     return ret;
// }

// static inline uint64_t PURE
// pte_ptr_get_write(pte_t *pte_ptr) {
//     uint64_t ret;
//     ret = (pte_ptr->words[0] & 0x4ull) >> 2;
//     /* Possibly sign extend */
//     if (__builtin_expect(!!(0 && (ret & (1ull << (38)))), 0)) {
//         ret |= 0x0;
//     }
//     return ret;
// }

// static inline uint64_t PURE
// pte_ptr_get_read(pte_t *pte_ptr) {
//     uint64_t ret;
//     ret = (pte_ptr->words[0] & 0x2ull) >> 1;
//     /* Possibly sign extend */
//     if (__builtin_expect(!!(0 && (ret & (1ull << (38)))), 0)) {
//         ret |= 0x0;
//     }
//     return ret;
// }

static inline uint64_t PURE
pte_ptr_get_valid(pte_t *pte_ptr) {
    uint64_t ret;
    ret = (pte_ptr->words[0] & 0x1ull);
    return ret;
}

static inline uint64_t PURE
pte_ptr_is_leaf(pte_t *pte_ptr) {
    uint64_t ret;
    ret = (pte_ptr->words[0] & 0x200ull);
    return ret;
}

static inline uint64_t PURE
pte_ptr_is_huge(pte_t *pte_ptr) {
    uint64_t ret;
    ret = (pte_ptr->words[0] & 0x1000ull);
    return ret;
}

static inline uint64_t PURE
pte_ptr_is_not_initial(pte_t *pte_ptr) {
    uint64_t ret;
    ret = pte_ptr->words[0];
    return ret;
}

static inline bool_t isPTEPageTable(pte_t *pte)
{
    if (pte_ptr_is_leaf(pte) || !pte_ptr_is_not_initial(pte)) {
        return 0;
    }
    else {
        return 1;
    }
}

// static inline bool_t isPTEPageTable(pte_t *pte)
// {
//     return pte_ptr_get_valid(pte) &&
//            !(pte_ptr_get_read(pte) || pte_ptr_get_write(pte) || pte_ptr_get_execute(pte));
// }

/** Helper function meant only to be used for mapping the kernel
 * window.
 *
 * Maps all pages with full RWX and supervisor perms by default.
 */
static pte_t pte_next(word_t phys_addr, bool_t is_leaf, enum PTE_TYPE pte_type)
{
    pte_t pte;

    if (!is_leaf) {
        pte.words[0] = PTE_CREATE_NEXT(phys_addr);
    }
    else {
        if (pte_type == PTE_L3) {
            pte.words[0] = PTE_CREATE_L3_LEAF(phys_addr);
        }
        else if (pte_type == PTE_L2) {
            pte.words[0] = PTE_CREATE_L2_LEAF(phys_addr);
        }
        else {
            pte.words[0] = PTE_CREATE_L1_LEAF(phys_addr);
        }
    }

    return pte;
}

static pte_t user_pte_next(word_t phys_addr, bool_t is_leaf, enum PTE_TYPE pte_type)
{
    pte_t pte;

    if (!is_leaf) {
        pte.words[0] = PTE_CREATE_NEXT(phys_addr);
    }
    else {
        if (pte_type == PTE_L3) {
            pte.words[0] = USER_PTE_CREATE_L3_LEAF(phys_addr);
        }
        else if (pte_type == PTE_L2) {
            pte.words[0] = USER_PTE_CREATE_L2_LEAF(phys_addr);
        }
        else {
            pte.words[0] = USER_PTE_CREATE_L1_LEAF(phys_addr);
        }
    }

    return pte;
}

/* ==================== BOOT CODE STARTS HERE ==================== */

BOOT_CODE void map_kernel_frame(paddr_t paddr, pptr_t vaddr, vm_rights_t vm_rights)
{
    if (vaddr >= KDEV_BASE) {
    /* Map devices in 2nd-level page table */
        paddr = ROUND_DOWN(paddr, LA_GET_LVL_PGSIZE_BITS(2));
        assert((paddr % LA_GET_LVL_PGSIZE(2)) == 0);
        pte_t pte_dev_uncache_plv0;
        pte_dev_uncache_plv0.words[0]=(word_t)(PTE_L2_PA(paddr) | PTE_H_GSRWXV_UNCACHE_PLV0);
        kernel_devices_pt[LA_GET_PT_INDEX(vaddr, 2)] = pte_dev_uncache_plv0;
    }
    else {
        paddr = ROUND_DOWN(paddr, LA_GET_LVL_PGSIZE_BITS(1));
        assert((paddr % LA_GET_LVL_PGSIZE(1)) == 0);
        kernel_l1pt[LA_GET_PT_INDEX(vaddr, 1)] = pte_next(paddr, true, PTE_L1);
    }
}

BOOT_CODE VISIBLE void map_kernel_window(void)
{
    /* mapping of KERNEL_ELF_BASE (virtual address) to kernel's
     * KERNEL_ELF_PHYS_BASE  */
    assert(CONFIG_PT_LEVELS > 1 && CONFIG_PT_LEVELS <= 4);

    /* kernel window starts at PPTR_BASE */
    word_t pptr = PPTR_BASE;

    /* first we map in memory from PADDR_BASE */
    word_t paddr = PADDR_BASE;

    word_t index = LA_GET_PT_INDEX(pptr, 1);
    while (pptr < PPTR_TOP) {
        assert(IS_ALIGNED(pptr, LA_GET_LVL_PGSIZE_BITS(1)));
        assert(IS_ALIGNED(paddr, LA_GET_LVL_PGSIZE_BITS(1)));

        kernel_l1pt[LA_GET_PT_INDEX(pptr, 1)] = pte_next(kpptr_to_paddr(kernel_l2pt[index++]), false, PTE_NONE);

        pptr += LA_GET_LVL_PGSIZE(1);
        paddr += LA_GET_LVL_PGSIZE(1);
    }

    assert(pptr == PPTR_TOP);

    pptr = PPTR_BASE;
    paddr = PADDR_BASE;

    index = LA_GET_PT_INDEX(pptr, 1);
    while (pptr < PPTR_TOP) {
        assert(IS_ALIGNED(pptr, LA_GET_LVL_PGSIZE_BITS(1)));
        assert(IS_ALIGNED(paddr, LA_GET_LVL_PGSIZE_BITS(1)));
        word_t pptr_end = pptr + LA_GET_LVL_PGSIZE(1);
        while (pptr < pptr_end) {
            assert(IS_ALIGNED(pptr, LA_GET_LVL_PGSIZE_BITS(2)));
            assert(IS_ALIGNED(paddr, LA_GET_LVL_PGSIZE_BITS(2)));

            kernel_l2pt[index][LA_GET_PT_INDEX(pptr, 2)] = pte_next(paddr, true, PTE_L2);

            pptr += LA_GET_LVL_PGSIZE(2);
            paddr += LA_GET_LVL_PGSIZE(2);
        }
        index++;
    }

    /* 64GB mapping */
    // word_t index = LA_GET_PT_INDEX(pptr, 1);
    // while (pptr < PPTR_TOP) {
    //     assert(IS_ALIGNED(pptr, LA_GET_LVL_PGSIZE_BITS(1)));
    //     assert(IS_ALIGNED(paddr, LA_GET_LVL_PGSIZE_BITS(1)));

    //     kernel_l1pt[LA_GET_PT_INDEX(pptr, 1)] = pte_next(paddr, true, PTE_L1);

    //     pptr += LA_GET_LVL_PGSIZE(1);
    //     paddr += LA_GET_LVL_PGSIZE(1);
    // }
    
    assert(pptr == PPTR_TOP);

    /* now we should be mapping the 1GiB kernel base */
    pptr = ROUND_DOWN(KERNEL_ELF_BASE, 25);
    paddr = ROUND_DOWN(KERNEL_ELF_PADDR_BASE, 25);

    /* The kernel image is mapped twice */
    kernel_l1pt[LA_GET_PT_INDEX(pptr, 1)] =
        pte_next(kpptr_to_paddr(kernel_image_pt), false, PTE_NONE);
    
    index = LA_GET_PT_INDEX(pptr, 2);
    while (pptr < PPTR_TOP + BIT(30)) {
        kernel_image_pt[index] = pte_next(paddr, true, PTE_L2);
        index++;
        pptr += LA_GET_LVL_PGSIZE(2);
        paddr += LA_GET_LVL_PGSIZE(2);
    }

    /* Map kernel device page table */
    kernel_l1pt[LA_GET_PT_INDEX(KDEV_BASE, 1)] =
        pte_next(kpptr_to_paddr(kernel_devices_pt), false, PTE_NONE);

    map_kernel_devices();
}

BOOT_CODE void map_it_pt_cap(cap_t vspace_cap, cap_t pt_cap)
{
    lookupPTSlot_ret_t pt_ret;
    pte_t *targetSlot;
    vptr_t vptr = cap_page_table_cap_get_capPTMappedAddress(pt_cap);
    pte_t *lvl1pt = PTE_PTR(pptr_of_cap(vspace_cap));

    /* pt to be mapped */
    pte_t *pt = PTE_PTR(pptr_of_cap(pt_cap));

    /* Get PT slot to install the address in */
    pt_ret = lookupPTSlot(lvl1pt, vptr);

    targetSlot = pt_ret.ptSlot;

    // if (!pt_ret.ptLevel) {
    //     *targetSlot = pte_next(addrFromPPtr(pt), true, PTE_L3);
    // } else {
    //     *targetSlot = pte_next(addrFromPPtr(pt), false, PTE_NONE);
    // }
    *targetSlot = user_pte_next(addrFromPPtr(pt), false, PTE_NONE);

    // sfence();//TODO 虚拟内存屏障指令
}

BOOT_CODE void map_it_frame_cap(cap_t vspace_cap, cap_t frame_cap)
{
    pte_t *lvl1pt = PTE_PTR(pptr_of_cap(vspace_cap));
    pte_t *frame_pptr = PTE_PTR(pptr_of_cap(frame_cap));
    vptr_t frame_vptr = cap_frame_cap_get_capFMappedAddress(frame_cap);

    /* We deal with a frame as 16KiB */
    lookupPTSlot_ret_t lu_ret = lookupPTSlot(lvl1pt, frame_vptr);
    assert(lu_ret.ptBitsLeft == seL4_PageBits);

    pte_t *targetSlot = lu_ret.ptSlot;

    *targetSlot = user_pte_next(addrFromPPtr(frame_pptr), true, PTE_L3);

    // sfence();
}

BOOT_CODE cap_t create_unmapped_it_frame_cap(pptr_t pptr, bool_t use_large)
{
    cap_t cap = cap_frame_cap_new(
                    asidInvalid,                     /* capFMappedASID       */
                    pptr,                            /* capFBasePtr          */
                    0,                               /* capFSize             */
                    0,                               /* capFVMRights         */
                    0,
                    0                                /* capFMappedAddress    */
                );

    return cap;
}

/* Create a page table for the initial thread */
static BOOT_CODE cap_t create_it_pt_cap(cap_t vspace_cap, pptr_t pptr, vptr_t vptr, asid_t asid)
{
    cap_t cap;
    cap = cap_page_table_cap_new(
              asid,   /* capPTMappedASID      */
              pptr,   /* capPTBasePtr         */
              1,      /* capPTIsMapped        */
              vptr    /* capPTMappedAddress   */
          );

    map_it_pt_cap(vspace_cap, cap);
    return cap;
}

BOOT_CODE word_t arch_get_n_paging(v_region_t it_v_reg)
{
    word_t n = 0;
    for (int i = 1; i < CONFIG_PT_LEVELS; i++) {
        n += get_n_paging(it_v_reg, LA_GET_LVL_PGSIZE_BITS(i));
    }
    return n;
}

/* Create an address space for the initial thread.
 * This includes page directory and page tables */
BOOT_CODE cap_t create_it_address_space(cap_t root_cnode_cap, v_region_t it_v_reg)
{
    cap_t      lvl1pt_cap;
    vptr_t     pt_vptr;

    copyGlobalMappings_t(PTE_PTR(rootserver.vspace));

    lvl1pt_cap =
        cap_page_table_cap_new(
            IT_ASID,               /* capPTMappedASID    */
            (word_t) rootserver.vspace,  /* capPTBasePtr       */
            1,                     /* capPTIsMapped      */
            (word_t) rootserver.vspace   /* capPTMappedAddress */
        );

    seL4_SlotPos slot_pos_before = ndks_boot.slot_pos_cur;
    write_slot(SLOT_PTR(pptr_of_cap(root_cnode_cap), seL4_CapInitThreadVSpace), lvl1pt_cap);

    /* create all n level PT caps necessary to cover userland image in 16KiB pages */
    for (int i = 1; i < CONFIG_PT_LEVELS; i++) {

        for (pt_vptr = ROUND_DOWN(it_v_reg.start, LA_GET_LVL_PGSIZE_BITS(i));
             pt_vptr < it_v_reg.end;
             pt_vptr += LA_GET_LVL_PGSIZE(i)) {
            if (!provide_cap(root_cnode_cap,
                             create_it_pt_cap(lvl1pt_cap, it_alloc_paging(), pt_vptr, IT_ASID))
               ) {
                return cap_null_cap_new();
            } 
        }

    }

    seL4_SlotPos slot_pos_after = ndks_boot.slot_pos_cur;
    ndks_boot.bi_frame->userImagePaging = (seL4_SlotRegion) {
        slot_pos_before, slot_pos_after
    };

    return lvl1pt_cap;
}

BOOT_CODE void activate_kernel_vspace(void)
{
    setVSpaceRoot(kpptr_to_paddr(&kernel_l1pt), 0);
}

BOOT_CODE void write_it_asid_pool(cap_t it_ap_cap, cap_t it_lvl1pt_cap)
{
    asid_pool_t *ap = ASID_POOL_PTR(pptr_of_cap(it_ap_cap));
    ap->array[IT_ASID] = PTE_PTR(pptr_of_cap(it_lvl1pt_cap));
    loongarchKSASIDTable[IT_ASID >> asidLowBits] = ap;
}

/* ==================== BOOT CODE FINISHES HERE ==================== */

static findVSpaceForASID_ret_t findVSpaceForASID(asid_t asid)
{
    findVSpaceForASID_ret_t ret;
    asid_pool_t        *poolPtr;
    pde_t     *vspace_root;

    poolPtr = loongarchKSASIDTable[asid >> asidLowBits];
    if (!poolPtr) {
        current_lookup_fault = lookup_fault_invalid_root_new();

        ret.vspace_root = NULL;
        ret.status = EXCEPTION_LOOKUP_FAULT;
        return ret;
    }

    vspace_root = poolPtr->array[asid & MASK(asidLowBits)];
    if (!vspace_root) {
        current_lookup_fault = lookup_fault_invalid_root_new();

        ret.vspace_root = NULL;
        ret.status = EXCEPTION_LOOKUP_FAULT;
        return ret;
    }

    ret.vspace_root = vspace_root;
    ret.status = EXCEPTION_NONE;
    return ret;
}

void copyGlobalMappings(pte_t *newLvl1pt)
{
    unsigned long i;
    pte_t *global_kernel_vspace = kernel_l1pt;

    for (i = LA_GET_PT_INDEX(PPTR_BASE, 1); i < BIT(PT_INDEX_BITS); i++) {
        newLvl1pt[i] = global_kernel_vspace[i];
    }
}

void copyGlobalMappings_t(pte_t *newLvl1pt)
{
    unsigned long i;

    for (i = LA_GET_PT_INDEX(0ull, 1); i < BIT(PT_INDEX_BITS); i++) {
        newLvl1pt[i].words[0] = PTE_CREATE_NEXT(0x0ull);
    }
}

word_t *PURE lookupIPCBuffer(bool_t isReceiver, tcb_t *thread)
{
    word_t w_bufferPtr;
    cap_t bufferCap;
    vm_rights_t vm_rights;

    w_bufferPtr = thread->tcbIPCBuffer;
    bufferCap = TCB_PTR_CTE_PTR(thread, tcbBuffer)->cap;

    if (unlikely(cap_get_capType(bufferCap) != cap_frame_cap)) {
        return NULL;
    }
    if (unlikely(cap_frame_cap_get_capFIsDevice(bufferCap))) {
        return NULL;
    }

    vm_rights = cap_frame_cap_get_capFVMRights(bufferCap);
    if (likely(vm_rights == VMReadWrite ||
               (!isReceiver && vm_rights == VMReadOnly))) {
        word_t basePtr, pageBits;

        basePtr = cap_frame_cap_get_capFBasePtr(bufferCap);
        pageBits = pageBitsForSize(cap_frame_cap_get_capFSize(bufferCap));
        return (word_t *)(basePtr + (w_bufferPtr & MASK(pageBits)));
    } else {
        return NULL;
    }
}

// static inline pte_t *getPPtrFromHWPTE(pte_t *pte)
// {
//     return PTE_PTR(ptrFromPAddr(pte_ptr_get_ppn(pte) << seL4_PageTableBits));
// }

lookupPTSlot_ret_t lookupPTSlot(pte_t *lvl1pt, vptr_t vptr)
{
    lookupPTSlot_ret_t ret;

    word_t level = CONFIG_PT_LEVELS - 1;
    pte_t *pt = lvl1pt;
    pte_t *save_ptSlot = lvl1pt;

    /* this is how many bits we potentially have left to decode. Initially we have the
     * full address space to decode, and every time we walk this will be reduced. The
     * final value of this after the walk is the size of the frame that can be inserted,
     * or already exists, in ret.ptSlot. The following formulation is an invariant of
     * the loop: */
    ret.ptBitsLeft = PT_INDEX_BITS * level + seL4_PageBits;
    save_ptSlot = pt + ((vptr >> ret.ptBitsLeft) & MASK(PT_INDEX_BITS));

    while (likely(0 < level) && isPTEPageTable((pte_t *)save_ptSlot)) {
        level--;
        ret.ptBitsLeft -= PT_INDEX_BITS;
        pt = PTE_PTR(paddr_to_pptr(save_ptSlot->words[0]));
        save_ptSlot = pt + ((vptr >> ret.ptBitsLeft) & MASK(PT_INDEX_BITS));
    }

    ret.ptSlot = (pte_t *)save_ptSlot;
    ret.ptLevel = level;

    return ret;
}

exception_t handleVMFault(tcb_t *thread, vm_fault_type_t vm_faultType)
{
    uint64_t addr;
    addr = read_csr_badv();

    switch (vm_faultType)
    {
        case LALoadPageInvalid:     //PIL
            current_fault = seL4_Fault_VMFault_new(addr, LALoadPageInvalid, false);
            return EXCEPTION_FAULT;
        case LAStorePageInvalid:    //PIS
            if (handle_tlb_store()) {
                current_fault = seL4_Fault_VMFault_new(addr, LAStorePageInvalid, false);
                return EXCEPTION_FAULT;
            } else {
                return EXCEPTION_NONE;
            }
        case LAFetchPageInvalid:    //PIF
            current_fault = seL4_Fault_VMFault_new(addr, LAFetchPageInvalid, false);
            return EXCEPTION_FAULT;
        case LAPageModException:    //PME
            current_fault = seL4_Fault_VMFault_new(addr, LAPageModException, false);
            return EXCEPTION_FAULT;
        case LAPageNoReadable:      //PNR
            current_fault = seL4_Fault_VMFault_new(addr, LAPageNoReadable, false);
            return EXCEPTION_FAULT;
        case LAPageNoExecutable:    //PNX
            current_fault = seL4_Fault_VMFault_new(addr, LAFetchPageInvalid, false);
            return EXCEPTION_FAULT;
        case LAPagePrivilegeIllegal://PPI
            current_fault = seL4_Fault_VMFault_new(addr, LAPagePrivilegeIllegal, false);
            return EXCEPTION_FAULT;
        case LAAddrError:           //ADEF or ADEM
            current_fault = seL4_Fault_VMFault_new(addr, LAAddrError, false);
            return EXCEPTION_FAULT;
        case LAAddrAlignFault:      //ALE
            current_fault = seL4_Fault_VMFault_new(addr, LAAddrAlignFault, false);
            return EXCEPTION_FAULT;
        case LABoundCheck:          //BCE
            current_fault = seL4_Fault_VMFault_new(addr, LABoundCheck, false);
            return EXCEPTION_FAULT;
        default:
            fail("Invalid VM fault type\n");
    }
}

void deleteASIDPool(asid_t asid_base, asid_pool_t *pool)
{
    /* Haskell error: "ASID pool's base must be aligned" */
    assert(IS_ALIGNED(asid_base, asidLowBits));

    if (loongarchKSASIDTable[asid_base >> asidLowBits] == pool) {
        loongarchKSASIDTable[asid_base >> asidLowBits] = NULL;
        setVMRoot(NODE_STATE(ksCurThread));
    }
}

static exception_t performASIDControlInvocation(void *frame, cte_t *slot, cte_t *parent, asid_t asid_base)
{
    /** AUXUPD: "(True, typ_region_bytes (ptr_val \<acute>frame) 12)" */
    /** GHOSTUPD: "(True, gs_clear_region (ptr_val \<acute>frame) 12)" */
    cap_untyped_cap_ptr_set_capFreeIndex(&(parent->cap),
                                         MAX_FREE_INDEX(cap_untyped_cap_get_capBlockSize(parent->cap)));

    memzero(frame, BIT(pageBitsForSize(LOONGARCH_16K_Page)));
    /** AUXUPD: "(True, ptr_retyps 1 (Ptr (ptr_val \<acute>frame) :: asid_pool_C ptr))" */

    cteInsert(
        cap_asid_pool_cap_new(
            asid_base,          /* capASIDBase  */
            WORD_REF(frame)     /* capASIDPool  */
        ),
        parent,
        slot
    );
    /* Haskell error: "ASID pool's base must be aligned" */
    assert((asid_base & MASK(asidLowBits)) == 0);
    loongarchKSASIDTable[asid_base >> asidLowBits] = (asid_pool_t *)frame;

    return EXCEPTION_NONE;
}

static exception_t performASIDPoolInvocation(asid_t asid, asid_pool_t *poolPtr, cte_t *vspaceCapSlot)
{
    cap_t cap = vspaceCapSlot->cap;
    pte_t *regionBase = PTE_PTR(cap_page_table_cap_get_capPTBasePtr(cap));
    cap = cap_page_table_cap_set_capPTMappedASID(cap, asid);
    cap = cap_page_table_cap_set_capPTMappedAddress(cap, 0);
    cap = cap_page_table_cap_set_capPTIsMapped(cap, 1);
    vspaceCapSlot->cap = cap;

    copyGlobalMappings_t(regionBase);

    poolPtr->array[asid & MASK(asidLowBits)] = regionBase;

    return EXCEPTION_NONE;
}

void deleteASID(asid_t asid, pte_t *vspace)
{
    asid_pool_t *poolPtr;

    poolPtr = loongarchKSASIDTable[asid >> asidLowBits];
    if (poolPtr != NULL && poolPtr->array[asid & MASK(asidLowBits)] == vspace) {
        // hwASIDFlush(asid);
        invtlb_info(0x4, asid, 0);
        poolPtr->array[asid & MASK(asidLowBits)] = NULL;
        setVMRoot(NODE_STATE(ksCurThread));
    }
}

void unmapPageTable(asid_t asid, vptr_t vptr, pte_t *target_pt)
{
    findVSpaceForASID_ret_t find_ret = findVSpaceForASID(asid);
    if (unlikely(find_ret.status != EXCEPTION_NONE)) {
        /* nothing to do */
        return;
    }
    /* We won't ever unmap a top level page table */
    assert(find_ret.vspace_root != target_pt);
    pte_t *ptSlot = NULL;
    pte_t *pt = find_ret.vspace_root;

    for (word_t i = 0; i < CONFIG_PT_LEVELS - 1 && pt != target_pt; i++) {
        ptSlot = pt + LA_GET_PT_INDEX(vptr, i);
        if (unlikely(!isPTEPageTable(ptSlot))) {
            /* couldn't find it */
            return;
        }
        pt = PTE_PTR(paddr_to_pptr(ptSlot->words[0]));
    }

    if (pt != target_pt) {
        /* didn't find it */
        return;
    }
    /* If we found a pt then ptSlot won't be null */
    assert(ptSlot != NULL);
    (*ptSlot).words[0] = 0ull;
    // sfence();
    // invtlb(0x5, asid, vptr);
}

static pte_t pte_pte_invalid_new(void)
{
    return (pte_t) {
        0
    };
}

void unmapPage(vm_page_size_t page_size, asid_t asid, vptr_t vptr, pptr_t pptr)
{
    findVSpaceForASID_ret_t find_ret;
    lookupPTSlot_ret_t  lu_ret;

    find_ret = findVSpaceForASID(asid);
    if (find_ret.status != EXCEPTION_NONE) {
        return;
    }

    lu_ret = lookupPTSlot(find_ret.vspace_root, vptr);
    if (unlikely(lu_ret.ptBitsLeft != pageBitsForSize(page_size))) {
        return;
    }
    if (!pte_ptr_get_valid(lu_ret.ptSlot) || isPTEPageTable(lu_ret.ptSlot)
        || (pte_ptr_get_ppn(lu_ret.ptSlot) << seL4_PageBits) != pptr_to_paddr((void *)pptr)) {
        return;
    }

    lu_ret.ptSlot[0] = pte_pte_invalid_new();
    invtlb(0x5, asid, vptr);
}

void setVMRoot(tcb_t *tcb)
{
    cap_t threadRoot;
    asid_t asid;
    pte_t *lvl1pt;
    findVSpaceForASID_ret_t  find_ret;

    threadRoot = TCB_PTR_CTE_PTR(tcb, tcbVTable)->cap;

    if (cap_get_capType(threadRoot) != cap_page_table_cap) {
        return;
    }

    lvl1pt = PTE_PTR(cap_page_table_cap_get_capPTBasePtr(threadRoot));

    asid = cap_page_table_cap_get_capPTMappedASID(threadRoot);
    find_ret = findVSpaceForASID(asid);
    if (unlikely(find_ret.status != EXCEPTION_NONE || find_ret.vspace_root != lvl1pt)) {
        return;
    }

    setVSpaceRoot(addrFromPPtr(lvl1pt), asid);
}

bool_t CONST isValidVTableRoot(cap_t cap)
{
    return (cap_get_capType(cap) == cap_page_table_cap &&
            cap_page_table_cap_get_capPTIsMapped(cap));
}

exception_t checkValidIPCBuffer(vptr_t vptr, cap_t cap)
{
    if (unlikely(cap_get_capType(cap) != cap_frame_cap)) {
        userError("Requested IPC Buffer is not a frame cap.");
        current_syscall_error.type = seL4_IllegalOperation;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (unlikely(cap_frame_cap_get_capFIsDevice(cap))) {
        userError("Specifying a device frame as an IPC buffer is not permitted.");
        current_syscall_error.type = seL4_IllegalOperation;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (unlikely(!IS_ALIGNED(vptr, seL4_IPCBufferSizeBits))) {
        userError("Requested IPC Buffer location 0x%x is not aligned.",
                  (int)vptr);
        current_syscall_error.type = seL4_AlignmentError;
        return EXCEPTION_SYSCALL_ERROR;
    }

    return EXCEPTION_NONE;
}

vm_rights_t CONST maskVMRights(vm_rights_t vm_rights, seL4_CapRights_t cap_rights_mask)
{
    if (vm_rights == VMReadOnly && seL4_CapRights_get_capAllowRead(cap_rights_mask)) {
        return VMReadOnly;
    }
    if (vm_rights == VMReadWrite && seL4_CapRights_get_capAllowRead(cap_rights_mask)) {
        if (!seL4_CapRights_get_capAllowWrite(cap_rights_mask)) {
            return VMReadOnly;
        } else {
            return VMReadWrite;
        }
    }
    return VMKernelOnly;
}

/* The rest of the file implements the LOONGARCH object invocations */

static pte_t CONST makeUserPTE(paddr_t paddr/*, bool_t executable, vm_rights_t vm_rights*/)
{
    return user_pte_next(paddr, true, PTE_L3);
}

static inline bool_t CONST checkVPAlignment(vm_page_size_t sz, word_t w)
{
    return (w & MASK(pageBitsForSize(sz))) == 0;
}

static exception_t decodeLOONGARCHPageTableInvocation(word_t label, word_t length,
                                                  cte_t *cte, cap_t cap, word_t *buffer)
{
    if (label == LOONGARCHPageTableUnmap) {
        if (unlikely(!isFinalCapability(cte))) {
            userError("LOONGARCHPageTableUnmap: cannot unmap if more than once cap exists");
            current_syscall_error.type = seL4_RevokeFirst;
            return EXCEPTION_SYSCALL_ERROR;
        }
        /* Ensure that if the page table is mapped, it is not a top level table */
        if (likely(cap_page_table_cap_get_capPTIsMapped(cap))) {
            asid_t asid = cap_page_table_cap_get_capPTMappedASID(cap);
            findVSpaceForASID_ret_t find_ret = findVSpaceForASID(asid);
            pte_t *pte = PTE_PTR(cap_page_table_cap_get_capPTBasePtr(cap));
            if (unlikely(find_ret.status == EXCEPTION_NONE &&
                         find_ret.vspace_root == pte)) {
                userError("LOONGARCHPageTableUnmap: cannot call unmap on top level PageTable");
                current_syscall_error.type = seL4_RevokeFirst;
                return EXCEPTION_SYSCALL_ERROR;
            }
        }

        setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
        return performPageTableInvocationUnmap(cap, cte);
    }

    if (unlikely((label != LOONGARCHPageTableMap))) {
        userError("LOONGARCHPageTableMap: Illegal Operation");
        current_syscall_error.type = seL4_IllegalOperation;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (unlikely(length < 2 || current_extra_caps.excaprefs[0] == NULL)) {
        userError("LOONGARCHPageTable: truncated message");
        current_syscall_error.type = seL4_TruncatedMessage;
        return EXCEPTION_SYSCALL_ERROR;
    }
    if (unlikely(cap_page_table_cap_get_capPTIsMapped(cap))) {
        userError("LOONGARCHPageTable: PageTable is already mapped.");
        current_syscall_error.type = seL4_InvalidCapability;
        current_syscall_error.invalidCapNumber = 0;
        return EXCEPTION_SYSCALL_ERROR;
    }

    word_t vaddr = getSyscallArg(0, buffer);
    cap_t lvl1ptCap = current_extra_caps.excaprefs[0]->cap;

    if (unlikely(cap_get_capType(lvl1ptCap) != cap_page_table_cap ||
                 cap_page_table_cap_get_capPTIsMapped(lvl1ptCap) == asidInvalid)) {
        userError("LOONGARCHPageTable: Invalid top-level PageTable.");
        current_syscall_error.type = seL4_InvalidCapability;
        current_syscall_error.invalidCapNumber = 1;

        return EXCEPTION_SYSCALL_ERROR;
    }

    pte_t *lvl1pt = PTE_PTR(cap_page_table_cap_get_capPTBasePtr(lvl1ptCap));
    asid_t asid = cap_page_table_cap_get_capPTMappedASID(lvl1ptCap);

    if (unlikely(vaddr >= USER_TOP)) {
        userError("LOONGARCHPageTableMap: Virtual address cannot be in kernel window.");
        current_syscall_error.type = seL4_InvalidArgument;
        current_syscall_error.invalidArgumentNumber = 0;

        return EXCEPTION_SYSCALL_ERROR;
    }

    findVSpaceForASID_ret_t find_ret = findVSpaceForASID(asid);
    if (unlikely(find_ret.status != EXCEPTION_NONE)) {
        userError("LOONGARCHPageTableMap: ASID lookup failed");
        current_syscall_error.type = seL4_FailedLookup;
        current_syscall_error.failedLookupWasSource = false;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (unlikely(find_ret.vspace_root != lvl1pt)) {
        userError("LOONGARCHPageTableMap: ASID lookup failed");
        current_syscall_error.type = seL4_InvalidCapability;
        current_syscall_error.invalidCapNumber = 1;
        return EXCEPTION_SYSCALL_ERROR;
    }

    lookupPTSlot_ret_t lu_ret = lookupPTSlot(lvl1pt, vaddr);

    /* if there is already something mapped (valid is set) or we have traversed far enough
     * that a page table is not valid to map then tell the user that they ahve to delete
     * something before they can put a PT here */
    if (lu_ret.ptBitsLeft == seL4_PageBits || pte_ptr_get_valid(lu_ret.ptSlot)) {
        userError("LOONGARCHPageTableMap: All objects mapped at this address");
        current_syscall_error.type = seL4_DeleteFirst;
        return EXCEPTION_SYSCALL_ERROR;
    }

    /* Get the slot to install the PT in */
    pte_t *ptSlot = lu_ret.ptSlot;

    paddr_t paddr = addrFromPPtr(
                        PTE_PTR(cap_page_table_cap_get_capPTBasePtr(cap)));
    pte_t pte = user_pte_next(paddr, false, PTE_NONE);

    cap = cap_page_table_cap_set_capPTIsMapped(cap, 1);
    cap = cap_page_table_cap_set_capPTMappedASID(cap, asid);
    cap = cap_page_table_cap_set_capPTMappedAddress(cap, (vaddr & ~MASK(lu_ret.ptBitsLeft)));

    setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
    return performPageTableInvocationMap(cap, cte, pte, ptSlot);
}

static exception_t decodeLOONGARCHFrameInvocation(word_t label, word_t length,
                                              cte_t *cte, cap_t cap, word_t *buffer)
{
    switch (label) {
    case LOONGARCHPageMap: {
        if (unlikely(length < 3 || current_extra_caps.excaprefs[0] == NULL)) {
            userError("LOONGARCHPageMap: Truncated message.");
            current_syscall_error.type = seL4_TruncatedMessage;
            return EXCEPTION_SYSCALL_ERROR;
        }

        word_t vaddr = getSyscallArg(0, buffer);
        // word_t w_rightsMask = getSyscallArg(1, buffer);
        // vm_attributes_t attr = vmAttributesFromWord(getSyscallArg(2, buffer));
        cap_t lvl1ptCap = current_extra_caps.excaprefs[0]->cap;

        vm_page_size_t frameSize = cap_frame_cap_get_capFSize(cap);
        // vm_rights_t capVMRights = cap_frame_cap_get_capFVMRights(cap);

        if (unlikely(cap_get_capType(lvl1ptCap) != cap_page_table_cap ||
                     !cap_page_table_cap_get_capPTIsMapped(lvl1ptCap))) {
            userError("LOONGARCHPageMap: Bad PageTable cap.");
            current_syscall_error.type = seL4_InvalidCapability;
            current_syscall_error.invalidCapNumber = 1;
            return EXCEPTION_SYSCALL_ERROR;
        }

        pte_t *lvl1pt = PTE_PTR(cap_page_table_cap_get_capPTBasePtr(lvl1ptCap));
        asid_t asid = cap_page_table_cap_get_capPTMappedASID(lvl1ptCap);

        findVSpaceForASID_ret_t find_ret = findVSpaceForASID(asid);
        if (unlikely(find_ret.status != EXCEPTION_NONE)) {
            userError("LOONGARCHPageMap: No PageTable for ASID");
            current_syscall_error.type = seL4_FailedLookup;
            current_syscall_error.failedLookupWasSource = false;
            return EXCEPTION_SYSCALL_ERROR;
        }

        if (unlikely(find_ret.vspace_root != lvl1pt)) {
            userError("LOONGARCHPageMap: ASID lookup failed");
            current_syscall_error.type = seL4_InvalidCapability;
            current_syscall_error.invalidCapNumber = 1;
            return EXCEPTION_SYSCALL_ERROR;
        }

        /* check the vaddr is valid */
        word_t vtop = vaddr + BIT(pageBitsForSize(frameSize)) - 1;
        if (unlikely(vtop >= USER_TOP)) {
            current_syscall_error.type = seL4_InvalidArgument;
            current_syscall_error.invalidArgumentNumber = 0;
            return EXCEPTION_SYSCALL_ERROR;
        }
        if (unlikely(!checkVPAlignment(frameSize, vaddr))) {
            current_syscall_error.type = seL4_AlignmentError;
            return EXCEPTION_SYSCALL_ERROR;
        }

        /* Check if this page is already mapped */
        lookupPTSlot_ret_t lu_ret = lookupPTSlot(lvl1pt, vaddr);
        if (unlikely(lu_ret.ptBitsLeft != pageBitsForSize(frameSize))) {
            current_lookup_fault = lookup_fault_missing_capability_new(lu_ret.ptBitsLeft);
            current_syscall_error.type = seL4_FailedLookup;
            current_syscall_error.failedLookupWasSource = false;
            return EXCEPTION_SYSCALL_ERROR;
        }

        asid_t frame_asid = cap_frame_cap_get_capFMappedASID(cap);
        if (unlikely(frame_asid != asidInvalid)) {
            /* this frame is already mapped */
            if (frame_asid != asid) {
                userError("LOONGARCHPageMap: Attempting to remap a frame that does not belong to the passed address space");
                current_syscall_error.type = seL4_InvalidCapability;
                current_syscall_error.invalidCapNumber = 1;
                return EXCEPTION_SYSCALL_ERROR;
            }
            word_t mapped_vaddr = cap_frame_cap_get_capFMappedAddress(cap);
            if (unlikely(mapped_vaddr != vaddr)) {
                userError("LOONGARCHPageMap: attempting to map frame into multiple addresses");
                current_syscall_error.type = seL4_InvalidArgument;
                current_syscall_error.invalidArgumentNumber = 0;
                return EXCEPTION_SYSCALL_ERROR;
            }
            /* this check is redundant, as lookupPTSlot does not stop on a page
             * table PTE */
            if (unlikely(isPTEPageTable(lu_ret.ptSlot))) {
                userError("LOONGARCHPageMap: no mapping to remap.");
                current_syscall_error.type = seL4_DeleteFirst;
                return EXCEPTION_SYSCALL_ERROR;
            }
        } else {
            /* check this vaddr isn't already mapped */
            if (unlikely(pte_ptr_get_valid(lu_ret.ptSlot))) {
                printf("lu_ret.ptSlot: %p", lu_ret.ptSlot);
                userError("Virtual address already mapped");
                current_syscall_error.type = seL4_DeleteFirst;
                return EXCEPTION_SYSCALL_ERROR;
            }
        }

        // vm_rights_t vmRights = maskVMRights(capVMRights, rightsFromWord(w_rightsMask));
        paddr_t frame_paddr = addrFromPPtr((void *) cap_frame_cap_get_capFBasePtr(cap));
        cap = cap_frame_cap_set_capFMappedASID(cap, asid);
        cap = cap_frame_cap_set_capFMappedAddress(cap,  vaddr);

        pte_t pte = makeUserPTE(frame_paddr/*, executable, vmRights*/);
        // bool_t executable = !vm_attributes_get_riscvExecuteNever(attr);

        // pte_t pte = makeUserPTE(frame_paddr, executable, vmRights);
        
        setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
        return performPageInvocationMapPTE(cap, cte, pte, lu_ret.ptSlot);
    }

    case LOONGARCHPageUnmap: {
        setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
        return performPageInvocationUnmap(cap, cte);
    }

    case LOONGARCHPageGetAddress: {

        /* Check that there are enough message registers */
        assert(n_msgRegisters >= 1);

        setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
        return performPageGetAddress((void *)cap_frame_cap_get_capFBasePtr(cap));
    }

    default:
        userError("LOONGARCHPage: Illegal operation.");
        current_syscall_error.type = seL4_IllegalOperation;

        return EXCEPTION_SYSCALL_ERROR;
    }

}

exception_t decodeLOONGARCHMMUInvocation(word_t label, word_t length, cptr_t cptr,
                                     cte_t *cte, cap_t cap, word_t *buffer)
{
    uint64_t tmpvalue=cap_get_capType(cap);
    // printf("%llu",tmpvalue);
    switch (tmpvalue) {

    case cap_page_table_cap:
        return decodeLOONGARCHPageTableInvocation(label, length, cte, cap, buffer);

    case cap_frame_cap:
        return decodeLOONGARCHFrameInvocation(label, length, cte, cap, buffer);

    case cap_asid_control_cap: {
        word_t     i;
        asid_t           asid_base;
        word_t           index;
        word_t           depth;
        cap_t            untyped;
        cap_t            root;
        cte_t           *parentSlot;
        cte_t           *destSlot;
        lookupSlot_ret_t lu_ret;
        void            *frame;
        exception_t      status;

        if (label != LOONGARCHASIDControlMakePool) {
            current_syscall_error.type = seL4_IllegalOperation;

            return EXCEPTION_SYSCALL_ERROR;
        }

        if (length < 2 || current_extra_caps.excaprefs[0] == NULL
            || current_extra_caps.excaprefs[1] == NULL) {
            current_syscall_error.type = seL4_TruncatedMessage;
            return EXCEPTION_SYSCALL_ERROR;
        }

        index = getSyscallArg(0, buffer);
        depth = getSyscallArg(1, buffer);
        parentSlot = current_extra_caps.excaprefs[0];
        untyped = parentSlot->cap;
        root = current_extra_caps.excaprefs[1]->cap;

        /* Find first free pool */
        for (i = 0; i < nASIDPools && loongarchKSASIDTable[i]; i++);

        if (i == nASIDPools) {
            /* no unallocated pool is found */
            current_syscall_error.type = seL4_DeleteFirst;

            return EXCEPTION_SYSCALL_ERROR;
        }

        asid_base = i << asidLowBits;

        if (cap_get_capType(untyped) != cap_untyped_cap ||
            cap_untyped_cap_get_capBlockSize(untyped) != seL4_ASIDPoolBits ||
            cap_untyped_cap_get_capIsDevice(untyped)) {
            current_syscall_error.type = seL4_InvalidCapability;
            current_syscall_error.invalidCapNumber = 1;

            return EXCEPTION_SYSCALL_ERROR;
        }

        status = ensureNoChildren(parentSlot);
        if (status != EXCEPTION_NONE) {
            return status;
        }

        frame = WORD_PTR(cap_untyped_cap_get_capPtr(untyped));

        lu_ret = lookupTargetSlot(root, index, depth);
        if (lu_ret.status != EXCEPTION_NONE) {
            return lu_ret.status;
        }
        destSlot = lu_ret.slot;

        status = ensureEmptySlot(destSlot);
        if (status != EXCEPTION_NONE) {
            return status;
        }

        setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
        return performASIDControlInvocation(frame, destSlot, parentSlot, asid_base);
    }

    case cap_asid_pool_cap: {
        cap_t        vspaceCap;
        cte_t       *vspaceCapSlot;
        asid_pool_t *pool;
        word_t i;
        asid_t       asid;

        if (label != LOONGARCHASIDPoolAssign) {
            current_syscall_error.type = seL4_IllegalOperation;

            return EXCEPTION_SYSCALL_ERROR;
        }
        if (current_extra_caps.excaprefs[0] == NULL) {
            current_syscall_error.type = seL4_TruncatedMessage;

            return EXCEPTION_SYSCALL_ERROR;
        }

        vspaceCapSlot = current_extra_caps.excaprefs[0];
        vspaceCap = vspaceCapSlot->cap;

        if (unlikely(
                cap_get_capType(vspaceCap) != cap_page_table_cap ||
                cap_page_table_cap_get_capPTIsMapped(vspaceCap))) {
            userError("LOONGARCHASIDPool: Invalid vspace root.");
            current_syscall_error.type = seL4_InvalidCapability;
            current_syscall_error.invalidCapNumber = 1;

            return EXCEPTION_SYSCALL_ERROR;
        }

        pool = loongarchKSASIDTable[cap_asid_pool_cap_get_capASIDBase(cap) >> asidLowBits];
        if (!pool) {
            current_syscall_error.type = seL4_FailedLookup;
            current_syscall_error.failedLookupWasSource = false;
            current_lookup_fault = lookup_fault_invalid_root_new();
            return EXCEPTION_SYSCALL_ERROR;
        }

        if (pool != ASID_POOL_PTR(cap_asid_pool_cap_get_capASIDPool(cap))) {
            current_syscall_error.type = seL4_InvalidCapability;
            current_syscall_error.invalidCapNumber = 0;
            return EXCEPTION_SYSCALL_ERROR;
        }

        /* Find first free ASID */
        asid = cap_asid_pool_cap_get_capASIDBase(cap);
        for (i = 0; i < BIT(asidLowBits) && (asid + i == 0 || pool->array[i]); i++);

        if (i == BIT(asidLowBits)) {
            current_syscall_error.type = seL4_DeleteFirst;

            return EXCEPTION_SYSCALL_ERROR;
        }

        asid += i;

        setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
        return performASIDPoolInvocation(asid, pool, vspaceCapSlot);
    }
    default:
        fail("Invalid arch cap type");
    }
}

exception_t performPageTableInvocationMap(cap_t cap, cte_t *ctSlot,
                                          pte_t pte, pte_t *ptSlot)
{
    ctSlot->cap = cap;
    *ptSlot = pte;
    // sfence();

    return EXCEPTION_NONE;
}

exception_t performPageTableInvocationUnmap(cap_t cap, cte_t *ctSlot)
{
    if (cap_page_table_cap_get_capPTIsMapped(cap)) {
        pte_t *pt = PTE_PTR(cap_page_table_cap_get_capPTBasePtr(cap));
        unmapPageTable(
            cap_page_table_cap_get_capPTMappedASID(cap),
            cap_page_table_cap_get_capPTMappedAddress(cap),
            pt
        );
        clearMemory((void *)pt, seL4_PageTableBits);
    }
    cap_page_table_cap_ptr_set_capPTIsMapped(&(ctSlot->cap), 0);

    return EXCEPTION_NONE;
}

static exception_t performPageGetAddress(void *vbase_ptr)
{
    // paddr_t capFBasePtr;

    // /* Get the physical address of this frame. */
    // capFBasePtr = addrFromPPtr(vbase_ptr);

    // /* return it in the first message register */
    // setRegister(NODE_STATE(ksCurThread), msgRegisters[0], capFBasePtr);
    // setRegister(NODE_STATE(ksCurThread), msgInfoRegister,
    //             wordFromMessageInfo(seL4_MessageInfo_new(0, 0, 0, 1)));
    assert(0);
    return EXCEPTION_NONE;
}

static exception_t updatePTE(pte_t pte, pte_t *base)
{
    *base = pte;
    // sfence();
    asm volatile("dbar 0" ::: "memory");
    return EXCEPTION_NONE;
}

exception_t performPageInvocationMapPTE(cap_t cap, cte_t *ctSlot,
                                        pte_t pte, pte_t *base)
{
    ctSlot->cap = cap;
    return updatePTE(pte, base);
}

exception_t performPageInvocationUnmap(cap_t cap, cte_t *ctSlot)
{
    if (cap_frame_cap_get_capFMappedASID(cap) != asidInvalid) {
        unmapPage(cap_frame_cap_get_capFSize(cap),
                  cap_frame_cap_get_capFMappedASID(cap),
                  cap_frame_cap_get_capFMappedAddress(cap),
                  cap_frame_cap_get_capFBasePtr(cap)
                 );
    }

    cap_t slotCap = ctSlot->cap;
    slotCap = cap_frame_cap_set_capFMappedAddress(slotCap, 0);
    slotCap = cap_frame_cap_set_capFMappedASID(slotCap, asidInvalid);
    ctSlot->cap = slotCap;

    return EXCEPTION_NONE;
}

#ifdef CONFIG_PRINTING
void Arch_userStackTrace(tcb_t *tptr)
{
    assert(0);
    // cap_t threadRoot = TCB_PTR_CTE_PTR(tptr, tcbVTable)->cap;
    // if (!isValidVTableRoot(threadRoot)) {
    //     printf("Invalid vspace\n");
    //     return;
    // }

    // word_t sp = getRegister(tptr, SP);
    // if (!IS_ALIGNED(sp, seL4_WordSizeBits)) {
    //     printf("SP %p not aligned", (void *) sp);
    //     return;
    // }

    // pte_t *vspace_root = PTE_PTR(pptr_of_cap(threadRoot));
    // for (int i = 0; i < CONFIG_USER_STACK_TRACE_LENGTH; i++) {
    //     word_t address = sp + (i * sizeof(word_t));
    //     lookupPTSlot_ret_t ret = lookupPTSlot(vspace_root, address);
    //     if (pte_ptr_get_valid(ret.ptSlot) && !isPTEPageTable(ret.ptSlot)) {
    //         pptr_t pptr = (pptr_t)(getPPtrFromHWPTE(ret.ptSlot));
    //         word_t *value = (word_t *)((word_t)pptr + (address & MASK(ret.ptBitsLeft)));
    //         printf("0x%lx: 0x%lx\n", (long) address, (long) *value);
    //     } else {
    //         printf("0x%lx: INVALID\n", (long) address);
    //     }
    // }
}
#endif

// #ifdef CONFIG_KERNEL_LOG_BUFFER
// exception_t benchmark_arch_map_logBuffer(word_t frame_cptr)
// {
//     lookupCapAndSlot_ret_t lu_ret;
//     vm_page_size_t frameSize;
//     pptr_t  frame_pptr;

//     /* faulting section */
//     lu_ret = lookupCapAndSlot(NODE_STATE(ksCurThread), frame_cptr);

//     if (unlikely(lu_ret.status != EXCEPTION_NONE)) {
//         userError("Invalid cap #%lu.", frame_cptr);
//         current_fault = seL4_Fault_CapFault_new(frame_cptr, false);

//         return EXCEPTION_SYSCALL_ERROR;
//     }

//     if (cap_get_capType(lu_ret.cap) != cap_frame_cap) {
//         userError("Invalid cap. Log buffer should be of a frame cap");
//         current_fault = seL4_Fault_CapFault_new(frame_cptr, false);

//         return EXCEPTION_SYSCALL_ERROR;
//     }

//     frameSize = cap_frame_cap_get_capFSize(lu_ret.cap);

//     if (frameSize != LOONGARCH_Mega_Page) {
//         userError("Invalid frame size. The kernel expects large page log buffer");
//         current_fault = seL4_Fault_CapFault_new(frame_cptr, false);

//         return EXCEPTION_SYSCALL_ERROR;
//     }

//     frame_pptr = cap_frame_cap_get_capFBasePtr(lu_ret.cap);

//     ksUserLogBuffer = pptr_to_paddr((void *) frame_pptr);

// #if __loongarch_xlen == 32
//     paddr_t physical_address = ksUserLogBuffer;
//     for (word_t i = 0; i < BIT(PT_INDEX_BITS); i += 1) {
//         kernel_image_level2_log_buffer_pt[i] = pte_next(physical_address, true);
//         physical_address += BIT(PAGE_BITS);
//     }
//     assert(physical_address - ksUserLogBuffer == BIT(seL4_LargePageBits));
// #else
//     kernel_image_level2_dev_pt[LOONGARCH_GET_PT_INDEX(KS_LOG_PPTR, 1)] = pte_next(ksUserLogBuffer, true);
// #endif

//     sfence();

//     return EXCEPTION_NONE;
// }
// #endif /* CONFIG_KERNEL_LOG_BUFFER */
