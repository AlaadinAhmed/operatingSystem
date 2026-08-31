#include "fs/vfs.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "proc/elf.h"
#include <cstdint>
#include <cstring>

bool load_elf_binary(vfs_node *file, uint64_t *new_plm4_phys, uint64_t *out_entry) {
    Elf32_Ehdr *header;
    if (file->read(file, 0, sizeof(Elf32_Ehdr), (uint8_t *)&header) < (int64_t)sizeof(Elf32_Ehdr)) {
        return false;
    }
    if (*(uint32_t *)header->e_ident != ELFMAGIC) {
        return false;
    }
    // if (header->e_ident[4] == ELFCLASS32) {
    //     // TO DO HANDLE 32-BIT
    // } else if (header->e_ident[4] == ELFCLASS64) {
    //     // TO DO HANDLE 64-BIT
    // } else {
    //     // TO DO HANDLE INVALID CLASS
    // }

    for (uint16_t i; i < header->e_phnum; i++) {
        Elf32_Phdr program_header;
        uint32_t   offset = header->e_phoff * (i * sizeof(Elf32_Phdr));
        file->read(file, offset, sizeof(Elf32_Phdr), (uint8_t *)&program_header);
        if (program_header.p_type == PT_LOAD) {
            uint32_t num_of_pages = (program_header.P_memsz + 0xFFF) / 0x1000;
            for (size_t p = 0; p < num_of_pages; p++) {
                uint64_t vaddr = program_header.p_vaddr + (p * 0x1000);
                void    *frame = pmm_alloc_page();
                uint32_t flags = VMM_PRESENT | VMM_USER;
                if (program_header.p_flags & PF_W) {
                    flags |= VMM_WRITE;
                }
                vmm_map_page((PageTable *)new_plm4_phys, vaddr, (uint64_t)frame, flags);
                memset((void *)virtual_to_physical(&vaddr), 0x0, 0x1000);
            }
        }
        if (program_header.p_filesz > 0) {
            file->read(file, program_header.p_offset, program_header.p_filesz,
                       (uint8_t *)(uintptr_t)program_header.p_vaddr);
        }
    }
    return true;
}
