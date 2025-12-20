ORG 0x7c00
bits 16
jmp $

times 510 - ($ - $$) db 0
db 0x55, 0xaa
