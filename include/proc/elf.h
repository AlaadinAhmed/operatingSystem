#pragma once
#include "fs/vfs.h"
#include <cstdint>
#define ELFMAGIC 0x7f454c46
#define ELF32HEADER 0x7f454c4601
#define ELF64HEADER 0x7f454c4602
enum E_TYPE {
    ET_NONE   = 0,
    ET_REL    = 1,
    ET_EXEC   = 2,
    ET_DYN    = 3,
    ET_CORE   = 3,
    ET_LOPROC = 0xff00,
    ET_HIPROc = 0xffff

};
enum E_MACHINE {
    EM_NONE           = 0,
    EM_M32            = 1,
    EM_SPARC          = 2,
    EM_386            = 3,
    EM_68K            = 4,
    EM_88K            = 5,
    EM_860            = 7,
    EM_MIPS           = 8,
    EM_MIPS_RS4_BE    = 10,
    EM_RESERVED_START = 11,
    EM_RESERVED_END   = 16
};
enum E_VERSION { EV_NONE = 0, EV_CURRENT = 1 };
enum E_IDENT_INDEX {
    EI_MAG0    = 0,
    EI_MAG1    = 1,
    EI_MAG2    = 2,
    EI_MAG3    = 3,
    EI_CLASS   = 4,
    EI_DATA    = 5,
    EI_VERSION = 6,
    EI_PAD     = 7,
    EI_NIDENT  = 16
};
enum EI_MAGIC { ELFMAG0 = 0x7f, ELFMAG1 = 'E', ELFMAG2 = 'L', ELFMAG3 = 'F' };
enum EI_CLASS {
    ELFCLASSNONE = 0,
    ELFCLASS32   = 1,
    ELFCLASS64   = 2,

};
enum EI_DATA { ELFDATANONE = 0, ELFDATA2LSB = 1, ELFDATA2MSB = 2 };
struct Elf32_Ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};
enum SH_TYPE {
    SHT_NULL = 0,
    SHT_PROGBITS,
    SHT_SYMTAB,
    SHT_STRTAB,
    SHT_RELA,
    SHT_HASH,
    SHT_DYNAMIC,
    SHT_NOTE,
    SHT_NOBITS,
    SHT_REL,
    SHT_SHLIB,
    SHT_DYNSYM = 11,
    SHT_HIPROC = 0x70000000,
    SHT_LOPROC = 0x7fffffff,
    SHT_LOUSER = 0x80000000,
    SHT_HIUSER = 0xffffffff
};
struct Elf32_Shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
};
enum P_TYPE {
    PT_NULL = 0,
    PT_LOAD,
    PT_DYNAMIC,
    PT_INTERP,
    PT_NONE,
    PT_SHLIB,
    PT_PHDR   = 6,
    PT_LOPROC = 0x70000000,
    PT_HIPROC = 0x7fffffff
};
enum P_FLAGS { PF_X = 1, PF_W = 2, PF_R = 4, PF_MASKPROC = 0xf0000000 };
struct Elf32_Phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t P_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};
bool load_elf_binary(vfs_node *file, uint64_t *new_plm4_phys, uint64_t *out_entry);
