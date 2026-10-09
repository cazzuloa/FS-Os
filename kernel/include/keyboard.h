// keyboard.h — Pilote clavier PS/2 (scancode set 1, layout US).
// ---------------------------------------------------------------
// Comment ça marche :
//   - Chaque touche pressée/relâchée envoie un "scancode" sur le port
//     0x60 et déclenche IRQ1 (vecteur 33 après remap du PIC).
//   - Le handler IRQ lit le scancode, le traduit en ASCII via une table
//     et le pousse dans un buffer circulaire.
//   - Le reste du kernel consomme avec kgetc() (bloquant) ou
//     keyboard_has_key() + kgetc() (non-bloquant).
//
// Limites volontaires étape 2 (documentées, pas des bugs oubliés) :
//   - Layout US QWERTY uniquement (QEMU défaut). AZERTY = étape future.
//   - Touches étendues (préfixe 0xE0 : flèches, Suppr...) ignorées.
//   - Pas de Ctrl/Alt applicatif : suivis mais non exposés.

#ifndef KEYBOARD_H
#define KEYBOARD_H

// keyboard_init : vide le buffer, enregistre le handler sur IRQ1 via
//   irq_install_handler(), puis démasque IRQ1 au PIC.
//   À appeler UNE fois après idt_init().
void keyboard_init(void);

// keyboard_has_key : 1 si le buffer contient au moins un caractère.
//   Non-bloquant, utilisable pour du polling.
int keyboard_has_key(void);

// kgetc : rend le prochain caractère du buffer (ASCII).
//   Bloquant : dort en "hlt" tant que le buffer est vide.
//   Retour : caractère ASCII ('a', '1', '\n' pour Entrée, '\b' pour
//   Retour arrière). Les interruptions doivent être actives (sti).
char kgetc(void);

#endif
