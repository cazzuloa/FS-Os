// io.h — Accès bas niveau aux ports d'E/S x86.
// ---------------------------------------------------------------
// Sur x86, les périphériques (PIC, clavier, écran VGA...) se
// pilotent via des ports d'E/S avec les instructions assembleur
// IN (lire) et OUT (écrire). Le C ne sait pas les faire seul,
// d'où ces petites fonctions inline en assembleur.
//
// Ports utilisés plus tard :
//   0x20/0x21 : PIC maître (commande/données)
//   0xA0/0xA1 : PIC esclave
//   0x60/0x64 : contrôleur clavier PS/2
//   0x3D4/0x3D5 : curseur VGA

#ifndef IO_H
#define IO_H

// inb : lit 1 octet depuis le port donné.
//   port : numéro du port (ex: 0x60 pour les scancodes clavier).
//   retour : l'octet lu.
static inline unsigned char inb(unsigned short port) {
    unsigned char val;
    // "inb %1, %0" : lit le port %1 (contrainte "Nd") vers %0 ("=a").
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

// outb : écrit 1 octet vers le port donné.
//   port : numéro du port (ex: 0x20 pour commander le PIC).
//   val  : valeur à envoyer.
static inline void outb(unsigned short port, unsigned char val) {
    // "outb %0, %1" : écrit %0 vers le port %1.
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

// io_wait : petite temporisation d'E/S.
//   Pourquoi ? Le PIC 8259 est lent : après chaque OUTB de
//   configuration, il faut laisser ~1µs au bus. L'astuce classique
//   est d'écrire sur le port 0x80 (utilisé historiquement pour les
//   checkpoints POST, sans effet de bord).
static inline void io_wait(void) {
    __asm__ volatile("outb %%al, $0x80" :: "a"(0));
}

#endif
