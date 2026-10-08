#define VGA_ADDR 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static int cursor_x = 0;
static int cursor_y = 0;
static unsigned char vga_color = 0x07;
static volatile unsigned short *vga = (volatile unsigned short*)VGA_ADDR;

static void outb(unsigned short port, unsigned char val) {
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

static void update_cursor(void) {
    unsigned short pos = cursor_y * VGA_WIDTH + cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
}

void vga_setcolor(unsigned char fg, unsigned char bg) {
    vga_color = (bg << 4) | (fg & 0x0F);
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga[i] = (vga_color << 8) | ' ';
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

static void scroll(void) {
    if (cursor_y < VGA_HEIGHT) return;
    for (int y = 1; y < VGA_HEIGHT; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            vga[(y-1)*VGA_WIDTH+x] = vga[y*VGA_WIDTH+x];
    for (int x = 0; x < VGA_WIDTH; x++)
        vga[(VGA_HEIGHT-1)*VGA_WIDTH+x] = (vga_color << 8) | ' ';
    cursor_y = VGA_HEIGHT - 1;
}

void vga_putc(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else {
        vga[cursor_y * VGA_WIDTH + cursor_x] = (vga_color << 8) | c;
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