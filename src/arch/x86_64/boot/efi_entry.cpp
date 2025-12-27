#include <efi.h>
#include <efilib.h>

// Serial port debugging
#define SERIAL_PORT 0x3F8

static void serial_putchar(char c) {
  __asm__ volatile("outb %0, %1" : : "a"(c), "Nd"((short)SERIAL_PORT));
}

static void serial_print(const char *s) {
  while (*s)
    serial_putchar(*s++);
}

static void serial_print_hex(unsigned long long n) {
  const char *digits = "0123456789ABCDEF";
  serial_print("0x");
  for (int i = 60; i >= 0; i -= 4) {
    serial_putchar(digits[(n >> i) & 0xF]);
  }
  serial_print("\n");
}

extern "C" char _binary_kernel_bin_start[];
extern "C" char _binary_kernel_bin_end[];

// Helper to set graphics mode
EFI_STATUS set_graphics_mode(EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop) {
  EFI_STATUS Status;
  UINT32 ModeIndex;
  UINTN SizeOfInfo;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
  UINT32 BestMode = 0;
  UINT32 BestWidth = 0;
  UINT32 BestHeight = 0;
  BOOLEAN Found = FALSE;

  serial_print("Available GOP Modes:\n");
  for (ModeIndex = 0; ModeIndex < Gop->Mode->MaxMode; ModeIndex++) {
    Status = uefi_call_wrapper(Gop->QueryMode, 4, Gop, ModeIndex, &SizeOfInfo,
                               &Info);
    if (EFI_ERROR(Status))
      continue;

    serial_print("Mode ");
    serial_print_hex(ModeIndex);
    serial_print(" Res: ");
    serial_print_hex(Info->HorizontalResolution);
    serial_print("x");
    serial_print_hex(Info->VerticalResolution);
    serial_print("\n");

    // Look for 1280x800 or 1024x768, 32-bit
    // (PixelBlueGreenRedReserved8BitPerColor or
    // PixelRedGreenBlueReserved8BitPerColor)
    // Look for 1920x1080, or fallback to others
    if (Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor ||
        Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {

      // Prefer 1920x1080
      if (Info->HorizontalResolution == 1920 &&
          Info->VerticalResolution == 1080) {
        BestWidth = Info->HorizontalResolution;
        BestHeight = Info->VerticalResolution;
        BestMode = ModeIndex;
        Found = TRUE;
        break; // Stop once we find 1920x1080
      }

      // Fallback to other resolutions (find max)
      if (Info->HorizontalResolution > BestWidth) {
        BestWidth = Info->HorizontalResolution;
        BestHeight = Info->VerticalResolution;
        BestMode = ModeIndex;
        Found = TRUE;
      }
    }
  }

  if (Found) {
    serial_print("Setting Mode ");
    serial_print_hex(BestMode);
    serial_print("\n");
    Status = uefi_call_wrapper(Gop->SetMode, 2, Gop, BestMode);
    if (EFI_ERROR(Status)) {
      serial_print("Error setting mode\n");
      return Status;
    }
  } else {
    serial_print("Warning: No suitable mode found, using default.\n");
  }
  return EFI_SUCCESS;
}

// UEFI entry point - EFIAPI ensures MS calling convention
extern "C" EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle,
                                      EFI_SYSTEM_TABLE *SystemTable) {
  EFI_STATUS Status;
  EFI_INPUT_KEY Key;
  UINTN Index;

  // Initialize gnu-efi library (sets ST, BS, RT globals)
  InitializeLib(ImageHandle, SystemTable);
  serial_print("UEFI: InitializeLib OK (DEBUG)\n");

  // Debug: Print handles
  serial_print("ImageHandle: ");
  serial_print_hex((unsigned long long)ImageHandle);
  serial_print("SystemTable: ");
  serial_print_hex((unsigned long long)SystemTable);

  // Clear screen using uefi_call_wrapper for correct MS ABI
  Status = uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
  if (EFI_ERROR(Status)) {
    serial_print("Warning: ClearScreen failed\n");
  }
  serial_print("UEFI: Screen cleared\n");

  // Print to UEFI console
  Print(L"=====================================\r\n");
  Print(L"       MyOS UEFI Bootloader\r\n");
  Print(L"=====================================\r\n");
  Print(L"\r\n");
  Print(L"Boot Method: Pure UEFI (gnu-efi)\r\n");

  // Define kernel entry point type
  // void main(uint32_t magic, uint64_t addr)
  typedef void (*KernelEntry)(uint32_t magic, uint64_t addr);
  KernelEntry kernel_entry;
  uint32_t magic;
  UINTN ExtraPages;
  UINT8 *BssStart;
  UINTN BssSize;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
  EFI_GUID GopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
  struct BootInfo {
    uint64_t fb_addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;

    void *rsdp; // ACPI pointer (essential for finding the audio chip)
    void *mmap;
    uint64_t mmap_size;
    uint64_t desc_size;
    uint64_t audio_bar;
  };
  struct BootInfo *boot_info;
  EFI_GUID Acpi20Guid = ACPI_20_TABLE_GUID;
  EFI_CONFIGURATION_TABLE *ConfigTable;
  // --- Load Kernel (Embedded) ---
  UINTN KernelSize =
      (UINTN)_binary_kernel_bin_end - (UINTN)_binary_kernel_bin_start;
  EFI_PHYSICAL_ADDRESS KernelAddr = 0x100000; // Load at 1MB
  UINTN Pages;

  Print(L"Embedded Kernel Size: %d bytes\r\n", KernelSize);
  serial_print("Kernel size: ");
  serial_print_hex(KernelSize);

  // 4. Allocate Memory
  // Allocate extra 4MB for BSS and Stack
  ExtraPages = 1024;
  Pages = (KernelSize + 0xFFF) / 0x1000 + ExtraPages;

  Status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress,
                             EfiLoaderData, Pages, &KernelAddr);
  if (EFI_ERROR(Status)) {
    Print(L"Warning: Could not allocate at 0x100000, trying any...\r\n");
    Status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages,
                               EfiLoaderData, Pages, &KernelAddr);
    if (EFI_ERROR(Status)) {
      Print(L"Error: Could not allocate memory\r\n");
      serial_print("Error: Could not allocate memory\n");
      goto error;
    }
  }
  Print(L"Allocated memory at: 0x%lx (Pages: %d)\r\n", KernelAddr, Pages);
  serial_print("Allocated memory at: ");
  serial_print_hex(KernelAddr);

  // 5. Copy Kernel from Embedded Data
  CopyMem((void *)KernelAddr, _binary_kernel_bin_start, KernelSize);

  // Zero out the BSS/Stack area (after the kernel code/data)
  // We allocated ExtraPages * 4096 bytes more than needed for the file
  // But strictly speaking, we should zero from KernelAddr + KernelSize to the
  // end of allocation
  BssStart = (UINT8 *)(KernelAddr + KernelSize);
  BssSize = (Pages * 4096) - KernelSize;
  SetMem(BssStart, BssSize, 0);
  Print(L"Zeroed BSS/Stack area: %d bytes\r\n", BssSize);
  serial_print("Zeroed BSS/Stack\n");

  Print(L"Kernel loaded successfully!\r\n");
  serial_print("Kernel loaded\n");

  // --- Graphics Output Protocol (GOP) ---
  Status =
      uefi_call_wrapper(BS->LocateProtocol, 3, &GopGuid, NULL, (void **)&Gop);
  if (EFI_ERROR(Status)) {
    Print(L"Error: Could not locate GOP\r\n");
    serial_print("Error: Could not locate GOP\n");
    goto error;
  }

  // Set Graphics Mode
  set_graphics_mode(Gop);

  Print(L"GOP located. FB: 0x%lx, Res: %dx%d, Pitch: %d, Fmt: %d\r\n",
        Gop->Mode->FrameBufferBase, Gop->Mode->Info->HorizontalResolution,
        Gop->Mode->Info->VerticalResolution, Gop->Mode->Info->PixelsPerScanLine,
        Gop->Mode->Info->PixelFormat);

  //
  // Prepare BootInfo structure
  Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
                             sizeof(struct BootInfo), (void **)&boot_info);
  if (EFI_ERROR(Status)) {
    Print(L"Error allocating boot info\r\n");
    goto error;
  }

  boot_info->fb_addr = Gop->Mode->FrameBufferBase;
  boot_info->width = Gop->Mode->Info->HorizontalResolution;
  boot_info->height = Gop->Mode->Info->VerticalResolution;
  boot_info->pitch = Gop->Mode->Info->PixelsPerScanLine; // Pitch in pixels

  Print(L"BootInfo at 0x%lx\r\n", (uint64_t)boot_info);

  // ACPI Detection
  ConfigTable = ST->ConfigurationTable;
  for (UINTN i = 0; i < ST->NumberOfTableEntries; i++) {
    if (CompareGuid(&ConfigTable[i].VendorGuid, &Acpi20Guid)) {
      boot_info->rsdp = ConfigTable[i].VendorTable; // Found it!
      Print(L"RSDP found at 0x%lx\r\n", (uint64_t)boot_info->rsdp);
      break;
    }
  }

  // Get Memory Map and Exit Boot Services
  {
    UINTN MemoryMapSize = 0;
    UINTN AllocatedMapSize = 0;
    EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
    UINTN MapKey;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;

    // 1. Get Memory Map Size
    Status = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap,
                               &MapKey, &DescriptorSize, &DescriptorVersion);
    if (Status == EFI_BUFFER_TOO_SMALL) {
      MemoryMapSize += 8 * DescriptorSize; // Add more slack (was 2)
      AllocatedMapSize = MemoryMapSize;    // Remember allocated size

      Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
                                 MemoryMapSize, (void **)&MemoryMap);

      if (EFI_ERROR(Status)) {
        Print(L"Error allocating memory map: %r\r\n", Status);
        goto error;
      }

      // 2. Get Memory Map
      // Note: MemoryMapSize is updated to actual size on success
      Status = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap,
                                 &MapKey, &DescriptorSize, &DescriptorVersion);
      if (EFI_ERROR(Status)) {
        Print(L"Error getting memory map: %r\r\n", Status);
        goto error;
      }

      boot_info->mmap = MemoryMap;
      boot_info->mmap_size = MemoryMapSize;
      boot_info->desc_size = DescriptorSize;
      Print(L"Memory Map retrieved. Size: %d, DescSize: %d\r\n", MemoryMapSize,
            DescriptorSize);
    }

    Print(L"\r\nKernel loaded at 0x%lx. Jumping to kernel...\r\n", KernelAddr);
    serial_print("Jumping to kernel...\n");

    // 3. Exit Boot Services
    // We might need to retry if the map key changes
    Status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, MapKey);
    if (EFI_ERROR(Status)) {
      // Retry once
      // Reset MemoryMapSize to the full allocated size, not the used size from
      // previous call
      MemoryMapSize = AllocatedMapSize;
      Status = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap,
                                 &MapKey, &DescriptorSize, &DescriptorVersion);
      if (!EFI_ERROR(Status)) {
        Status =
            uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, MapKey);
      }
    }

    if (EFI_ERROR(Status)) {
      Print(L"Error exiting boot services: %r\r\n", Status);
      goto error;
    }
  }

  kernel_entry = (KernelEntry)KernelAddr;

  // UEFI Magic: 'EFI ' = 0x45464920
  magic = 0x45464920;

  // Jump to kernel
  // Pass BootInfo pointer as second argument
  kernel_entry(magic, (uint64_t)boot_info);

  // If kernel returns (it shouldn't)
  // We can't use Print anymore because Boot Services are gone!
  while (1) {
    __asm__ volatile("hlt");
  }

  // Label for error handling (only reachable if ExitBootServices failed or
  // wasn't called) But wait, if ExitBootServices succeeded, we jumped to
  // kernel. If we are here, it means we failed BEFORE ExitBootServices or
  // ExitBootServices failed. So Print is safe here.

error:
  Print(L"\r\nBoot failed. Press any key to halt.\r\n");
  uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ST->ConIn->WaitForKey, &Index);
  uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);
  while (1)
    __asm__ volatile("hlt");

  return EFI_SUCCESS;
}
