#ifndef VGA_H
#define VGA_H

void vga_clear(void);
void vga_setcolor(unsigned char fg, unsigned char bg);
void vga_putc(char c);
void vga_puts(const char *s);

#endif