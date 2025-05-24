#define CONFIG_ENABLE_PREFETCHER 1


#ifdef CONFIG_ENABLE_PREFETCHER

static inline void tlb_prefetch(paddr_t PGD, vptr_t vaddr) {
    /* complete pte info */
    csr_writeq(vaddr, LOONGARCH_CSR_TLBRBADV);
    asm volatile (
        "move   $t0, %[pgd_addr]\n"
        "lddir  $t0, $t0, 3\n"
        "lddir	$t0, $t0, 1\n"
        "ldpte	$t0, 0\n"
	    "ldpte	$t0, 1\n"
        :
        : [pgd_addr] "r" (PGD)
        : "t0"
    );
    csr_writeq(csr_readq(LOONGARCH_CSR_TLBRELO0), LOONGARCH_CSR_TLBELO0);
    csr_writeq(csr_readq(LOONGARCH_CSR_TLBRELO1), LOONGARCH_CSR_TLBELO1);
    csr_writeq(vaddr, LOONGARCH_CSR_TLBEHI);
    asm volatile (
        "tlbfill\n"
    );
}

#endif