#!/bin/bash
# ============================================================
# compile.sh - Compiler MyOS
# ============================================================
# Prérequis:
#   sudo apt install nasm gcc qemu-system-x86
#   (ou sur macOS: brew install nasm x86_64-elf-gcc qemu)
#
# Usage:
#   ./scripts/compile.sh            (depuis la racine du projet)
#   ./compile.sh                    (depuis scripts/)
# ============================================================

set -e

# --- 0. Se placer à la racine du projet (quel que soit le cwd) ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$ROOT_DIR"

echo "======================================"
echo "  MyOS Build Script"
echo "======================================"

OUTPUT_DIR="$ROOT_DIR/bin"
BOOT_SRC="$ROOT_DIR/bootloader/boot.asm"
SRC_DIR="$ROOT_DIR/kernel/src"
LD_SCRIPT="$ROOT_DIR/kernel/kernel.ld"

mkdir -p "$OUTPUT_DIR"

# --- Vérification des outils ---
if ! command -v nasm >/dev/null 2>&1; then
    echo "ERREUR: nasm introuvable. Installez-le (sudo apt install nasm)."
    exit 1
fi

# --- 1. Nettoyage des anciens fichiers ---
echo "[0/3] Nettoyage des anciens fichiers..."
rm -f "$OUTPUT_DIR"/boot.bin "$OUTPUT_DIR"/*.o "$OUTPUT_DIR"/kernel.bin "$OUTPUT_DIR"/os.img "$OUTPUT_DIR"/os_raw.img
echo "      -> Nettoyage terminé"

# --- 2. Compiler le bootloader avec NASM ---
echo "[1/3] Compilation du bootloader (bootloader/boot.asm)..."
if [ ! -f "$BOOT_SRC" ]; then
    echo "ERREUR: $BOOT_SRC introuvable."
    exit 1
fi
nasm -f bin "$BOOT_SRC" -o "$OUTPUT_DIR/boot.bin"

boot_size=$(wc -c < "$OUTPUT_DIR/boot.bin")
if [ "$boot_size" -ne 512 ]; then
    echo "ERREUR: Le bootloader doit faire exactement 512 octets (actuel: $boot_size bytes)."
    echo "        Vérifiez le 'times 510-(\$-\$\$) db 0' + 'dw 0xAA55' en fin de boot.asm."
    exit 1
fi

# Vérifie la signature de boot 0xAA55 (derniers 2 octets = 55 AA)
sig=$(tail -c 2 "$OUTPUT_DIR/boot.bin" | od -An -t x1 | tr -d ' \n')
if [ "$sig" != "55aa" ]; then
    echo "ERREUR: Signature de boot invalide (attendu 55aa, obtenu $sig). Il manque 'dw 0xAA55' ?"
    exit 1
fi
echo "      -> boot.bin OK (512 bytes, signature 55AA)"

# --- 3. Détecter si un kernel existe ---
# Mode "bootloader seul" si aucun .c/.asm/.S ni dans kernel/src ni dans kernel/.
HAS_KERNEL=false
if [ -d "$SRC_DIR" ]; then
    if compgen -G "$SRC_DIR/*.c" > /dev/null || compgen -G "$SRC_DIR/*.asm" > /dev/null || compgen -G "$SRC_DIR/*.S" > /dev/null; then
        HAS_KERNEL=true
    fi
fi
# Fallback historique : sources directement dans kernel/ (ex: kernel/kernel.c)
if [ "$HAS_KERNEL" = false ]; then
    if compgen -G "$ROOT_DIR/kernel/*.c" > /dev/null || compgen -G "$ROOT_DIR/kernel/*.asm" > /dev/null || compgen -G "$ROOT_DIR/kernel/*.S" > /dev/null; then
        HAS_KERNEL=true
    fi
fi

if [ "$HAS_KERNEL" = false ]; then
    echo "[2/3] Aucune source kernel trouvée dans $SRC_DIR -> mode bootloader seul."
    # boot.bin seul EST déjà une image bootable (512 octets + 55AA).
    # On le copie en os.img pour que run_qemu.sh fonctionne tel quel.
    cp "$OUTPUT_DIR/boot.bin" "$OUTPUT_DIR/os.img"
    echo "[3/3] Image disque (os.img) = copie de boot.bin (512 octets)."
    echo ""
    echo "======================================"
    echo "  Build terminé avec succès! (bootloader seul)"
    echo "  Lancer avec: ./scripts/run_qemu.sh"
    echo "======================================"
    exit 0
fi

# --- 4. Mode complet : compiler + linker le kernel ---
echo "[2/3] Compilation du kernel (sources)..."
OBJ_FILES=()

# Compile l'entrée (start.S) en premier afin que le point d'entrée soit au début du binaire
if [ -f "$SRC_DIR/start.S" ]; then
    obj="$OUTPUT_DIR/start.o"
    gcc -m32 \
        -ffreestanding \
        -fno-pie \
        -fno-stack-protector \
        -nostdlib \
        -nostdinc \
        -I "$ROOT_DIR/kernel/include" \
        -c "$SRC_DIR/start.S" \
        -o "$obj"
    OBJ_FILES+=("$obj")
    echo "      -> $(basename "$obj") OK"
fi

# Compiler les fichiers ASM du kernel (ex: isr_wrapper.asm)
for src in "$SRC_DIR"/*.asm; do
    [ -e "$src" ] || continue

    obj="$OUTPUT_DIR/$(basename "${src%.*}").o"
    nasm -f elf32 "$src" -o "$obj"
    OBJ_FILES+=("$obj")
    echo "      -> $(basename "$obj") OK"
done

# Compiler les fichiers C du kernel (kernel/src puis fallback kernel/)
for src in "$SRC_DIR"/*.c "$ROOT_DIR"/kernel/*.c; do
    [ -e "$src" ] || continue

    obj="$OUTPUT_DIR/$(basename "${src%.*}").o"
    gcc -m32 \
        -ffreestanding \
        -fno-pie \
        -fno-stack-protector \
        -nostdlib \
        -nostdinc \
        -I "$ROOT_DIR/kernel/include" \
        -c "$src" \
        -o "$obj"
    OBJ_FILES+=("$obj")
    echo "      -> $(basename "$obj") OK ($src)"
done

if [ ${#OBJ_FILES[@]} -eq 0 ]; then
    echo "ERREUR: Aucun fichier objet généré depuis $SRC_DIR."
    exit 1
fi

if [ ! -f "$LD_SCRIPT" ]; then
    echo "ERREUR: script de link introuvable: $LD_SCRIPT"
    exit 1
fi

echo "      Linkage du kernel..."
ld -m elf_i386 \
   -T "$LD_SCRIPT" \
   --oformat binary \
   -e _start \
   "${OBJ_FILES[@]}" \
   -o "$OUTPUT_DIR/kernel.bin"
kernel_size=$(wc -c < "$OUTPUT_DIR/kernel.bin")
echo "      -> kernel.bin OK ($kernel_size bytes)"

# --- 5. Créer l'image disque finale ---
echo "[3/3] Création de l'image disque (os.img)..."
# Concaténer boot.bin + kernel.bin dans une image de 1.44MB
cat "$OUTPUT_DIR"/boot.bin "$OUTPUT_DIR"/kernel.bin > "$OUTPUT_DIR"/os_raw.img
# Créer une image de 1.44MB (taille d'une disquette standard)
dd if=/dev/zero of="$OUTPUT_DIR"/os.img bs=1024 count=1440 2>/dev/null
# Copier os_raw.img dans os.img
dd if="$OUTPUT_DIR"/os_raw.img of="$OUTPUT_DIR"/os.img conv=notrunc 2>/dev/null
rm -f "$OUTPUT_DIR"/os_raw.img
echo "      -> os.img OK (1.44MB)"

echo ""
echo "======================================"
echo "  Build terminé avec succès!"
echo "======================================"