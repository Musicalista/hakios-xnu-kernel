#!/system/bin/sh
# switch-to-android.sh: Dual-boot rollback switcher to Stock Samsung Android
# Target: Samsung Galaxy A56 5G (SM-A566E / Exynos 1580 / s5e8855)

set -e

if [ "$(id -u)" -ne 0 ]; then
    echo "[-] Erro: Root requerido. Execute com 'su'."
    exit 1
fi

BOOT_DEV="/dev/block/by-name/boot"
if [ ! -e "$BOOT_DEV" ]; then
    SLOT=$(getprop ro.boot.slot_suffix)
    BOOT_DEV="/dev/block/by-name/boot$SLOT"
fi

if [ ! -e "$BOOT_DEV" ]; then
    echo "[-] Particao boot nao encontrada."
    exit 1
fi

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
IMAGE="$SCRIPT_DIR/stock-boot.img"

if [ ! -f "$IMAGE" ]; then
    IMAGE="/sdcard/stock-boot.img"
fi

if [ ! -f "$IMAGE" ]; then
    echo "[-] Imagem stock-boot.img nao encontrada em $SCRIPT_DIR nem em /sdcard."
    exit 1
fi

SIZE=$(wc -c < "$IMAGE" | tr -d ' ')
if [ "$SIZE" -ne 67108864 ]; then
    echo "[-] Tamanho da imagem invalido ($SIZE bytes). Esperado: 67108864 (64 MB)."
    exit 1
fi

echo "[*] Restaurando kernel oficial Samsung One UI em $BOOT_DEV..."
dd if="$IMAGE" of="$BOOT_DEV" bs=4096 status=none
sync

echo "[+] Android oficial restaurado com sucesso."
echo "[*] Reiniciando o aparelho em 3 segundos..."
sleep 3
reboot
