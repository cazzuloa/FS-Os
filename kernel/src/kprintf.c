// kprintf.c — printk() vers VGA. Voir kprintf.h pour les formats.
//
// Note va_args : compilé avec -nostdinc, donc PAS de <stdarg.h>.
// On utilise les builtins GCC (__builtin_va_list...) qui marchent sans
// header. Les macros va_start/va_arg ci-dessous sont la version minimale
// suffisante pour ce projet (x86 32-bit, cdecl).

#include "kprintf.h"
#include "vga.h"

// --- Mini va_args sur builtins GCC (pas de <stdarg.h> en freestanding) ---
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)

// print_uint : affiche un non-signé en base 10 ou 16 (récursif simple).
//   v    : valeur à afficher.
//   base : 10 (décimal) ou 16 (hexa minuscules).
static void print_uint(unsigned int v, int base) {
    // Chiffres hexa minuscules (0-9a-f), aussi valables pour base 10.
    const char *digits = "0123456789abcdef";
    // Récursion : afficher les poids forts d'abord. Profondeur max ~10
    // (32-bit en décimal), sans danger pour notre pile à 0x90000.
    if (v >= (unsigned int)base) {
        print_uint(v / (unsigned int)base, base);
    }
    vga_putc(digits[v % (unsigned int)base]);
}

// print_int : affiche un signé en décimal (gère le '-' puis délègue).
static void print_int(int v) {
    if (v < 0) {
        vga_putc('-');
        // Attention : -INT_MIN déborde en théorie ; ignoré ici (pédagogique).
        v = -v;
    }
    print_uint((unsigned int)v, 10);
}

void printk(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    for (int i = 0; fmt[i] != '\0'; i++) {
        // Caractère normal : recopier tel quel (vga_putc gère \n et \b).
        if (fmt[i] != '%') {
            vga_putc(fmt[i]);
            continue;
        }
        // '%' rencontré : regarder le spécificateur suivant.
        i++;
        char spec = fmt[i];
        if (spec == '\0') {
            break;  // '%' en fin de chaîne : on s'arrête (format tronqué).
        }
        switch (spec) {
            case '%': vga_putc('%'); break;
            case 'c': vga_putc((char)va_arg(ap, int)); break;  // char promu en int.
            case 's': {
                // Chaîne 0 -> "(null)" comme la libc (évite un crash).
                const char *s = va_arg(ap, const char *);
                if (!s) {
                    s = "(null)";
                }
                vga_puts(s);
                break;
            }
            case 'd': print_int(va_arg(ap, int)); break;
            case 'u': print_uint(va_arg(ap, unsigned int), 10); break;
            case 'x': print_uint(va_arg(ap, unsigned int), 16); break;
            case 'p': {
                // Pointeur : "0x" + hexa (largeur non paddée, volontaire).
                vga_puts("0x");
                print_uint(va_arg(ap, unsigned int), 16);
                break;
            }
            default:
                // Spécificateur inconnu : afficher "%X" tel quel pour que
                // l'erreur de format soit VISIBLE au lieu d'être silencieuse.
                vga_putc('%');
                vga_putc(spec);
                break;
        }
    }
    va_end(ap);
}
