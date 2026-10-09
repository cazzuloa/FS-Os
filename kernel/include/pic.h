// pic.h — Pilote du PIC 8259 (Programmable Interrupt Controller).
// ---------------------------------------------------------------
// Contexte : en mode protégé, le BIOS ne nous aide plus. Les IRQs
// matérielles (timer, clavier...) arrivent au CPU via deux PIC en
// cascade (maître + esclave). Par défaut, ils renvoient les IRQs
// vers les vecteurs 0-15, qui collisionnent avec les exceptions CPU
// (0-31). Il FAUT donc les "remapper" vers 32-47.
//
// Ports :
//   PIC1 (maître) : commande 0x20, données 0x21
//   PIC2 (esclave): commande 0xA0, données 0xA1

#ifndef PIC_H
#define PIC_H

// Vecteurs choisis après remap : IRQ0-7 -> 32-39, IRQ8-15 -> 40-47.
// 32 est le premier vecteur libre après les 32 exceptions CPU.
#define PIC1_OFFSET 32
#define PIC2_OFFSET 40

// pic_remap : reprogramme les deux PIC avec la séquence ICW1-ICW4.
//   offset1 : vecteur de base du maître (32 chez nous).
//   offset2 : vecteur de base de l'esclave (40 chez nous).
//   Détail de la séquence (à comprendre une fois, puis à recopier) :
//     ICW1 = 0x11 : mode cascade + on attend ICW4.
//     ICW2 = offset : vecteur de base.
//     ICW3 maître = 0x04 (bit 2 : esclave sur IRQ2), esclave = 0x02.
//     ICW4 = 0x01 : mode 8086 (et non 8080).
void pic_remap(int offset1, int offset2);

// pic_send_eoi : signale la fin d'interruption (End Of Interrupt).
//   irq : numéro d'IRQ 0-15.
//   Pourquoi ? Tant qu'on n'envoie pas EOI (0x20), le PIC ne
//   transmet plus d'IRQ de priorité égale ou inférieure -> tout
//   se fige. Si IRQ >= 8, il faut prévenir l'esclave ET le maître.
void pic_send_eoi(unsigned char irq);

// pic_set_mask : masque/démasque une IRQ (1 = masquée/ignorée).
//   Utile étape 1 : tout masquer (0xFF) pour tester l'IDT sans être
//   parasité par le timer. Étape 2 : démasquer le clavier (IRQ1).
void pic_set_mask(unsigned char irq, int masked);

#endif
