#include <efi.h>
#include <efilib.h>
#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITE (1ULL << 1)
#define PAGE_LARGE (1ULL << 7) // Enables 2MB pages in a PD entry

// Helper indices for higher-half 0xFFFFFFFF80000000
#define PML4_INDEX(vaddr) (((vaddr) >> 39) & 0x1FF)
#define PDPT_INDEX(vaddr) (((vaddr) >> 30) & 0x1FF)
#define PD_INDEX(vaddr) (((vaddr) >> 21) & 0x1FF)

// Serial port debugging
#define SERIAL_PORT 0x3F8

static void serial_putchar(char c) { __asm__ volatile("outb %0, %1" : : "a"(c), "Nd"((short)SERIAL_PORT)); }

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
        Status = uefi_call_wrapper(Gop->QueryMode, 4, Gop, ModeIndex, &SizeOfInfo, &Info);
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
            if (Info->HorizontalResolution == 1920 && Info->VerticalResolution == 1080) {
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
extern "C" EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
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
    UINTN KernelSize = (UINTN)_binary_kernel_bin_end - (UINTN)_binary_kernel_bin_start;
    EFI_PHYSICAL_ADDRESS KernelAddr = 0x100000; // Load at 1MB
    UINTN Pages;

    // Declare PML4 pointer here so it is accessible globally across the function scope
    uint64_t *pml4 = NULL;

    Print(L"Embedded Kernel Size: %d bytes\r\n", KernelSize);
    serial_print("Kernel size: ");
    serial_print_hex(KernelSize);

    // Allocate Memory
    // Allocate extra 4MB for BSS and Stack
    ExtraPages = 1024;
    Pages = (KernelSize + 0xFFF) / 0x1000 + ExtraPages;

    Status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress, EfiLoaderData, Pages, &KernelAddr);
    if (EFI_ERROR(Status)) {
        Print(L"Warning: Could not allocate at 0x100000, trying any...\r\n");
        Status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, Pages, &KernelAddr);
        if (EFI_ERROR(Status)) {
            Print(L"Error: Could not allocate memory\r\n");
            serial_print("Error: Could not allocate memory\n");
            goto error;
        }
    }
    Print(L"Allocated memory at: 0x%lx (Pages: %d)\r\n", KernelAddr, Pages);
    serial_print("Allocated memory at: ");
    serial_print_hex(KernelAddr);

    // Copy Kernel from Embedded Data
    CopyMem((void *)KernelAddr, _binary_kernel_bin_start, KernelSize);

    // Zero out the BSS/Stack area
    BssStart = (UINT8 *)(KernelAddr + KernelSize);
    BssSize = (Pages * 4096) - KernelSize;
    SetMem(BssStart, BssSize, 0);
    Print(L"Zeroed BSS/Stack area: %d bytes\r\n", BssSize);
    serial_print("Zeroed BSS/Stack\n");

    Print(L"Kernel loaded successfully!\r\n");
    serial_print("Kernel loaded\n");

    // --- Graphics Output Protocol (GOP) ---
    Status = uefi_call_wrapper(BS->LocateProtocol, 3, &GopGuid, NULL, (void **)&Gop);
    if (EFI_ERROR(Status)) {
        Print(L"Error: Could not locate GOP\r\n");
        serial_print("Error: Could not locate GOP\n");
        goto error;
    }

    // Set Graphics Mode
    set_graphics_mode(Gop);

    Print(L"GOP located. FB: 0x%lx, Res: %dx%d, Pitch: %d, Fmt: %d\r\n", Gop->Mode->FrameBufferBase,
          Gop->Mode->Info->HorizontalResolution, Gop->Mode->Info->VerticalResolution,
          Gop->Mode->Info->PixelsPerScanLine, Gop->Mode->Info->PixelFormat);

    // Prepare BootInfo structure
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, sizeof(struct BootInfo), (void **)&boot_info);
    if (EFI_ERROR(Status)) {
        Print(L"Error allocating boot info\r\n");
        goto error;
    }

    boot_info->fb_addr = Gop->Mode->FrameBufferBase;
    boot_info->width = Gop->Mode->Info->HorizontalResolution;
    boot_info->height = Gop->Mode->Info->VerticalResolution;
    boot_info->pitch = Gop->Mode->Info->PixelsPerScanLine;

    Print(L"BootInfo at 0x%lx\r\n", (uint64_t)boot_info);

    // ACPI Detection
    ConfigTable = ST->ConfigurationTable;
    for (UINTN i = 0; i < ST->NumberOfTableEntries; i++) {
        if (CompareGuid(&ConfigTable[i].VendorGuid, &Acpi20Guid)) {
            boot_info->rsdp = ConfigTable[i].VendorTable;
            Print(L"RSDP found at 0x%lx\r\n", (uint64_t)boot_info->rsdp);
            break;
        }
    }

    // --- Paging Configuration Setup ---
    {
#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITE (1ULL << 1)
#define PAGE_LARGE (1ULL << 7)
#define PML4_INDEX(vaddr) (((vaddr) >> 39) & 0x1FF)
#define PDPT_INDEX(vaddr) (((vaddr) >> 30) & 0x1FF)

        EFI_PHYSICAL_ADDRESS PagingMemory = 0;
        UINTN PagingPages = 8;
        Status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, PagingPages, &PagingMemory);
        if (EFI_ERROR(Status)) {
            Print(L"Error: Failed to allocate paging tables\r\n");
            goto error;
        }
        SetMem((void *)PagingMemory, PagingPages * 4096, 0);

        uint64_t *pml4_ptr = (uint64_t *)PagingMemory;
        uint64_t *pdpt = (uint64_t *)(PagingMemory + 4096);
        uint64_t *identity_pd = (uint64_t *)(PagingMemory + (2 * 4096));
        uint64_t *higher_half_pd = (uint64_t *)(PagingMemory + (3 * 4096));
        uint64_t *pd_1to2 = (uint64_t *)(PagingMemory + (4 * 4096));
        uint64_t *pd_2to3 = (uint64_t *)(PagingMemory + (5 * 4096));
        uint64_t *pd_3to4 = (uint64_t *)(PagingMemory + (6 * 4096));

        // Identity map the first 4GB of physical address space
        for (uint64_t i = 0; i < 512; i++) {
            identity_pd[i] = (i * 0x200000) | PAGE_PRESENT | PAGE_WRITE | PAGE_LARGE;
            higher_half_pd[i] = (i * 0x200000) | PAGE_PRESENT | PAGE_WRITE | PAGE_LARGE;
            
            pd_1to2[i] = ((i + 512) * 0x200000) | PAGE_PRESENT | PAGE_WRITE | PAGE_LARGE;
            pd_2to3[i] = ((i + 1024) * 0x200000) | PAGE_PRESENT | PAGE_WRITE | PAGE_LARGE;
            pd_3to4[i] = ((i + 1536) * 0x200000) | PAGE_PRESENT | PAGE_WRITE | PAGE_LARGE;
        }

        // Link the first 4GB PDs into PDPT
        pdpt[0] = ((uint64_t)identity_pd) | PAGE_PRESENT | PAGE_WRITE;
        pdpt[1] = ((uint64_t)pd_1to2) | PAGE_PRESENT | PAGE_WRITE;
        pdpt[2] = ((uint64_t)pd_2to3) | PAGE_PRESENT | PAGE_WRITE;
        pdpt[3] = ((uint64_t)pd_3to4) | PAGE_PRESENT | PAGE_WRITE;

        // IDENTITY MAP THE GRAPHICS FRAMEBUFFER (if located above 4GB)
        uint64_t fb_phys_start = Gop->Mode->FrameBufferBase;
        uint64_t pdpt_idx = fb_phys_start / 0x40000000;

        if (pdpt_idx >= 4) {
            uint64_t *fb_pd = (uint64_t *)(PagingMemory + (7 * 4096));
            uint64_t pd_index_start = (fb_phys_start % 0x40000000) / 0x200000;
            UINTN fb_size = Gop->Mode->FrameBufferSize;
            UINTN fb_2mb_pages = (fb_size + 0x1FFFFF) / 0x200000;
            if (fb_2mb_pages > 512 - pd_index_start) {
                fb_2mb_pages = 512 - pd_index_start;
            }

            for (uint64_t i = 0; i < fb_2mb_pages; i++) {
                uint64_t phys_addr = fb_phys_start + (i * 0x200000);
                fb_pd[pd_index_start + i] = phys_addr | PAGE_PRESENT | PAGE_WRITE | PAGE_LARGE;
            }

            // Link fb_pd into pdpt
            if (pdpt_idx < 512) {
                pdpt[pdpt_idx] = ((uint64_t)fb_pd) | PAGE_PRESENT | PAGE_WRITE;
            }
        }

        uint64_t kernel_virtual_base = 0xFFFFFFFF80000000ULL;
        pdpt[PDPT_INDEX(kernel_virtual_base)] = ((uint64_t)higher_half_pd) | PAGE_PRESENT | PAGE_WRITE;

        pml4_ptr[0] = ((uint64_t)pdpt) | PAGE_PRESENT | PAGE_WRITE;
        pml4_ptr[PML4_INDEX(kernel_virtual_base)] = ((uint64_t)pdpt) | PAGE_PRESENT | PAGE_WRITE;

        // Assign to our outer function scope variable
        pml4 = pml4_ptr;

        Print(L"Early Multi-Level Page Tables mapped successfully (with Framebuffer fixed).\r\n");
    }

    // Get Memory Map and Exit Boot Services
    {
        UINTN MemoryMapSize = 0;
        UINTN AllocatedMapSize = 0;
        EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
        UINTN MapKey;
        UINTN DescriptorSize;
        UINT32 DescriptorVersion;

        Status = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize,
                                   &DescriptorVersion);
        if (Status == EFI_BUFFER_TOO_SMALL) {
            MemoryMapSize += 8 * DescriptorSize;
            AllocatedMapSize = MemoryMapSize;

            Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, MemoryMapSize, (void **)&MemoryMap);
            if (EFI_ERROR(Status)) {
                Print(L"Error allocating memory map: %r\r\n", Status);
                goto error;
            }

            Status = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize,
                                       &DescriptorVersion);
            if (EFI_ERROR(Status)) {
                Print(L"Error getting memory map: %r\r\n", Status);
                goto error;
            }

            boot_info->mmap = MemoryMap;
            boot_info->mmap_size = MemoryMapSize;
            boot_info->desc_size = DescriptorSize;
            Print(L"Memory Map retrieved. Size: %d, DescSize: %d\r\n", MemoryMapSize, DescriptorSize);
        }

        Print(L"\r\nKernel loaded at 0x%lx. Jumping to kernel...\r\n", KernelAddr);
        serial_print("Jumping to kernel...\n");

        Status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, MapKey);
        if (EFI_ERROR(Status)) {
            MemoryMapSize = AllocatedMapSize;
            Status = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize,
                                       &DescriptorVersion);
            if (!EFI_ERROR(Status)) {
                Status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, MapKey);
            }
        }

        if (EFI_ERROR(Status)) {
            Print(L"Error exiting boot services: %r\r\n", Status);
            goto error;
        }
    }

    // Enclose execution calculation blocks inside an independent scope block
    // to prevent C++ compiler initialization crossing errors with label jumps
    {
        __asm__ volatile("mov %0, %%cr3" ::"r"((uint64_t)pml4) : "memory");

        uint64_t virtual_kernel_offset = 0xFFFFFFFF80000000ULL;
        kernel_entry = (KernelEntry)(KernelAddr + virtual_kernel_offset);
        magic = 0x45464920;

        serial_print("CR3 Swapped! Handing execution context to Higher-Half Kernel...\n");
        kernel_entry(magic, (uint64_t)boot_info);
    }

    while (1) {
        __asm__ volatile("hlt");
    }

error:
    Print(L"\r\nBoot failed. Press any key to halt.\r\n");
    uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ST->ConIn->WaitForKey, &Index);
    uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);
    while (1)
        __asm__ volatile("hlt");

    return EFI_SUCCESS;
}
