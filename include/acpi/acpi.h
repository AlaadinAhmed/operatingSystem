#pragma once

#include <stdint.h>
#include <stddef.h>

struct RSDPDescriptor {
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
} __attribute__((packed));

struct RSDPDescriptor20 {
    struct RSDPDescriptor firstPart;
    uint32_t Length;
    uint64_t XsdtAddress;
    uint8_t ExtendedChecksum;
    uint8_t reserved[3];
} __attribute__((packed));

struct ACPISDTHeader {
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} __attribute__((packed));

struct MADT {
    struct ACPISDTHeader h;
    uint32_t localApicAddress;
    uint32_t flags;
    // Followed by variable number of records
} __attribute__((packed));

struct MADTRecord {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct MADTLocalAPIC {
    struct MADTRecord header;
    uint8_t acpiProcessorId;
    uint8_t apicId;
    uint32_t flags;
} __attribute__((packed));

struct MADTIOAPIC {
    struct MADTRecord header;
    uint8_t ioApicId;
    uint8_t reserved;
    uint32_t ioApicAddress;
    uint32_t globalSystemInterruptBase;
} __attribute__((packed));

struct MADTInterruptOverride {
    struct MADTRecord header;
    uint8_t bus;
    uint8_t source;
    uint32_t globalSystemInterrupt;
    uint16_t flags;
} __attribute__((packed));

struct MADTLocalAPICNMI {
    struct MADTRecord header;
    uint8_t acpiProcessorId;
    uint16_t flags;
    uint8_t lint;
} __attribute__((packed));

void acpi_init();
void* acpi_find_table(const char* signature);
uint32_t acpi_get_ioapic_base();
uint32_t acpi_get_irq_override(uint8_t irq);
