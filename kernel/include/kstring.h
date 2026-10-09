// kstring.h — Mini-libc chaînes/mémoire (freestanding, sans libc).
// ---------------------------------------------------------------
// Pourquoi ? En freestanding (-nostdlib -nostdinc), pas de <string.h>.
// Le shell a besoin de strlen/strcmp/strncmp, et le compilateur peut
// lui-même émettre des appels à memcpy/memset : on les fournit aussi.
// Toutes les fonctions sont documentées avec IN/OUT comme le reste du projet.

#ifndef KSTRING_H
#define KSTRING_H

// size_t maison : gcc -nostdinc ne fournit pas <stddef.h>.
// Sur x86 32-bit, un unsigned int (32-bit) suffit pour les tailles.
typedef unsigned int size_t;

// --- Chaînes (toujours terminées par '\0') ---
size_t strlen(const char *s);                          // Longueur sans le '\0'.
int    strcmp(const char *a, const char *b);           // 0 si égales.
int    strncmp(const char *a, const char *b, size_t n);// Compare au plus n octets.
char  *strcpy(char *dst, const char *src);             // Copie + '\0'. dst assez grand !

// --- Mémoire brute ---
void  *memset(void *dst, int c, size_t n);             // Remplit n octets.
void  *memcpy(void *dst, const void *src, size_t n);   // Copie n octets.
int    memcmp(const void *a, const void *b, size_t n); // 0 si identiques.

#endif
