// shell.c — Mini-shell : readline + parse + dispatch.
// Voir shell.h pour AJOUTER TES COMMANDES (3 lignes, exemple fourni).
//
// Architecture :
//   shell_run() : boucle infinie [prompt -> readline -> parse -> exec].
//   readline()  : lit avec kgetc(), écho via vga_putc(), gère backspace.
//   parse()     : découpe "  add   2  3 " en argc=3, argv={"add","2","3"}.
//   exec()      : cherche argv[0] dans la table, appelle fn ou erreur.

#include "shell.h"
#include "vga.h"
#include "keyboard.h"  // kgetc().
#include "kprintf.h"   // printk().
#include "kstring.h"   // strcmp/strlen/memset.

// --- Limites (statiques, pas de malloc dans ce noyau) ---
#define SHELL_LINE_MAX 256  // Longueur max d'une ligne tapée.
#define SHELL_ARGS_MAX 16   // Nombre max de mots par ligne.
#define SHELL_CMDS_MAX 16   // Nombre max de commandes enregistrées.

// --- Table des commandes (remplie par shell_add_command) ---
typedef struct {
    const char  *name;  // Mot-clé ("help"). Pointeur vers littéral, pas copié.
    const char  *help;  // Aide une ligne. Idem, pas copié.
    shell_cmd_fn fn;    // Fonction à appeler.
} shell_cmd_t;

static shell_cmd_t cmds[SHELL_CMDS_MAX];
static int         ncmds = 0;

void shell_add_command(const char *name, const char *help, shell_cmd_fn fn) {
    // Refuser les enregistrements invalides sans crasher.
    if (!name || !fn || ncmds >= SHELL_CMDS_MAX) {
        return;  // Table pleine (16) : augmente SHELL_CMDS_MAX si besoin.
    }
    // Refuser les doublons (deux "help" -> le 2e est ignoré, pas écrasé).
    for (int i = 0; i < ncmds; i++) {
        if (strcmp(cmds[i].name, name) == 0) {
            return;
        }
    }
    cmds[ncmds].name = name;
    cmds[ncmds].help = help ? help : "";  // help 0 -> chaîne vide (pas de crash).
    cmds[ncmds].fn = fn;
    ncmds++;
}

// ================== COMMANDES INTÉGRÉES (exemples à copier) ==================
// Chaque commande est "static" (visible seulement ici) et suit la
// signature shell_cmd_fn. Pour les tiennes, copie ce modèle.

// cmd_help : liste les commandes ("help" seul suffit, args ignorés).
static void cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;  // Inutilisés : convention anti-warning.
    printk("Commandes :\n");
    for (int i = 0; i < ncmds; i++) {
        printk("  %s - %s\n", cmds[i].name, cmds[i].help);
    }
}

// cmd_clear : efface l'écran (via le pilote VGA).
static void cmd_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    vga_clear();
}

// cmd_echo : réaffiche ses arguments ("echo hello world" -> "hello world").
// Modèle parfait pour TA première commande : boucle sur argv[1..].
static void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            printk(" ");  // Séparateur entre les mots (pas avant le 1er).
        }
        printk("%s", argv[i]);
    }
    printk("\n");
}

// cmd_info : infos système (montre printk %s/%d/%x en action).
static void cmd_info(int argc, char **argv) {
    (void)argc; (void)argv;
    printk("FS-Os - x86 32-bit, mode protege\n");
    printk("entree=0x%x, pile=0x%x, cmds=%d/%d\n",
           0x1000, 0x90000, ncmds, SHELL_CMDS_MAX);
}

// ================== TON CODE VA ICI (exemple) ==================
// Décommente pour tester le mécanisme, puis écris les tiennes :
//
// static void cmd_hello(int argc, char **argv) {
//     (void)argc; (void)argv;
//     printk("Salut, ceci est MA commande !\n");
// }

// shell_init_builtins : enregistre les commandes de base.
// + C'EST ICI que tu ajoutes les tiennes (une ligne par commande).
static void shell_init_builtins(void) {
    shell_add_command("help",  "liste les commandes", cmd_help);
    shell_add_command("clear", "efface l'ecran",      cmd_clear);
    shell_add_command("echo",  "affiche ses args",    cmd_echo);
    shell_add_command("info",  "infos systeme",       cmd_info);
    // ---> AJOUTE LES TIENNES CI-DESSOUS (exemple) :
    // shell_add_command("hello", "dit salut", cmd_hello);
}

// ================== MOTEUR DU SHELL (pas besoin d'y toucher) ==================

// readline : lit une ligne au clavier avec écho.
//   buf : buffer de n octets (n = SHELL_LINE_MAX ici).
//   Retour : longueur de la ligne (sans le '\0').
//   - Écho chaque frappe via vga_putc (le '\b' efface visuellement).
//   - Backspace en buffer vide : ignoré (pas de '\b' fantôme).
//   - Ligne trop longue : caractères en trop JETÉS (pas de dépassement).
static int readline(char *buf, int n) {
    int len = 0;
    for (;;) {
        char c = kgetc();  // Bloquant (hlt jusqu'à IRQ1).
        // Entrée : fin de ligne. On écho '\n' puis on termine la chaîne.
        if (c == '\n') {
            vga_putc('\n');
            buf[len] = '\0';
            return len;
        }
        // Retour arrière : effacer le dernier char SI il y en a un.
        if (c == '\b') {
            if (len > 0) {
                len--;
                vga_putc('\b');  // Le pilote VGA recule + efface la case.
            }
            continue;  // Buffer vide : on ignore silencieusement.
        }
        // Frappe normale : stocker (si place) + écho. Sans place : jeter
        // mais QUAND MÊME pas d'écho (sinon écran et buffer divergent).
        // On garde 1 octet pour le '\0' final -> limite à n-1.
        if (len < n - 1) {
            buf[len++] = c;
            vga_putc(c);
        }
    }
}

// parse : découpe buf en mots (séparateurs : espace/tab, multiples OK).
//   Modifie buf EN PLACE (' ' -> '\0') et remplit argv (pointeurs dedans).
//   Retour : argc (0 si ligne vide). argv[argc] = 0 (convention, pratique).
static int parse(char *buf, char **argv, int max_args) {
    int argc = 0;
    int i = 0;
    while (buf[i] != '\0' && argc < max_args) {
        // Sauter les séparateurs (espaces/tab en tête ou entre les mots).
        while (buf[i] == ' ' || buf[i] == '\t') {
            i++;
        }
        if (buf[i] == '\0') {
            break;  // Fin de ligne après des espaces : terminé.
        }
        // Début d'un mot : le mémoriser puis avancer jusqu'au séparateur.
        argv[argc++] = &buf[i];
        while (buf[i] != '\0' && buf[i] != ' ' && buf[i] != '\t') {
            i++;
        }
        // Couper le mot : si on s'est arrêté sur un séparateur, le remplacer
        // par '\0' (fin de chaîne C) et continuer après.
        if (buf[i] != '\0') {
            buf[i] = '\0';
            i++;
        }
    }
    argv[argc] = 0;  // Terminateur (comme execvp Unix, pour tes boucles).
    return argc;
}

void shell_run(void) {
    // Buffers statiques (pas de pile énorme, pas de malloc).
    static char  line[SHELL_LINE_MAX];
    static char *argv[SHELL_ARGS_MAX + 1];  // +1 pour le argv[argc] = 0.

    shell_init_builtins();
    printk("Shell pret. Tape 'help'.\n");

    for (;;) {
        // Prompt vert "fs-os> " puis retour au gris pour la frappe.
        vga_setcolor(2, 0);
        printk("fs-os> ");
        vga_setcolor(7, 0);

        int len = readline(line, SHELL_LINE_MAX);
        (void)len;  // len inutile ici (parse s'arrête au '\0'), anti-warning.
        int argc = parse(line, argv, SHELL_ARGS_MAX);
        if (argc == 0) {
            continue;  // Ligne vide (que des espaces) : nouveau prompt.
        }
        // Chercher argv[0] dans la table et exécuter.
        int found = 0;
        for (int i = 0; i < ncmds; i++) {
            if (strcmp(argv[0], cmds[i].name) == 0) {
                cmds[i].fn(argc, argv);
                found = 1;
                break;
            }
        }
        if (!found) {
            // Commande inconnue : message + piste (pas de halt, on continue).
            printk("commande inconnue: %s (tape help)\n", argv[0]);
        }
    }
}
