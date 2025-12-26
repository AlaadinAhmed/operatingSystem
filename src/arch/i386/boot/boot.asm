[org 0x7c00]
[bits 16]
%define NULL_T 0x00 
%define ENDL 0x0D, 0x0A, 0x00

start:
    jmp short main
    nop

; ============================================================
; BIOS Parameter Block (BPB) - Required for some BIOSes
; This makes the boot sector look like a valid FAT boot sector
; ============================================================
bpb_oem:                    db "MYOS    "    ; 8 bytes
bpb_bytes_per_sector:       dw 512
bpb_sectors_per_cluster:    db 1
bpb_reserved_sectors:       dw 1
bpb_fat_count:              db 2
bpb_root_dir_entries:       dw 224
bpb_total_sectors:          dw 2880
bpb_media_type:             db 0xF0
bpb_sectors_per_fat:        dw 9
bpb_sectors_per_track:      dw 18
bpb_head_count:             dw 2
bpb_hidden_sectors:         dd 0
bpb_large_sector_count:     dd 0

; Extended Boot Record
ebr_drive_number:           db 0
                            db 0        ; Reserved
ebr_signature:              db 0x29
ebr_volume_id:              dd 0x12345678
ebr_volume_label:           db "MYOS BOOT  " ; 11 bytes
ebr_system_id:              db "FAT12   "    ; 8 bytes

main:
    ; CRITICAL: Save Boot Drive IMMEDIATELY before any BIOS calls
    ; GRUB or chainloading may have set DL, we must preserve it
    mov [BOOT_DISK], dl

    ; Set up segments - DS=ES=SS=0, SP below boot sector
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00          ; Stack grows down from boot sector

    ; Ensure direction flag is clear
    cld

    ; Debug: Boot started
    mov si, START_BOOT
    call log_message

    ; Debug: Print Drive Number
    mov si, DRIVE_MSG
    call log_message
    mov al, [BOOT_DISK]
    call print_hex_byte
    mov si, NEWLINE_STR
    call log_message

    ; ============================================================
    ; VALIDATE DISK - Check LBA extensions and parameters
    ; ============================================================
    
    ; Check LBA Extensions (INT 13h AH=41h)
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [BOOT_DISK]
    int 0x13
    jc .no_lba_extensions
    cmp bx, 0xAA55          ; BX should be swapped if extensions present
    jne .no_lba_extensions
    test cl, 1              ; Check if packet structure access is supported
    jz .no_lba_extensions

    ; Debug: LBA extensions supported
    mov dx, 0x3f8
    mov al, 'E'
    out dx, al

    ; Get Extended Disk Parameters (optional, for validation)
    mov ah, 0x48
    mov dl, [BOOT_DISK]
    mov si, drive_params
    mov word [drive_params], 0x1E   ; Buffer size
    int 0x13
    jc .skip_validation             ; If fails, proceed anyway

    ; Validate: Check if drive has enough sectors for our LBA target
    ; We read from LBA 2000, so total sectors must be > 2000
    mov eax, [drive_params + 16]    ; Total sectors (low dword)
    cmp eax, 2015                   ; 2000 + 15 sectors we read
    jb .disk_too_small

    ; Debug: Disk validated
    mov dx, 0x3f8
    mov al, 'D'
    out dx, al

.skip_validation:
    ; Read Loader using LBA Packet
    ; Read 15 sectors from LBA 2000 to 0x7E00
    mov ah, 0x42
    mov dl, [BOOT_DISK]
    mov si, dap
    int 0x13
    jc disk_error

    ; Verify first bytes of loaded code (sanity check)
    cmp word [0x7E00], 0      ; Check if something was loaded
    je disk_error             ; If zero, probably failed silently

    ; Debug: Loader Loaded
    mov si, START_LOADER
    call log_message
    
    ; Pass Boot Drive to Loader in DL (critical for hardware!)
    mov dl, [BOOT_DISK]
    
    ; Jump to Loader
    jmp 0x0000:0x7E00

.no_lba_extensions:
    mov si, NO_LBA_MSG
    call log_message
    call print_message_screen
    jmp halt_loop

.disk_too_small:
    mov si, SMALL_DISK_MSG
    call log_message
    call print_message_screen
    jmp halt_loop

disk_error:
    mov si, DISK_ERROR_MSG
    call log_message
    
    ; Also print to screen
    mov si, DISK_ERROR_MSG
    call print_message_screen
    
    ; Print error code from AH
    mov al, ah
    call print_hex_byte
    
halt_loop:
    cli
    hlt
    jmp halt_loop

; ============================================================
; Print hex byte in AL to serial port
; ============================================================
print_hex_byte:
    push ax
    shr al, 4
    call .print_nibble
    pop ax
    and al, 0x0F
    call .print_nibble
    ret
.print_nibble:
    cmp al, 9
    jg .letter
    add al, '0'
    jmp .print
.letter:
    add al, 'A' - 10
.print:
    mov dx, 0x3f8
    out dx, al
    ret

; ============================================================
; Print message to screen (BIOS INT 10h)
; ============================================================
print_message_screen:
    lodsb
    cmp al, 0
    je .done
    cmp al, 0x0D            ; Skip CR
    je print_message_screen
    cmp al, 0x0A            ; Skip LF
    je print_message_screen
    mov ah, 0x0e
    int 0x10
    jmp print_message_screen
.done:
    ret

; ============================================================
; Log message to serial port (0x3F8)
; ============================================================
log_message:
    lodsb
    cmp al, 0
    je .done
    mov dx, 0x3f8
    out dx, al
    jmp log_message
.done:
    ret

; ============================================================
; DATA SECTION
; ============================================================

BOOT_DISK: db 0

; Disk Address Packet for LBA read
align 4
dap:
    db 0x10         ; Size of packet (16 bytes)
    db 0            ; Reserved (must be 0)
    dw 15           ; Number of sectors to read
    dw 0x7E00       ; Offset (destination)
    dw 0x0000       ; Segment (destination)
    dd 1            ; LBA address (low 32 bits)
    dd 0            ; LBA address (high 32 bits)


; Extended drive parameters buffer
drive_params:
    dw 0x1E         ; Buffer size
    dw 0            ; Information flags
    dd 0            ; Physical cylinders
    dd 0            ; Physical heads
    dd 0            ; Physical sectors per track
    dq 0            ; Total sectors on drive
    dw 512          ; Bytes per sector

; Messages
START_BOOT:
    db "Boot Started", ENDL
DRIVE_MSG:
    db "Drive: 0x", 0
NEWLINE_STR:
    db ENDL
START_LOADER:
    db "Loader OK", ENDL
DISK_ERROR_MSG:
    db "Disk Error!", ENDL
NO_LBA_MSG:
    db "No LBA!", ENDL
SMALL_DISK_MSG:
    db "Disk Small!", ENDL

; Boot signature
times 510 - ($ - $$) db 0
db 0x55, 0xaa
