# FS-Os

Petit système d'exploitation x86 expérimental, écrit en Assembleur NASM (bootloader) et C freestanding (kernel). But pédagogique : comprendre le boot, le passage en mode protégé et l'affichage VGA en mode texte.

Au démarrage : le bootloader affiche un message en mode réel 16-bit, charge le kernel depuis le disque (INT 13h), bascule en mode protégé 32-bit via une GDT, puis saute vers le kernel qui efface l'écran et affiche `Hello from Kernel !`.

## Architecture

```
FS-Os/
├── bootloader/boot.asm   # Bootloader 16-bit (512 octets, signature 0xAA55)
├── kernel/
│   ├── kernel.ld         # Linker script : base physique 0x1000, sortie binaire plate
│   ├── include/          # Headers : io, pic, idt, keyboard, kstring, kprintf, shell, vga
│   └── src/
│       ├── start.S       # Point d'entrée _start (linké en premier à 0x1000)
│       ├── kernel.c      # kernel_main : init IDT -> clavier -> shell
│       ├── idt.c         # IDT + dispatch IRQ
│       ├── isr.asm       # Stubs asm exceptions 0-31 + IRQ 0-15
│       ├── pic.c         # Remap PIC 8259 vers 32/40
│       ├── keyboard.c    # Pilote PS/2 (set 1, US) sur IRQ1
│       ├── kstring.c     # Mini-libc : strlen/strcmp/memset/...
│       ├── kprintf.c     # printk() vers VGA
│       ├── shell.c       # Shell : readline + parse + commandes
│       └── vga.c         # Pilote VGA texte 80x25
├── scripts/
│   ├── compile.sh        # Build : NASM + gcc -m32 + ld + image disque 1.44 Mo
│   └── run_qemu.sh       # Lancement QEMU
└── bin/                  # Artefacts générés (ignoré par git) : boot.bin, kernel.bin, os.img
```

## Fonctionnement

1. **BIOS** charge `boot.bin` à `0x7C00`.
2. **Bootloader (`boot.asm`)** :
   - Initialise segments et pile (`SS:SP = 0x0000:0x7C00`).
   - Affiche `CA MARCHE OMGGG !` en rouge/vert/blanc via la mémoire vidéo `0xB800`.
   - Charge 16 secteurs à partir du secteur 2 vers `0x1000` (`INT 13h, AH=02h`).
   - Charge la GDT (segments code/data ring0, flat 4 Go), positionne `CR0.PE`, saut lointain en 32-bit, pile à `0x90000`, puis `jmp 0x1000`.
3. **Kernel (`kernel/src/kernel.c`, entrée `_start` dans `start.S`)** :
   - Linké à `0x1000` par `kernel/kernel.ld` en binaire plat.
   - `idt_init` : remap du PIC vers 32/40, IDT (exceptions + IRQ), puis `sti`.
   - `keyboard_init` : handler sur IRQ1, scancodes traduits en ASCII (layout US).
   - `shell_run` : prompt `fs-os>`, lecture de ligne, découpage `argc/argv`, commandes (`help`, `clear`, `echo`, `info`). Nouvelles commandes : voir `kernel/include/shell.h`.

## Prérequis

```bash
sudo apt install nasm gcc-multilib build-essential qemu-system-x86
```

## Compiler

Depuis la racine du projet :

```bash
chmod +x scripts/compile.sh
./scripts/compile.sh
```

Le script vérifie que `boot.bin` fait exactement 512 octets avec signature `55 AA`, compile le kernel (`gcc -m32 -ffreestanding -nostdlib`), le linke en binaire plat à `0x1000`, puis concatène `boot.bin + kernel.bin` dans `bin/os.img` (1.44 Mo). S'il ne trouve aucune source kernel, il bascule en mode « bootloader seul » (`os.img` = copie de `boot.bin`).

## Lancer dans QEMU

```bash
chmod +x scripts/run_qemu.sh
./scripts/run_qemu.sh
# équivaut à :
qemu-system-x86_64 -drive format=raw,file=bin/os.img,index=0,media=disk -m 32M
```

Résultat attendu : messages d'init (IDT, clavier), puis prompt `fs-os>` ; essayer `help`, `echo hello`, `info`, `clear`.

## CI

Workflow `.github/workflows/build_img.yml` : installe les dépendances sur `ubuntu-latest`, lance `compile.sh`, vérifie `bin/os.img` / `boot.bin`, puis publie `os.img`, `boot.bin`, `kernel.bin` en artefacts.

## Limites et pistes

IDT + PIC, clavier PS/2 (US), `printk`, shell de base : OK. Pas encore : timer (PIT), layout AZERTY, pagination, heap `kmalloc`, drivers disque, syscalls.
