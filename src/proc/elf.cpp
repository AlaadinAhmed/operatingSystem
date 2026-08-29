#include "fs/vfs.h"
#include "proc/elf.h"
#include <cstdint>

bool load_elf_binary(vfs_node *file, uint64_t *new_plm4_phys,
                     uint64_t *out_entry) {
    Elf32_Ehdr *header;
    if (file->read(file, 0, sizeof(Elf32_Ehdr), (uint8_t *)&header) <
        (int64_t)sizeof(Elf32_Ehdr)) {
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

    uint32_t   offset = header->e_phoff;
    Elf32_Phdr program_header;
    if (file->read(file, offset, sizeof(Elf32_Phdr),
                   (uint8_t *)&program_header)) {
    }
}
