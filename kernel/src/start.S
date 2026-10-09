// start.S — Point d'entrée réel du kernel (syntaxe GAS, 32-bit).
// ----------------------------------------------------------------
// Pourquoi ce fichier ? Le linker sort un binaire PLAT (kernel.ld) où
// l'ordre des sections suit l'ordre des .o sur la ligne de commande.
// compile.sh compile start.S EN PREMIER pour garantir que _start est
// à l'adresse 0x1000, là où le bootloader saute (jmp 0x1000).
// Sans ça, _start pourrait se retrouver après idt.o/pic.o et le boot
// sauterait au milieu de nulle part.
//
// Le bootloader a déjà : GDT flat, mode protégé 32-bit, pile à 0x90000.
// Ici on se contente d'appeler kernel_main (défini en C) puis de figer.

.code32
.text
.globl _start        // Rend _start visible pour le linker (ENTRY(_start)).
.extern kernel_main  // Défini dans kernel/src/kernel.c.

_start:
    call kernel_main // Appelle le vrai main C. Il ne doit pas revenir...
    cli              // ...mais par sécurité on coupe les interruptions...
.hang:
    hlt              // ...et on fige le CPU en boucle économe.
    jmp .hang
