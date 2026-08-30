#!/usr/bin/env bash
# ==============================================================================
# Android 15 ROM Preparation & Packaging Pipeline
# ==============================================================================
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}====================================================${NC}"
echo -e "${GREEN}===   Android 15 Rootless Container ROM Pipeline   ===${NC}"
echo -e "${GREEN}====================================================${NC}"

WORK_DIR=$(pwd)
ROOTFS_DIR="${WORK_DIR}/rootfs"
OUTPUT_ARCHIVE="${WORK_DIR}/android15_rootfs.tar.xz"

# Link Resmi Google AOSP ARM64 Android 15 GSI
OFFICIAL_GSI_URL="https://dl.google.com/developers/android/vic/images/gsi/aosp_arm64-exp-AP3A.241005.015-12366759-3c0ee79d.zip"
GSI_ZIP_NAME="aosp_arm64_android15_gsi.zip"

# 1. Cek Tools
echo -e "\n${CYAN}[1/5] Memeriksa tools sistem...${NC}"
for cmd in python3 tar xz 7z curl; do
    if ! command -v $cmd &> /dev/null; then
        echo -e "${RED}Error: Tool '$cmd' tidak ditemukan. Silakan install dengan: sudo apt install 7zip xz-utils tar python3 curl android-sdk-libsparse-utils${NC}"
        exit 1
    fi
done
echo "Semua tools tersedia."

# 2. Cek atau Download system.img
if [ ! -f "system.img" ] && [ ! -d "rootfs/system" ]; then
    echo -e "\n${YELLOW}[2/5] File 'system.img' belum ada di folder.${NC}"
    echo -e "Mengunduh Android 15 ARM64 GSI Resmi dari Google Developer CDN..."
    echo -e "URL: ${CYAN}${OFFICIAL_GSI_URL}${NC}"
    
    curl -L -C - -o "${GSI_ZIP_NAME}" "${OFFICIAL_GSI_URL}" --progress-bar

    echo -e "${GREEN}Ekstraksi file ZIP...${NC}"
    7z x "${GSI_ZIP_NAME}" -y system.img || true
    rm -f "${GSI_ZIP_NAME}"
else
    echo -e "\n${GREEN}[2/5] File 'system.img' atau folder rootfs/ sudah tersedia.${NC}"
fi

# 3. Ekstraksi system.img jika folder rootfs belum ada
if [ ! -d "rootfs/system" ]; then
    echo -e "\n${CYAN}[3/5] Mengekstrak partisi system.img ke folder rootfs/...${NC}"
    mkdir -p "${ROOTFS_DIR}"

    # Cek apakah sparse image dan gunakan simg2img bila ada
    if command -v simg2img &> /dev/null; then
        echo "Mengonversi Android sparse image..."
        simg2img system.img system_raw.img || cp system.img system_raw.img
        7z x system_raw.img -o"${ROOTFS_DIR}" -y || true
        rm -f system_raw.img
    else
        echo "Mengekstrak langsung dengan 7z..."
        7z x system.img -o"${ROOTFS_DIR}" -y || true
    fi
else
    echo -e "\n${GREEN}[3/5] Folder rootfs/system sudah diekstrak.${NC}"
fi

# 4. Jalankan Python Deep Patcher
echo -e "\n${CYAN}[4/5] Menjalankan deep-patching Android 15...${NC}"
python3 "${WORK_DIR}/patch_rootfs.py" "${ROOTFS_DIR}"

# 5. Kompresi Hasil Akhir ke tar.xz
echo -e "\n${CYAN}[5/5] Mengompres RootFS ke format ${OUTPUT_ARCHIVE}...${NC}"
echo "Proses kompresi sedang berjalan, mohon tunggu..."

cd "${ROOTFS_DIR}"
# Gunakan tar dengan multi-threading xz (T0)
tar -I 'xz -T0' -cf "${OUTPUT_ARCHIVE}" *

echo -e "\n${GREEN}====================================================${NC}"
echo -e "${GREEN}=== SUKSES! File ROM Android 15 Siap Digunakan ===${NC}"
echo -e "${GREEN}====================================================${NC}"
echo -e "File Output: ${CYAN}${OUTPUT_ARCHIVE}${NC}"
ls -lh "${OUTPUT_ARCHIVE}"
