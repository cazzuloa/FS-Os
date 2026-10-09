// pic.c — Implémentation du remap et du masquage du PIC 8259.
// Voir pic.h pour le pourquoi du remap.

#include "pic.h"
#include "io.h"

// Adresses des ports du PIC (cf. datasheet 8259A).
#define PIC1_CMD  0x20  // Port de commande du maître.
#define PIC1_DATA 0x21  // Port de données du maître (masques + ICW2-4).
#define PIC2_CMD  0xA0  // Port de commande de l'esclave.
#define PIC2_DATA 0xA1  // Port de données de l'esclave.
#define PIC_EOI   0x20  // Valeur "End Of Interrupt" à envoyer.

// pic_remap : voir pic.h. Chaque outb est suivi de io_wait car le
// 8259 d'origine est plus lent que le CPU.
void pic_remap(int offset1, int offset2) {
    // Sauvegarder les masques actuels pour les restaurer à la fin.
    // (Le BIOS/QEMU a déjà une config, on ne veut pas la perdre.)
    unsigned char mask1 = inb(PIC1_DATA);
    unsigned char mask2 = inb(PIC2_DATA);

    // ICW1 : 0x11 = 00010001b.
    //   bit 0 (ICW4 nécessaire) = 1, bit 1 (cascade) = 0 seul... en
    //   pratique 0x11 = "init + cascade + ICW4 à venir".
    outb(PIC1_CMD, 0x11); io_wait();
    outb(PIC2_CMD, 0x11); io_wait();

    // ICW2 : vecteur de base où le PIC va mapper IRQ0.
    outb(PIC1_DATA, (unsigned char)offset1); io_wait();
    outb(PIC2_DATA, (unsigned char)offset2); io_wait();

    // ICW3 : câblage maître/esclave.
    //   Maître : 0x04 = 00000100b -> esclave branché sur IRQ2.
    //   Esclave : 0x02 = son ID de cascade (entrée 2 du maître).
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();

    // ICW4 : 0x01 = mode 8086/88 (fonctionnement x86 moderne).
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();

    // Restaurer les masques sauvegardés (en général tout masqué
    // juste après, voir idt_init).
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_send_eoi(unsigned char irq) {
    // Si l'IRQ vient de l'esclave (8-15), prévenir l'esclave d'abord.
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);
    }
    // Puis toujours prévenir le maître (l'esclave passe par lui).
    outb(PIC1_CMD, PIC_EOI);
}

void pic_set_mask(unsigned char irq, int masked) {
    unsigned short port;
    unsigned char mask;

    // Choisir le bon PIC : IRQ 0-7 = maître, 8-15 = esclave.
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;  // Ramener à 0-7 pour le calcul du bit.
    }

    // Lire-modifier-écrire : on ne touche qu'au bit de l'IRQ.
    mask = inb(port);
    if (masked) {
        mask |= (unsigned char)(1 << irq);   // Met le bit à 1 = masque.
    } else {
        mask &= (unsigned char)~(1 << irq);  // Met le bit à 0 = autorise.
    }
    outb(port, mask);
}
