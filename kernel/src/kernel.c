// kernel.c — Point d'entrée C du noyau (appelé par start.S).
// ---------------------------------------------------------------
// Séquence de boot : VGA -> IDT/PIC -> clavier (IRQ1) -> shell.
// Le shell (shell_run) ne revient jamais : c'est la boucle finale.

#include "vga.h"
#include "idt.h"
#include "keyboard.h"
#include "shell.h"

// kernel_main : appelé par _start (start.S). Ne doit jamais revenir.
//   Le bootloader a déjà mis la pile à 0x90000.
void kernel_main(void) {
    // Écran propre + couleur par défaut (gris clair sur noir).
    vga_setcolor(7, 0);
    vga_clear();
    vga_puts("FS-Os kernel\n");
    vga_puts("[*] init IDT + PIC...\n");

    // Construit l'IDT, remappe le PIC vers 32/40, masque les IRQ, STI.
    idt_init();
    vga_puts("[+] IDT chargee. Test int3...\n");

    // Interruption logicielle n°3 : doit être interceptée par isr3
    // -> isr_common -> isr_handler_c, qui affiche un message et revient.
    // Si l'IDT est fausse : triple fault -> reboot QEMU.
    __asm__ volatile("int $3");

    vga_puts("[+] Retour de int3 : interruptions OK.\n");

    // Enregistre le handler clavier sur IRQ1 et démasque IRQ1.
    // À partir d'ici, chaque frappe remplit le buffer via IRQ1.
    vga_puts("[*] init clavier (IRQ1)...\n");
    keyboard_init();
    vga_puts("[+] Clavier OK.\n");

    // Boucle finale : prompt "fs-os> ", readline, dispatch.
    // Pour ajouter TES commandes : voir kernel/include/shell.h (3 lignes).
    shell_run();

    // Sécurité : shell_run ne revient jamais, mais si un jour ça change,
    // on fige proprement au lieu de tomber dans le vide.
    for (;;) {
        __asm__ volatile("hlt");
    }
}
