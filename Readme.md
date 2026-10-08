# FS-Os

Petit système d'exploitation x86 expérimental, écrit en Assembleur NASM (bootloader) et C freestanding (kernel). But pédagogique : comprendre le boot, le passage en mode protégé et l'affichage VGA en mode texte.

Au démarrage : le bootloader affiche un message en mode réel 16-bit, charge le kernel depuis le disque (INT 13h), bascule en mode protégé 32-bit via une GDT, puis saute vers le kernel qui efface l'écran et affiche `Hello from Kernel !`.

## Architecture

```
FS-Os/
├── bootloader/boot.asm   # Bootloader 16-bit (512 octets, signature 0xAA55)
├── kernel/
│   ├── kernel.c          # Kernel 32-bit minimal (VGA 0xB8000)
│   └── kernel.ld         # Linker script : base physique 0x1000, sortie binaire plate
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
3. **Kernel (`kernel.c`, point d'entrée `_start`)** :
   - Linké à `0x1000` par `kernel/kernel.ld` en binaire plat.
   - Efface l'écran VGA texte 80x25 et écrit `Hello from Kernel !` (vert sur noir, `0x0A`).
   - Boucle infinie sur `hlt`.

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

Résultat attendu : message du bootloader, puis écran effacé avec `Hello from Kernel !` en vert en haut à gauche.

## CI

Workflow `.github/workflows/build_img.yml` : installe les dépendances sur `ubuntu-latest`, lance `compile.sh`, vérifie `bin/os.img` / `boot.bin`, puis publie `os.img`, `boot.bin`, `kernel.bin` en artefacts.

## Limites et pistes

Pas d'interruptions (IDT), pas de gestion mémoire, pas de drivers clavier/disque au-delà du chargement initial, pas de syscalls. Prochaines étapes logiques : IDT + ISR, pilote clavier PS/2, `printk`/`printf`, pagination, heap `kmalloc`.
