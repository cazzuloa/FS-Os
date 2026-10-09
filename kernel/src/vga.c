// vga.c — Pilote d'affichage VGA en mode texte 80x25.
// ---------------------------------------------------------------
// La mémoire vidéo texte est mappée à 0xB8000 : chaque case = 2 octets
// (octet caractère + octet couleur). Écrire ici affiche à l'écran.
// Le curseur matériel se pilote via les ports 0x3D4 (index) / 0x3D5
// (données) du contrôleur CRT.

#include "vga.h"
#include "io.h"  // outb() commun (aussi utilisé par PIC/clavier).

#define VGA_ADDR   0xB8000  // Base de la mémoire vidéo texte.
#define VGA_WIDTH  80       // 80 colonnes en mode texte standard.
#define VGA_HEIGHT 25       // 25 lignes.

// État interne du pilote : position du curseur + couleur courante.
//   vga_color : octet attribut = (fond << 4) | (premier plan & 0x0F).
//   Exemples : 0x07 = gris sur noir, 0x0A = vert sur noir, 0x04 = rouge.
static int cursor_x = 0;
static int cursor_y = 0;
static unsigned char vga_color = 0x07;
static volatile unsigned short *vga = (volatile unsigned short*)VGA_ADDR;

// update_cursor : déplace le curseur matériel à (cursor_x, cursor_y).
//   Protocole CRT : envoyer 0x0F (poids faible) puis 0x0E (poids fort)
//   de la position linéaire (y * 80 + x) via 0x3D4/0x3D5.
static void update_cursor(void) {
    unsigned short pos = (unsigned short)(cursor_y * VGA_WIDTH + cursor_x);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
}

void vga_setcolor(unsigned char fg, unsigned char bg) {
    vga_color = (unsigned char)((bg << 4) | (fg & 0x0F));
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga[i] = (unsigned short)((vga_color << 8) | ' ');
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

// scroll : remonte tout l'écran d'une ligne quand le curseur dépasse.
//   On recopie les lignes 1-24 vers 0-23, puis on vide la ligne 24.
static void scroll(void) {
    if (cursor_y < VGA_HEIGHT) return;
    for (int y = 1; y < VGA_HEIGHT; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            vga[(y-1)*VGA_WIDTH+x] = vga[y*VGA_WIDTH+x];
    for (int x = 0; x < VGA_WIDTH; x++)
        vga[(VGA_HEIGHT-1)*VGA_WIDTH+x] = (unsigned short)((vga_color << 8) | ' ');
    cursor_y = VGA_HEIGHT - 1;
}

void vga_putc(char c) {
    // '\b' (Retour arrière, 0x08) : recule d'une case et l'efface.
    // Nécessaire pour l'écho clavier et le futur shell (readline).
    // Cas limite : en début de ligne on remonte à la fin de la précédente
    // (sauf tout en haut de l'écran où on reste à 0,0).
    if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = VGA_WIDTH - 1;
        }
        vga[cursor_y * VGA_WIDTH + cursor_x] = (unsigned short)((vga_color << 8) | ' ');
    } else if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else {
        vga[cursor_y * VGA_WIDTH + cursor_x] = (unsigned short)((vga_color << 8) | c);
        cursor_x++;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }
    scroll();
    update_cursor();
}

void vga_puts(const char *s) {
    for (int i = 0; s[i] != '\0'; i++)
        vga_putc(s[i]);
}
