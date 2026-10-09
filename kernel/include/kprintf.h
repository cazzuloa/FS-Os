// kprintf.h — printk(), le printf du noyau (vers VGA).
// ---------------------------------------------------------------
// vga_puts() ne suffit plus dès qu'on veut afficher des nombres
// (adresse d'une exception, code d'erreur...). printk() formate vers
// vga_putc(). Formats supportés, volontairement restreints :
//   %c : caractère, %s : chaîne, %d : décimal signé,
//   %u : décimal non signé, %x : hexa minuscule (sans 0x),
//   %p : pointeur (0x + hexa), %% : signe pourcent.
// Tout autre %X est affiché tel quel ("%X") pour signaler l'oubli.

#ifndef KPRINTF_H
#define KPRINTF_H

// printk : affiche formaté sur l'écran VGA.
//   fmt : chaîne de format (voir ci-dessus).
//   ... : arguments variables (convention cdecl, comme printf).
void printk(const char *fmt, ...);

#endif
