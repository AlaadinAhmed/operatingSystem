#pragma once
#include <stdint.h>
struct BootInfo {
  uint64_t fb_addr;
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  void *rsdp;
  void *mmap;
  uint64_t mmap_size;
  uint64_t desc_size;
  uint64_t audio_bar;
};

extern struct BootInfo g_efi_boot_info;

inline void initialize_bootinfo(struct BootInfo *boot_info) {
  g_efi_boot_info.fb_addr = boot_info->fb_addr;
  g_efi_boot_info.width = boot_info->width;
  g_efi_boot_info.height = boot_info->height;
  g_efi_boot_info.pitch = boot_info->pitch;
  g_efi_boot_info.rsdp = boot_info->rsdp;
  g_efi_boot_info.mmap = boot_info->mmap;
  g_efi_boot_info.mmap_size = boot_info->mmap_size;
  g_efi_boot_info.desc_size = boot_info->desc_size;
  g_efi_boot_info.audio_bar = boot_info->audio_bar;
}
