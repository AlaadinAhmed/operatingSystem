#pragma once
#include <stdint.h>
void find_nvme_devices();
struct base_addr_reg {
    uint64_t cap;
    uint32_t vs;
    uint32_t intms;
    uint32_t intmc;
    uint32_t cc;
    uint32_t reserved0;
    uint32_t csts;
    uint32_t aqa;
    uint64_t asq;
    uint64_t acq;
} __attribute__((packed));
void init_base_add_reg();
