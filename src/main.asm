[org 0x7c00]
bits 16
start:
    ; Set up the stack
    cli
    xor ax, ax
    mov ss, ax
    mov sp, 0x7c00

    ; Print "Hello, World!" to the screen
    mov si, hello_msg
print_loop:
    lodsb
    or al, al
    jz done
    mov ah, 0x0e
    int 0x10
    jmp print_loop

done:
    hlt
    jmp done

times 510 - ($ - $$) db 0
db 0x55, 0xaa

hello_msg db 'Hello, World!', 0x0D, 0x0A, 0
