#include "acpi/acpi.h"
#include "common/boot_info.h"
#include "mem/vmm.h"
#include "print/print.h"
#include <string.h>

static struct MADT* s_madt = nullptr;
static uint32_t s_ioapic_base = 0;
static struct MADTInterruptOverride s_irq_overrides[16];
static int s_irq_override_count = 0;

static bool verify_checksum(void* ptr, size_t length) {
    uint8_t sum = 0;
    uint8_t* bytes = (uint8_t*)ptr;
    for (size_t i = 0; i < length; i++) {
        sum += bytes[i];
    }
    return sum == 0;
}

void* acpi_find_table(const char* signature) {
    if (!g_efi_boot_info.rsdp) {
        kprintf("ACPI: RSDP is null!\n");
        return nullptr;
    }

    kprintf("ACPI: Using RSDP physical address: 0x%lx\n", (uint64_t)g_efi_boot_info.rsdp);
    struct RSDPDescriptor20* rsdp = (struct RSDPDescriptor20*)phys_to_kvirt((uint64_t)g_efi_boot_info.rsdp);
    
    // Check signature
    char sig[9];
    memcpy(sig, rsdp->firstPart.Signature, 8);
    sig[8] = 0;
    kprintf("ACPI: RSDP Signature: '%s'\n", sig);
    kprintf("ACPI: RSDP Revision %d, XSDT: 0x%lx, RSDT: 0x%x\n", rsdp->firstPart.Revision, rsdp->XsdtAddress, rsdp->firstPart.RsdtAddress);

    // Try XSDT first if Revision >= 2
    if (rsdp->firstPart.Revision >= 2 && rsdp->XsdtAddress != 0) {
        struct ACPISDTHeader* xsdt = (struct ACPISDTHeader*)phys_to_kvirt(rsdp->XsdtAddress);
        kprintf("ACPI: Checking XSDT at 0x%lx, Length: %d\n", (uint64_t)xsdt, xsdt->Length);
        if (xsdt->Length > 0 && xsdt->Length < 0x100000 && verify_checksum(xsdt, xsdt->Length)) {
            int entries = (xsdt->Length - sizeof(struct ACPISDTHeader)) / 8;
            uint64_t* pointers = (uint64_t*)((uint8_t*)xsdt + sizeof(struct ACPISDTHeader));
            for (int i = 0; i < entries; i++) {
                struct ACPISDTHeader* header = (struct ACPISDTHeader*)phys_to_kvirt(pointers[i]);
                if (strncmp(header->Signature, signature, 4) == 0 && verify_checksum(header, header->Length)) {
                    return header;
                }
            }
        } else {
            kprintf("ACPI: XSDT checksum failed or invalid length!\n");
        }
    }

    // Fallback to RSDT
    if (rsdp->firstPart.RsdtAddress != 0) {
        struct ACPISDTHeader* rsdt = (struct ACPISDTHeader*)phys_to_kvirt(rsdp->firstPart.RsdtAddress);
        kprintf("ACPI: Checking RSDT at 0x%lx, Length: %d\n", (uint64_t)rsdt, rsdt->Length);
        if (rsdt->Length > 0 && rsdt->Length < 0x100000 && verify_checksum(rsdt, rsdt->Length)) {
            int entries = (rsdt->Length - sizeof(struct ACPISDTHeader)) / 4;
            uint32_t* pointers = (uint32_t*)((uint8_t*)rsdt + sizeof(struct ACPISDTHeader));
            for (int i = 0; i < entries; i++) {
                struct ACPISDTHeader* header = (struct ACPISDTHeader*)phys_to_kvirt(pointers[i]);
                if (strncmp(header->Signature, signature, 4) == 0 && verify_checksum(header, header->Length)) {
                    return header;
                }
            }
        } else {
            kprintf("ACPI: RSDT checksum failed or invalid length!\n");
        }
    }

    return nullptr;
}

void acpi_init() {
    kprintf("ACPI: Initializing...\n");
    s_madt = (struct MADT*)acpi_find_table("APIC");
    if (!s_madt) {
        kprintf("ACPI: MADT not found!\n");
        return;
    }

    kprintf("ACPI: Found MADT at 0x%lx\n", (uint64_t)s_madt);
    kprintf("ACPI: Local APIC Address: 0x%x\n", s_madt->localApicAddress);

    uint8_t* ptr = (uint8_t*)s_madt + sizeof(struct MADT);
    uint8_t* end = (uint8_t*)s_madt + s_madt->h.Length;

    while (ptr < end) {
        struct MADTRecord* record = (struct MADTRecord*)ptr;
        if (record->length == 0) {
            kprintf("ACPI: Invalid MADT record length 0!\n");
            break;
        }
        if (record->type == 1) { // I/O APIC
            struct MADTIOAPIC* ioapic = (struct MADTIOAPIC*)ptr;
            kprintf("ACPI: Found I/O APIC at 0x%x, GSI Base: %d\n", ioapic->ioApicAddress, ioapic->globalSystemInterruptBase);
            if (s_ioapic_base == 0) {
                s_ioapic_base = ioapic->ioApicAddress;
            }
        } else if (record->type == 2) { // Interrupt Source Override
            struct MADTInterruptOverride* override = (struct MADTInterruptOverride*)ptr;
            kprintf("ACPI: IRQ Override: Bus %d, Source %d -> GSI %d\n", override->bus, override->source, override->globalSystemInterrupt);
            if (s_irq_override_count < 16) {
                s_irq_overrides[s_irq_override_count++] = *override;
            }
        }
        ptr += record->length;
    }
}

uint32_t acpi_get_ioapic_base() {
    return s_ioapic_base;
}

uint32_t acpi_get_irq_override(uint8_t irq) {
    for (int i = 0; i < s_irq_override_count; i++) {
        if (s_irq_overrides[i].source == irq) {
            return s_irq_overrides[i].globalSystemInterrupt;
        }
    }
    return irq; // 1:1 mapping by default
}
