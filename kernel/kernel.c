// kernel.c — kernel minimal "Hello from Kernel !"
// Chargé à 0x1000 par le bootloader, entré via _start en 32-bit.

#define VGA_ADDR   0xB8000
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define GREEN_ON_BLACK 0x0A

void _start(void)
{
    const char *msg = "Hello from Kernel !";
    volatile unsigned short *vga = (volatile unsigned short *)VGA_ADDR;

    // Nettoie l'écran (caractères bootloader encore visibles)
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga[i] = (unsigned short)(0x0700 | ' ');

    // Affiche le message en haut à gauche, en vert
    for (int i = 0; msg[i] != '\0'; i++)
        vga[i] = (unsigned short)((GREEN_ON_BLACK << 8) | msg[i]);

    // Boucle infinie, sans brûler le CPU
    for (;;)
        __asm__ volatile("hlt");
}