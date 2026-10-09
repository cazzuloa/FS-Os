// kstring.c — Implémentation de la mini-libc. Voir kstring.h.
// Chaque fonction est volontairement naïve et lisible : but pédagogique.

#include "kstring.h"

size_t strlen(const char *s) {
    // Compte jusqu'au '\0' (non inclus). s ne doit pas être 0.
    size_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

int strcmp(const char *a, const char *b) {
    // Compare octet par octet (non signé pour éviter les surprises avec
    // les accents > 127). Rend <0 / 0 / >0 comme la libc.
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    // Comme strcmp mais au plus n octets. n == 0 -> toujours 0 (égal).
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i] || a[i] == '\0') {
            return (unsigned char)a[i] - (unsigned char)b[i];
        }
    }
    return 0;
}

char *strcpy(char *dst, const char *src) {
    // Copie src + son '\0'. L'APPELANT garantit dst assez grand
    // (le shell utilise des buffers de 256, vérifié à l'appel).
    size_t i = 0;
    while (src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return dst;
}

void *memset(void *dst, int c, size_t n) {
    // Remplit n octets avec (c & 0xFF). Utilisé pour vider les buffers.
    unsigned char *d = (unsigned char *)dst;
    for (size_t i = 0; i < n; i++) {
        d[i] = (unsigned char)c;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n) {
    // Copie n octets (zones NON chevauchantes ; sinon il faudrait memmove).
    // Le compilateur peut appeler memcpy tout seul -> signature standard.
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *x = (const unsigned char *)a;
    const unsigned char *y = (const unsigned char *)b;
    for (size_t i = 0; i < n; i++) {
        if (x[i] != y[i]) {
            return (int)x[i] - (int)y[i];
        }
    }
    return 0;
}
