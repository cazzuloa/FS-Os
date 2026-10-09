// idt.h — Table des descripteurs d'interruptions (IDT).
// ---------------------------------------------------------------
// L'IDT dit au CPU où sauter quand survient :
//   - une exception CPU (vecteurs 0-31 : division par zéro, page fault...),
//   - une IRQ matérielle (vecteurs 32-47 après remap du PIC),
//   - une interruption logicielle (ex: "int $0x80").
//
// Chaque entrée (8 octets) contient l'adresse du handler, le sélecteur
// de segment code (0x08 = notre segment code ring0 de la GDT posée
// dans bootloader/boot.asm) et des flags (0x8E = présente, ring0,
// porte d'interruption 32-bit).

#ifndef IDT_H
#define IDT_H

// Nombre total de vecteurs x86 : 256 (0-255).
#define IDT_ENTRIES 256

// regs_t : image de la pile au moment où le stub asm appelle le
// handler C. L'ordre DOIT correspondre exactement aux push faits
// dans kernel/src/isr.asm (pusha + push ds + push int_no/err_code).
// Si tu modifies isr.asm, mets cette struct à jour aussi.
typedef struct {
    unsigned int ds;                          // Segment de données sauvé.
    unsigned int edi, esi, ebp, esp_dummy;    // Registres de pusha...
    unsigned int ebx, edx, ecx, eax;          // ... (suite, ordre pusha).
    unsigned int int_no;    // Numéro de vecteur (0-47 pour nous).
    unsigned int err_code;  // Code d'erreur CPU (ou 0 factice).
    unsigned int eip, cs, eflags;  // Poussés automatiquement par le CPU.
    unsigned int useresp, ss;      // Seulement si changement de ring.
} regs_t;

// idt_init : remap le PIC, remplit l'IDT (exceptions + IRQ),
// charge le registre IDTR avec LIDT, masque tout, puis STI.
// À appeler UNE fois au démarrage depuis kernel_main.
void idt_init(void);

// Handlers C appelés depuis les stubs asm (isr.asm) :
//   isr_handler_c : exceptions 0-31 + interruptions logicielles.
//   irq_handler_c : IRQ matérielles 32-47 (dispatche puis EOI au PIC).
void isr_handler_c(regs_t *r);
void irq_handler_c(regs_t *r);

// irq_handler_t : signature d'un handler d'IRQ enregistrable.
//   Exemple : le clavier enregistre son handler pour IRQ1 via
//   irq_install_handler(1, keyboard_irq_handler).
typedef void (*irq_handler_t)(regs_t *r);

// irq_install_handler : enregistre un handler C pour une IRQ 0-15.
//   irq     : 0-15 (ex: 1 = clavier).
//   handler : fonction appelée par irq_handler_c avant l'EOI.
//   Pourquoi une table ? idt.c reste générique : il ne connaît pas le
//   clavier, c'est keyboard_init() qui s'enregistre tout seul.
void irq_install_handler(unsigned char irq, irq_handler_t handler);

#endif
