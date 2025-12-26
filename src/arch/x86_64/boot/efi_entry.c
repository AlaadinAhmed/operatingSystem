#include <efi.h>
#include <efilib.h>

// Serial port debugging
#define SERIAL_PORT 0x3F8

static void serial_putchar(char c) {
    __asm__ volatile("outb %0, %1" : : "a"(c), "Nd"((short)SERIAL_PORT));
}

static void serial_print(const char *s) {
    while (*s) serial_putchar(*s++);
}

// UEFI entry point - EFIAPI ensures MS calling convention
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    // Serial debug first
    serial_print("UEFI: Entry (gnu-efi)\n");
    
    // Initialize gnu-efi library (sets ST, BS, RT)
    InitializeLib(ImageHandle, SystemTable);
    serial_print("UEFI: InitializeLib OK\n");

    // Clear screen
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    serial_print("UEFI: Screen cleared\n");
    
    // Print to UEFI console
    Print(L"Hello from gnu-efi!\r\n");
    Print(L"Boot Method: UEFI\r\n");
    Print(L"Press any key...\r\n");
    serial_print("UEFI: Print OK\n");

    // Wait for key
    EFI_INPUT_KEY Key;
    UINTN Index;
    uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ST->ConIn->WaitForKey, &Index);
    uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);
    
    serial_print("UEFI: Key pressed, halting\n");
    
    // Halt
    while(1) {
        __asm__ volatile("hlt");
    }
    
    return EFI_SUCCESS;
}
