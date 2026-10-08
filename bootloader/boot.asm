[bits 16]
[org 0x7C00]

VIDEO_MEMORY equ 0xB800
COLOR_RED    equ 0x0C
COLOR_GREEN  equ 0x0A
COLOR_WHITE  equ 0x07
jmp short start
nop

msg            db "CA MARCHE OMGGG !", 0
disk_error_msg db "[-] : Erreur lecture disque !", 0
boot_drive     db 0

; -------------------------------------------------------
start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl        ; Sauvegarder le numéro de disque

    mov si, msg
    mov di, 2240
    mov ah, COLOR_RED
    call print_string_color

    mov si, msg
    mov di, 2400
    mov ah, COLOR_GREEN
    call print_string_color

    mov si, msg
    mov di, 2560
    mov ah, COLOR_WHITE
    call print_string_color

    call load_kernel
    call enter_pm
    jmp $

; -------------------------------------------------------
; print_string_color : SI=chaîne, DI=offset vidéo, AH=couleur
; -------------------------------------------------------
print_string_color:
    push es
    push bx
    mov bx, VIDEO_MEMORY
    mov es, bx
    cld
.loop:
    lodsb
    or al, al
    jz .done
    mov [es:di], al
    mov [es:di+1], ah
    add di, 2
    jmp .loop
.done:
    pop bx
    pop es
    ret

; -------------------------------------------------------
; load_kernel : charge 16 secteurs depuis le secteur 2 vers 0x0000:0x1000
; -------------------------------------------------------
load_kernel:
    xor ax, ax
    mov es, ax              ; ES = 0x0000
    mov bx, 0x1000          ; BX = 0x1000  →  destination = 0x1000

    mov ah, 0x02            ; INT 13h : lire secteurs
    mov al, 16              ; 16 secteurs = 8 Ko
    mov ch, 0               ; Cylindre 0
    mov cl, 2               ; Secteur 2
    mov dh, 0               ; Tête 0
    mov dl, [boot_drive]
    int 0x13
    jc .disk_err
    ret
.disk_err:
    mov si, disk_error_msg
    mov di, 160
    mov ah, COLOR_RED
    call print_string_color
    hlt

; -------------------------------------------------------
; enter_pm : bascule en mode protégé 32-bit
; -------------------------------------------------------
enter_pm:
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG:init_pm

; -------------------------------------------------------
[bits 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov esp, 0x90000
    mov ebp, esp
    jmp CODE_SEG:0x1000         ; kernel_main

; -------------------------------------------------------
; GDT
; -------------------------------------------------------
gdt_start:
    dq 0x0000000000000000       ; Descripteur nul

gdt_code:
    dw 0xFFFF                   ; Limite 0-15
    dw 0x0000                   ; Base  0-15
    db 0x00                     ; Base  16-23
    db 10011010b                ; Accès : présent, ring0, code, exec/read
    db 11001111b                ; Flags : 4KB, 32-bit + limite 16-19
    db 0x00                     ; Base  24-31

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b                ; Accès : présent, ring0, data, read/write
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

times 510-($-$$) db 0
dw 0xAA55