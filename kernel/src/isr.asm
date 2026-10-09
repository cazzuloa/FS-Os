; isr.asm — Stubs assembleur des interruptions (32-bit, NASM elf32).
; ----------------------------------------------------------------
; Pourquoi ce fichier ? Quand le CPU prend une interruption, il pousse
; EFLAGS/CS/EIP (+ code d'erreur pour certaines exceptions) puis saute
; à l'adresse de l'IDT. On ne peut pas écrire ce prologue en C : il faut
; de l'asm pour sauver TOUS les registres, passer en segment noyau,
; appeler le handler C, restaurer et faire IRET.
;
; Convention utilisée ici (doit matcher regs_t dans idt.h) :
;   - exceptions SANS code d'erreur CPU : on push un faux 0.
;   - exceptions AVEC code d'erreur (8,10-14,17,21) : le CPU l'a déjà push.
;   - dans tous les cas on push le numéro de vecteur, puis on saute
;     vers le stub commun qui fait pusha + appel C.

[bits 32]
section .text

; Les handlers C définis dans idt.c.
extern isr_handler_c
extern irq_handler_c

; --- Macros pour générer les 48 stubs sans répéter 48 fois le code ---
%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0        ; faux code d'erreur pour uniformiser la pile
    push dword %1       ; numéro du vecteur
    jmp isr_common      ; suite commune
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    ; Pas de push 0 : le CPU a déjà empilé le vrai code d'erreur.
    push dword %1       ; numéro du vecteur
    jmp isr_common
%endmacro

%macro IRQ_STUB 2
global irq%1
irq%1:
    push dword 0        ; pas de code d'erreur pour les IRQ
    push dword %2       ; vecteur = 32 + irq (ex: IRQ1 -> 33)
    jmp irq_common
%endmacro

; --- 32 exceptions CPU ---
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8    ; Double Fault (code d'erreur)
ISR_NOERR 9
ISR_ERR   10   ; TSS invalide
ISR_ERR   11   ; Segment absent
ISR_ERR   12   ; Stack fault
ISR_ERR   13   ; General Protection Fault
ISR_ERR   14   ; Page Fault
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17   ; Alignment Check
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21   ; Control Protection (CET, CPU récents)
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; --- 16 IRQ matérielles (vecteurs 32-47 après remap) ---
IRQ_STUB 0, 32
IRQ_STUB 1, 33
IRQ_STUB 2, 34
IRQ_STUB 3, 35
IRQ_STUB 4, 36
IRQ_STUB 5, 37
IRQ_STUB 6, 38
IRQ_STUB 7, 39
IRQ_STUB 8, 40
IRQ_STUB 9, 41
IRQ_STUB 10, 42
IRQ_STUB 11, 43
IRQ_STUB 12, 44
IRQ_STUB 13, 45
IRQ_STUB 14, 46
IRQ_STUB 15, 47

; --- Stub commun des exceptions ---
; Pile à l'entrée : [int_no] [err_code] [EIP] [CS] [EFLAGS] ...
global isr_common
isr_common:
    pusha               ; sauve EAX..EDI (8 registres)
    mov ax, ds          ; sauve DS...
    push eax
    mov ax, 0x10        ; ...et passe en segment data noyau
    mov ds, ax          ; (0x10 = DATA_SEG de la GDT du bootloader)
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp            ; passe un pointeur regs_t* au handler C
    call isr_handler_c
    add esp, 4          ; nettoie l'argument (convention cdecl)
    pop ebx             ; restaure DS/ES/FS/GS d'origine
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx
    popa                ; restaure les registres généraux
    add esp, 8          ; enlève int_no + err_code poussés au début
    iret                ; retour d'interruption (restaure EIP/CS/EFLAGS)

; --- Stub commun des IRQ (identique, mais appelle irq_handler_c) ---
global irq_common
irq_common:
    pusha
    mov ax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    call irq_handler_c
    add esp, 4
    pop ebx
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx
    popa
    add esp, 8
    iret
