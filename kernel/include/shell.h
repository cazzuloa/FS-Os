// shell.h — Mini-shell interactif EXTENSIBLE.
// ---------------------------------------------------------------
// Pour AJOUTER TA PROPRE COMMANDE (c'est fait pour ça), 3 lignes :
//   1. Écris une fonction :  void ma_cmd(int argc, char **argv) { ... }
//      - argc : nombre de mots tapés ("add 2 3" -> argc = 3).
//      - argv : les mots (argv[0] = nom de la commande, argv[1..] = args).
//      - Affiche avec printk(), pas de valeur de retour.
//   2. Enregistre-la dans shell_init_builtins() (en bas de shell.c) :
//        shell_add_command("macmd", "ce que ça fait", ma_cmd);
//   3. Recompile + teste dans QEMU. C'est tout, pas de malloc ni de header.
//
// Exemple minimal (copie-colle dans shell.c) :
//   static void cmd_hello(int argc, char **argv) {
//       (void)argc; (void)argv;   // Dit au compilateur "inutilisés, normal".
//       printk("Salut !\n");
//   }
//   // ... dans shell_init_builtins() :
//   shell_add_command("hello", "dit salut", cmd_hello);

#ifndef SHELL_H
#define SHELL_H

// shell_cmd_fn : signature OBLIGATOIRE de toute commande.
//   argc/argv comme en Unix (voir l'exemple ci-dessus).
typedef void (*shell_cmd_fn)(int argc, char **argv);

// shell_add_command : enregistre une commande (nom + aide + fonction).
//   name : mot-clé tapé par l'utilisateur ("help", "echo"...).
//   help : une ligne d'aide affichée par la commande "help".
//   fn   : ta fonction. Appelée quand l'utilisateur tape name.
//   Si la table est pleine ou name déjà pris : ignoré (pas de crash).
void shell_add_command(const char *name, const char *help, shell_cmd_fn fn);

// shell_run : boucle principale du shell. NE REVIENT JAMAIS.
//   Affiche le prompt "fs-os> ", lit une ligne (readline avec écho +
//   backspace), la découpe en argc/argv et appelle la bonne commande.
//   À appeler depuis kernel_main après keyboard_init().
void shell_run(void);

#endif
