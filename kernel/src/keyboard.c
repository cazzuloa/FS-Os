// keyboard.c — Pilote clavier PS/2, scancode set 1.
// Voir keyboard.h pour le mode d'emploi et les limites.
//
// Rappels matériels :
//   0x64 : port de STATUT du contrôleur (lecture seule ici).
//          bit 0 = 1 -> le port 0x60 contient un octet à lire.
//          bit 1 = 1 -> buffer d'entrée plein (on n'écrit pas ici).
//   0x60 : port de DONNÉES (le scancode).
//   IRQ1 : déclenchée à chaque scancode. Le PIC est déjà remappé vers
//          le vecteur 33 par idt_init() (voir pic.c).

#include "keyboard.h"
#include "idt.h"   // regs_t + irq_install_handler (enregistrement IRQ1).
#include "pic.h"   // pic_set_mask (démasquer IRQ1).
#include "io.h"    // inb().

// --- Ports et bits de statut (cf. datasheet contrôleur 8042) ---
#define KB_DATA   0x60  // Données : le scancode à lire.
#define KB_STATUS 0x64  // Statut : bit 0 = données dispo.
#define KB_OUT_FULL 0x01  // Masque du bit 0 du statut.

// --- Scancodes spéciaux du set 1 (valeurs "make" = touche pressée) ---
#define SC_LSHIFT  0x2A  // Shift gauche pressé.
#define SC_RSHIFT  0x36  // Shift droit pressé.
#define SC_CAPS    0x3A  // Verr Maj (toggle à chaque pression).
#define SC_ENTER   0x1C  // Entrée -> on rend '\n'.
#define SC_BACKSP  0x0E  // Retour arrière -> on rend '\b'.
#define SC_EXTENDED 0xE0 // Préfixe des touches étendues (flèches...).
#define SC_RELEASE  0x80 // Bit : scancode >= 0x80 = touche RELÂCHÉE.

// --- Buffer circulaire des caractères ASCII prêts ---
//   Pourquoi circulaire ? L'IRQ écrit (producteur) pendant que le code
//   principal lit (consommateur), sans s'attendre. Taille 128 : assez
//   pour une ligne de shell, assez petit pour rester simple.
#define KB_BUF_SIZE 128
static char     kb_buf[KB_BUF_SIZE];
static int      kb_head = 0;  // Prochain octet à LIRE (consommateur).
static int      kb_tail = 0;  // Prochain emplacement d'ÉCRITURE (IRQ).
static int      kb_count = 0; // Nombre d'octets en attente (0-128).

// --- État des modifieurs ---
static int shift_down = 0;  // 1 tant qu'un Shift est maintenu.
static int caps_on = 0;     // 1 si Verr Maj actif (toggle sur pression).
static int ext_prefix = 0;  // 1 si on vient de voir 0xE0 (touche étendue).

// Table set 1 -> ASCII SANS Shift (layout US QWERTY).
//   Index = scancode "make" (0x00-0x39 utiles). 0 = touche non gérée.
//   Les entrées sont les caractères US : qwerty, chiffres, ponctuation.
static const char sc_normal[64] = {
    0,    0,    '1',  '2',  '3',  '4',  '5',  '6',   // 0x00-0x07
    '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t', // 0x08-0x0F
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',   // 0x10-0x17
    'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',   // 0x18-0x1F
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',   // 0x20-0x27
    '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',   // 0x28-0x2F
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',   // 0x30-0x37
    0,    ' ',  0,    0,    0,    0,    0,    0,      // 0x38-0x3F
};

// Table set 1 -> ASCII AVEC Shift (mêmes index).
//   Chiffres -> symboles (!@#$%...), lettres -> MAJUSCULES.
static const char sc_shift[64] = {
    0,    0,    '!',  '@',  '#',  '$',  '%',  '^',   // 0x00-0x07
    '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t', // 0x08-0x0F
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',   // 0x10-0x17
    'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',   // 0x18-0x1F
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',   // 0x20-0x27
    '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',   // 0x28-0x2F
    'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',   // 0x30-0x37
    0,    ' ',  0,    0,    0,    0,    0,    0,      // 0x38-0x3F
};

// kb_push : ajoute un caractère au buffer (contexte IRQ).
//   Si le buffer est plein, le caractère est JETÉ (pas de blocage en
//   IRQ : on ne peut pas attendre, ça figerait le système).
static void kb_push(char c) {
    // Section critique courte : l'IRQ est déjà en cours (les IRQ de même
    // priorité sont masquées par le CPU via la porte d'interruption),
    // donc pas besoin de cli ici pour ce petit OS mono-tâche.
    if (kb_count >= KB_BUF_SIZE) {
        return;  // Plein : on jette (choix documenté, pas de fuite).
    }
    kb_buf[kb_tail] = c;
    kb_tail++;
    if (kb_tail >= KB_BUF_SIZE) {
        kb_tail = 0;  // Reboucle (circulaire).
    }
    kb_count++;
}

// keyboard_irq_handler : handler IRQ1, appelé par irq_handler_c.
//   r : image des registres (inutilisée ici, on ne fait que lire 0x60).
//   NE PAS envoyer l'EOI ici : irq_handler_c s'en charge après retour.
//   NE PAS bloquer (pas de hlt/boucle infinie en IRQ).
static void keyboard_irq_handler(regs_t *r) {
    (void)r;  // Paramètre imposé par la signature irq_handler_t, inutile ici.

    // Vérifier que le contrôleur a vraiment une donnée (bit 0 du statut).
    // (IRQ parasite possible : on sort sans rien faire plutôt que de
    // lire un octet poubelle sur 0x60.)
    if ((inb(KB_STATUS) & KB_OUT_FULL) == 0) {
        return;
    }
    unsigned char sc = inb(KB_DATA);

    // 1. Préfixe étendu 0xE0 (flèches, Inser, Suppr...) : on mémorise et
    //    on attend le scancode suivant, qu'on IGNORERA (limite étape 2).
    if (sc == SC_EXTENDED) {
        ext_prefix = 1;
        return;
    }
    // Touche étendue : jeter le scancode qui suit le préfixe.
    if (ext_prefix) {
        ext_prefix = 0;
        return;
    }

    // 2. Touche RELÂCHÉE (bit 7 = 1) : mettre à jour Shift, ignorer le reste.
    //    Exemple : relâcher 'A' donne 0x1E + 0x80 = 0x9E -> on jette.
    if (sc & SC_RELEASE) {
        unsigned char make = (unsigned char)(sc & ~SC_RELEASE);  // Retire le bit 7.
        if (make == SC_LSHIFT || make == SC_RSHIFT) {
            shift_down = 0;  // Un Shift relâché : on suppose l'autre aussi
                             // relâché (simplification : pas de compteur).
        }
        return;
    }

    // 3. Touche PRESSÉE : gérer les modifieurs d'abord.
    if (sc == SC_LSHIFT || sc == SC_RSHIFT) {
        shift_down = 1;
        return;
    }
    if (sc == SC_CAPS) {
        caps_on = !caps_on;  // Toggle UNIQUEMENT à la pression (pas au release,
                             // déjà filtré ci-dessus, donc pas de double toggle).
        return;
    }

    // 4. Traduction scancode -> ASCII via les tables.
    if (sc >= 64) {
        return;  // Hors tables (pavé numérique étendu...) : ignoré étape 2.
    }
    // Shift XOR Caps pour les lettres : les deux s'annulent (comportement PC).
    // Simplification : on applique le XOR à tout (chiffres + lettres).
    // Effet de bord mineur : Caps + Shift sur '1' donne '1' au lieu de '!' ;
    // acceptable et documenté pour un driver pédagogique.
    int use_shift = shift_down ^ caps_on;
    char c = use_shift ? sc_shift[sc] : sc_normal[sc];
    if (c == 0) {
        return;  // Touche non imprimable (F1-F10, Ctrl...) : ignorée étape 2.
    }
    kb_push(c);
}

int keyboard_has_key(void) {
    return kb_count > 0;
}

char kgetc(void) {
    // Attente bloquante : "hlt" endort le CPU jusqu'à la prochaine IRQ
    // (le clavier réveillera via IRQ1). Boucle car plusieurs IRQ non-clavier
    // peuvent réveiller sans remplir le buffer (interruptions parasites).
    while (kb_count == 0) {
        __asm__ volatile("hlt");
    }
    // Lecture : pas de cli ici car mono-tâche + lecture d'un int aligné ;
    // l'IRQ ne peut interrompre qu'ENTRE deux instructions C, et kb_count
    // reste cohérent (écrit par IRQ, lu ici). Suffisant étape 2.
    char c = kb_buf[kb_head];
    kb_head++;
    if (kb_head >= KB_BUF_SIZE) {
        kb_head = 0;
    }
    kb_count--;
    return c;
}

void keyboard_init(void) {
    // Réinitialiser l'état (utile si ré-init après un test).
    kb_head = 0;
    kb_tail = 0;
    kb_count = 0;
    shift_down = 0;
    caps_on = 0;
    ext_prefix = 0;

    // Vider un scancode résiduel éventuel (touche pressée pendant le boot).
    // Lecture conditionnelle : seulement si le bit 0 du statut est à 1,
    // sinon inb(0x60) rendrait une valeur poubelle/stale.
    if (inb(KB_STATUS) & KB_OUT_FULL) {
        (void)inb(KB_DATA);  // Jette l'octet résiduel.
    }

    // S'enregistrer sur IRQ1 (vecteur 33) puis autoriser IRQ1 au PIC.
    // Ordre : enregistrer AVANT de démasquer, sinon la première frappe
    // arriverait sans handler (dispatch vers 0 = EOI seule, touche perdue).
    irq_install_handler(1, keyboard_irq_handler);
    pic_set_mask(1, 0);  // 0 = démasquer = autoriser le clavier.
}
