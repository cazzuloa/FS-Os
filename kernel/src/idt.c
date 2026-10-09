// idt.c — Construction et chargement de l'IDT.
// Voir idt.h pour la théorie. Ce fichier fait :
//   1. Définir la table IDT (256 entrées) + le pointeur IDTR.
//   2. Remplir les entrées 0-31 (exceptions) et 32-47 (IRQ).
//   3. Charger avec LIDT, remapper le PIC, activer les interruptions.

#include "idt.h"
#include "pic.h"
#include "io.h"
#include "vga.h"

// --- Format d'une entrée IDT (8 octets, cf. manuel Intel vol.3) ---
typedef struct {
    unsigned short base_low;   // Bits 0-15 de l'adresse du handler.
    unsigned short sel;        // Sélecteur de segment code (0x08 chez nous).
    unsigned char  always0;    // Toujours 0 (réservé Intel).
    unsigned char  flags;      // Type/attributs : 0x8E = présente, ring0, int gate 32-bit.
    unsigned short base_high;  // Bits 16-31 de l'adresse du handler.
} __attribute__((packed)) idt_entry_t;

// --- Pointeur IDTR pour l'instruction LIDT (6 octets) ---
typedef struct {
    unsigned short limit;  // Taille de l'IDT - 1 (ex: 256*8-1 = 2047).
    unsigned int   base;   // Adresse linéaire de idt[].
} __attribute__((packed)) idt_ptr_t;

// La table + son pointeur. "static" = visibles seulement ici.
static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idt_p;

// Messages d'exceptions en français, indexés par vecteur 0-31.
// Servent au handler à dire POURQUOI on a planté.
static const char *exception_messages[32] = {
    "Division par zero",            // 0
    "Debug",                        // 1
    "NMI",                          // 2
    "Breakpoint (int3)",            // 3
    "Overflow",                     // 4
    "Bound Range Exceeded",         // 5
    "Opcode invalide",              // 6
    "FPU absente",                  // 7
    "Double Fault",                 // 8
    "Coprocessor Segment Overrun",  // 9
    "TSS invalide",                 // 10
    "Segment absent",               // 11
    "Stack Fault",                  // 12
    "General Protection Fault",     // 13
    "Page Fault",                   // 14
    "Reserve",                      // 15
    "x87 FPU",                      // 16
    "Alignment Check",              // 17
    "Machine Check",                // 18
    "SIMD",                         // 19
    "Virtualization",               // 20
    "Control Protection",           // 21
    "Reserve", "Reserve", "Reserve", "Reserve", "Reserve",  // 22-26
    "Reserve", "Reserve", "Reserve", "Reserve", "Reserve"   // 27-31
};

// Déclarations des stubs asm (définis dans isr.asm).
// "extern" = "ce symbole existe ailleurs, le linker le résoudra".
extern void isr0(void);  extern void isr1(void);
extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);
extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void);
extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void);
extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void);
extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);
extern void irq0(void);  extern void irq1(void);
extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);
extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void);
extern void irq14(void); extern void irq15(void);

// Table des handlers d'IRQ enregistrés (un par IRQ 0-15).
//   0 = aucune action (IRQ ignorée après EOI). Remplie via
//   irq_install_handler(). Exemple : keyboard_init() y met son handler
//   pour IRQ1. Séparée de l'IDT : l'IDT pointe vers les stubs asm,
//   qui appellent irq_handler_c, qui dispatche vers cette table.
static irq_handler_t irq_handlers[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                          0, 0, 0, 0, 0, 0, 0, 0};

// irq_install_handler : voir idt.h. Pas de protection concurrence ici :
// appelé au boot, interruptions masquées ou avant STI pour le clavier.
void irq_install_handler(unsigned char irq, irq_handler_t handler) {
    // Garde-fou : ignorer les numéros hors 0-15 (bug d'appel).
    if (irq >= 16) {
        return;
    }
    irq_handlers[irq] = handler;
}

// idt_set_gate : remplit UNE entrée de l'IDT.
//   num   : vecteur 0-255.
//   base  : adresse du stub asm (ex: (unsigned int)isr0).
//   sel   : sélecteur GDT (0x08 = code ring0 posé par le bootloader).
//   flags : 0x8E = 1 00 0 1110b -> présente, DPL=0, porte 32-bit.
static void idt_set_gate(unsigned char num, unsigned int base,
                          unsigned short sel, unsigned char flags) {
    idt[num].base_low  = (unsigned short)(base & 0xFFFF);
    idt[num].base_high = (unsigned short)((base >> 16) & 0xFFFF);
    idt[num].sel       = sel;
    idt[num].always0   = 0;
    idt[num].flags     = flags;
}

// isr_handler_c : appelé par isr_common (isr.asm) pour les vecteurs 0-31.
//   r : image des registres (voir regs_t).
//   Comportement étape 1 : affiche le message d'erreur en rouge et
//   fige le CPU (hlt en boucle). Les exceptions sont fatales : on ne
//   peut pas "reprendre" sans les traiter (sauf int3 de test).
void isr_handler_c(regs_t *r) {
    // int3 (breakpoint, vecteur 3) sert de test volontaire dans
    // kernel_main : on l'affiche en vert et on REVIENT (pas de halt).
    if (r->int_no == 3) {
        vga_setcolor(2, 0);  // Vert sur noir : c'est un test, pas un crash.
        vga_puts("[IDT] breakpoint OK (int3 intercepte)\n");
        vga_setcolor(7, 0);  // Retour au gris clair par défaut.
        return;
    }

    // Vraie exception : message rouge + halt définitif.
    vga_setcolor(4, 0);  // Rouge sur noir.
    vga_puts("[EXCEPTION] ");
    if (r->int_no < 32) {
        vga_puts(exception_messages[r->int_no]);
    } else {
        vga_puts("vecteur inconnu");
    }
    vga_puts(" (err=0x?) -> HALT\n");
    // NOTE étape 1 : on n'affiche pas encore le code d'erreur en hexa,
    // faute de printk %x (prévu étape 3). Le "?" est volontaire.

    // Figer le CPU proprement : hlt en boucle (économise énergie/QEMU).
    for (;;) {
        __asm__ volatile("hlt");
    }
}

// irq_handler_c : appelé par irq_common pour les vecteurs 32-47.
//   Dispatche vers le handler enregistré (table irq_handlers), puis
//   envoie EOI dans TOUS les cas pour ne pas bloquer le PIC.
//   Étape 1 : table vide -> on ne faisait que l'EOI.
//   Étape 2 : IRQ1 -> keyboard_irq_handler() remplit le buffer clavier.
void irq_handler_c(regs_t *r) {
    // r->int_no vaut 32-47, l'IRQ vaut int_no - 32.
    // Exemple : IRQ1 clavier -> vecteur 33 -> irq = 1.
    unsigned char irq = (unsigned char)(r->int_no - 32);
    // Sécurité : vecteur hors 32-47 (bug IDT) -> ignorer sans EOI ciblé.
    if (irq >= 16) {
        return;
    }
    // Appeler le driver enregistré s'il existe (ex: clavier sur IRQ1).
    // Le handler NE DOIT PAS envoyer l'EOI lui-même : c'est fait ci-dessous.
    if (irq_handlers[irq] != 0) {
        irq_handlers[irq](r);
    }
    pic_send_eoi(irq);
}

// idt_init : point d'entrée à appeler depuis kernel_main.
void idt_init(void) {
    // Tables de correspondance vecteur -> stub asm.
    // L'ordre DOIT suivre les vecteurs : isr0 = vecteur 0, etc.
    static void (*isr_stubs[32])(void) = {
        isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
        isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };
    static void (*irq_stubs[16])(void) = {
        irq0,  irq1,  irq2,  irq3,  irq4,  irq5,  irq6,  irq7,
        irq8,  irq9,  irq10, irq11, irq12, irq13, irq14, irq15
    };

    // 1. Remapper le PIC AVANT d'activer les interruptions, sinon les
    //    IRQ 0-15 écrasent les exceptions CPU 0-15 (conflit historique).
    pic_remap(PIC1_OFFSET, PIC2_OFFSET);

    // 2. Remplir les 32 portes d'exceptions + 16 portes d'IRQ.
    //    sel = 0x08 (segment code ring0 de notre GDT),
    //    flags = 0x8E (présente, DPL0, interrupt gate 32-bit).
    for (int i = 0; i < 32; i++) {
        idt_set_gate((unsigned char)i, (unsigned int)isr_stubs[i], 0x08, 0x8E);
    }
    for (int i = 0; i < 16; i++) {
        idt_set_gate((unsigned char)(32 + i), (unsigned int)irq_stubs[i], 0x08, 0x8E);
    }

    // 3. Masquer TOUTES les IRQ pour l'étape 1 : on veut tester les
    //    interruptions logicielles (int3) sans être parasité par le
    //    timer (IRQ0) qui tirerait 18x/seconde. Étape 2 démasquera IRQ1.
    for (unsigned char irq = 0; irq < 16; irq++) {
        pic_set_mask(irq, 1);
    }

    // 4. Charger l'IDTR et activer les interruptions.
    idt_p.limit = (unsigned short)(sizeof(idt) - 1);  // 256*8-1 = 2047.
    idt_p.base  = (unsigned int)&idt;
    __asm__ volatile("lidt %0" :: "m"(idt_p));  // Charge IDTR.
    __asm__ volatile("sti");                    // Autorise les int (flag IF).
}
