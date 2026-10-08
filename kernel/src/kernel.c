// kernel.c — kernel minimal "Hello from Kernel !"
// Chargé à 0x1000 par le bootloader, entré via _start en 32-bit.

#include "vga.h"

void _start(void) {
    vga_clear();
    vga_puts("hello\ntest\n");

    // test scroll : 5 lignes
    for (int i = 0; i < 5; i++) {
        vga_puts("ligne ");
        vga_putc('0' + (i % 10));
        vga_putc('\n');
    }

    for (;;) __asm__ volatile("hlt");
}